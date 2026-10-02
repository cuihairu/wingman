#!/usr/bin/env bash
# Wingman Agent 一键安装脚本（Linux / macOS）
#
# 一条命令安装（匿名下载，无需登录 GitHub）：
#   curl -fsSL https://raw.githubusercontent.com/cuihairu/wingman/main/scripts/install.sh | bash
#
# 从 GitHub Releases（nightly / 正式版）下载与本机 OS + CPU 架构匹配的
# wingman-agent 单二进制产物，安装到用户 bin 目录并验证。重跑 = 覆盖升级（幂等）。
#
# 选项：
#   --prefix DIR     安装目录（默认 ~/.local/bin）
#   --version TAG    指定 release tag（默认：最新含 agent 产物的 release；
#                    正式版优先于 nightly 预发布，按 release 时间倒序取第一个命中）
#   --os OS          覆盖 OS 检测（linux / macos，测试用）
#   --arch ARCH      覆盖架构检测（x64 / arm64，测试用）
#   --service        安装后注册为常驻服务（Linux: systemd user unit；
#                    macOS: launchd LaunchAgent）
#   --token TOKEN    GitHub API token（仅用于查询 API 提升限流额度；
#                    下载走 release 资产匿名直链，无需 token）
#   -h, --help       显示帮助
#
# 环境变量：GITHUB_TOKEN / WINGMAN_INSTALL_TOKEN 可替代 --token；
#           WINGMAN_INSTALL_REPO 可覆盖仓库（默认 cuihairu/wingman）。
set -euo pipefail

REPO="${WINGMAN_INSTALL_REPO:-cuihairu/wingman}"
API_BASE="https://api.github.com/repos/${REPO}"

PREFIX="${HOME}/.local/bin"
REQUESTED_VERSION=""
OS_OVERRIDE=""
ARCH_OVERRIDE=""
TOKEN="${WINGMAN_INSTALL_TOKEN:-${GITHUB_TOKEN:-}}"
ENABLE_SERVICE=0

info() { printf '[wingman-install] %s\n' "$*"; }
die()  { printf '[wingman-install] ERROR: %s\n' "$*" >&2; exit 1; }

api_die() {
    case "${http_code:-}" in
        403) die "GitHub API returned 403 (rate limited). Anonymous quota is 60 req/hour;
retry later or pass --token <GITHUB_TOKEN>." ;;
        000) die "Failed to reach the GitHub API (network error).
Check connectivity; the installer needs https://api.github.com and
https://github.com (release asset download) to be reachable." ;;
        404) die "Not found while querying $1 — the tag does not exist in ${REPO}." ;;
        *)   die "Failed to query $1 (unexpected HTTP ${http_code:-none})." ;;
    esac
}

