#!/usr/bin/env python3
"""Ein Name, gleiche Aritaet, Zeiger auf VERSCHIEDENE Typen (MF-1155).

Anlass ist ein Einzelfund, und der Weg dorthin ist der Punkt: `uft_pipeline_run`
ist zweimal deklariert —

    include/uft/uft_integration.h:354   uft_error_t uft_pipeline_run(uft_pipeline_t *);
    src/core/uft_decode_pipeline.h:27   uft_error_t uft_pipeline_run(uft_decode_session_t *);

Es gibt genau EINE Definition (`src/core/uft_decode_pipeline.c:207`, die
Sitzungs-Fassung). Wer den anderen Header einbindet, uebergibt einen
`uft_pipeline_t*` an eine Funktion, die ihn als `uft_decode_session_t*`
dereferenziert.

## Warum das vorhandene Tor es nicht sehen kann, und warum das kein Fehler ist

`scripts/extern_decl_conflicts.py` prueft genau diese Lage — Regel B, seit
MF-464, gefunden an `uft_cpm_format()` in zwei Headern. Aber es vergleicht
**die Aritaet**, und sein Kopf sagt ausdruecklich warum:

    "Comparison is on ARITY, not on spelling. Types are written differently
     across files for the same thing (`size_t` vs `unsigned long`, a typedef
     vs the underlying struct), and flagging those would drown the real
     finding."

Das ist richtig entschieden. `uft_pipeline_run` hat auf beiden Seiten
Aritaet 1 — die Wahl kann diesen Fall nicht sehen. Was fehlte, war nicht eine
andere Wahl, sondern ihr **Preis**: wie gross ist die Teilklasse, die dabei
durchfaellt?

Gemessen: **20 Namen**. Dieses Tor haelt sie.

## Die Regel ist so eng gefasst, dass das Ergebnis lesbar bleibt

Gemeldet wird nur, wenn ALLE vier Bedingungen zutreffen:

  1. derselbe Funktionsname in mehr als einer Deklaration,
  2. alle Deklarationen haben DIESELBE Aritaet (sonst sieht es Regel B),
  3. an mindestens einer Parameterstelle sind alle Fassungen Zeiger,
  4. und die Namen der Zeigerziele sind VERSCHIEDEN.

Bedingung 4 ist der Kern: `uft_disk_image_t*` gegen `uft_disk_t*` sind nicht
zwei Schreibweisen desselben Typs, das sind zwei Typen. `size_t` gegen
`unsigned long` faellt nicht darunter, weil weder das eine noch das andere ein
Zeiger ist.

## Was dieses Tor NICHT sehen kann, und das gehoert hierher (MF-1000)

  - **Klassenbereich.** Eine Qt-Methode `updateHexDump(const QByteArray&)` in
    ZWEI Klassen ist keine Kollision. Header mit Klassenrumpf (`Q_OBJECT` oder
    `class X`) werden deshalb ausgelassen. Der erste Messlauf ohne diesen
    Ausschluss meldete **93** statt 41 — die Halterung war weiter als ihr
    Gegenstand.
  - **C++-Ueberladungen in EINER Datei.** `src/samdisk/record.h` deklariert
    `ReadRecord` zweimal, absichtlich. Ein Ueberladungssatz innerhalb einer
    Datei wird ausgelassen.
  - **Aritaets-Unterschiede.** Die deckt `extern_decl_conflicts.py` ab; hier
    waeren sie eine zweite Meldung derselben Sache.
  - **Funktionszeiger-Parameter.** Deren Klammern zerlegt dieser Zerteiler
    nicht; solche Deklarationen werden uebersprungen statt falsch bewertet.
  - **Typedef-Ketten.** Zeigt `uft_mfm_ctx_t` per `typedef` auf dasselbe
    struct wie `uft_mfm_context_t`, waere es kein Defekt — dieses Tor sagt
    nur, dass die NAMEN verschieden sind, und ueberlaesst die Aufloesung dem
    Eintrag in `docs/OPEN_ITEMS.md`. Es ist ein Tor gegen NEUE Faelle, kein
    Urteil ueber die bestehenden.

## Bauform

Fallende Grundlinie wie Tor 57 (`audit_schreibzusage.py`, MF-883/930): die
Zahl darf nur sinken. Ein Name, der nicht in `docs/typkollision_baseline.txt`
steht, ist ein Fehler; ein Name, der darin steht und nicht mehr kollidiert,
ist auch einer — sonst friert die Grundlinie ein, was behoben ist.

Dateimenge aus `git ls-files` (MF-636), nie aus einer gepflegten Liste.
"""
from __future__ import annotations

