#!/usr/bin/env python3
"""Wächter gegen wachsende Unerreichbarkeit (MF-509, ARCH-25).

── Warum es diesen Wächter gibt ─────────────────────────────────────────

`audit_orphan_modules.py` hat gemessen: von 537 gebauten Quelldateien mit
exportierten Funktionen ruft **niemand 228 auf**. 42 % der Modulfläche.

Diese 228 heute zu beheben geht nicht, und zwar aus einem Grund, der
festgehalten gehört: unerreichbarer Code ist per Definition ungeprüfter
Code. Fünf Format-Parser dieses Baums waren gegen erfundene Specs gebaut
(FMT-2/3/10/11/12). Sie einfach anzuschließen hieße, dieselbe Wette in
größerem Maßstab noch einmal einzugehen — die Einfrier-Regel (MF-498)
verlangt für jeden von ihnen eine benannte Referenz.

Was heute geht, ist die Blutung stoppen: **kein neues verwaistes Modul
ohne ausdrückliche Entscheidung.** Ein unbegrenztes Problem wird damit zu
einem begrenzten.

── Wie der Wächter arbeitet ─────────────────────────────────────────────

Er vergleicht die gemessene Zahl gegen eine eingefrorene Grundlinie
(`docs/orphan_baseline.txt`, eine Datei mit den bekannten Pfaden).

- **Neue Verwaiste** → Fehler. Wer ein Modul hinzufügt, das niemand ruft,
  soll das jetzt entscheiden und nicht in sechs Monaten entdecken.
- **Weniger Verwaiste** → Hinweis mit den Namen und die Aufforderung, die
  Grundlinie zu kürzen. Kein Fehler: Fortschritt darf nicht rot leuchten.
- **Gleich viele** → still.

Die Grundlinie ist bewusst eine Liste von Pfaden und keine Zahl. Eine Zahl
ließe zu, dass ein Modul verdrahtet und ein anderes verwaist wird, ohne
dass jemand es merkt.
"""
from __future__ import annotations

import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
AUDIT = ROOT / "scripts" / "audit_orphan_modules.py"
BASELINE = ROOT / "docs" / "orphan_baseline.txt"


def measure() -> set[str]:
    """Die aktuell verwaisten Module, als Pfadmenge."""
    r = subprocess.run([sys.executable, str(AUDIT), "--detail"],
                       cwd=ROOT, capture_output=True, text=True, timeout=600)
    if r.returncode != 0:
        print("audit_orphan_modules.py schlug fehl:")
        print(r.stdout[-2000:])
        print(r.stderr[-2000:])
        sys.exit(2)

    # Die Detailausgabe hat ZWEI Abschnitte: "nur Tests rufen auf" und
    # "Ohne jeden Aufrufer". Nur der zweite zaehlt hier — ein Modul, das
    # wenigstens ein Test benutzt, ist geprueft und nicht verwaist. Die
    # erste Fassung dieses Waechters las beide und meldete 306 statt 228;
    # ein Waechter auf falscher Messung ist schlimmer als keiner.
    found: set[str] = set()
    in_section = False
    for line in r.stdout.splitlines():
        s = line.strip()
        if s.startswith("Ohne jeden Aufrufer"):
            in_section = True
            continue
        if s and not s.startswith("src/") and not line.startswith(" "):
            in_section = False          # naechste Ueberschrift
        if not in_section or not s.startswith("src/"):
            continue
        path = s.split()[0]
        if path.endswith((".c", ".cpp")):
            found.add(path.replace("\\", "/"))
    return found


def load_baseline() -> set[str] | None:
    if not BASELINE.exists():
        return None
    out: set[str] = set()
    for line in BASELINE.read_text(encoding="utf-8").splitlines():
        s = line.strip()
        if s and not s.startswith("#"):
            out.add(s.replace("\\", "/"))
    return out


def main() -> int:
    now = measure()
    base = load_baseline()

    if base is None:
        BASELINE.parent.mkdir(parents=True, exist_ok=True)
        BASELINE.write_text(
            "# Verwaiste Module — eingefrorene Grundlinie (ARCH-25, MF-509).\n"
            "#\n"
            "# Jede Zeile ist ein gebautes Modul mit exportierten Funktionen,\n"
            "# das NIEMAND aufruft. Fuer jedes gibt es genau drei zulaessige\n"
            "# Ausgaenge: verdrahten, loeschen, oder ausdruecklich als\n"
            "# unerreichbar dokumentieren. Was nicht geht: liegenlassen UND\n"
            "# weiter als Faehigkeit fuehren.\n"
            "#\n"
            "# Wird eines behoben, gehoert seine Zeile hier heraus — das Tor\n"
            "# sagt dann, welche.\n"
            + "".join(sorted(p + "\n" for p in now)),
            encoding="utf-8")
        print("Grundlinie angelegt: %d verwaiste Module" % len(now))
        return 0

    added = sorted(now - base)
    gone = sorted(base - now)

    print("Verwaiste Module: %d (Grundlinie %d)" % (len(now), len(base)))

    if gone:
        print("\n  %d weniger als in der Grundlinie — bitte dort streichen:"
              % len(gone))
        for p in gone[:20]:
            print("    - " + p)
        if len(gone) > 20:
            print("    ... und %d weitere" % (len(gone) - 20))

    if added:
        print("\nFEHLER: %d NEUE verwaiste Module." % len(added))
        for p in added:
            print("    + " + p)
        print("\n  Ein Modul, das niemand aufruft, ist keine Faehigkeit.")
        print("  Entweder verdrahten, oder nicht hinzufuegen, oder die")
        print("  Grundlinie bewusst erweitern und begruenden.")
        return 1

    return 0


def check(repo) -> list:
    """Schnittstelle fuer check_consistency.py.

    Gibt die NEUEN Verwaisten als Befundliste zurueck. Ein Rueckgang
    gegenueber der Grundlinie ist kein Befund — Fortschritt darf nicht rot
    leuchten; der eigenstaendige Lauf nennt ihn trotzdem.
    """
    base = load_baseline()
    if base is None:
        return []                      # noch keine Grundlinie: nichts zu pruefen
    try:
        now = measure()
    except Exception as exc:           # noqa: BLE001
        return ["Verwaisten-Audit nicht ausfuehrbar: %s" % exc]
    return ["neu verwaist (ruft niemand auf): " + p
            for p in sorted(now - base)]


if __name__ == "__main__":
    sys.exit(main())
