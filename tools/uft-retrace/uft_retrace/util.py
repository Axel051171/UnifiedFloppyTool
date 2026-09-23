"""Shared deterministic and bounds-safe helpers."""

from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
from typing import Any


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def write_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    text = json.dumps(value, indent=2, sort_keys=True, ensure_ascii=False)
    path.write_text(text + "\n", encoding="utf-8", newline="\n")


def read_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


def parse_int(value: Any) -> int:
    if isinstance(value, int):
        return value
    if isinstance(value, str):
        return int(value, 0)
    raise ValueError(f"keine Ganzzahl: {value!r}")


def relative_posix(path: Path, root: Path) -> str:
    return path.relative_to(root).as_posix()


def stable_files(root: Path):
    return sorted((p for p in root.rglob("*") if p.is_file()),
                  key=lambda p: relative_posix(p, root))


def tool_identity() -> dict[str, Any]:
    return {
        "name": "uft-retrace",
        "version": "0.1.0",
        "python": os.sys.version.split()[0],
    }
