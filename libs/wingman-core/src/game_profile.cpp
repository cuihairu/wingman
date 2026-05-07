#include "wingman/game_profile.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>

namespace wingman {

// ========== GameProfileManager Implementation ==========

GameProfileManager& GameProfileManager::instance() {
    static GameProfileManager inst;
    return inst;
}

GameProfileManager::GameProfileManager() {
    // 设置默认配置目录
    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    std::filesystem::path exeDir = std::filesystem::path(exePath).parent_path();
    m_profilesDirectory = (exeDir / "profiles").string();
}

// ========== 配置管理 ==========

bool GameProfileManager::loadProfile(const std::string& id) {
    std::string path = getProfilePath(id);
    return loadProfileFromFile(path);
}

bool GameProfileManager::loadProfileFromFile(const std::string& path) {
    GameProfile profile;
    if (!parseProfileFile(path, profile)) {
        return false;
    }

    std::string error;
    if (!validateProfile(profile, error)) {
        return false;
    }

    m_profiles[profile.id] = profile;
    return true;
}

bool GameProfileManager::loadProfileFromDir(const std::string& dir) {
    std::string configPath = (std::filesystem::path(dir) / m_configFileName).string();
    return loadProfileFromFile(configPath);
}

bool GameProfileManager::saveProfile(const GameProfile& profile) {
    std::string error;
    if (!validateProfile(profile, error)) {
        return false;
    }

    m_profiles[profile.id] = profile;
    return saveProfileToFile(profile, getProfilePath(profile.id));
}

bool GameProfileManager::saveProfileToFile(const GameProfile& profile, const std::string& path) {
    // 确保目录存在
    std::filesystem::path filePath(path);
    std::filesystem::create_directories(filePath.parent_path());

    return writeProfileFile(path, profile);
}

bool GameProfileManager::deleteProfile(const std::string& id) {
    auto it = m_profiles.find(id);
    if (it == m_profiles.end()) {
        return false;
    }

    // 删除文件
    std::string path = getProfilePath(id);
    std::filesystem::remove_all(std::filesystem::path(path).parent_path());

    m_profiles.erase(it);
    return true;
}

GameProfile* GameProfileManager::getProfile(const std::string& id) {
    auto it = m_profiles.find(id);
    if (it != m_profiles.end()) {
        return &it->second;
    }
    return nullptr;
}

const GameProfile* GameProfileManager::getProfile(const std::string& id) const {
    auto it = m_profiles.find(id);
    if (it != m_profiles.end()) {
        return &it->second;
    }
    return nullptr;
}

GameProfile* GameProfileManager::getActiveProfile() {
    return getProfile(m_activeProfileId);
}

const GameProfile* GameProfileManager::getActiveProfile() const {
    return getProfile(m_activeProfileId);
}

bool GameProfileManager::setActiveProfile(const std::string& id) {
    if (!hasProfile(id)) {
        return false;
    }
    m_activeProfileId = id;
    return true;
}

// ========== 配置列表 ==========

std::vector<std::string> GameProfileManager::getProfileIds() const {
    std::vector<std::string> ids;
    ids.reserve(m_profiles.size());
    for (const auto& [id, _] : m_profiles) {
        ids.push_back(id);
    }
    return ids;
}

std::vector<GameProfile> GameProfileManager::getAllProfiles() const {
    std::vector<GameProfile> profiles;
    profiles.reserve(m_profiles.size());
    for (const auto& [_, profile] : m_profiles) {
        profiles.push_back(profile);
    }
    return profiles;
}

bool GameProfileManager::hasProfile(const std::string& id) const {
    return m_profiles.find(id) != m_profiles.end();
}

// ========== 配置搜索 ==========

GameProfile* GameProfileManager::findProfileByWindow(const std::string& title) {
    for (auto& [id, profile] : m_profiles) {
        const auto& win = profile.window;

        if (win.exactMatch) {
            if (title == win.title) return &profile;
        } else {
            // 通配符匹配
            if (title.find(win.title) != std::string::npos) {
                return &profile;
            }
        }
    }
    return nullptr;
}

GameProfile* GameProfileManager::findProfileByProcess(const std::string& processName) {
    for (auto& [id, profile] : m_profiles) {
        if (profile.window.processName == processName) {
            return &profile;
        }
    }
    return nullptr;
}

// ========== 配置目录 ==========

void GameProfileManager::setProfilesDirectory(const std::string& dir) {
    m_profilesDirectory = dir;
    scanProfilesDirectory();
}

std::string GameProfileManager::getProfilesDirectory() const {
    return m_profilesDirectory;
}

void GameProfileManager::scanProfilesDirectory() {
    if (!std::filesystem::exists(m_profilesDirectory)) {
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(m_profilesDirectory)) {
        if (entry.is_directory()) {
            std::string configPath = (entry.path() / m_configFileName).string();
            if (std::filesystem::exists(configPath)) {
                loadProfileFromFile(configPath);
            }
        }
    }
}

// ========== 配置导入导出 ==========

std::string GameProfileManager::exportProfileToJson(const std::string& id) const {
    const auto* profile = getProfile(id);
    if (!profile) {
        return "";
    }

    // TODO: 使用 nlohmann/json 序列化
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"id\": \"" << profile->id << "\",\n";
    ss << "  \"name\": \"" << profile->name << "\",\n";
    ss << "  \"version\": \"" << profile->version << "\",\n";
    ss << "  \"description\": \"" << profile->description << "\"\n";
    ss << "}";
    return ss.str();
}

bool GameProfileManager::importProfileFromJson(const std::string& json, const std::string& id) {
    // TODO: 使用 nlohmann/json 解析
    return false;
}

bool GameProfileManager::exportProfilePackage(const std::string& id, const std::string& outputPath) {
    const auto* profile = getProfile(id);
    if (!profile) {
        return false;
    }

    // 创建临时目录
    std::filesystem::path tempDir = std::filesystem::temp_directory_path() / id;
    std::filesystem::create_directories(tempDir);

    // 复制配置和图像文件
    std::filesystem::copy(getProfilePath(id), tempDir,
        std::filesystem::copy_options::recursive);

    // TODO: 创建 ZIP 压缩包

    return true;
}

bool GameProfileManager::importProfilePackage(const std::string& packagePath) {
    // TODO: 解压 ZIP 包
    return false;
}

// ========== 验证 ==========

bool GameProfileManager::validateProfile(const GameProfile& profile, std::string& error) const {
    if (profile.id.empty()) {
        error = "Profile ID is required";
        return false;
    }

    if (profile.name.empty()) {
        error = "Profile name is required";
        return false;
    }

    if (profile.window.title.empty() && profile.window.processName.empty()) {
        error = "Either window title or process name is required";
        return false;
    }

    return true;
}

// ========== 模板 ==========

GameProfile GameProfileManager::createTemplate(const std::string& gameName) const {
    GameProfile profile;

    // 从游戏名生成 ID
    std::string id = gameName;
    std::transform(id.begin(), id.end(), id.begin(), ::tolower);
    std::replace_if(id.begin(), id.end(), ::isspace, '_');

    profile.id = id;
    profile.name = gameName;
    profile.version = "1.0.0";
    profile.description = gameName + " Automation Profile";

    profile.window.title = gameName;
    profile.window.exactMatch = false;

    return profile;
}

// ========== 私有方法 ==========

bool GameProfileManager::parseProfileFile(const std::string& path, GameProfile& profile) {
    // TODO: 使用 nlohmann/json 解析
    // 这里提供简单的 INI 解析作为后备

    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    std::string section;

    while (std::getline(file, line)) {
        // 跳过注释和空行
        if (line.empty() || line[0] == ';' || line[0] == '#') {
            continue;
        }

        // 解析节
        if (line[0] == '[' && line.back() == ']') {
            section = line.substr(1, line.size() - 2);
            continue;
        }

        // 解析键值
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);

            // 去除空格
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            if (section == "profile") {
                if (key == "id") profile.id = value;
                else if (key == "name") profile.name = value;
                else if (key == "version") profile.version = value;
                else if (key == "description") profile.description = value;
            } else if (section == "window") {
                if (key == "title") profile.window.title = value;
                else if (key == "process") profile.window.processName = value;
                else if (key == "exact_match") profile.window.exactMatch = (value == "true");
            }
        }
    }

    return !profile.id.empty();
}

