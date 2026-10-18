---
name: 功能需求 / Feature request
about: 新能力、新平台、新 API 或行为变更提案
labels: [enhancement]
---

## 需求背景 / Use case

<解决什么问题、谁在用、现在的绕行办法是什么>

## 期望能力 / Expected capability

<希望的行为，尽量具体到 API 形态>

## 架构影响 / Architecture impact

架构硬约束（详见 `docs/architecture-decisions.md`）：

- [ ] 我已确认该需求不要求 runtime 引入 HTTP/WebSocket server 作为本地 UI 或远程控制面
- [ ] 该需求符合"runtime 主动 outbound 连 Go server、Dashboard 只连 Go server"的编排方向
- [ ] 该需求不破坏"本地 Tauri UI 只通过本地 IPC 控制 runtime"

## 替代方案 / Alternatives considered

<现有 API 能否组合实现；为何不够>

## 平台范围 / Platform scope

<Windows / Linux / macOS / Android，以及可接受的降级行为>
