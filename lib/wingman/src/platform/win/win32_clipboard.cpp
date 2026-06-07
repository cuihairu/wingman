#include "wingman/platform/win/win32_clipboard.hpp"
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

Win32Clipboard::Win32Clipboard() : initialized_(false), opened_(false) {}

Win32Clipboard::~Win32Clipboard() {
    shutdown();
}

bool Win32Clipboard::initialize() {
    if (initialized_) return true;
    initialized_ = true;
    return true;
}

void Win32Clipboard::shutdown() {
    if (opened_) {
        CloseClipboard();
        opened_ = false;
    }
    initialized_ = false;
}

// ========== Text Operations ==========

bool Win32Clipboard::setText(const std::string& text) {
    if (!openClipboard()) return false;

    // Convert to UTF-16
    int wideSize = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (wideSize <= 0) {
        closeClipboard();
        return false;
    }

    std::vector<wchar_t> wideText(wideSize);
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wideText.data(), wideSize);

    // Calculate required memory size
    SIZE_T bytes = wideSize * sizeof(wchar_t);

    // Allocate global memory
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!hMem) {
        spdlog::error("[Win32Clipboard] GlobalAlloc failed");
        closeClipboard();
        return false;
    }

    // Lock memory and copy data
    wchar_t* pData = static_cast<wchar_t*>(GlobalLock(hMem));
    if (pData) {
        // Verify buffer size before copying
        SIZE_T hMemSize = GlobalSize(hMem);
        if (hMemSize >= bytes) {
            memcpy(pData, wideText.data(), bytes);
        } else {
            spdlog::error("[Win32Clipboard] GlobalAlloc returned smaller buffer than requested");
            GlobalUnlock(hMem);
            GlobalFree(hMem);
            closeClipboard();
            return false;
        }
        GlobalUnlock(hMem);
    }

    // Set clipboard data
    BOOL result = SetClipboardData(CF_UNICODETEXT, hMem) != nullptr;

    closeClipboard();

    if (!result) {
        GlobalFree(hMem);
        spdlog::error("[Win32Clipboard] SetClipboardData failed");
    }

    return result != FALSE;
}

std::string Win32Clipboard::getText() {
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
            // Get buffer size to validate bounds
            SIZE_T hMemSize = GlobalSize(hMem);
            if (hMemSize >= sizeof(wchar_t)) {
                // Calculate maximum string length based on buffer size
                size_t maxChars = hMemSize / sizeof(wchar_t);

                // Find null terminator within buffer bounds
                size_t length = 0;
                while (length < maxChars && pData[length] != L'\0') {
                    length++;
                }

                // Convert to UTF-8 with explicit length
                int size = WideCharToMultiByte(CP_UTF8, 0, pData, static_cast<int>(length), nullptr, 0, nullptr, nullptr);
                if (size > 0) {
                    result.resize(size);
                    WideCharToMultiByte(CP_UTF8, 0, pData, static_cast<int>(length), &result[0], size, nullptr, nullptr);
                }
            }
            GlobalUnlock(hMem);
        }
    }

    closeClipboard();
    return result;
}

bool Win32Clipboard::hasText() {
    return IsClipboardFormatAvailable(CF_UNICODETEXT) != FALSE ||
           IsClipboardFormatAvailable(CF_TEXT) != FALSE;
}

// ========== HTML Operations ==========

bool Win32Clipboard::setHTML(const std::string& html) {
    if (!openClipboard()) return false;

    // Register HTML format
    static UINT cfHTML = RegisterClipboardFormatW(L"HTML Format");

    if (cfHTML == 0) {
        spdlog::error("[Win32Clipboard] RegisterClipboardFormatW for HTML failed");
        closeClipboard();
        return false;
    }

    // Convert to UTF-16
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
        SIZE_T hMemSize = GlobalSize(hMem);
        if (hMemSize >= bytes) {
            memcpy(pData, wideHTML.data(), bytes);
        } else {
            spdlog::error("[Win32Clipboard] GlobalAlloc returned smaller buffer than requested for HTML");
            GlobalUnlock(hMem);
            GlobalFree(hMem);
            closeClipboard();
            return false;
        }
        GlobalUnlock(hMem);
    }

    BOOL result = SetClipboardData(cfHTML, hMem) != nullptr;
    closeClipboard();

    if (!result) {
        GlobalFree(hMem);
    }

    return result != FALSE;
}

