#include "wingman/runtime/packer.hpp"
#include "wingman/runtime/resource_pack.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>  // std::min（显式包含，不依赖传递包含）
#include <fstream>
#include <sstream>
#include <cstring>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#include <imagehlp.h>
#include <shlobj.h>
#pragma comment(lib, "imagehlp.lib")
#endif

#ifdef WINGMAN_HAS_LUA
#include <sol/sol.hpp>
#endif

namespace wingman::runtime {

// ========== 资源类型定义 ==========
constexpr const char* WM_SCRIPT_RESOURCE = "WM_SCRIPT";
constexpr const char* WM_MANIFEST_RESOURCE = "WM_MANIFEST";

namespace {

/// 简化 LZ 压缩（LZ4 风格，非严格兼容）；与 resource_loader.cpp 的解压互为逆操作。
/// 压缩不省字节时原样返回——调用方按「长度是否变短」决定是否置 PACK_FLAG_COMPRESSED。
std::vector<uint8_t> compressPayload(const std::vector<uint8_t>& data) {
    // 简单实现：使用重复序列压缩
    std::vector<uint8_t> compressed;
    size_t i = 0;

    while (i < data.size()) {
        // 查找重复序列
        size_t maxRepeat = 0;
        size_t repeatPos = 0;

        for (size_t j = 1; j < 128 && j <= i; j++) {
            size_t count = 0;
            while (i + count < data.size() && count < 127 &&
                   data[i - j] == data[i + count]) {
                count++;
            }
            if (count > maxRepeat) {
                maxRepeat = count;
                repeatPos = j;
            }
        }

        if (maxRepeat >= 4) {
            // 写入重复引用
            compressed.push_back(0x80 | static_cast<uint8_t>(repeatPos));  // 高位表示重复
            compressed.push_back(static_cast<uint8_t>(maxRepeat));
            i += maxRepeat;
        } else {
            // 写入原始字节（最多 127 字节）
            size_t literalCount = std::min(size_t(127), data.size() - i);
            compressed.push_back(static_cast<uint8_t>(literalCount));
            compressed.insert(compressed.end(), data.begin() + i, data.begin() + i + literalCount);
            i += literalCount;
        }
    }

    spdlog::debug("Compression: {} -> {} bytes", data.size(), compressed.size());
    return compressed.size() < data.size() ? compressed : data;
}

} // namespace

class Packer::Impl {
public:
    PackerOptions options;

    // 更新 PE 资源
    bool updateResource(const std::vector<uint8_t>& scriptData) {
#ifdef _WIN32
        std::wstring outputPathW;
        outputPathW.assign(options.outputPath.begin(), options.outputPath.end());

        // 开始资源更新
        HANDLE hUpdate = BeginUpdateResourceW(outputPathW.c_str(), FALSE);
        if (!hUpdate) {
            spdlog::error("Failed to begin resource update: {}", GetLastError());
            return false;
        }

        // 完整资源数据（头部 + 负载）由平台无关的同一条代码产出：
        // PE 嵌入只是给这份字节流换个容器，Linux 上测的字节语义与此处完全一致。
        //
        // 这里不能写成 const：UpdateResourceA 的第 5 参是 LPVOID（它只读这份
        // 字节，签名不带 const 是 Win32 的历史包袱），MSVC 对 const uint8_t*
        // → LPVOID 直接 C2664，而这段在 Linux 上根本不参与编译——只有 Windows
        // CI 会红（实测踩过，改回非 const 即可，无需 const_cast）。
        std::vector<uint8_t> resourceData =
            Packer::buildResourceBytes(scriptData, options.encrypt, options.compress, options.password);

        // 添加资源
        BOOL result = UpdateResourceA(
            hUpdate,
            RT_RCDATA,                                    // 资源类型
            MAKEINTRESOURCEA(PACK_PE_RESOURCE_ID),        // 资源 ID（与读侧同源常量）
            MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
            resourceData.data(),
            static_cast<DWORD>(resourceData.size())
        );

        if (!result) {
            spdlog::error("Failed to update resource: {}", GetLastError());
            EndUpdateResource(hUpdate, TRUE);
            return false;
        }

        // 结束资源更新
        if (!EndUpdateResource(hUpdate, FALSE)) {
            spdlog::error("Failed to end resource update: {}", GetLastError());
            return false;
        }

        spdlog::info("Resource embedded successfully: {} bytes", resourceData.size());
        return true;
#else
        // PE 资源写入（BeginUpdateResource/UpdateResource）不存在非 Windows 对应物：
        // ELF 侧要产出可分发的自包含可执行文件得另设容器方案，未实现即明确失败，
        // 不静默写出一份「看起来成功、实际没嵌脚本」的产物。
        (void)scriptData;
        spdlog::error("Resource update not supported on this platform");
        return false;
#endif
    }

