#!/usr/bin/env bash
set -euo pipefail

vcpkg_root="${VCPKG_ROOT:-$PWD/vcpkg}"

if [ -f "$vcpkg_root/vcpkg" ]; then
    echo "vcpkg already installed at $vcpkg_root, skipping clone/bootstrap."
    exit 0
fi

rm -rf "$vcpkg_root"
git clone https://github.com/Microsoft/vcpkg.git "$vcpkg_root"
"$vcpkg_root/bootstrap-vcpkg.sh"
