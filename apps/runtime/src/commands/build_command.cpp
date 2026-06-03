#include "wingman/runtime/commands/build_command.hpp"
#include "wingman/runtime/packer.hpp"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>
#include <optional>
#include <vector>

namespace wingman::runtime::commands {

namespace {

std::vector<std::filesystem::path> candidateStubPaths() {
#ifdef _WIN32
    constexpr const char* stubName = "wingman-runtime.exe";
    return {
        std::filesystem::path(stubName),
        std::filesystem::path("build/apps/runtime/Release") / stubName,
        std::filesystem::path("../build/apps/runtime/Release") / stubName,
        std::filesystem::path("build/apps/runtime/Debug") / stubName,
        std::filesystem::path("../build/apps/runtime/Debug") / stubName,
    };
#else
    constexpr const char* stubName = "wingman-runtime";
    return {
        std::filesystem::path(stubName),
        std::filesystem::path("build/apps/runtime") / stubName,
        std::filesystem::path("../build/apps/runtime") / stubName,
    };
#endif
}

std::optional<std::filesystem::path> resolveStubPath() {
    for (const auto& candidate : candidateStubPaths()) {
        if (std::filesystem::exists(candidate)) {
            return std::filesystem::absolute(candidate);
        }
    }
    return std::nullopt;
}

} // namespace

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

    const auto stubPath = resolveStubPath();
    if (!stubPath) {
        spdlog::error("Stub executable not found. Expected one of the configured wingman-runtime build outputs.");
        return 1;
    }

    // 准备打包选项
    PackerOptions packerOptions;
    packerOptions.scriptPath = options.scriptPath;
    packerOptions.outputPath = options.outputPath;
    packerOptions.iconPath = options.iconPath;
    packerOptions.stubPath = stubPath->string();
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
