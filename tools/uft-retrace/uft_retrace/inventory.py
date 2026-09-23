"""Input inventory, safe extraction and static marker collection."""

from __future__ import annotations

import json
import shutil
import zipfile
from pathlib import Path

from .hunk import HunkError, describe_hunk, is_hunk, parse_hunk
from .util import relative_posix, sha256_file, stable_files, tool_identity, write_json


def safe_extract_zip(archive: Path, destination: Path,
                     max_members: int = 10_000,
                     max_unpacked_bytes: int = 2 * 1024 * 1024 * 1024) -> None:
    destination.mkdir(parents=True, exist_ok=True)
    root = destination.resolve()
    with zipfile.ZipFile(archive) as zf:
        members = zf.infolist()
        if len(members) > max_members:
            raise ValueError(f"ZIP enthält zu viele Einträge: {len(members)}")
        total = sum(info.file_size for info in members)
        if total > max_unpacked_bytes:
            raise ValueError(f"ZIP ist entpackt zu groß: {total} Bytes")
        for info in members:
            target = (destination / info.filename).resolve()
            if target != root and root not in target.parents:
                raise ValueError(f"unsicherer ZIP-Pfad: {info.filename}")
            if info.is_dir():
                target.mkdir(parents=True, exist_ok=True)
                continue
            target.parent.mkdir(parents=True, exist_ok=True)
            with zf.open(info) as source, target.open("wb") as sink:
                shutil.copyfileobj(source, sink)


def static_markers(data: bytes) -> list[dict]:
    registers = {
        0x00DFF01A: "DSKBYTR",
        0x00DFF020: "DSKPTH",
        0x00DFF022: "DSKPTL",
        0x00DFF024: "DSKLEN",
        0x00DFF026: "DSKDAT",
        0x00DFF096: "DMACON",
        0x00DFF09C: "INTREQ",
        0x00BFD100: "CIAB_PRB",
    }
    hits: list[dict] = []
    for address, name in registers.items():
        needle = address.to_bytes(4, "big")
        start = 0
        while True:
            offset = data.find(needle, start)
            if offset < 0:
                break
            hits.append({"offset": offset, "kind": "absolute_address",
                         "name": name, "value": address,
                         "evidence": "raw-byte-hit-only"})
            start = offset + 1
    for value, name in ((0x4489, "MFM_SYNC_4489"), (0x1600, "TRACK_5632"),
                        (0x1800, "BUFFER_6144"), (0x0370, "ROOT_BLOCK_880")):
        needle = value.to_bytes(2, "big")
        start = 0
        while True:
            offset = data.find(needle, start)
            if offset < 0:
                break
            hits.append({"offset": offset, "kind": "constant",
                         "name": name, "value": value,
                         "evidence": "raw-byte-hit-only"})
            start = offset + 1
    return sorted(hits, key=lambda item: (item["offset"], item["name"]))


def create_inventory(source: Path, output: Path) -> dict:
    output.mkdir(parents=True, exist_ok=True)
    extracted = output / "input"
    if extracted.exists():
        shutil.rmtree(extracted)
    extracted.mkdir()

    if source.is_dir():
        for path in stable_files(source):
            target = extracted / path.relative_to(source)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, target)
        source_kind = "directory"
    elif zipfile.is_zipfile(source):
        safe_extract_zip(source, extracted)
        source_kind = "zip"
    else:
        shutil.copy2(source, extracted / source.name)
        source_kind = "file"

    records = []
    hunk_dir = output / "hunks"
    hunk_dir.mkdir(exist_ok=True)
    for path in stable_files(extracted):
        data = path.read_bytes()
        record = {
            "path": relative_posix(path, extracted),
            "size": len(data),
            "sha256": sha256_file(path),
            "is_hunk": is_hunk(data),
            "markers": static_markers(data),
        }
        if record["is_hunk"]:
            try:
                hunk = parse_hunk(data)
                record["hunk"] = describe_hunk(hunk)
                stem = record["sha256"][:16]
                for segment in hunk.segments:
                    if segment.kind in ("CODE", "DATA"):
                        name = f"{stem}.seg{segment.index}.{segment.kind.lower()}.bin"
                        (hunk_dir / name).write_bytes(segment.payload)
            except HunkError as exc:
                record["hunk_error"] = str(exc)
        records.append(record)

    manifest = {
        "schema": "uft-retrace-inventory-1",
        "tool": tool_identity(),
        "source": {
            "kind": source_kind,
            "name": source.name,
            "sha256": sha256_file(source) if source.is_file() else None,
        },
        "files": records,
    }
    write_json(output / "inventory.json", manifest)
    return manifest
