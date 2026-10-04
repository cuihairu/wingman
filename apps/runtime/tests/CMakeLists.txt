find_package(GTest CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

add_executable(runtime_tests
    agent_config_test.cpp
    agent_loopback_test.cpp
    cli_test.cpp
    resource_pack_test.cpp
    # ResourceLoader 实例接口补测（构造探测/错误回调/资源信息/loadScript
    # 无嵌入早退；PE 读取腿为 Windows 产物路径，Linux 结构性不可达）
    resource_loader_interface_test.cpp
    screen_runtime_test.cpp
    screenshot_handler_test.cpp
    system_handler_test.cpp
    rpc_ipc_test.cpp
    # StandaloneMode 缺口补测（start 幂等/建目录失败、autoStart 装配与输出
    # 回调空串跳过、状态映射现状钉、批量操作闲置计数 0、manager 失同步降级腿、
    # getConfig；pause/resume/stop 成功腿为状态机结构性不可达，登记在案）
    standalone_mode_coverage_test.cpp
    # notify.tray.* 意图 → EventBuffer 桥（命名去前缀/透传/幂等安装）
    notify_bridge_test.cpp
    cli_test_stubs.cpp
    ../src/cli.cpp
    ../src/packer.cpp
    ../src/runtime_context.cpp
    ../src/agent.cpp
    ../src/agent_config.cpp
    ../src/standalone_mode.cpp
    ../src/local_ipc_server.cpp
    ../src/notify_bridge.cpp
    ../src/rpc/handlers/script_handler.cpp
    ../src/rpc/handlers/system_handler.cpp
    ../src/rpc/handlers/config_handler.cpp
    ../src/rpc/handlers/screenshot_handler.cpp
    ../src/rpc/handlers/event_handler.cpp
    ../src/rpc/handlers/macro_handler.cpp
    ../src/commands/build_command.cpp
    ../src/commands/script_command.cpp
    ../src/commands/stop_command.cpp
    ../src/resource_loader.cpp
)

target_include_directories(runtime_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../libs/lua/include
    ${WINGMAN_GENERATED_DIR}
)

target_link_libraries(runtime_tests PRIVATE
    GTest::gtest
    GTest::gtest_main
    wingman
    wingman::agentcore
    wingman::transport
    nlohmann_json::nlohmann_json
    spdlog::spdlog_header_only
)

# rpc_ipc_test 的脚本生命周期用例需要真实 Lua 引擎（registerLuaEngine()
# 自注册进 ScriptEngineFactory，与 lib/wingman/tests 的接线方式一致）
if(TARGET wingman::lua)
    target_link_libraries(runtime_tests PRIVATE wingman::lua)
endif()

include(GoogleTest)
gtest_discover_tests(runtime_tests DISCOVERY_MODE PRE_TEST)
