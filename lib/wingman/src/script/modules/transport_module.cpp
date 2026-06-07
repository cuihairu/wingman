#include "wingman/script/iscript_engine.hpp"

#ifdef WINGMAN_HAS_TRANSPORT
#include "wingman/transport/transport.hpp"
#include "wingman/transport/transport_client.hpp"
#include "wingman/transport/transport_server.hpp"
#include <asio.hpp>
#include <spdlog/spdlog.h>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#endif

namespace wingman {
namespace script {
namespace modules {

#ifdef WINGMAN_HAS_TRANSPORT

// ========== Transport 客户端管理 ==========

class TransportClientManager {
public:
	static TransportClientManager& instance() {
		static TransportClientManager inst;
		return inst;
	}

	// 创建客户端
	int createClient(const std::string& id) {
		std::lock_guard lock(mutex_);
		auto client = wingman::transport::createTcpClient();
		int handle = nextHandle_++;
		clients_[handle] = std::move(client);
		clientIds_[handle] = id;
		return handle;
	}

	// 获取客户端
	wingman::transport::TransportClient* getClient(int handle) {
		std::lock_guard lock(mutex_);
		auto it = clients_.find(handle);
		return it != clients_.end() ? it->second.get() : nullptr;
	}

	// 删除客户端
	void removeClient(int handle) {
		std::lock_guard lock(mutex_);
		auto it = clients_.find(handle);
		if (it != clients_.end()) {
			it->second->disconnect();
			clientIds_.erase(handle);
			clients_.erase(it);
		}
	}

	// 获取客户端 ID
	std::string getClientId(int handle) {
		std::lock_guard lock(mutex_);
		auto it = clientIds_.find(handle);
		return it != clientIds_.end() ? it->second : "";
	}

private:
	mutable std::mutex mutex_;
	std::unordered_map<int, std::unique_ptr<wingman::transport::TransportClient>> clients_;
	std::unordered_map<int, std::string> clientIds_;
	int nextHandle_ = 1;
};

// ========== Transport 服务器管理 ==========

class TransportServerManager {
public:
	static TransportServerManager& instance() {
		static TransportServerManager inst;
		return inst;
	}

	// 创建服务器
	int createServer(const std::string& id) {
		std::lock_guard lock(mutex_);
		auto server = wingman::transport::createTcpServer();
		int handle = nextHandle_++;
		servers_[handle] = std::move(server);
		serverIds_[handle] = id;
		return handle;
	}

	// 获取服务器
	wingman::transport::TransportServer* getServer(int handle) {
		std::lock_guard lock(mutex_);
		auto it = servers_.find(handle);
		return it != servers_.end() ? it->second.get() : nullptr;
	}

	// 删除服务器
	void removeServer(int handle) {
		std::lock_guard lock(mutex_);
		auto it = servers_.find(handle);
		if (it != servers_.end()) {
			it->second->stop();
			serverIds_.erase(handle);
			servers_.erase(it);
		}
	}

	// 获取服务器 ID
	std::string getServerId(int handle) {
		std::lock_guard lock(mutex_);
		auto it = serverIds_.find(handle);
		return it != serverIds_.end() ? it->second : "";
	}

private:
	mutable std::mutex mutex_;
	std::unordered_map<int, std::unique_ptr<wingman::transport::TransportServer>> servers_;
	std::unordered_map<int, std::string> serverIds_;
	int nextHandle_ = 1000;  // 与客户端 handle 区分
};

// ========== UDP Socket 管理器 ==========

class UdpSocket {
public:
	UdpSocket() : ioContext_(), socket_(ioContext_), recvBuffer_(65536), bound_(false) {}

	~UdpSocket() {
		close();
	}

	bool bind(const std::string& host, int port) {
		try {
			asio::ip::udp::endpoint endpoint(asio::ip::make_address(host), static_cast<asio::ip::port_type>(port));
			socket_.open(endpoint.protocol());
			socket_.bind(endpoint);
			bound_ = true;
			localEndpoint_ = endpoint;
			return true;
		} catch (const std::exception& e) {
			spdlog::error("[UDP] Bind failed: {}", e.what());
			return false;
		}
	}

	bool sendTo(const std::string& host, int port, const std::string& data) {
		try {
			asio::ip::udp::endpoint endpoint(asio::ip::make_address(host), static_cast<asio::ip::port_type>(port));
			socket_.send_to(asio::buffer(data), endpoint);
			return true;
		} catch (const std::exception& e) {
			spdlog::error("[UDP] Send failed: {}", e.what());
			return false;
		}
	}

