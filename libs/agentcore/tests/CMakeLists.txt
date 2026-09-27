# wingman::agentcore 单元测试
# 模式与 libs/transport/tests 一致：独立可执行 + gtest_discover_tests。
find_package(GTest CONFIG REQUIRED)

add_executable(agentcore_tests agentcore_test.cpp)

target_link_libraries(agentcore_tests PRIVATE
    wingman::agentcore
    GTest::gtest
    GTest::gtest_main
)

# agentcore_tests 继承 wingman_agentcore 的 PUBLIC 依赖（transport→asio、
# nlohmann_json、spdlog），无需重复 find_package。
gtest_discover_tests(agentcore_tests DISCOVERY_MODE PRE_TEST)
