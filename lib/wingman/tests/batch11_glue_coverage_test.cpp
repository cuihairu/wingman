// 第十一批胶水补测：team 未加入空态、macro 模块全家族（lazy 默认实例）、
// task async 超时/取消/cleanup 与防御、inbox report 参数变体、
// Bitmap BMP 解析容错与 save 失败分支、Screen 静态 bounds 查询。
//
// team 空态用例要求进程内尚未有任何 joinTeam 建出的默认客户端（TeamManager
// 是进程级单例，client 一旦创建便无法移除）。gtest 单进程按注册顺序而非
// 套件名字母序运行，本文件在 CMake target_sources 中注册靠后，全量跑时
// 空态前提必然不成立——用例以 teamId 是否为空检测前提，不满足时
// GTEST_SKIP 明示跳过；ctest 每用例独立进程的模式（gtest_discover_tests）
// 与 gtest_filter 单跑下前提成立，完整断言照常执行，两种跑法均有效。
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <sys/stat.h>
#include <thread>
#include <vector>

#include "script/modules/macro_module.hpp" // cleanupMacroModule（公开头只有 setGlobalRecorder）
#include "wingman/screen.hpp"
#include "wingman/script/module_registry.hpp"
#include "wingman/script/iscript_engine.hpp"

using namespace wingman;
using namespace wingman::script;
using namespace wingman::script::modules;

namespace {

ModuleDescriptor getModule(const std::string& name) {
    for (auto& mod : getAllModules()) {
        if (mod.name == name) return mod;
    }
    return {};
}

const ModuleDescriptor::FunctionEntry* findFunction(const ModuleDescriptor& mod, const std::string& name) {
    for (const auto& f : mod.functions) {
        if (f.name == name) return &f;
    }
    return nullptr;
}

ScriptValue call(const ModuleDescriptor& mod, const std::string& name,
                 std::vector<ScriptValue> args = {}) {
    const auto* fn = findFunction(mod, name);
    EXPECT_NE(fn, nullptr) << "missing function: " << name;
    if (!fn) return ScriptValue::null();
    return (*fn)(std::move(args));
}

ScriptValue callable(ScriptValue::CallableFunc fn, bool threadSafe = false) {
    return ScriptValue::fromCallable(std::move(fn), threadSafe);
}

// 按 pack(1) BMP 布局手工拼一份 BMP 文件字节（screen.cpp 的手工解析分支
// 只在无 vision 构建编译，内部结构体不可从测试引用，故按字节数构造）
std::vector<uint8_t> makeBmpBytes(int width, int height, uint16_t bitCount,
                                  bool badSignature = false, bool badPlanes = false,
                                  bool badBitCount = false, size_t truncatePixelsTo = SIZE_MAX) {
    const uint32_t bytesPerPixel = bitCount / 8;
    const uint32_t rowStride = ((static_cast<uint32_t>(width) * bytesPerPixel) + 3u) & ~3u;
    const uint32_t imageSize = rowStride * static_cast<uint32_t>(height);
    constexpr uint32_t kPixelOffset = 14 + 40;
    const uint32_t fileSize = kPixelOffset + imageSize;

    std::vector<uint8_t> bytes;
    bytes.reserve(fileSize);

    auto put16 = [&bytes](uint16_t v) {
        bytes.push_back(static_cast<uint8_t>(v & 0xFF));
        bytes.push_back(static_cast<uint8_t>(v >> 8));
    };
    auto put32 = [&bytes](uint32_t v) {
        for (int i = 0; i < 4; ++i) bytes.push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xFF));
    };

    // BmpFileHeader（14B）
    put16(badSignature ? 0x5858 : 0x4D42); // 'BM'；坏签名变体用 'XX'
    put32(fileSize);
    put16(0);
    put16(0);
    put32(kPixelOffset);
    // BmpInfoHeader（40B）
    put32(40);              // headerSize
    put32(static_cast<uint32_t>(width));
    put32(static_cast<uint32_t>(height));
    put16(badPlanes ? 2 : 1);
    put16(badBitCount ? 16 : bitCount);
    put32(0);               // compression = RGB
    put32(imageSize);
    put32(0); put32(0);     // 像素密度
    put32(0); put32(0);     // 调色板
    // 像素数据（可截断）
    const size_t pixelBytes = imageSize < truncatePixelsTo ? imageSize : truncatePixelsTo;
    for (size_t i = 0; i < pixelBytes; ++i) bytes.push_back(static_cast<uint8_t>(i & 0xFF));
    return bytes;
}

