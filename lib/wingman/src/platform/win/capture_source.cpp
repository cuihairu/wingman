// Windows 平台的捕获源实现（P2 由 src/capture/capture_source.cpp 收回，
// 接口定义见 include/wingman/capture/capture_source.hpp）。

#include "capture_source.hpp"
#include <spdlog/spdlog.h>

namespace wingman::capture {

namespace {

struct WindowEnumData {
    std::string titlePattern;
    HWND result = nullptr;
};

BOOL CALLBACK enumWindowsCallback(HWND hwnd, LPARAM lParam) {
    auto* data = reinterpret_cast<WindowEnumData*>(lParam);

    char title[256];
    if (GetWindowTextA(hwnd, title, sizeof(title)) > 0) {
        std::string titleStr(title);
        if (titleStr.find(data->titlePattern) != std::string::npos) {
            data->result = hwnd;
            return FALSE;
        }
    }
    return TRUE;
}

BOOL CALLBACK listTopLevelWindowsCallback(HWND hwnd, LPARAM lParam) {
    auto* windows = reinterpret_cast<std::vector<HWND>*>(lParam);
    if (IsWindowVisible(hwnd)) {
        windows->push_back(hwnd);
    }
    return TRUE;
}

} // namespace

// ========== ScreenCaptureSource ==========

ScreenCaptureSource::ScreenCaptureSource(int monitorIndex)
    : monitorIndex_(monitorIndex) {
    setName("ScreenCaptureSource:" + std::to_string(monitorIndex));
}

bool ScreenCaptureSource::onInitialize() {
    return queryDisplayInfo();
}

bool ScreenCaptureSource::onStart() {
    available_ = queryDisplayInfo();
    return available_;
}

void ScreenCaptureSource::onStop() {
    available_ = false;
}

std::unique_ptr<Bitmap> ScreenCaptureSource::capture(const Rect& region) {
    if (!available_) {
        spdlog::warn("[ScreenCaptureSource] Source not available");
        return nullptr;
    }

    Rect captureRegion = region.isEmpty() ? bounds_ : region;

    HDC hdcScreen = GetDC(nullptr);
    if (!hdcScreen) {
        spdlog::error("[ScreenCaptureSource] Failed to get screen DC");
        return nullptr;
    }

    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    if (!hdcMem) {
        ReleaseDC(nullptr, hdcScreen);
        return nullptr;
    }

    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen,
        captureRegion.width, captureRegion.height);
    if (!hBitmap) {
        DeleteDC(hdcMem);
        ReleaseDC(nullptr, hdcScreen);
        return nullptr;
    }

    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);
    BitBlt(hdcMem, 0, 0, captureRegion.width, captureRegion.height,
           hdcScreen, captureRegion.x, captureRegion.y, SRCCOPY);
    SelectObject(hdcMem, hOldBitmap);

    auto bitmap = std::unique_ptr<Bitmap>(new Bitmap(captureRegion.width, captureRegion.height));

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = captureRegion.width;
    bmi.bmiHeader.biHeight = -captureRegion.height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    ::GetDIBits(hdcScreen, hBitmap, 0, captureRegion.height,
               bitmap->getData(), &bmi, DIB_RGB_COLORS);

    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);

    return bitmap;
}

Rect ScreenCaptureSource::getBounds() const {
    return bounds_;
}

bool ScreenCaptureSource::isAvailable() const {
    return available_;
}

std::string ScreenCaptureSource::getName() const {
    return "Screen:" + std::to_string(monitorIndex_);
}

bool ScreenCaptureSource::queryDisplayInfo() {
    DISPLAY_DEVICE dd = {};
    dd.cb = sizeof(dd);
    if (!EnumDisplayDevicesA(nullptr, monitorIndex_, &dd, 0)) {
        spdlog::error("[ScreenCaptureSource] Invalid monitor index: {}", monitorIndex_);
        return false;
    }

    DEVMODE dm = {};
    dm.dmSize = sizeof(dm);
    if (!EnumDisplaySettingsA(dd.DeviceName, ENUM_CURRENT_SETTINGS, &dm)) {
        spdlog::error("[ScreenCaptureSource] Failed to query display settings");
        return false;
    }

    bounds_ = Rect{0, 0, static_cast<int>(dm.dmPelsWidth), static_cast<int>(dm.dmPelsHeight)};
    return true;
}

int ScreenCaptureSource::getMonitorCount() {
    int count = 0;
    DISPLAY_DEVICE dd = {};
    dd.cb = sizeof(dd);
    while (EnumDisplayDevicesA(nullptr, count, &dd, 0)) {
        count++;
    }
    return count;
}

std::shared_ptr<ScreenCaptureSource> ScreenCaptureSource::getPrimaryScreen() {
    return std::shared_ptr<ScreenCaptureSource>(new ScreenCaptureSource(0));
}

// ========== WindowCaptureSource ==========

WindowCaptureSource::WindowCaptureSource(HWND hwnd)
    : hwnd_(hwnd) {
    setName("WindowCaptureSource");
}

bool WindowCaptureSource::onInitialize() {
    return queryWindowInfo();
}

bool WindowCaptureSource::onStart() {
    available_ = queryWindowInfo();
    return available_;
}

void WindowCaptureSource::onStop() {
    available_ = false;
}

