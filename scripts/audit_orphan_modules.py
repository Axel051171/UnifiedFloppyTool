#!/usr/bin/env python3
"""Welche gebauten Module hat niemand aufgerufen? (MF-476)

ARCH-19 hat den Befund fuer `src/formats/misc/` einmalig ausgezaehlt: 19 von
21 Dateien ohne Aufrufer. Beim Verdrahten von MF-475 tauchte dasselbe Bild in
anderen Schichten auf. Eine Zahl, die einmal von Hand ermittelt wurde, ist ab
dem naechsten Commit falsch — deshalb hier ein Skript statt einer Zahl in
einer Doku.

Gezaehlt wird nur, was auch gebaut wird: die Quelldateien aus SOURCES in
`UnifiedFloppyTool.pro`. Eine Datei ohne Aufrufer, die nicht gebaut wird, ist
toter Code (dafuer gibt es die Loeschbeweis-Pipeline aus MF-369); eine Datei
ohne Aufrufer, die MITGEBAUT wird, ist etwas anderes — sie kostet Bauzeit,
sieht im Baum nach Funktion aus und hat keine.

Unterschieden wird ausserdem, WER aufruft:

  * Aufrufer in `src/`      — das Modul ist im Betrieb.
  * nur Aufrufer in `tests/`— das Modul ist geprueft, aber unverdrahtet. Das
                              ist der Zustand, in dem MF-471/473/474 gelandet
                              sind: getestet und ohne Wirkung.
  * kein Aufrufer           — niemand nutzt es, auch kein Test.

Genauigkeitsgrenzen, ausdruecklich:

  * Der Praeprozessor wird nicht ausgewertet. Ein Aufruf, der hinter einem
    `#if` liegt, das nirgends gesetzt ist, zaehlt hier trotzdem als Aufruf.
  * Makro-erzeugte Aufrufe (`DSK_PLUGIN(...)`) sind unsichtbar.
  * Kommentare werden entfernt, bevor gesucht wird — die Falle aus MF-468,
    wo ein Kommentar wie Code gelesen wurde.

Das heisst: das Skript kann ein Modul faelschlich als BENUTZT melden, aber
kaum faelschlich als verwaist. Der Fehler zeigt in die vorsichtige Richtung.

Aufruf:
    python scripts/audit_orphan_modules.py               # Uebersicht
    python scripts/audit_orphan_modules.py --detail      # jede Datei
    python scripts/audit_orphan_modules.py --json        # maschinenlesbar
"""
from __future__ import annotations

import argparse
import json
import pathlib
import re
import sys
from collections import defaultdict

ROOT = pathlib.Path(__file__).resolve().parent.parent
PRO = ROOT / "UnifiedFloppyTool.pro"

# Referenz-Orakel: im Baum, nicht im Build, absichtlich nicht angefasst.
SKIP_PARTS = {"samdisk", "a8rawconv"}

BLOCK_COMMENT = re.compile(r"/\*.*?\*/", re.S)
LINE_COMMENT = re.compile(r"//[^\n]*")
STRING_LIT = re.compile(r'"(?:[^"\\]|\\.)*"')

# Eine Definition am Zeilenanfang: Rueckgabetyp, Name, offene Klammer.
DEFINITION = re.compile(
    r"^(?!\s)(?:[A-Za-z_][\w \t*]*?[ \t*])([a-z_][a-z0-9_]{3,})[ \t]*\(", re.M
)
STATIC_DEF = re.compile(
    r"^static\s+(?:[A-Za-z_][\w \t*]*?[ \t*])([a-z_][a-z0-9_]{3,})[ \t]*\(", re.M
)

