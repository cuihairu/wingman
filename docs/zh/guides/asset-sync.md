> [English](../../guides/asset-sync.md) | **中文**

# 资产下发指南（模板图片等静态资源）

本指南面向脚本开发者，说明如何把模板图片等静态资源从服务端分发到多台 agent 本地，以及脚本里如何引用。

## 什么时候用资产下发

- 脚本需要**模板图片**（找图 / 找色 / OCR 参考图），且希望资源由服务端统一维护、跨 agent 版本一致。
- 目标设备**没有共享文件系统**（如 Android 端），脚本无法直接读服务端文件，只能由服务端内联分发。
- 资源需要**完整性校验**（sha256 强校验，下发即验）。

## 服务端存储与端点

资产存储在 `WINGMAN_ASSETS_DIR`（默认 `./data/assets`）下：每个资产是 `<dir>/<name>` 文件 + `<dir>/.meta/<name>.json` sidecar（sha256 / 大小 / 上传时间），无 DB 依赖，原子写。

| 端点 | 方法 | 权限 | 说明 |
|---|---|---|---|
| `/api/assets` | POST | `assets:manage` | 上传（内联 base64，≤1MB） |
| `/api/assets` | GET | 只读 | 列表（含 sha256） |
| `/api/assets/content` | GET | 只读 | 原始字节下载（`?name=`） |
| `/api/assets` | DELETE | `assets:manage` | 删除 |
| `/api/assets/sync` | POST | `assets:manage` | 按选择器下发到 agent |

约束（两端同口径）：

- name 白名单 `[A-Za-z0-9][A-Za-z0-9._-]{0,127}`，禁止 `..`（路径穿越前置拒绝）。
- 单资产 ≤1MB（base64 内联，与内联脚本同口径）。
- 提供 sha256 即强校验，不匹配直接拒绝落盘。

## 下发语义

`POST /api/assets/sync` 请求体：

```json
{ "name": "login_button.png", "agentIds": ["agent-001"], "tags": ["prod"] }
```

- 选择器 `{agentIds, tags}` 取**并集**（与批量运行/停止/触发器同口径），信号量 8 并发。
- agent 收到 `asset.sync` 命令 → base64 解码 → sha256 强校验 → 原子写 `<scriptDir>/assets/<name>` → 响应 `{path, size}`。
- 返回 `BatchSummary`（逐台成功/失败原因）；离线 agent 记 `agent offline`。
- **部分失败不算整体失败**：HTTP 200 + 逐台 `results`。

## 脚本里怎么引用

下发响应返回落盘**绝对路径**（`result.path`），把它固化进脚本配置或直接用。Lua 示例：

```lua
-- 资产已由服务端下发到本机 <scriptDir>/assets/login_button.png
wingman.vision.templateAdd("loginButton", "/path/to/scriptDir/assets/login_button.png")
local hit = wingman.vision.findImage("/path/to/scriptDir/assets/login_button.png", 0.9)
if hit.found then
  wingman.input.click(hit.position.x, hit.position.y)
end
```

> ⚠️ 当前 runtime 没有暴露「脚本目录」的脚本侧 API（`wingman.config` 是用户 KV 存储，不是运行时配置）。脚本里引用资产路径时，用下发响应返回的绝对路径，或在脚本配置里维护一份路径常量。

## 与脚本版本管理的关系

- 资产是「内容即版本」的**资源面**：sha256 由 sidecar 记录，下载/下发一致。
- 脚本侧指纹是 `run_script` payload 的 `contentSha256` + `Execution.scriptSha256`（见 `docs/protocols.md` ② Request 表）。
- 两者互不干扰：资产走 `asset.sync` 通道，脚本走 `run_script` 通道。

## 限制

- **Android 端侧已接入（2026-10-15）**：Android agent 的独立 RpcDispatcher 注册同一 `registerAssetHandlers`（`apps/android/cpp/agent/android_agent.cpp:135`），`asset.sync` 落 `<filesDir>/assets/`；脚本以 `assets/<name>` 相对引用、与桌面 `scriptDir/assets` 同形（findImage 相对路径按 filesDir 解析）。端侧行为经桌面同源测试（`android_asset_test`）与 NDK 层宿主编译核验，真机走查待真机（见 `docs/android-agent.md` A4 里程碑）。
- 资产为内存 + 文件系统存储，**无 DB 版本历史**；删除即不可回滚。
- 单资产 1MB 上限；超大资源请走脚本内联或外部存储。
