#include "wingman/script/iscript_engine.hpp"
#include "wingman/event.hpp"

#ifdef WINGMAN_HAS_TRANSPORT
#include "wingman/transport/transport.hpp"
#include "wingman/transport/transport_client.hpp"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <mutex>
#include <functional>
#include <chrono>
#include <unordered_map>
#include <string>
#endif

namespace wingman {
namespace script {
namespace modules {

#ifdef WINGMAN_HAS_TRANSPORT

// ========== Inbox Message ==========

struct InboxMessage {
	std::string msgId;
	std::string type;
	nlohmann::json payload;
	uint64_t timestamp;
	bool acked = false;

	InboxMessage() : timestamp(0) {}
	InboxMessage(const std::string& id, const std::string& t, const nlohmann::json& p, uint64_t ts)
		: msgId(id), type(t), payload(p), timestamp(ts), acked(false) {}
};

// ========== Inbox Client ==========

class InboxClient {
public:
	struct Config {
		std::string agentId;
		int heartbeatIntervalMs = 30000;      // 心跳间隔
		int consumeTimeoutMs = 5000;          // consume 超时
		int reconnectDelayMs = 5000;         // 重连延迟
		int maxPendingMessages = 100;        // 最大待处理消息数
	};

	InboxClient(const std::string& id, const Config& config)
		: clientId_(id), config_(config), running_(false), connected_(false),
		  client_(wingman::transport::createTcpClient()), nextSeq_(1) {}

	~InboxClient() {
		stop();
	}

	bool connect(const std::string& host, int port) {
		std::lock_guard<std::mutex> lock(clientMutex_);

		if (running_.load()) {
			spdlog::warn("[Inbox] Already connected");
			return true;
		}

		// 设置消息处理器
		client_->setMessageHandler([this](const wingman::transport::MessagePtr& msg) {
			handleMessage(msg);
		});

		if (!client_->connect(host, port)) {
			spdlog::error("[Inbox] Failed to connect to {}:{}", host, port);
			return false;
		}

		// 发送注册消息
		if (!sendRegister()) {
			client_->disconnect();
			return false;
		}

		running_.store(true);
		connected_.store(true);
		heartbeatThread_ = std::thread(&InboxClient::heartbeatLoop, this);

		spdlog::info("[Inbox] Connected to {}:{}", host, port);
		return true;
	}

	void stop() {
		if (!running_.load()) return;

		running_.store(false);
		heartbeatCond_.notify_all();

		if (heartbeatThread_.joinable()) {
			heartbeatThread_.join();
		}

		std::lock_guard<std::mutex> lock(clientMutex_);
		client_->disconnect();
		connected_.store(false);
	}

	bool isConnected() const {
		return connected_.load() && running_.load();
	}

	// consume - 拉取消息（阻塞直到有消息或超时）
	InboxMessage consume(int timeoutMs) {
		std::unique_lock<std::mutex> lock(queueMutex_);

		auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

		// 等待消息或超时
		messageCond_.wait_until(lock, deadline, [this] {
			return !messageQueue_.empty() || !running_.load();
		});

		if (messageQueue_.empty() || !running_.load()) {
			return InboxMessage(); // 空消息表示超时或已停止
		}

		InboxMessage msg = std::move(messageQueue_.front());
		messageQueue_.pop();

		// 跟踪待处理消息
		pendingMessages_[msg.msgId] = msg;

		return msg;
	}

	// ack - 确认收到消息
	bool ack(const std::string& msgId) {
		{
			std::lock_guard<std::mutex> lock(queueMutex_);
			auto it = pendingMessages_.find(msgId);
			if (it != pendingMessages_.end()) {
				it->second.acked = true;
			}
		}

		return sendAck(msgId);
	}

	// report - 上报任务完成
	bool report(const std::string& msgId, const nlohmann::json& result) {
		{
			std::lock_guard<std::mutex> lock(queueMutex_);
			pendingMessages_.erase(msgId);
		}

		return sendReport(msgId, result);
	}

	// heartbeat - 发送心跳
	bool heartbeat() {
		return sendHeartbeat();
	}

	// 获取待处理消息数
	size_t getPendingCount() const {
		std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(queueMutex_));
		return pendingMessages_.size();
	}

	// 设置消息回调（用于事件通知）
	void setMessageCallback(std::function<void(const InboxMessage&)> callback) {
		std::lock_guard<std::mutex> lock(queueMutex_);
		messageCallback_ = callback;
	}

private:
	bool sendRegister() {
		nlohmann::json msg;
		msg["type"] = "inbox.register";
		msg["agentId"] = config_.agentId;

		return sendNotify(msg);
	}

