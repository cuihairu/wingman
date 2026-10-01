#include "wingman/script_manager.hpp"
#include "wingman/script/module_registry.hpp"

#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <utility>
#include <future>
#include <atomic>
#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <Windows.h>
#else
#include <sys/stat.h>
#endif

namespace wingman {

namespace {

// 活跃执行态：执行线程正在（或即将）占用该脚本的引擎实例。
// 从这些状态迁出必须经 stopScript_Locked（协作停止 + 释放 manager 侧引擎
// 引用），否则后续重跑可能复用执行线程仍在使用的引擎（数据竞争）。
bool isActiveScriptState(ScriptState state) {
	return state == ScriptState::running ||
	       state == ScriptState::paused ||
	       state == ScriptState::starting ||
	       state == ScriptState::stopping;
}

} // namespace

// ========== ScriptManager Implementation ==========

ScriptManager::ScriptManager() = default;

ScriptManager::~ScriptManager() {
	stopHotReload();
	for (auto& pair : m_scripts) {
		if (pair.second->engine) {
			pair.second->engine->shutdown();
			pair.second->engine.reset();
		}
	}
}

// ========== Engine Creation ==========

std::unique_ptr<script::IScriptEngine> ScriptManager::createEngineForLanguage(const std::string& language, const ScriptConfig& scriptConfig) {
	auto engine = script::ScriptEngineFactory::instance().createEngine(language);
	if (!engine) return nullptr;

	script::EngineConfig config;
	// Use script's sandboxed config instead of hardcoded false
	config.sandboxed = scriptConfig.sandboxed;
	config.memoryLimit = m_sandboxConfig.memoryLimit;
	config.instructionLimit = m_sandboxConfig.instructionLimit;
	config.timeLimitMs = m_sandboxConfig.timeLimitMs;
	config.env = m_env;

	if (!engine->initialize(config)) {
		return nullptr;
	}

	// Register all module descriptors
	script::modules::registerAllModules(*engine);

	return engine;
}

// ========== Script Loading Management ==========

bool ScriptManager::loadScript(const std::string& name, const std::string& path, const ScriptConfig& config) {
	ScriptEventCallback callback;
	std::vector<std::pair<ScriptEvent, std::string>> events;

	{
		std::lock_guard<std::mutex> lock(m_mutex);

		if (!std::filesystem::exists(path)) {
			callback = m_eventCallback;
			events.emplace_back(ScriptEvent::error, "file not found: " + path);
		} else {
			if (m_scripts.find(name) != m_scripts.end()) {
				unloadScript_Locked(name);
				events.emplace_back(ScriptEvent::unloaded, "");
			}

			auto info = std::make_shared<ScriptInfo>();
			info->config = config;
			info->config.name = name;
			info->config.path = path;
			info->lastModified = getFileModifiedTime(path);
			info->language = detectLanguage(path);

			m_scripts[name] = std::move(info);

			callback = m_eventCallback;
			events.emplace_back(ScriptEvent::loaded, "");
		}
	}

	if (callback) {
		for (const auto& [event, message] : events) {
			callback(name, event, message);
		}
	}
	return !events.empty() && events.back().first == ScriptEvent::loaded;
}

bool ScriptManager::unloadScript(const std::string& name) {
	ScriptEventCallback callback;
	bool unloaded = false;

	{
		std::lock_guard<std::mutex> lock(m_mutex);
		unloaded = unloadScript_Locked(name);
		if (unloaded) {
			callback = m_eventCallback;
		}
	}

	if (unloaded && callback) {
		callback(name, ScriptEvent::unloaded, "");
	}
	return unloaded;
}

bool ScriptManager::unloadScript_Locked(const std::string& name) {
	auto it = m_scripts.find(name);
	if (it == m_scripts.end()) {
		return false;
	}

	if (isActiveScriptState(it->second->state)) {
		stopScript_Locked(name);
	}

	m_scripts.erase(it);
	return true;
}

bool ScriptManager::reloadScript(const std::string& name) {
	ScriptEventCallback callback;
	bool reloaded = false;
	bool restart = false;

	{
		std::lock_guard<std::mutex> lock(m_mutex);
		auto it = m_scripts.find(name);
		restart = it != m_scripts.end() && isActiveScriptState(it->second->state);
		reloaded = reloadScript_Locked(name);
		if (reloaded) {
			callback = m_eventCallback;
		}
	}

	if (reloaded && restart) {
		runScript(name);
	}

	if (reloaded && callback) {
		callback(name, ScriptEvent::reloaded, "");
	}
	return reloaded;
}

bool ScriptManager::reloadScript_Locked(const std::string& name) {
	auto it = m_scripts.find(name);
	if (it == m_scripts.end()) {
		return false;
	}

	auto& info = it->second;
	bool wasActive = isActiveScriptState(info->state);

	if (wasActive) {
		stopScript_Locked(name);
	}

	info->lastModified = getFileModifiedTime(info->config.path);
	info->state = ScriptState::loaded;
	info->lastError.clear();

	return true;
}

bool ScriptManager::checkReload(const std::string& name) {
	bool restart = false;
	bool reloaded = false;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		auto it = m_scripts.find(name);
		restart = it != m_scripts.end() && isActiveScriptState(it->second->state);
		reloaded = checkReload_Locked(name);
	}

