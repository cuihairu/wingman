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

// ========== HttpClient 构造/析构 ==========

TEST(HttpClientTest, ConstructionDoesNotCrash) {
    EXPECT_NO_THROW(HttpClient client);
}

TEST(HttpClientTest, GetInvalidUrlReturnsError) {
    HttpClient client;
    HttpOptions opts;
    opts.timeout = 2;
    HttpResponse resp = client.get("http://127.0.0.1:1/nonexistent", opts);
    // 连接应该失败，但不应崩溃
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
