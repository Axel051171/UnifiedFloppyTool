"""Command-line entry point."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from .analyze import analyze_workspace
from .inventory import create_inventory
from .report import render_report
from .snapshot import compare_files
from .trace import summarize_trace
from .util import write_json


def number(value: str) -> int:
    return int(value, 0)


def parser() -> argparse.ArgumentParser:
    root = argparse.ArgumentParser(prog="uft-retrace")
    sub = root.add_subparsers(dest="command", required=True)

    inv = sub.add_parser("inventory", help="Eingabe inventarisieren und Hunks extrahieren")
    inv.add_argument("source", type=Path)
    inv.add_argument("-o", "--output", required=True, type=Path)

    ana = sub.add_parser("analyze", help="Inventar in eine Analysewarteschlange überführen")
    ana.add_argument("workspace", type=Path)
    ana.add_argument("-o", "--output", type=Path)

    snap = sub.add_parser("snapshot-diff", help="Speicherabbilder vergleichen")
    snap.add_argument("before", type=Path)
    snap.add_argument("after", type=Path)
    snap.add_argument("--base", type=number, default=0)
    snap.add_argument("--min-run", type=int, default=16)
    snap.add_argument("--merge-gap", type=int, default=8)
    snap.add_argument("-o", "--output", required=True, type=Path)

    trace = sub.add_parser("trace", help="JSONL-/CSV-Trace normalisieren und auswerten")
    trace.add_argument("source", type=Path)
    trace.add_argument("-o", "--output", required=True, type=Path)

    report = sub.add_parser("report", help="Markdown-Evidenzbericht erzeugen")
    report.add_argument("workspace", type=Path)
    report.add_argument("-o", "--output", required=True, type=Path)
    return root


def main(argv=None) -> int:
    args = parser().parse_args(argv)
    try:
        if args.command == "inventory":
            create_inventory(args.source, args.output)
        elif args.command == "analyze":
            result = analyze_workspace(args.workspace)
            if args.output and args.output != args.workspace / "analysis.json":
                write_json(args.output, result)
        elif args.command == "snapshot-diff":
            write_json(args.output, compare_files(args.before, args.after,
                                                   args.base, args.min_run,
                                                   args.merge_gap))
        elif args.command == "trace":
            write_json(args.output, summarize_trace(args.source))
        elif args.command == "report":
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(render_report(args.workspace),
                                   encoding="utf-8", newline="\n")
    except (OSError, ValueError) as exc:
        print(f"Fehler: {exc}", file=sys.stderr)
        return 2
    return 0
