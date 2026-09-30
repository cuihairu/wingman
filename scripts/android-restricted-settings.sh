#!/usr/bin/env bash
# Android 13+ 受限设置（ACCESS_RESTRICTED_SETTINGS）预授权工具（A3 部署体验）。
#
# 背景：Android 13（API 33）起，侧载（非应用商店安装）App 的无障碍等服务被
#       「受限设置」默认屏蔽——用户去系统设置打不开开关，是端侧 Agent 最高
#       的开箱失败来源（mobile-support-feasibility.md §5.2，风险表评级：高）。
#       本脚本把三档解法中的「adb 预授权」做成幂等命令；手动允许与 Device
#       Owner 批量部署见 docs/guides/android-restricted-settings.md。
#
# 边界（设计决策 mobile-automation-design.md D7）：adb 只出现在部署/一次性
#       授权路径——本脚本与文档即是全部，不出现在任何运行时链路里。
#
# 三态约定（同 verify-*.sh）：PASS=0 / FAIL=1 / SKIP(未验证)=2。
#       「没跑成」（无 adb / 无设备）判 SKIP，绝不与「检查不通过」混同。
#
# 用法：
#   scripts/android-restricted-settings.sh check    # 检查预授权状态（默认）
#   scripts/android-restricted-settings.sh allow    # 幂等预授权（allow 后复核）
#   scripts/android-restricted-settings.sh revoke   # 还原为 default
#   scripts/android-restricted-settings.sh status   # 只打印当前状态
# 环境变量：
#   WINGMAN_ANDROID_PKG  目标包名（默认 com.wingman.agent）
#   ANDROID_SERIAL       多设备选串号（adb 原生识别，本脚本无需处理）
#
# 手动允许（单台）与 Device Owner（批量纳管）步骤见手册：
#   docs/guides/android-restricted-settings.md
set -euo pipefail

PKG="${WINGMAN_ANDROID_PKG:-com.wingman.agent}"

# usage 打印用法。显式求助（-h/--help/help）算成功（exit 0），
# 用法错误（未知子命令/多余参数）算 exit 2——与三态的 SKIP(2) 区分：
# 这里是「用法错」，不是「验证没跑成」。
usage() {
	local code="${1:-2}"
	sed -n '2,26p' "$0" | sed 's/^# \{0,1\}//'
	exit "$code"
}

# 引擎探测：只认 command -v（本脚本对 adb 没有子命令级探测需求，
# get-state 的设备态探测在 require_device 里做）。
ADB_BIN=""
if command -v adb >/dev/null 2>&1; then
	ADB_BIN="$(command -v adb)"
fi

skip_no_adb() {
	echo "SKIP: 未找到 adb——本脚本只做部署期一次性预授权（不进运行时）"
	echo "      装 platform-tools 后重试；无 adb 时按手册手动允许：docs/guides/android-restricted-settings.md"
	exit 2
}

require_adb() {
	if [[ -z "$ADB_BIN" ]]; then
		skip_no_adb
	fi
}

# device_state 输出 adb get-state 结果（失败输出空）。
device_state() {
	"$ADB_BIN" get-state 2>/dev/null || true
}

# require_device：无已授权设备属「环境未就绪」→ SKIP(2)，不判 FAIL——
# 「没跑成」既不算通过也不算不通过（同 verify-guacd-e2e.sh 的引擎缺位口径）。
require_device() {
	require_adb
	if [[ "$(device_state)" != "device" ]]; then
		echo "SKIP: adb 已装但无已授权设备（adb devices 查看；真机需开 USB 调试并授权弹窗，模拟器需先启动）"
		exit 2
	fi
}

# device_sdk 输出 API level（读不到输出 0，不作为 SKIP 依据——prop 读取
# 失败时 appops 结果仍然有效，继续往下查）。
device_sdk() {
	local sdk
	sdk="$("$ADB_BIN" shell getprop ro.build.version.sdk 2>/dev/null | tr -d '\r' | grep -E '^[0-9]+$' || true)"
	echo "${sdk:-0}"
}

