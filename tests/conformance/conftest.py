"""
pytest configuration for the differential conformance suite.

Puts the repo's `tools/` directory on sys.path so `import uft_diff_test`
works without an install step / editable package. Keeps the skeleton
zero-config: `pytest tests/conformance/` just works after
`pip install pytest pyyaml`.
"""

import sys
from pathlib import Path

_REPO_ROOT = Path(__file__).resolve().parents[2]
_TOOLS = _REPO_ROOT / "tools"

if str(_TOOLS) not in sys.path:
    sys.path.insert(0, str(_TOOLS))
