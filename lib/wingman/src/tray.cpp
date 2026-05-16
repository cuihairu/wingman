#include "wingman/tray.hpp"

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include <map>
#include <mutex>
#include <atomic>
#include <iostream>
#include <sstream>

// GDI+ 用于图标灰度转换
// 必须在 gdiplus.h 之前包含这些头文件
#include <objbase.h>
#include <comdef.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "ole32.lib")

namespace wingman {

// WM_APP 消息范围用于托盘图标
#define WM_TRAYICON (WM_APP + 1)
#define ID_TRAY_FIRST 1000

// 全局窗口类注册标志
static bool g_trayClassRegistered = false;
static std::mutex g_classRegistrationMutex;

// GDI+ 初始化
static ULONG_PTR g_gdiplusToken = 0;
static bool g_gdiplusInitialized = false;

static void initializeGdiPlus() {
    if (!g_gdiplusInitialized) {
        Gdiplus::GdiplusStartupInput gdiplusStartupInput;
        Gdiplus::GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, nullptr);
        g_gdiplusInitialized = true;
    }
}

// 将 HICON 转换为灰度
static HICON convertToGrayscale(HICON hIcon) {
    if (!hIcon) return nullptr;

    initializeGdiPlus();

    ICONINFO iconInfo;
    if (!GetIconInfo(hIcon, &iconInfo)) return hIcon;

    // 获取图标尺寸
    BITMAP bm;
    GetObject(iconInfo.hbmColor, sizeof(BITMAP), &bm);
    int width = bm.bmWidth;
    int height = bm.bmHeight;

    // 创建 GDI+ Bitmap 从 HBITMAP
    Gdiplus::Bitmap* pBitmap = Gdiplus::Bitmap::FromHBITMAP(iconInfo.hbmColor, nullptr);
    if (!pBitmap) {
        DeleteObject(iconInfo.hbmColor);
        DeleteObject(iconInfo.hbmMask);
        return hIcon;
    }

    // 创建灰度版本的 Bitmap
    Gdiplus::Bitmap grayscaleBitmap(width, height, PixelFormat32bppARGB);

    // 处理每个像素，转换为灰度
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            Gdiplus::Color color;
            pBitmap->GetPixel(x, y, &color);

            // 使用亮度公式转换为灰度
            BYTE gray = (BYTE)(0.299 * color.GetR() + 0.587 * color.GetG() + 0.114 * color.GetB());
            Gdiplus::Color grayColor(color.GetA(), gray, gray, gray);
            grayscaleBitmap.SetPixel(x, y, grayColor);
        }
    }

    // 转换回 HICON
    HBITMAP hGrayscaleBitmap = nullptr;
    Gdiplus::Status status = grayscaleBitmap.GetHBITMAP(Gdiplus::Color(0, 0, 0), &hGrayscaleBitmap);

    HICON hGrayIcon = nullptr;
    if (status == Gdiplus::Ok) {
        ICONINFO ii;
        ii.fIcon = TRUE;
        ii.hbmMask = iconInfo.hbmMask;
        ii.hbmColor = hGrayscaleBitmap;
        ii.xHotspot = 0;
        ii.yHotspot = 0;
        hGrayIcon = CreateIconIndirect(&ii);
    }

    delete pBitmap;
    DeleteObject(iconInfo.hbmColor);
    DeleteObject(iconInfo.hbmMask);

    return hGrayIcon ? hGrayIcon : hIcon;
}

class TrayIcon::Impl {
public:
    std::string tooltip_;
    std::string iconPath_;
    HWND hwnd_;
    NOTIFYICONDATAW nid_;
    std::vector<TrayItem> items_;
    std::mutex itemsMutex_;
    std::atomic<bool> visible_{false};
    UINT nextId_ = ID_TRAY_FIRST;

    // 配置支持
    TrayIcon::ActionHandler actionHandler_;
    std::map<std::string, TrayMenuItemConfig> itemConfigs_;  // 菜单项配置映射

