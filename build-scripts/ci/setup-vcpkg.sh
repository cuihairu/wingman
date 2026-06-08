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

max_retries=3
retry_count=0
success=false

while [ "$success" = "false" ] && [ $retry_count -lt $max_retries ]; do
    retry_count=$((retry_count + 1))
    echo "Clone attempt $retry_count of $max_retries..."

    if git clone https://github.com/Microsoft/vcpkg.git "$vcpkg_root"; then
        success=true
    else
        echo "Clone failed with exit code $?"
        if [ $retry_count -lt $max_retries ]; then
            echo "Waiting 30 seconds before retry..."
            sleep 30
            rm -rf "$vcpkg_root"
        fi
    fi
done

if [ "$success" = "false" ]; then
    echo "Failed to clone vcpkg after $max_retries attempts"
    exit 1
fi

if ! git -C "$vcpkg_root" checkout "$vcpkg_commit"; then
    echo "Failed to checkout vcpkg at commit $vcpkg_commit, using current HEAD"
fi

"$vcpkg_root/bootstrap-vcpkg.sh"
