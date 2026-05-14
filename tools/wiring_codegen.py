#!/usr/bin/env python3
"""
UFT GUI Action Wiring Code Generator (P1.0 — skeleton)

Refactor branch: refactor/type-driven-hal
Spec:           docs/REFACTOR_BRIEF.md  (Type-Driven HAL, §Wiring)
Tasks:          docs/REFACTOR_TASKS.md  P1.0 / P1.2 / P1.4
Agent owner:    .claude/agents/wiring-codegen-author.md  (rule H-3 / H-4)

WHAT THIS DOES
--------------
Reads a Qt Designer .ui XML file and a hand-written `*.actions.yaml`
that maps named widgets to type-driven HAL capabilities, and emits a
deterministic `<tab>_wiring.gen.cpp` that calls
`wire_action<Capability>(...)` for each entry.

The generated file is the **structural enforcement** of:

  H-3   "Action without provider" — a YAML entry referencing a method no
        provider implements would compile-fail at the call site of
        `wire_action<Capability>()`.

  H-4   "Provider without action" — declared capabilities that aren't
        wired anywhere get flagged by the conformance test (P1.5),
        not here.

  F-4   3-part errors (what / why / fix) — every error this script
        emits follows that contract verbatim.

PURE-PYTHON, NO EXTERNAL DEPENDENCIES
-------------------------------------
We parse YAML by hand on a restricted subset (the YAML this codegen
actually accepts) so the build does not need `pip install pyyaml`.
The accepted subset is documented in YAML_SUBSET below and matches
the schema the wiring-codegen-author agent describes in its frontmatter.

DETERMINISM CONTRACT
--------------------
Output bytes MUST be identical when inputs are identical, regardless
of:
  - dict insertion order (Python 3.7+ preserves but Cpp output sorts)
  - filesystem mtimes (no timestamps in output)
  - environment locale

Any nondeterministic source = bug. CI compares
`git diff --exit-code generated/`.

EXIT CODES
----------
  0   success, file written
  2   bad arguments / missing input
  3   YAML schema violation
  4   widget referenced in YAML not present in .ui (rule H-3 surface)
  5   capability concept not in known set (rule H-4 surface)
"""

from __future__ import annotations

import argparse
import sys
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Any

# ════════════════════════════════════════════════════════════════════════
# Schema constants
# ════════════════════════════════════════════════════════════════════════

YAML_SUBSET = """
Accepted YAML subset (intentionally small — additive changes only):

  tab: <CamelCase identifier>          # required; Cpp class name
  provider_source: <Cpp expression>    # required; produces a V2 provider*
  extra_includes:                      # optional; list of header paths
    - <header.h>                       #   prepended above the wiring_runtime.h
                                       #   include in the generated file. Use
                                       #   for the tab class header, the uic
                                       #   header, the V2 provider header, etc.
  actions:                             # required; list of action entries
    - widget: <name from .ui>
      requires: <Capability>           # one of KNOWN_CAPABILITIES
      invoke: <method-call expression> # e.g. set_motor(true)
      on_outcome:                      # optional; map of variant alt -> handler
        <AltName>: <handler expression>
"""

# Must mirror include/uft/hal/concepts.h. When concepts.h adds/renames
# a concept, this list MUST be updated in the same commit. The
# type-system-architect agent owns that synchronization.
KNOWN_CAPABILITIES: tuple[str, ...] = (
    "HasIdentity",
    "ReadsSectors",
    "ReadsRawFlux",
    "WritesSectors",
    "WritesRawFlux",
    "ControlsMotor",
    "SeeksHead",
    "Recalibrates",
    "MeasuresRPM",
    "DetectsDrive",
    # Composites (from concepts.h)
    "ImagesFlux",
    "ImagesSectors",
    "WritesAnything",
    "FullDriveControl",
)


# ════════════════════════════════════════════════════════════════════════
# Error helper — F-4 3-part contract
# ════════════════════════════════════════════════════════════════════════


