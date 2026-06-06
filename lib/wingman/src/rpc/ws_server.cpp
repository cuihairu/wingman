#include "wingman/rpc/ws_server.hpp"

#ifdef WINGMAN_HAS_WEBSOCKET
#include <ixwebsocket/IXWebSocketServer.h>
#endif

#include <spdlog/spdlog.h>

namespace wingman::rpc {

class WsServer::Impl {
public:
#ifdef WINGMAN_HAS_WEBSOCKET
    std::unique_ptr<ix::WebSocketServer> server;
#endif
};

WsServer::WsServer(int port)
    : port_(port), impl_(std::make_unique<Impl>()) {}

WsServer::~WsServer() {
    stop();
}

void WsServer::start() {
#ifdef WINGMAN_HAS_WEBSOCKET
    if (running_.load()) return;

    impl_->server = std::make_unique<ix::WebSocketServer>(port_);
    impl_->server->setOnClientMessageCallback(
        [this](std::shared_ptr<ix::ConnectionState> connectionState,
               ix::WebSocket& ws,
               const ix::WebSocketMessagePtr& msg) {
            if (msg->type == ix::WebSocketMessageType::Message) {
                if (handler_) {
                    std::string response = handler_(msg->str);
                    ws.send(response);
                }
            } else if (msg->type == ix::WebSocketMessageType::Open) {
                spdlog::info("WebSocket client connected");
            } else if (msg->type == ix::WebSocketMessageType::Close) {
                spdlog::info("WebSocket client disconnected");
            }
        });

    auto result = impl_->server->listen();
    if (!result.first) {
        spdlog::error("WebSocket server failed to listen: {}", result.second);
        return;
    }

    impl_->server->start();
    running_.store(true);
    spdlog::info("WebSocket server started on port {}", port_);
#else
    spdlog::warn("WebSocket support not compiled in");
#endif
}

void WsServer::stop() {
#ifdef WINGMAN_HAS_WEBSOCKET
    if (!running_.load()) return;
    if (impl_->server) {
        impl_->server->stop();
        impl_->server.reset();
    }
    running_.store(false);
    spdlog::info("WebSocket server stopped");
#endif
}

bool WsServer::isRunning() const {
    return running_.load();
}

void WsServer::onMessage(MessageHandler handler) {
    handler_ = std::move(handler);
}

} // namespace wingman::rpc
