#include "wingman/gui/imgui_backend.hpp"

#define IMGUI_IMPL_WIN32_NO_LIBRARIES
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>

#include <d3d12.h>
#include <dxgi1_4.h>
#include <tchar.h>

namespace wingman::gui {

// 全局窗口类名
static const wchar_t* WINDOW_CLASS_NAME = L"WingmanImGuiWindow";

// 窗口过程
static LRESULT WINAPI ImGuiWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return true;

    switch (msg) {
        case WM_SIZE:
            // 在应用程序中处理大小改变
            break;

        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
                return 0;
            break;

        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;

        case WM_DPICHANGED:
            if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DpiEnableScaleViewports) {
                const float dpi = HIWORD(wParam);
                ImGui::GetIO().DisplayFramebufferScale = ImVec2(dpi / 96.0f, dpi / 96.0f);
            }
            break;
    }
    return ::DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ========== ImGuiBackend 实现 ==========

ImGuiBackend::ImGuiBackend()
    : initialized_(false)
    , hwnd_(nullptr)
    , width_(0)
    , height_(0)
    , device_(nullptr)
    , srvDescriptorHeap_(nullptr)
    , commandQueue_(nullptr)
    , commandList_(nullptr)
    , frameIndex_(0)
    , mouseWheel_(0)
{
    memset(mouseButtons_, 0, sizeof(mouseButtons_));
    memset(commandAllocators_, 0, sizeof(commandAllocators_));
    memset(mainRenderTargetResource_, 0, sizeof(mainRenderTargetResource_));
    memset(&mainRenderTargetDescriptor_, 0, sizeof(mainRenderTargetDescriptor_));
}

ImGuiBackend::~ImGuiBackend() {
    shutdown();
}

bool ImGuiBackend::initialize(HWND hwnd, int width, int height) {
    if (initialized_) return true;

    hwnd_ = hwnd;
    width_ = width;
    height_ = height;

    // 创建 D3D12 设备
    if (!createDevice()) {
        return false;
    }

    // 创建渲染目标
    if (!createRenderTarget()) {
        cleanupDevice();
        return false;
    }

    // 初始化 ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    // 设置 ImGui 样式
    ImGui::StyleColorsDark();

    // 初始化平台/渲染器后端
    if (!ImGui_ImplWin32_Init(hwnd)) {
        shutdown();
        return false;
    }

    if (!ImGui_ImplDX12_Init(device_, 2,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            srvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart(),
            srvDescriptorHeap_->GetGPUDescriptorHandleForHeapStart())) {
        ImGui_ImplWin32_Shutdown();
        shutdown();
        return false;
    }

    initialized_ = true;
    return true;
}

void ImGuiBackend::shutdown() {
    if (!initialized_) return;

    // 清理 ImGui
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    // 清理 D3D 资源
    cleanupRenderTarget();
    cleanupDevice();

    initialized_ = false;
}

void ImGuiBackend::newFrame() {
    if (!initialized_) return;

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void ImGuiBackend::render() {
    if (!initialized_) return;

    ImGui::Render();

    // 获取当前命令分配器
    ID3D12CommandAllocator* commandAllocator = commandAllocators_[frameIndex_];

    // 重置命令列表
    commandAllocator->Reset();
    commandList_->Reset(commandAllocator, nullptr);

    // 设置资源屏障
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = mainRenderTargetResource_[frameIndex_];
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    commandList_->ResourceBarrier(1, &barrier);

    // 设置渲染目标
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = mainRenderTargetDescriptor_[frameIndex_];
    commandList_->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    // 清除渲染目标
    const float clear_color[] = { 0.1f, 0.1f, 0.1f, 1.0f };
    commandList_->ClearRenderTargetView(rtvHandle, clear_color, 0, nullptr);

    // 渲染 ImGui
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList_);

    // 设置资源屏障（Present）
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    commandList_->ResourceBarrier(1, &barrier);

    // 执行命令列表
    commandList_->Close();

    ID3D12CommandList* cmdLists[] = { commandList_ };
    commandQueue_->ExecuteCommandLists(1, cmdLists);

    // 呈现
    DXGI_PRESENT_PARAMETERS params = {};
    params.DirtyRectsCount = 0;
    params.pDirtyRects = nullptr;
    params.pScrollRect = nullptr;
    params.pScrollOffset = nullptr;

    // 获取 swap chain
    IDXGISwapChain3* swapChain = nullptr;
    if (SUCCEEDED(mainRenderTargetResource_[0]->QueryInterface(__uuidof(IDXGISwapChain3), (void**)&swapChain))) {
        swapChain->Present(1, 0);
        swapChain->Release();

        // 等待完成
        frameIndex_ = swapChain->GetCurrentBackBufferIndex();
    }
}