std::string Win32Clipboard::getHTML() {
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
            // Get buffer size to validate bounds
            SIZE_T hMemSize = GlobalSize(hMem);
            if (hMemSize >= sizeof(wchar_t)) {
                // Calculate maximum string length based on buffer size
                size_t maxChars = hMemSize / sizeof(wchar_t);

                // Find null terminator within buffer bounds
                size_t length = 0;
                while (length < maxChars && pData[length] != L'\0') {
                    length++;
                }

                // Convert to UTF-8 with explicit length
                int size = WideCharToMultiByte(CP_UTF8, 0, pData, static_cast<int>(length), nullptr, 0, nullptr, nullptr);
                if (size > 0) {
                    result.resize(size);
                    WideCharToMultiByte(CP_UTF8, 0, pData, static_cast<int>(length), &result[0], size, nullptr, nullptr);
                }
            }
            GlobalUnlock(hMem);
        }
    }

    closeClipboard();
    return result;
}

bool Win32Clipboard::hasHTML() {
    static UINT cfHTML = RegisterClipboardFormatW(L"HTML Format");
    return cfHTML != 0 && IsClipboardFormatAvailable(cfHTML) != FALSE;
}

// ========== Image Operations ==========

bool Win32Clipboard::setImage(const std::vector<uint8_t>& imageData, int width, int height) {
    if (!openClipboard()) return false;

    // Create BITMAPINFOHEADER
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
        // Verify buffer size
        SIZE_T hMemSize = GlobalSize(hMem);
        if (hMemSize < static_cast<SIZE_T>(totalSize)) {
            spdlog::error("[Win32Clipboard] GlobalAlloc returned smaller buffer than required for image");
            GlobalUnlock(hMem);
            GlobalFree(hMem);
            closeClipboard();
            return false;
        }

        // Fill BITMAPINFOHEADER
        BITMAPINFOHEADER* bmi = reinterpret_cast<BITMAPINFOHEADER*>(pData);
        bmi->biSize = sizeof(BITMAPINFOHEADER);
        bmi->biWidth = width;
        bmi->biHeight = height;
        bmi->biPlanes = 1;
        bmi->biBitCount = 32;
        bmi->biCompression = BI_RGB;
        bmi->biSizeImage = imageSize;
        bmi->biXPelsPerMeter = 0;
        bmi->biYPelsPerMeter = 0;
        bmi->biClrUsed = 0;
        bmi->biClrImportant = 0;

        // Copy image data
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

std::vector<uint8_t> Win32Clipboard::getImage(int* outWidth, int* outHeight) {
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
            // Verify buffer size is at least large enough for BITMAPINFOHEADER
            SIZE_T hMemSize = GlobalSize(hMem);
            if (hMemSize >= sizeof(BITMAPINFOHEADER)) {
                BITMAPINFOHEADER* bmi = reinterpret_cast<BITMAPINFOHEADER*>(pData);

                int width = bmi->biWidth;
                int height = abs(bmi->biHeight);
                int bitCount = bmi->biBitCount;
                int imageSize = bmi->biSizeImage;

                // Verify the claimed image size fits in the buffer
                if (imageSize > 0 && sizeof(BITMAPINFOHEADER) + static_cast<SIZE_T>(imageSize) <= hMemSize) {
                    if (bitCount == 32) {
                        result.resize(imageSize);
                        memcpy(result.data(), pData + sizeof(BITMAPINFOHEADER), imageSize);

                        if (outWidth) *outWidth = width;
                        if (outHeight) *outHeight = height;
                    }
                }
            }

            GlobalUnlock(hMem);
        }
    }

    closeClipboard();
    return result;
}

bool Win32Clipboard::hasImage() {
    return IsClipboardFormatAvailable(CF_DIB) != FALSE ||
           IsClipboardFormatAvailable(CF_DIBV5) != FALSE;
}

// ========== File Operations ==========

bool Win32Clipboard::setFiles(const std::vector<std::string>& files) {
    if (files.empty()) return false;

    if (!openClipboard()) return false;

    // Calculate required memory size with overflow checking
    SIZE_T totalSize = sizeof(DROPFILES);
    for (const auto& file : files) {
        // Convert to wide character path
        int wideSize = MultiByteToWideChar(CP_UTF8, 0, file.c_str(), -1, nullptr, 0);
        if (wideSize <= 0) {
            closeClipboard();
            return false;
        }

        // Check for overflow
        if (totalSize > MAXSIZE - static_cast<SIZE_T>(wideSize * sizeof(wchar_t))) {
            spdlog::error("[Win32Clipboard] Total size overflow when calculating file list size");
            closeClipboard();
            return false;
        }
        totalSize += wideSize * sizeof(wchar_t);
    }
    // Null terminator
    if (totalSize > MAXSIZE - sizeof(wchar_t)) {
        spdlog::error("[Win32Clipboard] Total size overflow when adding null terminator");
        closeClipboard();
        return false;
    }
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
        pDropFiles->fWide = TRUE;

        wchar_t* pFiles = reinterpret_cast<wchar_t*>(reinterpret_cast<uint8_t*>(pDropFiles) + sizeof(DROPFILES));

        for (const auto& file : files) {
            int wideSize = MultiByteToWideChar(CP_UTF8, 0, file.c_str(), -1, pFiles, static_cast<int>(file.size()) + 1);
            pFiles += wideSize;
        }
        *pFiles = L'\0';

        GlobalUnlock(hMem);
    }

    BOOL result = SetClipboardData(CF_HDROP, hMem) != nullptr;
    closeClipboard();

    if (!result) {
        GlobalFree(hMem);
    }

    return result != FALSE;
}

