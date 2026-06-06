# Wingman 项目待办事项

> 最后更新: 2026-06-07
> 状态: 开发中 - GUI/Dashboard 功能补全阶段

---

## 🎯 里程碑规划

| 里程碑 | 目标 | 状态 | 完成度 |
|--------|------|------|--------|
| M1: 核心功能 | 基础屏幕捕获、输入模拟、Lua 脚本 | ✅ 完成 | 100% |
| M2: 触发器系统 | 条件触发、自动化配置 | ✅ 完成 | 100% |
| M3: 宏系统 | 录制回放 | ✅ 完成 | 100% |
| M4: 远程编排 | Orchestrator 中控、Agent 通信 | 🚧 进行中 | 80% |
| M5: GUI 界面 | 本地控制台 + 远程 Dashboard | 🚧 进行中 | 75% |
| M6: 人性化模拟 | 防检测、随机化 | ✅ 完成 | 100% |
| M7: 调试器集成 | EmmyLua 调试支持 | ✅ 完成 | 100% |

---

## 🔴 P0 - 必须完成（阻塞交付）

### Runtime GUI (Tauri + Svelte)

**已有页面**: dashboard, scripts, triggers, settings, logs

- [ ] **VSCode 开发环境配置** (替代内置编辑器)
  - [ ] 提供 `.vscode/settings.json` 配置 (Lua + Python 语法检查、EmmyLua 调试)
  - [ ] 提供 `.vscode/launch.json` EmmyLua Attach 调试配置
  - [ ] 提供 `.vscode/extensions.json` 推荐扩展列表 (EmmyLua、Python、Pylance 等)
  - [ ] 脚本目录 workspace 推荐
  - [ ] 删除 `editor/+page.svelte` 空占位页面
- [ ] **屏幕预览面板** (未实现，需新建页面或组件)
  - [ ] 实时截图显示 (通过 IPC `screenshot.capture`)
  - [ ] 区域选择可视化 (RegionPicker 已有，需集成到预览)
  - [ ] 坐标拾取器
  - [ ] 颜色拾取器 (ColorPicker 已有，需集成)
  - [ ] 多显示器切换
- [ ] **Dashboard 页面增强** (`dashboard/+page.svelte`)
  - [ ] 截图实时推送（当前只轮询系统状态）
  - [ ] CPU/内存使用率图表
  - [ ] 触发器实时触发事件流
- [ ] **IPC 连接稳定性**
  - [ ] 断线自动重连 (connection store)
  - [ ] 连接状态 UI 指示器增强
  - [ ] IPC 调用超时处理
- [ ] **Settings 页面增强** (`settings/+page.svelte`)
  - [ ] IPC 端点配置 (已有基础)
  - [ ] 远程 Orchestrator 地址配置
  - [ ] 开机自启设置
  - [ ] 日志级别配置
- [ ] **Logs 页面增强** (`logs/+page.svelte` — 当前仅内存日志)
  - [ ] 接收 IPC 推送的 runtime 日志
  - [ ] 日志级别过滤 (info/warn/error)
  - [ ] 日志搜索
  - [ ] 日志导出

### Orchestrator Dashboard (React + Ant Design Pro)

**已有页面**: Welcome, Agents, Monitor, Scripts, Workflows, Admin(LoginLogs, OperationLogs), User(Login), Profile

- [ ] **Monitor 页面修复** (`Monitor/index.tsx` — import 有误)
  - [ ] 修复从 `@ant-design/icons` 导入非 icon 组件的错误 (Button, Card, Switch 等应从 antd 导入)
  - [ ] 截图实时更新 (ScreenshotView 组件已存在)
  - [ ] 触发器状态面板
  - [ ] 性能指标图表 (CPU/内存/网络)
- [ ] **Agents 页面完善** (`Agents/index.tsx` — 框架已有)
  - [ ] Agent 实时状态推送 (wsService 已接入)
  - [ ] Agent 远程控制 (启动/停止脚本)
  - [ ] Agent 截图查看
  - [ ] Agent 分组/标签管理
