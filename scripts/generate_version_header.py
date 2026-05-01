#!/usr/bin/env python3
"""
Regenerate include/uft/uft_version.h from VERSION.txt.

Single source of truth: VERSION.txt at the repo root holds the canonical
version string ("4.1.3\n"). This script materializes it as the C macros
UFT_VERSION_MAJOR / MINOR / PATCH / STRING / FULL.

The script is idempotent — when the source-tree header is already
in sync, the file is left untouched (no spurious mtime change, no
churning git diff). When it has drifted, the file is rewritten and a
short message is printed to stderr.

Invoked from:
  * CMakeLists.txt   (execute_process at configure time)
  * UnifiedFloppyTool.pro (system() at qmake time)
  * Manually:        python3 scripts/generate_version_header.py
  * pre-push hook:   scripts/check_consistency.py --check version
                     (verification only — does not regenerate)

Why a checked-in file rather than build-time-only generation:
  * IDE / grep / code-reader workflow: the version string must be
    findable in source without a configured build dir.
  * qmake users on Windows occasionally build out-of-tree but expect
    `find . -name uft_version.h` to surface the right header.
  * A dual-source setup (template + generated) confused contributors
    in the past.

The macros are all #ifndef-guarded so the file can be overridden at
compile time via -DUFT_VERSION_STRING="..." for branch builds where
modifying the source tree isn't practical (e.g. CI nightly tags).
"""

from __future__ import annotations

import sys
from pathlib import Path
import re

REPO_ROOT = Path(__file__).resolve().parent.parent
VERSION_FILE = REPO_ROOT / "VERSION.txt"
HEADER_FILE = REPO_ROOT / "include" / "uft" / "uft_version.h"


def render(version: str) -> str:
    m = re.fullmatch(r"(\d+)\.(\d+)\.(\d+)", version)
    if not m:
        raise SystemExit(
            f"VERSION.txt={version!r} does not match X.Y.Z pattern; "
            "release scripts only emit numeric semver."
        )
    major, minor, patch = m.groups()
    return (
        '/**\n'
        ' * @file uft_version.h\n'
        ' * @brief UnifiedFloppyTool Version Information\n'
        ' *\n'
        ' * GENERATED FILE — do not edit by hand. Regenerate via:\n'
        ' *   python3 scripts/generate_version_header.py\n'
        ' * Source of truth: VERSION.txt at the repository root.\n'
        ' *\n'
        ' * Every macro is #ifndef-guarded so a build-system override\n'
        ' * (-DUFT_VERSION_STRING="...") wins over the checked-in value.\n'
        ' */\n'
        '#ifndef UFT_VERSION_H\n'
        '#define UFT_VERSION_H\n'
        '\n'
        f'#ifndef UFT_VERSION_MAJOR\n#define UFT_VERSION_MAJOR {major}\n#endif\n'
        f'#ifndef UFT_VERSION_MINOR\n#define UFT_VERSION_MINOR {minor}\n#endif\n'
        f'#ifndef UFT_VERSION_PATCH\n#define UFT_VERSION_PATCH {patch}\n#endif\n'
        '\n'
        f'#ifndef UFT_VERSION_STRING\n#define UFT_VERSION_STRING "{version}"\n#endif\n'
        f'#ifndef UFT_VERSION_FULL\n#define UFT_VERSION_FULL "UnifiedFloppyTool v{version}"\n#endif\n'
        '\n'
        '#ifndef UFT_BUILD_DATE\n#define UFT_BUILD_DATE __DATE__\n#endif\n'
        '#ifndef UFT_BUILD_TIME\n#define UFT_BUILD_TIME __TIME__\n#endif\n'
        '#ifndef UFT_GIT_HASH\n#define UFT_GIT_HASH "unknown"\n#endif\n'
        '\n'
        '#endif /* UFT_VERSION_H */\n'
    )


def main() -> int:
    if not VERSION_FILE.is_file():
        print(f"error: {VERSION_FILE} missing", file=sys.stderr)
        return 1
    version = VERSION_FILE.read_text(encoding="utf-8").strip()
    if not version:
        print(f"error: {VERSION_FILE} is empty", file=sys.stderr)
        return 1

    rendered = render(version)

    # Compare against existing file in a normalized way (strip CR so
    # CRLF-checkout repos don't trigger spurious rewrites).
    existing = ""
    if HEADER_FILE.is_file():
        existing = HEADER_FILE.read_text(encoding="utf-8").replace("\r\n", "\n")

    if existing == rendered:
        return 0  # no-op when in sync

    HEADER_FILE.write_text(rendered, encoding="utf-8", newline="\n")
    print(
        f"generate_version_header: wrote {HEADER_FILE.relative_to(REPO_ROOT)} "
        f"for version {version}",
        file=sys.stderr,
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
