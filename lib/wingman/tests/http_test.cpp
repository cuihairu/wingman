#include <gtest/gtest.h>
#include "wingman/http.hpp"

using namespace wingman;

// ========== HttpResponse ==========

TEST(HttpResponseTest, DefaultValues) {
    HttpResponse resp;
    EXPECT_EQ(resp.statusCode, 0);
    EXPECT_TRUE(resp.body.empty());
    EXPECT_TRUE(resp.headers.empty());
    EXPECT_TRUE(resp.error.empty());
    EXPECT_DOUBLE_EQ(resp.elapsed, 0.0);
}

TEST(HttpResponseTest, IsSuccess) {
    HttpResponse resp;
    resp.statusCode = 200;
    EXPECT_TRUE(resp.isSuccess());

    resp.statusCode = 201;
    EXPECT_TRUE(resp.isSuccess());

    resp.statusCode = 299;
    EXPECT_TRUE(resp.isSuccess());

    resp.statusCode = 100;
    EXPECT_FALSE(resp.isSuccess());

    resp.statusCode = 300;
    EXPECT_FALSE(resp.isSuccess());

    resp.statusCode = 404;
    EXPECT_FALSE(resp.isSuccess());

    resp.statusCode = 500;
    EXPECT_FALSE(resp.isSuccess());
}

// ========== HttpOptions ==========

TEST(HttpOptionsTest, DefaultValues) {
    HttpOptions opts;
    EXPECT_EQ(opts.timeout, 30);
    EXPECT_TRUE(opts.headers.empty());
    EXPECT_TRUE(opts.followRedirects);
    EXPECT_EQ(opts.maxRedirects, 5);
}

// ========== HttpClient Construction/Destruction ==========

TEST(HttpClientTest, ConstructionDoesNotCrash) {
    EXPECT_NO_THROW(HttpClient client);
}

TEST(HttpClientTest, GetInvalidUrlReturnsError) {
    HttpClient client;
    HttpOptions opts;
    opts.timeout = 2;
    HttpResponse resp = client.get("http://127.0.0.1:1/nonexistent", opts);
    // Connection should fail, but should not crash
    EXPECT_NE(resp.statusCode, 200);
}

TEST(HttpClientTest, SetDefaultHeaderDoesNotCrash) {
    HttpClient client;
    EXPECT_NO_THROW(client.setDefaultHeader("X-Custom", "value"));
}

TEST(HttpClientTest, SetDefaultTimeoutDoesNotCrash) {
    HttpClient client;
    EXPECT_NO_THROW(client.setDefaultTimeout(60));
}

TEST(HttpClientTest, PostToInvalidUrl) {
    HttpClient client;
    HttpOptions opts;
    opts.timeout = 2;
    HttpResponse resp = client.post("http://127.0.0.1:1/api", "{}", opts);
    EXPECT_NE(resp.statusCode, 200);
}

TEST(HttpClientTest, PostFormToInvalidUrl) {
    HttpClient client;
    HttpOptions opts;
    opts.timeout = 2;
    std::unordered_map<std::string, std::string> fields = {{"key", "value"}};
    HttpResponse resp = client.postForm("http://127.0.0.1:1/api", fields, opts);
    EXPECT_NE(resp.statusCode, 200);
}

TEST(HttpClientTest, PutToInvalidUrl) {
    HttpClient client;
    HttpOptions opts;
    opts.timeout = 2;
    HttpResponse resp = client.put("http://127.0.0.1:1/api", "{}", opts);
    EXPECT_NE(resp.statusCode, 200);
}

TEST(HttpClientTest, DeleteInvalidUrl) {
    HttpClient client;
    HttpOptions opts;
    opts.timeout = 2;
    HttpResponse resp = client.del("http://127.0.0.1:1/api", opts);
    EXPECT_NE(resp.statusCode, 200);
}

// ========== Additional Http Tests ==========

TEST(HttpResponseTest, FieldAssignment) {
    HttpResponse resp;
    resp.statusCode = 404;
    resp.body = "Not Found";
    resp.error = "Connection refused";
    resp.elapsed = 1.5;
    resp.headers["Content-Type"] = "text/plain";

    EXPECT_EQ(resp.statusCode, 404);
    EXPECT_EQ(resp.body, "Not Found");
    EXPECT_EQ(resp.error, "Connection refused");
    EXPECT_DOUBLE_EQ(resp.elapsed, 1.5);
    EXPECT_EQ(resp.headers["Content-Type"], "text/plain");
    EXPECT_FALSE(resp.isSuccess());
}

TEST(HttpResponseTest, SuccessBoundaryCodes) {
    HttpResponse resp;
    resp.statusCode = 200;
    EXPECT_TRUE(resp.isSuccess());
    resp.statusCode = 199;
    EXPECT_FALSE(resp.isSuccess());
    resp.statusCode = 300;
    EXPECT_FALSE(resp.isSuccess());
}

