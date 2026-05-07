#pragma once

#include "wingman/screen.hpp"
#include "wingman/gui/config.hpp"
#include <imgui.h>
#include <string>
#include <memory>

namespace wingman::gui {

// 屏幕预览面板
class PreviewPanel {
public:
    PreviewPanel();
    ~PreviewPanel() = default;

    void render(bool* show);

    // 设置配置指针（用于获取默认值）
    void setConfig(const GuiConfig* config) { config_ = config; }

private:
    void renderScreenPreview();
    void renderColorPicker();
    void renderRegionSelector();

    // 屏幕捕获
    std::unique_ptr<Bitmap> screenCapture_;
    bool autoRefresh_;
    float refreshInterval_;
    float lastRefreshTime_;

    // 颜色选择
    Color selectedColor_;
    int colorTolerance_;

    // 区域选择
    Rect selectedRegion_;
    bool selectingRegion_;

    // 配置引用
    const GuiConfig* config_;
};

} // namespace wingman::gui
