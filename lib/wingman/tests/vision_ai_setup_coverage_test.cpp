#include <gtest/gtest.h>
#include "wingman/script/module_registry.hpp"
#include "wingman/script/iscript_engine.hpp"
#include "wingman/vision_ai.hpp"

#include <string>
#include <vector>

using namespace wingman;
using namespace wingman::script;
using namespace wingman::script::modules;

// vision.aiSetup / aiSetupStatus / aiSetupLocal / aiLocate / aiElements 缺口补测
// （2026-11-08 覆盖率收口）：aiSetup 校验分支矩阵（非对象/缺 passphrase/解密失败/
// 空 baseUrl/空 model）、aiSetupLocal 无参与加载失败、aiLocate/aiElements 在本地
// 不可达 provider（127.0.0.1:1 瞬时拒绝）下的 not-found 错误面。
// 成功路径 aiSetup 只存配置不联网（VisionAi::setup 纯存储），HTTP 往返由
// vision_ai 测试/集成覆盖。

namespace {

ModuleDescriptor getModule(const std::string& name) {
    for (auto& mod : getAllModules()) {
        if (mod.name == name) return mod;
    }
    return {};
}

const ModuleDescriptor::FunctionEntry* findFunction(const ModuleDescriptor& mod,
                                                    const std::string& name) {
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

const ScriptValue* getField(const ScriptValue& obj, const char* key) {
    return obj.isObject() ? obj.get(key) : nullptr;
}

bool fieldBool(const ScriptValue& obj, const char* key) {
    const ScriptValue* v = getField(obj, key);
    return v && v->asBool();
}

} // namespace

TEST(VisionAiSetupCoverageTest, AiSetupValidationMatrix) {
    ModuleDescriptor mod = getModule("vision");
    ASSERT_FALSE(mod.name.empty());

    // 无参 / 非对象 → false
    EXPECT_FALSE(call(mod, "aiSetup").asBool());
    EXPECT_FALSE(call(mod, "aiSetup", {ScriptValue::fromInt(42)}).asBool());
    EXPECT_FALSE(call(mod, "aiSetup", {ScriptValue::fromString("x")}).asBool());

    // apiKeyEnc 无 passphrase → false（204-205 分支外，216 分支）
    EXPECT_FALSE(call(mod, "aiSetup", {ScriptValue::fromObject({
        {"apiKeyEnc", ScriptValue::fromString("enc")},
    })}).asBool());

    // apiKeyEnc + passphrase 但解密失败 → false（217-218）
    EXPECT_FALSE(call(mod, "aiSetup", {ScriptValue::fromObject({
        {"apiKeyEnc", ScriptValue::fromString("garbage-not-a-cipher")},
        {"passphrase", ScriptValue::fromString("pw")},
    })}).asBool());

    // baseUrl 空 / model 空 → false（220-221）
    EXPECT_FALSE(call(mod, "aiSetup", {ScriptValue::fromObject({
        {"baseUrl", ScriptValue::fromString("")},
        {"model", ScriptValue::fromString("m")},
    })}).asBool());
    EXPECT_FALSE(call(mod, "aiSetup", {ScriptValue::fromObject({
        {"baseUrl", ScriptValue::fromString("http://127.0.0.1:1")},
        {"model", ScriptValue::fromString("")},
    })}).asBool());
    // 两者皆空 → false
    EXPECT_FALSE(call(mod, "aiSetup", {ScriptValue::fromObject({})}).asBool());
}

TEST(VisionAiSetupCoverageTest, AiSetupLocalRejectsAndFails) {
    ModuleDescriptor mod = getModule("vision");
    ASSERT_FALSE(mod.name.empty());

    // 无参 / 非对象 → false（240-241）
    EXPECT_FALSE(call(mod, "aiSetupLocal").asBool());
    EXPECT_FALSE(call(mod, "aiSetupLocal", {ScriptValue::fromString("x")}).asBool());

    // modelPath + labels 数组 + minConfidence：非 ML 构建恒 false；ML 构建下
    // 路径不存在 → loadModel 失败 → false。两态都触达 243-256 体。
    EXPECT_FALSE(call(mod, "aiSetupLocal", {ScriptValue::fromObject({
        {"modelPath", ScriptValue::fromString("/nonexistent/model.onnx")},
        {"labels", ScriptValue::fromArray({
            ScriptValue::fromString("a"), ScriptValue::fromString("b")})},
        {"minConfidence", ScriptValue::fromFloat(0.3f)},
    })}).asBool());
    // labels 非数组（字符串）→ 跳过数组解析分支
    EXPECT_FALSE(call(mod, "aiSetupLocal", {ScriptValue::fromObject({
        {"modelPath", ScriptValue::fromString("/nonexistent/model.onnx")},
        {"labels", ScriptValue::fromString("not-array")},
    })}).asBool());
}

TEST(VisionAiSetupCoverageTest, AiSetupStatusShape) {
    ModuleDescriptor mod = getModule("vision");
    ASSERT_FALSE(mod.name.empty());

    const ScriptValue status = call(mod, "aiSetupStatus");
    ASSERT_TRUE(status.isObject());
    EXPECT_NE(getField(status, "configured"), nullptr);
    EXPECT_NE(getField(status, "localMode"), nullptr);
    EXPECT_NE(getField(status, "baseUrl"), nullptr);
    EXPECT_NE(getField(status, "model"), nullptr);
    EXPECT_NE(getField(status, "hasKey"), nullptr);
    EXPECT_NE(getField(status, "lastError"), nullptr);
}

// 成功配置（纯存储不联网）→ 状态可读 → aiLocate/aiElements 对瞬时拒绝 provider
// 走 not-found 错误面（127.0.0.1:1 拒绝，无挂起）。作为文件末用例，配置态留给后续。
TEST(VisionAiSetupCoverageTest, AiSetupSuccessThenLocateAndElementsRefused) {
    ModuleDescriptor mod = getModule("vision");
    ASSERT_FALSE(mod.name.empty());

    EXPECT_TRUE(call(mod, "aiSetup", {ScriptValue::fromObject({
        {"baseUrl", ScriptValue::fromString("http://127.0.0.1:1")},
        {"model", ScriptValue::fromString("test-model")},
        {"apiKey", ScriptValue::fromString("k")},
        {"timeoutSeconds", ScriptValue::fromInt(2)},
    })}).asBool());

    const ScriptValue status = call(mod, "aiSetupStatus");
    // 成功配置断言仅真实构建成立（stub 构建无 OpenCV 时 setup 为空操作，configured 保持 false）
#ifdef WINGMAN_ENABLE_VISION
    EXPECT_TRUE(fieldBool(status, "configured"));
    EXPECT_FALSE(fieldBool(status, "localMode"));
    EXPECT_TRUE(fieldBool(status, "hasKey"));
#else
    EXPECT_FALSE(fieldBool(status, "configured"));
#endif

    // aiLocate：provider 拒绝 → {found:false, error}（274-277）；stub 构建同样该形态
    const ScriptValue loc = call(mod, "aiLocate", {ScriptValue::fromString("target")});
    ASSERT_TRUE(loc.isObject());
    EXPECT_FALSE(fieldBool(loc, "found"));
    EXPECT_NE(getField(loc, "error"), nullptr);

    // aiElements 两种合法形态：真实构建拒绝 provider → {found:false,error}（284-290）；
    // stub 构建 lastError 空 → {found:false,elements:[]}（303-306）。按 error 键有无区分。
    const ScriptValue elems = call(mod, "aiElements", {ScriptValue::fromString("")});
    ASSERT_TRUE(elems.isObject());
    EXPECT_FALSE(fieldBool(elems, "found"));
    const ScriptValue* elemsErr = getField(elems, "error");
    if (elemsErr) {
        EXPECT_FALSE(elemsErr->asString().empty());
    } else {
        EXPECT_NE(getField(elems, "elements"), nullptr);
    }

    // aiElements 带 region 参数（toRect 分支）
    const ScriptValue region = ScriptValue::fromObject({
        {"x", ScriptValue::fromInt(0)}, {"y", ScriptValue::fromInt(0)},
        {"w", ScriptValue::fromInt(100)}, {"h", ScriptValue::fromInt(100)},
    });
    const ScriptValue elems2 = call(mod, "aiElements",
                                    {ScriptValue::fromString("btn"), region});
    ASSERT_TRUE(elems2.isObject());
    EXPECT_FALSE(fieldBool(elems2, "found"));

    // 恢复全局配置初态（本文件末尾用例配置了单例，避免污染同进程后续用例）
    VisionAi::reset();
}
