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

> **Status: HARNESS WIRED (P3.2, Welle 2).** `differential_command()`
> now really runs `gw` and `uft`, applies the registry masks, and
> classifies the result. What is still pending is the *corpus* —
> real `gw` v1.23 reference inputs/outputs and the ~50 per-command
> tests, which require a machine with `gw` installed (HIL / Axel's
> setup, TESTER_STRATEGY §4). When `gw`/`uft` are not on PATH the
> harness returns `TOOL_MISSING` and `assert_pass()` skips — an
> un-runnable check must not masquerade as green.

Usage (the shape every per-command test uses unchanged):

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

from .registry import DivergenceEntry, DivergenceRegistry, load_registry
from .runner import (
    SubprocessRunner,
    ToolInvocation,
    ToolResult,
    ToolRunner,
    resolve_executable,
)

__all__ = [
    "DiffStatus",
    "DiffResult",
    "differential_command",
    "corpus",
    "corpus_root",
    "sha256_file",
    "DivergenceEntry",
    "DivergenceRegistry",
    "load_registry",
    "ToolInvocation",
    "ToolResult",
    "ToolRunner",
    "SubprocessRunner",
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
    DIVERGE_OK = "DIVERGE_OK"     # differs, registered+masked reason → PASS
    DIVERGE_BAD = "DIVERGE_BAD"   # differs, no registry entry → FAIL
    FAIL = "FAIL"                 # a tool crashed / no output → FAIL
    TOOL_MISSING = "TOOL_MISSING"  # gw / uft not on PATH → SKIP
    SKELETON = "SKELETON"         # harness not wired for this command → SKIP


# Statuses that assert_pass() treats as a skip rather than pass/fail.
_SKIP_STATUSES = (DiffStatus.TOOL_MISSING, DiffStatus.SKELETON)


@dataclass
class DiffResult:
    """Outcome of one differential_command() comparison."""

    status: DiffStatus
    command: str
    args: Sequence[str] = field(default_factory=list)
    detail: str = ""
    divergence_ids: list[str] = field(default_factory=list)  # DIV-NNN applied

    def assert_pass(self) -> None:
        """
        Assert this differential passed. Semantics:

            IDENT / DIVERGE_OK         → pass silently
            DIVERGE_BAD / FAIL         → raise AssertionError with detail
            TOOL_MISSING / SKELETON    → pytest.skip()

        The skip statuses are skipped, never passed: a check that could
        not actually run must not masquerade as a green gw-compat proof.
        """
        if self.status in (DiffStatus.IDENT, DiffStatus.DIVERGE_OK):
            return
        if self.status in _SKIP_STATUSES:
            import pytest

            pytest.skip(
                f"uft_diff_test [{self.status.value}]: '{self.command}' "
                f"differential could not run — {self.detail}"
            )
            return
        raise AssertionError(
            f"differential FAILED [{self.status.value}] for "
            f"`{self.command} {' '.join(self.args)}`\n  {self.detail}"
        )


# --------------------------------------------------------------------------
# The heart — differential_command()
# --------------------------------------------------------------------------


def _mask_payload(payload: bytes, masks: Sequence) -> tuple[str, set]:
    """
    Drop every line matching any mask regex. Returns the surviving text
    and the set of mask-pattern objects that actually removed something
    (so the caller knows which DIV entries were load-bearing).
    """
    text = payload.decode("utf-8", errors="replace")
    kept_lines = []
    used = set()
    for line in text.splitlines():
        matched = next((m for m in masks if m.search(line)), None)
        if matched is not None:
            used.add(matched)
        else:
            kept_lines.append(line)
    return "\n".join(kept_lines), used


def differential_command(
    command: str,
    args: Sequence[str] | None = None,
    input_file: Path | None = None,
    registry: DivergenceRegistry | None = None,
    *,
    runner: ToolRunner | None = None,
    gw_path: str | None = None,
    uft_path: str | None = None,
) -> DiffResult:
    """
    Run `gw <command>` and `uft <command>` on identical input, compare
    their outputs, classify the result against the divergence registry.

    Classification:
        1. either tool absent / crashed     → TOOL_MISSING / FAIL
        2. payloads byte-identical          → IDENT
        3. apply every registry mask that applies to `command`;
           if the masked payloads are now equal → DIVERGE_OK
           (divergence_ids lists the DIV-NNN entries that were used)
        4. still different                  → DIVERGE_BAD

    `runner` is injectable so the classification logic is unit-testable
    with a FakeRunner — no `gw` / `uft` binary needed in CI. The default
    is a real SubprocessRunner.
    """
    args = list(args or [])
    runner = runner or SubprocessRunner()
    if registry is None:
        registry = load_registry()

    if input_file is not None:
        input_file = Path(input_file)
        if not input_file.exists():
            return DiffResult(
                status=DiffStatus.FAIL,
                command=command,
                args=args,
                detail=f"input_file does not exist: {input_file}",
            )

    # --- resolve both binaries -------------------------------------------
    gw_exe = resolve_executable("gw", gw_path)
    uft_exe = resolve_executable("uft", uft_path)
    missing = [
        name
        for name, exe in (("gw", gw_exe), ("uft", uft_exe))
        if exe is None
    ]
    if missing:
        return DiffResult(
            status=DiffStatus.TOOL_MISSING,
            command=command,
            args=args,
            detail=(
                f"not on PATH: {', '.join(missing)} "
                f"(set UFT_DIFF_GW / UFT_DIFF_UFT to override)"
            ),
        )

    # --- run both ---------------------------------------------------------
    argv = [command, *args]
    if input_file is not None:
        argv.append(str(input_file))

    gw_res = runner.run(ToolInvocation("gw", gw_exe, argv))
    uft_res = runner.run(ToolInvocation("uft", uft_exe, argv))

    for res in (gw_res, uft_res):
        if res.crashed or res.exit_code != 0:
            how = "crashed" if res.crashed else f"exit {res.exit_code}"
            return DiffResult(
                status=DiffStatus.FAIL,
                command=command,
                args=args,
                detail=(
                    f"{res.invocation.tool} {how}: "
                    f"{res.stderr.decode('utf-8', 'replace').strip()[:200]}"
                ),
            )

    # --- compare ----------------------------------------------------------
    if gw_res.payload == uft_res.payload:
        return DiffResult(
            status=DiffStatus.IDENT, command=command, args=args
        )

    applicable = registry.for_command(command)
    masks = [e.mask for e in applicable if e.mask is not None]
    gw_masked, gw_used = _mask_payload(gw_res.payload, masks)
    uft_masked, uft_used = _mask_payload(uft_res.payload, masks)

    if gw_masked == uft_masked and (gw_used or uft_used):
        used_patterns = gw_used | uft_used
        div_ids = sorted(
            e.id
            for e in applicable
            if e.mask is not None and e.mask in used_patterns
        )
        return DiffResult(
            status=DiffStatus.DIVERGE_OK,
            command=command,
            args=args,
            detail=f"divergence neutralised by {', '.join(div_ids)}",
            divergence_ids=div_ids,
        )

    return DiffResult(
        status=DiffStatus.DIVERGE_BAD,
        command=command,
        args=args,
        detail=(
            "outputs differ and no divergence registry entry accounts "
            "for it — fix the code or add a DIV-NNN entry with a reason"
        ),
    )
