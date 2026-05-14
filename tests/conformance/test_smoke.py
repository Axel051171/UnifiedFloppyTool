"""
test_smoke.py — skeleton smoke test for the differential conformance
harness (P3.1, Tester-Strategie Welle 1).

What this proves:
  * the gw_corpus skeleton fixture exists and is byte-intact
  * the divergence registry parses and DIV-001 is present + well-formed
  * the uft_diff_test API contract holds (DiffResult / assert_pass)
  * differential_command() is wired end-to-end — and honestly *skips*
    instead of fooling itself green, because the real gw-vs-uft
    execution lands in Welle 2 (P3.2)

When pytest is green here (3 passed, 1 skipped), the skeleton stands
and everything in P3.2+ stacks on top of it.
"""

from pathlib import Path

import pytest

from uft_diff_test import (
    DiffResult,
    DiffStatus,
    corpus,
    corpus_root,
    differential_command,
    load_registry,
    sha256_file,
)

SKELETON_FIXTURE = "inputs/flux_streams/synthetic_mfm_dd_10trk.scp"


# --------------------------------------------------------------------------
# 1. corpus integrity
# --------------------------------------------------------------------------


def test_corpus_root_exists():
    root = corpus_root()
    assert root.is_dir(), f"gw_corpus root missing: {root}"
    assert (root / "README.md").is_file()


def test_skeleton_fixture_present_and_intact():
    """The one synthetic SCP fixture exists and matches its manifest."""
    fixture = corpus(SKELETON_FIXTURE)
    assert fixture.stat().st_size > 0, "skeleton fixture is empty"

    manifest = fixture.parent / "MANIFEST.sha256"
    assert manifest.is_file(), f"MANIFEST.sha256 missing in {fixture.parent}"

    # MANIFEST line format: "<sha256> *<filename>"  (sha256sum output)
    expected = {}
    for line in manifest.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line:
            continue
        digest, name = line.split(maxsplit=1)
        expected[name.lstrip("*")] = digest

    assert fixture.name in expected, (
        f"{fixture.name} not listed in MANIFEST.sha256"
    )
    actual = sha256_file(fixture)
    assert actual == expected[fixture.name], (
        f"{fixture.name} sha256 mismatch — fixture was modified without "
        f"regenerating MANIFEST.sha256\n  manifest: {expected[fixture.name]}"
        f"\n  actual:   {actual}"
    )


def test_corpus_missing_input_raises():
    """corpus() on a non-existent path is a setup error, surfaced loudly."""
    with pytest.raises(FileNotFoundError):
        corpus("inputs/flux_streams/this_does_not_exist.scp")


# --------------------------------------------------------------------------
# 2. divergence registry
# --------------------------------------------------------------------------


def test_divergence_registry_parses():
    reg = load_registry()
    assert len(reg) >= 1, "registry must contain at least DIV-001"
    assert "DIV-001" in reg, "bootstrap entry DIV-001 missing"


def test_div_001_is_well_formed():
    reg = load_registry()
    div1 = reg.get("DIV-001")
    assert div1 is not None
    # DIV-001 is the banner divergence and applies to every command.
    assert div1.command == "*"
    assert "banner" in div1.scope.lower() or "version" in div1.scope.lower()
    for field_name in ("summary", "reason", "scope"):
        assert getattr(div1, field_name).strip(), (
            f"DIV-001.{field_name} must not be empty"
        )


def test_registry_for_command_includes_wildcards():
    reg = load_registry()
    # DIV-001 has command "*", so it applies to an arbitrary command.
    applicable = reg.for_command("read")
    assert any(e.id == "DIV-001" for e in applicable)


# --------------------------------------------------------------------------
# 3. uft_diff_test API contract
# --------------------------------------------------------------------------


def test_diff_result_pass_statuses_pass():
    for status in (DiffStatus.IDENT, DiffStatus.DIVERGE_OK):
        DiffResult(status=status, command="read").assert_pass()  # no raise


def test_diff_result_fail_statuses_raise():
    for status in (DiffStatus.DIVERGE_BAD, DiffStatus.FAIL):
        result = DiffResult(status=status, command="read", detail="x")
        with pytest.raises(AssertionError):
            result.assert_pass()


def test_diff_result_skeleton_skips():
    """A SKELETON result must skip, never silently pass."""
    result = DiffResult(status=DiffStatus.SKELETON, command="read")
    with pytest.raises(pytest.skip.Exception):
        result.assert_pass()


# --------------------------------------------------------------------------
# 4. end-to-end smoke — the shape every P3.2 test will use
# --------------------------------------------------------------------------


def test_smoke_version_differential():
    """
    Trivial differential: `uft --version` vs `gw --version`.

    In the skeleton this SKIPS (differential_command() is a stub). When
    P3.2 wires real execution, this becomes the first genuinely-passing
    differential — output differs only by the banner, covered by
    DIV-001, so the expected result is DIVERGE_OK.
    """
    result = differential_command(command="--version")
    assert result.status == DiffStatus.SKELETON
    result.assert_pass()  # skips in skeleton
