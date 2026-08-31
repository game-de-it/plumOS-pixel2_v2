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
PORT_STOP="$PACKAGE/bin/plumos-portmaster-port-stop"
MOUNT_CLEANUP="$PACKAGE/bin/plumos-portmaster-mount-cleanup"
AUDIT_HELPER="$PACKAGE/bin/plumos-portmaster-audit-port"
PM_LAUNCH="$PACKAGE/bin/plumos-portmaster-launch"
PM_UPDATE="$PACKAGE/bin/plumos-portmaster-update"
UPDATE_ADAPTER="$PACKAGE/apps/portmaster/adapter/plumos_portmaster_update.py"
MOONLIGHT_PATCHER="$PACKAGE/apps/portmaster/adapter/plumos_moonlight_gui_patch.py"
ESUDO_SHIM="$PACKAGE/apps/portmaster/adapter/shims/esudo"
DF_SHIM="$PACKAGE/apps/portmaster/adapter/shims/df"
PKILL_SHIM="$PACKAGE/apps/portmaster/adapter/shims/pkill"
PATCH_SHIM="$PACKAGE/apps/portmaster/adapter/shims/run-patchscript"
PATCHER_OVERRIDE="$PACKAGE/apps/portmaster/adapter/overrides/patcher.txt"
BUILDER="$ROOT_DIR/scripts/build-portmaster-pixel2.sh"
ROTATE_SOURCE="$ROOT_DIR/package/portmaster-pixel2/src/plumos_portmaster_sdl_rotate.c"
GL_ROTATE_SOURCE="$ROOT_DIR/package/portmaster-pixel2/src/plumos_portmaster_gl_rotate.c"
EXEC_GUARD_SOURCE="$ROOT_DIR/package/portmaster-pixel2/src/plumos_portmaster_exec_guard.c"
ROCKBOX_SOURCE="$ROOT_DIR/package/portmaster-pixel2/src/plumos_portmaster_rockbox.c"

python3 -m py_compile "$BOOTSTRAP" "$RENDERER" "$UPDATE_ADAPTER" "$MOONLIGHT_PATCHER"
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
grep -q 'SDL_ShowCursor(sdl2.SDL_DISABLE)' "$RENDERER"
grep -q 'DRI_DIR}/rockchip_dri.so' "$GUI_LAUNCH"
grep -q 'PLUMOS_PORTMASTER_FIT:-0' "$GUI_LAUNCH"
grep -q 'DRI_DIR}/rockchip_dri.so' "$PORT_LAUNCH"
grep -q 'SDL_VIDEO_EGL_DRIVER=.*apps/pyxel/lib/libEGL.so.1' "$PORT_LAUNCH"
grep -q 'SDL_VIDEO_GL_DRIVER=.*apps/pyxel/lib/libGLESv2.so.2' "$PORT_LAUNCH"
grep -q 'apps/pyxel/site/pygame.libs' "$PORT_LAUNCH"
grep -q 'MESA_SHADER_CACHE_DISABLE' "$PORT_LAUNCH"
grep -q 'PLUMOS_PORTMASTER_SDL_ROTATION=270' "$PORT_LAUNCH"
grep -q 'libplumos-portmaster-sdl-rotate.so' "$PORT_LAUNCH"
grep -q 'libplumos-portmaster-gl-rotate.so' "$PORT_LAUNCH"
grep -q 'PLUMOS_PORTMASTER_GL_ROTATION=270' "$PORT_LAUNCH"
grep -q 'ADAPTER_VERSION="54"' "$BUILDER"
grep -q 'ADAPTER_VERSION = 54' "$UPDATE_ADAPTER"
grep -q 'plumos_moonlight_gui_patch.py' "$PORT_LAUNCH"
grep -q 'math.max(28, 38 \* scaleFactor)' "$MOONLIGHT_PATCHER"
grep -q 'appName ~= "Load apps first"' "$MOONLIGHT_PATCHER"
grep -q 'runInBackground = command:match' "$MOONLIGHT_PATCHER"
grep -q 'exec "$@"' "$ESUDO_SHIM"
grep -q 'adapter/shims/esudo' "$CONTROL"
grep -A8 -F "'moonlight new.sh')" "$PORT_LAUNCH" | \
    grep -q 'export SDL_RENDER_DRIVER="software"'
