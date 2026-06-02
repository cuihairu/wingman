#include "wingman/remote_server.hpp"

#include "wingman/version.hpp"
#include "wingman/window.hpp"
#include "wingman/vision.hpp"
#include "wingman/screen.hpp"
#include "wingman/platform/input_factory.hpp"
#include <spdlog/spdlog.h>
#include <thread>
#include <chrono>
#include <vector>

#ifdef WINGMAN_ENABLE_VISION
#include <opencv2/opencv.hpp>
#endif

// ========== Base64 Encoding Helper Functions ==========

static std::string base64Encode(const std::vector<uchar>& data) {
    const std::string base64Chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string base64;
    base64.reserve((data.size() * 4) / 3 + 4);

    for (size_t i = 0; i < data.size(); i += 3) {
        auto b0 = data[i];
        auto b1 = (i + 1 < data.size()) ? data[i + 1] : 0;
        auto b2 = (i + 2 < data.size()) ? data[i + 2] : 0;

        base64.push_back(base64Chars[(b0 >> 2) & 0x3F]);
        base64.push_back(base64Chars[((b0 & 0x03) << 4) | ((b1 >> 4) & 0x0F)]);
        base64.push_back((i + 1 < data.size()) ? base64Chars[((b1 & 0x0F) << 2) | ((b2 >> 6) & 0x03)] : '=');
        base64.push_back((i + 2 < data.size()) ? base64Chars[b2 & 0x3F] : '=');
    }

    return base64;
}

