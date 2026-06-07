#include "wingman/script/iscript_engine.hpp"
#include "wingman/event.hpp"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <unordered_map>
#include <string>
#include <vector>
#include <functional>
#include <mutex>

namespace wingman {
namespace script {
namespace modules {

// ========== Team Module (纯本地，通过 inbox 与服务器通信) ==========

// Team 状态
struct TeamStatus {
	std::string teamId;
	std::string leaderId;
	std::vector<std::string> members;
	std::string state; // "idle", "voting", "working"
	uint64_t lastUpdate;

	TeamStatus() : lastUpdate(0) {}
	TeamStatus(const std::string& id) : teamId(id), state("idle"), lastUpdate(0) {}
};

// 投票信息
struct VoteInfo {
	std::string voteId;
	std::string teamId;
	std::string proposerId;
	std::string subject;
	std::unordered_map<std::string, std::string> responses; // memberId -> response
	uint64_t deadline;
	bool active;

	VoteInfo() : deadline(0), active(false) {}
};

// Team Client (本地状态管理 + 通过 inbox 发送消息)
class TeamClient {
public:
	TeamClient(const std::string& id) : clientId_(id), joined_(false) {}

	// 加入队伍
	bool joinTeam(const std::string& teamId, const std::string& memberId) {
		std::lock_guard<std::mutex> lock(mutex_);

		if (joined_) {
			spdlog::warn("[Team] Already joined a team");
			return false;
		}

		// 通过 inbox 发送加入请求
		nlohmann::json msg;
		msg["type"] = "team.join";
		msg["teamId"] = teamId;
		msg["memberId"] = memberId;
		msg["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();

		// 这里假设通过事件系统发送到 inbox
		EventHub::instance().emit("team.outbound", msg);

		// 更新本地状态（乐观更新）
		currentTeam_.teamId = teamId;
		currentTeam_.members.push_back(memberId);
		memberId_ = memberId;
		joined_ = true;

		spdlog::info("[Team] Joining team {} as {}", teamId, memberId);
		return true;
	}

	// 离开队伍
	bool leaveTeam() {
		std::lock_guard<std::mutex> lock(mutex_);

		if (!joined_) {
			return false;
		}

		nlohmann::json msg;
		msg["type"] = "team.leave";
		msg["teamId"] = currentTeam_.teamId;
		msg["memberId"] = memberId_;
		msg["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();

		EventHub::instance().emit("team.outbound", msg);

		// 清空本地状态
		currentTeam_ = TeamStatus();
		memberId_.clear();
		joined_ = false;

		spdlog::info("[Team] Left team {}", currentTeam_.teamId);
		return true;
	}

	// 发起投票
	bool createVote(const std::string& subject, uint64_t timeoutMs) {
		std::lock_guard<std::mutex> lock(mutex_);

		if (!joined_) {
			spdlog::warn("[Team] Not in a team");
			return false;
		}

		nlohmann::json msg;
		msg["type"] = "team.vote_create";
		msg["teamId"] = currentTeam_.teamId;
		msg["proposerId"] = memberId_;
		msg["subject"] = subject;
		msg["timeout"] = timeoutMs;
		msg["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();

		EventHub::instance().emit("team.outbound", msg);

		spdlog::info("[Team] Created vote: {} in team {}", subject, currentTeam_.teamId);
		return true;
	}

	// 投票
	bool castVote(const std::string& voteId, const std::string& response) {
		std::lock_guard<std::mutex> lock(mutex_);

		if (!joined_) {
			return false;
		}

		nlohmann::json msg;
		msg["type"] = "team.vote_cast";
		msg["teamId"] = currentTeam_.teamId;
		msg["voteId"] = voteId;
		msg["memberId"] = memberId_;
		msg["response"] = response;
		msg["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();

		EventHub::instance().emit("team.outbound", msg);

		spdlog::debug("[Team] Cast vote for {}", voteId);
		return true;
	}

	// 获取投票结果
	nlohmann::json getVoteResult(const std::string& voteId) {
		std::lock_guard<std::mutex> lock(mutex_);

		auto it = votes_.find(voteId);
		if (it == votes_.end()) {
			return nlohmann::json{{"error", "Vote not found"}};
		}

		const VoteInfo& vote = it->second;

		nlohmann::json result;
		result["voteId"] = vote.voteId;
		result["teamId"] = vote.teamId;
		result["subject"] = vote.subject;
		result["active"] = vote.active;
		result["deadline"] = vote.deadline;

		// 统计投票结果
		nlohmann::json responses;
		for (const auto& [member, resp] : vote.responses) {
			responses[member] = resp;
		}
		result["responses"] = responses;

		return result;
	}

	// 上报状态
	bool reportStatus(const nlohmann::json& status) {
		std::lock_guard<std::mutex> lock(mutex_);

		if (!joined_) {
			return false;
		}

		nlohmann::json msg;
		msg["type"] = "team.status_report";
		msg["teamId"] = currentTeam_.teamId;
		msg["memberId"] = memberId_;
		msg["status"] = status;
		msg["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();

		EventHub::instance().emit("team.outbound", msg);

		return true;
	}

	// broadcast - 向所有队友广播消息
	bool broadcast(const nlohmann::json& message) {
		std::lock_guard<std::mutex> lock(mutex_);

		if (!joined_) {
			return false;
		}

		nlohmann::json msg;
		msg["type"] = "team.broadcast";
		msg["teamId"] = currentTeam_.teamId;
		msg["memberId"] = memberId_;
		msg["message"] = message;
		msg["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();

		EventHub::instance().emit("team.outbound", msg);

		spdlog::debug("[Team] Broadcast message to team {}", currentTeam_.teamId);
		return true;
	}

	// 获取队伍状态
	TeamStatus getTeamStatus() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return currentTeam_;
	}

	// 是否已加入队伍
	bool isJoined() const {
		return joined_;
	}

	// 获取当前成员 ID
	std::string getMemberId() const {
		return memberId_;
	}

	// 处理来自服务器的消息
	void handleServerMessage(const nlohmann::json& msg) {
		std::string type = msg.value("type", "");

		std::lock_guard<std::mutex> lock(mutex_);

		if (type == "team.joined") {
			currentTeam_.teamId = msg.value("teamId", "");
			currentTeam_.leaderId = msg.value("leaderId", "");
			if (msg.contains("members")) {
				currentTeam_.members = msg["members"].get<std::vector<std::string>>();
			}
			joined_ = true;
			spdlog::info("[Team] Joined team {}", currentTeam_.teamId);
		}
		else if (type == "team.left") {
			currentTeam_ = TeamStatus();
			memberId_.clear();
			joined_ = false;
			spdlog::info("[Team] Left team");
		}
		else if (type == "team.member_joined") {
			std::string newMember = msg.value("memberId", "");
			currentTeam_.members.push_back(newMember);
			spdlog::info("[Team] Member {} joined team {}", newMember, currentTeam_.teamId);
		}
		else if (type == "team.member_left") {
			std::string leavingMember = msg.value("memberId", "");
			auto it = std::find(currentTeam_.members.begin(), currentTeam_.members.end(), leavingMember);
			if (it != currentTeam_.members.end()) {
				currentTeam_.members.erase(it);
			}
			spdlog::info("[Team] Member {} left team {}", leavingMember, currentTeam_.teamId);
		}
		else if (type == "team.vote_started") {
			VoteInfo vote;
			vote.voteId = msg.value("voteId", "");
			vote.teamId = msg.value("teamId", "");
			vote.proposerId = msg.value("proposerId", "");
			vote.subject = msg.value("subject", "");
			vote.deadline = msg.value("deadline", uint64_t(0));
			vote.active = true;
			votes_[vote.voteId] = vote;

			// 触发本地事件
			EventHub::instance().emit("team.vote_started", msg);

			spdlog::info("[Team] Vote started: {}", vote.voteId);
		}
		else if (type == "team.vote_ended") {
			std::string voteId = msg.value("voteId", "");
			auto it = votes_.find(voteId);
			if (it != votes_.end()) {
				it->second.active = false;

				// 更新投票结果
				if (msg.contains("result")) {
					auto& responses = msg["result"];
					if (responses.is_object()) {
						for (auto respIt = responses.begin(); respIt != responses.end(); ++respIt) {
							it->second.responses[respIt.key()] = respIt.value().get<std::string>();
						}
					}
				}
			}

			// 触发本地事件
			EventHub::instance().emit("team.vote_ended", msg);

			spdlog::info("[Team] Vote ended: {}", voteId);
		}
		else if (type == "team.broadcast_received") {
			std::string senderId = msg.value("senderId", "");
			auto message = msg.value("message", nlohmann::json::object());

			spdlog::info("[Team] Received broadcast from {} in team {}", senderId, currentTeam_.teamId);

			// 触发本地事件
			EventHub::instance().emit("team.broadcast_received", msg);
		}
	}

private:
	std::string clientId_;
	std::string memberId_;
	bool joined_;
	TeamStatus currentTeam_;
	std::unordered_map<std::string, VoteInfo> votes_;
	mutable std::mutex mutex_;
};

// ========== Team Manager ==========

class TeamManager {
public:
	static TeamManager& instance() {
		static TeamManager inst;
		return inst;
	}

	int createClient(const std::string& id) {
		std::lock_guard<std::mutex> lock(mutex_);

		auto client = std::make_unique<TeamClient>(id);
		int handle = nextHandle_++;
		clients_[handle] = std::move(client);
		return handle;
	}

	TeamClient* getClient(int handle) {
		std::lock_guard<std::mutex> lock(mutex_);
		auto it = clients_.find(handle);
		return it != clients_.end() ? it->second.get() : nullptr;
	}

	void removeClient(int handle) {
		std::lock_guard<std::mutex> lock(mutex_);
		clients_.erase(handle);
	}

private:
	mutable std::mutex mutex_;
	std::unordered_map<int, std::unique_ptr<TeamClient>> clients_;
	int nextHandle_ = 1;
};

// ========== Module Definition ==========

ModuleDescriptor createTeamModule() {
	ModuleDescriptor mod;
	mod.name = "team";

	// joinTeam(teamId, memberId?) -> bool
	mod.functions.push_back({"joinTeam", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty()) return ScriptValue::fromBool(false);

		std::string teamId = args[0].asString();
		std::string memberId = "member_" + std::to_string(std::hash<std::string>{}(teamId));

		if (args.size() > 1 && args[1].isString()) {
			memberId = args[1].asString();
		}

		auto& manager = TeamManager::instance();
		int handle = manager.createClient("default");
		auto* client = manager.getClient(handle);

		return ScriptValue::fromBool(client ? client->joinTeam(teamId, memberId) : false);
	}, "teamId:string, memberId?:string -> bool"});

	// leaveTeam() -> bool
	mod.functions.push_back({"leaveTeam", [](const std::vector<ScriptValue>&) -> ScriptValue {
		auto& manager = TeamManager::instance();
		auto* client = manager.getClient(1); // 默认 handle
		return ScriptValue::fromBool(client ? client->leaveTeam() : false);
	}, "() -> bool"});

	// createVote(subject, timeout?) -> bool
	mod.functions.push_back({"createVote", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty()) return ScriptValue::fromBool(false);

		std::string subject = args[0].asString();
		uint64_t timeout = 30000; // 默认 30 秒

		if (args.size() > 1 && args[1].isInt()) {
			timeout = static_cast<uint64_t>(args[1].asInt());
		}

		auto& manager = TeamManager::instance();
		auto* client = manager.getClient(1); // 默认 handle
		return ScriptValue::fromBool(client ? client->createVote(subject, timeout) : false);
	}, "subject:string, timeout?:number -> bool"});

	// castVote(voteId, response) -> bool
	mod.functions.push_back({"castVote", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.size() < 2) return ScriptValue::fromBool(false);

		std::string voteId = args[0].asString();
		std::string response = args[1].asString();

		auto& manager = TeamManager::instance();
		auto* client = manager.getClient(1); // 默认 handle
		return ScriptValue::fromBool(client ? client->castVote(voteId, response) : false);
	}, "voteId:string, response:string -> bool"});

