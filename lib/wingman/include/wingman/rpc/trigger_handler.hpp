#pragma once

#include "wingman/rpc/rpc_dispatcher.hpp"

namespace wingman {
class TriggerManager;
}

namespace wingman::rpc {

void registerTriggerHandlers(RpcDispatcher& dispatcher, TriggerManager& manager);

} // namespace wingman::rpc
