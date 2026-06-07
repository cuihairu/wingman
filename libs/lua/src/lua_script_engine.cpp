#include "wingman/lua/lua_script_engine.hpp"
#include "wingman/lua/lua_marshal.hpp"
#include "wingman/script/script_engine_factory.hpp"
#include "wingman/event.hpp"
#include <memory>
#include <iostream>

namespace wingman {
namespace script {
namespace modules {
	void cleanupFsmModule();
}
}
}

namespace wingman {
namespace lua {

LuaScriptEngine::LuaScriptEngine() = default;

LuaScriptEngine::~LuaScriptEngine() {
	shutdown();
}

bool LuaScriptEngine::initialize(const script::EngineConfig& config) {
	if (initialized_) return true;

	try {
		if (config.sandboxed) {
			// Sandbox mode: only open safe libraries
			lua_.open_libraries(
				sol::lib::base,
				sol::lib::string,
				sol::lib::table,
				sol::lib::math
			);
			// Explicitly remove dangerous modules (even though they weren't loaded above)
			applySandbox(config);
		} else {
			// Non-sandbox mode: open all libraries
			lua_.open_libraries(
				sol::lib::base,
				sol::lib::package,
				sol::lib::string,
				sol::lib::table,
				sol::lib::math,
				sol::lib::io,
				sol::lib::os,
				sol::lib::debug
			);
		}

		// 设置环境变量到 registry
		for (const auto& [k, v] : config.env) {
			lua_[std::string("_ENV_") + k] = v;
		}

		initialized_ = true;
		return true;
	} catch (const std::exception& e) {
		lastError_ = e.what();
		return false;
	}
}

void LuaScriptEngine::shutdown() {
	if (initialized_) {
		lua_ = sol::state();
		// Note: Event subscriptions with lua_ callbacks will become invalid
		// They will be safely skipped during event dispatch due to exception handling
		// The EventHub cleanup is NOT called here to avoid affecting other scripts
		// Clear FSM global state
		script::modules::cleanupFsmModule();
		initialized_ = false;
	}
}

bool LuaScriptEngine::executeFile(const std::string& path) {
	if (!initialized_) return false;

	try {
		auto result = lua_.safe_script_file(path);
		if (!result.valid()) {
			sol::error err = result;
			lastError_ = err.what();
			return false;
		}
		return true;
	} catch (const std::exception& e) {
		lastError_ = e.what();
		return false;
	}
}

bool LuaScriptEngine::executeString(const std::string& code) {
	if (!initialized_) return false;

	try {
		auto result = lua_.safe_script(code);
		if (!result.valid()) {
			sol::error err = result;
			lastError_ = err.what();
			return false;
		}
		return true;
	} catch (const std::exception& e) {
		lastError_ = e.what();
		return false;
	}
}

bool LuaScriptEngine::callFunction(const std::string& name,
                                   const std::vector<script::ScriptValue>& args,
                                   script::ScriptValue& result) {
	if (!initialized_) return false;

	sol::protected_function func = lua_[name];
	if (!func.valid()) {
		lastError_ = "Function not found: " + name;
		return false;
	}

	// 构建 sol 调用参数
	std::vector<sol::object> luaArgs;
	luaArgs.reserve(args.size());
	sol::state_view sv(lua_);
	for (const auto& arg : args) {
		luaArgs.push_back(toLuaObject(sv, arg));
	}

	// 调用
	auto callResult = func(sol::as_args(luaArgs));
	if (!callResult.valid()) {
		sol::error err = callResult;
		lastError_ = err.what();
		return false;
	}

	sol::object retObj = callResult[0];
	result = toScriptValue(retObj);
	return true;
}

void LuaScriptEngine::registerModule(const script::ModuleDescriptor& module) {
	if (!initialized_) return;

	sol::table tbl = lua_.create_table();
	sol::state_view sv(lua_);

	for (const auto& fn : module.functions) {
		// 拷贝 ScriptFunction 到 lambda 捕获中，生命周期由 Lua GC 管理
		script::ScriptFunction funcCopy = fn.func;
		tbl[fn.name] = [sv, funcCopy = std::move(funcCopy)](sol::variadic_args va) mutable -> sol::object {
			std::vector<script::ScriptValue> args;
			args.reserve(va.size());
			for (const auto& a : va) {
				args.push_back(toScriptValue(sol::object(a)));
			}

			script::ScriptValue result;
			try {
				result = funcCopy(args);
			} catch (const std::exception& e) {
				// sol2 会将 Lua 抛出的异常传播
				throw sol::error(e.what());
			}

			return toLuaObject(sv, result);
		};
	}

	lua_[module.name] = tbl;
}

void LuaScriptEngine::setGlobal(const std::string& name, const script::ScriptValue& value) {
	if (!initialized_) return;
	lua_[name] = toLuaObject(lua_, value);
}

script::ScriptValue LuaScriptEngine::getGlobal(const std::string& name) {
	if (!initialized_) return script::ScriptValue::null();
	sol::object obj = lua_[name];
	return toScriptValue(obj);
}

std::string LuaScriptEngine::getLastError() const {
	return lastError_;
}

std::string LuaScriptEngine::getLanguageName() const {
	return "lua";
}

std::vector<std::string> LuaScriptEngine::getSupportedExtensions() const {
	return {".lua"};
}

void LuaScriptEngine::enableSandbox(const script::EngineConfig& config) {
	sandboxed_ = true;
	applySandbox(config);
}

void LuaScriptEngine::disableSandbox() {
	if (!initialized_) return;
	lua_.open_libraries(sol::lib::io, sol::lib::os, sol::lib::debug);
	sandboxed_ = false;
}

void LuaScriptEngine::applySandbox(const script::EngineConfig& /*config*/) {
	// 移除危险库和函数
	lua_["io"] = sol::lua_nil;
	lua_["os"] = sol::lua_nil;
	lua_["debug"] = sol::lua_nil;
	lua_["package"] = sol::lua_nil;
	// 移除文件加载函数
	lua_["dofile"] = sol::lua_nil;
	lua_["loadfile"] = sol::lua_nil;
	lua_["load"] = sol::lua_nil;
	lua_["require"] = sol::lua_nil;
}

// ========== 自注册 ==========

namespace {
struct LuaEngineAutoRegistrar {
	LuaEngineAutoRegistrar() {
		script::ScriptEngineFactory::instance().registerEngine("lua", []() {
			return std::make_unique<LuaScriptEngine>();
		});
	}
};
} // anonymous namespace

void registerLuaEngine() {
	static LuaEngineAutoRegistrar registrar;
}

} // namespace lua
} // namespace wingman
