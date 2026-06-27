#include "wingman/python/python_script_engine.hpp"
#include "wingman/python/python_marshal.hpp"
#include "wingman/script/script_engine_factory.hpp"
#include "wingman/event.hpp"
#include <memory>
#include <iostream>
#include <filesystem>
#include <mutex>
#include <unordered_map>

namespace wingman {
namespace script {
namespace modules {
	void cleanupFsmModule();
}
}
}

namespace wingman {
namespace python {

// 跟踪 CPython 解释器是否已初始化（全局共享，进程只初始化一次）
static bool g_interpreterInitialized = false;

namespace {
std::mutex g_outputCallbacksMutex;
std::unordered_map<uintptr_t, std::function<void(const std::string&)>> g_outputCallbacks;
std::unordered_map<uintptr_t, std::string> g_stdoutBuffers;
std::unordered_map<uintptr_t, std::string> g_stderrBuffers;

void forwardCapturedOutput(uintptr_t engineId, const std::string& text) {
	std::function<void(const std::string&)> callback;
	{
		std::lock_guard<std::mutex> lock(g_outputCallbacksMutex);
		auto it = g_outputCallbacks.find(engineId);
		if (it != g_outputCallbacks.end()) {
			callback = it->second;
		}
	}

	if (callback) {
		callback(text);
	}
}

void captureStreamWrite(uintptr_t engineId, bool stderr, const std::string& text) {
	if (text.empty()) return;

	std::vector<std::string> completedLines;
	{
		std::lock_guard<std::mutex> lock(g_outputCallbacksMutex);
		auto& buffer = stderr ? g_stderrBuffers[engineId] : g_stdoutBuffers[engineId];
		buffer += text;

		size_t newlinePos = std::string::npos;
		while ((newlinePos = buffer.find('\n')) != std::string::npos) {
			std::string line = buffer.substr(0, newlinePos);
			if (!line.empty() && line.back() == '\r') {
				line.pop_back();
			}
			completedLines.push_back(std::move(line));
			buffer.erase(0, newlinePos + 1);
		}
	}

	for (const auto& line : completedLines) {
		forwardCapturedOutput(engineId, line);
	}
}

void flushStreamBuffer(uintptr_t engineId, bool stderr) {
	std::string pending;
	{
		std::lock_guard<std::mutex> lock(g_outputCallbacksMutex);
		auto& buffers = stderr ? g_stderrBuffers : g_stdoutBuffers;
		auto it = buffers.find(engineId);
		if (it != buffers.end()) {
			pending = std::move(it->second);
			it->second.clear();
		}
	}

	if (!pending.empty()) {
		if (!pending.empty() && pending.back() == '\r') {
			pending.pop_back();
		}
		forwardCapturedOutput(engineId, pending);
	}
}

void clearOutputState(uintptr_t engineId) {
	std::lock_guard<std::mutex> lock(g_outputCallbacksMutex);
	g_outputCallbacks.erase(engineId);
	g_stdoutBuffers.erase(engineId);
	g_stderrBuffers.erase(engineId);
}
} // anonymous namespace

// camelCase -> snake_case 转换，使模块函数名同时以 pythonic 形式暴露。
//
// 算法：在大写字母前插入下划线并转小写；首字母前不插；连续大写
// （如 URL、HTTP）作为一个整体处理，下划线只插在该连续大写段的最后一个
// 大写字母之前（parseURL -> parse_url, getHTTP -> get_http）。
//
// 放在 wingman::python 命名空间（非匿名）以便单元测试直接调用，但只在该
// 翻译单元内定义，不进公共头文件，避免污染公开 API。
std::string camelToSnake(const std::string& camel) {
	if (camel.empty()) return {};

	std::string out;
	out.reserve(camel.size() + 4); // 预留少量下划线空间

	for (size_t i = 0; i < camel.size(); ++i) {
		const char c = camel[i];
		const bool isUpper = c >= 'A' && c <= 'Z';

		if (isUpper && i > 0) {
			const char prev = camel[i - 1];
			const bool prevIsLower = prev >= 'a' && prev <= 'z';
			const bool prevIsUnderscore = prev == '_';
			// 当前字符后面是否为小写（用于判断连续大写段的尾部，如 URL 的 L）
			const bool nextIsLower = (i + 1 < camel.size()) &&
			                        (camel[i + 1] >= 'a' && camel[i + 1] <= 'z');

			// 在以下情况插入下划线：
			//  - 前一个是字母（小写）：getForeground 的 F 前
			//  - 前一个是下划线：避免双下划线
			//  - 连续大写段的最后一个小写前：parseURL 的 L 前
			if (prevIsLower) {
				out.push_back('_');
			} else if (prevIsUnderscore) {
				// 已有下划线后的大写，不再额外加下划线
			} else if (nextIsLower) {
				// 前面也是大写、后面是小写 -> 连续大写段的边界
				out.push_back('_');
			}
		}

		out.push_back(static_cast<char>(tolower(static_cast<unsigned char>(c))));
	}

	return out;
}

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

		// 设置环境变量 (在沙箱之前，因为需要 os 模块)
		if (!config.sandboxed) {
			for (const auto& [k, v] : config.env) {
				py::module_::import("os").attr("environ")[k] = v;
			}
		}

		if (config.sandboxed) {
			applySandbox(config);
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
		if (outputCallback_) {
			installOutputCapture();
		}
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
		flushOutputCapture();
		py::module_ sys = py::module_::import("sys");
		if (previousStdout_ && !previousStdout_.is_none()) {
			sys.attr("stdout") = previousStdout_;
		}
		if (previousStderr_ && !previousStderr_.is_none()) {
			sys.attr("stderr") = previousStderr_;
		}
		stdoutProxy_ = py::object();
		stderrProxy_ = py::object();
		previousStdout_ = py::object();
		previousStderr_ = py::object();
		globals_ = py::dict();
		mainModule_ = py::module_();
	} catch (...) {
		// 忽略 shutdown 时的异常
	}

