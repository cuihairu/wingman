# Core Library Tests

if(BUILD_CORE_TESTS)
    enable_testing()

    # 使用 FetchContent 下载 GTest
    include(FetchContent)

    # 检查是否已经通过 vcpkg 安装了 GTest
    set(GTEST_LIB_PATH "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows-static/lib/gtest.lib")
    set(GTEST_MAIN_LIB_PATH "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows-static/lib/gtest_main.lib")
    set(GTEST_INCLUDE_PATH "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows-static/include")

    if(EXISTS "${GTEST_LIB_PATH}" AND EXISTS "${GTEST_INCLUDE_PATH}/gtest/gtest.h")
        message(STATUS "Using GTest from vcpkg")
        set(GTEST_FOUND TRUE)
    else()
        message(STATUS "GTest not found in vcpkg, downloading with FetchContent")
        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.17.0
        )

        # 对于 Windows，防止覆盖父项目的选项
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)

        # 设置 GTest 路径
        set(GTEST_INCLUDE_PATH "${googletest_SOURCE_DIR}/include")
        set(GTEST_LIB_PATH "${googletest_BINARY_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}gtest${CMAKE_STATIC_LIBRARY_SUFFIX}")
        set(GTEST_MAIN_LIB_PATH "${googletest_BINARY_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}gtest_main${CMAKE_STATIC_LIBRARY_SUFFIX}")

        set(GTEST_FOUND TRUE)
    endif()

    if(GTEST_FOUND)
        # 创建测试可执行文件
        add_executable(core_tests
            kvstore_test.cpp
            trigger_test.cpp
            recorder_test.cpp
            remote_server_test.cpp
            screen_test.cpp
            input_test.cpp
            vision_test.cpp
            window_test.cpp
        )

        # 添加 include 目录
        target_include_directories(core_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../include
            ${GTEST_INCLUDE_PATH}
        )

        # 链接库
        if(TARGET gtest)
            # 使用 FetchContent 的 target
            target_link_libraries(core_tests PRIVATE
                gtest
                gtest_main
                wingman
            )
        else()
            # 使用 vcpkg 的库文件
            target_link_libraries(core_tests PRIVATE
                ${GTEST_LIB_PATH}
                ${GTEST_MAIN_LIB_PATH}
                wingman
            )
        endif()
    else()
        message(FATAL_ERROR "GTest not found")
    endif()

    # 注册测试
    include(GoogleTest)
    gtest_discover_tests(core_tests)

    message(STATUS "Core tests enabled")
endif()
