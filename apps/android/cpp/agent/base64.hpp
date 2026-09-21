#pragma once

// Header-only base64 编码（A2）。
//
// 语义拷贝自 lib/wingman/src/crypt.cpp 的 crypt::base64Encode（该翻译
// 单元携带 OpenSSL 依赖，不进 NDK 构建；远程截图只需 encode）。
// 实现处标注来源，未来 crypt 抽离 base64 时可切回。

#include <cstdint>
#include <string>
#include <vector>

namespace wingman::android {

inline std::string base64Encode(const std::vector<uint8_t>& data) {
    static const char kAlphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string encoded;
    encoded.reserve(((data.size() + 2) / 3) * 4);
    size_t i = 0;
    while (i + 2 < data.size()) {
        const uint32_t triple = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
        encoded += kAlphabet[(triple >> 18) & 0x3F];
        encoded += kAlphabet[(triple >> 12) & 0x3F];
        encoded += kAlphabet[(triple >> 6) & 0x3F];
        encoded += kAlphabet[triple & 0x3F];
        i += 3;
    }
    if (i + 1 == data.size()) {
        const uint32_t pair = data[i] << 16;
        encoded += kAlphabet[(pair >> 18) & 0x3F];
        encoded += kAlphabet[(pair >> 12) & 0x3F];
        encoded += "==";
    } else if (i + 2 == data.size()) {
        const uint32_t triple = (data[i] << 16) | (data[i + 1] << 8);
        encoded += kAlphabet[(triple >> 18) & 0x3F];
        encoded += kAlphabet[(triple >> 12) & 0x3F];
        encoded += kAlphabet[(triple >> 6) & 0x3F];
        encoded += "=";
    }
    return encoded;
}

} // namespace wingman::android