	clearOutputState(reinterpret_cast<uintptr_t>(this));

	// Note: Event subscriptions with Python callbacks will become invalid
	// They will be safely skipped during event dispatch due to exception handling
	// The EventHub cleanup is NOT called here to avoid affecting other scripts
	// Clear FSM global state
	script::modules::cleanupFsmModule();

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
		flushOutputCapture();
		return true;
	} catch (const py::error_already_set& e) {
		flushOutputCapture();
		lastError_ = e.what();
		return false;
	}
}

bool PythonScriptEngine::executeString(const std::string& code) {
	if (!initialized_) return false;

	try {
		py::gil_scoped_acquire gil;
		py::exec(code, globals_);
		flushOutputCapture();
		return true;
	} catch (const py::error_already_set& e) {
		flushOutputCapture();
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
		flushOutputCapture();
		result = toScriptValue(pyResult);
		return true;
	} catch (const py::error_already_set& e) {
		flushOutputCapture();
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
			// 将 ScriptFunction 包装为 Python 可调用对象的工厂：
			// 同一份包装逻辑为 camelCase（原始名，兼容）和 snake_case（pythonic）
			// 两个属性名各生成一个 py::cpp_function。
			auto makeCallable = [&fn]() -> py::cpp_function {
				script::ScriptFunction funcCopy = fn.func;
				return py::cpp_function(
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
			};

			// 保留原始 camelCase 名（兼容已有脚本与 Lua 一致性）
			moduleObject.attr(fn.name.c_str()) = makeCallable();

			// 额外注册 snake_case 别名（PEP 8 pythonic 风格）
			const std::string snakeName = camelToSnake(fn.name);
			if (!snakeName.empty() && snakeName != fn.name) {
				moduleObject.attr(snakeName.c_str()) = makeCallable();
			}
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

void PythonScriptEngine::setOutputCallback(const std::function<void(const std::string&)>& callback) {
	if (initialized_ && !callback && outputCallback_) {
		py::gil_scoped_acquire gil;
		flushOutputCapture();
		py::module_ sys = py::module_::import("sys");
		if (previousStdout_ && !previousStdout_.is_none()) {
			sys.attr("stdout") = previousStdout_;
		}
		if (previousStderr_ && !previousStderr_.is_none()) {
			sys.attr("stderr") = previousStderr_;
		}
		stdoutProxy_ = py::object();
		stderrProxy_ = py::object();
		previousStdout_ = py::object();
		previousStderr_ = py::object();
	}

	outputCallback_ = callback;
	const auto engineId = reinterpret_cast<uintptr_t>(this);
	{
		std::lock_guard<std::mutex> lock(g_outputCallbacksMutex);
		if (outputCallback_) {
			g_outputCallbacks[engineId] = outputCallback_;
		} else {
			g_outputCallbacks.erase(engineId);
		}
	}

	if (!initialized_) return;
	py::gil_scoped_acquire gil;
	if (outputCallback_) {
		installOutputCapture();
	}
}

void PythonScriptEngine::installOutputCapture() {
	if (!initialized_) return;

	const auto engineId = reinterpret_cast<uintptr_t>(this);
	py::module_ types = py::module_::import("types");
	py::module_ sys = py::module_::import("sys");

	if (!previousStdout_ || previousStdout_.is_none()) {
		previousStdout_ = sys.attr("stdout");
	}
	if (!previousStderr_ || previousStderr_.is_none()) {
		previousStderr_ = sys.attr("stderr");
	}

	auto makeProxy = [&](const char* streamName, bool stderr) {
		py::object proxy = types.attr("SimpleNamespace")();
		proxy.attr("encoding") = py::str("utf-8");
		proxy.attr("closed") = py::bool_(false);
		proxy.attr("isatty") = py::cpp_function([]() { return false; });
		proxy.attr("flush") = py::cpp_function([engineId, stderr]() {
			flushStreamBuffer(engineId, stderr);
		});
		proxy.attr("write") = py::cpp_function([engineId, stderr](const py::object& data) -> py::int_ {
			py::str textObject = py::str(data);
			std::string text = textObject.cast<std::string>();
			captureStreamWrite(engineId, stderr, text);
			return py::int_(text.size());
		});
		proxy.attr("writelines") = py::cpp_function([engineId, stderr](const py::iterable& lines) {
			for (const auto& line : lines) {
				captureStreamWrite(engineId, stderr, py::str(line).cast<std::string>());
			}
		});
		proxy.attr("name") = py::str(streamName);
		return proxy;
	};

	stdoutProxy_ = makeProxy("wingman.stdout", false);
	stderrProxy_ = makeProxy("wingman.stderr", true);
	sys.attr("stdout") = stdoutProxy_;
	sys.attr("stderr") = stderrProxy_;
}

void PythonScriptEngine::flushOutputCapture() {
	const auto engineId = reinterpret_cast<uintptr_t>(this);
	flushStreamBuffer(engineId, false);
	flushStreamBuffer(engineId, true);
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

	// 移除危险模块（防止通过 import 访问）
	globals_["os"] = py::none();
	globals_["sys"] = py::none();
	globals_["subprocess"] = py::none();
	globals_["io"] = py::none();
	globals_["importlib"] = py::none();
	globals_["__import__"] = py::none();  // 禁用 import
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