    // 替换图标
    bool replaceIcon() {
        if (options.iconPath.empty()) {
            return true;  // 没有指定图标，跳过
        }

#ifdef _WIN32
        std::wstring iconPathW, outputPathW;
        iconPathW.assign(options.iconPath.begin(), options.iconPath.end());
        outputPathW.assign(options.outputPath.begin(), options.outputPath.end());

        // 简单实现：复制图标到资源
        HANDLE hUpdate = BeginUpdateResourceW(outputPathW.c_str(), FALSE);
        if (!hUpdate) {
            spdlog::warn("Failed to begin resource update for icon: {}", GetLastError());
            return false;
        }

        // 读取图标文件
        std::ifstream iconFile(options.iconPath, std::ios::binary);
        if (!iconFile) {
            spdlog::warn("Failed to open icon file: {}", options.iconPath);
            EndUpdateResource(hUpdate, TRUE);
            return false;
        }

        std::vector<uint8_t> iconData((std::istreambuf_iterator<char>(iconFile)),
                                       std::istreambuf_iterator<char>());
        iconFile.close();

        // 替换主图标 (ID = 1)
        BOOL result = UpdateResourceA(
            hUpdate,
            RT_ICON,
            MAKEINTRESOURCEA(1),
            MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
            iconData.data(),
            static_cast<DWORD>(iconData.size())
        );

        if (!result) {
            spdlog::warn("Failed to update icon resource: {}", GetLastError());
            EndUpdateResource(hUpdate, TRUE);
            return false;
        }

        EndUpdateResource(hUpdate, FALSE);
        spdlog::info("Icon replaced successfully");
        return true;
#else
        spdlog::warn("Icon replacement not supported on this platform");
        return true;
#endif
    }
};

// ========== Packer 实现 ==========

Packer::Packer(const PackerOptions& options)
    : impl_(std::make_unique<Impl>())
{
    impl_->options = options;
}

Packer::~Packer() = default;

PackerResult Packer::build() {
    PackerResult result;
    result.outputPath = impl_->options.outputPath;

    auto cleanupPartialOutput = [&]() {
        std::error_code ec;
        std::filesystem::remove(impl_->options.outputPath, ec);
        if (ec) {
            spdlog::warn("Failed to remove partial output '{}': {}", impl_->options.outputPath, ec.message());
        }
    };

    try {
        spdlog::info("=== Wingman Packer ===");
        spdlog::info("Script: {}", impl_->options.scriptPath);
        spdlog::info("Output: {}", impl_->options.outputPath);
        // 口令只参与密钥派生：不进日志、不写进产物（产物里只有 salt/IV 与密钥指纹）
        spdlog::info("Encrypt: {}", impl_->options.encrypt);
        spdlog::info("Compress: {}", impl_->options.compress);

        if (impl_->options.encrypt && impl_->options.password.empty()) {
            // 加密产物没有口令就是永久打不开的字节流，故在动手（复制 stub）之前硬拒。
            result.message = "Encryption requires a password: pass --password (or set WINGMAN_PACK_PASSWORD)";
            spdlog::error("{}", result.message);
            return result;
        }

        // 1. 读取并处理脚本
        std::vector<uint8_t> scriptData = processScript();
        if (scriptData.empty()) {
            result.message = "Failed to read script file";
            return result;
        }

        // 2. 复制 stub 程序
        if (!copyStub()) {
            result.message = "Failed to copy stub executable";
            return result;
        }

        // 3. 嵌入资源
        if (!embedResource(scriptData)) {
            result.message = "Failed to embed script resource";
            cleanupPartialOutput();
            return result;
        }

        // 4. 替换图标
        if (!impl_->replaceIcon()) {
            spdlog::warn("Icon replacement failed, continuing...");
        }

        result.success = true;
        result.message = "Build completed successfully";

        spdlog::info("=== Build Complete ===");
        spdlog::info("Output: {}", impl_->options.outputPath);
        spdlog::info("Size: {} KB", std::filesystem::file_size(impl_->options.outputPath) / 1024);

    } catch (const std::exception& e) {
        result.message = std::string("Build failed: ") + e.what();
        spdlog::error("{}", result.message);
        cleanupPartialOutput();
    }

    return result;
}

std::vector<uint8_t> Packer::processScript() {
    std::ifstream file(impl_->options.scriptPath, std::ios::binary);
    if (!file) {
        spdlog::error("Failed to open script: {}", impl_->options.scriptPath);
        return {};
    }

    std::vector<uint8_t> content((std::istreambuf_iterator<char>(file)),
                                 std::istreambuf_iterator<char>());
    file.close();

    spdlog::info("Script loaded: {} bytes", content.size());
    return content;
}

std::vector<uint8_t> Packer::compileToBytecode(const std::string& source) {
#ifdef WINGMAN_HAS_LUA
    // Use Lua C API to compile source to bytecode
    sol::state lua;
    // Load source without executing
    auto result = lua.load(source, "packed_script");
    if (!result.valid()) {
        sol::error err = result;
        spdlog::error("Failed to compile Lua script: {}", err.what());
        return std::vector<uint8_t>(source.begin(), source.end());
    }

    // Dump bytecode from the compiled function
    std::vector<uint8_t> bytecode;
    sol::protected_function fn = result;

    lua_State* L = lua.lua_state();
    // Push the function onto the stack
    fn.push(L);

    // Dump bytecode
    struct BytecodeWriter {
        std::vector<uint8_t>* output;
        BytecodeWriter(std::vector<uint8_t>* out) : output(out) {}
    };

    auto writer = [](lua_State* /*L*/, const void* data, size_t sz, void* ud) -> int {
        auto* writer = static_cast<BytecodeWriter*>(ud);
        const uint8_t* bytes = static_cast<const uint8_t*>(data);
        writer->output->insert(writer->output->end(), bytes, bytes + sz);
        return 0;
    };

    BytecodeWriter bw(&bytecode);
    if (lua_dump(L, writer, &bw, 0) != 0) {
        spdlog::error("Failed to dump Lua bytecode");
        lua_pop(L, 1);
        return std::vector<uint8_t>(source.begin(), source.end());
    }

    lua_pop(L, 1);
    spdlog::info("Compiled to Lua bytecode: {} bytes", bytecode.size());
    return bytecode;
#else
    spdlog::warn("Lua not available, using source code");
    return std::vector<uint8_t>(source.begin(), source.end());
#endif
}

std::vector<uint8_t> Packer::buildResourceBytes(const std::vector<uint8_t>& scriptData,
                                                bool encrypt,
                                                bool compress,
                                                const std::string& password) {
    PACK_HEADER header = {};
    setPackMagic(header);
    header.version = PACK_FORMAT_VERSION;
    header.flags = 0;
    header.originalSize = scriptData.size();  // 始终记录原始大小

    // dataHash 必须基于原始明文——加密/压缩后的字节不参与完整性定义
    const std::vector<uint8_t> hash = sha256Bytes(scriptData);
    std::memcpy(header.dataHash, hash.data(), std::min(hash.size(), sizeof(header.dataHash)));

    std::vector<uint8_t> payload = scriptData;

    // 变换顺序是 compress → encrypt（读侧镜像：decrypt → decompress）。
    // 反过来的话密文里没有字节连串可压，压缩永远不省字节 ——「加密 + 压缩」的组合
    // 会静默退化成「只加密」（v2 之前正是这个顺序，但当时 build() 拒绝 encrypt，
    // 故不存在依赖旧顺序的产物）。压缩明文只泄露「明文可压缩程度」这一弱信号，
    // 且这里是静态一次性压缩、无攻击者可参与的自适应压缩，不构成 CRIME 类面。
    if (compress) {
        // 只有真的压小才置位：packer 在没省字节时原样返回，读侧据标志决定是否解压
        const size_t preCompressSize = payload.size();
        payload = compressPayload(payload);
        if (payload.size() < preCompressSize) {
            header.flags |= PACK_FLAG_COMPRESSED;
        }
    }

    if (encrypt) {
        // 口令 → PBKDF2 → AES-256 密钥；salt/IV 随机生成并写进头部（同口令两次打包密文不同）。
        // derivePackKey 对空口令直接抛错：没有口令的加密产物再也解不开，宁可不产出。
        const PackCryptoParams params = makePackCryptoParams();
        const std::vector<uint8_t> key = derivePackKey(password, params);

        setPackCryptoParams(header, params);
        payload = wingman::crypt::aesGcmEncrypt(key, packCryptoIv(params), payload);
        // keyHash 存的是 sha256(派生密钥)——不是密钥本身，只用于加载前的口令快速校验，
        // 真正的边界是 AES-GCM 的认证标签。
        const std::vector<uint8_t> keyHash = packKeyFingerprint(key);
        std::memcpy(header.keyHash, keyHash.data(), std::min(keyHash.size(), sizeof(header.keyHash)));
        header.flags |= PACK_FLAG_ENCRYPTED;
    }

    header.compressedSize = payload.size();

    std::vector<uint8_t> resourceData(sizeof(PACK_HEADER) + payload.size());
    std::memcpy(resourceData.data(), &header, sizeof(PACK_HEADER));
    std::memcpy(resourceData.data() + sizeof(PACK_HEADER), payload.data(), payload.size());
    return resourceData;
}

bool Packer::copyStub() {
    std::error_code ec;

    // 如果输出文件已存在，先删除
    if (std::filesystem::exists(impl_->options.outputPath)) {
        std::filesystem::remove(impl_->options.outputPath, ec);
        if (ec) {
            spdlog::error("Failed to remove existing output file: {}", ec.message());
            return false;
        }
    }

    // 复制 stub 程序
    std::filesystem::copy_file(impl_->options.stubPath, impl_->options.outputPath, ec);
    if (ec) {
        spdlog::error("Failed to copy stub: {} -> {}", impl_->options.stubPath, impl_->options.outputPath);
        return false;
    }

    spdlog::info("Stub copied: {} bytes", std::filesystem::file_size(impl_->options.outputPath));
    return true;
}

bool Packer::embedResource(const std::vector<uint8_t>& data) {
    return impl_->updateResource(data);
}

bool Packer::replaceIcon() {
    return impl_->replaceIcon();
}

bool Packer::setVersionInfo() {
#ifdef _WIN32
    std::wstring outputPathW;
    outputPathW.assign(impl_->options.outputPath.begin(), impl_->options.outputPath.end());

    HANDLE hUpdate = BeginUpdateResourceW(outputPathW.c_str(), FALSE);
    if (!hUpdate) {
        spdlog::warn("Failed to begin resource update for version info");
        return false;
    }

    // Parse version string (e.g. "1.2.3")
    WORD major = 1, minor = 0, patch = 0, build = 0;
    sscanf(impl_->options.appVersion.c_str(), "%hu.%hu.%hu.%hu", &major, &minor, &patch, &build);

    // Build VS_VERSIONINFO resource
    struct {
        VS_FIXEDFILEINFO ffi;
    } versionData = {};

    versionData.ffi.dwSignature = 0xFEEF04BD;
    versionData.ffi.dwStrucVersion = 0x00010000;
    versionData.ffi.dwFileVersionMS = MAKELONG(minor, major);
    versionData.ffi.dwFileVersionLS = MAKELONG(build, patch);
    versionData.ffi.dwProductVersionMS = MAKELONG(minor, major);
    versionData.ffi.dwProductVersionLS = MAKELONG(build, patch);
    versionData.ffi.dwFileFlagsMask = 0x3F;
    versionData.ffi.dwFileFlags = 0;
    versionData.ffi.dwFileOS = VOS_NT_WINDOWS32;
    versionData.ffi.dwFileType = VFT_APP;
    versionData.ffi.dwFileSubtype = VFT2_UNKNOWN;
    versionData.ffi.dwFileDateMS = 0;
    versionData.ffi.dwFileDateLS = 0;

    // Build full version info resource with string table
    // The resource format is complex, we build a minimal valid block
    std::wstring appNameW(impl_->options.appName.begin(), impl_->options.appName.end());
    std::wstring versionW(impl_->options.appVersion.begin(), impl_->options.appVersion.end());

    // Calculate total size needed for VS_VERSIONINFO + StringFileInfo
    size_t headerSize = sizeof(VS_FIXEDFILEINFO) + 40; // VS_VERSIONINFO header
    size_t strTableSize = 0;

    // String entries: each has key + value (wchar_t aligned)
    struct StrEntry { const wchar_t* key; const std::wstring& value; };
    StrEntry entries[] = {
        {L"ProductName", appNameW},
        {L"FileDescription", appNameW},
        {L"FileVersion", versionW},
        {L"ProductVersion", versionW},
        {L"OriginalFilename", appNameW},
        {L"CompanyName", std::wstring(L"")},
    };

    for (const auto& e : entries) {
        size_t keyLen = wcslen(e.key);
        size_t valLen = e.value.size();
        // String structure: sizeof(WORD)*6 + key wchars + padding + value wchars + padding
        strTableSize += 6 * sizeof(WORD) + keyLen * sizeof(wchar_t);
        strTableSize = (strTableSize + 3) & ~3; // align
        strTableSize += valLen * sizeof(wchar_t);
        strTableSize = (strTableSize + 3) & ~3;
    }

    size_t strTableHeaderSize = 6 * sizeof(WORD) + 8; // StringTable header + "040904b0"
    size_t strFileInfoHeaderSize = 6 * sizeof(WORD) + 14 * sizeof(wchar_t); // "StringFileInfo"
    size_t totalSize = headerSize + strFileInfoHeaderSize + strTableHeaderSize + strTableSize;
    totalSize = (totalSize + 3) & ~3;

    std::vector<uint8_t> resource(totalSize, 0);
    uint8_t* p = resource.data();

    // VS_VERSIONINFO header
    auto writeWord = [&p](WORD w) { memcpy(p, &w, sizeof(WORD)); p += sizeof(WORD); };
    auto writeDword = [&p](DWORD dw) { memcpy(p, &dw, sizeof(DWORD)); p += sizeof(DWORD); };
    auto writeWString = [&p](const wchar_t* s, size_t len) {
        memcpy(p, s, len * sizeof(wchar_t));
        p += len * sizeof(wchar_t);
    };
    auto align = [&p]() { p = (uint8_t*)(((uintptr_t)p + 3) & ~3); };

    // Write VS_FIXEDFILEINFO directly at the correct offset
    // VS_VERSIONINFO starts at offset 0
    WORD vsVersionInfoLen = (WORD)(headerSize + strFileInfoHeaderSize + strTableHeaderSize + strTableSize);
    writeWord(vsVersionInfoLen);  // wLength
    writeWord(sizeof(VS_FIXEDFILEINFO) + 40); // wValueLength
    writeWord(0);  // wType (binary)
    writeWString(L"VS_VERSION_INFO", 15);
    align();
    memcpy(p, &versionData.ffi, sizeof(VS_FIXEDFILEINFO));
    p += sizeof(VS_FIXEDFILEINFO);
    align();

    // StringFileInfo block
    size_t sfiStart = p - resource.data();
    writeWord(0); // placeholder for length
    writeWord(0); // wValueLength
    writeWord(1); // wType (text)
    writeWString(L"StringFileInfo", 14);
    align();

    // StringTable
    size_t stStart = p - resource.data();
    writeWord(0); // placeholder for length
    writeWord(0); // wValueLength
    writeWord(1); // wType (text)
    writeWString(L"040904b0", 8);
    align();

    // String entries
    for (const auto& e : entries) {
        size_t entryStart = p - resource.data();
        size_t keyLen = wcslen(e.key);
        size_t valLen = e.value.size();

        writeWord(0); // placeholder for length
        writeWord((WORD)valLen);
        writeWord(1); // wType (text)
        writeWString(e.key, keyLen);
        align();
        if (valLen > 0) writeWString(e.value.c_str(), valLen);
        align();

        // Fill in length
        size_t entryLen = (p - resource.data()) - entryStart;
        *(WORD*)(resource.data() + entryStart) = (WORD)entryLen;
    }

    // Fill in StringTable length
    *(WORD*)(resource.data() + stStart) = (WORD)((p - resource.data()) - stStart);
    // Fill in StringFileInfo length
    *(WORD*)(resource.data() + sfiStart) = (WORD)((p - resource.data()) - sfiStart);

    // Update total resource length
    *(WORD*)(resource.data()) = (WORD)(p - resource.data());

    BOOL result = UpdateResourceA(
        hUpdate,
        RT_VERSION,
        MAKEINTRESOURCEA(VS_VERSION_INFO),
        MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
        resource.data(),
        static_cast<DWORD>(p - resource.data())
    );

    if (!result) {
        spdlog::warn("Failed to update version info resource: {}", GetLastError());
        EndUpdateResource(hUpdate, TRUE);
        return false;
    }

    if (!EndUpdateResource(hUpdate, FALSE)) {
        spdlog::warn("Failed to end version info update: {}", GetLastError());
        return false;
    }

    spdlog::info("Version info set: {} v{}", impl_->options.appName, impl_->options.appVersion);
    return true;
#else
    spdlog::warn("Version info not supported on this platform");
    return true;
#endif
}

} // namespace wingman::runtime