    // 图标状态支持
    TrayIconState currentState_ = TrayIconState::normal;
    std::map<TrayIconState, std::string> stateIcons_;  // 状态对应的图标路径
    std::map<TrayIconState, HICON> stateIconHandles_;   // 状态对应的图标句柄
    HICON originalIcon_ = nullptr;  // 原始图标句柄
    std::mutex stateMutex_;

    Impl(const std::string& tooltip) : tooltip_(tooltip), hwnd_(nullptr) {
        // 注册窗口类（只注册一次）
        {
            std::lock_guard<std::mutex> lock(g_classRegistrationMutex);
            if (!g_trayClassRegistered) {
                WNDCLASSEXA wc = {0};
                wc.cbSize = sizeof(WNDCLASSEXA);
                wc.lpfnWndProc = WindowProc;
                wc.hInstance = GetModuleHandle(nullptr);
                wc.lpszClassName = "WingmanTrayClass";

                if (RegisterClassExA(&wc)) {
                    g_trayClassRegistered = true;
                } else {
                    // 窗口类可能已经注册
                    if (GetLastError() == ERROR_CLASS_ALREADY_EXISTS) {
                        g_trayClassRegistered = true;
                    }
                }
            }
        }

        // 创建消息窗口
        hwnd_ = CreateWindowExA(
            0,
            "WingmanTrayClass",
            "Wingman Tray",
            0,
            0, 0, 0, 0,
            HWND_MESSAGE,
            nullptr,
            GetModuleHandle(nullptr),
            this
        );

        if (!hwnd_) {
            std::cerr << "Failed to create tray window: " << GetLastError() << std::endl;
            return;
        }

        // 初始化 NOTIFYICONDATA (使用宽字符版本)
        memset(&nid_, 0, sizeof(nid_));
        nid_.cbSize = sizeof(NOTIFYICONDATAW);
        nid_.hWnd = hwnd_;
        nid_.uID = 1;
        nid_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nid_.uCallbackMessage = WM_TRAYICON;

        // 尝试从 exe 资源加载图标
        HICON hIcon = nullptr;
        HMODULE hModule = GetModuleHandle(nullptr);

        // 尝试加载资源图标 (IDI_ICON1)
        hIcon = static_cast<HICON>(LoadImageA(
            hModule,
            MAKEINTRESOURCEA(1),  // IDI_ICON1
            IMAGE_ICON,
            GetSystemMetrics(SM_CXICON),
            GetSystemMetrics(SM_CYICON),
            0
        ));

        // 如果资源图标加载失败，使用系统信息图标
        if (!hIcon) {
            hIcon = LoadIcon(nullptr, IDI_INFORMATION);
        }

        nid_.hIcon = hIcon;

        // 转换 tooltip 为宽字符
        MultiByteToWideChar(CP_UTF8, 0, tooltip.c_str(), -1,
                           nid_.szTip, sizeof(nid_.szTip) / sizeof(wchar_t));
    }

    ~Impl() {
        hide();

        // 释放缓存的图标
        {
            std::lock_guard<std::mutex> lock(stateMutex_);
            HICON defaultIcon = LoadIcon(nullptr, IDI_INFORMATION);
            for (auto& pair : stateIconHandles_) {
                if (pair.second && pair.second != defaultIcon) {
                    DestroyIcon(pair.second);
                }
            }
            stateIconHandles_.clear();
        }

        if (hwnd_) {
            DestroyWindow(hwnd_);
        }
    }

    void setIcon(const std::string& iconPath) {
        iconPath_ = iconPath;
        HICON hIcon = static_cast<HICON>(LoadImageA(
            nullptr,
            iconPath.c_str(),
            IMAGE_ICON,
            GetSystemMetrics(SM_CXICON),
            GetSystemMetrics(SM_CYICON),
            LR_LOADFROMFILE
        ));

        if (hIcon) {
            // 释放旧图标（如果是系统默认图标则不释放）
            if (nid_.hIcon) {
                HICON defaultIcon = LoadIcon(nullptr, IDI_INFORMATION);
                if (nid_.hIcon != defaultIcon) {
                    DestroyIcon(nid_.hIcon);
                }
            }
            nid_.hIcon = hIcon;
            nid_.uFlags |= NIF_ICON;
            if (visible_) {
                Shell_NotifyIconW(NIM_MODIFY, &nid_);
            }

            // 保存为原始图标
            {
                std::lock_guard<std::mutex> lock(stateMutex_);
                originalIcon_ = hIcon;
                stateIconHandles_[TrayIconState::normal] = hIcon;
            }
        }
    }