# Nicht-statische OBJEKTE, nicht nur Funktionen.
#
# Ein Format-Plugin exportiert genau ein Symbol — die Plugin-Struktur — und
# haelt jede Funktion dahinter `static`. Wer nur Funktionsdefinitionen sammelt,
# meldet `src/formats/cqm/uft_cqm.c` als verwaist, obwohl die Registry es in
# `uft_format_registry.c:260` fuehrt und es taeglich laeuft. Genau dieser
# Fehler — ein Messinstrument, das selbst falsch misst — hat in dieser
# Codebasis schon mehrfach zu Fehlschluessen gefuehrt (MF-461, MF-468).
OBJECT_DEF = re.compile(
    r"^(?!static\b)(?:const\s+)?[A-Za-z_]\w*(?:\s+\w+)*[\s*]+([a-z_][a-z0-9_]{3,})"
    r"\s*(?:\[[^\]]*\])?\s*=",
    re.M,
)

C_KEYWORDS = {"if", "for", "while", "switch", "return", "sizeof", "else", "case"}


def strip_noise(text: str) -> str:
    """Kommentare und Zeichenketten raus — beide koennen Namen enthalten."""
    return STRING_LIT.sub('""', LINE_COMMENT.sub("", BLOCK_COMMENT.sub("", text)))


def built_sources() -> list[pathlib.Path]:
    """Die Quelldateien aus SOURCES in der .pro-Datei.

    Bewusst KEIN eigener Parser. `verify_build_sources.py` liest dieselbe
    Liste seit MF-458 und kennt zwei Fallen, die ein neu geschriebener
    Parser beide neu haette treten muessen:

      * Opt-in-Bloecke (`kalman_pll { ... }`) gehoeren nicht zum
        Standard-Build. Ein naiver Parser meldet `uft_kalman_pll.c` als
        gebaut-und-verwaist, obwohl die Datei ohne `CONFIG+=kalman_pll`
        gar nicht uebersetzt wird.
      * Kommentare muessen VOR den Zeilenfortsetzungen weg, weil eine
        auskommentierte Zeile in dieser .pro selbst auf einen
        Fortsetzungs-Backslash endet.

    Ein Fakt, ein Parser.
    """
    sys.path.insert(0, str(ROOT / "scripts"))
    from verify_build_sources import parse_pro_sources  # noqa: E402

    out = []
    for rel in parse_pro_sources(PRO):
        p = ROOT / rel
        if p.is_file() and not (SKIP_PARTS & set(p.parts)):
            out.append(p)
    return sorted(set(out))


def strip_anon_namespace(body: str) -> str:
    """Anonyme Namensraeume entfernen — ihr Inhalt ist NICHT exportiert.

    MF-663: dieses Skript ist fuer C geschrieben und kannte
    `namespace { ... }` nicht. Es zaehlte jede Funktion darin als
    exportiert, obwohl sie interne Bindung hat — genau wie `static`.

    Die Folge, gemessen: **fuenf von sechs** Dateien mit anonymem
    Namensraum standen als "ohne jeden Aufrufer" da, weil ihre einzigen
    "Exporte" dateilokale Helfer waren. Bei
    `src/diskanalyzerwindow.cpp` war es eine einzige Funktion namens
    `metadatum`, in MF-662 hinzugefuegt.

    Das ist dieselbe Fehlerklasse wie die Aufzaehlungsfalle, nur in der
    Grammatik: der Pruefer kannte eine Schreibweise nicht und hat sie
    deshalb falsch bewertet — nicht uebersehen, sondern FALSCH GEZAEHLT,
    was schlimmer ist.

    Entfernt wird blockweise mit Klammerzaehlung, weil ein anonymer
    Namensraum verschachtelte Bloecke enthaelt und ein Regex bis zur
    ersten `}` zu frueh aufhoeren wuerde.
    """
    out = []
    i = 0
    n = len(body)
    while i < n:
        m = re.compile(r"\bnamespace\s*\{").search(body, i)
        if not m:
            out.append(body[i:])
            break
        out.append(body[i:m.start()])
        # Klammern zaehlen ab der oeffnenden `{`
        tiefe = 0
        j = m.end() - 1
        while j < n:
            if body[j] == "{":
                tiefe += 1
            elif body[j] == "}":
                tiefe -= 1
                if tiefe == 0:
                    break
            j += 1
        i = j + 1 if j < n else n
    return "".join(out)


