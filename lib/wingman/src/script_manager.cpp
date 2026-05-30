#include "wingman/script_manager.hpp"
#include "wingman/script/module_registry.hpp"

#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <utility>

#ifdef _WIN32
#include <Windows.h>
#else
#include <sys/stat.h>
#endif

namespace wingman {

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

std::unique_ptr<script::IScriptEngine> ScriptManager::createEngineForLanguage(const std::string& language) {
	auto engine = script::ScriptEngineFactory::instance().createEngine(language);
	if (!engine) return nullptr;

	script::EngineConfig config;
	config.sandboxed = false;
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

			auto info = std::make_unique<ScriptInfo>();
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

	if (it->second->state == ScriptState::running) {
		stopScript_Locked(name);
	}

	m_scripts.erase(it);
	return true;
}

bool ScriptManager::reloadScript(const std::string& name) {
	ScriptEventCallback callback;
	bool reloaded = false;

	{
		std::lock_guard<std::mutex> lock(m_mutex);
		reloaded = reloadScript_Locked(name);
		if (reloaded) {
			callback = m_eventCallback;
		}
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
	bool wasRunning = info->state == ScriptState::running;

	if (wasRunning) {
		stopScript_Locked(name);
	}

	info->lastModified = getFileModifiedTime(info->config.path);
	info->state = ScriptState::loaded;
	info->lastError.clear();

	if (wasRunning) {
		runScript_Locked(name);
	}

	return true;
}

bool ScriptManager::checkReload(const std::string& name) {
	std::lock_guard<std::mutex> lock(m_mutex);
	return checkReload_Locked(name);
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
	std::lock_guard<std::mutex> lock(m_mutex);
	checkAllReloads_Locked();
}

void ScriptManager::checkAllReloads_Locked() {
	std::vector<std::string> names = getScriptNames_Locked();
	for (const auto& name : names) {
		checkReload_Locked(name);
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
		result = runScript_Locked(name);
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

bool ScriptManager::runScript_Locked(const std::string& name) {
	auto it = m_scripts.find(name);
	if (it == m_scripts.end()) {
		return false;
	}

	auto& infoPtr = it->second;

	if (infoPtr->state == ScriptState::running) {
		stopScript_Locked(name);
	}

	// Create engine instance
	if (!infoPtr->engine) {
		infoPtr->engine = createEngineForLanguage(infoPtr->language);
		if (!infoPtr->engine) {
			infoPtr->state = ScriptState::error;
			infoPtr->lastError = "failed to create engine for language: " + infoPtr->language;
			return false;
		}
	}

	// Set environment variables
	for (const auto& [k, v] : infoPtr->config.env) {
		infoPtr->engine->setGlobal(k, script::ScriptValue::fromString(v));
	}

	// Execute script file
	if (!infoPtr->engine->executeFile(infoPtr->config.path)) {
		infoPtr->state = ScriptState::error;
		infoPtr->lastError = infoPtr->engine->getLastError();
		return false;
	}

	infoPtr->state = ScriptState::running;
	infoPtr->lastLoaded = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now().time_since_epoch()
	).count();

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

	if (it->second->state != ScriptState::running && it->second->state != ScriptState::paused) {
		return false;
	}

	it->second->state = ScriptState::loaded;
	return true;
}

bool ScriptManager::pauseScript(const std::string& name) {
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
		std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
		if (ext == ".json") {
			return loadJsonConfig(path);
		}
	}

	if (path.size() > 4) {
		std::string ext = path.substr(path.size() - 4);
		std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
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
	char buffer[4096];
	if (GetEnvironmentVariableA(key.c_str(), buffer, sizeof(buffer)) > 0) {
		return buffer;
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

ScriptInfo* ScriptManager::getScriptInfo(const std::string& name) {
	std::lock_guard<std::mutex> lock(m_mutex);

	auto it = m_scripts.find(name);
	if (it != m_scripts.end()) {
		return it->second.get();
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

	// Join outside the lock to avoid deadlock with the hot-reload thread
	if (threadToJoin.joinable()) {
		threadToJoin.join();
	}
}

// ========== Private Helpers ==========

bool ScriptManager::checkTimeLimit(const std::string& name) {
	auto it = m_scripts.find(name);
	if (it == m_scripts.end()) {
		return true;
	}

	uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now().time_since_epoch()
	).count();

	if (it->second->config.timeoutMs > 0 &&
		now - it->second->lastLoaded > static_cast<uint64_t>(it->second->config.timeoutMs)) {
		return false;
	}

	return true;
}

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
		return static_cast<uint64_t>(st.st_mtime) * 1000;
	}
#endif
	return 0;
}

bool ScriptManager::loadJsonConfig(const std::string& /*path*/) {
	// TODO: Parse using nlohmann/json
	return false;
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

void ScriptManager::triggerEvent(const std::string& name, ScriptEvent event, const std::string& message) {
	ScriptEventCallback callback;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		callback = m_eventCallback;
	}
	if (callback) {
		callback(name, event, message);
	}
}

void ScriptManager::triggerEventUnlocked(const std::string& name, ScriptEvent event, const std::string& message) {
	if (m_eventCallback) {
		m_eventCallback(name, event, message);
	}
}

} // namespace wingman
