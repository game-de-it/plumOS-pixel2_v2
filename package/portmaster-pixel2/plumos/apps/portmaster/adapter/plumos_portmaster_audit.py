#!/usr/bin/env python3
"""Audit an installed PortMaster port without executing its content.

The Pixel2 PortMaster adapter owns the hardware boundary.  Installed ports are
mutable third-party data, so this scanner reports compatibility requirements
without rewriting launchers, binaries, settings, or saves.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import stat
import struct
import sys
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Iterable


ELF_MAGIC = b"\x7fELF"
PT_LOAD = 1
PT_DYNAMIC = 2
PT_INTERP = 3
DT_NULL = 0
DT_NEEDED = 1
DT_STRTAB = 5
DT_STRSZ = 10
DT_SONAME = 14
DT_RPATH = 15
DT_RUNPATH = 29
DEFAULT_MACHINE = 183  # EM_AARCH64
MAX_FILES = 20000
MAX_TEXT_BYTES = 2 * 1024 * 1024
AUDIT_POLICY_VERSION = 2


@dataclass(frozen=True)
class ElfInfo:
    path: str
    elf_class: int
    machine: int
    elf_type: int
    has_interp: bool
    needed: tuple[str, ...]
    soname: str | None
    rpath: str | None
    runpath: str | None


@dataclass(frozen=True)
class Finding:
    severity: str
    code: str
    path: str
    detail: str


class ElfError(ValueError):
    pass


def _read_cstring(data: bytes, offset: int, limit: int) -> str:
    if offset < 0 or offset >= len(data) or offset >= limit:
        raise ElfError("string offset is outside the ELF file")
    end = data.find(b"\0", offset, min(limit, len(data)))
    if end < 0:
        raise ElfError("unterminated ELF string")
    return data[offset:end].decode("utf-8", "replace")


def parse_elf(path: Path) -> ElfInfo:
    data = path.read_bytes()
    if len(data) < 64 or data[:4] != ELF_MAGIC:
        raise ElfError("not an ELF file")
    elf_class = data[4]
    encoding = data[5]
    if elf_class not in (1, 2) or encoding not in (1, 2):
        raise ElfError("unsupported ELF class or byte order")
    endian = "<" if encoding == 1 else ">"
    if elf_class == 2:
        header_fmt = endian + "HHIQQQIHHHHHH"
        header = struct.unpack_from(header_fmt, data, 16)
        elf_type, machine = header[0], header[1]
        phoff, phentsize, phnum = header[4], header[8], header[9]
        ph_fmt = endian + "IIQQQQQQ"
        dyn_fmt = endian + "qQ"
    else:
        header_fmt = endian + "HHIIIIIHHHHHH"
        header = struct.unpack_from(header_fmt, data, 16)
        elf_type, machine = header[0], header[1]
        phoff, phentsize, phnum = header[4], header[8], header[9]
        ph_fmt = endian + "IIIIIIII"
        dyn_fmt = endian + "iI"
    ph_size = struct.calcsize(ph_fmt)
    dyn_size = struct.calcsize(dyn_fmt)
    if phentsize < ph_size or phnum > 4096:
        raise ElfError("invalid ELF program-header table")

    loads: list[tuple[int, int, int, int]] = []
    dynamic: tuple[int, int] | None = None
    has_interp = False
    for index in range(phnum):
        offset = phoff + index * phentsize
        if offset < 0 or offset + ph_size > len(data):
            raise ElfError("truncated ELF program-header table")
        fields = struct.unpack_from(ph_fmt, data, offset)
        if elf_class == 2:
            p_type, p_offset, p_vaddr, p_filesz = fields[0], fields[2], fields[3], fields[5]
        else:
            p_type, p_offset, p_vaddr, p_filesz = fields[0], fields[1], fields[2], fields[4]
        if p_type == PT_LOAD:
            loads.append((p_vaddr, p_vaddr + p_filesz, p_offset, p_filesz))
        elif p_type == PT_DYNAMIC:
            dynamic = (p_offset, p_filesz)
        elif p_type == PT_INTERP:
            has_interp = True

    needed_offsets: list[int] = []
    string_offsets: dict[int, int] = {}
    strtab_address: int | None = None
    strtab_size = 0
    if dynamic:
        dyn_offset, dyn_length = dynamic
        end = min(len(data), dyn_offset + dyn_length)
        for offset in range(dyn_offset, end - dyn_size + 1, dyn_size):
            tag, value = struct.unpack_from(dyn_fmt, data, offset)
            if tag == DT_NULL:
                break
            if tag == DT_NEEDED:
                needed_offsets.append(value)
            elif tag == DT_STRTAB:
                strtab_address = value
            elif tag == DT_STRSZ:
                strtab_size = value
            elif tag in (DT_SONAME, DT_RPATH, DT_RUNPATH):
                string_offsets[tag] = value

    def virtual_to_file(address: int) -> int:
        for start, end, file_offset, _ in loads:
            if start <= address < end:
                return file_offset + address - start
        raise ElfError("dynamic string table is outside loadable segments")

    needed: tuple[str, ...] = ()
    soname = rpath = runpath = None
    if strtab_address is not None:
        strtab = virtual_to_file(strtab_address)
        limit = strtab + strtab_size if strtab_size else len(data)
        needed = tuple(_read_cstring(data, strtab + value, limit) for value in needed_offsets)
        if DT_SONAME in string_offsets:
            soname = _read_cstring(data, strtab + string_offsets[DT_SONAME], limit)
        if DT_RPATH in string_offsets:
            rpath = _read_cstring(data, strtab + string_offsets[DT_RPATH], limit)
        if DT_RUNPATH in string_offsets:
            runpath = _read_cstring(data, strtab + string_offsets[DT_RUNPATH], limit)

    return ElfInfo(
        path=str(path),
        elf_class=elf_class,
        machine=machine,
        elf_type=elf_type,
        has_interp=has_interp,
        needed=needed,
        soname=soname,
        rpath=rpath,
        runpath=runpath,
    )


def iter_files(root: Path, *, include_file_symlinks: bool = False) -> Iterable[Path]:
    count = 0
    for directory, dirnames, filenames in os.walk(root):
        dirnames.sort()
        filenames.sort()
        for name in filenames:
            count += 1
            if count > MAX_FILES:
                raise RuntimeError(f"port contains more than {MAX_FILES} files")
            path = Path(directory, name)
            if not path.is_file():
                continue
            if path.is_symlink() and not include_file_symlinks:
                continue
            yield path


def collect_text(paths: Iterable[Path]) -> tuple[str, dict[str, str]]:
    corpus: list[str] = []
    texts: dict[str, str] = {}
    for path in paths:
        if path.suffix.lower() not in {".sh", ".txt", ".lua", ".py", ".json"}:
            continue
        try:
            if path.stat().st_size > MAX_TEXT_BYTES:
                continue
            text = path.read_text("utf-8", "replace")
        except OSError:
            continue
        texts[str(path)] = text
        corpus.append(text)
    return "\n".join(corpus), texts


def environment_findings(texts: dict[str, str]) -> list[Finding]:
    findings: list[Finding] = []
    assignment = re.compile(
        r"(?m)^\s*(?:export\s+)?(LD_LIBRARY_PATH|LD_PRELOAD)\s*=\s*([^\n#]+)"
    )
    for path, text in texts.items():
        if re.search(r"(?m)^\s*(?:export\s+)?PORT_32BIT\s*=\s*['\"]?[Yy]", text):
            findings.append(Finding("error", "unsupported_armhf", path, "PORT_32BIT=Y"))
        for match in assignment.finditer(text):
            name, value = match.groups()
            if f"${name}" not in value and f"${{{name}" not in value:
                findings.append(
                    Finding(
                        "warning",
                        "environment_replaced",
                        path,
                        f"{name} assignment does not retain the inherited value",
                    )
                )
        for command in ("sudo", "service", "modprobe", "xrandr", "weston", "Xorg"):
            if re.search(rf"(?m)(?:^|[;&|]\s*|\s){re.escape(command)}(?:\s|$)", text):
                findings.append(
                    Finding("warning", "host_command", path, f"host command referenced: {command}")
                )
        for match in re.finditer(r"(?m)(?:^|[;&|]\s*|\s)systemctl\s+([^\n;&|]+)", text):
            args = " ".join(match.group(1).split())
            if not args.startswith("restart oga_events"):
                findings.append(
                    Finding("warning", "host_command", path, f"unsupported systemctl use: {args}")
                )
    return findings


def index_libraries(infos: Iterable[ElfInfo]) -> dict[str, ElfInfo]:
    result: dict[str, ElfInfo] = {}
    for info in infos:
        path = Path(info.path)
        result.setdefault(path.name, info)
        if info.soname:
            result.setdefault(info.soname, info)
    return result


def compatibility_audit(
    script: Path,
    port_root: Path,
    library_dirs: list[Path],
    allowed_machine: int,
) -> dict[str, object]:
    port_files = list(iter_files(port_root))
    all_text_paths = [script] + [path for path in port_files if path != script]
    corpus, texts = collect_text(all_text_paths)
    findings = environment_findings(texts)
    port_elfs: list[ElfInfo] = []
    library_elfs: list[ElfInfo] = []
    parse_errors: list[Finding] = []

    for path in port_files:
        try:
            with path.open("rb") as stream:
                is_elf = stream.read(4) == ELF_MAGIC
        except OSError:
            continue
        if not is_elf:
            continue
        try:
            info = parse_elf(path)
        except (OSError, ElfError) as exc:
            parse_errors.append(Finding("warning", "elf_parse", str(path), str(exc)))
            continue
        port_elfs.append(info)
        if info.machine != allowed_machine:
            name = Path(info.path).name
            referenced = info.has_interp and bool(
                re.search(
                    rf"(?<![A-Za-z0-9_.-]){re.escape(name)}(?![A-Za-z0-9_.-])",
                    corpus,
                )
            )
            findings.append(
                Finding(
                    "error" if referenced else "warning",
                    "unsupported_machine",
                    str(path),
                    f"ELF machine {info.machine}, expected {allowed_machine}",
                )
            )

    for directory in library_dirs:
        if not directory.is_dir():
            continue
        # Runtime SONAME directories intentionally consist mostly of links to
        # component-owned libraries.  Index the link name as well as the
        # target ELF's DT_SONAME, but keep port-content links excluded so an
        # installed port cannot make the audit escape its own tree.
        for path in iter_files(directory, include_file_symlinks=True):
            try:
                with path.open("rb") as stream:
                    if stream.read(4) != ELF_MAGIC:
                        continue
                library_elfs.append(parse_elf(path))
            except (OSError, ElfError):
                continue

    index = index_libraries([*port_elfs, *library_elfs])
    checked: set[tuple[str, bool]] = set()

    def check_closure(info: ElfInfo, referenced: bool) -> None:
        key = (info.path, referenced)
        if key in checked:
            return
        checked.add(key)
        for soname in info.needed:
            dependency = index.get(soname)
            if dependency is None:
                findings.append(
                    Finding(
                        "error" if referenced else "warning",
                        "missing_soname",
                        info.path,
                        soname,
                    )
                )
            else:
                check_closure(dependency, referenced)

    for info in port_elfs:
        name = Path(info.path).name
        referenced = info.has_interp and bool(re.search(rf"(?<![A-Za-z0-9_.-]){re.escape(name)}(?![A-Za-z0-9_.-])", corpus))
        check_closure(info, referenced)

    findings.extend(parse_errors)
    unique = sorted(
        {finding for finding in findings},
        key=lambda item: (item.severity, item.code, item.path, item.detail),
    )
    errors = sum(item.severity == "error" for item in unique)
    warnings = sum(item.severity == "warning" for item in unique)
    status = "blocked" if errors else "warning" if warnings else "compatible"
    return {
        "schema": 1,
        "device": "pixel2",
        "script": str(script),
        "port_root": str(port_root),
        "status": status,
        "errors": errors,
        "warnings": warnings,
        "elf_files": len(port_elfs),
        "findings": [asdict(item) for item in unique],
    }


def audit_key(script: Path, port_root: Path) -> str:
    digest = hashlib.sha256()
    digest.update(f"policy={AUDIT_POLICY_VERSION}\0".encode())
    for path in sorted({script, *iter_files(port_root)}, key=lambda item: str(item)):
        relative = str(path.relative_to(port_root)) if path.is_relative_to(port_root) else str(path)
        digest.update(relative.encode("utf-8", "surrogateescape"))
        digest.update(b"\0")
        try:
            metadata = path.stat()
            digest.update(str(metadata.st_size).encode())
            digest.update(b":")
            digest.update(str(metadata.st_mtime_ns).encode())
        except OSError:
            digest.update(b"missing")
        digest.update(b"\0")
    return digest.hexdigest()


def write_atomic(path: Path, payload: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(f".{path.name}.tmp.{os.getpid()}")
    temporary.write_text(payload, encoding="utf-8")
    os.replace(temporary, path)


def discover_port_root(script: Path, ports_root: Path) -> Path | None:
    try:
        text = script.read_text("utf-8", "replace")
    except OSError:
        return None
    for name in re.findall(r"/ports/([^/\"'\s;$]+)", text, flags=re.IGNORECASE):
        candidate = ports_root / name
        if candidate.is_dir():
            return candidate.resolve()
    return None


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--script", required=True, type=Path)
    root_group = parser.add_mutually_exclusive_group(required=True)
    root_group.add_argument("--port-root", type=Path)
    root_group.add_argument("--ports-root", type=Path)
    parser.add_argument("--library-dir", action="append", default=[], type=Path)
    parser.add_argument("--allowed-machine", type=int, default=DEFAULT_MACHINE)
    parser.add_argument("--cache-dir", type=Path)
    parser.add_argument("--no-cache", action="store_true")
    parser.add_argument("--enforce", action="store_true")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args(argv)

    script = args.script.resolve()
    if args.port_root:
        port_root = args.port_root.resolve()
    else:
        ports_root = args.ports_root.resolve()
        port_root = discover_port_root(script, ports_root)
        if port_root is None:
            parser.error(f"cannot discover port root from launcher: {script}")
    if not script.is_file() or not port_root.is_dir():
        parser.error("script and port root must exist")
    key = audit_key(script, port_root)
    cache_path = args.cache_dir / f"{key}.json" if args.cache_dir else None
    if cache_path and cache_path.is_file() and not args.no_cache:
        payload = cache_path.read_text("utf-8")
        report = json.loads(payload)
        report["cache"] = "hit"
        payload = json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True) + "\n"
    else:
        report = compatibility_audit(script, port_root, args.library_dir, args.allowed_machine)
        report["audit_key"] = key
        report["cache"] = "miss"
        payload = json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True) + "\n"
        if cache_path:
            write_atomic(cache_path, payload)
    if args.output:
        write_atomic(args.output, payload)
    else:
        sys.stdout.write(payload)
    return 78 if args.enforce and report["status"] == "blocked" else 0


if __name__ == "__main__":
    raise SystemExit(main())
