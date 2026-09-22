#!/usr/bin/env bash
# XRecord 宏录制真桌面验证（development-todo 剩余项的自动化入口）
#
# 覆盖：XRecord 正向闭环——start 后经 XTest 注入按键，断言捕获与 JSON 序列化。
#       对应 core_tests 的 RecorderX11E2E.*（recorder_x11_e2e_test.cpp）。
#
# 环境要求：Linux + 真实桌面 X server。Xvfb 的 RECORD 扩展 EnableContext
#           必然失败（XRecordBadContext），用例会 SKIP——脚本把 SKIP 判为
#           「未验证」而非通过，防止在无头环境误报绿。
#
# 用法：scripts/verify-xrecord-desktop.sh [--build]
#       --build  强制重新配置并编译 core_tests（默认缺二进制时才构建）
set -euo pipefail
cd "$(dirname "$0")/.."

if [[ "$(uname -s)" != "Linux" ]]; then
	echo "SKIP: 本脚本仅用于 Linux（当前 $(uname -s)）"; exit 2
fi
if [[ -z "${DISPLAY:-}" ]]; then
	echo "SKIP: DISPLAY 为空——请在真实桌面 X 会话的终端中运行"; exit 2
fi

BIN=build/lib/wingman/tests/core_tests
if [[ ! -x "$BIN" || "${1:-}" == "--build" ]]; then
	echo "==> 构建 core_tests（首次或 --build）"
	cmake -B build \
		-DVCPKG_MANIFEST_FEATURES=tests -DWINGMAN_BUILD_TESTS=ON >/dev/null
	cmake --build build --target core_tests -j"$(nproc)"
fi

echo "==> 运行 RecorderX11E2E.*（DISPLAY=$DISPLAY）"
set +e
OUT=$("$BIN" --gtest_filter='RecorderX11E2E.*' 2>&1)
RC=$?
set -e
echo "$OUT" | grep -E '^\[ (RUN|OK|FAILED|SKIPPED)' || true

PASSED=$(echo "$OUT" | grep -cE '^\[  PASSED  \] [1-9]' || true)
if echo "$OUT" | grep -q '\[  FAILED  \]' || [[ $RC -ne 0 ]]; then
	echo "FAIL: XRecord 真桌面验证未通过（exit=$RC）"; exit 1
fi
if [[ "$PASSED" -eq 0 ]]; then
	echo "SKIP: 用例全部跳过——当前是 Xvfb/无头环境，无 RECORD 正向能力。"
	echo "      请在真实桌面（GNOME/KDE/XFCE 等）的终端重跑本脚本。"; exit 2
fi
echo "PASS: XRecord 捕获 + JSON 序列化闭环验证通过。"
