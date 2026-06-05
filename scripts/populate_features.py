#!/usr/bin/env python3
"""
populate_features.py — Mass-populate features array in plugin .c files.

V415-PLAN PLUGIN.features (MF-263) — closes docs/KNOWN_ISSUES.md §7.2.

For each `uft_format_plugin_t <name> = { ... };` literal that lacks an
explicit `.features = ...,` field, insert a minimal feature matrix
derived from the plugin's `.capabilities` bitfield:

  CAP_READ      → "Read"      → SUPPORTED if set
  CAP_WRITE     → "Write"     → SUPPORTED if set, UNSUPPORTED otherwise
  CAP_CREATE    → "Create"    → SUPPORTED if set, UNSUPPORTED otherwise
  CAP_FLUX      → "Flux"      → SUPPORTED if set, UNSUPPORTED otherwise
  CAP_TIMING    → "Timing"    → SUPPORTED if set, UNSUPPORTED otherwise
  CAP_WEAK_BITS → "Weak Bits" → SUPPORTED if set, UNSUPPORTED otherwise
  CAP_MULTI_REV → "MultiRev"  → SUPPORTED if set, UNSUPPORTED otherwise

The matrix is a STARTING POINT — per-plugin authors should refine
PARTIAL/note specifics as needed. This script makes 84/84 plugins
satisfy the "features != NULL" gate, closing §7.2 from the structural
"matrix infrastructure exists but is unpopulated" state.

Generates a per-plugin static `<plugin>_features[]` array immediately
before the plugin literal and inserts `.features = <plugin>_features,
.feature_count = sizeof(<plugin>_features)/sizeof(<plugin>_features[0]),`.
"""
from __future__ import annotations
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FMT_DIR = ROOT / "src" / "formats"

CAP_TO_FEATURE = [
    ("UFT_FORMAT_CAP_READ",      "Read"),
    ("UFT_FORMAT_CAP_WRITE",     "Write"),
    ("UFT_FORMAT_CAP_CREATE",    "Create"),
    ("UFT_FORMAT_CAP_FLUX",      "Flux"),
    ("UFT_FORMAT_CAP_TIMING",    "Timing"),
    ("UFT_FORMAT_CAP_WEAK_BITS", "Weak Bits"),
    ("UFT_FORMAT_CAP_MULTI_REV", "MultiRev"),
]

# Plugins where a hand-curated feature matrix would conflict with the
# auto-generated one (e.g. partial-support plugins) — skip them.
SKIP_HAND_CURATED = {
    # ADF/HFE/IPF/STX/WOZ already have hand-curated matrices in their
    # plugin .c — the script detects .features and skips automatically.
}


def extract_capability_bits(plugin_body: str) -> list[str]:
    """Find .capabilities = X | Y | Z and return the bit names."""
    m = re.search(r"\.capabilities\s*=\s*([^,;]+)", plugin_body)
    if not m:
        return []
    expr = m.group(1)
    return [cap for cap, _ in CAP_TO_FEATURE if cap in expr]


def render_features_array(name: str, caps: list[str]) -> str:
    """Render: static const uft_plugin_feature_t <name>_features[] = { ... };"""
    lines = [f"static const uft_plugin_feature_t {name}_features[] = {{"]
    for cap, label in CAP_TO_FEATURE:
        status = "UFT_FEATURE_SUPPORTED" if cap in caps else "UFT_FEATURE_UNSUPPORTED"
        lines.append(f'    {{ "{label}", {status}, NULL }},')
    lines.append("};")
    return "\n".join(lines)


def patch_file(path: Path, dry_run: bool = False) -> bool:
    text = path.read_text(encoding="utf-8", errors="replace")
    if ".features" in text and ".feature_count" in text:
        return False  # already populated

    # Find the plugin literal — capture the name + body separately.
    pat = re.compile(
        r"((?:static\s+)?(?:const\s+)?uft_format_plugin_t\s+(\w+)\s*=\s*\{)([^}]*?)(\n\};)",
        re.MULTILINE | re.DOTALL,
    )
    m = pat.search(text)
    if not m:
        print(f"  SKIP: {path.relative_to(ROOT)} — no plugin literal")
        return False

    header, plugin_name, body, close = m.group(1), m.group(2), m.group(3), m.group(4)
    caps = extract_capability_bits(body)
    if not caps:
        print(f"  SKIP: {path.relative_to(ROOT)} — no .capabilities found")
        return False

    # Find indent for consistency.
    indent_match = re.search(r"\n(\s+)\.", body)
    indent = indent_match.group(1) if indent_match else "    "

    # Build the features array — inserted BEFORE the plugin literal.
    arr = render_features_array(plugin_name, caps)

    # Build the .features insertion — appended at end of plugin literal.
    # Strip trailing whitespace; if the body ends with a `/* ... */`
    # comment, the comma we need to add belongs BEFORE the comment, not
    # after it (a stray comma after a comment is a syntax error in
    # designated init). So strip the trailing comment first, check
    # whether THAT body needs a comma, then re-attach the comment.
    body_stripped = body.rstrip()
    trailing_comment = ""
    cm = re.search(r"(\s*/\*[^*]*\*/\s*)$", body_stripped)
    if cm:
        trailing_comment = cm.group(1)
        body_stripped = body_stripped[: cm.start()].rstrip()
    if not body_stripped.endswith(","):
        body_stripped += ","
    body_stripped += trailing_comment
    feat_insertion = (
        f"\n{indent}.features = {plugin_name}_features,"
        f"  /* V415-PLAN PLUGIN.features (MF-263) */"
        f"\n{indent}.feature_count = sizeof({plugin_name}_features) / "
        f"sizeof({plugin_name}_features[0]),"
    )

    new_plugin = header + body_stripped + feat_insertion + close
    new_text = (
        text[: m.start()] +
        arr + "\n\n" +
        new_plugin +
        text[m.end():]
    )

    if not dry_run:
        path.write_text(new_text, encoding="utf-8", newline="\n")
    print(f"  WROTE {len(caps)} caps: {path.relative_to(ROOT)}")
    return True


def main() -> int:
    dry_run = "--dry-run" in sys.argv

    candidates: list[Path] = []
    for p in FMT_DIR.rglob("*.c"):
        text = p.read_text(encoding="utf-8", errors="replace")
        if not re.search(r"uft_format_plugin_t\s+\w+\s*=", text):
            continue
        candidates.append(p)

    print(f"Found {len(candidates)} candidate plugin files. Patching...")
    patched = 0
    for p in sorted(candidates):
        if patch_file(p, dry_run=dry_run):
            patched += 1

    print(f"\nResult: {patched} files patched ({'dry-run' if dry_run else 'written'})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