TEST(HttpOptionsTest, CustomValues) {
    HttpOptions opts;
    opts.timeout = 60;
    opts.followRedirects = false;
    opts.maxRedirects = 10;
    opts.headers["Authorization"] = "Bearer token";

    EXPECT_EQ(opts.timeout, 60);
    EXPECT_FALSE(opts.followRedirects);
    EXPECT_EQ(opts.maxRedirects, 10);
    EXPECT_EQ(opts.headers.at("Authorization"), "Bearer token");
}

TEST(HttpClientTest, MultipleDefaultHeaders) {
    HttpClient client;
    EXPECT_NO_THROW(client.setDefaultHeader("X-Header-1", "value1"));
    EXPECT_NO_THROW(client.setDefaultHeader("X-Header-2", "value2"));
}

TEST(HttpClientTest, GetWithCustomHeaders) {
    HttpClient client;
    HttpOptions opts;
    opts.timeout = 2;
    opts.headers["X-Custom"] = "test";
    HttpResponse resp = client.get("http://127.0.0.1:1/test", opts);
    EXPECT_NE(resp.statusCode, 200);
}

TEST(HttpClientTest, PostWithEmptyBody) {
    HttpClient client;
    HttpOptions opts;
    opts.timeout = 2;
    HttpResponse resp = client.post("http://127.0.0.1:1/api", "", opts);
    EXPECT_NE(resp.statusCode, 200);
}

TEST(HttpClientTest, PutWithEmptyBody) {
    HttpClient client;
    HttpOptions opts;
    opts.timeout = 2;
    HttpResponse resp = client.put("http://127.0.0.1:1/api", "", opts);
    EXPECT_NE(resp.statusCode, 200);
}

TEST(HttpClientTest, PostFormEmptyFields) {
    HttpClient client;
    HttpOptions opts;
    opts.timeout = 2;
    std::unordered_map<std::string, std::string> empty;
    HttpResponse resp = client.postForm("http://127.0.0.1:1/api", empty, opts);
    EXPECT_NE(resp.statusCode, 200);
}

// ========== 本地真 HTTP server：成功路径 / 各方法 / 头与表单编码 ==========
// 此前用例全部打 127.0.0.1:1（连接拒绝），perform 主干与回调零覆盖。
// server 为 POSIX socket 手写最小实现（收整请求 → 回测试定制的响应），
// Windows 无此套接字 API，整体 gate 在 _WIN32 之外。

#ifndef _WIN32

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <functional>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

namespace {

// 收单个请求的线程化本地 server：handler 收到解析后的请求四元组，
// 返回要发送的原始响应字节。析构时停线程并关闭监听。
class LocalHttpServer {
public:
    struct Request {
        std::string method;
        std::string path;
        std::string body;
        std::string headers;  // 原始头块（不含终止空行）
    };

