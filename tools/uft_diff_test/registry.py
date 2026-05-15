"""
uft_diff_test.registry — loader for the Divergence Registry.

The registry (tests/conformance/divergence_registry.yaml) documents
every *legitimate* difference between `uft` output and `gw` output.
Each entry is simultaneously a test spec and its own documentation:
a DIVERGE_OK result is only allowed if a matching entry exists.

Strategy: docs/TESTER_STRATEGY.md §2.4.
"""

from __future__ import annotations

import re
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Pattern

import yaml

__all__ = ["DivergenceEntry", "DivergenceRegistry", "load_registry"]

_REPO_ROOT = Path(__file__).resolve().parents[2]
_DEFAULT_REGISTRY = (
    _REPO_ROOT / "tests" / "conformance" / "divergence_registry.yaml"
)


@dataclass(frozen=True)
class DivergenceEntry:
    """One documented, legitimate UFT↔GW divergence."""

    id: str               # "DIV-001"
    command: str          # GW command the divergence applies to ("*" = any)
    summary: str          # one-line description
    reason: str           # why the divergence is intentional
    scope: str            # what part of the output differs
    mask: Pattern | None = None   # per-line regex neutralising the divergence

    @classmethod
    def from_dict(cls, raw: dict) -> "DivergenceEntry":
        missing = [
            k
            for k in ("id", "command", "summary", "reason", "scope")
            if k not in raw
        ]
        if missing:
            raise ValueError(
                f"divergence entry {raw.get('id', '<no id>')} is missing "
                f"required field(s): {', '.join(missing)}"
            )
        mask_raw = raw.get("mask")
        mask: Pattern | None = None
        if mask_raw is not None:
            try:
                mask = re.compile(str(mask_raw))
            except re.error as exc:
                raise ValueError(
                    f"divergence entry {raw['id']}: 'mask' is not a valid "
                    f"regex: {exc}"
                ) from exc
        return cls(
            id=str(raw["id"]),
            command=str(raw["command"]),
            summary=str(raw["summary"]),
            reason=str(raw["reason"]),
            scope=str(raw["scope"]),
            mask=mask,
        )


class DivergenceRegistry:
    """In-memory view of the divergence registry."""

    def __init__(self, entries: Iterable[DivergenceEntry]):
        self._by_id: dict[str, DivergenceEntry] = {}
        for e in entries:
            if e.id in self._by_id:
                raise ValueError(f"duplicate divergence id: {e.id}")
            self._by_id[e.id] = e

    def __len__(self) -> int:
        return len(self._by_id)

    def __contains__(self, div_id: str) -> bool:
        return div_id in self._by_id

    def __iter__(self):
        return iter(self._by_id.values())

    def get(self, div_id: str) -> DivergenceEntry | None:
        return self._by_id.get(div_id)

    def for_command(self, command: str) -> list[DivergenceEntry]:
        """All entries that apply to `command` (plus the '*' wildcards)."""
        return [
            e
            for e in self._by_id.values()
            if e.command == command or e.command == "*"
        ]


def load_registry(path: Path | None = None) -> DivergenceRegistry:
    """
    Load and validate the divergence registry. Raises on malformed YAML
    or on an entry missing a required field — a broken registry is a
    setup error, surfaced loudly, never silently tolerated.
    """
    path = Path(path) if path is not None else _DEFAULT_REGISTRY
    if not path.exists():
        raise FileNotFoundError(f"divergence registry not found: {path}")

    with open(path, "r", encoding="utf-8") as fh:
        doc = yaml.safe_load(fh) or {}

    raw_entries = doc.get("divergences", [])
    if not isinstance(raw_entries, list):
        raise ValueError(
            f"{path}: top-level 'divergences' must be a list, "
            f"got {type(raw_entries).__name__}"
        )

    return DivergenceRegistry(
        DivergenceEntry.from_dict(raw) for raw in raw_entries
    )
