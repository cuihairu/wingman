// 胶水模块缺口补测（2026-09-22 覆盖率第五批）：filewatcher（29.4%）、
// clipboard_module（29.5%）、verification（64.4%）、ml 可达错误分支（66.3%）、
// game_profile 目录/模板/JSON 导入导出/删除段（64.0%）。
// 端到端原则：clipboard 走真实后端（ClipboardLockGuard 跨进程串行化，
// 探测失败自动 skip）；game_profile 走 GameProfileManager 单例 + 临时目录
// 真实扫描/导入导出。
#include <gtest/gtest.h>

#include "clipboard_lock_guard.hpp"
#include "wingman/script/module_registry.hpp"
#include "wingman/clipboard.hpp"
#include "wingman/game_profile.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>

// MSVC Debug CRT：ctype 类函数收到负值（UTF-8 字节经 signed char）默认触发
// _CrtDbgReport 模态断言对话框，headless CI/无人值守环境下进程永久挂死
//（第六批 Windows CI 实测挂死 60 分钟直到步骤超时，见 CHANGELOG fix）。
// 静态对象先于 main 把断言重定向为 stderr/调试器输出：断言可见、进程可继续，
// 与 Linux 行为对齐。此为兜底防线，产品侧已在全部 ctype 调用点转 unsigned char。
#if defined(_WIN32) && defined(_DEBUG)
#include <crtdbg.h>
namespace {
struct CrtAssertToStderr {
    CrtAssertToStderr() {
        _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG | _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
        _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG | _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
        _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG | _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    }
};
const CrtAssertToStderr crtAssertToStderrInstance;
} // namespace
#endif

using namespace wingman;
using wingman::script::ScriptValue;
using wingman::script::ModuleDescriptor;
using wingman::script::modules::getAllModules;

namespace {

const ModuleDescriptor::FunctionEntry* findFn(const ModuleDescriptor& mod, const std::string& name) {
    for (const auto& f : mod.functions) {
        if (f.name == name) return &f;
    }
    return nullptr;
}

ScriptValue call(const ModuleDescriptor& mod, const std::string& name,
                 std::vector<ScriptValue> args = {}) {
    const auto* fn = findFn(mod, name);
    EXPECT_NE(fn, nullptr) << "missing function: " << name;
    if (!fn) return ScriptValue::null();
    return (*fn)(args);
}

ModuleDescriptor getModule(const std::string& name) {
    for (auto& m : getAllModules()) {
        if (m.name == name) return m;
    }
    return {};
}

} // namespace

// ========== filewatcher：stub 胶水参数校验分支 ==========
// 实现为简化桩（不接 FileWatcher）。第五批曾论证"ScriptValue 无公开 Callable
// 构造器、watch 成功行不可达"——该论证有误：iscript_engine.hpp 提供
// ScriptValue::fromCallable(CallableFunc, bool threadSafe=false)，测试可直接
// 构造 callable（第六批修正）。watch/unwatch/unwatchAll/isWatching/getWatchedPaths
// 全部可达行均已覆盖。

