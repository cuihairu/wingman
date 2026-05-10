#!/bin/bash
# Wingman 一键构建脚本

set -e

echo "========================================"
echo "  Wingman 构建脚本"
echo "========================================"
echo ""

# 1. 构建 Dashboard
echo "[1/3] 构建 Dashboard..."
cd dashboard
pnpm install
pnpm run build
cd ..
echo "✓ Dashboard 构建成功"
echo ""

# 2. 复制 Dashboard 到构建目录
echo "[2/3] 复制静态资源..."
mkdir -p build/dist
cp -r dashboard/dist/* build/dist/
echo "✓ 静态资源已复制到 build/dist"
echo ""

# 3. 构建 C++ 项目
echo "[3/3] 构建 C++ 项目..."
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
cd ..
echo "✓ C++ 项目构建成功"
echo ""

echo "========================================"
echo "  构建完成！"
echo "========================================"
echo ""
echo "运行程序:"
echo "  ./build/Release/wingman"
echo ""
