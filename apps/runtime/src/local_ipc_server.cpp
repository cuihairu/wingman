#include "wingman/runtime/local_ipc_server.hpp"

#include "wingman/ipc/ipc_factory.hpp"
#include "wingman/rpc/rpc_dispatcher.hpp"
#include "wingman/rpc/script_handler.hpp"
#include "wingman/rpc/system_handler.hpp"
#include "wingman/rpc/trigger_handler.hpp"
#include "wingman/runtime/standalone_mode.hpp"
#include "wingman/trigger.hpp"
#include "wingman/version.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <chrono>
#include <mutex>
#include <thread>

namespace wingman::runtime {

class LocalIpcServer::Impl {
public:
    StandaloneMode& standalone;
    std::string endpoint;
    std::unique_ptr<rpc::RpcDispatcher> dispatcher;
    std::unique_ptr<TriggerManager> triggerManager;
    std::thread serverThread;
    std::atomic<bool> stopping{false};
    std::mutex channelMutex;
    ipc::IIpcChannel* currentChannel = nullptr;

    Impl(StandaloneMode& standaloneMode, std::string endpointName)
        : standalone(standaloneMode), endpoint(std::move(endpointName)) {}
};

LocalIpcServer::LocalIpcServer(StandaloneMode& standalone, std::string endpoint)
    : impl_(std::make_unique<Impl>(standalone, std::move(endpoint))) {}

LocalIpcServer::~LocalIpcServer() {
    stop();
}

bool LocalIpcServer::start() {
    if (running_.load()) {
        return true;
    }

    ipc::IpcConfig config;
    config.serverName = impl_->endpoint.empty() ? ipc::IpcFactory::getDefaultEndpoint() : impl_->endpoint;

    impl_->dispatcher = std::make_unique<rpc::RpcDispatcher>();
    impl_->triggerManager = std::make_unique<TriggerManager>();
    rpc::registerSystemHandlers(*impl_->dispatcher, WINGMAN_VERSION);
    rpc::registerTriggerHandlers(*impl_->dispatcher, *impl_->triggerManager);
    rpc::registerScriptHandlers(*impl_->dispatcher, impl_->standalone);

    auto configureChannel = [this](ipc::IIpcChannel& channel) {
        auto* channelPtr = &channel;
        channel.setMessageCallback([this, channelPtr](const ipc::IpcMessage& message) {
            nlohmann::json params = nlohmann::json::object();
            if (!message.payload.empty()) {
                try {
                    params = nlohmann::json::parse(message.payload);
                } catch (const std::exception& e) {
                    params = nlohmann::json{{"__parse_error", e.what()}};
                }
            }

            nlohmann::json responsePayload;
            if (message.type == ipc::IpcMessageType::Error) {
                responsePayload = {
                    {"type", "response"},
                    {"id", std::to_string(message.id)},
                    {"data", {{"success", false}, {"error", params.value("error", "Invalid IPC message")}}}
                };
            } else if (message.method.empty()) {
                responsePayload = {
                    {"type", "response"},
                    {"id", std::to_string(message.id)},
                    {"data", {{"success", false}, {"error", "Missing IPC method"}}}
                };
            } else if (params.contains("__parse_error")) {
                responsePayload = {
                    {"type", "response"},
                    {"id", std::to_string(message.id)},
                    {"data", {{"success", false}, {"error", "Invalid params JSON: " + params["__parse_error"].get<std::string>()}}}
                };
            } else {
                nlohmann::json request = {
                    {"type", "call"},
                    {"id", std::to_string(message.id)},
                    {"method", message.method},
                    {"params", params}
                };
                try {
                    responsePayload = nlohmann::json::parse(impl_->dispatcher->dispatch(request.dump()));
                } catch (const std::exception& e) {
                    responsePayload = {
                        {"type", "response"},
                        {"id", std::to_string(message.id)},
                        {"data", {{"success", false}, {"error", std::string("Invalid dispatcher response: ") + e.what()}}}
                    };
                }
            }

            ipc::IpcMessage response;
            response.type = ipc::IpcMessageType::Response;
            response.method = message.method;
            response.id = message.id;
            response.timestamp = static_cast<uint64_t>(
                std::chrono::steady_clock::now().time_since_epoch().count() / 1000000);
            response.payload = responsePayload.dump();

            if (!channelPtr->send(response)) {
                spdlog::warn("Failed to send local IPC response for {}", message.method);
            }
        });

        channel.setErrorCallback([](const std::string& error) {
            spdlog::warn("Local IPC error: {}", error);
        });
    };

    impl_->stopping.store(false);
    impl_->serverThread = std::thread([this, config, configureChannel]() {
        spdlog::info("Starting local IPC server on endpoint {}", config.serverName);

        while (!impl_->stopping.load()) {
            auto channel = ipc::IpcFactory::createServer(config);
            if (!channel) {
                spdlog::error("Failed to create local IPC server channel");
                running_.store(false);
                return;
            }

            configureChannel(*channel);

            {
                std::lock_guard<std::mutex> lock(impl_->channelMutex);
                impl_->currentChannel = channel.get();
            }

            if (!channel->connect(config.serverName)) {
                {
                    std::lock_guard<std::mutex> lock(impl_->channelMutex);
                    if (impl_->currentChannel == channel.get()) {
                        impl_->currentChannel = nullptr;
                    }
                }

                if (!impl_->stopping.load()) {
                    spdlog::error("Local IPC server failed to listen on {}", config.serverName);
                }
                running_.store(false);
                return;
            }

            channel->startReceiving();
            running_.store(true);
            spdlog::info("Local IPC server connected on {}", config.serverName);

            while (!impl_->stopping.load() && channel->isConnected()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            channel->disconnect();

            {
                std::lock_guard<std::mutex> lock(impl_->channelMutex);
                if (impl_->currentChannel == channel.get()) {
                    impl_->currentChannel = nullptr;
                }
            }
        }
    });

    // Don't set running_ = true here; the worker thread sets it after
    // channel->connect() succeeds. Callers should check isConnected()
    // or add a synchronization mechanism if they need to wait for readiness.
    return true;
}

void LocalIpcServer::stop() {
    if (!running_.load() && !impl_->serverThread.joinable()) {
        return;
    }

    impl_->stopping.store(true);

    {
        std::lock_guard<std::mutex> lock(impl_->channelMutex);
        if (impl_->currentChannel) {
            impl_->currentChannel->disconnect();
        }
    }

    if (impl_->serverThread.joinable()) {
        impl_->serverThread.join();
    }

    impl_->dispatcher.reset();
    impl_->triggerManager.reset();
    running_.store(false);
}

} // namespace wingman::runtime
