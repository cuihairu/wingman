#include "wingman/runtime/controllers/script.hpp"
#include "wingman/script_manager.hpp"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>

namespace wingman::runtime::controllers {

RpcResponse ScriptCtrl::start(const RpcRequest& req) {
    RpcResponse resp;
    resp.id = req.id;

    try {
        std::string scriptPath = req.params.value("path", "");
        if (scriptPath.empty()) {
            scriptPath = req.params.value("script", "");
        }

        if (scriptPath.empty()) {
            resp.success = false;
            resp.error = "Script path is required";
            return resp;
        }

        // TODO: 实际启动脚本
        // 目前返回模拟响应
        resp.success = true;
        resp.result["scriptId"] = "script_" + std::to_string(std::hash<std::string>{}(scriptPath));
        resp.result["status"] = "running";

        spdlog::info("[Script] Starting: {}", scriptPath);

    } catch (const std::exception& e) {
        resp.success = false;
        resp.error = e.what();
    }

    return resp;
}

RpcResponse ScriptCtrl::stop(const RpcRequest& req) {
    RpcResponse resp;
    resp.id = req.id;

    try {
        std::string scriptId = req.params.value("scriptId", "");

        if (scriptId.empty()) {
            resp.success = false;
            resp.error = "Script ID is required";
            return resp;
        }

        // TODO: 实际停止脚本
        resp.success = true;
        resp.result["status"] = "stopped";

        spdlog::info("[Script] Stopping: {}", scriptId);

    } catch (const std::exception& e) {
        resp.success = false;
        resp.error = e.what();
    }

    return resp;
}

RpcResponse ScriptCtrl::list(const RpcRequest& req) {
    RpcResponse resp;
    resp.id = req.id;

    try {
        auto scripts = scanScripts();
        nlohmann::json arr = nlohmann::json::array();

        for (const auto& script : scripts) {
            nlohmann::json j;
            j["id"] = script.id;
            j["name"] = script.name;
            j["path"] = script.path;
            j["size"] = script.size;
            j["isRunning"] = script.isRunning;
            arr.push_back(j);
        }

        resp.success = true;
        resp.result["scripts"] = arr;

    } catch (const std::exception& e) {
        resp.success = false;
        resp.error = e.what();
    }

    return resp;
}

RpcResponse ScriptCtrl::getContent(const RpcRequest& req) {
    RpcResponse resp;
    resp.id = req.id;

    try {
        std::string path = req.params.value("path", "");

        if (path.empty()) {
            resp.success = false;
            resp.error = "Path is required";
            return resp;
        }

        std::ifstream file(path);
        if (!file.is_open()) {
            resp.success = false;
            resp.error = "Failed to open file: " + path;
            return resp;
        }

        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        file.close();

        resp.success = true;
        resp.result["content"] = content;

    } catch (const std::exception& e) {
        resp.success = false;
        resp.error = e.what();
    }

    return resp;
}

RpcResponse ScriptCtrl::save(const RpcRequest& req) {
    RpcResponse resp;
    resp.id = req.id;

    try {
        std::string path = req.params.value("path", "");
        std::string content = req.params.value("content", "");

        if (path.empty()) {
            resp.success = false;
            resp.error = "Path is required";
            return resp;
        }

        std::ofstream file(path);
        if (!file.is_open()) {
            resp.success = false;
            resp.error = "Failed to open file for writing: " + path;
            return resp;
        }

        file << content;
        file.close();

        resp.success = true;

    } catch (const std::exception& e) {
        resp.success = false;
        resp.error = e.what();
    }

    return resp;
}

std::vector<ScriptInfo> ScriptCtrl::scanScripts(const std::string& dir) {
    std::vector<ScriptInfo> scripts;

    if (!std::filesystem::exists(dir)) {
        return scripts;
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".lua") {
            ScriptInfo info;
            info.id = entry.path().stem().string();
            info.name = entry.path().filename().string();
            info.path = entry.path().string();
            info.size = entry.file_size();
            info.isRunning = false;  // TODO: 检查实际运行状态
            scripts.push_back(info);
        }
    }

    return scripts;
}

} // namespace wingman::runtime::controllers
