FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential clang cmake ninja-build python3 pkg-config git ca-certificates \
    libegl1-mesa-dev libgles2-mesa-dev libgl1-mesa-dri libegl-mesa0 mesa-utils libx11-dev \
    libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libx11-xcb-dev && rm -rf /var/lib/apt/lists/*
