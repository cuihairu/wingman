#include "wingman/gui/panels/trigger_panel.hpp"
#include <spdlog/spdlog.h>
#include <imgui.h>

namespace wingman::gui {

TriggerPanel::TriggerPanel(TriggerManager* manager)
    : manager_(manager)
    , showAddDialog_(false)
    , showEditDialog_(false)
    , selectedTriggerId_(0)
    , newTriggerType_(0)
    , newTriggerCondition_(0)
    , newTriggerAction_(0)
{
    memset(newTriggerName_, 0, sizeof(newTriggerName_));
}

void TriggerPanel::render(bool* show) {
    if (!ImGui::Begin("触发器", show)) {
        ImGui::End();
        return;
    }

    // 工具栏
    if (ImGui::Button("添加触发器")) {
        showAddDialog_ = true;
        memset(newTriggerName_, 0, sizeof(newTriggerName_));
        newTriggerCondition_ = 0;
        newTriggerAction_ = 0;
    }
    ImGui::SameLine();
    if (ImGui::Button("全选启用")) {
        // 启用所有触发器
        auto triggers = manager_->getAllTriggerInstances();
        for (const auto& t : triggers) {
            if (!t.config.enabled) {
                manager_->enable(t.id);
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("全禁用")) {
        // 禁用所有触发器
        auto triggers = manager_->getAllTriggerInstances();
        for (const auto& t : triggers) {
            if (t.config.enabled) {
                manager_->disable(t.id);
            }
        }
    }

    ImGui::Separator();

    // 触发器统计
    auto triggers = manager_->getAllTriggerInstances();
    int enabledCount = 0;
    for (const auto& t : triggers) {
        if (t.config.enabled) enabledCount++;
    }
    ImGui::Text("总计: %zu | 启用: %d | 禁用: %zu",
        triggers.size(), enabledCount, triggers.size() - enabledCount);

    ImGui::Separator();

    // 触发器列表
    renderTriggerList();

    // 添加触发器对话框
    if (showAddDialog_) {
        renderAddTriggerDialog();
    }

    // 编辑触发器对话框
    if (showEditDialog_ && selectedTriggerId_ != 0) {
        renderTriggerEditor();
    }

    ImGui::End();
}

void TriggerPanel::renderTriggerList() {
    // 获取触发器列表
    triggerList_.clear();
    auto instances = manager_->getAllTriggerInstances();
    for (const auto& t : instances) {
        std::string status = t.config.enabled ? "启用" : "禁用";
        triggerList_.push_back({t.id, status});
    }

    if (ImGui::BeginTable("TriggerTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 40);
        ImGui::TableSetupColumn("名称");
        ImGui::TableSetupColumn("状态", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("触发次数", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("操作", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableHeadersRow();

        for (const auto& [id, status] : triggerList_) {
            auto config = manager_->getTriggerConfig(id);
            if (!config) continue;

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%zu", id);

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", config->name.c_str());

            ImGui::TableSetColumnIndex(2);
            if (config->enabled) {
                ImGui::TextColored(ImVec4(0, 1, 0, 1), "启用");
            } else {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), "禁用");
            }

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("0"); // TODO: 获取实际触发次数

            ImGui::TableSetColumnIndex(4);
            char editId[64], toggleId[64], deleteId[64];
            snprintf(editId, sizeof(editId), "编辑##%zu", id);
            snprintf(toggleId, sizeof(toggleId), "%s##%zu", config->enabled ? "禁用" : "启用", id);
            snprintf(deleteId, sizeof(deleteId), "删除##%zu", id);

            if (ImGui::SmallButton(editId)) {
                selectedTriggerId_ = id;
                showEditDialog_ = true;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton(toggleId)) {
                if (config->enabled) {
                    manager_->disable(id);
                } else {
                    manager_->enable(id);
                }
            }
            ImGui::SameLine();
            if (ImGui::SmallButton(deleteId)) {
                manager_->remove(id);
                spdlog::info("Removed trigger: {}", id);
            }
        }

        ImGui::EndTable();
    }
}

void TriggerPanel::renderTriggerEditor() {
    char title[64];
    snprintf(title, sizeof(title), "编辑触发器 ##%zu", selectedTriggerId_);

    if (!ImGui::Begin(title, &showEditDialog_)) {
        ImGui::End();
        return;
    }

    auto config = manager_->getTriggerConfig(selectedTriggerId_);
    if (!config) {
        ImGui::Text("触发器不存在");
        if (ImGui::Button("关闭")) {
            showEditDialog_ = false;
            selectedTriggerId_ = 0;
        }
        ImGui::End();
        return;
    }

    // 编辑用的本地变量
    static TriggerConfig editConfig;
    static bool editConfigInitialized = false;

    if (!editConfigInitialized || ImGui::IsWindowAppearing()) {
        editConfig = *config;
        editConfigInitialized = true;
    }

    // 显示当前配置
    ImGui::Text("ID: %zu", selectedTriggerId_);

    ImGui::Separator();

    // 名称编辑
    char nameBuffer[256];
    strncpy(nameBuffer, editConfig.name.c_str(), sizeof(nameBuffer));
    nameBuffer[sizeof(nameBuffer) - 1] = '\0';
    ImGui::Text("名称:");
    if (ImGui::InputText("##EditName", nameBuffer, sizeof(nameBuffer))) {
        editConfig.name = nameBuffer;
    }

    // 启用状态
    bool enabled = editConfig.enabled;
    if (ImGui::Checkbox("启用", &enabled)) {
        editConfig.enabled = enabled;
    }

    ImGui::Separator();

    // 条件配置
    ImGui::Text("条件类型:");
    int conditionType = static_cast<int>(editConfig.condition.type);
    const char* conditionTypes[] = {
        "颜色出现", "颜色消失", "图像出现", "图像消失",
        "窗口打开", "窗口关闭", "进程启动", "进程停止"
    };
    if (ImGui::Combo("##EditCondition", &conditionType, conditionTypes, IM_ARRAYSIZE(conditionTypes))) {
        editConfig.condition.type = static_cast<TriggerType>(conditionType);
    }

    ImGui::Text("条件值:");
    char conditionValue[256];
    strncpy(conditionValue, editConfig.condition.value.c_str(), sizeof(conditionValue));
    conditionValue[sizeof(conditionValue) - 1] = '\0';
    if (ImGui::InputText("##ConditionValue", conditionValue, sizeof(conditionValue))) {
        editConfig.condition.value = conditionValue;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(颜色: 0xRRGGBB, 图像: 路径, 窗口/进程: 标题/名称)");

    ImGui::Text("容差:");
    int tolerance = editConfig.condition.tolerance;
    if (ImGui::SliderInt("##Tolerance", &tolerance, 0, 100)) {
        editConfig.condition.tolerance = tolerance;
    }

    ImGui::Text("检查间隔 (ms):");
    int interval = editConfig.condition.interval;
    if (ImGui::InputInt("##Interval", &interval, 10, 100)) {
        if (interval >= 0) {
            editConfig.condition.interval = interval;
        }
    }

    ImGui::Separator();

    // 动作配置
    ImGui::Text("动作数量: %zu", editConfig.actions.size());
    if (ImGui::Button("添加动作")) {
        TriggerActionData action;
        action.type = BasicTriggerAction::Click;
        action.x = 100;
        action.y = 100;
        action.delay = 0;
        editConfig.actions.push_back(action);
    }

    // 显示和编辑现有动作
    for (size_t i = 0; i < editConfig.actions.size(); ++i) {
        auto& action = editConfig.actions[i];
        char actionId[64];
        snprintf(actionId, sizeof(actionId), "动作 %zu##%zu", i, i);

        if (ImGui::CollapsingHeader(actionId)) {
            // 动作类型
            int actionType = static_cast<int>(action.type);
            const char* actionTypes[] = {"运行脚本", "点击", "按键", "输入", "停止脚本", "暂停脚本", "显示消息", "播放声音", "记录日志"};
            if (ImGui::Combo("Type", &actionType, actionTypes, IM_ARRAYSIZE(actionTypes))) {
                action.type = static_cast<BasicTriggerAction>(actionType);
            }

            // 根据类型显示不同参数
            switch (action.type) {
                case BasicTriggerAction::Click:
                    ImGui::InputInt("X", &action.x);
                    ImGui::InputInt("Y", &action.y);
                    break;
                case BasicTriggerAction::KeyPress:
                    ImGui::Text("键码:");
                    ImGui::InputInt("KeyCode", &action.x);  // 复用 x 存储键码
                    break;
                case BasicTriggerAction::Type:
                case BasicTriggerAction::RunScript:
                case BasicTriggerAction::ShowMessage:
                case BasicTriggerAction::PlayAudio:
                case BasicTriggerAction::Log: {
                    char valueBuffer[256];
                    strncpy(valueBuffer, action.value.c_str(), sizeof(valueBuffer));
                    valueBuffer[sizeof(valueBuffer) - 1] = '\0';
                    if (ImGui::InputText("Value", valueBuffer, sizeof(valueBuffer))) {
                        action.value = valueBuffer;
                    }
                    break;
                }
                default:
                    break;
            }

            ImGui::InputInt("Delay (ms)", &action.delay);

            // 删除按钮
            char deleteId[64];
            snprintf(deleteId, sizeof(deleteId), "删除##%zu", i);
            if (ImGui::Button(deleteId)) {
                editConfig.actions.erase(editConfig.actions.begin() + i);
                --i;
            }
        }
    }

    ImGui::Separator();

    // 冷却时间
    ImGui::Text("冷却时间 (ms):");
    int cooldown = editConfig.cooldown;
    if (ImGui::InputInt("##Cooldown", &cooldown, 100, 1000)) {
        if (cooldown >= 0) {
            editConfig.cooldown = cooldown;
        }
    }

    // 只触发一次
    bool oneShot = editConfig.oneShot;
    if (ImGui::Checkbox("只触发一次", &oneShot)) {
        editConfig.oneShot = oneShot;
    }

    ImGui::Separator();

    if (ImGui::Button("保存")) {
        // 更新触发器配置
        if (manager_->update(selectedTriggerId_, editConfig)) {
            spdlog::info("Updated trigger: {} ({})", editConfig.name, selectedTriggerId_);
        } else {
            spdlog::error("Failed to update trigger: {}", selectedTriggerId_);
        }
        showEditDialog_ = false;
        selectedTriggerId_ = 0;
        editConfigInitialized = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("取消")) {
        showEditDialog_ = false;
        selectedTriggerId_ = 0;
        editConfigInitialized = false;
    }

    ImGui::End();
}

void TriggerPanel::renderAddTriggerDialog() {
    if (!ImGui::Begin("添加触发器", &showAddDialog_)) {
        ImGui::End();
        return;
    }

    // 名称输入
    ImGui::InputText("名称", newTriggerName_, sizeof(newTriggerName_));

    // 条件类型选择
    ImGui::Text("条件类型:");
    ImGui::Combo("##Condition", &newTriggerCondition_,
        "颜色出现\0颜色消失\0图像出现\0图像消失\0窗口打开\0窗口关闭\0进程启动\0进程停止\0");

    // 条件值
    ImGui::Text("条件值:");
    char conditionValue[256] = {0};
    ImGui::InputText("##ConditionValue", conditionValue, sizeof(conditionValue));
    ImGui::SameLine();
    ImGui::TextDisabled("(颜色: 0xRRGGBB, 图像: 路径, 窗口/进程: 标题/名称)");

    // 容差
    int tolerance = 10;
    ImGui::Text("容差:");
    ImGui::SliderInt("##Tolerance", &tolerance, 0, 100);

    // 动作类型选择
    ImGui::Separator();
    ImGui::Text("动作类型:");
    ImGui::Combo("##Action", &newTriggerAction_,
        "点击\0按键\0输入文本\0执行脚本\0显示消息\0播放声音\0");

    // 动作参数
    char actionValue[256] = {0};
    int actionX = 100, actionY = 100;
    int actionKeyCode = 0x41; // 'A' key

    switch (newTriggerAction_) {
        case 0: // 点击
            ImGui::Text("坐标:");
            ImGui::InputInt("X##ActionX", &actionX);
            ImGui::SameLine();
            ImGui::InputInt("Y##ActionY", &actionY);
            break;
        case 1: // 按键
            ImGui::Text("虚拟键码:");
            ImGui::InputInt("##ActionKeyCode", &actionKeyCode, 1, 16);
            ImGui::SameLine();
            ImGui::TextDisabled("(十六进制)");
            break;
        case 2: // 输入文本
            ImGui::Text("文本:");
            ImGui::InputText("##ActionText", actionValue, sizeof(actionValue));
            break;
        case 3: // 执行脚本
            ImGui::Text("脚本路径:");
            ImGui::InputText("##ActionScript", actionValue, sizeof(actionValue));
            break;
        case 4: // 显示消息
            ImGui::Text("消息内容:");
            ImGui::InputText("##ActionMessage", actionValue, sizeof(actionValue));
            break;
        case 5: // 播放声音
            ImGui::Text("音频文件:");
            ImGui::InputText("##ActionSound", actionValue, sizeof(actionValue));
            break;
    }

    // 冷却时间和选项
    ImGui::Separator();
    int cooldown = 500;
    ImGui::Text("冷却时间 (ms):");
    ImGui::InputInt("##Cooldown", &cooldown, 100, 1000);

    bool oneShot = false;
    ImGui::Checkbox("只触发一次", &oneShot);

    // 按钮
    ImGui::Separator();
    if (ImGui::Button("添加")) {
        // 创建触发器配置
        TriggerConfig config;
        config.name = newTriggerName_;
        config.condition.type = static_cast<TriggerType>(newTriggerCondition_);
        config.condition.value = conditionValue;
        config.condition.tolerance = tolerance;
        config.condition.region = Rect(0, 0, 0, 0); // TODO: 从区域选择器获取
        config.condition.interval = 50;
        config.condition.enabled = true;

        // 添加动作
        TriggerActionData action;
        action.type = static_cast<BasicTriggerAction>(newTriggerAction_);
        action.value = actionValue;
        action.x = actionX;
        action.y = actionY;
        action.keyCode = actionKeyCode;
        action.delay = 0;
        config.actions.push_back(action);

        config.oneShot = oneShot;
        config.cooldown = cooldown;
        config.enabled = true;

        // 添加到管理器
        size_t id = manager_->add(config);
        spdlog::info("Added trigger '{}' with ID: {}", config.name, id);

        showAddDialog_ = false;
        memset(newTriggerName_, 0, sizeof(newTriggerName_));
    }
    ImGui::SameLine();
    if (ImGui::Button("取消")) {
        showAddDialog_ = false;
    }

    ImGui::End();
}

} // namespace wingman::gui
