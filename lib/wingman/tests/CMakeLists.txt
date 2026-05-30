# Core Library Tests

if(BUILD_CORE_TESTS)
    enable_testing()

    # 代码覆盖率选项
    option(CODE_COVERAGE "Enable code coverage" OFF)

    # 检查是否已经通过 vcpkg 安装了 GTest
    # 设置 GTest_DIR 指向全局 vcpkg 安装（如果存在）
    if(EXISTS "C:/Users/admin/vcpkg/installed/x64-windows-static/share/gtest/GTestConfig.cmake")
        set(GTest_DIR "C:/Users/admin/vcpkg/installed/x64-windows-static/share/gtest")
    endif()
    find_package(GTest CONFIG QUIET)

    if(GTest_FOUND)
        message(STATUS "Using GTest from vcpkg")
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
            clipboard_test.cpp
            filewatcher_test.cpp
            ipc_test.cpp
            json_test.cpp
            node_status_test.cpp
            component_test.cpp
            event_bus_test.cpp
            event_test.cpp
            event_module_test.cpp
            task_module_test.cpp
            behavior_tree_test.cpp
            config_test.cpp
            verification_test.cpp
            game_profile_test.cpp
            qrcode_test.cpp
            script_modules_test.cpp
            security_test.cpp
            ipc_factory_test.cpp
            accounts_test.cpp
            storage_test.cpp
            extra_modules_test.cpp
            script_manager_test.cpp
            http_test.cpp
            ui_automation_test.cpp
            ml_test.cpp
        )

        if(WIN32)
            target_sources(core_tests PRIVATE smart_trigger_test.cpp)
        endif()

        target_include_directories(core_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../include
            ${CMAKE_CURRENT_SOURCE_DIR}/../../../libs/lua/include
        )

        target_link_libraries(core_tests PRIVATE
            GTest::gtest
            GTest::gtest_main
            wingman
        )
    else()
        message(STATUS "GTest not found in vcpkg, downloading with FetchContent")
        include(FetchContent)

        # 检查本地源码树中是否有 googletest（绕过镜像问题）
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../../libs/googletest/CMakeLists.txt")
            message(STATUS "Using local libs/googletest")
            add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../../libs/googletest"
                             "${CMAKE_BINARY_DIR}/libs/googletest" EXCLUDE_FROM_ALL)

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
                human_test.cpp
                clipboard_test.cpp
                json_test.cpp
                node_status_test.cpp
                component_test.cpp
                event_bus_test.cpp
                event_test.cpp
                event_module_test.cpp
                task_module_test.cpp
                behavior_tree_test.cpp
                config_test.cpp
                verification_test.cpp
                game_profile_test.cpp
                qrcode_test.cpp
                script_modules_test.cpp
                security_test.cpp
                ipc_factory_test.cpp
                accounts_test.cpp
                storage_test.cpp
                extra_modules_test.cpp
                script_manager_test.cpp
                http_test.cpp
                ui_automation_test.cpp
                ml_test.cpp
                ocr_test.cpp
                script_function_test.cpp
            )

            if(WIN32)
                target_sources(core_tests PRIVATE smart_trigger_test.cpp)
            endif()

            target_include_directories(core_tests PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/../include
                ${CMAKE_CURRENT_SOURCE_DIR}/../../../libs/lua/include
            )

            target_link_libraries(core_tests PRIVATE
                gtest
                gtest_main
                wingman
            )
        else()
            # 使用 FetchContent 下载
            FetchContent_Declare(
                googletest
                GIT_REPOSITORY https://github.com/google/googletest.git
                GIT_TAG v1.17.0
            )

            # 对于 Windows，防止覆盖父项目的选项
            set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
            FetchContent_MakeAvailable(googletest)

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
                human_test.cpp
                clipboard_test.cpp
                json_test.cpp
                node_status_test.cpp
                component_test.cpp
                event_bus_test.cpp
                event_test.cpp
                event_module_test.cpp
                task_module_test.cpp
                behavior_tree_test.cpp
                config_test.cpp
                verification_test.cpp
                game_profile_test.cpp
                qrcode_test.cpp
                script_modules_test.cpp
                security_test.cpp
                ipc_factory_test.cpp
                accounts_test.cpp
                storage_test.cpp
                extra_modules_test.cpp
                script_manager_test.cpp
                http_test.cpp
                ui_automation_test.cpp
                ml_test.cpp
                ocr_test.cpp
                script_function_test.cpp
            )

            if(WIN32)
                target_sources(core_tests PRIVATE smart_trigger_test.cpp)
            endif()

            target_include_directories(core_tests PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/../include
                ${CMAKE_CURRENT_SOURCE_DIR}/../../../libs/lua/include
            )

            target_link_libraries(core_tests PRIVATE
                gtest
                gtest_main
                wingman
            )
        endif()
    endif()

    # 应用覆盖率选项（必须在目标创建之后）
    if(CODE_COVERAGE)
        message(STATUS "Code coverage enabled")

        # GCC/Clang 覆盖率标志
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(core_tests PRIVATE --coverage)
            target_link_options(core_tests PRIVATE --coverage)
        # MSVC 覆盖率标志
        elseif(MSVC)
            target_compile_options(core_tests PRIVATE /profile)
            target_link_options(core_tests PRIVATE /PROFILE)
        endif()
    endif()

    # 注册测试
    include(GoogleTest)
    gtest_discover_tests(core_tests)

    message(STATUS "Core tests enabled")
endif()
