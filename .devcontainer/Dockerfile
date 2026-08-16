# 基于 Rust 官方镜像（指定具体版本可按需调整）
FROM --platform=linux/amd64 rust:1.90.0-bookworm

# 避免交互
ENV DEBIAN_FRONTEND=noninteractive
ENV DOCKER_DEFAULT_PLATFORM=linux/amd64

# 使用国内镜像源（可选，若不需要可移除这些 sed 行）
RUN sed -i 's|http://deb.debian.org|http://mirrors.ustc.edu.cn|g' /etc/apt/sources.list.d/debian.sources || true && \
    sed -i 's|http://security.debian.org|http://mirrors.ustc.edu.cn|g' /etc/apt/sources.list.d/debian.sources || true

# 安装系统依赖：构建工具、mingw、cmake、ninja、wine 等
RUN apt-get update && apt-get install -y --no-install-recommends \
    git build-essential cmake make ninja-build zip unzip tar curl ca-certificates \
    pkg-config autoconf automake libtool python3 python3-pip \
    mingw-w64 gcc-mingw-w64 g++-mingw-w64 \
    wine64 libgl1-mesa-dev libglu1-mesa-dev libx11-dev libxrandr-dev libxi-dev libxxf86vm-dev \
    libssl-dev \
  && rm -rf /var/lib/apt/lists/*

# 在 /opt 安装 vcpkg（镜像构建时 bootstrap）
WORKDIR /opt
# 在工作区安装 vcpkg（镜像构建时 bootstrap）
WORKDIR /workspace
RUN git clone --depth=1 https://github.com/microsoft/vcpkg.git vcpkg && \
    ./vcpkg/bootstrap-vcpkg.sh && \
    chown -R vscode:vscode /workspace/vcpkg

# 设置 vcpkg 环境变量并放入 PATH
ENV VCPKG_ROOT=/workspace/vcpkg
ENV PATH=$VCPKG_ROOT:$PATH

# 设置工作目录与路径
WORKDIR /workspace
ENV PATH="/root/.cargo/bin:${PATH}"
ENV CARGO_TERM_COLOR=always

# 切回 vscode 用户
USER vscode

# 设置 vcpkg 环境变量并放入 PATH
ENV VCPKG_ROOT=/opt/vcpkg
ENV PATH=$VCPKG_ROOT:$PATH

# 设置工作目录与路径
WORKDIR /workspace
ENV PATH="/root/.cargo/bin:${PATH}"
ENV CARGO_TERM_COLOR=always

# 默认以 root 用户运行（与你的 devcontainer.json 中 remoteUser 要一致）
USER root

# 可选：在镜像构建时预装一些常用 vcpkg 包（如失败不会中断）
# RUN $VCPKG_ROOT/vcpkg install --triplet x64-mingw-static --recurse || true

# 容器启动时默认命令（保留为空）
CMD [ "/bin/bash" ]
