# wingman::agentcore 单元测试
# 模式与 libs/transport/tests 一致：独立可执行 + gtest_discover_tests。
find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(agentcore_tests agentcore_test.cpp)

# ProxyTunnel 是 RemoteClient 的内部组件（src/ 头），测试经源目录直含
target_include_directories(agentcore_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)

target_link_libraries(agentcore_tests PRIVATE
    wingman::agentcore
    GTest::gtest
    GTest::gtest_main
)

# agentcore_tests 继承 wingman_agentcore 的 PUBLIC 依赖（transport→asio、
# nlohmann_json、spdlog），无需重复 find_package。
gtest_discover_tests(agentcore_tests DISCOVERY_MODE PRE_TEST)
