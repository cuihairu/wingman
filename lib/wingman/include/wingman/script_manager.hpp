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

namespace wingman {

// 脚本配置
struct ScriptConfig {
	std::string name;
	std::string path;
	bool autoReload = false;
	bool sandboxed = true;
	int timeoutMs = 30000;
	std::unordered_map<std::string, std::string> env;
};

// 脚本状态
enum class ScriptState {
	unloaded,
	loaded,
	running,
	paused,
	error
};

// 脚本信息
struct ScriptInfo {
	ScriptConfig config;
	ScriptState state = ScriptState::unloaded;
	std::string lastError;
	uint64_t lastModified = 0;
	uint64_t lastLoaded = 0;

	// 语言无关的脚本引擎实例
	std::unique_ptr<script::IScriptEngine> engine;
	std::string language;

	// 脚本存储的数据
	std::unordered_map<std::string, std::string> data;
};

// 沙箱配置
struct SandboxConfig {
	bool disableIO = true;
	bool disableOS = true;
	bool disableDebug = true;
	bool disablePackage = true;
	bool disableCoroutine = false;
	uint64_t memoryLimit = 100 * 1024 * 1024;
	uint64_t instructionLimit = 1000000;
	uint64_t timeLimitMs = 30000;
};

// 脚本事件
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

	// ========== 脚本加载管理 ==========

	bool loadScript(const std::string& name, const std::string& path, const ScriptConfig& config = {});
	bool unloadScript(const std::string& name);
	bool reloadScript(const std::string& name);
	bool checkReload(const std::string& name);
	void checkAllReloads();

	// ========== 脚本执行 ==========

	bool runScript(const std::string& name);
	bool stopScript(const std::string& name);
	bool pauseScript(const std::string& name);
	bool resumeScript(const std::string& name);

	bool callFunction(const std::string& name, const std::string& func,
	                  const std::vector<std::string>& args = {},
	                  std::string* result = nullptr);

	// ========== 配置管理 ==========

	bool loadConfig(const std::string& path);
	bool saveConfig(const std::string& path);
	std::string getConfig(const std::string& key, const std::string& defaultValue = "");
	void setConfig(const std::string& key, const std::string& value);
	std::string getEnv(const std::string& key) const;
	void setEnv(const std::string& key, const std::string& value);

	// ========== 状态查询 ==========

	ScriptInfo* getScriptInfo(const std::string& name);
	std::vector<ScriptInfo> getAllScriptInfos() const;
	std::vector<std::string> getScriptNames() const;
	std::vector<std::string> getRunningScripts() const;
	bool hasScript(const std::string& name) const;

	// ========== 引擎访问 ==========

	// 获取脚本的引擎实例
	script::IScriptEngine* getEngine(const std::string& name);

	// 根据文件路径检测语言
	std::string detectLanguage(const std::string& path) const;

	// 获取所有可用语言
	std::vector<std::string> getAvailableLanguages() const;

	// ========== 事件回调 ==========

	void setEventCallback(ScriptEventCallback callback);
	void setOutputCallback(ScriptOutputCallback callback);
	void logScriptOutput(const std::string& scriptName, const std::string& output);

	// ========== 沙箱管理 ==========

	void setSandboxConfig(const SandboxConfig& config);
	const SandboxConfig& getSandboxConfig() const;

	// ========== 热加载控制 ==========

	void setAutoReload(const std::string& name, bool enabled);
	void setGlobalAutoReload(bool enabled);
	void startHotReload();
	void stopHotReload();

private:
	mutable std::mutex m_mutex;
	std::unordered_map<std::string, std::unique_ptr<ScriptInfo>> m_scripts;
	std::unordered_map<std::string, std::string> m_config;
	std::unordered_map<std::string, std::string> m_env;
	SandboxConfig m_sandboxConfig;
	ScriptEventCallback m_eventCallback;
	ScriptOutputCallback m_outputCallback;
	bool m_globalAutoReload = false;
	bool m_hotReloadRunning = false;
	std::thread m_hotReloadThread;

	// 创建引擎并注册所有模块
	std::unique_ptr<script::IScriptEngine> createEngineForLanguage(const std::string& language);

	// 私有方法
	bool checkTimeLimit(const std::string& name);
	uint64_t getFileModifiedTime(const std::string& path);
	bool loadJsonConfig(const std::string& path);
	bool loadIniConfig(const std::string& path);
	void triggerEvent(const std::string& name, ScriptEvent event, const std::string& message = "");
};

} // namespace wingman
