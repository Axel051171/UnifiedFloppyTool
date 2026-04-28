#!/usr/bin/env python3
"""
Pre-push consistency check.

Catches the bug-class that produced the 4-CI-annotations whack-a-mole:

  (1) .c files in qmake SOURCES that include a header that doesn't
      exist on disk (MF-011-style cleanup leaves dangling #include).
      Caught: silent compile-fail under CMake, qmake also fails.

  (2) tests/CMakeLists.txt target_sources(... ${CMAKE_SOURCE_DIR}/X.c)
      pointing to deleted source files (uft_write_verify_pipeline-style).
      Caught: link error in one specific test.

  (3) tests/CMakeLists.txt list(APPEND TEST_LIBS uft_X) where uft_X
      is not a CMake target (uft_ride-style).
      Caught at configure time IF cmake is available, otherwise warn.

Run:
  python3 scripts/check_consistency.py                  # all checks, fail on error
  python3 scripts/check_consistency.py --warn-only      # report but exit 0
  python3 scripts/check_consistency.py --check includes # only check #1
  python3 scripts/check_consistency.py --check sources  # only check #2
  python3 scripts/check_consistency.py --check libs     # only check #3
"""

from __future__ import annotations

import argparse
import re
import sys
import subprocess
import tempfile
import shutil
from pathlib import Path


# Reuse the .pro parser from verify_build_sources.py
sys.path.insert(0, str(Path(__file__).resolve().parent))
import verify_build_sources as vbs  # noqa: E402


def parse_pro_includepaths(pro_path: Path) -> list[Path]:
    """Return INCLUDEPATH entries from .pro, opt-in blocks excluded."""
    text = pro_path.read_text(encoding="utf-8", errors="replace")
    text = re.sub(r"\\\s*\n\s*", " ", text)

    out: list[Path] = []
    optin_open = re.compile(r"^\s*([A-Za-z_][A-Za-z0-9_]*)\s*\{\s*$")
    any_open = re.compile(r"\{\s*$")
    any_close = re.compile(r"^\s*\}")
    pat = re.compile(r"^\s*INCLUDEPATH\s*\+?=\s*(.*?)\s*$")
    skip_depth = 0
    brace_depth = 0
    skip_origin = -1
    for line in text.splitlines():
        if skip_depth == 0:
            m_optin = optin_open.match(line)
            if m_optin and m_optin.group(1) in vbs._OPTIN_FLAGS:
                skip_depth = 1
                skip_origin = brace_depth
                brace_depth += 1
                continue
        if any_open.search(line):
            brace_depth += 1
            if skip_depth > 0:
                skip_depth += 1
        if any_close.match(line):
            brace_depth = max(0, brace_depth - 1)
            if skip_depth > 0:
                skip_depth -= 1
                if skip_depth == 0 and brace_depth == skip_origin:
                    skip_origin = -1
            continue
        if skip_depth > 0:
            continue
        m = pat.match(line)
        if not m:
            continue
        rest = re.sub(r"#.*$", "", m.group(1))
        for tok in rest.split():
            if tok in ("+=",):
                continue
            tok = tok.replace("$$PWD/", "").replace("$$PWD", "")
            if tok and not tok.startswith("$$"):
                out.append(Path(tok))
    return out


_INCLUDE_RE = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.MULTILINE)

# Generated headers — qmake writes these to the build dir at compile time.
# Format: regex pattern matched against the bare include name.
_GENERATED_INCLUDE_PATTERNS = [
    re.compile(r"^ui_.*\.h$"),       # AUTOUIC: forms/*.ui → ui_*.h
    re.compile(r".*\.moc$"),         # AUTOMOC pattern: X.cpp → X.moc
    re.compile(r"^moc_.*\.cpp$"),    # AUTOMOC standard
    re.compile(r"^qrc_.*\.cpp$"),    # AUTORCC
]


def _is_generated(inc: str) -> bool:
    return any(p.match(inc) for p in _GENERATED_INCLUDE_PATTERNS)


def check_dangling_includes(repo: Path) -> list[str]:
    """Find .c/.cpp files that #include a header that can't be resolved.
    Skips Qt-generated headers (ui_*.h, moc_*.cpp, *.moc, qrc_*.cpp) since
    they live in the build dir, not src/."""
    pro = repo / "UnifiedFloppyTool.pro"
    sources = sorted(vbs.parse_pro_sources(pro))
    inc_paths = parse_pro_includepaths(pro)
    inc_paths_abs = [(repo / p).resolve() for p in inc_paths]

    errors: list[str] = []
    for src_rel in sources:
        src = repo / src_rel
        if not src.is_file():
            continue
        text = src.read_text(encoding="utf-8", errors="replace")
        src_dir = src.parent
        for inc in _INCLUDE_RE.findall(text):
            if inc.endswith((".c", ".cpp")):
                continue
            if _is_generated(inc):
                continue
            candidates = [src_dir / inc] + [p / inc for p in inc_paths_abs]
            if any(c.is_file() for c in candidates):
                continue
            errors.append(f"{src_rel}: #include \"{inc}\" not resolvable")
    return errors


