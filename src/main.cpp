#include "wingman/screen.hpp"
#include "wingman/input.hpp"
#include "wingman/window.hpp"
#include "wingman/process.hpp"
#include "wingman/version.hpp"
#include "wingman/http_server.hpp"
#include "wingman/tray.hpp"
#include "wingman/config.hpp"

// Lua HTTP support (requires sol2)
#ifdef WINGMAN_BUILD_LUA_HTTP
#include "wingman/lua_http.hpp"
#include "lua_http_bindings.hpp"
#include <sol/sol.hpp>
#endif

#include "lua_bindings.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <memory>
#include <csignal>
#include <thread>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define GetCurrentDir _getcwd
#else
#include <unistd.h>
#define GetCurrentDir getcwd
#endif

using namespace wingman;
using namespace wingman::lua;

// Global HTTP server pointers for signal handling
static std::unique_ptr<HTTPServer> g_httpServer;
#ifdef WINGMAN_BUILD_LUA_HTTP
static std::unique_ptr<LuaHTTPServer> g_luaHttpServer;
#endif
static std::unique_ptr<ConfigManager> g_config;
static bool g_running = true;

// Signal handler for graceful shutdown
void signalHandler(int signal) {
    std::cout << "\nShutting down...\n";
    g_running = false;
    if (g_httpServer) {
        g_httpServer->stop();
    }
#ifdef WINGMAN_BUILD_LUA_HTTP
    if (g_luaHttpServer) {
        g_luaHttpServer->stop();
    }
#endif
    exit(0);
}

// Print usage information
void printUsage() {
    std::cout << "Wingman v" << wingman::version::getFullVersion()
              << " - Game Automation Programmable Control Engine\n\n";
    std::cout << "Usage:\n";
    std::cout << "  wingman.exe <script.lua>              Run Lua script\n";
    std::cout << "  wingman.exe --server [port]           Start HTTP server (default: 8080)\n";
#ifdef WINGMAN_BUILD_LUA_HTTP
    std::cout << "  wingman.exe --lua-http [port] [script]  Start Lua HTTP server (default: 8081)\n";
#endif
    std::cout << "  wingman.exe --pixel <x> <y>          Get pixel color at position\n";
    std::cout << "  wingman.exe --find <color> <region>  Find color on screen\n";
    std::cout << "  wingman.exe --capture [file.png]     Capture screen and save\n";
    std::cout << "  wingman.exe --list                   List all windows\n";
    std::cout << "  wingman.exe --version               Show version information\n";
    std::cout << "\nExamples:\n";
    std::cout << "  wingman.exe script.lua\n";
    std::cout << "  wingman.exe --server\n";
#ifdef WINGMAN_BUILD_LUA_HTTP
    std::cout << "  wingman.exe --lua-http 8081 scripts/http_routes.lua\n";
#endif
    std::cout << "  wingman.exe --server 9000\n";
    std::cout << "  wingman.exe --pixel 100 200\n";
    std::cout << "  wingman.exe --find 0xFF0000 0,0,1920,1080\n";
    std::cout << "  wingman.exe --capture screenshot.png\n";
}

// Parse color (format: 0xRRGGBB or #RRGGBB)
Color parseColor(const std::string& colorStr) {
    std::string str = colorStr;
    if (str.empty()) {
        return Color();
    }

    // Remove 0x prefix
    if (str.substr(0, 2) == "0x") {
        str = str.substr(2);
    }
    // Remove # prefix
    if (str.substr(0, 1) == "#") {
        str = str.substr(1);
    }

    try {
        uint32_t rgb = std::stoul(str, nullptr, 16);
        return Color::fromRGB(rgb);
    } catch (...) {
        return Color();
    }
}

// Parse region (format: x,y,width,height or x1,y1,x2,y2)
Rect parseRegion(const std::string& regionStr) {
    // Simple implementation, default to full screen
    int width = Screen::getScreenWidth();
    int height = Screen::getScreenHeight();
    return Rect(0, 0, width, height);
}

// Command: Get pixel color
int cmdPixel(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cerr << "Usage: --pixel <x> <y>\n";
        return 1;
    }

    int x = std::stoi(args[0]);
    int y = std::stoi(args[1]);

    Color color = Screen::getPixel(x, y);

    std::cout << "Color at position (" << x << ", " << y << "):\n";
    std::cout << "  RGB: (" << static_cast<int>(color.r) << ", "
              << static_cast<int>(color.g) << ", "
              << static_cast<int>(color.b) << ")\n";
    std::cout << "  HEX: 0x" << std::hex << color.toRGB() << std::dec << "\n";

    return 0;
}

