#!/usr/bin/env python3
"""Wächter gegen geteilte Wächter (MF-511).

── Warum es diesen Wächter gibt ─────────────────────────────────────────

Zwei Funde desselben Tages, dieselbe Bauart:

  uft_scp_disk_type_name  war zweimal definiert — `static inline` in
  uft/uft_scp_format.h und gelinkt aus src/flux/uft_scp_parser.c, beide
  unter `UFT_SCP_DISK_TYPE_NAME_DECLARED`. Sie gaben fuer dieselben Codes
  verschiedene Namen zurueck (MF-510).

  uft_verify_result_t     ist zweimal definiert — in uft/uft_write_verify.h
  und uft/uft_disk_verify.h, beide unter `UFT_VERIFY_RESULT_T_DEFINED`,
  mit UNTERSCHIEDLICHEM Layout: `int` gegen `size_t`, und der
  Detailzeiger liegt an verschiedenen Offsets.

Das Muster ist in beiden Faellen dasselbe und es ist heimtueckisch:

    #ifndef GLEICHER_NAME
    #define GLEICHER_NAME
    ... Inhalt A ...
    #endif

in Header 1, und derselbe Waechter mit Inhalt B in Header 2. Der
Praeprozessor meldet **nichts**. Er nimmt, was zuerst kam. Welchen Inhalt
eine Uebersetzungseinheit sieht, entscheidet damit die
Include-Reihenfolge — also etwas, das niemand bewusst festlegt.

Ohne den Waechter waere es eine laute Neudefinition. MIT ihm ist es ein
stilles Auswuerfeln. Der Waechter, der Fehler verhindern soll, verdeckt
sie.

Bei einem Struct ist die Folge nicht kosmetisch: wer Layout A sieht und
eine Funktion ruft, die gegen Layout B uebersetzt wurde, liest und
schreibt an falschen Offsets. Bei uft_verify_result_t traefe es
`uft_verify_result_free`, das `free(r->track_results)` aufruft — in
Layout A liegt an dieser Stelle kein Zeiger.

── Was gemeldet wird ────────────────────────────────────────────────────

Nur Waechter, die in mehr als einer Datei stehen UND deren geschuetzter
Inhalt sich unterscheidet. Ein geteilter Waechter mit identischem Inhalt
ist Redundanz, kein Fehler — er wird gezaehlt, aber nicht gemeldet.

Verglichen wird normalisiert (Leerraum und Kommentare entfernt), damit
reine Formatierung nicht als Abweichung durchgeht.
"""
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]

# #ifndef X / #define X  ... #endif  — der uebliche Ein-Symbol-Waechter.
GUARD = re.compile(
    r"^[ \t]*#[ \t]*ifndef[ \t]+(\w+)[ \t]*\r?\n"
    r"[ \t]*#[ \t]*define[ \t]+\1[ \t]*\r?\n",
    re.M)


def strip(t: str) -> str:
    t = re.sub(r"/\*.*?\*/", " ", t, flags=re.S)
    t = re.sub(r"//[^\n]*", " ", t)
    return re.sub(r"\s+", " ", t).strip()


def guarded_blocks(path: pathlib.Path) -> dict[str, tuple[str, bool]]:
    """Waechtername -> (normalisierter Inhalt, ist_Dateiwaechter).

    Die Unterscheidung ist wichtig, weil die beiden Faelle verschieden
    schlimm sind:

      DATEI   Der Waechter umschliesst die ganze Datei. Kollidieren zwei
              Dateien darin, wird die zweite, die eingebunden wird,
              **komplett uebersprungen** — der Aufrufer bekommt gar
              nichts, und der Uebersetzer meldet erst spaeter fehlende
              Deklarationen an ganz anderer Stelle.

      TYP     Der Waechter umschliesst einen Typ oder eine Deklaration
              mitten in der Datei. Kollidieren zwei davon, sieht die
              Uebersetzungseinheit **einen** Inhalt, entschieden von der
              Include-Reihenfolge. Bei einem Struct heisst das:
              verschiedene Feld-Offsets, ohne jede Warnung.
    """
    text = path.read_text(encoding="utf-8", errors="replace")
    whole = len(strip(text))
    out: dict[str, tuple[str, bool]] = {}
    for m in GUARD.finditer(text):
        name = m.group(1)
        depth = 1
        i = m.end()
        lines = text[i:].splitlines(keepends=True)
        body: list[str] = []
        for ln in lines:
            s = ln.lstrip()
            if s.startswith("#if"):
                depth += 1
            elif s.startswith("#endif"):
                depth -= 1
                if depth == 0:
                    break
            body.append(ln)
        else:
            continue                     # unbalanciert: nicht auswertbar
        norm = strip("".join(body))
        # Dateiwaechter: der Block deckt praktisch die ganze Datei ab.
        out[name] = (norm, whole > 0 and len(norm) >= 0.8 * whole)
    return out


BASELINE = ROOT / "docs" / "shared_guard_baseline.txt"