_CMAKE_PATH_RE = re.compile(
    r'\$\{CMAKE_SOURCE_DIR\}/([A-Za-z0-9_./]+\.(?:c|cpp|h))'
)


_EXCLUDED_TEST_RE = re.compile(r'"\s*(test_[a-zA-Z0-9_]+)\s*"')
_TEST_NAME_RE = re.compile(r'STREQUAL\s+"\s*(test_[a-zA-Z0-9_]+)\s*"')


def _parse_excluded_tests(cml_text: str) -> set[str]:
    """Extract names listed in `set(EXCLUDED_TESTS ...)` block.
    Strip line comments first — they may contain `)` chars that would
    prematurely terminate the set(...) match."""
    stripped = "\n".join(re.sub(r"#.*$", "", ln) for ln in cml_text.splitlines())
    m = re.search(r"set\(\s*EXCLUDED_TESTS([^)]*)\)", stripped, re.DOTALL)
    if not m:
        return set()
    body = m.group(1)
    excluded: set[str] = set()
    for tok in _EXCLUDED_TEST_RE.findall(body):
        excluded.add(tok)
    return excluded


def check_test_target_sources(repo: Path) -> list[str]:
    """Find target_sources(...) refs in tests/CMakeLists.txt that don't exist.
    Skips paths inside if/elseif branches gated on a STREQUAL test name that
    is in EXCLUDED_TESTS — those branches never execute."""
    cml = repo / "tests" / "CMakeLists.txt"
    if not cml.is_file():
        return []
    text = cml.read_text(encoding="utf-8")
    excluded = _parse_excluded_tests(text)

    errors: list[str] = []
    current_branch_test: str | None = None
    for ln_no, raw in enumerate(text.splitlines(), 1):
        line = re.sub(r"#.*$", "", raw)
        # Track which test_X branch we're inside.
        m = _TEST_NAME_RE.search(line)
        if m:
            current_branch_test = m.group(1)
        elif re.match(r"\s*endif\s*\(", line):
            current_branch_test = None
        for path in _CMAKE_PATH_RE.findall(line):
            if (repo / path).is_file():
                continue
            if current_branch_test and current_branch_test in excluded:
                continue
            errors.append(
                f"tests/CMakeLists.txt:{ln_no}: refs missing {path}"
                + (f" (branch: {current_branch_test})" if current_branch_test else "")
            )
    return errors


_LIB_REF_RE = re.compile(
    r'(?:list\(APPEND\s+TEST_LIBS|target_link_libraries[^)]*PRIVATE)\s+([a-zA-Z_][\w]*)'
)


def check_test_lib_targets(repo: Path) -> list[str]:
    """
    Best-effort scan for `list(APPEND TEST_LIBS X)` where X looks like a
    UFT static lib but no `add_library(X ...)` exists in any CMakeLists.txt.

    System libs (m, pthread, ${M_LIBRARY}) are skipped — they're absolute
    paths or system-resolved, not target names.
    """
    cml = repo / "tests" / "CMakeLists.txt"
    if not cml.is_file():
        return []

    # Collect every add_library(NAME ...) across the repo.
    defined: set[str] = set()
    add_lib_re = re.compile(r"add_library\(\s*([a-zA-Z_][\w]*)\s+")
    for path in repo.rglob("CMakeLists.txt"):
        if any(part in {".claude", "build", "build-tests"} for part in path.parts):
            continue
        try:
            for line in path.read_text(encoding="utf-8").splitlines():
                m = add_lib_re.search(re.sub(r"#.*$", "", line))
                if m:
                    defined.add(m.group(1))
        except OSError:
            continue
    # Also Qt + system targets we know are imported.
    defined.update({
        "Qt6::Core", "Qt6::Test", "Qt6::Widgets", "Qt6::Gui",
        "Qt6::SerialPort", "hardware_providers",
    })

    errors: list[str] = []
    referenced: list[tuple[int, str]] = []
    for ln_no, raw in enumerate(cml.read_text(encoding="utf-8").splitlines(), 1):
        line = re.sub(r"#.*$", "", raw)
        # We only care about TEST_LIBS-style refs (per-test).
        m = re.search(r"list\(APPEND\s+TEST_LIBS\s+([a-zA-Z_][\w]*)\b", line)
        if m:
            referenced.append((ln_no, m.group(1)))
        m = re.search(r"set\(\s*TEST_LIBS\s+([^)]*)\)", line)
        if m:
            for tok in m.group(1).split():
                if tok and not tok.startswith("$") and tok != '""':
                    referenced.append((ln_no, tok))
    skip = {"m", "pthread", "dl", "rt", "stdc++"}
    for ln, name in referenced:
        if name in skip or name in defined or name.startswith("$"):
            continue
        if not name.startswith("uft_"):
            continue   # Only flag uft_* refs, leave other unknowns alone.
        errors.append(
            f"tests/CMakeLists.txt:{ln}: TEST_LIBS '{name}' has no add_library() definition"
        )
    return errors