class WiringError(Exception):
    """3-part error: what / why / fix (rule F-4)."""

    def __init__(self, what: str, why: str, fix: str, exit_code: int = 3):
        self.what = what
        self.why = why
        self.fix = fix
        self.exit_code = exit_code
        super().__init__(f"{what}\n  why: {why}\n  fix: {fix}")


def fail(what: str, why: str, fix: str, exit_code: int) -> "None":
    err = WiringError(what, why, fix, exit_code)
    print(f"[wiring_codegen] ERROR: {err.what}", file=sys.stderr)
    print(f"  why: {err.why}", file=sys.stderr)
    print(f"  fix: {err.fix}", file=sys.stderr)
    sys.exit(err.exit_code)


# ════════════════════════════════════════════════════════════════════════
# Tiny YAML parser — accepts only the documented subset
# ════════════════════════════════════════════════════════════════════════


def parse_yaml(text: str, source_label: str) -> dict[str, Any]:
    """Minimal YAML parser for the documented subset.

    Supports:  scalar mappings, list of mappings under `actions:`,
               nested mapping under `on_outcome:`, comments, blank lines.
    Rejects:   anchors, aliases, multi-doc, flow style, tags.
    """
    root: dict[str, Any] = {}
    actions: list[dict[str, Any]] = []
    cur_action: dict[str, Any] | None = None
    in_actions = False
    in_extra_includes = False
    in_on_outcome = False

    for raw_lineno, raw in enumerate(text.splitlines(), start=1):
        # Strip comments. A '#' inside a quoted value would be a problem
        # in general YAML, but the subset has no quoted values, so this
        # is safe.
        line = raw.split("#", 1)[0].rstrip()
        if not line.strip():
            continue

        indent = len(line) - len(line.lstrip(" "))
        body = line.strip()

        # Top-level key: value
        if indent == 0:
            in_actions = False
            in_extra_includes = False
            in_on_outcome = False
            cur_action = None
            if body == "actions:":
                in_actions = True
                continue
            if body == "extra_includes:":
                in_extra_includes = True
                root["extra_includes"] = []
                continue
            if ":" not in body:
                fail(
                    "malformed top-level YAML line",
                    f"line {raw_lineno} of {source_label} has no ':' separator: {body!r}",
                    "use the form 'key: value' at column 0",
                    3,
                )
            k, _, v = body.partition(":")
            root[k.strip()] = v.strip()
            continue

        # `extra_includes:` list of scalar strings — '- header.h'
        if in_extra_includes and body.startswith("- "):
            item = body[2:].strip()
            root["extra_includes"].append(item)
            continue

        # `actions:` list item — '-' at indent>=2
        if in_actions and body.startswith("- "):
            if cur_action is not None:
                actions.append(cur_action)
            cur_action = {}
            after_dash = body[2:].strip()
            if ":" in after_dash:
                k, _, v = after_dash.partition(":")
                cur_action[k.strip()] = v.strip()
            in_on_outcome = False
            continue

        # Inside an action entry
        if in_actions and cur_action is not None:
            if body == "on_outcome:":
                cur_action["on_outcome"] = {}
                in_on_outcome = True
                continue
            if in_on_outcome and ":" in body:
                k, _, v = body.partition(":")
                cur_action["on_outcome"][k.strip()] = v.strip()  # type: ignore[index]
                continue
            if ":" in body:
                k, _, v = body.partition(":")
                cur_action[k.strip()] = v.strip()
                in_on_outcome = False
                continue

        fail(
            "unrecognized YAML line",
            f"line {raw_lineno} of {source_label} did not match the accepted subset: {body!r}",
            f"see YAML_SUBSET in {Path(__file__).name} for what is accepted",
            3,
        )

    if cur_action is not None:
        actions.append(cur_action)
    if actions:
        root["actions"] = actions
    return root


# ════════════════════════════════════════════════════════════════════════
# .ui parsing
# ════════════════════════════════════════════════════════════════════════