    void setTooltip(const std::string& tooltip) {
        tooltip_ = tooltip;
        MultiByteToWideChar(CP_UTF8, 0, tooltip.c_str(), -1,
                           nid_.szTip, sizeof(nid_.szTip) / sizeof(wchar_t));
        nid_.uFlags |= NIF_TIP;
        if (visible_) {
            Shell_NotifyIconW(NIM_MODIFY, &nid_);
        }
    }

    // 设置指定状态的图标路径
    void setStateIcon(TrayIconState state, const std::string& iconPath) {
        std::lock_guard<std::mutex> lock(stateMutex_);
        stateIcons_[state] = iconPath;
    }

    // 切换图标状态
    void setIconState(TrayIconState state) {
        std::lock_guard<std::mutex> lock(stateMutex_);
        currentState_ = state;

        HICON hIcon = nullptr;

        // 查找对应状态的图标句柄（已缓存）
        auto handleIt = stateIconHandles_.find(state);
        if (handleIt != stateIconHandles_.end() && handleIt->second) {
            hIcon = handleIt->second;
        } else {
            // 尝试从文件加载
            auto pathIt = stateIcons_.find(state);
            if (pathIt != stateIcons_.end() && !pathIt->second.empty()) {
                hIcon = static_cast<HICON>(LoadImageA(
                    nullptr,
                    pathIt->second.c_str(),
                    IMAGE_ICON,
                    GetSystemMetrics(SM_CXICON),
                    GetSystemMetrics(SM_CYICON),
                    LR_LOADFROMFILE
                ));
                if (hIcon) {
                    stateIconHandles_[state] = hIcon;
                }
            }

            // 如果是 idle 状态且没有找到图标，自动生成灰度版本
            if (!hIcon && state == TrayIconState::idle && originalIcon_) {
                std::cout << "[TRAY] 自动生成灰度图标\n";
                std::cout.flush();
                hIcon = convertToGrayscale(originalIcon_);
                if (hIcon && hIcon != originalIcon_) {
                    stateIconHandles_[state] = hIcon;
                }
            }

            // 保存原始图标（normal 状态）
            if (state == TrayIconState::normal && hIcon) {
                originalIcon_ = hIcon;
            }
        }

        if (hIcon) {
            // 释放旧图标（但不释放缓存的图标）
            if (nid_.hIcon) {
                // 检查是否是缓存的图标，如果不是则释放
                bool isCached = false;
                for (const auto& pair : stateIconHandles_) {
                    if (pair.second == nid_.hIcon) {
                        isCached = true;
                        break;
                    }
                }
                if (!isCached) {
                    HICON defaultIcon = LoadIcon(nullptr, IDI_INFORMATION);
                    if (nid_.hIcon != defaultIcon) {
                        DestroyIcon(nid_.hIcon);
                    }
                }
            }
            nid_.hIcon = hIcon;
            nid_.uFlags |= NIF_ICON;
            if (visible_) {
                Shell_NotifyIconW(NIM_MODIFY, &nid_);
            }
            std::cout << "[TRAY] 图标状态切换到: " << static_cast<int>(state) << "\n";
            std::cout.flush();
        }
    }

    TrayIconState getIconState() const {
        return currentState_;
    }

    void show() {
        if (!visible_ && hwnd_) {
            if (Shell_NotifyIconW(NIM_ADD, &nid_)) {
                visible_ = true;
                std::cout << "Tray icon shown" << std::endl;
            } else {
                std::cerr << "Failed to show tray icon: " << GetLastError() << std::endl;
            }
        }
    }

