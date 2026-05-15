"""
test_harness.py — unit tests for the differential_command() classifier.

These exercise the IDENT / DIVERGE_OK / DIVERGE_BAD / FAIL logic with a
FakeRunner, so the harness classification is fully tested WITHOUT a real
`gw` / `uft` binary. The real subprocess path (SubprocessRunner) only
runs on a machine where the tools are actually installed; that coverage
is the per-command corpus suite (P3.2-proper, HIL).

Wired in P3.2 (Tester-Strategie Welle 2).
"""

import sys
from dataclasses import dataclass

from uft_diff_test import (
    DiffStatus,
    ToolInvocation,
    ToolResult,
    differential_command,
    load_registry,
)

# Both binaries must "resolve" so the harness reaches the runner. We point
# the resolver at a file that definitely exists (the python executable);
# the FakeRunner ignores the executable entirely and returns canned output.
_EXISTS = sys.executable


@dataclass
class FakeOutput:
    stdout: bytes = b""
    exit_code: int = 0
    crashed: bool = False
    output_file: bytes | None = None


class FakeRunner:
    """ToolRunner that returns canned ToolResults keyed by tool name."""

    def __init__(self, gw: FakeOutput, uft: FakeOutput):
        self._spec = {"gw": gw, "uft": uft}

    def run(self, inv: ToolInvocation) -> ToolResult:
        s = self._spec[inv.tool]
        return ToolResult(
            invocation=inv,
            exit_code=s.exit_code,
            stdout=s.stdout,
            stderr=b"" if not s.crashed else b"boom",
            output_file=s.output_file,
            crashed=s.crashed,
        )


def _diff(gw: FakeOutput, uft: FakeOutput, command: str = "info"):
    return differential_command(
        command=command,
        runner=FakeRunner(gw, uft),
        gw_path=_EXISTS,
        uft_path=_EXISTS,
        registry=load_registry(),
    )


# --------------------------------------------------------------------------
# classification
# --------------------------------------------------------------------------


def test_ident_when_payloads_equal():
    r = _diff(FakeOutput(stdout=b"same payload\n"),
              FakeOutput(stdout=b"same payload\n"))
    assert r.status == DiffStatus.IDENT
    r.assert_pass()  # passes


def test_diverge_ok_when_div_001_mask_neutralises_banner():
    """Banner differs, payload identical → DIV-001 mask makes it DIVERGE_OK."""
    gw = FakeOutput(stdout=b"Greaseweazle 1.23\nflux: 12345 transitions\n")
    uft = FakeOutput(stdout=b"UnifiedFloppyTool 4.1.4\nflux: 12345 transitions\n")
    r = _diff(gw, uft)
    assert r.status == DiffStatus.DIVERGE_OK
    assert r.divergence_ids == ["DIV-001"]
    r.assert_pass()  # passes


def test_diverge_bad_when_payload_differs_unregistered():
    """Substantive payload differs, no mask covers it → DIVERGE_BAD → FAIL."""
    gw = FakeOutput(stdout=b"Greaseweazle 1.23\nflux: 12345 transitions\n")
    uft = FakeOutput(stdout=b"UnifiedFloppyTool 4.1.4\nflux: 99999 transitions\n")
    r = _diff(gw, uft)
    assert r.status == DiffStatus.DIVERGE_BAD
    import pytest

    with pytest.raises(AssertionError):
        r.assert_pass()


def test_fail_when_tool_crashes():
    r = _diff(FakeOutput(crashed=True), FakeOutput(stdout=b"ok\n"))
    assert r.status == DiffStatus.FAIL
    assert "gw" in r.detail


def test_fail_when_tool_exits_nonzero():
    r = _diff(FakeOutput(stdout=b"ok\n"), FakeOutput(stdout=b"", exit_code=2))
    assert r.status == DiffStatus.FAIL
    assert "uft" in r.detail


def test_output_file_is_the_payload_when_present():
    """When a command produces an output file, that file — not stdout — is
    what gets compared."""
    gw = FakeOutput(stdout=b"Greaseweazle 1.23\n", output_file=b"\x01\x02\x03")
    uft = FakeOutput(stdout=b"UnifiedFloppyTool 4.1.4\n", output_file=b"\x01\x02\x03")
    r = _diff(gw, uft)
    assert r.status == DiffStatus.IDENT  # stdout banners differ but ignored


# --------------------------------------------------------------------------
# binary resolution
# --------------------------------------------------------------------------


def test_tool_missing_when_executable_path_does_not_exist():
    r = differential_command(
        command="info",
        gw_path="/definitely/not/here/gw",
        uft_path="/definitely/not/here/uft",
    )
    assert r.status == DiffStatus.TOOL_MISSING
    assert "gw" in r.detail and "uft" in r.detail
    import pytest

    with pytest.raises(pytest.skip.Exception):
        r.assert_pass()  # skips, never fails


def test_input_file_missing_is_fail():
    from pathlib import Path

    r = differential_command(
        command="read",
        input_file=Path("/no/such/input.scp"),
        gw_path=_EXISTS,
        uft_path=_EXISTS,
        runner=FakeRunner(FakeOutput(), FakeOutput()),
    )
    assert r.status == DiffStatus.FAIL
    assert "input_file" in r.detail