def collect_widget_names(ui_path: Path) -> set[str]:
    try:
        tree = ET.parse(ui_path)
    except ET.ParseError as exc:
        fail(
            "Qt Designer .ui XML parse error",
            f"{ui_path}: {exc}",
            "open the file in Qt Designer; if it loads, save and retry",
            2,
        )
    root = tree.getroot()
    names: set[str] = set()
    for w in root.iter("widget"):
        n = w.get("name")
        if n:
            names.add(n)
    return names


# ════════════════════════════════════════════════════════════════════════
# Validation
# ════════════════════════════════════════════════════════════════════════


REQUIRED_TOP = ("tab", "provider_source", "actions")
REQUIRED_ACTION = ("widget", "requires", "invoke")


def validate(spec: dict[str, Any], known_widgets: set[str]) -> None:
    for key in REQUIRED_TOP:
        if key not in spec:
            fail(
                f"missing required top-level YAML key '{key}'",
                "every actions.yaml needs tab, provider_source and actions",
                "add the key; see YAML_SUBSET in tools/wiring_codegen.py",
                3,
            )

    actions = spec["actions"]
    if not isinstance(actions, list) or not actions:
        fail(
            "'actions' must be a non-empty list",
            f"got: {type(actions).__name__} with {len(actions) if hasattr(actions, '__len__') else '?'} items",
            "add at least one action entry under 'actions:'",
            3,
        )

    for idx, a in enumerate(actions):
        for key in REQUIRED_ACTION:
            if key not in a:
                fail(
                    f"action #{idx}: missing key '{key}'",
                    f"action contents: {a}",
                    "every action needs widget, requires, and invoke",
                    3,
                )
        if a["widget"] not in known_widgets:
            fail(
                f"action #{idx}: widget '{a['widget']}' not in .ui",
                "widget name is referenced in YAML but absent from the Designer file",
                "rename in YAML to match the .ui, or add the widget in Designer",
                4,  # rule H-3 surface
            )
        if a["requires"] not in KNOWN_CAPABILITIES:
            fail(
                f"action #{idx}: capability '{a['requires']}' is unknown",
                f"not in KNOWN_CAPABILITIES = {KNOWN_CAPABILITIES}",
                "either add the concept to include/uft/hal/concepts.h "
                "(via type-system-architect) and update KNOWN_CAPABILITIES, "
                "or fix the YAML",
                5,  # rule H-4 surface
            )


# ════════════════════════════════════════════════════════════════════════
# Code emission — deterministic
# ════════════════════════════════════════════════════════════════════════


HEADER = """\
// SPDX-License-Identifier: GPL-3.0-or-later
//
// AUTO-GENERATED — DO NOT EDIT
//
// Generated by:  tools/wiring_codegen.py
// Inputs:        {ui_rel}
//                {yaml_rel}
//
// Hand-edits to this file will be reverted. Change the .ui (in Qt Designer)
// or the .actions.yaml (by hand) and re-run the codegen instead.
//
// This file is the structural enforcement of rules H-3 / H-4 from the
// UFT Master Coding Standards: a YAML entry referencing a capability no
// provider implements will fail to compile at the wire_action<cap::X> call.

// MF-210: the emitted body uses std::visit / std::holds_alternative /
// std::is_same_v / std::monostate directly (MF-205 made provider_source a
// std::variant). Include <variant> + <type_traits> here rather than rely
// on a transitive include from hardwaretab.h / wiring_runtime.h.
#include <type_traits>
#include <variant>

{extra_includes_block}#include "uft/gui/wiring_runtime.h"

namespace uft::gui::generated {{

void wire_{tab_snake}(class {tab} *self) {{
    namespace cap = ::uft::gui::cap;
"""

FOOTER = """\
}}  // wire_{tab_snake}

}}  // namespace uft::gui::generated
"""


def camel_to_snake(name: str) -> str:
    out: list[str] = []
    for i, c in enumerate(name):
        if c.isupper() and i > 0 and not name[i - 1].isupper():
            out.append("_")
        out.append(c.lower())
    return "".join(out)


