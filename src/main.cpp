#include "wingman/screen.hpp"
#include "wingman/input.hpp"
#include "wingman/window.hpp"
#include "wingman/process.hpp"

#include "bindings/lua_bindings.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <fstream>

using namespace wingman;

// 打印使用说明
void printUsage() {
    std::cout << "Wingman v0.1.0 - 游戏自动化可编程控制引擎\n\n";
    std::cout << "用法:\n";
    std::cout << "  wingman.exe <script.lua>              运行 Lua 脚本\n";
    std::cout << "  wingman.exe --pixel <x> <y>          获取指定位置像素颜色\n";
    std::cout << "  wingman.exe --find <color> <region>  查找颜色\n";
    std::cout << "  wingman.exe --capture [file.png]     截取屏幕并保存\n";
    std::cout << "  wingman.exe --list                   列出所有窗口\n";
    std::cout << "  wingman.exe --version               显示版本信息\n";
    std::cout << "\n示例:\n";
    std::cout << "  wingman.exe script.lua\n";
    std::cout << "  wingman.exe --pixel 100 200\n";
    std::cout << "  wingman.exe --find 0xFF0000 0,0,1920,1080\n";
    std::cout << "  wingman.exe --capture screenshot.png\n";
}

// 解析颜色 (格式: 0xRRGGBB 或 #RRGGBB)
Color parseColor(const std::string& colorStr) {
    std::string str = colorStr;
    if (str.empty()) {
        return Color();
    }

    // 移除 0x 前缀
    if (str.substr(0, 2) == "0x") {
        str = str.substr(2);
    }
    // 移除 # 前缀
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

// 解析区域 (格式: x,y,width,height 或 x1,y1,x2,y2)
Rect parseRegion(const std::string& regionStr) {
    // 简单实现，默认全屏
    int width = Screen::getScreenWidth();
    int height = Screen::getScreenHeight();
    return Rect(0, 0, width, height);
}

// 命令: 获取像素颜色
int cmdPixel(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cerr << "用法: --pixel <x> <y>\n";
        return 1;
    }

    int x = std::stoi(args[0]);
    int y = std::stoi(args[1]);

    Color color = Screen::getPixel(x, y);

    std::cout << "位置 (" << x << ", " << y << ") 的颜色:\n";
    std::cout << "  RGB: (" << static_cast<int>(color.r) << ", "
              << static_cast<int>(color.g) << ", "
              << static_cast<int>(color.b) << ")\n";
    std::cout << "  HEX: 0x" << std::hex << color.toRGB() << std::dec << "\n";

    return 0;
}

// 命令: 查找颜色
int cmdFind(const std::vector<std::string>& args) {
    if (args.size() < 1) {
        std::cerr << "用法: --find <color> [region]\n";
        return 1;
    }

    Color color = parseColor(args[0]);
    Rect region = Screen::getScreenBounds();

    if (args.size() >= 2) {
        region = parseRegion(args[1]);
    }

    Point result;
    if (Screen::findColor(color, region, 10, result)) {
        std::cout << "找到颜色在: (" << result.x << ", " << result.y << ")\n";
        return 0;
    } else {
        std::cout << "未找到指定颜色\n";
        return 1;
    }
}

// 命令: 截图
int cmdCapture(const std::vector<std::string>& args) {
    std::string filename = "screenshot.png";

    if (!args.empty()) {
        filename = args[0];
    }

    auto bitmap = Screen::capture();
    if (!bitmap) {
        std::cerr << "截图失败\n";
        return 1;
    }

    if (bitmap->save(filename)) {
        std::cout << "截图已保存到: " << filename << "\n";
        return 0;
    } else {
        std::cerr << "保存截图失败\n";
        return 1;
    }
}

// 命令: 列出窗口
int cmdList() {
    auto windows = Window::enumerate();

    std::cout << "窗口列表:\n";
    for (const auto& win : windows) {
        std::cout << "  [" << win.handle << "] "
                  << (win.isForeground ? "* " : "  ")
                  << win.title << "\n";
    }

    std::cout << "\n共 " << windows.size() << " 个窗口\n";
    return 0;
}

// 执行 Lua 脚本
int runScript(const std::string& scriptPath) {
    lua::LuaState luaState;

    if (!luaState.getState()) {
        std::cerr << "无法初始化 Lua 环境\n";
        return 1;
    }

    std::cout << "正在执行脚本: " << scriptPath << "\n";

    if (!luaState.doFile(scriptPath)) {
        std::cerr << "脚本执行失败: " << luaState.getLastError() << "\n";
        return 1;
    }

    return 0;
}

// 主函数
int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage();
        return 0;
    }

    std::string arg1 = argv[1];

    // === 命令行模式 ===

    if (arg1 == "--help" || arg1 == "-h") {
        printUsage();
        return 0;
    }

    if (arg1 == "--version" || arg1 == "-v") {
        std::cout << "Wingman v0.1.0\n";
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

    // === 脚本模式 ===

    // 检查是否为 Lua 脚本
    if (arg1.size() > 4 && arg1.substr(arg1.size() - 4) == ".lua") {
        return runScript(arg1);
    }

    // 默认作为脚本处理
    return runScript(arg1);
}
