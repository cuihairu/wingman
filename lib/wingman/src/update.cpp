#include "wingman/update.hpp"

#include "wingman/http.hpp"

#include <nlohmann/json.hpp>
#include <openssl/evp.h>

#include <filesystem>
#include <fstream>
#include <vector>

namespace wingman {

namespace {

// 与 Go 服务端 splitUpdateVersion 同口径：+ 元数据先行剥离，取首个 '-'
// 之后的预发布后缀；三元组必须恰好 3 段非负整数，否则整体按 0.0.0。
struct SplitVersion {
    int nums[3] = {0, 0, 0};
    std::string prerelease;
};

SplitVersion splitUpdateVersion(const std::string& version) {
    SplitVersion out;
    std::string main = version;
    if (const auto plus = main.find('+'); plus != std::string::npos) {
        main.resize(plus); // 构建元数据不参与比较（semver §10）
    }
    if (const auto dash = main.find('-'); dash != std::string::npos) {
        out.prerelease = main.substr(dash + 1);
        main.resize(dash);
    }

    const auto d1 = main.find('.');
    const auto d2 = d1 == std::string::npos ? std::string::npos : main.find('.', d1 + 1);
    if (d1 == std::string::npos || d2 == std::string::npos) {
        return SplitVersion{};
    }
    const std::string parts[3] = {
        main.substr(0, d1),
        main.substr(d1 + 1, d2 - d1 - 1),
        main.substr(d2 + 1),
    };
    for (int i = 0; i < 3; ++i) {
        const std::string& part = parts[i];
        if (part.empty() || part.find_first_not_of("0123456789") != std::string::npos) {
            return SplitVersion{};
        }
        try {
            out.nums[i] = std::stoi(part); // 溢出等同 Go Atoi 失败 → 0.0.0
        } catch (...) {
            return SplitVersion{};
        }
    }
    return out;
}

// 流式 sha256（1MiB 分块喂 EVP_DigestUpdate），hex 小写；打不开/读失败返回空
bool sha256OfFile(const std::string& path, std::string& outHex) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        return false;
    }
    bool ok = EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) == 1;

    std::vector<char> buf(1 << 20);
    while (ok) {
        in.read(buf.data(), static_cast<std::streamsize>(buf.size()));
        const std::streamsize got = in.gcount();
        if (got > 0) {
            ok = EVP_DigestUpdate(ctx, buf.data(), static_cast<size_t>(got)) == 1;
        }
        if (got < static_cast<std::streamsize>(buf.size())) {
            break; // 短读 = EOF 或 IO 错误
        }
    }
    if (!in.eof()) {
        ok = false; // 非正常终止（badbit）
    }

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digestLen = 0;
    if (ok && EVP_DigestFinal_ex(ctx, digest, &digestLen) != 1) {
        ok = false;
    }
    EVP_MD_CTX_free(ctx);
    if (!ok) {
        return false;
    }

    static constexpr char kHexDigits[] = "0123456789abcdef";
    outHex.clear();
    outHex.reserve(digestLen * 2);
    for (unsigned int i = 0; i < digestLen; ++i) {
        outHex.push_back(kHexDigits[digest[i] >> 4]);
        outHex.push_back(kHexDigits[digest[i] & 0xF]);
    }
    return true;
}

// 统一传输成功判定：HTTP 2xx；file:// 等非 HTTP 方案 curl 返回状态码 0
// 且无 error——这一支让更新链路可以走 file:// 做网络无关的端到端测试。
bool transportOk(const HttpResponse& resp) {
    if (resp.isSuccess()) {
        return true;
    }
    return resp.statusCode == 0 && resp.error.empty();
}

} // namespace

