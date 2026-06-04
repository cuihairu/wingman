#include <gtest/gtest.h>
#include "wingman/trigger_engine.hpp"
#include <fstream>
#include <filesystem>

using namespace wingman;

namespace {

std::string writeTempLua(const std::string& content, const std::string& name) {
    auto dir = std::filesystem::temp_directory_path() / "wingman_trigger_test";
    std::filesystem::create_directories(dir);
    auto path = dir / name;
    std::ofstream f(path);
    f << content;
    f.close();
    return path.string();
}

} // namespace

class TriggerEngineLuaTest : public ::testing::Test {
protected:
    void TearDown() override {
        engine.stop();
        // Clean up temp files
        auto dir = std::filesystem::temp_directory_path() / "wingman_trigger_test";
        std::filesystem::remove_all(dir);
    }

    TriggerEngine engine;
};

TEST_F(TriggerEngineLuaTest, LoadColorTrigger) {
    std::string lua = R"(
triggers = {
    {
        name = "color_trigger",
        enabled = true,
        condition = {
            type = "pixel",
            value = "0xFF0000",
            region = {x=10, y=20, width=100, height=100},
            tolerance = 10,
            interval = 500,
            enabled = true
        },
        actions = {
            { type = "click", x = 50, y = 50, delay = 100 }
        },
        cooldown = 1000
    }
}
)";
    auto path = writeTempLua(lua, "color_trigger.lua");
    EXPECT_TRUE(engine.loadFromLua(path));
}

TEST_F(TriggerEngineLuaTest, LoadImageTrigger) {
    std::string lua = R"(
triggers = {
    {
        name = "image_trigger",
        enabled = true,
        oneShot = true,
        condition = {
            type = "image",
            value = "button.png",
            region = {10, 20, 100, 100},
            tolerance = 5,
            interval = 200,
            enabled = true
        },
        actions = {
            { type = "key", value = "Enter", delay = 50 }
        },
        cooldown = 500
    }
}
)";
    auto path = writeTempLua(lua, "image_trigger.lua");
    EXPECT_TRUE(engine.loadFromLua(path));
}

TEST_F(TriggerEngineLuaTest, LoadMultipleTriggers) {
    std::string lua = R"(
triggers = {
    {
        name = "trigger1",
        enabled = true,
        condition = {
            type = "ColorFound",
            color = "0x00FF00",
            region = {x=0, y=0, width=800, height=600},
            tolerance = 15,
            interval = 100,
            enabled = true
        },
        actions = {
            { type = "KeyPress", value = "Space" }
        },
        cooldown = 500
    },
    {
        name = "trigger2",
        enabled = true,
        condition = {
            type = "ImageFound",
            value = "icon.png",
            enabled = true
        },
        actions = {
            { type = "Click", x = 100, y = 200 }
        },
        cooldown = 1000
    }
}
)";
    auto path = writeTempLua(lua, "multi_trigger.lua");
    EXPECT_TRUE(engine.loadFromLua(path));
}

TEST_F(TriggerEngineLuaTest, LoadWithAllActionTypes) {
    std::string lua = R"(
triggers = {
    {
        name = "all_actions",
        enabled = true,
        condition = {
            type = "TimeElapsed",
            interval = 1000,
            enabled = true
        },
        actions = {
            { type = "key", value = "A" },
            { type = "click", x = 10, y = 20 },
            { type = "macro", value = "script.lua" },
            { type = "Type", value = "hello" },
            { type = "Log", value = "log message" },
            { type = "Delay", value = "100" }
        },
        cooldown = 2000
    }
}
)";
    auto path = writeTempLua(lua, "all_actions.lua");
    EXPECT_TRUE(engine.loadFromLua(path));
}

