#include <gtest/gtest.h>
#include "test_helpers.hpp"
#include "wingman/script/module_registry.hpp"
#include "wingman/script/iscript_engine.hpp"
#include <cstdio>

using namespace wingman;
using namespace wingman::script;
using namespace wingman::script::modules;

// crypto / clipboard / macro 胶水层补测（2026-09-22 覆盖率收口）。
// crypto 全部为纯计算（往返断言确定性最强）；clipboard/macro 依赖平台状态
// （X11 剪贴板、录制 hook），无头下只断言返回形状与参数校验分支。

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

} // anonymous namespace

// ========== crypto：纯计算往返 ==========

TEST(CryptoModuleTest, Sha256KnownVector) {
    auto mod = getModule("crypto");
    ASSERT_FALSE(mod.name.empty());
    const auto* fn = findFunction(mod, "sha256");
    ASSERT_NE(fn, nullptr);
    // "abc" 的 SHA-256 标准测试向量
    EXPECT_EQ((*fn)({ScriptValue::fromString("abc")}).asString(),
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(CryptoModuleTest, Sha512KnownVector) {
    auto mod = getModule("crypto");
    const auto* fn = findFunction(mod, "sha512");
    ASSERT_NE(fn, nullptr);
    auto digest = (*fn)({ScriptValue::fromString("abc")}).asString();
    EXPECT_EQ(digest.size(), 128u);
    EXPECT_EQ(digest.substr(0, 8), "ddaf35a1");
}

TEST(CryptoModuleTest, Base64RoundTrip) {
    auto mod = getModule("crypto");
    const auto* enc = findFunction(mod, "base64Encode");
    const auto* dec = findFunction(mod, "base64Decode");
    ASSERT_NE(enc, nullptr);
    ASSERT_NE(dec, nullptr);
    EXPECT_EQ((*enc)({ScriptValue::fromString("hello wingman")}).asString(), "aGVsbG8gd2luZ21hbg==");
    EXPECT_EQ((*dec)({ScriptValue::fromString("aGVsbG8gd2luZ21hbg==")}).asString(), "hello wingman");
}

TEST(CryptoModuleTest, HexRoundTrip) {
    auto mod = getModule("crypto");
    const auto* enc = findFunction(mod, "hexEncode");
    const auto* dec = findFunction(mod, "hexDecode");
    ASSERT_NE(enc, nullptr);
    ASSERT_NE(dec, nullptr);
    auto hexed = (*enc)({ScriptValue::fromString("ABC")}).asString();
    EXPECT_EQ(hexed, "414243");
    EXPECT_EQ((*dec)({ScriptValue::fromString(hexed)}).asString(), "ABC");
}

TEST(CryptoModuleTest, AesRoundTripWithSalt) {
    auto mod = getModule("crypto");
    const auto* enc = findFunction(mod, "encryptAES");
    const auto* dec = findFunction(mod, "decryptAES");
    ASSERT_NE(enc, nullptr);
    ASSERT_NE(dec, nullptr);

    auto cipher = (*enc)({ScriptValue::fromString("secret payload"), ScriptValue::fromString("pw123")});
    ASSERT_FALSE(cipher.asString().empty());
    EXPECT_EQ((*dec)({cipher, ScriptValue::fromString("pw123")}).asString(), "secret payload");

    // 带 salt 的往返（crypt 要求 salt 恰好 16 字节，即 32 个 hex 字符）
    auto salted = (*enc)({ScriptValue::fromString("s2"), ScriptValue::fromString("pw"),
                          ScriptValue::fromString("aabbccddeeff00112233445566778899")});
    EXPECT_EQ((*dec)({salted, ScriptValue::fromString("pw")}).asString(), "s2");

    // 非法长度 salt → 加密失败返回空串
    auto badSalt = (*enc)({ScriptValue::fromString("s3"), ScriptValue::fromString("pw"),
                           ScriptValue::fromString("aabb")});
    EXPECT_TRUE((*dec)({badSalt, ScriptValue::fromString("pw")}).asString().empty());

    // 参数校验分支：非字符串/缺参 → 空串
    EXPECT_EQ((*enc)({ScriptValue::fromInt(1), ScriptValue::fromString("pw")}).asString(), "");
    EXPECT_EQ((*enc)({ScriptValue::fromString("x")}).asString(), "");
    EXPECT_EQ((*dec)({ScriptValue::fromInt(2), ScriptValue::fromString("pw")}).asString(), "");
    EXPECT_EQ((*dec)({}).asString(), "");
}

TEST(CryptoModuleTest, DeriveKeyGenerateSaltRandomBytes) {
    auto mod = getModule("crypto");
    const auto* derive = findFunction(mod, "deriveKey");
    const auto* saltFn = findFunction(mod, "generateSalt");
    const auto* randFn = findFunction(mod, "randomBytes");
    ASSERT_NE(derive, nullptr);
    ASSERT_NE(saltFn, nullptr);
    ASSERT_NE(randFn, nullptr);

    auto salt = (*saltFn)({ScriptValue::fromInt(16)}).asString();
    EXPECT_EQ(salt.size(), 32u); // 16 字节 hex 编码
    auto key = (*derive)({ScriptValue::fromString("pw"), ScriptValue::fromString(salt),
                          ScriptValue::fromInt(1000), ScriptValue::fromInt(32)});
    EXPECT_EQ(key.asString().size(), 64u);
    // 默认参数分支
    EXPECT_EQ((*derive)({ScriptValue::fromString("pw"), ScriptValue::fromString(salt)}).asString().size(), 64u);
    // 缺参分支
    EXPECT_EQ((*derive)({ScriptValue::fromString("pw")}).asString(), "");

    auto rnd = (*randFn)({ScriptValue::fromInt(8)}).asString();
    EXPECT_EQ(rnd.size(), 8u);
    EXPECT_EQ((*randFn)({}).asString().size(), 16u);
}

TEST(CryptoModuleTest, HashAndCodecParamValidation) {
    auto mod = getModule("crypto");
    for (const char* fname : {"sha256", "sha512", "base64Encode", "base64Decode", "hexEncode", "hexDecode"}) {
        const auto* fn = findFunction(mod, fname);
        ASSERT_NE(fn, nullptr) << fname;
        // 非字符串与空参数均走校验分支返回空串
        EXPECT_EQ((*fn)({ScriptValue::fromInt(9)}).asString(), "") << fname;
        EXPECT_EQ((*fn)({}).asString(), "") << fname;
    }
}

// ========== clipboard：参数校验与平台失败语义 ==========

TEST(ClipboardModuleTest, TextRoundTripWhenAvailable) {
    auto mod = getModule("clipboard");
    const auto* setText = findFunction(mod, "setText");
    const auto* getText = findFunction(mod, "getText");
    ASSERT_NE(setText, nullptr);
    ASSERT_NE(getText, nullptr);

    // Xvfb 下剪贴板真实可用：往返成立；无显示环境 setText 失败，跳过往返断言
    if ((*setText)({ScriptValue::fromString("wingman-cov-text")}).asBool()) {
        EXPECT_EQ((*getText)({}).asString(), "wingman-cov-text");
    }
}

TEST(ClipboardModuleTest, SetTextArgumentSafety) {
    auto mod = getModule("clipboard");
    const auto* setText = findFunction(mod, "setText");
    ASSERT_NE(setText, nullptr);
    // 有参调用安全（无 X display 时返回 false；返回 null 也是合法失败语义）
    auto result = (*setText)({ScriptValue::fromString("x")});
    EXPECT_TRUE(result.isBool() || result.isNull());
}

// ========== macro：录制器胶水状态机 ==========

TEST(MacroModuleTest, StatusShapeAndStopLifecycle) {
    auto mod = getModule("macro");
    ASSERT_FALSE(mod.name.empty());
    const auto* statusFn = findFunction(mod, "status");
    const auto* stopFn = findFunction(mod, "stop");
    const auto* clearFn = findFunction(mod, "clear");
    const auto* countFn = findFunction(mod, "getEventCount");
    ASSERT_NE(statusFn, nullptr);
    ASSERT_NE(stopFn, nullptr);
    ASSERT_NE(clearFn, nullptr);
    ASSERT_NE(countFn, nullptr);

    auto st = (*statusFn)({});
    ASSERT_TRUE(st.isObject());
    ASSERT_NE(st.get("recording"), nullptr);
    ASSERT_NE(st.get("paused"), nullptr);
    ASSERT_NE(st.get("eventCount"), nullptr);

    // 未录制状态下 stop/clear 是安全空操作
    (*stopFn)({});
    (*clearFn)({});
    EXPECT_GE((*countFn)({}).asInt(), 0);
}

TEST(MacroModuleTest, SaveLoadValidationBranches) {
    auto mod = getModule("macro");
    const auto* saveLua = findFunction(mod, "saveToLua");
    const auto* saveJson = findFunction(mod, "saveToJSON");
    const auto* loadJson = findFunction(mod, "loadFromJSON");
    ASSERT_NE(saveLua, nullptr);
    ASSERT_NE(saveJson, nullptr);
    ASSERT_NE(loadJson, nullptr);

    // 非字符串/缺参 → false
    EXPECT_FALSE((*saveLua)({ScriptValue::fromInt(1)}).asBool());
    EXPECT_FALSE((*saveLua)({}).asBool());
    EXPECT_FALSE((*saveJson)({ScriptValue::fromInt(1)}).asBool());
    EXPECT_FALSE((*loadJson)({ScriptValue::fromInt(1)}).asBool());
    // 不存在的路径 → false（文件写入失败分支）
    EXPECT_FALSE((*saveLua)({ScriptValue::fromString("/nonexistent-dir-cov/x.lua")}).asBool());
    EXPECT_FALSE((*loadJson)({ScriptValue::fromString("/nonexistent-dir-cov/x.json")}).asBool());
}

TEST(MacroModuleTest, StartStopRoundTripAndPlayback) {
    auto mod = getModule("macro");
    const auto* startFn = findFunction(mod, "start");
    const auto* stopFn = findFunction(mod, "stop");
    const auto* pauseFn = findFunction(mod, "pause");
    const auto* resumeFn = findFunction(mod, "resume");
    const auto* playbackFn = findFunction(mod, "playback");
    const auto* saveJson = findFunction(mod, "saveToJSON");
    const auto* loadJson = findFunction(mod, "loadFromJSON");
    ASSERT_NE(startFn, nullptr);
    ASSERT_NE(stopFn, nullptr);
    ASSERT_NE(pauseFn, nullptr);
    ASSERT_NE(resumeFn, nullptr);
    ASSERT_NE(playbackFn, nullptr);
    ASSERT_NE(saveJson, nullptr);
    ASSERT_NE(loadJson, nullptr);

    // start/stop 状态机（Xvfb 下 hook 可能启动失败，两种结果都合法）
    (*startFn)({});
    (*pauseFn)({});
    (*resumeFn)({});
    (*stopFn)({});

    // playback 默认/显式参数分支（无事件回放为空操作）
    EXPECT_TRUE((*playbackFn)({}).asBool());
    EXPECT_TRUE((*playbackFn)({ScriptValue::fromInt(50), ScriptValue::fromInt(1)}).asBool());

    // 空事件列表 saveToJSON 到临时文件成功 → loadFromJSON 往返
    std::string path = "/tmp/wingman_cov_macro.json";
    bool saved = (*saveJson)({ScriptValue::fromString(path)}).asBool();
    if (saved) {
        EXPECT_TRUE((*loadJson)({ScriptValue::fromString(path)}).asBool());
        std::remove(path.c_str());
    }
}
