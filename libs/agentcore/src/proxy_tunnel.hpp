#pragma once

// ProxyTunnel 代理数据面：server 下发 proxy.* Notify 帧，runtime 侧裸 TCP
// 拨号分支（自 cockpit guac_relay 配套的 agent 侧实现移植，2026-10-02）。
// 用途：Guacamole 目标转发——guacd 拨 server 回环中继，字节流经 agent 链路
// 到这里，由 runtime 从自己网络位拨 target（host 语义 = agent 侧可达地址）。
//
// 架构约束（CLAUDE.md）：runtime 禁止引入监听面——本类只拨号不监听，全部
// socket 操作收敛在每连接一个 worker 线程的 io_context 里（async 模型），
// server→runtime 方向的写入经 per-conn 队列 post 进同一线程，避免跨线程
// 混用同步/异步 socket 操作。
//
// 生命周期语义：连接不跨远程链路断线存续。proxy.data 不入 outbox（陈旧
// 数据重放会污染协议会话），链路收口时 stop() 拆掉全部连接，会话由
// server 侧中继随其会话关闭一并拆除。

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

namespace wingman::runtime {

class ProxyTunnel : public std::enable_shared_from_this<ProxyTunnel> {
public:
    // SendFunc server ← runtime 方向的 Notify 帧发送口。返回 false 表示
    // 远程链路不可用（未连接/写入失败），调用方随即拆除对应连接——代理流
    // 无法缓冲重放，链路失效即会话失效。
    using SendFunc =
        std::function<bool(const std::string& msgType, const nlohmann::json& fields)>;

    explicit ProxyTunnel(SendFunc send);
    ~ProxyTunnel();

    ProxyTunnel(const ProxyTunnel&) = delete;
    ProxyTunnel& operator=(const ProxyTunnel&) = delete;

    // handleNotify 处理 server 下发的 proxy.new / proxy.data / proxy.close
    // 帧（RemoteClient::handleNotifyMessage 按 type 前缀路由进来）。
    // proxy.error 是 runtime → server 方向，收到即忽略。
    void handleNotify(const std::string& type, const nlohmann::json& msg);

    // stop 拆除全部连接（幂等）。RemoteClient::stop() 收口远程链路时调用。
    void stop();

private:
    struct Conn;

    void handleNew(const nlohmann::json& msg);
    void handleData(const nlohmann::json& msg);
    void handleClose(const nlohmann::json& msg);
    void runConn(std::shared_ptr<Conn> conn);

    SendFunc send_;

    std::mutex mu_;
    std::map<std::string, std::shared_ptr<Conn>> conns_; // key: proxyId + "/" + connId
    std::vector<std::thread> workers_;                   // 连接 worker（stop 统一 join）
    bool stopped_ = false;
};

} // namespace wingman::runtime
