// Linux UI Automation 存根：CUIA 无 macOS/iOS，故返回 nullptr 让门面层
// 打印 warn 并回退空语义（与旧 ifdef 非 Win/Mac 分支一致）。
#include "wingman/ui_automation.hpp"
#include "platform/ui_automation_factory.hpp"

std::unique_ptr<wingman::IUIAManager> createUIAManager() {
    return nullptr;
}
