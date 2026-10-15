#!/usr/bin/env bash
# 构建（按需）并运行全部基准，输出带机器上下文的结果。
# 用法：scripts/run-benchmarks.sh [build-dir]（默认 build-ml；需已按常规
#   vcpkg 工具链配置，本脚本只负责追加 -DWINGMAN_BUILD_BENCHMARKS=ON 重配）
# 注意：vision_bench 需要可用显示面（本机跑法：Xvfb :99 + DISPLAY=:99）。
#   绝对数值受宿主负载影响大（共享机上 wall/CPU 比即负载系数），横向对比
#   须同机同负载成对测；基线记录见 docs/benchmarks.md。
set -euo pipefail

BUILD_DIR="${1:-build-ml}"
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT"

TOOLCHAIN="${CMAKE_TOOLCHAIN_FILE:-$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake}"
TRIPLET="${VCPKG_TARGET_TRIPLET:-x64-linux}"

if ! grep -q "WINGMAN_BUILD_BENCHMARKS:BOOL=ON" "$BUILD_DIR/CMakeCache.txt" 2>/dev/null; then
    echo ">> configuring $BUILD_DIR with WINGMAN_BUILD_BENCHMARKS=ON"
    cmake -B "$BUILD_DIR" \
        -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
        -DVCPKG_TARGET_TRIPLET="$TRIPLET" \
        -DCMAKE_BUILD_TYPE=Release \
        -DWINGMAN_BUILD_BENCHMARKS=ON
fi

TARGETS=(kvstore_bench event_hub_bench matcher_bench lua_bench)
if grep -q "WINGMAN_HAS_OPENCV:INTERNAL=1" "$BUILD_DIR/CMakeCache.txt" 2>/dev/null; then
    TARGETS+=(vision_bench)
fi

echo ">> building: ${TARGETS[*]}"
cmake --build "$BUILD_DIR" --target "${TARGETS[@]}"

BENCH_BIN="$BUILD_DIR/lib/wingman/tests/benchmarks"

echo
echo "== machine context =="
echo "date: $(date -Is)"
echo "kernel: $(uname -r)  load: $(cut -d' ' -f1-3 /proc/loadavg)"
if command -v lscpu >/dev/null 2>&1; then
    echo "cpu: $(lscpu | awk '/Model name/ {$1=$2=""; sub(/^  */, ""); print}')"
fi
echo "cpu cores: $(nproc)"
# matcher_bench 读数随并行度变化（未设置=自动档=cpu cores；0/1=串行；N>=2=强制 N 线程）
echo "matcher threads: ${WINGMAN_MATCHER_THREADS:-<unset=auto>}"

for t in "${TARGETS[@]}"; do
    echo
    echo ">> run $t"
    DISPLAY="${DISPLAY:-:99}" "$BENCH_BIN/$t"
done