def emit(spec: dict[str, Any], ui_rel: str, yaml_rel: str) -> str:
    tab = spec["tab"]
    provider_source = spec["provider_source"]
    actions = spec["actions"]
    sorted_actions = sorted(actions, key=lambda x: x["widget"])

    # Build the extra-includes block. YAML order is preserved — the YAML
    # author controls include order, important for headers with implicit
    # dependencies (e.g. ui_*.h after the tab header).
    extra_includes = spec.get("extra_includes", [])
    if extra_includes:
        extra_includes_block = (
            "".join(f'#include "{h}"\n' for h in extra_includes) + "\n"
        )
    else:
        extra_includes_block = ""

    parts: list[str] = []
    parts.append(
        HEADER.format(
            ui_rel=ui_rel,
            yaml_rel=yaml_rel,
            tab=tab,
            tab_snake=camel_to_snake(tab),
            provider_source=provider_source,
            extra_includes_block=extra_includes_block,
        )
    )

    # ── Phase 1: tear-down any prior wiring on every action button ────
    # This makes wire_<tab>(self) idempotent + safe to call repeatedly.
    # Without it, a second invocation would STACK new clicked-handlers on
    # top of old ones (silent double-execution) and the old handlers
    # would carry captured pointers to a possibly-destroyed prior provider.
    parts.append(
        "    /* Phase 1 — tear down any wiring left from a previous call.\n"
        "     *\n"
        "     * `wire_<tab>` is the entry point for both initial wire-up\n"
        "     * (provider null) and re-wire (provider switched). Without\n"
        "     * this disconnect, repeated calls would stack signal handlers\n"
        "     * (silent double-execution) and lambdas captured against an\n"
        "     * already-destroyed prior provider would dangle until the\n"
        "     * next click. Disconnecting a still-valid lambda is a no-op\n"
        "     * — Qt does not invoke its body — so this is safe. */\n"
    )
    for a in sorted_actions:
        parts.append(
            f"    QObject::disconnect(self->ui->{a['widget']},"
            f" &QAbstractButton::clicked, nullptr, nullptr);\n"
        )
    parts.append("\n")

    # ── Phase 2: provider-disconnected gate ───────────────────────────
    # MF-205 (P1.23): provider_source now yields a ProviderV2Variant
    # (std::variant<std::monostate, unique_ptr<...>x9>). "No provider
    # connected" is the std::monostate alternative. Disable every action
    # button in that case. Keeps the UI consistent with reality (rule
    # H-1) without the caller knowing which buttons are capability-bound.
    parts.append(
        f"    const auto &providerVar = {provider_source};\n"
        "    if (std::holds_alternative<std::monostate>(providerVar)) {\n"
        "        /* Phase 2 — no provider connected (monostate): disable\n"
        "         * every action button. They are re-enabled by Phase 3\n"
        "         * on a later call once a V2 provider is connected. */\n"
    )
    for a in sorted_actions:
        parts.append(
            f"        self->ui->{a['widget']}->setEnabled(false);\n"
        )
    parts.append(
        "        return;\n"
        "    }\n"
        "\n"
        "    /* Phase 3 — wire each action against the connected provider.\n"
        "     * std::visit dispatches on the active variant alternative;\n"
        "     * inside the lambda `p` is a CONCRETE provider pointer, so\n"
        "     * wire_action<cap::X> instantiates per concrete type and its\n"
        "     * capability gating stays 100% structural (rule H-3). */\n"
    )

    # ── Phase 3: per-action wire_action emission ──────────────────────
    # MF-205 (P1.23): each wire_action<cap::X> call is wrapped in a
    # std::visit over the variant. The INNER emission (wire_action, the
    # generic-lambda factory, the per-alternative handlers) is unchanged
    # from the pre-P1.23 codegen — only the std::visit shell + the
    # `provider` -> `p` substitution are new.
    for a in sorted_actions:
        widget = a["widget"]
        cap = a["requires"]
        invoke = a["invoke"]
        parts.append(
            f"    // widget={widget}  requires={cap}  invoke={invoke}\n"
            f"    std::visit([&]<class HeldT>(const HeldT &held) {{\n"
            f"        if constexpr (std::is_same_v<HeldT, std::monostate>) {{\n"
            f"            /* Unreachable after the Phase-2 guard, but\n"
            f"             * std::visit must be exhaustive — disable\n"
            f"             * defensively. */\n"
            f"            self->ui->{widget}->setEnabled(false);\n"
            f"        }} else {{\n"
            f"            auto *p = held.get();  /* concrete provider* */\n"
            f"            ::uft::gui::wire_action<cap::{cap}>(\n"
            f"                self->ui->{widget},\n"
            f"                p,\n"
            # Generic-lambda invoke factory: body type-checked lazily —
            # see WHY INVOKE IS A GENERIC LAMBDA in wiring_runtime.h.
            f"                []<class P>(P *q) {{ return q->{invoke}; }}"
        )
        on = a.get("on_outcome", {}) or {}
        if on:
            parts.append(",\n")
            for alt in sorted(on.keys()):
                handler = on[alt]
                parts.append(
                    # Concrete-typed handler: one alternative of the variant
                    # by const-ref. std::visit enforces exhaustiveness — a
                    # missing alternative fails to compile.
                    f"                /* on {alt}: */ "
                    f"[self](const ::uft::hal::{alt} &v) {{ {handler}; }},\n"
                )
            # remove trailing ',\n' on the last handler line
            parts[-1] = parts[-1].rstrip(",\n") + "\n"
        else:
            parts.append("\n")
        parts.append("            );\n        }\n    }, providerVar);\n\n")

    parts.append(FOOTER.format(tab_snake=camel_to_snake(tab)))
    return "".join(parts)