std::vector<std::string> Win32Clipboard::getFiles() {
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
            // Get buffer size for bounds checking
            SIZE_T hMemSize = GlobalSize(hMem);
            if (hMemSize >= sizeof(DROPFILES) && pDropFiles->pFiles < hMemSize) {
                if (pDropFiles->fWide) {
                    wchar_t* pFiles = reinterpret_cast<wchar_t*>(
                        reinterpret_cast<uint8_t*>(pDropFiles) + pDropFiles->pFiles);

                    // Calculate remaining space in buffer
                    size_t remainingBytes = hMemSize - pDropFiles->pFiles;
                    size_t maxChars = remainingBytes / sizeof(wchar_t);
                    size_t charsProcessed = 0;

                    while (charsProcessed < maxChars && pFiles[charsProcessed] != L'\0') {
                        // Find null terminator within buffer bounds
                        size_t length = 0;
                        while ((charsProcessed + length) < maxChars && pFiles[charsProcessed + length] != L'\0') {
                            length++;
                        }

                        if (length > 0) {
                            int size = WideCharToMultiByte(CP_UTF8, 0, &pFiles[charsProcessed], static_cast<int>(length), nullptr, 0, nullptr, nullptr);
                            if (size > 0) {
                                std::string file(size, 0);
                                WideCharToMultiByte(CP_UTF8, 0, &pFiles[charsProcessed], static_cast<int>(length), &file[0], size, nullptr, nullptr);
                                result.push_back(file);
                            }
                            charsProcessed += length + 1; // Skip null terminator
                        } else {
                            break;
                        }
                    }
                } else {
                    char* pFiles = reinterpret_cast<char*>(
                        reinterpret_cast<uint8_t*>(pDropFiles) + pDropFiles->pFiles);

                    // Calculate remaining space in buffer
                    size_t remainingBytes = hMemSize - pDropFiles->pFiles;
                    size_t charsProcessed = 0;

                    while (charsProcessed < remainingBytes && pFiles[charsProcessed] != '\0') {
                        // Find null terminator within buffer bounds
                        size_t length = 0;
                        while ((charsProcessed + length) < remainingBytes && pFiles[charsProcessed + length] != '\0') {
                            length++;
                        }

                        if (length > 0) {
                            result.push_back(std::string(&pFiles[charsProcessed], length));
                            charsProcessed += length + 1; // Skip null terminator
                        } else {
                            break;
                        }
                    }
                }
            }

            GlobalUnlock(hMem);
        }
    }

    closeClipboard();
    return result;
}

bool Win32Clipboard::hasFiles() {
    return IsClipboardFormatAvailable(CF_HDROP) != FALSE;
}

// ========== General Operations ==========

void Win32Clipboard::clear() {
    if (OpenClipboard(nullptr)) {
        EmptyClipboard();
        CloseClipboard();
    }
}

bool Win32Clipboard::isEmpty() {
    return !IsClipboardFormatAvailable(CF_TEXT) &&
           !IsClipboardFormatAvailable(CF_UNICODETEXT) &&
           !IsClipboardFormatAvailable(CF_DIB) &&
           !IsClipboardFormatAvailable(CF_HDROP);
}

std::vector<ClipboardFormat> Win32Clipboard::getAvailableFormats() {
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

    // Check custom format
    static UINT cfHTML = RegisterClipboardFormatW(L"HTML Format");
    if (cfHTML != 0 && IsClipboardFormatAvailable(cfHTML)) {
        formats.push_back(ClipboardFormat::HTML);
    }

    closeClipboard();
    return formats;
}

BackendInfo Win32Clipboard::getBackendInfo() const {
    return BackendInfo{
        "Win32",
        "1.0",
        initialized_,
        "Windows Clipboard API"
    };
}

// ========== Private Methods ==========

bool Win32Clipboard::openClipboard() {
    if (!initialized_) return false;

    if (opened_) {
        return true;
    }

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

void Win32Clipboard::closeClipboard() {
    if (opened_) {
        CloseClipboard();
        opened_ = false;
    }
}

UINT Win32Clipboard::getHtmlFormat() {
    static UINT cfHTML = RegisterClipboardFormatW(L"HTML Format");
    return cfHTML;
}

} // namespace wingman::platform::windows

#endif // _WIN32
