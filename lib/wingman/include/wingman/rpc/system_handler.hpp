#pragma once

#include "wingman/rpc/rpc_dispatcher.hpp"
#include <string>

namespace wingman::rpc {

void registerSystemHandlers(RpcDispatcher& dispatcher, const std::string& version);

} // namespace wingman::rpc