void writeBytes(const std::string& path, const std::vector<uint8_t>& bytes) {
    std::ofstream file(path, std::ios::binary);
    ASSERT_TRUE(file.is_open()) << path;
    file.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    ASSERT_TRUE(file.good()) << path;
}

} // anonymous namespace

// ========== team：进程内尚无 client 1 时的全空态查询 ==========

TEST(A11Batch11GlueTest, TeamEmptyStateQueriesBeforeFirstJoin) {
    const auto mod = getModule("team");
    ASSERT_EQ(mod.name, "team");

    // 前提检测：单进程全量跑时此前套件已建出默认 client（TeamManager 进程级
    // 单例不可销毁），空态分支无从触达——明示 skip 而非误报失败
    const nlohmann::json precheck = nlohmann::json::parse(call(mod, "getTeamStatus").asString());
    if (!precheck["teamId"].get<std::string>().empty()) {
        GTEST_SKIP() << "team client already created by earlier suite in this process";
    }

    // getTeamStatus 空 client → 组装 idle JSON（此前 490-497 从未触达：
    // 既有用例都先 joinTeam 再查询）
    const auto status = call(mod, "getTeamStatus");
    ASSERT_TRUE(status.isString());
    const nlohmann::json parsed = nlohmann::json::parse(status.asString());
    EXPECT_EQ(parsed["teamId"].get<std::string>(), "");
    EXPECT_EQ(parsed["leaderId"].get<std::string>(), "");
    EXPECT_TRUE(parsed["members"].is_array());
    EXPECT_TRUE(parsed["members"].empty());
    EXPECT_EQ(parsed["state"].get<std::string>(), "idle");
    EXPECT_EQ(parsed["lastUpdate"], 0);

    EXPECT_EQ(call(mod, "isJoined").asBool(), false);
    EXPECT_EQ(call(mod, "getMemberId").asString(), "");
    // 未加入时 leaveTeam 走 client 缺失分支返回 false
    EXPECT_EQ(call(mod, "leaveTeam").asBool(), false);
}

// ========== macro：未注入时的 lazy 默认实例全家族 ==========

TEST(A11Batch11GlueTest, MacroModuleFullFamilyViaLazyDefault) {
    const auto mod = getModule("macro");
    ASSERT_EQ(mod.name, "macro");

    // 显式清空注入指针（34-36），确认后续走 lazy 默认实例路径
    setGlobalRecorder(nullptr);

    const std::string jsonPath = "/tmp/batch11_macro_events.json";
    const std::string luaPath = "/tmp/batch11_macro_events.lua";

    // 空事件列表导出两种格式均成功（97/103）
    EXPECT_EQ(call(mod, "saveToJSON", {ScriptValue::fromString(jsonPath)}).asBool(), true);
    EXPECT_EQ(call(mod, "saveToLua", {ScriptValue::fromString(luaPath)}).asBool(), true);
    // 往返加载自身导出的合法文件（109 的成功分支）
    EXPECT_EQ(call(mod, "loadFromJSON", {ScriptValue::fromString(jsonPath)}).asBool(), true);
    EXPECT_EQ(call(mod, "loadFromJSON", {ScriptValue::fromString("/tmp/batch11_no_such.json")}).asBool(), false);
    std::remove(jsonPath.c_str());
    std::remove(luaPath.c_str());

    // 状态机家族（有 Xvfb 时 start 真实开启 XRecord，无 X 时安全返回 false，
    // 两种环境均不断言 start 返回值）
    call(mod, "start");
    EXPECT_EQ(call(mod, "isRecording").isBool(), true);
    call(mod, "pause");
    EXPECT_EQ(call(mod, "isPaused").isBool(), true);
    call(mod, "resume");
    call(mod, "stop");
    call(mod, "clear");
    EXPECT_EQ(call(mod, "getEventCount").asInt(), 0);

    const auto status = call(mod, "status");
    ASSERT_TRUE(status.isObject());
    EXPECT_NE(status.get("recording"), nullptr);
    EXPECT_NE(status.get("paused"), nullptr);
    EXPECT_NE(status.get("eventCount"), nullptr);

    // 空事件 playback 立即完成（117 的 return true）
    EXPECT_EQ(call(mod, "playback", {ScriptValue::fromInt(100), ScriptValue::fromInt(1)}).asBool(), true);

    cleanupMacroModule(); // 释放 lazy 默认实例，避免跨用例残留 XRecord 线程
}