	if (reloaded && restart) {
		runScript(name);
	}
	return reloaded;
}

bool ScriptManager::checkReload_Locked(const std::string& name) {
	auto it = m_scripts.find(name);
	if (it == m_scripts.end()) {
		return false;
	}

	auto& info = it->second;
	if (!info->config.autoReload && !m_globalAutoReload) {
		return false;
	}

	uint64_t currentModified = getFileModifiedTime(info->config.path);
	if (currentModified > info->lastModified) {
		return reloadScript_Locked(name);
	}

	return false;
}

void ScriptManager::checkAllReloads() {
	std::vector<std::string> restartNames;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		std::vector<std::string> names = getScriptNames_Locked();
		for (const auto& name : names) {
			auto it = m_scripts.find(name);
			bool restart = it != m_scripts.end() && isActiveScriptState(it->second->state);
			if (checkReload_Locked(name) && restart) {
				restartNames.push_back(name);
			}
		}
	}

	for (const auto& name : restartNames) {
		runScript(name);
	}
}

// ========== Script Execution ==========

bool ScriptManager::runScript(const std::string& name) {
	ScriptEventCallback callback;
	ScriptEvent event = ScriptEvent::started;
	std::string message;
	bool result = false;
	bool hasEvent = false;

	{
		std::lock_guard<std::mutex> lock(m_mutex);
		auto it = m_scripts.find(name);
		if (it != m_scripts.end()) {
			callback = m_eventCallback;
		}
	}

	result = runScriptInternal(name);

	{
		std::lock_guard<std::mutex> lock(m_mutex);
		auto it = m_scripts.find(name);
		if (it != m_scripts.end()) {
			callback = m_eventCallback;
			if (result) {
				event = ScriptEvent::started;
				hasEvent = true;
			} else if (it->second->state == ScriptState::error) {
				event = ScriptEvent::error;
				message = it->second->lastError;
				hasEvent = true;
			}
		}
	}

	if (hasEvent && callback) {
		callback(name, event, message);
	}
	return result;
}

