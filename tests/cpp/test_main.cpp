#include <gtest/gtest.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

int main(int argc, char** argv) {
    // Initialize logging
    auto console = spdlog::stdout_color_mt("wingman_test");
    spdlog::set_default_logger(console);
    spdlog::set_level(spdlog::level::debug);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
