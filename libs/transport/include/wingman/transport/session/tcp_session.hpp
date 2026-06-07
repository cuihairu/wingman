#pragma once

#include "wingman/transport/session/session.hpp"

namespace wingman::transport {

// ========== TCP 会话 ==========

class TcpSession : public Session {
public:
    using Session::Session;

    // 工厂方法
    static SessionPtr create(SessionId id, asio::ip::tcp::socket socket) {
        // Use shared_ptr directly with new instead of make_shared, because
        // the Session constructor is protected and make_shared cannot access it.
        return std::shared_ptr<TcpSession>(new TcpSession(id, std::move(socket)));
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
