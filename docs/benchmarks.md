> [中文](benchmarks.md) | **English**

# Performance Benchmarks

Wingman's performance benchmarks are a set of standalone executables located in `lib/wingman/tests/benchmarks/`, covering five shared paths: KV store, event dispatch, Lua engine, vision pipeline, and the PatternMatcher matching core (`kvstore_bench` / `event_hub_bench` / `lua_bench` / `vision_bench` / `matcher_bench`; the last three are built conditionally on their dependencies, see "How to Run" below). Benchmarks are not part of the CI gate (the numbers are strongly affected by host load, so they are unsuitable for regression assertions; `.github/workflows/` has zero references repo-wide); they are used for qualitative comparison during development.

## How to Run

Build and run all benchmarks with one command (defaults to the `build-ml` directory; another build directory can be given as an argument):

```bash
scripts/run-benchmarks.sh [build-dir]
```

Script behavior:

- If the build directory was not configured with `WINGMAN_BUILD_BENCHMARKS=ON`, it reconfigures automatically;
- Whether `vision_bench` is included is decided by `WINGMAN_HAS_OPENCV`;
- It prints the machine context (time / kernel / load / CPU model / matcher thread tier) and then runs them one by one;
- `vision_bench` needs an available display surface; how to run it locally: `Xvfb :99` + `DISPLAY=:99` (the script already passes this by default).

The manual form is equivalent to:

```bash
cmake -B build-ml -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake \
    -DVCPKG_TARGET_TRIPLET=x64-linux -DCMAKE_BUILD_TYPE=Release \
    -DWINGMAN_BUILD_BENCHMARKS=ON
cmake --build build-ml --target kvstore_bench event_hub_bench matcher_bench lua_bench vision_bench
build-ml/lib/wingman/tests/benchmarks/kvstore_bench   # 其余同理
```

Two constraints shared with the script (the easiest ones to trip over when writing commands by hand):

- The gate for the benchmark subdirectory is `if(WINGMAN_BUILD_BENCHMARKS AND WINGMAN_BUILD_CORE)` (root `CMakeLists.txt`); `lua_bench` additionally requires `TARGET wingman::lua`, and `vision_bench` additionally requires `WINGMAN_HAS_OPENCV`; when a target does not exist, `cmake --build --target` reports unknown target directly.
- You must pass `-DCMAKE_BUILD_TYPE=Release` explicitly (this is exactly what the `run-benchmarks.sh` script sets): a default build directory carries no optimization, and the benchmarks would then report unoptimized readings.

## Notes on Interpreting Results

- Except for `kvstore_bench` (early hand-written timing), everything goes through the `Bench` helper in `bench.hpp`: each case first runs `iterations/10` warm-up rounds, then times the whole batch for 7 rounds and takes the **median ns/op**, which resists single-run jitter.
- **The kvstore numbers use a different methodology from the other four and cannot be compared side by side directly**: `kvstore_bench.cpp` ships its own hand-written timing helper (1 warm-up call + a single timed round taking the mean, unit µs/op), with no 7-round median; and its two cases measure **composite operations** — each `DELETE` iteration is `set` + `del`, and `LPOP` is `lpop` + `lpush` (restoring the list) — so the single-column values are higher than the cost of one pure single operation.
- **Absolute values are heavily affected by host load**: on a shared dev machine the wall time can be several times the real CPU cost (you can use `time` and look at the wall/user ratio to estimate a load factor). Cross-comparison must be done in pairs on the same machine under the same load; the baselines below only indicate order of magnitude.
- `vision_bench` prints one notice line and exits directly when there is no display surface (this does not count as a failure).

## Baseline Record (2026-10-13)

Machine: i9-10880H (14 cores) / Linux 7.0.0-38 / load 26~42 / Xvfb :99 1280x800.

### kvstore (in-memory KV store)

| Case | Result |
| --- | --- |
| SET | 0.05 µs/op |
| GET (hit) | 0.03 µs/op |
| GET (miss) | 0.04 µs/op |
| EXISTS / DELETE / INCR | 0.02 / 0.16 / 0.11 µs/op |
| HSET / HGET | 0.08 / 0.03 µs/op |
| LPUSH / LPOP | 0.10 / 0.19 µs/op |
| Throughput read / write / mixed | 10M / 5M / 5M ops/s |

### event_hub (synchronous event dispatch)

| Case | ns/op | Throughput |
| --- | --- | --- |
| emit empty payload → 1 subscriber | 288 | 3.47M ops/s |
| emit empty payload → 10 subscribers | 542 | 1.84M ops/s |
| emit 1KB payload → 1 subscriber | 354 | 2.82M ops/s |
| subscribe + unsubscribe churn | 360 | 2.77M ops/s |

### lua (engine and module glue)

