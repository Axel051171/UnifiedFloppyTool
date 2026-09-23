"""Turn inventory facts into an honest analysis queue."""

from __future__ import annotations

from pathlib import Path

from .util import read_json, write_json


def analyze_workspace(work: Path) -> dict:
    inventory_path = work / "inventory.json"
    if not inventory_path.exists():
        raise FileNotFoundError("inventory.json fehlt; zuerst inventory ausführen")
    inventory = read_json(inventory_path)

    hunk_files = []
    marker_counts: dict[str, int] = {}
    warnings = []
    for file in inventory.get("files", []):
        if file.get("is_hunk"):
            hunk_files.append(file["path"])
        for marker in file.get("markers", []):
            marker_counts[marker["name"]] = marker_counts.get(marker["name"], 0) + 1
        if file.get("hunk_error"):
            warnings.append(f"{file['path']}: Hunk nicht vollständig lesbar: "
                            f"{file['hunk_error']}")

    if not hunk_files:
        warnings.append("Keine ursprüngliche Amiga-Hunk-Binärdatei gefunden. "
                        "Disassemblies allein können nicht erneut entpackt werden.")

    result = {
        "schema": "uft-retrace-analysis-1",
        "hunk_files": hunk_files,
        "marker_counts": dict(sorted(marker_counts.items())),
        "warnings": warnings,
        "required_next_steps": [
            "Originalbinärdateien anhand der dokumentierten SHA-256 beschaffen",
            "Snapshot vor und nach dem Loader-Sprung erzeugen",
            "MMIO- und PC-Trace normalisieren",
            "nur ausgeführte Registerzugriffe als Funktionskandidaten behandeln",
            "Verhaltensfixture und Gegenprobe vor der C-Neuimplementierung erstellen",
        ],
    }
    write_json(work / "analysis.json", result)
    return result
