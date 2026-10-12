#include "wingman/rpc/asset_handler.hpp"

#include "wingman/crypt.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace wingman::rpc {

namespace {

namespace fs = std::filesystem;

// 与 server 侧 assets.SanitizeName 一致：字母数字开头，[A-Za-z0-9._-]，
// 最长 128，拒绝路径分隔符与 ".."
bool validAssetName(const std::string& name) {
    if (name.empty() || name.size() > 128) {
        return false;
    }
    if (!std::isalnum(static_cast<unsigned char>(name[0]))) {
        return false;
    }
    if (name.find("..") != std::string::npos) {
        return false;
    }
    return std::all_of(name.begin(), name.end(), [](char c) {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '.' ||
               c == '_' || c == '-';
    });
}

std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

// 原子落盘：临时文件 + rename（目标已存在时各平台 rename 语义均为替换）
void writeFileAtomic(const fs::path& target, const std::vector<uint8_t>& bytes) {
    fs::create_directories(target.parent_path());
    fs::path tmp = target;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) {
            throw std::runtime_error("cannot open temp file: " + tmp.string());
        }
        if (!bytes.empty()) {
            out.write(reinterpret_cast<const char*>(bytes.data()),
                      static_cast<std::streamsize>(bytes.size()));
            if (!out) {
                throw std::runtime_error("short write: " + tmp.string());
            }
        }
    }
    fs::rename(tmp, target);
}

} // namespace

void registerAssetHandlers(RpcDispatcher& dispatcher, const std::string& assetsDir) {
    dispatcher.registerHandler("asset.sync", [assetsDir](const json& params) -> json {
        const std::string name = params.value("name", "");
        if (!validAssetName(name)) {
            return {{"success", false}, {"error", "invalid asset name"}};
        }
        const std::string encoded = params.value("contentBase64", "");
        if (encoded.empty()) {
            return {{"success", false}, {"error", "missing contentBase64"}};
        }
        // 上限按解码前粗筛：base64 膨胀系数 4/3，解码后必然更小
        if (encoded.size() > (kMaxAssetSize / 3) * 4 + 4) {
            return {{"success", false}, {"error", "asset too large"}};
        }

        const std::vector<uint8_t> bytes = crypt::base64Decode(encoded);
        if (bytes.size() > kMaxAssetSize) {
            return {{"success", false}, {"error", "asset too large"}};
        }
        // sha256 提供时强校验（server 端 hex.EncodeToString 输出小写）
        const std::string expect = toLower(params.value("sha256", ""));
        if (!expect.empty()) {
            const std::string actual = toLower(crypt::sha256(
                std::string(bytes.begin(), bytes.end())));
            if (actual != expect) {
                return {{"success", false}, {"error", "sha256 mismatch"}};
            }
        }

        const fs::path target = fs::path(assetsDir) / name;
        try {
            writeFileAtomic(target, bytes);
        } catch (const std::exception& e) {
            return {{"success", false}, {"error", std::string("write failed: ") + e.what()}};
        }

        return {
            {"success", true},
            {"result", {
                {"path", target.string()},
                {"size", bytes.size()},
            }},
        };
    });
}

} // namespace wingman::rpc