void ImGuiBackend::resize(int width, int height) {
    if (!initialized_) return;

    width_ = width;
    height_ = height;

    // 重新创建渲染目标
    cleanupRenderTarget();
    createRenderTarget();
}

void ImGuiBackend::handleMouseWheel(float delta) {
    mouseWheel_ += static_cast<int>(delta);
}

void ImGuiBackend::handleMouseButton(int button, bool down) {
    if (button >= 0 && button < 3) {
        mouseButtons_[button] = down ? 1 : 0;
    }
}

void ImGuiBackend::handleMousePos(int x, int y) {
    ImGui::GetIO().MousePos = ImVec2(static_cast<float>(x), static_cast<float>(y));
}

void ImGuiBackend::handleKey(unsigned int keycode, bool down) {
    ImGui::GetIO().AddKeyEvent(static_cast<ImGuiKey>(keycode), down);
}

void ImGuiBackend::handleChar(unsigned int c) {
    ImGui::GetIO().AddInputCharacter(c);
}

bool ImGuiBackend::wantsMouseCapture() const {
    return ImGui::GetIO().WantCaptureMouse;
}

bool ImGuiBackend::wantsKeyboardCapture() const {
    return ImGui::GetIO().WantCaptureKeyboard;
}

bool ImGuiBackend::createDevice() {
    DXGI_SWAP_CHAIN_DESC1 sd = {};
    sd.BufferCount = 2;
    sd.Width = width_;
    sd.Height = height_;
    sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.SampleDesc.Count = 1;

    // 创建设备
    D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device_));

    // 创建命令队列
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    device_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue_));

    // 创建命令分配器
    for (UINT i = 0; i < 2; i++) {
        device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocators_[i]));
    }

    // 创建命令列表
    device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocators_[0], nullptr, IID_PPV_ARGS(&commandList_));
    commandList_->Close();

    // 创建描述符堆
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = 2;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&srvDescriptorHeap_));

    return true;
}

void ImGuiBackend::cleanupDevice() {
    if (commandList_) { commandList_->Release(); commandList_ = nullptr; }
    for (UINT i = 0; i < 2; i++) {
        if (commandAllocators_[i]) { commandAllocators_[i]->Release(); commandAllocators_[i] = nullptr; }
    }
    if (commandQueue_) { commandQueue_->Release(); commandQueue_ = nullptr; }
    if (srvDescriptorHeap_) { srvDescriptorHeap_->Release(); srvDescriptorHeap_ = nullptr; }
    if (device_) { device_->Release(); device_ = nullptr; }
}

bool ImGuiBackend::createRenderTarget() {
    // 简化实现：创建纹理资源作为渲染目标
    for (UINT i = 0; i < 2; i++) {
        D3D12_RESOURCE_DESC texDesc = {};
        texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        texDesc.Width = width_;
        texDesc.Height = height_;
        texDesc.DepthOrArraySize = 1;
        texDesc.MipLevels = 1;
        texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texDesc.SampleDesc.Count = 1;
        texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        D3D12_HEAP_PROPERTIES heapProperties = {};
        heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

        device_->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &texDesc,
            D3D12_RESOURCE_STATE_PRESENT,
            nullptr,
            IID_PPV_ARGS(&mainRenderTargetResource_[i])
        );

        // 创建渲染目标视图
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Texture2D.MipSlice = 0;

        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = srvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
        rtvHandle.ptr += i * device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        device_->CreateRenderTargetView(mainRenderTargetResource_[i], &rtvDesc, rtvHandle);
        mainRenderTargetDescriptor_[i] = rtvHandle;
    }
    return true;
}

void ImGuiBackend::cleanupRenderTarget() {
    for (UINT i = 0; i < 2; i++) {
        if (mainRenderTargetResource_[i]) {
            mainRenderTargetResource_[i]->Release();
            mainRenderTargetResource_[i] = nullptr;
        }
    }
}

// ========== 辅助函数 ==========

HWND CreateImGuiWindow(const wchar_t* title, int width, int height, WNDPROC wndProc) {
    // 注册窗口类
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = wndProc ? wndProc : ImGuiWndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = WINDOW_CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);

    // 计算窗口大小
    RECT rect = { 0, 0, width, height };
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    // 创建窗口
    HWND hwnd = CreateWindowW(
        WINDOW_CLASS_NAME,
        title,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        GetModuleHandle(nullptr),
        nullptr
    );

    return hwnd;
}

void RunImGuiLoop(HWND hwnd, std::function<void()> renderCallback) {
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);

        if (renderCallback) {
            renderCallback();
        }
    }
}

} // namespace wingman::gui
