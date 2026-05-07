#pragma once

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <functional>
#include <memory>

namespace wingman::gui {

// ImGui 后端状态
class ImGuiBackend {
public:
    ImGuiBackend();
    ~ImGuiBackend();

    // 禁止拷贝和移动
    ImGuiBackend(const ImGuiBackend&) = delete;
    ImGuiBackend& operator=(const ImGuiBackend&) = delete;
    ImGuiBackend(ImGuiBackend&&) = delete;
    ImGuiBackend& operator=(ImGuiBackend&&) = delete;

    // 初始化和清理
    bool initialize(HWND hwnd, int width, int height);
    void shutdown();

    // 渲染
    void newFrame();
    void render();

    // 窗口大小改变
    void resize(int width, int height);

    // 输入处理
    void handleMouseWheel(float delta);
    void handleMouseButton(int button, bool down);
    void handleMousePos(int x, int y);
    void handleKey(unsigned int keycode, bool down);
    void handleChar(unsigned int c);

    // 状态查询
    bool isInitialized() const { return initialized_; }
    bool wantsMouseCapture() const;
    bool wantsKeyboardCapture() const;

private:
    bool createRenderTarget();
    void cleanupRenderTarget();
    bool createDevice();
    void cleanupDevice();

    bool initialized_;
    HWND hwnd_;
    int width_;
    int height_;

    // D3D12 资源
    ID3D12Device* device_;
    ID3D12DescriptorHeap* srvDescriptorHeap_;
    ID3D12CommandQueue* commandQueue_;
    ID3D12CommandAllocator* commandAllocators_[2];
    ID3D12GraphicsCommandList* commandList_;
    ID3D12Resource* mainRenderTargetResource_[2];
    D3D12_CPU_DESCRIPTOR_HANDLE mainRenderTargetDescriptor_[2];
    UINT frameIndex_;

    // 输入状态
    int mouseButtons_[3];
    int mouseWheel_;
};

// ImGui 后端辅助函数 - 创建窗口
HWND CreateImGuiWindow(const wchar_t* title, int width, int height, WNDPROC wndProc = nullptr);

// ImGui 后端辅助函数 - 消息循环
void RunImGuiLoop(HWND hwnd, std::function<void()> renderCallback);

} // namespace wingman::gui
