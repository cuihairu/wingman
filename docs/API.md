# Wingman Runtime API 文档

Wingman Runtime 同时提供 HTTP RESTful API 和 WebSocket RPC 接口，供 Tauri UI 或其他客户端调用。

## 启动服务器

```bash
# 默认配置
wingman-runtime start

# 指定配置文件
wingman-runtime start --config agent.toml
```

> 主机和端口在 `agent.toml` 中配置，参见 `server_ip` / `server_port` 字段。

## HTTP RESTful API

### 基础信息

| 项目 | 值 |
|------|-----|
| 基础 URL | `http://127.0.0.1:8080` |
| Content-Type | `application/json` |
| 响应格式 | JSON |

### 通用响应格式

**成功响应:**
```json
{
  "success": true,
  "data": { ... }
}
```

**错误响应:**
```json
{
  "success": false,
  "error": "错误信息"
}
```

### API 端点

#### 1. 首页信息
```
GET /
```

**响应示例:**
```json
{
  "success": true,
  "data": {
    "name": "Wingman WebSocket Server",
    "version": "0.1.0",
    "description": "Game Automation Programmable Control Engine",
    "endpoints": [...]
  }
}
```

#### 2. 健康检查
```
GET /api/health
```

**响应示例:**
```json
{
  "success": true,
  "data": {
    "status": "ok",
    "timestamp": 1715299200000
  }
}
```

#### 3. 获取系统状态
```
GET /api/status
```

**响应示例:**
```json
{
  "success": true,
  "data": {
    "server": "wingman",
    "version": "0.1.0",
    "uptime": 1715299200,
    "runningScripts": 0
  }
}
```

#### 4. 获取版本信息
```
GET /api/version
```

**响应示例:**
```json
{
  "success": true,
  "data": {
    "server": "wingman",
    "version": "0.1.0",
    "buildDate": "May 10 2026 08:30:00"
  }
}
```

#### 5. 列出所有脚本
```
GET /api/scripts
```

**响应示例:**
```json
{
  "success": true,
  "data": {
    "scripts": [
      {
        "id": "example",
        "name": "example.lua",
        "path": "scripts/example.lua",
        "size": 1024,
        "isRunning": false
      }
    ]
  }
}
```

#### 6. 获取脚本详情
```
GET /api/scripts/{id}
```

**响应示例:**
```json
{
  "success": true,
  "data": {
    "id": "example",
    "path": "scripts/example.lua",
    "content": "-- 脚本内容\nprint('Hello, Wingman!')"
  }
}
```

#### 7. 启动脚本
```
POST /api/scripts/{id}/start
```

**响应示例:**
```json
{
  "success": true,
  "data": {
    "scriptId": "script_12345",
    "status": "running"
  }
}
```

#### 8. 停止脚本
```
POST /api/scripts/{id}/stop
```

**响应示例:**
```json
{
  "success": true,
  "data": {
    "status": "stopped"
  }
}
```

#### 9. 保存脚本
```
PUT /api/scripts/{id}
Content-Type: application/json

{
  "content": "-- 新的脚本内容\nprint('Updated')"
}
```

**响应示例:**
```json
{
  "success": true,
  "data": {
    "id": "example",
    "path": "scripts/example.lua"
  }
}
```

#### 10. 删除脚本
```
DELETE /api/scripts/{id}
```

## WebSocket RPC 接口

### 连接
```
ws://127.0.0.1:8080/ws
```

### 消息格式

**请求消息:**
```json
{
  "type": "call",
  "id": "req-1",
  "method": "script.list",
  "params": {}
}
```

**响应消息:**
```json
{
  "type": "result",
  "data": {
    "id": "req-1",
    "success": true,
    "result": { ... }
  }
}
```

### 支持的 RPC 方法

#### script.start
启动指定的脚本

```json
{
  "type": "call",
  "id": "req-1",
  "method": "script.start",
  "params": {
    "path": "scripts/example.lua"
  }
}
```

#### script.stop
停止运行中的脚本

```json
{
  "type": "call",
  "id": "req-2",
  "method": "script.stop",
  "params": {
    "scriptId": "script_12345"
  }
}
```

#### script.list
列出所有可用脚本

```json
{
  "type": "call",
  "id": "req-3",
  "method": "script.list",
  "params": {}
}
```

#### script.getContent
获取脚本内容

```json
{
  "type": "call",
  "id": "req-4",
  "method": "script.getContent",
  "params": {
    "path": "scripts/example.lua"
  }
}
```

#### script.save
保存脚本内容

```json
{
  "type": "call",
  "id": "req-5",
  "method": "script.save",
  "params": {
    "path": "scripts/example.lua",
    "content": "-- 新内容"
  }
}
```

#### system.getStatus
获取系统状态

```json
{
  "type": "call",
  "id": "req-6",
  "method": "system.getStatus",
  "params": {}
}
```

#### system.getVersion
获取版本信息

```json
{
  "type": "call",
  "id": "req-7",
  "method": "system.getVersion",
  "params": {}
}
```

#### system.quit
退出应用

```json
{
  "type": "call",
  "id": "req-8",
  "method": "system.quit",
  "params": {}
}
```

### 心跳/Ping
```json
{
  "type": "ping"
}
```

**Pong 响应:**
```json
{
  "type": "pong",
  "timestamp": 1715299200000
}
```

### 事件订阅 (TODO)
```json
{
  "type": "subscribe",
  "params": {
    "events": ["script.*", "system.*"]
  }
}
```

## CORS 支持

所有 HTTP API 端点已启用 CORS，允许跨域访问。

## 使用示例

### cURL (HTTP)
```bash
# 获取脚本列表
curl http://127.0.0.1:8080/api/scripts

# 启动脚本
curl -X POST http://127.0.0.1:8080/api/scripts/example/start

# 保存脚本
curl -X PUT http://127.0.0.1:8080/api/scripts/example \
  -H "Content-Type: application/json" \
  -d '{"content": "print(\"Hello\")"}'
```

### JavaScript (WebSocket)
```javascript
const ws = new WebSocket('ws://127.0.0.1:8080/ws');

ws.onopen = () => {
  console.log('Connected to Wingman');

  // 脚本列表
  ws.send(JSON.stringify({
    type: 'call',
    id: 'req-1',
    method: 'script.list',
    params: {}
  }));
};

ws.onmessage = (event) => {
  const msg = JSON.parse(event.data);
  console.log('Received:', msg);

  if (msg.type === 'result' && msg.data.success) {
    console.log('Result:', msg.data.result);
  }
};
```