TEST(FileWatcherModuleGlue, StubValidationBranches) {
    const auto mod = getModule("filewatcher");
    ASSERT_FALSE(mod.name.empty());

    // watch：path 非 string / callable 非 callable → false
    ScriptValue notCallable = ScriptValue::fromBool(true);
    EXPECT_EQ(call(mod, "watch", {ScriptValue::fromInt(1), notCallable}).asBool(), false);
    EXPECT_EQ(call(mod, "watch", {ScriptValue::fromString("/tmp/x"), notCallable}).asBool(), false);

    // watch：合法参数（fromCallable 构造回调）→ 简化桩直接 true（成功返回行）
    ScriptValue cb = ScriptValue::fromCallable(
        [](const std::vector<ScriptValue>&) { return ScriptValue::null(); });
    EXPECT_EQ(call(mod, "watch", {ScriptValue::fromString("/tmp/wg6_watch"), cb}).asBool(), true);
    // 缺参防御（第六批修复的越界崩溃回归守卫：原实现 args[1] 裸下标）
    EXPECT_EQ(call(mod, "watch", {ScriptValue::fromString("/tmp/wg6_watch")}).asBool(), false);
    EXPECT_EQ(call(mod, "watch", {}).asBool(), false);

    EXPECT_EQ(call(mod, "unwatch", {ScriptValue::fromString("/tmp/x")}).asBool(), true);
    EXPECT_EQ(call(mod, "unwatch", {ScriptValue::fromInt(2)}).asBool(), false);
    EXPECT_EQ(call(mod, "unwatch", {}).asBool(), false);

    EXPECT_TRUE(call(mod, "unwatchAll").isNull());

    EXPECT_EQ(call(mod, "isWatching", {ScriptValue::fromString("/tmp/x")}).asBool(), false);
    EXPECT_EQ(call(mod, "isWatching", {ScriptValue::fromInt(3)}).asBool(), false);
    EXPECT_EQ(call(mod, "isWatching", {}).asBool(), false);

    const auto paths = call(mod, "getWatchedPaths");
    EXPECT_TRUE(paths.isArray());
    EXPECT_EQ(paths.arrayVal.size(), 0u);
}

// ========== clipboard：胶水直调 Clipboard 静态接口 ==========

class ClipboardModuleGlue : public ::testing::Test {
protected:
    void SetUp() override {
        mod_ = getModule("clipboard");
        ASSERT_FALSE(mod_.name.empty());
        // 后端可用性探测：OS 拒绝/无 X 时跳过（环境问题），语义对齐 ClipboardTest
        if (!Clipboard::setText("wingman-clipboard-module-probe")) {
            GTEST_SKIP() << "Clipboard unavailable — skipping";
        }
    }

    ModuleDescriptor mod_;
    ClipboardLockGuard lock_; // 锁覆盖 SetUp 探测→测试体全程
};

TEST_F(ClipboardModuleGlue, TextRoundtripAndClear) {
    EXPECT_EQ(call(mod_, "setText", {ScriptValue::fromString("glue-文本")}).asBool(), true);
    EXPECT_EQ(call(mod_, "hasText").asBool(), true);
    EXPECT_EQ(call(mod_, "getText").asString(), "glue-文本");
    EXPECT_EQ(call(mod_, "isEmpty").asBool(), false);

    call(mod_, "clear");
    EXPECT_EQ(call(mod_, "isEmpty").asBool(), true);
    EXPECT_EQ(call(mod_, "hasText").asBool(), false);
}

TEST_F(ClipboardModuleGlue, HtmlImageAndFilesBehavior) {
    // HTML/图像通道：后端能力相关，断言接口行为自洽而非具体值
    const auto setHtml = call(mod_, "setHTML", {ScriptValue::fromString("<b>x</b>")});
    EXPECT_TRUE(setHtml.isBool());
    if (setHtml.asBool()) {
        EXPECT_EQ(call(mod_, "getHTML").asString(), "<b>x</b>");
        EXPECT_EQ(call(mod_, "hasHTML").asBool(), true);
    }

    // setImage 非法参数防御（非 string / 非 int）→ false
    EXPECT_EQ(call(mod_, "setImage", {ScriptValue::fromInt(1),
                                      ScriptValue::fromInt(2),
                                      ScriptValue::fromInt(2)}).asBool(), false);
    EXPECT_EQ(call(mod_, "setImage", {ScriptValue::fromString("ab"),
                                      ScriptValue::fromString("2"),
                                      ScriptValue::fromInt(2)}).asBool(), false);
    // image 通道后端能力相关：X11/xclip 无 image 写入能力（恒 false）；Windows CF_DIB
    // 后端可真实写入（"ab" 4 字节恰为 2×2 像素 BGRA 合法尺寸。第五批按 X11 行为写死
    // 断言致 Windows CI 失败，第六批修正为平台无关自洽断言）。
    // 接口自洽：写入成功 ↔ hasImage true ↔ getImage 非空；写入失败则三者反向一致。
    const bool imageSet = call(mod_, "setImage", {ScriptValue::fromString("ab"),
                                                  ScriptValue::fromInt(2),
                                                  ScriptValue::fromInt(2)}).asBool();
    EXPECT_EQ(imageSet, call(mod_, "hasImage").asBool());
    EXPECT_EQ(imageSet, !call(mod_, "getImage").isNull());

    // setFiles 混合参数形态：字符串、数组内字符串、非字符串项忽略
    EXPECT_EQ(call(mod_, "setFiles", {
                  ScriptValue::fromString("/tmp/a.txt"),
                  ScriptValue::fromArray({ScriptValue::fromString("/tmp/b.txt"),
                                          ScriptValue::fromInt(9)}),
                  ScriptValue::fromInt(7)}).asBool(), true);
    const auto files = call(mod_, "getFiles");
    ASSERT_TRUE(files.isArray());
    ASSERT_EQ(files.arrayVal.size(), 2u);
    EXPECT_EQ(files.arrayVal[0].asString(), "/tmp/a.txt");
    EXPECT_EQ(files.arrayVal[1].asString(), "/tmp/b.txt");
    EXPECT_EQ(call(mod_, "hasFiles").asBool(), true);

    call(mod_, "clear");
    EXPECT_EQ(call(mod_, "hasFiles").asBool(), false);
}

