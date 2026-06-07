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
        performance_test.cpp
        image_analyzer_test.cpp
        db_module_test.cpp
        ini_module_test.cpp
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

    # Propagate vision/OCR/ML compile definitions from parent library
    if(WINGMAN_HAS_OPENCV)
        target_compile_definitions(core_tests PRIVATE WINGMAN_ENABLE_VISION)
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
    gtest_discover_tests(core_tests)

    message(STATUS "Core tests enabled")
endif()