	std::string recvFrom(int /*timeoutMs*/ = 5000) {
		try {
			if (!bound_) {
				spdlog::error("[UDP] Socket not bound");
				return "";
			}

			asio::ip::udp::endpoint senderEndpoint;
			size_t len = socket_.receive_from(asio::buffer(recvBuffer_), senderEndpoint);
			lastSender_ = senderEndpoint;
			return std::string(recvBuffer_.data(), len);
		} catch (const std::exception& e) {
			spdlog::error("[UDP] Recv failed: {}", e.what());
			return "";
		}
	}

	void close() {
		if (socket_.is_open()) {
			asio::error_code ec;
			socket_.close(ec);
		}
		bound_ = false;
	}

	bool isBound() const {
		return bound_;
	}

	std::string getLastError() const {
		return lastError_;
	}

private:
	asio::io_context ioContext_;
	asio::ip::udp::socket socket_;
	std::vector<char> recvBuffer_;
	asio::ip::udp::endpoint localEndpoint_;
	asio::ip::udp::endpoint lastSender_;
	bool bound_;
	std::string lastError_;
};

class UdpSocketManager {
public:
	static UdpSocketManager& instance() {
		static UdpSocketManager inst;
		return inst;
	}

	int createSocket(const std::string& id) {
		std::lock_guard lock(mutex_);
		auto socket = std::make_unique<UdpSocket>();
		int handle = nextHandle_++;
		sockets_[handle] = std::move(socket);
		socketIds_[handle] = id;
		return handle;
	}

	UdpSocket* getSocket(int handle) {
		std::lock_guard lock(mutex_);
		auto it = sockets_.find(handle);
		return it != sockets_.end() ? it->second.get() : nullptr;
	}

	void removeSocket(int handle) {
		std::lock_guard lock(mutex_);
		auto it = sockets_.find(handle);
		if (it != sockets_.end()) {
			it->second->close();
			socketIds_.erase(handle);
			sockets_.erase(it);
		}
	}

	std::string getSocketId(int handle) {
		std::lock_guard lock(mutex_);
		auto it = socketIds_.find(handle);
		return it != socketIds_.end() ? it->second : "";
	}

private:
	mutable std::mutex mutex_;
	std::unordered_map<int, std::unique_ptr<UdpSocket>> sockets_;
	std::unordered_map<int, std::string> socketIds_;
	int nextHandle_ = 10000;  // 与 TCP handle 区分
};

#endif

// ========== Module Definition ==========

ModuleDescriptor createTransportModule() {
	ModuleDescriptor mod;
	mod.name = "transport";

#ifdef WINGMAN_HAS_TRANSPORT

	// ========== TCP 客户端 ==========

	mod.functions.push_back({"tcpConnect", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		const std::string clientId = args.size() > 0 ? args[0].asString() : "default";
		const std::string host = args.size() > 1 ? args[1].asString() : "127.0.0.1";
		int port = args.size() > 2 ? static_cast<int>(args[2].asInt()) : 8080;

		auto& manager = TransportClientManager::instance();
		int handle = manager.createClient(clientId);

		auto* client = manager.getClient(handle);
		if (!client) {
			return ScriptValue::fromObject(std::unordered_map<std::string, ScriptValue>{
				{"success", ScriptValue::fromBool(false)},
				{"error", ScriptValue::fromString("Failed to create client")}
			});
		}

		// 设置消息处理器
		client->setMessageHandler([](const wingman::transport::MessagePtr& msg) {
			// 可以通过事件系统通知脚本
			spdlog::debug("[Transport] Received message: type={}, size={}",
				static_cast<int>(msg->header.type), msg->body.size());
		});

		// 连接
		if (!client->connect(host, port)) {
			manager.removeClient(handle);
			return ScriptValue::fromObject(std::unordered_map<std::string, ScriptValue>{
				{"success", ScriptValue::fromBool(false)},
				{"error", ScriptValue::fromString("Failed to connect")}
			});
		}

		return ScriptValue::fromObject(std::unordered_map<std::string, ScriptValue>{
			{"success", ScriptValue::fromBool(true)},
			{"handle", ScriptValue::fromInt(handle)},
			{"id", ScriptValue::fromString(clientId)}
		});
	}, "id:string?, host:string?, port:int? -> {success,handle,id,error?}"});

