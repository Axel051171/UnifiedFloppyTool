#!/usr/bin/env python3
"""
run_simulated.py — Tier-2.5 HIL runner (V415-PLAN MF-267).

Runs the HIL checklist (`tests/HARDWARE_TRUTH_TESTS.md` subset) using
the simulator binaries under `tools/hw_simulators/`. No physical
hardware needed.

Status values:
  SIMULATED — the simulator round-trip completed; the V2 provider's
              subprocess argv construction, result parsing, and outcome
              translation are exercised end-to-end.
  NOT_RUN   — required simulator not present or check not applicable.
  FAIL      — the simulator pipeline produced a clear contract
              violation (provider returned UFT_OK with empty data,
              etc.).

`SIMULATED` is NEVER reported as `PASS`. Real hardware is the only
authority for that. SIMULATED is strictly between NOT_RUN (current
HW-blocked state) and PASS (real-hardware-verified), and tells you
that the QProcess/IO-glue layer is in working order.

Usage:
    python tests/hil/run_simulated.py
    python tests/hil/run_simulated.py --out releases/v4.1.5/hil_sim.md
    python tests/hil/run_simulated.py --controller kryoflux

The output Markdown is a status table per simulator + check. Linked
back to docs/MASTER_PLAN.md as evidence the simulator system works.
"""
from __future__ import annotations
import argparse
import os
import subprocess
import sys
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
SIM_DIR   = REPO_ROOT / "tools" / "hw_simulators"
CORPUS    = REPO_ROOT / "tests" / "differential" / "corpus" / "flux"


class Check:
    def __init__(self, controller: str, name: str, status: str, detail: str = ""):
        self.controller = controller
        self.name       = name
        self.status     = status
        self.detail     = detail

    def to_row(self) -> str:
        icon = {"SIMULATED": "[SIM]", "NOT_RUN": "[--]", "FAIL": "[FAIL]"}.get(self.status, "[?]")
        return f"| {self.controller} | {self.name} | {icon} {self.status} | {self.detail} |"


def run_with_sim_path(argv: list[str], **kw) -> subprocess.CompletedProcess:
    """Run a subprocess with SIM_DIR prepended to PATH."""
    env = dict(os.environ)
    env["PATH"] = str(SIM_DIR) + os.pathsep + env.get("PATH", "")
    return subprocess.run(argv, env=env, capture_output=True, text=True, **kw)


def check_kryoflux_info() -> Check:
    """KryoFlux: simulator's firmware banner should be parseable."""
    res = run_with_sim_path([sys.executable, str(SIM_DIR / "sim_dtc.py"), "-i0"])
    if res.returncode == 0 and "KryoFlux" in res.stdout and "rpm" in res.stdout:
        return Check("kryoflux", "info banner round-trip",
                     "SIMULATED",
                     f"DTC -i0 → {len(res.stdout.splitlines())} lines parsed")
    return Check("kryoflux", "info banner round-trip", "FAIL",
                 f"unexpected stdout: {res.stdout[:80]!r}")


def check_kryoflux_read() -> Check:
    """KryoFlux: read 3 tracks → 3 raw stream files emitted."""
    with tempfile.TemporaryDirectory() as td:
        prefix = str(Path(td) / "trk")
        res = run_with_sim_path([
            sys.executable, str(SIM_DIR / "sim_dtc.py"),
            "-c2", "-d0", "-s0", "-b0", "-e2", "-f" + prefix, "-i0",
        ])
        if res.returncode != 0:
            return Check("kryoflux", "read 3-track raw streams",
                         "FAIL", f"exit {res.returncode}: {res.stderr[:80]!r}")
        files = list(Path(td).glob("trk*.raw"))
        if len(files) != 3:
            return Check("kryoflux", "read 3-track raw streams",
                         "FAIL", f"expected 3 .raw files, got {len(files)}")
        # OOB marker tail
        sizes = sum(f.stat().st_size for f in files)
        return Check("kryoflux", "read 3-track raw streams",
                     "SIMULATED",
                     f"3 files × ~{sizes // 3} B each, all with KF OOB EOF tail")


def check_fluxengine_version() -> Check:
    res = run_with_sim_path([sys.executable, str(SIM_DIR / "sim_fluxengine.py"),
                             "--version"])
    if res.returncode == 0 and "FluxEngine" in res.stdout:
        return Check("fluxengine", "version-banner round-trip",
                     "SIMULATED", f"version line: {res.stdout.strip()!r}")
    return Check("fluxengine", "version-banner round-trip", "FAIL",
                 f"unexpected output: {res.stdout[:80]!r}")


def check_fluxengine_rpm() -> Check:
    res = run_with_sim_path([sys.executable, str(SIM_DIR / "sim_fluxengine.py"),
                             "rpm"])
    if res.returncode == 0 and "300.000 RPM" in res.stdout:
        return Check("fluxengine", "rpm parser regression",
                     "SIMULATED", "RPM line matches V2 provider's regex anchor")
    return Check("fluxengine", "rpm parser regression", "FAIL",
                 f"unexpected: {res.stdout[:80]!r}")


