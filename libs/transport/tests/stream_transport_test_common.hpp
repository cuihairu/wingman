#pragma once
/**
 * Stream / Session / Channel / Transport 集成测试
 * 使用本地回环 TCP 连接验证流通道、会话、通道多路复用与传输层。
 */

#include <gtest/gtest.h>
#include "wingman/transport/stream_type.hpp"
#include "wingman/transport/stream_channel.hpp"
#include "wingman/transport/stream_manager.hpp"
#include "wingman/transport/transport.hpp"
#include "wingman/transport/transport_client.hpp"
#include "wingman/transport/transport_server.hpp"
#include <asio.hpp>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <optional>
#include <random>
#include <thread>

#ifndef _WIN32
    #include <fcntl.h>
    #include <sys/resource.h>
#endif

using namespace std::chrono_literals;
using namespace wingman::transport;

// 选择一个空闲端口（通过绑定后释放来探测）
inline int findFreePort() {
    asio::io_context io;
    asio::ip::tcp::acceptor acceptor(io);
    asio::ip::tcp::endpoint endpoint(asio::ip::make_address("127.0.0.1"), 0);
    acceptor.open(endpoint.protocol());
    acceptor.bind(endpoint);
    auto port = acceptor.local_endpoint().port();
    acceptor.close();
    return static_cast<int>(port);
}

#ifndef _WIN32
// 将 RLIMIT_NOFILE 软上限压到当前已打开的 fd 数量，使下一个 ::socket()
// 以 EMFILE 失败；析构时恢复原值。RLIMIT 是进程级设置，gtest 串行执行
// 下安全（各 fixture 的后台线程均已在 teardown 中 join）。
class FdExhaustionGuard {
public:
    FdExhaustionGuard() {
        ::getrlimit(RLIMIT_NOFILE, &original_);

        // Linux 的 RLIMIT_NOFILE 按「新分配 fd 的编号」判 EMFILE（编号 >= soft），
        // 不是按「已打开个数」——ctest 独立进程 / CI runner 环境下 fd 编号可能
        // 稀疏（如 {0,1,2,3,4,900}，存在高于「打开个数」的已占编号），把 soft
        // 压到「打开个数」拦不住低位的空闲编号，socket 照样成功（CI 实测）。
        // 改为把 soft 压到「最低空闲编号」：任何新分配的首选编号必 >= soft，
        // 与 fd 布局无关，严格生效。
        rlim_t lowestFree = 0;
        while (::fcntl(static_cast<int>(lowestFree), F_GETFD) != -1) {
            ++lowestFree;
        }

        rlimit exhausted{lowestFree, original_.rlim_max};
        ::setrlimit(RLIMIT_NOFILE, &exhausted);
    }

    ~FdExhaustionGuard() {
        ::setrlimit(RLIMIT_NOFILE, &original_);
    }

    FdExhaustionGuard(const FdExhaustionGuard&) = delete;
    FdExhaustionGuard& operator=(const FdExhaustionGuard&) = delete;

private:
    rlimit original_{};
};
#endif

class DataSink {
public:
    void append(const uint8_t* data, size_t size) {
        std::lock_guard<std::mutex> lock(mutex_);
        buffer.insert(buffer.end(), data, data + size);
        cv.notify_all();
    }

    bool waitForSize(size_t expected, std::chrono::milliseconds timeout = 2s) {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv.wait_for(lock, timeout, [&] { return buffer.size() >= expected; });
    }

    std::vector<uint8_t> data() {
        std::lock_guard<std::mutex> lock(mutex_);
        return buffer;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        buffer.clear();
    }

private:
    std::mutex mutex_;
    std::condition_variable cv;
    std::vector<uint8_t> buffer;
};

