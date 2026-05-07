# Wingman HTTP Server 重构方案

## 目标

用 Go + Gin + GORM 替换现有 C++ Crow HTTP Server

## 架构设计

```
┌─────────────────┐
│  Web Dashboard  │
│   (Frontend)    │
└────────┬────────┘
         │ HTTP/WebSocket
         ↓
┌─────────────────────────────────────┐
│   Go HTTP Server (Gin + GORM)      │
│   - REST API                        │
│   - WebSocket                      │
│   - Static Files                   │
│   - JWT Auth                       │
└────────┬────────────────────────────┘
         │ gRPC / TCP
         ↓
┌─────────────────────────────────────┐
│   C++ Agent (独立进程)              │
│   - wingman-agent                   │
│   - Screen/Input/Window/Script...   │
└─────────────────────────────────────┘

┌─────────────────────────────────────┐
│   C++ TCP Server (server/)          │
│   - Agent 间通信                    │
│   - 工作流编排                      │
└─────────────────────────────────────┘
```

## 目录结构

```
wingman/
├── go-http/                    # Go HTTP Server (新建)
│   ├── cmd/
│   │   └── server/
│   │       └── main.go
│   ├── internal/
│   │   ├── handlers/          # API 处理器
│   │   │   ├── auth.go
│   │   │   ├── script.go
│   │   │   ├── debugger.go
│   │   │   └── status.go
│   │   ├── middleware/        # 中间件
│   │   │   ├── cors.go
│   │   │   └── auth.go
│   │   ├── models/            # GORM 模型
│   │   │   └── models.go
│   │   └── agent/             # gRPC 客户端
│   │       └── client.go
│   ├── pkg/
│   │   └── websocket/         # WebSocket Hub
│   │       └── hub.go
│   ├── configs/
│   │   └── config.yaml
│   ├── go.mod
│   └── go.sum
├── server/                     # C++ TCP Server (保持不变)
│   ├── include/wingman/server/
│   └── src/
├── agent/                      # C++ Agent (保持不变)
└── build/dist/                 # 前端静态文件
```

## 实施步骤

### Phase 1: Go 项目基础结构
- [x] 创建目录结构
- [ ] 初始化 go.mod
- [ ] 定义 GORM 模型
- [ ] 实现 gRPC 客户端封装

### Phase 2: 核心 API 实现
- [ ] 认证 API (登录/登出/JWT)
- [ ] 脚本管理 API (CRUD)
- [ ] 调试器 API
- [ ] 状态查询 API

### Phase 3: WebSocket 实现
- [ ] WebSocket Hub
- [ ] 房间管理
- [ ] 事件广播

### Phase 4: 集成与部署
- [ ] 前端静态文件服务
- [ ] 与 C++ Agent 联调
- [ ] 构建脚本
- [ ] 移除旧 C++ HTTP Server

## API 路由设计

```
POST   /api/v1/auth/login       - 登录
POST   /api/v1/auth/logout      - 登出

GET    /api/v1/status           - 系统状态
GET    /api/v1/scripts          - 获取脚本列表
POST   /api/v1/scripts          - 创建脚本
DELETE /api/v1/scripts          - 删除脚本
POST   /api/v1/scripts/run      - 运行脚本
POST   /api/v1/scripts/stop     - 停止脚本
GET    /api/v1/scripts/logs     - 获取日志

GET    /api/v1/debugger/status  - 调试器状态
POST   /api/v1/debugger/command - 调试命令

WS     /ws                      - WebSocket 连接
```

## C++ gRPC 服务

复用 `proto/agent_api.proto` 定义的接口：

```protobuf
service AgentService {
    rpc Ping(Empty) returns (PingResponse);
    rpc RunScript(RunScriptRequest) returns (RunScriptResponse);
    rpc StopScript(StopScriptRequest) returns (Response);
    rpc ListScripts(Empty) returns (ListScriptsResponse);
    // ...
}
```

## 数据持久化

使用 GORM + SQLite：

- users (用户)
- scripts (脚本)
- settings (配置)
- execution_logs (执行日志)

## 配置文件

`go-http/configs/config.yaml`:

```yaml
server:
  port: 9527
  host: "0.0.0.0"

agent:
  address: "localhost:50051"  # gRPC 地址
  timeout: 30s

database:
  path: "./data/wingman.db"

jwt:
  secret: "wingman-secret-key"
  # expire: 24h

static:
  dir: "../build/dist"

cors:
  allow_origins: ["*"]
  allow_methods: ["GET", "POST", "PUT", "DELETE", "OPTIONS"]
  allow_headers: ["Content-Type", "Authorization"]
```

## 构建与运行

```bash
# 1. 编译 C++ Agent
cd build && cmake --build . --target wingman-agent

# 2. 编译 Go HTTP Server
cd go-http && go build -o ../build/go-server.exe ./cmd/server

# 3. 运行
./build/wingman-agent.exe     # C++ Agent (端口 50051)
./build/go-server.exe         # Go HTTP Server (端口 9527)
```

## 依赖项

Go 依赖：
- github.com/gin-gonic/gin
- github.com/gin-contrib/cors
- github.com/gorilla/websocket
- google.golang.org/grpc
- gorm.io/gorm
- gorm.io/driver/sqlite
- github.com/golang-jwt/jwt/v5

## 备注

- C++ TCP Server (server/) 保持不变，用于 Agent 间通信
- C++ Agent (agent/) 暴露 gRPC 服务供 Go HTTP Server 调用
- 前端 Dashboard 只与 Go HTTP Server 通信