    explicit LocalHttpServer(std::function<std::string(const Request&)> handler) : handler_(std::move(handler)) {
        listenFd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        int reuse = 1;
        ::setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;  // 随机端口，避免并行用例冲突
        ok_ = ::bind(listenFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0 &&
              ::listen(listenFd_, 4) == 0;
        socklen_t len = sizeof(addr);
        if (ok_ && ::getsockname(listenFd_, reinterpret_cast<sockaddr*>(&addr), &len) == 0) {
            port_ = ntohs(addr.sin_port);
        }
        if (ok_) thread_ = std::thread([this] { serveLoop(); });
    }

    bool valid() const { return ok_ && port_ > 0; }

    ~LocalHttpServer() {
        running_ = false;
        if (listenFd_ >= 0) {
            // Linux 上 close 不会唤醒阻塞中的 accept，必须先 shutdown
            ::shutdown(listenFd_, SHUT_RDWR);
            ::close(listenFd_);
        }
        if (thread_.joinable()) thread_.join();
    }

    int port() const { return port_; }

    Request lastRequest() {
        std::lock_guard<std::mutex> lock(m_);
        return last_;
    }

private:
    void serveLoop() {
        while (running_) {
            int cfd = ::accept(listenFd_, nullptr, nullptr);
            if (cfd < 0) return;
            serveOne(cfd);
            ::close(cfd);
        }
    }

    void serveOne(int cfd) {
        Request req;
        std::string raw;
        char buf[4096];
        while (raw.find("\r\n\r\n") == std::string::npos) {
            ssize_t n = ::recv(cfd, buf, sizeof(buf), 0);
            if (n <= 0) return;
            raw.append(buf, static_cast<size_t>(n));
        }
        const size_t headerEnd = raw.find("\r\n\r\n");
        req.headers = raw.substr(0, headerEnd);

        // Content-Length 补齐 body
        size_t contentLen = 0;
        std::string clKey = "Content-Length:";
        size_t at = req.headers.find(clKey);
        if (at == std::string::npos) {
            at = req.headers.find("content-length:");
            clKey = "content-length:";
        }
        if (at != std::string::npos) {
            contentLen = static_cast<size_t>(std::atoi(req.headers.c_str() + at + clKey.size()));
        }
        while (raw.size() < headerEnd + 4 + contentLen) {
            ssize_t n = ::recv(cfd, buf, sizeof(buf), 0);
            if (n <= 0) break;
            raw.append(buf, static_cast<size_t>(n));
        }
        req.body = raw.substr(headerEnd + 4, contentLen);

        std::istringstream line(req.headers);
        line >> req.method >> req.path;

        {
            std::lock_guard<std::mutex> lock(m_);
            last_ = req;
        }
        const std::string response = handler_(req);
        size_t sent = 0;
        while (sent < response.size()) {
            ssize_t n = ::send(cfd, response.data() + sent, response.size() - sent, 0);
            if (n <= 0) break;
            sent += static_cast<size_t>(n);
        }
    }

    std::function<std::string(const Request&)> handler_;
    bool ok_ = false;
    int listenFd_ = -1;
    int port_ = 0;
    std::atomic<bool> running_{true};
    std::thread thread_;
    std::mutex m_;
    Request last_;
};

std::string httpRequestLine(const char* method, int port, const char* path = "/t") {
    std::ostringstream oss;
    oss << method << " " << path << " HTTP/1.1\r\n"
        << "Host: 127.0.0.1:" << port << "\r\n";
    return oss.str();
}

} // namespace

TEST(HttpClientTest, LocalServerGetRoundtrip) {
    LocalHttpServer server([](const LocalHttpServer::Request&) {
        return std::string("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n"
                           "X-Wingman: test\r\nContent-Length: 5\r\n\r\nhello");
    });
    ASSERT_TRUE(server.valid());
    HttpClient client;
    HttpResponse resp = client.get("http://127.0.0.1:" + std::to_string(server.port()) + "/t");
    EXPECT_TRUE(resp.error.empty()) << resp.error;
    EXPECT_EQ(resp.statusCode, 200);
    EXPECT_TRUE(resp.isSuccess());
    EXPECT_EQ(resp.body, "hello");
    EXPECT_EQ(resp.headers["Content-Type"], "text/plain");
    EXPECT_EQ(resp.headers["X-Wingman"], "test");
    EXPECT_GT(resp.elapsed, 0.0);
}

TEST(HttpClientTest, LocalServerMethodBranches) {
    LocalHttpServer server([](const LocalHttpServer::Request& req) {
        std::ostringstream oss;
        oss << "HTTP/1.1 200 OK\r\nX-Method: " << req.method
            << "\r\nContent-Length: " << req.body.size() << "\r\n\r\n" << req.body;
        return oss.str();
    });
    ASSERT_TRUE(server.valid());
    const std::string base = "http://127.0.0.1:" + std::to_string(server.port()) + "/t";
    HttpClient client;

    auto post = client.post(base, R"({"k":1})");
    EXPECT_EQ(post.statusCode, 200);
    EXPECT_EQ(post.headers["X-Method"], "POST");
    EXPECT_EQ(post.body, R"({"k":1})");  // POSTFIELDS 真实送达

    auto put = client.put(base, "put-payload");
    EXPECT_EQ(put.headers["X-Method"], "PUT");
    EXPECT_EQ(put.body, "put-payload");

    auto del = client.del(base);
    EXPECT_EQ(del.headers["X-Method"], "DELETE");

    // HEAD 分支（CURLOPT_NOBODY）经 perform 私有接口、公共面无 HEAD 入口——
    // 胶水不可达，不硬凑（记录于 CHANGELOG）
}

TEST(HttpClientTest, LocalServerPostFormAndDefaultHeaders) {
    LocalHttpServer server([](const LocalHttpServer::Request&) {
        return std::string("HTTP/1.1 204 No Content\r\nContent-Length: 0\r\n\r\n");
    });
    HttpClient client;
    ASSERT_TRUE(server.valid());
    client.setDefaultHeader("X-Default", "d1");

    std::unordered_map<std::string, std::string> fields{{"user", "wm"}, {"token", "abc"}};
    HttpResponse resp = client.postForm(
        "http://127.0.0.1:" + std::to_string(server.port()) + "/form", fields);
    EXPECT_EQ(resp.statusCode, 204);
    EXPECT_TRUE(resp.body.empty());

    const auto req = server.lastRequest();
    EXPECT_NE(req.headers.find("X-Default: d1"), std::string::npos);
    EXPECT_NE(req.headers.find("Content-Type: application/x-www-form-urlencoded"),
              std::string::npos);
    EXPECT_NE(req.body.find("user=wm"), std::string::npos);
    EXPECT_NE(req.body.find("token=abc"), std::string::npos);
    EXPECT_NE(req.body.find("&"), std::string::npos);
}

#endif // _WIN32