bool UpdateManifest::parse(const std::string& body, UpdateManifest& out) {
    try {
        const auto root = nlohmann::json::parse(body);
        if (!root.is_object() || !root.contains("manifest") || !root["manifest"].is_object()) {
            return false;
        }
        const auto& m = root["manifest"];
        if (!m.contains("version") || !m["version"].is_string()) {
            return false;
        }
        if (!m.contains("url") || !m["url"].is_string()) {
            return false;
        }
        if (!m.contains("sha256") || !m["sha256"].is_string()) {
            return false;
        }
        out.version = m["version"].get<std::string>();
        out.url = m["url"].get<std::string>();
        out.sha256 = m["sha256"].get<std::string>();
        if (out.version.empty() || out.url.empty() || out.sha256.empty()) {
            return false;
        }
        if (m.contains("size") && m["size"].is_number_integer()) {
            out.size = m["size"].get<int64_t>();
        } else {
            out.size = 0;
        }
        if (m.contains("notes") && m["notes"].is_string()) {
            out.notes = m["notes"].get<std::string>();
        } else {
            out.notes.clear();
        }
        if (m.contains("mandatory") && m["mandatory"].is_boolean()) {
            out.mandatory = m["mandatory"].get<bool>();
        } else {
            out.mandatory = false;
        }
        return true;
    } catch (...) {
        return false;
    }
}

int compareUpdateVersions(const std::string& a, const std::string& b) {
    const SplitVersion sa = splitUpdateVersion(a);
    const SplitVersion sb = splitUpdateVersion(b);
    for (int i = 0; i < 3; ++i) {
        if (sa.nums[i] != sb.nums[i]) {
            return sa.nums[i] < sb.nums[i] ? -1 : 1;
        }
    }
    if (sa.prerelease == sb.prerelease) {
        return 0;
    }
    if (sa.prerelease.empty()) {
        return 1; // 正式版高于预发布
    }
    if (sb.prerelease.empty()) {
        return -1;
    }
    return sa.prerelease < sb.prerelease ? -1 : 1;
}

bool verifyFileSha256(const std::string& path, const std::string& expectedHex) {
    std::string actual;
    if (!sha256OfFile(path, actual)) {
        return false;
    }
    return actual == expectedHex;
}

bool checkUpdateLatest(const std::string& baseUrl,
                       const std::string& channel,
                       const std::string& platform,
                       const std::string& arch,
                       UpdateManifest& out) {
#ifdef WINGMAN_HAS_CURL
    std::string url = baseUrl;
    while (!url.empty() && url.back() == '/') {
        url.pop_back();
    }
    url += "/api/v1/update/latest";

    std::string query;
    auto appendParam = [&query](const char* key, const std::string& value) {
        if (value.empty()) {
            return;
        }
        query += query.empty() ? "?" : "&";
        query += key;
        query += '=';
        query += value;
    };
    appendParam("channel", channel);
    appendParam("platform", platform);
    appendParam("arch", arch);

    HttpClient http;
    const HttpResponse resp = http.get(url + query);
    if (!transportOk(resp)) {
        return false;
    }
    return UpdateManifest::parse(resp.body, out);
#else
    (void)baseUrl;
    (void)channel;
    (void)platform;
    (void)arch;
    (void)out;
    return false;
#endif
}

bool downloadUpdateArtifact(const std::string& url,
                            const std::string& destPath,
                            const std::string& sha256Hex) {
#ifdef WINGMAN_HAS_CURL
    if (destPath.empty()) {
        return false;
    }
    HttpClient http;
    const HttpResponse resp = http.get(url);
    if (!transportOk(resp) || resp.body.empty()) {
        return false;
    }

    namespace fs = std::filesystem;
    std::error_code ec;
    if (const fs::path parent = fs::path(destPath).parent_path(); !parent.empty()) {
        fs::create_directories(parent, ec);
        if (ec) {
            return false;
        }
    }

    // 先写临时文件，校验通过再改名：半截/被篡改的制品不会顶掉已有文件
    fs::path tmpPath = destPath;
    tmpPath += ".part";
    {
        std::ofstream out(tmpPath, std::ios::binary | std::ios::trunc);
        if (!out) {
            return false;
        }
        out.write(resp.body.data(), static_cast<std::streamsize>(resp.body.size()));
        out.close();
        if (!out) {
            return false;
        }
    }
    if (!sha256Hex.empty() && !verifyFileSha256(tmpPath.string(), sha256Hex)) {
        std::error_code rmEc;
        fs::remove(tmpPath, rmEc);
        return false;
    }
    fs::rename(tmpPath, destPath, ec);
    if (ec) {
        std::error_code rmEc;
        fs::remove(tmpPath, rmEc);
        return false;
    }
    return true;
#else
    (void)url;
    (void)destPath;
    (void)sha256Hex;
    return false;
#endif
}

} // namespace wingman
