#!/usr/bin/env python3
"""Repo-Hygiene: H1, H3, H4 aus der Aufraeum-Anweisung (MF-1090).

── Warum dieses Tor, obwohl es nichts zu holen gibt ──────────────────────

Die Anweisung des Eigentuemers ordnet Entfernungskandidaten in vier
Klassen. **Klasse A — das, was ohne Rueckfrage weg darf — ist leer**, und
zwar zweimal gemessen: am Stand `3ca2f3f` (13.09.2026, 2761 Dateien) und
erneut bei MF-1090 am Stand `77602b4e` (2777 Dateien). Beide Male:

    A1 Bauartefakt          0
    A2 Werkzeugabfall       0
    A3 leere Dateien        0
    A4 Fixtures ohne Beleg  0   (102 Fixtures gegen 131 Manifest-Eintraege)
    A5 versionierte Archive 0

Der Auftrag lautet deshalb woertlich nicht "aufraeumen", sondern **"Tor
H4 einrichten, damit es so bleibt"**. Ein Tor an einer sauberen Stelle
ist kein Leerlauf — es ist der einzige Weg, eine Null zu halten, ohne
sie jedes Mal neu von Hand zu suchen.

── Was hier BLOCKIERT ────────────────────────────────────────────────────

**H1** Jede versionierte Datei unter `tests/corpus_free/` hat einen
       Eintrag in `tests/corpus_manifest/manifest.json`. Heute **99/99**.
       Ohne Eintrag ist ein Abbild eine Datei ohne Herkunft, und die
       Stufe, die darauf zeigt, ruht auf nichts (G4: Herkunft ueberlebt
       den Inhalt).

**H4** Kein Treffer auf die Muster aus Klasse A1/A2 — Bauartefakte und
       Werkzeugabfall. Heute **0**.

── Was hier nur BERICHTET, und warum ────────────────────────────────────

**H3** Herkunft der Fremddokumente unter `docs/format_specs/`. Gemessen
       **0 von 47** Dateien (552 KB, alle unter `commodore/`) haben eine
       Herkunftsdatei. Das ist ein echter Befund — aber die Anweisung
       nennt dafuer ausdruecklich **zwei zulaessige Fassungen**
       (behalten mit README, oder auslagern mit Verweis), und welche es
       wird, ist eine **Eigentuemer-Entscheidung** (Klasse C2). Ein Tor,
       das hier blockiert, wuerde sie erzwingen statt sie vorzulegen.
       Dieselbe Bauart wie `audit_spdx_policy.py`, das seine
       Attributions-Erklaerungen bewusst als Liste fuehrt: "eine
       Attribution ist nichts Verbotenes, sondern etwas
       Entscheidungsbeduerftiges" (MF-636).

**H2** Positivliste der versionierten Binaerdateien. Gemessen **108**
       Stueck, 56 279 KB; **94** davon liegen in den Korpus-
       Verzeichnissen und sind ueber das Manifest belegt, **14**
       ausserhalb. Eine Positivliste mit Pfad, Herkunft und Lizenz gibt
       es noch nicht — sie zu erfinden waere genau die gepflegte Liste,
       vor der MF-636 warnt. Deshalb wird gezaehlt und aufgefuehrt, und
       die Liste bleibt eine Entscheidung.

**H4b** Ausfuehrbare Dateien an der KENNUNG statt an der Endung.
       A1 zaehlt Endungen auf — `*.o`, `*.exe`, `*.so`, … — und eine
       endungslose Binaerdatei faellt durch jede davon. Gemessen
       MF-1090: **`tests/test_smoke`, 34 848 Byte, `ELF`**,
       versioniert seit dem v4.1.0-Commit `4d622192`. Es ist die
       einzige ihrer Art im Baum, und **nichts verweist auf sie** —
       jeder Treffer auf "test_smoke" meint
       `tests/conformance/test_smoke.py` oder den gleichnamigen
       C-Test, der laut `docs/MASTER_PLAN.md:558` seit MF-011 in
       `EXCLUDED_TESTS` steht, weil seine Quellen geloescht wurden.
       Sie wird hier **berichtet und nicht blockiert**: die
       Entfernung ist eine Eigentuemer-Entscheidung, weil die
       Pruefung P1 der Anweisung sie in der vorliegenden Fassung
       ablehnt (siehe unten).

**H5** Repo-Groesse gegen eine Obergrenze. **Nicht umgesetzt.** Die
       Anweisung sagt in G2 selbst: eine Kennzahl ist nie ein Grund. Ein
       Groessentor braucht eine Zahl, die jemand setzt — und sie zu
       erfinden hiesse, dem Baum ein Motiv unterzuschieben, das die
       Anweisung ausdruecklich verbietet.

Aufruf:
    python scripts/audit_repo_hygiene.py            # prueft
    python scripts/audit_repo_hygiene.py --bericht  # auch H2/H3 zeigen
    python scripts/audit_repo_hygiene.py --selftest # Selbsttest
"""
from __future__ import annotations

