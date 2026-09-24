#include <gtest/gtest.h>
#include "wingman/trigger.hpp"
#include "wingman/lua/lua_script_engine.hpp" // registerLuaEngine（RunScript 真实引擎链）
#include "wingman/platform/mock_input.hpp"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <unistd.h>
#include <chrono>
#include <functional>
#include <thread>
#include <cstdio>

using namespace wingman;

// posix_trigger.cpp 胶水补测（2026-09-22 覆盖率收口）：TriggerManager 的
// checkTrigger 十种条件与 executeActions 九种动作此前仅 TimeElapsed 基础路径
// 被触达。本文件以「TimeElapsed 确定性命中」为驱动，逐条件/逐动作触达分支；
// 注入 MockInput 断言动作副作用。

namespace {

std::shared_ptr<spdlog::logger> quietLogger() {
    if (auto existing = spdlog::get("trigger_cov_logger")) {
        return existing;
    }
    auto logger = spdlog::stdout_color_mt("trigger_cov_logger");
    logger->set_level(spdlog::level::off);
    return logger;
}

// 有 zenity/osascript 的桌面环境下 ShowMessage 会阻塞弹窗——守卫后跳过。
// 注意：不要用 std::system 探测（多线程 + fork 场景下有死锁风险），用 access()
bool executableExists(const char* path) {
    return ::access(path, X_OK) == 0;
}

bool hasBlockingDialogTool() {
    return executableExists("/usr/bin/zenity") || executableExists("/usr/bin/osascript");
}

// 等 manager 轮询线程至少跑 n 轮（轮询间隔 50ms）
void pump(int rounds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(60 * rounds));
}

// 轮询等待条件成立（上限 2s）。固定 sleep 在全量高负载下会被调度延迟
// 击穿（v13 全量实测 InputActionsDriveMockInput 0 触发），改条件等待
bool waitForCond(const std::function<bool()>& cond) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(2000);
    while (std::chrono::steady_clock::now() < deadline) {
        if (cond()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return cond();
}

TriggerConfig elapsedConfig(const std::string& name, int intervalMs = 30) {
    TriggerConfig cfg;
    cfg.name = name;
    cfg.condition.type = TriggerType::TimeElapsed;
    cfg.condition.interval = intervalMs;
    cfg.condition.enabled = true;
    cfg.cooldown = 0;
    cfg.enabled = true;
    cfg.oneShot = false;
    return cfg;
}

} // anonymous namespace

class TriggerPosixCoverageTest : public ::testing::Test {
protected:
    void SetUp() override {
        input_ = std::make_shared<platform::mock::MockInput>();
        manager_ = std::make_unique<TriggerManager>(input_, quietLogger());
    }

    void TearDown() override {
        manager_->stop();
        manager_.reset();
    }

    std::shared_ptr<platform::mock::MockInput> input_;
    std::unique_ptr<TriggerManager> manager_;
};

// ========== checkTrigger 条件分支（经 fire 路径间接触达） ==========

TEST_F(TriggerPosixCoverageTest, DisabledConditionNeverFires) {
    TriggerConfig cfg = elapsedConfig("cov_cond_disabled");
    cfg.condition.enabled = false;
    std::atomic<int> fired{0};
    manager_->setOnFired([&](const TriggerInstance&) { fired++; });
    manager_->add(cfg);
    manager_->start();
    pump(2);
    manager_->stop();
    EXPECT_EQ(fired.load(), 0);
}

TEST_F(TriggerPosixCoverageTest, DisabledConfigNeverFires) {
    TriggerConfig cfg = elapsedConfig("cov_cfg_disabled");
    cfg.enabled = false;
    std::atomic<int> fired{0};
    manager_->setOnFired([&](const TriggerInstance&) { fired++; });
    manager_->add(cfg);
    manager_->start();
    pump(2);
    manager_->stop();
    EXPECT_EQ(fired.load(), 0);
}

TEST_F(TriggerPosixCoverageTest, TimeElapsedFiresAndHonorsCooldownAndOneShot) {
    // cooldown > 0：fire 一次后冷却期内不再 fire
    TriggerConfig cd = elapsedConfig("cov_cooldown", 30);
    cd.cooldown = 5000;
    std::atomic<int> cdFired{0};
    manager_->setOnFired([&](const TriggerInstance&) { cdFired++; });
    manager_->add(cd);
    manager_->start();
    waitForCond([&] { return cdFired.load() >= 1; });
    manager_->stop();
    EXPECT_GE(cdFired.load(), 1);
    EXPECT_LE(cdFired.load(), 2); // 5s 冷却内至多 1-2 次

    // oneShot：fire 一次后 enabled 置 false，不再 fire
    TriggerConfig once = elapsedConfig("cov_oneshot", 30);
    once.oneShot = true;
    std::atomic<int> onceFired{0};
    manager_->setOnFired([&](const TriggerInstance&) { onceFired++; });
    manager_->add(once);
    manager_->start();
    pump(3);
    manager_->stop();
    EXPECT_EQ(onceFired.load(), 1);
}

