// UI Automation 平台工厂声明。
// 平台私有实现（src/platform/<os>）提供具体定义；Linux/Unix 等未支持平台
// 提供返回 nullptr 的 stub，让门面层保持平台中立。
#pragma once

#include <memory>

namespace wingman {
class IUIAManager;
}

std::unique_ptr<wingman::IUIAManager> createUIAManager();
