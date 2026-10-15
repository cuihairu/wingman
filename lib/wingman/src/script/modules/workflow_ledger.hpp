#pragma once

// 工作流状态账本（JSONL 追加式）：把工作流的提交与终局落到 app 数据目录，
// 供进程重启后做事后查询。设计取舍（2026-10-16 巡检代决，见
// docs/development-todo.md P3 任务持久化条目）：
//   * 介质 = 单文件 JSONL 追加，零新依赖；单文件超阈值轮转为 .old。
//   * 语义 = 只落账不自动恢复。v1 任务体是 Lua/Python 闭包，不可序列化，
//     重启后无可执行体——自动重放会是「假恢复」，故不做；改为启动恢复把
//     前一次运行遗留的「有提交无终局」条目合成 interrupted 终局落回账本
//     （中断 ≠ 失败，与运行失败可区分），查询面可见。
//   * 内容 = 任务 id/依赖图/状态，不落 run() 结果载荷（结果可能含闭包引用
//     或大对象，且快照语义下与事件总线口径已可查）。
//   * 全部 IO 失败静默降级：账本绝不能破坏工作流执行。
//   * run 字段 = 进程标识（pid + 启动毫秒），区分同一 workflowId 跨进程
//     重号（nextWorkflowId 每进程从 1 起）。

#include <nlohmann/json.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace wingman {
namespace script {
namespace modules {

class WorkflowLedger {
public:
	static constexpr uint64_t kDefaultMaxFileBytes = 8 * 1024 * 1024;

	// file 为空路径时账本整体关闭（app 数据目录不可得），所有方法为空操作。
	WorkflowLedger(std::filesystem::path file = std::filesystem::path(),
				   uint64_t maxFileBytes = kDefaultMaxFileBytes)
		: file_(std::move(file)), maxFileBytes_(maxFileBytes) {}

	// 提交记录：graph = 任务 id → dependsOn（提交时刻的依赖图）
	void recordSubmit(const std::string& id, const std::string& run, const std::string& name,
					 int maxParallel,
					 const std::vector<std::pair<std::string, std::vector<std::string>>>& graph) {
		std::lock_guard<std::mutex> lock(mutex_);
		if (file_.empty()) return;
		nlohmann::json arr;
		for (const auto& [taskId, deps] : graph) {
			nlohmann::json depsArr;
			for (const auto& d : deps) depsArr.push_back(d);
			arr.push_back({{"id", taskId}, {"dependsOn", std::move(depsArr)}});
		}
		appendLine({{"t", "submit"}, {"run", run}, {"id", id}, {"name", name},
					{"maxParallel", maxParallel}, {"ts", nowMs()}, {"graph", std::move(arr)}});
	}

	// 终局记录：tasks = 任务 id → 状态字符串（与快照口径一致）
	void recordFinish(const std::string& id, const std::string& run, const std::string& status,
					  const std::vector<std::pair<std::string, std::string>>& tasks) {
		std::lock_guard<std::mutex> lock(mutex_);
		if (file_.empty()) return;
		nlohmann::json obj;
		for (const auto& [taskId, st] : tasks) obj[taskId] = st;
		appendLine({{"t", "finish"}, {"run", run}, {"id", id}, {"status", status},
					{"ts", nowMs()}, {"tasks", std::move(obj)}});
	}

	// 启动恢复：为「有提交无终局」的条目合成 interrupted 终局并落回账本，
	// 返回合成条数（幂等——第二次调用无遗留可补，恒 0）。outIds 非空时
	// 返回被合成的工作流 id（供调用方发通知事件）。
	int recoverInterrupted(std::vector<std::string>* outIds = nullptr) {
		std::lock_guard<std::mutex> lock(mutex_);
		if (outIds) outIds->clear();  // 幂等：调用方复用同一容器不残留上次结果
		if (file_.empty()) return 0;
		const std::vector<Entry> entries = readEntries();
		std::vector<std::pair<std::string, std::string>> missing;  // (run, id)
		for (const auto& e : entries) {
			if (!e.finish && e.submit) missing.emplace_back(e.run, e.id);
		}
		for (const auto& [run, id] : missing) {
			if (outIds) outIds->push_back(id);
			appendLine({{"t", "finish"}, {"run", run}, {"id", id}, {"status", "interrupted"},
						{"ts", nowMs()}, {"synthesized", true}});
		}
		return static_cast<int>(missing.size());
	}