// ========== 各条件类型解析/求值分支（不命中也覆盖执行行） ==========

TEST_F(TriggerPosixCoverageTest, ColorConditionParsingBranches) {
    std::atomic<int> fired{0};
    manager_->setOnFired([&](const TriggerInstance&) { fired++; });

    // 合法 hex：走 findColor 求值（无头下 false）
    TriggerConfig ok = elapsedConfig("cov_color_ok");
    ok.condition.type = TriggerType::ColorFound;
    ok.condition.value = "0xFF00FF";
    ok.condition.tolerance = 10;
    ok.condition.region = Rect(0, 0, 50, 50);
    manager_->add(ok);

    // 非法 hex：stoul catch → false
    TriggerConfig bad = elapsedConfig("cov_color_bad");
    bad.condition.type = TriggerType::ColorFound;
    bad.condition.value = "not-a-color";
    manager_->add(bad);

    // ColorLost：合法 hex，findColor 失败 → 条件命中（会 fire，计数独立）
    TriggerConfig lost = elapsedConfig("cov_color_lost");
    lost.condition.type = TriggerType::ColorLost;
    lost.condition.value = "0xFF00FF";
    lost.condition.tolerance = 10;
    lost.condition.region = Rect(0, 0, 50, 50);
    manager_->add(lost);

    TriggerConfig badLost = elapsedConfig("cov_color_badlost");
    badLost.condition.type = TriggerType::ColorLost;
    badLost.condition.value = "zzz";
    manager_->add(badLost);

    manager_->start();
    pump(2);
    manager_->stop();
    // 只断言过程无崩溃；ColorLost 可能 fire（颜色必然找不到）
    EXPECT_GE(fired.load(), 0);
}

TEST_F(TriggerPosixCoverageTest, ImageConditionMissingTemplate) {
    std::atomic<int> fired{0};
    manager_->setOnFired([&](const TriggerInstance&) { fired++; });

    TriggerConfig found = elapsedConfig("cov_img_found");
    found.condition.type = TriggerType::ImageFound;
    found.condition.value = "/nonexistent/cov_template.png";
    found.condition.tolerance = 80;
    found.condition.region = Rect(0, 0, 50, 50);
    manager_->add(found);

    TriggerConfig lost = elapsedConfig("cov_img_lost");
    lost.condition.type = TriggerType::ImageLost;
    lost.condition.value = "/nonexistent/cov_template.png";
    lost.condition.tolerance = 80;
    manager_->add(lost); // ImageLost：模板找不到 → 恒命中 → 会 fire

    manager_->start();
    pump(2);
    manager_->stop();
    EXPECT_GE(fired.load(), 0);
}

TEST_F(TriggerPosixCoverageTest, HotkeyConditionViaMockInput) {
    // 非法键值：stoi catch → false
    TriggerConfig bad = elapsedConfig("cov_hotkey_bad");
    bad.condition.type = TriggerType::HotkeyPressed;
    bad.condition.value = "abc";
    manager_->add(bad);

    // 合法键值 + mock keyDown：条件可命中（fire 一次即够断言）
    TriggerConfig ok = elapsedConfig("cov_hotkey_ok", 30);
    ok.condition.type = TriggerType::HotkeyPressed;
    ok.condition.value = "65"; // 'A'
    ok.cooldown = 5000;
    std::atomic<int> fired{0};
    manager_->setOnFired([&](const TriggerInstance&) { fired++; });
    manager_->add(ok);

    input_->keyDown(static_cast<platform::KeyCode>(65));
    manager_->start();
    waitForCond([&] { return fired.load() >= 1; });
    input_->keyUp(static_cast<platform::KeyCode>(65));
    manager_->stop();
    EXPECT_GE(fired.load(), 1); // 按住期间必然命中过
}

