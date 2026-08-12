FROM debian:bookworm-slim

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        android-libbase-dev android-libboringssl-dev android-libcutils-dev \
        android-liblog-dev android-libutils-dev abootimg autoconf automake bash bc bison \
        build-essential busybox-static \
        ca-certificates clang cmake cpio curl debootstrap device-tree-compiler dosfstools dpkg-dev \
        dropbear-bin e2fsprogs faketime fdisk file flex gawk genimage gettext git iproute2 iw jq kmod \
        libasound2-dev libavcodec-dev libavformat-dev libavutil-dev libboost-date-time-dev \
        libboost-filesystem-dev libboost-locale-dev libboost-system-dev libbrotli-dev libbz2-dev \
        libcap-dev libcurl4-openssl-dev \
        libdrm-dev libegl1-mesa-dev libelf-dev libexpat1-dev libfmt-dev libfreetype6-dev libgbm-dev \
        libgles2-mesa-dev libharfbuzz-dev libicu-dev libinih-dev libjpeg-dev liblhasa-dev \
        libltdl-dev liblzma-dev libmad0-dev libmpg123-dev libncurses-dev libogg-dev \
        libopus-dev libopusfile-dev libpixman-1-dev libpng-dev libpulse-dev libreadline-dev \
        libsamplerate0-dev libsdl1.2-dev libsdl2-dev libsdl2-gfx-dev libsdl2-image-dev \
        libsdl2-mixer-dev libsdl2-net-dev libsdl2-ttf-dev \
        libselinux1-dev libsndfile1-dev libspeexdsp-dev libssl-dev libtool \
        libswresample-dev libudev-dev libusb-1.0-0-dev libvorbis-dev libxmp-dev libxml2-dev \
        meson mtools ninja-build parted \
        pkg-config python3 python3-pip rsync \
        squashfs-tools util-linux \
        wpasupplicant xz-utils zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*
