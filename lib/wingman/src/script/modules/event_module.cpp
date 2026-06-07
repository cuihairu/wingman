#include "wingman/event.hpp"
#include "wingman/script/iscript_engine.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#ifdef WINGMAN_HAS_HIREDIS
#include <hiredis/hiredis.h>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <mutex>
#endif

namespace wingman {
namespace script {
namespace modules {

namespace {

nlohmann::json toJson(const ScriptValue& value) {
	switch (value.type) {
	case ScriptValue::Null:
		return nullptr;
	case ScriptValue::Bool:
		return value.boolVal;
	case ScriptValue::Int:
		return value.intVal;
	case ScriptValue::Float:
		return value.floatVal;
	case ScriptValue::String:
		return value.strVal;
	case ScriptValue::Array: {
		nlohmann::json arr = nlohmann::json::array();
		for (const auto& item : value.arrayVal) {
			arr.push_back(toJson(item));
		}
		return arr;
	}
	case ScriptValue::Object: {
		nlohmann::json obj = nlohmann::json::object();
		for (const auto& [key, item] : value.objectVal) {
			obj[key] = toJson(item);
		}
		return obj;
	}
	case ScriptValue::Callable:
		return nullptr;
	}
	return nullptr;
}

ScriptValue fromJson(const nlohmann::json& value) {
	if (value.is_null()) return ScriptValue::null();
	if (value.is_boolean()) return ScriptValue::fromBool(value.get<bool>());
	if (value.is_number_integer()) return ScriptValue::fromInt(value.get<int64_t>());
	if (value.is_number_unsigned()) return ScriptValue::fromInt(static_cast<int64_t>(value.get<uint64_t>()));
	if (value.is_number_float()) return ScriptValue::fromFloat(value.get<double>());
	if (value.is_string()) return ScriptValue::fromString(value.get<std::string>());
	if (value.is_array()) {
		std::vector<ScriptValue> arr;
		arr.reserve(value.size());
		for (const auto& item : value) {
			arr.push_back(fromJson(item));
		}
		return ScriptValue::fromArray(std::move(arr));
	}
	if (value.is_object()) {
		std::unordered_map<std::string, ScriptValue> obj;
		for (auto it = value.begin(); it != value.end(); ++it) {
			obj[it.key()] = fromJson(it.value());
		}
		return ScriptValue::fromObject(std::move(obj));
	}
	return ScriptValue::null();
}

ScriptValue fromEventMessage(const EventMessage& msg) {
	return ScriptValue::fromObject({
		{"type", ScriptValue::fromString(msg.type)},
		{"source", ScriptValue::fromString(msg.source)},
		{"correlationId", ScriptValue::fromString(msg.correlationId)},
		{"timestamp", ScriptValue::fromInt(static_cast<int64_t>(msg.timestamp))},
		{"priority", ScriptValue::fromInt(msg.priority)},
		{"payload", fromJson(msg.payload)}
	});
}

#ifdef WINGMAN_HAS_HIREDIS

// ========== Remote Channel (Redis Stream) ==========

class RemoteChannel {
public:
	struct Config {
		std::string host = "localhost";
		int port = 6379;
		std::string stream;
		std::string consumerGroup = "wingman";
		std::string consumerName;
		int pollIntervalMs = 100;
		int blockingTimeoutMs = 5000;
	};

	RemoteChannel(const std::string& name, const Config& config)
		: name_(name), config_(config), running_(false), redis_(nullptr) {
		if (config_.consumerName.empty()) {
			config_.consumerName = "consumer_" + std::to_string(std::hash<std::string>{}(name + std::to_string(rand())));
		}
	}

	~RemoteChannel() {
		stop();
	}

	bool connect() {
		redis_ = redisConnect(config_.host.c_str(), config_.port);
		if (!redis_ || redis_->err) {
			spdlog::error("[RemoteChannel] Failed to connect to Redis: {}",
				redis_ ? redis_->errstr : "Connection failed");
			if (redis_) redisFree(redis_);
			redis_ = nullptr;
			return false;
		}
		return true;
	}

