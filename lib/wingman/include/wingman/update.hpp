#pragma once

#include <cstdint>
#include <string>

namespace wingman {

// 自动更新客户端核心（ROADMAP M8.1）。服务端实现见
// orchestrator/server/internal/handlers/update.go：admin 发布制品 +
// manifest，客户端经 latest 轮询 + download 拉取，下载后先校验 sha256
// 再落名（先校验后启用）。

// 服务端 latest 响应里的 manifest 字段。url 为相对路径
// （/api/v1/update/download/<id>），由调用方拼接 server 基址。
struct UpdateManifest {
    std::string version;
    std::string url;
    std::string sha256; // 小写十六进制
    int64_t size = 0;
    std::string notes;
    bool mandatory = false;

    // 从 latest 响应 JSON 解析；缺 manifest 对象或 version/url/sha256
    // 任一缺失/为空返回 false
    static bool parse(const std::string& body, UpdateManifest& out);
};

// 语义化版本比较，与服务端 Go compareUpdateVersions 同口径：先按 x.y.z
// 数值分量；三元组相同则正式版 > 预发布（1.2.3 > 1.2.3-beta.1，预发布后缀
// 字典序）；构建元数据（+…）不参与（semver §10）；三元组解析失败按 0.0.0。
// 返回 <0 / 0 / >0。
int compareUpdateVersions(const std::string& a, const std::string& b);

// 流式计算文件 sha256 并与 expectedHex（小写十六进制）比对；文件不存在、
// 读取失败或哈希不匹配返回 false。
bool verifyFileSha256(const std::string& path, const std::string& expectedHex);

// 查询最新 manifest：GET <baseUrl>/api/v1/update/latest，channel/platform/
// arch 非空才拼 query（空 = 用服务端默认 stable/windows/amd64；值须为
// URL 安全 token，服务端发布面已按同一约定生产）。HTTP 非 2xx 或解析
// 失败返回 false。无 CURL 构建恒返 false。
bool checkUpdateLatest(const std::string& baseUrl,
                       const std::string& channel,
                       const std::string& platform,
                       const std::string& arch,
                       UpdateManifest& out);

// 下载制品到 destPath：先写 <destPath>.part 临时文件，sha256Hex 非空时
// 校验通过才改名落定（半截/被篡改的制品不会顶掉已有文件）。
// 无 CURL 构建恒返 false。
bool downloadUpdateArtifact(const std::string& url,
                            const std::string& destPath,
                            const std::string& sha256Hex);

} // namespace wingman
