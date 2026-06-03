#include "wingman/http.hpp"

#include <chrono>

#ifdef WINGMAN_HAS_CURL
#include <curl/curl.h>
#endif

namespace wingman {

namespace {

#ifndef WINGMAN_HAS_CURL
HttpResponse makeUnavailableResponse() {
    HttpResponse response;
    response.error = "HTTP support is not enabled in this build";
    return response;
}
#endif

#ifdef WINGMAN_HAS_CURL

void ensureCurlGlobalInit() {
    static const bool initialized = []() {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        return true;
    }();
    (void)initialized;
}

size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    const size_t totalSize = size * nmemb;
    auto* response = static_cast<std::string*>(userp);
    response->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

size_t HeaderCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    const size_t totalSize = size * nmemb;
    std::string header(static_cast<char*>(contents), totalSize);

    if (!header.empty() && header.back() == '\n') {
        header.pop_back();
        if (!header.empty() && header.back() == '\r') {
            header.pop_back();
        }
    }

    if (!header.empty()) {
        const size_t colonPos = header.find(':');
        if (colonPos != std::string::npos) {
            std::string key = header.substr(0, colonPos);
            std::string value = header.substr(colonPos + 1);

            while (!value.empty() && value.front() == ' ') {
                value.erase(0, 1);
            }

            auto* headers = static_cast<std::unordered_map<std::string, std::string>*>(userp);
            (*headers)[key] = value;
        }
    }

    return totalSize;
}

#endif

} // namespace

HttpClient::HttpClient() : m_curl(nullptr), m_defaultTimeout(30) {
#ifdef WINGMAN_HAS_CURL
    ensureCurlGlobalInit();
    m_curl = curl_easy_init();
#endif
}

HttpClient::~HttpClient() {
#ifdef WINGMAN_HAS_CURL
    if (m_curl) {
        curl_easy_cleanup(static_cast<CURL*>(m_curl));
    }
#endif
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
#ifndef WINGMAN_HAS_CURL
    (void)url;
    (void)method;
    (void)body;
    (void)options;
    return makeUnavailableResponse();
#else
    HttpResponse response;
    auto* curl = static_cast<CURL*>(m_curl);
    if (!curl) {
        response.error = "CURL not initialized";
        return response;
    }

    const auto startTime = std::chrono::steady_clock::now();

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    const long timeout = options.timeout > 0 ? options.timeout : m_defaultTimeout;
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);

    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, options.followRedirects ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, static_cast<long>(options.maxRedirects));

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);

    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, HeaderCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response.headers);

    curl_slist* headers = nullptr;
    for (const auto& [key, value] : m_defaultHeaders) {
        const std::string header = key + ": " + value;
        headers = curl_slist_append(headers, header.c_str());
    }

    for (const auto& [key, value] : options.headers) {
        const std::string header = key + ": " + value;
        headers = curl_slist_append(headers, header.c_str());
    }

    if (headers) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        if (!body.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        }
    } else if (method == "PUT") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        if (!body.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        }
    } else if (method == "DELETE") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    } else if (method == "HEAD") {
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    }

    const CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        response.statusCode = static_cast<int>(httpCode);
    } else {
        response.error = curl_easy_strerror(res);
    }

    if (headers) {
        curl_slist_free_all(headers);
    }

    curl_easy_reset(curl);

    const auto endTime = std::chrono::steady_clock::now();
    response.elapsed = std::chrono::duration<double>(endTime - startTime).count();
    return response;
#endif
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
