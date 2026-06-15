#pragma once

#include "wingman/rpc/rpc_dispatcher.hpp"

namespace wingman::platform {
class IScreen;
}

namespace wingman::rpc {

// Register screenshot handlers using the legacy static Screen capture path.
// Backwards compatible: default monitor (primary) only.
void registerScreenshotHandlers(RpcDispatcher& dispatcher);

// Register screenshot handlers with an injected IScreen for multi-monitor
// support. When `screen` is non-null, the dispatcher additionally exposes
// `screen.listMonitors` and accepts an optional `displayId` field in
// `screenshot.capture` payloads to target a non-primary monitor.
void registerScreenshotHandlers(RpcDispatcher& dispatcher,
                                wingman::platform::IScreen& screen);

} // namespace wingman::rpc
