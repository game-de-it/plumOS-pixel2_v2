#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="${ROOT_DIR:-$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)}"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build/network-services-pixel2}"
OUT_DIR="${OUT_DIR:-$ROOT_DIR/output/network-services/pixel2}"
PLUMOS_DIR="$OUT_DIR/plumos"
NETWORK_DIR="$PLUMOS_DIR/network"
LIB_DIR="$NETWORK_DIR/lib"
BUSYBOX_VERSION=1.36.1
BUSYBOX_SHA256=b8cc24c9574d809e7279c3be349795c5d5ceb6fdf19ca709f80cde50e47de314
BUSYBOX_URL="https://busybox.net/downloads/busybox-${BUSYBOX_VERSION}.tar.bz2"

find_library() {
    local soname="$1" path
    for path in \
        "/lib/aarch64-linux-gnu/$soname" \
        "/usr/lib/aarch64-linux-gnu/$soname" \
        "/usr/lib/aarch64-linux-gnu/samba/$soname" \
        "/lib/$soname" "/usr/lib/$soname"; do
        [ -e "$path" ] && { readlink -f "$path"; return 0; }
    done
    return 1
}

copy_dep_tree() {
    local elf="$1" soname source real
    readelf -d "$elf" 2>/dev/null | awk -F'[][]' '/NEEDED/ {print $2}' |
        while IFS= read -r soname; do
            [ -n "$soname" ] || continue
            [ -e "$LIB_DIR/$soname" ] && continue
            source="$(find_library "$soname" || true)"
            [ -n "$source" ] || {
                printf 'error: network runtime dependency missing: %s (%s)\n' \
                    "$soname" "$elf" >&2
                exit 1
            }
            real="$(basename "$source")"
            install -m 0644 "$source" "$LIB_DIR/$real"
            if [ "$real" != "$soname" ]; then
                cp -f "$LIB_DIR/$real" "$LIB_DIR/$soname"
            fi
            copy_dep_tree "$source"
        done
}

rm -rf "$BUILD_DIR" "$OUT_DIR"
mkdir -p "$BUILD_DIR" "$PLUMOS_DIR/bin" "$NETWORK_DIR/bin" "$LIB_DIR" \
    "$NETWORK_DIR/samba/sbin" "$NETWORK_DIR/samba/lib" \
    "$PLUMOS_DIR/ssh/libexec" "$PLUMOS_DIR/ssh" \
    "$PLUMOS_DIR/components/network-services" \
    "$PLUMOS_DIR/share/doc/network-services"

archive="$BUILD_DIR/busybox.tar.bz2"
curl -LfsS "$BUSYBOX_URL" -o "$archive"
printf '%s  %s\n' "$BUSYBOX_SHA256" "$archive" | sha256sum -c -
tar -xjf "$archive" -C "$BUILD_DIR"
busybox_src="$BUILD_DIR/busybox-$BUSYBOX_VERSION"
make -C "$busybox_src" defconfig >/dev/null
sed -i \
    -e 's/^# CONFIG_STATIC is not set/CONFIG_STATIC=y/' \
    -e 's/^# CONFIG_TCPSVD is not set/CONFIG_TCPSVD=y/' \
    -e 's/^# CONFIG_FTPD is not set/CONFIG_FTPD=y/' \
    -e 's/^# CONFIG_FEATURE_FTP_WRITE is not set/CONFIG_FEATURE_FTP_WRITE=y/' \
    "$busybox_src/.config"
set +o pipefail
yes '' | make -C "$busybox_src" oldconfig >/dev/null
oldconfig_rc="${PIPESTATUS[1]}"
set -o pipefail
[ "$oldconfig_rc" -eq 0 ]
make -C "$busybox_src" -j"${JOBS:-$(nproc)}" busybox >/dev/null
install -m 0755 "$busybox_src/busybox" "$NETWORK_DIR/bin/busybox"
for applet in tcpsvd ftpd; do
    cat >"$PLUMOS_DIR/bin/$applet" <<EOF
#!/bin/sh
root="\${PLUMOS_ROOT:-/mnt/plumos}"
exec "\$root/network/bin/busybox" "$applet" "\$@"
EOF
    chmod 0755 "$PLUMOS_DIR/bin/$applet"
done

sftp_real=/usr/lib/openssh/sftp-server
[ -x "$sftp_real" ] || sftp_real=/usr/lib/sftp-server
[ -x "$sftp_real" ] || { printf 'error: sftp-server unavailable\n' >&2; exit 1; }
install -m 0755 "$sftp_real" "$PLUMOS_DIR/ssh/libexec/sftp-server.real"
copy_dep_tree "$sftp_real"
cat >"$PLUMOS_DIR/ssh/libexec/sftp-server.bin" <<'EOF'
#!/bin/sh
root="${PLUMOS_ROOT:-/mnt/plumos}"
export LD_LIBRARY_PATH="$root/network/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$root/ssh/libexec/sftp-server.real" "$@"
EOF
chmod 0755 "$PLUMOS_DIR/ssh/libexec/sftp-server.bin"
ln -s sftp-server.bin "$PLUMOS_DIR/ssh/libexec/sftp-server"

