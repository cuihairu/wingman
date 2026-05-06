#pragma once

#include <asio.hpp>
#include <memory>
#include <functional>
#include <string>
#include <vector>
#include "wingman/server/protocol.hpp"
#include "wingman/server/agent_manager.hpp"
#include "wingman/server/workflow_orchestrator.hpp"

namespace wingman::server {

using asio::ip::tcp;

// Request handler callback
using RequestHandler = std::function<Response(const Request&)>;

// Connection class - 前向声明
class Connection : public std::enable_shared_from_this<Connection> {
public:
    using ptr = std::shared_ptr<Connection>;

    explicit Connection(tcp::socket socket, RequestHandler handler);
    ~Connection();

    void start();
    void send(const Response& response);
    void close();

    std::string getRemoteAddress() const;
    bool isOpen() const;

private:
    tcp::socket socket_;
    RequestHandler handler_;
    asio::streambuf buffer_;

    void doRead();
    void doWrite(const std::string& data);
    void handleError(const std::error_code& ec);
};

// TCP Server
class Server {
public:
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    Server(asio::io_context& ioContext, unsigned short port);
    ~Server();

    void start();
    void stop();

    // ========== Request Handler 注册 ==========

    void setHandler(RequestType type, RequestHandler handler);

    // ========== Agent 管理 ==========

    // 获取所有在线客户端
    std::vector<AgentInfo> getOnlineAgents() const;

    // 获取特定客户端
    std::optional<AgentInfo> getAgent(const std::string& agentId) const;

    // 向指定客户端发送消息
    bool sendToAgent(const std::string& agentId, const Response& response);

    // 断开指定客户端
    bool disconnectAgent(const std::string& agentId);

    // 获取在线客户端数量
    size_t getOnlineCount() const;

    // ========== 工作流管理 ==========

    // 提交工作流
    std::string submitWorkflow(const Workflow& workflow);

    // 取消工作流
    bool cancelWorkflow(const std::string& workflowId);

    // 获取工作流状态
    std::optional<Workflow> getWorkflow(const std::string& workflowId) const;

    // 获取所有工作流
    std::vector<Workflow> getAllWorkflows() const;

    // ========== 事件回调 ==========

    using ConnectCallback = std::function<void(const std::string& agentId, const AgentInfo& info)>;
    using DisconnectCallback = std::function<void(const std::string& agentId)>;

    void setConnectCallback(ConnectCallback callback);
    void setDisconnectCallback(DisconnectCallback callback);

    // ========== 配置 ==========

    void setHeartbeatTimeout(std::chrono::seconds timeout) {
        heartbeatTimeout_ = timeout;
    }

    void setHeartbeatCheckInterval(std::chrono::seconds interval) {
        heartbeatCheckInterval_ = interval;
    }

private:
    asio::io_context& ioContext_;
    tcp::acceptor acceptor_;
    std::unordered_map<RequestType, RequestHandler> handlers_;
    std::vector<Connection::ptr> connections_;
    std::mutex connectionsMutex_;

    // Agent 管理
    std::unique_ptr<AgentManager> agentManager_;

    // 工作流编排
    std::unique_ptr<WorkflowOrchestrator> orchestrator_;

    // 心跳检测
    asio::steady_timer heartbeatTimer_;
    std::chrono::seconds heartbeatTimeout_{60};      // 60秒超时
    std::chrono::seconds heartbeatCheckInterval_{30}; // 30秒检查一次

    void doAccept();
    void removeConnection(Connection::ptr conn);

    // 心跳检测
    void startHeartbeatCheck();
    void checkHeartbeat(const asio::error_code& ec);

    // 默认请求处理器
    Response defaultHandler(const Request& request);

    // 处理注册请求
    Response handleRegister(const Request& request, Connection::ptr conn);

    // 处理心跳请求
    Response handleHeartbeat(const Request& request);

    // 处理获取客户端列表请求
    Response handleGetAgents(const Request& request);

    // 处理同步任务请求
    Response handleSyncTask(const Request& request);

    // 处理关闭客户端请求
    Response handleShutdown(const Request& request);

    // ========== 工作流请求处理器 ==========

    // 提交工作流
    Response handleSubmitWorkflow(const Request& request);

    // 取消工作流
    Response handleCancelWorkflow(const Request& request);

    // 获取工作流状态
    Response handleGetWorkflow(const Request& request);

    // 获取下一步任务
    Response handleGetNextTask(const Request& request);

    // 报告任务进度
    Response handleReportProgress(const Request& request);

    // 完成任务
    Response handleCompleteTask(const Request& request);

    // 任务失败
    Response handleFailTask(const Request& request);
};

} // namespace wingman::server
