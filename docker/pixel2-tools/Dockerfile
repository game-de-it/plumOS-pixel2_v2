FROM debian:bookworm-slim

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        android-libbase-dev android-libboringssl-dev android-libcutils-dev \
        android-liblog-dev android-libutils-dev bc bison build-essential busybox-static \
        ca-certificates clang curl device-tree-compiler dosfstools dpkg-dev \
        dropbear-bin e2fsprogs file flex git iproute2 iw jq kmod libasound2-dev libcap-dev \
        libdrm-dev libegl1-mesa-dev libelf-dev libfreetype6-dev libgbm-dev \
        libgles2-mesa-dev libpng-dev libselinux1-dev \
        libssl-dev libudev-dev mtools parted pkg-config python3 rsync \
        squashfs-tools util-linux \
        wpasupplicant xz-utils zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*