// ========== verification：TOTP 校验链 ==========

TEST(VerificationModuleGlue, TotpValidatesSecretFormat) {
    const auto mod = getModule("verification");
    ASSERT_FALSE(mod.name.empty());

    // 空参 / 非 string → ""
    EXPECT_EQ(call(mod, "totp").asString(), "");
    EXPECT_EQ(call(mod, "totp", {ScriptValue::fromInt(42)}).asString(), "");
    // 过短 / 空 secret → ""
    EXPECT_EQ(call(mod, "totp", {ScriptValue::fromString("SHORT")}).asString(), "");
    EXPECT_EQ(call(mod, "totp", {ScriptValue::fromString("")}).asString(), "");
    // 含非 Base32 字符（0/1/小写）→ ""
    EXPECT_EQ(call(mod, "totp", {ScriptValue::fromString("ABCDEFGHIJKLMNOP01")}).asString(), "");
    EXPECT_EQ(call(mod, "totp", {ScriptValue::fromString("ABCDEFghijklmnop")}).asString(), "");

    // 合法 Base32（32 字符）→ digits 位纯数字
    const std::string secret = "JBSWY3DPEHPK3PXPJBSWY3DPEHPK3PXP";
    const auto code = call(mod, "totp", {ScriptValue::fromString(secret)});
    // asString() 按值返回：必须先落局部变量，begin/end 迭代器才能配对同一缓冲区
    const std::string codeStr = code.asString();
    EXPECT_EQ(codeStr.size(), 6u);
    EXPECT_TRUE(std::all_of(codeStr.begin(), codeStr.end(),
                            [](char c) { return c >= '0' && c <= '9'; }));

    // 自定义 digits/period
    const auto code8 = call(mod, "totp", {ScriptValue::fromString(secret),
                                          ScriptValue::fromInt(8),
                                          ScriptValue::fromInt(60)});
    EXPECT_EQ(code8.asString().size(), 8u);
}

