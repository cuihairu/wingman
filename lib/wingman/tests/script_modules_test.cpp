#include <gtest/gtest.h>
#include "wingman/script/module_registry.hpp"
#include "wingman/script/iscript_engine.hpp"
#include "wingman/game_profile.hpp"

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

// ========== Config Module Coverage ==========

namespace {

// Helper: find a function entry in a module by name
const ModuleDescriptor::FunctionEntry* findFunction(const ModuleDescriptor& mod, const std::string& name) {
    for (const auto& f : mod.functions) {
        if (f.name == name) return &f;
    }
    return nullptr;
}

// Helper: get config module (must keep vector alive)
ModuleDescriptor getConfigModule() {
    auto modules = getAllModules();
    for (auto& mod : modules) {
        if (mod.name == "config") return mod;
    }
    return {};
}

// Helper: get gameprofile module
ModuleDescriptor getGameProfileModule() {
    auto modules = getAllModules();
    for (auto& mod : modules) {
        if (mod.name == "gameprofile") return mod;
    }
    return {};
}

} // anonymous namespace

TEST(ConfigModuleTest, SetAndGetPlainStringTriggersCatchBlock) {
    auto mod = getConfigModule();
    ASSERT_FALSE(mod.name.empty()) << "config module not found in registry";

    const auto* setFn = findFunction(mod, "set");
    const auto* getFn = findFunction(mod, "get");
    ASSERT_NE(setFn, nullptr);
    ASSERT_NE(getFn, nullptr);

    // Store a plain string (not valid JSON) — triggers unwrapConfigString catch block
    (*setFn)({ScriptValue::fromString("coverage_plain_key"), ScriptValue::fromString("just a plain string")});

    auto result = (*getFn)({ScriptValue::fromString("coverage_plain_key")});
    EXPECT_TRUE(result.isString());
    // The plain string should pass through unchanged
    EXPECT_EQ(result.asString(), "just a plain string");
}

TEST(ConfigModuleTest, SetAndGetJsonValueUnwrapped) {
    auto mod = getConfigModule();
    ASSERT_FALSE(mod.name.empty());

    const auto* setFn = findFunction(mod, "set");
    const auto* getFn = findFunction(mod, "get");
    ASSERT_NE(setFn, nullptr);
    ASSERT_NE(getFn, nullptr);

    // Store a JSON string — unwrapConfigString should parse and return the inner string
    (*setFn)({ScriptValue::fromString("coverage_json_key"), ScriptValue::fromString(R"("hello world")")});

    auto result = (*getFn)({ScriptValue::fromString("coverage_json_key")});
    EXPECT_TRUE(result.isString());
    EXPECT_EQ(result.asString(), "hello world");
}

TEST(ConfigModuleTest, GetMissingKeyReturnsNull) {
    auto mod = getConfigModule();
    ASSERT_FALSE(mod.name.empty());

    const auto* getFn = findFunction(mod, "get");
    ASSERT_NE(getFn, nullptr);

    auto result = (*getFn)({ScriptValue::fromString("nonexistent_coverage_key_999")});
    EXPECT_TRUE(result.isNull());
}

// ========== GameProfile Module Coverage ==========

TEST(GameProfileModuleTest, GetProfileSuccessPath) {
    auto mod = getGameProfileModule();
    ASSERT_FALSE(mod.name.empty()) << "gameprofile module not found in registry";

    const auto* getFn = findFunction(mod, "get");
    ASSERT_NE(getFn, nullptr);

    // Create and save a profile
    auto& mgr = GameProfileManager::instance();
    GameProfile profile;
    profile.id = "coverage_get_test";
    profile.name = "Coverage Get";
    profile.window.title = "CoverageGetWnd";
    ASSERT_TRUE(mgr.saveProfile(profile));

    // Call get — should hit the success path (line 25)
    auto result = (*getFn)({ScriptValue::fromString("coverage_get_test")});
    EXPECT_TRUE(result.isArray());
    EXPECT_EQ(result.size(), 2u);
    EXPECT_EQ(result.at(0).asString(), "Coverage Get");
    EXPECT_EQ(result.at(1).asString(), "CoverageGetWnd");

    mgr.deleteProfile("coverage_get_test");
}

TEST(GameProfileModuleTest, GetActiveProfileSuccessPath) {
    auto mod = getGameProfileModule();
    ASSERT_FALSE(mod.name.empty());

    const auto* getActiveFn = findFunction(mod, "getActive");
    ASSERT_NE(getActiveFn, nullptr);

    // Create and save a profile, then set it as active
    auto& mgr = GameProfileManager::instance();
    GameProfile profile;
    profile.id = "coverage_active_test";
    profile.name = "Coverage Active";
    profile.window.title = "CoverageActiveWnd";
    ASSERT_TRUE(mgr.saveProfile(profile));
    ASSERT_TRUE(mgr.setActiveProfile("coverage_active_test"));

    // Call getActive — should hit the success path (line 36)
    auto result = (*getActiveFn)({});
    EXPECT_TRUE(result.isArray());
    EXPECT_EQ(result.size(), 2u);
    EXPECT_EQ(result.at(0).asString(), "coverage_active_test");
    EXPECT_EQ(result.at(1).asString(), "Coverage Active");

    mgr.deleteProfile("coverage_active_test");
}

TEST(GameProfileModuleTest, FindByWindowSuccessPath) {
    auto mod = getGameProfileModule();
    ASSERT_FALSE(mod.name.empty());

    const auto* findByWindowFn = findFunction(mod, "findByWindow");
    ASSERT_NE(findByWindowFn, nullptr);

    // Create and save a profile with a known window title
    auto& mgr = GameProfileManager::instance();
    GameProfile profile;
    profile.id = "coverage_findbywin_test";
    profile.name = "Coverage FindByWindow";
    profile.window.title = "UniqueCoverageWindow_12345";
    profile.window.exactMatch = false;
    ASSERT_TRUE(mgr.saveProfile(profile));

    // Call findByWindow — should hit the success path (line 58)
    auto result = (*findByWindowFn)({ScriptValue::fromString("Prefix_UniqueCoverageWindow_12345_Suffix")});
    EXPECT_TRUE(result.isArray());
    EXPECT_EQ(result.size(), 2u);
    EXPECT_EQ(result.at(0).asString(), "coverage_findbywin_test");
    EXPECT_EQ(result.at(1).asString(), "Coverage FindByWindow");

    mgr.deleteProfile("coverage_findbywin_test");
}
