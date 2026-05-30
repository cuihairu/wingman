#include <gtest/gtest.h>

#include "wingman/script/iscript_engine.hpp"

namespace wingman {
namespace script {
namespace modules {

ModuleDescriptor createEventModule();

} // namespace modules
} // namespace script
} // namespace wingman

using namespace wingman::script;
using namespace wingman::script::modules;

TEST(EventModuleTest, HasExpectedFunctions) {
    auto mod = createEventModule();

    EXPECT_EQ(mod.name, "event");
    ASSERT_GE(mod.functions.size(), 3u);

    bool hasEmit = false;
    bool hasOff = false;
    bool hasClear = false;
    for (const auto& fn : mod.functions) {
        if (fn.name == "emit") hasEmit = true;
        if (fn.name == "off") hasOff = true;
        if (fn.name == "clear") hasClear = true;
    }

    EXPECT_TRUE(hasEmit);
    EXPECT_TRUE(hasOff);
    EXPECT_TRUE(hasClear);
}