	// getVoteResult(voteId) -> object
	mod.functions.push_back({"getVoteResult", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty()) return ScriptValue::null();

		std::string voteId = args[0].asString();

		auto& manager = TeamManager::instance();
		auto* client = manager.getClient(1); // 默认 handle
		if (!client) return ScriptValue::null();

		auto result = client->getVoteResult(voteId);

		// 简化处理：转字符串返回
		return ScriptValue::fromString(result.dump());
	}, "voteId:string -> string"});

	// reportStatus(status) -> bool
	mod.functions.push_back({"reportStatus", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty()) return ScriptValue::fromBool(false);

		nlohmann::json status;
		if (args[0].isString()) {
			try {
				status = nlohmann::json::parse(args[0].asString());
			} catch (...) {
				status = {{"value", args[0].asString()}};
			}
		} else if (args[0].isObject()) {
			// 简化处理
			status = {{"reported", true}};
		}

		auto& manager = TeamManager::instance();
		auto* client = manager.getClient(1); // 默认 handle
		return ScriptValue::fromBool(client ? client->reportStatus(status) : false);
	}, "status:object|string -> bool"});

	// broadcast(message) -> bool
	mod.functions.push_back({"broadcast", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty()) return ScriptValue::fromBool(false);

		nlohmann::json message;
		if (args[0].isString()) {
			try {
				message = nlohmann::json::parse(args[0].asString());
			} catch (...) {
				message = {{"text", args[0].asString()}};
			}
		} else if (args[0].isObject()) {
			// 简化处理
			message = {{"data", "message"}};
		}

		auto& manager = TeamManager::instance();
		auto* client = manager.getClient(1); // 默认 handle
		return ScriptValue::fromBool(client ? client->broadcast(message) : false);
	}, "message:object|string -> bool"});

	// getTeamStatus() -> object
	mod.functions.push_back({"getTeamStatus", [](const std::vector<ScriptValue>&) -> ScriptValue {
		auto& manager = TeamManager::instance();
		auto* client = manager.getClient(1); // 默认 handle
		if (!client) return ScriptValue::null();

		auto status = client->getTeamStatus();

		nlohmann::json result;
		result["teamId"] = status.teamId;
		result["leaderId"] = status.leaderId;
		result["members"] = status.members;
		result["state"] = status.state;
		result["lastUpdate"] = status.lastUpdate;

		return ScriptValue::fromString(result.dump());
	}, "() -> string"});

	// isJoined() -> bool
	mod.functions.push_back({"isJoined", [](const std::vector<ScriptValue>&) -> ScriptValue {
		auto& manager = TeamManager::instance();
		auto* client = manager.getClient(1); // 默认 handle
		return ScriptValue::fromBool(client ? client->isJoined() : false);
	}, "() -> bool"});

	// getMemberId() -> string
	mod.functions.push_back({"getMemberId", [](const std::vector<ScriptValue>&) -> ScriptValue {
		auto& manager = TeamManager::instance();
		auto* client = manager.getClient(1); // 默认 handle
		return ScriptValue::fromString(client ? client->getMemberId() : "");
	}, "() -> string"});

	// on(event, callback) - 监听队伍事件
	mod.functions.push_back({"on", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.size() < 2) return ScriptValue::fromBool(false);

		std::string eventType = "team." + args[0].asString();
		if (!args[1].isCallable()) return ScriptValue::fromBool(false);

		ScriptValue::CallableFunc callback = args[1].callableVal;

		EventHub::instance().subscribe(eventType, [callback, eventType](const EventMessage& msg) {
			std::vector<ScriptValue> cbArgs;
			cbArgs.push_back(ScriptValue::fromString(msg.payload.dump()));
			try {
				callback(cbArgs);
			} catch (const std::exception& e) {
				spdlog::error("[team] on callback exception for '{}': {}", eventType, e.what());
			}
		});

		return ScriptValue::fromBool(true);
	}, "event:string, callback:function -> bool"});

	return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
