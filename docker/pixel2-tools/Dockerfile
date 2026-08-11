FROM debian:bookworm-slim

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        ca-certificates device-tree-compiler dosfstools e2fsprogs file jq \
        mtools parted squashfs-tools util-linux xz-utils \
    && rm -rf /var/lib/apt/lists/*

