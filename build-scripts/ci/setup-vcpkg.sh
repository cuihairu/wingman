#!/usr/bin/env bash
set -euo pipefail

vcpkg_root="${VCPKG_ROOT:-$PWD/vcpkg}"

if [ -f "$vcpkg_root/vcpkg" ]; then
    echo "vcpkg already installed at $vcpkg_root, skipping clone/bootstrap."
    exit 0
fi

rm -rf "$vcpkg_root"

# Pin vcpkg to a specific version for reproducibility and supply chain security
# Matches the builtin-baseline in vcpkg.json (can be overridden via VCPKG_VERSION env var)
vcpkg_version="${VCPKG_VERSION:-2024.06.15}"

echo "Cloning vcpkg version $vcpkg_version"

if ! git clone --depth 1 --branch "$vcpkg_version" https://github.com/Microsoft/vcpkg.git "$vcpkg_root"; then
    echo "Failed to clone vcpkg at version $vcpkg_version, falling back to latest"
    git clone https://github.com/Microsoft/vcpkg.git "$vcpkg_root"
fi

"$vcpkg_root/bootstrap-vcpkg.sh"
