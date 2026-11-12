// MCP tools 实现（7 个）。零平台宏于调用面：平台差异只出现在
// screen_shot 的 vision 编译分支与生产单例的运行时降级（Linux 无 UIA
// 工厂 → 明确报不支持）。Windows CI（codecov 唯一数据源）整文件编译。
#include "wingman/mcp/mcp_tools.hpp"

#include "wingman/process.hpp"
#include "wingman/screen.hpp"
#include "wingman/script_manager.hpp"
#include "wingman/ui_automation.hpp"
#include "wingman/window.hpp"

#include "wingman/platform/input_factory.hpp"

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#ifdef WINGMAN_ENABLE_VISION
#include <opencv2/opencv.hpp>

#include "wingman/crypt.hpp"
#endif

namespace wingman::mcp {

namespace {

// ========== 公共形状 ==========

nlohmann::json textContent(const nlohmann::json& payload) {
    return {{"type", "text"}, {"text", payload.dump()}};
}

nlohmann::json textResult(nlohmann::json payload) {
    return {{"content", nlohmann::json::array({textContent(std::move(payload))})}};
}

nlohmann::json errorResult(const std::string& message) {
    return {
        {"content", nlohmann::json::array({textContent({{"error", message}})})},
        {"isError", true},
    };
}

// ========== UIA ==========

// UIARole → 字符串（全枚举；仓库无现成 helper，LLM 消费面要名字不要整数）。
const char* roleToString(UIARole role) {
    switch (role) {
        case UIARole::Unknown: return "Unknown";
        case UIARole::Window: return "Window";
        case UIARole::Button: return "Button";
        case UIARole::TextBox: return "TextBox";
        case UIARole::CheckBox: return "CheckBox";
        case UIARole::RadioButton: return "RadioButton";
        case UIARole::ComboBox: return "ComboBox";
        case UIARole::ListBox: return "ListBox";
        case UIARole::ListItem: return "ListItem";
        case UIARole::Menu: return "Menu";
        case UIARole::MenuItem: return "MenuItem";
        case UIARole::Table: return "Table";
        case UIARole::Tree: return "Tree";
        case UIARole::TreeItem: return "TreeItem";
        case UIARole::Hyperlink: return "Hyperlink";
        case UIARole::Image: return "Image";
        case UIARole::Slider: return "Slider";
        case UIARole::Spinner: return "Spinner";
        case UIARole::ProgressBar: return "ProgressBar";
        case UIARole::Tab: return "Tab";
        case UIARole::TabItem: return "TabItem";
    }
    return "Unknown";
}

// 字符串 → UIARole（uia_find 的 role 入参；未知名 → Unknown 交由调用方报错）。
UIARole roleFromString(const std::string& name) {
    static const std::map<std::string, UIARole> table = {
        {"Unknown", UIARole::Unknown},
        {"Window", UIARole::Window},
        {"Button", UIARole::Button},
        {"TextBox", UIARole::TextBox},
        {"CheckBox", UIARole::CheckBox},
        {"RadioButton", UIARole::RadioButton},
        {"ComboBox", UIARole::ComboBox},
        {"ListBox", UIARole::ListBox},
        {"ListItem", UIARole::ListItem},
        {"Menu", UIARole::Menu},
        {"MenuItem", UIARole::MenuItem},
        {"Table", UIARole::Table},
        {"Tree", UIARole::Tree},
        {"TreeItem", UIARole::TreeItem},
        {"Hyperlink", UIARole::Hyperlink},
        {"Image", UIARole::Image},
        {"Slider", UIARole::Slider},
        {"Spinner", UIARole::Spinner},
        {"ProgressBar", UIARole::ProgressBar},
        {"Tab", UIARole::Tab},
        {"TabItem", UIARole::TabItem},
    };
    const auto it = table.find(name);
    return it != table.end() ? it->second : UIARole::Unknown;
}

// UIA 元素句柄注册表（misc_modules.cpp 同款先例：int handle 从 1 自增，
// 0 无效）。MCP 会话内单线程（McpServer::run 单线程前提），注册表无锁、
// 只增不减——进程生命周期与 server 一致，泄漏量以会话查询过的元素数为上界。
std::map<int, std::shared_ptr<IUIAElement>>& elementRegistry() {
    static std::map<int, std::shared_ptr<IUIAElement>> registry;
    return registry;
}

int storeElement(const std::shared_ptr<IUIAElement>& element) {
    static std::atomic<int> nextId{0};
    const int id = ++nextId;
    elementRegistry()[id] = element;
    return id;
}

std::shared_ptr<IUIAElement> elementByHandle(int handle) {
    auto& registry = elementRegistry();
    const auto it = registry.find(handle);
    return it != registry.end() ? it->second : nullptr;
}

nlohmann::json boundsToJson(const Rect& bounds) {
    return {
        {"x", bounds.x},
        {"y", bounds.y},
        {"width", bounds.width},
        {"height", bounds.height},
    };
}

// 元素 JSON 键沿用 misc_modules uia 模块口径（+handle / role 名字化）。
nlohmann::json elementToJson(const std::shared_ptr<IUIAElement>& element,
                             bool withHandle, int depth = 0) {
    const auto info = element->getInfo();
    nlohmann::json node = {
        {"name", info.name},
        {"id", info.id},
        {"className", info.className},
        {"role", roleToString(info.role)},
        {"text", info.text},
        {"is_enabled", element->isEnabled()},
        {"is_visible", element->isVisible()},
        {"has_focus", element->hasFocus()},
        {"bounds", boundsToJson(info.bounds)},
    };
    if (withHandle) {
        node["handle"] = storeElement(element);
    }
    if (depth > 0) {
        nlohmann::json children = nlohmann::json::array();
        for (const auto& child : element->getChildren()) {
            if (child) {
                children.push_back(elementToJson(child, withHandle, depth - 1));
            }
        }
        node["children"] = std::move(children);
    }
    return node;
}

// 解析 UIA 后端：注入优先（测试 mock），生产走 wingman::uia() 门面（懒初始
// 化幂等；Linux 工厂返回 nullptr → initialize false → 明确报不支持）。
bool resolveUia(const ToolContext& ctx, std::string& error) {
    if (ctx.uiaManager != nullptr) {
        return true;
    }
    auto& facade = uia();
    if (!facade.initialize()) {
        error = "UI Automation is not supported on this platform";
        return false;
    }
    return true;
}

std::shared_ptr<IUIAElement> elementFromWindow(const ToolContext& ctx, uint64_t hwnd) {
    if (ctx.uiaManager != nullptr) {
        return ctx.uiaManager->getElementFromWindow(hwnd);
    }
    return uia().fromWindow(hwnd);
}

std::shared_ptr<IUIAElement> findElementBySelector(const ToolContext& ctx,
                                                   const UIASelector& selector) {
    if (ctx.uiaManager != nullptr) {
        return ctx.uiaManager->findElement(selector);
    }
    return uia().find(selector);
}

std::vector<std::shared_ptr<IUIAElement>> findAllByRole(const ToolContext& ctx, UIARole role) {
    if (ctx.uiaManager != nullptr) {
        return ctx.uiaManager->findAllByRole(role);
    }
    return uia().findAllByRole(role);
}

nlohmann::json uiaTreeTool(const ToolContext& ctx, const nlohmann::json& args) {
    std::string error;
    if (!resolveUia(ctx, error)) {
        return errorResult(error);
    }

    // window 缺省 = 前台窗口；给了标题找不到 = 明确错误（不静默换前台）。
    WindowHandle hwnd = 0;
    if (args.contains("window") && args.at("window").is_string()) {
        const auto title = args.at("window").get<std::string>();
        hwnd = Window::find(title);
        if (hwnd == 0) {
            return errorResult("window not found: " + title);
        }
    } else {
        hwnd = Window::getForeground();
    }

    const auto root = elementFromWindow(ctx, hwnd);
    if (!root) {
        // 无元素是正常空结果（该窗口无 UIA 树），不是错误。
        return textResult({{"window", hwnd}, {"elements", nlohmann::json::array()}});
    }
    return textResult({{"window", hwnd},
                       {"elements", elementToJson(root, /*withHandle=*/true, /*depth=*/3)}});
}

nlohmann::json uiaFindTool(const ToolContext& ctx, const nlohmann::json& args) {
    std::string error;
    if (!resolveUia(ctx, error)) {
        return errorResult(error);
    }

    UIASelector selector;
    if (args.contains("name") && args.at("name").is_string()) {
        selector.withName(args.at("name").get<std::string>());
    }
    if (args.contains("id") && args.at("id").is_string()) {
        selector.withId(args.at("id").get<std::string>());
    }
    if (args.contains("className") && args.at("className").is_string()) {
        selector.withClassName(args.at("className").get<std::string>());
    }
    if (args.contains("text") && args.at("text").is_string()) {
        selector.withText(args.at("text").get<std::string>());
    }
    UIARole role = UIARole::Unknown;
    if (args.contains("role") && args.at("role").is_string()) {
        role = roleFromString(args.at("role").get<std::string>());
        if (role == UIARole::Unknown) {
            return errorResult("unknown role name (use e.g. Button, TextBox, CheckBox)");
        }
        selector.withRole(role);
    }
    // 空选择器 = 全量匹配 ≈ 整棵桌面树倒给 LLM——明确拒绝。
    if (selector.name.empty() && selector.id.empty() && selector.className.empty() &&
        selector.text.empty() && role == UIARole::Unknown) {
        return errorResult("at least one criterion is required: name/id/className/role/text");
    }

    nlohmann::json matches = nlohmann::json::array();
    if (role != UIARole::Unknown) {
        // 角色可枚举：findAllByRole（后端唯一的多元素查询面）+ 选择器过滤。
        for (const auto& element : findAllByRole(ctx, role)) {
            if (element && selector.matches(element->getInfo())) {
                matches.push_back(elementToJson(element, /*withHandle=*/true));
            }
        }
    } else {
        if (const auto element = findElementBySelector(ctx, selector)) {
            matches.push_back(elementToJson(element, /*withHandle=*/true));
        }
    }
    return textResult({{"elements", std::move(matches)}});
}

// click 定位顺序：handle → name/id 选择器。失败明确报错，绝不静默转坐标。
nlohmann::json uiaClickTool(const ToolContext& ctx, const nlohmann::json& args) {
    std::string error;
    if (!resolveUia(ctx, error)) {
        return errorResult(error);
    }

    std::shared_ptr<IUIAElement> element;
    if (args.contains("handle") && args.at("handle").is_number_integer()) {
        element = elementByHandle(args.at("handle").get<int>());
        if (!element) {
            return errorResult("invalid element handle (expired or from another session)");
        }
    } else {
        UIASelector selector;
        if (args.contains("name") && args.at("name").is_string()) {
            selector.withName(args.at("name").get<std::string>());
        }
        if (args.contains("id") && args.at("id").is_string()) {
            selector.withId(args.at("id").get<std::string>());
        }
        if (selector.name.empty() && selector.id.empty()) {
            return errorResult("identify the element via handle, name or id");
        }
        element = findElementBySelector(ctx, selector);
        if (!element) {
            return errorResult("element not found");
        }
    }

    if (!element->click()) {
        return errorResult("click failed (element may not support the invoke pattern)");
    }
    const auto info = element->getInfo();
    return textResult({{"clicked", true},
                       {"name", info.name},
                       {"role", roleToString(info.role)}});
}

nlohmann::json uiaActionTool(const ToolContext& ctx, const nlohmann::json& args) {
    std::string error;
    if (!resolveUia(ctx, error)) {
        return errorResult(error);
    }
    if (!args.contains("handle") || !args.at("handle").is_number_integer()) {
        return errorResult("integer handle is required");
    }
    const auto element = elementByHandle(args.at("handle").get<int>());
    if (!element) {
        return errorResult("invalid element handle (expired or from another session)");
    }
    if (!args.contains("action") || !args.at("action").is_string()) {
        return errorResult("string action is required");
    }
    const auto action = args.at("action").get<std::string>();

    bool ok = false;
    if (action == "click") {
        ok = element->click();
    } else if (action == "doubleClick") {
        ok = element->doubleClick();
    } else if (action == "rightClick") {
        ok = element->rightClick();
    } else if (action == "setFocus") {
        ok = element->setFocus();
    } else if (action == "select") {
        ok = element->select();
    } else if (action == "expand") {
        ok = element->expand();
    } else if (action == "collapse") {
        ok = element->collapse();
    } else if (action == "setText") {
        ok = element->setText(args.value("value", ""));
    } else if (action == "setChecked") {
        ok = element->setChecked(args.value("checked", false));
    } else {
        return errorResult("unknown action (click/doubleClick/rightClick/setFocus/"
                           "select/expand/collapse/setText/setChecked)");
    }
    if (!ok) {
        return errorResult("action failed: " + action);
    }
    return textResult({{"action", action}, {"ok", true}});
}

// ========== screen_shot ==========

nlohmann::json screenShotTool(const ToolContext& /*ctx*/, const nlohmann::json& args) {
#ifdef WINGMAN_ENABLE_VISION
    // region: [x, y, width, height]；缺省 = 全屏（主显示器边界）。
    Rect region = Screen::getScreenBounds();
    if (args.contains("region") && args.at("region").is_array() &&
        args.at("region").size() == 4) {
        const auto& r = args.at("region");
        region = Rect(r[0].get<int>(), r[1].get<int>(), r[2].get<int>(), r[3].get<int>());
        if (region.width <= 0 || region.height <= 0) {
            return errorResult("region width/height must be positive");
        }
    }

    const auto bitmap = Screen::capture(region);
    if (!bitmap) {
        return errorResult("failed to capture screen");
    }

    const int width = bitmap->getWidth();
    const int height = bitmap->getHeight();
    cv::Mat bgraMat(height, width, CV_8UC4, bitmap->getData());
    cv::Mat bgrMat;
    cv::cvtColor(bgraMat, bgrMat, cv::COLOR_BGRA2BGR);

    std::vector<uchar> jpegBuffer;
    std::vector<int> encodeParams = {cv::IMWRITE_JPEG_QUALITY, 82};
    if (!cv::imencode(".jpg", bgrMat, jpegBuffer, encodeParams)) {
        return errorResult("failed to encode screenshot");
    }
    const std::vector<uint8_t> bytes(jpegBuffer.begin(), jpegBuffer.end());

    // MCP image content block（客户端可直接渲染）+ text 元数据（结构化消费面）。
    return {
        {"content", nlohmann::json::array({
            {{"type", "image"},
             {"data", crypt::base64Encode(bytes)},
             {"mimeType", "image/jpeg"}},
            textContent({
                {"width", width},
                {"height", height},
                {"region", boundsToJson(region)},
            }),
        })},
    };
#else
    (void)args;
    return errorResult("screenshot requires a vision build (OpenCV)");
#endif
}

// ========== input_click ==========

nlohmann::json inputClickTool(const ToolContext& ctx, const nlohmann::json& args) {
    if (!args.contains("x") || !args.at("x").is_number_integer() ||
        !args.contains("y") || !args.at("y").is_number_integer()) {
        return errorResult("integer x and y are required");
    }
    const auto button = args.value("button", "left");
    if (button != "left" && button != "right") {
        return errorResult("button must be left or right");
    }

    platform::IInput* input = ctx.input != nullptr ? ctx.input : platform::defaultSharedInput().get();
    if (input == nullptr) {
        return errorResult("input backend unavailable");
    }
    // 共享单例可能已被进程内其他链路初始化；重复 initialize 会替换底层
    // display 连接（X11 泄漏），BackendInfo.isInitialized 先探再 init。
    if (!input->getBackendInfo().isInitialized && !input->initialize({})) {
        return errorResult("input backend initialization failed (no display?)");
    }

    const int x = args.at("x").get<int>();
    const int y = args.at("y").get<int>();
    input->mouseMove(x, y);
    input->mouseClick(button == "right" ? platform::MouseButton::Right
                                        : platform::MouseButton::Left);
    return textResult({{"clicked", true}, {"x", x}, {"y", y}, {"button", button}});
}

// ========== script_run ==========

// 临时脚本文件 RAII（进程级唯一名，掺 pid 防并行互踩）。
class TempScriptFile {
public:
    TempScriptFile(const std::string& code, const char* extension) {
        static std::atomic<int> seq{0};
        path_ = std::filesystem::temp_directory_path() /
                ("wingman_mcp_" + std::to_string(wingman::Process::getCurrentId()) + "_" +
                 std::to_string(seq.fetch_add(1)) + extension);
        std::ofstream out(path_, std::ios::binary | std::ios::trunc);
        if (out) {
            out << code;
            valid_ = true;
        }
    }
    ~TempScriptFile() {
        std::error_code ec;
        std::filesystem::remove(path_, ec);
    }
    TempScriptFile(const TempScriptFile&) = delete;
    TempScriptFile& operator=(const TempScriptFile&) = delete;