bool ScriptManager::runScriptInternal(const std::string& name) {
	std::shared_ptr<ScriptInfo> infoPtr;
	std::shared_ptr<script::IScriptEngine> engineToRun;
	uint32_t myGeneration = 0;

	{
		std::lock_guard<std::mutex> lock(m_mutex);
		auto it = m_scripts.find(name);
		if (it == m_scripts.end()) {
			return false;
		}

		infoPtr = it->second;

		// 重入防护：同步执行模型下 running/starting/paused/stopping 都意味着
		// 执行线程正在（或即将）占用当前引擎实例，放行重跑会让两个执行线程
		// 并发复用同一引擎（数据竞争）。旧实现在此对 running 先 stop 再重启，
		// 但 running 从不可达（见下方 running 赋值点注释），该分支实为死代码。
		if (isActiveScriptState(infoPtr->state)) {
			infoPtr->lastError = "script is already running: " + name;
			return false;
		}

		// Mark as starting (initializing engine)
		infoPtr->state = ScriptState::starting;
		infoPtr->lastError.clear();
		infoPtr->stopRequested = false;
		++infoPtr->runGeneration;
		myGeneration = infoPtr->runGeneration;

		// Create engine instance
		if (!infoPtr->engine) {
			infoPtr->engine = createEngineForLanguage(infoPtr->language, infoPtr->config);
			if (!infoPtr->engine) {
				infoPtr->state = ScriptState::error;
				infoPtr->lastError = "failed to create engine for language: " + infoPtr->language;
				return false;
			}
			// 将脚本引擎的 print/stdout 路由到 logScriptOutput（经 m_outputCallback 下发）。
			auto scriptName = infoPtr->config.name;
			infoPtr->engine->setOutputCallback([this, scriptName](const std::string& output) {
				logScriptOutput(scriptName, output);
			});
		}

		// Set environment variables
		for (const auto& [k, v] : infoPtr->config.env) {
			infoPtr->engine->setGlobal(k, script::ScriptValue::fromString(v));
		}

		// 执行引用必须在锁内取：锁外读取会与并发 stopScript_Locked 的
		// engine.reset() 竞争（可能拿到空引用）
		engineToRun = infoPtr->engine;
	}

	// Execute script file with timeout support
	// Note: true interruptibility requires engine-level cooperation (e.g., debug hooks)
	// This implementation provides timeout detection and state cleanup, but the engine
	// may continue running in the background until it naturally completes or fails.
	// 超时/协作停止路径会 detach 执行线程，因此所有跨 detach 生命周期的对象必须
	// 按值捕获（此前按引用捕获局部栈对象，线程在主线程返回后写悬空引用，实测段
	// 错误）；engine 以 shared_ptr 与线程共享，manager 侧只释放引用（绝不
	// shutdown——那会销毁执行线程正在使用的 lua_State）——detach 后线程跑完
	// 自然销毁引擎，重跑/卸载也不会与执行并发复用同一引擎。
	auto scriptDone = std::make_shared<std::atomic<bool>>(false);
	auto scriptSuccess = std::make_shared<std::atomic<bool>>(false);
	const std::string scriptPath = infoPtr->config.path;
	std::thread execThread;

	// Launch execution in a worker thread
	try {
		execThread = std::thread([engineToRun, scriptPath, scriptDone, scriptSuccess]() {
			scriptSuccess->store(engineToRun->executeFile(scriptPath));
			scriptDone->store(true);
		});
	} catch (const std::exception& e) {
		// 线程创建失败不能停在 starting：重入防护会永久拒绝后续 run/stop
		std::lock_guard<std::mutex> lock(m_mutex);
		infoPtr->state = ScriptState::error;
		infoPtr->lastError = std::string("failed to launch execution thread: ") + e.what();
		infoPtr->engine.reset();
		return false;
	}

	{
		// 执行线程已起动：状态机迁移到 running。此前实现从不进入 running
		// （全文件唯一赋值点在 resumeScript，而 resume 前置 paused、pause 前置
		// running——死锁环），导致 pause/resume 成功腿与 running/paused 状态
		// 映射全部不可达（2026-09-29 覆盖率扫描登记的结构性缺陷）。
		// 复查仍是本次运行的 starting 才迁移：并发 stop 可能已置 stopping，
		// 更新的运行（runGeneration 变化）也不由本次代为迁移。
		std::lock_guard<std::mutex> lock(m_mutex);
		if (infoPtr->state == ScriptState::starting && infoPtr->runGeneration == myGeneration) {
			infoPtr->state = ScriptState::running;
		}
	}

	// Get timeout from config
	int timeoutMs = infoPtr->config.timeoutMs > 0 ? infoPtr->config.timeoutMs : 30000;

	// Wait for completion / cooperative stop / timeout
	auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
	bool timedOut = false;
	bool stopRequested = false;

	while (true) {
		if (scriptDone->load()) {
			break;
		}
		if (infoPtr->stopRequested.load()) {
			stopRequested = true;
			break;
		}
		if (std::chrono::steady_clock::now() >= deadline) {
			timedOut = true;
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}

	if (timedOut || stopRequested) {
		// 超时与协作停止共用收尾：置终态 + detach 执行线程 + 释放 manager 侧
		// 引擎（线程侧 shared_ptr 保活，跑完自然销毁；脚本可能继续在后台跑到
		// 自然结束/失败——引擎级中断[指令钩子]登记为后续独立项）。
		std::lock_guard<std::mutex> lock(m_mutex);
		// Detach the thread to let it complete independently
		if (execThread.joinable()) {
			execThread.detach();
		}
		auto it = m_scripts.find(name);
		const bool superseded = it == m_scripts.end() || it->second != infoPtr ||
		                        infoPtr->runGeneration != myGeneration;
		if (!superseded) {
			// 只处置本次运行的引擎；被超越时 infoPtr->engine 属于更新的运行
			infoPtr->engine.reset();
			if (timedOut) {
				infoPtr->state = ScriptState::error;
				infoPtr->lastError = "Script execution timeout after " + std::to_string(timeoutMs) + "ms";
			} else {
				// stopScript 请求的停止：终态 loaded（区别于 completed/error）
				infoPtr->state = ScriptState::loaded;
				infoPtr->lastError.clear();
			}
		}
		return false;
	}

	// Execution completed - join the thread
	if (execThread.joinable()) {
		execThread.join();
	}

	// Check result
	if (!scriptSuccess->load()) {
		std::lock_guard<std::mutex> lock(m_mutex);
		auto it = m_scripts.find(name);
		if (it != m_scripts.end() && it->second == infoPtr &&
		    infoPtr->runGeneration == myGeneration) {
			infoPtr->state = ScriptState::error;
			infoPtr->lastError = engineToRun->getLastError();
		}
		return false;
	}

	{
		std::lock_guard<std::mutex> lock(m_mutex);
		auto it = m_scripts.find(name);
		if (it == m_scripts.end() || it->second != infoPtr ||
		    infoPtr->runGeneration != myGeneration) {
			return false;
		}

		// Script execution completed successfully
		// Mark as completed since executeFile() has returned
		infoPtr->state = ScriptState::completed;
		infoPtr->lastLoaded = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now().time_since_epoch()
		).count();
	}

	return true;
}

