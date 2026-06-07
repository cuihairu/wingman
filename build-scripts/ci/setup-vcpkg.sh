#!/usr/bin/env bash
set -euo pipefail

vcpkg_root="${VCPKG_ROOT:-$PWD/vcpkg}"

if [ -f "$vcpkg_root/vcpkg" ]; then
    echo "vcpkg already installed at $vcpkg_root, skipping clone/bootstrap."
    exit 0
fi

rm -rf "$vcpkg_root"

# Pin vcpkg to a specific commit for reproducibility and supply chain security
# Use the builtin-baseline from vcpkg.json (can be overridden via VCPKG_COMMIT env var)
vcpkg_commit="${VCPKG_COMMIT:-00d899c410b31467733472fc3a83a25729046b13}"

echo "Cloning vcpkg and checking out commit $vcpkg_commit"

if ! git clone https://github.com/Microsoft/vcpkg.git "$vcpkg_root"; then
    echo "Failed to clone vcpkg"
    exit 1
fi

if ! git -C "$vcpkg_root" checkout "$vcpkg_commit"; then
    echo "Failed to checkout vcpkg at commit $vcpkg_commit, using current HEAD"
fi

"$vcpkg_root/bootstrap-vcpkg.sh"
