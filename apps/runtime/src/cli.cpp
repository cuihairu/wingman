#include "wingman/runtime/cli.hpp"
#include "wingman/runtime/commands/start_command.hpp"
#include "wingman/runtime/commands/stop_command.hpp"
#include "wingman/runtime/commands/script_command.hpp"
#include "wingman/runtime/commands/build_command.hpp"
#include "wingman/runtime/commands/crash_test_command.hpp"
#include "wingman/version.hpp"

#include <cstdlib>  // std::getenv（WINGMAN_PACK_PASSWORD）
#include <iostream>
#include <string_view>

namespace wingman::runtime {

namespace {

using Args = std::vector<std::string>;

void printUsage() {
    std::cout
        << "wingman-runtime <command> [options]\n"
        << "Commands:\n"
        << "  start   [--config|-c <path>] [--standalone]\n"
        << "  stop\n"
        << "  status\n"
        << "  script  <script-path> [args...]\n"
        << "  crash-test\n"
        << "  version\n"
        << "  build   --script|-s <path> --output|-o <path> [--icon|-i <path>]\n"
        << "          [--encrypt [--password <p>] | --no-encrypt] [--no-compress]\n"
        << "          --password implies --encrypt; without a password --encrypt is refused.\n"
        << "          Password may come from WINGMAN_PACK_PASSWORD instead of the command line.\n"
        << "          Encrypted packs read the password from WINGMAN_SCRIPT_PASSWORD at startup.\n";
}

bool requireValue(const Args& args, size_t& index, const char* flag, std::string& out) {
    if (index + 1 >= args.size()) {
        std::cerr << "Error: " << flag << " requires a value\n";
        return false;
    }
    out = args[++index];
    return true;
}

int runStart(const Args& args) {
    commands::StartOptions options;
    for (size_t i = 0; i < args.size(); ++i) {
        const auto& arg = args[i];
        if (arg == "--config" || arg == "-c") {
            if (!requireValue(args, i, arg.c_str(), options.configPath)) {
                return 1;
            }
        } else if (arg == "--standalone") {
            options.forceStandalone = true;
        } else {
            std::cerr << "Error: unknown option for start: " << arg << "\n";
            return 1;
        }
    }
    return commands::startCommand(options);
}

int runScript(const Args& args) {
    if (args.empty()) {
        std::cerr << "Error: script path is required\n";
        return 1;
    }

    const std::string& scriptPath = args[0];
    Args scriptArgs;
    if (args.size() > 1) {
        scriptArgs.assign(args.begin() + 1, args.end());
    }

    return commands::scriptCommand(scriptPath, scriptArgs);
}

int runBuild(const Args& args) {
    commands::BuildOptions options;

    // 口令的默认来源是环境变量：命令行值会留在 shell 历史与进程列表里。
    // 显式的 --password 在后面覆盖它。
    if (const char* envPassword = std::getenv("WINGMAN_PACK_PASSWORD")) {
        options.password = envPassword;
        options.encrypt = true;  // 只给口令不给 --encrypt 视为要加密（见 --help）
    }

    for (size_t i = 0; i < args.size(); ++i) {
        const auto& arg = args[i];
        if (arg == "--script" || arg == "-s") {
            if (!requireValue(args, i, arg.c_str(), options.scriptPath)) {
                return 1;
            }
        } else if (arg == "--output" || arg == "-o") {
            if (!requireValue(args, i, arg.c_str(), options.outputPath)) {
                return 1;
            }
        } else if (arg == "--icon" || arg == "-i") {
            if (!requireValue(args, i, arg.c_str(), options.iconPath)) {
                return 1;
            }
        } else if (arg == "--encrypt") {
            options.encrypt = true;
        } else if (arg == "--password") {
            if (!requireValue(args, i, arg.c_str(), options.password)) {
                return 1;
            }
            // 给了口令却没开加密 = 「以为加密了其实没加密」，比报错更危险，
            // 故 --password 同时打开加密（--help 已注明该语义）。
            options.encrypt = true;
        } else if (arg == "--no-encrypt") {
            // 兼容旧命令行；在 --password 之后出现即取消加密（口令一并清掉）。
            options.encrypt = false;
            options.password.clear();
        } else if (arg == "--no-compress") {
            options.compress = false;
        } else {
            std::cerr << "Error: unknown option for build: " << arg << "\n";
            return 1;
        }
    }

    if (options.scriptPath.empty() || options.outputPath.empty()) {
        std::cerr << "Error: --script and --output are required\n";
        return 1;
    }

    return commands::buildCommand(options);
}

} // namespace

int dispatchCommand(const std::vector<std::string>& args) {
    if (args.empty()) {
        printUsage();
        return 1;
    }

    const std::string_view command = args[0];
    const Args tail(args.begin() + 1, args.end());

    if (command == "start") {
        return runStart(tail);
    }
    if (command == "stop") {
        return tail.empty() ? commands::stopCommand() : (std::cerr << "Error: stop does not accept arguments\n", 1);
    }
    if (command == "status") {
        return tail.empty() ? commands::statusCommand() : (std::cerr << "Error: status does not accept arguments\n", 1);
    }
    if (command == "script") {
        return runScript(tail);
    }
    // 崩溃采集验收入口：触发故意空指针崩溃（进程异常退出为预期行为）
    if (command == "crash-test") {
        return tail.empty() ? commands::crashTestCommand()
                            : (std::cerr << "Error: crash-test does not accept arguments\n", 1);
    }
    // 一键安装脚本的版本验证入口：wingman-agent --version / version / -V
    if (command == "--version" || command == "-V" || command == "version") {
        std::cout << WINGMAN_VERSION_STRING << "\n";
        return 0;
    }
    if (command == "build") {
        return runBuild(tail);
    }
    if (command == "--help" || command == "-h" || command == "help") {
        printUsage();
        return 0;
    }

    std::cerr << "Error: unknown command: " << command << "\n";
    printUsage();
    return 1;
}

} // namespace wingman::runtime