bool ScriptManager::stopScript(const std::string& name) {
	ScriptEventCallback callback;
	bool stopped = false;

	{
		std::lock_guard<std::mutex> lock(m_mutex);
		stopped = stopScript_Locked(name);
		if (stopped) {
			callback = m_eventCallback;
		}
	}

	if (stopped && callback) {
		callback(name, ScriptEvent::stopped, "");
	}
	return stopped;
}

bool ScriptManager::stopScript_Locked(const std::string& name) {
	auto it = m_scripts.find(name);
	if (it == m_scripts.end()) {
		return false;
	}

	if (it->second->state != ScriptState::running &&
		it->second->state != ScriptState::paused &&
		it->second->state != ScriptState::starting) {
		return false;
	}

	// 协作停止：只置停止请求 + 簿记态，不触碰引擎。旧实现直接
	// engine->shutdown()+reset()——shutdown 会销毁执行线程正在使用的
	// lua_State（sol::state 重建），跨线程并发即 use-after-free（stop 成功腿
	// 仅 starting 窗口的并发 stop 可达且必然伴生该竞争，此前从不触发）。
	// 真正的引擎级中断（Lua 指令钩子 / Python trace）登记为后续独立项；
	// 当前语义：执行线程自然跑完（与超时路径相同的 detach + 线程侧
	// shared_ptr 保活模型），runScriptInternal 的等待循环看到 stopRequested
	// 后收尾。
	it->second->stopRequested = true;
	it->second->state = ScriptState::stopping;

	// 释放 manager 侧引擎引用（引用计数操作，线程安全）：确保后续重跑创建
	// 新引擎，不会与仍在执行的旧引擎并发复用；引擎对象由执行线程的
	// shared_ptr 保活、跑完自然销毁。
	it->second->engine.reset();
	return true;
}

