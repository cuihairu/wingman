# wingman-transport 使用说明

分层传输架构，提供 TCP/WebSocket 通信能力。

## 架构层次

```
┌─────────────────────────────────────────┐
│         Application Layer               │
├─────────────────────────────────────────┤
│         Channel (消息通道)               │  ← 多路复用、请求-响应匹配
├─────────────────────────────────────────┤
│         Session (会话管理)               │  ← 连接会话、消息收发
├─────────────────────────────────────────┤
│         Transport (传输层)               │  ← TCP/WebSocket
└─────────────────────────────────────────┘
```

## 快速开始

### 服务端

```cpp
#include "wingman/transport/transport.hpp"

using namespace wingman::transport;

// 创建服务端
auto server = createTcpServer();

// 设置消息处理器
server->setMessageHandler([](const MessagePtr& msg) {
    std::cout << "Received: " << msg->body << std::endl;
});

// 设置事件处理器
server->setEventHandler([](Session* session, SessionEvent event) {
    if (event == SessionEvent::Connected) {
        std::cout << "Client connected: " << session->getId() << std::endl;
    }
});

// 启动并监听
server->start();
server->listen("0.0.0.0", 9527);
```

### 客户端

```cpp
// 创建客户端
auto client = createTcpClient();

// 设置消息处理器
client->setMessageHandler([](const MessagePtr& msg) {
    std::cout << "Received: " << msg->body << std::endl;
});

// 连接
client->connect("127.0.0.1", 9527);

// 发送消息
auto msg = Message::create(MessageType::Notify, "Hello, Server!");
client->send(msg);

// 请求-响应模式
auto request = Message::create(MessageType::Request, "Ping");
auto future = client->request(request);
// ... 等待响应
```

### 使用 Channel

```cpp
// 创建通道
auto channel = channelManager.createChannel(
    channelId,
    ChannelType::RequestResponse,
    sessionWeakPtr
);

// 发送请求并等待响应
auto requestMsg = Message::create(MessageType::Request, "GetStatus");
auto responseFuture = channel->request(requestMsg);

// 阻塞等待响应
auto response = responseFuture.get();
if (response) {
    std::cout << "Response: " << response->body << std::endl;
}

// 发送通知（无需响应）
auto notifyMsg = Message::create(MessageType::Notify, "Update");
channel->notify(notifyMsg);
```

## API 参考

### Message 消息

```cpp
// 创建消息
auto msg = Message::create();
msg->header.type = MessageType::Request;
msg->body = "data";

// 序列化/反序列化
auto data = msg->serialize();
auto parsed = Message::deserialize(data);
```

### Session 会话

```cpp
// 获取会话信息
auto id = session->getId();
auto addr = session->getRemoteAddress();
auto port = session->getRemotePort();

// 发送消息
session->send(message);

// 检查连接
if (session->isConnected()) {
    // ...
}

// 关闭连接
session->close();
```

### TransportServer 服务端

```cpp
// 会话管理
size_t count = server->getSessionCount();
auto ids = server->getSessionIds();
Session* session = server->getSession(id);

// 关闭会话
server->closeSession(id);

// 广播消息
server->broadcast(message);

// 发送到指定会话
server->send(sessionId, message);
```

## 编译

已集成到 wingman 项目，自动编译：

```bash
cmake -B build -S .
cmake --build build
```

链接：

```cmake
target_link_libraries(your_target PRIVATE
    wingman::transport
)
```
