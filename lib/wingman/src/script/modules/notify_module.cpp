#include "wingman/event.hpp"
#include "wingman/http.hpp"
#include "wingman/script/iscript_engine.hpp"

#include <nlohmann/json.hpp>
#include <unordered_map>
#include <vector>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <queue>
#include <atomic>

namespace wingman {
namespace script {
namespace modules {

namespace {

// HTTP webhook sender
class WebhookSender {
public:
	static constexpr size_t MAX_CONCURRENT_WEBHOOKS = 10;

	WebhookSender() : shutdown_(false), webhooksEnabled_(true) {}

	~WebhookSender() {
		shutdown();
	}

	void shutdown() {
		{
			std::lock_guard<std::mutex> lock(mutex_);
			shutdown_ = true;
		}

		// Wait for all worker threads to finish
		for (auto& worker : workers_) {
			if (worker.joinable()) {
				worker.join();
			}
		}
		workers_.clear();
	}

	void setWebhooksEnabled(bool enabled) {
		std::lock_guard<std::mutex> lock(mutex_);
		webhooksEnabled_ = enabled;
	}

	void setAllowedHosts(const std::vector<std::string>& hosts) {
		std::lock_guard<std::mutex> lock(mutex_);
		allowedHosts_ = hosts;
	}

	bool isUrlAllowed(const std::string& url) const {
		// If webhooks are disabled, reject all
		if (!webhooksEnabled_) {
			return false;
		}

		// If no whitelist configured, reject all for security (SSRF protection)
		if (allowedHosts_.empty()) {
			return false;
		}

		// Parse URL to extract hostname
		std::string host;
		size_t schemeEnd = url.find("://");
		if (schemeEnd != std::string::npos) {
			size_t hostStart = schemeEnd + 3;
			size_t pathStart = url.find('/', hostStart);
			size_t portStart = url.find(':', hostStart);

			if (portStart != std::string::npos && portStart < pathStart) {
				host = url.substr(hostStart, portStart - hostStart);
			} else if (pathStart != std::string::npos) {
				host = url.substr(hostStart, pathStart - hostStart);
			} else {
				host = url.substr(hostStart);
			}
		}

		// Check if host is in whitelist
		for (const auto& allowed : allowedHosts_) {
			if (host == allowed) {
				return true;
			}
		}

		return false;
	}

	void send(const std::string& url, const nlohmann::json& payload, const std::function<void(bool, std::string)>& callback) {
		{
			std::lock_guard<std::mutex> lock(mutex_);
			if (shutdown_) {
				return; // Reject if shutdown started
			}

			// Validate URL against whitelist (SSRF protection)
			if (!isUrlAllowed(url)) {
				// Emit blocked event
				EventHub::instance().emit("notify.webhook.blocked", {
					{"url", url},
					{"reason", "URL not in whitelist or webhooks disabled"}
				}, "notify");
				callback(false, "Webhook URL is not allowed");
				return;
			}

			// Enforce maximum concurrent webhook limit
			if (activeThreadCount_.load() >= MAX_CONCURRENT_WEBHOOKS) {
				EventHub::instance().emit("notify.webhook.blocked", {
					{"url", url},
					{"reason", "Maximum concurrent webhook limit reached"}
				}, "notify");
				callback(false, "Maximum concurrent webhook limit reached");
				return;
			}

			// Increment active thread counter
			activeThreadCount_++;
		}

		// Emit pending event
		EventHub::instance().emit("notify.webhook.pending", {
			{"url", url},
			{"payload", payload}
		}, "notify");

		// Send HTTP request asynchronously using managed thread
		workers_.emplace_back([this, url, payload, callback]() {
			try {
				HttpClient client;
				std::string jsonBody = payload.dump();
				HttpResponse response = client.post(url, jsonBody, {});

				bool success = response.isSuccess();
				std::string error = success ? "" : response.error;

				// Emit result event
				EventHub::instance().emit(success ? "notify.webhook.success" : "notify.webhook.failed", {
					{"url", url},
					{"statusCode", response.statusCode},
					{"error", error}
				}, "notify");

				callback(success, error);
			} catch (const std::exception& e) {
				EventHub::instance().emit("notify.webhook.failed", {
					{"url", url},
					{"error", e.what()}
				}, "notify");
				callback(false, e.what());
			}

			// Decrement active thread counter when done
			activeThreadCount_--;
		});
	}

private:
	void cleanupFinishedThreads() {
		// Join finished threads and remove them from the workers vector
		// Note: join() blocks if thread is still running, so we only join threads that have finished
		// This is a best-effort cleanup - the vector might still contain running threads
		workers_.erase(
			std::remove_if(workers_.begin(), workers_.end(),
				[](std::thread& t) {
					// If thread is joinable and has finished, join() returns quickly
					// We don't have a portable try_join, so we use a heuristic
					return !t.joinable();  // Remove non-joinable threads
				}),
			workers_.end());
	}

private:
	std::vector<std::thread> workers_;
	std::mutex mutex_;
	std::atomic<bool> shutdown_;
	std::atomic<bool> webhooksEnabled_;
	std::vector<std::string> allowedHosts_;
	std::atomic<size_t> activeThreadCount_{0};
};

// Global webhook sender instance
WebhookSender g_webhookSender;

// Notification manager
class NotifyManager {
public:
	void debug(const std::string& message, const nlohmann::json& meta) {
		log("DEBUG", message, meta);
	}