import re
import subprocess
import sys
import tempfile
from collections import defaultdict
from pathlib import Path

GRUNDLINIE = "docs/typkollision_baseline.txt"

# `<Rueckgabe> <name>(<params>);` am Zeilenanfang, ohne Rumpf.
DEKL = re.compile(
    r"^[ \t]*(?:extern[ \t]+)?"
    r"((?:const[ \t]+|unsigned[ \t]+|signed[ \t]+|struct[ \t]+|enum[ \t]+)*"
    r"[A-Za-z_]\w*(?:[ \t]*\*)*)[ \t]+"
    r"([A-Za-z_]\w*)[ \t]*\(([^;{)]*)\)[ \t]*;",
    re.M,
)

SCHLUESSELWORT = {"if", "for", "while", "switch", "return", "sizeof", "defined"}
KLASSENRUMPF = re.compile(r"\bQ_OBJECT\b|^\s*class\s+\w+", re.M)


def ohne_kommentare(text: str) -> str:
    """Kommentare durch Leerzeichen ersetzen, Zeilenumbrueche erhalten.

    Die Zeilennummern muessen gelten: ein Beispielaufruf in einem Kommentar
    darf nicht mitzaehlen (`uft_decode_session.h:14` nennt `uft_pipeline_run`
    genau so), aber die Fundstelle darunter muss die richtige Zeile melden.
    """
    text = re.sub(r"/\*.*?\*/",
                  lambda m: re.sub(r"[^\n]", " ", m.group(0)), text, flags=re.S)
    return re.sub(r"//[^\n]*", " ", text)


def parameter_liste(params: str) -> list[str] | None:
    """Parameterstellen auf ihren Typ reduzieren. None = nicht bewertbar."""
    params = re.sub(r"\s+", " ", params).strip()
    if params in ("", "void"):
        return []
    if "(" in params or "..." in params:
        return None                       # Funktionszeiger / variadisch
    stellen = []
    for p in params.split(","):
        p = p.strip()
        p = re.sub(r"\[[^\]]*\]\s*$", "*", p)      # `x[]` ist ein Zeiger
        p = re.sub(r"\b[A-Za-z_]\w*\s*$", "", p).strip()   # Argumentname weg
        p = re.sub(r"\s*\*\s*", "*", p)
        stellen.append(re.sub(r"\s+", " ", p))
    return stellen


def zeigerziel(stelle: str) -> str | None:
    """Name des Zeigerziels, oder None wenn es kein Zeiger ist."""
    s = stelle.replace("const", "").replace("volatile", "").strip()
    if not s.endswith("*"):
        return None
    kern = s.rstrip("*").strip()
    kern = re.sub(r"^(struct|enum|union)\s+", "", kern)
    return kern or None


def header_dateien(wurzel: Path, still: bool = False) -> list[str]:
    try:
        aus = subprocess.run(
            ["git", "ls-files", "--cached", "--others", "--exclude-standard"],
            cwd=wurzel, capture_output=True, text=True, check=True,
        ).stdout
    except (OSError, subprocess.CalledProcessError):
        # MF-636: ist git nicht befragbar, alles durchlassen UND es sagen.
        # `still` setzt NUR der Selbsttest: seine Attrappen haben absichtlich
        # kein git, dort ist der Rueckfall der gewollte Weg und die Warnung
        # waere ein Fehlalarm, der die echte Warnung entwertet.
        if not still:
            print("WARNUNG: git nicht befragbar — Dateimenge unvollstaendig",
                  file=sys.stderr)
        return [str(p.relative_to(wurzel)).replace("\\", "/")
                for p in wurzel.rglob("*.h")]
    pfade = [p for p in aus.splitlines() if p.endswith((".h", ".hpp"))]
    # MF-633: geklonte Fremd-Repos sind nicht unser Baum.
    return [p for p in pfade if not p.startswith("tools/uft-scout/work/")]


