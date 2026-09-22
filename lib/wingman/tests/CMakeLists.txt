# Core Library Tests

if(BUILD_CORE_TESTS)
    enable_testing()

    # Code coverage option
    option(CODE_COVERAGE "Enable code coverage" OFF)

    find_package(GTest CONFIG REQUIRED)
    message(STATUS "Using GTest from vcpkg")

    add_executable(core_tests
        kvstore_test.cpp
        trigger_test.cpp
        recorder_test.cpp
        screen_test.cpp
        input_test.cpp
        vision_test.cpp
        window_test.cpp
        human_test.cpp
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
        notify_module_test.cpp
        fsm_module_test.cpp
        timer_module_test.cpp
        behavior_tree_test.cpp
        config_test.cpp
        verification_test.cpp
        game_profile_test.cpp
        script_modules_test.cpp
        security_test.cpp
        ipc_factory_test.cpp
        storage_test.cpp
        extra_modules_test.cpp
        script_manager_test.cpp
        http_test.cpp
        ui_automation_test.cpp
        ml_test.cpp
        ocr_test.cpp
        script_function_test.cpp
        game_profile_io_test.cpp
        tcp_channel_test.cpp
        trigger_engine_test.cpp
        image_analyzer_test.cpp
        db_module_test.cpp
        ini_module_test.cpp
        crypt_test.cpp
        rpc_test.cpp
    )

    # Unix domain socket 通道与 posix TriggerManager 分支补测仅在非 Windows 平台编译
    # （前者依赖 POSIX socket API；后者针对 posix_trigger.cpp 实现行为，用例含
    #   unistd.h/access——2026-09-22 Windows CI 编译失败后收进本 gate）
    if(NOT WIN32)
        target_sources(core_tests PRIVATE unix_socket_channel_test.cpp)
        target_sources(core_tests PRIVATE trigger_posix_coverage_test.cpp)
    endif()

    # Linux X11 平台集成测试（X11 链接随 wingman PUBLIC 传递；无 X 运行时用例自行 skip）
    if(UNIX AND NOT APPLE)
        target_sources(core_tests PRIVATE platform_x11_test.cpp)
        # XRecord 正向录制端到端（真实桌面 X server 上执行；Xvfb 下 EnableContext
        # 必然失败，用例自行 skip —— 见 recorder_x11_e2e_test.cpp 顶部注释）
        target_sources(core_tests PRIVATE recorder_x11_e2e_test.cpp)
    endif()

    # smart_trigger 测试此前误 gate 在 WIN32（用例全部走 wingman 跨平台抽象，
    # 无 Windows API），导致 Linux 覆盖率长期缺失该文件——2026-09-22 解除
    target_sources(core_tests PRIVATE smart_trigger_test.cpp)
    # misc_modules（smarttrigger/bt/node/ocr 胶水）缺口补测
    target_sources(core_tests PRIVATE misc_modules_coverage_test.cpp)
    # crypto/clipboard/macro 胶水补测
    target_sources(core_tests PRIVATE small_modules_coverage_test.cpp)

    target_include_directories(core_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../libs/lua/include
    )

    target_link_libraries(core_tests PRIVATE
        GTest::gtest
        GTest::gtest_main
        wingman
    )

    if(TARGET wingman::python)
        target_link_libraries(core_tests PRIVATE wingman::python)
        target_compile_definitions(core_tests PRIVATE WINGMAN_ENABLE_PYTHON)
    endif()

    # Propagate vision/OCR/ML compile definitions from parent library
    if(WINGMAN_HAS_OPENCV)
        target_compile_definitions(core_tests PRIVATE WINGMAN_ENABLE_VISION)
        # performance.cpp 仅 OpenCV 版本（无 stub），其测试同理只在有 OpenCV 时编译
        target_sources(core_tests PRIVATE performance_test.cpp)
        message(STATUS "Core tests: WINGMAN_ENABLE_VISION defined")
    endif()

    # Apply coverage options (must be after target creation)
    if(CODE_COVERAGE)
        message(STATUS "Code coverage enabled")

        # GCC/Clang coverage flags
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(core_tests PRIVATE --coverage)
            target_link_options(core_tests PRIVATE --coverage)
        # MSVC coverage flags
        elseif(MSVC)
            target_compile_options(core_tests PRIVATE /profile)
            target_link_options(core_tests PRIVATE /PROFILE)
        endif()
    endif()

    # Register tests
    include(GoogleTest)
    gtest_discover_tests(core_tests DISCOVERY_MODE PRE_TEST)

    message(STATUS "Core tests enabled")
endif()