	void info(const std::string& message, const nlohmann::json& meta) {
		log("INFO", message, meta);
	}

	void warn(const std::string& message, const nlohmann::json& meta) {
		log("WARN", message, meta);
	}

	void error(const std::string& message, const nlohmann::json& meta) {
		log("ERROR", message, meta);
	}

	void toast(const std::string& title, const std::string& message, const std::string& level) {
		std::lock_guard<std::mutex> lock(mutex_);
		// Emit toast event
		EventHub::instance().emit("notify.toast", {
			{"title", title},
			{"message", message},
			{"level", level}
		}, "notify");
	}

	void webhook(const std::string& url, const nlohmann::json& payload, const nlohmann::json& /*options*/) {
		// Send webhook asynchronously
		g_webhookSender.send(url, payload, [url](bool success, std::string error) {
			if (!success) {
				EventHub::instance().emit("notify.failed", {
					{"type", "webhook"},
					{"url", url},
					{"error", error}
				}, "notify");
			}
		});
	}

	void bridge(const std::string& eventName, const std::string& target, const nlohmann::json& options) {
		std::lock_guard<std::mutex> lock(mutex_);

		// Create a bridge subscription (exact event name match only, not pattern)
		uint64_t subId = EventHub::instance().subscribe(eventName, [target, options](const EventMessage& msg) {
			// Forward to target (could be another event, webhook, etc.)
			nlohmann::json payload = options.value("transform", nlohmann::json::object());
			payload["original"] = msg.payload;

			if (target.rfind("http://", 0) == 0 || target.rfind("https://", 0) == 0) {
				// Target is a webhook URL
				g_webhookSender.send(target, payload, [](bool, std::string) {});
			} else if (target.rfind("event://", 0) == 0) {
				// Target is another event
				std::string targetEvent = target.substr(8);
				EventHub::instance().emit(targetEvent, payload, "notify.bridge");
			}
		}, "notify.bridge." + target, false);

		bridges_[subId] = {eventName, target};
	}

