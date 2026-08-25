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
    r'\$\{CMAKE_SOURCE_DIR\}/([A-Za-z0-9_./]+\.(?:cpp|c|h))'
    # Note: cpp before c to prevent .cpp being incorrectly matched as .c
    # (alternation is ordered; the longer alternative must come first).
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

    # RELEASE_NOTES.md — top-level "# UnifiedFloppyTool vX.Y.Z Release Notes"
    # headline must match VERSION.txt. Caught the v4.2.0-vs-v4.1.3 lie
    # that shipped to all release-page visitors.
    rn = repo / "RELEASE_NOTES.md"
    if rn.is_file():
        first = rn.read_text(encoding="utf-8", errors="replace").splitlines()[:5]
        for ln in first:
            m = re.search(r"^#\s+UnifiedFloppyTool\s+v([0-9]+\.[0-9]+\.[0-9]+)", ln)
            if m and m.group(1) != canonical:
                errors.append(
                    f"{rn}: headline 'v{m.group(1)}' drifted from VERSION.txt={canonical!r}"
                )
                break

    # Doxygen @version drift in public headers — only flag values that
    # claim to be the project version (X.Y.Z matching the major). Module
    # API versions like '1.0.0' / '2.0.0' are legitimate and stay.
    canonical_major = canonical.split(".")[0]
    inc_root = repo / "include" / "uft"
    if inc_root.is_dir():
        version_re = re.compile(r"^\s*\*\s*@version\s+(\S+)")
        for hdr in inc_root.rglob("*.h"):
            try:
                head = hdr.read_text(encoding="utf-8", errors="replace").splitlines()[:30]
            except OSError:
                continue
            for ln in head:
                m = version_re.match(ln)
                if not m:
                    continue
                ver = m.group(1)
                # Only flag drift that claims to track the project version.
                if not re.match(r"^[0-9]+\.[0-9]+\.[0-9]+$", ver):
                    continue
                if ver.split(".")[0] != canonical_major:
                    continue
                if ver != canonical:
                    errors.append(
                        f"{hdr}: @version {ver} drifted from VERSION.txt={canonical} "
                        "(remove the line or sync it; see scripts/check_consistency.py)"
                    )
                break  # one finding per file is enough

    # UFT-A11 (follow-up to UFT-A06): scan src/ + include/ for hardcoded
    # tool-version literals like "UFT 4.1" or "UnifiedFloppyTool v4.1.x".
    # The A06 finding (forensic provenance chain stamping "UFT 4.1" in
    # v4.1.5 binaries) slipped past this checker because we only audited
    # the meta-files above, not the source. SSOT rule: any version stamp
    # must come from UFT_VERSION_STRING / UFT_VERSION_FULL, never a
    # literal.
    #
    # Pattern matches:
    #   "UFT 4.1"             "UFT v4.1.5"
    #   "UnifiedFloppyTool v4.1.5"
    # Carve-outs:
    #   - include/uft/uft_version.h is the SSOT definition itself
    #   - vendored code under src/switch/, src/mbedtls/, src/samdisk/,
    #     src/formats/uff/ is not under our SSOT contract
    #   - comments are allowed (a `* "UFT 4.1.0"` in a docstring is fine)
    tool_version_re = re.compile(
        r'"(?:UFT\s+v?\d+\.\d+(?:\.\d+)?|UnifiedFloppyTool\s+v\d+\.\d+(?:\.\d+)?)"'
    )
    canon_dirs = [repo / "src", repo / "include"]
    vendored_prefixes = (
        "src/switch/", "src/mbedtls/", "src/samdisk/", "src/formats/uff/",
    )
    skip_files = {
        (repo / "include" / "uft" / "uft_version.h").resolve(),
    }
    for root in canon_dirs:
        if not root.is_dir():
            continue
        for path in list(root.rglob("*.c")) + list(root.rglob("*.cpp")) \
                  + list(root.rglob("*.h")) + list(root.rglob("*.hpp")):
            try:
                rel = path.resolve().relative_to(repo).as_posix()
            except ValueError:
                continue
            if path.resolve() in skip_files:
                continue
            if any(rel.startswith(p) for p in vendored_prefixes):
                continue
            try:
                text = path.read_text(encoding="utf-8", errors="replace")
            except OSError:
                continue
            in_block_comment = False
            for ln_no, line in enumerate(text.splitlines(), 1):
                code = line
                # Strip continuation of a /* ... */ block comment.
                if in_block_comment:
                    end = code.find("*/")
                    if end < 0:
                        continue
                    code = code[end + 2:]
                    in_block_comment = False
                # Strip same-line block comments and detect block-start
                # without a close.
                code = re.sub(r"/\*.*?\*/", "", code)
                if "/*" in code:
                    code = code[:code.index("/*")]
                    in_block_comment = True
                # Strip // line comments.
                code = code.split("//", 1)[0]
                stripped = code.lstrip()
                if stripped.startswith("*"):
                    continue
                m = tool_version_re.search(code)
                if not m:
                    continue
                errors.append(
                    f"{rel}:{ln_no}: hardcoded tool-version literal {m.group(0)} — "
                    "use UFT_VERSION_STRING or UFT_VERSION_FULL from uft_version.h"
                )

    return errors


