"""
uft_diff_test — differential testing harness for UFT ↔ Greaseweazle.

Strategy: docs/TESTER_STRATEGY.md §2 + §5.

The contract: for every GW command, run `gw <cmd>` and `uft <cmd>` on
identical input and compare outputs byte-for-byte. Four outcomes:

    IDENT        outputs byte-identical                      → PASS
    DIVERGE_OK   outputs differ, but the divergence is in     → PASS
                 the registry with a documented reason
    DIVERGE_BAD  outputs differ, NOT in the registry          → FAIL
    FAIL         a tool crashed / produced no output          → FAIL

> **Status: SKELETON (P3.1, Welle 1).** `differential_command()` does
> NOT yet execute `gw`/`uft`. It returns a `SKELETON` result and the
> real subprocess wiring + byte comparison land in Welle 2 (P3.2).
> `DiffResult.assert_pass()` therefore *skips* on a SKELETON result so
> the suite stays honest: a skeleton test is not a passing gw-compat
> proof, it is a not-yet-implemented one.

Usage (the shape Welle 2 tests will use unchanged):

    from uft_diff_test import differential_command, corpus

    def test_gw_read_ibm_360k():
        differential_command(
            command="read",
            args=["--format=ibm.dos.360"],
            input_file=corpus("inputs/flux_streams/ibm_360k_dos_boot.scp"),
        ).assert_pass()
"""

from __future__ import annotations

import enum
import hashlib
from dataclasses import dataclass, field
from pathlib import Path
from typing import Sequence

from .registry import DivergenceRegistry, load_registry

__all__ = [
    "DiffStatus",
    "DiffResult",
    "differential_command",
    "corpus",
    "corpus_root",
    "sha256_file",
    "DivergenceRegistry",
    "load_registry",
]

# --------------------------------------------------------------------------
# Corpus path helpers
# --------------------------------------------------------------------------

# tools/uft_diff_test/__init__.py → parents[2] is the repo root.
_REPO_ROOT = Path(__file__).resolve().parents[2]
_CORPUS_ROOT = _REPO_ROOT / "tests" / "gw_corpus"


def corpus_root() -> Path:
    """Absolute path to tests/gw_corpus/."""
    return _CORPUS_ROOT


def corpus(relpath: str) -> Path:
    """
    Resolve a path inside the gw_corpus. Raises if it does not exist —
    a missing corpus input is a test-setup bug, not a test failure.
    """
    p = _CORPUS_ROOT / relpath
    if not p.exists():
        raise FileNotFoundError(
            f"corpus input not found: {relpath}\n"
            f"  looked in: {p}\n"
            f"  fix: add the file under tests/gw_corpus/ and update "
            f"MANIFEST.sha256"
        )
    return p


def sha256_file(path: Path) -> str:
    """Hex sha256 of a file's contents."""
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


# --------------------------------------------------------------------------
# Differential result
# --------------------------------------------------------------------------


class DiffStatus(enum.Enum):
    IDENT = "IDENT"               # byte-identical → PASS
    DIVERGE_OK = "DIVERGE_OK"     # differs, registered reason → PASS
    DIVERGE_BAD = "DIVERGE_BAD"   # differs, no registry entry → FAIL
    FAIL = "FAIL"                 # a tool crashed / no output → FAIL
    SKELETON = "SKELETON"         # harness not yet wired (Welle 1) → SKIP


@dataclass
class DiffResult:
    """Outcome of one differential_command() comparison."""

    status: DiffStatus
    command: str
    args: Sequence[str] = field(default_factory=list)
    detail: str = ""
    divergence_id: str | None = None   # DIV-NNN when status is DIVERGE_OK

    def assert_pass(self) -> None:
        """
        Assert this differential passed. Semantics:

            IDENT / DIVERGE_OK   → pass silently
            DIVERGE_BAD / FAIL   → raise AssertionError with detail
            SKELETON             → pytest.skip() (harness not wired yet)

        SKELETON is skipped, never passed: a not-yet-implemented
        gw-compat check must not masquerade as a green proof.
        """
        if self.status in (DiffStatus.IDENT, DiffStatus.DIVERGE_OK):
            return
        if self.status == DiffStatus.SKELETON:
            import pytest

            pytest.skip(
                f"uft_diff_test skeleton: '{self.command}' differential "
                f"not wired yet — lands in Welle 2 (P3.2). {self.detail}"
            )
            return
        raise AssertionError(
            f"differential FAILED [{self.status.value}] for "
            f"`{self.command} {' '.join(self.args)}`\n  {self.detail}"
        )


# --------------------------------------------------------------------------
# The heart — differential_command()
# --------------------------------------------------------------------------


def differential_command(
    command: str,
    args: Sequence[str] | None = None,
    input_file: Path | None = None,
    registry: DivergenceRegistry | None = None,
) -> DiffResult:
    """
    Run `gw <command>` and `uft <command>` on identical input, compare
    their outputs, classify the result against the divergence registry.

    SKELETON (P3.1): this function does NOT execute anything yet. It
    validates its arguments and returns a SKELETON DiffResult. Welle 2
    (P3.2) replaces the body below with real subprocess execution and
    byte comparison — the *signature and return type stay frozen* so
    tests written against it now keep working unchanged.

    Welle-2 body sketch (intentionally not implemented here):
        1. run gw  → capture stdout/stderr/exit + any output file
        2. run uft → same
        3. if either crashed                → DiffStatus.FAIL
        4. if outputs byte-identical         → DiffStatus.IDENT
        5. else look up (command, args) in `registry`
             - found  → DiffStatus.DIVERGE_OK + divergence_id
             - absent → DiffStatus.DIVERGE_BAD
    """
    args = list(args or [])

    if input_file is not None:
        input_file = Path(input_file)
        if not input_file.exists():
            return DiffResult(
                status=DiffStatus.FAIL,
                command=command,
                args=args,
                detail=f"input_file does not exist: {input_file}",
            )

    # Skeleton: argument plumbing verified, execution deferred to Welle 2.
    return DiffResult(
        status=DiffStatus.SKELETON,
        command=command,
        args=args,
        detail="differential_command() body is a P3.1 skeleton stub",
    )
