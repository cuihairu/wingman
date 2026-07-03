---
title: Dashboard 使用教程
---

# Dashboard 使用教程

## 概述

Wingman Dashboard 是基于 React + Ant Design Pro 构建的远程管理界面，通过连接 Go Orchestrator Server 实现对多个 Runtime Agent 的集中监控和管理。

## 架构关系

```
┌─────────────────┐     HTTP/WS      ┌───────────────────┐     TCP      ┌─────────────────┐
│  Dashboard      │ ◄──────────────► │  Go Orchestrator  │ ◄──────────► │  Runtime Agent  │
│  (React)        │                  │  Server           │              │  (C++)          │
└─────────────────┘                  └───────────────────┘              └─────────────────┘
```

**重要**：Dashboard 只连接 Go Server，不直接连接 Runtime Agent。

## 访问方式

### 启动 Go Orchestrator Server

```bash
cd orchestrator/server
go run main.go
```

默认监听端口：`8080`

### 访问 Dashboard

1. **开发模式**：
   ```bash
   cd orchestrator/dashboard
   pnpm install
   pnpm start
   ```
   访问 `http://localhost:8000`

2. **生产模式**：
   构建后访问 `http://<server-ip>:8080`

## 用户认证

### 登录

1. 访问 Dashboard 首页，自动跳转到登录页
2. 输入用户名和密码
3. 点击「登录」按钮

### 默认账号

| 用户名 | 密码 | 角色 |
|--------|------|------|
| admin | admin123 | 管理员（全部权限） |

> ⚠️ 首次登录后请立即修改默认密码

## 页面功能

### 1. Welcome（欢迎页）

- 显示系统概览信息
- 快速入口链接
- 系统状态摘要

### 2. Agents（Agent 管理）

管理和监控所有已连接的 Runtime Agent。

#### 功能列表

- **Agent 列表**：显示所有已注册的 Agent 及其状态
- **实时状态**：通过 WebSocket 实时更新 Agent 在线/离线状态
- **Agent 详情**：查看 Agent 的系统信息、资源占用
- **标签管理**：为 Agent 设置分组标签（需要 `agents:manage` 权限）
- **远程关闭**：发送关闭命令到 Agent（需要 `agents:manage` 权限）

#### Agent 状态说明

| 状态 | 颜色 | 说明 |
|------|------|------|
| Online | 🟢 绿色 | Agent 在线，可接收命令 |
| Offline | 🔴 红色 | Agent 离线或心跳超时 |
| Busy | 🟡 黄色 | Agent 正在执行任务 |

#### 标签管理

1. 点击 Agent 行的「标签」列
2. 在弹出的 Popover 中添加/移除标签
3. 标签用于工作流负载均衡和分组筛选

### 3. Scripts（脚本管理）

管理远程脚本库，支持在线编辑和执行。

#### 功能列表

- **脚本列表**：查看所有已上传的脚本
- **创建脚本**：新建 Lua/Python 脚本
- **在线编辑**：使用 Monaco 编辑器编辑脚本
- **执行脚本**：选择目标 Agent 运行脚本
- **查看日志**：实时查看脚本执行输出

#### 创建脚本

1. 点击「新建脚本」按钮
2. 填写脚本名称和描述
3. 选择脚本语言（Lua/Python）
4. 编写脚本内容
5. 点击「保存」

#### 运行脚本

1. 在脚本列表中找到目标脚本
2. 点击「运行」按钮
3. 选择要执行的 Agent
4. 查看执行日志

### 4. Workflows（工作流管理）

创建和管理复杂的自动化工作流。

#### 功能列表

- **工作流列表**：查看所有工作流定义
- **创建工作流**：使用 DAG 图形定义工作流
- **模板选择**：从内置模板快速创建工作流
- **提交执行**：将工作流提交到 Agent 执行
- **执行监控**：查看工作流执行状态和进度

#### 工作流步骤类型

| 类型 | 说明 |
|------|------|
| script | 执行 Lua/Python 脚本 |
| wait | 等待指定时间 |
| condition | 条件判断分支 |
| screenshot | 截图并上传 |

#### 内置模板

- **单步监控**：单个脚本定期执行
- **并行采集**：多个 Agent 并行执行相同任务
- **串行流水线**：步骤按顺序依次执行
- **Fan-out 汇总**：分发任务并汇总结果
- **带 Wait 节拍**：包含等待步骤的定时任务