// ========== task：async 超时/取消/cleanup 与缺参防御 ==========

TEST(A11Batch11GlueTest, TaskAsyncTimeoutCancelCleanupAndDefenses) {
    const auto mod = getModule("task");
    ASSERT_EQ(mod.name, "task");

    // 缺参防御（489/495/501/508/514）
    EXPECT_EQ(call(mod, "cancel").asBool(), false);
    EXPECT_EQ(call(mod, "status").asString(), "failed");
    EXPECT_EQ(call(mod, "wait").asBool(), false);
    EXPECT_TRUE(call(mod, "result").isNull());
    EXPECT_EQ(call(mod, "error").asString(), "Task not found");
    // work 非 callable（444-446）
    EXPECT_EQ(call(mod, "submit", {ScriptValue::fromString("nope")}).asBool(), false);

    // 同步任务（async 缺省 false，306-308 阻塞执行）完成后仍留在 map，
    // retry 依 id 复提交成功（359-374）。必须先于任何 async 提交执行：
    // async worker 收尾的 cleanup 会把已终态的同步任务一并 erase，届时
    // retry 只能得到 false
    const auto syncId = call(mod, "submit", {callable(
        [](const std::vector<ScriptValue>&) { return ScriptValue::fromInt(1); }, true)});
    ASSERT_TRUE(syncId.isString());
    EXPECT_EQ(call(mod, "retry", {syncId}).asBool(), true);

    const auto asyncOpt = [](int timeoutMs) {
        return ScriptValue::fromObject({{"async", ScriptValue::fromBool(true)},
                                        {"timeoutMs", ScriptValue::fromInt(timeoutMs)}});
    };
    // work 睡 500ms：为 submit 后立即查询 / cancel / wait 超时留出宽窗口，
    // 同时保证这些检查点全部落在 worker 收尾（execute 返回 → cleanupFinishedTasks
    // erase 任务）之前——终态任务会被立即移除，事后 status 只会得到 "failed"
    const auto sleepyWork = [](const std::vector<ScriptValue>&) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        return ScriptValue::fromString("done");
    };

    // 运行中查询（胶水 496 正常路径）→ pending/running；随后取消：
    // Task::cancel 置 canceled（149-158），work 返回后循环顶收尾（87-92）
    const auto cancelId = call(mod, "submit", {callable(sleepyWork, true), asyncOpt(0)});
    ASSERT_TRUE(cancelId.isString());
    const std::string earlyStatus = call(mod, "status", {cancelId}).asString();
    EXPECT_TRUE(earlyStatus == "pending" || earlyStatus == "running") << earlyStatus;
    std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 等 worker 进入 work
    EXPECT_EQ(call(mod, "cancel", {cancelId}).asBool(), true);
    EXPECT_EQ(call(mod, "wait", {cancelId, ScriptValue::fromInt(5000)}).asBool(), true);
    // wait 返回（≈50ms）距 execute 返回（≈500ms）窗口充裕，任务尚在 map
    EXPECT_EQ(call(mod, "status", {cancelId}).asString(), "canceled");

    // 任务超时：timeoutMs=100 < work 500ms，timeoutThread 置 timeout（79-81）。
    // 胶水 wait 用 200ms deadline——先于 wait 自身超时醒来，且覆盖两种置态
    // 路径殊途同归；不断言 wait 返回值（极端负载下可能走 wait 超时分支返回 false）
    const auto timeoutId = call(mod, "submit", {callable(sleepyWork, true), asyncOpt(100)});
    ASSERT_TRUE(timeoutId.isString());
    call(mod, "wait", {timeoutId, ScriptValue::fromInt(200)});
    EXPECT_EQ(call(mod, "status", {timeoutId}).asString(), "timeout");

    // wait 自身超时：deadline 先到 → wait 置 timeout 并 emit（187-211），
    // 任务尚在 map（work 未返回），error 为空串而非 "Task not found"
    const auto waitId = call(mod, "submit", {callable(sleepyWork, true), asyncOpt(0)});
    EXPECT_EQ(call(mod, "wait", {waitId, ScriptValue::fromInt(100)}).asBool(), false);
    EXPECT_NE(call(mod, "error", {waitId}).asString(), "Task not found");

    // cleanupFinishedTasks 的两条路径：fast 微秒完成 → 其 worker 收尾触发
    // cleanup 时 slow 仍在运行（非终态走 ++it 跳过分支），fast 自身被即时
    // erase（自 erase 语义：事后 status 查询得到未知 id 回退 "failed"）
    const auto slowId = call(mod, "submit", {callable(sleepyWork, true), asyncOpt(0)});
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const auto fastId = call(mod, "submit", {callable(
        [](const std::vector<ScriptValue>&) { return ScriptValue::fromInt(1); }, true),
        asyncOpt(0)});
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(call(mod, "status", {fastId}).asString(), "failed");
    // slow 的 wait 被 succeeded 置位的 notify 及时唤醒（≈500ms 返回 true；
    // notify 缺失时只能睡满 deadline）
    const bool slowWait = call(mod, "wait", {slowId, ScriptValue::fromInt(5000)}).asBool();
    EXPECT_TRUE(slowWait);
    // wait 返回时 worker 可能还在 emitEvent 收尾，稍候再验证自 erase
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(call(mod, "status", {slowId}).asString(), "failed");

    // metadata 传 callable → toJson 落 default 分支（431-432，submit 即触发）；
    // 任务本身微秒完成被即时 erase，wait 不作断言
    call(mod, "submit", {callable(
        [](const std::vector<ScriptValue>&) { return ScriptValue::fromInt(1); }, true),
        ScriptValue::fromObject({{"async", ScriptValue::fromBool(true)},
                                 {"metadata", callable(
                                     [](const std::vector<ScriptValue>&) { return ScriptValue::null(); })}})});
}

