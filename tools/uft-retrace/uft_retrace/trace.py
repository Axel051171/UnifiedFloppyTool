"""Normalize and summarize emulator traces."""

from __future__ import annotations

import csv
import json
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any, Iterable

from .util import parse_int, sha256_file

REGISTERS = {
    0x00DFF01A: "DSKBYTR",
    0x00DFF020: "DSKPTH",
    0x00DFF022: "DSKPTL",
    0x00DFF024: "DSKLEN",
    0x00DFF026: "DSKDAT",
    0x00DFF096: "DMACON",
    0x00DFF09C: "INTREQ",
    0x00BFD100: "CIAB_PRB",
}


def normalize_event(raw: dict[str, Any], sequence: int) -> dict[str, Any]:
    event = {
        "seq": parse_int(raw.get("seq", sequence)),
        "cycle": parse_int(raw.get("cycle", 0)),
        "pc": parse_int(raw["pc"]),
        "type": str(raw["type"]),
    }
    for key in ("address", "size", "value", "target"):
        if key in raw and raw[key] not in (None, ""):
            event[key] = parse_int(raw[key])
    if "note" in raw:
        event["note"] = str(raw["note"])
    if "data" in raw:
        event["data"] = str(raw["data"])
    if "address" in event and event["address"] in REGISTERS:
        event["register"] = REGISTERS[event["address"]]
    return event


def load_trace(path: Path) -> list[dict[str, Any]]:
    events = []
    if path.suffix.lower() == ".csv":
        with path.open(newline="", encoding="utf-8-sig") as stream:
            source: Iterable[dict[str, Any]] = csv.DictReader(stream)
            for sequence, raw in enumerate(source):
                events.append(normalize_event(raw, sequence))
    else:
        with path.open(encoding="utf-8-sig") as stream:
            for sequence, line in enumerate(stream):
                if line.strip():
                    events.append(normalize_event(json.loads(line), sequence))
    events.sort(key=lambda item: (item["seq"], item["cycle"]))
    return events


def summarize_trace(path: Path) -> dict:
    events = load_trace(path)
    types = Counter(event["type"] for event in events)
    registers = Counter(event.get("register") for event in events
                        if event.get("register"))
    pc_pages = Counter(event["pc"] & ~0xFF for event in events)
    register_pcs: dict[str, Counter] = defaultdict(Counter)
    for event in events:
        if "register" in event:
            register_pcs[event["register"]][event["pc"]] += 1

    # A jump to an address outside the previous 64-KiB region is a useful
    # unpacker handoff candidate, not proof by itself.
    handoffs = []
    for left, right in zip(events, events[1:]):
        if (left["pc"] >> 16) != (right["pc"] >> 16):
            handoffs.append({"seq": right["seq"], "from_pc": left["pc"],
                             "to_pc": right["pc"],
                             "evidence": "pc-region-transition"})

    return {
        "schema": "uft-retrace-trace-summary-1",
        "source": {"name": path.name, "sha256": sha256_file(path)},
        "event_count": len(events),
        "first_pc": events[0]["pc"] if events else None,
        "last_pc": events[-1]["pc"] if events else None,
        "event_types": dict(sorted(types.items())),
        "register_accesses": dict(sorted(registers.items())),
        "hot_pc_pages": [
            {"page": page, "count": count}
            for page, count in pc_pages.most_common(32)
        ],
        "register_pcs": {
            register: [{"pc": pc, "count": count}
                       for pc, count in counts.most_common(32)]
            for register, counts in sorted(register_pcs.items())
        },
        "handoff_candidates": handoffs[:128],
    }
