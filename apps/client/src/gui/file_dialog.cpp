#include "wingman/client/gui/file_dialog.hpp"
#include <Windows.h>
#include <comdef.h>
#include <shlobj.h>
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

namespace wingman::gui {

// 将过滤器字符串转换为 Windows 格式
static std::wstring convertFilter(const std::string& filter) {
    std::wstring result;
    size_t start = 0;
    while (start < filter.length()) {
        size_t end = filter.find('|', start);
        if (end == std::string::npos) break;

        // 描述
        std::string desc = filter.substr(start, end - start);
        result += std::wstring(desc.begin(), desc.end());
        result += L'\0';

        start = end + 1;
        end = filter.find('|', start);
        if (end == std::string::npos) {
            // 最后一个过滤器
            std::string ext = filter.substr(start);
            result += std::wstring(ext.begin(), ext.end());
        } else {
            std::string ext = filter.substr(start, end - start);
            result += std::wstring(ext.begin(), ext.end());
        }
        result += L'\0';
        start = end + 1;
    }
    result += L'\0';
    return result;
}

std::optional<std::string> OpenFileDialog(const std::string& title, const std::string& filter) {
    OPENFILENAMEW ofn = {};
    wchar_t szFile[260] = {0};

    std::wstring titleW(title.begin(), title.end());
    std::wstring filterW = convertFilter(filter);

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetActiveWindow();
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
    ofn.lpstrFilter = filterW.c_str();
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = nullptr;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = nullptr;
    ofn.lpstrTitle = titleW.c_str();
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameW(&ofn)) {
        return std::optional<std::string>(std::string(szFile, szFile + wcslen(szFile)));
    }
    return std::nullopt;
}

std::optional<std::string> SaveFileDialog(const std::string& title, const std::string& filter) {
    OPENFILENAMEW ofn = {};
    wchar_t szFile[260] = {0};

    std::wstring titleW(title.begin(), title.end());
    std::wstring filterW = convertFilter(filter);

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetActiveWindow();
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
    ofn.lpstrFilter = filterW.c_str();
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = nullptr;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = nullptr;
    ofn.lpstrTitle = titleW.c_str();
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetSaveFileNameW(&ofn)) {
        return std::optional<std::string>(std::string(szFile, szFile + wcslen(szFile)));
    }
    return std::nullopt;
}

std::optional<std::vector<std::string>> OpenFilesDialog(const std::string& title, const std::string& filter) {
    // 分配更大的缓冲区用于多文件选择
    const DWORD bufferSize = 65536;  // 64KB 应该足够了
    wchar_t* buffer = new wchar_t[bufferSize];
    std::fill(buffer, buffer + bufferSize, 0);

    OPENFILENAMEW ofn = {};
    std::wstring titleW(title.begin(), title.end());
    std::wstring filterW = convertFilter(filter);

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetActiveWindow();
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = bufferSize;
    ofn.lpstrFilter = filterW.c_str();
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = nullptr;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = nullptr;
    ofn.lpstrTitle = titleW.c_str();
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR | OFN_ALLOWMULTISELECT | OFN_EXPLORER;

    std::vector<std::string> result;
    if (GetOpenFileNameW(&ofn)) {
        // 解析多文件选择结果
        std::wstring directory = buffer;

        // 查找第一个空字符，即目录字符串的结尾
        size_t offset = wcslen(buffer);
        offset++;  // 跳过空字符

        // 如果下一个字符也是空字符，说明只选择了一个文件
        if (buffer[offset] == L'\0') {
            result.push_back(std::string(directory.begin(), directory.end()));
        } else {
            // 多个文件：目录 + 文件名1\0文件名2\0...\0\0
            while (buffer[offset] != L'\0') {
                std::wstring filename = buffer + offset;
                std::wstring fullPath = directory + L"\\" + filename;

                // 转换为 UTF-8
                int size = WideCharToMultiByte(CP_UTF8, 0, fullPath.c_str(), -1, nullptr, 0, nullptr, nullptr);
                if (size > 0) {
                    std::string utf8Path(size - 1, 0);
                    WideCharToMultiByte(CP_UTF8, 0, fullPath.c_str(), -1, &utf8Path[0], size, nullptr, nullptr);
                    result.push_back(utf8Path);
                }

                offset += wcslen(buffer + offset) + 1;
            }
        }
    }

    delete[] buffer;

    if (result.empty()) {
        return std::nullopt;
    }
    return result;
}

} // namespace wingman::gui