// Command: Find color
int cmdFind(const std::vector<std::string>& args) {
    if (args.size() < 1) {
        std::cerr << "Usage: --find <color> [region]\n";
        return 1;
    }

    Color color = parseColor(args[0]);
    Rect region = Screen::getScreenBounds();

    if (args.size() >= 2) {
        region = parseRegion(args[1]);
    }

    Point result;
    if (Screen::findColor(color, region, 10, result)) {
        std::cout << "Found color at: (" << result.x << ", " << result.y << ")\n";
        return 0;
    } else {
        std::cout << "Color not found\n";
        return 1;
    }
}

// Command: Capture screen
int cmdCapture(const std::vector<std::string>& args) {
    std::string filename = "screenshot.png";

    if (!args.empty()) {
        filename = args[0];
    }

    auto bitmap = Screen::capture();
    if (!bitmap) {
        std::cerr << "Screen capture failed\n";
        return 1;
    }

    if (bitmap->save(filename)) {
        std::cout << "Screenshot saved to: " << filename << "\n";
        return 0;
    } else {
        std::cerr << "Failed to save screenshot\n";
        return 1;
    }
}

// Command: List windows
int cmdList() {
    auto windows = Window::enumerate();

    std::cout << "Window list:\n";
    for (const auto& win : windows) {
        std::cout << "  [" << win.handle << "] "
                  << (win.isForeground ? "* " : "  ")
                  << win.title << "\n";
    }

    std::cout << "\nTotal: " << windows.size() << " windows\n";
    return 0;
}