bool ScriptManager::pauseScript(const std::string& name) {
	// pause 是簿记级状态：底层脚本线程不被挂起（无引擎级暂停钩子），执行
	// 继续到自然完成/超时/停止；簿记态影响 pause/resume/callFunction 的前置
	// 判定与状态上报（GUI/远程观察到的 Paused）。
	std::lock_guard<std::mutex> lock(m_mutex);

	auto it = m_scripts.find(name);
	if (it == m_scripts.end()) {
		return false;
	}

	if (it->second->state != ScriptState::running) {
		return false;
	}

	it->second->state = ScriptState::paused;
	return true;
}

bool ScriptManager::resumeScript(const std::string& name) {
	std::lock_guard<std::mutex> lock(m_mutex);

	auto it = m_scripts.find(name);
	if (it == m_scripts.end()) {
		return false;
	}

	if (it->second->state != ScriptState::paused) {
		return false;
	}

	it->second->state = ScriptState::running;
	return true;
}

bool ScriptManager::callFunction(const std::string& name, const std::string& func,
                                const std::vector<std::string>& args,
                                std::string* result) {
	// 注意：同步执行模型下 running 窗口 = 引擎正被执行线程独占；此刻跨线程
	// 调用会与 executeFile 并发操作同一引擎（数据竞争）。本接口面向异步执行
	// 模型设计（脚本驻留后按名调用其函数），当前无生产调用方；引擎级并发
	// 安全的调用通道随协作停止钩子一并登记为后续独立项。
	std::lock_guard<std::mutex> lock(m_mutex);

	auto it = m_scripts.find(name);
	if (it == m_scripts.end() || it->second->state != ScriptState::running) {
		return false;
	}

	auto* engine = it->second->engine.get();
	if (!engine) {
		return false;
	}

	// Convert arguments
	std::vector<script::ScriptValue> svArgs;
	svArgs.reserve(args.size());
	for (const auto& arg : args) {
		svArgs.push_back(script::ScriptValue::fromString(arg));
	}

	script::ScriptValue svResult;
	if (!engine->callFunction(func, svArgs, svResult)) {
		it->second->lastError = engine->getLastError();
		return false;
	}

	// Convert result
	if (result) {
		if (svResult.isString()) {
			*result = svResult.asString();
		} else if (svResult.isBool()) {
			*result = svResult.asBool() ? "true" : "false";
		} else if (svResult.isInt()) {
			*result = std::to_string(svResult.asInt());
		} else if (svResult.isFloat()) {
			*result = std::to_string(svResult.asFloat());
		} else {
			*result = "";
		}
	}

	return true;
}

// ========== Configuration Management ==========

