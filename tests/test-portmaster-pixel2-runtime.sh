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
! grep -q 'kms_swrast' "$GUI_LAUNCH" "$PORT_LAUNCH"

printf 'portmaster_pixel2_runtime=result-ok\n'