# 内联帮助（curl | bash 时 $0 不是文件路径，无法回读脚本头）
usage() {
    cat <<'EOF'
Usage: install.sh [options]

Options:
  --prefix DIR     Install directory (default: ~/.local/bin)
  --version TAG    Pin a release tag (default: latest release shipping an
                   agent build for this platform; stable preferred over nightly)
  --os OS          Override OS detection (linux / macos, for testing)
  --arch ARCH      Override arch detection (x64 / arm64, for testing)
  --service        Register a user service after install
                   (Linux: systemd user unit; macOS: launchd LaunchAgent)
  --token TOKEN    GitHub API token (raises API rate limit only; the binary
                   itself downloads via anonymous release asset URLs)
  -h, --help       Show this help

Environment: GITHUB_TOKEN / WINGMAN_INSTALL_TOKEN (same as --token),
             WINGMAN_INSTALL_REPO (default: cuihairu/wingman)
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --prefix)  [[ $# -ge 2 ]] || die "--prefix requires a value"; PREFIX="$2"; shift 2 ;;
        --version) [[ $# -ge 2 ]] || die "--version requires a value"; REQUESTED_VERSION="$2"; shift 2 ;;
        --os)      [[ $# -ge 2 ]] || die "--os requires a value"; OS_OVERRIDE="$2"; shift 2 ;;
        --arch)    [[ $# -ge 2 ]] || die "--arch requires a value"; ARCH_OVERRIDE="$2"; shift 2 ;;
        --service) ENABLE_SERVICE=1; shift ;;
        --token)   [[ $# -ge 2 ]] || die "--token requires a value"; TOKEN="$2"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) die "Unknown option: $1 (see https://github.com/${REPO}#一键安装-agent)" ;;
    esac
done

command -v curl >/dev/null 2>&1 || die "curl is required but not installed"
command -v tar  >/dev/null 2>&1 || die "tar is required but not installed"

# ---------- OS / 架构检测 ----------
UNAME_S="$(uname -s)"
UNAME_M="$(uname -m)"
# 报错必须引用用户实际见到的输入（override 值优先于 uname 值）
OS_INPUT="${OS_OVERRIDE:-$UNAME_S}"
ARCH_INPUT="${ARCH_OVERRIDE:-$UNAME_M}"

case "$OS_INPUT" in
    Linux|linux)              OS="linux" ;;
    Darwin|darwin|macos)      OS="macos" ;;
    MINGW*|MSYS*|CYGWIN*)
        die "Windows detected — use the PowerShell installer instead:
  irm https://raw.githubusercontent.com/${REPO}/main/scripts/install.ps1 | iex" ;;
    *) die "Unsupported OS: '${OS_INPUT}'. Supported: Linux, macOS (Windows: install.ps1)" ;;
esac

case "$ARCH_INPUT" in
    x86_64|amd64|x64)      ARCH="x64" ;;
    arm64|aarch64)         ARCH="arm64" ;;
    armv7l|armv7hl|armv8l|armv6l)
        die "Unsupported CPU architecture: '${ARCH_INPUT}'.
The agent build matrix currently covers: x64 (x86_64) and arm64 (aarch64).
armv7/armv6 builds are not produced yet — please file an issue at
https://github.com/${REPO}/issues if you need them." ;;
    *) die "Unsupported CPU architecture: '${ARCH_INPUT}'.
Supported: x86_64 (x64), aarch64/arm64 (arm64).
If you believe this architecture should be supported, file an issue at
https://github.com/${REPO}/issues." ;;
esac

ASSET_EXT="tar.gz"
ASSET_PATTERN="wingman-agent-.+-${OS}-${ARCH}\.${ASSET_EXT}"
info "Detected: OS=${OS} ARCH=${ARCH} (uname: ${UNAME_S} ${UNAME_M})"

# ---------- 解析下载地址（GitHub API，匿名可访问；下载走资产匿名直链） ----------
api_curl() {
    if [[ -n "$TOKEN" ]]; then
        curl -fsSL --retry 3 --retry-delay 2 -H "Authorization: Bearer ${TOKEN}" "$@"
    else
        curl -fsSL --retry 3 --retry-delay 2 "$@"
    fi
}

# 从 JSON 提取全部 browser_download_url（无需 jq/python，grep/cut 即可；
# 紧凑与缩进两种 JSON 形态下 URL 都落在按双引号切分的第 4 个字段）
extract_urls() {
    grep -oE '"browser_download_url": *"[^"]+"' | cut -d'"' -f4
}

if [[ -n "$REQUESTED_VERSION" ]]; then
    info "Looking up release '${REQUESTED_VERSION}' ..."
    api_json="$(mktemp)"
    http_code="$(curl -sSL --retry 3 --retry-delay 2 -o "$api_json" -w '%{http_code}' \
        ${TOKEN:+-H "Authorization: Bearer ${TOKEN}"} \
        "${API_BASE}/releases/tags/${REQUESTED_VERSION}")" || true
    [[ "$http_code" == "200" ]] || api_die "release '${REQUESTED_VERSION}' (HTTP ${http_code})"
    DOWNLOAD_URL="$(extract_urls < "$api_json" | grep -E "/${ASSET_PATTERN}$" | head -1 || true)"
    rm -f "$api_json"
    [[ -n "$DOWNLOAD_URL" ]] || die "Release '${REQUESTED_VERSION}' has no asset matching ${ASSET_PATTERN}.