std::unique_ptr<Bitmap> WindowCaptureSource::capture(const Rect& region) {
    if (!available_ || !IsWindow(hwnd_)) {
        spdlog::warn("[WindowCaptureSource] Window not available");
        return nullptr;
    }

    RECT windowRect;
    GetWindowRect(hwnd_, &windowRect);

    Rect captureRegion = region.isEmpty() ?
        Rect{0, 0, windowRect.right - windowRect.left, windowRect.bottom - windowRect.top} :
        region;

    HDC hdcWindow = GetDC(hwnd_);
    if (!hdcWindow) {
        spdlog::error("[WindowCaptureSource] Failed to get window DC");
        return nullptr;
    }

    HDC hdcMem = CreateCompatibleDC(hdcWindow);
    if (!hdcMem) {
        ReleaseDC(hwnd_, hdcWindow);
        return nullptr;
    }

    HBITMAP hBitmap = CreateCompatibleBitmap(hdcWindow,
        captureRegion.width, captureRegion.height);
    if (!hBitmap) {
        DeleteDC(hdcMem);
        ReleaseDC(hwnd_, hdcWindow);
        return nullptr;
    }

    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);

    PrintWindow(hwnd_, hdcMem, 0);

    SelectObject(hdcMem, hOldBitmap);

    auto bitmap = std::unique_ptr<Bitmap>(new Bitmap(captureRegion.width, captureRegion.height));

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = captureRegion.width;
    bmi.bmiHeader.biHeight = -captureRegion.height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    ::GetDIBits(hdcWindow, hBitmap, 0, captureRegion.height,
               bitmap->getData(), &bmi, DIB_RGB_COLORS);

    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(hwnd_, hdcWindow);

    return bitmap;
}

Rect WindowCaptureSource::getBounds() const {
    return bounds_;
}

bool WindowCaptureSource::isAvailable() const {
    return available_ && IsWindow(hwnd_);
}

std::string WindowCaptureSource::getName() const {
    return "Window:" + windowTitle_;
}

bool WindowCaptureSource::queryWindowInfo() {
    if (!IsWindow(hwnd_)) {
        return false;
    }

    windowTitle_ = getWindowTitle(hwnd_);

    RECT rect;
    GetWindowRect(hwnd_, &rect);
    bounds_ = Rect{
        rect.left,
        rect.top,
        rect.right - rect.left,
        rect.bottom - rect.top
    };

    available_ = true;
    return true;
}

std::string WindowCaptureSource::getWindowTitle(HWND hwnd) {
    char title[256];
    if (GetWindowTextA(hwnd, title, sizeof(title)) > 0) {
        return std::string(title);
    }
    return "Unknown";
}

std::unique_ptr<WindowCaptureSource> WindowCaptureSource::findByTitle(const std::string& title) {
    WindowEnumData data;
    data.titlePattern = title;
    EnumWindows(enumWindowsCallback, reinterpret_cast<LPARAM>(&data));

    if (data.result) {
        return std::unique_ptr<WindowCaptureSource>(
            new WindowCaptureSource(data.result));
    }
    return nullptr;
}

std::unique_ptr<WindowCaptureSource> WindowCaptureSource::findByClassName(const std::string& className) {
    HWND hwnd = FindWindowA(className.c_str(), nullptr);
    if (hwnd) {
        return std::unique_ptr<WindowCaptureSource>(
            new WindowCaptureSource(hwnd));
    }
    return nullptr;
}

std::vector<HWND> WindowCaptureSource::listTopLevelWindows() {
    std::vector<HWND> windows;
    EnumWindows(listTopLevelWindowsCallback, reinterpret_cast<LPARAM>(&windows));
    return windows;
}

// ========== CaptureSourceManager ==========

CaptureSourceManager& CaptureSourceManager::instance() {
    static CaptureSourceManager instance;
    return instance;
}

void CaptureSourceManager::registerSource(std::shared_ptr<ICaptureSource> source) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string name = source->getName();
    sources_[name] = source;
    spdlog::info("[CaptureSourceManager] Registered source: {}", name);
}

void CaptureSourceManager::removeSource(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    sources_.erase(name);
    spdlog::info("[CaptureSourceManager] Removed source: {}", name);
}

std::shared_ptr<ICaptureSource> CaptureSourceManager::getSource(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sources_.find(name);
    return it != sources_.end() ? it->second : nullptr;
}

std::shared_ptr<ScreenCaptureSource> CaptureSourceManager::getPrimaryScreen() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string name = "Screen:0";
    auto it = sources_.find(name);
    if (it != sources_.end()) {
        return std::dynamic_pointer_cast<ScreenCaptureSource>(it->second);
    }

    auto source = ScreenCaptureSource::getPrimaryScreen();
    source->initialize();
    sources_[name] = source;
    return source;
}

std::shared_ptr<ScreenCaptureSource> CaptureSourceManager::getScreenSource(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sources_.find(name);
    if (it != sources_.end()) {
        return std::dynamic_pointer_cast<ScreenCaptureSource>(it->second);
    }
    return nullptr;
}

std::shared_ptr<WindowCaptureSource> CaptureSourceManager::getWindowSource(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sources_.find(name);
    if (it != sources_.end()) {
        return std::dynamic_pointer_cast<WindowCaptureSource>(it->second);
    }
    return nullptr;
}

std::vector<std::shared_ptr<ICaptureSource>> CaptureSourceManager::listSources() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::shared_ptr<ICaptureSource>> result;
    for (const auto& [name, source] : sources_) {
        result.push_back(source);
    }
    return result;
}

void CaptureSourceManager::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    sources_.clear();
}

std::string CaptureSourceManager::generateName(const std::string& prefix) {
    return prefix + "_" + std::to_string(nextId_++);
}

} // namespace wingman::capture
