#pragma once

#include "wingman/script/iscript_engine.hpp"
#include <sol/sol.hpp>
#include <string>
#include <vector>

namespace wingman {
namespace lua {

// Lua 实现的 IScriptEngine（基于 sol2）
class LuaScriptEngine : public script::IScriptEngine {
public:
	LuaScriptEngine();
	~LuaScriptEngine() override;

	// IScriptEngine 接口
	bool initialize(const script::EngineConfig& config = {}) override;
	void shutdown() override;

	bool executeFile(const std::string& path) override;
	bool executeString(const std::string& code) override;

	bool callFunction(const std::string& name,
	                  const std::vector<script::ScriptValue>& args,
	                  script::ScriptValue& result) override;

	void registerModule(const script::ModuleDescriptor& module) override;

	void setGlobal(const std::string& name, const script::ScriptValue& value) override;
	script::ScriptValue getGlobal(const std::string& name) override;

	std::string getLastError() const override;
	std::string getLanguageName() const override;
	std::vector<std::string> getSupportedExtensions() const override;

	void enableSandbox(const script::EngineConfig& config) override;
	void disableSandbox() override;

	// sol2 特有访问
	sol::state& getState() { return lua_; }
	const sol::state& getState() const { return lua_; }

private:
	sol::state lua_;
	std::string lastError_;
	bool initialized_ = false;
	bool sandboxed_ = false;

	void applySandbox(const script::EngineConfig& config);
};

// 自注册到 ScriptEngineFactory
void registerLuaEngine();

} // namespace lua
} // namespace wingman