bool ScriptManager::loadConfig(const std::string& path) {
	std::lock_guard<std::mutex> lock(m_mutex);

	if (path.empty()) {
		return false;
	}

	if (path.size() > 5) {
		std::string ext = path.substr(path.size() - 5);
		std::transform(ext.begin(), ext.end(), ext.begin(),
					   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		if (ext == ".json") {
			return loadJsonConfig(path);
		}
	}

	if (path.size() > 4) {
		std::string ext = path.substr(path.size() - 4);
		std::transform(ext.begin(), ext.end(), ext.begin(),
					   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		if (ext == ".ini" || ext == ".cfg") {
			return loadIniConfig(path);
		}
	}

	return false;
}

bool ScriptManager::saveConfig(const std::string& path) {
	std::lock_guard<std::mutex> lock(m_mutex);

	std::ofstream file(path);
	if (!file.is_open()) {
		return false;
	}

	for (const auto& pair : m_config) {
		file << pair.first << "=" << pair.second << "\n";
	}

	return true;
}

std::string ScriptManager::getConfig(const std::string& key, const std::string& defaultValue) {
	std::lock_guard<std::mutex> lock(m_mutex);

	auto it = m_config.find(key);
	if (it != m_config.end()) {
		return it->second;
	}
	return defaultValue;
}

void ScriptManager::setConfig(const std::string& key, const std::string& value) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_config[key] = value;
}

std::string ScriptManager::getEnv(const std::string& key) const {
	std::lock_guard<std::mutex> lock(m_mutex);

	auto it = m_env.find(key);
	if (it != m_env.end()) {
		return it->second;
	}

#ifdef _WIN32
	DWORD needed = GetEnvironmentVariableA(key.c_str(), nullptr, 0);
	if (needed > 0) {
		std::string buf(needed - 1, '\0');
		GetEnvironmentVariableA(key.c_str(), buf.data(), needed);
		return buf;
	}
#else
	const char* val = std::getenv(key.c_str());
	if (val) {
		return val;
	}
#endif

	return "";
}

void ScriptManager::setEnv(const std::string& key, const std::string& value) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_env[key] = value;
}

// ========== State Query ==========

std::shared_ptr<ScriptInfo> ScriptManager::getScriptInfo(const std::string& name) {
	std::lock_guard<std::mutex> lock(m_mutex);

	auto it = m_scripts.find(name);
	if (it != m_scripts.end()) {
		return it->second;
	}
	return nullptr;
}

std::vector<std::string> ScriptManager::getScriptNames() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	return getScriptNames_Locked();
}

std::vector<std::string> ScriptManager::getScriptNames_Locked() const {
	std::vector<std::string> names;
	names.reserve(m_scripts.size());
	for (const auto& pair : m_scripts) {
		names.push_back(pair.first);
	}
	return names;
}

std::vector<std::string> ScriptManager::getRunningScripts() const {
	std::lock_guard<std::mutex> lock(m_mutex);

	std::vector<std::string> names;
	for (const auto& pair : m_scripts) {
		if (pair.second->state == ScriptState::running) {
			names.push_back(pair.first);
		}
	}
	return names;
}

bool ScriptManager::hasScript(const std::string& name) const {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_scripts.find(name) != m_scripts.end();
}

// ========== Engine Access ==========

script::IScriptEngine* ScriptManager::getEngine(const std::string& name) {
	std::lock_guard<std::mutex> lock(m_mutex);

	auto it = m_scripts.find(name);
	if (it != m_scripts.end()) {
		return it->second->engine.get();
	}
	return nullptr;
}

std::string ScriptManager::detectLanguage(const std::string& path) const {
	return script::ScriptEngineFactory::instance().detectLanguage(path);
}

std::vector<std::string> ScriptManager::getAvailableLanguages() const {
	return script::ScriptEngineFactory::instance().getAvailableLanguages();
}

// ========== Event Callbacks ==========

void ScriptManager::setEventCallback(ScriptEventCallback callback) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_eventCallback = std::move(callback);
}

void ScriptManager::setOutputCallback(ScriptOutputCallback callback) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_outputCallback = std::move(callback);
}

void ScriptManager::logScriptOutput(const std::string& scriptName, const std::string& output) {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (m_outputCallback) {
		m_outputCallback(scriptName, output);
	}
}

std::vector<ScriptInfo> ScriptManager::getAllScriptInfos() const {
	std::lock_guard<std::mutex> lock(m_mutex);

	std::vector<ScriptInfo> infos;
	infos.reserve(m_scripts.size());
	for (const auto& pair : m_scripts) {
		ScriptInfo info;
		info.config = pair.second->config;
		info.state = pair.second->state;
		info.lastError = pair.second->lastError;
		info.lastModified = pair.second->lastModified;
		info.lastLoaded = pair.second->lastLoaded;
		info.language = pair.second->language;
		info.data = pair.second->data;
		infos.push_back(std::move(info));
	}
	return infos;
}

// ========== Sandbox Management ==========