TEST_F(TriggerEngineLuaTest, LoadWithWindowTriggerTypes) {
    std::string lua = R"(
triggers = {
    {
        name = "window_open",
        enabled = true,
        condition = {
            type = "WindowOpened",
            value = "Notepad",
            enabled = true
        },
        actions = {
            { type = "ShowMessage", value = "Window opened!" }
        }
    },
    {
        name = "window_close",
        enabled = true,
        condition = {
            type = "WindowClosed",
            value = "Notepad",
            enabled = true
        },
        actions = {
            { type = "ShowMessage", value = "Window closed!" }
        }
    },
    {
        name = "process_start",
        enabled = true,
        condition = {
            type = "ProcessStarted",
            value = "notepad.exe",
            enabled = true
        },
        actions = {
            { type = "PlayAudio", value = "alert.wav" }
        }
    },
    {
        name = "process_stop",
        enabled = true,
        condition = {
            type = "ProcessStopped",
            value = "notepad.exe",
            enabled = true
        },
        actions = {
            { type = "Log", value = "Process stopped" }
        }
    }
}
)";
    auto path = writeTempLua(lua, "window_triggers.lua");
    EXPECT_TRUE(engine.loadFromLua(path));
}

TEST_F(TriggerEngineLuaTest, LoadWithColorLostAndPixelChanged) {
    std::string lua = R"(
triggers = {
    {
        name = "color_lost",
        enabled = true,
        condition = {
            type = "ColorLost",
            value = "0xFF0000",
            region = {x=0, y=0, width=100, height=100},
            enabled = true
        },
        actions = {
            { type = "Log", value = "Color lost" }
        }
    },
    {
        name = "pixel_changed",
        enabled = true,
        condition = {
            type = "PixelChanged",
            region = {x=10, y=10, width=50, height=50},
            tolerance = 20,
            enabled = true
        },
        actions = {
            { type = "Log", value = "Pixel changed" }
        }
    },
    {
        name = "hotkey",
        enabled = true,
        condition = {
            type = "HotkeyPressed",
            value = "F5",
            enabled = true
        },
        actions = {
            { type = "StopScript" }
        }
    }
}
)";
    auto path = writeTempLua(lua, "color_lost.lua");
    EXPECT_TRUE(engine.loadFromLua(path));
}

TEST_F(TriggerEngineLuaTest, LoadWithImageLost) {
    std::string lua = R"(
triggers = {
    {
        name = "image_lost",
        enabled = true,
        condition = {
            type = "ImageLost",
            value = "target.png",
            region = {x=0, y=0, width=640, height=480},
            enabled = true
        },
        actions = {
            { type = "PauseScript" }
        }
    }
}
)";
    auto path = writeTempLua(lua, "image_lost.lua");
    EXPECT_TRUE(engine.loadFromLua(path));
}

TEST_F(TriggerEngineLuaTest, LoadDisabledTrigger) {
    std::string lua = R"(
triggers = {
    {
        name = "disabled_trigger",
        enabled = false,
        condition = {
            type = "ColorFound",
            value = "0xFF0000",
            enabled = false
        },
        actions = {
            { type = "Log", value = "should not fire" }
        }
    }
}
)";
    auto path = writeTempLua(lua, "disabled.lua");
    EXPECT_TRUE(engine.loadFromLua(path));
}

TEST_F(TriggerEngineLuaTest, LoadTriggerWithActionTable) {
    std::string lua = R"(
triggers = {
    {
        name = "single_action",
        enabled = true,
        condition = {
            type = "ColorFound",
            value = "0xFF0000",
            enabled = true
        },
        action = {
            type = "click",
            x = 100,
            y = 200,
            delay = 50
        }
    }
}
)";
    auto path = writeTempLua(lua, "single_action.lua");
    EXPECT_TRUE(engine.loadFromLua(path));
}

TEST_F(TriggerEngineLuaTest, SkipTriggerWithUnknownConditionType) {
    std::string lua = R"(
triggers = {
    {
        name = "unknown_cond",
        enabled = true,
        condition = {
            type = "UnknownType",
            value = "test",
            enabled = true
        },
        actions = {
            { type = "Log", value = "test" }
        }
    }
}
)";
    auto path = writeTempLua(lua, "unknown_cond.lua");
    EXPECT_TRUE(engine.loadFromLua(path));
}

