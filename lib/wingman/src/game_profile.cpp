#include "wingman/game_profile.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifdef _WIN32
#include <Windows.h>
#endif
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <algorithm>

namespace wingman {

using json = nlohmann::json;

// ========== JSON Serialization Helpers ==========

static json toJson(const GameWindowConfig& w) {
	return json{
		{"title", w.title},
		{"className", w.className},
		{"processName", w.processName},
		{"exactMatch", w.exactMatch},
		{"fullscreen", w.fullscreen}
	};
}

static json toJson(const ColorConfig& c) {
	return json{
		{"name", c.name},
		{"r", c.r}, {"g", c.g}, {"b", c.b},
		{"tolerance", c.tolerance}
	};
}

static json toJson(const ImageConfig& img) {
	return json{
		{"name", img.name},
		{"path", img.path},
		{"threshold", img.threshold},
		{"preload", img.preload}
	};
}

static json toJson(const GameTriggerConfig& t) {
	return json{
		{"name", t.name},
		{"type", t.type},
		{"action", t.action},
		{"target", t.target},
		{"interval", t.interval},
		{"enabled", t.enabled}
	};
}

static json toJson(const GameScriptConfig& s) {
	return json{
		{"name", s.name},
		{"path", s.path},
		{"autoStart", s.autoStart},
		{"restartOnCrash", s.restartOnCrash},
		{"priority", s.priority}
	};
}

static json toJson(const GameProfile& p) {
	json j;
	j["id"] = p.id;
	j["name"] = p.name;
	j["version"] = p.version;
	j["description"] = p.description;
	j["window"] = toJson(p.window);

	if (!p.colors.empty()) {
		for (const auto& c : p.colors) j["colors"].push_back(toJson(c));
	}
	if (!p.images.empty()) {
		for (const auto& img : p.images) j["images"].push_back(toJson(img));
	}
	if (!p.triggers.empty()) {
		for (const auto& t : p.triggers) j["triggers"].push_back(toJson(t));
	}
	if (!p.scripts.empty()) {
		for (const auto& s : p.scripts) j["scripts"].push_back(toJson(s));
	}
	if (!p.settings.empty()) {
		j["settings"] = p.settings;
	}

	return j;
}

static void fromJson(const json& j, GameWindowConfig& w) {
	j.at("title").get_to(w.title);
	if (j.contains("className")) j.at("className").get_to(w.className);
	if (j.contains("processName")) j.at("processName").get_to(w.processName);
	if (j.contains("exactMatch")) j.at("exactMatch").get_to(w.exactMatch);
	if (j.contains("fullscreen")) j.at("fullscreen").get_to(w.fullscreen);
}

static void fromJson(const json& j, ColorConfig& c) {
	j.at("name").get_to(c.name);
	j.at("r").get_to(c.r);
	j.at("g").get_to(c.g);
	j.at("b").get_to(c.b);
	if (j.contains("tolerance")) j.at("tolerance").get_to(c.tolerance);
}

static void fromJson(const json& j, ImageConfig& img) {
	j.at("name").get_to(img.name);
	j.at("path").get_to(img.path);
	if (j.contains("threshold")) j.at("threshold").get_to(img.threshold);
	if (j.contains("preload")) j.at("preload").get_to(img.preload);
}

static void fromJson(const json& j, GameTriggerConfig& t) {
	j.at("name").get_to(t.name);
	j.at("type").get_to(t.type);
	j.at("action").get_to(t.action);
	j.at("target").get_to(t.target);
	if (j.contains("interval")) j.at("interval").get_to(t.interval);
	if (j.contains("enabled")) j.at("enabled").get_to(t.enabled);
}

static void fromJson(const json& j, GameScriptConfig& s) {
	j.at("name").get_to(s.name);
	j.at("path").get_to(s.path);
	if (j.contains("autoStart")) j.at("autoStart").get_to(s.autoStart);
	if (j.contains("restartOnCrash")) j.at("restartOnCrash").get_to(s.restartOnCrash);
	if (j.contains("priority")) j.at("priority").get_to(s.priority);
}

static bool parseGameProfile(const json& j, GameProfile& profile) {
	try {
		j.at("id").get_to(profile.id);
		j.at("name").get_to(profile.name);
		if (j.contains("version")) j.at("version").get_to(profile.version);
		if (j.contains("description")) j.at("description").get_to(profile.description);
		if (j.contains("window")) fromJson(j.at("window"), profile.window);
		if (j.contains("colors")) {
			for (const auto& c : j.at("colors")) {
				ColorConfig cc;
				fromJson(c, cc);
				profile.colors.push_back(cc);
			}
		}
		if (j.contains("images")) {
			for (const auto& img : j.at("images")) {
				ImageConfig ic;
				fromJson(img, ic);
				profile.images.push_back(ic);
			}
		}
		if (j.contains("triggers")) {
			for (const auto& t : j.at("triggers")) {
				GameTriggerConfig tc;
				fromJson(t, tc);
				profile.triggers.push_back(tc);
			}
		}
		if (j.contains("scripts")) {
			for (const auto& s : j.at("scripts")) {
				GameScriptConfig sc;
				fromJson(s, sc);
				profile.scripts.push_back(sc);
			}
		}
		if (j.contains("settings")) {
			j.at("settings").get_to(profile.settings);
		}
		return true;
	} catch (const json::exception&) {
		return false;
	}
}

// ========== GameProfileManager Implementation ==========

GameProfileManager& GameProfileManager::instance() {
    static GameProfileManager inst;
    return inst;
}

GameProfileManager::GameProfileManager() {
#ifdef _WIN32
    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    std::filesystem::path exeDir = std::filesystem::path(exePath).parent_path();
#else
    std::filesystem::path exeDir = std::filesystem::current_path();
#endif
    m_profilesDirectory = (exeDir / "profiles").string();
}

// ========== Configuration Management ==========

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
    std::filesystem::path filePath(path);
    std::filesystem::create_directories(filePath.parent_path());
    return writeProfileFile(path, profile);
}