	// 终局查询视图，最新在前：{id, name, run, status, startedAt?,
	// finishedAt?, interrupted?, tasks?}。进行中（仅提交无终局）的条目
	// status 报 running、不带 finishedAt，排在最前。
	std::vector<nlohmann::json> history(int limit = 20) const {
		std::lock_guard<std::mutex> lock(mutex_);
		// 视图带追加序号（readEntries 已按文件追加序合并）：账本常在同一轮
		// 循环内连续追加，毫秒级时间戳会大量相等，用序号打破平局。
		std::vector<View> views;
		int64_t seq = 0;
		for (const auto& e : readEntries()) {
			nlohmann::json v = {{"id", e.id}, {"run", e.run}};
			if (e.submit) {
				v["name"] = e.submit->value("name", "");
				v["startedAt"] = e.submit->value("ts", int64_t{0});
			}
			View view;
			view.seq = seq++;
			if (e.finish) {
				v["status"] = e.finish->value("status", "unknown");
				view.finishedAt = e.finish->value("ts", int64_t{0});
				v["finishedAt"] = view.finishedAt;
				if (e.finish->value("synthesized", false)) v["interrupted"] = true;
				const nlohmann::json& tasks = e.finish->value("tasks", nlohmann::json::object());
				if (!tasks.empty()) v["tasks"] = tasks;
			} else {
				view.finishedAt = 0;  // 进行中条目无终局时间，不参与时间序
				v["status"] = "running";
			}
			view.v = std::move(v);
			views.push_back(std::move(view));
		}
		// 进行中（仅提交无终局）条目固定置顶；其余按终局时间倒序，同时间戳
		// 取追加序更晚者（后追加=更新）。
		std::stable_sort(views.begin(), views.end(), [](const View& a, const View& b) {
			const bool aInFlight = a.finishedAt == 0;
			const bool bInFlight = b.finishedAt == 0;
			if (aInFlight != bInFlight) return aInFlight;
			if (a.finishedAt != b.finishedAt) return a.finishedAt > b.finishedAt;
			return a.seq > b.seq;
		});
		if (limit > 0 && static_cast<int>(views.size()) > limit) {
			views.resize(static_cast<size_t>(limit));
		}
		std::vector<nlohmann::json> out;
		out.reserve(views.size());
		for (auto& view : views) out.push_back(std::move(view.v));
		return out;
	}

	const std::filesystem::path& path() const { return file_; }

private:
	struct Entry {
		std::string run;
		std::string id;
		std::optional<nlohmann::json> submit;
		std::optional<nlohmann::json> finish;
	};

	// history 排序用视图：finishedAt=0 表示进行中（无终局）
	struct View {
		int64_t finishedAt = 0;
		int64_t seq = 0;
		nlohmann::json v;
	};

	static int64_t nowMs() {
		return std::chrono::duration_cast<std::chrono::milliseconds>(
				   std::chrono::system_clock::now().time_since_epoch())
			.count();
	}

	// 追加一行 JSON（超阈值先轮转）
	void appendLine(const nlohmann::json& line) {
		try {
			if (maxFileBytes_ > 0) {
				std::error_code ec;
				std::filesystem::path old = file_;
				old += ".old";
				const std::uintmax_t size =
					std::filesystem::exists(file_, ec) ? std::filesystem::file_size(file_, ec) : 0;
				if (size >= maxFileBytes_) {
					std::filesystem::remove(old, ec);
					std::filesystem::rename(file_, old, ec);
				}
			}
			std::filesystem::create_directories(file_.parent_path());
			std::ofstream out(file_, std::ios::app);
			if (!out) return;
			out << line.dump() << "\n";
		} catch (const std::exception& e) {
			spdlog::warn("workflow ledger append failed: {}", e.what());
		}
	}

	std::vector<Entry> readEntries() const {
		std::vector<Entry> entries;
		std::error_code ec;
		if (!std::filesystem::exists(file_, ec)) return entries;
		std::ifstream in(file_);
		if (!in) return entries;
		std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		size_t lineStart = 0;
		while (lineStart < text.size()) {
			size_t end = text.find('\n', lineStart);
			const bool last = end == std::string::npos;
			if (last) end = text.size();
			const std::string line = text.substr(lineStart, end - lineStart);
			if (!line.empty()) {
				try {
					const nlohmann::json j = nlohmann::json::parse(line);
					if (j.contains("id")) {
						Entry e;
						e.run = j.value("run", "");
						e.id = j["id"].get<std::string>();
						if (j.value("t", "") == "submit") e.submit = j;
						else if (j.value("t", "") == "finish") e.finish = j;
						entries.push_back(std::move(e));
					}
				} catch (...) {
					// 坏行跳过：账本健壮性优先于解析完整性
				}
			}
			if (last) break;
			lineStart = end + 1;
		}
		// 合并为 run+id 唯一条目：保留首次提交与末次终局（追加序即时间序）
		std::vector<Entry> merged;
		for (const auto& e : entries) {
			bool found = false;
			for (auto& m : merged) {
				if (m.run == e.run && m.id == e.id) {
					if (e.submit) m.submit = e.submit;
					if (e.finish) m.finish = e.finish;
					found = true;
					break;
				}
			}
			if (!found) merged.push_back(e);
		}
		return merged;
	}

	std::filesystem::path file_;
	uint64_t maxFileBytes_;
	mutable std::mutex mutex_;
};

} // namespace modules
} // namespace script
} // namespace wingman