	bool sendHeartbeat() {
		nlohmann::json msg;
		msg["type"] = "inbox.heartbeat";
		msg["agentId"] = config_.agentId;
		msg["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();

		return sendNotify(msg);
	}

	bool sendAck(const std::string& msgId) {
		nlohmann::json msg;
		msg["type"] = "inbox.ack";
		msg["msgId"] = msgId;
		msg["agentId"] = config_.agentId;

		return sendNotify(msg);
	}

	bool sendReport(const std::string& msgId, const nlohmann::json& result) {
		nlohmann::json msg;
		msg["type"] = "inbox.report";
		msg["msgId"] = msgId;
		msg["agentId"] = config_.agentId;
		msg["result"] = result;
		msg["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();

		return sendNotify(msg);
	}

	bool sendNotify(const nlohmann::json& data) {
		std::string body = data.dump();
		auto message = wingman::transport::Message::create(
			wingman::transport::MessageType::Notify, body);

		std::lock_guard<std::mutex> lock(clientMutex_);
		if (!connected_.load()) {
			return false;
		}
		return client_->send(message);
	}

	void handleMessage(const wingman::transport::MessagePtr& msg) {
		try {
			nlohmann::json data = nlohmann::json::parse(msg->body);
			std::string type = data.value("type", "");

			if (type == "inbox.message") {
				handleInboxMessage(data);
			} else if (type == "inbox.heartbeat_ack") {
				// 心跳确认，更新最后心跳时间
				std::lock_guard<std::mutex> lock(queueMutex_);
				lastHeartbeatTime_ = std::chrono::steady_clock::now();
			} else if (type == "inbox.register_ack") {
				spdlog::info("[Inbox] Registration acknowledged");
			}
		} catch (const std::exception& e) {
			spdlog::error("[Inbox] Message parse error: {}", e.what());
		}
	}

	void handleInboxMessage(const nlohmann::json& data) {
		std::string msgId = data.value("msgId", "");
		std::string messageType = data.value("messageType", "");
		auto payload = data.value("payload", nlohmann::json::object());
		uint64_t timestamp = data.value("timestamp", uint64_t(0));

		if (msgId.empty()) {
			spdlog::warn("[Inbox] Received message without msgId");
			return;
		}

		std::lock_guard<std::mutex> lock(queueMutex_);

		// 检查待处理消息数
		if (pendingMessages_.size() >= static_cast<size_t>(config_.maxPendingMessages)) {
			spdlog::warn("[Inbox] Too many pending messages, dropping: {}", msgId);
			return;
		}

		// 加入队列
		messageQueue_.push(InboxMessage(msgId, messageType, payload, timestamp));
		messageCond_.notify_one();

		// 调用回调（如果设置了）
		if (messageCallback_) {
			messageCallback_(messageQueue_.back());
		}

		spdlog::debug("[Inbox] Received message: {} (type: {})", msgId, messageType);
	}

	void heartbeatLoop() {
		while (running_.load()) {
			auto nextHeartbeat = std::chrono::steady_clock::now() + std::chrono::milliseconds(config_.heartbeatIntervalMs);

			std::unique_lock<std::mutex> lock(queueMutex_);
			heartbeatCond_.wait_until(lock, nextHeartbeat, [this] {
				return !running_.load();
			});

			if (!running_.load()) break;

			lock.unlock();

			if (connected_.load()) {
				sendHeartbeat();
			}
		}
	}

	std::string clientId_;
	Config config_;
	std::atomic<bool> running_;
	std::atomic<bool> connected_;
	std::unique_ptr<wingman::transport::TransportClient> client_;

	// 消息队列
	std::queue<InboxMessage> messageQueue_;
	std::unordered_map<std::string, InboxMessage> pendingMessages_;
	mutable std::mutex queueMutex_;
	std::condition_variable messageCond_;

	// 心跳线程
	std::thread heartbeatThread_;
	std::condition_variable heartbeatCond_;
	std::chrono::steady_clock::time_point lastHeartbeatTime_;

	// 消息回调
	std::function<void(const InboxMessage&)> messageCallback_;

	// 客户端 mutex
	mutable std::mutex clientMutex_;

	// 序列号
	uint32_t nextSeq_;
};

// ========== Inbox Manager ==========

class InboxManager {
public:
	static InboxManager& instance() {
		static InboxManager inst;
		return inst;
	}

	int createClient(const std::string& id, const InboxClient::Config& config) {
		std::lock_guard<std::mutex> lock(mutex_);

		// 检查是否已存在
		for (const auto& [handle, client] : clients_) {
			if (client && client->clientId_ == id) {
				spdlog::warn("[Inbox] Client '{}' already exists", id);
				return handle;
			}
		}

		auto client = std::make_unique<InboxClient>(id, config);
		int handle = nextHandle_++;
		clients_[handle] = std::move(client);
		return handle;
	}

	InboxClient* getClient(int handle) {
		std::lock_guard<std::mutex> lock(mutex_);
		auto it = clients_.find(handle);
		return it != clients_.end() ? it->second.get() : nullptr;
	}

	void removeClient(int handle) {
		std::lock_guard<std::mutex> lock(mutex_);
		auto it = clients_.find(handle);
		if (it != clients_.end()) {
			it->second->stop();
			clients_.erase(it);
		}
	}

private:
	mutable std::mutex mutex_;
	std::unordered_map<int, std::unique_ptr<InboxClient>> clients_;
	int nextHandle_ = 1;
};

#endif // WINGMAN_HAS_TRANSPORT

// ========== Module Definition ==========

ModuleDescriptor createInboxModule() {
	ModuleDescriptor mod;
	mod.name = "inbox";

#ifdef WINGMAN_HAS_TRANSPORT

	// connect(url, config?) -> {success, handle, error?}
	mod.functions.push_back({"connect", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty()) return ScriptValue::fromObject({
			{"success", ScriptValue::fromBool(false)},
			{"error", ScriptValue::fromString("URL required")}
		});

		std::string url = args[0].asString();

		// 解析 URL: tcp://host:port
		std::string host = "127.0.0.1";
		int port = 8080;

		if (url.find("tcp://") == 0) {
			size_t hostStart = 6; // Skip "tcp://"
			size_t portStart = url.find(':', hostStart);
			if (portStart != std::string::npos) {
				host = url.substr(hostStart, portStart - hostStart);
				port = std::stoi(url.substr(portStart + 1));
			} else {
				host = url.substr(hostStart);
			}
		} else {
			return ScriptValue::fromObject({
				{"success", ScriptValue::fromBool(false)},
				{"error", ScriptValue::fromString("Only tcp:// URLs supported")}
			});
		}

		// 解析配置
		InboxClient::Config config;
		config.agentId = "runtime_" + std::to_string(std::hash<std::string>{}(url));

		if (args.size() > 1 && args[1].isObject()) {
			if (auto* v = args[1].get("agentId")) config.agentId = v->asString();
			if (auto* v = args[1].get("heartbeatInterval")) config.heartbeatIntervalMs = static_cast<int>(v->asInt());
			if (auto* v = args[1].get("consumeTimeout")) config.consumeTimeoutMs = static_cast<int>(v->asInt());
			if (auto* v = args[1].get("maxPendingMessages")) config.maxPendingMessages = static_cast<int>(v->asInt());
		}

		auto& manager = InboxManager::instance();
		int handle = manager.createClient("default", config);

		auto* client = manager.getClient(handle);
		if (!client || !client->connect(host, port)) {
			manager.removeClient(handle);
			return ScriptValue::fromObject({
				{"success", ScriptValue::fromBool(false)},
				{"error", ScriptValue::fromString("Failed to connect")}
			});
		}

		return ScriptValue::fromObject({
			{"success", ScriptValue::fromBool(true)},
			{"handle", ScriptValue::fromInt(handle)}
		});
	}, "url:string, config?:object -> {success,handle,error?}"});