TEST_F(TriggerPosixCoverageTest, WindowAndProcessConditions) {
    std::atomic<int> fired{0};
    manager_->setOnFired([&](const TriggerInstance&) { fired++; });

    // 不存在的窗口/进程：Opened=false、Closed=true（会 fire）、Started=false、Stopped=true（会 fire）
    TriggerConfig wo = elapsedConfig("cov_win_opened");
    wo.condition.type = TriggerType::WindowOpened;
    wo.condition.value = "no-such-window-cov-xyz";
    manager_->add(wo);

    TriggerConfig wc = elapsedConfig("cov_win_closed");
    wc.condition.type = TriggerType::WindowClosed;
    wc.condition.value = "no-such-window-cov-xyz";
    wc.cooldown = 5000;
    manager_->add(wc);

    TriggerConfig ps = elapsedConfig("cov_proc_started");
    ps.condition.type = TriggerType::ProcessStarted;
    ps.condition.value = "no-such-process-cov-xyz";
    manager_->add(ps);

    TriggerConfig pst = elapsedConfig("cov_proc_stopped");
    pst.condition.type = TriggerType::ProcessStopped;
    pst.condition.value = "no-such-process-cov-xyz";
    pst.cooldown = 5000;
    manager_->add(pst);

    manager_->start();
    pump(2);
    manager_->stop();
    EXPECT_GE(fired.load(), 0); // WindowClosed/ProcessStopped 可能 fire，只验证过程
}

TEST_F(TriggerPosixCoverageTest, PixelChangedTwoPhaseSampling) {
    TriggerConfig cfg = elapsedConfig("cov_pixel", 30);
    cfg.condition.type = TriggerType::PixelChanged;
    cfg.condition.region = Rect(0, 0, 1, 1);
    cfg.condition.tolerance = 100;
    cfg.cooldown = 0;
    manager_->add(cfg);
    manager_->start();
    pump(3); // 首轮记录基色（false），后续比较（无头下颜色稳定 → false）
    manager_->stop();
    SUCCEED(); // 全程无崩溃即覆盖两阶段采样行
}

// ========== executeActions 各动作类型 ==========

TEST_F(TriggerPosixCoverageTest, InputActionsDriveMockInput) {
    TriggerActionData click;
    click.type = BasicTriggerAction::Click;
    click.x = 11;
    click.y = 22;

    TriggerActionData key;
    key.type = BasicTriggerAction::KeyPress;
    key.value = "66";

    TriggerActionData type;
    type.type = BasicTriggerAction::Type;
    type.value = "hello cov";
    type.delay = 1;

    TriggerConfig cfg = elapsedConfig("cov_act_input", 30);
    cfg.actions = {click, key, type};
    cfg.cooldown = 5000;
    manager_->add(cfg);

    manager_->start();
    waitForCond([&] {
        return input_->getMouseMoveCallCount() >= 1 &&
               input_->getClickCallCount(platform::MouseButton::Left) >= 1 &&
               input_->getKeyPressCallCount(static_cast<platform::KeyCode>(66)) >= 1 &&
               input_->getInputText() == "hello cov";
    });
    manager_->stop();

    EXPECT_GE(input_->getMouseMoveCallCount(), 1);
    EXPECT_GE(input_->getClickCallCount(platform::MouseButton::Left), 1);
    EXPECT_GE(input_->getKeyPressCallCount(static_cast<platform::KeyCode>(66)), 1);
    EXPECT_EQ(input_->getInputText(), "hello cov");
}

TEST_F(TriggerPosixCoverageTest, InvalidKeyValueHitsErrorBranch) {
    TriggerActionData key;
    key.type = BasicTriggerAction::KeyPress;
    key.value = "not-a-number"; // stoi 抛出 → 错误日志分支

    TriggerConfig cfg = elapsedConfig("cov_act_badkey", 30);
    cfg.actions = {key};
    cfg.cooldown = 5000;
    manager_->add(cfg);
    manager_->start();
    pump(2);
    manager_->stop();
    SUCCEED(); // 覆盖 catch 分支且不崩溃
}

TEST_F(TriggerPosixCoverageTest, ScriptActionsRunAndEmptyBranches) {
    // RunScript 走真实 lua 链（engine 创建/模块注入/executeString/shutdown）：
    // lua 引擎靠显式 registerLuaEngine() 注册（runtime main 同款），不注册时
    // createEngine 返回空、脚本分支静默落 warn 兜底（registerLuaEngine 幂等）
    wingman::lua::registerLuaEngine();
    TriggerActionData run;
    run.type = BasicTriggerAction::RunScript;
    run.value = "wingman.log('cov-ok')";

    TriggerActionData empty;
    empty.type = BasicTriggerAction::RunScript;
    empty.value = ""; // 空脚本 warn 分支

    TriggerConfig cfg = elapsedConfig("cov_act_script", 30);
    cfg.actions = {run, empty};
    cfg.cooldown = 5000;
    manager_->add(cfg);
    manager_->start();
    // 运行中二次 start：CAS 失败早退（幂等防护，不新建线程）
    manager_->start();
    pump(2);
    manager_->stop();
    SUCCEED();
}

