"""
pytest configuration for the Hardware-in-the-Loop suite (P3.4).

Puts `tests/hil/` on sys.path so `import run_hil` works without an
install step. `run_hil.py` itself adds `tests/improvement/` to resolve
the shared `_support` binary locator — single source of truth for
"where is the uft CLI".
"""

import sys
from pathlib import Path

_HIL_DIR = Path(__file__).resolve().parent

if str(_HIL_DIR) not in sys.path:
    sys.path.insert(0, str(_HIL_DIR))
