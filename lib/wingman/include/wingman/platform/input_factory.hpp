#pragma once

#include "wingman/platform/iinput.hpp"
#include <memory>

namespace wingman::platform {

std::unique_ptr<IInput> createDefaultInput(const InputConfig& config = {});
std::shared_ptr<IInput> defaultSharedInput();
std::shared_ptr<IInput> createSharedInput(const InputConfig& config = {});

} // namespace wingman::platform
