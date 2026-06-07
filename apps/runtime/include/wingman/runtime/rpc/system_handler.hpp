#pragma once

#include "wingman/rpc/rpc_dispatcher.hpp"
#include <string>

namespace wingman::runtime {
class StandaloneMode;
}

namespace wingman::rpc {

void registerRuntimeSystemHandlers(RpcDispatcher& dispatcher,
                                   const std::string& version,
                                   runtime::StandaloneMode& standalone);

} // namespace wingman::rpc
