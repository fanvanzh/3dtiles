# Use official Rust image as base
FROM --platform=linux/amd64 rust:1.90.0-bookworm

# Disable Rosetta and force QEMU emulation
ENV DOCKER_DEFAULT_PLATFORM=linux/amd64

# Replace Debian package sources with faster mirrors
RUN sed -i 's|http://deb.debian.org|http://mirrors.ustc.edu.cn|g' /etc/apt/sources.list.d/debian.sources && \
    sed -i 's|http://security.debian.org|http://mirrors.ustc.edu.cn|g' /etc/apt/sources.list.d/debian.sources

# Install vcpkg dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    git build-essential cmake make zip unzip tar curl \
    pkg-config autoconf automake libtool linux-libc-dev libgl1-mesa-dev \
 && rm -rf /var/lib/apt/lists/*

# Install vcpkg
WORKDIR /opt
RUN git clone https://gitee.com/Wallance/vcpkg.git && \
    ./vcpkg/bootstrap-vcpkg.sh

# Install OpenGL related dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    libgl1-mesa-dev libglu1-mesa-dev \
    libx11-dev libxrandr-dev libxi-dev libxxf86vm-dev \
 && rm -rf /var/lib/apt/lists/*

# Set environment variables for vcpkg
ENV VCPKG_ROOT=/opt/vcpkg
ENV PATH=$VCPKG_ROOT:$PATH

# Set working directory
WORKDIR /workspace

# Set environment variables
ENV PATH="/root/.cargo/bin:${PATH}"
ENV CARGO_TERM_COLOR=always