import json
import re
import subprocess
import sys
from pathlib import Path

WURZEL = Path(__file__).resolve().parent.parent

# Klasse A1/A2 aus der Anweisung, woertlich uebernommen.
A1 = re.compile(r"\.(o|a|so|exe|obj|d|gcda|gcno|dll|lib|pyc)$"
                r"|(^|/)(build|CMakeFiles)/")
A2 = re.compile(r"(~$)|\.(bak|orig|rej|swp)$|(^|/)(\.DS_Store|Thumbs\.db)$")

KORPUS_FREI = "tests/corpus_free/"
SPECS = "docs/format_specs/"


def versionierte(wurzel: Path) -> list[str]:
    """Dateimenge aus git, nie aus einer gepflegten Liste (MF-636)."""
    aus = subprocess.run(["git", "ls-files"], cwd=wurzel,
                         capture_output=True, text=True)
    if aus.returncode != 0:
        print("HINWEIS: git nicht befragbar — Hygiene ungeprueft",
              file=sys.stderr)
        return []
    return [z.strip() for z in aus.stdout.split("\n") if z.strip()]


def manifest_pfade(wurzel: Path) -> tuple[set[str], set[str]]:
    p = wurzel / "tests" / "corpus_manifest" / "manifest.json"
    if not p.exists():
        return set(), set()
    m = json.loads(p.read_text(encoding="utf-8"))
    liste = m if isinstance(m, list) else m.get("entries", m.get("images", []))
    pfade = {e.get("file", "").replace("\\", "/") for e in liste}
    pfade.discard("")
    return pfade, {f.rsplit("/", 1)[-1] for f in pfade}


def check(wurzel: Path) -> list[str]:
    """Nur die BLOCKIERENDEN Zusagen: H1 und H4."""
    dat = versionierte(wurzel)
    if not dat:
        return []
    befunde: list[str] = []

    pfade, basen = manifest_pfade(wurzel)
    for d in dat:
        if not d.startswith(KORPUS_FREI):
            continue
        if d in pfade or d.rsplit("/", 1)[-1] in basen:
            continue
        befunde.append(
            f"H1 {d}: kein Eintrag in tests/corpus_manifest/manifest.json — "
            f"ein Abbild ohne Herkunft belegt nichts")

    for d in dat:
        if A1.search(d):
            befunde.append(f"H4 {d}: Bauartefakt (Klasse A1) ist versioniert")
        elif A2.search(d):
            befunde.append(f"H4 {d}: Werkzeugabfall (Klasse A2) ist versioniert")
    return befunde