	bool start() {
		if (running_) return true;

		if (!connect()) {
			return false;
		}

		// 创建消费者组（如果不存在）
		createConsumerGroup();

		running_ = true;
		listenerThread_ = std::thread(&RemoteChannel::listen, this);
		return true;
	}

	void stop() {
		if (!running_) return;
		running_ = false;

		if (listenerThread_.joinable()) {
			listenerThread_.join();
		}

		if (redis_) {
			redisFree(redis_);
			redis_ = nullptr;
		}
	}

	void subscribe(const std::string& eventType, ScriptValue::CallableFunc callback) {
		std::lock_guard<std::mutex> lock(callbacksMutex_);
		callbacks_[eventType] = callback;
	}

	void unsubscribe(const std::string& eventType) {
		std::lock_guard<std::mutex> lock(callbacksMutex_);
		callbacks_.erase(eventType);
	}

	bool emit(const std::string& eventType, const ScriptValue& payload) {
		if (!redis_) return false;

		try {
			nlohmann::json data;
			data["type"] = eventType;
			data["payload"] = toJson(payload);
			data["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::system_clock::now().time_since_epoch()).count();

			std::string jsonStr = data.dump();

			// XADD stream * field value
			std::string cmd = "XADD " + config_.stream + " * data " + jsonStr;
			redisReply* reply = reinterpret_cast<redisReply*>(redisCommand(redis_, cmd.c_str()));

			if (!reply) {
				spdlog::error("[RemoteChannel] emit failed: null reply");
				return false;
			}

			bool success = (reply->type == REDIS_REPLY_STRING || reply->type == REDIS_REPLY_STATUS);
			freeReplyObject(reply);
			return success;

		} catch (const std::exception& e) {
			spdlog::error("[RemoteChannel] emit exception: {}", e.what());
			return false;
		}
	}

private:
	void createConsumerGroup() {
		if (!redis_) return;

		// XGROUP CREATE stream groupName $ MKSTREAM
		std::string cmd = "XGROUP CREATE " + config_.stream + " " +
			config_.consumerGroup + " $ MKSTREAM";

		redisReply* reply = reinterpret_cast<redisReply*>(redisCommand(redis_, cmd.c_str()));
		if (reply) {
			freeReplyObject(reply);
		}
	}

	void listen() {
		std::string lastId = ">";  // ">" 表示只接收新消息

		while (running_) {
			if (!redis_ || redis_->err) {
				// 尝试重连
				if (redis_) redisFree(redis_);
				std::this_thread::sleep_for(std::chrono::seconds(1));
				if (!connect()) continue;
			}

			// XREADGROUP GROUP consumer consumerName STREAMS stream ID COUNT 1 BLOCK timeout
			std::string cmd = "XREADGROUP GROUP " + config_.consumerGroup + " " +
				config_.consumerName + " STREAMS " + config_.stream + " " +
				lastId + " COUNT 1 BLOCK " + std::to_string(config_.blockingTimeoutMs);

			redisReply* reply = reinterpret_cast<redisReply*>(redisCommand(redis_, cmd.c_str()));
			if (!reply) {
				std::this_thread::sleep_for(std::chrono::milliseconds(config_.pollIntervalMs));
				continue;
			}

			if (reply->type == REDIS_REPLY_ARRAY && reply->elements > 0) {
				// 解析消息
				if (reply->element[0]->type == REDIS_REPLY_ARRAY) {
					for (size_t i = 0; i < reply->element[0]->elements; ++i) {
						redisReply* streamReply = reply->element[0]->element[i];
						if (streamReply->type == REDIS_REPLY_ARRAY && streamReply->elements >= 2) {
							lastId = streamReply->element[0]->str;

							if (streamReply->element[1]->type == REDIS_REPLY_ARRAY) {
								for (size_t j = 0; j < streamReply->element[1]->elements; j += 2) {
									if (strcmp(streamReply->element[1]->element[j]->str, "data") == 0) {
										std::string jsonStr = streamReply->element[1]->element[j + 1]->str;
										handleMessage(jsonStr);
										break;
									}
								}
							}
						}
					}
				}
			}

			freeReplyObject(reply);
		}
	}

	void handleMessage(const std::string& jsonStr) {
		try {
			nlohmann::json data = nlohmann::json::parse(jsonStr);
			std::string eventType = data.value("type", "");

			if (!eventType.empty()) {
				std::lock_guard<std::mutex> lock(callbacksMutex_);
				auto it = callbacks_.find(eventType);
				if (it != callbacks_.end()) {
					// 构造 ScriptValue 消息
					std::vector<ScriptValue> args;
					args.push_back(fromJson(data));
					it->second(args);
				}
			}
		} catch (const std::exception& e) {
			spdlog::error("[RemoteChannel] handleMessage exception: {}", e.what());
		}
	}

	static void freeReplyObject(redisReply* reply) {
		if (!reply) return;
		// hiredis < 1.0: 使用 freeReplyObject
		// hiredis >= 1.0: 使用 freeReply
		#if HIREDIS_MAJOR_VERSION >= 1
			freeReply(reply);
		#else
			freeReplyObject(reply);
		#endif
	}

	std::string name_;
	Config config_;
	bool running_;
	redisContext* redis_;
	std::thread listenerThread_;

	std::unordered_map<std::string, ScriptValue::CallableFunc> callbacks_;
	mutable std::mutex callbacksMutex_;
};

// ========== Remote Channel Manager ==========

class RemoteChannelManager {
public:
	static RemoteChannelManager& instance() {
		static RemoteChannelManager inst;
		return inst;
	}

