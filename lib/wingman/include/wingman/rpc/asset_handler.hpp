#pragma once

#include "wingman/rpc/rpc_dispatcher.hpp"

#include <cstddef>
#include <string>

namespace wingman::rpc {

// 单资产上限：与 server 侧 assets.MaxAssetSize / scripts.MaxInlineScriptSize
// 同口径（帧协议 16MB 上限留足余量）
inline constexpr size_t kMaxAssetSize = 1 << 20;

// 资产同步 handler（asset.sync）：Go server 经 agent 通道下发的模板图片等
// 脚本配套文件落盘 assetsDir（原子写：临时文件 + rename），响应 data 携带
// 落盘绝对路径与字节数，供脚本/触发器引用。payload 契约见 docs/protocols.md ②
// Request 表：{name, contentBase64, sha256?}——name 白名单与 server 侧
// assets.SanitizeName 一致，sha256 提供时强校验（十六进制小写，不匹配拒绝）。
void registerAssetHandlers(RpcDispatcher& dispatcher, const std::string& assetsDir);

} // namespace wingman::rpc
