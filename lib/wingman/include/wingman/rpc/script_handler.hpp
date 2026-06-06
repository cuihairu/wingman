#pragma once

#include "wingman/rpc/rpc_dispatcher.hpp"

namespace wingman::runtime {
class StandaloneMode;
}

namespace wingman::rpc {

void registerScriptHandlers(RpcDispatcher& dispatcher, runtime::StandaloneMode& standalone);

} // namespace wingman::rpc