    void hide() {
        if (visible_) {
            Shell_NotifyIconW(NIM_DELETE, &nid_);
            visible_ = false;
        }
    }

    void addItem(const TrayItem& item) {
        std::lock_guard<std::mutex> lock(itemsMutex_);
        items_.push_back(item);
    }

    void addSeparator(const std::string& id) {
        TrayItem item;
        item.id = id;
        item.type = TrayItemType::SEPARATOR;
        std::lock_guard<std::mutex> lock(itemsMutex_);
        items_.push_back(item);
    }

    void addSubmenu(const std::string& id, const std::string& label, const std::vector<TrayItem>& items) {
        TrayItem item;
        item.id = id;
        item.label = label;
        item.type = TrayItemType::SUBMENU;
        item.subitems = items;
        std::lock_guard<std::mutex> lock(itemsMutex_);
        items_.push_back(item);
    }

    void removeItem(const std::string& id) {
        std::lock_guard<std::mutex> lock(itemsMutex_);
        items_.erase(
            std::remove_if(items_.begin(), items_.end(),
                [&id](const TrayItem& item) { return item.id == id; }),
            items_.end()
        );
    }

    void clearItems() {
        std::lock_guard<std::mutex> lock(itemsMutex_);
        items_.clear();
    }

    void setItemChecked(const std::string& id, bool checked) {
        std::lock_guard<std::mutex> lock(itemsMutex_);
        for (auto& item : items_) {
            if (item.id == id) {
                item.checked = checked;
                return;
            }
            // 检查子菜单
            for (auto& subItem : item.subitems) {
                if (subItem.id == id) {
                    subItem.checked = checked;
                    return;
                }
            }
        }
    }

    void setItemEnabled(const std::string& id, bool enabled) {
        std::lock_guard<std::mutex> lock(itemsMutex_);
        for (auto& item : items_) {
            if (item.id == id) {
                item.enabled = enabled;
                return;
            }
            // 检查子菜单
            for (auto& subItem : item.subitems) {
                if (subItem.id == id) {
                    subItem.enabled = enabled;
                    return;
                }
            }
        }
    }

    void setItemLabel(const std::string& id, const std::string& label) {
        std::lock_guard<std::mutex> lock(itemsMutex_);
        for (auto& item : items_) {
            if (item.id == id) {
                item.label = label;
                return;
            }
            // 检查子菜单
            for (auto& subItem : item.subitems) {
                if (subItem.id == id) {
                    subItem.label = label;
                    return;
                }
            }
        }
    }

    void updateMenu() {
        // 菜单在点击时动态创建，这里只是标记需要更新
    }

    bool isVisible() const {
        return visible_.load();
    }

