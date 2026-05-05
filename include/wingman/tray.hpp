#ifndef WINGMAN_TRAY_HPP
#define WINGMAN_TRAY_HPP

#include <string>
#include <functional>
#include <vector>
#include <memory>

namespace wingman {

// 菜单项类型
enum class TrayItemType {
    NORMAL,
    SEPARATOR,
    SUBMENU
};

// 菜单项
struct TrayItem {
    std::string id;
    std::string label;
    TrayItemType type = TrayItemType::NORMAL;
    std::function<void()> callback;
    std::vector<TrayItem> subitems;
};

// 托盘图标类
class TrayIcon {
public:
    explicit TrayIcon(const std::string& tooltip);
    ~TrayIcon();

    // 禁止拷贝
    TrayIcon(const TrayIcon&) = delete;
    TrayIcon& operator=(const TrayIcon&) = delete;

    // 设置图标（.ico 文件路径）
    void setIcon(const std::string& iconPath);

    // 设置提示文本
    void setTooltip(const std::string& tooltip);

    // 添加菜单项
    void addItem(const TrayItem& item);

    // 便捷方法：添加菜单项（id, label, callback）
    void addItem(const std::string& id, const std::string& label, std::function<void()> callback);

    // 添加分隔符
    void addSeparator(const std::string& id);

    // 添加子菜单
    void addSubmenu(const std::string& id, const std::string& label, const std::vector<TrayItem>& items);

    // 移除菜单项
    void removeItem(const std::string& id);

    // 清空所有菜单项
    void clearItems();

    // 显示/隐藏托盘图标
    void show();
    void hide();

    // 更新菜单
    void updateMenu();

    // 检查是否可见
    bool isVisible() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

// 托盘管理器（单例）
class TrayManager {
public:
    static TrayManager& instance();

    // 创建托盘图标
    std::shared_ptr<TrayIcon> createIcon(const std::string& id, const std::string& tooltip);

    // 获取托盘图标
    std::shared_ptr<TrayIcon> getIcon(const std::string& id);

    // 移除托盘图标
    void removeIcon(const std::string& id);

    // 清空所有图标
    void clear();

private:
    TrayManager();
    ~TrayManager() = default;

    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace wingman

#endif // WINGMAN_TRAY_HPP
