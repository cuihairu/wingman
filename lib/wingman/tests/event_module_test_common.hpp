#pragma once

#include <gtest/gtest.h>

#include "wingman/event.hpp"
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
using wingman::EventHub;
using wingman::EventMessage;