def scan() -> tuple[int, list[tuple[str, str, list[tuple[str, int]]]]]:
    headers = sorted(list((ROOT / "include").rglob("*.h"))
                     + list((ROOT / "src").rglob("*.h")))
    seen: dict[str, list[tuple[str, str, bool]]] = {}
    for h in headers:
        rel = str(h.relative_to(ROOT)).replace("\\", "/")
        for name, (body, is_file) in guarded_blocks(h).items():
            if body and len(body) > 20:
                seen.setdefault(name, []).append((rel, body, is_file))

    shared = {k: v for k, v in seen.items() if len(v) > 1}
    findings = []
    for name, places in sorted(shared.items()):
        if len({b for _, b, _ in places}) == 1:
            continue                     # gleicher Inhalt: Redundanz, kein Fehler
        kind = "DATEI" if any(f for _, _, f in places) else "TYP"
        findings.append((name, kind, [(f, len(b)) for f, b, _ in places]))
    return len(shared), findings


def load_baseline() -> set[str] | None:
    if not BASELINE.exists():
        return None
    return {ln.strip() for ln in BASELINE.read_text(encoding="utf-8").splitlines()
            if ln.strip() and not ln.startswith("#")}


def check(repo) -> list:
    """Schnittstelle fuer check_consistency.py.

    Gemeldet werden nur Kollisionen, die NICHT in der Grundlinie stehen.
    Die 38 bestehenden einzeln aufzuloesen ist Arbeit pro Fall (jede
    Zusammenlegung kann stillschweigend aendern, welche Deklaration ein
    Aufrufer sieht); was heute geht, ist verhindern, dass es 39 werden.
    """
    base = load_baseline()
    if base is None:
        return []
    try:
        _, findings = scan()
    except Exception as exc:             # noqa: BLE001
        return ["Waechter-Kollisionspruefung nicht ausfuehrbar: %s" % exc]
    return ["neue Waechter-Kollision (%s), Include-Reihenfolge entscheidet: "
            "%s in %s" % (kind, name, ", ".join(f for f, _ in places))
            for name, kind, places in findings if name not in base]


def main() -> int:
    n_shared, findings = scan()
    base = load_baseline()

    n_file = sum(1 for _, k, _ in findings if k == "DATEI")
    print("Geteilte Waechter (in >1 Header)        : %d" % n_shared)
    print("  davon mit ABWEICHENDEM Inhalt         : %d" % len(findings))
    print("    Klasse DATEI (zweite Datei leer)    : %d" % n_file)
    print("    Klasse TYP   (Layout per Reihenfolge): %d"
          % (len(findings) - n_file))

    if base is None:
        BASELINE.parent.mkdir(parents=True, exist_ok=True)
        BASELINE.write_text(
            "# Waechter-Kollisionen - eingefrorene Grundlinie (MF-511).\n"
            "#\n"
            "# Jede Zeile ist ein Waechtername, den mehr als ein Header\n"
            "# benutzt, mit ABWEICHENDEM Inhalt. Welchen Inhalt eine\n"
            "# Uebersetzungseinheit sieht, entscheidet die\n"
            "# Include-Reihenfolge - still, ohne Warnung.\n"
            "#\n"
            "# Klasse DATEI: der Waechter umschliesst die ganze Datei. Wird\n"
            "#   die zweite eingebunden, liefert sie GAR NICHTS.\n"
            "# Klasse TYP:   der Waechter umschliesst einen Typ. Zwei\n"
            "#   Layouts, ein Name - falsche Feld-Offsets ohne Warnung.\n"
            "#\n"
            "# Wird eine aufgeloest, gehoert ihre Zeile hier heraus.\n"
            + "".join("%s\n" % n for n, _, _ in findings),
            encoding="utf-8")
        print("\nGrundlinie angelegt: %d Kollisionen" % len(findings))
        return 0

    names = {n for n, _, _ in findings}
    added = sorted(names - base)
    gone = sorted(base - names)

    if gone:
        print("\n  %d weniger als in der Grundlinie — bitte dort streichen:"
              % len(gone))
        for n in gone:
            print("    - " + n)

    if added:
        print("\nFEHLER: %d NEUE Kollision(en)." % len(added))
        for name, kind, places in findings:
            if name in added:
                print("  %-36s [%s]" % (name, kind))
                for f, n in places:
                    print("      %-52s %5d Zeichen" % (f, n))
        print("\n  Ein geteilter Waechter mit abweichendem Inhalt ist keine")
        print("  Redundanz. Er entscheidet per Include-Reihenfolge, welchen")
        print("  Inhalt eine Uebersetzungseinheit sieht — still.")
        return 1

    if "--detail" in sys.argv:
        print()
        for name, kind, places in findings:
            print("  %-36s [%s]" % (name, kind))
            for f, n in places:
                print("      %-52s %5d Zeichen" % (f, n))
    return 0


if __name__ == "__main__":
    sys.exit(main())
