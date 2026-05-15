# Wingman 远程控制协议文档

## 概述

Wingman 远程控制协议是基于 TCP 的 JSON-RPC 风格协议，用于通过网络远程控制游戏自动化功能。

**默认端口**: 9999

**连接**: TCP Socket

**编码**: UTF-8 JSON

---

## 协议格式

### 请求格式

```json
{
  "action": "动作名称",
  "params": {
    // 动作参数
  }
}
```

### 响应格式

```json
{
  "success": true/false,
  "data": {
    // 返回数据
  },
  "error": "错误信息（仅失败时）"
}
```

---

## API 端点

### 系统操作

#### ping

健康检查。

**请求**:
```json
{"action": "ping"}
```

**响应**:
```json
{"success": true, "data": {"status": "ok"}}
```

---

#### get_version

获取版本信息。

**请求**:
```json
{"action": "get_version"}
```

**响应**:
```json
{
  "success": true,
  "data": {
    "name": "Wingman",
    "version": "0.1.0"
  }
}
```

---

### 屏幕操作

#### capture_screen

截取屏幕区域。

**请求**:
```json
{
  "action": "capture_screen",
  "params": {
    "x": 0,
    "y": 0,
    "width": 1920,
    "height": 1080,
    "format": "jpg",    // jpg 或 png
    "quality": 90       // JPEG 质量 1-100
  }
}
```

**响应**:
```json
{
  "success": true,
  "data": {
    "image": "base64_encoded_image_data",
    "width": 1920,
    "height": 1080,
    "format": "jpg"
  }
}
```

---

#### get_pixel

获取指定坐标的像素颜色。

**请求**:
```json
{
  "action": "get_pixel",
  "params": {
    "x": 100,
    "y": 200
  }
}
```

**响应**:
```json
{
  "success": true,
  "data": {
    "r": 255,
    "g": 128,
    "b": 64,
    "color": 0xFF8040
  }
}
```

---

#### find_color

在指定区域查找颜色。

**请求**:
```json
{
  "action": "find_color",
  "params": {
    "color": 0xFF0000,      // 要查找的颜色 (BGR)
    "x": 0,
    "y": 0,
    "width": 1920,
    "height": 1080,
    "tolerance": 10,        // 颜色容差 0-255
    "limit": 1              // 最多返回点数
  }
}
```

**响应**:
```json
{
  "success": true,
  "data": {
    "points": [
      {"x": 100, "y": 200},
      {"x": 150, "y": 250}
    ],
    "count": 2
  }
}
```

---

#### find_image

在屏幕上查找图像。

**请求**:
```json
{
  "action": "find_image",
  "params": {
    "image": "base64_encoded_template_image",
    "x": 0,
    "y": 0,
    "width": 1920,
    "height": 1080,
    "threshold": 0.9         // 匹配阈值 0.0-1.0
  }
}
```

**响应**:
```json
{
  "success": true,
  "data": {
    "found": true,
    "x": 100,
    "y": 200,
    "confidence": 0.95
  }
}
```

---

### 输入模拟

#### click

模拟鼠标点击。

**请求**:
```json
{
  "action": "click",
  "params": {
    "x": 100,
    "y": 200,
    "button": "left"        // left, right, middle
  }
}
```

**响应**:
```json
{"success": true}
```

---

#### move

移动鼠标到指定位置。

**请求**:
```json
{
  "action": "move",
  "params": {
    "x": 100,
    "y": 200,
    "duration": 100         // 移动时长（毫秒）
  }
}
```

**响应**:
```json
{"success": true}
```

---

#### key

模拟键盘按键。

**请求**:
```json
{
  "action": "key",
  "params": {
    "keyCode": 65,          // 虚拟键码
    "down": true            // true=按下, false=释放
  }
}
```

**响应**:
```json
{"success": true}
```

---

#### type_text

输入文本。

**请求**:
```json
{
  "action": "type_text",
  "params": {
    "text": "Hello World",
    "delay": 50             // 每字符延迟（毫秒）
  }
}
```

**响应**:
```json
{"success": true}
```

---

### 触发器管理

#### list_triggers

列出所有触发器。

**请求**:
```json
{"action": "list_triggers"}
```

**响应**:
```json
{
  "success": true,
  "data": {
    "count": 2,
    "triggers": [
      {
        "id": "trigger_1",
        "enabled": true,
        "config": {...}
      }
    ]
  }
}
```

---

#### add_trigger

添加新触发器。

**请求**:
```json
{
  "action": "add_trigger",
  "params": {
    "id": "my_trigger",
    "enabled": true,
    "condition": {...},
    "action": {...},
    "cooldown": 500
  }
}
```

**响应**:
```json
{"success": true}
```

---

#### remove_trigger

删除触发器。

**请求**:
```json
{
  "action": "remove_trigger",
  "params": {
    "id": "my_trigger"
  }
}
```

**响应**:
```json
{"success": true}
```

---

#### enable_trigger

启用触发器。

**请求**:
```json
{
  "action": "enable_trigger",
  "params": {
    "id": "my_trigger"
  }
}
```

**响应**:
```json
{"success": true}
```

---

#### disable_trigger

禁用触发器。

**请求**:
```json
{
  "action": "disable_trigger",
  "params": {
    "id": "my_trigger"
  }
}
```

**响应**:
```json
{"success": true}
```

---

### 宏操作

#### record_macro

开始录制宏。

**请求**:
```json
{"action": "record_macro"}
```

**响应**:
```json
{"success": true}
```

---

#### stop_macro_recording

停止宏录制。

**请求**:
```json
{"action": "stop_macro_recording"}
```

**响应**:
```json
{
  "success": true,
  "data": {
    "eventCount": 42
  }
}
```

---

#### play_macro

回放宏。

**请求**:
```json
{
  "action": "play_macro",
  "params": {
    "speed": 100,           // 速度百分比
    "repeat": 1             // 重复次数
  }
}
```

**响应**:
```json
{"success": true}
```

---

## 错误码

| 错误 | 说明 |
|------|------|
| `unknown_action` | 未知的动作类型 |
| `invalid_params` | 参数格式错误 |
| `capture_failed` | 屏幕捕获失败 |
| `not_found` | 资源不存在 |

---

## 使用示例

### C++ 客户端

```cpp
#include "wingman/remote_client.hpp"

wingman::RemoteClient client;

// 连接
if (client.connect("192.168.1.100", 9999)) {
    // 截图
    auto resp = client.captureScreen(0, 0, 1920, 1080);

    // 查找颜色
    resp = client.findColor(0xFF0000, 0, 0, 1920, 1080, 10);

    // 点击
    if (resp.success && resp.data["count"] > 0) {
        auto x = resp.data["points"][0]["x"];
        auto y = resp.data["points"][0]["y"];
        client.click(x, y);
    }

    client.disconnect();
}
```

### Python 客户端

```python
import socket
import json

def call_remote(host, port, action, params=None):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((host, port))

    request = {"action": action, "params": params or {}}
    sock.sendall(json.dumps(request).encode() + b"\n")

    data = b""
    while b"\n" not in data:
        data += sock.recv(4096)
    sock.close()

    return json.loads(data.decode())

# 使用
resp = call_remote("192.168.1.100", 9999, "capture_screen", {
    "width": 1920,
    "height": 1080,
    "format": "jpg"
})
```

---

## 安全建议

1. **不要暴露在公网** - 仅在可信局域网使用
2. **添加认证** - 生产环境应实现 Token 认证
3. **限制来源** - 使用防火墙限制访问 IP
4. **加密传输** - 敏感场景应使用 TLS