| Case | Result |
| --- | --- |
| Engine create + initialize + shutdown | 119 µs/run (8400 runs/s) |
| executeString `return 1+1` | 2.5 µs |
| callFunction (3 arguments) | 1.1 µs |
| `wingman.kv.set` via executeString (after all modules registered) | 5.5 µs |
| `wingman.kv.get` via executeString | 5.1 µs |

Conclusion reference: under the one-engine-per-script convention, engine creation at ~0.1ms is not a hotspot; script round-trips are at the microsecond level, module glue is ~5µs end to end, and a single script can execute hundreds of thousands of API calls per second.

### vision (screen capture / color search / image search, Xvfb 1280x800)

| Case | Result |
| --- | --- |
| Screen::capture full screen | ~129 ms/op |
| findColor full screen, no match (tol=10) | ~92 ms/op |
| findColor full screen, match (tol=0) | ~91 ms/op |
| findImage 32x32 template, full screen, no match | ~350 ms/op |

Note: in this round the host load was 26~42 (14 cores), so the vision numbers carry the heaviest wall-time skew (an earlier low-load re-measurement showed a wall/user ratio of about 5x). Read them for order of magnitude only: color-search / image-search cost is dominated by the full-screen capture, and image search adds full-screen template matching on top. For a real evaluation, re-measure during an idle period.

## Baseline Record (2026-10-14 Re-measurement, Low Load)

Machine: i9-10880H (14 cores) / Linux 7.0.0-38 / load 14.6 / display surface 1600x1000 (geometry differs from the Xvfb 1280x800 of 10-13, so the vision rows are not directly comparable item by item; read them after correcting for the pixel count).

### kvstore / event_hub / lua (same order of magnitude as the 10-13 baseline)

| Path | This round | 10-13 baseline |
| --- | --- | --- |
| kv SET / GET hit | 0.07 / 0.04 µs | 0.05 / 0.03 µs |
| kv DELETE / HSET | 0.32 / 0.22 µs | 0.16 / 0.08 µs |
| kv throughput read / write / mixed | 10M / 2.5M / 3.33M ops/s | 10M / 5M / 5M ops/s |
| emit → 1 / 10 subscribers | 177 / 521 ns | 288 / 542 ns |
| Engine lifecycle / `return 1+1` | 118 µs / 2.3 µs | 119 µs / 2.5 µs |
| `kv.set` via executeString | 5.4 µs | 5.5 µs |

The 2~3x fluctuation of kv micro-operations appears even at lower load; it is sensitivity noise from 0.0x µs-level operations to frequency / cache state, and does not constitute a regression signal; event_hub and lua are all no worse than the baseline.

### vision (1600x1000 = 1.6M pixels)

The right column is uniformly converted to this round's pixel count (cost per million pixels), so all rows share one baseline and can be subtracted from each other.

| Case | This round | Converted (per million pixels) |
| --- | --- | --- |
| Screen::capture full screen | ~18 ms/op | ~11 ms/million pixels |
| findColor full screen, no match (tol=10) | ~23 ms/op | capture cost + ~3 ms |
| findColor full screen, match (tol=0) | ~18 ms/op | ≈ capture cost |
| findImage 32x32 template, full screen, no match | ~264 ms/op | capture cost + ~154 ms/million pixels |

### Correcting the 10-13 Conclusion

Under low load, "color-search / image-search cost is dominated by the full-screen capture" does not hold: the 129 ms capture reading from 10-13 was a load artifact (the ratio of readings for the same case across the two rounds is 129/18 ≈ 7x; note that this is a different quantity from the ~5x wall/user ratio mentioned in the 10-13 baseline section — the latter is an independent reading from an even earlier re-measurement). The real cost structure is —

- findColor ≈ capture cost (the full-screen pixel scan adds only ~5 ms per round, i.e. ~3 ms/million pixels — an order of magnitude that can be ignored);
- **findImage is dominated by full-screen template matching**: of the 264 ms, capture accounts for only ~18 ms (7%) and matching for ~93% (~154 ms/million pixels). This is the first data-driven optimization target for the second half (region-limited search is already supported; speeding up full-screen image search would require a pyramid / downsample-style matching strategy, which is a design trade-off and has not been touched).

## Optimization Record (2026-10-15, i9-10880H, paired measurement on the same day under the same load)

### Two Paths in Fact

findImage has two implementation paths; the earlier baseline only measured the desktop one:

- **Desktop `Vision::findImage`** (the path taken by the vision/screen surface of scripts on the desktop runtime): `cv::matchTemplate(TM_CCOEFF_NORMED)`, OpenCV is already an optimized implementation;
- **`PatternMatcher::templateMatch`** (the path used by `screen/vision.findImage` on the Android side and by the image condition of `wingman.trigger`; the desktop does not go through it): a naive quadruple scalar loop.

