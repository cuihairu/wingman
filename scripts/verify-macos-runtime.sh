#!/usr/bin/env bash
# macOS runtime 平台功能真机验证（development-todo 剩余项的自动化入口）
#
# 覆盖 development-todo 中 macOS 真机验证五项里可自动化的部分：
#   UDS IPC（UnixSocketChannelTest）、剪贴板（ClipboardTest）、
#   CGWindowList 截图（ScreenTest）、FileWatcher（FileWatcherTest）、
#   CGEvent 输入（InputTest）。
#
# 环境要求：macOS + vcpkg（VCPKG_ROOT 已设置）。依赖一律走 vcpkg
#           manifest，找不到 VCPKG_ROOT 时明确报错，不回退系统库。
#
# 用法：scripts/verify-macos-runtime.sh [--build]
#       --build  强制重新配置并编译 core_tests（默认缺二进制时才构建）
set -euo pipefail
cd "$(dirname "$0")/.."

if [[ "$(uname -s)" != "Darwin" ]]; then
	echo "SKIP: 本脚本仅用于 macOS（当前 $(uname -s)）"; exit 2
fi
if [[ -z "${VCPKG_ROOT:-}" ]]; then
	echo "FAIL: 未设置 VCPKG_ROOT——本仓库依赖一律由 vcpkg 管理，不回退系统库。"
	echo "      请先安装 vcpkg 并 export VCPKG_ROOT=<路径>"; exit 1
fi

ARCH="$(uname -m)"
case "$ARCH" in
	arm64) TRIPLET=arm64-osx ;;
	*)     TRIPLET=x64-osx ;;
esac

BIN=build/lib/wingman/tests/core_tests
if [[ ! -x "$BIN" || "${1:-}" == "--build" ]]; then
	echo "==> 构建 core_tests（首次或 --build，triplet=$TRIPLET）"
	cmake -B build \
		-DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
		-DVCPKG_TARGET_TRIPLET="$TRIPLET" \
		-DVCPKG_MANIFEST_FEATURES=tests -DWINGMAN_BUILD_TESTS=ON
	cmake --build build --target core_tests -j"$(sysctl -n hw.ncpu)"
fi

FILTER='ClipboardTest.*:FileWatcherTest.*:ScreenTest.*:InputTest.*:UnixSocketChannelTest.*'
echo "==> 运行 macOS 平台套件（$FILTER）"
set +e
"$BIN" --gtest_filter="$FILTER"
RC=$?
set -e

if [[ $RC -ne 0 ]]; then
	echo "FAIL: macOS 平台套件存在失败用例（exit=$RC）"; exit 1
fi
echo "PASS: macOS 可自动化验证项全部通过。"
echo "提示: 以下仍属人工观察项——CGEvent 注入的焦点/权限手感（辅助功能授权）、"
echo "      录屏授权弹窗行为、通知/系统休眠交互等，需真机桌面会话中人工确认。"
