#pragma once

#include "wingman/platform/iclipboard.hpp"
#include <memory>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

namespace wingman {

// 前向声明
namespace platform {
    class IClipboard;
}

/**
 * @brief 剪贴板管理类
 *
 * 提供跨平台的剪贴板访问接口，自动使用平台最佳实现。
 */
class Clipboard {
public:
    /**
     * @brief 获取剪贴板单例
     */
    static platform::IClipboard& instance();

    // ========== 便捷静态方法 ==========

    /**
     * @brief 设置剪贴板文本
     */
    static bool setText(const std::string& text);

    /**
     * @brief 获取剪贴板文本
     */
    static std::string getText();

    /**
     * @brief 检查剪贴板是否包含文本
     */
    static bool hasText();

    /**
     * @brief 设置剪贴板 HTML
     */
    static bool setHTML(const std::string& html);

    /**
     * @brief 获取剪贴板 HTML
     */
    static std::string getHTML();

    /**
     * @brief 检查剪贴板是否包含 HTML
     */
    static bool hasHTML();

    /**
     * @brief 设置剪贴板图像
     */
    static bool setImage(const std::vector<uint8_t>& imageData, int width, int height);

    /**
     * @brief 获取剪贴板图像
     */
    static std::vector<uint8_t> getImage(int* outWidth, int* outHeight);

    /**
     * @brief 检查剪贴板是否包含图像
     */
    static bool hasImage();

    /**
     * @brief 设置剪贴板文件列表
     */
    static bool setFiles(const std::vector<std::string>& files);

    /**
     * @brief 获取剪贴板文件列表
     */
    static std::vector<std::string> getFiles();

    /**
     * @brief 检查剪贴板是否包含文件
     */
    static bool hasFiles();

    /**
     * @brief 清空剪贴板
     */
    static void clear();

    /**
     * @brief 检查剪贴板是否为空
     */
    static bool isEmpty();
};

} // namespace wingman
