// Winsock must be included before windows.h
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

#include "wingman/screen.hpp"
#include "wingman/input.hpp"
#include "wingman/window.hpp"
#include "wingman/process.hpp"
#include "wingman/version.hpp"
#include "wingman/http_server.hpp"
#include "wingman/tray.hpp"
#include "wingman/config.hpp"
#include "wingman/script_manager.hpp"

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
#include <dbghelp.h>
#define GetCurrentDir _getcwd
#pragma comment(lib, "dbghelp.lib")
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

#ifdef _WIN32
// Windows 崩溃处理
LONG WINAPI CrashHandler(EXCEPTION_POINTERS* exceptionInfo) {
    std::cout << "\n!!! 程序崩溃 !!!\n";
    std::cout << "异常代码: 0x" << std::hex << exceptionInfo->ExceptionRecord->ExceptionCode << std::dec << "\n";
    std::cout << "异常地址: " << exceptionInfo->ExceptionRecord->ExceptionAddress << "\n";
    std::cout << "异常标志: 0x" << std::hex << exceptionInfo->ExceptionRecord->ExceptionFlags << std::dec << "\n";

    // 打印堆栈跟踪
    HANDLE process = GetCurrentProcess();
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(process, NULL, TRUE);

    void* stack[64];
    USHORT frames = CaptureStackBackTrace(0, 64, stack, NULL);
    SYMBOL_INFO* symbol = (SYMBOL_INFO*)malloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char));
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = 255;

    std::cout << "\n堆栈跟踪:\n";
    for (USHORT i = 0; i < frames; i++) {
        SymFromAddr(process, (DWORD64)(stack[i]), 0, symbol);
        std::cout << "  " << i << ": " << symbol->Name << " (0x" << symbol->Address << ")\n";
    }
    free(symbol);

    std::cout.flush();
    return EXCEPTION_CONTINUE_SEARCH;
}

void EnableCrashHandler() {
    SetUnhandledExceptionFilter(CrashHandler);
}
#endif

// 获取本地 IP 地址
#ifdef _WIN32
static std::string getLocalIP() {
    static std::string cachedIP;
    if (!cachedIP.empty()) return cachedIP;

    // 初始化 Winsock
    static bool ws_initialized = false;
    if (!ws_initialized) {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
        ws_initialized = true;
    }

    char hostbuffer[256];
    if (gethostname(hostbuffer, sizeof(hostbuffer)) == 0) {
        struct addrinfo hints = {}, *info = nullptr, *p = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        if (getaddrinfo(hostbuffer, nullptr, &hints, &info) == 0) {
            // 优先级：192.168.x.x > 10.x.x.x > 172.16-31.x.x > 其他
            std::string bestIP;
            int bestPriority = 0;

            for (p = info; p != nullptr; p = p->ai_next) {
                char ipstr[INET_ADDRSTRLEN];
                struct sockaddr_in* ipv4 = (struct sockaddr_in*)p->ai_addr;
                if (inet_ntop(p->ai_family, &(ipv4->sin_addr), ipstr, sizeof(ipstr))) {
                    std::string ip(ipstr);

                    // 跳过本地回环和特殊地址
                    if (ip == "127.0.0.1" ||
                        ip.find("198.18.") == 0 ||
                        ip.find("169.254.") == 0) {
                        continue;
                    }

                    // 计算优先级
                    int priority = 0;
                    if (ip.find("192.168.") == 0) {
                        priority = 3;
                    } else if (ip.find("10.") == 0) {
                        priority = 2;
                    } else if (ip.find("172.") == 0) {
                        size_t secondDot = ip.find('.', 4);
                        if (secondDot != std::string::npos) {
                            int second = std::stoi(ip.substr(4, secondDot - 4));
                            if (second >= 16 && second <= 31) {
                                priority = 1;
                            }
                        }
                    }

                    if (priority > bestPriority) {
                        bestPriority = priority;
                        bestIP = ip;
                    }
                }
            }
            freeaddrinfo(info);

            if (!bestIP.empty()) {
                cachedIP = bestIP;
                return bestIP;
            }
        }
    }

    cachedIP = "127.0.0.1";
    return cachedIP;
}
#else
#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static std::string getLocalIP() {
    static std::string cachedIP;
    if (!cachedIP.empty()) return cachedIP;

    struct ifaddrs* ifaddr;
    if (getifaddrs(&ifaddr) == -1) {
        return "127.0.0.1";
    }

    std::string bestIP;
    int bestPriority = 0;

    for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr) continue;
        if (ifa->ifa_addr->sa_family == AF_INET) {
            char addr[INET_ADDRSTRLEN];
            struct sockaddr_in* addr_in = (struct sockaddr_in*)ifa->ifa_addr;
            if (inet_ntop(AF_INET, &(addr_in->sin_addr), addr, sizeof(addr))) {
                std::string ip(addr);

                // 跳过本地回环和特殊地址
                if (ip == "127.0.0.1" ||
                    ip.substr(0, 8) == "198.18." ||
                    ip.substr(0, 8) == "169.254.") {
                    continue;
                }

                // 计算优先级
                int priority = 0;
                if (ip.substr(0, 8) == "192.168.") {
                    priority = 3;
                } else if (ip.substr(0, 3) == "10.") {
                    priority = 2;
                } else if (ip.substr(0, 4) == "172.") {
                    int second = std::stoi(ip.substr(4, ip.find('.', 4) - 4));
                    if (second >= 16 && second <= 31) {
                        priority = 1;
                    }
                }

                if (priority > bestPriority) {
                    bestPriority = priority;
                    bestIP = ip;
                }
            }
        }
    }

    freeifaddrs(ifaddr);

    if (!bestIP.empty()) {
        cachedIP = bestIP;
        return bestIP;
    }

    cachedIP = "127.0.0.1";
    return cachedIP;
}
#endif

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
    std::cout << "  wingman.exe --server [port]           Start HTTP server (default: 9527)\n";
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
#ifdef _WIN32
    EnableCrashHandler();
