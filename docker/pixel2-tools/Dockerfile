FROM debian:bookworm-slim

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        busybox-static ca-certificates device-tree-compiler dosfstools \
        dropbear-bin e2fsprogs file git iproute2 iw jq kmod mtools parted \
        squashfs-tools util-linux wpasupplicant xz-utils \
    && rm -rf /var/lib/apt/lists/*
