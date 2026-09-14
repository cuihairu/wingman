#pragma once

#include "wingman/script/iscript_engine.hpp"

namespace wingman {
namespace script {
namespace modules {

// 机器学习模块：ID 句柄式绑定 ModelEngine（多模型）。
// 暴露模型管理与推理 API（providers/loadModel/unload/isLoaded/inputs/outputs/run）。
// detect/classify 待 ModelHelpers::detectObjects 实现 + 图像加载支持后补充。
ModuleDescriptor createMlModule();

// 引擎 shutdown 时释放所有 ModelEngine 实例。
void cleanupMlModule();

} // namespace modules
} // namespace script
} // namespace wingman
