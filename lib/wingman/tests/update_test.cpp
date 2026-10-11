// 自动更新客户端核心测试（ROADMAP M8.1）：版本比较与 Go 服务端同口径、
// manifest 解析、文件 sha256 校验；file:// 端到端（拉 manifest + 下载制品）
// 只在 WINGMAN_HAS_CURL 构建编译（与 wingman 库同条件，见 tests CMake）。
#include <gtest/gtest.h>

#include "wingman/crypt.hpp"
#include "wingman/update.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <string>

namespace fs = std::filesystem;

namespace {

// 字段形状对齐服务端 HandleLatest 响应体
std::string makeLatestBody(const std::string& version, const std::string& sha) {
    return std::string(R"({"success":true,"manifest":{"version":")" + version +
                       R"(","url":"/api/v1/update/download/1","sha256":")" + sha +
                       R"(","size":7,"notes":"n","mandatory":true,)" +
                       R"("publishedAt":"2026-01-01T00:00:00Z"}})");
}

void writeFile(const fs::path& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary);
    ASSERT_TRUE(out.is_open());
    out << content;
}

// file:// 三斜杠形式（Windows 盘符路径 file:///C:/... 与 POSIX file:///... 通吃）
std::string fileUrlOf(const fs::path& path) {
    const std::string s = path.string();
    return (s.front() == '/') ? "file://" + s : "file:///" + s;
}

struct TempDir {
    fs::path path;

    TempDir() : path(fs::temp_directory_path() /
                     ("wingman-update-test-" + std::to_string(std::random_device{}()))) {
        fs::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
};

} // namespace

// ========== compareUpdateVersions ==========

TEST(UpdateVersionCompare, NumericOrdering) {
    EXPECT_GT(wingman::compareUpdateVersions("1.10.0", "1.9.0"), 0);
    EXPECT_LT(wingman::compareUpdateVersions("1.9.0", "1.10.0"), 0);
    EXPECT_EQ(wingman::compareUpdateVersions("1.10.0", "1.10.0"), 0);
    EXPECT_GT(wingman::compareUpdateVersions("2.0.0", "1.99.99"), 0);
    EXPECT_GT(wingman::compareUpdateVersions("0.2.0", "0.1.9"), 0);
}

TEST(UpdateVersionCompare, PrereleaseRules) {
    // 同三元组：正式版 > 预发布；预发布后缀字典序
    EXPECT_GT(wingman::compareUpdateVersions("2.0.0", "2.0.0-beta.1"), 0);
    EXPECT_LT(wingman::compareUpdateVersions("2.0.0-beta.1", "2.0.0"), 0);
    EXPECT_GT(wingman::compareUpdateVersions("2.0.0-beta.2", "2.0.0-beta.1"), 0);
    EXPECT_EQ(wingman::compareUpdateVersions("2.0.0-beta.1", "2.0.0-beta.1"), 0);
    // 高三元组预发布仍高于低三元组正式版（semver 语义，服务端 Go 同口径）
    EXPECT_GT(wingman::compareUpdateVersions("2.0.0-beta.1", "1.10.0"), 0);
}

TEST(UpdateVersionCompare, MetadataIgnored) {
    // 构建元数据不参与比较（semver §10）
    EXPECT_EQ(wingman::compareUpdateVersions("1.2.3+build.5", "1.2.3"), 0);
    EXPECT_EQ(wingman::compareUpdateVersions("1.2.3+a", "1.2.3+b"), 0);
}

TEST(UpdateVersionCompare, UnparsableCountsAsZero) {
    // 三元组解析失败按 0.0.0（发布面由服务端版本号正则守门，这里从宽）
    EXPECT_GT(wingman::compareUpdateVersions("1.2.3", "not-a-version"), 0);
    EXPECT_GT(wingman::compareUpdateVersions("1.2.3", "1.2"), 0);
    EXPECT_EQ(wingman::compareUpdateVersions("abc", "bad"), 0);
}

// ========== UpdateManifest::parse ==========

TEST(UpdateManifestParse, FullBody) {
    wingman::UpdateManifest m;
    ASSERT_TRUE(wingman::UpdateManifest::parse(makeLatestBody("1.2.3", "abc123"), m));
    EXPECT_EQ(m.version, "1.2.3");
    EXPECT_EQ(m.url, "/api/v1/update/download/1");
    EXPECT_EQ(m.sha256, "abc123");
    EXPECT_EQ(m.size, 7);
    EXPECT_EQ(m.notes, "n");
    EXPECT_TRUE(m.mandatory);
}

TEST(UpdateManifestParse, DefaultsWhenOptionalFieldsMissing) {
    wingman::UpdateManifest m;
    ASSERT_TRUE(wingman::UpdateManifest::parse(
        R"({"success":true,"manifest":{"version":"1.0.0","url":"u","sha256":"s"}})", m));
    EXPECT_EQ(m.size, 0);
    EXPECT_FALSE(m.mandatory);
    EXPECT_TRUE(m.notes.empty());
}

