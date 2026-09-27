#!/usr/bin/env bash
# Guacamole 三协议 e2e 容器栈的一键拉起/验证/拆除。
#
# 覆盖：integration/testdata/guacd-e2e-compose.yml 的四个服务（guacd /
#       sshd / vnc / xrdp）+ guacd_e2e_test.go 的三协议链路用例。
#
# 为什么要有这个脚本（原来只有一段 README 里的手抄命令）：
#   1. `compose up -d` 在容器**尚未监听端口**时就返回，随即跑用例必然连不上
#      ——xrdp 首启要几十秒。原文档没提「等就绪」，于是「栈没起好」和
#      「代码有 bug」两种失败长得一模一样，只能靠人肉重试区分。
#   2. 四个目标端点分布在四个镜像上，任何一个没起来都只表现为某条协议红。
#      脚本把「谁没就绪」显式报出来。
#   3. 三态自检：PASS / FAIL / SKIP(未验证)——沿用 verify-xrecord-desktop.sh
#      与 verify-macos-runtime.sh 的约定，绝不把「没跑成」判成通过。
#
# 用法：
#   scripts/verify-guacd-e2e.sh up       # 拉起栈并等全部就绪
#   scripts/verify-guacd-e2e.sh test     # 跑三协议 e2e（需栈已就绪）
#   scripts/verify-guacd-e2e.sh run      # up + test + 自动 down（一条龙）
#   scripts/verify-guacd-e2e.sh down     # 拆除栈
#   scripts/verify-guacd-e2e.sh status   # 只看就绪状态
#   scripts/verify-guacd-e2e.sh --keep    # run 之后保留栈（便于手工复现）
set -euo pipefail
cd "$(dirname "$0")/.."

COMPOSE_FILE=orchestrator/server/integration/testdata/guacd-e2e-compose.yml

# 引擎探测：既要「装了这个命令」，也要「它支持 compose 子命令」。
# 只判 command -v 是不够的——装了 docker 但没装 compose v2 插件（或
# podman 是残缺安装）时，脚本会在 compose 那一步炸出一串无关的
# 「unknown command」把人带偏。宁可现在就报 SKIP 并说清缺什么。
ENGINE=""
for candidate in podman docker; do
	if ! command -v "$candidate" >/dev/null 2>&1; then
		continue
	fi
	if "$candidate" compose version >/dev/null 2>&1; then
		ENGINE="$candidate"
		break
	fi
	# 装了该命令但不支持 compose：记下来，探测结束时给准确提示
	ENGINE_MISSING_COMPOSE="$candidate"
done

# 三协议 e2e 用例名（WINGMAN_GUACD_E2E 门控，见 guacd_e2e_test.go）
E2E_TESTS='TestGuacdE2ESSHLink|TestGuacdE2EVNCLink|TestGuacdE2ERDPLink'

# usage 打印用法。显式求助（-h/--help/help）算成功（exit 0），
# 用法错误（未知子命令/未知 flag）算 exit 2——与其它 verify-*.sh 的
# SKIP(2)/FAIL(1) 三态区分开：这里是「用法错」，不是「验证没跑成」。
usage() {
	local code="${1:-2}"
	sed -n '2,25p' "$0" | sed 's/^# \{0,1\}//'
	exit "$code"
}

# ---------- 就绪探测 ----------
# 端口可达性 = 「容器已监听」；不等于「协议握手可用」——后者由 e2e 用例断言。
# 分开是因为二者失败含义不同：端口不通是栈问题，通了不 ready 是链路问题。
# 就绪目标用两条平行数组，不用 declare -A 关联数组：macOS 自带 bash 3.2 不支持
# 后者，`[guacd]=4822` 会被解释成给未变量 $guacd 赋值，在 set -u 下当场崩
# （CI 实测 "verify-guacd-e2e.sh: line 60: guacd: unbound variable"）。崩溃发生在
# 引擎探测之后、任何 SKIP 分支之前，于是「环境不具备 → SKIP(2)」被吞成 exit 1，
# 与本脚本立意相反——绝不把「没跑成」和「有 bug」混成同一种表现。仓库其余
# verify-*.sh 也一律不用关联数组，这里是唯一的例外，现已改齐。
READY_NAMES=(guacd sshd vnc xrdp)
READY_PORTS=(4822 2222 5901 3389)

# ready_port NAME 按名字查端口（数组下标对齐）。
ready_port() {
	local i
	for i in "${!READY_NAMES[@]}"; do
		if [[ "${READY_NAMES[$i]}" == "$1" ]]; then
			echo "${READY_PORTS[$i]}"
			return 0
		fi
	done
	return 1
}

# 冷启动预算：xrdp 最慢（X session 冷启实测数十秒）
READY_TIMEOUT="${GUACD_E2E_READY_TIMEOUT:-180}"

