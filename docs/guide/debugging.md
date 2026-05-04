# 调试指南

## VS Code 插件

Wingman 提供 VS Code 调试插件，支持：

- 断点调试
- 变量查看
- 单步执行
- 性能分析

### 安装

```bash
code --install-extension wingman.debugger
```

### 配置

创建 `.vscode/launch.json`：

```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "type": "wingman",
      "request": "launch",
      "name": "Launch Wingman Script",
      "program": "${workspaceFolder}/scripts/main.lua",
      "wingmanPath": "${workspaceFolder}/build/Debug/wingman.exe"
    }
  ]
}
```

### 使用

1. 在 Lua 脚本中设置断点
2. 按 F5 启动调试
3. 使用调试工具栏控制执行

## 日志调试

使用 `debug` 模块输出日志：

```lua
local debug = require("debug")

debug.log("Info message")
debug.warn("Warning message")
debug.error("Error message")
```

## 常见问题

### 脚本无响应

- 检查是否有死循环
- 使用 `debug.traceback()` 查看调用栈

### 性能问题

- 使用 `debug.profile()` 分析性能
- 优化图像查找范围
- 减少不必要的屏幕截图
