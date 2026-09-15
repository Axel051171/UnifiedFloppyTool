#!/usr/bin/env python3
"""Woraus kommt die Zahl? Das Tor zur Sonden-Doktrin (MF-1153)

── Warum es dieses Tor gibt ─────────────────────────────────────────────

Eigentuemer-Entscheidung vom 2026-09-15, verbindliche Fassung
`docs/SONDEN_DOKTRIN.md`: eine Sondenkonfidenz ist die SUMME benannter
Belege, nie ein Handwert.

Der Anlass ist gemessen. Von 23 offenen Punkten in `docs/OPEN_ITEMS.md`
waren ACHT dieselbe Frage — P3-392, P3-393, P3-394, P3-401, P3-402,
P3-403, P3-405, P3-406 —, und sie wurde je Fall neu beantwortet. Der
Beleg dafuer stammt aus einem einzigen Tag: MF-1151 hat `dmk` von 100
auf 75 gesenkt, weil das Format keine Kennung hat, und MF-1152 hat `ssd`
bei 85 gelassen, obwohl Acorn DFS ebenso keine hat.

**Eine Zahl, die jemand VERGIBT, traegt keine Begruendung.** Dieses Tor
zaehlt die Sonden, die noch eine vergeben.

── Warum eine GRUNDLINIE und nicht sofort rot ──────────────────────────

Es gibt 137 registrierte Sonden. Jede einzeln zu migrieren heisst, fuer
jede zu entscheiden, WAS sie wirklich liest — das ist Arbeit, kein
Streit, und sie braucht Zeit. Ein Tor, das am ersten Tag rot ist, wird
abgeschaltet; ein Tor mit fallender Grundlinie kann nicht schlechter
werden.

Dieselbe Bauform wie Tor 57 (`audit_schreibzusage.py`, MF-883/930): die
Zahl darf nur SINKEN. Und der eigentliche Zweck ist nicht die Liste,
sondern der Rand: **ein NEUES Format kann gar nicht mehr anders
anfangen**, weil seine Sonde nicht in der Grundlinie steht.

── Was es NICHT sieht ──────────────────────────────────────────────────

Ob die BELEGE stimmen. Eine Sonde kann `UFT_BELEG_KENNUNG` setzen, wo
sie nur ein Byte in einem Wertebereich geprueft hat — das faellt hier
nicht auf, sondern nur einem Leser. Was dieses Tor leistet, ist die
Erzwingung der HERLEITUNG; die Richtigkeit der Belege entscheiden die
Eichungen (`test_probe_confidence_on_{zeros,random,text}`) und der Test
je Format.

Dateimenge aus `git ls-files` ueber `scripts/repo_scope.py` (MF-636),
nie aus einer Verzeichnisliste.
"""
from __future__ import annotations

import argparse
import re
import sys
import tempfile
from pathlib import Path

WURZEL = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(WURZEL / "scripts"))

GRUNDLINIE = WURZEL / "docs" / "sondendoktrin_baseline.txt"

# Die Sondenfunktion wird am VERTRAG gefunden, nicht am Namensmuster —
# Lehre aus MF-1137, wo ein Muster `*_probe` zwoelf Faelle uebersah.
PROBE_FELD = re.compile(r"\.probe\s*=\s*(\w+)")

# Eine Zuweisung an das Konfidenzziel — IRGENDEINE.
#
# Hier stand `\*\s*(\w+)\s*=\s*(\d+)\s*;`, also nur ein LITERAL. Die
# Zahlen gingen daraufhin nicht auf: nach zwei Migrationen fiel die
# Messung von 75 auf 74 statt auf 73, und der Grund war `dmk` — es
# schrieb `*confidence = conf;` mit einer vorher berechneten Variablen
# und war damit fuer das Muster unsichtbar. Eine Handzahl bleibt eine
# Handzahl, auch wenn sie ueber eine Variable geht.
#
# Die Frage lautet nicht „steht hier ein Literal", sondern **„wird die
# Zahl abgeleitet"**. Deshalb zaehlt jede Zuweisung an `*<name>`, und
# entlastend wirkt allein der Aufruf der Ableitung. Klasse MF-1000: ein
# Tor, das schmaler ist als sein Gegenstand, meldet zuverlaessig zu
# wenig — diesmal im eigenen Instrument, gefunden daran, dass eine
# Summe nicht aufging.
ZUWEISUNG = re.compile(r"\*\s*(\w+)\s*=[^=]")

