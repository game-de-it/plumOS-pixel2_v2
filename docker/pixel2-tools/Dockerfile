FROM debian:bookworm-slim

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        android-libbase-dev android-libboringssl-dev android-libcutils-dev \
        android-liblog-dev android-libutils-dev autoconf automake bc bison \
        build-essential busybox-static \
        ca-certificates clang cmake curl device-tree-compiler dosfstools dpkg-dev \
        dropbear-bin e2fsprogs faketime file flex git iproute2 iw jq kmod libasound2-dev libcap-dev \
        libdrm-dev libegl1-mesa-dev libelf-dev libfreetype6-dev libgbm-dev \
        libgles2-mesa-dev libharfbuzz-dev libicu-dev libjpeg-dev libltdl-dev \
        libmpg123-dev libogg-dev libopus-dev libpng-dev libsamplerate0-dev \
        libselinux1-dev libsndfile1-dev libspeexdsp-dev libssl-dev libtool \
        libudev-dev libvorbis-dev libxmp-dev mtools ninja-build parted \
        pkg-config python3 python3-pip rsync \
        squashfs-tools util-linux \
        wpasupplicant xz-utils zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*