### matcher_bench (new, purely synthetic frames with no display-surface dependency, 1600x1000 vs 32x32 no match)

| Case | Before optimization | After optimization | Improvement |
| --- | --- | --- | --- |
| templateMatch full screen, no match | 18,145 ms/op | 2,491 ms/op | 7.3x |
| templateMatch 400x300 region, no match | 1,481 ms/op | 162 ms/op | 9.1x |

What was optimized (constant factors, **bit-identical output**): the template norm was previously recomputed at every search position (hoisted out as an invariant); template/target pixels were previously re-read through `getPixel` (bounds checking + Color construction) w*h times at every position (precomputed into a luminance plane, one read per pixel); repeated luminance evaluation. The accumulation order matches the original implementation item by item (ty outer / tx inner, three independent accumulators), and the golden-value regression pins (`TemplateMatchGoldenBits` / `FindImageGoldenBits` in `image_analyzer_test.cpp`, LCG deterministic input, EXPECT_EQ bit-level comparison) pass with identical values before and after optimization.

### templateMatch Row-Blocked Parallelism (2026-10-15, 14 logical cores on Linux, paired measurement on the same day under the same load)

| Case | Serial (THREADS=1) | Auto tier (unset) | Improvement |
| --- | --- | --- | --- |
| templateMatch full screen, no match | 2,440–2,488 ms/op | 464–590 ms/op | 4.8x |
| templateMatch 400x300 region, no match | 155–161 ms/op | 34–37 ms/op | 4.4x |

What was optimized (thread parallelism, **bit-identical output**): sum/sumTarget at each search position are mutually independent, so they are parallelized by row blocks while the original scan order is preserved within a block, and blocks are reduced in block order with a strict `>`, meaning the IEEE-754 result bits and the tie-breaking order of "first maximum position wins" both match the serial version. The auto tier is enabled only when the number of scan rows is ≥ 64 and the number of positions is ≥ 16384 (blocking loses net performance on small images), and the thread count is taken from `hardware_concurrency()`; the environment variable `WINGMAN_MATCHER_THREADS` can force an override (0/1 = serial, N≥2 = force N threads and bypass the size threshold). The regression pin `TemplateMatchParallelBitExact` in `image_analyzer_test.cpp` (forcing 2/3/5/8 threads plus the auto tier, EXPECT_EQ bit-level comparison against the serial baseline, both the full-screen and region forms) passes. On single-core devices the auto tier degenerates to serial and does not apply; the multi-core gains are in the table above (this machine has 14 logical cores).

### Remaining Thresholds (2026-10-15 inspection verdict: intentionally not done, not a leftover gap)

> Convergence criterion: **only open an item if semantics do not change** — bit-level identity and match semantics are hard constraints. The two legs already optimized on the matcher have reached the bit-legal upper limit (first increment 7.3–9.1x + parallel tier 4.8/4.4x), which is the final state of the first optimization half; going forward only engineering-level optimizations that do not change the output are accepted (caching, parallelism, lazy evaluation). The following three items are intentionally not done:

- **SIMD/float32**: reordering the floating-point accumulation immediately breaks bit-level identity, and the golden-value regression pin (TemplateMatchGoldenBits) loses its meaning — not done.
- **Pyramid / downsample**: candidate pre-filtering at a coarse level changes match semantics (the unique exact-match position may be filtered out); the 2.4 s single-core residual is accepted together with the desktop 264 ms (the bulk of which is in the OpenCV matching proper, which is the same class of thing) — not done.
- **Bringing the OpenCV matcher on-device (NDK has opencv4)**: it introduces a difference in score convention versus desktop TM_CCOEFF_NORMED, making same-named API results on the two ends incomparable — not done.

On multi-core machines the worst full-screen serial path of 2.5 s has been pushed down to ~0.5 s by the parallel tier (usable with the Android trigger's 500ms polling interval); single-core devices remain at 2.4 s, which is one of the intentionally accepted items above.

## Adding a New Benchmark

1. Create `<name>_bench.cpp` under `lib/wingman/tests/benchmarks/`, using the `Bench` class in `bench.hpp` (`b.header(...)` for grouping + `b.run(name, fn, iterations)`);
2. Add the corresponding `add_executable` in `lib/wingman/tests/benchmarks/CMakeLists.txt` (including `target_include_directories` + `target_link_libraries(… PRIVATE wingman)`, with the Windows tier additionally adding `ws2_32` in the `if(WIN32)` block); gate optional dependencies with `if(TARGET ...)` / `if(WINGMAN_HAS_OPENCV)`;
3. Add the new target to the `TARGETS` list in `scripts/run-benchmarks.sh`;
4. After running, add a section to the "Baseline Record" of this document (noting the date and the machine/load context).