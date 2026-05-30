#include "wingman/python/python_script_engine.hpp"
#include "wingman/python/python_marshal.hpp"
#include "wingman/script/script_engine_factory.hpp"
#include <memory>
#include <iostream>
#include <filesystem>

namespace wingman {
namespace python {

// 跟踪 CPython 解释器是否已初始化（全局共享，进程只初始化一次）
static bool g_interpreterInitialized = false;

PythonScriptEngine::PythonScriptEngine() = default;

PythonScriptEngine::~PythonScriptEngine() {
	shutdown();
}

bool PythonScriptEngine::initialize(const script::EngineConfig& config) {
	if (initialized_) return true;

	try {
		// 首次初始化 CPython 解释器
		if (!g_interpreterInitialized) {
			Py_Initialize();
			g_interpreterInitialized = true;
			ownsInterpreter_ = true;
		}

		// 确保当前线程有 GIL
		py::gil_scoped_acquire gil;

		// 创建隔离的 globals dict
		globals_ = py::dict();
		// 导入 builtins 使基本函数可用
		globals_["__builtins__"] = py::module_::import("builtins");

		// 创建主模块命名空间
		mainModule_ = py::module_::import("__main__");

		if (config.sandboxed) {
			applySandbox(config);
		}

		// 设置环境变量
		for (const auto& [k, v] : config.env) {
			py::module_::import("os").attr("environ")[k] = v;
		}

		// Create the root Wingman package up front so every registered module
		// lives under a single public namespace: wingman.<module>.
		py::module_ types = py::module_::import("types");
		py::module_ sys = py::module_::import("sys");
		py::dict sysModules = sys.attr("modules").cast<py::dict>();
		py::object wingmanModule = types.attr("ModuleType")("wingman");
		wingmanModule.attr("__path__") = py::list();
		sysModules["wingman"] = wingmanModule;
		globals_["wingman"] = wingmanModule;

		initialized_ = true;
		return true;
	} catch (const py::error_already_set& e) {
		lastError_ = e.what();
		return false;
	}
}

void PythonScriptEngine::shutdown() {
	if (!initialized_) return;

	try {
		py::gil_scoped_acquire gil;
		globals_ = py::dict();
		mainModule_ = py::module_();
	} catch (...) {
		// 忽略 shutdown 时的异常
	}

	// 注意：不调用 Py_Finalize()，因为其他 Python 对象可能仍存在
	// CPython 会在进程退出时清理
	initialized_ = false;
}

bool PythonScriptEngine::executeFile(const std::string& path) {
	if (!initialized_) return false;

	try {
		py::gil_scoped_acquire gil;

		// 读取文件内容
		std::ifstream file(path);
		if (!file.is_open()) {
			lastError_ = "Cannot open file: " + path;
			return false;
		}
		std::string code((std::istreambuf_iterator<char>(file)),
		                  std::istreambuf_iterator<char>());

		// 在隔离的 globals 中执行
		py::exec(code, globals_);
		return true;
	} catch (const py::error_already_set& e) {
		lastError_ = e.what();
		return false;
	}
}

bool PythonScriptEngine::executeString(const std::string& code) {
	if (!initialized_) return false;

	try {
		py::gil_scoped_acquire gil;
		py::exec(code, globals_);
		return true;
	} catch (const py::error_already_set& e) {
		lastError_ = e.what();
		return false;
	}
}

bool PythonScriptEngine::callFunction(const std::string& name,
                                      const std::vector<script::ScriptValue>& args,
                                      script::ScriptValue& result) {
	if (!initialized_) return false;

	try {
		py::gil_scoped_acquire gil;

		// 从 globals 查找函数
		if (!globals_.contains(name.c_str())) {
			lastError_ = "Function not found: " + name;
			return false;
		}

		py::object func = globals_[name.c_str()];
		if (!py::isinstance<py::function>(func)) {
			lastError_ = name + " is not a function";
			return false;
		}

		// 构建参数
		py::tuple pyArgs(args.size());
		for (size_t i = 0; i < args.size(); ++i) {
			pyArgs[i] = toPythonObject(args[i]);
		}

		// 调用（解包 tuple）
		py::object pyResult = func(*pyArgs);
		result = toScriptValue(pyResult);
		return true;
	} catch (const py::error_already_set& e) {
		lastError_ = e.what();
		return false;
	}
}

void PythonScriptEngine::registerModule(const script::ModuleDescriptor& module) {
	if (!initialized_) return;

	try {
		py::gil_scoped_acquire gil;

		py::module_ types = py::module_::import("types");
		py::module_ sys = py::module_::import("sys");
		py::dict sysModules = sys.attr("modules").cast<py::dict>();

		py::object wingmanModule;
		if (sysModules.contains("wingman")) {
			wingmanModule = sysModules["wingman"];
		} else {
			wingmanModule = types.attr("ModuleType")("wingman");
			wingmanModule.attr("__path__") = py::list();
			sysModules["wingman"] = wingmanModule;
			globals_["wingman"] = wingmanModule;
		}

		const std::string fullName = "wingman." + module.name;
		py::object moduleObject = types.attr("ModuleType")(fullName);

		for (const auto& fn : module.functions) {
			// 将 ScriptFunction 包装为 Python 可调用对象
			script::ScriptFunction funcCopy = fn.func;

			moduleObject.attr(fn.name.c_str()) = py::cpp_function(
				[funcCopy = std::move(funcCopy)](py::args args) -> py::object {
					std::vector<script::ScriptValue> svArgs;
					svArgs.reserve(args.size());
					for (py::ssize_t i = 0; i < args.size(); ++i) {
						svArgs.push_back(toScriptValue(args[i].cast<py::object>()));
					}

					script::ScriptValue result;
					try {
						result = funcCopy(svArgs);
					} catch (const std::exception& e) {
						PyErr_SetString(PyExc_RuntimeError, e.what());
						throw py::error_already_set();
					}

					return toPythonObject(result);
				}
			);
		}

		wingmanModule.attr(module.name.c_str()) = moduleObject;
		sysModules[fullName.c_str()] = moduleObject;

	} catch (const py::error_already_set& e) {
		lastError_ = e.what();
		std::cerr << "Python registerModule error: " << e.what() << "\n";
	}
}

void PythonScriptEngine::setGlobal(const std::string& name, const script::ScriptValue& value) {
	if (!initialized_) return;
	py::gil_scoped_acquire gil;
	globals_[name.c_str()] = toPythonObject(value);
}

script::ScriptValue PythonScriptEngine::getGlobal(const std::string& name) {
	if (!initialized_) return script::ScriptValue::null();
	py::gil_scoped_acquire gil;
	if (!globals_.contains(name.c_str())) {
		return script::ScriptValue::null();
	}
	return toScriptValue(globals_[name.c_str()].cast<py::object>());
}

std::string PythonScriptEngine::getLastError() const {
	return lastError_;
}

std::string PythonScriptEngine::getLanguageName() const {
	return "python";
}

std::vector<std::string> PythonScriptEngine::getSupportedExtensions() const {
	return {".py"};
}

void PythonScriptEngine::enableSandbox(const script::EngineConfig& config) {
	sandboxed_ = true;
	applySandbox(config);
}

void PythonScriptEngine::disableSandbox() {
	if (!initialized_) return;
	// 恢复完整的 builtins
	py::gil_scoped_acquire gil;
	globals_["__builtins__"] = py::module_::import("builtins");
	sandboxed_ = false;
}

void PythonScriptEngine::applySandbox(const script::EngineConfig& config) {
	if (!initialized_) return;

	py::gil_scoped_acquire gil;

	// 创建受限的 builtins
	py::dict safeBuiltins;
	py::module_ builtins = py::module_::import("builtins");

	// 只保留安全的内建函数
	const char* safeNames[] = {
		"print", "len", "range", "int", "float", "str", "bool",
		"list", "dict", "tuple", "set", "type", "isinstance",
		"abs", "min", "max", "sum", "round", "enumerate",
		"zip", "map", "filter", "sorted", "reversed",
		"None", "True", "False", "Exception", "ValueError",
		"TypeError", "RuntimeError", "KeyError", "IndexError",
		nullptr
	};

	for (int i = 0; safeNames[i]; ++i) {
		std::string name = safeNames[i];
		if (builtins.contains(name.c_str())) {
			safeBuiltins[name.c_str()] = builtins.attr(name.c_str());
		}
	}

	globals_["__builtins__"] = safeBuiltins;
}

// ========== 自注册 ==========

namespace {
struct PythonEngineAutoRegistrar {
	PythonEngineAutoRegistrar() {
		script::ScriptEngineFactory::instance().registerEngine("python", []() {
			return std::make_unique<PythonScriptEngine>();
		});
	}
};
} // anonymous namespace

void registerPythonEngine() {
	static PythonEngineAutoRegistrar registrar;
}

} // namespace python
} // namespace wingman
