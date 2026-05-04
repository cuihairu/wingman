#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <filesystem>

namespace wingman {

// 游戏窗口配置
struct GameWindowConfig {
    std::string title;           // 窗口标题 (支持通配符)
    std::string className;       // 窗口类名
    std::string processName;     // 进程名称
    bool exactMatch = false;     // 是否精确匹配
    bool fullscreen = false;     // 是否全屏
};

// 颜色配置
struct ColorConfig {
    std::string name;
    uint8_t r, g, b;
    int tolerance = 10;
};

// 图像配置
struct ImageConfig {
    std::string name;
    std::string path;
    double threshold = 0.9;
    bool preload = true;
};

// 游戏触发器配置
struct GameTriggerConfig {
    std::string name;
    std::string type;            // pixel, image, timer, window, process
    std::string action;          // click, key, script
    std::string target;          // 目标值
    int interval = 1000;         // 检查间隔 (ms)
    bool enabled = true;
};

// 脚本配置
struct ScriptConfig {
    std::string name;
    std::string path;
    bool autoStart = false;
    bool restartOnCrash = false;
    int priority = 0;            // 优先级 (0-10)
};

// 游戏配置
struct GameProfile {
    std::string id;              // 唯一标识
    std::string name;            // 显示名称
    std::string version;         // 配置版本
    std::string description;     // 描述

    GameWindowConfig window;
    std::vector<ColorConfig> colors;
    std::vector<ImageConfig> images;
    std::vector<GameTriggerConfig> triggers;
    std::vector<ScriptConfig> scripts;

    // 高级设置
    std::unordered_map<std::string, std::string> settings;
};

// 游戏配置管理器
class GameProfileManager {
public:
    static GameProfileManager& instance();

    // ========== 配置管理 ==========

    // 加载配置
    bool loadProfile(const std::string& id);
    bool loadProfileFromFile(const std::string& path);
    bool loadProfileFromDir(const std::string& dir);

    // 保存配置
    bool saveProfile(const GameProfile& profile);
    bool saveProfileToFile(const GameProfile& profile, const std::string& path);

    // 删除配置
    bool deleteProfile(const std::string& id);

    // 获取配置
    GameProfile* getProfile(const std::string& id);
    const GameProfile* getProfile(const std::string& id) const;

    // 获取当前活动配置
    GameProfile* getActiveProfile();
    const GameProfile* getActiveProfile() const;

    // 设置活动配置
    bool setActiveProfile(const std::string& id);

    // ========== 配置列表 ==========

    // 获取所有配置 ID
    std::vector<std::string> getProfileIds() const;

    // 获取所有配置
    std::vector<GameProfile> getAllProfiles() const;

    // 配置是否存在
    bool hasProfile(const std::string& id) const;

    // ========== 配置搜索 ==========

    // 根据窗口查找配置
    GameProfile* findProfileByWindow(const std::string& title);
    GameProfile* findProfileByProcess(const std::string& processName);

    // ========== 配置目录 ==========

    // 设置配置目录
    void setProfilesDirectory(const std::string& dir);

    // 获取配置目录
    std::string getProfilesDirectory() const;

    // 扫描配置目录
    void scanProfilesDirectory();

    // ========== 配置导入导出 ==========

    // 导出配置为 JSON
    std::string exportProfileToJson(const std::string& id) const;

    // 从 JSON 导入配置
    bool importProfileFromJson(const std::string& json, const std::string& id = "");

    // 导出配置包
    bool exportProfilePackage(const std::string& id, const std::string& outputPath);

    // 导入配置包
    bool importProfilePackage(const std::string& packagePath);

    // ========== 验证 ==========

    // 验证配置
    bool validateProfile(const GameProfile& profile, std::string& error) const;

    // ========== 模板 ==========

    // 创建默认配置模板
    GameProfile createTemplate(const std::string& gameName) const;

private:
    GameProfileManager();
    ~GameProfileManager() = default;

    std::unordered_map<std::string, GameProfile> m_profiles;
    std::string m_activeProfileId;
    std::string m_profilesDirectory;
    std::string m_configFileName = "profile.json";

    // 解析配置文件
    bool parseProfileFile(const std::string& path, GameProfile& profile);

    // 写入配置文件
    bool writeProfileFile(const std::string& path, const GameProfile& profile);

    // 获取配置路径
    std::string getProfilePath(const std::string& id) const;
};

} // namespace wingman