### 5. Monitor（监控面板）

实时监控系统运行状态。

#### 功能列表

- **触发器事件**：实时显示触发器触发记录（WebSocket 推送）
- **脚本输出**：查看脚本的标准输出
- **系统指标**：Agent 资源使用情况

> 注：触发器列表通过 WebSocket 事件驱动更新，上限显示 20 条记录

### 6. Admin（管理后台）

需要管理员权限访问。

#### 6.1 Users（用户管理）

- **用户列表**：查看所有注册用户
- **创建用户**：添加新用户账号
- **编辑用户**：修改用户信息、重置密码
- **角色分配**：为用户分配角色
- **启用/禁用**：控制用户账号状态

#### 6.2 Roles（角色管理）

- **角色列表**：查看所有角色
- **创建角色**：添加自定义角色
- **权限分配**：为角色分配权限
- **内置角色保护**：admin/operator/viewer 内置角色不可删除

#### 6.3 Login Logs（登录日志）

- 查看用户登录历史记录
- 支持按时间、用户筛选
- CSV 导出功能

#### 6.4 Operation Logs（操作日志）

- 查看系统操作审计记录
- 支持按时间、用户、操作类型筛选
- CSV 导出功能

### 7. Settings（系统设置）

配置 Go Orchestrator Server 参数。

#### 可配置项

- **日志级别**：控制日志输出详细程度
- **并发上限**：最大并发任务数
- **端口配置**：服务监听端口（只读）

> 注：部分配置需要管理员权限才能修改

### 8. Profile（个人资料）

管理当前用户的个人信息。

#### 功能标签页

- **基本信息**：修改昵称、邮箱、手机号
- **头像设置**：上传/修改头像
- **密码修改**：修改登录密码
- **安全设置**：两步验证等
- **通知偏好**：配置通知方式
- **主题设置**：切换亮/暗主题
- **语言设置**：界面语言选择

### 9. Support/Feedback（支持反馈）

- **提交反馈**：报告问题或提出建议
- **权限申请**：申请额外权限
- **反馈历史**：查看已提交的反馈

### 10. Account/Messages（站内信）

- **消息列表**：查看系统通知和站内信
- **已读标记**：标记消息为已读
- **筛选功能**：按已读/未读状态筛选

## WebSocket 实时事件

Dashboard 通过 WebSocket 接收实时事件推送：

| 事件类型 | 说明 |
|----------|------|
| agent_connected | Agent 上线 |
| agent_disconnected | Agent 下线 |
| trigger_fired | 触发器触发 |
| script_state | 脚本状态变更 |
| script_output | 脚本输出 |
| workflow_progress | 工作流进度更新 |

## 权限系统

Dashboard 使用 RBAC 权限控制：

### 内置角色

| 角色 | 说明 |
|------|------|
| admin | 管理员，拥有所有权限 |
| operator | 操作员，可执行任务和查看 |
| viewer | 只读用户，仅可查看 |

### 权限码

权限格式：`resource:action`

常用权限码：
- `agents:view` - 查看 Agent 列表
- `agents:manage` - 管理 Agent（标签、关闭）
- `scripts:view` - 查看脚本
- `scripts:edit` - 编辑脚本
- `scripts:run` - 运行脚本
- `workflows:view` - 查看工作流
- `workflows:run` - 执行工作流
- `users:view` - 查看用户列表
- `users:manage` - 管理用户
- `roles:view` - 查看角色列表
- `roles:manage` - 管理角色
- `settings:view` - 查看设置
- `settings:edit` - 修改设置

## 常见问题

### Q: Dashboard 无法连接到 Server？

1. 检查 Go Orchestrator Server 是否正常运行
2. 确认网络连接是否正常
3. 检查防火墙设置是否允许对应端口

### Q: Agent 显示离线但实际在运行？

1. 检查 Agent 是否能正常连接到 Go Server
2. 查看 Agent 日志是否有连接错误
3. 确认 Agent 的 `agent.toml` 配置正确

### Q: 如何修改 Dashboard 端口？

Dashboard 端口由 Go Server 控制，修改 Server 配置文件中的端口设置。

### Q: 忘记管理员密码？

需要直接访问数据库重置密码，或重新初始化数据库。