bool GameProfileManager::writeProfileFile(const std::string& path, const GameProfile& profile) {
    // TODO: 使用 nlohmann/json 序列化
    // 这里提供简单的 INI 格式作为后备

    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }

    file << "; Wingman Game Profile\n";
    file << "; Game Configuration File\n\n";

    file << "[profile]\n";
    file << "id=" << profile.id << "\n";
    file << "name=" << profile.name << "\n";
    file << "version=" << profile.version << "\n";
    file << "description=" << profile.description << "\n\n";

    file << "[window]\n";
    file << "title=" << profile.window.title << "\n";
    file << "process=" << profile.window.processName << "\n";
    file << "exact_match=" << (profile.window.exactMatch ? "true" : "false") << "\n\n";

    // 颜色配置
    if (!profile.colors.empty()) {
        file << "[colors]\n";
        for (const auto& color : profile.colors) {
            file << color.name << "=" << (int)color.r << "," << (int)color.g << "," << (int)color.b << "\n";
        }
        file << "\n";
    }

    // 图像配置
    if (!profile.images.empty()) {
        file << "[images]\n";
        for (const auto& img : profile.images) {
            file << img.name << "=" << img.path << "\n";
        }
        file << "\n";
    }

    return true;
}

std::string GameProfileManager::getProfilePath(const std::string& id) const {
    return (std::filesystem::path(m_profilesDirectory) / id / m_configFileName).string();
}

} // namespace wingman