void ScriptManager::setSandboxConfig(const SandboxConfig& config) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_sandboxConfig = config;
}

const SandboxConfig& ScriptManager::getSandboxConfig() const {
	return m_sandboxConfig;
}

// ========== Hot Reload Control ==========

void ScriptManager::setAutoReload(const std::string& name, bool enabled) {
	std::lock_guard<std::mutex> lock(m_mutex);

	auto it = m_scripts.find(name);
	if (it != m_scripts.end()) {
		it->second->config.autoReload = enabled;
	}
}

void ScriptManager::setGlobalAutoReload(bool enabled) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_globalAutoReload = enabled;
}

void ScriptManager::startHotReload() {
	std::lock_guard<std::mutex> lock(m_mutex);

	if (m_hotReloadRunning) {
		return;
	}

	m_hotReloadRunning = true;
	m_hotReloadThread = std::thread([this]() {
		while (m_hotReloadRunning) {
			std::this_thread::sleep_for(std::chrono::seconds(1));
			checkAllReloads();
		}
	});
}

void ScriptManager::stopHotReload() {
	std::thread threadToJoin;

	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (!m_hotReloadRunning) {
			return;
		}
		m_hotReloadRunning = false;
		if (m_hotReloadThread.joinable()) {
			threadToJoin = std::move(m_hotReloadThread);
		}
	}

	// Join outside the lock to avoid deadlock with the hot-reload thread.
	// The thread checks m_hotReloadRunning every 1s, so join completes promptly.
	if (threadToJoin.joinable()) {
		threadToJoin.join();
	}
}

// ========== Private Helpers ==========

uint64_t ScriptManager::getFileModifiedTime(const std::string& path) {
#ifdef _WIN32
	WIN32_FILE_ATTRIBUTE_DATA data;
	if (GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &data)) {
		LARGE_INTEGER time;
		time.HighPart = data.ftLastWriteTime.dwHighDateTime;
		time.LowPart = data.ftLastWriteTime.dwLowDateTime;
		return static_cast<uint64_t>(time.QuadPart / 10000 - 11644473600000LL);
	}
#else
	struct stat st;
	if (stat(path.c_str(), &st) == 0) {
#if defined(__APPLE__)
		return static_cast<uint64_t>(st.st_mtimespec.tv_sec) * 1000000000ULL +
		       static_cast<uint64_t>(st.st_mtimespec.tv_nsec);
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
		return static_cast<uint64_t>(st.st_mtim.tv_sec) * 1000000000ULL +
		       static_cast<uint64_t>(st.st_mtim.tv_nsec);
#else
		return static_cast<uint64_t>(st.st_mtime) * 1000000000ULL;
#endif
	}
#endif
	return 0;
}

	bool ScriptManager::loadJsonConfig(const std::string& path) {
		std::ifstream file(path);
		if (!file.is_open()) {
			return false;
		}

		try {
			auto j = nlohmann::json::parse(file);
			if (!j.is_object()) {
				return false;
			}

			for (auto it = j.begin(); it != j.end(); ++it) {
				if (it->is_string()) {
					m_config[it.key()] = it->get<std::string>();
				} else {
					m_config[it.key()] = it->dump();
				}
			}
			return true;
		} catch (const nlohmann::json::exception&) {
			return false;
		}
	}

bool ScriptManager::loadIniConfig(const std::string& path) {
	std::ifstream file(path);
	if (!file.is_open()) {
		return false;
	}

	std::string line;
	while (std::getline(file, line)) {
		if (line.empty() || line[0] == ';' || line[0] == '#') {
			continue;
		}

		size_t pos = line.find('=');
		if (pos != std::string::npos) {
			std::string key = line.substr(0, pos);
			std::string value = line.substr(pos + 1);

			key.erase(0, key.find_first_not_of(" \t"));
			key.erase(key.find_last_not_of(" \t") + 1);
			value.erase(0, value.find_first_not_of(" \t"));
			value.erase(value.find_last_not_of(" \t") + 1);

			m_config[key] = value;
		}
	}

	return true;
}

} // namespace wingman
