#pragma once

#include "wingman/screen.hpp"
#include "wingman/core/component.hpp"
#include <memory>
#include <vector>

namespace wingman::capture {

/**
 * @brief 捕获源抽象接口
 *
 * 支持不同的捕获源：屏幕、窗口、相机等
 */
class ICaptureSource {
public:
    virtual ~ICaptureSource() = default;

    /**
     * @brief 捕获画面
     * @param region 捕获区域（空则捕获全部）
     * @return 捕获的位图
     */
    virtual std::unique_ptr<Bitmap> capture(const Rect& region = {}) = 0;

    /**
     * @brief 获取边界
     * @return 捕获源的边界矩形
     */
    virtual Rect getBounds() const = 0;

    /**
     * @brief 检查可用性
     * @return 可用返回 true
     */
    virtual bool isAvailable() const = 0;

    /**
     * @brief 获取名称
     * @return 捕获源名称
     */
    virtual std::string getName() const = 0;

    /**
     * @brief 获取宽度
     */
    virtual int getWidth() const { return getBounds().width; }

    /**
     * @brief 获取高度
     */
    virtual int getHeight() const { return getBounds().height; }
};

/**
 * @brief 屏幕捕获源
 *
 * 捕获整个显示器或指定显示器
 */
class ScreenCaptureSource : public core::ComponentBase, public ICaptureSource {
public:
    /**
     * @brief 构造函数
     * @param monitorIndex 显示器索引（0 为主显示器）
     */
    explicit ScreenCaptureSource(int monitorIndex = 0);

    ~ScreenCaptureSource() override = default;

    // ICaptureSource 接口
    std::unique_ptr<Bitmap> capture(const Rect& region = {}) override;
    Rect getBounds() const override;
    bool isAvailable() const override;
    std::string getName() const override;

    // ComponentBase 接口
    std::string getName() const override {
        return ICaptureSource::getName();
    }

    /**
     * @brief 获取显示器索引
     */
    int getMonitorIndex() const { return monitorIndex_; }

    /**
     * @brief 获取显示器数量
     */
    static int getMonitorCount();

    /**
     * @brief 获取主显示器捕获源
     */
    static std::shared_ptr<ScreenCaptureSource> getPrimaryScreen();

protected:
    bool onInitialize() override;
    bool onStart() override;
    void onStop() override;

private:
    int monitorIndex_ = 0;
    Rect bounds_;
    bool available_ = false;

    bool queryDisplayInfo();
};

/**
 * @brief 窗口捕获源
 *
 * 捕获特定窗口的内容
 */
class WindowCaptureSource : public core::ComponentBase, public ICaptureSource {
public:
    /**
     * @brief 构造函数
     * @param hwnd 窗口句柄
     */
    explicit WindowCaptureSource(HWND hwnd);

    ~WindowCaptureSource() override = default;

    // ICaptureSource 接口
    std::unique_ptr<Bitmap> capture(const Rect& region = {}) override;
    Rect getBounds() const override;
    bool isAvailable() const override;
    std::string getName() const override;

    // ComponentBase 接口
    std::string getName() const override {
        return ICaptureSource::getName();
    }

    /**
     * @brief 获取窗口句柄
     */
    HWND getHwnd() const { return hwnd_; }

    /**
     * @brief 获取窗口标题
     */
    const std::string& getWindowTitle() const { return windowTitle_; }

    /**
     * @brief 按标题查找窗口
     * @param title 窗口标题（支持部分匹配）
     * @return 窗口捕获源，未找到返回 nullptr
     */
    static std::unique_ptr<WindowCaptureSource> findByTitle(const std::string& title);

    /**
     * @brief 按类名查找窗口
     * @param className 窗口类名
     * @return 窗口捕获源，未找到返回 nullptr
     */
    static std::unique_ptr<WindowCaptureSource> findByClassName(const std::string& className);

    /**
     * @brief 列出所有顶层窗口
     * @return 窗口句柄列表
     */
    static std::vector<HWND> listTopLevelWindows();

protected:
    bool onInitialize() override;
    bool onStart() override;
    void onStop() override;

private:
    HWND hwnd_ = nullptr;
    std::string windowTitle_;
    Rect bounds_;
    bool available_ = false;

    bool queryWindowInfo();
    static std::string getWindowTitle(HWND hwnd);
};

/**
 * @brief 捕获源管理器
 *
 * 管理所有注册的捕获源
 */
class CaptureSourceManager {
public:
    static CaptureSourceManager& instance();

    // 禁止拷贝
    CaptureSourceManager(const CaptureSourceManager&) = delete;
    CaptureSourceManager& operator=(const CaptureSourceManager&) = delete;

    /**
     * @brief 注册捕获源
     * @param source 捕获源
     */
    void registerSource(std::shared_ptr<ICaptureSource> source);

    /**
     * @brief 移除捕获源
     * @param name 捕获源名称
     */
    void removeSource(const std::string& name);

    /**
     * @brief 获取捕获源
     * @param name 捕获源名称
     * @return 捕获源指针，不存在返回 nullptr
     */
    std::shared_ptr<ICaptureSource> getSource(const std::string& name) const;

    /**
     * @brief 获取默认屏幕捕获源
     * @return 主显示器捕获源
     */
    std::shared_ptr<ScreenCaptureSource> getPrimaryScreen();

    /**
     * @brief 按名称获取屏幕捕获源
     */
    std::shared_ptr<ScreenCaptureSource> getScreenSource(const std::string& name);

    /**
     * @brief 按名称获取窗口捕获源
     */
    std::shared_ptr<WindowCaptureSource> getWindowSource(const std::string& name);

    /**
     * @brief 列出所有捕获源
     * @return 捕获源列表
     */
    std::vector<std::shared_ptr<ICaptureSource>> listSources() const;

    /**
     * @brief 列出指定类型的捕获源
     */
    template<typename T>
    std::vector<std::shared_ptr<T>> listSourcesByType() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::shared_ptr<T>> result;
        for (const auto& [name, source] : sources_) {
            auto typed = std::dynamic_pointer_cast<T>(source);
            if (typed) {
                result.push_back(typed);
            }
        }
        return result;
    }

    /**
     * @brief 清空所有捕获源
     */
    void clear();

    /**
     * @brief 获取捕获源数量
     */
    size_t getSourceCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return sources_.size();
    }

private:
    CaptureSourceManager() = default;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<ICaptureSource>> sources_;

    std::string generateName(const std::string& prefix);
    uint32_t nextId_ = 1;
};

} // namespace wingman::capture
