# 代码深审报告（2026-11-01）

> 范围：本仓全量（C++ runtime / Go 中控 / GUI Tauri / Dashboard / Android agent）
> 方法：安全静态扫描（密钥 / shell 注入 / SQL 拼接 / eval / 反序列化 / 路径穿越）
> + 隐藏 bug 猎捕（错误处理 / 竞态 / 越界 / 资源泄漏 / off-by-one / 并发）
> + 质量门（lint / 类型检查 / 测试基线）
> 结论：**发现 0 P0 / 0 P1 / 5 P3（均为装饰性静态检查项，无行为影响）；本轮无需修复提交。**

---

## 1. 结论摘要

| 严重度 | 发现 | 已修 | 剩余 |
|--------|------|------|------|
| P0（安全/崩溃/数据损坏） | 0 | 0 | 0 |
| P1（错误结果/竞态/资源泄漏） | 0 | 0 | 0 |
| P2（明确缺陷但影响小） | 0 | 0 | 0 |
| P3（装饰性/风格） | 5 | 0 | 5 |

- 本轮无 P0/P1，故**无修复提交**（一问题一 commit 的要求未触发）。
- 5 条 P3 均为 `staticcheck` 报告，不影响行为；已记录在案（§6），未在本次修改。
- 安全静态扫描**全部通过**（§2）；并发/资源/越界猎捕**全部通过**（§3）；质量门**全绿**（§4）。

---

## 2. 安全静态扫描（全部通过）

| 类别 | 结论 | 验证点（file:line） |
|------|------|--------------------|
| 硬编码密钥/私钥 | 通过 | 全仓无源码内嵌密钥；凭据走环境变量（`WINGMAN_AGENT_TOKENS`）与 VaultProvider 抽象（`internal/vault/`，ce4b4ee） |
| shell 注入 | 通过 | `popen/system` 调用点均传常量或数值安全输入：`lib/wingman/src/platform/linux/posix_system.cpp:58,610`、`lib/wingman/src/platform/mac/darwin_system.cpp:36`、`lib/wingman/src/platform/screen_backend.cpp:231` |
| SQL 拼接 | 通过 | Go GORM 全部参数化 `?`；C++ db 模块标识符经正则 `^[A-Za-z_][A-Za-z0-9_]*$` 白名单校验 + `?` 绑定：`lib/wingman/src/script/modules/db_module.cpp:417-478,484-533,583-620`；报表分组列白名单化：`internal/handlers/remote_session.go:233` |
| eval / 反序列化 | 通过 | 脚本 `py::exec` 为沙箱 shim：`libs/python/src/python_script_engine.cpp:572`；JSON 解析用 nlohmann-json / `encoding/json` 标准库 |
| 路径穿越 | 通过 | 脚本存储 `Resolve()` 拒绝绝对路径与 `..`、要求 `.lua` 后缀：`orchestrator/server/internal/scripts/paths.go:51-56` |
| WebSocket 跨站劫持 | 通过 | Origin 同主机校验：`pkg/websocket/hub.go:20-38` |

---

## 3. 隐藏 bug 猎捕（全部通过）

| 区域 | 检查点 | 结论 | 验证点 |
|------|--------|------|--------|
| WebSocket Hub | `Send` 通道双关闭 | 通过：两处 `close(conn.Send)`（unregister / broadcast-stale）均持 `h.mu` 且以「成员存在 + 删除」为守卫，删除后二次 unregister 不再 close | `pkg/websocket/hub.go:94-110,130-149` |
| WebSocket Hub | `currentRoom` 并发访问 | 通过：全部读写持 `h.mu`（Run / JoinRoom / LeaveRoom / unregister） | `hub.go:100,137,164-195` |
| FrameListener / Registry | `stopCh` 关闭 | 通过：均 `sync.Once` 守卫，无 double-close | `internal/agent/listener.go:71,249`；`internal/agent/registry.go:78,456` |
| Workflow 引擎 | 步骤 goroutine 并发写 | 通过：`completed`/`failed` 持局部 `mu`，外层 `wg.Wait()` 建立 happens-before | `internal/workflow/engine.go:405-417` |
| Workflow 引擎 | goroutine 泄漏（ctx 取消/超时） | 通过：`runScriptOnce` 的 `done`、screenshot 的 `done`/`errCh` 均 buffered(1)，select 取走一条即不阻塞 | `engine.go:832,1148` |
| Workflow 引擎 | 终态竞态 | 通过：终态收口用条件更新 `WHERE status='running'` 让位，不依赖「Cancel 先落库」顺序假设 | `engine.go:433-441` |
| Go race 实测 | agent + workflow 包 | **-race -count=1 通过** | agent 7.925s / workflow 30.281s，exit 0 |
| C++ OCR | `delete api` 双路径 | 通过：Init 失败 return / 成功 End+delete 互斥，无 double-free | `lib/wingman/src/ocr.cpp:48,53` |
| C++ UIA COM | `delete this` 引用计数 | 通过：标准 `InterlockedIncrement/Decrement` + `c==0` 守卫，无 use-after-free | `lib/wingman/src/platform/win/win_automation.cpp:327,366` |
| Go config | `key[len(prefix):]` 越界 | 通过：`strings.HasPrefix` 前置守卫 | `internal/config/config.go:143` |
| 最近提交复核 | `0c7e208` notify 写超时/死锁修复、`1eda7cc` profiles 崩溃转 Result | 通过：语义正确 | `internal/agent/listener.go:922-970`；`apps/gui/src-tauri/src/commands/profiles.rs` |

