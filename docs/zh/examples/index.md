> [English](../../examples/index.md) | **中文**

# 示例脚本

以下示例展示 Wingman 的各种功能用法：

## 基础示例

- [Hello World](hello-world.md) - 最简单的示例
- [像素检测](pixel-detection.md) - 检测屏幕上的指定颜色
- [图像匹配](image-matching.md) - 查找屏幕上的图像
- [自动化循环](auto-loop.md) - 持续监控并执行操作
- [宏录制](macro-record.md) - 录制并回放操作

## UI Automation 示例

- [UI Automation](ui-automation.md) - Windows 控件自动化交互

## 功能示例（examples/lua_scripts）

- `template_batch.lua` - 模板具名登记与批量识别（templateAdd / templateFindAll）
- `kv_store.lua`、`color_detect.lua`、`window_control.lua` 等 - 各模块独立示例，见目录

## Python 示例（examples/python_scripts）

Python 与 Lua 共享同一套模块面，函数同时暴露 camelCase 原名与 snake_case 别名（推荐 snake_case，与 `libs/python/typing` 桩一致）：

- `hello.py` - KV 存储 + 通知入门
- `template_batch.py` - 模板批量管理（与 `template_batch.lua` 同型）
- `workflow_pipeline.py` - 工作流编排：依赖、条件分支、产出聚合（run/when 需线程安全可调用体，Lua 函数体会被提交即拒，编排示例仅 Python）
- `events_fsm.py` - 事件订阅与 FSM 状态机组合

## 运行示例

```bash
wingman-agent.exe script examples/lua_scripts/hello_world.lua
```
