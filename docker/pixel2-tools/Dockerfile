FROM debian:bookworm-slim

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        android-libbase-dev android-libboringssl-dev android-libcutils-dev \
        android-liblog-dev android-libutils-dev build-essential busybox-static \
        ca-certificates clang curl device-tree-compiler dosfstools dpkg-dev \
        dropbear-bin e2fsprogs file git iproute2 iw jq kmod libcap-dev \
        libselinux1-dev mtools parted squashfs-tools util-linux \
        wpasupplicant xz-utils \
    && rm -rf /var/lib/apt/lists/*
