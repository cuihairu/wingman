#include "wingman/transport/transport_client.hpp"

namespace wingman::transport {

std::unique_ptr<TransportClient> TransportClient::create(TransportType type) {
    if (type == TransportType::TCP) {
        TcpClient* client = new TcpClient();
        return std::unique_ptr<TransportClient>(static_cast<TransportClient*>(client));
    }
    return std::unique_ptr<TransportClient>(static_cast<TransportClient*>(nullptr));
}

}