	bool registerChannel(const std::string& name, const RemoteChannel::Config& config) {
		std::lock_guard<std::mutex> lock(mutex_);

		if (channels_.find(name) != channels_.end()) {
			spdlog::warn("[RemoteChannel] Channel '{}' already registered", name);
			return false;
		}

		auto channel = std::make_unique<RemoteChannel>(name, config);
		if (!channel->start()) {
			spdlog::error("[RemoteChannel] Failed to start channel '{}'", name);
			return false;
		}

		channels_[name] = std::move(channel);
		return true;
	}

	RemoteChannel* getChannel(const std::string& name) {
		std::lock_guard<std::mutex> lock(mutex_);
		auto it = channels_.find(name);
		return it != channels_.end() ? it->second.get() : nullptr;
	}

	void unregisterChannel(const std::string& name) {
		std::lock_guard<std::mutex> lock(mutex_);
		channels_.erase(name);
	}

private:
	mutable std::mutex mutex_;
	std::unordered_map<std::string, std::unique_ptr<RemoteChannel>> channels_;
};

#endif // WINGMAN_HAS_HIREDIS

} // namespace

ModuleDescriptor createEventModule() {
	ModuleDescriptor mod;
	mod.name = "event";

	// on(type: string, callback: function, name?: string) -> subscriptionId
	mod.functions.push_back({"on", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.size() < 2) return ScriptValue::fromBool(false);

		std::string type = args[0].asString();
		if (!args[1].isCallable()) return ScriptValue::fromBool(false);

		ScriptValue::CallableFunc callback = args[1].callableVal;
		std::string name = args.size() > 2 && args[2].isString() ? args[2].asString() : "";

		// Wrap the callback to convert EventMessage to ScriptValue
		uint64_t id = EventHub::instance().subscribe(type, [callback, type](const EventMessage& msg) {
			std::vector<ScriptValue> cbArgs;
			cbArgs.push_back(ScriptValue::fromObject({
				{"type", ScriptValue::fromString(msg.type)},
				{"source", ScriptValue::fromString(msg.source)},
				{"correlationId", ScriptValue::fromString(msg.correlationId)},
				{"timestamp", ScriptValue::fromInt(static_cast<int64_t>(msg.timestamp))},
				{"priority", ScriptValue::fromInt(msg.priority)},
				{"payload", fromJson(msg.payload)}
			}));
			// Wrap callback in try-catch to prevent exception propagation
			try {
				callback(cbArgs);
			} catch (const std::exception& e) {
				spdlog::error("[event] on callback exception for '{}': {}", type, e.what());
			} catch (...) {
				spdlog::error("[event] on callback unknown exception for '{}'", type);
			}
		}, name, false);

		return ScriptValue::fromInt(static_cast<int64_t>(id));
	}, "type:string, callback:function, name?:string -> subscriptionId:int"});

