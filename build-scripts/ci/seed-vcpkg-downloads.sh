#!/usr/bin/env bash
set -euo pipefail

# 预置 vcpkg 源码包下载缓存（sourceforge 单点旁路）。
#
# 背景（2026-10-01 CI 红）：cpp-linux-python-tests 的 vcpkg 依赖图中
# python3 → libuuid 走 vcpkg_from_sourceforge。sourceforge 事故窗口内
# 源站 522、~20 个镜像全部返回坏内容（unexpected hash），configure 的
# 3 次重试全灭——job 恒红。仓库无 ExternalProject/FetchContent；哈希
# 钉死的对应机制是 vcpkg port 的 SHA512（vcpkg_from_sourceforge 内置，
# 随 builtin-baseline 固定），本脚本补的是「预缓存」腿：把 vendor 的
# tarball 放进 vcpkg downloads 目录。vcpkg 下载前先查本地文件并按
# portfile 的 SHA512 复验，命中即零网络。
# （本地实证：vcpkg install libuuid --no-downloads 禁网全链通过。）
#
# 供应链口径：manifest 哈希与 vcpkg baseline（vcpkg.json builtin-baseline）
# 对应 portfile 的 SHA512 保持一致，vcpkg 安装时会独立复验一次——本脚本
# 校验失败或 vendor 内容被篡改都会在进入构建前被拦下。baseline 升级若
# 改动端口哈希：更新 vcpkg-downloads/ 里的文件与 manifest（从新 portfile
# 取 SHA512 钉值）。新增脆弱源（其它 vcpkg_from_sourceforge 依赖）时：
# 文件放进 vcpkg-downloads/ 并 `sha512sum <file> >> sha512.manifest`。

seed_dir="$(cd "$(dirname "$0")" && pwd)/vcpkg-downloads"
vcpkg_root="${VCPKG_ROOT:-$PWD/vcpkg}"
downloads_dir="$vcpkg_root/downloads"

manifest="$seed_dir/sha512.manifest"
if [ ! -f "$manifest" ]; then
    echo "error: seed manifest not found: $manifest" >&2
    exit 1
fi

# 先验后拷：vendor 内容与清单不符直接失败，未验证字节不进缓存
(cd "$seed_dir" && sha512sum --check --strict sha512.manifest)

mkdir -p "$downloads_dir"
copied=0
shopt -s nullglob
for entry in "$seed_dir"/*; do
    base="$(basename "$entry")"
    if [ "$base" = "sha512.manifest" ]; then
        continue
    fi
    cp -f "$entry" "$downloads_dir/$base"
    echo "seeded: $downloads_dir/$base"
    copied=$((copied + 1))
done
shopt -u nullglob

echo "seeded $copied file(s) into $downloads_dir (sourceforge SPOF bypass)"
