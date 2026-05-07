#pragma once

#include "wingman/trigger.hpp"
#include <imgui.h>
#include <string>
#include <vector>

namespace wingman::gui {

// 触发器管理面板
class TriggerPanel {
public:
    explicit TriggerPanel(TriggerManager* manager);
    ~TriggerPanel() = default;

    void render(bool* show);

private:
    void renderTriggerList();
    void renderTriggerEditor();
    void renderAddTriggerDialog();

    TriggerManager* manager_;
    bool showAddDialog_;
    bool showEditDialog_;
    size_t selectedTriggerId_;
    int newTriggerType_;
    char newTriggerName_[256];
    int newTriggerCondition_;
    int newTriggerAction_;

    // 触发器状态缓存
    std::vector<std::pair<size_t, std::string>> triggerList_;
};

} // namespace wingman::gui
