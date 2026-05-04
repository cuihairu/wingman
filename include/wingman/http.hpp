#pragma once

#include <string>
#include <unordered_map>
#include <memory>

namespace wingman {

// HTTP 响应
struct HttpResponse {
    int statusCode;          // HTTP 状态码
    std::string body;        // 响应体
    std::unordered_map<std::string, std::string> headers;  // 响应头
    std::string error;       // 错误信息
    double elapsed;          // 请求耗时（秒）

    HttpResponse() : statusCode(0), elapsed(0) {}
    bool isSuccess() const { return statusCode >= 200 && statusCode < 300; }
};

// HTTP 请求选项
struct HttpOptions {
    int timeout = 30;        // 超时时间（秒）
    std::unordered_map<std::string, std::string> headers;
    bool followRedirects = true;
    int maxRedirects = 5;
};

// HTTP 客户端
class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    // GET 请求
    HttpResponse get(const std::string& url, const HttpOptions& options = {});

    // POST 请求 (JSON body)
    HttpResponse post(const std::string& url,
                      const std::string& jsonBody,
                      const HttpOptions& options = {});

    // POST 表单
    HttpResponse postForm(const std::string& url,
                          const std::unordered_map<std::string, std::string>& fields,
                          const HttpOptions& options = {});

    // PUT 请求
    HttpResponse put(const std::string& url,
                     const std::string& jsonBody,
                     const HttpOptions& options = {});

    // DELETE 请求
    HttpResponse del(const std::string& url, const HttpOptions& options = {});

    // 设置默认请求头
    void setDefaultHeader(const std::string& key, const std::string& value);

    // 设置全局超时
    void setDefaultTimeout(int seconds);

private:
    void* m_curl;  // CURL* (void* 避免在头文件中包含 curl.h)
    std::unordered_map<std::string, std::string> m_defaultHeaders;
    int m_defaultTimeout;

    // 执行请求
    HttpResponse perform(const std::string& url,
                         const std::string& method,
                         const std::string& body,
                         const HttpOptions& options);
};

} // namespace wingman
