"""Rotate the PortMaster GUI into the Pixel2 portrait DRM scanout."""

from ctypes import byref

import sdl2
import sdl2.ext


class Pixel2Renderer(sdl2.ext.Renderer):
    """Render at logical 640x480 and present CCW to native 480x640."""

    LOGICAL_WIDTH = 640
    LOGICAL_HEIGHT = 480

    def __init__(self, target, *args, **kwargs):
        super().__init__(target, *args, **kwargs)

        # Pixel2 is controller-only. The stock Rockchip DRM driver can leave
        # SDL's hardware cursor plane scanned out after this process exits.
        sdl2.SDL_ShowCursor(sdl2.SDL_DISABLE)

        info = sdl2.SDL_RendererInfo()
        if sdl2.SDL_GetRendererInfo(self.sdlrenderer, byref(info)) != 0:
            raise sdl2.ext.SDLError(sdl2.SDL_GetError())
        self._plumos_frame = sdl2.SDL_CreateTexture(
            self.sdlrenderer,
            info.texture_formats[0],
            sdl2.SDL_TEXTUREACCESS_TARGET,
            self.LOGICAL_WIDTH,
            self.LOGICAL_HEIGHT,
        )
        if not self._plumos_frame:
            raise sdl2.ext.SDLError(sdl2.SDL_GetError())
        if sdl2.SDL_SetRenderTarget(self.sdlrenderer, self._plumos_frame) != 0:
            raise sdl2.ext.SDLError(sdl2.SDL_GetError())
        self.logical_size = (self.LOGICAL_WIDTH, self.LOGICAL_HEIGHT)

    def present(self):
        renderer = self.sdlrenderer
        sdl2.SDL_ShowCursor(sdl2.SDL_DISABLE)
        if sdl2.SDL_SetRenderTarget(renderer, None) != 0:
            raise sdl2.ext.SDLError(sdl2.SDL_GetError())

        # A 640x480 rectangle centred on the 480x640 output fills it exactly
        # after the counter-clockwise quarter turn.
        if sdl2.SDL_RenderSetLogicalSize(renderer, 0, 0) != 0:
            raise sdl2.ext.SDLError(sdl2.SDL_GetError())
        sdl2.SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255)
        sdl2.SDL_RenderClear(renderer)
        destination = sdl2.SDL_Rect(-80, 80, 640, 480)
        if (
            sdl2.SDL_RenderCopyEx(
                renderer,
                self._plumos_frame,
                None,
                byref(destination),
                270.0,
                None,
                sdl2.SDL_FLIP_NONE,
            )
            != 0
        ):
            raise sdl2.ext.SDLError(sdl2.SDL_GetError())
        sdl2.SDL_RenderPresent(renderer)

        if sdl2.SDL_SetRenderTarget(renderer, self._plumos_frame) != 0:
            raise sdl2.ext.SDLError(sdl2.SDL_GetError())
        self.logical_size = (self.LOGICAL_WIDTH, self.LOGICAL_HEIGHT)

    def destroy(self):
        frame = getattr(self, "_plumos_frame", None)
        if frame and self._renderer_ref and self._renderer_ref[0]:
            sdl2.SDL_DestroyTexture(frame)
            self._plumos_frame = None
        super().destroy()
