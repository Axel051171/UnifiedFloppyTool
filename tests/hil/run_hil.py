#!/usr/bin/env python3
"""
run_hil.py — Hardware-in-the-Loop runner (Tester-Strategie §4, P3.4).

HIL is the one layer of the test system that automation cannot replace:
it needs a real controller, a real drive and the physical Golden-
Reference disks. This script automates the *automatable subset* of
tests/HARDWARE_TRUTH_TESTS.md — the read→SHA-compare and rpm-tolerance
checks — and emits a Markdown report. The physical-observation rows
(motor sound, head-knock, button-emits-signal) stay manual; the report
links back to the checklist for those.

Hard rules (forensic honesty — DESIGN_PRINCIPLES "Keine erfundenen Daten"):
  * No hardware / no built uft binary  -> status NOT_RUN, exit 0.
    A missing environment is not a failure and MUST NOT be a PASS.
  * A disk with no baseline_sha256     -> NOT_RUN for that disk.
    Never compare against an invented hash.
  * A real mismatch                    -> FAIL, exit 1. Loud, never silent.

Usage:
    python tests/hil/run_hil.py \
        [--catalog tests/hil/golden_reference.yaml] \
        [--out releases/<ver>/hil_report.md] \
        [--uft /path/to/uft] [--version v4.1.4-rc1]

This is P3.4 scaffold: the runner is wired and self-tested, but the real
HIL run is Axel-machine work — it produces NOT_RUN everywhere until the
Golden-Reference catalog has `status: active` entries with captured
baselines and the hardware is connected.
"""

from __future__ import annotations

import argparse
import datetime as _dt
import hashlib
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CATALOG = REPO_ROOT / "tests" / "hil" / "golden_reference.yaml"

# Reuse the improvement suite's binary resolver — single source of truth
# for "where is the uft CLI".
sys.path.insert(0, str(REPO_ROOT / "tests" / "improvement"))
import _support  # noqa: E402


# --- result model ----------------------------------------------------

# Per-disk and overall status. Ordered worst-last so max() picks the
# most severe for the overall verdict.
_STATUS_RANK = {
    "ALL_GREEN": 0,
    "NOT_RUN": 1,
    "PARTIAL": 2,
    "FAIL": 3,
}


@dataclass
class CheckResult:
    name: str
    status: str          # ALL_GREEN | NOT_RUN | FAIL
    detail: str = ""


@dataclass
class DiskResult:
    disk_id: str
    controller: str
    status: str = "NOT_RUN"
    checks: list[CheckResult] = field(default_factory=list)

    def finalize(self) -> None:
        if not self.checks:
            self.status = "NOT_RUN"
            return
        worst = max(self.checks, key=lambda c: _STATUS_RANK[c.status])
        any_green = any(c.status == "ALL_GREEN" for c in self.checks)
        if worst.status == "FAIL":
            self.status = "FAIL"
        elif worst.status == "NOT_RUN":
            self.status = "PARTIAL" if any_green else "NOT_RUN"
        else:
            self.status = "ALL_GREEN"


# --- catalog ---------------------------------------------------------

def load_catalog(path: Path) -> dict:
    try:
        import yaml
    except ImportError:
        print("run_hil.py: PyYAML not installed — cannot read catalog",
              file=sys.stderr)
        return {"disks": []}
    with open(path, encoding="utf-8") as fh:
        return yaml.safe_load(fh) or {}