    void showMenu(int x, int y) {
        // 复制菜单项以避免长时间持锁
        std::vector<TrayItem> itemsCopy;
        {
            std::lock_guard<std::mutex> lock(itemsMutex_);
            itemsCopy = items_;
        }

        HMENU hMenu = CreatePopupMenu();
        if (!hMenu) return;

        UINT itemId = ID_TRAY_FIRST;
        std::vector<std::function<void()>> callbacks;
        std::vector<std::string> itemIds;  // 存储每个 itemId 对应的配置 ID

        std::cout << "[DEBUG] Creating menu with " << itemsCopy.size() << " items\n";
        std::cout.flush();

        for (const auto& item : itemsCopy) {
            if (item.type == TrayItemType::SEPARATOR) {
                AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            } else if (item.type == TrayItemType::SUBMENU) {
                HMENU hSubMenu = CreatePopupMenu();
                for (const auto& subItem : item.subitems) {
                    if (subItem.type == TrayItemType::SEPARATOR) {
                        AppendMenuW(hSubMenu, MF_SEPARATOR, 0, nullptr);
                    } else {
                        wchar_t wlabel[256];
                        MultiByteToWideChar(CP_UTF8, 0, subItem.label.c_str(), -1,
                                           wlabel, 256);
                        UINT flags = MF_STRING;
                        if (subItem.checked) flags |= MF_CHECKED;
                        if (!subItem.enabled) flags |= MF_GRAYED;
                        AppendMenuW(hSubMenu, flags, itemId++, wlabel);
                        callbacks.push_back(subItem.callback);
                        itemIds.push_back(subItem.id);
                    }
                }
                wchar_t wlabel[256];
                MultiByteToWideChar(CP_UTF8, 0, item.label.c_str(), -1,
                                   wlabel, 256);
                AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hSubMenu), wlabel);
            } else {
                wchar_t wlabel[256];
                MultiByteToWideChar(CP_UTF8, 0, item.label.c_str(), -1,
                                   wlabel, 256);
                UINT flags = MF_STRING;
                if (item.checked) flags |= MF_CHECKED;
                if (!item.enabled) flags |= MF_GRAYED;
                UINT thisItemId = itemId++;
                AppendMenuW(hMenu, flags, thisItemId, wlabel);
                callbacks.push_back(item.callback);
                itemIds.push_back(item.id);
            }
        }

        SetForegroundWindow(hwnd_);
        UINT clicked = TrackPopupMenu(
            hMenu,
            TPM_RETURNCMD | TPM_NONOTIFY,
            x, y,
            0,
            hwnd_,
            nullptr
        );

        DestroyMenu(hMenu);

        std::cout << "[DEBUG] Menu destroyed, clicked: " << clicked << "\n";
        std::cout.flush();

        if (clicked >= ID_TRAY_FIRST) {
            size_t index = clicked - ID_TRAY_FIRST;
            std::cout << "[DEBUG] Menu clicked: " << clicked << ", index: " << index << ", callbacks.size: " << callbacks.size() << "\n";
            std::cout.flush();
            if (index < callbacks.size()) {
                // 检查回调是否为空
                bool hasCallback = static_cast<bool>(callbacks[index]);
                std::cout << "[DEBUG] hasCallback: " << hasCallback << ", index < itemIds.size: " << (index < itemIds.size()) << "\n";
                std::cout.flush();

                // 首先尝试执行直接回调
                if (hasCallback) {
                    try {
                        std::cout << "[DEBUG] Executing direct callback\n";
                        std::cout.flush();
                        callbacks[index]();
                        std::cout << "[DEBUG] Direct callback completed\n";
                        std::cout.flush();
                    } catch (const std::exception& e) {
                        std::cout << "[DEBUG] Exception in callback: " << e.what() << "\n";
                    }
                } else {
                    // 如果没有直接回调，尝试使用动作处理器
                    if (actionHandler_ && index < itemIds.size()) {
                        const std::string& configId = itemIds[index];
                        std::cout << "[DEBUG] Using action handler for configId: " << configId << "\n";
                        std::cout.flush();
                        auto it = itemConfigs_.find(configId);
                        if (it != itemConfigs_.end()) {
                            try {
                                std::cout << "[DEBUG] Calling action handler...\n";
                                std::cout.flush();
                                actionHandler_(it->second);
                                std::cout << "[DEBUG] Action handler returned\n";
                                std::cout.flush();
                            } catch (const std::exception& e) {
                                std::cout << "[DEBUG] Exception in action handler: " << e.what() << "\n";
                            }
                        } else {
                            std::cout << "[DEBUG] Config not found for id: " << configId << "\n";
                        }
                    } else {
                        std::cout << "[DEBUG] No action handler or index out of bounds\n";
                    }
                    std::cout.flush();
                }
            }
        }
        std::cout << "[DEBUG] showMenu() about to return\n";
        std::cout.flush();
    }

    void loadFromConfig(const TrayConfig& config) {
        std::cout << "[TRAY] loadFromConfig 开始, menuItems.size: " << config.menuItems.size() << "\n";
        std::cout.flush();

        std::lock_guard<std::mutex> lock(itemsMutex_);
        items_.clear();
        itemConfigs_.clear();

        std::cout << "[TRAY] items cleared\n";
        std::cout.flush();

        // 设置提示文本
        if (!config.tooltip.empty()) {
            tooltip_ = config.tooltip;
            MultiByteToWideChar(CP_UTF8, 0, config.tooltip.c_str(), -1,
                               nid_.szTip, sizeof(nid_.szTip) / sizeof(wchar_t));
            nid_.uFlags |= NIF_TIP;
            if (visible_) {
                Shell_NotifyIconW(NIM_MODIFY, &nid_);
            }
        }

        std::cout << "[TRAY] tooltip set\n";
        std::cout.flush();

        // 设置默认图标
        if (!config.iconPath.empty()) {
            setIcon(config.iconPath);
        }

        // 加载状态图标
        {
            std::lock_guard<std::mutex> stateLock(stateMutex_);
            if (!config.iconNormal.empty()) stateIcons_[TrayIconState::normal] = config.iconNormal;
            if (!config.iconIdle.empty()) stateIcons_[TrayIconState::idle] = config.iconIdle;
            if (!config.iconBusy.empty()) stateIcons_[TrayIconState::busy] = config.iconBusy;
            if (!config.iconError.empty()) stateIcons_[TrayIconState::error] = config.iconError;
        }

        std::cout << "[TRAY] state icons loaded, loading menu items...\n";
        std::cout.flush();

        // 加载菜单项 (直接操作 items_ 避免死锁)
        for (const auto& menuItem : config.menuItems) {
            itemConfigs_[menuItem.id] = menuItem;

            if (menuItem.isSeparator) {
                // 直接添加分隔符，不调用 addSeparator (避免死锁)
                TrayItem sepItem;
                sepItem.id = menuItem.id;
                sepItem.type = TrayItemType::SEPARATOR;
                items_.push_back(sepItem);
            } else if (!menuItem.subitems.empty()) {
                // 子菜单 - 直接添加，不调用 addSubmenu (避免死锁)
                TrayItem subItem;
                subItem.id = menuItem.id;
                subItem.label = menuItem.label;
                subItem.type = TrayItemType::SUBMENU;
                subItem.checked = menuItem.checked;
                subItem.enabled = menuItem.enabled;

                for (const auto& subConfig : menuItem.subitems) {
                    itemConfigs_[subConfig.id] = subConfig;
                    TrayItem item;
                    item.id = subConfig.id;
                    item.label = subConfig.label;
                    item.type = subConfig.isSeparator ? TrayItemType::SEPARATOR : TrayItemType::NORMAL;
                    item.checked = subConfig.checked;
                    item.enabled = subConfig.enabled;
                    subItem.subitems.push_back(item);
                }
                items_.push_back(subItem);
            } else {
                // 普通菜单项
                TrayItem item;
                item.id = menuItem.id;
                item.label = menuItem.label;
                item.type = TrayItemType::NORMAL;
                item.checked = menuItem.checked;
                item.enabled = menuItem.enabled;
                items_.push_back(item);
            }
        }

        std::cout << "[TRAY] loadFromConfig 完成\n";
        std::cout.flush();
    }

    void setActionHandler(TrayIcon::ActionHandler handler) {
        actionHandler_ = std::move(handler);
    }

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        Impl* impl = nullptr;

        if (msg == WM_CREATE) {
            // 从 CREATESTRUCT 获取 this 指针
            CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
            impl = static_cast<Impl*>(cs->lpCreateParams);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(impl));
            std::cout << "[DEBUG] WM_CREATE received\n";
            std::cout.flush();
        } else {
            impl = reinterpret_cast<Impl*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        }

        switch (msg) {
            case WM_TRAYICON:
                std::cout << "[DEBUG] WM_TRAYICON received, lParam: 0x" << std::hex << lParam << std::dec << "\n";
                std::cout.flush();
                if (impl && (lParam == WM_LBUTTONDOWN || lParam == WM_RBUTTONDOWN)) {
                    POINT pt;
                    GetCursorPos(&pt);
                    impl->showMenu(pt.x, pt.y);
                }
                return 0;

            case WM_DESTROY:
                std::cout << "[DEBUG] WM_DESTROY received, posting quit message\n";
                std::cout.flush();
                PostQuitMessage(0);
                return 0;

            case WM_CLOSE:
                std::cout << "[DEBUG] WM_CLOSE received\n";
                std::cout.flush();
                return 0;

            default:
                // 记录一些关键消息
                if (msg == WM_COMMAND || msg == WM_SYSCOMMAND || msg == WM_ACTIVATE) {
                    std::cout << "[DEBUG] Message: 0x" << std::hex << msg << std::dec << ", wParam: 0x" << std::hex << wParam << std::dec << ", lParam: 0x" << std::hex << lParam << std::dec << "\n";
                    std::cout.flush();
                }
                break;
        }

        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
};