port_open() {
	local port="$1"
	# 优先 bash 内建 /dev/tcp；退化到 nc；都没有就退出 2 让上层报「缺工具」
	if (exec 3<>"/dev/tcp/127.0.0.1/$port") 2>/dev/null; then
		return 0
	fi
	if command -v nc >/dev/null 2>&1; then
		nc -z -w 2 127.0.0.1 "$port" >/dev/null 2>&1
		return $?
	fi
	return 2
}

require_engine() {
	if [[ -n "$ENGINE" ]]; then
		return 0
	fi
	if [[ -n "${ENGINE_MISSING_COMPOSE:-}" ]]; then
		echo "SKIP: 找到 $ENGINE_MISSING_COMPOSE，但它不支持 'compose' 子命令"
		echo "      装插件后重试：docker compose version   # 或改用 podman compose"
		echo "      纯逻辑验证请直接跑：cd orchestrator/server && go test ./integration/ -run 'TestGuacE2E' -v"
		exit 2
	fi
	echo "SKIP: 未找到 podman/docker——本脚本需要容器引擎来拉起 guacd 与三协议目标端点"
	echo "      纯逻辑验证请直接跑：cd orchestrator/server && go test ./integration/ -run 'TestGuacE2E' -v"
	exit 2
}

compose() {
	"$ENGINE" compose -f "$COMPOSE_FILE" "$@"
}

# wait_ready 等四个端口全部可达；逐个报缺哪个。
# 返回 1 = 超时（有端口不通），返回 2 = 缺探测工具（环境问题，不判失败）。
wait_ready() {
	local deadline=$((SECONDS + READY_TIMEOUT))
	local pending=("${READY_NAMES[@]}")
	while ((SECONDS < deadline)); do
		local still=()
		for name in "${pending[@]}"; do
			local port
			port="$(ready_port "$name")"
			port_open "$port"
			case $? in
				0) : ;;
				2)
					echo "SKIP: 缺端口探测工具（bash /dev/tcp 或 nc 都没有）"
					return 2
					;;
				*) still+=("$name") ;;
			esac
		done
		if ((${#still[@]} == 0)); then
			echo "READY: guacd/sshd/vnc/xrdp 四端口全部可达（${READY_TIMEOUT}s 内）"
			return 0
		fi
		pending=("${still[@]}")
		sleep 2
	done
	echo "FAIL: ${READY_TIMEOUT}s 内以下端口未就绪："
	for name in "${pending[@]}"; do
		echo "  - $name (127.0.0.1:$(ready_port "$name"))"
	done
	echo "  排查：$ENGINE compose -f $COMPOSE_FILE logs --tail 50"
	return 1
}

do_up() {
	require_engine
	echo "==> 拉起容器栈（$ENGINE）"
	compose up -d
	echo "==> 等待端口就绪（预算 ${READY_TIMEOUT}s）"
	if ! wait_ready; then
		return 1
	fi
	# 端口可达后再看容器健康状态：端口通但 healthcheck 挂红要显式报出来
	local unhealthy
	unhealthy=$(compose ps --format '{{.Name}} {{.Health}}' 2>/dev/null |
		awk '$2 != "healthy" && $2 != "" {print $1" ("$2")"}' || true)
	if [[ -n "$unhealthy" ]]; then
		echo "WARN: 端口已通但以下容器健康检查未过（多为探针在镜像内不可用，可忽略）："
		echo "$unhealthy" | sed 's/^/  - /'
	fi
}

do_down() {
	require_engine
	echo "==> 拆除容器栈"
	compose down --remove-orphans
}

do_status() {
	require_engine
	local rc=0
	for name in "${READY_NAMES[@]}"; do
		local port
		port="$(ready_port "$name")"
		if port_open "$port"; then
			printf '  %-6s :%s  READY\n' "$name" "$port"
		else
			printf '  %-6s :%s  NOT READY\n' "$name" "$port"
			rc=1
		fi
	done
	return $rc
}

do_test() {
	require_engine
	echo "==> 跑三协议链路用例（WINGMAN_GUACD_E2E=1）"
	if ! wait_ready; then
		return 1
	fi
	cd orchestrator/server
	set +e
	WINGMAN_GUACD_E2E=1 go test ./integration/ -run "$E2E_TESTS" -v -timeout 600s
	local rc=$?
	set -e
	cd - >/dev/null
	return $rc
}

main() {
	local action="${1:-run}"
	local keep=0
	shift || true
	for arg in "$@"; do
		case "$arg" in
			--keep) keep=1 ;;
			*) usage ;;
		esac
	done

	case "$action" in
		up)
			do_up
			;;
		down)
			do_down
			;;
		status)
			do_status
			;;
		test)
			do_test
			;;
		run)
			do_up || exit 1
			local test_rc=0
			do_test || test_rc=$?
			if ((keep == 0)); then
				do_down || true
			else
				echo "--keep: 保留容器栈（拆除：scripts/verify-guacd-e2e.sh down）"
			fi
			exit $test_rc
			;;
		-h | --help | help) usage 0 ;;
		*)
			echo "未知子命令：$action"
			usage
			;;
	esac
}

main "$@"