	// consume(handle?, timeout?) -> {msgId, type, payload, timestamp}
	mod.functions.push_back({"consume", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int handle = 1; // 默认 handle
		int timeout = 5000; // 默认 5 秒超时

		if (args.size() > 0 && !args[0].isNull()) handle = static_cast<int>(args[0].asInt());
		if (args.size() > 1 && !args[1].isNull()) timeout = static_cast<int>(args[1].asInt());

		auto& manager = InboxManager::instance();
		auto* client = manager.getClient(handle);
		if (!client || !client->isConnected()) {
			return ScriptValue::fromObject({
				{"success", ScriptValue::fromBool(false)},
				{"error", ScriptValue::fromString("Not connected")}
			});
		}

		InboxMessage msg = client->consume(timeout);
		if (msg.msgId.empty()) {
			return ScriptValue::null(); // 超时
		}

		// 转换 payload
		ScriptValue payload;
		if (msg.payload.is_null()) {
			payload = ScriptValue::null();
		} else if (msg.payload.is_boolean()) {
			payload = ScriptValue::fromBool(msg.payload.get<bool>());
		} else if (msg.payload.is_number()) {
			payload = ScriptValue::fromFloat(msg.payload.get<double>());
		} else if (msg.payload.is_string()) {
			payload = ScriptValue::fromString(msg.payload.get<std::string>());
		} else if (msg.payload.is_array()) {
			std::vector<ScriptValue> arr;
			for (const auto& item : msg.payload) {
				arr.push_back(ScriptValue::fromString(item.dump()));
			}
			payload = ScriptValue::fromArray(arr);
		} else if (msg.payload.is_object()) {
			std::unordered_map<std::string, ScriptValue> obj;
			for (auto it = msg.payload.begin(); it != msg.payload.end(); ++it) {
				obj[it.key()] = ScriptValue::fromString(it.value().dump());
			}
			payload = ScriptValue::fromObject(obj);
		}

		return ScriptValue::fromObject({
			{"msgId", ScriptValue::fromString(msg.msgId)},
			{"type", ScriptValue::fromString(msg.type)},
			{"payload", payload},
			{"timestamp", ScriptValue::fromInt(static_cast<int64_t>(msg.timestamp))}
		});
	}, "handle?:int, timeout?:int -> {msgId,type,payload,timestamp}|null"});

