#pragma once

#include "wingman/transport/session/session.hpp"

namespace wingman::transport {

// ========== TCP 会话 ==========

class TcpSession : public Session {
public:
    using Session::Session;

    // 工厂方法
    static SessionPtr create(SessionId id, asio::ip::tcp::socket socket) {
        return std::make_shared<TcpSession>(id, std::move(socket));
    }

    // 设置 TCP 选项
    void setNoDelay(bool enable) {
        asio::error_code ec;
        socket_.set_option(asio::ip::tcp::no_delay(enable), ec);
    }

    void setKeepAlive(bool enable) {
        asio::error_code ec;
        socket_.set_option(asio::socket_base::keep_alive(enable), ec);
    }
};

} // namespace wingman::transport