bool GameProfileManager::deleteProfile(const std::string& id) {
    auto it = m_profiles.find(id);
    if (it == m_profiles.end()) {
        return false;
    }

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

// ========== Configuration List ==========

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

// ========== Configuration Search ==========

GameProfile* GameProfileManager::findProfileByWindow(const std::string& title) {
    for (auto& [id, profile] : m_profiles) {
        const auto& win = profile.window;

        if (win.exactMatch) {
            if (title == win.title) return &profile;
        } else {
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

// ========== Configuration Directory ==========

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

// ========== Configuration Import/Export ==========

std::string GameProfileManager::exportProfileToJson(const std::string& id) const {
    const auto* profile = getProfile(id);
    if (!profile) {
        return "";
    }
    return toJson(*profile).dump(2);
}

bool GameProfileManager::importProfileFromJson(const std::string& jsonStr, const std::string& id) {
    try {
        auto j = json::parse(jsonStr);
        GameProfile profile;
        if (!parseGameProfile(j, profile)) {
            return false;
        }
        if (!id.empty()) {
            profile.id = id;
        }
        std::string error;
        if (!validateProfile(profile, error)) {
            return false;
        }
        m_profiles[profile.id] = profile;
        return true;
    } catch (const json::exception&) {
        return false;
    }
}

bool GameProfileManager::exportProfilePackage(const std::string& id, const std::string& outputPath) {
    const auto* profile = getProfile(id);
    if (!profile) {
        return false;
    }

    // Export as JSON file (ZIP packaging not yet available)
    std::string jsonStr = toJson(*profile).dump(2);
    std::ofstream file(outputPath);
    if (!file.is_open()) {
        return false;
    }
    file << jsonStr;
    return true;
}

bool GameProfileManager::importProfilePackage(const std::string& packagePath) {
    std::ifstream file(packagePath);
    if (!file.is_open()) {
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    return importProfileFromJson(content);
}

// ========== Validation ==========

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

// ========== Templates ==========

GameProfile GameProfileManager::createTemplate(const std::string& gameName) const {
    GameProfile profile;

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

// ========== Private Methods ==========

bool GameProfileManager::parseProfileFile(const std::string& path, GameProfile& profile) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    // Try JSON first
    try {
        auto j = json::parse(content);
        return parseGameProfile(j, profile);
    } catch (const json::exception&) {
        // Fall through to INI parsing for backward compatibility
    }

    // Legacy INI format fallback
    std::istringstream iss(content);
    std::string line;
    std::string section;

    while (std::getline(iss, line)) {
        if (line.empty() || line[0] == ';' || line[0] == '#') {
            continue;
        }

        if (line[0] == '[' && line.back() == ']') {
            section = line.substr(1, line.size() - 2);
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
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }
    file << toJson(profile).dump(2);
    return true;
}

std::string GameProfileManager::getProfilePath(const std::string& id) const {
    return (std::filesystem::path(m_profilesDirectory) / id / m_configFileName).string();
}

} // namespace wingman
