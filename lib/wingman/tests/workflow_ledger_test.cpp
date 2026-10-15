// 工作流状态账本单测（workflow_ledger.hpp）：直接测类，走临时文件，不依赖
// 工作流模块装配与真实 app 数据目录。覆盖提交/终局落账、启动恢复合成
// interrupted、历史视图排序与 limit、轮转、坏行容错、空路径降级、跨进程重号。

#include <gtest/gtest.h>

#include "script/modules/workflow_ledger.hpp"
#include "wingman/process.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace wingman {
namespace script {
namespace modules {

namespace {

// 每个用例独立临时文件；轮转测试同时清掉 .old
class WorkflowLedgerFixture : public ::testing::Test {
protected:
	void SetUp() override {
		// 文件名带 pid：进程内互不覆盖，跨运行不残留
		path_ = std::filesystem::temp_directory_path() /
			("wf-ledger-test-" + std::to_string(Process::getCurrentId()) + ".jsonl");
		std::error_code ec;
		std::filesystem::remove(path_, ec);
		std::filesystem::remove(oldPath(), ec);
		ledger_ = std::make_unique<WorkflowLedger>(path_);
	}

	void TearDown() override {
		std::error_code ec;
		std::filesystem::remove(path_, ec);
		std::filesystem::remove(oldPath(), ec);
	}

	void submit(int n) {
		ledger_->recordSubmit("wf-" + std::to_string(n), "run-a",
							  "name" + std::to_string(n), 1, {{"t1", {}}});
	}

	void finish(int n, const std::string& status = "succeeded") {
		ledger_->recordFinish("wf-" + std::to_string(n), "run-a", status, {{"t1", status}});
	}

