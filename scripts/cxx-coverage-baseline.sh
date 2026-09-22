#!/usr/bin/env bash
# C++ 行覆盖率基线采集（lcov 1.16 本地安装于 ~/.local/opt/lcov-1.16）
#
# 前置：build-cov 已以 --coverage 全局插桩（CMAKE_CXX_FLAGS="--coverage -O0"——
#       仅 CODE_COVERAGE 选项只插桩 core_tests 测试代码自身，库对象无 gcda，
#       数字虚高无意义）。
# 用法：scripts/cxx-coverage-baseline.sh   # 在仓库根执行
#       DISPLAY 指向 Xvfb/桌面可多覆盖 X11 正向路径；无 X 用例自动 skip。
set -euo pipefail
cd "$(dirname "$0")/.."

LC=~/.local/opt/lcov-1.16/bin
BIN=build-cov/lib/wingman/tests/core_tests

[[ -x "$BIN" ]] || { echo "core_tests 不存在，先构建 build-cov"; exit 1; }

echo "==> 全量测试（DISPLAY=${DISPLAY:-无}）"
# 个别用例失败（含在案 flaky）不中断采集：gcda 按已执行代码写出，依然有效
"$BIN" 2>&1 | tail -3 || echo "（测试存在失败，继续采集）"

echo "==> lcov 捕获（gcda 于进程退出时已写出）"
"$LC/lcov" --capture --directory build-cov --output-file /tmp/cxx-base.info \
	--gcov-tool "$(command -v gcov)" 2>&1 | tail -1
"$LC/lcov" --extract /tmp/cxx-base.info "$PWD/lib/wingman/*" "$PWD/apps/*" \
	--output-file /tmp/cxx-prod.info >/dev/null
# tests 目录是测试代码自身，不计生产覆盖口径
"$LC/lcov" --remove /tmp/cxx-prod.info "*/tests/*" \
	--output-file /tmp/cxx-final.info >/dev/null
"$LC/lcov" --list /tmp/cxx-final.info
