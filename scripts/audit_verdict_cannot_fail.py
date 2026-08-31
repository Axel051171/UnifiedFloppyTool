#!/usr/bin/env python3
"""Ein Unversehrtheits-Urteil, das nicht scheitern kann (MF-570)

── Warum es dieses Tor gibt ─────────────────────────────────────────────

Der forensische Bericht bescheinigte drei Dinge, ohne ein Byte dafuer zu
lesen:

    if (checkValidateDirectory->isChecked())
        addResultRow("Directory", "✓ Valid", ...);

    if (formatName.contains("FAT") || formatName.contains("IMG") || ...)
        addResultRow("FAT Structure", "✓ Valid",
                     "File allocation table intact");

    if (checkValidateFilesystem->isChecked())
        addResultRow("Filesystem", "✓ Valid",
                     "No structural errors detected");

Ein Haekchen ankreuzen erzeugte ein gruenes "Valid" — auch auf einer
vollstaendig zerstoerten Diskette. Die FAT-Zeile urteilte allein danach,
ob der FORMATNAME "FAT" enthaelt. Und diese Zeilen gehen in den Export
(PDF/HTML): sie verlassen das Werkzeug als Dokument.

Fuer ein forensisches Werkzeug ist das die schaerfste Form des Fehlers.
Ein gruenes Haekchen ist genau das, was ein Pruefer sehen will, und
niemand zweifelt es an. Eine erfundene Dateiliste koennte jemand
hinterfragen; "Filesystem: ✓ Valid" nicht.

── Woran es zu erkennen ist ─────────────────────────────────────────────

Gemessen wurde der Unterschied zwischen den falschen und den ECHTEN
Zeilen derselben Datei:

    echt      hash1 == hash2 ? "✓ MATCH"  : "✗ MISMATCH"
    echt      diffs == 0     ? "✓ IDENTICAL" : "✗ DIFFERENT"
    echt      hasBootSig     ? "✓ Present" : "— Not found"
    falsch    "✓ Valid"           <- kein Gegenzweig
    falsch    "✓ None detected"   <- kein Gegenzweig

Jede echte Aussage steht in einem Ausdruck, der auch NEIN sagen kann, und
die Bedingung kommt aus den Daten. Jede erfundene kann nur ja sagen.

Das Tor sucht deshalb **Unversehrtheits-Wortschatz** in `src/*.cpp`:
"Valid", "intact", "No structural errors", "None detected", "no errors",
"clean". Fuer jede Fundstelle muss in `BASELINE` stehen, WELCHE Daten das
Urteil tragen.

── Die Grundlinie ist LEER, und das ist der Punkt ───────────────────────

Nach MF-570 gibt es keine einzige solche Behauptung mehr. Eine leere
Grundlinie ist die schaerfste Fassung eines Tors: die naechste feuert,
ohne dass jemand entscheiden muss, ob sie „so aehnlich" wie eine erlaubte
ist.

Wer eine echte Pruefung verdrahtet, darf ihr Urteil eintragen — mit der
Angabe, welche Bytes es tragen. Das ist kein Hindernis, das ist der Preis
fuer einen Satz, den ein Archiv spaeter zitiert.

── Was dieses Tor NICHT kann ────────────────────────────────────────────

Es liest Zeichenketten, nicht Logik. Ein Urteil mit anderem Wortlaut
("Struktur i.O.") faellt nicht auf, und ein Ternaer, dessen Bedingung
IMMER wahr ist, sieht fuer das Tor echt aus. Es ist eine Schranke gegen
die Wiederkehr dieser Form, kein Beweis ihrer Abwesenheit.

Kommentare werden entfernt, bevor gesucht wird — sonst feuert das Tor auf
den Kommentaren, die den entfernten Code zitieren.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

# Wortschatz, mit dem ueber den ZUSTAND eines Abbilds geurteilt wird.
#
# Absichtlich NICHT dabei: "Calculated", "Present", "PASS", "MATCH",
# "IDENTICAL". Die sagen etwas ueber eine ausgefuehrte Handlung oder
# stehen in einem Ternaer mit Gegenzweig — gemessen an forensictab.cpp
# sind genau das die ehrlichen Zeilen.
# Als REGEX, mit Wortgrenze. Der erste Lauf suchte Teilzeichenketten und
# meldete neun Treffer, von denen NEUN falsch waren: "Valid" steckt in
# "Validate", "Validation", "Validating" — also in Fenstertiteln,
# Schaltflaechen und Fortschrittsmeldungen. Ein Tor, das den eigenen
# Wortschatz nicht abgrenzt, meldet Rauschen und wird abgeschaltet.
#
# "No errors found" ist ebenfalls raus: der einzige Treffer war "No errors
# found THAT CAN BE AUTOMATICALLY REPAIRED" — eine Aussage ueber die
# Reparierbarkeit, nicht ueber die Diskette. Textuell nicht zu trennen,
# also nicht im Wortschatz.
# Ein Ternaer, dessen ZWEITER Zweig auch eine Zeichenkette ist — die
# ehrliche Form: das Urteil kann anders lauten.
#
# MF-735: hier stand `if "?" in line and "—" in line`. Der Geviertstrich
# war eine Eigenheit der drei Stellen im Baum, an denen die Ausnahme
# gebraucht wurde, kein Merkmal der ehrlichen Form. Ein Ternaer ohne
# Strich —
#
#     setText(ok ? tr("Valid") : tr("FAIL"))
#
# — wurde gemeldet, obwohl er genau das tut, was das Tor verlangt.
# Gefunden vom gepflanzten `sauber`-Fall in `audit_selbsttest.py`; am
# echten Baum unsichtbar, weil dort jede solche Stelle zufaellig einen
# Strich trug. Die Zahl bleibt unveraendert: 0 vorher, 0 nachher.
TERNAER_MIT_NEIN = re.compile(r'\?[^:]*:\s*(?:tr\s*\(\s*)?["✗]')

VERDICTS = [
    r"\bValid\b",
    r"\bintact\b",
    r"No structural errors",
    r"None detected",
]

BASELINE: dict[str, str] = {
    # LEER, und das ist Absicht — siehe Kopf.
    #
    # Ein Eintrag hier braucht die Antwort auf: WELCHE Bytes wurden
    # gelesen, damit dieser Satz stimmt? Nicht "das ist schon in
    # Ordnung".
}


def _strip_comments(text: str) -> str:
    """Kommentare raus, Zeilenzahl behalten.

    Die Zeilenzahl muss stimmen, sonst zeigen die Fundstellen auf
    unbeteiligten Code — dieser Fehler ist in diesem Baum schon einmal
    passiert (MF-559).
    """
    def repl(m: re.Match) -> str:
        return "\n" * m.group(0).count("\n")

    text = re.sub(r"/\*.*?\*/", repl, text, flags=re.S)
    text = re.sub(r"//[^\n]*", "", text)
    return text


def check(repo: Path) -> list[str]:
    errors: list[str] = []
    d = repo / "src"
    if not d.exists():
        return errors

    seen: set[str] = set()

    for p in sorted(d.glob("*.cpp")):
        rel = p.relative_to(repo).as_posix()
        try:
            raw = p.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        text = _strip_comments(raw)

        for lineno, line in enumerate(text.splitlines(), 1):
            # Nur Zeichenketten, die dem Benutzer gezeigt werden.
            if 'tr("' not in line and '"' not in line:
                continue
            # Eine Zeile, die das NEIN-Zeichen traegt, ist die ehrliche
            # Form — entweder ein Ternaer mit Gegenzweig oder gleich das
            # negative Urteil selbst ("✗ FAIL").
            if "✗" in line:
                continue
            for word in VERDICTS:
                if not re.search(word, line):
                    continue
                # Ein Ternaer mit Gegenzweig ist die ehrliche Form.
                if TERNAER_MIT_NEIN.search(line):
                    continue
                key = f"{rel}:{word}"
                seen.add(key)
                if key in BASELINE:
                    continue
                errors.append(
                    f"{rel}:{lineno}: `{word}` — ein Urteil ueber den "
                    f"Zustand des Abbilds. Welche Bytes tragen es? Kommt "
                    f"es aus einem Ausdruck, der auch NEIN sagen kann? "
                    f"Wenn ja: Eintrag in BASELINE mit der Antwort. Wenn "
                    f"nein: es ist eine Bescheinigung ohne Pruefung.")

    for key in BASELINE:
        if key not in seen:
            errors.append(
                f"{key}: begruendete Ausnahme ohne Fundstelle — erledigt, "
                f"bitte aus BASELINE entfernen.")

    return errors


def main() -> int:
    repo = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    errs = check(repo)
    print(f"Urteile, die nicht scheitern koennen (root={repo}):")
    print(f"  begruendete Ausnahmen : {len(BASELINE)}")
    print(f"  Befunde               : {len(errs)}")
    for e in errs[:40]:
        print(f"    {e}")
    if len(errs) > 40:
        print(f"    ... und {len(errs) - 40} weitere")
    return 1 if errs else 0


if __name__ == "__main__":
    sys.exit(main())
