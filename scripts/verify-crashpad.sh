#!/usr/bin/env bash
# Crashpad 崩溃采集验收（设计简档 docs/design/crash-reporting-design.md §9 的固化入口）
#
# 覆盖：crash-test 命令 → 故意空指针崩溃 → dump 落盘 → 符号化还原调用栈
#       （断言栈内出现 testCrashNullPointer 帧）。
#
# 符号化工具（开发机本机安装，不入仓不入 CI）：dump_syms / minidump-stackwalk
#       （rust 实现：cargo install dump_syms minidump-stackwalk；crashpad 官方
#       无对应符号工具，breakpad 生态同源 minidump 格式，见设计简档 §6）。
#
# 未构建 crashpad（WINGMAN_ENABLE_CRASHPAD=OFF）或缺符号化工具时判
# 「未验证」（exit 2）而非通过，防止环境缺件误报绿。
#
# 用法：scripts/verify-crashpad.sh [--build]
#       --build  强制重新构建 wingman-runtime（默认缺二进制时才构建）
set -euo pipefail
cd "$(dirname "$0")/.."

if [[ "$(uname -s)" != "Linux" ]]; then
	echo "SKIP: 本脚本仅用于 Linux（当前 $(uname -s)）"; exit 2
fi

RUNTIME=build/apps/runtime/wingman-runtime
HANDLER=build/apps/runtime/crashpad_handler
DUMP_SYMS="${DUMP_SYMS:-$HOME/.cargo/bin/dump_syms}"
STACKWALK="${STACKWALK:-$HOME/.cargo/bin/minidump-stackwalk}"

if [[ ! -x "$DUMP_SYMS" || ! -x "$STACKWALK" ]]; then
	echo "SKIP: 缺符号化工具（$DUMP_SYMS / $STACKWALK）。"
	echo "      开发机安装：cargo install dump_syms minidump-stackwalk"; exit 2
fi

if [[ ! -x "$RUNTIME" || "${1:-}" == "--build" ]]; then
	echo "==> 构建 wingman-runtime（首次或 --build；WINGMAN_ENABLE_CRASHPAD 默认 Linux 开）"
	cmake -B build >/dev/null
	cmake --build build --target wingman-runtime -j"$(nproc)"
fi
if [[ ! -x "$HANDLER" ]]; then
	echo "SKIP: crashpad_handler 未随产物生成——本构建未启用 WINGMAN_ENABLE_CRASHPAD"; exit 2
fi

# 隔离的数据目录：验收不读写真实 ~/.local/share/wingman
XDG_DATA_HOME=$(mktemp -d /tmp/wingman-crashpad-verify.XXXXXX)
SYMROOT=$(mktemp -d /tmp/wingman-crashpad-syms.XXXXXX)
LOG="$XDG_DATA_HOME/crash-test.log"
trap 'rm -rf "$XDG_DATA_HOME" "$SYMROOT"' EXIT
export XDG_DATA_HOME

echo "==> 触发 crash-test（故意空指针，进程异常退出为预期）"
set +e
"$RUNTIME" crash-test > "$LOG" 2>&1
RC=$?
set -e
grep -E 'Crash reporting|crash-test' "$LOG" || true
if [[ $RC -eq 0 ]]; then
	echo "FAIL: crash-test 正常退出（exit=0），未触发崩溃"; exit 1
fi
if ! grep -q 'Crash reporting enabled' "$LOG"; then
	echo "FAIL: crashpad 初始化未成功（日志缺 enabled 行），完整日志："; cat "$LOG"; exit 1
fi

CRASH_DIR="$XDG_DATA_HOME/wingman/crashes"
# fork 数据库布局：new/（写入中）→ pending/（完整落盘，无上传时最终态）→
# completed/（上传后）；完整 dump 按非空文件判定，空壳文件不算
DUMP=$(find "$CRASH_DIR" -name '*.dmp' -size +1k 2>/dev/null | head -1 || true)
if [[ -z "$DUMP" ]]; then
	echo "FAIL: 崩溃后未见完整 dump 落盘（$CRASH_DIR 下无非空 .dmp）"; exit 1
fi
echo "==> dump 落盘：$DUMP（$(stat -c%s "$DUMP") bytes，exit=$RC）"

echo "==> 符号化（dump_syms → minidump-stackwalk）"
"$DUMP_SYMS" --store "$SYMROOT" "$RUNTIME" >/dev/null
"$STACKWALK" --human "$DUMP" "$SYMROOT" > "$SYMROOT/stackwalk.txt" 2>/dev/null
sed -n '/^Crash reason\|^Crash address/,+0p' "$SYMROOT/stackwalk.txt" || true

if grep -q 'testCrashNullPointer' "$SYMROOT/stackwalk.txt"; then
	echo "PASS: dump 落盘 + 符号化还原出 testCrashNullPointer 调用栈。"
else
	echo "FAIL: 符号化栈中未见 testCrashNullPointer 帧，完整输出："
	sed -n '1,40p' "$SYMROOT/stackwalk.txt"; exit 1
fi
