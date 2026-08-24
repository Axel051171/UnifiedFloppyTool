#!/usr/bin/env python3
"""Eine Zahl, die mit sich selbst verglichen wird (MF-552)

── Warum es dieses Tor gibt ─────────────────────────────────────────────

Drei der schwersten Funde der Pruef-Sitzung MF-534…551 sind DASSELBE
Muster in verschiedener Kleidung:

    MF-542  src/recovery/uft_forensic_recovery.c
            crc_stored = crc_computed;
            crc_valid  = (crc_computed == crc_stored);
            -> crc_valid war per Konstruktion IMMER wahr. Jede noch so
               zerstoerte Diskette kam als "100 % CRC-gueltig" heraus.

    MF-551  src/fs/uft_fat12.c
            *size = written;                       // die GEKUERZTE Zahl
            return (w == sz) ? UFT_FAT_OK : ...;   // gegen sie verglichen
            -> eine abgebrochene Extraktion lieferte eine kuerzere Datei
               UND eine kuerzere Sollzahl. Der Vergleich ging immer auf.

    MF-545  src/formats/uft_format_convert_*.c
            success = (die Datei liess sich ANLEGEN)
            -> Erfolg folgte aus dem Schreiben, nicht aus dem Inhalt.

Alle drei sehen im Quelltext wie eine Pruefung aus. Keine davon prueft
etwas. Und alle drei sind an einer Stelle, an der ein forensisches
Werkzeug eine Zusicherung gibt — CRC gueltig, Datei vollstaendig,
Wandlung gelungen.

**Eine Zahl ohne unabhaengige Bezugsgroesse ist keine Messung.** Das ist
die Regel, die aus dieser Sitzung bleibt, und dieses Tor macht sie
maschinell nachpruefbar.

── Was gesucht wird ─────────────────────────────────────────────────────

Innerhalb EINER Funktion:

    (1)  a = b;              (irgendwo)
    (2)  ... a == b ...      (weiter unten, ohne dass a oder b dazwischen
                              neu zugewiesen wurden)

Das ist die Tautologie. Zusaetzlich der offene Fall `x == x`.

Gesucht wird nur in Verzeichnissen, in denen eine solche Pruefung eine
ZUSICHERUNG traegt: recovery, fs, formats, core, decoder, analysis.

── Was NICHT gefunden wird, und das ist wichtig ─────────────────────────

Rein textuell, ohne Datenflussanalyse. Nicht gefunden werden:

  * Tautologien ueber Funktionsgrenzen hinweg (der MF-545-Fall: `success`
    folgte aus dem Schreiberfolg, nicht aus einer Zuweisung daneben)
  * Zuweisungen ueber Zeiger oder Felder, deren Gleichheit erst zur
    Laufzeit entsteht
  * Faelle, in denen zwischen (1) und (2) eine FUNKTION eine der beiden
    Groessen aendert, ohne sie sichtbar zuzuweisen — dann meldet das Tor
    einen Fehlalarm, und der Eintrag gehoert mit Begruendung in die
    Grundlinie

Das Tor faengt den offenen Fall. Der offene Fall ist der, der zweimal
vorkam.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

SCAN_DIRS = ("src/recovery", "src/fs", "src/formats", "src/core",
             "src/decoder", "src/analysis")

# Begruendete Ausnahmen. Ein Eintrag hier ist kein Freibrief, sondern die
# Aussage "geprueft, und es ist keine Tautologie — hier steht warum".
BASELINE: dict[str, str] = {}

ASSIGN = re.compile(r"^\s*(?:[\w\s*]+?\s+)?([\w.\->\[\]]+)\s*=\s*([\w.\->\[\]]+)\s*;")
COMPARE = re.compile(r"([\w.\->\[\]]+)\s*==\s*([\w.\->\[\]]+)")
BRANCH = re.compile(r"(if|else|while|for|switch|case|do|goto|return|continue|break)")
FUNC_START = re.compile(r"^[A-Za-z_][\w \t*]*\**\s*\w+\s*\([^;]*$")


def _strip_comments(text: str) -> str:
    """Kommentare entfernen, ZEILENNUMMERN ERHALTEN.

    MF-552: die erste Fassung ersetzte Blockkommentare durch nichts. Damit
    verschoben sich alle Zeilennummern dahinter, und das Tor meldete fuenf
    Befunde mit Stellenangaben, an denen etwas voellig anderes stand.

    Ein Befund mit falscher Stelle ist schlimmer als kein Befund: er kostet
    die Zeit, ihn zu widerlegen, und macht beim naechsten Mal misstrauisch
    gegen das Tor statt gegen den Code. Aufgefallen ist es nur, weil jeder
    der fuenf Treffer einzeln nachgelesen wurde — was hier Pflicht ist,
    seit acht Messwerkzeuge dieser Sitzung beim ersten Anlauf falsch
    gemessen haben.

    Jeder Kommentar wird deshalb durch GENAU SO VIELE Zeilenumbrueche
    ersetzt, wie er enthielt.
    """
    def _keep_lines(m):
        return "\n" * m.group(0).count("\n")
    text = re.sub(r"/\*.*?\*/", _keep_lines, text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def _functions(text: str):
    """(startzeile, zeilenliste) je Funktion — grob, aber klammerbalanciert."""
    lines = text.splitlines()
    i = 0
    while i < len(lines):
        if FUNC_START.match(lines[i]) and not lines[i].lstrip().startswith(
                ("if", "for", "while", "switch", "return", "else")):
            # oeffnende Klammer suchen
            j = i
            while j < len(lines) and "{" not in lines[j]:
                if ";" in lines[j]:
                    j = -1
                    break
                j += 1
            if j < 0 or j >= len(lines):
                i += 1
                continue
            depth = 0
            k = j
            while k < len(lines):
                depth += lines[k].count("{") - lines[k].count("}")
                if depth == 0 and k > j:
                    break
                k += 1
            yield i, lines[i:k + 1]
            i = k + 1
            continue
        i += 1


def check(repo: Path) -> list[str]:
    errors: list[str] = []
    for sub in SCAN_DIRS:
        d = repo / sub
        if not d.exists():
            continue
        for p in sorted(d.rglob("*.c")):
            try:
                raw = p.read_text(encoding="utf-8", errors="replace")
            except OSError:
                continue
            text = _strip_comments(raw)
            rel = p.relative_to(repo).as_posix()

            for start, body in _functions(text):
                assigned: dict[str, tuple[str, int]] = {}
                for off, line in enumerate(body):
                    lineno = start + off + 1

                    # ZUERST zuruecksetzen, DANN pruefen.
                    #
                    # Der zweite Lauf setzte erst am Zeilenende zurueck —
                    # und meldete deshalb weiterhin Zeilen, die selbst den
                    # Kontrollfluss wechseln:
                    #
                    #   while (src_pos < src_len && src[src_pos] == byte) {
                    #   } else if (conf == r.confidence && r.winner) {
                    #
                    # Beide sind keine Tautologien: die eine ist nur im
                    # ERSTEN Schleifendurchlauf trivial wahr, die andere
                    # steht in einem Zweig, den die Zuweisung ausschliesst.
                    # Eine Reihenfolge zu spaet, und das Tor liegt bei drei
                    # von fuenf Meldungen daneben.
                    if (BRANCH.search(line) or "{" in line or "}" in line
                            or "++" in line or "--" in line):
                        assigned.clear()

                    # offener Fall: x == x
                    for a, b in COMPARE.findall(line):
                        if a == b and not a.isdigit():
                            key = f"{rel}:{lineno}"
                            if key in BASELINE:
                                continue
                            errors.append(
                                f"{rel}:{lineno}: `{a} == {a}` — ein Wert "
                                f"mit sich selbst verglichen.")

                    # Tautologie ueber eine vorangegangene Zuweisung
                    for a, b in COMPARE.findall(line):
                        if a == b:
                            continue
                        for lhs, rhs in ((a, b), (b, a)):
                            prev = assigned.get(lhs)
                            if prev and prev[0] == rhs:
                                key = f"{rel}:{lineno}"
                                if key in BASELINE:
                                    continue
                                errors.append(
                                    f"{rel}:{lineno}: `{a} == {b}` ist immer "
                                    f"wahr — Zeile {prev[1]} setzt "
                                    f"`{lhs} = {rhs};`. Eine Pruefung, die "
                                    f"nicht fehlschlagen kann, prueft nichts "
                                    f"(MF-552; Vorbilder MF-542, MF-551).")

                    m = ASSIGN.match(line)
                    if m:
                        lhs, rhs = m.group(1), m.group(2)
                        # jede Neuzuweisung loescht alte Paare, die sie betrifft
                        for k in [k for k, v in assigned.items()
                                  if k == lhs or v[0] == lhs]:
                            assigned.pop(k, None)
                        if not rhs.isdigit():
                            assigned[lhs] = (rhs, lineno)
                    elif "=" in line and "==" not in line:
                        # unklare Zuweisung -> Gedaechtnis vorsichtshalber leeren
                        assigned.clear()


    return errors


def main() -> int:
    repo = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    errs = check(repo)
    print(f"Selbstvergleiche (root={repo}):")
    print(f"  begruendete Ausnahmen : {len(BASELINE)}")
    print(f"  Befunde               : {len(errs)}")
    for e in errs:
        print(f"    {e}")
    return 1 if errs else 0


if __name__ == "__main__":
    raise SystemExit(main())
