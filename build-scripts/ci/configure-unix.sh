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

# macOS-specific settings
if [[ "$OSTYPE" == "darwin"* ]]; then
  cmake_args+=(
    -DCMAKE_CXX_COMPILER=clang++
    -DCMAKE_C_COMPILER=clang
  )
  if [ -n "${MACOSX_DEPLOYMENT_TARGET:-}" ]; then
    cmake_args+=(-DCMAKE_OSX_DEPLOYMENT_TARGET="$MACOSX_DEPLOYMENT_TARGET")
  fi
  # 确保使用 Homebrew 的 ninja（如果存在）
  if command -v ninja &> /dev/null; then
    NINJA_PATH=$(command -v ninja)
    cmake_args+=(-DCMAKE_MAKE_PROGRAM="$NINJA_PATH")
    export PATH="$(dirname "$NINJA_PATH"):$PATH"
  fi
fi

if [ -n "$version_suffix" ]; then
  cmake_args+=("-DWINGMAN_VERSION_SUFFIX=$version_suffix")
fi

cmake "${cmake_args[@]}"