// ========== task：重试间隙的 cancel/timeout 早退检查（execute 循环顶） ==========

TEST(A11Batch11GlueTest, TaskRetryGapCancelAndTimeoutDetection) {
    const auto mod = getModule("task");
    ASSERT_EQ(mod.name, "task");

    // work 首轮抛异常 → catch 计入 attempts（130-137）→ backoff 睡眠 →
    // 循环顶 isCanceled()/status()==timeout 命中早退（90-100）。两分支各
    // 走一个任务；异常经胶水 callableVal 直接传播到 Task::execute 的 catch
    const auto failingWork = callable([](const std::vector<ScriptValue>&) -> ScriptValue {
        throw std::runtime_error("batch11 retry-gap probe");
    }, true);
    const auto retryOpt = [](int backoffMs, int timeoutMs) {
        return ScriptValue::fromObject({{"async", ScriptValue::fromBool(true)},
                                        {"maxRetries", ScriptValue::fromInt(1)},
                                        {"backoffMs", ScriptValue::fromInt(backoffMs)},
                                        {"timeoutMs", ScriptValue::fromInt(timeoutMs)}});
    };

    // cancel 分支：backoff 400ms 期间（≈100ms 时）cancel → 循环顶 isCanceled
    const auto cancelId = call(mod, "submit", {failingWork, retryOpt(400, 0)});
    ASSERT_TRUE(cancelId.isString());
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 首轮失败已入 backoff
    EXPECT_EQ(call(mod, "cancel", {cancelId}).asBool(), true);
    EXPECT_EQ(call(mod, "wait", {cancelId, ScriptValue::fromInt(5000)}).asBool(), true);
    EXPECT_EQ(call(mod, "status", {cancelId}).asString(), "canceled");

    // timeout 分支：backoff 500ms 期间 timeoutMs=100 触发 timeoutThread 置态
    // → 循环顶 status()==timeout 早退；wait 唤醒后终态 timeout（自 erase 前
    // 的窗口内查询，宽 sleep 后仅断言非 running）
    const auto timeoutId = call(mod, "submit", {failingWork, retryOpt(500, 100)});
    ASSERT_TRUE(timeoutId.isString());
    EXPECT_EQ(call(mod, "wait", {timeoutId, ScriptValue::fromInt(5000)}).asBool(), true);
    const std::string timeoutStatus = call(mod, "status", {timeoutId}).asString();
    EXPECT_TRUE(timeoutStatus == "timeout" || timeoutStatus == "failed") << timeoutStatus;
}

// ========== inbox：report 第三参非字符串 + disconnect 幂等 ==========