- [ ] **Scripts 页面完善** (`Scripts/index.tsx` — 框架已有)
  - [ ] Monaco 编辑器集成 (依赖已安装 `@monaco-editor/react`)
  - [ ] 脚本在线编辑和保存
  - [ ] 脚本分发到指定 Agent
  - [ ] 脚本运行日志查看
- [ ] **Workflows 页面完善** (`Workflows/index.tsx` — 框架已有)
  - [ ] 工作流创建/编辑 UI (ModalForm 已集成)
  - [ ] 工作流步骤可视化 (Steps 组件已有)
  - [ ] 工作流执行状态实时更新
  - [ ] 工作流模板库
- [ ] **权限系统** (Admin 页面框架已有)
  - [ ] RBAC 模型实现 (后端)
  - [ ] 前端权限路由守卫
  - [ ] 角色/权限管理 UI
- [ ] **WebSocket 服务** (wsService 已实现基础)
  - [ ] 事件类型完善 (agent/screenshot/workflow/script 事件)
  - [ ] 连接状态管理增强
  - [ ] 断线重连

### Go Orchestrator 后端

**已有**: handlers(auth, script, status, window, screenshot, debugger, settings), models, config, middleware(auth, cors), websocket hub, agent client

- [ ] **工作流引擎** (缺失 — 最核心功能)
  - [ ] Workflow 模型定义
  - [ ] 步骤依赖解析 (DAG)
  - [ ] 工作流调度器 (串行/并行)
  - [ ] 步骤执行器 (script/screenshot/wait/condition)
  - [ ] 工作流状态持久化
  - [ ] 超时和错误处理
- [ ] **Agent 管理增强**
  - [ ] Agent 注册/注销服务
  - [ ] 心跳检测 + 超时判定
  - [ ] Agent 状态同步 (CPU/内存/脚本状态)
  - [ ] Agent 负载均衡 (工作流分配策略)
- [ ] **权限系统**
  - [ ] User/Role/Permission 模型
  - [ ] JWT token 验证 (已有基础 auth middleware)
  - [ ] API 权限中间件
- [ ] **通知系统**
  - [ ] WebSocket 事件推送 (hub 已实现基础)
  - [ ] Agent 事件 → Dashboard 广播
  - [ ] 通知历史记录
- [ ] **API 完善**
  - [ ] Workflow CRUD API
  - [ ] Agent 管理 API 补充
  - [ ] 权限管理 API
  - [ ] API 文档 (Swagger/OpenAPI)

---

## 🟡 P1 - 高优先级（功能完善）

### Runtime IPC 增强

**已有**: Named Pipe (Windows) / Unix Domain Socket (macOS/Linux), IPC factory, RPC handlers (system, trigger)

- [ ] **IPC 事件推送**
  - [ ] 日志事件推送 (runtime → GUI)
  - [ ] 截图事件推送
  - [ ] 触发器触发事件推送
  - [ ] 脚本状态变更事件推送
- [ ] **新增 RPC Handler**
  - [ ] `screenshot_handler` — 截图控制
  - [ ] `script_handler` — 脚本管理 (start/stop/list)
  - [ ] `window_handler` — 窗口管理
  - [ ] `profile_handler` — 配置管理
- [ ] **Agent Outbound 连接增强**
  - [ ] 心跳保活机制
  - [ ] 断线重连策略 (指数退避)
  - [ ] 连接状态回调
  - [ ] 命令队列 (离线缓存)

### 跨平台验证

- [ ] **macOS** (基础实现已存在)
  - [ ] Unix Domain Socket 验证
  - [ ] Clipboard 验证
  - [ ] Capture (CGWindowListCreateImage) 验证
  - [ ] FileWatcher 验证
  - [ ] 输入模拟 (CGEvent) 验证
- [ ] **Linux** (基础实现已存在)
  - [ ] Unix Domain Socket 验证
  - [ ] X11/XTest 输入模拟验证
  - [ ] inotify 文件监控验证
  - [ ] Clipboard (xclip) 验证
  - [ ] 截图 (XGetImage) 验证

