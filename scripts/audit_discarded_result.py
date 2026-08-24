#!/usr/bin/env python3
"""Ein Schreibaufruf, dessen Antwort niemand liest (MF-555)

── Warum es dieses Tor gibt ─────────────────────────────────────────────

Dreimal in der Pruef-Sitzung MF-534…554 war eine Stelle repariert und ihr
Geschwister nicht:

    MF-526  `lut[].length` als Gesamtlaenge statt je Seite — in EINER von
            zwei Kopien derselben Rechnung behoben, die andere stuerzte
            weiter ab.
    MF-550  die Flux-Kappung meldete sich an EINER Stelle (MF-528), an
            drei weiteren nicht.
    MF-554  der CPC-DSK-Index war in `uft_dsk_cpc.c` gedeckelt (MF-512),
            in `uft_format_extensions.c` nicht — 65024 in ein Feld mit
            204 Eintraegen.

Das ist keine Nachlaessigkeit im Einzelfall, sondern eine Eigenschaft des
Baums: **derselbe Gedanke steht mehrfach da, und eine Reparatur trifft
eine Kopie.** Wer eine Stelle repariert, muss nach ihren Geschwistern
suchen — dieses Tor tut das fuer eine Klasse, in der es besonders weh tut.

── Was gesucht wird ─────────────────────────────────────────────────────

Aufrufe von Funktionen, die einen Fehlercode liefern, deren Rueckgabewert
aber verworfen wird. Gemessen beim Anlegen:

    scp_writer_add_track   4 von 5 Aufrufen verwarfen die Antwort
    g64_set_track          3 von 4 Aufrufen verwarfen die Antwort

Beide schreiben eine Spur in ein Abbild. Schlaegt das fehl, fehlt die Spur
— und der Wandler zaehlt sie trotzdem als gewandelt. Das ist die
"Erfolgsmeldung ohne Tat" aus MF-545, eine Ebene tiefer: dort folgte der
Erfolg daraus, dass sich die DATEI anlegen liess; hier daraus, dass der
Aufruf zurueckkam.

── Grenzen ──────────────────────────────────────────────────────────────

Rein textuell: ein Aufruf gilt als geprueft, wenn sein Ergebnis zugewiesen
oder in einer Bedingung benutzt wird. Ein Aufruf, dessen Ergebnis
zugewiesen und dann nie gelesen wird, faellt hier nicht auf.

Die Liste `MUST_CHECK` ist bewusst kurz. Sie wachse mit den Funktionen,
bei denen ein verworfener Fehler wirklich Daten kostet — nicht mit allen,
die zufaellig `int` liefern.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

# Funktionen, deren Antwort Daten betrifft. Kurz halten.
MUST_CHECK = {
    "scp_writer_add_track":
        "schreibt eine Spur in das SCP-Abbild. Schlaegt es fehl, fehlt die "
        "Spur, und der Wandler zaehlt sie trotzdem.",
    "g64_set_track":
        "schreibt eine Spur in das G64-Abbild. Weist unter anderem "
        "Halbspuren und zu lange Spuren ab (siehe MF-534).",
}

BASELINE: set[str] = set()


def _strip_comments(text: str) -> str:
    def keep(m):
        return "\n" * m.group(0).count("\n")
    text = re.sub(r"/\*.*?\*/", keep, text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def check(repo: Path) -> list[str]:
    errors: list[str] = []
    for sub in ("src",):
        d = repo / sub
        if not d.exists():
            continue
        for p in sorted(d.rglob("*.c")):
            try:
                raw = p.read_text(encoding="utf-8", errors="replace")
            except OSError:
                continue
            text = _strip_comments(raw)
            lines = text.splitlines()
            rel = p.relative_to(repo).as_posix()

            for i, ln in enumerate(lines):
                for fn, why in MUST_CHECK.items():
                    if not re.search(r"\b" + fn + r"\s*\(", ln):
                        continue
                    # Definition selbst ueberspringen
                    stripped = ln.lstrip()
                    if re.match(r"^(?:static\s+)?\w[\w \t*]*\b" + fn + r"\s*\(",
                                stripped) and not stripped.startswith(
                            ("return", "if", "while", "}")):
                        # Zeile beginnt mit Typ + Name -> Definition/Prototyp
                        continue
                    # Wird das Ergebnis benutzt?
                    before = ln[:ln.index(fn)]
                    used = ("=" in before or
                            re.search(r"\b(?:if|while|return|assert)\b", before))
                    if used:
                        continue
                    key = f"{rel}:{i + 1}:{fn}"
                    if key in BASELINE:
                        continue
                    errors.append(
                        f"{rel}:{i + 1}: `{fn}()` — die Antwort wird "
                        f"verworfen. {why} Wer den Fehler nicht liest, "
                        f"meldet Erfolg fuer eine Spur, die nicht "
                        f"geschrieben wurde (MF-555).")

    return errors


def main() -> int:
    repo = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    errs = check(repo)
    print(f"Verworfene Schreib-Antworten (root={repo}):")
    print(f"  ueberwachte Funktionen : {len(MUST_CHECK)}")
    print(f"  begruendete Ausnahmen  : {len(BASELINE)}")
    print(f"  Befunde                : {len(errs)}")
    for e in errs:
        print(f"    {e}")
    return 1 if errs else 0


if __name__ == "__main__":
    raise SystemExit(main())
