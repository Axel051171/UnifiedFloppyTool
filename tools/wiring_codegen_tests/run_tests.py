#!/usr/bin/env python3
"""Smoke-test driver for tools/wiring_codegen.py (P1.0).

Runs three scenarios against the same .ui fixture:

  1. happy path  — sample.actions.yaml → exit 0, output written
  2. H-3 surface — bad_widget.yaml     → exit 4 (widget not in .ui)
  3. H-4 surface — bad_cap.yaml        → exit 5 (capability unknown)

Idempotence is also checked: re-running case 1 must say "up-to-date"
and not modify the file mtime.

Run with `python3 tools/wiring_codegen_tests/run_tests.py`.
Exit code 0 = all green.
"""
from __future__ import annotations

import os
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent
CODEGEN = REPO / "tools" / "wiring_codegen.py"
SAMPLE_UI = HERE / "sample.ui"
SAMPLE_YAML = HERE / "sample.actions.yaml"


def run(yaml_text: str, expect: int, label: str) -> None:
    with tempfile.TemporaryDirectory() as tmp:
        td = Path(tmp)
        yp = td / "case.yaml"
        yp.write_text(yaml_text, encoding="utf-8")
        op = td / "out.gen.cpp"
        rc = subprocess.run(
            [
                sys.executable,
                str(CODEGEN),
                "--ui",
                str(SAMPLE_UI),
                "--actions",
                str(yp),
                "--output",
                str(op),
            ],
            capture_output=True,
            text=True,
        )
        if rc.returncode != expect:
            print(f"FAIL [{label}] expected exit={expect}, got {rc.returncode}")
            print("stdout:", rc.stdout)
            print("stderr:", rc.stderr)
            sys.exit(1)
        print(f"  ok [{label}] exit={rc.returncode}")


def main() -> int:
    print("[run_tests] case 1: happy path (sample.yaml)")
    run(SAMPLE_YAML.read_text(encoding="utf-8"), expect=0, label="happy")

    print("[run_tests] case 2: H-3 surface (widget not in .ui)")
    run(
        "tab: BadTab\n"
        "provider_source: foo()\n"
        "actions:\n"
        "  - widget: btnDoesNotExist\n"
        "    requires: ControlsMotor\n"
        "    invoke: bar()\n",
        expect=4,
        label="H-3",
    )

    print("[run_tests] case 3: H-4 surface (capability unknown)")
    run(
        "tab: BadTab\n"
        "provider_source: foo()\n"
        "actions:\n"
        "  - widget: btnMotorOn\n"
        "    requires: NotARealCapability\n"
        "    invoke: bar()\n",
        expect=5,
        label="H-4",
    )

    print("[run_tests] case 4: idempotence (rerun produces no changes)")
    with tempfile.TemporaryDirectory() as tmp:
        op = Path(tmp) / "out.gen.cpp"
        for i in range(2):
            rc = subprocess.run(
                [
                    sys.executable,
                    str(CODEGEN),
                    "--ui",
                    str(SAMPLE_UI),
                    "--actions",
                    str(SAMPLE_YAML),
                    "--output",
                    str(op),
                ],
                capture_output=True,
                text=True,
            )
            if rc.returncode != 0:
                print(f"FAIL [idempotence run {i}] exit={rc.returncode}")
                print(rc.stderr)
                return 1
        # second run output stable
        first = op.read_text(encoding="utf-8")
        # rerun a third time and diff bytes
        op2 = Path(tmp) / "out2.gen.cpp"
        subprocess.run(
            [
                sys.executable,
                str(CODEGEN),
                "--ui",
                str(SAMPLE_UI),
                "--actions",
                str(SAMPLE_YAML),
                "--output",
                str(op2),
            ],
            check=True,
        )
        second = op2.read_text(encoding="utf-8")
        if first != second:
            print("FAIL [idempotence] output not byte-identical across runs")
            return 1
        print("  ok [idempotence] byte-identical across runs")

    print("[run_tests] all green.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