# require_installed：包未装是部署前提不满足 → FAIL(1)（可行动：先装 APK）。
require_installed() {
	local out
	if ! out="$("$ADB_BIN" shell pm path "$PKG" 2>/dev/null)" || [[ "$out" != *"package:"* ]]; then
		echo "FAIL: 设备上未安装 ${PKG}（先安装 APK 再预授权；批量部署路径见 docs/guides/android-restricted-settings.md）"
		return 1
	fi
}

# appops_value 输出 ACCESS_RESTRICTED_SETTINGS 当前值（allow/default/ignore；
# 读不到输出空串，按「未授权」处理）。tr 去 \r：部分宿主机的 adb shell 输出
# 带 CRLF 行尾。取值用参数展开而非 sed——避免行前缀解析问题（见 e8dc57c）。
appops_value() {
	local raw
	raw="$("$ADB_BIN" shell appops get "$PKG" ACCESS_RESTRICTED_SETTINGS 2>/dev/null |
		tr -d '\r' | grep 'ACCESS_RESTRICTED_SETTINGS' | head -n 1 || true)"
	echo "${raw##*: }"
}

set_appops() {
	"$ADB_BIN" shell appops set "$PKG" ACCESS_RESTRICTED_SETTINGS "$1"
}

do_check() {
	require_device
	local sdk
	sdk="$(device_sdk)"
	if ((sdk > 0 && sdk < 33)); then
		echo "PASS: 设备 API $sdk < 33（Android 12-），不受限设置约束"
		return 0
	fi
	require_installed || return 1
	local value
	value="$(appops_value)"
	if [[ "$value" == "allow" ]]; then
		echo "PASS: $PKG 的 ACCESS_RESTRICTED_SETTINGS 已预授权（allow）——无障碍等受限服务可在设置中直接开启"
		return 0
	fi
	echo "FAIL: $PKG 未预授权（当前 ${value:-default}）——Android 13+ 侧载默认屏蔽受限设置，无障碍开关会打不开"
	echo "      修复：scripts/android-restricted-settings.sh allow；或手动允许（应用信息 → ⋮ → 允许受限制的设置）"
	return 1
}

do_allow() {
	require_device
	require_installed || return 1
	set_appops allow
	local value
	value="$(appops_value)"
	if [[ "$value" == "allow" ]]; then
		echo "PASS: 已预授权并复核通过（幂等，重复执行安全）：appops set $PKG ACCESS_RESTRICTED_SETTINGS allow"
		return 0
	fi
	echo "FAIL: 预授权后复核仍为 ${value:-default}——部分 ROM 不支持该 appop，改走手动允许（docs/guides/android-restricted-settings.md）"
	return 1
}

do_revoke() {
	require_device
	require_installed || return 1
	set_appops default
	echo "PASS: 已还原 default（appops set $PKG ACCESS_RESTRICTED_SETTINGS default）"
	return 0
}

do_status() {
	require_device
	require_installed || return 1
	local sdk value
	sdk="$(device_sdk)"
	value="$(appops_value)"
	echo "包名     : $PKG"
	echo "API level: ${sdk:-未知}"
	echo "受限设置 : ${value:-未知（appops 无输出）}"
	if ((sdk > 0 && sdk < 33)); then
		echo "说明     : API < 33，不受限设置约束"
	fi
	return 0
}

main() {
	local action="${1:-check}"
	if [[ "$#" -gt 1 ]]; then
		usage
	fi
	case "$action" in
		check) do_check ;;
		allow) do_allow ;;
		revoke) do_revoke ;;
		status) do_status ;;
		-h | --help | help) usage 0 ;;
		*)
			echo "未知子命令：$action"
			usage
			;;
	esac
}

main "$@"
