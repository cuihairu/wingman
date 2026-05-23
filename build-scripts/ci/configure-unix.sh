#!/usr/bin/env bash
set -euo pipefail

triplet="${1:?triplet is required}"
generator="${2:-Ninja}"
build_dir="${3:-build}"
build_type="${4:-Release}"
version_suffix="${5:-}"

vcpkg_root="${VCPKG_ROOT:-$PWD/vcpkg}"
overlay_ports="$PWD/vcpkg-ports"

cmake_args=(
  -S . -B "$build_dir" -G "$generator"
  -DCMAKE_TOOLCHAIN_FILE="$vcpkg_root/scripts/buildsystems/vcpkg.cmake"
  -DVCPKG_OVERLAY_PORTS="$overlay_ports"
  -DVCPKG_TARGET_TRIPLET="$triplet"
  -DCMAKE_BUILD_TYPE="$build_type"
  -DWINGMAN_BUILD_GUI=OFF
  -DWINGMAN_COMPAT_BUILD=ON
)

if [ -n "$version_suffix" ]; then
  cmake_args+=("-DWINGMAN_VERSION_SUFFIX=$version_suffix")
fi

cmake "${cmake_args[@]}"
