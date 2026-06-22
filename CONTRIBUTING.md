# 贡献指南

感谢参与 Wingman 开发。请先阅读 [CLAUDE.md](./CLAUDE.md)（架构硬约束）与 [BUILD.md](./BUILD.md)（构建步骤），再开始改动。

## 架构硬约束（务必遵守）

修改 runtime、GUI、orchestrator 或 transport 前，必须先读 [docs/architecture-decisions.md](./docs/architecture-decisions.md)。核心：

1. **Go server 是远程中控编排器**。
2. **Runtime 作为 agent 主动 outbound 连接 Go server**（不是反过来）。
3. **本地 Tauri UI 通过本地 IPC 控制 runtime**（Named Pipe/UDS）。
4. **Runtime 禁止引入 HTTP/WebSocket server** 作为本地 UI 或远程控制面。
5. **Dashboard / 远程客户端只连 Go server**，不直连 runtime。

引入违反上述约束的代码（如 runtime 内开 HTTP server、server 反向拨号 runtime）会被拒绝。

## 依赖管理

- 所有第三方 C++ 依赖**必须**由 `vcpkg` 管理（`vcpkg.json` + `x64-windows-static` triplet）。
- **禁止**回退到系统依赖（`/usr/local`、Homebrew、系统 SDK、自带包管理器安装的库）。
- **禁止**用 `FetchContent` 临时拉取或手工 include 目录兜底绕过 vcpkg。
- 依赖缺失时应补齐 `vcpkg.json` / 配置 `CMAKE_TOOLCHAIN_FILE` / 执行 `vcpkg install`，而不是切换来源。

## 构建与验证

```bash
# 配置（vcpkg 静态库）
cmake -B build -DCMAKE_TOOLCHAIN_FILE="<vcpkg>/scripts/buildsystems/vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows-static
# 编译
cmake --build build --config Release
```

提交前请按改动范围运行对应检查：

| 改动范围 | 检查命令 |
|----------|----------|
| Go server | `cd orchestrator/server && go vet ./... && go test ./...` |
| Dashboard | `cd orchestrator/dashboard && npx tsc --noEmit` |
| C++ runtime/lib | `cmake --build build --config Release` + `ctest --test-dir build --build-config Release` |
| GUI (Tauri) | `cd apps/gui && npx svelte-check --tsconfig ./tsconfig.json` + `cd src-tauri && cargo check` |

**测试基线**：C++ runtime 维持 ~1705 测试 / 90%+ 覆盖；Go server 75+ 测试函数，`go vet` 全清。新功能请补测试。

## 代码风格

- **C++**：C++17，命名遵循既有约定（成员 `m_` 前缀、`snake_case` 函数 / `PascalCase` 类名）。不引入新的第三方网络服务框架到 runtime。
- **Go**：`gofmt` + `go vet` 通过。
- **TypeScript/Svelte**：既有缩进为 Tab，沿用即可。
- **命令行入口**：每个具体命令实现拆到独立文件，不要把多条命令逻辑堆在同一个源文件里（见 CLAUDE.md）。
- **注释**：中文注释可读、集中在参数分发处。除非必要，不为显然的代码加注释。

## 提交规范

使用 [Conventional Commits](https://www.conventionalcommits.org/)，与现有历史一致：

```
<type>(<scope>): <简短描述>

<可选 body，说明动机/关键改动>
```

常用 type：`feat` / `fix` / `chore` / `docs` / `refactor` / `test` / `perf` / `ci`。
scope 举例：`gui` / `runtime` / `server` / `dashboard` / `deps` / `ci`。

- 一个提交聚焦一件事；大型跨模块改动在 body 中分点说明。
- **不要在提交里夹带无关的格式化/换行改动**（仓库已用 `.gitattributes` 统一 LF，无需手动处理换行）。
- 提交前用 `git status` / `git diff` 复核，**不要提交 secrets / 密钥 / 构建产物**（`build/`、`node_modules/`、`vcpkg_installed/`、`target/` 已在 `.gitignore`）。

## 文档

改动 runtime 控制、本地 UI、远程编排相关代码时，**同次提交**更新 [docs/architecture-decisions.md](./docs/architecture-decisions.md) 与相关说明文档。若为实验性实现，请显式标注 `experimental`，不要冒充稳定架构。

## Pull Request

1. 从 `main` 切分支开发。
2. PR 描述写清楚：解决了什么、如何验证（贴测试/构建结果）、是否触及架构约束。
3. CI 全绿后再请求 review。
4. 涉及架构变更的 PR 需先在 issue / discussion 达成共识。