TEST(VerificationModuleGlue, VerifyAcceptsOwnCodeAndRejectsBad) {
    const auto mod = getModule("verification");
    const std::string secret = "JBSWY3DPEHPK3PXPJBSWY3DPEHPK3PXP";

    // 参数不足 / 非法 secret / 空 code → false
    EXPECT_EQ(call(mod, "verify", {ScriptValue::fromString(secret)}).asBool(), false);
    EXPECT_EQ(call(mod, "verify", {ScriptValue::fromInt(1),
                                   ScriptValue::fromString("123456")}).asBool(), false);
    EXPECT_EQ(call(mod, "verify", {ScriptValue::fromString("SHORT"),
                                   ScriptValue::fromString("123456")}).asBool(), false);
    EXPECT_EQ(call(mod, "verify", {ScriptValue::fromString(secret),
                                   ScriptValue::fromString("")}).asBool(), false);
    EXPECT_EQ(call(mod, "verify", {ScriptValue::fromString("ABCDEFGHIJKLMNOP01"),
                                   ScriptValue::fromString("123456")}).asBool(), false);

    // 自产自销：totp 生成的码立即 verify 通过（window=1）
    const auto code = call(mod, "totp", {ScriptValue::fromString(secret)}).asString();
    EXPECT_EQ(call(mod, "verify", {ScriptValue::fromString(secret),
                                   ScriptValue::fromString(code)}).asBool(), true);
    // 错误码 → false
    EXPECT_EQ(call(mod, "verify", {ScriptValue::fromString(secret),
                                   ScriptValue::fromString("000000")}).asBool(), false);
}

TEST(VerificationModuleGlue, RemainingSecondsBounded) {
    const auto mod = getModule("verification");
    EXPECT_GT(call(mod, "remaining").asInt(), 0);
    EXPECT_LE(call(mod, "remaining").asInt(), 30);
    const auto r = call(mod, "remaining", {ScriptValue::fromInt(120)});
    EXPECT_GT(r.asInt(), 0);
    EXPECT_LE(r.asInt(), 120);
}

// ========== ml：stub 后端下可达的错误分支 ==========
// 真实模型路径（load 成功 / ioInfoToArray / run 成功）依赖 WINGMAN_ENABLE_ML
// 与真实 onnx 模型——CI 环境为 ml_stub，该半边论证记录，不硬凑。

TEST(MlModuleGlue, StubBackendErrorBranches) {
    const auto mod = getModule("ml");
    ASSERT_FALSE(mod.name.empty());

    // stub 后端 loadModel 恒失败 → nil
    EXPECT_TRUE(call(mod, "loadModel", {ScriptValue::fromString("/nonexistent/model.onnx")}).isNull());

    // unload / isLoaded：不存在 id 与空参
    EXPECT_EQ(call(mod, "unload", {ScriptValue::fromString("ml-nope")}).asBool(), false);
    EXPECT_EQ(call(mod, "unload").asBool(), false);
    EXPECT_EQ(call(mod, "isLoaded", {ScriptValue::fromString("ml-nope")}).asBool(), false);
    EXPECT_EQ(call(mod, "isLoaded").asBool(), false);

    // inputs / outputs：未知 id 与空参 → 空数组
    EXPECT_EQ(call(mod, "inputs", {ScriptValue::fromString("ml-nope")}).arrayVal.size(), 0u);
    EXPECT_EQ(call(mod, "outputs", {ScriptValue::fromString("ml-nope")}).arrayVal.size(), 0u);
    EXPECT_EQ(call(mod, "inputs").arrayVal.size(), 0u);

    // run：参数不足 → usage fail
    const auto usage = call(mod, "run", {ScriptValue::fromString("ml-nope")});
    ASSERT_NE(usage.get("success"), nullptr);
    EXPECT_EQ(usage.get("success")->asBool(), false);
    EXPECT_NE(usage.get("error")->asString().find("usage"), std::string::npos);

    // run：张量缺 name → fail
    const auto noName = call(mod, "run", {ScriptValue::fromString("ml-nope"),
        ScriptValue::fromArray({ScriptValue::fromObject({
            {"data", ScriptValue::fromArray({ScriptValue::fromFloat(1.0)})}
        })})});
    EXPECT_EQ(noName.get("success")->asBool(), false);
    EXPECT_NE(noName.get("error")->asString().find("name"), std::string::npos);

    // run：空 inputs 通过解析 → model not found fail；契约 outputs 空数组 + timeMs 0
    const auto notFound = call(mod, "run", {ScriptValue::fromString("ml-nope"),
                                            ScriptValue::fromArray({})});
    EXPECT_EQ(notFound.get("success")->asBool(), false);
    EXPECT_NE(notFound.get("error")->asString().find("model not found"), std::string::npos);
    EXPECT_EQ(notFound.get("outputs")->arrayVal.size(), 0u);
    EXPECT_EQ(notFound.get("timeMs")->asFloat(), 0.0);
}

