"""
uft_diff_test.runner — tool execution layer for the differential harness.

Separated from differential_command() so the classification logic can be
unit-tested with a FakeRunner — no `gw` / `uft` binary required in CI.
The real SubprocessRunner is exercised only when the binaries are
actually present (on a HIL machine / Axel's setup).

Binary resolution order:
    1. explicit path argument
    2. environment override  (UFT_DIFF_GW / UFT_DIFF_UFT)
    3. bare name on PATH     ("gw" / "uft")
"""

from __future__ import annotations

import os
import shutil
import subprocess
from dataclasses import dataclass
from pathlib import Path
from typing import Protocol, Sequence

__all__ = ["ToolInvocation", "ToolResult", "ToolRunner", "SubprocessRunner"]


@dataclass(frozen=True)
class ToolInvocation:
    """One concrete command line to run."""

    tool: str                    # "gw" or "uft" — which side of the diff
    executable: str              # resolved path / name actually invoked
    argv: Sequence[str]          # full argv excluding the executable


@dataclass(frozen=True)
class ToolResult:
    """Outcome of running one tool."""

    invocation: ToolInvocation
    exit_code: int
    stdout: bytes
    stderr: bytes
    output_file: bytes | None = None   # contents of a produced output file
    crashed: bool = False              # could not execute at all

    @property
    def ok(self) -> bool:
        """The tool ran and exited cleanly."""
        return not self.crashed and self.exit_code == 0

    @property
    def payload(self) -> bytes:
        """
        The bytes a differential compares. An output file (if the command
        produced one) is the substantive payload; otherwise stdout is.
        """
        return self.output_file if self.output_file is not None else self.stdout


class ToolRunner(Protocol):
    """Runs one tool invocation and returns its result."""

    def run(self, inv: ToolInvocation) -> ToolResult: ...


def resolve_executable(tool: str, explicit: str | None = None) -> str | None:
    """Resolve the `gw` / `uft` binary; None if not found."""
    if explicit:
        return explicit if Path(explicit).exists() else None
    env_key = {"gw": "UFT_DIFF_GW", "uft": "UFT_DIFF_UFT"}.get(tool)
    if env_key and os.environ.get(env_key):
        cand = os.environ[env_key]
        return cand if Path(cand).exists() else None
    return shutil.which(tool)


class SubprocessRunner:
    """Real runner — executes `gw` / `uft` as subprocesses."""

    def __init__(self, timeout_s: float = 120.0):
        self.timeout_s = timeout_s

    def run(self, inv: ToolInvocation) -> ToolResult:
        try:
            proc = subprocess.run(
                [inv.executable, *inv.argv],
                capture_output=True,
                timeout=self.timeout_s,
                check=False,
            )
        except (FileNotFoundError, OSError) as exc:
            return ToolResult(
                invocation=inv,
                exit_code=-1,
                stdout=b"",
                stderr=str(exc).encode(),
                crashed=True,
            )
        except subprocess.TimeoutExpired:
            return ToolResult(
                invocation=inv,
                exit_code=-1,
                stdout=b"",
                stderr=b"timeout",
                crashed=True,
            )
        return ToolResult(
            invocation=inv,
            exit_code=proc.returncode,
            stdout=proc.stdout,
            stderr=proc.stderr,
        )