Check the assets at https://github.com/${REPO}/releases/tag/${REQUESTED_VERSION}"
else
    info "Looking up the latest release with an agent build for ${OS}-${ARCH} ..."
    api_json="$(mktemp)"
    http_code="$(curl -sSL --retry 3 --retry-delay 2 -o "$api_json" -w '%{http_code}' \
        ${TOKEN:+-H "Authorization: Bearer ${TOKEN}"} \
        "${API_BASE}/releases?per_page=30")" || true
    [[ "$http_code" == "200" ]] || api_die "the release list (HTTP ${http_code})"
    # release 列表按时间倒序：取第一个含本平台 agent 资产的 release
    # （正式版在前；正式版尚无 agent 产物时自然落到 nightly 预发布）
    DOWNLOAD_URL="$(extract_urls < "$api_json" | grep -E "/${ASSET_PATTERN}$" | head -1 || true)"
    rm -f "$api_json"
    [[ -n "$DOWNLOAD_URL" ]] || die "No release in ${REPO} provides wingman-agent for ${OS}-${ARCH}.
The build matrix covers: linux/macos/windows × x64/arm64. If this platform
should be supported, file an issue at https://github.com/${REPO}/issues."
fi

TAG="$(printf '%s' "$DOWNLOAD_URL" | sed -E 's#^.*/releases/download/([^/]+)/.*$#\1#')"
ASSET_NAME="$(printf '%s' "$DOWNLOAD_URL" | sed -E 's#^.*/##')"
info "Selected: ${TAG} → ${ASSET_NAME}"

# ---------- 下载（release 资产匿名直链） ----------
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

info "Downloading ${DOWNLOAD_URL}"
curl -fL --retry 3 --retry-delay 2 --progress-bar -o "${TMP_DIR}/asset.${ASSET_EXT}" "$DOWNLOAD_URL" \
    || die "Download failed: ${DOWNLOAD_URL}"

# ---------- 解包 ----------
mkdir -p "${TMP_DIR}/extract"
tar -xzf "${TMP_DIR}/asset.${ASSET_EXT}" -C "${TMP_DIR}/extract" \
    || die "Failed to extract ${ASSET_NAME}"
BIN_SRC="$(find "${TMP_DIR}/extract" -type f -name 'wingman-agent' | head -1)"
[[ -n "$BIN_SRC" ]] || die "No 'wingman-agent' binary found inside ${ASSET_NAME} (unexpected package layout)"

# ---------- 安装（重跑 = 覆盖升级） ----------
mkdir -p "$PREFIX"
[[ -w "$PREFIX" ]] || die "Install directory is not writable: ${PREFIX}
Re-run with --prefix pointing to a writable directory, e.g.:
  curl -fsSL .../install.sh | bash -s -- --prefix \$HOME/.local/bin"

OLD_VERSION=""
if [[ -x "${PREFIX}/wingman-agent" ]]; then
    OLD_VERSION="$("${PREFIX}/wingman-agent" --version 2>/dev/null | head -1 || true)"
fi

# 先 rm 再 install：目标正被运行中的 agent 占用时 Linux 会对覆盖写入报
# ETXTBSY；--service 场景服务已在下方重启逻辑中先停（此处置零风险兜底）
rm -f "${PREFIX}/wingman-agent"
install -m 0755 "$BIN_SRC" "${PREFIX}/wingman-agent" \
    || die "Failed to install binary to ${PREFIX}/wingman-agent"

# macOS：清除可能残留的 quarantine 属性（curl 下载一般不带，幂等防御）
if [[ "$OS" == "macos" ]] && command -v xattr >/dev/null 2>&1; then
    xattr -c "${PREFIX}/wingman-agent" >/dev/null 2>&1 || true
fi

