#pragma once

#include "wingman/script/iscript_engine.hpp"
#include <pybind11/pybind11.h>
#include <string>
#include <vector>

namespace py = pybind11;

namespace wingman {
namespace python {

// Python 实现的 IScriptEngine（嵌入式 CPython + pybind11）
class PythonScriptEngine : public script::IScriptEngine {
public:
	PythonScriptEngine();
	~PythonScriptEngine() override;

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

	// Python 特有访问
	py::module_& mainModule() { return mainModule_; }

private:
	py::module_ mainModule_;
	py::dict globals_;
	std::string lastError_;
	bool initialized_ = false;
	bool sandboxed_ = false;

	// 是否由本实例初始化 CPython（第一个引擎负责初始化和终止）
	bool ownsInterpreter_ = false;

	void applySandbox(const script::EngineConfig& config);
};

// 自注册到 ScriptEngineFactory
void registerPythonEngine();

} // namespace python
} // namespace wingman