# Der erlaubte Weg.
ABLEITUNG = re.compile(r"uft_probe_konfidenz\s*\(")


def quellen(wurzel: Path) -> list[Path]:
    try:
        from repo_scope import dateien_im_scope  # type: ignore
        pfade = [wurzel / p for p in dateien_im_scope()]
    except Exception:
        import subprocess
        aus = subprocess.run(
            ["git", "ls-files", "--cached", "--others", "--exclude-standard",
             "src"], cwd=wurzel, capture_output=True, text=True)
        if aus.returncode != 0:
            print("[hinweis] git nicht befragbar — alle Dateien gelten als "
                  "im Scope, und das ist Absicht (MF-636)", file=sys.stderr)
            pfade = list((wurzel / "src").rglob("*.c"))
        else:
            pfade = [wurzel / z for z in aus.stdout.splitlines()]
    return [p for p in pfade
            if p.suffix == ".c" and "src" in p.parts and "formats" in p.parts]


def rumpf(text: str, fn: str):
    """Der Rumpf der Funktion `fn`, oder None."""
    d = re.search(r"\b" + re.escape(fn) + r"\s*\([^;{]*\)\s*\{", text)
    if not d:
        return None
    i = text.index("{", d.start())
    tiefe = 0
    for j in range(i, len(text)):
        if text[j] == "{":
            tiefe += 1
        elif text[j] == "}":
            tiefe -= 1
            if tiefe == 0:
                return text[i:j + 1]
    return None


def ohne_kommentare(s: str) -> str:
    s = re.sub(r"/\*.*?\*/", " ", s, flags=re.S)
    return re.sub(r"//[^\n]*", " ", s)


def messe(pfade, wurzel: Path):
    """(noch nicht migriert, migriert) als `pfad:funktion`-Zeilen."""
    offen = []
    fertig = []
    for p in pfade:
        try:
            text = p.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        try:
            rel = p.relative_to(wurzel).as_posix()
        except ValueError:
            rel = p.name
        for m in PROBE_FELD.finditer(text):
            fn = m.group(1)
            r = rumpf(text, fn)
            if r is None:
                continue
            r = ohne_kommentare(r)
            zeile = f"{rel}:{fn}"
            if ABLEITUNG.search(r):
                fertig.append(zeile)
            elif ZUWEISUNG.search(r):
                offen.append(zeile)
            # Eine Sonde ohne beides vergibt keine Zahl (z. B. weil sie
            # immer absagt) — die ist nicht zu migrieren.
    return sorted(set(offen)), sorted(set(fertig))


def grundlinie_lesen(p: Path) -> set:
    if not p.exists():
        return set()
    return {z.strip() for z in p.read_text(encoding="utf-8").splitlines()
            if z.strip() and not z.startswith("#")}


