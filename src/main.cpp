#include "wingman/screen.hpp"
#include "wingman/input.hpp"
#include "wingman/window.hpp"
#include "wingman/process.hpp"
#include "wingman/version.hpp"
#include "wingman/http_server.hpp"

#include "lua_bindings.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <memory>
#include <csignal>
#include <thread>
#include <chrono>

using namespace wingman;

// Global HTTP server pointer for signal handling
static std::unique_ptr<HTTPServer> g_httpServer;
static bool g_running = true;

// Signal handler for graceful shutdown
void signalHandler(int signal) {
    std::cout << "\nShutting down...\n";
    g_running = false;
    if (g_httpServer) {
        g_httpServer->stop();
    }
    exit(0);
}

// Print usage information
void printUsage() {
    std::cout << "Wingman v" << wingman::version::getFullVersion()
              << " - Game Automation Programmable Control Engine\n\n";
    std::cout << "Usage:\n";
    std::cout << "  wingman.exe <script.lua>              Run Lua script\n";
    std::cout << "  wingman.exe --server [port]           Start HTTP server (default: 8080)\n";
    std::cout << "  wingman.exe --pixel <x> <y>          Get pixel color at position\n";
    std::cout << "  wingman.exe --find <color> <region>  Find color on screen\n";
    std::cout << "  wingman.exe --capture [file.png]     Capture screen and save\n";
    std::cout << "  wingman.exe --list                   List all windows\n";
    std::cout << "  wingman.exe --version               Show version information\n";
    std::cout << "\nExamples:\n";
    std::cout << "  wingman.exe script.lua\n";
    std::cout << "  wingman.exe --server\n";
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

// Main function
int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage();
        return 0;
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

    // === Script mode ===

    // Check if it's a Lua script
    if (arg1.size() > 4 && arg1.substr(arg1.size() - 4) == ".lua") {
        return runScript(arg1);
    }

    // Default to script mode
    return runScript(arg1);
}