def exported_names(body: str) -> set[str]:
    """Nicht-statische Funktionen UND Objekte, die diese Datei definiert.

    Ohne den Inhalt anonymer Namensraeume — der hat interne Bindung
    (MF-663).
    """
    body = strip_anon_namespace(body)
    names = set(DEFINITION.findall(body)) - set(STATIC_DEF.findall(body))
    names |= set(OBJECT_DEF.findall(body))
    return {n for n in names if n not in C_KEYWORDS}


def scan_tree(dirs: list[str]) -> dict[pathlib.Path, str]:
    out: dict[pathlib.Path, str] = {}
    for d in dirs:
        base = ROOT / d
        if not base.is_dir():
            continue
        for ext in ("*.c", "*.cpp", "*.h", "*.hpp"):
            for p in base.rglob(ext):
                if SKIP_PARTS & set(p.parts):
                    continue
                out[p] = strip_noise(p.read_text(encoding="utf-8", errors="ignore"))
    return out


# Aufruf `name(` und Funktionszeiger `= name` / `, name` / `{ name`.
#
# Der Aufruf muss EINGERUECKT stehen. Ohne diese Bedingung zaehlt der eigene
# Header als Aufrufer: ein Prototyp `multiread_execute(...);` sieht wie ein
# Aufruf aus. Damit galt praktisch jedes Modul mit Header als benutzt — das
# Skript meldete `uft_multiread_pipeline.c` als verdrahtet, obwohl der einzige
# Treffer sein eigener Prototyp war. Aufrufe stehen in einem Funktionsrumpf
# und damit eingerueckt; Prototypen und Definitionen beginnen in Spalte 0.
#
# Preis: eine Definition am Zeilenanfang, die eine andere Funktion im selben
# Ausdruck aufruft, wird uebersehen. Das ist selten und faellt in die
# vorsichtige Richtung nicht — deshalb faengt PTR_REF (ohne
# Einrueckungs-Bedingung) den Rest ab.
INDENTED_LINE = re.compile(r"^[ \t]+.*$", re.M)
CALL_REF = re.compile(r"\b([a-z_][a-z0-9_]{3,})\s*\(")
# Trennzeichen werden NICHT verbraucht, sondern nur vorausgeschaut. Eine
# Registry-Zeile `&plugin_a, &plugin_b,` haette sonst nur den ersten Eintrag
# geliefert: das Match auf `, &plugin_a,` frisst das Komma, das `&plugin_b`
# als Anfang braucht. Die Folge war, dass registrierte Format-Plugins als
# verwaist gemeldet wurden.
PTR_REF = re.compile(r"(?<=[=,{])\s*&?([a-z_][a-z0-9_]{3,})\b(?=\s*[,;}\)])")


def reference_index(tree: dict[pathlib.Path, str]) -> dict[str, set[pathlib.Path]]:
    """name -> Dateien, die ihn nennen.

    Ein Durchlauf durch den Baum statt einer Kreuzsuche Modul x Baum: bei
    ~700 gebauten Dateien gegen ~1200 Baumdateien war die Kreuzsuche
    quadratisch und lief minutenlang.
    """
    idx: dict[str, set[pathlib.Path]] = defaultdict(set)
    for path, text in tree.items():
        for name in CALL_REF.findall("\n".join(INDENTED_LINE.findall(text))):
            idx[name].add(path)
        for rx in (PTR_REF,):
            for name in rx.findall(text):
                idx[name].add(path)
    return idx


def used_elsewhere(names: set[str], idx: dict[str, set[pathlib.Path]],
                   own: pathlib.Path) -> bool:
    for n in names:
        hits = idx.get(n)
        if hits and (hits - {own}):
            return True
    return False