TEST_F(TriggerEngineLuaTest, SkipTriggerWithUnknownActionType) {
    std::string lua = R"(
triggers = {
    {
        name = "unknown_action",
        enabled = true,
        condition = {
            type = "ColorFound",
            value = "0xFF0000",
            enabled = true
        },
        actions = {
            { type = "UnknownAction", value = "test" }
        }
    }
}
)";
    auto path = writeTempLua(lua, "unknown_action.lua");
    EXPECT_TRUE(engine.loadFromLua(path));
}

TEST_F(TriggerEngineLuaTest, SkipTriggerWithNoActions) {
    std::string lua = R"(
triggers = {
    {
        name = "no_actions",
        enabled = true,
        condition = {
            type = "ColorFound",
            value = "0xFF0000",
            enabled = true
        }
    }
}
)";
    auto path = writeTempLua(lua, "no_actions.lua");
    EXPECT_TRUE(engine.loadFromLua(path));
}

TEST_F(TriggerEngineLuaTest, LoadWithKeyField) {
    std::string lua = R"(
triggers = {
    {
        name = "key_field",
        enabled = true,
        condition = {
            type = "TimeElapsed",
            interval = 500,
            enabled = true
        },
        actions = {
            { type = "key", key = "F1" }
        }
    }
}
)";
    auto path = writeTempLua(lua, "key_field.lua");
    EXPECT_TRUE(engine.loadFromLua(path));
}

TEST_F(TriggerEngineLuaTest, LoadInvalidLuaReturnsFalse) {
    auto path = writeTempLua("this is not valid lua!!", "invalid.lua");
    EXPECT_FALSE(engine.loadFromLua(path));
}

TEST_F(TriggerEngineLuaTest, LoadNoTriggersTableReturnsFalse) {
    auto path = writeTempLua("other_var = 42", "no_table.lua");
    EXPECT_FALSE(engine.loadFromLua(path));
}

TEST_F(TriggerEngineLuaTest, LoadNonTableTriggersReturnsFalse) {
    auto path = writeTempLua("triggers = \"not a table\"", "non_table.lua");
    EXPECT_FALSE(engine.loadFromLua(path));
}

TEST_F(TriggerEngineLuaTest, EnableDisableByNameAfterLoad) {
    std::string lua = R"(
triggers = {
    {
        name = "my_trigger",
        enabled = true,
        condition = {
            type = "ColorFound",
            value = "0xFF0000",
            enabled = true
        },
        actions = {
            { type = "Log", value = "test" }
        }
    }
}
)";
    auto path = writeTempLua(lua, "enable_disable.lua");
    ASSERT_TRUE(engine.loadFromLua(path));

    EXPECT_TRUE(engine.enableTrigger("my_trigger"));
    EXPECT_TRUE(engine.disableTrigger("my_trigger"));
    EXPECT_FALSE(engine.enableTrigger("nonexistent"));
    EXPECT_FALSE(engine.disableTrigger("nonexistent"));
}

TEST_F(TriggerEngineLuaTest, GetStatsAfterLoad) {
    std::string lua = R"(
triggers = {
    {
        name = "stat_trigger1",
        enabled = true,
        condition = {
            type = "ColorFound",
            value = "0xFF0000",
            enabled = true
        },
        actions = {
            { type = "Log", value = "test" }
        }
    },
    {
        name = "stat_trigger2",
        enabled = true,
        condition = {
            type = "ImageFound",
            value = "img.png",
            enabled = true
        },
        actions = {
            { type = "Log", value = "test" }
        }
    }
}
)";
    auto path = writeTempLua(lua, "stats.lua");
    ASSERT_TRUE(engine.loadFromLua(path));

    auto stats = engine.getStats();
    EXPECT_EQ(stats.totalTriggers, 2u);
    EXPECT_EQ(stats.enabledTriggers, 2u);
}
