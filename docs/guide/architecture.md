# 架构设计

## 系统架构概览

```mermaid
graph TB
    subgraph "用户层"
        UI[Web Dashboard<br/>监控与控制]
        CLI[CLI Client<br/>命令行工具]
    end

    subgraph "中控机 Server"
        Orchestrator[任务编排器<br/>Orchestrator]
        AgentMgr[客户端管理<br/>AgentManager]
        TCPServer[TCP Server<br/>asio 异步]
        Storage[状态存储<br/>KV Store]
    end

    subgraph "受控机 1"
        Client1[TCP Client]
        Lua1[Lua Engine]
        Core1[C++ Core]
    end

    subgraph "受控机 2"
        Client2[TCP Client]
        Lua2[Lua Engine]
        Core2[C++ Core]
    end

    subgraph "受控机 N"
        ClientN[TCP Client]
        LuaN[Lua Engine]
        CoreN[C++ Core]
    end

    UI -->|WebSocket| TCPServer
    CLI -->|TCP| TCPServer
    TCPServer --> AgentMgr
    TCPServer --> Orchestrator
    Orchestrator --> Storage

    TCPServer -->|TCP 长连接| Client1
    TCPServer -->|TCP 长连接| Client2
    TCPServer -->|TCP 长连接| ClientN

    Client1 --> Lua1
    Client2 --> Lua2
    ClientN --> LuaN

    Lua1 --> Core1
    Lua2 --> Core2
    LuaN --> CoreN

    style Orchestrator fill:#f9f,stroke:#333,stroke-width:2px
    style TCPServer fill:#bbf,stroke:#333,stroke-width:2px
```

## 中控机-受控机架构

### 通信协议

```mermaid
sequenceDiagram
    participant C as 受控机 Client
    participant S as 中控机 Server

    C->>S: TCP 连接
    S-->>C: 连接建立

    Note over C,S: 注册阶段
    C->>S: Register(agentId, hostname)
    S-->>C: OK(code=0)

    Note over C,S: 心跳保活
    loop 每 15 秒
        C->>S: Heartbeat(status, currentTask)
        S-->>C: Pong
    end

    Note over C,S: 任务执行
    S->>C: ExecuteTask(script, params)
    C->>C: Lua 执行
    C->>S: ReportProgress(stepId, progress)
    C->>S: TaskComplete(result)
    S-->>C: ACK

    Note over C,S: 断线处理
    C--xS: 连接断开
    S->>S: 标记 Agent 离线
    C->>C: 自动重连 (5秒后)
    C->>S: TCP 重连
    C->>S: Register(重新注册)
```

### TCP 消息格式

```mermaid
graph LR
    subgraph "请求消息"
        A[Request] --> B[type: 消息类型]
        A --> C[id: 请求ID]
        A --> D[timestamp: 时间戳]
        A --> E[agent_id: 发送者ID]
        A --> F[data: 业务数据JSON]
    end

    subgraph "响应消息"
        G[Response] --> H[request_id: 对应请求ID]
        G --> I[code: 错误码]
        G --> J[timestamp: 响应时间戳]
        G --> K[message: 可读描述]
        G --> L[data: 业务数据JSON]
    end

    style A fill:#e1f5ff
    style G fill:#fff4e1
```

#### 信封协议

```
┌─────────────────────────────────────────────────────────┐
│  长度(16进制)\n                                          │
│  {"type":"heartbeat","id":"xxx","timestamp":...}\n      │
└─────────────────────────────────────────────────────────┘
```

#### 错误码定义

| Code | 名称 | 说明 |
|------|------|------|
| 0 | OK | 成功 |
| 1 | UNKNOWN | 未知错误 |
| 2 | INVALID_REQUEST | 请求格式错误 |
| 3 | NOT_FOUND | 资源未找到 |
| 4 | TIMEOUT | 操作超时 |
| 5 | BUSY | 服务忙碌 |
| 6 | NOT_AUTHORIZED | 未授权 |
| 7 | ALREADY_EXISTS | 资源已存在 |
| 8 | FAILED | 操作失败 |
| 9 | DISCONNECTED | 连接断开 |
| 10 | RATE_LIMITED | 请求频率限制 |
| **1024+** | **用户自定义** | **业务错误码** |

## 多账号协作编排

### 工作流模型

```mermaid
graph TB
    subgraph "工作流定义"
        WF[Workflow]
        S1[Step 1: 登录]
        S2[Step 2: 任务A]
        S3[Step 3: 任务B]
        S4[Step 4: 结算]

        WF --> S1
        S1 --> S2
        S1 --> S3
        S2 --> S4
        S3 --> S4
    end

    subgraph "Agent 分配"
        S1 --> A1[Agent1, Agent2, Agent3]
        S2 --> A2[Agent1, Agent2]
        S3 --> A3[Agent3]
    end

    subgraph "执行流程"
        B1[屏障同步: 所有Agent完成Step1] --> S2
        B2[屏障同步: 所有Agent完成Step2/3] --> S4
    end

    style WF fill:#f9f,stroke:#333,stroke-width:2px
    style B1 fill:#ff9,stroke:#333,stroke-width:2px
    style B2 fill:#ff9,stroke:#333,stroke-width:2px
```

