#include "wingman/http.hpp"

#include <curl/curl.h>
#include <cstring>
#include <chrono>

namespace wingman {

// Callback: write response body
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    std::string* response = static_cast<std::string*>(userp);
    response->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

// Callback: write response headers
static size_t HeaderCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    std::string header(static_cast<char*>(contents), totalSize);

    // Remove \r\n
    if (!header.empty() && header.back() == '\n') {
        header.pop_back();
        if (!header.empty() && header.back() == '\r') {
            header.pop_back();
        }
    }

    if (!header.empty()) {
        size_t colonPos = header.find(':');
        if (colonPos != std::string::npos) {
            std::string key = header.substr(0, colonPos);
            std::string value = header.substr(colonPos + 1);

            // Trim spaces
            while (!value.empty() && value.front() == ' ') {
                value.erase(0, 1);
            }

            auto* headers = static_cast<std::unordered_map<std::string, std::string>*>(userp);
            (*headers)[key] = value;
        }
    }

    return totalSize;
}

HttpClient::HttpClient() : m_curl(nullptr), m_defaultTimeout(30) {
    m_curl = curl_easy_init();
}

HttpClient::~HttpClient() {
    if (m_curl) {
        curl_easy_cleanup(m_curl);
    }
}

void HttpClient::setDefaultHeader(const std::string& key, const std::string& value) {
    m_defaultHeaders[key] = value;
}

void HttpClient::setDefaultTimeout(int seconds) {
    m_defaultTimeout = seconds;
}

HttpResponse HttpClient::perform(const std::string& url,
                                  const std::string& method,
                                  const std::string& body,
                                  const HttpOptions& options) {
    HttpResponse response;
    if (!m_curl) {
        response.error = "CURL not initialized";
        return response;
    }

    auto startTime = std::chrono::steady_clock::now();

    // Set URL
    curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());

    // Set timeout
    long timeout = options.timeout > 0 ? options.timeout : m_defaultTimeout;
    curl_easy_setopt(m_curl, CURLOPT_TIMEOUT, timeout);
    curl_easy_setopt(m_curl, CURLOPT_CONNECTTIMEOUT, 10L);

    // Set follow redirects
    curl_easy_setopt(m_curl, CURLOPT_FOLLOWLOCATION, options.followRedirects ? 1L : 0L);
    curl_easy_setopt(m_curl, CURLOPT_MAXREDIRS, static_cast<long>(options.maxRedirects));

    // Set response callback
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &response.body);

    curl_easy_setopt(m_curl, CURLOPT_HEADERFUNCTION, HeaderCallback);
    curl_easy_setopt(m_curl, CURLOPT_HEADERDATA, &response.headers);

    // Set request headers
    struct curl_slist* headers = nullptr;

    // Default headers
    for (const auto& [key, value] : m_defaultHeaders) {
        std::string header = key + ": " + value;
        headers = curl_slist_append(headers, header.c_str());
    }

    // Option headers
    for (const auto& [key, value] : options.headers) {
        std::string header = key + ": " + value;
        headers = curl_slist_append(headers, header.c_str());
    }

    if (headers) {
        curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, headers);
    }

    // Set method and request body
    if (method == "POST") {
        curl_easy_setopt(m_curl, CURLOPT_POST, 1L);
        if (!body.empty()) {
            curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, body.c_str());
        }
    } else if (method == "PUT") {
        curl_easy_setopt(m_curl, CURLOPT_CUSTOMREQUEST, "PUT");
        if (!body.empty()) {
            curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, body.c_str());
        }
    } else if (method == "DELETE") {
        curl_easy_setopt(m_curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    } else if (method == "HEAD") {
        curl_easy_setopt(m_curl, CURLOPT_NOBODY, 1L);
    }
    // GET is default

    // Execute request
    CURLcode res = curl_easy_perform(m_curl);

    if (res == CURLE_OK) {
        long httpCode = 0;
        curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &httpCode);
        response.statusCode = static_cast<int>(httpCode);
    } else {
        response.error = curl_easy_strerror(res);
    }

    // Cleanup
    if (headers) {
        curl_slist_free_all(headers);
    }

    // Reset options (prepare for next request)
    curl_easy_reset(m_curl);

    // Calculate elapsed time
    auto endTime = std::chrono::steady_clock::now();
    response.elapsed = std::chrono::duration<double>(endTime - startTime).count();

    return response;
}

HttpResponse HttpClient::get(const std::string& url, const HttpOptions& options) {
    return perform(url, "GET", "", options);
}

HttpResponse HttpClient::post(const std::string& url,
                               const std::string& jsonBody,
                               const HttpOptions& options) {
    HttpOptions opts = options;
    if (opts.headers.find("Content-Type") == opts.headers.end()) {
        opts.headers["Content-Type"] = "application/json";
    }
    return perform(url, "POST", jsonBody, opts);
}

HttpResponse HttpClient::postForm(const std::string& url,
                                  const std::unordered_map<std::string, std::string>& fields,
                                  const HttpOptions& options) {
    // Build form data
    std::string formBody;
    for (auto it = fields.begin(); it != fields.end(); ++it) {
        if (it != fields.begin()) {
            formBody += "&";
        }
        formBody += it->first + "=" + it->second;
    }

    HttpOptions opts = options;
    opts.headers["Content-Type"] = "application/x-www-form-urlencoded";

    return perform(url, "POST", formBody, opts);
}

HttpResponse HttpClient::put(const std::string& url,
                              const std::string& jsonBody,
                              const HttpOptions& options) {
    HttpOptions opts = options;
    if (opts.headers.find("Content-Type") == opts.headers.end()) {
        opts.headers["Content-Type"] = "application/json";
    }
    return perform(url, "PUT", jsonBody, opts);
}

HttpResponse HttpClient::del(const std::string& url, const HttpOptions& options) {
    return perform(url, "DELETE", "", options);
}

} // namespace wingman
