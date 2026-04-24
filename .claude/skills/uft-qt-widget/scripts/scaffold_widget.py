#!/usr/bin/env python3
"""scaffold_widget.py — Scaffold a Qt6 widget + header + stub .ui file."""

import argparse
import re
import sys
from pathlib import Path

SKILL_DIR = Path(__file__).resolve().parent.parent
TEMPLATES = SKILL_DIR / "templates"


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--name", required=True,
                   help="widget name (lowercase, e.g. 'foo_panel')")
    p.add_argument("--class", dest="class_name", required=True,
                   help="C++ class name (e.g. 'UftFooPanel')")
    p.add_argument("--kind", default="analysis_panel",
                   choices=["analysis_panel", "tab", "dialog", "dock"])
    p.add_argument("--description", default="New UFT widget")
    p.add_argument("--uft-root", type=Path, default=Path.cwd())
    p.add_argument("--dry-run", action="store_true")
    args = p.parse_args()

    if not (args.uft_root / "UnifiedFloppyTool.pro").exists():
        print(f"ERROR: not UFT root", file=sys.stderr)
        return 1

    subs = {
        "name": args.name,
        "Class": args.class_name,
        "Description": args.description,
    }

    # Pick directory by kind
    if args.kind == "analysis_panel":
        dir_ = args.uft_root / "src" / "gui"
    elif args.kind == "tab":
        dir_ = args.uft_root / "src"
    elif args.kind == "dialog":
        dir_ = args.uft_root / "src"
    else:
        dir_ = args.uft_root / "src" / "widgets"

    h_dst = dir_ / f"uft_{args.name}.h"
    cpp_dst = dir_ / f"uft_{args.name}.cpp"
    ui_dst = args.uft_root / "ui" / f"{args.name}.ui"

    for dst, tmpl_name in [(h_dst, "widget.h.tmpl"),
                             (cpp_dst, "widget.cpp.tmpl")]:
        if dst.exists():
            print(f"  [skip] {dst.relative_to(args.uft_root)} exists")
            continue
        text = (TEMPLATES / tmpl_name).read_text()
        for k, v in subs.items():
            text = text.replace(f"@@{k}@@", str(v))
        if args.dry_run:
            print(f"  [dry]  would create {dst.relative_to(args.uft_root)}")
        else:
            dst.parent.mkdir(parents=True, exist_ok=True)
            dst.write_text(text)
            print(f"  [new]  {dst.relative_to(args.uft_root)}")

    # Stub .ui file
    if not ui_dst.exists():
        stub = f'''<?xml version="1.0" encoding="UTF-8"?>
<ui version="4.0">
 <class>{args.class_name}</class>
 <widget class="QWidget" name="{args.class_name}">
  <layout class="QVBoxLayout">
   <item>
    <widget class="QPushButton" name="btnStart">
     <property name="text"><string>Start</string></property>
    </widget>
   </item>
   <item>
    <widget class="QPushButton" name="btnReset">
     <property name="text"><string>Reset</string></property>
    </widget>
   </item>
  </layout>
 </widget>
</ui>
'''
        if args.dry_run:
            print(f"  [dry]  would create {ui_dst.relative_to(args.uft_root)}")
        else:
            ui_dst.parent.mkdir(parents=True, exist_ok=True)
            ui_dst.write_text(stub)
            print(f"  [new]  {ui_dst.relative_to(args.uft_root)}")

    # Patch .pro
    pro = args.uft_root / "UnifiedFloppyTool.pro"
    if pro.exists():
        text = pro.read_text()
        rel_cpp = str(cpp_dst.relative_to(args.uft_root))
        rel_h = str(h_dst.relative_to(args.uft_root))
        rel_ui = str(ui_dst.relative_to(args.uft_root))

        if rel_cpp not in text:
            m = re.search(r"^SOURCES\s*[\+=]{1,2}\s*\\", text, re.M)
            if m:
                # Find the last source line to append after
                lines = text.split("\n")
                idx = None
                for i, line in enumerate(lines):
                    if line.strip().startswith("src/") and line.endswith("\\"):
                        idx = i
                if idx is not None:
                    lines.insert(idx + 1, f"    {rel_cpp} \\")
                    text = "\n".join(lines)
                    if args.dry_run:
                        print(f"  [dry]  would add {rel_cpp} to SOURCES")
                    else:
                        pro.write_text(text)
                        print(f"  [edit] added {rel_cpp} to SOURCES")

    print()
    print("Next:")
    print(f"  1. Design UI visually: designer {ui_dst.relative_to(args.uft_root)}")
    print(f"  2. Implement onStartClicked() — use QThread worker for >16ms ops")
    print(f"  3. Wire from mainwindow.cpp: #include and instantiate")
    print(f"  4. qmake && make -j$(nproc)")
    print(f"  5. Manual GUI check: ./uft and verify widget appears")
    return 0


if __name__ == "__main__":
    sys.exit(main())
