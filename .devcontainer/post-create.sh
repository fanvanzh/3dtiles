#!/usr/bin/env bash
set -euo pipefail

# 进入工作目录（Codespaces 会把仓库挂载到 /workspace）
cd /workspace || exit 0

# 更新子模块
git submodule update --init --recursive || true

# 安装 Rust Windows GNU 目标（用于 mingw 交叉编译）
rustup target add x86_64-pc-windows-gnu || true

# （可选）安装常用 vcpkg 包（可能很慢）
VCPKG_ROOT=/workspace/vcpkg
if [ -d "${VCPKG_ROOT}" ]; then
  echo "Using VCPKG_ROOT=${VCPKG_ROOT}"
  ${VCPKG_ROOT}/vcpkg install --triplet x64-mingw-static --recurse || true
fi

echo "post-create done"
