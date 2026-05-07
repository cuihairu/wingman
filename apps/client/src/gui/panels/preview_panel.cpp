#include "wingman/client/gui/panels/preview_panel.hpp"
#include <spdlog/spdlog.h>
#include <imgui_internal.h>

namespace wingman::gui {

PreviewPanel::PreviewPanel()
    : autoRefresh_(false)
    , refreshInterval_(0.5f)
    , lastRefreshTime_(0)
    , colorTolerance_(10)
    , selectingRegion_(false)
    , config_(nullptr)
{
}

void PreviewPanel::setConfig(const GuiConfig* config) {
    config_ = config;
    // 应用配置中的刷新间隔
    if (config) {
        refreshInterval_ = config->previewRefreshInterval;
    }
}

void PreviewPanel::render(bool* show) {
    if (!ImGui::Begin("预览", show)) {

void PreviewPanel::render(bool* show) {
    if (!ImGui::Begin("预览", show)) {
        ImGui::End();
        return;
    }

    // 工具栏
    if (ImGui::Button("刷新屏幕")) {
        screenCapture_ = Screen::capture();
        lastRefreshTime_ = ImGui::GetTime();
    }
    ImGui::SameLine();
    ImGui::Checkbox("自动刷新", &autoRefresh_);

    if (autoRefresh_) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        ImGui::SliderFloat("间隔", &refreshInterval_, 0.1f, 5.0f, "%.1f 秒");

        float currentTime = ImGui::GetTime();
        if (currentTime - lastRefreshTime_ >= refreshInterval_) {
            screenCapture_ = Screen::capture();
            lastRefreshTime_ = currentTime;
        }
    }

    ImGui::Separator();

    // 标签页
    if (ImGui::BeginTabBar("PreviewTabs")) {
        if (ImGui::BeginTabItem("屏幕预览")) {
            renderScreenPreview();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("颜色选择")) {
            renderColorPicker();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("区域选择")) {
            renderRegionSelector();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

void PreviewPanel::renderScreenPreview() {
    if (screenCapture_) {
        int width = screenCapture_->getWidth();
        int height = screenCapture_->getHeight();

        // 计算显示大小（保持宽高比）
        ImVec2 avail = ImGui::GetContentRegionAvail();
        float scale = std::min(avail.x / width, avail.y / height);
        scale = std::min(scale, 1.0f); // 不放大

        int displayWidth = static_cast<int>(width * scale);
        int displayHeight = static_cast<int>(height * scale);

        // 显示图像
        ImGui::Image(
            screenCapture_.get(),
            ImVec2(static_cast<float>(displayWidth), static_cast<float>(displayHeight))
        );

        // 悬停显示像素信息
        if (ImGui::IsItemHovered()) {
            ImVec2 mousePos = ImGui::GetMousePos();
            ImVec2 itemPos = ImGui::GetItemRectMin();
            int x = static_cast<int>((mousePos.x - itemPos.x) / scale);
            int y = static_cast<int>((mousePos.y - itemPos.y) / scale);

            if (x >= 0 && x < width && y >= 0 && y < height) {
                Color color = screenCapture_->getPixel(x, y);
                ImGui::SetTooltip(
                    "位置: (%d, %d)\n"
                    "RGB: (%d, %d, %d)\n"
                    "HEX: 0x%02X%02X%02X",
                    x, y,
                    color.r, color.g, color.b,
                    color.r, color.g, color.b
                );

                // 点击选择颜色
                if (ImGui::IsMouseClicked(0)) {
                    selectedColor_ = color;
                    spdlog::info("Selected color at ({}, {}): RGB({}, {}, {})",
                        x, y, color.r, color.g, color.b);
                }
            }
        }
    } else {
        ImGui::Text("点击 "刷新屏幕" 开始预览");
    }
}

void PreviewPanel::renderColorPicker() {
    ImGui::Text("当前选中颜色:");
    ImGui::ColorButton("##SelectedColor",
        ImVec4(selectedColor_.r / 255.0f, selectedColor_.g / 255.0f,
               selectedColor_.b / 255.0f, selectedColor_.a / 255.0f),
        ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoTooltip,
        ImVec2(100, 100)
    );
    ImGui::SameLine();
    ImGui::Text(
        "R: %d\nG: %d\nB: %d\nA: %d\n\n0x%02X%02X%02X",
        selectedColor_.r, selectedColor_.g, selectedColor_.b, selectedColor_.a,
        selectedColor_.r, selectedColor_.g, selectedColor_.b
    );

    ImGui::Separator();

    ImGui::Text("颜色容差:");
    ImGui::SliderInt("##Tolerance", &colorTolerance_, 0, 100);

    ImGui::Separator();

    if (ImGui::Button("从屏幕选择")) {
        screenCapture_ = Screen::capture();
        ImGui::OpenPopup("ColorPickerPopup");
    }

    // 全屏取色器弹窗
    if (ImGui::BeginPopup("ColorPickerPopup")) {
        ImGui::Text("点击图像选择颜色");
        if (screenCapture_) {
            renderScreenPreview();
        } else {
            screenCapture_ = Screen::capture();
        }
        if (ImGui::Button("关闭")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void PreviewPanel::renderRegionSelector() {
    ImGui::Text("选择屏幕区域:");

    if (screenCapture_) {
        int width = screenCapture_->getWidth();
        int height = screenCapture_->getHeight();

        // 计算显示大小
        ImVec2 avail = ImGui::GetContentRegionAvail();
        float scale = std::min(avail.x / width, avail.y / height);
        scale = std::min(scale, 1.0f);

        int displayWidth = static_cast<int>(width * scale);
        int displayHeight = static_cast<int>(height * scale);

        // 显示图像
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        ImGui::Image(
            screenCapture_.get(),
            ImVec2(static_cast<float>(displayWidth), static_cast<float>(displayHeight))
        );

        // 绘制选择区域
        if (selectingRegion_) {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 p1(
                cursor.x + selectedRegion_.x * scale,
                cursor.y + selectedRegion_.y * scale
            );
            ImVec2 p2(
                cursor.x + (selectedRegion_.x + selectedRegion_.width) * scale,
                cursor.y + (selectedRegion_.y + selectedRegion_.height) * scale
            );
            drawList->AddRect(p1, p2, IM_COL32(255, 0, 0, 255), 0, 0, 2);
        }

        // 处理选择
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) {
            selectingRegion_ = true;
            ImVec2 mousePos = ImGui::GetMousePos();
            selectedRegion_.x = static_cast<int>((mousePos.x - cursor.x) / scale);
            selectedRegion_.y = static_cast<int>((mousePos.y - cursor.y) / scale);
            selectedRegion_.width = 0;
            selectedRegion_.height = 0;
        }

        if (selectingRegion_ && ImGui::IsMouseDragging(0)) {
            ImVec2 mousePos = ImGui::GetMousePos();
            int currentX = static_cast<int>((mousePos.x - cursor.x) / scale);
            int currentY = static_cast<int>((mousePos.y - cursor.y) / scale);
            selectedRegion_.width = currentX - selectedRegion_.x;
            selectedRegion_.height = currentY - selectedRegion_.y;
        }

        if (selectingRegion_ && ImGui::IsMouseReleased(0)) {
            selectingRegion_ = false;
            // 规范化区域（负宽度/高度）
            if (selectedRegion_.width < 0) {
                selectedRegion_.x += selectedRegion_.width;
                selectedRegion_.width = -selectedRegion_.width;
            }
            if (selectedRegion_.height < 0) {
                selectedRegion_.y += selectedRegion_.height;
                selectedRegion_.height = -selectedRegion_.height;
            }
            spdlog::info("Selected region: x={}, y={}, w={}, h={}",
                selectedRegion_.x, selectedRegion_.y,
                selectedRegion_.width, selectedRegion_.height);
        }
    } else {
        if (ImGui::Button("刷新屏幕")) {
            screenCapture_ = Screen::capture();
        }
    }

    if (selectedRegion_.width > 0 && selectedRegion_.height > 0) {
        ImGui::Text("区域: X=%d, Y=%d, 宽=%d, 高=%d",
            selectedRegion_.x, selectedRegion_.y,
            selectedRegion_.width, selectedRegion_.height);
    }
}

} // namespace wingman::gui
