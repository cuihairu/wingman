#include "wingman/transport/transport_server.hpp"

namespace wingman::transport {

std::unique_ptr<TransportServer> TransportServer::create(TransportType type) {
    if (type == TransportType::TCP) {
        TcpServer* server = new TcpServer();
        return std::unique_ptr<TransportServer>(static_cast<TransportServer*>(server));
    }
    return std::unique_ptr<TransportServer>(static_cast<TransportServer*>(nullptr));
}

}