TEST(UpdateManifestParse, RejectsMalformed) {
    wingman::UpdateManifest m;
    EXPECT_FALSE(wingman::UpdateManifest::parse("", m));
    EXPECT_FALSE(wingman::UpdateManifest::parse("not json", m));
    EXPECT_FALSE(wingman::UpdateManifest::parse(R"({"success":true})", m));                       // 缺 manifest
    EXPECT_FALSE(wingman::UpdateManifest::parse(R"({"manifest":{"version":"1.0.0"}})", m));       // 缺 url/sha256
    EXPECT_FALSE(wingman::UpdateManifest::parse(R"({"manifest":{"version":"","url":"u","sha256":"s"}})", m));
    EXPECT_FALSE(wingman::UpdateManifest::parse(R"({"manifest":{"version":1,"url":"u","sha256":"s"}})", m));
}

// ========== verifyFileSha256 ==========

TEST(UpdateVerifySha256, MatchesAndRejects) {
    TempDir dir;
    const fs::path file = dir.path / "artifact.bin";
    const std::string content = "wingman setup payload";
    writeFile(file, content);

    EXPECT_TRUE(wingman::verifyFileSha256(file.string(), wingman::crypt::sha256(content)));
    EXPECT_FALSE(wingman::verifyFileSha256(file.string(), std::string(64, '0')));
    EXPECT_FALSE(wingman::verifyFileSha256((dir.path / "missing.bin").string(),
                                           wingman::crypt::sha256(content)));
}

TEST(UpdateVerifySha256, StreamsBeyondSingleChunk) {
    TempDir dir;
    // 3MiB > 1MiB 分块缓冲，覆盖多轮 DigestUpdate
    const std::string big(3 << 20, 'x');
    const fs::path file = dir.path / "big.bin";
    writeFile(file, big);
    EXPECT_TRUE(wingman::verifyFileSha256(file.string(), wingman::crypt::sha256(big)));
}

// ========== file:// 端到端（需 CURL） ==========

#if defined(WINGMAN_HAS_CURL)

TEST(UpdateTransport, FetchLatestViaFileScheme) {
    TempDir dir;
    // checkUpdateLatest 固定拼 /api/v1/update/latest（无 query 时），
    // 用同名空扩展名文件承载响应体
    const fs::path manifestFile = dir.path / "api" / "v1" / "update" / "latest";
    fs::create_directories(manifestFile.parent_path());

    const std::string content = "update artifact payload";
    const std::string sum = wingman::crypt::sha256(content);
    writeFile(manifestFile, makeLatestBody("1.2.3", sum));

    // 空 channel/platform/arch = 不拼 query，走服务端默认
    wingman::UpdateManifest m;
    EXPECT_TRUE(wingman::checkUpdateLatest(fileUrlOf(dir.path), "", "", "", m));
    EXPECT_EQ(m.version, "1.2.3");
    EXPECT_EQ(m.sha256, sum);
    EXPECT_EQ(m.url, "/api/v1/update/download/1");

    // 源不存在 → false
    wingman::UpdateManifest miss;
    EXPECT_FALSE(wingman::checkUpdateLatest(fileUrlOf(dir.path / "no-such-root"), "", "", "", miss));
}

TEST(UpdateTransport, DownloadArtifactVerifiesSha) {
    TempDir dir;
    const fs::path src = dir.path / "setup-1.2.3.exe";
    const std::string content = "update artifact payload";
    writeFile(src, content);
    const std::string sum = wingman::crypt::sha256(content);

    const fs::path dest = dir.path / "out" / "setup.exe";
    ASSERT_TRUE(wingman::downloadUpdateArtifact(fileUrlOf(src), dest.string(), sum));
    std::ifstream in(dest, std::ios::binary);
    const std::string got((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    EXPECT_EQ(got, content);
    // 临时 .part 已改名落定，不残留
    EXPECT_FALSE(fs::exists(dest.string() + ".part"));

    // sha256 不匹配 → 拒绝且不留半成品
    const fs::path dest2 = dir.path / "out2" / "setup.exe";
    EXPECT_FALSE(wingman::downloadUpdateArtifact(fileUrlOf(src), dest2.string(), std::string(64, '0')));
    EXPECT_FALSE(fs::exists(dest2));
    EXPECT_FALSE(fs::exists(dest2.string() + ".part"));

    // 源不存在 → false
    const fs::path dest3 = dir.path / "out3" / "setup.exe";
    EXPECT_FALSE(wingman::downloadUpdateArtifact(fileUrlOf(dir.path / "missing.bin"), dest3.string(), ""));
    EXPECT_FALSE(fs::exists(dest3));
}

#endif // WINGMAN_HAS_CURL