# ════════════════════════════════════════════════════════════════════════
# CLI
# ════════════════════════════════════════════════════════════════════════


def main(argv: list[str]) -> int:
    p = argparse.ArgumentParser(
        prog="wiring_codegen.py",
        description="UFT GUI action wiring code generator (P1.0 skeleton)",
    )
    p.add_argument("--ui", type=Path, required=True, help="Qt Designer .ui file")
    p.add_argument("--actions", type=Path, required=True, help="*.actions.yaml")
    p.add_argument(
        "--output",
        type=Path,
        required=False,
        default=None,
        help="output .gen.cpp (required unless --check is given)",
    )
    p.add_argument(
        "--check",
        action="store_true",
        help="validate inputs without writing output (CI gate)",
    )
    args = p.parse_args(argv)
    if not args.check and args.output is None:
        fail(
            "--output is required when --check is not given",
            "the codegen needs to know where to emit the .gen.cpp",
            "pass --output <path> or use --check to validate without writing",
            2,
        )

    for arg_name, path in (("ui", args.ui), ("actions", args.actions)):
        if not path.is_file():
            fail(
                f"--{arg_name} input not found: {path}",
                "the codegen needs both inputs to validate cross-references",
                f"create the file or correct the --{arg_name} argument",
                2,
            )

    spec = parse_yaml(args.actions.read_text(encoding="utf-8"), str(args.actions))
    widgets = collect_widget_names(args.ui)
    validate(spec, widgets)

    # Normalize paths to forward-slash so identical inputs on Windows
    # and Linux produce byte-identical output (determinism contract).
    code = emit(
        spec,
        ui_rel=str(args.ui).replace("\\", "/"),
        yaml_rel=str(args.actions).replace("\\", "/"),
    )

    if args.check:
        print(f"[wiring_codegen] check OK: {len(spec['actions'])} action(s) validated")
        return 0

    args.output.parent.mkdir(parents=True, exist_ok=True)
    # Write only if content changed — avoids touching mtime on no-op runs.
    if args.output.exists():
        prev = args.output.read_text(encoding="utf-8")
        if prev == code:
            print(f"[wiring_codegen] up-to-date: {args.output}")
            return 0
    args.output.write_text(code, encoding="utf-8", newline="\n")
    print(f"[wiring_codegen] wrote {args.output} ({len(spec['actions'])} action(s))")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