namespace wingman {

// ========== RemoteRequest ==========

RemoteRequest RemoteRequest::fromJson(const std::string& jsonStr) {
    try {
        nlohmann::json j = nlohmann::json::parse(jsonStr);
        return fromJson(j);
    } catch (const std::exception& e) {
        spdlog::error("Failed to parse RemoteRequest: {}", e.what());
        return RemoteRequest{};
    }
}

RemoteRequest RemoteRequest::fromJson(const nlohmann::json& j) {
    RemoteRequest req;
    req.action = j.value("action", "");
    req.params = j.value("params", nlohmann::json{});
    return req;
}

nlohmann::json RemoteRequest::toJson() const {
    nlohmann::json j;
    j["action"] = action;
    j["params"] = params;
    return j;
}

// ========== RemoteResponse ==========

std::string RemoteResponse::toJsonString() const {
    nlohmann::json j;
    j["success"] = success;
    j["data"] = data;
    j["error"] = error;
    return j.dump();
}

RemoteResponse RemoteResponse::fromJson(const nlohmann::json& j) {
    RemoteResponse resp;
    resp.success = j.value("success", false);
    resp.data = j.value("data", nlohmann::json{});
    resp.error = j.value("error", "");
    return resp;
}

// ========== RemoteControlServer Request Handling ==========

RemoteResponse RemoteControlServer::handleRequest(const RemoteRequest& req) {
    try {
        if (req.action == "ping") return handlePing(req.params);
        if (req.action == "get_version") return handleGetVersion(req.params);
#ifdef WINGMAN_ENABLE_VISION
        if (req.action == "capture_screen") return handleCaptureScreen(req.params);
        if (req.action == "find_image") return handleFindImage(req.params);
#else
        // Return unsupported error
        if (req.action == "capture_screen" || req.action == "find_image") {
            RemoteResponse resp;
            resp.success = false;
            resp.error = "Action not supported: Vision features require OpenCV";
            return resp;
        }
#endif
        if (req.action == "get_pixel") return handleGetPixel(req.params);
        if (req.action == "find_color") return handleFindColor(req.params);
        if (req.action == "click") return handleClick(req.params);
        if (req.action == "move") return handleMove(req.params);
        if (req.action == "key") return handleKey(req.params);
        if (req.action == "type_text") return handleTypeText(req.params);
        if (req.action == "list_triggers") return handleListTriggers(req.params);
        if (req.action == "add_trigger") return handleAddTrigger(req.params);
        if (req.action == "remove_trigger") return handleRemoveTrigger(req.params);
        if (req.action == "enable_trigger") return handleEnableTrigger(req.params);
        if (req.action == "disable_trigger") return handleDisableTrigger(req.params);
        if (req.action == "record_macro") return handleRecordMacro(req.params);
        if (req.action == "stop_macro_recording") return handleStopMacroRecording(req.params);
        if (req.action == "play_macro") return handlePlayMacro(req.params);

        RemoteResponse resp;
        resp.success = false;
        resp.error = "Unknown action: " + req.action;
        return resp;
    } catch (const std::exception& e) {
        RemoteResponse resp;
        resp.success = false;
        resp.error = e.what();
        return resp;
    }
}

// ========== Action Handlers ==========

RemoteResponse RemoteControlServer::handlePing(const nlohmann::json& /*params*/) {
    RemoteResponse resp;
    resp.success = true;
    resp.data["status"] = "ok";
    resp.data["version"] = WINGMAN_VERSION;
    return resp;
}

RemoteResponse RemoteControlServer::handleGetVersion(const nlohmann::json& /*params*/) {
    RemoteResponse resp;
    resp.success = true;
    resp.data["version"] = WINGMAN_VERSION;
    resp.data["name"] = "Wingman";
    return resp;
}

#ifdef WINGMAN_ENABLE_VISION

RemoteResponse RemoteControlServer::handleCaptureScreen(const nlohmann::json& params) {
    RemoteResponse resp;

    try {
        int x = params.value("x", 0);
        int y = params.value("y", 0);
        int reqWidth = params.value("width", 0);
        int reqHeight = params.value("height", 0);
        int quality = params.value("quality", 75);
        std::string format = params.value("format", "jpeg");

        Rect region(x, y, reqWidth, reqHeight);
        auto bitmap = region.isEmpty() ? Screen::capture() : Screen::capture(region);

        if (!bitmap) {
            resp.success = false;
            resp.error = "Failed to capture screen";
            return resp;
        }

        cv::Mat mat(bitmap->getHeight(), bitmap->getWidth(), CV_8UC4,
                    const_cast<uchar*>(bitmap->getData()));

        cv::Mat bgrMat;
        cv::cvtColor(mat, bgrMat, cv::COLOR_BGRA2BGR);

        std::vector<uchar> buffer;
        std::string ext = (format == "png") ? ".png" : ".jpg";
        std::vector<int> encodeParams;

        if (format == "png") {
            encodeParams = {cv::IMWRITE_PNG_COMPRESSION, 9};
        } else {
            encodeParams = {cv::IMWRITE_JPEG_QUALITY, quality};
        }

        if (!cv::imencode(ext, bgrMat, buffer, encodeParams)) {
            resp.success = false;
            resp.error = "Failed to encode image";
            return resp;
        }

        std::string base64 = base64Encode(buffer);

        resp.success = true;
        resp.data["width"] = bitmap->getWidth();
        resp.data["height"] = bitmap->getHeight();
        resp.data["format"] = format;
        resp.data["size"] = static_cast<int>(buffer.size());
        resp.data["data"] = base64;

        spdlog::debug("Screenshot captured: {}x{}, {} bytes, base64: {} chars",
                     bitmap->getWidth(), bitmap->getHeight(), buffer.size(), base64.size());

    } catch (const std::exception& e) {
        resp.success = false;
        resp.error = std::string("Screenshot error: ") + e.what();
        spdlog::error("Screenshot error: {}", e.what());
    }

    return resp;
}

RemoteResponse RemoteControlServer::handleFindImage(const nlohmann::json& params) {
    RemoteResponse resp;

    try {
        std::string templatePath = params["template_path"];
        double threshold = params.value("threshold", 0.8);

        bool hasRegion = params.contains("region");
        ImageMatch result;

        if (hasRegion) {
            Rect region;
            region.x = params["region"].value("x", 0);
            region.y = params["region"].value("y", 0);
            region.width = params["region"].value("width", 0);
            region.height = params["region"].value("height", 0);
            result = Vision::findImage(templatePath, region, threshold);
        } else {
            result = Vision::findImage(templatePath, threshold);
        }

        resp.success = result.found;
        if (result.found) {
            resp.data["x"] = result.position.x;
            resp.data["y"] = result.position.y;
            resp.data["confidence"] = result.confidence;
            resp.data["region"]["x"] = result.region.x;
            resp.data["region"]["y"] = result.region.y;
            resp.data["region"]["width"] = result.region.width;
            resp.data["region"]["height"] = result.region.height;
        }
    } catch (const std::exception& e) {
        resp.success = false;
        resp.error = e.what();
    }

    return resp;
}

#endif // WINGMAN_ENABLE_VISION

RemoteResponse RemoteControlServer::handleGetPixel(const nlohmann::json& params) {
    RemoteResponse resp;

    int x = params["x"];
    int y = params["y"];

    Color color = Screen::getPixel(x, y);
    resp.success = true;
    resp.data["r"] = color.r;
    resp.data["g"] = color.g;
    resp.data["b"] = color.b;

    char hex[16];
#ifdef _WIN32
    sprintf_s(hex, sizeof(hex), "0x%02X%02X%02X", color.r, color.g, color.b);
#else
    snprintf(hex, sizeof(hex), "0x%02X%02X%02X", color.r, color.g, color.b);
#endif
    resp.data["hex"] = hex;

    return resp;
}

RemoteResponse RemoteControlServer::handleFindColor(const nlohmann::json& params) {
    RemoteResponse resp;

    std::string colorStr = params["color"];
    uint32_t color = std::stoul(colorStr, nullptr, 0);

    int x = params.value("x", 0);
    int y = params.value("y", 0);

#ifdef _WIN32
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
#else
    auto bounds = Screen::getScreenBounds();
    int screenWidth = bounds.width;
    int screenHeight = bounds.height;
#endif

    int width = params.value("width", screenWidth);
    int height = params.value("height", screenHeight);
    int tolerance = params.value("tolerance", 10);

    Rect region{ x, y, width, height };
    Point result;
    bool found = Screen::findColor(Color::fromRGB(color), region, tolerance, result);

    resp.success = found;
    if (found) {
        resp.data["x"] = result.x;
        resp.data["y"] = result.y;
    }
    return resp;
}

RemoteResponse RemoteControlServer::handleClick(const nlohmann::json& params) {
    RemoteResponse resp;

    int x = params["x"];
    int y = params["y"];
    std::string button = params.value("button", "left");

    platform::MouseButton btn = platform::MouseButton::Left;
    if (button == "right") btn = platform::MouseButton::Right;
    else if (button == "middle") btn = platform::MouseButton::Middle;

    if (input_) {
        input_->mouseMove(x, y);
        input_->mouseClick(btn);
    }
    resp.success = true;
    return resp;
}

RemoteResponse RemoteControlServer::handleMove(const nlohmann::json& params) {
    RemoteResponse resp;

    int x = params["x"];
    int y = params["y"];
    int duration = params.value("duration", 100);

    if (input_) {
        if (duration > 0) {
            input_->mouseMove(x, y);
            std::this_thread::sleep_for(std::chrono::milliseconds(duration));
        } else {
            input_->mouseMove(x, y);
        }
    }
    resp.success = true;
    return resp;
}

RemoteResponse RemoteControlServer::handleKey(const nlohmann::json& params) {
    RemoteResponse resp;

    int keyCode = params["key"];
    if (input_) {
        input_->keyPress(static_cast<platform::KeyCode>(keyCode));
    }
    resp.success = true;
    return resp;
}

RemoteResponse RemoteControlServer::handleTypeText(const nlohmann::json& params) {
    RemoteResponse resp;

    std::string text = params["text"];
    int delay = params.value("delay", 50);

    if (input_) {
        input_->textInput(text);
    }
    if (delay > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
    }
    resp.success = true;
    return resp;
}

RemoteResponse RemoteControlServer::handleAddTrigger(const nlohmann::json& params) {
    RemoteResponse resp;

    try {
        TriggerConfig config{};
        config.name = params["config"]["name"];
        config.enabled = params["config"].value("enabled", true);
        config.oneShot = params["config"].value("one_shot", false);
        config.cooldown = params["config"].value("cooldown", 0);

        const auto& cond = params["config"]["condition"];
        std::string typeStr = cond.value("type", "ColorFound");
        if (typeStr == "ColorFound") config.condition.type = TriggerType::ColorFound;
        else if (typeStr == "ColorLost") config.condition.type = TriggerType::ColorLost;
        else if (typeStr == "ImageFound") config.condition.type = TriggerType::ImageFound;
        else if (typeStr == "ImageLost") config.condition.type = TriggerType::ImageLost;
        else if (typeStr == "WindowOpened") config.condition.type = TriggerType::WindowOpened;
        else if (typeStr == "WindowClosed") config.condition.type = TriggerType::WindowClosed;
        else if (typeStr == "ProcessStarted") config.condition.type = TriggerType::ProcessStarted;
        else if (typeStr == "ProcessStopped") config.condition.type = TriggerType::ProcessStopped;
        else if (typeStr == "TimeElapsed") config.condition.type = TriggerType::TimeElapsed;
        else if (typeStr == "HotkeyPressed") config.condition.type = TriggerType::HotkeyPressed;
        else if (typeStr == "PixelChanged") config.condition.type = TriggerType::PixelChanged;
        else {
            resp.success = false;
            resp.error = "Unknown condition type: " + typeStr;
            return resp;
        }

        config.condition.value = cond.value("value", "");
        config.condition.tolerance = cond.value("tolerance", 10);
        config.condition.interval = cond.value("interval", 100);
        config.condition.enabled = cond.value("enabled", true);

        if (cond.contains("region")) {
            config.condition.region.x = cond["region"].value("x", 0);
            config.condition.region.y = cond["region"].value("y", 0);
            config.condition.region.width = cond["region"].value("width", 0);
            config.condition.region.height = cond["region"].value("height", 0);
        }

        auto parseAction = [](const nlohmann::json& actionJson) -> std::optional<TriggerActionData> {
            TriggerActionData action;
            std::string actionType = actionJson.value("type", "Click");
            if (actionType == "RunScript") action.type = BasicTriggerAction::RunScript;
            else if (actionType == "Click") action.type = BasicTriggerAction::Click;
            else if (actionType == "KeyPress") action.type = BasicTriggerAction::KeyPress;
            else if (actionType == "Type") action.type = BasicTriggerAction::Type;
            else if (actionType == "StopScript") action.type = BasicTriggerAction::StopScript;
            else if (actionType == "PauseScript") action.type = BasicTriggerAction::PauseScript;
            else if (actionType == "ShowMessage") action.type = BasicTriggerAction::ShowMessage;
            else if (actionType == "PlayAudio") action.type = BasicTriggerAction::PlayAudio;
            else if (actionType == "Log") action.type = BasicTriggerAction::Log;
            else if (actionType == "Delay") action.type = BasicTriggerAction::Delay;
            else return std::nullopt;

            action.value = actionJson.value("value", "");
            action.x = actionJson.value("x", 0);
            action.y = actionJson.value("y", 0);
            action.delay = actionJson.value("delay", 0);
            return action;
        };

        if (params["config"].contains("actions")) {
            for (const auto& actionJson : params["config"]["actions"]) {
                auto act = parseAction(actionJson);
                if (!act) {
                    resp.success = false;
                    resp.error = "Unknown action type: " + actionJson.value("type", "");
                    return resp;
                }
                config.actions.push_back(*act);
            }
        } else if (params["config"].contains("action")) {
            auto act = parseAction(params["config"]["action"]);
            if (!act) {
                resp.success = false;
                resp.error = "Unknown action type: " + params["config"]["action"].value("type", "");
                return resp;
            }
            config.actions.push_back(*act);
        }

        if (config.actions.empty()) {
            resp.success = false;
            resp.error = "Trigger must have at least one action";
            return resp;
        }

        size_t id = triggerManager_->add(config);

        resp.success = true;
        resp.data["id"] = id;
        resp.data["message"] = "Trigger added";
    } catch (const std::exception& e) {
        resp.success = false;
        resp.error = e.what();
    }

    return resp;
}

RemoteResponse RemoteControlServer::handleRemoveTrigger(const nlohmann::json& params) {
    RemoteResponse resp;

    try {
        size_t id = params["id"];
        triggerManager_->remove(id);
        resp.success = true;
        resp.data["message"] = "Trigger removed";
    } catch (const std::exception& e) {
        resp.success = false;
        resp.error = e.what();
    }

    return resp;
}

RemoteResponse RemoteControlServer::handleEnableTrigger(const nlohmann::json& params) {
    RemoteResponse resp;

    try {
        size_t id = params["id"];
        triggerManager_->enable(id);
        resp.success = true;
        resp.data["message"] = "Trigger enabled";
    } catch (const std::exception& e) {
        resp.success = false;
        resp.error = e.what();
    }

    return resp;
}

RemoteResponse RemoteControlServer::handleDisableTrigger(const nlohmann::json& params) {
    RemoteResponse resp;

    try {
        size_t id = params["id"];
        triggerManager_->disable(id);
        resp.success = true;
        resp.data["message"] = "Trigger disabled";
    } catch (const std::exception& e) {
        resp.success = false;
        resp.error = e.what();
    }

    return resp;
}

RemoteResponse RemoteControlServer::handleListTriggers(const nlohmann::json& /*params*/) {
    RemoteResponse resp;

    auto instances = triggerManager_->getAllTriggerInstances();
    nlohmann::json j = nlohmann::json::array();
    for (const auto& inst : instances) {
        nlohmann::json item;
        item["id"] = inst.id;
        item["name"] = inst.config.name;
        item["enabled"] = inst.config.enabled;

        nlohmann::json cfg;
        cfg["oneShot"] = inst.config.oneShot;
        cfg["cooldown"] = inst.config.cooldown;

        nlohmann::json cond;
        const char* triggerTypeNames[] = {
            "ColorFound", "ColorLost", "ImageFound", "ImageLost",
            "WindowOpened", "WindowClosed", "ProcessStarted", "ProcessStopped",
            "TimeElapsed", "HotkeyPressed", "PixelChanged"
        };
        int ti = static_cast<int>(inst.config.condition.type);
        cond["type"] = (ti >= 0 && ti < 11) ? triggerTypeNames[ti] : "ColorFound";
        cond["value"] = inst.config.condition.value;
        cond["tolerance"] = inst.config.condition.tolerance;
        cond["interval"] = inst.config.condition.interval;
        cond["enabled"] = inst.config.condition.enabled;
        nlohmann::json region;
        region["x"] = inst.config.condition.region.x;
        region["y"] = inst.config.condition.region.y;
        region["width"] = inst.config.condition.region.width;
        region["height"] = inst.config.condition.region.height;
        cond["region"] = region;
        cfg["condition"] = cond;

        const char* actionTypeNames[] = {
            "RunScript", "Click", "KeyPress", "Type",
            "StopScript", "PauseScript", "ShowMessage", "PlayAudio", "Log", "Delay"
        };
        nlohmann::json actionsArr = nlohmann::json::array();
        for (const auto& act : inst.config.actions) {
            nlohmann::json a;
            int ai = static_cast<int>(act.type);
            a["type"] = (ai >= 0 && ai < 10) ? actionTypeNames[ai] : "Click";
            a["value"] = act.value;
            a["x"] = act.x;
            a["y"] = act.y;
            a["delay"] = act.delay;
            actionsArr.push_back(a);
        }
        cfg["actions"] = actionsArr;
        item["config"] = cfg;

        j.push_back(item);
    }

    resp.success = true;
    resp.data["triggers"] = j;
    resp.data["count"] = instances.size();
    return resp;
}

RemoteResponse RemoteControlServer::handleRecordMacro(const nlohmann::json& /*params*/) {
    RemoteResponse resp;
    macroRecorder_->start();
    resp.success = true;
    resp.data["message"] = "Recording started";
    return resp;
}

RemoteResponse RemoteControlServer::handleStopMacroRecording(const nlohmann::json& /*params*/) {
    RemoteResponse resp;
    macroRecorder_->stop();
    resp.success = true;
    resp.data["event_count"] = macroRecorder_->getEventCount();
    return resp;
}

RemoteResponse RemoteControlServer::handlePlayMacro(const nlohmann::json& params) {
    RemoteResponse resp;
    int speed = params.value("speed", 100);
    int repeat = params.value("repeat", 1);
    macroRecorder_->playback(speed, repeat);
    resp.success = true;
    return resp;
}

} // namespace wingman