grep -q 'PortMaster/runtimes/love_\*/love.aarch64' "$UPDATE_ADAPTER"
grep -q -- "-name 'love.aarch64' -exec chmod 0755" "$RUNTIME"
grep -q -- "-name 'love.aarch64'" "$BUILDER"
grep -q 'plumos_portmaster_sdl_rotate.c' "$BUILDER"
grep -q 'plumos_portmaster_gl_rotate.c' "$BUILDER"
grep -q 'plumos_portmaster_exec_guard.c' "$BUILDER"
grep -q 'plumos_portmaster_rockbox.c' "$BUILDER"
grep -q 'PLUMOS_PORTMASTER_REQUIRED_LD_LIBRARY_PATH' "$PORT_LAUNCH"
grep -q 'PLUMOS_PORTMASTER_REQUIRED_LD_PRELOAD' "$PORT_LAUNCH"
grep -q 'libplumos-portmaster-exec-guard.so' "$PORT_LAUNCH"
grep -q 'libplumos-portmaster-rockbox.so' "$PORT_LAUNCH"
grep -q 'rockbox.sh)' "$PORT_LAUNCH"
grep -q 'SDL_WINDOWEVENT_RESIZED' "$ROCKBOX_SOURCE"
grep -q 'SDL_WINDOWEVENT_SIZE_CHANGED' "$ROCKBOX_SOURCE"
grep -q 'SDL_TEXTUREACCESS_STREAMING' "$ROCKBOX_SOURCE"
grep -q 'int execve(' "$EXEC_GUARD_SOURCE"
grep -q 'int execveat(' "$EXEC_GUARD_SOURCE"
grep -q 'int posix_spawn(' "$EXEC_GUARD_SOURCE"
grep -q 'int posix_spawnp(' "$EXEC_GUARD_SOURCE"
grep -q 'link_one libavahi-common.so.3' "$RUNTIME"
grep -q 'link_one libavahi-client.so.3' "$RUNTIME"
grep -q 'link_one libnghttp2.so.14' "$RUNTIME"
grep -q 'plumos-portmaster-audit-port' "$PORT_LAUNCH"
grep -q 'deferred_audit=1' "$PORT_LAUNCH"
! grep -q -- '--enforce' "$PORT_LAUNCH" "$AUDIT_HELPER"
grep -q 'plumos_portmaster_audit.py' "$AUDIT_HELPER"
grep -q -- '--ports-root "$ports_root"' "$AUDIT_HELPER"
grep -q 'plumos-portmaster-mount-cleanup' "$PORT_LAUNCH" "$PM_LAUNCH"
grep -q 'UMOUNT_RETRIES' "$MOUNT_CLEANUP"
grep -q 'plumos-portmaster-session-cleanup' "$PORT_LAUNCH"
grep -q 'PLUMOS_PORTMASTER_SESSION_ID' "$PORT_LAUNCH"
grep -q 'current_start=.*proc.*stat' "$PORT_LAUNCH"
grep -q 'FRONTEND_PID_FILE=.*frontend.pid' "$PORT_LAUNCH"
grep -q '\[ "$parent" = "$frontend_pid" \].*return 0' "$PORT_LAUNCH"
grep -q 'plumos-frontend-pixel2' "$PORT_LAUNCH"
grep -q 'SDL_GL_GetDrawableSize' "$GL_ROTATE_SOURCE"
grep -q 'SDL_GL_SwapWindow' "$GL_ROTATE_SOURCE"
python3 - "$GL_ROTATE_SOURCE" <<'PY'
from pathlib import Path
import sys

source = Path(sys.argv[1]).read_text()
load_gl = source.split("static int load_gl(void) {", 1)[1].split("\n}", 1)[0]
assert load_gl.index('LOAD_NEXT(sdl_gl_get_proc_address, "SDL_GL_GetProcAddress")') \
    < load_gl.index('LOAD_GL(gl_bind_framebuffer, "glBindFramebuffer")')
PY
grep -q 'glBindFramebuffer' "$GL_ROTATE_SOURCE"
grep -q 'GL_DEPTH24_STENCIL8' "$GL_ROTATE_SOURCE"
grep -q 'GL_STENCIL_ATTACHMENT' "$GL_ROTATE_SOURCE"
grep -q 'GL_COLOR_WRITEMASK' "$GL_ROTATE_SOURCE"
grep -q 'real_gl_color_mask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE)' \
    "$GL_ROTATE_SOURCE"
grep -q 'real_gl_color_mask(color_mask\[0\], color_mask\[1\]' \
    "$GL_ROTATE_SOURCE"