def check_version_ssot(repo: Path) -> list[str]:
    """
    MF-116: catch version-string drift across the SSOT family. The
    canonical version lives in VERSION.txt; everything that ships a
    version string to a user (uft_version.h, Flatpak metainfo, Win32 .rc)
    must agree, otherwise we get the v4.1.0-vs-v4.1.3 title-bar bug
    (MF-109) class of mismatch shipping to end users.

    The goal isn't to derive every consumer at build time — that's a
    bigger refactor. The goal is to fail-loud at pre-push when one of
    them drifts.
    """
    errors: list[str] = []
    canonical_path = repo / "VERSION.txt"
    if not canonical_path.is_file():
        return [f"VERSION.txt missing at {canonical_path}"]
    canonical = canonical_path.read_text(encoding="utf-8").strip()
    if not canonical:
        return [f"VERSION.txt is empty at {canonical_path}"]

    # uft_version.h — UFT_VERSION_STRING macro.
    vh = repo / "include" / "uft" / "uft_version.h"
    if vh.is_file():
        m = re.search(r'UFT_VERSION_STRING\s+"([^"]+)"', vh.read_text(encoding="utf-8"))
        if not m:
            errors.append(f"{vh}: UFT_VERSION_STRING not found")
        elif m.group(1) != canonical:
            errors.append(
                f"{vh}: UFT_VERSION_STRING={m.group(1)!r} drifted from VERSION.txt={canonical!r}"
            )

    # Flatpak metainfo — latest <release version="X">.
    metainfo_glob = list((repo / "packaging" / "flatpak").glob("*.metainfo.xml"))
    for mi in metainfo_glob:
        text = mi.read_text(encoding="utf-8")
        m = re.search(r'<release\s+version="([^"]+)"', text)
        if m and m.group(1) != canonical:
            errors.append(
                f"{mi}: latest <release version={m.group(1)!r}> drifted from VERSION.txt={canonical!r}"
            )

    # Win32 .rc — VALUE "FileVersion" / "ProductVersion" must reference
    # UFT_VERSION_STRING-derived macros, never a literal "X.Y.Z.0".
    rc = repo / "resources" / "uft.rc"
    if rc.is_file():
        text = rc.read_text(encoding="utf-8", errors="replace")
        for label in ("FileVersion", "ProductVersion"):
            for m in re.finditer(
                r'VALUE\s+"' + label + r'"\s*,\s*"([0-9]+\.[0-9]+\.[0-9]+\.[0-9]+)"',
                text):
                errors.append(
                    f"{rc}: VALUE \"{label}\" hardcoded to {m.group(1)!r} — "
                    "use UFT_RC_FILE_VERSION_STR macro from uft_version.h"
                )

    return errors


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", type=Path,
                    default=Path(__file__).resolve().parent.parent)
    ap.add_argument("--warn-only", action="store_true",
                    help="Print findings but exit 0 (non-blocking mode)")
    ap.add_argument("--check", choices=["includes", "sources", "libs", "version", "all"],
                    default="all")
    args = ap.parse_args()

    repo = args.root.resolve()
    all_errors: list[tuple[str, list[str]]] = []

    if args.check in ("includes", "all"):
        e = check_dangling_includes(repo)
        all_errors.append(("dangling #include", e))
    if args.check in ("sources", "all"):
        e = check_test_target_sources(repo)
        all_errors.append(("missing target_sources path", e))
    if args.check in ("libs", "all"):
        e = check_test_lib_targets(repo)
        all_errors.append(("undefined TEST_LIBS target", e))
    if args.check in ("version", "all"):
        e = check_version_ssot(repo)
        all_errors.append(("version SSOT drift", e))

    total = sum(len(e) for _, e in all_errors)
    print(f"Consistency check ({len(all_errors)} categories, root={repo}):")
    for label, errs in all_errors:
        print(f"  {label:30s}: {len(errs):3d}")
    if total == 0:
        print("OK")
        return 0

    print("\nFindings:")
    for label, errs in all_errors:
        if not errs:
            continue
        print(f"\n  [{label}]")
        for line in errs[:50]:
            print(f"    {line}")
        if len(errs) > 50:
            print(f"    ... and {len(errs) - 50} more")

    if args.warn_only:
        print(f"\nWARN: {total} issues (non-blocking mode)")
        return 0
    print(f"\nFAIL: {total} issues — fix before push, or run with --warn-only")
    return 1


if __name__ == "__main__":
    sys.exit(main())
