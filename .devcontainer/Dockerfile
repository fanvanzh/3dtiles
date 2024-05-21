# 使用官方 Rust 镜像作为基础镜像
FROM rust:latest

# （可选）安装额外工具
RUN apt-get update && apt-get install -y \
    git \
    pkg-config \
    libc6-dev \
    m4 \
    gcc \
    && rm -rf /var/lib/apt/lists/*

# 设置工作目录
WORKDIR /workspace

# （可选）设置环境变量，如果需要的话
ENV PATH="/root/.cargo/bin:${PATH}"
