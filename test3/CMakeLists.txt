# Minimal C++ Unit Tests for Wingman
# Tests that don't require external dependencies

cmake_minimum_required(VERSION 3.20)

project(WingmanMinimalTests)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add WIN32_LEAN_AND_MEAN
add_compile_definitions(WIN32_LEAN_AND_MEAN)

# Find GTest - try to find it in common locations
find_package(GTest QUIET)

if(NOT GTest_FOUND)
    message(STATUS "GTest not found, trying FetchContent")
    include(FetchContent)

    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.12.1
    )

    # For Windows: Prevent overriding parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

# Create a minimal test executable
add_executable(minimal_tests
    # Test files for header-only modules
    ../cpp/version_test.cpp
    ../cpp/json_test.cpp
    ../cpp/accounts_test.cpp
    ../cpp/node_status_test.cpp
    ../cpp/behavior_tree_test.cpp
    ../cpp/human_test.cpp
    ../cpp/smart_trigger_test.cpp
    ../cpp/qrcode_test.cpp
    ../cpp/tray_test.cpp
    ../cpp/verification_test.cpp
    ../cpp/recorder_test.cpp
    ../cpp/ml_test.cpp
    ../cpp/ocr_test.cpp
    ../cpp/ui_automation_test.cpp
    ../cpp/game_profile_test.cpp
    ../cpp/performance_test.cpp
    ../cpp/config_test.cpp
    ../cpp/kvstore_test.cpp
    ../cpp/auth_test.cpp
)

# Include directory (must come after add_executable)
target_include_directories(minimal_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

# Link libraries (only Windows and GTest)
target_link_libraries(minimal_tests PRIVATE
    GTest::gtest
    GTest::gtest_main
    gdi32
    user32
    ole32
    oleaut32
    uiautomationcore
    shell32
    advapi32
    ws2_32
)

# Enable testing
enable_testing()
include(GoogleTest)
gtest_discover_tests(minimal_tests)

message(STATUS "Minimal tests configured successfully")
message(STATUS "To run tests: cmake --build . --target minimal_tests && ./minimal_tests")
