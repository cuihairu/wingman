#include "wingman/gui/config.hpp"
#include "wingman/config.hpp"
#include <nlohmann/json.hpp>

namespace wingman::gui {

using json = nlohmann::json;

std::string GuiConfig::toJson() const {
    json j;
    j["showTriggerPanel"] = showTriggerPanel;
    j["showScriptPanel"] = showScriptPanel;
    j["showPreviewPanel"] = showPreviewPanel;
    j["showLogPanel"] = showLogPanel;
    j["showDemoWindow"] = showDemoWindow;
    j["logFilterInfo"] = logFilterInfo;
    j["logFilterWarning"] = logFilterWarning;
    j["logFilterError"] = logFilterError;
    j["logFilterDebug"] = logFilterDebug;
    j["previewRefreshInterval"] = previewRefreshInterval;
    j["windowWidth"] = windowWidth;
    j["windowHeight"] = windowHeight;
    j["themeIndex"] = themeIndex;
    return j.dump();
}

GuiConfig GuiConfig::fromJson(const std::string& jsonStr) {
    GuiConfig config;
    try {
        json j = json::parse(jsonStr);
        if (j.contains("showTriggerPanel")) config.showTriggerPanel = j["showTriggerPanel"];
        if (j.contains("showScriptPanel")) config.showScriptPanel = j["showScriptPanel"];
        if (j.contains("showPreviewPanel")) config.showPreviewPanel = j["showPreviewPanel"];
        if (j.contains("showLogPanel")) config.showLogPanel = j["showLogPanel"];
        if (j.contains("showDemoWindow")) config.showDemoWindow = j["showDemoWindow"];
        if (j.contains("logFilterInfo")) config.logFilterInfo = j["logFilterInfo"];
        if (j.contains("logFilterWarning")) config.logFilterWarning = j["logFilterWarning"];
        if (j.contains("logFilterError")) config.logFilterError = j["logFilterError"];
        if (j.contains("logFilterDebug")) config.logFilterDebug = j["logFilterDebug"];
        if (j.contains("previewRefreshInterval")) config.previewRefreshInterval = j["previewRefreshInterval"];
        if (j.contains("windowWidth")) config.windowWidth = j["windowWidth"];
        if (j.contains("windowHeight")) config.windowHeight = j["windowHeight"];
        if (j.contains("themeIndex")) config.themeIndex = j["themeIndex"];
    } catch (...) {
        // 解析失败，使用默认值
    }
    return config;
}

GuiConfig GuiConfig::load(ConfigManager* configMgr) {
    if (!configMgr) return GuiConfig();

    auto configJson = configMgr->get("gui");
    if (configJson.has_value()) {
        return fromJson(configJson.value());
    }
    return GuiConfig();
}

bool GuiConfig::save(ConfigManager* configMgr) const {
    if (!configMgr) return false;
    return configMgr->set("gui", toJson());
}

} // namespace wingman::gui
