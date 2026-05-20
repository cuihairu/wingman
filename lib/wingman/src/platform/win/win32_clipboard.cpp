#include "wingman/platform/iclipboard.hpp"
#include <spdlog/spdlog.h>
#include <vector>
#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shlobj.h>

namespace wingman::platform::windows {

/**
 * @brief Windows 剪贴板实现
 *
 * 支持 CF_TEXT, CF_UNICODETEXT, CF_HDROP, CF_DIB 等格式。
 */
class Win32Clipboard : public IClipboard {
public:
    Win32Clipboard() : initialized_(false), opened_(false) {}

    ~Win32Clipboard() override {
        shutdown();
    }

    bool initialize() override {
        if (initialized_) return true;
        initialized_ = true;
        return true;
    }

    void shutdown() override {
        if (opened_) {
            CloseClipboard();
            opened_ = false;
        }
        initialized_ = false;
    }

    // ========== 文本操作 ==========

    bool setText(const std::string& text) override {
        if (!openClipboard()) return false;

        // 转换为 UTF-16
        int wideSize = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
        if (wideSize <= 0) {
            closeClipboard();
            return false;
        }

        std::vector<wchar_t> wideText(wideSize);
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wideText.data(), wideSize);

        // 计算所需内存大小
        SIZE_T bytes = wideSize * sizeof(wchar_t);

        // 分配全局内存
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (!hMem) {
            spdlog::error("[Win32Clipboard] GlobalAlloc failed");
            closeClipboard();
            return false;
        }

        // 锁定内存并复制数据
        wchar_t* pData = static_cast<wchar_t*>(GlobalLock(hMem));
        if (pData) {
            memcpy(pData, wideText.data(), bytes);
            GlobalUnlock(hMem);
        }

        // 设置剪贴板数据
        BOOL result = SetClipboardData(CF_UNICODETEXT, hMem) != nullptr;

        closeClipboard();

        if (!result) {
            GlobalFree(hMem);
            spdlog::error("[Win32Clipboard] SetClipboardData failed");
        }

        return result != FALSE;
    }

    std::string getText() override {
        std::string result;

        if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) {
            return result;
        }

        if (!openClipboard()) {
            return result;
        }

        HGLOBAL hMem = GetClipboardData(CF_UNICODETEXT);
        if (hMem) {
            wchar_t* pData = static_cast<wchar_t*>(GlobalLock(hMem));
            if (pData) {
                // 转换为 UTF-8
                int size = WideCharToMultiByte(CP_UTF8, 0, pData, -1, nullptr, 0, nullptr, nullptr);
                if (size > 0) {
                    result.resize(size - 1);
                    WideCharToMultiByte(CP_UTF8, 0, pData, -1, &result[0], size, nullptr, nullptr);
                }
                GlobalUnlock(hMem);
            }
        }

