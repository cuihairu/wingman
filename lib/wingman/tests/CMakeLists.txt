# Core Library Tests

if(BUILD_CORE_TESTS)
    enable_testing()

    # 添加 GTest include 目录
    include_directories(${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows-static/include)

    # 创建测试可执行文件
    add_executable(core_tests
        kvstore_test.cpp
        trigger_test.cpp
        recorder_test.cpp
        remote_server_test.cpp
    )

    # 添加 include 目录
    target_include_directories(core_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )

    # 链接库 - 直接使用库文件路径
    target_link_libraries(core_tests PRIVATE
        ${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows-static/lib/gtest.lib
        ${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows-static/lib/gtest_main.lib
        wingman
    )

    # 注册测试
    include(GoogleTest)
    gtest_discover_tests(core_tests)

    message(STATUS "Core tests enabled")
endif()