	std::filesystem::path path_;
	std::filesystem::path oldPath() const { return path_.string() + ".old"; }
	std::unique_ptr<WorkflowLedger> ledger_;
};

// 终局视图字段口径：name/startedAt 来自提交记录，status/finishedAt/tasks 来自终局
TEST_F(WorkflowLedgerFixture, SubmitAndFinishProduceHistoryView) {
	submit(1);
	finish(1);

	const auto views = ledger_->history(10);
	ASSERT_EQ(views.size(), 1u);
	EXPECT_EQ(views[0]["id"].get<std::string>(), "wf-1");
	EXPECT_EQ(views[0]["run"].get<std::string>(), "run-a");
	EXPECT_EQ(views[0]["name"].get<std::string>(), "name1");
	EXPECT_EQ(views[0]["status"].get<std::string>(), "succeeded");
	EXPECT_EQ(views[0]["tasks"]["t1"].get<std::string>(), "succeeded");
	ASSERT_NE(views[0].find("finishedAt"), views[0].end());
	ASSERT_NE(views[0].find("startedAt"), views[0].end());
	EXPECT_FALSE(views[0]["startedAt"].get<int64_t>() > views[0]["finishedAt"].get<int64_t>());
	EXPECT_EQ(views[0].find("interrupted"), views[0].end());  // 自然终局无中断标记
}

// 启动恢复：有提交无终局 → 合成 interrupted；幂等；合成记录回写后仍可见
TEST_F(WorkflowLedgerFixture, RecoverInterruptedSynthesizesAndIsIdempotent) {
	submit(1);

	std::vector<std::string> ids;
	ASSERT_EQ(ledger_->recoverInterrupted(&ids), 1);
	ASSERT_EQ(ids.size(), 1u);
	EXPECT_EQ(ids[0], "wf-1");

	const auto views = ledger_->history(10);
	ASSERT_EQ(views.size(), 1u);
	EXPECT_EQ(views[0]["status"].get<std::string>(), "interrupted");
	EXPECT_TRUE(views[0]["interrupted"].get<bool>());
	EXPECT_EQ(views[0].find("tasks"), views[0].end());  // 强杀时刻无任务状态

	EXPECT_EQ(ledger_->recoverInterrupted(&ids), 0);  // 已补过，不再重复
	EXPECT_EQ(ids.empty(), true);
}

// 进行中（仅提交）报 running 且排最前；随后终局按 finishedAt 倒序
TEST_F(WorkflowLedgerFixture, InFlightFirstThenNewestFinishedFirst) {
	submit(1);
	finish(1);
	submit(2);
	finish(2);

	const auto views = ledger_->history(10);
	ASSERT_EQ(views.size(), 2u);
	EXPECT_EQ(views[0]["id"].get<std::string>(), "wf-2");
	EXPECT_EQ(views[0]["status"].get<std::string>(), "succeeded");
	EXPECT_EQ(views[1]["id"].get<std::string>(), "wf-1");

	submit(3);  // 未终局：置顶且不带 finishedAt
	const auto inFlight = ledger_->history(10);
	ASSERT_EQ(inFlight.size(), 3u);
	EXPECT_EQ(inFlight[0]["id"].get<std::string>(), "wf-3");
	EXPECT_EQ(inFlight[0]["status"].get<std::string>(), "running");
	EXPECT_EQ(inFlight[0].find("finishedAt"), inFlight[0].end());
}

// limit 截取窗口（最新优先）
TEST_F(WorkflowLedgerFixture, HistoryLimitTrimsToNewest) {
	for (int n = 1; n <= 5; ++n) {
		submit(n);
		finish(n);
	}

	const auto views = ledger_->history(2);
	ASSERT_EQ(views.size(), 2u);
	EXPECT_EQ(views[0]["id"].get<std::string>(), "wf-5");
	EXPECT_EQ(views[1]["id"].get<std::string>(), "wf-4");
}

// 单文件超阈值轮转：早于轮转的条目移出 .old，历史视图只见当前文件
TEST_F(WorkflowLedgerFixture, RotationMovesOldEntriesAside) {
	// 阈值 120 字节 < 单条提交记录（~135 字节），保证第一条追加后即触发
	// 轮转——第一条的提交记录必然已被移出 .old
	WorkflowLedger rotated(path_, 120);
	for (int n = 1; n <= 3; ++n) {
		rotated.recordSubmit("wf-" + std::to_string(n), "run-a",
							 "name" + std::to_string(n), 1, {{"t1", {}}});
		rotated.recordFinish("wf-" + std::to_string(n), "run-a", "succeeded",
							{{"t1", "succeeded"}});
	}

	const auto views = rotated.history(10);
	ASSERT_GE(views.size(), 1u);
	bool seenWf1 = false;
	bool seenWf3 = false;
	for (const auto& v : views) {
		const auto id = v["id"].get<std::string>();
		if (id == "wf-1") seenWf1 = true;
		if (id == "wf-3") seenWf3 = true;
	}
	EXPECT_TRUE(std::filesystem::exists(oldPath()));  // 轮转确实发生
	EXPECT_TRUE(seenWf3);                             // 最新条目始终可见
	EXPECT_FALSE(seenWf1);                            // 最老条目已移出当前文件
}

// 坏行跳过：账本健壮性优先于解析完整性
TEST_F(WorkflowLedgerFixture, MalformedLinesAreSkipped) {
	{
		std::ofstream out(path_, std::ios::app);
		out << "not json at all\n"
			<< "[1, 2, 3]\n";  // 无 id 字段的合法 json 同样跳过
	}
	submit(1);
	finish(1);

	const auto views = ledger_->history(10);
	ASSERT_EQ(views.size(), 1u);
	EXPECT_EQ(views[0]["id"].get<std::string>(), "wf-1");
}

// app 数据目录不可得 = 空路径：整体降级为空操作，不落盘也不读盘
TEST_F(WorkflowLedgerFixture, EmptyPathDisablesLedger) {
	WorkflowLedger disabled;
	disabled.recordSubmit("wf-1", "run-a", "a", 1, {{"t1", {}}});
	disabled.recordFinish("wf-1", "run-a", "succeeded", {{"t1", "succeeded"}});
	EXPECT_EQ(disabled.recoverInterrupted(), 0);
	EXPECT_TRUE(disabled.history(10).empty());
}

// run 标识区分跨进程重号 workflowId（每进程 nextWorkflowId 从 1 起）
TEST_F(WorkflowLedgerFixture, CrossProcessDuplicateIdsStayDistinct) {
	submit(1);
	finish(1);
	ledger_->recordSubmit("wf-1", "run-b", "name1", 1, {{"t1", {}}});  // 同 id 换进程
	ledger_->recordFinish("wf-1", "run-b", "succeeded", {{"t1", "succeeded"}});

	const auto views = ledger_->history(10);
	ASSERT_EQ(views.size(), 2u);
	EXPECT_EQ(views[0]["run"].get<std::string>(), "run-b");  // 新进程在前
	EXPECT_EQ(views[1]["run"].get<std::string>(), "run-a");
	EXPECT_EQ(views[0]["status"].get<std::string>(), "succeeded");
	EXPECT_EQ(views[1]["status"].get<std::string>(), "succeeded");

	// 只提交不终局的 run 才被合成 interrupted，不牵连已终局的 run
	ledger_->recordSubmit("wf-1", "run-c", "name1", 1, {{"t1", {}}});
	std::vector<std::string> ids;
	ASSERT_EQ(ledger_->recoverInterrupted(&ids), 1);
	EXPECT_EQ(ids[0], "wf-1");
	EXPECT_EQ(ledger_->history(10).size(), 3u);
}

} // namespace
} // namespace modules
} // namespace script
} // namespace wingman
