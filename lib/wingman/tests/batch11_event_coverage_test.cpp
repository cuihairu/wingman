// EventHub 防御分支补测（第十一批，此前零覆盖）：
// 空 type 订阅/发射拒绝（event.cpp 18-19/97）、type 超长（23-25/102-104）、
// 订阅名超长（29-31）、单事件订阅数上限（39-41）、once 订阅 emit 成功后
// 自动移除与 handler 异常吞噬（136-144）。
// 全部为纯内存逻辑分支，无平台依赖。
// 注：23/29/39/102 四行 miss 为跨行 spdlog::warn 调用首行的行归属伪影
//（GCC 将调用指令全部归属续行），对应分支实际已覆盖——条件行/续行/
// return 行计数齐全。
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <vector>

#include "wingman/event.hpp"

using wingman::EventHub;
using wingman::EventMessage;

namespace {

constexpr size_t kMaxType = wingman::MAX_EVENT_TYPE_LENGTH;   // 256
constexpr size_t kMaxName = wingman::MAX_EVENT_NAME_LENGTH;   // 128

} // anonymous namespace

TEST(EventHubGuardTest, SubscribeRejectsEmptyType) {
    EventHub& hub = EventHub::instance();
    EXPECT_EQ(hub.subscribe("", [](const EventMessage&) {}, "batch11-empty-type"), 0u);
}

TEST(EventHubGuardTest, SubscribeRejectsOverlongType) {
    EventHub& hub = EventHub::instance();
    const std::string tooLong(kMaxType + 1, 't');
    EXPECT_EQ(hub.subscribe(tooLong, [](const EventMessage&) {}), 0u);
    // 边界长度（=256）应被接受，随后清理
    const std::string atLimit(kMaxType, 't');
    const uint64_t id = hub.subscribe(atLimit, [](const EventMessage&) {}, "batch11-limit-type");
    EXPECT_NE(id, 0u);
    hub.unsubscribe(id);
}

TEST(EventHubGuardTest, SubscribeRejectsOverlongName) {
    EventHub& hub = EventHub::instance();
    const std::string tooLongName(kMaxName + 1, 'n');
    EXPECT_EQ(hub.subscribe("batch11.long.name", [](const EventMessage&) {}, tooLongName), 0u);
}

TEST(EventHubGuardTest, SubscribeCapsAtMaxPerEvent) {
    EventHub& hub = EventHub::instance();
    // MAX_SUBSCRIPTIONS_PER_EVENT = 1000：先清场（若本用例因重复运行残留）
    const std::string type = "batch11.cap";
    std::vector<uint64_t> ids;
    for (int i = 0; i < static_cast<int>(wingman::MAX_SUBSCRIPTIONS_PER_EVENT); ++i) {
        uint64_t id = hub.subscribe(type, [](const EventMessage&) {});
        if (id != 0) ids.push_back(id);
    }
    // 容量满后第 1001 个订阅被拒
    EXPECT_EQ(hub.subscribe(type, [](const EventMessage&) {}), 0u);
    for (uint64_t id : ids) hub.unsubscribe(id);
    // 清场后可再订阅（证明拒绝来自上限而非 type 损坏）
    const uint64_t after = hub.subscribe(type, [](const EventMessage&) {});
    EXPECT_NE(after, 0u);
    hub.unsubscribe(after);
}

TEST(EventHubGuardTest, EmitRejectsEmptyAndOverlongType) {
    EventHub& hub = EventHub::instance();
    // 不崩溃即通过：emit 对非法 type 早退（无 handler 触发也无副作用可断言）
    hub.emit("", {{"k", 1}}, "batch11");
    const std::string tooLong(kMaxType + 1, 'e');
    hub.emit(tooLong, {{"k", 1}}, "batch11");
    SUCCEED();
}

TEST(EventHubGuardTest, EmitRunsOnceHandlerAndSwallowsHandlerException) {
    EventHub& hub = EventHub::instance();
    const std::string type = "batch11.once.emit";
    // once 订阅：handler 成功后 emit 收尾自动移除（event.cpp once 链 +
    // successfulOnceIds 清理块）；抛异常订阅：异常被 catch 吞噬，不阻断
    // 同 type 的其他订阅者（unordered_map 迭代序不定，用例不依赖顺序）
    int onceCalls = 0;
    const uint64_t onceId = hub.subscribe(type,
        [&](const EventMessage&) { ++onceCalls; }, "batch11-once", /*once=*/true);
    ASSERT_NE(onceId, 0u);
    int afterThrowCalls = 0;
    const uint64_t throwId = hub.subscribe(type,
        [](const EventMessage&) { throw std::runtime_error("batch11 handler boom"); },
        "batch11-throw");
    const uint64_t afterId = hub.subscribe(type,
        [&](const EventMessage&) { ++afterThrowCalls; }, "batch11-after-throw");
    ASSERT_NE(afterId, 0u);

    hub.emit(type);
    EXPECT_EQ(onceCalls, 1);
    EXPECT_EQ(afterThrowCalls, 1); // throw 订阅未阻断 after 订阅

    // once 已被自动移除：再 emit 只走常驻订阅
    hub.emit(type);
    EXPECT_EQ(onceCalls, 1);
    EXPECT_EQ(afterThrowCalls, 2);

    hub.unsubscribe(throwId);
    hub.unsubscribe(afterId);
}
