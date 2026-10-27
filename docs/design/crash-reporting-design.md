> [中文](../zh/design/crash-reporting-design.md) | **English**

# Crash Collection Design: Direct Crashpad Integration (Abridged)

> Design date: 2026-10-08. Scope: the desktop C++ runtime (`apps/agent`).
> The tooling choice is settled: Google **Crashpad**, integrated directly in C++; Breakpad appears only as a historical comparison (§2).
> Implementation is split into batches: docs → build integration → library and runtime wiring → acceptance script.

---

## 1. Tooling Choice and Dependency Policy

| Item | Decision | Rationale |
|----|------|------|
| Collector | Crashpad (upstream gn build, not integrated) | Independent handler process model; continuity of the minidump ecosystem |
| Build integration | [TheAssemblyArmada/crashpad-cmake](https://github.com/TheAssemblyArmada/crashpad-cmake) (Apache-2.0) brought in as **pinned git submodules**; only its `cmake/` module files are taken and driven directly (our `cmake/crashpad.cmake` sets `*_git_SOURCE_DIR` and then includes its root files in order), **not going through its FetchContent root**; crashpad/mini_chromium/lss are pinned to exact commits as submodules of this repository — zero network at configure time | Upstream crashpad only supports gn/depot_tools builds; this packaging fork is the only open-source CMake wrapper covering all three platforms Win/mac/Linux (the Windows-only alternative unidentifieddeveloper/crashpad is MSVC-single-stream, not adopted). Reasons for bypassing the FetchContent root: its GIT_REPOSITORY hits the network at configure time, and single-argument `FetchContent_Populate` is deprecated as of CMake 4.x |
| Pinning | crashpad `backtrace-labs/crashpad@7b9686b` (2021-07), mini_chromium `chromium/mini_chromium@9cdc2a7` (2021-06), lss `e1e7b0a` (taken from the [cpp-pm/linux-syscall-support](https://github.com/cpp-pm/linux-syscall-support) mirror, same commit as upstream chromium.googlesource.com/linux-syscall-support) | Matches the combination empirically validated by the packaging fork's CI (its original FetchContent GIT_TAG values); upgrades are done by moving the submodule pointers |
| zlib | **vcpkg manifest** (a crashpad util/net dependency) | CLAUDE.md dependency rule: whatever can go through vcpkg goes through vcpkg |
| Dependency-rule exception declaration | crashpad itself has no vcpkg port (the official registry has neither crashpad nor breakpad), so it cannot be managed uniformly via vcpkg; pinning via submodules was approved by user directive | Clause 3 of CLAUDE.md forbids FetchContent ad-hoc fetching with the intent of banning "ad-hoc sources that bypass vcpkg"; a commit-pinned submodule is a deterministic source and touches no network at build time |
| New-toolchain exemptions | Appended on the wrapper INTERFACE: `-Wno-error` (the pinned code is frozen at 2021; new warnings from newer compilers are not used as a gate) and `-include cstdint` (GCC 13+ no longer transitively includes `<cstdint>`; proven necessary on GCC 15) | Submodule sources stay as-is and unpatched; the exemptions are concentrated in this repository's glue |

**Architecture red-line check**: crashpad_handler is a collection-side child process; it opens no listening port and is not part of the control plane. The runtime still has no HTTP/WebSocket server, consistent with the Forbidden Changes in architecture-decisions.md.

**Platform rollout**: `WINGMAN_ENABLE_CRASHPAD` is on by default only on Linux (validated on a real machine in this batch); the Windows/macOS build code is in place but off by default, to be enabled in a dedicated validation round — avoiding blindly turning it on and tripping the Windows CI gate.

## 2. Breakpad as a Historical Comparison (Why It Is Not Adopted)

- Breakpad is Crashpad's predecessor, both from Google, sharing the same minidump format lineage. Crashpad's official documentation positions itself as "the next generation of breakpad": in-process collection is replaced by an **independent crash-reporting process** (a crash of the collector itself does not affect the collected process), writes are safe (the handler is isolated from the collected process), and real Windows exceptions are supported (breakpad on modern Windows relies on patchwork paths such as handle snapshots).
- This design brings in no breakpad code; the only two exceptions are the **symbol toolchain** — `dump_syms` (DWARF→.sym) and `minidump_stackwalk` (stack reconstruction) remain breakpad-ecosystem tools with no official crashpad counterpart. They are used as local developer-machine tools (§6), not committed to the repository and not part of CI.

## 3. Initialization Point

- Location: inside `main()` in `apps/agent/src/main.cpp`, after the event log sink is attached and before command dispatch — covering all command paths such as `start`/`script`/`crash-test` and the embedded script path.
- Call: `setupCrashReporting(argv[0])` (in `apps/agent/src/crash_setup.cpp`), which assembles a `wingman::crash::CrashpadConfig` and then calls `wingman::crash::initialize()`.
- Semantics: `CrashpadClient::StartHandler` (restartable=true, asynchronous_start=false); **an initialization failure only logs an spdlog warning and degrades to no collection**, with the process running as usual — crash collection must not become a single point of failure for availability.
- Compile gate: `#ifdef WINGMAN_HAS_CRASHPAD` (following the optional-library gate pattern of `WINGMAN_HAS_LUA`).

## 4. Dump On-Disk Directory

| Platform | Default Directory |
|------|----------|
| Windows | `%LOCALAPPDATA%\wingman\crashes` |
| Linux | `$XDG_DATA_HOME/wingman/crashes`, or `$HOME/.local/share/wingman/crashes` when unset |
| macOS | `~/Library/Application Support/wingman/crashes` |

- The directory is built by appending `crashes` to `wingman::platform::appDataDir()` (newly added in this batch, implemented inside the platform layer `lib/wingman/src/platform/` — platform macros are allowed only in that layer, per the boundary guard rule); `libs/crash` and `apps/` remain free of platform macros.
- Layout (empirically confirmed on 2026-10-09, the fork's database layout): `settings.dat` (database settings, including the uploads-disabled bit) + `new/` (being written) → `pending/` (fully written; the final state when there are no uploads) → `completed/` (after upload) + `attachments/`; metrics live in the same directory. A complete dump is judged by a non-empty `.dmp` file; empty shells under `new/` do not count. The directory is created before initialization if it does not exist.
- Retention policy: no automatic cleanup in this batch; by design `pendingReports()` read-only enumerates `pending/`, and the cleanup policy will be decided later based on capacity.

## 5. Packaging the Independent Handler Process

- The `crashpad_handler` executable ships with the runtime artifacts, in the same directory as `wingman-agent`; `libs/crash` resolves the handler path by default as "the directory of this executable" (derived from `argv[0]`; when invoked directly from PATH this degrades to a PATH lookup, see §3).
- Build time: the `wingman-agent` POST_BUILD step copies `$<TARGET_FILE:crashpad_handler>` into the runtime output directory; the CI packaging scripts collect that directory, so the handler comes along naturally.
- Licenses carried along: crashpad / mini_chromium / lss are BSD-style licenses and the cmake wrapper is Apache-2.0; all registered in `docs/dependencies.md`.

## 6. Symbol Management

- **Build side**: debug information is kept in the build artifacts (Debug/RelWithDebInfo naturally carry `-g`; Windows Release omits it by default for size — when publishing a symbol package, a dedicated symbol build with `-Zi` is produced). Release artifacts can be split with `objcopy --only-keep-debug` and then stripped; the split-out ELF/debug pairs are archived alongside the release binaries keyed by `version+platform+commit`.
- **Reconstruction side** (local developer-machine tools, not committed to the repository):
  1. `dump_syms <runtime-binary>` produces `*.sym` (a breakpad tool; the build commands are in the header comment of the acceptance script `scripts/verify-crashpad.sh`);
  2. `minidump_stackwalk <dump.dmp> <symbols-dir>` reconstructs a symbolized call stack;
  3. the minidump format shares its lineage with breakpad, so the tools work directly.
- Symbol directory convention: `<symbols-root>/<version>-<platform>/`; `minidump_stackwalk` accepts multiple symbol roots and searches them recursively.

## 7. Upload Placeholder (Off by Default)

- `CrashpadConfig.uploadUrl` defaults to the empty string: `StartHandler` is not given a URL, and `settings->SetUploadsEnabled(false)` is set explicitly (double insurance at the database level).
- Only a non-empty `uploadUrl` passes the URL and enables uploading — reserved for a future self-hosted collector (official crashpad tests use `generate_test_server`; a production self-build would follow the minidump HTTP POST convention). Whether to deploy a collector is a data-egress decision, off by default, to be decided separately.
- This batch implements no network upload path whatsoever.

## 8. Integration with Existing Logging/Monitoring

- **Logging**: one spdlog line each for initialization success/failure (including the handler path and the database directory); at startup, `pendingReports()` queries for crash reports left over from the previous run, and any found are listed path by path at `warn` level — bringing "it crashed last time" into the regular log stream (reusing the same chain as `createEventLogSink`).
- **Annotations**: `version` (`WINGMAN_VERSION_FULL`), `platform` (`wingman::platform::platformName()`), `argv0` — the dump is self-describing, so the symbol version can be matched during stack reconstruction.
- **Monitoring (a later decision, not in this batch)**: crash report metadata reported to the Go server via `agent.event` Notify for dashboard display — this involves crash metadata leaving the machine and is off by default, same policy as §7.

## 9. Acceptance Plan (Expansion of ③)

1. `wingman-agent crash-test`: a new CLI command (separate file `commands/crash_test_command.cpp`, following the command-splitting convention) that initializes crashpad and then deliberately dereferences a null pointer (`wingman::crash::testCrashNullPointer()`, a write to page 0 triggering SIGSEGV).
2. Assertion chain: the process exits abnormally → a complete `.dmp` appears in `crashes/pending/` → `dump_syms` produces symbols → `minidump_stackwalk` reconstructs the `testCrashNullPointer` frame and source line at the top of the stack.
3. Solidified as `scripts/verify-crashpad.sh` (following the `verify-xrecord-desktop.sh` pattern: a missing crashpad build or missing tools is judged "not verified" rather than passed).
4. Acceptance evidence (excerpts of the script output) is backfilled into §10 of this document.

## 10. Acceptance Record

**2026-10-09 (real Linux machine, GCC 15.2 / CMake 4.2.3 / kernel 7.0, `scripts/verify-crashpad.sh` full-chain PASS)**

- Environment: an isolated `XDG_DATA_HOME` directory; `wingman-agent crash-test` exit=139 (SIGSEGV).
- Dump on disk: `$XDG_DATA_HOME/wingman/crashes/pending/<uuid>.dmp`, 14992–17392 bytes, 6/6 successful runs in a row (see the "sporadic first run" note below).
- Symbolization (rust-implemented toolchain, format-compatible with the §6 breakpad tools):
  ```bash
  dump_syms --store <symroot> build/apps/agent/wingman-agent
  minidump-stackwalk --human <dump> <symroot>
  ```
  Top-of-stack reconstruction (source lines come from the Debug build's `-g`):
  ```
  Crash reason:  SIGSEGV / SEGV_MAPERR
  Crash address: 0x0000000000000000
  Thread 0  (crashed)
   0  wingman-agent!wingman::crash::testCrashNullPointer() [crash_client.cpp : 53 + 0x4]
  ```
- Investigation notes: the early sporadic failures were 0-byte shells under `new/` or no file at all — the client's 5s wait timed out within the cold-load window of the first run after a build (strace evidence: PTRACE_ATTACH failing because the slot is taken is strace itself seizing the tracer, not a product issue); in steady state 6/6 all pass. If a CI/script environment reproduces the sporadic failure, the acceptance script can add a single retry.
- ptrace authorization chain verified in practice: the client's `prctl(PR_SET_PTRACER, handler_pid)` (required under yama ptrace_scope=1) plus the handler's double fork detaching from the process group both work as designed.

## 11. Sources

- Upstream and packaging: [crashpad source (github mirror)](https://github.com/chromium/crashpad), [crashpad documentation (doxygen)](https://chromium.googlesource.com/crashpad/crashpad/+/master/doc/), [TheAssemblyArmada/crashpad-cmake](https://github.com/TheAssemblyArmada/crashpad-cmake) (README usage, CMakeLists pin values, Apache-2.0)
- Client API: `client/crashpad_client.h` (StartHandler), `client/crash_report_database.h` (Initialize/GetPendingReports), `client/settings.h` (SetUploadsEnabled); the vendored source is authoritative
- Breakpad comparison: [breakpad repository](https://chromium.googlesource.com/breakpad/breakpad/), [minidump_stackwalk](https://chromium.googlesource.com/breakpad/breakpad/+/master/docs/processor_design.md)
- In-repo: docs/architecture-decisions.md (Forbidden Changes), scripts/check_platform_boundary.sh (platform macro boundary), docs/dependencies.md (dependency registry)