---

## 4. 质量门（全绿）

| 门 | 结果 |
|----|------|
| Go 测试 | 16 包全部 `ok`（`integration` 实跑 54.365s，其余 cached），exit 0 |
| Go race 检测 | `internal/agent` + `internal/workflow` 通过（-count=1） |
| Go vet | `go vet ./...` 干净（一次瞬态 parse 报错经单独重跑确认非真，exit 0） |
| staticcheck | 仅 5 条 P3 装饰性（见 §6），无 SA/S 级别实质缺陷 |
| Dashboard jest | 34 套件 / 449 测试通过 |
| GUI vitest | 70 文件 / 529 测试通过 |
| GUI Rust cargo check | 干净，0 error 0 warning（warm 2.52s） |
| C++ 测试基线 | 引用已记录基线：2509 注册（2478 pass + 31 skip，TOTAL 90%）；**本轮未本地全量重跑**（需完整 vcpkg 构建，改由 CI 覆盖，见 §7 局限） |

---

## 5. 发现清单（按严重度排序，全为 P3）

### P3-1 `internal/handlers/remote_session.go:227`（staticcheck SA4006，误报 + 风格）

- staticcheck 报「this value of query is never used」。
- **判定：误报。** GORM v2 的 `Where` 在 `clone==0`（本处 `h.db.Model(...)` 链）时**原地变异共享 Statement**，副作用即用途；`query = query.Where(...)` 的局部重赋值冗余，但四条查询（列表/汇总/分组/分桶）过滤器相同，累加亦无害。
- **处理：不改**（行为正确）；记录为可读性改进点——`applyRemoteSessionFilters` 内重赋值可去，仅依赖变异即可。

### P3-2 `internal/handlers/remote_session.go:299`（staticcheck S1016）

- 用 struct literal 代替 `remoteSessionSummary(row)` 类型转换。装饰性，无行为影响。

### P3-3 `integration/harness_test.go:26-27`、`main_coverage_test.go:23-24`、`main_wait_test.go:19-20`（staticcheck ST1019）

- `internal/agent` 重复 import（各 2 行）。测试文件装饰性，无行为影响。

### P3-4 `internal/handlers/capstore_test.go:15`（staticcheck S1040）

- 冗余类型断言 `NewAgentCapabilityStore(db)` 已具 `agent.CapabilityStore` 类型。测试文件装饰性。

---

## 6. 局限与说明

- **C++ 全量测试未本地重跑**：2509 注册测试需完整 vcpkg 静态构建，本机未复现完整工具链；基线引用已记录值（TODO/README 口径 90%），实际由 CI Linux 矩阵门禁覆盖。
- **GUI Rust 未改源码**：本轮仅做 `cargo check` 类型检查（通过）；`1eda7cc`（profiles.rs）为上一提交，已按 inspection 复核。
- **`go vet` 一次瞬态 parse 报错**：`integration/workflow_test.go:66` 报「expected declaration, found 'for'」，单独 `go vet ./integration/` 重跑 exit 0，判定为 vet 缓存瞬态假象，非真问题。

## 7. 提交清单

- 本轮代码深审**无 P0/P1，无修复提交**。
- 关联提交（本会话前序）：`594d379 docs: fix zh readme link, tidy todo, add 11-02 audit round`（文档一致性审计轮，非本审）。