### 屏障同步机制

```mermaid
stateDiagram-v2
    [*] --> Pending: 提交工作流

    Pending --> Step1_Running: 启动 Step 1
    Step1_Running --> Step1_Complete: Agent1 完成
    Step1_Running --> Step1_Complete: Agent2 完成
    Step1_Running --> Step1_Complete: Agent3 完成

    Step1_Complete --> Barrier_Waiting: 等待所有 Agent
    Barrier_Waiting --> Step2_Running: 屏障通过

    Step2_Running --> Step2_Complete: Agent1 完成
    Step2_Running --> Step2_Complete: Agent2 完成

    Step2_Complete --> Barrier_Waiting2: 等待所有 Agent
    Barrier_Waiting2 --> Step3_Running: 屏障通过

    Step3_Running --> [*]: 工作流完成

    note right of Barrier_Waiting
        屏障同步: 等待该步骤
        所有分配的 Agent 完成
    end note
```

### Agent 状态机

```mermaid
stateDiagram-v2
    [*] --> Disconnected: 初始状态

    Disconnected --> Connecting: 发起连接
    Connecting --> Registering: TCP 连接成功
    Registering --> Idle: 注册成功

    Idle --> Busy: 接收任务
    Busy --> Idle: 任务完成
    Busy --> Error: 任务失败

    Idle --> Disconnected: 心跳超时
    Busy --> Disconnected: 心跳超时
    Error --> Idle: 恢复

    Registering --> Disconnected: 注册失败
    Connecting --> Disconnected: 连接失败

    Disconnected --> Connecting: 自动重连

    note right of Idle
        空闲状态，等待任务
    end note

    note right of Busy
        执行任务中，定期上报进度
    end note
```

## 核心模块

### 中控机模块

| 模块 | 职责 | 文件 |
|-----|------|------|
| **TCPServer** | TCP 长连接服务 | `server/src/server.cpp` |
| **AgentManager** | 客户端会话管理 | `server/src/agent_manager.cpp` |
| **Orchestrator** | 任务编排引擎 | `server/src/orchestrator.cpp` |
| **WorkflowStore** | 工作流状态存储 | `server/src/workflow_store.cpp` |

### 受控机模块

| 模块 | 职责 | 文件 |
|-----|------|------|
| **TCPClient** | TCP 客户端 | `server/src/client.cpp` |
| **Heartbeat** | 心跳保活 | `server/src/heartbeat.cpp` |
| **TaskExecutor** | 任务执行器 | `server/src/task_executor.cpp` |

### 核心能力模块

| 模块 | 职责 | 文件 |
|-----|------|------|
| **Screen** | 屏幕操作 | `src/screen.cpp` |
| **Input** | 输入模拟 | `src/input.cpp` |
| **Window** | 窗口管理 | `src/window.cpp` |
| **Process** | 进程管理 | `src/process.cpp` |
| **Recorder** | 宏录制 | `src/recorder.cpp` |
| **Trigger** | 触发器系统 | `src/trigger.cpp` |
| **Storage** | 存储系统 | `src/storage.cpp` |
| **Verification** | 验证码能力 | `src/verification.cpp` |
| **QRCode** | 二维码登录 | `src/qrcode.cpp` |

## C++ / Lua 分层

```
┌─────────────────────────────────────────────────────────────────┐
│                    分层决策                                      │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│   C++ 实现                    Lua 实现                         │
│   ────────                    ─────────                         │
│                                                                  │
│   ├── 性能敏感操作                ├── 业务逻辑                  │
│   ├── 系统调用                    ├── 状态机                    │
│   ├── 内存操作                    ├── 触发器组合                │
│   ├── 图像处理                    ├── 用户自定义行为            │
│   └── TCP/网络通信                └── 脚本编排                  │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

## 目录结构

```
wingman/
├── .github/workflows/       # CI/CD 配置
├── docs/                    # VitePress 文档
├── server/                  # 网络服务层
│   ├── include/wingman/server/
│   │   ├── server.hpp       # TCP Server
│   │   ├── client.hpp       # TCP Client
│   │   ├── protocol.hpp     # 通信协议
│   │   └── orchestrator.hpp # 任务编排
│   └── src/
├── src/                     # C++ 核心引擎
├── include/wingman/         # 公共头文件
├── bindings/                # Lua 绑定
├── scripts/                 # Lua 脚本示例
└── tests/                   # 测试
```