	// once(type: string, callback: function) -> subscriptionId
	mod.functions.push_back({"once", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.size() < 2) return ScriptValue::fromBool(false);

		std::string type = args[0].asString();
		if (!args[1].isCallable()) return ScriptValue::fromBool(false);

		ScriptValue::CallableFunc callback = args[1].callableVal;

		uint64_t id = EventHub::instance().subscribe(type, [callback, type](const EventMessage& msg) {
			std::vector<ScriptValue> cbArgs;
			cbArgs.push_back(ScriptValue::fromObject({
				{"type", ScriptValue::fromString(msg.type)},
				{"source", ScriptValue::fromString(msg.source)},
				{"correlationId", ScriptValue::fromString(msg.correlationId)},
				{"timestamp", ScriptValue::fromInt(static_cast<int64_t>(msg.timestamp))},
				{"priority", ScriptValue::fromInt(msg.priority)},
				{"payload", fromJson(msg.payload)}
			}));
			// Wrap callback in try-catch to prevent exception propagation
			try {
				callback(cbArgs);
			} catch (const std::exception& e) {
				spdlog::error("[event] on callback exception for '{}': {}", type, e.what());
			} catch (...) {
				spdlog::error("[event] on callback unknown exception for '{}'", type);
			}
		}, "", true);

		return ScriptValue::fromInt(static_cast<int64_t>(id));
	}, "type:string, callback:function -> subscriptionId:int"});

	mod.functions.push_back({"emit", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty()) return ScriptValue::fromBool(false);

		std::string type = args[0].asString();
		nlohmann::json payload = args.size() > 1 ? toJson(args[1]) : nlohmann::json::object();
		std::string source;
		std::string correlationId;
		int priority = 0;

		if (args.size() > 2 && args[2].isObject()) {
			if (auto* value = args[2].get("source")) source = value->asString();
			if (auto* value = args[2].get("correlationId")) correlationId = value->asString();
			if (auto* value = args[2].get("priority")) priority = static_cast<int>(value->asInt());
		}

		EventHub::instance().emit(type, payload, source, correlationId, priority);
		return ScriptValue::fromBool(true);
	}, "type:string, payload?:object, meta?:{source?,correlationId?,priority?} -> bool"});

	mod.functions.push_back({"off", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty()) return ScriptValue::fromBool(false);
		if (args[0].isString()) {
			EventHub::instance().unsubscribe(args[0].asString());
		} else {
			EventHub::instance().unsubscribe(static_cast<uint64_t>(args[0].asInt()));
		}
		return ScriptValue::fromBool(true);
	}, "subscription:int|string -> bool"});

	mod.functions.push_back({"clear", [](const std::vector<ScriptValue>&) -> ScriptValue {
		EventHub::instance().clear();
		return ScriptValue::null();
	}, "() -> nil"});

	mod.functions.push_back({"message", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		EventMessage msg;
		if (!args.empty()) msg.type = args[0].asString();
		if (args.size() > 1) msg.payload = toJson(args[1]);
		if (args.size() > 2 && args[2].isObject()) {
			if (auto* value = args[2].get("source")) msg.source = value->asString();
			if (auto* value = args[2].get("correlationId")) msg.correlationId = value->asString();
			if (auto* value = args[2].get("priority")) msg.priority = static_cast<int>(value->asInt());
		}
		return fromEventMessage(msg);
	}, "type:string, payload?:object, meta?:object -> event"});