TrayIcon::TrayIcon(const std::string& tooltip)
    : impl_(std::make_unique<Impl>(tooltip)) {}

TrayIcon::~TrayIcon() = default;

void TrayIcon::setIcon(const std::string& iconPath) {
    impl_->setIcon(iconPath);
}

void TrayIcon::setTooltip(const std::string& tooltip) {
    impl_->setTooltip(tooltip);
}

void TrayIcon::addItem(const TrayItem& item) {
    impl_->addItem(item);
}

void TrayIcon::addItem(const std::string& id, const std::string& label, std::function<void()> callback) {
    TrayItem item;
    item.id = id;
    item.label = label;
    item.type = TrayItemType::NORMAL;
    item.callback = std::move(callback);
    impl_->addItem(item);
}

void TrayIcon::addSeparator(const std::string& id) {
    impl_->addSeparator(id);
}

void TrayIcon::addSubmenu(const std::string& id, const std::string& label, const std::vector<TrayItem>& items) {
    impl_->addSubmenu(id, label, items);
}

void TrayIcon::removeItem(const std::string& id) {
    impl_->removeItem(id);
}

void TrayIcon::clearItems() {
    impl_->clearItems();
}

void TrayIcon::show() {
    impl_->show();
}

void TrayIcon::hide() {
    impl_->hide();
}