for daemon in smbd nmbd; do
    real="$(command -v "$daemon")"
    install -m 0755 "$real" "$NETWORK_DIR/samba/sbin/$daemon.real"
    copy_dep_tree "$real"
    cat >"$NETWORK_DIR/samba/sbin/$daemon" <<EOF
#!/bin/sh
root="\${PLUMOS_ROOT:-/mnt/plumos}"
export LD_LIBRARY_PATH="\$root/network/lib:\$root/network/samba/lib\${LD_LIBRARY_PATH:+:\$LD_LIBRARY_PATH}"
export SAMBA_DATADIR="\$root/network/samba/share"
exec "\$root/network/samba/sbin/$daemon.real" "\$@"
EOF
    chmod 0755 "$NETWORK_DIR/samba/sbin/$daemon"
done
if [ -d /usr/lib/aarch64-linux-gnu/samba ]; then
    cp -a /usr/lib/aarch64-linux-gnu/samba/. "$NETWORK_DIR/samba/lib/"
    # Debian's Samba modules may contain compatibility links whose targets live
    # in a package directory that is not part of the self-contained payload.
    # A broken link is unusable on Pixel2 and cannot be covered by the component
    # checksum, so discard it instead of silently shipping an incomplete ABI.
    find "$NETWORK_DIR/samba/lib" -type l -print0 |
        while IFS= read -r -d '' link; do
            [ -e "$link" ] || rm -f -- "$link"
        done
    find "$NETWORK_DIR/samba/lib" -type f -print0 | while IFS= read -r -d '' module; do
        file "$module" | grep -q 'ELF ' || continue
        copy_dep_tree "$module"
    done
fi
mkdir -p "$NETWORK_DIR/samba/share"
[ ! -d /usr/share/samba ] || cp -a /usr/share/samba/. "$NETWORK_DIR/samba/share/"

cat >"$PLUMOS_DIR/ssh/start-ssh.sh" <<'EOF'
#!/bin/sh
exec /usr/lib/plumos/init.d/30-ssh start
EOF
cat >"$PLUMOS_DIR/ssh/stop-ssh.sh" <<'EOF'
#!/bin/sh
pid_file="${PLUMOS_SSH_RUN_DIR:-/run/plumos/ssh}/dropbear.pid"
if [ -s "$pid_file" ]; then
  pid="$(cat "$pid_file" 2>/dev/null || true)"
  cmdline="$(tr '\000' ' ' <"/proc/$pid/cmdline" 2>/dev/null || true)"
  case "$cmdline" in *dropbear*) kill "$pid" 2>/dev/null || true ;; esac
  rm -f "$pid_file"
fi
for proc in /proc/[0-9]*; do
  [ -r "$proc/cmdline" ] || continue
  pid="${proc##*/}"
  [ "$(awk '{ print $3 }' "$proc/stat" 2>/dev/null || true)" != Z ] || continue
  cmdline="$(tr '\000' ' ' <"$proc/cmdline" 2>/dev/null || true)"
  case "$cmdline" in *dropbear*) kill "$pid" 2>/dev/null || true ;; esac
done
EOF
chmod 0755 "$PLUMOS_DIR/ssh/start-ssh.sh" "$PLUMOS_DIR/ssh/stop-ssh.sh"

install -m 0755 "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-network-services" \
    "$PLUMOS_DIR/bin/plumos-network-services"
install -m 0644 /usr/share/doc/busybox-static/copyright \
    "$PLUMOS_DIR/share/doc/network-services/busybox-copyright"
install -m 0644 /usr/share/doc/openssh-sftp-server/copyright \
    "$PLUMOS_DIR/share/doc/network-services/openssh-copyright"
install -m 0644 /usr/share/doc/samba/copyright \
    "$PLUMOS_DIR/share/doc/network-services/samba-copyright"

source_ref="$(git -C "$ROOT_DIR" rev-parse --short HEAD)"
cat >"$PLUMOS_DIR/components/network-services/manifest.json" <<EOF
{
  "name": "plumOS Pixel2 network services",
  "component": "network-services",
  "device": "pixel2",
  "architecture": "aarch64",
  "source_ref": "$source_ref",
  "services": ["ssh", "ftp", "sftp", "samba", "adb"],
  "share_root": "/mnt/plumos-user",
  "ftp_runtime": "BusyBox $BUSYBOX_VERSION",
  "ssh_runtime": "System Dropbear with packaged SFTP subsystem"
}
EOF
(
    cd "$PLUMOS_DIR"
    find bin network ssh share/doc/network-services \
        components/network-services/manifest.json \
        \( -type f -o -type l \) -print | sort |
        while IFS= read -r file; do sha256sum "$file"; done
) >"$PLUMOS_DIR/components/network-services/checksums.sha256"
(
    cd "$PLUMOS_DIR"
    sha256sum -c components/network-services/checksums.sha256
)
printf 'network_services=result-ok output=%s\n' "$OUT_DIR"