	mod.functions.push_back({"tcpSend", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int handle = static_cast<int>(args[0].asInt());
		const std::string data = args[1].asString();

		auto& manager = TransportClientManager::instance();
		auto* client = manager.getClient(handle);
		if (!client) {
			return ScriptValue::fromBool(false);
		}

		auto message = wingman::transport::Message::create(
			wingman::transport::MessageType::Request, data);
		return ScriptValue::fromBool(client->send(message));
	}, "handle:int, data:string -> bool"});

	mod.functions.push_back({"tcpDisconnect", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int handle = static_cast<int>(args[0].asInt());

		auto& manager = TransportClientManager::instance();
		manager.removeClient(handle);
		return ScriptValue::fromBool(true);
	}, "handle:int -> bool"});

	mod.functions.push_back({"tcpIsConnected", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int handle = static_cast<int>(args[0].asInt());

		auto& manager = TransportClientManager::instance();
		auto* client = manager.getClient(handle);
		return ScriptValue::fromBool(client ? client->isConnected() : false);
	}, "handle:int -> bool"});

	// ========== TCP 服务器 ==========

	mod.functions.push_back({"tcpListen", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		const std::string serverId = args.size() > 0 ? args[0].asString() : "default";
		const std::string host = args.size() > 1 ? args[1].asString() : "0.0.0.0";
		int port = args.size() > 2 ? static_cast<int>(args[2].asInt()) : 8080;

		auto& manager = TransportServerManager::instance();
		int handle = manager.createServer(serverId);

		auto* server = manager.getServer(handle);
		if (!server) {
			return ScriptValue::fromObject(std::unordered_map<std::string, ScriptValue>{
				{"success", ScriptValue::fromBool(false)},
				{"error", ScriptValue::fromString("Failed to create server")}
			});
		}

		// 启动服务器
		if (!server->start()) {
			manager.removeServer(handle);
			return ScriptValue::fromObject(std::unordered_map<std::string, ScriptValue>{
				{"success", ScriptValue::fromBool(false)},
				{"error", ScriptValue::fromString("Failed to start server")}
			});
		}

		// 监听端口
		if (!server->listen(host, port)) {
			server->stop();
			manager.removeServer(handle);
			return ScriptValue::fromObject(std::unordered_map<std::string, ScriptValue>{
				{"success", ScriptValue::fromBool(false)},
				{"error", ScriptValue::fromString("Failed to listen on port")}
			});
		}

		return ScriptValue::fromObject(std::unordered_map<std::string, ScriptValue>{
			{"success", ScriptValue::fromBool(true)},
			{"handle", ScriptValue::fromInt(handle)},
			{"id", ScriptValue::fromString(serverId)}
		});
	}, "id:string?, host:string?, port:int? -> {success,handle,id,error?}"});

	mod.functions.push_back({"tcpSendTo", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int handle = static_cast<int>(args[0].asInt());
		uint64_t sessionId = static_cast<uint64_t>(args[1].asInt());
		const std::string data = args[2].asString();

		auto& manager = TransportServerManager::instance();
		auto* server = manager.getServer(handle);
		if (!server) {
			return ScriptValue::fromBool(false);
		}

		auto message = wingman::transport::Message::create(
			wingman::transport::MessageType::Response, data);
		return ScriptValue::fromBool(server->send(sessionId, message));
	}, "handle:int, sessionId:int, data:string -> bool"});

	mod.functions.push_back({"tcpBroadcast", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int handle = static_cast<int>(args[0].asInt());
		const std::string data = args[1].asString();

		auto& manager = TransportServerManager::instance();
		auto* server = manager.getServer(handle);
		if (!server) {
			return ScriptValue::fromBool(false);
		}

		auto message = wingman::transport::Message::create(
			wingman::transport::MessageType::Notify, data);
		server->broadcast(message);
		return ScriptValue::fromBool(true);
	}, "handle:int, data:string -> bool"});

	mod.functions.push_back({"tcpCloseSession", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int handle = static_cast<int>(args[0].asInt());
		uint64_t sessionId = static_cast<uint64_t>(args[1].asInt());

		auto& manager = TransportServerManager::instance();
		auto* server = manager.getServer(handle);
		if (!server) {
			return ScriptValue::fromBool(false);
		}

		server->closeSession(sessionId);
		return ScriptValue::fromBool(true);
	}, "handle:int, sessionId:int -> bool"});

	mod.functions.push_back({"tcpGetSessions", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int handle = static_cast<int>(args[0].asInt());

		auto& manager = TransportServerManager::instance();
		auto* server = manager.getServer(handle);
		if (!server) {
			return ScriptValue::fromArray({});
		}

		auto sessionIds = server->getSessionIds();
		std::vector<ScriptValue> sessions;
		for (auto id : sessionIds) {
			sessions.push_back(ScriptValue::fromInt(static_cast<int64_t>(id)));
		}
		return ScriptValue::fromArray(std::move(sessions));
	}, "handle:int -> [sessionId,...]"});

	mod.functions.push_back({"tcpStop", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int handle = static_cast<int>(args[0].asInt());

		auto& manager = TransportServerManager::instance();
		manager.removeServer(handle);
		return ScriptValue::fromBool(true);
	}, "handle:int -> bool"});

	// ========== UDP Socket ==========

	mod.functions.push_back({"udpSocket", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		const std::string socketId = args.size() > 0 ? args[0].asString() : "default";

		auto& manager = UdpSocketManager::instance();
		int handle = manager.createSocket(socketId);

		return ScriptValue::fromObject(std::unordered_map<std::string, ScriptValue>{
			{"success", ScriptValue::fromBool(true)},
			{"handle", ScriptValue::fromInt(handle)},
			{"id", ScriptValue::fromString(socketId)}
		});
	}, "id:string? -> {success,handle,id}"});

	mod.functions.push_back({"udpBind", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int handle = static_cast<int>(args[0].asInt());
		const std::string host = args.size() > 1 ? args[1].asString() : "0.0.0.0";
		int port = static_cast<int>(args[2].asInt());

		auto& manager = UdpSocketManager::instance();
		auto* socket = manager.getSocket(handle);
		if (!socket) {
			return ScriptValue::fromObject(std::unordered_map<std::string, ScriptValue>{
				{"success", ScriptValue::fromBool(false)},
				{"error", ScriptValue::fromString("Invalid socket handle")}
			});
		}

		bool result = socket->bind(host, port);
		return ScriptValue::fromObject(std::unordered_map<std::string, ScriptValue>{
			{"success", ScriptValue::fromBool(result)}
		});
	}, "handle:int, host:string, port:int -> {success,error?}"});

	mod.functions.push_back({"udpSendTo", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int handle = static_cast<int>(args[0].asInt());
		const std::string host = args[1].asString();
		int port = static_cast<int>(args[2].asInt());
		const std::string data = args[3].asString();

		auto& manager = UdpSocketManager::instance();
		auto* socket = manager.getSocket(handle);
		if (!socket) {
			return ScriptValue::fromBool(false);
		}

		return ScriptValue::fromBool(socket->sendTo(host, port, data));
	}, "handle:int, host:string, port:int, data:string -> bool"});

	mod.functions.push_back({"udpRecvFrom", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int handle = static_cast<int>(args[0].asInt());
		int timeout = args.size() > 1 ? static_cast<int>(args[1].asInt()) : 5000;

		auto& manager = UdpSocketManager::instance();
		auto* socket = manager.getSocket(handle);
		if (!socket) {
			return ScriptValue::fromObject(std::unordered_map<std::string, ScriptValue>{
				{"success", ScriptValue::fromBool(false)},
				{"error", ScriptValue::fromString("Invalid socket handle")}
			});
		}

		std::string data = socket->recvFrom(timeout);
		return ScriptValue::fromObject(std::unordered_map<std::string, ScriptValue>{
			{"success", ScriptValue::fromBool(!data.empty())},
			{"data", ScriptValue::fromString(data)}
		});
	}, "handle:int, timeout:int? -> {success,data,error?}"});

	mod.functions.push_back({"udpClose", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int handle = static_cast<int>(args[0].asInt());

		auto& manager = UdpSocketManager::instance();
		manager.removeSocket(handle);
		return ScriptValue::fromBool(true);
	}, "handle:int -> bool"});

#else
	// 当 transport 不可用时，提供错误信息
	mod.functions.push_back({"tcpConnect", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::fromObject(std::unordered_map<std::string, ScriptValue>{
			{"success", ScriptValue::fromBool(false)},
			{"error", ScriptValue::fromString("Transport module not enabled in this build")}
		});
	}, "id:string?, host:string?, port:int? -> {success,handle,id,error?}"});
#endif

	return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
