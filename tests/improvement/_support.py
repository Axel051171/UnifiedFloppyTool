"""
tests/improvement/_support.py — shared helpers for the improvement suite.

The improvement suite proves UFT does things `gw` expectedly cannot. Most
of these tests drive the built `uft` binary; some drive the Qt GUI. This
module resolves those externals and skips cleanly when they are absent —
an improvement test that cannot observe the behaviour must skip, never
silently pass and never hard-fail the suite on an environment gap.

Strategy: docs/TESTER_STRATEGY.md §3.
"""

from __future__ import annotations

import os
import shutil
import subprocess
from dataclasses import dataclass
from pathlib import Path

__all__ = [
    "REPO_ROOT",
    "resolve_uft",
    "uft_available",
    "run_uft",
    "qt_test_available",
    "UftRun",
]

# tests/improvement/_support.py → parents[2] is the repo root.
REPO_ROOT = Path(__file__).resolve().parents[2]

# Where a qmake / cmake build is likely to drop the binary. Not
# exhaustive — UFT_BIN is the reliable override.
_CANDIDATE_BIN_PATHS = (
    REPO_ROOT / "uft",
    REPO_ROOT / "uft.exe",
    REPO_ROOT / "UnifiedFloppyTool",
    REPO_ROOT / "UnifiedFloppyTool.exe",
    REPO_ROOT / "build" / "uft",
    REPO_ROOT / "build" / "uft.exe",
)


def resolve_uft() -> Path | None:
    """
    Resolve the built `uft` CLI binary, or None if it cannot be found.

    Order: $UFT_BIN → common build-output paths → bare `uft` on PATH.
    """
    env = os.environ.get("UFT_BIN")
    if env:
        p = Path(env)
        return p if p.exists() else None
    for cand in _CANDIDATE_BIN_PATHS:
        if cand.exists():
            return cand
    on_path = shutil.which("uft")
    return Path(on_path) if on_path else None


def uft_available() -> bool:
    return resolve_uft() is not None


@dataclass(frozen=True)
class UftRun:
    """Result of running the uft CLI."""

    argv: list[str]
    exit_code: int
    stdout: bytes
    stderr: bytes

    @property
    def ok(self) -> bool:
        return self.exit_code == 0


def run_uft(*args: str, timeout_s: float = 120.0) -> UftRun:
    """
    Run the built `uft` binary. Raises pytest.skip if it is not built —
    so a missing binary surfaces as a skip on the calling test, not a
    failure. Use the `uft_cli` fixture (conftest.py) to skip once per
    test instead of per call.
    """
    import pytest

    exe = resolve_uft()
    if exe is None:
        pytest.skip(
            "uft binary not built — set UFT_BIN or build the app "
            "(the cmake test-subset does NOT build the CLI; use qmake)"
        )
    proc = subprocess.run(
        [str(exe), *args],
        capture_output=True,
        timeout=timeout_s,
        check=False,
    )
    return UftRun(
        argv=[str(exe), *args],
        exit_code=proc.returncode,
        stdout=proc.stdout,
        stderr=proc.stderr,
    )


def qt_test_available() -> bool:
    """True when pytest-qt is importable (GUI improvement tests need it)."""
    try:
        import pytestqt  # noqa: F401
    except ImportError:
        return False
    return True
