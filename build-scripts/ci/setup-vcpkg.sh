#!/usr/bin/env bash
set -euo pipefail

vcpkg_root="${VCPKG_ROOT:-$PWD/vcpkg}"

rm -rf "$vcpkg_root"
git clone https://github.com/Microsoft/vcpkg.git "$vcpkg_root"
"$vcpkg_root/bootstrap-vcpkg.sh"