### 配置和协议统一

- [ ] 统一 IPC 协议格式 (JSON-RPC over Named Pipe/UDS)
- [ ] 统一 Agent-Orchestrator 通信协议 (TCP 二进制帧 + JSON body)
- [ ] 统一配置文件格式和路径 (TOML/YAML)
- [ ] 清理 `WINGMAN_BUILD_SERVER` / `WINGMAN_BUILD_DEBUGGER` 旧路径

---

## 🟢 P2 - 中优先级（功能增强）

### 高级功能

- [ ] **OCR 支持**
  - [ ] Tesseract 集成 (vcpkg 管理)
  - [ ] 文本识别 API (IPC + RPC)
  - [ ] 多语言支持
- [ ] **ML/AI 支持**
  - [ ] ONNX Runtime 集成
  - [ ] 图像识别模型推理
  - [ ] 自定义模型加载
- [ ] **宏系统 UI**
  - [ ] 宏录制界面 (Tauri GUI)
  - [ ] 宏编辑器 (时间轴视图)
  - [ ] 宏回放可视化
- [ ] **游戏配置模板**
  - [ ] 配置模板库
  - [ ] 配置导入/导出
  - [ ] 配置版本管理

### 用户体验

- [ ] **快捷键支持**
  - [ ] 全局快捷键注册
  - [ ] 快捷键冲突检测
  - [ ] 自定义快捷键配置
- [ ] **系统托盘**
  - [ ] 最小化到托盘
  - [ ] 托盘菜单 (状态/启停/退出)
  - [ ] 托盘通知
- [ ] **主题系统**
  - [ ] 暗色/亮色主题切换
  - [ ] 自定义主题色
  - [ ] 紧凑/舒适布局

---

## 🔵 P3 - 低优先级（工程优化）

### 测试

- [ ] **单元测试补充**
  - [ ] IPC 协议测试
  - [ ] RPC Handler 测试
  - [ ] Dashboard 组件测试 (已有 login.test.tsx)
  - [ ] Tauri command 测试
- [ ] **集成测试**
  - [ ] GUI → IPC → Runtime 端到端测试
  - [ ] Agent → Orchestrator 通信测试
  - [ ] Dashboard → API → Orchestrator 端到端测试
  - [ ] 多 Agent 协同测试
- [ ] **性能测试**
  - [ ] 截图性能基准
  - [ ] WebSocket 并发连接测试
  - [ ] 工作流调度性能测试

### 文档

- [ ] **用户文档**
  - [ ] 快速开始指南
  - [ ] VSCode 开发环境配置指南 (Lua EmmyLua 调试 + Python 调试 + 补全)
  - [ ] Runtime GUI 使用教程
  - [ ] Dashboard 使用教程
  - [ ] 脚本开发指南 (Lua API)
  - [ ] FAQ
- [ ] **开发文档**
  - [ ] 架构设计文档更新 (docs/architecture.md)
  - [ ] IPC 协议规范
  - [ ] Agent-Orchestrator 协议规范
  - [ ] 贡献指南

### 构建和部署

- [ ] **CI/CD**
  - [ ] GitHub Actions: Windows 构建
  - [ ] GitHub Actions: macOS/Linux 构建
  - [ ] 代码覆盖率报告
  - [ ] 自动发布 (tag → release)
- [ ] **打包分发**
  - [ ] Windows 安装包 (MSIX / InnoSetup)
  - [ ] macOS dmg 包
  - [ ] Linux AppImage
  - [ ] Docker 镜像 (Orchestrator)

---

## 📊 各模块实际完成度