TEST(A11Batch11GlueTest, InboxReportNonStringResultBranch) {
    const auto mod = getModule("inbox");
    ASSERT_EQ(mod.name, "inbox");

    // args[2] 非 string 非 null → resultStr 走 "{}" 简化分支（545），
    // 且 handle 无效 → client 缺失返回 false
    EXPECT_EQ(call(mod, "report", {ScriptValue::fromInt(987654),
                                   ScriptValue::fromString("m"),
                                   ScriptValue::fromInt(5)}).asBool(), false);
}

// ========== Bitmap：手工 BMP 解析容错与 save 失败分支 ==========

TEST(A11Batch11GlueTest, BitmapBmpParsingAndSaveFailureBranches) {
    const std::string dir = "/tmp/batch11_bmp";
    std::filesystem::create_directories(dir);

    // 文件不足双头大小 → 头读取失败（310-311；root 环境同样有效）
    {
        std::ofstream tiny(dir + "/tiny.bmp", std::ios::binary);
        tiny << "BM\x00\x00";
    }
    EXPECT_EQ(Bitmap::fromFile(dir + "/tiny.bmp"), nullptr);

    // 坏签名 / 坏 planes / 坏 bitCount → 头校验拒绝（321）
    writeBytes(dir + "/badsig.bmp", makeBmpBytes(2, 2, 24, /*badSignature=*/true));
    EXPECT_EQ(Bitmap::fromFile(dir + "/badsig.bmp"), nullptr);
    writeBytes(dir + "/badplanes.bmp", makeBmpBytes(2, 2, 24, false, /*badPlanes=*/true));
    EXPECT_EQ(Bitmap::fromFile(dir + "/badplanes.bmp"), nullptr);
    writeBytes(dir + "/badbpp.bmp", makeBmpBytes(2, 2, 24, false, false, /*badBitCount=*/true));
    EXPECT_EQ(Bitmap::fromFile(dir + "/badbpp.bmp"), nullptr);

    // 像素数据截断 → 行读取失败（341）
    writeBytes(dir + "/truncated.bmp", makeBmpBytes(2, 2, 24, false, false, false, /*truncatePixelsTo=*/1));
    EXPECT_EQ(Bitmap::fromFile(dir + "/truncated.bmp"), nullptr);

    // 完整 24bpp 文件解析成功（回归保护：无 vision 构建走手工解析，有 vision
    // 走 cv::imread，两者都应 non-null）
    writeBytes(dir + "/ok.bmp", makeBmpBytes(2, 2, 24));
    EXPECT_NE(Bitmap::fromFile(dir + "/ok.bmp"), nullptr);

    // 存在但不可读 → ifstream 打不开（303；root 无视文件权限，跳过）
    if (geteuid() != 0) {
        writeBytes(dir + "/noread.bmp", makeBmpBytes(2, 2, 24));
        ::chmod((dir + "/noread.bmp").c_str(), 0000);
        EXPECT_EQ(Bitmap::fromFile(dir + "/noread.bmp"), nullptr);
        ::chmod((dir + "/noread.bmp").c_str(), 0644);
    }

    // save：非法宽高（778）/ 打不开的路径（807）
    Bitmap empty(0, 4);
    EXPECT_EQ(empty.save(dir + "/empty.bmp"), false);
    Bitmap valid(8, 8);
    EXPECT_EQ(valid.save("/tmp/batch11_no_such_dir/x.bmp"), false);

    // /dev/full：打开成功但缓冲刷新 ENOSPC → 行写入失败（828）。
    // 流缓冲使 header 不落盘即返回，行数据量大时才会触发流错误
    Bitmap big(64, 64);
    EXPECT_EQ(big.save("/dev/full"), false);

    // 常规路径保存成功（回归保护）
    EXPECT_EQ(valid.save(dir + "/saved.bmp"), true);

    std::filesystem::remove_all(dir);
}

// ========== Screen：静态 bounds 查询（有/无 X 均可执行） ==========

TEST(A11Batch11GlueTest, ScreenBoundsStaticQuery) {
    // Linux 实现经 createPlatformScreen 组装（969-970），无 X 时走
    // 未初始化的 {0,0,1920,1080} 回退，断言保持宽松
    const Rect bounds = Screen::getScreenBounds();
    EXPECT_GE(bounds.width, 0);
    EXPECT_GE(bounds.height, 0);
}
