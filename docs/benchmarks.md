# 性能基准测试

Wingman 的性能基准是一组独立可执行文件，位于 `lib/wingman/tests/benchmarks/`，
覆盖 KV 存储、事件分发、Lua 引擎与视觉链路四条公共路径。基准不进 CI 门禁
（数值受宿主负载影响大，不适合做回归断言），用于开发期定性对比。

## 运行方式

一键构建并运行全部基准（默认 `build-ml` 目录，可在参数里指定其他构建目录）：

```bash
scripts/run-benchmarks.sh [build-dir]
```

脚本行为：

- 构建目录未开 `WINGMAN_BUILD_BENCHMARKS=ON` 时自动重配；
- 按 `WINGMAN_HAS_OPENCV` 决定是否包含 `vision_bench`；
- 输出机器上下文（时间 / 内核 / 负载 / CPU 型号）后再逐个运行；
- `vision_bench` 需要可用显示面，本机跑法：`Xvfb :99` + `DISPLAY=:99`（脚本已默认传递）。

手动方式等价于：

```bash
cmake -B build-ml -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake \
    -DVCPKG_TARGET_TRIPLET=x64-linux -DWINGMAN_BUILD_BENCHMARKS=ON
cmake --build build-ml --target kvstore_bench event_hub_bench lua_bench vision_bench
build-ml/lib/wingman/tests/benchmarks/kvstore_bench   # 其余同理
```

## 结果解读须知

- 除 `kvstore_bench`（早期自写计时）外，全部走 `bench.hpp` 的 `Bench` 助手：
  每个用例先跑 1/10 轮热身，再整批计时 7 轮取**中位 ns/op**，抗单次抖动。
- **绝对数值受宿主负载影响大**：共享开发机上 wall 时间可能数倍于真实 CPU
  成本（可用 `time` 看 wall/user 比估算负载系数）。横向对比必须同机同负载
  成对测；下表基线仅示意量级。
- `vision_bench` 在无显示面时打印一行提示后直接退出（不算失败）。

## 基线记录（2026-10-13）

机器：i9-10880H（14 核）/ Linux 7.0.0-38 / load 26~42 / Xvfb :99 1280x800。

### kvstore（内存 KV 存储）

| 用例 | 结果 |
| --- | --- |
| SET | 0.05 µs/op |
| GET（命中） | 0.03 µs/op |
| GET（未命中） | 0.04 µs/op |
| EXISTS / DELETE / INCR | 0.02 / 0.16 / 0.11 µs/op |
| HSET / HGET | 0.08 / 0.03 µs/op |
| LPUSH / LPOP | 0.10 / 0.19 µs/op |
| 吞吐 读 / 写 / 混合 | 1000 万 / 500 万 / 500 万 ops/s |

### event_hub（事件同步分发）

| 用例 | ns/op | 吞吐 |
| --- | --- | --- |
| emit 空负载 → 1 订阅者 | 288 | 347 万 ops/s |
| emit 空负载 → 10 订阅者 | 542 | 184 万 ops/s |
| emit 1KB 负载 → 1 订阅者 | 354 | 282 万 ops/s |
| subscribe + unsubscribe churn | 360 | 277 万 ops/s |

### lua（引擎与模块胶水）

| 用例 | 结果 |
| --- | --- |
| 引擎 create + initialize + shutdown | 119 µs/次（8400 次/s） |
| executeString `return 1+1` | 2.5 µs |
| callFunction（3 参数） | 1.1 µs |
| `wingman.kv.set` 经 executeString（全模块注册后） | 5.5 µs |
| `wingman.kv.get` 经 executeString | 5.1 µs |

结论参考：每脚本一引擎的口径下，引擎创建 ~0.1ms 不构成热点；脚本往返
微秒级，模块胶水端到端 ~5µs，单脚本每秒可执行数十万条 API 调用。

### vision（截屏 / 找色 / 找图，Xvfb 1280x800）

| 用例 | 结果 |
| --- | --- |
| Screen::capture 全屏 | ~129 ms/op |
| findColor 全屏未命中（tol=10） | ~92 ms/op |
| findColor 全屏命中（tol=0） | ~91 ms/op |
| findImage 32x32 模板全屏未命中 | ~350 ms/op |

注意：本轮宿主 load 26~42（14 核），vision 数值 wall 偏移最重（此前低载
复测 wall/user 比约 5x），只看量级：找色/找图成本由全屏截屏主导，找图再
叠加全屏模板匹配。真实评估请在空闲时段复测。

## 新增基准

1. 在 `lib/wingman/tests/benchmarks/` 新建 `<name>_bench.cpp`，用 `bench.hpp`
   的 `Bench` 类（`b.header(...)` 分组 + `b.run(name, fn, iterations)`）；
2. `CMakeLists.txt` 加对应 `add_executable`，可选依赖用 `if(TARGET ...)` /
   `if(WINGMAN_HAS_OPENCV)` 门控；
3. `scripts/run-benchmarks.sh` 的 `TARGETS` 列表加进新目标；
4. 运行后在本文档「基线记录」补一段（注明日期与机器/负载上下文）。