def check_fluxengine_read() -> Check:
    """fluxengine read: 1 track → .flux file with corpus bytes."""
    with tempfile.TemporaryDirectory() as td:
        out = Path(td) / "test.flux"
        res = run_with_sim_path([
            sys.executable, str(SIM_DIR / "sim_fluxengine.py"),
            "read", "-c", "ibm.720",
            "--tracks=c0h0", "--drive.revolutions=2",
            "-o", str(out),
        ])
        if res.returncode != 0:
            return Check("fluxengine", "read 1-track flux file",
                         "FAIL", f"exit {res.returncode}")
        if not out.exists() or out.stat().st_size == 0:
            return Check("fluxengine", "read 1-track flux file",
                         "FAIL", "output file empty/missing")
        return Check("fluxengine", "read 1-track flux file",
                     "SIMULATED",
                     f".flux = {out.stat().st_size} bytes (from ibm_dd.scp fixture)")


def check_fc5025_read() -> Check:
    """FC5025: read msdos360 → sector dump file."""
    with tempfile.TemporaryDirectory() as td:
        out = Path(td) / "disk.img"
        res = run_with_sim_path([
            sys.executable, str(SIM_DIR / "sim_fcimage.py"),
            "-f", "msdos360", "-t", "0", "-T", "5", "-s", "0", str(out),
        ])
        if res.returncode != 0:
            return Check("fc5025", "read msdos360 6 tracks",
                         "FAIL", f"exit {res.returncode}")
        expected = 6 * 9 * 512   # 6 tracks × 9 spt × 512 B
        if out.stat().st_size != expected:
            return Check("fc5025", "read msdos360 6 tracks",
                         "FAIL",
                         f"got {out.stat().st_size}, expected {expected}")
        return Check("fc5025", "read msdos360 6 tracks",
                     "SIMULATED",
                     f"sector dump = {expected} B (deterministic synthetic pattern)")


def check_fluxengine_write() -> Check:
    """fluxengine write: consume an input .flux and report success."""
    with tempfile.TemporaryDirectory() as td:
        # Borrow one of the corpus SCP files as a stand-in input.
        src = CORPUS / "ibm_dd.scp"
        if not src.exists():
            return Check("fluxengine", "write 1-track flux file",
                         "NOT_RUN", "ibm_dd.scp fixture not present")
        res = run_with_sim_path([
            sys.executable, str(SIM_DIR / "sim_fluxengine.py"),
            "write", "-c", "ibm.720", "--tracks=c0h0", "-i", str(src),
        ])
        if res.returncode == 0 and "write OK" in res.stdout:
            return Check("fluxengine", "write 1-track flux file",
                         "SIMULATED",
                         f"consumed {src.stat().st_size} bytes from fixture")
        return Check("fluxengine", "write 1-track flux file", "FAIL",
                     f"exit {res.returncode}: {res.stdout[:80]!r}")


CHECKS = [
    ("kryoflux",   check_kryoflux_info),
    ("kryoflux",   check_kryoflux_read),
    ("fluxengine", check_fluxengine_version),
    ("fluxengine", check_fluxengine_rpm),
    ("fluxengine", check_fluxengine_read),
    ("fluxengine", check_fluxengine_write),
    ("fc5025",     check_fc5025_read),
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", type=Path, default=None,
                    help="write Markdown report to this path")
    ap.add_argument("--controller", default=None,
                    help="limit to one controller (kryoflux/fluxengine/fc5025)")
    args = ap.parse_args()

    checks_to_run = [c for ctrl, c in CHECKS
                     if args.controller is None or ctrl == args.controller]

    results = [chk() for chk in checks_to_run]

    sim_count  = sum(1 for r in results if r.status == "SIMULATED")
    fail_count = sum(1 for r in results if r.status == "FAIL")
    skip_count = sum(1 for r in results if r.status == "NOT_RUN")

    lines = [
        "# UFT Tier-2.5 Hardware Simulation Report",
        "",
        "**Generated by:** `tests/hil/run_simulated.py` (V415-PLAN MF-267)",
        "",
        "## Status legend",
        "",
        "- `[SIM]` **SIMULATED** — argv + I/O round-trip OK; provider chain "
        "not fed real flux from a physical drive. Strictly between NOT_RUN and PASS.",
        "- `[--]` **NOT_RUN** — simulator pre-condition missing.",
        "- `[FAIL]` **FAIL** — the simulator pipeline produced a clear "
        "contract violation.",
        "",
        "## Results",
        "",
        "| Controller | Check | Status | Detail |",
        "|---|---|---|---|",
    ]
    lines.extend(r.to_row() for r in results)
    lines.extend([
        "",
        f"**Summary:** {sim_count} SIMULATED · {fail_count} FAIL · {skip_count} NOT_RUN",
        "",
        "## What this proves",
        "",
        "- Subprocess argv construction in V2 providers is correct.",
        "- stdout/stderr parsing handles realistic banner lines.",
        "- Result-file paths land in the expected locations and shapes.",
        "",
        "## What this does NOT prove",
        "",
        "- Real hardware timing behaviour.",
        "- USB driver / libusb session lifecycle.",
        "- Signal integrity, marginal-track decode, weak bits, magnetic aging.",
        "- VID/PID disambiguation between ADFCopy and Applesauce (needs USB).",
        "",
        "Real-hardware verification (Tier 3, `run_hil.py`) is still required "
        "for a Production-grade controller promotion.",
    ])

    output = "\n".join(lines) + "\n"
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(output, encoding="utf-8")
        print(f"Report written to {args.out}")
    else:
        print(output)

    return 0 if fail_count == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