def bericht(wurzel: Path) -> None:
    """H2 und H3 — gezaehlt, nicht erzwungen."""
    dat = versionierte(wurzel)
    spec = [d for d in dat if d.startswith(SPECS)]
    mit = [d for d in spec if d.rsplit("/", 1)[-1].lower()
           in ("readme.md", "readme.txt", "herkunft.md", "quellen.md")]
    kb = sum((wurzel / d).stat().st_size for d in spec
             if (wurzel / d).is_file()) // 1024
    print(f"H3  {SPECS}: {len(mit)} von {len(spec)} Dateien mit "
          f"Herkunftsdatei ({kb} KB) — Entscheidung C2 offen")

    binaer = []
    for d in dat:
        p = wurzel / d
        if not p.is_file():
            continue
        try:
            if b"\x00" in p.open("rb").read(8000):
                binaer.append((p.stat().st_size, d))
        except OSError:
            continue
    ausser = [b for b in binaer
              if not b[1].startswith(("tests/corpus", "tests/golden",
                                      "tests/crashers"))]
    ausfuehrbar = []
    for d in dat:
        p = wurzel / d
        if not p.is_file() or "." in p.name:
            continue
        try:
            k = p.open("rb").read(4)
        except OSError:
            continue
        if k[:4] == b"ELF" or k[:2] == b"MZ":
            ausfuehrbar.append((p.stat().st_size, d,
                                "ELF" if k[:4] == b"ELF" else "PE"))
    print(f"H4b endungslose AUSFUEHRBARE Dateien: {len(ausfuehrbar)} "
          f"— von A1 nicht erfasst, weil A1 Endungen aufzaehlt")
    for s, d, art in sorted(ausfuehrbar, reverse=True):
        print(f"      {s:8d} B  {d}  ({art})")

    print(f"H2  versionierte Binaerdateien: {len(binaer)} "
          f"({sum(s for s, _ in binaer) // 1024} KB), davon "
          f"{len(ausser)} ausserhalb der Korpus-Verzeichnisse — "
          f"Positivliste steht aus")
    for s, d in sorted(ausser, reverse=True)[:10]:
        print(f"      {s // 1024:6d} KB  {d}")


# ── Selbsttest ──────────────────────────────────────────────────────────
#
# Gepflanzte Faelle in einem Wegwerfbaum mit eigenem git — ein Tor, das
# nicht feuert, beweist nichts (MF-1000).

def _selbsttest() -> int:
    import tempfile

    faelle = [
        ("sauberer Baum -> still", [], None, False),
        ("Bauartefakt versioniert", ["src/foo.o"], None, True),
        ("Werkzeugabfall versioniert", ["src/foo.c.orig"], None, True),
        ("CMakeFiles versioniert", ["build/CMakeFiles/x.txt"], None, True),
        ("Fixture MIT Manifest-Eintrag -> still",
         ["tests/corpus_free/a.img"], "tests/corpus_free/a.img", False),
        ("Fixture OHNE Manifest-Eintrag",
         ["tests/corpus_free/b.img"], None, True),
        ("Datei ausserhalb corpus_free ohne Eintrag -> still",
         ["docs/irgendwas.md"], None, False),
    ]
    ok = 0
    for titel, extra, eintrag, soll in faelle:
        with tempfile.TemporaryDirectory() as d:
            baum = Path(d)
            (baum / "tests" / "corpus_manifest").mkdir(parents=True)
            (baum / "tests" / "corpus_manifest" / "manifest.json").write_text(
                json.dumps({"images": ([{"file": eintrag}] if eintrag else [])}),
                encoding="utf-8")
            for rel in extra:
                p = baum / rel
                p.parent.mkdir(parents=True, exist_ok=True)
                p.write_text("x", encoding="utf-8")
            subprocess.run(["git", "init", "-q"], cwd=baum,
                           capture_output=True)
            subprocess.run(["git", "add", "-A", "-f"], cwd=baum,
                           capture_output=True)
            feuert = bool(check(baum))
        gut = feuert == soll
        ok += gut
        print("  %-44s %-7s%s" % (
            titel, "feuert" if feuert else "still",
            "" if gut else "  <- erwartet: " + ("feuert" if soll else "still")))
    print("Selbsttest %d/%d" % (ok, len(faelle)))
    return 0 if ok == len(faelle) else 1


def main() -> int:
    if "--selftest" in sys.argv:
        return _selbsttest()
    befunde = check(WURZEL)
    for b in befunde:
        print("  " + b)
    dat = versionierte(WURZEL)
    frei = sum(1 for d in dat if d.startswith(KORPUS_FREI))
    print(f"Repo-Hygiene: {len(befunde)} Befunde "
          f"({frei} Dateien unter {KORPUS_FREI} geprueft, "
          f"{len(dat)} versionierte Dateien auf A1/A2 abgesucht)")
    if "--bericht" in sys.argv:
        bericht(WURZEL)
    return 1 if befunde else 0


if __name__ == "__main__":
    raise SystemExit(main())
