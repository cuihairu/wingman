#pragma once

#include "wingman/script/iscript_engine.hpp"
#include "wingman/script/script_engine_factory.hpp"
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <memory>
#include <functional>
#include <filesystem>
#include <thread>
#include <atomic>

namespace wingman {

// Script configuration
struct ScriptConfig {
	ScriptConfig() = default;
	ScriptConfig(const ScriptConfig& other)
		: name(other.name),
		  path(other.path),
		  autoReload(other.autoReload),
		  sandboxed(other.sandboxed),
		  timeoutMs(other.timeoutMs),
		  env(other.env) {}
	ScriptConfig(ScriptConfig&&) noexcept = default;
	ScriptConfig& operator=(const ScriptConfig& other) {
		if (this != &other) {
			name = other.name;
			path = other.path;
			autoReload = other.autoReload;
			sandboxed = other.sandboxed;
			timeoutMs = other.timeoutMs;
			env = other.env;
		}
		return *this;
	}
	ScriptConfig& operator=(ScriptConfig&&) noexcept = default;

	std::string name;
	std::string path;
	bool autoReload = false;
	bool sandboxed = false;
	int timeoutMs = 30000;
	std::unordered_map<std::string, std::string> env;
};

// Script state
enum class ScriptState {
	unloaded,
	loaded,
	starting,   // 正在启动（初始化引擎）
	running,
	paused,
	stopping,   // 正在停止
	completed,  // 正常完成
	error
};

// Script info
struct ScriptInfo {
	ScriptConfig config;
	ScriptState state = ScriptState::unloaded;
	std::string lastError;
	uint64_t lastModified = 0;
	uint64_t lastLoaded = 0;

	// Language-agnostic script engine instance
	std::unique_ptr<script::IScriptEngine> engine;
	std::string language;

	// Script stored data
	std::unordered_map<std::string, std::string> data;
};

// Sandbox configuration
struct SandboxConfig {
	bool disableIO = false;
	bool disableOS = false;
	bool disableDebug = false;
	bool disablePackage = false;
	bool disableCoroutine = false;
	uint64_t memoryLimit = 100 * 1024 * 1024;
	uint64_t instructionLimit = 1000000;
	uint64_t timeLimitMs = 30000;
};

// Script events
enum class ScriptEvent {
	loaded,
	unloaded,
	started,
	stopped,
	error,
	reloaded
};

using ScriptEventCallback = std::function<void(const std::string& scriptName, ScriptEvent event, const std::string& message)>;
using ScriptOutputCallback = std::function<void(const std::string& scriptName, const std::string& output)>;

class ScriptManager {
public:
	ScriptManager();
	~ScriptManager();

	// ========== Script Loading Management ==========

	bool loadScript(const std::string& name, const std::string& path, const ScriptConfig& config = {});
	bool unloadScript(const std::string& name);
	bool reloadScript(const std::string& name);
	bool checkReload(const std::string& name);
	void checkAllReloads();

	// ========== Script Execution ==========

	bool runScript(const std::string& name);
	bool stopScript(const std::string& name);
	bool pauseScript(const std::string& name);
	bool resumeScript(const std::string& name);

	bool callFunction(const std::string& name, const std::string& func,
	                  const std::vector<std::string>& args = {},
	                  std::string* result = nullptr);

	// ========== Configuration Management ==========

	bool loadConfig(const std::string& path);
	bool saveConfig(const std::string& path);
	std::string getConfig(const std::string& key, const std::string& defaultValue = "");
	void setConfig(const std::string& key, const std::string& value);
	std::string getEnv(const std::string& key) const;
	void setEnv(const std::string& key, const std::string& value);

	// ========== State Query ==========

	std::shared_ptr<ScriptInfo> getScriptInfo(const std::string& name);
	std::vector<ScriptInfo> getAllScriptInfos() const;
	std::vector<std::string> getScriptNames() const;
	std::vector<std::string> getRunningScripts() const;
	bool hasScript(const std::string& name) const;

	// ========== Engine Access ==========

	// Get the script engine instance
	script::IScriptEngine* getEngine(const std::string& name);

	// Detect language from file path
	std::string detectLanguage(const std::string& path) const;

	// Get all available languages
	std::vector<std::string> getAvailableLanguages() const;

	// ========== Event Callbacks ==========

	void setEventCallback(ScriptEventCallback callback);
	void setOutputCallback(ScriptOutputCallback callback);
	void logScriptOutput(const std::string& scriptName, const std::string& output);

	// ========== Sandbox Management ==========

	void setSandboxConfig(const SandboxConfig& config);
	const SandboxConfig& getSandboxConfig() const;

	// ========== Hot Reload Control ==========

	void setAutoReload(const std::string& name, bool enabled);
	void setGlobalAutoReload(bool enabled);
	void startHotReload();
	void stopHotReload();

private:
	mutable std::mutex m_mutex;
	std::unordered_map<std::string, std::shared_ptr<ScriptInfo>> m_scripts;
	std::unordered_map<std::string, std::string> m_config;
	std::unordered_map<std::string, std::string> m_env;
	SandboxConfig m_sandboxConfig;
	ScriptEventCallback m_eventCallback;
	ScriptOutputCallback m_outputCallback;
	bool m_globalAutoReload = false;
	std::atomic<bool> m_hotReloadRunning{false};
	std::thread m_hotReloadThread;

	// Create engine and register all modules
	std::unique_ptr<script::IScriptEngine> createEngineForLanguage(const std::string& language, const ScriptConfig& scriptConfig);

	// Private helpers (caller must NOT hold m_mutex)
	bool checkTimeLimit(const std::string& name);
	uint64_t getFileModifiedTime(const std::string& path);
	bool loadJsonConfig(const std::string& path);
	bool loadIniConfig(const std::string& path);
    void triggerEvent(const std::string& name, ScriptEvent event, const std::string& message = "");
    void triggerEventUnlocked(const std::string& name, ScriptEvent event, const std::string& message = "");

	// Internal implementations (caller must hold m_mutex)
	bool stopScript_Locked(const std::string& name);
	bool unloadScript_Locked(const std::string& name);
	bool reloadScript_Locked(const std::string& name);
	bool checkReload_Locked(const std::string& name);
	void checkAllReloads_Locked();
	std::vector<std::string> getScriptNames_Locked() const;

	// Runs script execution outside m_mutex.
	bool runScriptInternal(const std::string& name);
};

} // namespace wingman