void TrayIcon::updateMenu() {
    impl_->updateMenu();
}

bool TrayIcon::isVisible() const {
    return impl_->isVisible();
}

void TrayIcon::loadFromConfig(const TrayConfig& config) {
    impl_->loadFromConfig(config);
}

void TrayIcon::setActionHandler(ActionHandler handler) {
    impl_->setActionHandler(std::move(handler));
}

void TrayIcon::setIconState(TrayIconState state) {
    impl_->setIconState(state);
}

TrayIconState TrayIcon::getIconState() const {
    return impl_->getIconState();
}

void TrayIcon::setItemChecked(const std::string& id, bool checked) {
    impl_->setItemChecked(id, checked);
}

void TrayIcon::setItemEnabled(const std::string& id, bool enabled) {
    impl_->setItemEnabled(id, enabled);
}

void TrayIcon::setItemLabel(const std::string& id, const std::string& label) {
    impl_->setItemLabel(id, label);
}

// TrayManager implementation
class TrayManager::Impl {
public:
    std::map<std::string, std::shared_ptr<TrayIcon>> icons_;
    std::mutex mutex_;
};

TrayManager::TrayManager() : impl_(std::make_unique<Impl>()) {}

TrayManager& TrayManager::instance() {
    static TrayManager instance_;
    return instance_;
}

std::shared_ptr<TrayIcon> TrayManager::createIcon(const std::string& id, const std::string& tooltip) {
    auto icon = std::make_shared<TrayIcon>(tooltip);
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->icons_[id] = icon;
    return icon;
}

std::shared_ptr<TrayIcon> TrayManager::getIcon(const std::string& id) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    auto it = impl_->icons_.find(id);
    return it != impl_->icons_.end() ? it->second : nullptr;
}

void TrayManager::removeIcon(const std::string& id) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->icons_.erase(id);
}

void TrayManager::clear() {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->icons_.clear();
}

} // namespace wingman

#endif // _WIN32
