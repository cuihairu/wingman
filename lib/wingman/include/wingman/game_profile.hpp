#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <filesystem>

namespace wingman {

// Game window configuration
struct GameWindowConfig {
    std::string title;           // Window title (supports wildcards)
    std::string className;       // Window class name
    std::string processName;     // Process name
    bool exactMatch = false;     // Whether to match exactly
    bool fullscreen = false;     // Whether fullscreen
};

// Color configuration
struct ColorConfig {
    std::string name;
    uint8_t r, g, b;
    int tolerance = 10;
};

// Image configuration
struct ImageConfig {
    std::string name;
    std::string path;
    double threshold = 0.9;
    bool preload = true;
};

// Game trigger configuration
struct GameTriggerConfig {
    std::string name;
    std::string type;            // pixel, image, timer, window, process
    std::string action;          // click, key, script
    std::string target;          // Target value
    int interval = 1000;         // Check interval (ms)
    bool enabled = true;
};

// Script configuration attached to a game profile.
struct GameScriptConfig {
    std::string name;
    std::string path;
    bool autoStart = false;
    bool restartOnCrash = false;
    int priority = 0;            // Priority (0-10)
};

// Game configuration
struct GameProfile {
    std::string id;              // Unique identifier
    std::string name;            // Display name
    std::string version;         // Configuration version
    std::string description;     // Description

    GameWindowConfig window;
    std::vector<ColorConfig> colors;
    std::vector<ImageConfig> images;
    std::vector<GameTriggerConfig> triggers;
    std::vector<GameScriptConfig> scripts;

    // Advanced settings
    std::unordered_map<std::string, std::string> settings;
};

// Game profile manager
class GameProfileManager {
public:
    static GameProfileManager& instance();

    // ========== Profile management ==========

    // Load profile
    bool loadProfile(const std::string& id);
    bool loadProfileFromFile(const std::string& path);
    bool loadProfileFromDir(const std::string& dir);

    // Save profile
    bool saveProfile(const GameProfile& profile);
    bool saveProfileToFile(const GameProfile& profile, const std::string& path);

    // Delete profile
    bool deleteProfile(const std::string& id);

    // Get profile
    GameProfile* getProfile(const std::string& id);
    const GameProfile* getProfile(const std::string& id) const;

    // Get active profile
    GameProfile* getActiveProfile();
    const GameProfile* getActiveProfile() const;

    // Set active profile
    bool setActiveProfile(const std::string& id);

    // ========== Profile list ==========

    // Get all profile IDs
    std::vector<std::string> getProfileIds() const;

    // Get all profiles
    std::vector<GameProfile> getAllProfiles() const;

    // Whether profile exists
    bool hasProfile(const std::string& id) const;

    // ========== Profile search ==========

    // Find profile by window
    GameProfile* findProfileByWindow(const std::string& title);
    GameProfile* findProfileByProcess(const std::string& processName);

    // ========== Profile directory ==========

    // Set profiles directory
    void setProfilesDirectory(const std::string& dir);

    // Get profiles directory
    std::string getProfilesDirectory() const;

    // Scan profiles directory
    void scanProfilesDirectory();

    // ========== Profile import/export ==========

    // Export profile to JSON
    std::string exportProfileToJson(const std::string& id) const;

    // Import profile from JSON
    bool importProfileFromJson(const std::string& json, const std::string& id = "");

    // Export profile package
    bool exportProfilePackage(const std::string& id, const std::string& outputPath);

    // Import profile package
    bool importProfilePackage(const std::string& packagePath);

    // ========== Validation ==========

    // Validate profile
    bool validateProfile(const GameProfile& profile, std::string& error) const;

    // ========== Template ==========

    // Create default profile template
    GameProfile createTemplate(const std::string& gameName) const;

private:
    GameProfileManager();
    ~GameProfileManager() = default;

    std::unordered_map<std::string, GameProfile> m_profiles;
    std::string m_activeProfileId;
    std::string m_profilesDirectory;
    std::string m_configFileName = "profile.json";

    // Parse profile file
    bool parseProfileFile(const std::string& path, GameProfile& profile);

    // Write profile file
    bool writeProfileFile(const std::string& path, const GameProfile& profile);

    // Get profile path
    std::string getProfilePath(const std::string& id) const;
};

} // namespace wingman