#endif
    std::cout << "[TRAY] cmdTray 开始\n";
    std::cout.flush();

    // Setup signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::cout << "[TRAY] 信号处理器已设置\n";
    std::cout.flush();

    // Initialize ConfigManager
    g_config = std::make_unique<ConfigManager>("config");
    auto serverConfig = g_config->getServerConfig();
    auto trayConfig = g_config->getTrayConfig();

    // 服务器运行状态
    bool serverRunning = false;

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
    icon->setActionHandler([&serverConfig, &serverRunning, icon](const TrayMenuItemConfig& item) {
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
                } else if (item.id == "server_toggle") {
                    // 切换服务器状态
                    if (serverRunning) {
                        // 停止服务器
                        std::cout << "[TRAY] 停止服务器\n";
                        std::cout.flush();
                        if (g_httpServer) {
                            g_httpServer->stop();
                            g_httpServer.reset();
                        }
                        serverRunning = false;
                        std::cout << "[TRAY] 服务器已停止\n";
                        std::cout.flush();

                        // 更新菜单状态
                        icon->setItemLabel("server_toggle", "☐ 启动服务器");
                        icon->setItemChecked("server_toggle", false);
                        icon->setItemLabel("server_info", "  服务器: 未连接");
                    } else {
                        // 启动服务器
                        std::cout << "[TRAY] 启动服务器\n";
                        std::cout.flush();
                        try {
                            std::cout << "[TRAY] 创建 HTTPServer...\n";
                            std::cout.flush();
                            g_httpServer = std::make_unique<HTTPServer>("wingman.db", serverConfig.port);
                            std::cout << "[TRAY] 调用 start()...\n";
                            std::cout.flush();
                            g_httpServer->start();
                            std::cout << "[TRAY] start() 返回\n";
                            std::cout.flush();
                            serverRunning = true;
                            std::cout << "[TRAY] 服务器已启动" << std::endl;
                            std::cout << "[TRAY] 服务器运行中，端口: " << serverConfig.port << std::endl;
                            std::cout << "[TRAY] 访问: http://" << getLocalIP() << ":" << serverConfig.port << std::endl;
                            std::cout.flush();

                            // 更新菜单状态
                            icon->setItemLabel("server_toggle", "☑ 停止服务器");
                            icon->setItemChecked("server_toggle", true);
                            icon->setItemLabel("server_info", "  服务器: " + getLocalIP() + ":" + std::to_string(serverConfig.port));
                        } catch (const std::exception& e) {
                            std::cout << "[TRAY] 启动服务器失败: " << e.what() << "\n";
                            std::cout.flush();
                        } catch (...) {
                            std::cout << "[TRAY] 启动服务器时发生未知异常\n";
                            std::cout.flush();
                        }
                    }
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
        std::cout << "[TRAY] 开始调用 loadFromConfig...\n";
        std::cout.flush();
        icon->loadFromConfig(trayConfig);
        std::cout << "[TRAY] loadFromConfig 完成\n";
        std::cout.flush();
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

        icon->addItem("config_view", "查看配置", [&serverConfig]() {
            std::cout << "\n=== 当前配置 ===\n";
            std::cout << "服务器: " << serverConfig.host << ":" << serverConfig.port << "\n";
            if (!serverConfig.username.empty()) {
                std::cout << "用户名: " << serverConfig.username << "\n";
            }
            std::cout << "自动连接: " << (serverConfig.autoConnect ? "是" : "否") << "\n";
            std::cout << "服务器控制: " << (serverConfig.serverControlled ? "是" : "否") << "\n";
            std::cout.flush();
        });

        icon->addSeparator("sep2");

        icon->addItem("exit", "退出", []() {
            std::cout << "[TRAY] 退出 Wingman\n";
            std::cout.flush();
            g_running = false;
            PostQuitMessage(0);
        });
    }

    // Auto-connect if configured
    if (serverConfig.autoConnect) {
        std::cout << "[CONFIG] 自动连接已启用\n";
        std::cout.flush();
    }

    std::cout << "[TRAY] 准备更新本地 IP...\n";
    std::cout.flush();

    // 更新本地 IP 显示
    try {
        std::string localIP = getLocalIP();
        std::cout << "[TRAY] 本地 IP: " << localIP << "\n";
        std::cout.flush();
        icon->setItemLabel("local_ip", "  本地 IP: " + localIP);
        std::cout << "[TRAY] 本地 IP 标签已更新\n";
        std::cout.flush();
    } catch (const std::exception& e) {
        std::cout << "[TRAY] 获取本地 IP 失败: " << e.what() << "\n";
        std::cout.flush();
        try {
            icon->setItemLabel("local_ip", "  本地 IP: 未知");
        } catch (...) {
            std::cout << "[TRAY] 设置本地 IP 标签失败\n";
            std::cout.flush();
        }
    } catch (...) {
        std::cout << "[TRAY] 获取本地 IP 时发生未知错误\n";
        std::cout.flush();
    }

    std::cout << "[TRAY] 准备显示托盘图标...\n";
    std::cout.flush();

    // Show the icon
    icon->show();

    std::cout << "[TRAY] 托盘图标已显示\n";
    std::cout.flush();

    std::cout << "Wingman v" << wingman::version::getFullVersion() << "\n";
    std::cout << "托盘图标已启动，点击图标查看菜单\n";
    std::cout << "按 Ctrl+C 退出\n\n";

    // Windows message loop
    MSG msg;
    bool firstLoop = true;
    while (g_running) {
        BOOL result = GetMessage(&msg, nullptr, 0, 0);

        if (firstLoop) {
            std::cout << "[DEBUG] 首次进入消息循环，GetMessage 返回: " << result << "\n";
            std::cout.flush();
            firstLoop = false;
        }

        if (result == 0) {
            // WM_QUIT
            std::cout << "[DEBUG] 收到 WM_QUIT\n";
            std::cout.flush();
            break;
        } else if (result == -1) {
            // Error
            std::cout << "[DEBUG] GetMessage 出错: " << GetLastError() << "\n";
            std::cout.flush();
            break;
        }

        std::cout << "[DEBUG] Message: 0x" << std::hex << msg.message << std::dec << "\n";
        std::cout.flush();
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    std::cout << "[TRAY] 消息循环退出, g_running=" << g_running << "\n";
    std::cout.flush();

    return 0;
}

// Execute Lua script
int runScript(const std::string& scriptPath) {
    // 创建 ScriptManager 实例
    auto scriptManager = std::make_unique<ScriptManager>();

    // 创建 LuaState
    lua::LuaState luaState;

    if (!luaState.getState()) {
        std::cerr << "Failed to initialize Lua environment\n";
        return 1;
    }

    // 设置 ScriptManager 到 Lua 模块
    lua::LuaState::setScriptManager(scriptManager.get());

    std::cout << "Executing script: " << scriptPath << "\n";

    if (!luaState.doFile(scriptPath)) {
        std::cerr << "Script execution failed: " << luaState.getLastError() << "\n";
        return 1;
    }

    return 0;
}

// Command: Start HTTP server
int cmdServer(const std::vector<std::string>& args) {
    int port = 9527;
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
