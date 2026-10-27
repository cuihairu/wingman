> [中文](../zh/guides/asset-sync.md) | **English**

# Asset Delivery Guide (Template Images and Other Static Resources)

This guide is for script developers. It explains how to distribute static resources such as template images from the server to the local storage of multiple agents, and how to reference them from scripts.

## When to Use Asset Delivery

- The script needs **template images** (find-image / find-color / OCR reference images) and you want the resources maintained centrally on the server, with consistent versions across agents.
- The target device has **no shared filesystem** (e.g. the Android side), so scripts cannot read server files directly — the server must deliver them inline.
- The resources need **integrity verification** (strict sha256 check, verified on delivery).

## Server Storage and Endpoints

Assets are stored under `WINGMAN_ASSETS_DIR` (default `./data/assets`): each asset is a `<dir>/<name>` file plus a `<dir>/.meta/<name>.json` sidecar (sha256 / size / upload time). No DB dependency; atomic writes.

| Endpoint | Method | Permission | Description |
|---|---|---|---|
| `/api/assets` | POST | `assets:manage` | Upload (inline base64, ≤1MB) |
| `/api/assets` | GET | Read-only | List (includes sha256) |
| `/api/assets/content` | GET | Read-only | Raw byte download (`?name=`) |
| `/api/assets` | DELETE | `assets:manage` | Delete |
| `/api/assets/sync` | POST | `assets:manage` | Deliver to agents by selector |

Constraints (same rules enforced on both sides):

- The name whitelist is `[A-Za-z0-9][A-Za-z0-9._-]{0,127}`; `..` is rejected up front (path traversal blocked).
- A single asset is capped at 1MB (base64 inline, same convention as inline scripts).
- If a sha256 is provided, a strict check is enforced; a mismatch is rejected before anything is written to disk.

## Delivery Semantics

Request body of `POST /api/assets/sync`:

```json
{ "name": "login_button.png", "agentIds": ["agent-001"], "tags": ["prod"] }
```

- The selector `{agentIds, tags}` takes a **union** (same convention as batch run/stop and triggers), with a semaphore of 8 concurrent deliveries.
- The agent receives the `asset.sync` command → base64 decode → strict sha256 check → atomic write to `<scriptDir>/assets/<name>` → responds with `{path, size}`.
- Returns a `BatchSummary` (per-agent success/failure reasons); offline agents are recorded as `agent offline`.
- **Partial failure does not count as overall failure**: HTTP 200 + per-agent `results`.

## How to Reference Assets from Scripts

The delivery response returns the **absolute path** of the file on disk (`result.path`); bake it into the script config or use it directly. Lua example:

```lua
-- 资产已由服务端下发到本机 <scriptDir>/assets/login_button.png
wingman.vision.templateAdd("loginButton", "/path/to/scriptDir/assets/login_button.png")
local hit = wingman.vision.findImage("/path/to/scriptDir/assets/login_button.png", 0.9)
if hit.found then
  wingman.input.click(hit.position.x, hit.position.y)
end
```

> ⚠️ The current runtime does not expose a script-side API for the "script directory" (`wingman.config` is user KV storage, not runtime config). When referencing asset paths in scripts, use the absolute path returned by the delivery response, or maintain a path constant in the script config.

## Relationship to Script Version Management

- Assets are a resource plane where "content is the version": the sha256 is recorded in the sidecar, and download/delivery stay consistent.
- The script-side fingerprint is the `contentSha256` of the `run_script` payload + `Execution.scriptSha256` (see the ② Request table in `docs/protocols.md`).
- The two do not interfere: assets go through the `asset.sync` channel, scripts go through the `run_script` channel.

## Limitations

- **The Android side is integrated (2026-10-15)**: the Android agent's standalone RpcDispatcher registers the same `registerAssetHandlers` (`apps/android/cpp/agent/android_agent.cpp:135`); `asset.sync` lands in `<filesDir>/assets/`; scripts reference assets relatively as `assets/<name>`, mirroring the desktop `scriptDir/assets` layout (findImage resolves relative paths against filesDir). On-device behavior has been verified via desktop-shared tests (`android_asset_test`) and host compilation at the NDK layer; a real-device walkthrough is pending until hardware is available (see the A4 milestone in `docs/android-agent.md`).
- Assets are stored in memory plus the filesystem, with **no DB version history**; deletion cannot be rolled back.
- 1MB cap per asset; for larger resources, use inline scripts or external storage.