// ========== game_profile：目录/模板/JSON 导入导出/删除段 ==========

class GameProfileModuleGlue : public ::testing::Test {
protected:
    void SetUp() override {
        mod_ = getModule("gameprofile");
        ASSERT_FALSE(mod_.name.empty());
        origDir_ = call(mod_, "getProfilesDirectory").asString();
        dir_ = std::filesystem::temp_directory_path() / "wingman_gp_glue_profiles";
        std::error_code ec;
        std::filesystem::remove_all(dir_, ec);
        std::filesystem::create_directories(dir_);
        call(mod_, "setProfilesDirectory", {ScriptValue::fromString(dir_.string())});
    }
    void TearDown() override {
        call(mod_, "setProfilesDirectory", {ScriptValue::fromString(origDir_)});
        std::error_code ec;
        std::filesystem::remove_all(dir_, ec);
    }

    ModuleDescriptor mod_;
    std::filesystem::path dir_;
    std::string origDir_;
};

TEST_F(GameProfileModuleGlue, DirectoryAndScan) {
    EXPECT_EQ(call(mod_, "getProfilesDirectory").asString(), dir_.string());
    EXPECT_EQ(call(mod_, "scan").asBool(), true);
}

TEST_F(GameProfileModuleGlue, CreateTemplateAndDelete) {
    const auto created = call(mod_, "createTemplate", {ScriptValue::fromString("胶水游戏")});
    ASSERT_TRUE(created.isArray());
    ASSERT_EQ(created.arrayVal.size(), 2u);
    const std::string id = created.arrayVal[0].asString();
    EXPECT_FALSE(id.empty());
    EXPECT_EQ(created.arrayVal[1].asString(), "胶水游戏");

    // 模板已落盘：scan 后仍可 get 到
    call(mod_, "scan");
    const auto got = call(mod_, "get", {ScriptValue::fromString(id)});
    EXPECT_FALSE(got.isNull()) << "template should persist after scan";

    // 空名创建失败 → null
    EXPECT_TRUE(call(mod_, "createTemplate", {ScriptValue::fromString("")}).isNull());

    EXPECT_EQ(call(mod_, "delete", {ScriptValue::fromString(id)}).asBool(), true);
    EXPECT_EQ(call(mod_, "delete", {ScriptValue::fromString(id)}).asBool(), false);
    EXPECT_EQ(call(mod_, "delete", {ScriptValue::fromString("nope")}).asBool(), false);
}

TEST_F(GameProfileModuleGlue, ExportImportJsonRoundtrip) {
    const auto created = call(mod_, "createTemplate", {ScriptValue::fromString("JsonGame")});
    ASSERT_TRUE(created.isArray());
    const std::string id = created.arrayVal[0].asString();

    const auto json = call(mod_, "exportJson", {ScriptValue::fromString(id)});
    EXPECT_FALSE(json.asString().empty());

    // override id 导入副本
    EXPECT_EQ(call(mod_, "importJson", {json,
                                        ScriptValue::fromString("glue_copy")}).asBool(), true);
    EXPECT_EQ(call(mod_, "importJson", {ScriptValue::fromString("{invalid"),
                                        ScriptValue::fromString("x")}).asBool(), false);

    // 不存在 id 导出为空串
    EXPECT_EQ(call(mod_, "exportJson", {ScriptValue::fromString("nope")}).asString(), "");

    // 包文件导出：不存在 id → false
    const auto pkg = dir_ / "nope.zip";
    EXPECT_EQ(call(mod_, "exportPackage", {ScriptValue::fromString("nope"),
                                           ScriptValue::fromString(pkg.string())}).asBool(), false);
}
