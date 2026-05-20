#pragma once

#include "wingman/platform/platform_types.hpp"
#include <string>
#include <vector>
#include <cstdint>

namespace wingman::platform {

/**
 * @brief 剪贴板格式
 */
enum class ClipboardFormat : uint32_t {
    Text,           // 纯文本 (CF_TEXT)
    UnicodeText,    // Unicode 文本 (CF_UNICODETEXT)
    HTML,           // HTML 格式
    Image,          // 图像 (DIB/Bitmap)
    Files,          // 文件列表 (CF_HDROP)
    Custom          // 自定义格式
};

/**
 * @brief 剪贴板数据
 */
struct ClipboardData {
    ClipboardFormat format = ClipboardFormat::Text;
    std::vector<uint8_t> data;
    std::string text;           // 文本内容（方便访问）
    std::string customFormat;   // 自定义格式名称

    ClipboardData() = default;
    explicit ClipboardData(const std::string& text)
        : format(ClipboardFormat::UnicodeText), text(text) {
        data.assign(text.begin(), text.end());
    }
};

/**
 * @brief 剪贴板接口
 *
 * 提供剪贴板读写功能，支持文本、HTML、图像、文件等格式。
 */
class IClipboard {
public:
    virtual ~IClipboard() = default;

    // ========== 初始化 ==========

    /**
     * @brief 初始化剪贴板管理器
     */
    virtual bool initialize() = 0;

    /**
     * @brief 关闭剪贴板管理器
     */
    virtual void shutdown() = 0;

    // ========== 文本操作 ==========

    /**
     * @brief 设置剪贴板文本
     * @param text 文本内容
     * @return 成功返回 true
     */
    virtual bool setText(const std::string& text) = 0;

    /**
     * @brief 获取剪贴板文本
     * @return 文本内容，失败返回空字符串
     */
    virtual std::string getText() = 0;

    /**
     * @brief 检查剪贴板是否包含文本
     * @return 包含文本返回 true
     */
    virtual bool hasText() = 0;

    // ========== HTML 操作 ==========

    /**
     * @brief 设置剪贴板 HTML 内容
     * @param html HTML 内容
     * @return 成功返回 true
     */
    virtual bool setHTML(const std::string& html) = 0;

    /**
     * @brief 获取剪贴板 HTML 内容
     * @return HTML 内容，失败返回空字符串
     */
    virtual std::string getHTML() = 0;

    /**
     * @brief 检查剪贴板是否包含 HTML
     * @return 包含 HTML 返回 true
     */
    virtual bool hasHTML() = 0;

    // ========== 图像操作 ==========

    /**
     * @brief 设置剪贴板图像
     * @param imageData 图像数据 (BGRA 格式，32位)
     * @param width 图像宽度
     * @param height 图像高度
     * @return 成功返回 true
     */
    virtual bool setImage(const std::vector<uint8_t>& imageData, int width, int height) = 0;

    /**
     * @brief 获取剪贴板图像
     * @param outWidth 输出图像宽度
     * @param outHeight 输出图像高度
     * @return 图像数据 (BGRA 格式，32位)，失败返回空 vector
     */
    virtual std::vector<uint8_t> getImage(int* outWidth, int* outHeight) = 0;

    /**
     * @brief 检查剪贴板是否包含图像
     * @return 包含图像返回 true
     */
    virtual bool hasImage() = 0;

    // ========== 文件操作 ==========

    /**
     * @brief 设置剪贴板文件列表
     * @param files 文件路径列表
     * @return 成功返回 true
     */
    virtual bool setFiles(const std::vector<std::string>& files) = 0;

    /**
     * @brief 获取剪贴板文件列表
     * @return 文件路径列表，失败返回空 vector
     */
    virtual std::vector<std::string> getFiles() = 0;

    /**
     * @brief 检查剪贴板是否包含文件
     * @return 包含文件返回 true
     */
    virtual bool hasFiles() = 0;

    // ========== 通用操作 ==========

    /**
     * @brief 清空剪贴板
     */
    virtual void clear() = 0;

    /**
     * @brief 检查剪贴板是否为空
     * @return 为空返回 true
     */
    virtual bool isEmpty() = 0;

    /**
     * @brief 获取可用格式列表
     * @return 格式列表
     */
    virtual std::vector<ClipboardFormat> getAvailableFormats() = 0;

    // ========== 后端信息 ==========

    /**
     * @brief 获取后端名称
     */
    virtual std::string getBackendName() const = 0;

    /**
     * @brief 获取后端信息
     */
    virtual BackendInfo getBackendInfo() const = 0;
};

} // namespace wingman::platform