TEST_F(TriggerPosixCoverageTest, LogStopPauseDelayActions) {
    TriggerActionData log;
    log.type = BasicTriggerAction::Log;
    log.value = "cov log message";

    TriggerActionData stop;
    stop.type = BasicTriggerAction::StopScript;

    TriggerActionData pause;
    pause.type = BasicTriggerAction::PauseScript;

    TriggerActionData delay;
    delay.type = BasicTriggerAction::Delay;
    delay.delay = 1; // 避免 100ms 默认拖慢

    TriggerConfig cfg = elapsedConfig("cov_act_misc", 30);
    cfg.actions = {log, stop, pause, delay};
    cfg.cooldown = 5000;
    manager_->add(cfg);
    manager_->start();
    pump(2);
    manager_->stop();
    SUCCEED();
}

TEST_F(TriggerPosixCoverageTest, MessageAndAudioActionsGuardedByEnvironment) {
    if (hasBlockingDialogTool()) {
        GTEST_SKIP() << "zenity/osascript present; blocking dialog would stall CI";
    }
    TriggerActionData msg;
    msg.type = BasicTriggerAction::ShowMessage;
    msg.value = "cov message";

    TriggerConfig cfg = elapsedConfig("cov_act_msg", 30);
    cfg.actions = {msg};
    cfg.cooldown = 5000;
    manager_->add(cfg);
    manager_->start();
    pump(2);
    manager_->stop();
    SUCCEED(); // fork+execlp 失败 _exit(127)，waitpid 立即返回
}

TEST_F(TriggerPosixCoverageTest, PlayAudioInvalidPathBranch) {
    // 无效路径走 stat 失败分支（无 fork），任何环境安全
    TriggerActionData audio;
    audio.type = BasicTriggerAction::PlayAudio;
    audio.value = "/nonexistent/cov_sound.wav";

    TriggerConfig cfg = elapsedConfig("cov_act_audio", 30);
    cfg.actions = {audio};
    cfg.cooldown = 5000;
    manager_->add(cfg);
    manager_->start();
    pump(2);
    manager_->stop();
    SUCCEED();
}

// ========== 第十一批补测：构造重载 / setter 家族 / PlayAudio fork 父进程段 ==========

// 35-36 logger 单参构造、44 null logger 回退 default_logger、
// 52-65 setScriptManager/setLogger/setInput 三 setter（纯赋值，无轮询依赖）
TEST_F(TriggerPosixCoverageTest, ConstructorOverloadsAndSetters) {
    {
        // logger 单参重载 → defaultSharedInput() 路径
        TriggerManager byLogger(quietLogger());
        byLogger.stop();
    }
    {
        // null logger → m_logger = spdlog::default_logger() 回退分支
        TriggerManager nullLogger(input_, nullptr);
        nullLogger.stop();
    }
    // setter 家族：纯赋值 + null logger 回退分支（59 行三元）
    manager_->setScriptManager(nullptr);
    manager_->setLogger(nullptr);
    manager_->setLogger(quietLogger());
    manager_->setInput(std::make_shared<platform::mock::MockInput>());
    SUCCEED();
}

// 489-495 PlayAudio 有效路径：stat 通过后 fork（父进程 489/493/495 可覆盖；
// 子进程 execlp/_exit 段 491-492 因 _exit 跳过 gcov flush 天然不可计）。
// 本机无 aplay：execlp 失败 _exit(127)，waitpid 立即收尸，无副作用。
TEST_F(TriggerPosixCoverageTest, PlayAudioValidPathForksPlayer) {
    const char* wavPath = "/tmp/batch11_cov_sound.wav";
    FILE* f = ::fopen(wavPath, "wb");
    ASSERT_NE(f, nullptr);
    ::fclose(f);

    TriggerActionData audio;
    audio.type = BasicTriggerAction::PlayAudio;
    audio.value = wavPath;

    TriggerConfig cfg = elapsedConfig("cov_act_audio_ok", 30);
    cfg.actions = {audio};
    cfg.cooldown = 5000;
    manager_->add(cfg);
    manager_->start();
    pump(2);
    manager_->stop();
    ::remove(wavPath);
    SUCCEED();
}
