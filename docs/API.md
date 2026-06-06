# Wingman Runtime API 文档

Wingman Runtime 提供 WebSocket RPC 接口，供 Tauri UI 或其他客户端调用。

## 启动服务器

```bash
# 默认配置
wingman-runtime start

# 指定配置文件
wingman-runtime start --config agent.toml
```

> 主机和端口在 `agent.toml` 中配置，参见 `server_ip` / `server_port` 字段。

## HTTP RESTful API

> **注意：** HTTP RESTful API 尚未实现，目前仅提供 WebSocket RPC 接口。HTTP API 在后续版本中计划支持。

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
  "method": "script.list",
  "params": {}
}
```

**成功响应:**
```json
{
  "type": "response",
  "data": {
    "success": true,
    "result": { ... }
  }
}
```

**错误响应:**
```json
{
  "type": "response",
  "data": {
    "success": false,
    "error": "错误信息"
  }
}
```

### 支持的 RPC 方法

#### system.getStatus
获取系统状态

```json
{ "type": "call", "method": "system.getStatus", "params": {} }
```

#### system.getVersion
获取版本信息

```json
{ "type": "call", "method": "system.getVersion", "params": {} }
```

#### trigger.list
列出所有触发器

```json
{ "type": "call", "method": "trigger.list", "params": {} }
```

#### trigger.add
添加触发器

```json
{
  "type": "call",
  "method": "trigger.add",
  "params": {
    "config": { ... }
  }
}
```

#### trigger.remove
删除触发器

```json
{
  "type": "call",
  "method": "trigger.remove",
  "params": {
    "id": "trigger-id"
  }
}
```

#### trigger.update
更新触发器

```json
{
  "type": "call",
  "method": "trigger.update",
  "params": {
    "id": "trigger-id",
    "config": { ... }
  }
}
```

#### trigger.toggle
启用/禁用触发器

```json
{
  "type": "call",
  "method": "trigger.toggle",
  "params": {
    "id": "trigger-id"
  }
}
```

#### script.list
列出所有可用脚本

```json
{ "type": "call", "method": "script.list", "params": {} }
```

#### script.start
启动指定的脚本

```json
{
  "type": "call",
  "method": "script.start",
  "params": {
    "id": "script-id"
  }
}
```

#### script.stop
停止运行中的脚本

```json
{
  "type": "call",
  "method": "script.stop",
  "params": {
    "id": "script-id"
  }
}
```

### 心跳/Ping
```json
{ "type": "ping" }
```

**Pong 响应:**
```json
{ "type": "pong", "timestamp": 1715299200000 }
```

## 使用示例

### JavaScript (WebSocket)
```javascript
const ws = new WebSocket('ws://127.0.0.1:8080/ws');

ws.onopen = () => {
  console.log('Connected to Wingman');

  // 获取系统状态
  ws.send(JSON.stringify({
    type: 'call',
    method: 'system.getStatus',
    params: {}
  }));
};

ws.onmessage = (event) => {
  const msg = JSON.parse(event.data);
  console.log('Received:', msg);

  if (msg.type === 'response' && msg.data.success) {
    console.log('Result:', msg.data.result);
  }
};
```
