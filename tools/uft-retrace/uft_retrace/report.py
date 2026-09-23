"""Generate a deterministic Markdown evidence report."""

from __future__ import annotations

from pathlib import Path

from .util import read_json


def render_report(work: Path) -> str:
    inventory = read_json(work / "inventory.json")
    analysis = read_json(work / "analysis.json") if (work / "analysis.json").exists() else {}
    trace_files = sorted(work.glob("*trace_summary*.json"))
    traces = [read_json(path) for path in trace_files]

    lines = [
        "# UFT ReTrace – Evidenzbericht",
        "",
        f"Schema: `{inventory.get('schema', '?')}`",
        f"Quelle: `{inventory.get('source', {}).get('name', '?')}`",
        f"Quell-SHA-256: `{inventory.get('source', {}).get('sha256') or 'Verzeichnis'}`",
        "",
        "## Inventar",
        "",
        "| Datei | Bytes | SHA-256 | Hunk |",
        "|---|---:|---|---|",
    ]
    for file in inventory.get("files", []):
        lines.append(f"| `{file['path']}` | {file['size']} | `{file['sha256']}` | "
                     f"{'ja' if file.get('is_hunk') else 'nein'} |")

    lines += ["", "## Hinweise und Grenzen", ""]
    warnings = analysis.get("warnings", [])
    if warnings:
        lines.extend(f"- {warning}" for warning in warnings)
    else:
        lines.append("- Keine strukturellen Warnungen.")

    lines += ["", "## Rohbyte-Marker", "",
              "Diese Treffer sind Suchhinweise, kein Beweis für ausgeführten Code.", "",
              "| Marker | Treffer |", "|---|---:|"]
    for name, count in analysis.get("marker_counts", {}).items():
        lines.append(f"| `{name}` | {count} |")

    lines += ["", "## Laufzeittraces", ""]
    if not traces:
        lines.append("Noch kein normalisierter Laufzeittrace vorhanden.")
    for trace in traces:
        lines.append(f"### `{trace['source']['name']}`")
        lines.append("")
        lines.append(f"- Ereignisse: {trace['event_count']}")
        for register, count in trace.get("register_accesses", {}).items():
            lines.append(f"- `{register}`: {count} Zugriffe")
        lines.append("")

    lines += [
        "## Clean-Room-Übergabe",
        "",
        "Eine Routine darf erst in UFT neu implementiert werden, wenn diese Punkte belegt sind:",
        "",
        "- ausgeführter PC-Bereich und Registerzugriffe dokumentiert;",
        "- definierte Eingabe und beobachtete Ausgabe;",
        "- mindestens eine Gegenprobe;",
        "- keine Übernahme historischer Binärbytes;",
        "- UFT-Test benutzt den öffentlichen Produktpfad;",
        "- CopyPlan-Fähigkeiten entsprechen lebenden Callbacks.",
        "",
    ]
    return "\n".join(lines)
