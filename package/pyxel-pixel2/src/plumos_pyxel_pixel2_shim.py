"""Publish Pyxel canvas geometry to the Pixel2 GL presenter.

The official Pyxel package remains unmodified.  This wrapper observes the
public ``pyxel.init`` call before SDL creates its GL context, exports the real
logical canvas size, and then runs the original Python command.
"""

from __future__ import annotations

import inspect
import os
import runpy
import shutil
import sys
import tempfile
from pathlib import Path


def _is_disabled(value: str | None) -> bool:
    return value is not None and value.lower() in {"0", "false", "no", "off", "none"}


def _as_positive_int(value: object) -> int | None:
    try:
        number = int(value)  # type: ignore[arg-type]
    except (TypeError, ValueError):
        return None
    return number if number > 0 else None


def _extract_init_value(
    name: str,
    index: int,
    args: tuple[object, ...],
    kwargs: dict[str, object],
    bound: dict[str, object],
) -> object | None:
    if name in bound:
        return bound[name]
    if name in kwargs:
        return kwargs[name]
    if len(args) > index:
        return args[index]
    return None


def _publish_pyxel_init_geometry(
    original_init, args: tuple[object, ...], kwargs: dict[str, object]
) -> None:
    bound: dict[str, object] = {}
    try:
        signature = inspect.signature(original_init)
        bound = dict(signature.bind_partial(*args, **kwargs).arguments)
    except (TypeError, ValueError):
        pass

    width = _as_positive_int(_extract_init_value("width", 0, args, kwargs, bound))
    height = _as_positive_int(_extract_init_value("height", 1, args, kwargs, bound))
    display_scale = _as_positive_int(
        _extract_init_value("display_scale", 5, args, kwargs, bound)
    )
    if width is None or height is None:
        return

    logical_width = width
    logical_height = height
    if display_scale is not None:
        logical_width *= display_scale
        logical_height *= display_scale

    os.environ["PLUMOS_PYXEL_CANVAS_SIZE"] = f"{width}x{height}"
    os.environ["PLUMOS_PYXEL_LOGICAL_SIZE"] = f"{logical_width}x{logical_height}"

    if not _is_disabled(os.environ.get("PLUMOS_PYXEL_INIT_SHIM_LOG")):
        print(
            "plumOS Pixel2 Pyxel init shim "
            f"canvas={width}x{height} "
            f"display_scale={display_scale or 'auto'} "
            f"logical={logical_width}x{logical_height}",
            file=sys.stderr,
            flush=True,
        )


def _patch_pyxel_init() -> None:
    if _is_disabled(os.environ.get("PLUMOS_PYXEL_INIT_SHIM")):
        return

    try:
        import pyxel  # type: ignore
    except Exception:
        return

    original_init = getattr(pyxel, "init", None)
    if original_init is None or getattr(original_init, "_plumos_pixel2_wrapped", False):
        return

    def init_wrapper(*args, **kwargs):
        cwd = Path.cwd()
        _publish_pyxel_init_geometry(original_init, args, kwargs)
        result = original_init(*args, **kwargs)
        if not _is_disabled(os.environ.get("PLUMOS_PYXEL_RESTORE_CWD_AFTER_INIT")):
            os.chdir(cwd)
        return result

    init_wrapper._plumos_pixel2_wrapped = True  # type: ignore[attr-defined]
    init_wrapper.__name__ = getattr(original_init, "__name__", "init")
    init_wrapper.__doc__ = getattr(original_init, "__doc__", None)
    pyxel.init = init_wrapper


def _cleanup_stale_play_dirs(play_dir: Path, pid_exists) -> None:
    """Remove only dead Pyxel play extractions from the controlled temp root."""
    if not play_dir.is_dir():
        return
    for path in play_dir.iterdir():
        if not path.is_dir():
            continue
        try:
            pid = int(path.name.split("_", 1)[0])
        except ValueError:
            continue
        if pid_exists(pid):
            continue
        shutil.rmtree(path, ignore_errors=True)


def _run_pyxel_play(argv: list[str]) -> None:
    if not argv:
        raise SystemExit("plumos_pyxel_pixel2_shim: pyxel play requires a .pyxapp path")

    import pyxel  # type: ignore
    import pyxel.cli as pyxel_cli  # type: ignore

    play_dir = Path(tempfile.gettempdir()) / pyxel.BASE_DIR / "play"
    _cleanup_stale_play_dirs(play_dir, pyxel._pid_exists)

    pyxel_app_file = argv[0]
    if Path(pyxel_app_file).suffix.lower() != ".zip":
        pyxel_app_file = pyxel_cli._complete_extension(
            pyxel_app_file, "play", pyxel.APP_FILE_EXTENSION
        )
    pyxel_cli._check_file_exists(pyxel_app_file)
    pyxel_cli.print_pyxel_app_metadata(pyxel_app_file)
    startup_script_file = pyxel_cli._extract_pyxel_app(pyxel_app_file)
    if not startup_script_file:
        pyxel_cli._exit_with_error(f"file not found: '{pyxel.APP_STARTUP_SCRIPT_FILE}'")

    startup_script = Path(startup_script_file).absolute()
    startup_dir = startup_script.parent
    old_cwd = Path.cwd()
    sys.path.insert(0, str(startup_dir))
    try:
        os.chdir(startup_dir)
        runpy.run_path(str(startup_script), run_name="__main__")
    finally:
        os.chdir(old_cwd)
        try:
            sys.path.remove(str(startup_dir))
        except ValueError:
            pass
        extraction_root = startup_dir.parent
        try:
            controlled_root = play_dir.resolve()
            if extraction_root.resolve().parent == controlled_root:
                shutil.rmtree(extraction_root, ignore_errors=True)
        except OSError:
            pass


def _run_python_command(argv: list[str]) -> None:
    if not argv:
        raise SystemExit("plumos_pyxel_pixel2_shim: missing Python command")
    if argv[0] == "-m":
        if len(argv) < 2:
            raise SystemExit("plumos_pyxel_pixel2_shim: -m requires a module name")
        module = argv[1]
        if module == "pyxel" and len(argv) >= 3 and argv[2] == "play":
            sys.argv = [module, *argv[2:]]
            _run_pyxel_play(argv[3:])
            return
        sys.argv = [module, *argv[2:]]
        runpy.run_module(module, run_name="__main__", alter_sys=True)
        return
    if argv[0] == "-c":
        if len(argv) < 2:
            raise SystemExit("plumos_pyxel_pixel2_shim: -c requires code")
        sys.argv = ["-c", *argv[2:]]
        namespace = {"__name__": "__main__", "__package__": None}
        exec(argv[1], namespace)
        return

    script = Path(argv[0])
    if script.parent:
        sys.path[0] = str(script.resolve().parent)
    sys.argv = argv
    runpy.run_path(str(script), run_name="__main__")


def main() -> None:
    _patch_pyxel_init()
    _run_python_command(sys.argv[1:])


if __name__ == "__main__":
    main()
