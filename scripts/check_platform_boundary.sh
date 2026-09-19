#!/usr/bin/env bash
# 平台边界守卫（thin-layer discipline，P0）
#
# 规则：平台相关宏（_WIN32/__APPLE__/__linux__/__ANDROID__ 等）只允许出现在
#       实现层 lib/wingman/src/platform/ 中；公共层（含接口层
#       include/wingman/platform/、libs/*、apps/* 生产代码）必须为零平台宏。
#       迁移期历史欠账见 scripts/platform_boundary_allowlist.txt（只减不增）。
# 规范文档：docs/platform-abstraction-design.md §8「薄层纪律与平台边界守卫」
#
# 用法：scripts/check_platform_boundary.sh    # 无依赖，CI ubuntu 直接跑
#       扫描范围是生产代码；tests/ 目录不扫描（测试可自由使用平台宏）。
set -euo pipefail
cd "$(dirname "$0")/.."

# 条件预处理指令中出现平台标识即算命中（#ifdef/#ifndef/#if/#elif）
PATTERN='^[[:space:]]*#[[:space:]]*(ifdef|ifndef|if|elif)[[:space:](].*(_WIN32|_WIN64|WIN32|__APPLE__|__ANDROID__|__linux__|_MSC_VER|LINUX_PLATFORM)'

ALLOWLIST="scripts/platform_boundary_allowlist.txt"

# 扫描范围：C++ 生产代码；排除平台实现层与所有 tests 目录
mapfile -t files < <(git ls-files -- \
	'*.cpp' '*.cc' '*.cxx' '*.c' '*.hpp' '*.hh' '*.h' '*.mm' '*.m' \
	| grep -E '^(lib/wingman/src|lib/wingman/include|libs/|apps/)' \
	| grep -v '^lib/wingman/src/platform/' \
	| grep -vE '(^|/)tests?/')

hits=$(mktemp)
trap 'rm -f "$hits" "$hits.viol"' EXIT
: > "$hits"
for f in "${files[@]}"; do
	grep -nE "$PATTERN" -- "$f" 2>/dev/null | sed "s|^|$f:|" >> "$hits" || true
done

# 与 allowlist 对账：清单中的文件属于迁移期欠账，其余视为违规
awk -F: '
	NR == FNR { if ($0 != "") allowed[$1] = 1; next }
	!($1 in allowed) { print }
' "$ALLOWLIST" "$hits" > "$hits.viol" 2>/dev/null || cp "$hits" "$hits.viol"

if [ -s "$hits.viol" ]; then
	echo "✗ 平台边界违规：公共层出现平台宏，且不在迁移 allowlist 中"
	echo
	cat "$hits.viol"
	echo
	echo "处理方式：实现移入 lib/wingman/src/platform/<os>/（薄层），"
	echo "或经维护者批准后加入 scripts/platform_boundary_allowlist.txt（只减不增）。"
	exit 1
fi

echo "✓ 平台边界检查通过：扫描 ${#files[@]} 个文件，$(wc -l < "$hits") 处平台宏全部位于 src/platform/ 或迁移 allowlist 中。"
