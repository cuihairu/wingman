# Wingman HTTP Server (Go)

用 Go + Gin + GORM 替换 C++ Crow HTTP Server

## 目录结构

```
server/
├── go.mod              # Go module 定义
├── main.go             # 入口文件
├── internal/           # 内部包
│   ├── handlers/       # HTTP 处理器
│   ├── middleware/     # 中间件 (CORS, JWT)
│   └── models/         # GORM 数据模型
├── pkg/                # 公共包
│   ├── agent/          # C++ Client HTTP 客户端
│   └── websocket/      # WebSocket Hub
├── include/            # C++ 头文件 (保留)
├── src/                # C++ 源文件 (保留)
└── proto/              # Protobuf 定义 (保留)
```

## 构建

```bash
cd server
go build -o ../build/go-server.exe .
```

## 运行

```bash
# 1. 启动 C++ Client (监听 8888 端口)
./build/wingman-agent.exe

# 2. 启动 Go HTTP Server (监听 9527 端口)
./build/go-server.exe
```

## API 端点

### 认证
- `POST /api/v1/auth/login` - 登录
- `POST /api/v1/auth/logout` - 登出

### 脚本管理 (需要认证)
- `GET /api/v1/scripts` - 获取脚本列表
- `POST /api/v1/scripts` - 创建脚本
- `DELETE /api/v1/scripts` - 删除脚本
- `POST /api/v1/scripts/content` - 获取脚本内容
- `POST /api/v1/scripts/save` - 保存脚本
- `POST /api/v1/scripts/run` - 运行脚本
- `POST /api/v1/scripts/stop` - 停止脚本
- `POST /api/v1/scripts/logs` - 获取脚本日志

### 系统状态 (需要认证)
- `GET /api/v1/status` - 系统状态
- `GET /api/v1/health` - 健康检查
- `GET /api/v1/windows` - 窗口列表
- `GET /api/v1/settings` - 获取配置
- `PUT /api/v1/settings` - 更新配置

### 调试器 (无需认证)
- `POST /api/debugger/connect` - 连接调试器
- `POST /api/debugger/command` - 调试命令
- `GET /api/debugger/breakpoints` - 获取断点
- `POST /api/debugger/breakpoints` - 设置断点

### WebSocket
- `WS /ws` - WebSocket 连接

## 数据库

使用 GORM + SQLite，数据存储在 `./data/wingman.db`

表结构：
- `users` - 用户
- `scripts` - 脚本
- `settings` - 配置
- `execution_logs` - 执行日志

默认管理员账户：
- 用户名: `admin`
- 密码: `admin123`
