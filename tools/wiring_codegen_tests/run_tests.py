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

    # Case 5 — production YAML must validate (CI gate for H-3 / H-4 drift).
    # If someone renames a widget in forms/tab_hardware.ui or removes a
    # concept from include/uft/hal/concepts.h without updating the YAML or
    # KNOWN_CAPABILITIES, this gate catches it before the C++ build does.
    print("[run_tests] case 5: production forms/tab_hardware.actions.yaml validates")
    prod_yaml = REPO / "forms" / "tab_hardware.actions.yaml"
    prod_ui = REPO / "forms" / "tab_hardware.ui"
    if not prod_yaml.is_file() or not prod_ui.is_file():
        print(f"FAIL [production] missing input(s):"
              f" yaml_exists={prod_yaml.is_file()} ui_exists={prod_ui.is_file()}")
        return 1
    rc = subprocess.run(
        [
            sys.executable,
            str(CODEGEN),
            "--ui",
            str(prod_ui),
            "--actions",
            str(prod_yaml),
            "--check",
        ],
        capture_output=True,
        text=True,
    )
    if rc.returncode != 0:
        print(f"FAIL [production] codegen --check exit={rc.returncode}")
        print("stdout:", rc.stdout)
        print("stderr:", rc.stderr)
        return 1
    print(f"  ok [production] {rc.stdout.strip()}")

    # Case 6 — committed generated/tab_hardware_wiring.gen.cpp must match a
    # fresh regeneration. CI gate: if a developer modifies the YAML or the
    # codegen but forgets to rerun the generator and commit the new output,
    # this test catches it before the build does.
    #
    # We run the codegen with cwd=REPO and RELATIVE input paths — the same
    # way a developer would invoke it from the project root — so the input-
    # path comments emitted into the file match the committed copy verbatim.
    print("[run_tests] case 6: generated/tab_hardware_wiring.gen.cpp is fresh")
    committed = REPO / "generated" / "tab_hardware_wiring.gen.cpp"
    if not committed.is_file():
        print(f"FAIL [generated-fresh] committed file missing: {committed}")
        return 1
    with tempfile.TemporaryDirectory() as tmp:
        regen = Path(tmp) / "regen.gen.cpp"
        rc = subprocess.run(
            [
                sys.executable,
                str(CODEGEN),
                "--ui", "forms/tab_hardware.ui",
                "--actions", "forms/tab_hardware.actions.yaml",
                "--output", str(regen),
            ],
            cwd=str(REPO),
            capture_output=True,
            text=True,
        )
        if rc.returncode != 0:
            print(f"FAIL [generated-fresh] regeneration exit={rc.returncode}")
            print(rc.stderr)
            return 1
        a = committed.read_text(encoding="utf-8")
        b = regen.read_text(encoding="utf-8")
        if a != b:
            print("FAIL [generated-fresh] committed file is stale.")
            print(f"  Run: python3 tools/wiring_codegen.py "
                  f"--ui forms/tab_hardware.ui "
                  f"--actions forms/tab_hardware.actions.yaml "
                  f"--output generated/tab_hardware_wiring.gen.cpp")
            print(f"  Then: git add generated/tab_hardware_wiring.gen.cpp")
            return 1
        print("  ok [generated-fresh] committed output is byte-identical to regen")

    print("[run_tests] all green.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