grep -q 'LOAD_GL(gl_finish, "glFinish")' "$GL_ROTATE_SOURCE"
grep -q 'real_gl_finish();' "$GL_ROTATE_SOURCE"
! grep -q 'glInvalidateFramebuffer' "$GL_ROTATE_SOURCE"
! grep -q 'glDiscardFramebufferEXT' "$GL_ROTATE_SOURCE"
grep -q 'LOAD(render_copy_ex, RenderCopyEx)' "$ROTATE_SOURCE"
grep -q '270.0' "$ROTATE_SOURCE"
grep -q 'SDL_SetRenderTarget' "$ROTATE_SOURCE"
grep -q 'SDL_GetRenderTarget' "$ROTATE_SOURCE"
grep -q 'SDL_GetWindowSize' "$ROTATE_SOURCE"
grep -q 'SDL_GetCurrentDisplayMode' "$ROTATE_SOURCE"
! grep -q 'SDL_GetDesktopDisplayMode' "$ROTATE_SOURCE"
! grep -q '^int SDL_GetDisplayMode' "$ROTATE_SOURCE"
grep -q 'mode->w = 640' "$ROTATE_SOURCE"
grep -q 'mode->h = 480' "$ROTATE_SOURCE"
grep -q 'output_width = 480' "$ROTATE_SOURCE"
grep -q 'output_height = 640' "$ROTATE_SOURCE"
! grep -q 'kms_swrast' "$GUI_LAUNCH" "$PORT_LAUNCH"
grep -q 'exec "$busybox" df "$@"' "$DF_SHIM"
grep -q 'called_by_owned_gptokey' "$PKILL_SHIM"
grep -q 'plumos-portmaster-port-stop" stop' "$PKILL_SHIM"
grep -q 'PORT_BASH="${APP_ROOT}/adapter/bin/aarch64/bash"' "$PORT_LAUNCH"
grep -q '^export PORT_BASH$' "$PORT_LAUNCH"
grep -q 'setsid "$PORT_BASH" "$script"' "$PORT_LAUNCH"
grep -q 'prepare_patcher_compat || exit 1' "$PORT_LAUNCH"
grep -q 'patcher-talkies.lua' "$PORT_LAUNCH"
grep -q 'patcher-main.lua' "$PORT_LAUNCH"
grep -q 'box_y_line}-12' "$PORT_LAUNCH"
grep -q 'local fontSize = 24' "$PORT_LAUNCH"
grep -q 'height                  = 128' "$PORT_LAUNCH"
grep -q 'PLUMOS_PORTMASTER_PATCHER_OVERRIDE:-' "$PORT_LAUNCH"
grep -q 'PLUMOS_PORTMASTER_PATCH_SCRIPT="$PATCHER_FILE"' "$PATCHER_OVERRIDE"
grep -q 'PLUMOS_PORTMASTER_PATCH_SHIM:-' "$PATCHER_OVERRIDE"
grep -q 'adapter/shims/run-patchscript' "$PATCHER_OVERRIDE"
grep -q 'exec "$PORT_BASH" "$PLUMOS_PORTMASTER_PATCH_SCRIPT"' "$PATCH_SHIM"
grep -q '\[ "$resolved_alias" -ef "$ROMS_ROOT" \]' "$PORT_LAUNCH"
grep -q '\[ "$canonical_script" -ef "$script" \]' "$PORT_LAUNCH"
grep -q 'script="$canonical_script"' "$PORT_LAUNCH"
grep -q '\[ "$resolved_alias" -ef "$PLUMOS_ROM_ROOT" \]' "$PORT_STOP"
grep -q '\[ "$candidate" -ef "$script" \]' "$PORT_STOP"
grep -q 'PYSDL2_DLL_PATH="${RUN_ROOT}/lib"' "$PORT_LAUNCH"
grep -q '"$BB" setsid "$BB" sh.*plumos-frontend-launch' "$PORT_LAUNCH"
grep -q '"$BB" setsid "$BB" sh.*plumos-frontend-launch' "$GUI_LAUNCH"
! grep -q 'nohup.*plumos-frontend-launch' "$PORT_LAUNCH" "$GUI_LAUNCH"
! grep -q 'LD_LIBRARY_PATH="${RUN_ROOT}/lib" setsid' "$PORT_LAUNCH"
grep -q '/bin/busybox --list' "$RUNTIME"
grep -q 'link_one libgcc_s.so.1' "$RUNTIME"
grep -q 'RUN_ROOT}/busybox-bin' "$PORT_LAUNCH"
grep -q 'RUN_ROOT}/busybox-bin' "$PM_LAUNCH"
grep -q 'RUN_ROOT}/busybox-bin' "$PM_UPDATE"
grep -q 'RESTART_FILE="${PM_DIR}/.pugwash-reboot"' "$PM_LAUNCH"
grep -q 'restart-marker=stale action=consume' "$PM_LAUNCH"
grep -q 'PLUMOS_PORTMASTER_RESTART_LIMIT:-8' "$PM_LAUNCH"
grep -q 'restart-marker=requested count=' "$PM_LAUNCH"
grep -q '\[ -e "$RESTART_FILE" \] || exit "$rc"' "$PM_LAUNCH"
grep -q 'rm -f "$RESTART_FILE" || exit 1' "$PM_LAUNCH"
grep -q 'BASH_RUNTIME_VERSION="5.2.15-2+b13"' "$BUILDER"
grep -q 'adapter/bin/aarch64/bash' "$BUILDER"
grep -q 'licenses/bash-copyright.txt' "$BUILDER"
grep -q -- "-path 'licenses/bash-\*'" "$BUILDER"

printf 'portmaster_pixel2_runtime=result-ok\n'