def sammle(wurzel: Path, still: bool = False) -> dict[str, list[tuple[str, int, tuple]]]:
    """name -> [(pfad, zeile, tuple(parameterstellen))]"""
    gefunden: dict[str, list[tuple[str, int, tuple]]] = defaultdict(list)
    for rel in header_dateien(wurzel, still):
        pfad = wurzel / rel
        try:
            roh = pfad.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        if KLASSENRUMPF.search(roh):
            continue                       # Klassenbereich, siehe Kopf
        text = ohne_kommentare(roh)
        for m in DEKL.finditer(text):
            rueck, name, params = m.group(1).strip(), m.group(2), m.group(3)
            if name in SCHLUESSELWORT or rueck.split()[0] in SCHLUESSELWORT:
                continue
            stellen = parameter_liste(params)
            if stellen is None:
                continue
            zeile = text[: m.start()].count("\n") + 1
            gefunden[name].append((rel, zeile, tuple(stellen)))
    return gefunden


def kollisionen(wurzel: Path, still: bool = False) -> dict[str, list[str]]:
    """name -> Fundbeschreibungen. Nur die vier Bedingungen aus dem Kopf."""
    treffer: dict[str, list[str]] = {}
    for name, orte in sammle(wurzel, still).items():
        dateien = {d for d, _, _ in orte}
        if len(dateien) < 2:
            continue                       # Ueberladungssatz in EINER Datei
        saetze = {s for _, _, s in orte}
        if len(saetze) < 2:
            continue                       # identisch deklariert, in Ordnung
        if len({len(s) for s in saetze}) != 1:
            continue                       # Aritaet: das sieht Regel B
        liste = sorted(saetze)
        for i in range(len(liste[0])):
            ziele = {zeigerziel(s[i]) for s in liste}
            if None in ziele or len(ziele) < 2:
                continue
            zeilen = [f"{d}:{z}" for d, z, _ in sorted(set(orte))]
            treffer[name] = [
                f"{name}(): Parameterstelle {i} ist "
                f"{' vs '.join(sorted(x for x in ziele if x))} — "
                f"{', '.join(zeilen)}"
            ]
            break
    return treffer


def lies_grundlinie(pfad: Path) -> set[str]:
    if not pfad.exists():
        return set()
    namen = set()
    for z in pfad.read_text(encoding="utf-8").splitlines():
        z = z.split("#", 1)[0].strip()
        if z:
            namen.add(z)
    return namen


def check(repo) -> list[str]:
    """Einstieg fuer scripts/check_consistency.py."""
    wurzel = Path(repo)
    gefunden = kollisionen(wurzel)
    bekannt = lies_grundlinie(wurzel / GRUNDLINIE)
    fehler: list[str] = []
    for name in sorted(set(gefunden) - bekannt):
        fehler.extend(gefunden[name])
        fehler.append(
            f"  ^ NEU. Ein Name, zwei Zeigertypen, eine Definition: wer den "
            f"anderen Header einbindet, uebergibt den falschen Typ. Entweder "
            f"einen der beiden umbenennen oder — mit Eigentuemer-Entscheidung "
            f"— in {GRUNDLINIE} aufnehmen."
        )
    for name in sorted(bekannt - set(gefunden)):
        fehler.append(
            f"{name}: kollidiert nicht mehr — Zeile aus {GRUNDLINIE} "
            f"entfernen, damit die Grundlinie faellt (Tor-57-Bauform)."
        )
    return fehler


# ── Selbsttest ──────────────────────────────────────────────────────────────
# Er steht VOR der Messung, weil eine Erstfassung von `tuersucher.py` einmal
# „Selbsttest 3/3" meldete und gemessen 0/3 lieferte. Jeder Fall prueft BEIDE
# Richtungen: der Fund muss kommen, und der Nicht-Fund darf nicht kommen.

