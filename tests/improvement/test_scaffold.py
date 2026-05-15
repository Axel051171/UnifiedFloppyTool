"""
test_scaffold.py — verifies the improvement-suite scaffold itself.

Analogous to tests/conformance/test_smoke.py: it asserts the layout,
support helpers and skip-discipline are in place, so `pytest
tests/improvement/` is green (not exit-5 "no tests collected") even
before the `improvement-test-author` agent populates the categories.

P3.3 scaffold (Tester-Strategie Welle 3).
"""

from pathlib import Path

import _support

_IMPROVEMENT_ROOT = Path(__file__).resolve().parent

_CATEGORIES = (
    "forensic",
    "multi_device",
    "format_extension",
    "copy_protection",
    "gui",
    "concurrency",
)


def test_all_category_dirs_exist():
    for cat in _CATEGORIES:
        d = _IMPROVEMENT_ROOT / cat
        assert d.is_dir(), f"improvement category dir missing: {cat}/"
        assert (d / "README.md").is_file(), (
            f"{cat}/ has no README stating which tests + principle it holds"
        )


def test_suite_readme_present():
    assert (_IMPROVEMENT_ROOT / "README.md").is_file()


def test_support_resolves_or_reports_missing_uft():
    """resolve_uft() returns a real path or None — never raises, never lies."""
    exe = _support.resolve_uft()
    assert exe is None or exe.exists()
    assert _support.uft_available() == (exe is not None)


def test_support_qt_probe_is_boolean():
    assert isinstance(_support.qt_test_available(), bool)


def test_tools_on_path_for_corpus_reuse():
    """conftest puts tools/ on sys.path so improvement tests can reuse
    the gw_corpus accessor."""
    import uft_diff_test

    assert callable(uft_diff_test.corpus)
