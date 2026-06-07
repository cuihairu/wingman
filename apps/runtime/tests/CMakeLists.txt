find_package(GTest CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

add_executable(runtime_tests
    cli_test.cpp
    screen_runtime_test.cpp
    cli_test_stubs.cpp
    ../src/cli.cpp
    ../src/packer.cpp
    ../src/runtime_context.cpp
    ../src/agent.cpp
    ../src/agent_config.cpp
    ../src/remote_client.cpp
    ../src/standalone_mode.cpp
    ../src/local_ipc_server.cpp
    ../src/rpc/handlers/script_handler.cpp
    ../src/commands/build_command.cpp
    ../src/commands/script_command.cpp
    ../src/commands/stop_command.cpp
)

target_include_directories(runtime_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${WINGMAN_GENERATED_DIR}
)

target_link_libraries(runtime_tests PRIVATE
    GTest::gtest
    GTest::gtest_main
    wingman
    wingman::proto
    wingman::transport
    nlohmann_json::nlohmann_json
    spdlog::spdlog_header_only
)

include(GoogleTest)
gtest_discover_tests(runtime_tests)