FAELLE = [
    ("zwei Header, gleiche Aritaet, verschiedene Zeigertypen", True, {
        "a.h": "int f(struct alpha *x);\n",
        "b.h": "int f(struct beta *x);\n",
    }),
    ("zwei Header, identische Deklaration", False, {
        "a.h": "int g(struct alpha *x);\n",
        "b.h": "int g(struct alpha *y);\n",
    }),
    ("Aritaet verschieden — gehoert Regel B, nicht hierher", False, {
        "a.h": "int h(struct alpha *x);\n",
        "b.h": "int h(struct alpha *x, int n);\n",
    }),
    ("Ueberladungssatz in EINER Datei", False, {
        "a.h": "int k(struct alpha *x);\nint k(struct beta *x);\n",
    }),
    ("Deklaration im Klassenrumpf", False, {
        "a.h": "class W {\npublic:\n    int m(struct alpha *x);\n};\n",
        "b.h": "class V {\npublic:\n    int m(struct beta *x);\n};\n",
    }),
    ("nur im Kommentar erwaehnt", False, {
        "a.h": "/* Beispiel: int n(struct alpha *x); */\nint real(int a);\n",
        "b.h": "int n(struct beta *x);\n",
    }),
    ("Nicht-Zeiger-Unterschied (size_t vs unsigned long)", False, {
        "a.h": "int p(size_t n);\n",
        "b.h": "int p(unsigned long n);\n",
    }),
    ("Funktionszeiger-Parameter wird uebersprungen", False, {
        "a.h": "int q(struct alpha *x, void (*cb)(int));\n",
        "b.h": "int q(struct beta *x, void (*cb)(long));\n",
    }),
]


def selbsttest() -> int:
    gut = 0
    for titel, erwartet, dateien in FAELLE:
        with tempfile.TemporaryDirectory() as td:
            wurzel = Path(td)
            for name, inhalt in dateien.items():
                (wurzel / name).write_text(inhalt, encoding="utf-8")
            # kein git in der Attrappe -> header_dateien() faellt auf rglob
            # zurueck und sagt es; das ist hier der gewollte Weg.
            treffer = kollisionen(wurzel, still=True)
        ist = bool(treffer)
        ok = ist == erwartet
        gut += ok
        print(f"  [{'ok' if ok else 'ROT'}] {titel}: "
              f"erwartet {'Fund' if erwartet else 'kein Fund'}, "
              f"gemessen {'Fund' if ist else 'kein Fund'}"
              + (f" {sorted(treffer)}" if ist else ""))
    print(f"Selbsttest {gut}/{len(FAELLE)}")
    return 0 if gut == len(FAELLE) else 1


def main() -> int:
    wurzel = Path(__file__).resolve().parent.parent

    if "--selbsttest" in sys.argv:
        return selbsttest()

    print("Selbsttest vor der Messung:")
    if selbsttest() != 0:
        print("Selbsttest ROT — Messung wird nicht ausgefuehrt.", file=sys.stderr)
        return 1
    print()

    gefunden = kollisionen(wurzel)
    print(f"{len(gefunden)} Namen mit gleicher Aritaet und verschiedenen "
          f"Zeigertypen:")
    for name in sorted(gefunden):
        for z in gefunden[name]:
            print(f"  {z}")

    if "--schreibe-grundlinie" in sys.argv:
        ziel = wurzel / GRUNDLINIE
        kopf = (
            "# Grundlinie fuer scripts/audit_typkollision.py (MF-1155).\n"
            "# Ein Funktionsname je Zeile: gleiche Aritaet, Zeiger auf\n"
            "# verschiedene Typen, in mehr als einem Header deklariert.\n"
            "# FALLENDE Grundlinie (Tor-57-Bauform): die Zahl darf nur sinken.\n"
            "# Eine Zeile verschwindet, wenn die Kollision aufgeloest ist —\n"
            "# welche der beiden Fassungen die kanonische ist, entscheidet der\n"
            "# Eigentuemer (P3-410), nicht dieses Tor.\n"
        )
        ziel.write_text(kopf + "".join(f"{n}\n" for n in sorted(gefunden)),
                        encoding="utf-8")
        print(f"\n-> {GRUNDLINIE}: {len(gefunden)} Namen")
        return 0

    if "--grundlinie" in sys.argv:
        fehler = check(wurzel)
        if fehler:
            print("\nGegen die Grundlinie:")
            for f in fehler:
                print(f"  {f}")
            return 1
        print(f"\nGegen die Grundlinie: 0 neu, 0 veraltet.")
        return 0

    return 0


if __name__ == "__main__":
    sys.exit(main())
