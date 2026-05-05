#include "wingman/tray.hpp"
#include <windows.h>
#include <shellapi.h>
#include <map>
#include <mutex>
#include <atomic>

namespace wingman {

// WM_APP 消息范围用于托盘图标
#define WM_TRAYICON (WM_APP + 1)
#define ID_TRAY_FIRST 1000

class TrayIcon::Impl {
public:
    std::string tooltip_;
    std::string iconPath_;
    HWND hwnd_;
    NOTIFYICONDATAA nid_;
    std::vector<TrayItem> items_;
    std::mutex itemsMutex_;
    std::atomic<bool> visible_{false};
    UINT nextId_ = ID_TRAY_FIRST;

    Impl(const std::string& tooltip) : tooltip_(tooltip), hwnd_(nullptr) {
        // 创建隐藏窗口用于接收消息
        WNDCLASSEXA wc = {0};
        wc.cbSize = sizeof(WNDCLASSEXA);
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = "WingmanTrayClass";

        RegisterClassExA(&wc);

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

        // 初始化 NOTIFYICONDATA
        memset(&nid_, 0, sizeof(nid_));
        nid_.cbSize = sizeof(NOTIFYICONDATAA);
        nid_.hWnd = hwnd_;
        nid_.uID = 1;
        nid_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nid_.uCallbackMessage = WM_TRAYICON;
        nid_.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
        strncpy_s(nid_.szTip, tooltip.c_str(), sizeof(nid_.szTip) - 1);

        // 保存 this 指针
        SetWindowLongPtr(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    }

    ~Impl() {
        hide();
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
            GetSystemMetrics(SM_CXSMICON),
            GetSystemMetrics(SM_CYSMICON),
            LR_LOADFROMFILE
        ));

        if (hIcon) {
            if (nid_.hIcon && nid_.hIcon != LoadIcon(nullptr, IDI_APPLICATION)) {
                DestroyIcon(nid_.hIcon);
            }
            nid_.hIcon = hIcon;
            nid_.uFlags |= NIF_ICON;
            if (visible_) {
                Shell_NotifyIconA(NIM_MODIFY, &nid_);
            }
        }
    }

    void setTooltip(const std::string& tooltip) {
        tooltip_ = tooltip;
        strncpy_s(nid_.szTip, tooltip.c_str(), sizeof(nid_.szTip) - 1);
        nid_.uFlags |= NIF_TIP;
        if (visible_) {
            Shell_NotifyIconA(NIM_MODIFY, &nid_);
        }
    }

    void show() {
        if (!visible_) {
            Shell_NotifyIconA(NIM_ADD, &nid_);
            visible_ = true;
        }
    }

    void hide() {
        if (visible_) {
            Shell_NotifyIconA(NIM_DELETE, &nid_);
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

    void updateMenu() {
        // 菜单在点击时动态创建，这里只是标记需要更新
    }

    bool isVisible() const {
        return visible_.load();
    }

    void showMenu(int x, int y) {
        std::lock_guard<std::mutex> lock(itemsMutex_);

        HMENU hMenu = CreatePopupMenu();
        if (!hMenu) return;

        UINT itemId = ID_TRAY_FIRST;

        for (const auto& item : items_) {
            if (item.type == TrayItemType::SEPARATOR) {
                AppendMenuA(hMenu, MF_SEPARATOR, 0, nullptr);
            } else if (item.type == TrayItemType::SUBMENU) {
                HMENU hSubMenu = CreatePopupMenu();
                for (const auto& subItem : item.subitems) {
                    if (subItem.type == TrayItemType::SEPARATOR) {
                        AppendMenuA(hSubMenu, MF_SEPARATOR, 0, nullptr);
                    } else {
                        AppendMenuA(hSubMenu, MF_STRING, itemId++, subItem.label.c_str());
                    }
                }
                AppendMenuA(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hSubMenu), item.label.c_str());
            } else {
                AppendMenuA(hMenu, MF_STRING, itemId++, item.label.c_str());
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

        if (clicked >= ID_TRAY_FIRST) {
            executeItem(clicked - ID_TRAY_FIRST);
        }

        DestroyMenu(hMenu);
    }

private:
    void executeItem(size_t index) {
        size_t currentIndex = 0;
        for (const auto& item : items_) {
            if (item.type == TrayItemType::NORMAL) {
                if (currentIndex == index && item.callback) {
                    item.callback();
                    return;
                }
                currentIndex++;
            } else if (item.type == TrayItemType::SUBMENU) {
                for (const auto& subItem : item.subitems) {
                    if (subItem.type == TrayItemType::NORMAL) {
                        if (currentIndex == index && subItem.callback) {
                            subItem.callback();
                            return;
                        }
                        currentIndex++;
                    }
                }
            }
        }
    }

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        Impl* impl = reinterpret_cast<Impl*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

        switch (msg) {
            case WM_TRAYICON:
                if (lParam == WM_LBUTTONUP || lParam == WM_RBUTTONUP) {
                    POINT pt;
                    GetCursorPos(&pt);
                    if (impl) {
                        impl->showMenu(pt.x, pt.y);
                    }
                }
                return 0;

            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
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

// TrayManager implementation
class TrayManager::Impl {
public:
    std::map<std::string, std::shared_ptr<TrayIcon>> icons_;
    std::mutex mutex_;
};

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
