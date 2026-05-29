#pragma once

#include <string>
#include <unordered_map>
#include <memory>

namespace wingman {

// HTTP response
struct HttpResponse {
    int statusCode;          // HTTP status code
    std::string body;        // Response body
    std::unordered_map<std::string, std::string> headers;  // Response headers
    std::string error;       // Error message
    double elapsed;          // Request duration (seconds)

    HttpResponse() : statusCode(0), elapsed(0) {}
    bool isSuccess() const { return statusCode >= 200 && statusCode < 300; }
};

// HTTP request options
struct HttpOptions {
    int timeout = 30;        // Timeout (seconds)
    std::unordered_map<std::string, std::string> headers;
    bool followRedirects = true;
    int maxRedirects = 5;
};

// HTTP client
class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    // GET request
    HttpResponse get(const std::string& url, const HttpOptions& options = {});

    // POST request (JSON body)
    HttpResponse post(const std::string& url,
                      const std::string& jsonBody,
                      const HttpOptions& options = {});

    // POST form
    HttpResponse postForm(const std::string& url,
                          const std::unordered_map<std::string, std::string>& fields,
                          const HttpOptions& options = {});

    // PUT request
    HttpResponse put(const std::string& url,
                     const std::string& jsonBody,
                     const HttpOptions& options = {});

    // DELETE request
    HttpResponse del(const std::string& url, const HttpOptions& options = {});

    // Set default request header
    void setDefaultHeader(const std::string& key, const std::string& value);

    // Set global timeout
    void setDefaultTimeout(int seconds);

private:
    void* m_curl;  // CURL* (void* to avoid including curl.h in header)
    std::unordered_map<std::string, std::string> m_defaultHeaders;
    int m_defaultTimeout;

    // Execute request
    HttpResponse perform(const std::string& url,
                         const std::string& method,
                         const std::string& body,
                         const HttpOptions& options);
};

} // namespace wingman
