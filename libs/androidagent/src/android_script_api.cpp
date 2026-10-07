#include "wingman/androidagent/android_script_api.hpp"

#include "platform/android/android_capture.hpp"
#include "wingman/crypt.hpp"
#include "wingman/vision/image_analyzer.hpp"
#include "wingman/vision_ai.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <thread>

namespace wingman::android {

namespace {

using platform::android::AndroidHostBridge;

// ---------- Lua 参数转换（镜像桌面 module_helpers 的 toColor/toRect 风格）----------

Color toColor(const sol::object& obj) {
    if (obj.is<int>() || obj.is<double>()) {
        // 0xRRGGBB 整数（与桌面 screen.findColor 一致）
        const auto rgb = obj.as<long long>();
        return Color::fromRGB(static_cast<uint32_t>(rgb & 0xFFFFFF));
    }
    if (obj.is<sol::table>()) {
        const sol::table t = obj.as<sol::table>();
        return Color(
            static_cast<uint8_t>(t.get_or("r", 0u)),
            static_cast<uint8_t>(t.get_or("g", 0u)),
            static_cast<uint8_t>(t.get_or("b", 0u)),
            static_cast<uint8_t>(t.get_or("a", 255u)));
    }
    return Color();
}

// 返回是否提供了 region（未提供时传空 Rect = 全屏，ImageAnalyzer 同语义）
bool toRect(const sol::object& obj, Rect& out) {
    if (obj.is<sol::table>()) {
        const sol::table t = obj.as<sol::table>();
        out = Rect(
            t.get_or("x", 0),
            t.get_or("y", 0),
            t.get_or("width", 0),
            t.get_or("height", 0));
        return true;
    }
    return false;
}

// findImage 模板路径：绝对路径直用，相对路径按 filesDir 解析
//（模板经 adb push 到 app external files，见 README）
std::string resolvePath(const std::string& filesDir, const std::string& path) {
    if (path.empty() || filesDir.empty() || path.front() == '/') {
        return path;
    }
    return (std::filesystem::path(filesDir) / path).generic_string();
}

// ---------- 组装辅助 ----------

// 桌面 screen 模块同形：{point|nil, found} 二元数组（索引 1/2）
sol::object pointFoundResult(sol::state& lua, const Point* p) {
    if (p) {
        return sol::make_object(lua, lua.create_table_with(
            1, lua.create_table_with("x", p->x, "y", p->y), 2, true));
    }
    return sol::make_object(lua, lua.create_table_with(2, false));
}

// 桌面 vision 模块同形：{x,y} 点表
sol::object makePointTable(sol::state& lua, const Point& p) {
    return sol::make_object(lua, lua.create_table_with("x", p.x, "y", p.y));
}

// ---------- wingman.input ----------

void registerInputModule(sol::state& lua, std::atomic<bool>& stopFlag,
                         AndroidHostBridge* bridge) {
    sol::table input = lua["wingman"]["input"] = lua.create_table();
    // click：默认短按；durationMs>=500 走长按（桌面 click(x,y,button) 的
    // 触屏化——button 参数不适用，省略）
    input.set_function("click",
        [bridge](int x, int y, sol::optional<int> durationMs) -> bool {
            const int ms = durationMs.value_or(60);
            if (!bridge) {
                return false;
            }
            return ms >= 500 ? bridge->longPress(x, y, ms)
                             : bridge->tap(x, y, ms);
        });
    input.set_function("tap",
        [bridge](int x, int y, sol::optional<int> durationMs) -> bool {
            return bridge && bridge->tap(x, y, durationMs.value_or(60));
        });
    input.set_function("longPress",
        [bridge](int x, int y, sol::optional<int> durationMs) -> bool {
            return bridge && bridge->longPress(x, y, durationMs.value_or(600));
        });
    input.set_function("swipe",
        [bridge](int x1, int y1, int x2, int y2,
                 sol::optional<int> durationMs) -> bool {
            return bridge &&
                   bridge->swipe(x1, y1, x2, y2, durationMs.value_or(300));
        });
    // 与 wingman.sleep 语义一致：50ms 分片 + 停止中断（独立实现避免
    // script_runner 匿名命名空间穿越）
    input.set_function("delay", [&stopFlag](double ms) {
        if (ms < 0) {
            ms = 0;
        }
        constexpr double kSliceMs = 50.0;
        double waited = 0;
        while (waited < ms && !stopFlag.load(std::memory_order_relaxed)) {
            const double step = std::min(kSliceMs, ms - waited);
            std::this_thread::sleep_for(
                std::chrono::milliseconds(static_cast<long long>(step)));
            waited += step;
        }
        if (stopFlag.load(std::memory_order_relaxed)) {
            throw sol::error("script stopped");
        }
    });
}

// ---------- wingman.screen ----------

void registerScreenModule(sol::state& lua, AndroidHostBridge* bridge,
                          const std::string& filesDir) {
    sol::table screen = lua["wingman"]["screen"] = lua.create_table();

    screen.set_function("getScreenWidth", [bridge]() -> int {
        int width = 0;
        int height = 0;
        return bridge && bridge->screenSize(width, height) ? width : 0;
    });
    screen.set_function("getScreenHeight", [bridge]() -> int {
        int width = 0;
        int height = 0;
        return bridge && bridge->screenSize(width, height) ? height : 0;
    });

    // 桌面同形：仅报成功与否
    screen.set_function("capture", [bridge]() -> bool {
        if (!bridge) {
            return false;
        }
        return bridge->captureFrame() != nullptr;
    });

    screen.set_function("getPixel",
        [&lua, bridge](int x, int y) -> sol::object {
            if (!bridge) {
                return sol::lua_nil;
            }
            const auto frame = bridge->captureFrame();
            if (!frame) {
                return sol::lua_nil;
            }
            const Color c = frame->getPixel(x, y);
            return sol::make_object(lua, lua.create_table_with(
                "r", c.r, "g", c.g, "b", c.b, "a", c.a));
        });

    screen.set_function("findColor",
        [&lua, bridge](sol::object colorObj, sol::object regionObj,
                       sol::optional<int> tolerance) -> sol::object {
            if (!bridge) {
                return pointFoundResult(lua, nullptr);
            }
            const auto frame = bridge->captureFrame();
            if (!frame) {
                return pointFoundResult(lua, nullptr);
            }
            const Color target = toColor(colorObj);
            Rect region;
            toRect(regionObj, region);
            vision::ImageAnalyzer analyzer;
            const auto match = analyzer.findColor(
                *frame, target, region, tolerance.value_or(10));
            return pointFoundResult(
                lua, match ? &match->position : nullptr);
        });

    screen.set_function("findColors",
        [&lua, bridge](sol::object colorObj, sol::object regionObj,
                       sol::optional<int> tolerance,
                       sol::optional<int> maxCount) -> sol::object {
            sol::table result = lua.create_table();
            if (!bridge) {
                return sol::make_object(lua, result);
            }
            const auto frame = bridge->captureFrame();
            if (!frame) {
                return sol::make_object(lua, result);
            }
            const Color target = toColor(colorObj);
            Rect region;
            toRect(regionObj, region);
            vision::ImageAnalyzer analyzer;
            const auto matches = analyzer.findColors(
                *frame, target, region, tolerance.value_or(10),
                maxCount.value_or(0));
            int index = 1;
            for (const auto& m : matches) {
                result[index++] = makePointTable(lua, m.position);
            }
            return sol::make_object(lua, result);
        });

    // 桌面同形：screen.findImage(imagePath, region?, threshold=0.9) →
    // {point|nil, found}（与 vision.findImage 的对象返回不同，保持两端一致）
    screen.set_function("findImage",
        [&lua, bridge, filesDir](const std::string& path,
                                 sol::object regionObj,
                                 sol::optional<double> threshold) -> sol::object {
            if (!bridge) {
                return pointFoundResult(lua, nullptr);
            }
            const auto frame = bridge->captureFrame();
            if (!frame) {
                return pointFoundResult(lua, nullptr);
            }
            Rect region;
            toRect(regionObj, region);
            vision::ImageAnalyzer analyzer;
            const auto match = analyzer.findImage(
                *frame, resolvePath(filesDir, path), region,
                threshold.value_or(0.9));
            return pointFoundResult(
                lua, match ? &match->position : nullptr);
        });
}

// ---------- wingman.vision ----------

void registerVisionModule(sol::state& lua, AndroidHostBridge* bridge,
                          const std::string& filesDir) {
    sol::table vision = lua["wingman"]["vision"] = lua.create_table();

    // 桌面同形：{x,y} 或 nil
    vision.set_function("findColor",
        [&lua, bridge](sol::object colorObj, sol::optional<int> tolerance,
                       sol::object regionObj) -> sol::object {
            if (!bridge) {
                return sol::lua_nil;
            }
            const auto frame = bridge->captureFrame();
            if (!frame) {
                return sol::lua_nil;
            }
            const Color target = toColor(colorObj);
            Rect region;
            toRect(regionObj, region);
            vision::ImageAnalyzer analyzer;
            const auto match = analyzer.findColor(
                *frame, target, region, tolerance.value_or(10));
            return match ? makePointTable(lua, match->position)
                         : sol::lua_nil;
        });

    vision.set_function("findAllColors",
        [&lua, bridge](sol::object colorObj, sol::optional<int> tolerance,
                       sol::object regionObj) -> sol::object {
            sol::table result = lua.create_table();
            if (!bridge) {
                return sol::make_object(lua, result);
            }
            const auto frame = bridge->captureFrame();
            if (!frame) {
                return sol::make_object(lua, result);
            }
            const Color target = toColor(colorObj);
            Rect region;
            toRect(regionObj, region);
            vision::ImageAnalyzer analyzer;
            const auto matches = analyzer.findColors(
                *frame, target, region, tolerance.value_or(10), 0);
            int index = 1;
            for (const auto& m : matches) {
                result[index++] = makePointTable(lua, m.position);
            }
            return sol::make_object(lua, result);
        });

    vision.set_function("hasColor",
        [bridge](sol::object colorObj, sol::optional<int> tolerance,
                 sol::object regionObj) -> bool {
            if (!bridge) {
                return false;
            }
            const auto frame = bridge->captureFrame();
            if (!frame) {
                return false;
            }
            const Color target = toColor(colorObj);
            Rect region;
            toRect(regionObj, region);
            vision::ImageAnalyzer analyzer;
            return analyzer.findColor(
                *frame, target, region, tolerance.value_or(10)).has_value();
        });

    // 桌面 getDominantColor 为 kmeans k=1；此处纯 C++ 均色近似（语义
    // 一致、NDK 免拉重依赖），差异在设计文档标注
    vision.set_function("getDominantColor",
        [&lua, bridge](sol::object regionObj) -> sol::object {
            if (!bridge) {
                return sol::lua_nil;
            }
            const auto frame = bridge->captureFrame();
            if (!frame) {
                return sol::lua_nil;
            }
            Rect region;
            if (!toRect(regionObj, region)) {
                region = Rect(0, 0, frame->getWidth(), frame->getHeight());
            }
            region = vision::ImageAnalyzer::clampRegion(region, *frame);
            if (region.isEmpty()) {
                return sol::lua_nil;
            }
            long long sumR = 0;
            long long sumG = 0;
            long long sumB = 0;
            const long long count = region.width * region.height;
            for (int y = region.y; y < region.y + region.height; ++y) {
                for (int x = region.x; x < region.x + region.width; ++x) {
                    const Color c = frame->getPixel(x, y);
                    sumR += c.r;
                    sumG += c.g;
                    sumB += c.b;
                }
            }
            return sol::make_object(lua, lua.create_table_with(
                "r", static_cast<unsigned>(sumR / count),
                "g", static_cast<unsigned>(sumG / count),
                "b", static_cast<unsigned>(sumB / count),
                "a", 255u));
        });

    // 桌面同形对象：vision.findImage(templatePath, threshold=0.9, region?)
    // → {found, position?, confidence?, region?}
    vision.set_function("findImage",
        [&lua, bridge, filesDir](const std::string& path,
                                 sol::optional<double> threshold,
                                 sol::object regionObj) -> sol::object {
            sol::table result = lua.create_table();
            result["found"] = false;
            if (!bridge) {
                return sol::make_object(lua, result);
            }
            const auto frame = bridge->captureFrame();
            if (!frame) {
                return sol::make_object(lua, result);
            }
            Rect region;
            const bool hasRegion = toRect(regionObj, region);
            vision::ImageAnalyzer analyzer;
            const auto match = analyzer.findImage(
                *frame, resolvePath(filesDir, path), region,
                threshold.value_or(0.9));
            if (match) {
                result["found"] = true;
                result["position"] = makePointTable(lua, match->position);
                result["confidence"] = match->confidence;
                if (!hasRegion) {
                    result["region"] = lua.create_table_with(
                        "x", match->matchedRegion.x,
                        "y", match->matchedRegion.y,
                        "width", match->matchedRegion.width,
                        "height", match->matchedRegion.height);
                }
            }
            return sol::make_object(lua, result);
        });

    // ===== AI 视觉识别（桌面 vision 模块同形；OpenAI 兼容 provider）=====
    // 语义与 docs/api/vision.md「AI 视觉识别」节一致：配置驻内存、
    // apiKeyEnc 走 AES-256-GCM 密文+口令解密面（明文不落盘不落日志）、
    // locate 结构化 bbox 供脚本 input.click 动作映射。截帧经
    // registerAndroidApis 注入的 captureFrame 适配（NDK 无 Screen::capture）。
    vision.set_function("aiSetup", [](const sol::table& cfg) -> bool {
        VisionAiConfig c;
        c.baseUrl = cfg.get_or("baseUrl", std::string{});
        c.model = cfg.get_or("model", std::string{});
        c.timeoutSeconds = cfg.get_or("timeoutSeconds", 60);
        c.apiKey = cfg.get_or("apiKey", std::string{});
        // 凭据加密面（与桌面 vision_module aiSetup 同语义）：
        // apiKeyEnc = crypto.encryptAES 密文，二者成对传入
        const sol::object enc = cfg["apiKeyEnc"];
        if (enc.valid() && !enc.is<sol::nil_t>()) {
            const sol::object pass = cfg["passphrase"];
            if (!pass.valid() || pass.is<sol::nil_t>()) {
                return false;
            }
            c.apiKey = crypt::decryptAES(enc.as<std::string>(),
                                         pass.as<std::string>());
            if (c.apiKey.empty()) {
                return false;
            }
        }
        if (c.baseUrl.empty() || c.model.empty()) {
            return false;
        }
        VisionAi::setup(c);
        return true;
    });

    vision.set_function("aiSetupStatus", [&lua]() -> sol::object {
        return sol::make_object(lua, lua.create_table_with(
            "configured", VisionAi::isConfigured(),
            "baseUrl", VisionAi::config().baseUrl,
            "model", VisionAi::config().model,
            "hasKey", VisionAi::hasApiKey(),
            "lastError", VisionAi::lastError()));
    });

    vision.set_function("aiLocate",
        [&lua](const std::string& desc, sol::optional<sol::object> regionObj)
            -> sol::object {
            sol::table result = lua.create_table();
            Rect region;
            const bool hasRegion = regionObj && toRect(*regionObj, region);
            const VisionAiBox box = VisionAi::locate(
                desc, hasRegion ? region : Rect());
            if (box.found) {
                result["found"] = true;
                result["x"] = box.x;
                result["y"] = box.y;
                result["w"] = box.w;
                result["h"] = box.h;
                result["confidence"] = box.confidence;
                result["label"] = box.label;
            } else {
                result["found"] = false;
                result["error"] = VisionAi::lastError();
            }
            return sol::make_object(lua, result);
        });
}

} // namespace

void registerAndroidApis(sol::state& lua, std::atomic<bool>& stopFlag,
                         platform::android::AndroidHostBridge* bridge,
                         const std::string& filesDir) {
    registerInputModule(lua, stopFlag, bridge);
    registerScreenModule(lua, bridge, filesDir);
    registerVisionModule(lua, bridge, filesDir);
    // AI provider 截帧注入：captureFrame 位图复用同一 provider 通路
    //（空 region=全屏；NDK 下 Screen::capture 无装配，公共层平台宏冻结）。
    // stub 构建（无 OpenCV）为空实现假体，注入无副作用。重复注册整体
    // 覆盖（与 setGlobalHostBridge 同款）。
    VisionAi::setFrameProvider([bridge](const Rect& region) {
        platform::android::AndroidCaptureSource source(bridge);
        return source.capture(region);
    });
}

} // namespace wingman::android