	// ack(handle, msgId) -> bool
	mod.functions.push_back({"ack", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.size() < 2) return ScriptValue::fromBool(false);

		int handle = static_cast<int>(args[0].asInt());
		std::string msgId = args[1].asString();

		auto& manager = InboxManager::instance();
		auto* client = manager.getClient(handle);
		if (!client) return ScriptValue::fromBool(false);

		return ScriptValue::fromBool(client->ack(msgId));
	}, "handle:int, msgId:string -> bool"});

	// report(handle, msgId, result) -> bool
	mod.functions.push_back({"report", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.size() < 2) return ScriptValue::fromBool(false);

		int handle = static_cast<int>(args[0].asInt());
		std::string msgId = args[1].asString();

		nlohmann::json result;
		if (args.size() > 2 && !args[2].isNull()) {
			// 简化处理：将 ScriptValue 转为 JSON 字符串再解析
			std::string resultStr;
			if (args[2].isString()) {
				resultStr = args[2].asString();
				try {
					result = nlohmann::json::parse(resultStr);
				} catch (...) {
					result = resultStr;
				}
			} else {
				resultStr = "{}"; // 简化处理
			}
		}

		auto& manager = InboxManager::instance();
		auto* client = manager.getClient(handle);
		if (!client) return ScriptValue::fromBool(false);

		return ScriptValue::fromBool(client->report(msgId, result));
	}, "handle:int, msgId:string, result?:object -> bool"});

	// heartbeat(handle?) -> bool
	mod.functions.push_back({"heartbeat", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int handle = 1; // 默认 handle

		if (args.size() > 0 && !args[0].isNull()) handle = static_cast<int>(args[0].asInt());

		auto& manager = InboxManager::instance();
		auto* client = manager.getClient(handle);
		if (!client) return ScriptValue::fromBool(false);

		return ScriptValue::fromBool(client->heartbeat());
	}, "handle?:int -> bool"});

	// disconnect(handle?) -> bool
	mod.functions.push_back({"disconnect", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int handle = 1; // 默认 handle

		if (args.size() > 0 && !args[0].isNull()) handle = static_cast<int>(args[0].asInt());

		auto& manager = InboxManager::instance();
		manager.removeClient(handle);
		return ScriptValue::fromBool(true);
	}, "handle?:int -> bool"});

	// isConnected(handle?) -> bool
	mod.functions.push_back({"isConnected", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int handle = 1; // 默认 handle

		if (args.size() > 0 && !args[0].isNull()) handle = static_cast<int>(args[0].asInt());

		auto& manager = InboxManager::instance();
		auto* client = manager.getClient(handle);
		return ScriptValue::fromBool(client ? client->isConnected() : false);
	}, "handle?:int -> bool"});

#else
	// 当 transport 不可用时，提供错误信息
	mod.functions.push_back({"connect", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::fromObject({
			{"success", ScriptValue::fromBool(false)},
			{"error", ScriptValue::fromString("Inbox module requires transport support")}
		});
	}, "url:string, config?:object -> {success,handle,error?}"});
#endif

	return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
