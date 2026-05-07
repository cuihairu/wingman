#include "wingman/server/server.hpp"
#include "lua_extensions.hpp"
#include <spdlog/spdlog.h>
#include <mutex>

namespace wingman::server {

// ========== Connection 实现 ==========

Connection::Connection(tcp::socket socket, RequestHandler handler)
    : socket_(std::move(socket)), handler_(std::move(handler)) {}

Connection::~Connection() {
    close();
}

void Connection::start() {
    doRead();
}

void Connection::send(const Response& response) {
    if (!isOpen()) return;

    auto self = shared_from_this();
    std::string data = Protocol::encode(response);

    asio::async_write(socket_, asio::buffer(data),
        [this, self](const std::error_code& ec, std::size_t) {
            if (ec) {
                handleError(ec);
            }
        });
}

void Connection::close() {
    if (isOpen()) {
        std::error_code ec;
        socket_.close(ec);
    }
}

std::string Connection::getRemoteAddress() const {
    try {
        if (isOpen()) {
            return socket_.remote_endpoint().address().to_string();
        }
    } catch (...) {}
    return "unknown";
}

bool Connection::isOpen() const {
    return socket_.is_open();
}

void Connection::doRead() {
    auto self = shared_from_this();

    asio::async_read_until(socket_, buffer_, '\n',
        [this, self](const std::error_code& ec, std::size_t) {
            if (ec) {
                handleError(ec);
                return;
            }

            std::istream is(&buffer_);
            std::string line;
            std::getline(is, line);

            // Parse length
            size_t length = 0;
            try {
                length = std::stoul(line, nullptr, 16);
            } catch (...) {
                handleError(asio::error::invalid_argument);
                return;
            }

            // Read JSON data
            std::string jsonStr(length, '\0');
            asio::error_code read_ec;
            std::size_t n = asio::read(socket_, asio::buffer(jsonStr, length + 1), read_ec); // +1 for newline

            if (read_ec || n != length + 1) {
                handleError(read_ec ? read_ec : asio::error::eof);
                return;
            }

            // Parse request
            Request request = Request::fromJson(jsonStr);

            // Handle request
            Response response = handler_(request);
            response.requestId = request.id;
            response.timestamp = Protocol::now();

            send(response);

            // Continue reading
            if (isOpen()) {
                doRead();
            }
        });
}

void Connection::doWrite(const std::string& data) {
    auto self = shared_from_this();
    asio::async_write(socket_, asio::buffer(data),
        [this, self](const std::error_code& ec, std::size_t) {
            if (ec) {
                handleError(ec);
            }
        });
}

void Connection::handleError(const std::error_code& ec) {
    if (ec) {
        spdlog::error("Connection error from {}: {}", getRemoteAddress(), ec.message());
        close();
    }
}

// ========== Server 实现 ==========

Server::Server(asio::io_context& ioContext, unsigned short port)
    : ioContext_(ioContext),
      acceptor_(ioContext, tcp::endpoint(tcp::v4(), port)),
      agentManager_(std::make_unique<AgentManager>()),
      orchestrator_(std::make_unique<WorkflowOrchestrator>()),
      teamOrchestrator_(std::make_unique<Orchestrator>()),
      heartbeatTimer_(ioContext) {

    // 设置 AgentManager 的回调
    agentManager_->setConnectCallback([this](const std::string& agentId, const AgentInfo& info) {
        if (connectCallback_) {
            connectCallback_(agentId, info);
        }
    });

    agentManager_->setDisconnectCallback([this](const std::string& agentId) {
        if (disconnectCallback_) {
            disconnectCallback_(agentId);
        }
    });

    // 设置全局 Orchestrator 实例（供 Lua team 模块使用）
    wingman::lua::team::setOrchestrator(teamOrchestrator_.get());
    wingman::lua::orchestration::setOrchestrator(orchestrator_.get());

    spdlog::info("Team orchestrator initialized");
}

Server::~Server() {
    stop();
}

void Server::start() {
    spdlog::info("Server started on port {}", acceptor_.local_endpoint().port());
    doAccept();
    startHeartbeatCheck();
}

void Server::stop() {
    std::error_code ec;
    acceptor_.close(ec);
    heartbeatTimer_.cancel(ec);

    std::lock_guard<std::mutex> lock(connectionsMutex_);
    for (auto& conn : connections_) {
        conn->close();
    }
    connections_.clear();

    if (ec) {
        spdlog::error("Server close error: {}", ec.message());
    }
}

void Server::setHandler(RequestType type, RequestHandler handler) {
    handlers_[type] = std::move(handler);
}

// ========== Client 管理 ==========

std::vector<AgentInfo> Server::getOnlineAgents() const {
    return agentManager_->getOnlineAgents();
}

std::optional<AgentInfo> Server::getAgent(const std::string& agentId) const {
    return agentManager_->getAgent(agentId);
}

bool Server::sendToAgent(const std::string& agentId, const Response& response) {
    auto conn = agentManager_->getConnection(agentId);
    if (conn && conn->isOpen()) {
        conn->send(response);
        return true;
    }
    return false;
}

bool Server::disconnectAgent(const std::string& agentId) {
    auto conn = agentManager_->getConnection(agentId);
    if (conn) {
        conn->close();
        agentManager_->unregisterAgent(agentId);
        return true;
    }
    return false;
}

size_t Server::getOnlineCount() const {
    return agentManager_->getOnlineCount();
}

// ========== 工作流管理 ==========

std::string Server::submitWorkflow(const Workflow& workflow) {
    return orchestrator_->submitWorkflow(workflow);
}

bool Server::cancelWorkflow(const std::string& workflowId) {
    return orchestrator_->cancelWorkflow(workflowId);
}

std::optional<Workflow> Server::getWorkflow(const std::string& workflowId) const {
    return orchestrator_->getWorkflow(workflowId);
}

std::vector<Workflow> Server::getAllWorkflows() const {
    return orchestrator_->getAllWorkflows();
}

void Server::setConnectCallback(ConnectCallback callback) {
    connectCallback_ = std::move(callback);
}

void Server::setDisconnectCallback(DisconnectCallback callback) {
    disconnectCallback_ = std::move(callback);
}

// ========== 私有方法 ==========

void Server::doAccept() {
    acceptor_.async_accept(
        [this](const std::error_code& ec, tcp::socket socket) {
            if (!ec) {
                auto conn = std::make_shared<Connection>(
                    std::move(socket),
                    [this, conn](const Request& req) {
                        // 处理特殊请求类型
                        if (req.type == RequestType::kRegister) {
                            return handleRegister(req, conn);
                        } else if (req.type == RequestType::kHeartbeat) {
                            return handleHeartbeat(req);
                        } else if (req.type == RequestType::kGetAgents) {
                            return handleGetAgents(req);
                        } else if (req.type == RequestType::kSyncTask) {
                            return handleSyncTask(req);
                        } else if (req.type == RequestType::kShutdown) {
                            return handleShutdown(req);
                        } else if (req.type == RequestType::kSubmitWorkflow) {
                            return handleSubmitWorkflow(req);
                        } else if (req.type == RequestType::kCancelWorkflow) {
                            return handleCancelWorkflow(req);
                        } else if (req.type == RequestType::kGetWorkflow) {
                            return handleGetWorkflow(req);
                        } else if (req.type == RequestType::kGetNextTask) {
                            return handleGetNextTask(req);
                        } else if (req.type == RequestType::kReportProgress) {
                            return handleReportProgress(req);
                        } else if (req.type == RequestType::kCompleteTask) {
                            return handleCompleteTask(req);
                        } else if (req.type == RequestType::kFailTask) {
                            return handleFailTask(req);
                        }
                        return defaultHandler(req);
                    }
                );

                std::string remoteAddr = conn->getRemoteAddress();
                spdlog::info("New connection from {}", remoteAddr);

                {
                    std::lock_guard<std::mutex> lock(connectionsMutex_);
                    connections_.push_back(conn);
                }

                conn->start();
            } else {
                spdlog::error("Accept error: {}", ec.message());
            }

            doAccept();
        });
}

void Server::removeConnection(Connection::ptr conn) {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    connections_.erase(
        std::remove(connections_.begin(), connections_.end(), conn),
        connections_.end()
    );
}

Response Server::defaultHandler(const Request& request) {
    auto it = handlers_.find(request.type);
    if (it != handlers_.end()) {
        return it->second(request);
    }

    return Response::error(request.id, ErrorCode::NOT_FOUND, "Unknown request type");
}

// ========== 请求处理器 ==========

Response Server::handleRegister(const Request& request, Connection::ptr conn) {
    auto agentId = request.agentId;
    if (agentId.empty()) {
        return Response::error(request.id, ErrorCode::INVALID_REQUEST, "Missing agent_id");
    }

    // 从 data 中获取客户端信息
    AgentInfo info;
    info.agentId = agentId;
    info.status = AgentStatus::Online;
    info.lastSeen = std::chrono::system_clock::now();

    if (request.data.contains("hostname")) {
        info.hostname = request.data["hostname"];
    }
    if (request.data.contains("ip")) {
        info.ip = request.data["ip"];
    } else {
        info.ip = conn->getRemoteAddress();
    }

    // 注册客户端
    agentManager_->registerAgent(agentId, info);
    agentManager_->setConnection(agentId, conn);

    spdlog::info("Agent registered: {} from {}", agentId, info.ip);

    return Response::ok(request.id, {
        {"server_time", Protocol::now()},
        {"agent_id", agentId}
    });
}

Response Server::handleHeartbeat(const Request& request) {
    auto agentId = request.agentId;
    if (agentId.empty()) {
        return Response::error(request.id, ErrorCode::INVALID_REQUEST, "Missing agent_id");
    }

    // 获取当前 Agent 信息
    auto info = agentManager_->getAgent(agentId);
    if (!info) {
        return Response::error(request.id, ErrorCode::NOT_FOUND, "Agent not registered");
    }

    AgentInfo updated = *info;
    updated.lastSeen = std::chrono::system_clock::now();

    // 更新状态
    if (request.data.contains("status")) {
        updated.status = parseAgentStatus(request.data["status"]);
    }

    // 更新当前任务（业务层）
    if (request.data.contains("current_task")) {
        updated.currentTask = request.data["current_task"];
    }

    // 更新资源状态（注册中心层）
    if (request.data.contains("resources")) {
        updated.resources = ResourceStats::fromJson(request.data["resources"]);
    }

    // 重新注册（更新信息）
    agentManager_->registerAgent(agentId, updated);

    return Response::ok(request.id, {
        {"server_time", Protocol::now()}
    });
}

Response Server::handleGetAgents(const Request& request) {
    auto agents = agentManager_->getOnlineAgents();

    nlohmann::json agentsJson = nlohmann::json::array();
    for (const auto& agent : agents) {
        agentsJson.push_back(agent.toJson());
    }

    return Response::ok(request.id, {
        {"agents", agentsJson},
        {"count", agents.size()}
    });
}

Response Server::handleSyncTask(const Request& request) {
    // 这个方法由 Orchestrator 实现
    // 这里暂时返回基础响应
    return Response::ok(request.id, {
        {"synced", false},
        {"message", "Orchestrator not implemented yet"}
    });
}

Response Server::handleShutdown(const Request& request) {
    auto agentId = request.agentId;
    if (agentId.empty()) {
        return Response::error(request.id, ErrorCode::INVALID_REQUEST, "Missing agent_id");
    }

    if (disconnectAgent(agentId)) {
        spdlog::info("Agent {} disconnected by shutdown request", agentId);
        return Response::ok(request.id);
    }

    return Response::error(request.id, ErrorCode::NOT_FOUND, "Agent not found");
}

// ========== 心跳检测 ==========

void Server::startHeartbeatCheck() {
    checkHeartbeat(asio::error_code{});
}

void Server::checkHeartbeat(const asio::error_code& ec) {
    if (ec) {
        // Timer 被取消，正常退出
        return;
    }

    // 检查超时客户端
    auto timeoutAgents = agentManager_->checkTimeout(heartbeatTimeout_);

    if (!timeoutAgents.empty()) {
        spdlog::warn("Detected {} timeout agents", timeoutAgents.size());
    }

    // 重新设置定时器
    heartbeatTimer_.expires_after(heartbeatCheckInterval_);
    heartbeatTimer_.async_wait([this](const asio::error_code& ec) {
        checkHeartbeat(ec);
    });
}

// ========== 工作流请求处理器 ==========

Response Server::handleSubmitWorkflow(const Request& request) {
    if (!request.data.contains("workflow")) {
        return Response::error(request.id, ErrorCode::INVALID_REQUEST, "Missing workflow data");
    }

    try {
        Workflow workflow = Workflow::fromJson(request.data["workflow"]);
        std::string workflowId = submitWorkflow(workflow);

        return Response::ok(request.id, {
            {"workflow_id", workflowId},
            {"status", workflowStatusToString(workflow.status)}
        });
    } catch (const std::exception& e) {
        return Response::error(request.id, ErrorCode::INVALID_REQUEST, std::string("Invalid workflow: ") + e.what());
    }
}

Response Server::handleCancelWorkflow(const Request& request) {
    if (!request.data.contains("workflow_id")) {
        return Response::error(request.id, ErrorCode::INVALID_REQUEST, "Missing workflow_id");
    }

    std::string workflowId = request.data["workflow_id"];
    bool cancelled = cancelWorkflow(workflowId);

    if (cancelled) {
        return Response::ok(request.id, {
            {"workflow_id", workflowId},
            {"cancelled", true}
        });
    }

    return Response::error(request.id, ErrorCode::NOT_FOUND, "Workflow not found or cannot be cancelled");
}

Response Server::handleGetWorkflow(const Request& request) {
    std::string workflowId;

    if (request.data.contains("workflow_id")) {
        workflowId = request.data["workflow_id"];
    } else if (!request.agentId.empty()) {
        // 如果没有指定 workflow_id，尝试从 agent 的当前任务获取
        auto agent = getAgent(request.agentId);
        if (agent && agent->currentTask.contains("workflow_id")) {
            workflowId = agent->currentTask["workflow_id"];
        }
    }

    if (workflowId.empty()) {
        return Response::error(request.id, ErrorCode::INVALID_REQUEST, "Missing workflow_id");
    }

    auto workflow = getWorkflow(workflowId);
    if (!workflow) {
        return Response::error(request.id, ErrorCode::NOT_FOUND, "Workflow not found");
    }

    return Response::ok(request.id, workflow->toJson());
}

Response Server::handleGetNextTask(const Request& request) {
    if (request.agentId.empty()) {
        return Response::error(request.id, ErrorCode::INVALID_REQUEST, "Missing agent_id");
    }

    if (!request.data.contains("workflow_id")) {
        return Response::error(request.id, ErrorCode::INVALID_REQUEST, "Missing workflow_id");
    }

    std::string workflowId = request.data["workflow_id"];
    auto nextTask = orchestrator_->getNextTask(request.agentId, workflowId);

    if (!nextTask) {
        return Response::ok(request.id, {
            {"has_task", false},
            {"message", "No available tasks"}
        });
    }

    return Response::ok(request.id, {
        {"has_task", true},
        {"task", nextTask->toJson()}
    });
}

Response Server::handleReportProgress(const Request& request) {
    if (request.agentId.empty()) {
        return Response::error(request.id, ErrorCode::INVALID_REQUEST, "Missing agent_id");
    }

    if (!request.data.contains("workflow_id") || !request.data.contains("step_id")) {
        return Response::error(request.id, ErrorCode::INVALID_REQUEST, "Missing workflow_id or step_id");
    }

    std::string workflowId = request.data["workflow_id"];
    std::string stepId = request.data["step_id"];
    nlohmann::json progress = request.data.value("progress", nlohmann::json{});

    bool reported = orchestrator_->reportProgress(request.agentId, workflowId, stepId, progress);

    if (reported) {
        return Response::ok(request.id, {
            {"reported", true}
        });
    }

    return Response::error(request.id, ErrorCode::NOT_FOUND, "Workflow not found");
}

Response Server::handleCompleteTask(const Request& request) {
    if (request.agentId.empty()) {
        return Response::error(request.id, ErrorCode::INVALID_REQUEST, "Missing agent_id");
    }

    if (!request.data.contains("workflow_id") || !request.data.contains("step_id")) {
        return Response::error(request.id, ErrorCode::INVALID_REQUEST, "Missing workflow_id or step_id");
    }

    std::string workflowId = request.data["workflow_id"];
    std::string stepId = request.data["step_id"];
    nlohmann::json result = request.data.value("result", nlohmann::json{});
    bool success = request.data.value("success", true);

    bool completed = orchestrator_->completeTask(request.agentId, workflowId, stepId, success, result);

    if (completed) {
        return Response::ok(request.id, {
            {"completed", true}
        });
    }

    return Response::error(request.id, ErrorCode::NOT_FOUND, "Workflow not found");
}

Response Server::handleFailTask(const Request& request) {
    if (request.agentId.empty()) {
        return Response::error(request.id, ErrorCode::INVALID_REQUEST, "Missing agent_id");
    }

    if (!request.data.contains("workflow_id") || !request.data.contains("step_id")) {
        return Response::error(request.id, ErrorCode::INVALID_REQUEST, "Missing workflow_id or step_id");
    }

    std::string workflowId = request.data["workflow_id"];
    std::string stepId = request.data["step_id"];
    std::string error = request.data.value("error", "Unknown error");

    bool failed = orchestrator_->failTask(request.agentId, workflowId, stepId, error);

    if (failed) {
        return Response::ok(request.id, {
            {"failed", true}
        });
    }

    return Response::error(request.id, ErrorCode::NOT_FOUND, "Workflow not found");
}

} // namespace wingman::server
