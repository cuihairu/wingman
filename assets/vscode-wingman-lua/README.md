# Wingman Lua 开发配置包

VSCode 配置文件，用于 Wingman Lua 脚本开发。

## 功能

- ✅ 代码自动补全
- ✅ 函数参数提示
- ✅ 语法检查
- ✅ 断点调试
- ✅ 类型定义

## 使用方法

### 1. 安装 VSCode 插件

在 VSCode 中安装 **EmmyLua** 插件（作者：tangzx）

### 2. 复制配置文件到你的脚本项目

```
你的脚本项目/
├── .vscode/
│   ├── .emmyrc.json    # EmmyLua 配置
│   └── launch.json      # 调试配置
├── wingman.d.lua        # API 类型定义
└── 你的脚本.lua
```

### 3. 重启 VSCode

重启后即可享受代码提示！

## 调试配置

### 启动调试器

在你的 Lua 脚本开头添加：

```lua
-- 连接到调试器
local dbg = require('emmy_core')
dbg.tcpConnect('localhost', 9966)

-- 你的代码
local wingman = require('wingman')
wingman.screen.capture(0, 0, 1920, 1080)
```

### VSCode 调试步骤

1. 在 VSCode 中按 `F5` 或点击调试面板
2. 选择 "Attach to Wingman"
3. 运行你的 Lua 脚本
4. 设置断点，开始调试！

## API 文档

查看 [Wingman API 文档](../../docs/api/) 了解完整的函数列表。

## 配置说明

### .emmyrc.json

```json
{
    "diagnostics": {
        "enable": true,
        "disable": ["param-type-not-match", "need-check-nil"]
    },
    "runtime": {
        "version": "Lua 5.4",
        "path": ["./?.lua", "./?/init.lua"]
    }
}
```

### 自定义配置

根据你的项目需求修改：

- `runtime.path` - Lua 模块搜索路径
- `workspace.ignoreDir` - 忽略的目录
- `diagnostics.globals` - 全局变量列表

## 常见问题

### Q: 没有代码提示？

A: 确保 `wingman.d.lua` 在项目根目录，并重启 VSCode。

### Q: 调试连接失败？

A: 确保 Wingman 已启动调试器（端口 9966）。

### Q: 类型检查报错？

A: 可以在 `.emmyrc.json` 的 `diagnostics.disable` 中添加相应规则。

## 版本

- 适用 Wingman 版本：0.1.0+
- 适用 Lua 版本：5.4
- 适用 EmmyLua 版本：0.5.0+
