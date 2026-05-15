"""
pytest configuration for the improvement suite.

- puts the repo's `tools/` on sys.path so improvement tests can reuse
  `uft_diff_test.corpus()` for shared test inputs
- provides the `uft_cli` fixture: resolves the built binary once per
  test and skips cleanly if it is not built
- provides the `requires_qt` marker for GUI tests
"""

import sys
from pathlib import Path

import pytest

_REPO_ROOT = Path(__file__).resolve().parents[2]
_TOOLS = _REPO_ROOT / "tools"
if str(_TOOLS) not in sys.path:
    sys.path.insert(0, str(_TOOLS))

from _support import qt_test_available, resolve_uft  # noqa: E402


@pytest.fixture
def uft_cli():
    """The built `uft` binary path; skips the test if it is not built."""
    exe = resolve_uft()
    if exe is None:
        pytest.skip(
            "uft binary not built — set UFT_BIN or build the app "
            "(the cmake test-subset does NOT build the CLI; use qmake)"
        )
    return exe


def pytest_configure(config):
    config.addinivalue_line(
        "markers",
        "requires_qt: GUI improvement test — needs pytest-qt installed",
    )


def pytest_collection_modifyitems(config, items):
    """Auto-skip @requires_qt tests when pytest-qt is not installed."""
    if qt_test_available():
        return
    skip_qt = pytest.mark.skip(reason="pytest-qt not installed")
    for item in items:
        if "requires_qt" in item.keywords:
            item.add_marker(skip_qt)