def closed_subsystems(sources, tree, idx, test_idx) -> list[dict]:
    """Verzeichnisse, in die von aussen nie hineingerufen wird (ORPH-2).

    Warum das eine ZWEITE Messung braucht: `used_elsewhere()` schliesst
    genau eine Datei aus, die gepruefte. Geschwister zaehlen als Aufrufer.
    Ein Ring von Dateien, die einander rufen und die sonst niemand ruft,
    ist damit unsichtbar — und zwar umso zuverlaessiger, je vollstaendiger
    der Ring ist. Belegt an `src/flux/fdc_bitstream/` (MF-626): zwoelf
    Dateien, 6483 Zeilen, kein Einbinder von aussen, trotzdem nie in der
    Grundlinie.

    Diese Messung fragt deshalb pro VERZEICHNIS: wird einer seiner
    exportierten Namen ausserhalb des Verzeichnisses genannt — in `src/`,
    in `include/` ODER in `tests/`?

    Die Tests gehoeren ausdruecklich dazu. Ein erster Entwurf liess sie
    weg und meldete daraufhin `src/formats/nintendo` als geschlossen,
    obwohl `test_rom_headers` es aufruft (MF-598). Ein Verzeichnis, das
    wenigstens ein Test benutzt, ist geprueft und nicht abgeschnitten —
    dieselbe Unterscheidung, die der Waechter oben schon fuer einzelne
    Dateien trifft.

    Ausdruecklich KEIN Tor, sondern ein Bericht. Ein Treffer heisst nicht
    "loeschen":

      - `src/` selbst enthaelt die Einstiegspunkte (`main.cpp`); ein
        Programmstart hat definitionsgemaess keinen Aufrufer. Deshalb
        sind Dateien direkt unter `src/` ausgenommen.
      - Ein Format-Verzeichnis ohne Aussenreferenz heisst in aller Regel
        **nicht registriert**, nicht ueberfluessig — das ist ARCH-11/12,
        und die Antwort darauf ist verdrahten, nicht loeschen.

    Wer daraus eine Loeschentscheidung ableitet, braucht denselben Beleg
    wie sonst auch: keinen Plan-Anker, keinen Einbinder, und einen Blick
    in die Datei.
    """
    by_dir: dict[str, list] = defaultdict(list)
    for q in sources:
        if q.suffix not in (".c", ".cpp"):
            continue
        try:
            rel = q.relative_to(ROOT)
        except ValueError:
            continue
        parts = rel.parts
        if parts[0] != "src" or len(parts) < 3:
            continue          # Dateien direkt unter src/: Einstiegspunkte
        by_dir["/".join(parts[:-1])].append(q)

    out = []
    for d, files in sorted(by_dir.items()):
        inside = {q for q in tree
                  if str(q.relative_to(ROOT)).replace(chr(92), "/").startswith(d + "/")}
        names: set[str] = set()
        for f in files:
            if f in tree:
                names |= exported_names(tree[f])
        if not names:
            continue
        outside: set = set()
        for n in names:
            outside |= (idx.get(n) or set()) - inside
            outside |= (test_idx.get(n) or set()) - inside
        if outside:
            continue
        out.append({
            "dir": d,
            "files": len(files),
            "exported": len(names),
            "lines": sum(len(tree[f].splitlines()) for f in files if f in tree),
        })
    out.sort(key=lambda r: -r["lines"])
    return out