    bool valid() const { return valid_; }
    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
    bool valid_ = false;
};

nlohmann::json scriptRunTool(const ToolContext& ctx, const nlohmann::json& args) {
    if (!args.contains("code") || !args.at("code").is_string() ||
        args.at("code").get<std::string>().empty()) {
        return errorResult("non-empty string code is required");
    }
    int timeoutMs = 30000;
    if (args.contains("timeoutMs")) {
        if (!args.at("timeoutMs").is_number_integer()) {
            return errorResult("timeoutMs must be an integer");
        }
        timeoutMs = args.at("timeoutMs").get<int>();
        if (timeoutMs <= 0) {
            return errorResult("timeoutMs must be positive");
        }
    }

    TempScriptFile file(args.at("code").get<std::string>(), ".lua");
    if (!file.valid()) {
        return errorResult("failed to write temp script file");
    }

    // 局部 manager：会话隔离（脚本名/输出回调/env 不与其它链路共享）；
    // ctx.scriptManager 非空时用注入实例（测试缝）。
    ScriptManager localManager;
    ScriptManager& manager =
        ctx.scriptManager != nullptr ? *ctx.scriptManager : localManager;
    // Lua 引擎由宿主进程注册（wingman-agent main / 测试 SetUp），lib 编译期
    // 不可见——运行时探测，未注册给明确错误而非哑失败。
    const auto& languages = manager.getAvailableLanguages();
    if (std::find(languages.begin(), languages.end(), "lua") == languages.end()) {
        return errorResult("script engine unavailable (no lua engine registered)");
    }
    std::string output;
    manager.setOutputCallback(
        [&output](const std::string& /*name*/, const std::string& line) {
            output += line;
            output += '\n';
        });

    const std::string scriptName = "__mcp__:" + file.path().filename().string();
    ScriptConfig config;
    config.name = scriptName;
    config.sandboxed = true; // MCP 面向 LLM：默认沙箱（与 CLI script 命令同口径）
    config.timeoutMs = timeoutMs;
    if (!manager.loadScript(scriptName, file.path().string(), config)) {
        return errorResult("failed to load script");
    }
    const bool ok = manager.runScript(scriptName);
    auto info = manager.getScriptInfo(scriptName);
    nlohmann::json payload = {
        {"ok", ok},
        {"output", output},
    };
    if (!ok && info && !info->lastError.empty()) {
        payload["error"] = info->lastError; // 超时/脚本异常文案（协作停止语义见
                                            // script_manager.cpp 超时收尾）
    }
    manager.unloadScript(scriptName);
    return textResult(std::move(payload));
}

struct ToolEntry {
    const char* name;
    nlohmann::json (*fn)(const ToolContext&, const nlohmann::json&);
};

constexpr ToolEntry kTools[] = {
    {"wingman_uia_tree", uiaTreeTool},
    {"wingman_uia_find", uiaFindTool},
    {"wingman_uia_click", uiaClickTool},
    {"wingman_uia_action", uiaActionTool},
    {"wingman_screen_shot", screenShotTool},
    {"wingman_input_click", inputClickTool},
    {"wingman_script_run", scriptRunTool},
};

} // namespace

std::optional<nlohmann::json> callTool(const std::string& name,
                                       const nlohmann::json& args,
                                       const ToolContext& ctx) {
    for (const auto& tool : kTools) {
        if (name == tool.name) {
            return tool.fn(ctx, args);
        }
    }
    return std::nullopt;
}

} // namespace wingman::mcp