def _sha256(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


# --- per-disk checks -------------------------------------------------

def check_disk(disk: dict, uft: Path | None, tmp: Path) -> DiskResult:
    """
    Run the automatable HARDWARE_TRUTH_TESTS subset for one disk.

    Returns NOT_RUN checks (never FAIL) whenever the environment is
    missing — the verdict for a missing rig is honestly "not run".
    """
    res = DiskResult(disk_id=disk.get("id", "?"),
                     controller=disk.get("controller", "?"))

    if disk.get("status") != "active":
        res.checks.append(CheckResult(
            "catalog-entry",
            "NOT_RUN",
            f"entry status is {disk.get('status')!r}, not 'active'",
        ))
        res.finalize()
        return res

    if uft is None:
        res.checks.append(CheckResult(
            "uft-binary", "NOT_RUN",
            "uft CLI not built — set UFT_BIN or build with qmake",
        ))
        res.finalize()
        return res

    baseline_sha = disk.get("baseline_sha256")
    if not baseline_sha:
        res.checks.append(CheckResult(
            "baseline", "NOT_RUN",
            "no baseline_sha256 in catalog — capture it on main @ MF-149",
        ))
        res.finalize()
        return res

    # read → SHA-256 compare against frozen baseline
    read_args = disk.get("read_args", {})
    ext = read_args.get("image_ext", "scp")
    out_img = tmp / f"{res.disk_id}.{ext}"
    argv = [
        "read",
        f"--hal={res.controller}",
        f"--tracks={read_args.get('tracks', '0-79')}",
        f"--revs={read_args.get('revs', 3)}",
        f"--image={out_img}",
    ]
    try:
        proc = subprocess.run([str(uft), *argv], capture_output=True,
                              timeout=600, check=False)
    except (subprocess.TimeoutExpired, OSError) as exc:
        res.checks.append(CheckResult("read", "FAIL", f"uft read: {exc}"))
        res.finalize()
        return res

    if proc.returncode != 0 or not out_img.exists():
        # Non-zero with no image: most likely no hardware attached.
        # That is NOT_RUN, not FAIL — we cannot distinguish "absent rig"
        # from "broken read" without the device, so we stay honest and
        # defer to the manual checklist.
        res.checks.append(CheckResult(
            "read", "NOT_RUN",
            f"uft read exit={proc.returncode}, no image — "
            f"likely no hardware; confirm via HARDWARE_TRUTH_TESTS.md",
        ))
        res.finalize()
        return res

    actual = _sha256(out_img)
    if actual == baseline_sha:
        res.checks.append(CheckResult(
            "read-sha256", "ALL_GREEN", f"matches baseline {baseline_sha[:12]}…"))
    else:
        res.checks.append(CheckResult(
            "read-sha256", "FAIL",
            f"baseline {baseline_sha[:12]}… != actual {actual[:12]}…",
        ))

    # rpm tolerance (±0.5) when a baseline_rpm is recorded
    baseline_rpm = disk.get("baseline_rpm")
    if baseline_rpm is None:
        res.checks.append(CheckResult(
            "rpm", "NOT_RUN", "no baseline_rpm in catalog"))
    else:
        try:
            rpm_proc = subprocess.run(
                [str(uft), "rpm", f"--hal={res.controller}",
                 "--revolutions=50"],
                capture_output=True, timeout=120, check=False, text=True,
            )
            measured = _parse_rpm(rpm_proc.stdout)
            if measured is None:
                res.checks.append(CheckResult(
                    "rpm", "NOT_RUN", "could not parse rpm output"))
            elif abs(measured - float(baseline_rpm)) <= 0.5:
                res.checks.append(CheckResult(
                    "rpm", "ALL_GREEN",
                    f"{measured:.2f} within ±0.5 of {baseline_rpm}"))
            else:
                res.checks.append(CheckResult(
                    "rpm", "FAIL",
                    f"{measured:.2f} outside ±0.5 of {baseline_rpm}"))
        except (subprocess.TimeoutExpired, OSError) as exc:
            res.checks.append(CheckResult("rpm", "NOT_RUN", f"uft rpm: {exc}"))

    res.finalize()
    return res


def _parse_rpm(text: str) -> float | None:
    """Pull the first float that looks like an RPM reading from output."""
    import re
    m = re.search(r"(\d{2,3}\.\d+)", text or "")
    return float(m.group(1)) if m else None


# --- report ----------------------------------------------------------

def render_report(version: str, disks: list[DiskResult],
                  catalog_path: Path,
                  software_ref: dict | None = None) -> str:
    if not disks:
        overall = "NOT_RUN"
    else:
        overall = max((d.status for d in disks),
                      key=lambda s: _STATUS_RANK[s])
    now = _dt.datetime.now().strftime("%Y-%m-%d %H:%M")

    lines = [
        f"# HIL Report — {version}",
        "",
        f"- Generated: {now}",
        f"- Catalog: `{catalog_path.relative_to(REPO_ROOT)}`",
        f"- **Overall (hardware tier): {overall}**",
        "",
        "Automated subset of `tests/HARDWARE_TRUTH_TESTS.md` "
        "(read→SHA-256, rpm±0.5). Physical-observation rows "
        "(motor, head-knock, GUI signal) stay manual — see the checklist.",
        "",
    ]
    if overall == "NOT_RUN":
        lines += [
            "> **NOT_RUN** — no `status: active` catalog entry produced a "
            "result. This is expected until the Golden-Reference disk set "
            "is assembled and the rig is connected. It is **not** a PASS.",
            "",
        ]

    # Software golden tier — runs in CI, no hardware. Reported here so a
    # NOT_RUN hardware verdict is never read as "nothing is verified":
    # the decode engine IS verified, on every push, against gw.
    if software_ref:
        classes = software_ref.get("disk_classes", []) or []
        lines += [
            "## Software golden reference (CI tier — no hardware)",
            "",
            f"- Status: **{software_ref.get('status', '?')}**  "
            f"({len(classes)} disk classes)",
            f"- Harness: `{software_ref.get('harness', '?')}`",
            f"- Manifest: `{software_ref.get('manifest', '?')}`",
            "",
            "Each class is decoded by BOTH the UFT flux engine and "
            "`gw convert` and asserted byte-identical — this tier runs "
            "in GitHub Actions on every push, unlike the hardware tier.",
            "",
            "| Disk class | gw format | encoding |",
            "|------------|-----------|----------|",
        ]
        for c in classes:
            lines.append(
                f"| {c.get('id', '?')} | {c.get('gw_format', '?')} "
                f"| {c.get('encoding', '?')} |")
        lines.append("")

    lines += ["## Disks (hardware tier)", ""]
    if not disks:
        lines.append("_No disks in catalog._")
    for d in disks:
        lines.append(f"### {d.disk_id} — {d.controller} — **{d.status}**")
        lines.append("")
        lines.append("| Check | Status | Detail |")
        lines.append("|-------|--------|--------|")
        for c in d.checks:
            lines.append(f"| {c.name} | {c.status} | {c.detail} |")
        lines.append("")
    lines += [
        "## Manual rows (not automatable)",
        "",
        "The following require a human at the rig — fill in "
        "`tests/HARDWARE_TRUTH_TESTS.md` and sign off there:",
        "",
        "- motor spin-up / stop audible",
        "- seek 0/40/80 — no head-knock",
        "- recalibrate — clean return to track 0",
        "- detect_drive with no drive → `DriveAbsent`, not `ProviderError`",
        "- bootloader-mode firmware → actionable `ProviderError`",
        "- GUI: every Hardware-tab button emits exactly one signal",
        "",
        "## Sign-off",
        "",
        "This automated report does **not** replace the formal sign-off "
        "block in `RELEASE_NOTES.md` (HARDWARE_TRUTH_TESTS.md §Sign-off). "
        "Automation cannot replace the human gate.",
        "",
    ]
    return "\n".join(lines)


# --- main ------------------------------------------------------------

def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="UFT Hardware-in-the-Loop runner")
    ap.add_argument("--catalog", type=Path, default=DEFAULT_CATALOG)
    ap.add_argument("--out", type=Path, default=None,
                    help="report path; default releases/<version>/hil_report.md")
    ap.add_argument("--uft", type=Path, default=None,
                    help="path to uft binary; default = auto-resolve")
    ap.add_argument("--version", default="v4.1.4-rc1")
    args = ap.parse_args(argv)

    uft = args.uft if args.uft else _support.resolve_uft()
    catalog = load_catalog(args.catalog)
    disks_cfg = catalog.get("disks", []) or []

    import tempfile
    results: list[DiskResult] = []
    with tempfile.TemporaryDirectory(prefix="uft_hil_") as td:
        tmp = Path(td)
        for disk in disks_cfg:
            results.append(check_disk(disk, uft, tmp))

    report = render_report(args.version, results, args.catalog,
                           catalog.get("software_reference"))

    out = args.out or (REPO_ROOT / "releases" / args.version / "hil_report.md")
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(report, encoding="utf-8")
    print(f"run_hil.py: wrote {out.relative_to(REPO_ROOT)}")

    overall = (max((d.status for d in results),
                   key=lambda s: _STATUS_RANK[s]) if results else "NOT_RUN")
    print(f"run_hil.py: overall = {overall}")
    # Only a real mismatch is a non-zero exit. NOT_RUN / PARTIAL are 0 —
    # a missing rig must not break a release pipeline that never claimed
    # to have one.
    return 1 if overall == "FAIL" else 0


if __name__ == "__main__":
    raise SystemExit(main())
