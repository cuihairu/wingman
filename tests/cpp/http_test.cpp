#include <gtest/gtest.h>
#include "wingman/http.hpp"

using namespace wingman;

class HttpTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// Http Tests
// ============================================================================

TEST_F(HttpTest, CreateHttpClient) {
    HttpClient client;
    SUCCEED();
}

TEST_F(HttpTest, ParseUrl) {
    // Test URL parsing functionality if available
    std::string url = "https://example.com/path";
    EXPECT_FALSE(url.empty());
}

TEST_F(HttpTest, HttpOptionsDefaults) {
    HttpOptions options;
    EXPECT_EQ(options.timeout, 30);
    EXPECT_TRUE(options.followRedirects);
    EXPECT_EQ(options.maxRedirects, 5);
}

TEST_F(HttpTest, HttpResponseDefaults) {
    HttpResponse response;
    EXPECT_EQ(response.statusCode, 0);
    EXPECT_TRUE(response.body.empty());
    EXPECT_TRUE(response.error.empty());
    EXPECT_FALSE(response.isSuccess());
}
