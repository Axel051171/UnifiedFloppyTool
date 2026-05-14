"""
test_hil_scaffold.py — verifies the HIL scaffold itself (P3.4).

Analogous to tests/improvement/test_scaffold.py and
tests/conformance/test_smoke.py: it keeps `pytest tests/hil/` green
(not exit-5 "no tests collected") and proves the runner's honesty
contract — a missing rig produces NOT_RUN, never a fabricated PASS —
without needing any hardware.
"""

from pathlib import Path

import run_hil

_HIL_ROOT = Path(__file__).resolve().parent


def test_catalog_parses_and_has_template_entries():
    cat = run_hil.load_catalog(_HIL_ROOT / "golden_reference.yaml")
    assert cat.get("catalog_version") == 1
    disks = cat.get("disks", [])
    assert disks, "catalog should ship documented template entries"
    # Every shipped entry is a template — no fabricated 'active' disk.
    assert all(d.get("status") == "template" for d in disks)


def test_template_entries_are_not_run_never_pass():
    """A template entry must yield NOT_RUN — never ALL_GREEN."""
    cat = run_hil.load_catalog(_HIL_ROOT / "golden_reference.yaml")
    for disk in cat.get("disks", []):
        res = run_hil.check_disk(disk, uft=None, tmp=_HIL_ROOT)
        assert res.status == "NOT_RUN"
        assert res.status != "ALL_GREEN"


def test_active_entry_without_baseline_is_not_run():
    """An active disk with no captured baseline_sha256 is NOT_RUN,
    never compared against an invented hash."""
    fake = {
        "id": "unit-active-no-baseline",
        "status": "active",
        "controller": "greaseweazle",
        "baseline_sha256": None,
    }
    res = run_hil.check_disk(fake, uft=None, tmp=_HIL_ROOT)
    assert res.status == "NOT_RUN"


def test_report_renders_not_run_for_empty_catalog():
    report = run_hil.render_report("v0.0-test", [], _HIL_ROOT / "x.yaml")
    assert "Overall: NOT_RUN" in report
    assert "not** a PASS" in report


def test_status_rank_orders_fail_worst():
    rank = run_hil._STATUS_RANK
    assert rank["FAIL"] == max(rank.values())
    assert rank["ALL_GREEN"] == min(rank.values())
