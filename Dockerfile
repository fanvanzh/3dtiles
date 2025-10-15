#==================== Build stage ====================
FROM rust:1.90.0-bookworm AS builder

ARG REPO_USER=fanvanzh
ARG REPO_REF=master

# 系统与编译依赖
RUN apt-get update && apt-get install -y --no-install-recommends \
    git build-essential cmake make \
    libgdal-dev \
    libopenscenegraph-dev \
 && rm -rf /var/lib/apt/lists/*

# 拉取源码并编译
WORKDIR /app
RUN git clone https://github.com/${REPO_USER}/3dtiles.git . && \
    git checkout $REPO_REF

ENV CARGO_TERM_COLOR=always
RUN cargo build --release

#==================== Runtime stage ====================
FROM debian:bookworm-slim

# 运行期需要的动态库
RUN apt-get update && apt-get install -y --no-install-recommends \
    libgdal-dev \
    libopenscenegraph-dev \
 && rm -rf /var/lib/apt/lists/*

 RUN mkdir -p /3dtiles
 WORKDIR /3dtiles

# 拷贝可执行文件
COPY --from=builder /app/target/release/_3dtile /3dtiles/_3dtile
COPY --from=builder /app/target/release/gdal /3dtiles/gdal
COPY --from=builder /app/target/release/proj /3dtiles/proj

WORKDIR /data
ENTRYPOINT ["/3dtiles/_3dtile", "--help"]
