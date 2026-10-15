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

## 基线记录（2026-10-14 复测，低负载）

机器：i9-10880H（14 核）/ Linux 7.0.0-38 / load 14.6 / 显示面 1600x1000
（与 10-13 的 Xvfb 1280x800 几何不同，vision 行不可逐项直比，按像素量
修正后阅读）。

### kvstore / event_hub / lua（与 10-13 基线同量级）

| 路径 | 本轮 | 10-13 基线 |
| --- | --- | --- |
| kv SET / GET 命中 | 0.07 / 0.04 µs | 0.05 / 0.03 µs |
| kv DELETE / HSET | 0.32 / 0.22 µs | 0.16 / 0.08 µs |
| kv 吞吐 读 / 写 / 混合 | 1000 万 / 250 万 / 333 万 ops/s | 1000 万 / 500 万 / 500 万 ops/s |
| emit → 1 / 10 订阅者 | 177 / 521 ns | 288 / 542 ns |
| 引擎生命周期 / `return 1+1` | 118 µs / 2.3 µs | 119 µs / 2.5 µs |
| `kv.set` 经 executeString | 5.4 µs | 5.5 µs |

kv 微操 2~3x 波动出现在更低负载下，属 0.0x µs 级操作对频率/缓存态的敏
感噪声，不构成回归信号；event_hub 与 lua 全部不劣于基线。

### vision（1600x1000 ≈ 200 万像素）

| 用例 | 本轮 | 按 10-13 几何折算 |
| --- | --- | --- |
| Screen::capture 全屏 | ~18 ms/op | ~11 ms/百万像素 |
| findColor 全屏未命中（tol=10） | ~23 ms/op | 截屏成本 + ~2 ms |
| findColor 全屏命中（tol=0） | ~18 ms/op | ≈ 截屏成本 |
| findImage 32x32 模板全屏未命中 | ~264 ms/op | 截屏成本 + ~123 ms/百万像素 |

### 修正 10-13 的结论

低负载下「找色/找图成本由全屏截屏主导」不成立：10-13 的 129 ms 截屏读
数是负载伪影（wall 被拉高约 7x）。真实成本结构是——

- findColor ≈ 截屏成本（全屏像素扫描仅加 ~2 ms，量级可忽略）；
- **findImage 由全屏模板匹配主导**：264 ms 中截屏只占 ~18 ms（7%），
  匹配占 ~93%。优化半场的第一个数据驱动目标在此（区域限定搜索已支持，
  全屏找图要提速需金字塔/降采样类匹配策略，属设计取舍，未动）。

## 优化记录（2026-10-15，i9-10880H，同日同负载成对测）

### 两路径事实

findImage 有两条实现路径，此前基线只量了桌面一条：

- **桌面 `Vision::findImage`**（脚本 vision/screen 面在桌面 runtime 的路径）：
  `cv::matchTemplate(TM_CCOEFF_NORMED)`，OpenCV 已是优化实现；
- **`PatternMatcher::templateMatch`**（Android 端 `screen/vision.findImage` 与
  `wingman.trigger` 图像条件的路径，桌面不经此）：朴素四重标量循环。

### matcher_bench（新增，纯合成帧无显示面依赖，1600x1000 vs 32x32 未命中）

| 用例 | 优化前 | 优化后 | 提升 |
| --- | --- | --- | --- |
| templateMatch 全屏未命中 | 18,145 ms/op | 2,491 ms/op | 7.3x |
| templateMatch 400x300 区域未命中 | 1,481 ms/op | 162 ms/op | 9.1x |

优化内容（常数因子，**输出位级不变**）：模板范数原在每个搜索位置重算
（不变量外提）、模板/目标像素原经 `getPixel`（边界检查 + Color 构造）在每
个位置重读 w*h 次（亮度面预计算，每像素读一次）、亮度重复求值。累加顺序
与原实现逐项一致（ty 外层 / tx 内层，三个累加器独立），黄金值回归钉
（`image_analyzer_test.cpp` 的 `TemplateMatchGoldenBits` / `FindImageGoldenBits`，
LCG 确定性输入，EXPECT_EQ 位级比较）在优化前后同值通过。

### templateMatch 按行分块并行（2026-10-15，14 逻辑核 Linux，同日同负载成对测）

| 用例 | 串行（THREADS=1） | 自动档（未设置） | 提升 |
| --- | --- | --- | --- |
| templateMatch 全屏未命中 | 2,440–2,488 ms/op | 464–590 ms/op | 4.8x |
| templateMatch 400x300 区域未命中 | 155–161 ms/op | 34–37 ms/op | 4.4x |

优化内容（线程并行，**输出位级不变**）：每个搜索位置的 sum/sumTarget 相互
独立，按行分块并行、块内保持原扫描序、块间按块序以严格 `>` 归约，因此
IEEE-754 结果位与「首个最大位置优先」的平局次序都与串行一致。自动档仅在
扫描行数 ≥ 64 且位置数 ≥ 16384 时启用（小画面分块净亏），线程数取
`hardware_concurrency()`；可用环境变量 `WINGMAN_MATCHER_THREADS` 强制覆盖
（0/1=串行，N≥2=强制 N 线程并绕过大小阈值）。回归钉
`image_analyzer_test.cpp` 的 `TemplateMatchParallelBitExact`（强制 2/3/5/8
线程 + 自动档，与串行基准 EXPECT_EQ 位级比较，全屏/区域两形态）。
单核设备自动档退化为串行，不适用；多核收益见上表（本机 14 逻辑核）。

### 剩余门槛（设计取舍，维持登记未动）

- 全屏 2.5 s 的串行最坏路径在多核机器上已被并行档压到 ~0.5 s（Android 触发
  器轮询 500ms 间隔可用）；单核设备仍为 2.4 s。进一步提速只剩
  改语义的选项——SIMD/float32 会重排浮点累加（位级一致失守）、金字塔/降
  采样会改命中语义、端侧接 OpenCV 匹配器（NDK 有 opencv4）会引入与桌面
  TM_CCOEFF_NORMED 的分数口径差。属设计取舍，待拍板后单独立项。
- 桌面路径的 264 ms 大头在 OpenCV 匹配本体，同属金字塔类取舍，口径同上。

## 新增基准

1. 在 `lib/wingman/tests/benchmarks/` 新建 `<name>_bench.cpp`，用 `bench.hpp`
   的 `Bench` 类（`b.header(...)` 分组 + `b.run(name, fn, iterations)`）；
2. `CMakeLists.txt` 加对应 `add_executable`，可选依赖用 `if(TARGET ...)` /
   `if(WINGMAN_HAS_OPENCV)` 门控；
3. `scripts/run-benchmarks.sh` 的 `TARGETS` 列表加进新目标；
4. 运行后在本文档「基线记录」补一段（注明日期与机器/负载上下文）。