// Command: Start with tray icon (default mode)
int cmdTray() {
    // Setup signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // Initialize ConfigManager
    g_config = std::make_unique<ConfigManager>("config");
    auto serverConfig = g_config->getServerConfig();
    auto trayConfig = g_config->getTrayConfig();

    std::cout << "[CONFIG] 服务器配置: " << serverConfig.host << ":" << serverConfig.port << "\n";
    std::cout.flush();

    // Create tray icon with tooltip from config
    auto icon = std::make_shared<TrayIcon>(trayConfig.tooltip);

    // Set icon from config or try default paths
    std::string iconPath = trayConfig.iconPath;
    if (iconPath.empty()) {
        std::vector<std::string> defaultPaths = {
            "assets/wingman.ico",
            "../assets/wingman.ico",
            "../../assets/wingman.ico",
            "wingman.ico"
        };
        for (const auto& path : defaultPaths) {
            if (std::ifstream(path).good()) {
                iconPath = path;
                break;
            }
        }
    }

    if (!iconPath.empty() && std::ifstream(iconPath).good()) {
        icon->setIcon(iconPath);
    } else {
        std::cout << "Warning: Could not find tray icon, using default icon\n";
    }

    // Set action handler for menu items
    icon->setActionHandler([&serverConfig](const TrayMenuItemConfig& item) {
        std::cout << "\n[TRAY] 菜单项点击: " << item.label << "\n";
        std::cout.flush();

        switch (item.actionType) {
            case TrayActionType::command:
                // 执行系统命令
                if (!item.action.empty()) {
                    std::cout << "[TRAY] 执行命令: " << item.action << "\n";
                    std::system(item.action.c_str());
                }
                break;

            case TrayActionType::lua:
                // 执行 Lua 脚本 (需要 Lua 环境)
                std::cout << "[TRAY] 执行 Lua 脚本: " << item.action << "\n";
                std::cout << "  (需要 Lua 环境，暂未实现)\n";
                std::cout.flush();
                break;

            case TrayActionType::startGame:
                // 启动游戏
                if (!item.action.empty()) {
                    auto games = g_config->getGameConfigList();
                    for (const auto& game : games) {
                        if (game.name == item.action) {
                            std::cout << "[TRAY] 启动游戏: " << game.name << "\n";
                            std::cout << "  路径: " << game.path << "\n";
                            // TODO: 实际启动游戏
                            std::cout.flush();
                            break;
                        }
                    }
                }
                break;

            case TrayActionType::http:
                // 发送 HTTP 请求
                if (!item.action.empty()) {
                    std::cout << "[TRAY] 发送 HTTP 请求: " << item.action << "\n";
                    // TODO: 实现 HTTP 请求
                    std::cout.flush();
                }
                break;

            case TrayActionType::callback:
                // 内部回调 - 特殊处理
                if (item.id == "exit") {
                    std::cout << "[TRAY] 退出 Wingman\n";
                    std::cout.flush();
                    g_running = false;
                    PostQuitMessage(0);
                } else if (item.id == "config_view") {
                    std::cout << "\n=== 当前配置 ===\n";
                    std::cout << "服务器: " << serverConfig.host << ":" << serverConfig.port << "\n";
                    if (!serverConfig.username.empty()) {
                        std::cout << "用户名: " << serverConfig.username << "\n";
                    }
                    std::cout << "自动连接: " << (serverConfig.autoConnect ? "是" : "否") << "\n";
                    std::cout << "服务器控制: " << (serverConfig.serverControlled ? "是" : "否") << "\n";
                    std::cout.flush();
                } else if (item.id == "help") {
                    std::cout << "\n=== Wingman 命令行用法 ===\n";
                    std::cout << "  wingman.exe --server [port]    启动 HTTP 服务器\n";
                    std::cout << "  wingman.exe <script.lua>       运行 Lua 脚本\n";
                    std::cout << "  wingman.exe --tray             托盘模式 (默认)\n";
                    std::cout.flush();
                }
                break;

            default:
                break;
        }
    });

    // Load menu from config
    if (!trayConfig.menuItems.empty()) {
        std::cout << "[TRAY] 从配置加载 " << trayConfig.menuItems.size() << " 个菜单项\n";
        icon->loadFromConfig(trayConfig);
    } else {
        // 使用默认菜单
        std::cout << "[TRAY] 使用默认菜单\n";

        icon->addItem("help", "帮助 / 用法", []() {
            std::cout << "\n=== Wingman 命令行用法 ===\n";
            std::cout << "  wingman.exe --server [port]    启动 HTTP 服务器\n";
            std::cout << "  wingman.exe <script.lua>       运行 Lua 脚本\n";
            std::cout.flush();
        });

        icon->addSeparator("sep1");

        icon->addItem("config_view", "查看配置", []() {
            // 由 actionHandler 处理
        });

        icon->addSeparator("sep2");

        icon->addItem("exit", "退出", []() {
            // 由 actionHandler 处理
        });
    }

    // Auto-connect if configured
    if (serverConfig.autoConnect) {
        std::cout << "[CONFIG] 自动连接已启用\n";
        std::cout.flush();
    }

    // Show the icon
    icon->show();

    std::cout << "Wingman v" << wingman::version::getFullVersion() << "\n";
    std::cout << "托盘图标已启动，点击图标查看菜单\n";
    std::cout << "按 Ctrl+C 退出\n\n";

    // Windows message loop
    MSG msg;
    while (g_running && GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

// Execute Lua script
int runScript(const std::string& scriptPath) {
    lua::LuaState luaState;

    if (!luaState.getState()) {
        std::cerr << "Failed to initialize Lua environment\n";
        return 1;
    }

    std::cout << "Executing script: " << scriptPath << "\n";

    if (!luaState.doFile(scriptPath)) {
        std::cerr << "Script execution failed: " << luaState.getLastError() << "\n";
        return 1;
    }

    return 0;
}

// Command: Start HTTP server
int cmdServer(const std::vector<std::string>& args) {
    int port = 8080;
    if (!args.empty()) {
        try {
            port = std::stoi(args[0]);
        } catch (...) {
            std::cerr << "Invalid port number\n";
            return 1;
        }
    }

    // Setup signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    g_httpServer = std::make_unique<HTTPServer>("wingman.db", port);
    g_httpServer->start();

    std::cout << "\nHTTP Server running on http://localhost:" << port << "\n";
    std::cout << "Dashboard: http://localhost:" << port << "\n";
    std::cout << "Default credentials: admin/admin\n";
    std::cout << "\nPress Ctrl+C to stop...\n\n";

    // Keep running
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return 0;
}

#ifdef WINGMAN_BUILD_LUA_HTTP
// Command: Start Lua HTTP server
int cmdLuaHttpServer(const std::vector<std::string>& args) {
    // Initialize spdlog
    auto console = spdlog::stdout_color_mt("console");
    spdlog::set_default_logger(console);
    spdlog::set_level(spdlog::level::info);

    int port = 8081;
    std::string scriptPath = "scripts/http_routes_example.lua";

    if (!args.empty()) {
        try {
            port = std::stoi(args[0]);
            if (args.size() >= 2) {
                scriptPath = args[1];
            }
        } catch (...) {
            // First arg is script path, use default port
            port = 8081;
            scriptPath = args[0];
        }
    }

    // Check if script exists
    std::ifstream scriptFile(scriptPath);
    if (!scriptFile.good()) {
        std::cerr << "Script not found: " << scriptPath << "\n";
        std::cerr << "Using default script: scripts/http_routes_example.lua\n";
        scriptPath = "scripts/http_routes_example.lua";
        scriptFile.open(scriptPath);
        if (!scriptFile.good()) {
            std::cerr << "Default script not found either. Starting without routes.\n";
        }
    }

    // Setup signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // Initialize Lua state
    sol::state lua;
    lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::table);

    // Create Lua HTTP server
    g_luaHttpServer = std::make_unique<LuaHTTPServer>(port);

    // Load routes from script
    if (scriptFile.good()) {
        std::cout << "Loading routes from: " << scriptPath << "\n";
        if (!load_http_routes(lua, scriptPath, *g_luaHttpServer)) {
            std::cerr << "Failed to load routes, starting server anyway...\n";
        }
    }

    // Start server
    g_luaHttpServer->start();

    std::cout << "\nLua HTTP Server running on http://localhost:" << port << "\n";
    std::cout << "\nTest endpoints:\n";
    std::cout << "  GET  http://localhost:" << port << "/test/health\n";
    std::cout << "  POST http://localhost:" << port << "/test/echo\n";
    std::cout << "  GET  http://localhost:" << port << "/test/query?name=world\n";
    std::cout << "\nPress Ctrl+C to stop...\n\n";

    // Keep running
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return 0;
}
#endif // WINGMAN_BUILD_LUA_HTTP

// Main function
int main(int argc, char* argv[]) {
#ifdef _WIN32
    // 自动检测并设置控制台编码为 UTF-8
    UINT outputCP = GetConsoleOutputCP();
    UINT inputCP = GetConsoleCP();

    // 如果当前不是 UTF-8，则设置为 UTF-8
    if (outputCP != CP_UTF8 || inputCP != CP_UTF8) {
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);

        // 只有在非托盘模式下才显示编码设置信息
        if (argc >= 2) {
            std::cout << "控制台编码已设置为 UTF-8 (原来: output=" << outputCP
                      << ", input=" << inputCP << ")\n";
        }
    }
#endif

    // 无参数时启动托盘模式
    if (argc < 2) {
        return cmdTray();
    }

    std::string arg1 = argv[1];

    // === Command line mode ===

    if (arg1 == "--help" || arg1 == "-h") {
        printUsage();
        return 0;
    }

    if (arg1 == "--version" || arg1 == "-v") {
        std::cout << "Wingman v" << wingman::version::getFullVersion() << "\n";
        std::cout << "Build: " << wingman::version::getBuildDate()
                  << " " << wingman::version::getBuildTime() << "\n";
        std::cout << "Compiler: " << wingman::version::getCompiler() << "\n";
        return 0;
    }

    if (arg1 == "--pixel") {
        std::vector<std::string> args;
        for (int i = 2; i < argc; ++i) {
            args.push_back(argv[i]);
        }
        return cmdPixel(args);
    }

    if (arg1 == "--find") {
        std::vector<std::string> args;
        for (int i = 2; i < argc; ++i) {
            args.push_back(argv[i]);
        }
        return cmdFind(args);
    }

    if (arg1 == "--capture") {
        std::vector<std::string> args;
        for (int i = 2; i < argc; ++i) {
            args.push_back(argv[i]);
        }
        return cmdCapture(args);
    }

    if (arg1 == "--list") {
        return cmdList();
    }

    if (arg1 == "--server") {
        std::vector<std::string> args;
        for (int i = 2; i < argc; ++i) {
            args.push_back(argv[i]);
        }
        return cmdServer(args);
    }

#ifdef WINGMAN_BUILD_LUA_HTTP
    if (arg1 == "--lua-http") {
        std::vector<std::string> args;
        for (int i = 2; i < argc; ++i) {
            args.push_back(argv[i]);
        }
        return cmdLuaHttpServer(args);
    }
#endif

    // === Script mode ===

    // Check if it's a Lua script
    if (arg1.size() > 4 && arg1.substr(arg1.size() - 4) == ".lua") {
        return runScript(arg1);
    }

    // Default to script mode
    return runScript(arg1);
}
