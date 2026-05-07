#pragma once

#include <string>
#include <optional>
#include <vector>

namespace wingman::gui {

// Windows 文件选择对话框
std::optional<std::string> OpenFileDialog(
    const std::string& title,
    const std::string& filter = "All Files (*.*)|*.*|Lua Files (*.lua)|*.lua||"
);

std::optional<std::string> SaveFileDialog(
    const std::string& title,
    const std::string& filter = "All Files (*.*)|*.*|Lua Files (*.lua)|*.lua||"
);

std::optional<std::vector<std::string>> OpenFilesDialog(
    const std::string& title,
    const std::string& filter = "All Files (*.*)|*.*|"
);

} // namespace wingman::gui