	void clearBridges() {
		std::lock_guard<std::mutex> lock(mutex_);
		for (const auto& [id, _] : bridges_) {
			EventHub::instance().unsubscribe(id);
		}
		bridges_.clear();
	}

private:
	void log(const std::string& level, const std::string& message, const nlohmann::json& meta) {
		std::lock_guard<std::mutex> lock(mutex_);
		nlohmann::json payload = {
			{"level", level},
			{"message", message},
			{"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::system_clock::now().time_since_epoch()).count()}
		};
		if (!meta.is_null() && !meta.empty()) {
			payload["meta"] = meta;
		}

		// Emit log event
		EventHub::instance().emit("notify.log", payload, "notify");

		// Also emit level-specific event
		EventHub::instance().emit("notify.log." + level, payload, "notify");
	}

	struct BridgeInfo {
		std::string eventName;  // Exact event name (not pattern)
		std::string target;
	};

	std::unordered_map<uint64_t, BridgeInfo> bridges_;
	std::mutex mutex_;
};

// Global notify manager
NotifyManager g_notifyManager;

// Helper to convert ScriptValue to JSON
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
	default:
		return nullptr;
	}
}

} // namespace

ModuleDescriptor createNotifyModule() {
	ModuleDescriptor mod;
	mod.name = "notify";

	// debug(message, meta?)
	mod.functions.push_back({"debug", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty() || !args[0].isString()) return ScriptValue::null();

		std::string message = args[0].asString();
		nlohmann::json meta = args.size() > 1 ? toJson(args[1]) : nlohmann::json::object();

		g_notifyManager.debug(message, meta);
		return ScriptValue::null();
	}, "message:string, meta?:object -> nil"});

	// info(message, meta?)
	mod.functions.push_back({"info", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty() || !args[0].isString()) return ScriptValue::null();

		std::string message = args[0].asString();
		nlohmann::json meta = args.size() > 1 ? toJson(args[1]) : nlohmann::json::object();

		g_notifyManager.info(message, meta);
		return ScriptValue::null();
	}, "message:string, meta?:object -> nil"});

	// warn(message, meta?)
	mod.functions.push_back({"warn", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty() || !args[0].isString()) return ScriptValue::null();

		std::string message = args[0].asString();
		nlohmann::json meta = args.size() > 1 ? toJson(args[1]) : nlohmann::json::object();

		g_notifyManager.warn(message, meta);
		return ScriptValue::null();
	}, "message:string, meta?:object -> nil"});

	// error(message, meta?)
	mod.functions.push_back({"error", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty() || !args[0].isString()) return ScriptValue::null();

		std::string message = args[0].asString();
		nlohmann::json meta = args.size() > 1 ? toJson(args[1]) : nlohmann::json::object();

		g_notifyManager.error(message, meta);
		return ScriptValue::null();
	}, "message:string, meta?:object -> nil"});

	// toast(title, message, level?)
	mod.functions.push_back({"toast", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.size() < 2 || !args[0].isString() || !args[1].isString()) {
			return ScriptValue::null();
		}

		std::string title = args[0].asString();
		std::string message = args[1].asString();
		std::string level = args.size() > 2 && args[2].isString() ? args[2].asString() : "info";

		g_notifyManager.toast(title, message, level);
		return ScriptValue::null();
	}, "title:string, message:string, level?:string -> nil"});

	// webhook(url, payload, options?)
	mod.functions.push_back({"webhook", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty() || !args[0].isString()) return ScriptValue::null();

		std::string url = args[0].asString();
		nlohmann::json payload = args.size() > 1 ? toJson(args[1]) : nlohmann::json::object();
		nlohmann::json options = args.size() > 2 ? toJson(args[2]) : nlohmann::json::object();

		g_notifyManager.webhook(url, payload, options);
		return ScriptValue::null();
	}, "url:string, payload?:object, options?:object -> nil"});

	// bridge(eventName, target, options?) - exact event name match (wildcards not supported)
	// To listen to multiple events, call bridge() multiple times with exact event names
	mod.functions.push_back({"bridge", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.size() < 2 || !args[0].isString() || !args[1].isString()) {
			return ScriptValue::null();
		}

		std::string eventName = args[0].asString();
		std::string target = args[1].asString();
		nlohmann::json options = args.size() > 2 ? toJson(args[2]) : nlohmann::json::object();

		g_notifyManager.bridge(eventName, target, options);
		return ScriptValue::null();
	}, "eventName:string, target:string, options?:object -> nil"});

	return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
