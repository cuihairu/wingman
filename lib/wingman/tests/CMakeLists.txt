# Core Library Tests

if(BUILD_CORE_TESTS)
    enable_testing()

    # 调试：输出 vcpkg 安装目录
    message(STATUS "CMAKE_BINARY_DIR: ${CMAKE_BINARY_DIR}")
    message(STATUS "VCPKG_INSTALLED_DIR: $ENV{VCPKG_INSTALLED_DIR}")

    # 检查 GTest 头文件是否存在
    set(GTEST_INCLUDE_BASE "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows-static/include")
    if(EXISTS "${GTEST_INCLUDE_BASE}/gtest/gtest.h")
        message(STATUS "Found gtest/gtest.h at ${GTEST_INCLUDE_BASE}/gtest/gtest.h")
    elseif(EXISTS "${GTEST_INCLUDE_BASE}/googletest/gtest/gtest.h")
        message(STATUS "Found googletest/gtest/gtest.h at ${GTEST_INCLUDE_BASE}/googletest/gtest/gtest.h")
    else()
        message(WARNING "GTest headers not found in ${GTEST_INCLUDE_BASE}")
    endif()

    # 添加多个可能的 GTest include 目录
    include_directories(
        ${GTEST_INCLUDE_BASE}
        "${GTEST_INCLUDE_BASE}/gtest"
        "${GTEST_INCLUDE_BASE}/googletest"
        "${GTEST_INCLUDE_BASE}/googletest/include"
    )

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