| 模块 | 子功能 | 完成度 | 说明 |
|------|--------|--------|------|
| **Runtime (C++)** | 核心引擎 | 95% | screen/input/trigger/vision/btree/macro/human 全部完成 |
| | IPC Server | 75% | Named Pipe + UDS 已实现，缺少事件推送 |
| | RPC Handlers | 40% | 仅 system + trigger，缺 script/screenshot/window |
| | Agent Client | 70% | outbound TCP 已实现，缺心跳重连 |
| | 脚本引擎 | 100% | Lua (sol2) + Python (pybind11) 双语言 |
| **GUI (Tauri)** | 框架 | 100% | Tauri 2.0 + Svelte 5 |
| | 页面实现 | 70% | dashboard/scripts/triggers/settings/logs 框架完成，编辑器改用 VSCode |
| | 组件库 | 55% | RegionPicker + ColorPicker，不建编辑器组件，缺预览组件 |
| | IPC 通信 | 80% | 6 个 command 模块，缺事件监听 |
| **Orchestrator (Go)** | HTTP API | 80% | 7 个 handler，缺 workflow API |
| | WebSocket | 70% | Hub 基础完成，缺事件广播 |
| | Agent Client | 75% | TCP 二进制协议已实现，缺心跳管理 |
| | 工作流引擎 | 0% | 完全缺失，核心功能 |
| | 权限系统 | 30% | auth middleware 已有，RBAC 未实现 |
| **Dashboard (React)** | 页面框架 | 85% | 7 个主要页面已搭建 |
| | 组件实现 | 60% | ProTable/ProCard 框架有，具体功能需填充 |
| | WebSocket | 70% | wsService 已实现，事件类型待完善 |
| | API 对接 | 70% | services 层已有，部分 API 未对接 |

---

## 🗓️ 迭代计划

### Sprint 1 (2周) — Runtime GUI 补全
- [ ] 删除 editor 占位页面，提供 VSCode 配置文件 (.vscode/)
- [ ] 屏幕预览组件: 实时截图 + 区域/颜色选择
- [ ] Logs 页面: 接收 IPC 推送日志，级别过滤
- [ ] Dashboard 页面: 状态图表增强

### Sprint 2 (2周) — Dashboard 功能填充
- [ ] Monitor 页面修复 + 截图实时更新
- [ ] Scripts 页面: Monaco 编辑器 + 脚本分发
- [ ] Agents 页面: 实时状态 + 远程控制
- [ ] Workflows 页面: 创建/编辑/执行可视化

### Sprint 3 (2周) — Go Orchestrator 核心功能
- [ ] 工作流引擎: 模型 + 调度器 + 步骤执行
- [ ] Agent 管理增强: 心跳 + 状态同步
- [ ] 权限系统: RBAC 模型 + API
- [ ] WebSocket 事件广播完善

### Sprint 4 (2周) — Runtime IPC 增强
- [ ] 事件推送机制 (日志/截图/触发器/脚本)
- [ ] 新增 RPC Handler (script/screenshot/window)
- [ ] Agent outbound 心跳重连
- [ ] 跨平台验证 (macOS/Linux)

### Sprint 5 (2周) — 高级功能 + 收尾
- [ ] OCR 集成 (可选)
- [ ] 宏录制 UI (可选)
- [ ] 系统托盘 + 主题切换
- [ ] 测试补充 + 文档更新
- [ ] CI/CD 流水线

---

## 📝 注意事项

1. **架构优先**: 修改 runtime/orchestrator 前，先看 `docs/architecture-decisions.md`
2. **IPC 边界**: GUI 只能通过 IPC 控制 runtime，禁止 runtime 开 HTTP/WS server
3. **远程链路**: Dashboard → Go server → runtime (outbound)，Dashboard 不直连 runtime
4. **vcpkg 约束**: 所有 C++ 依赖必须走 vcpkg x64-windows-static
5. **测试基线**: 当前 1584 个测试 / 90.04% 覆盖率，新增代码需保持水准

---

## 🔗 相关文档

- [ROADMAP.md](./ROADMAP.md) — 项目开发路线图
- [docs/architecture.md](./docs/architecture.md) — 架构设计文档
- [docs/architecture-decisions.md](./docs/architecture-decisions.md) — 架构决策记录
- [docs/API.md](./docs/API.md) — API 文档