def print_closed(gefunden: list[dict]) -> None:
    print("Geschlossene Subsysteme - von aussen nie gerufen (ORPH-2)")
    print("")
    print("  Ein Treffer heisst NICHT 'loeschen'. Bei Format-Verzeichnissen")
    print("  heisst er meist 'nicht registriert' (ARCH-11/12).")
    print("")
    print("  %-44s %7s %6s %5s" % ("Verzeichnis", "Zeilen", "Dat.", "Fn"))
    print("  %-44s %7s %6s %5s" % ("-" * 44, "-" * 7, "-" * 6, "-" * 5))
    for r in gefunden:
        print("  %-44s %7d %6d %5d"
              % (r["dir"], r["lines"], r["files"], r["exported"]))
    print("")
    print("  %d Verzeichnisse, %d Zeilen"
          % (len(gefunden), sum(r["lines"] for r in gefunden)))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--detail", action="store_true", help="jede verwaiste Datei nennen")
    ap.add_argument("--json", action="store_true", help="maschinenlesbare Ausgabe")
    ap.add_argument("--dirs", action="store_true",
                    help="Bericht: Verzeichnisse, in die von aussen nie "
                         "hineingerufen wird (ORPH-2). Kein Tor.")
    args = ap.parse_args()

    sources = built_sources()
    if not sources:
        print("FEHLER: keine SOURCES aus der .pro-Datei gelesen", file=sys.stderr)
        return 2

    src_tree = scan_tree(["src", "include"])
    src_idx = reference_index(src_tree)
    test_idx = reference_index(scan_tree(["tests"]))

    rows = []
    for path in sources:
        body = src_tree.get(path)
        if body is None:
            body = strip_noise(path.read_text(encoding="utf-8", errors="ignore"))
        names = exported_names(body)
        if not names:
            continue  # z.B. reine Tabellen-Dateien

        in_src = used_elsewhere(names, src_idx, path)
        in_tests = False if in_src else used_elsewhere(names, test_idx, path)

        state = "src" if in_src else ("tests" if in_tests else "none")
        rows.append(
            {
                "file": str(path.relative_to(ROOT)).replace("\\", "/"),
                "exported": len(names),
                "used_by": state,
            }
        )

    if args.dirs:
        gefunden = closed_subsystems(sources, src_tree, src_idx, test_idx)
        if args.json:
            print(json.dumps(gefunden, indent=2))
        else:
            print_closed(gefunden)
        return 0

    if args.json:
        print(json.dumps(rows, indent=2))
        return 0

    by_dir: dict[str, dict[str, int]] = defaultdict(lambda: defaultdict(int))
    for r in rows:
        d = str(pathlib.PurePosixPath(r["file"]).parent)
        by_dir[d][r["used_by"]] += 1

    print("Gebaute Module und ihre Aufrufer (MF-476)")
    print(f"  {len(rows)} gebaute Quelldateien mit exportierten Funktionen\n")
    print(f"  {'Verzeichnis':<40} {'in src':>7} {'nur Test':>9} {'keiner':>7}")
    print(f"  {'-' * 40} {'-' * 7} {'-' * 9} {'-' * 7}")
    tot = defaultdict(int)
    for d in sorted(by_dir, key=lambda k: -(by_dir[k]["none"] + by_dir[k]["tests"])):
        c = by_dir[d]
        for k in ("src", "tests", "none"):
            tot[k] += c[k]
        if c["none"] or c["tests"]:
            print(f"  {d:<40} {c['src']:>7} {c['tests']:>9} {c['none']:>7}")
    print(f"  {'-' * 40} {'-' * 7} {'-' * 9} {'-' * 7}")
    print(f"  {'SUMME (alle Verzeichnisse)':<40} {tot['src']:>7} "
          f"{tot['tests']:>9} {tot['none']:>7}")

    if args.detail:
        print("\nGeprueft, aber unverdrahtet (nur Tests rufen auf):")
        for r in rows:
            if r["used_by"] == "tests":
                print(f"  {r['file']}  ({r['exported']} exportierte Fn)")
        print("\nOhne jeden Aufrufer:")
        for r in rows:
            if r["used_by"] == "none":
                print(f"  {r['file']}  ({r['exported']} exportierte Fn)")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
