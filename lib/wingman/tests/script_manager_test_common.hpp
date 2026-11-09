#pragma once

#include <gtest/gtest.h>
#include "wingman/script_manager.hpp"
#include "wingman/lua/script_manager.hpp"
#ifdef WINGMAN_ENABLE_PYTHON
#include "wingman/python/python_script_engine.hpp"
#endif
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <thread>
#include <type_traits>

using namespace wingman;

class ScriptManagerFileTest : public ::testing::Test {
protected:
    std::string tempDir;

    void SetUp() override {
        tempDir = std::filesystem::temp_directory_path().string() + "/wingman_sm_test_" +
            std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id()));
        std::filesystem::create_directories(tempDir);
    }

    void TearDown() override {
        std::filesystem::remove_all(tempDir);
    }

    std::string createTempScript(const std::string& name, const std::string& content) {
        std::string path = tempDir + "/" + name;
        std::ofstream(path) << content;
        return path;
    }
};