def selbsttest() -> int:
    """Erst der Selbsttest, dann der Nenner — eine Erstfassung dieses
    Musters hat einmal „3/3" gemeldet und 0/3 geliefert."""
    faelle = [
        ('const uft_format_plugin_t p = { .probe = a_probe };\n'
         'static bool a_probe(const uint8_t*d,size_t s,size_t f,int*c)'
         '{ *c = 85; return true; }', ["x.c:a_probe"], [], "Literal 85"),
        ('const uft_format_plugin_t p = { .probe = b_probe };\n'
         'static bool b_probe(const uint8_t*d,size_t s,size_t f,int*c)'
         '{ *c = uft_probe_konfidenz(UFT_BELEG_KENNUNG); return true; }',
         [], ["x.c:b_probe"], "Ableitung"),
        ('const uft_format_plugin_t p = { .probe = c_probe };\n'
         'static bool c_probe(const uint8_t*d,size_t s,size_t f,int*c)'
         '{ /* *c = 85; nur ein Kommentar */ '
         '*c = uft_probe_konfidenz(0); return true; }',
         [], ["x.c:c_probe"], "Literal NUR im Kommentar zaehlt nicht"),
        ('const uft_format_plugin_t p = { .probe = d_probe };\n'
         'static bool d_probe(const uint8_t*d,size_t s,size_t f,int*c)'
         '{ (void)c; return false; }', [], [],
         "sagt immer ab — nichts zu migrieren"),
        ('const uft_format_plugin_t p = { .probe = e_probe };\n'
         'static bool e_probe(const uint8_t*d,size_t s,size_t f,int*conf)'
         '{ *conf = 40; return true; }', ["x.c:e_probe"], [],
         "anderer Parametername"),
    ]
    gut = 0
    for text, soll_offen, soll_fertig, was in faelle:
        with tempfile.TemporaryDirectory() as d:
            # Der Pfadfilter verlangt src und formats im Pfad.
            q = Path(d) / "src" / "formats" / "t"
            q.mkdir(parents=True)
            f = q / "x.c"
            f.write_text(text, encoding="utf-8")
            o, fe = messe([f], Path(d))
            o = [z.split("/")[-1] for z in o]
            fe = [z.split("/")[-1] for z in fe]
            ok = (o == soll_offen and fe == soll_fertig)
        print(f"  {'ok  ' if ok else 'FAIL'} {was}")
        gut += 1 if ok else 0
    print(f"SELBSTTEST {gut}/{len(faelle)}")
    return 0 if gut == len(faelle) else 1


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--selbsttest", action="store_true")
    ap.add_argument("--grundlinie", default=None)
    ap.add_argument("--schreibe-grundlinie", action="store_true")
    a = ap.parse_args()

    if a.selbsttest:
        return selbsttest()

    gl_pfad = Path(a.grundlinie) if a.grundlinie else GRUNDLINIE
    offen, fertig = messe(quellen(WURZEL), WURZEL)

    if a.schreibe_grundlinie:
        gl_pfad.parent.mkdir(parents=True, exist_ok=True)
        gl_pfad.write_text(
            "# Sonden, die ihre Konfidenz noch SELBST vergeben statt sie\n"
            "# ueber uft_probe_konfidenz() abzuleiten (MF-1153).\n"
            "#\n"
            "# Verbindliche Fassung der Doktrin: docs/SONDEN_DOKTRIN.md\n"
            "#\n"
            "# Diese Zahl darf nur SINKEN. Erzeugt mit\n"
            "# scripts/audit_sondendoktrin.py --schreibe-grundlinie\n"
            + "".join(z + "\n" for z in offen), encoding="utf-8")
        print(f"-> {gl_pfad.relative_to(WURZEL)} ({len(offen)} Zeilen)")
        return 0

    gl = grundlinie_lesen(gl_pfad)
    neu = [z for z in offen if z not in gl]
    weg = [z for z in gl if z not in offen]

    print(f"  Sonden mit abgeleiteter Konfidenz : {len(fertig)}")
    print(f"  Sonden mit vergebener Konfidenz   : {len(offen)}")
    print(f"  Grundlinie                        : {len(gl)}")
    print(f"  NEU hinzugekommen                 : {len(neu)}")
    print(f"  seit der Grundlinie migriert      : {len(weg)}")

    if weg:
        print("\n  migriert:")
        for z in sorted(weg):
            print(f"    {z}")
    if neu:
        print("\n  NEU — eine Sonde vergibt eine Zahl, ohne sie herzuleiten:")
        for z in sorted(neu):
            print(f"    {z}")
        print("\n  Die Doktrin steht in docs/SONDEN_DOKTRIN.md. Der Weg ist\n"
              "  `*confidence = uft_probe_konfidenz(belege)` — und die\n"
              "  Belege sind der Punkt: sie sagen, WORAUS die Zahl kommt.")
        return 1

    print("\nOK: keine neue Sonde vergibt eine Konfidenz von Hand.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