_VENDORED_PREFIXES = (
    "src/switch/", "src/mbedtls/", "src/samdisk/", "src/formats/uff/",
)


def check_lazy_stubs(repo: Path) -> list[str]:
    """
    STUB_ELIMINATION_PLAN Phase-2/6 lock-in: block NEW lazy stubs.

    Pattern 1 — a uft_* function whose entire body is `(void)`-casts plus
    `return UFT_OK;`. Claiming success while doing nothing is a forensic
    lie (DESIGN_PRINCIPLE 4/7). Honest stubs return
    UFT_ERR_NOT_IMPLEMENTED and are NOT flagged. A deliberate no-op is
    allowed only with an `INTENTIONAL-NOOP` marker in the comment block
    directly above the definition (see uft_register_builtin_format_plugins
    for the reference use).

    Pattern 2 — a TODO/FIXME/XXX comment containing the word `implement`
    with no tracking token on the same line (KNOWN_ISSUES / MASTER_PLAN /
    STUB_ELIMINATION_PLAN / XCOPY / A8RAWCONV / MF-n / UFT-n / § / issue
    number). "TODO ohne Plan = Bug" per .claude/CLAUDE.md Anti-Goals.

    Calibrated 2026-07-02: tree is at 0/0 after MF-292 cleanup — every
    new finding is a regression, not baseline noise.
    """
    errors: list[str] = []

    def _strip(t: str) -> str:
        t = re.sub(r"/\*.*?\*/", "", t, flags=re.DOTALL)
        return re.sub(r"//[^\n]*", "", t)

    body_re = re.compile(r"\b(uft_\w+)\s*\([^;{]*\)\s*\{([^{}]*)\}", re.DOTALL)
    for c in (repo / "src").rglob("*.c"):
        rel = c.relative_to(repo).as_posix()
        if any(rel.startswith(v) for v in _VENDORED_PREFIXES):
            continue
        try:
            raw = c.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        stripped = _strip(raw)
        for m in body_re.finditer(stripped):
            body = m.group(2).strip()
            norm = re.sub(r"\(void\)\s*\w+\s*;", "", body).strip()
            if norm != "return UFT_OK;":
                continue
            # INTENTIONAL-NOOP carve-out: marker within 600 raw chars
            # before the definition (covers the doc-comment block).
            fn = m.group(1)
            pos = raw.find(fn + "(")
            if pos < 0:
                pos = raw.find(fn)
            window = raw[max(0, pos - 600):pos]
            if "INTENTIONAL-NOOP" in window:
                continue
            errors.append(
                f"{rel}: {fn}() body is only 'return UFT_OK;' — lazy stub. "
                "Implement it, return UFT_ERR_NOT_IMPLEMENTED, or mark "
                "INTENTIONAL-NOOP with justification."
            )

    tracked_re = re.compile(
        r"KNOWN_ISSUES|MASTER_PLAN|REFACTOR_TASKS|STUB_ELIMINATION"
        r"|XCOPY_INTEGRATION|A8RAWCONV|MF-\d|UFT-\d|§|issue\s*#?\d", re.I)
    todo_re = re.compile(r"\b(TODO|FIXME|XXX)\b", re.I)
    for base in ("src", "include"):
        for c in list((repo / base).rglob("*.c")) + list((repo / base).rglob("*.h")):
            rel = c.relative_to(repo).as_posix()
            if any(rel.startswith(v) for v in _VENDORED_PREFIXES):
                continue
            try:
                lines = c.read_text(encoding="utf-8", errors="replace").splitlines()
            except OSError:
                continue
            for ln_no, line in enumerate(lines, 1):
                if not todo_re.search(line):
                    continue
                if "implement" not in line.lower():
                    continue
                if tracked_re.search(line):
                    continue
                errors.append(
                    f"{rel}:{ln_no}: untracked 'TODO implement' — link it "
                    "(KNOWN_ISSUES / STUB_ELIMINATION_PLAN / MF-n) or "
                    "implement it. TODO ohne Plan = Bug."
                )

    return errors


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", type=Path,
                    default=Path(__file__).resolve().parent.parent)
    ap.add_argument("--warn-only", action="store_true",
                    help="Print findings but exit 0 (non-blocking mode)")
    ap.add_argument("--check",
                    choices=["includes", "sources", "libs", "version",
                             "stubs", "freeze", "all"],
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
    if args.check in ("stubs", "all"):
        e = check_lazy_stubs(repo)
        all_errors.append(("lazy-stub patterns", e))
    if args.check in ("stubs", "all"):
        # AUD-6 (MF-369): stale IN-SOURCE build artifacts poison every shadow
        # qmake build's depend scan (fatal: xcopytab.moc — qmake picks up old
        # mocs/ui headers from <repo>/release|debug). CI never sees them;
        # local shadow builds do. Fail fast with the fix.
        e = []
        for d in ("release", "debug"):
            if (repo / d).is_dir():
                e.append(f"stale in-source build dir '{d}/' exists — poisons "
                         f"shadow qmake builds; fix: rm -rf {d}")
        # MF-472: dieselbe Falle eine Ebene tiefer. `INCPATH` beginnt mit
        # `-I<repo> -I.` — die Repo-Wurzel steht VOR dem Shadow-Build-
        # Verzeichnis. Ein altes `ui_*.h` oder `moc_*.cpp` dort ueberschattet
        # damit die Fassung, die uic/moc gerade frisch erzeugt haben, und der
        # Compiler sieht eine Oberflaeche von vor vier Monaten. Der Fehler
        # liest sich dann als "class Ui::X has no member named Y", obwohl das
        # .ui-File das Widget sehr wohl enthaelt.
        #
        # Sie sind alle gitignored (`ui_*.h` in .gitignore:22), also sieht CI
        # sie nie — nur der lokale Baum leidet, und zwar still.
        for pat, tool in (("ui_*.h", "uic"), ("moc_*.cpp", "moc")):
            stale = sorted(p.name for p in repo.glob(pat))
            if stale:
                shown = ", ".join(stale[:3])
                more = f" (+{len(stale) - 3} weitere)" if len(stale) > 3 else ""
                e.append(
                    f"{len(stale)} veraltete {tool}-Artefakte in der "
                    f"Repo-Wurzel: {shown}{more}. INCPATH stellt <repo> vor "
                    f"das Shadow-Build-Verzeichnis, also gewinnen diese gegen "
                    f"die frisch erzeugten — der Compiler sieht eine alte "
                    f"Oberflaeche. Sie sind gitignored, CI merkt nichts; "
                    f"fix: rm -f {pat}")
        all_errors.append(("in-source build artifacts", e))
    if args.check in ("freeze", "all"):
        # Inventory SSOT + format-layer freeze rule (MF-363/364).
        sys.path.insert(0, str(Path(__file__).resolve().parent))
        import update_inventory as _inv
        all_errors.append(("inventory drift", _inv.check_inventory(repo)))
        all_errors.append(("format-layer freeze", _inv.check_freeze(repo)))
        all_errors.append(("corpus integrity",
                           _inv.check_corpus_manifest(repo)))
        all_errors.append(("verification tiers stale",
                           _inv.check_tiers_fresh(repo)))
        all_errors.append(("include guard collisions",
                           _inv.check_include_guards(repo)))
        all_errors.append(("unbacked compat claims",
                           _inv.check_compat_claims(repo)))
        all_errors.append(("macro value conflicts",
                           _inv.check_macro_conflicts(repo)))
        # A name that is an enum constant here and a macro there. The
        # preprocessor cannot see enum constants, so #ifndef guards around such
        # a name never fire and the macro wins silently — MF-427 cost 43
        # translation units a comparison that could never match.
        import enum_macro_conflicts as _enum_macro
        all_errors.append(("enum vs macro conflicts",
                           _enum_macro.check(repo)))
        # A hand-written `extern` in a .c file is a promise the compiler
        # believes. MF-442 found three of them wrong in one file, one causing
        # an arbitrary-address write. The class is currently empty — the gate
        # keeps it that way.
        import extern_decl_conflicts as _extern
        all_errors.append(("extern vs definition arity",
                           _extern.check(repo)))
        # The registry could hold 7 of 88 plugins (duplicate-check on the
        # container id, MAX_FORMAT_PLUGINS=32, and no caller at all), and every
        # cap failed silently because an empty registry answers NULL. MF-445
        # fixed all three; this keeps the capacity honest, .name unique, and
        # new uft_get_format_plugin() call sites out.
        import plugin_registry_gate as _plugreg
        all_errors.append(("plugin registry capacity/lookup",
                           _plugreg.check(repo)))
        # Three probe entry points had three buffer sizes, so one file came
        # back as two formats depending on which one the caller used — and
        # 4096 was too small for jv3_probe()'s 8960-byte floor, making JV3
        # unidentifiable through uft_disk_open(). One constant now; this keeps
        # it big enough and keeps literals out.
        import probe_buffer_gate as _probebuf
        all_errors.append(("probe buffer size", _probebuf.check(repo)))
        # Eight headers defined UFT_PACKED/UFT_PACK_BEGIN/UFT_PACK_END, and
        # they disagreed — for macros that decide the layout of structs cast
        # onto disk bytes. fat12_bpb_t was 40 bytes instead of 36 because a
        # UFT_PACK_BEGIN was missing, so every field after the OEM name was
        # read from the wrong offset. One definer now, and the pragmas must
        # balance.
        import packing_gate as _packing
        all_errors.append(("struct packing", _packing.check(repo)))
        # Zwei uft_platform.h mit demselben Guard: der grosse Header bekam
        # die compat-Schicht nie, zwei .c-Dateien sahen den grossen nie. Seit
        # MF-455 getrennt — die POSIX-Shims bleiben Opt-in, und Endianness
        # wird mit #if statt #ifdef gefragt (der Wert ist immer 0 oder 1).
        import platform_header_gate as _plathdr
        all_errors.append(("platform header split", _plathdr.check(repo)))
        # Sektoren pro Spur sind eine Eigenschaft des Laufwerks, nicht des
        # Dateiformats — der Fakt stand 24-mal im Baum, in drei unvertraeglichen
        # Indexkonventionen (ARCH-7). Die SSOT steht seit MF-434; dieser
        # Waechter rechnet jede verbliebene Kopie gegen sie, damit aus
        # "stimmen zufaellig ueberein" ein nachgewiesenes Uebereinstimmen wird.
        import cbm_zone_gate as _cbmzone
        all_errors.append(("CBM zone tables", _cbmzone.check(repo)))
        # UFT_HAS_LIBUSB setzte nur CMake — der qmake-Build, aus dem die
        # Releases entstehen, kompilierte den fertigen SCP-Lesepfad also
        # jahrelang in den Stub-Zweig, und beide Builds liefen dabei
        # fehlerfrei durch. verify_build_sources.py vergleicht Quelldateien,
        # keine Defines; genau diese Luecke schliesst der Waechter (MF-468).
        import define_parity_gate as _defparity
        all_errors.append(("build define parity", _defparity.check(repo)))
        # Das Sicherheitstor fuehrt Schreibrechte ein zweites Mal, neben der
        # Plugin-Registry — und die beiden liefen auseinander, in der
        # gefaehrlichen Richtung: WOZ, WOZ2, SCP, XDF und DMF galten dem Tor
        # als beschreibbar, obwohl kein Plugin sie schreiben kann (MF-491).
        # Ein Tor, das Schreibvorgaenge durchwinkt, die der Schreiber nicht
        # ausfuehren kann, erzeugt Vertrauen, das nichts traegt.
        import write_gate_caps_gate as _wgcaps
        all_errors.append(("write-gate caps vs plugins", _wgcaps.check(repo)))
        # 228 gebaute Module ruft niemand auf (ARCH-25, MF-509). Sie heute zu
        # beheben geht nicht — unerreichbarer Code ist per Definition
        # ungepruefter Code, und ihn ohne Referenz anzuschliessen waere die
        # Wette der fuenf fabrizierten Parser in groesserem Massstab. Was
        # geht, ist die Blutung stoppen: kein NEUES verwaistes Modul ohne
        # ausdrueckliche Entscheidung.
        import orphan_module_gate as _orphan
        all_errors.append(("neue verwaiste Module", _orphan.check(repo)))
        # Geteilte Include-Waechter (MF-511). Zwei Header, derselbe
        # #ifndef-Name, verschiedener Inhalt: der Praeprozessor meldet
        # nichts, er nimmt was zuerst kam. Bei einem Typ heisst das zwei
        # Layouts unter einem Namen — gefunden an uft_verify_result_t, wo
        # uft_verify_result_free() free() auf ein Feld gerufen haette, das
        # im anderen Layout kein Zeiger ist. 37 bestehende sind
        # eingefroren; was das Tor verhindert, ist die 38ste.
        import shared_guard_gate as _guards
        all_errors.append(("neue Waechter-Kollisionen", _guards.check(repo)))
        # read_track-Vertrag (MF-516). `uft_track_t.sectors` ist ein
        # dynamischer Zeiger; uft_track_init() legt ihn NICHT an. Wer
        # direkt `track->sectors[s] = ...` schreibt, schreibt durch NULL —
        # so ein read_track kann nie funktioniert haben. Zwoelf Plugins
        # taten es, woertlich derselbe kopierte Rumpf. Das Tor verhindert
        # den dreizehnten.
        import audit_read_track_contract as _rtc
        all_errors.append(("read_track schreibt durch NULL", _rtc.check(repo)))
        # Format-ID-Drift (MF-540). Fuenf Header definieren
        # `uft_format_id_t` unter demselben Waechter
        # UFT_FORMAT_ID_T_DEFINED — wer zuerst inkludiert wird, gewinnt,
        # der Rest wird still uebersprungen. `UFT_FMT_D64` ist je nach
        # Header 4, 6 oder 20. Gemessen: heute sehen alle gebauten
        # Einheiten dieselbe Zahl (4), die Falle ist also scharfgestellt,
        # aber nicht ausgeloest. Das Tor MISST je Uebersetzungseinheit mit
        # dem Praeprozessor und roetet an dem Tag, an dem eine
        # Include-Zeile die Bedeutung verschiebt.
        import audit_format_id_drift as _fid
        all_errors.append(("Format-ID-Drift je Einheit", _fid.check(repo)))
        # Tote Sonden (MF-546). uft_disk_open() waehlt AUSSCHLIESSLICH ueber
        # den Inhalt — die Endungs-Rueckfalllinie ist seit MF-444/449 weg.
        # Ein Plugin, dessen probe nie `true` liefert, steht damit in der
        # Registry, wird in der Format-Liste gezaehlt und ist unerreichbar.
        # Gemessen: genau EIN Fall im Baum (POSIX, mit Begruendung in der
        # Grundlinie). Das Tor verhindert den zweiten.
        import audit_dead_probe as _dp
        all_errors.append(("Sonden, die nie zustimmen", _dp.check(repo)))
        # Selbstvergleiche (MF-552). Drei der schwersten Funde dieser
        # Sitzung sind dasselbe Muster: eine Zahl wird mit sich selbst
        # verglichen, und die Pruefung kann nicht fehlschlagen.
        # MF-542 (CRC), MF-551 (Dateigroesse), MF-545 (Erfolg aus dem
        # Anlegen der Datei). Alle drei sahen im Quelltext wie eine
        # Pruefung aus.
        import audit_self_comparison as _sc
        all_errors.append(("Selbstvergleiche", _sc.check(repo)))
        # Ungedeckelte Groessen aus der Datei (MF-554). Die Form von
        # MF-543: eine Zahl kommt aus dem Dateikopf und bestimmt eine
        # Allokation oder Schleifengrenze, ohne gegen etwas geprueft zu
        # werden, das NICHT aus derselben Datei stammt. Beim Anlegen fand
        # das Tor drei echte Faelle (myz80, CPC-DSK-Index bis 65024 in ein
        # Feld mit 204 Eintraegen, xdf-Schiebeweite mit UB ab 32).
        import audit_unbounded_alloc as _ua
        all_errors.append(("Groessen ohne Schranke", _ua.check(repo)))
        # Verworfene Schreib-Antworten (MF-555). Dreimal in dieser Sitzung
        # war eine Stelle repariert und ihr Geschwister nicht (MF-526,
        # MF-550, MF-554). Dieses Tor sucht eine Klasse, in der das
        # besonders weh tut: ein Schreibaufruf, dessen Fehler niemand
        # liest — die Spur fehlt im Abbild und zaehlt trotzdem als
        # gewandelt. Gemessen beim Anlegen: 7 von 9 Aufrufen.
        import audit_discarded_result as _dr
        all_errors.append(("Verworfene Schreib-Antworten", _dr.check(repo)))
        # Kopierschutz-Katalog gegen die Behauptung (MF-557). CLAUDE.md und
        # README fuehren "55+ Verfahren" als Kernfunktion; gemessen sind es
        # 350 Funktionen mit 4 Aufrufern von aussen, davon einer ein
        # CRC-Helfer. Das Tor haelt die Zahl fest — wer verdrahtet oder
        # prueft, aendert sie, und die Doku muss nachziehen.
        import audit_protection_claims as _pc
        all_errors.append(("Kopierschutz-Behauptung", _pc.check(repo)))
        # Index aus cyl/head ohne untere Schranke (MF-560). Die CI fand mit
        # ASan einen heap-buffer-overflow in mfi_read_track: der Index wurde
        # nur nach OBEN geprueft. MF-516/522 hatte diese Klasse in 54
        # Dateien behoben; drei Stellen, die den Index VOR der Schranke
        # ausrechnen, sind durchgerutscht. Lokal unsichtbar — MinGW hat
        # keinen Sanitizer.
        import audit_negative_index as _ni
        all_errors.append(("Index ohne untere Schranke", _ni.check(repo)))
        # Schranke auf der falschen Groesse (MF-563). ASan fand in
        # dmk_read_track einen heap-buffer-overflow: die Schranke prueste
        # `idam_off`, indiziert wurde mit `DMK_IDAM_SIZE + idam_off` — um
        # 128 zu kurz. Eine Pruefung, die die falsche Zahl prueft, sieht
        # aus wie eine Pruefung. Die uebrigen Tore fangen das nicht: hier
        # FEHLT keine Schranke, sie steht nur auf der falschen Groesse.
        import audit_bound_on_wrong_value as _bw
        all_errors.append(("Schranke auf falscher Groesse", _bw.check(repo)))

        # 34. Kategorie (MF-569): eine Anzeige, die im Quelltext zugibt,
        # keine echten Daten zu zeigen, muss es auch auf dem Bildschirm
        # sagen. Der Datei-Browser lieferte 13 erfundene
        # Verzeichniseintraege, die Belegungskarte zeigte jede Diskette
        # als leer -- beides nur im Quelltext eingeraeumt. Der
        # Bootsektor-Hexdump daneben macht es richtig und beweist, dass
        # es geht.
        import audit_display_admits_placeholder as _dp
        all_errors.append(("Anzeige gibt Platzhalter zu", _dp.check(repo)))

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