# ---------- 验证 ----------
NEW_VERSION="$("${PREFIX}/wingman-agent" --version 2>/dev/null | head -1 || true)"
if [[ -z "$NEW_VERSION" ]]; then
    ERR_MSG="Installed binary at ${PREFIX}/wingman-agent failed to run (--version produced no output)."
    if [[ "$OS" == "linux" ]] && command -v ldd >/dev/null 2>&1; then
        MISSING="$(ldd "${PREFIX}/wingman-agent" 2>/dev/null | grep 'not found' || true)"
        if [[ -n "$MISSING" ]]; then
            ERR_MSG="${ERR_MSG}
Missing shared libraries:
${MISSING}
Install the corresponding system packages (e.g. libX11) and re-run the installer."
        fi
    fi
    die "$ERR_MSG"
fi

if [[ -n "$OLD_VERSION" && "$OLD_VERSION" != "$NEW_VERSION" ]]; then
    info "Upgraded: ${OLD_VERSION} → ${NEW_VERSION}"
else
    info "Installed: ${NEW_VERSION}"
fi
info "Location: ${PREFIX}/wingman-agent"

# PATH 提示（不代改 rc 文件）
case ":${PATH}:" in
    *":${PREFIX}:"*) ;;
    *)
        info "NOTE: ${PREFIX} is not in your PATH. Add it with:"
        info "  export PATH=\"${PREFIX}:\$PATH\"   # add to ~/.bashrc or ~/.zshrc" ;;
esac

# ---------- 可选：注册常驻服务 ----------
register_service() {
    local bin="${PREFIX}/wingman-agent"
    if [[ "$OS" == "linux" ]]; then
        command -v systemctl >/dev/null 2>&1 || die "--service requires systemd (systemctl not found)"
        systemctl --user daemon-reload >/dev/null 2>&1 \
            || die "systemd user session is not available (systemctl --user failed).
On a headless/SSH host: loginctl enable-linger \$USER, or run as a system service instead."
        mkdir -p "${XDG_CONFIG_HOME:-$HOME/.config}/systemd/user"
        cat > "${XDG_CONFIG_HOME:-$HOME/.config}/systemd/user/wingman-agent.service" <<EOF
[Unit]
Description=Wingman Agent
After=network-online.target

[Service]
ExecStart=${bin} start
Restart=on-failure
RestartSec=5

[Install]
WantedBy=default.target
EOF
        # 幂等：先停旧实例（未注册时忽略）
        systemctl --user disable --now wingman-agent.service >/dev/null 2>&1 || true
        systemctl --user daemon-reload
        systemctl --user enable --now wingman-agent.service
        info "Service registered (systemd user): wingman-agent.service"
        info "  logs:   journalctl --user -u wingman-agent -f"
        info "  stop:   systemctl --user stop wingman-agent"
    else
        local plist="${HOME}/Library/LaunchAgents/com.wingman.agent.plist"
        mkdir -p "${HOME}/Library/LaunchAgents"
        launchctl bootout "gui/$(id -u)/com.wingman.agent" >/dev/null 2>&1 \
            || launchctl unload "$plist" >/dev/null 2>&1 || true
        cat > "$plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>com.wingman.agent</string>
    <key>ProgramArguments</key>
    <array>
        <string>${bin}</string>
        <string>start</string>
    </array>
    <key>RunAtLoad</key>
    <true/>
    <key>KeepAlive</key>
    <true/>
    <key>StandardOutPath</key>
    <string>${HOME}/Library/Logs/wingman-agent.log</string>
    <key>StandardErrorPath</key>
    <string>${HOME}/Library/Logs/wingman-agent.log</string>
</dict>
</plist>
EOF
        launchctl bootstrap "gui/$(id -u)" "$plist" 2>/dev/null \
            || launchctl load -w "$plist" \
            || die "Failed to register LaunchAgent (${plist})"
        info "Service registered (launchd): com.wingman.agent"
        info "  logs: tail -f ${HOME}/Library/Logs/wingman-agent.log"
        info "  stop: launchctl bootout gui/\$(id -u)/com.wingman.agent"
    fi
}

if [[ "$ENABLE_SERVICE" -eq 1 ]]; then
    register_service
fi

info "Done."
