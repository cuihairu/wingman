#include "build_command.hpp"
#include "wingman/runtime/packer.hpp"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>

namespace wingman::runtime::commands {

int buildCommand(const BuildOptions& options) {
    spdlog::info("=== Wingman Build ===");
    spdlog::info("Script: {}", options.scriptPath);
    spdlog::info("Output: {}", options.outputPath);
    if (!options.iconPath.empty()) {
        spdlog::info("Icon: {}", options.iconPath);
    }

    // 检查输入文件
    if (!std::filesystem::exists(options.scriptPath)) {
        spdlog::error("Script file not found: {}", options.scriptPath);
        return 1;
    }

    // 检查 stub 程序（当前运行的 wingman-runtime.exe）
    std::string stubPath = "wingman-runtime.exe";
    if (!std::filesystem::exists(stubPath)) {
        // 尝试从构建目录获取
        stubPath = "../build/apps/runtime/Release/wingman-runtime.exe";
        if (!std::filesystem::exists(stubPath)) {
            spdlog::error("Stub executable not found. Please ensure wingman-runtime.exe is in current directory.");
            return 1;
        }
    }

    // 准备打包选项
    PackerOptions packerOptions;
    packerOptions.scriptPath = options.scriptPath;
    packerOptions.outputPath = options.outputPath;
    packerOptions.iconPath = options.iconPath;
    packerOptions.stubPath = stubPath;
    packerOptions.encrypt = options.encrypt;
    packerOptions.compress = options.compress;

    // 确保输出目录存在
    std::filesystem::path outputPath(packerOptions.outputPath);
    if (outputPath.has_parent_path()) {
        std::filesystem::create_directories(outputPath.parent_path());
    }

    // 执行打包
    try {
        Packer packer(packerOptions);
        PackerResult result = packer.build();

        if (result.success) {
            spdlog::info("Build successful!");
            spdlog::info("Output: {}", result.outputPath);
            return 0;
        } else {
            spdlog::error("Build failed: {}", result.message);
            return 1;
        }

    } catch (const std::exception& e) {
        spdlog::error("Build exception: {}", e.what());
        return 1;
    }
}

} // namespace wingman::runtime::commands
