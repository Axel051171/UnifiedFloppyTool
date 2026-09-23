"""Minimal, strict Amiga Hunk parser used for evidence extraction."""

from __future__ import annotations

import math
import struct
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

HUNK_UNIT = 999
HUNK_NAME = 1000
HUNK_CODE = 1001
HUNK_DATA = 1002
HUNK_BSS = 1003
HUNK_RELOC32 = 1004
HUNK_RELOC16 = 1005
HUNK_RELOC8 = 1006
HUNK_EXT = 1007
HUNK_SYMBOL = 1008
HUNK_DEBUG = 1009
HUNK_END = 1010
HUNK_HEADER = 1011

TYPE_MASK = 0x3FFFFFFF
MEM_MASK = 0xC0000000


class HunkError(ValueError):
    pass


class Reader:
    def __init__(self, data: bytes):
        self.data = data
        self.offset = 0

    def remaining(self) -> int:
        return len(self.data) - self.offset

    def u32(self) -> int:
        if self.remaining() < 4:
            raise HunkError(f"unerwartetes Dateiende bei 0x{self.offset:x}")
        value = struct.unpack_from(">I", self.data, self.offset)[0]
        self.offset += 4
        return value

    def take(self, count: int) -> bytes:
        if count < 0 or count > self.remaining():
            raise HunkError(f"ungültige Länge {count} bei 0x{self.offset:x}")
        value = self.data[self.offset:self.offset + count]
        self.offset += count
        return value

    def name(self, words: Optional[int] = None) -> str:
        if words is None:
            words = self.u32()
        raw = self.take(words * 4)
        return raw.rstrip(b"\0").decode("latin-1", errors="replace")


@dataclass
class Relocation:
    target_hunk: int
    offsets: list[int]


@dataclass
class Segment:
    index: int
    kind: str
    memory_flags: int
    declared_words: int
    payload: bytes = b""
    bss_bytes: int = 0
    reloc32: list[Relocation] = field(default_factory=list)
    symbols: dict[str, int] = field(default_factory=dict)
    source_offset: int = 0

    @property
    def size_bytes(self) -> int:
        return self.bss_bytes if self.kind == "BSS" else len(self.payload)

    @property
    def entropy(self) -> float:
        if not self.payload:
            return 0.0
        counts = [0] * 256
        for value in self.payload:
            counts[value] += 1
        total = len(self.payload)
        return -sum((n / total) * math.log2(n / total)
                    for n in counts if n)


@dataclass
class HunkFile:
    table_size: int
    first_hunk: int
    last_hunk: int
    segments: list[Segment]
    resident_names: list[str]
    trailing_bytes: int


def is_hunk(data: bytes) -> bool:
    return len(data) >= 4 and struct.unpack_from(">I", data, 0)[0] == HUNK_HEADER


def parse_hunk(data: bytes) -> HunkFile:
    r = Reader(data)
    if r.u32() != HUNK_HEADER:
        raise HunkError("keine Amiga-Hunk-Datei")

    names: list[str] = []
    while True:
        words = r.u32()
        if words == 0:
            break
        names.append(r.name(words))

    table_size = r.u32()
    first = r.u32()
    last = r.u32()
    if table_size == 0 or last < first or last - first + 1 > table_size:
        raise HunkError("ungültige Hunk-Tabelle")
    if table_size > 4096:
        raise HunkError("unplausibel große Hunk-Tabelle")

    declared = [r.u32() for _ in range(table_size)]
    segments: list[Segment] = []

    while r.remaining() >= 4 and len(segments) < (last - first + 1):
        raw_type = r.u32()
        kind = raw_type & TYPE_MASK
        while kind in (HUNK_NAME, HUNK_DEBUG):
            words = r.u32()
            r.take(words * 4)
            raw_type = r.u32()
            kind = raw_type & TYPE_MASK

        index = len(segments)
        # The size table contains ``table_size`` entries in table order.  The
        # first/last values are hunk *numbers*, not offsets into that table.
        declared_raw = declared[index]
        declared_words = declared_raw & TYPE_MASK
        memory_flags = declared_raw & MEM_MASK
        source_offset = r.offset - 4

        if kind == HUNK_CODE:
            words = r.u32()
            segment = Segment(index, "CODE", memory_flags, declared_words,
                              payload=r.take(words * 4), source_offset=source_offset)
        elif kind == HUNK_DATA:
            words = r.u32()
            segment = Segment(index, "DATA", memory_flags, declared_words,
                              payload=r.take(words * 4), source_offset=source_offset)
        elif kind == HUNK_BSS:
            words = r.u32()
            segment = Segment(index, "BSS", memory_flags, declared_words,
                              bss_bytes=words * 4, source_offset=source_offset)
        else:
            raise HunkError(f"unerwarteter Hunk-Typ {kind} bei 0x{source_offset:x}")

        while r.remaining() >= 4:
            record_offset = r.offset
            record = r.u32() & TYPE_MASK
            if record == HUNK_END:
                break
            if record == HUNK_RELOC32:
                while True:
                    count = r.u32()
                    if count == 0:
                        break
                    target = r.u32()
                    if count > r.remaining() // 4:
                        raise HunkError("Relocation-Liste abgeschnitten")
                    segment.reloc32.append(Relocation(target,
                                                       [r.u32() for _ in range(count)]))
            elif record == HUNK_SYMBOL:
                while True:
                    words = r.u32()
                    if words == 0:
                        break
                    name = r.name(words)
                    segment.symbols[name] = r.u32()
            elif record == HUNK_DEBUG:
                r.take(r.u32() * 4)
            elif record == HUNK_NAME:
                r.take(r.u32() * 4)
            else:
                raise HunkError(f"nicht unterstützter Datensatz {record} "
                                f"bei 0x{record_offset:x}")
        else:
            raise HunkError("HUNK_END fehlt")

        segments.append(segment)

    if len(segments) != last - first + 1:
        raise HunkError("Segmentzahl stimmt nicht mit Hunk-Tabelle überein")

    return HunkFile(table_size, first, last, segments, names, r.remaining())


def parse_hunk_file(path: Path) -> HunkFile:
    return parse_hunk(path.read_bytes())


def describe_hunk(hunk: HunkFile) -> dict:
    return {
        "table_size": hunk.table_size,
        "first_hunk": hunk.first_hunk,
        "last_hunk": hunk.last_hunk,
        "resident_names": hunk.resident_names,
        "trailing_bytes": hunk.trailing_bytes,
        "segments": [
            {
                "index": s.index,
                "kind": s.kind,
                "memory_flags": s.memory_flags,
                "declared_words": s.declared_words,
                "size_bytes": s.size_bytes,
                "source_offset": s.source_offset,
                "entropy": round(s.entropy, 5),
                "likely_packed": s.kind in ("CODE", "DATA") and
                                 len(s.payload) >= 512 and s.entropy >= 7.2,
                "relocation_groups": [
                    {"target_hunk": x.target_hunk, "offsets": x.offsets}
                    for x in s.reloc32
                ],
                "symbols": s.symbols,
            }
            for s in hunk.segments
        ],
    }
