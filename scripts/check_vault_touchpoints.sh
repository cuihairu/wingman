#!/usr/bin/env bash
# 密钥保险箱接入点扫描（todo.md 2026-10-11「密码箱抽象」验收② 的可重跑口径）：
# server 业务代码不得直读 env/配置里的明文凭据——一律经 internal/vault 的
# Provider。白名单（自举层）之外的 os.Getenv 凭据形直读即报错退出 1。
#
# 自举层白名单（先于保险箱存在，结构性不可改走保险箱，论证见
# docs/vault-provider-design.md §5）：
#   WINGMAN_JWT_SECRET      JWT 签名密钥（保险箱自身鉴权的前提）
#   WINGMAN_AGENT_TOKENS    agent 注册白名单（连接鉴权先于保险箱）
#   WINGMAN_ADMIN_PASSWORD  首个管理员引导（尚无用户，无保险箱可查）
#
# bash 3.2 兼容（macOS CI）：仅用 POSIX 构造，不用关联数组/映射。
set -euo pipefail
cd "$(dirname "$0")/.."

hits=$(grep -rn --include='*.go' -E 'os\.Getenv|getenv\(' orchestrator/server |
  grep -v '_test.go' |
  grep -iE 'SECRET|TOKEN|PASSWORD|PASSWD|API_?KEY|CREDENTIAL|WEBHOOK|SMTP' |
  grep -v -e 'WINGMAN_JWT_SECRET' -e 'WINGMAN_AGENT_TOKENS' -e 'WINGMAN_ADMIN_PASSWORD' ||
  true)

if [ -n "$hits" ]; then
  echo "vault touchpoint scan: plaintext credential env reads outside the boot-tier allowlist:"
  echo "$hits"
  echo ""
  echo "new credential surfaces must go through internal/vault Provider,"
  echo "or be added to the boot-tier allowlist with justification in"
  echo "docs/vault-provider-design.md"
  exit 1
fi

echo "vault touchpoint scan: clean (boot-tier allowlist only)"