        closeClipboard();
        return result;
    }

    bool hasText() override {
        return IsClipboardFormatAvailable(CF_UNICODETEXT) != FALSE ||
               IsClipboardFormatAvailable(CF_TEXT) != FALSE;
    }

    // ========== HTML 操作 ==========

    bool setHTML(const std::string& html) override {
        if (!openClipboard()) return false;

        // HTML Clipboard Format
        // 格式: "Version:0.9\r\nStartHTML:0000000105\r\nEndHTML:0000000134\r\nStartFragment:0000000139\r\nEndFragment:0000000156\r\n<html>...</html>"
        // 简化版本：直接使用 HTML 格式名

        // 注册 HTML 格式
        static UINT cfHTML = RegisterClipboardFormatW(L"HTML Format");

        if (cfHTML == 0) {
            spdlog::error("[Win32Clipboard] RegisterClipboardFormatW for HTML failed");
            closeClipboard();
            return false;
        }

        // 转换为 UTF-16
        int wideSize = MultiByteToWideChar(CP_UTF8, 0, html.c_str(), -1, nullptr, 0);
        if (wideSize <= 0) {
            closeClipboard();
            return false;
        }

        std::vector<wchar_t> wideHTML(wideSize);
        MultiByteToWideChar(CP_UTF8, 0, html.c_str(), -1, wideHTML.data(), wideSize);

        SIZE_T bytes = wideSize * sizeof(wchar_t);
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (!hMem) {
            closeClipboard();
            return false;
        }

        wchar_t* pData = static_cast<wchar_t*>(GlobalLock(hMem));
        if (pData) {
            memcpy(pData, wideHTML.data(), bytes);
            GlobalUnlock(hMem);
        }

        BOOL result = SetClipboardData(cfHTML, hMem) != nullptr;
        closeClipboard();

        if (!result) {
            GlobalFree(hMem);
        }

        return result != FALSE;
    }

    std::string getHTML() override {
        std::string result;

        static UINT cfHTML = RegisterClipboardFormatW(L"HTML Format");
        if (cfHTML == 0) {
            return result;
        }

        if (!IsClipboardFormatAvailable(cfHTML)) {
            return result;
        }

        if (!openClipboard()) {
            return result;
        }

        HGLOBAL hMem = GetClipboardData(cfHTML);
        if (hMem) {
            wchar_t* pData = static_cast<wchar_t*>(GlobalLock(hMem));
            if (pData) {
                int size = WideCharToMultiByte(CP_UTF8, 0, pData, -1, nullptr, 0, nullptr, nullptr);
                if (size > 0) {
                    result.resize(size - 1);
                    WideCharToMultiByte(CP_UTF8, 0, pData, -1, &result[0], size, nullptr, nullptr);
                }
                GlobalUnlock(hMem);
            }
        }

        closeClipboard();
        return result;
    }

    bool hasHTML() override {
        static UINT cfHTML = RegisterClipboardFormatW(L"HTML Format");
        return cfHTML != 0 && IsClipboardFormatAvailable(cfHTML) != FALSE;
    }

    // ========== 图像操作 ==========

    bool setImage(const std::vector<uint8_t>& imageData, int width, int height) override {
        if (!openClipboard()) return false;

        // 创建 BITMAPINFOHEADER
        int headerSize = sizeof(BITMAPINFOHEADER);
        int imageSize = static_cast<int>(imageData.size());
        int totalSize = headerSize + imageSize;

        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, totalSize);
        if (!hMem) {
            closeClipboard();
            return false;
        }

        uint8_t* pData = static_cast<uint8_t*>(GlobalLock(hMem));
        if (pData) {
            // 填充 BITMAPINFOHEADER
            BITMAPINFOHEADER* bmi = reinterpret_cast<BITMAPINFOHEADER*>(pData);
            bmi->biSize = sizeof(BITMAPINFOHEADER);
            bmi->biWidth = width;
            bmi->biHeight = height;  // 正数表示自下而上
            bmi->biPlanes = 1;
            bmi->biBitCount = 32;
            bmi->biCompression = BI_RGB;
            bmi->biSizeImage = imageSize;
            bmi->biXPelsPerMeter = 0;
            bmi->biYPelsPerMeter = 0;
            bmi->biClrUsed = 0;
            bmi->biClrImportant = 0;

            // 复制图像数据
            memcpy(pData + headerSize, imageData.data(), imageSize);
            GlobalUnlock(hMem);
        }

        BOOL result = SetClipboardData(CF_DIB, hMem) != nullptr;
        closeClipboard();

        if (!result) {
            GlobalFree(hMem);
        }

        return result != FALSE;
    }

    std::vector<uint8_t> getImage(int* outWidth, int* outHeight) override {
        std::vector<uint8_t> result;

        if (outWidth) *outWidth = 0;
        if (outHeight) *outHeight = 0;

        if (!IsClipboardFormatAvailable(CF_DIB)) {
            return result;
        }

        if (!openClipboard()) {
            return result;
        }

        HGLOBAL hMem = GetClipboardData(CF_DIB);
        if (hMem) {
            uint8_t* pData = static_cast<uint8_t*>(GlobalLock(hMem));
            if (pData) {
                BITMAPINFOHEADER* bmi = reinterpret_cast<BITMAPINFOHEADER*>(pData);

                int width = bmi->biWidth;
                int height = abs(bi->biHeight);
                int bitCount = bmi->biBitCount;
                int imageSize = bmi->biSizeImage;

                if (bitCount == 32 && imageSize > 0) {
                    result.resize(imageSize);
                    memcpy(result.data(), pData + sizeof(BITMAPINFOHEADER), imageSize);

                    if (outWidth) *outWidth = width;
                    if (outHeight) *outHeight = height;
                }

                GlobalUnlock(hMem);
            }
        }

        closeClipboard();
        return result;
    }

    bool hasImage() override {
        return IsClipboardFormatAvailable(CF_DIB) != FALSE ||
               IsClipboardFormatAvailable(CF_DIBV5) != FALSE;
    }

    // ========== 文件操作 ==========

    bool setFiles(const std::vector<std::string>& files) override {
        if (files.empty()) return false;

        if (!openClipboard()) return false;

        // 计算所需内存大小
        SIZE_T totalSize = sizeof(DROPFILES);
        for (const auto& file : files) {
            // 转换为宽字符路径
            int wideSize = MultiByteToWideChar(CP_UTF8, 0, file.c_str(), -1, nullptr, 0);
            totalSize += wideSize * sizeof(wchar_t);
        }
        // 结束符
        totalSize += sizeof(wchar_t);

        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, totalSize);
        if (!hMem) {
            closeClipboard();
            return false;
        }

        DROPFILES* pDropFiles = static_cast<DROPFILES*>(GlobalLock(hMem));
        if (pDropFiles) {
            pDropFiles->pFiles = sizeof(DROPFILES);
            pDropFiles->pt = {0, 0};
            pDropFiles->fNC = FALSE;
            pDropFiles->fWide = TRUE;  // 使用宽字符

            wchar_t* pFiles = reinterpret_cast<wchar_t*>(reinterpret_cast<uint8_t*>(pDropFiles) + sizeof(DROPFILES));

            for (const auto& file : files) {
                int wideSize = MultiByteToWideChar(CP_UTF8, 0, file.c_str(), -1, pFiles, wideSize);
                pFiles += wideSize;
            }
            *pFiles = L'\0';  // 双 null 终止符

            GlobalUnlock(hMem);
        }

        BOOL result = SetClipboardData(CF_HDROP, hMem) != nullptr;
        closeClipboard();

        if (!result) {
            GlobalFree(hMem);
        }

        return result != FALSE;
    }

    std::vector<std::string> getFiles() override {
        std::vector<std::string> result;

        if (!IsClipboardFormatAvailable(CF_HDROP)) {
            return result;
        }

        if (!openClipboard()) {
            return result;
        }

        HGLOBAL hMem = GetClipboardData(CF_HDROP);
        if (hMem) {
            DROPFILES* pDropFiles = static_cast<DROPFILES*>(GlobalLock(hMem));
            if (pDropFiles) {
                if (pDropFiles->fWide) {
                    wchar_t* pFiles = reinterpret_cast<wchar_t*>(
                        reinterpret_cast<uint8_t*>(pDropFiles) + pDropFiles->pFiles);

                    while (*pFiles) {
                        int size = WideCharToMultiByte(CP_UTF8, 0, pFiles, -1, nullptr, 0, nullptr, nullptr);
                        if (size > 0) {
                            std::string file(size - 1, 0);
                            WideCharToMultiByte(CP_UTF8, 0, pFiles, -1, &file[0], size, nullptr, nullptr);
                            result.push_back(file);
                        }
                        pFiles += wcslen(pFiles) + 1;
                    }
                } else {
                    char* pFiles = reinterpret_cast<char*>(
                        reinterpret_cast<uint8_t*>(pDropFiles) + pDropFiles->pFiles);

                    while (*pFiles) {
                        result.push_back(pFiles);
                        pFiles += strlen(pFiles) + 1;
                    }
                }

                GlobalUnlock(hMem);
            }
        }

        closeClipboard();
        return result;
    }

    bool hasFiles() override {
        return IsClipboardFormatAvailable(CF_HDROP) != FALSE;
    }

    // ========== 通用操作 ==========

    void clear() override {
        if (OpenClipboard(nullptr)) {
            EmptyClipboard();
            CloseClipboard();
        }
    }

    bool isEmpty() override {
        return !IsClipboardFormatAvailable(CF_TEXT) &&
               !IsClipboardFormatAvailable(CF_UNICODETEXT) &&
               !IsClipboardFormatAvailable(CF_DIB) &&
               !IsClipboardFormatAvailable(CF_HDROP);
    }

    std::vector<ClipboardFormat> getAvailableFormats() override {
        std::vector<ClipboardFormat> formats;

        if (!openClipboard()) {
            return formats;
        }

        UINT enumFormat = EnumClipboardFormats(0);
        while (enumFormat != 0) {
            if (enumFormat == CF_TEXT || enumFormat == CF_UNICODETEXT) {
                formats.push_back(ClipboardFormat::Text);
            } else if (enumFormat == CF_DIB || enumFormat == CF_DIBV5) {
                formats.push_back(ClipboardFormat::Image);
            } else if (enumFormat == CF_HDROP) {
                formats.push_back(ClipboardFormat::Files);
            }
            enumFormat = EnumClipboardFormats(enumFormat);
        }

        // 检查自定义格式
        static UINT cfHTML = RegisterClipboardFormatW(L"HTML Format");
        if (cfHTML != 0 && IsClipboardFormatAvailable(cfHTML)) {
            formats.push_back(ClipboardFormat::HTML);
        }

        closeClipboard();
        return formats;
    }

    // ========== 后端信息 ==========

    std::string getBackendName() const override {
        return "Win32";
    }

    BackendInfo getBackendInfo() const override {
        return BackendInfo{
            "Win32",
            "1.0",
            initialized_,
            "Windows Clipboard API"
        };
    }

private:
    bool initialized_;
    bool opened_;

    bool openClipboard() {
        if (!initialized_) return false;

        // 如果已经打开，先关闭
        if (opened_) {
            return true;
        }

        // 尝试打开剪贴板
        // 使用较小的重试次数避免阻塞太久
        for (int i = 0; i < 5; ++i) {
            if (OpenClipboard(nullptr)) {
                opened_ = true;
                return true;
            }
            Sleep(10);
        }

        spdlog::error("[Win32Clipboard] Failed to open clipboard");
        return false;
    }

    void closeClipboard() {
        if (opened_) {
            CloseClipboard();
            opened_ = false;
        }
    }
};

} // namespace wingman::platform::windows

#endif // _WIN32
