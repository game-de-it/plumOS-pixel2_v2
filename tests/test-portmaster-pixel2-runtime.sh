#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
PACKAGE="$ROOT_DIR/package/portmaster-pixel2/plumos"
BOOTSTRAP="$PACKAGE/apps/portmaster/adapter/plumos_portmaster_bootstrap.py"
RENDERER="$PACKAGE/apps/portmaster/adapter/plumos_pixel2_renderer.py"
CONTROL="$PACKAGE/apps/portmaster/adapter/control.txt"
RUNTIME="$PACKAGE/bin/plumos-portmaster-runtime"
GUI_LAUNCH="$PACKAGE/bin/plumos-portmaster-launch"
PORT_LAUNCH="$PACKAGE/bin/plumos-portmaster-port-launch"
PM_LAUNCH="$PACKAGE/bin/plumos-portmaster-launch"
PM_UPDATE="$PACKAGE/bin/plumos-portmaster-update"
DF_SHIM="$PACKAGE/apps/portmaster/adapter/shims/df"
BUILDER="$ROOT_DIR/scripts/build-portmaster-pixel2.sh"
ROTATE_SOURCE="$ROOT_DIR/package/portmaster-pixel2/src/plumos_portmaster_sdl_rotate.c"

python3 -m py_compile "$BOOTSTRAP" "$RENDERER"
python3 - "$BOOTSTRAP" <<'PY'
import importlib.util
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
spec = importlib.util.spec_from_file_location("plumos_pm_bootstrap", path)
module = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(module)
source = b"before\n" + module.RENDERER_INIT + b"\nafter\n"
result = module.install_pixel2_renderer(source)
assert result.count(module.PIXEL2_RENDERER_INIT) == 1
assert module.RENDERER_INIT not in result
try:
    module.install_pixel2_renderer(b"unsupported")
except RuntimeError:
    pass
else:
    raise AssertionError("unknown upstream renderer layout was accepted")
PY

guid='19008d96010000000221000000010000'
grep -q "DEVICE=\"$guid\"" "$CONTROL"
grep -q "^$guid,plumOS Pixel2 Controller,a:b1,b:b0,x:b2,y:b3," "$RUNTIME"
grep -q 'back:b8,start:b9,guide:b14,dpup:b10,dpdown:b11,dpleft:b12,dpright:b13' "$RUNTIME"
! grep -R -q '06000000091200006635000001000000' "$CONTROL" "$RUNTIME"

grep -q 'SDL_RenderCopyEx' "$RENDERER"
grep -q '270.0' "$RENDERER"
grep -q 'DRI_DIR}/rockchip_dri.so' "$GUI_LAUNCH"
grep -q 'PLUMOS_PORTMASTER_FIT:-0' "$GUI_LAUNCH"
grep -q 'DRI_DIR}/rockchip_dri.so' "$PORT_LAUNCH"
grep -q 'SDL_VIDEO_EGL_DRIVER=.*apps/pyxel/lib/libEGL.so.1' "$PORT_LAUNCH"
grep -q 'SDL_VIDEO_GL_DRIVER=.*apps/pyxel/lib/libGLESv2.so.2' "$PORT_LAUNCH"
grep -q 'apps/pyxel/site/pygame.libs' "$PORT_LAUNCH"
grep -q 'MESA_SHADER_CACHE_DISABLE' "$PORT_LAUNCH"
grep -q 'PLUMOS_PORTMASTER_SDL_ROTATION=270' "$PORT_LAUNCH"
grep -q 'libplumos-portmaster-sdl-rotate.so' "$PORT_LAUNCH"
grep -q 'ADAPTER_VERSION="24"' "$BUILDER"
grep -q 'plumos_portmaster_sdl_rotate.c' "$BUILDER"
grep -q 'SDL_RenderCopyEx' "$ROTATE_SOURCE"
grep -q '270.0' "$ROTATE_SOURCE"
grep -q 'SDL_SetRenderTarget' "$ROTATE_SOURCE"
grep -q 'SDL_GetRenderTarget' "$ROTATE_SOURCE"
grep -q 'SDL_GetWindowSize' "$ROTATE_SOURCE"
! grep -q 'kms_swrast' "$GUI_LAUNCH" "$PORT_LAUNCH"
grep -q 'exec "$busybox" df "$@"' "$DF_SHIM"
grep -q 'PORT_BASH="${APP_ROOT}/adapter/bin/aarch64/bash"' "$PORT_LAUNCH"
grep -q 'setsid "$PORT_BASH" "$script"' "$PORT_LAUNCH"
grep -q '"$BB" setsid "$BB" sh.*plumos-frontend-launch' "$PORT_LAUNCH"
grep -q '"$BB" setsid "$BB" sh.*plumos-frontend-launch' "$GUI_LAUNCH"
! grep -q 'nohup.*plumos-frontend-launch' "$PORT_LAUNCH" "$GUI_LAUNCH"
! grep -q 'LD_LIBRARY_PATH="${RUN_ROOT}/lib" setsid' "$PORT_LAUNCH"
grep -q '/bin/busybox --list' "$RUNTIME"
grep -q 'link_one libgcc_s.so.1' "$RUNTIME"
grep -q 'RUN_ROOT}/busybox-bin' "$PORT_LAUNCH"
grep -q 'RUN_ROOT}/busybox-bin' "$PM_LAUNCH"
grep -q 'RUN_ROOT}/busybox-bin' "$PM_UPDATE"
grep -q 'BASH_RUNTIME_VERSION="5.2.15-2+b13"' "$BUILDER"
grep -q 'adapter/bin/aarch64/bash' "$BUILDER"
grep -q 'licenses/bash-copyright.txt' "$BUILDER"
grep -q -- "-path 'licenses/bash-\*'" "$BUILDER"

printf 'portmaster_pixel2_runtime=result-ok\n'