#ifdef WINGMAN_HAS_HIREDIS
	// ========== Remote Channel Functions ==========

	mod.functions.push_back({"registerChannel", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.size() < 2) return ScriptValue::fromBool(false);

		std::string name = args[0].asString();
		if (!args[1].isObject()) return ScriptValue::fromBool(false);

		RemoteChannel::Config config;
		if (auto* value = args[1].get("redis")) {
			if (value->isObject()) {
				if (auto* host = value->get("host")) config.host = host->asString();
				if (auto* port = value->get("port")) config.port = static_cast<int>(port->asInt());
			}
		}
		if (auto* value = args[1].get("stream")) config.stream = value->asString();
		if (auto* value = args[1].get("consumerGroup")) config.consumerGroup = value->asString();
		if (auto* value = args[1].get("pollInterval")) config.pollIntervalMs = static_cast<int>(value->asInt());
		if (auto* value = args[1].get("blockingTimeout")) config.blockingTimeoutMs = static_cast<int>(value->asInt());

		// 默认 stream 名称为 channel 名
		if (config.stream.empty()) config.stream = name + ":events";

		auto& manager = RemoteChannelManager::instance();
		return ScriptValue::fromBool(manager.registerChannel(name, config));
	}, "name:string, config:{redis?:{host?,port?},stream?,consumerGroup?,pollInterval?,blockingTimeout?} -> bool"});

	mod.functions.push_back({"getChannel", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty()) return ScriptValue::null();

		std::string name = args[0].asString();
		auto& manager = RemoteChannelManager::instance();
		RemoteChannel* channel = manager.getChannel(name);

		if (!channel) {
			return ScriptValue::fromObject(std::unordered_map<std::string, ScriptValue>{
				{"success", ScriptValue::fromBool(false)},
				{"error", ScriptValue::fromString("Channel not found")}
			});
		}

		// 返回 channel 对象的句柄
		return ScriptValue::fromObject(std::unordered_map<std::string, ScriptValue>{
			{"success", ScriptValue::fromBool(true)},
			{"name", ScriptValue::fromString(name)}
		});
	}, "name:string -> {success,true,error?}"});

	// channel().on() - 远程频道订阅
	mod.functions.push_back({"channelOn", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.size() < 3) return ScriptValue::fromBool(false);

		std::string channelName = args[0].asString();
		std::string eventType = args[1].asString();
		if (!args[2].isCallable()) return ScriptValue::fromBool(false);

		auto& manager = RemoteChannelManager::instance();
		RemoteChannel* channel = manager.getChannel(channelName);
		if (!channel) {
			return ScriptValue::fromBool(false);
		}

		channel->subscribe(eventType, args[2].callableVal);
		return ScriptValue::fromBool(true);
	}, "channelName:string, eventType:string, callback:function -> bool"});

	// channel().emit() - 远程频道发布
	mod.functions.push_back({"channelEmit", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.size() < 2) return ScriptValue::fromBool(false);

		std::string channelName = args[0].asString();
		std::string eventType = args[1].asString();
		nlohmann::json payload = args.size() > 2 ? toJson(args[2]) : nlohmann::json::object();

		auto& manager = RemoteChannelManager::instance();
		RemoteChannel* channel = manager.getChannel(channelName);
		if (!channel) {
			return ScriptValue::fromBool(false);
		}

		return ScriptValue::fromBool(channel->emit(eventType, args.size() > 2 ? args[2] : ScriptValue::null()));
	}, "channelName:string, eventType:string, payload?:object -> bool"});

	// 取消远程频道订阅
	mod.functions.push_back({"channelOff", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.size() < 2) return ScriptValue::fromBool(false);

		std::string channelName = args[0].asString();
		std::string eventType = args[1].asString();

		auto& manager = RemoteChannelManager::instance();
		RemoteChannel* channel = manager.getChannel(channelName);
		if (!channel) {
			return ScriptValue::fromBool(false);
		}

		channel->unsubscribe(eventType);
		return ScriptValue::fromBool(true);
	}, "channelName:string, eventType:string -> bool"});

#else
	// 当 hiredis 不可用时，提供错误信息
	mod.functions.push_back({"registerChannel", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::fromObject(std::unordered_map<std::string, ScriptValue>{
			{"success", ScriptValue::fromBool(false)},
			{"error", ScriptValue::fromString("Remote channels not enabled - hiredis not available")}
		});
	}, "name:string, config:object -> {success,error}"});
#endif

	return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
