#include <gtest/gtest.h>
#include "wingman/script/module_registry.hpp"
#include "wingman/script/iscript_engine.hpp"

using namespace wingman;
using namespace wingman::script;
using namespace wingman::script::modules;

// ========== ModuleDescriptor ==========

TEST(ScriptValueTest, FactoryMethods) {
    auto nullVal = ScriptValue::null();
    EXPECT_TRUE(nullVal.isNull());

    auto boolVal = ScriptValue::fromBool(true);
    EXPECT_TRUE(boolVal.isBool());
    EXPECT_TRUE(boolVal.asBool());

    auto intVal = ScriptValue::fromInt(42);
    EXPECT_TRUE(intVal.isInt());
    EXPECT_EQ(intVal.asInt(), 42);

    auto floatVal = ScriptValue::fromFloat(3.14);
    EXPECT_TRUE(floatVal.isFloat());
    EXPECT_DOUBLE_EQ(floatVal.asFloat(), 3.14);

    auto strVal = ScriptValue::fromString("hello");
    EXPECT_TRUE(strVal.isString());
    EXPECT_EQ(strVal.asString(), "hello");

    auto arrVal = ScriptValue::fromArray({ScriptValue::fromInt(1), ScriptValue::fromInt(2)});
    EXPECT_TRUE(arrVal.isArray());
    EXPECT_EQ(arrVal.size(), 2u);
    EXPECT_EQ(arrVal.at(0).asInt(), 1);
}

TEST(ScriptValueTest, ObjectAccess) {
    auto obj = ScriptValue::fromObject({
        {"name", ScriptValue::fromString("test")},
        {"count", ScriptValue::fromInt(5)}
    });
    EXPECT_TRUE(obj.isObject());

    auto* name = obj.get("name");
    ASSERT_NE(name, nullptr);
    EXPECT_EQ(name->asString(), "test");

    auto* missing = obj.get("missing");
    EXPECT_EQ(missing, nullptr);

    auto def = ScriptValue::fromInt(99);
    auto val = obj.get("missing", def);
    EXPECT_EQ(val.asInt(), 99);
}

TEST(ScriptValueTest, ArrayOutOfBounds) {
    auto arr = ScriptValue::fromArray({ScriptValue::fromInt(1)});
    EXPECT_TRUE(arr.at(5).isNull());
}

TEST(ScriptValueTest, TypeMismatchDefaults) {
    ScriptValue v = ScriptValue::fromInt(42);
    EXPECT_FALSE(v.asBool());
    EXPECT_EQ(v.asString(), "");
    EXPECT_DOUBLE_EQ(v.asFloat(), 42.0); // Int → Float conversion
}

// ========== getAllModules ==========

TEST(ModuleRegistryTest, GetAllModulesReturnsNonEmpty) {
    auto modules = getAllModules();
    EXPECT_FALSE(modules.empty());
}

TEST(ModuleRegistryTest, ModulesHaveNames) {
    auto modules = getAllModules();
    for (const auto& mod : modules) {
        EXPECT_FALSE(mod.name.empty()) << "Module should have a non-empty name";
    }
}

TEST(ModuleRegistryTest, ModuleNamesAreUnique) {
    auto modules = getAllModules();
    std::set<std::string> names;
    for (const auto& mod : modules) {
        EXPECT_TRUE(names.find(mod.name) == names.end())
            << "Duplicate module name: " << mod.name;
        names.insert(mod.name);
    }
}

TEST(ModuleRegistryTest, ModulesHaveFunctions) {
    auto modules = getAllModules();
    int totalFunctions = 0;
    for (const auto& mod : modules) {
        totalFunctions += static_cast<int>(mod.functions.size());
    }
    EXPECT_GT(totalFunctions, 0) << "At least some modules should have functions";
}

// ========== EngineConfig ==========

TEST(EngineConfigTest, DefaultValues) {
    EngineConfig cfg;
    EXPECT_FALSE(cfg.sandboxed);
    EXPECT_EQ(cfg.memoryLimit, 100ULL * 1024 * 1024);
    EXPECT_EQ(cfg.instructionLimit, 1000000ULL);
    EXPECT_EQ(cfg.timeLimitMs, 30000ULL);
}

// ========== ModuleDescriptor FunctionEntry ==========

TEST(ModuleDescriptorTest, FunctionEntryCreation) {
    ModuleDescriptor::FunctionEntry entry;
    entry.name = "test_func";
    entry.func = [](const std::vector<ScriptValue>& args) -> ScriptValue {
        return ScriptValue::fromInt(42);
    };
    entry.signature = "-> int";

    EXPECT_EQ(entry.name, "test_func");
    EXPECT_EQ(entry.signature, "-> int");

    ScriptValue result = entry.func({});
    EXPECT_EQ(result.asInt(), 42);
}

// ========== registerAllModules ==========

namespace {

class MockScriptEngine : public IScriptEngine {
public:
    std::vector<std::string> registeredModules;

    bool initialize(const EngineConfig&) override { return true; }
    void shutdown() override {}
    bool executeFile(const std::string&) override { return true; }
    bool executeString(const std::string&) override { return true; }
    bool callFunction(const std::string&, const std::vector<ScriptValue>&, ScriptValue&) override { return true; }
    void registerModule(const ModuleDescriptor& module) override {
        registeredModules.push_back(module.name);
    }
    void setGlobal(const std::string&, const ScriptValue&) override {}
    ScriptValue getGlobal(const std::string&) override { return ScriptValue::null(); }
    std::string getLastError() const override { return ""; }
    std::string getLanguageName() const override { return "mock"; }
    std::vector<std::string> getSupportedExtensions() const override { return {".mock"}; }
    void enableSandbox(const EngineConfig&) override {}
    void disableSandbox() override {}
};

} // anonymous namespace

TEST(ModuleRegistryTest, RegisterAllModules) {
    MockScriptEngine engine;
    registerAllModules(engine);

    auto allModules = getAllModules();
    EXPECT_EQ(engine.registeredModules.size(), allModules.size());
}
