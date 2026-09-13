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

       **A1 ist seit MF-1091 an der MAGIE gefasst, nicht an der
       Endung** (Entscheidung des Eigentuemers). Eine Datei gilt als
       Bauartefakt, wenn ihre ersten Bytes `ELF`, `MZ`, ein
       Mach-O-Wort oder `!<arch>` sind — unabhaengig davon, wie sie
       heisst. Die Endungsliste bleibt DANEBEN stehen, weil sie
       `*.o`, `*.gcda` und `build/` auch dann fasst, wenn die Datei
       leer ist und gar keine Magie hat.

       Der Anlass ist gemessen: `tests/test_smoke` war eine
       **endungslose** ELF-Datei und fiel durch jede Endungsregel.
       Ueber die Magie gesucht war sie die einzige ihrer Art unter
       2779 versionierten Dateien. Entfernt in MF-1091 — nicht weil
       sie gross war, sondern weil ihr Quelltext seit MF-011
       geloescht ist und GPL-2 ihn verlangt.

**H3** Jede Datei unter `docs/format_specs/` steht in der `README.md`
       ihres Verzeichnisses, **mit ihrer SHA-256**. Heute **47/47**.
       Damit ist die Herkunftsdatei nicht nur da, sondern sie DECKT
       auch, was daneben liegt — eine neue Datei ohne Eintrag laesst
       das Tor feuern, und ein geaenderter Inhalt ebenso.

       **Bis MF-1091 hat H3 nur GEZAEHLT** (0 von 47 mit
       Herkunftsdatei) und ausdruecklich nicht blockiert, weil die
       Entscheidung zwischen „behalten mit README" und „auslagern"
       dem Eigentuemer zustand (Klasse C2). Sie ist gefallen —
       behalten —, also darf das Tor jetzt halten, was entschieden
       ist. Die WEITERGABEFRAGE bleibt davon unberuehrt offen
       (P3-372); ein Tor kann eine Herkunft belegen, keine Erlaubnis.

── Was hier nur BERICHTET, und warum ────────────────────────────────────

**H2** Positivliste der versionierten Binaerdateien. Gemessen **108**
       Stueck, 56 279 KB; **94** davon liegen in den Korpus-
       Verzeichnissen und sind ueber das Manifest belegt, **14**
       ausserhalb. Eine Positivliste mit Pfad, Herkunft und Lizenz gibt
       es noch nicht — sie zu erfinden waere genau die gepflegte Liste,
       vor der MF-636 warnt. Deshalb wird gezaehlt und aufgefuehrt, und
       die Liste bleibt eine Entscheidung.

**H4b** (aufgegangen in H4, siehe oben.) Ausfuehrbare Dateien an der
       KENNUNG statt an der Endung.
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

**H5** Groesse des versionierten Bestands — **ohne Schwelle und ohne
       Blockade**, nur zwei Zahlen je Lauf: Gesamtgroesse und
       Zuwachs seit dem letzten Stand in `docs/repo_groesse.json`.

       Die erste Fassung war ABSICHTLICH nicht umgesetzt, mit der
       Begruendung, G2 verbiete die Kennzahl als Motiv und eine
       Schwelle mache genau daraus eines. Der Eigentuemer hat dem
       zugestimmt und die Anweisung geaendert: **eine berichtete
       Zahl kann kein Motiv werden, eine erzwungene schon.** Also
       wird berichtet.

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


MAGIE = {
    bytes([0x7F]) + b"ELF": "ELF",
    b"MZ": "PE/DOS",
    b"!<arch>": "ar",
    bytes([0xCA, 0xFE, 0xBA, 0xBE]): "Mach-O",
    bytes([0xCF, 0xFA, 0xED, 0xFE]): "Mach-O",
    bytes([0xFE, 0xED, 0xFA, 0xCE]): "Mach-O",
}


def magie(p: Path) -> str:
    """Programmdatei nach den ersten Bytes. Leer, wenn keine."""
    try:
        if not p.is_file():
            return ""
        k = p.open("rb").read(8)
    except OSError:
        return ""
    for m, name in MAGIE.items():
        if k.startswith(m):
            return name
    return ""


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

    # H3: Herkunftsdatei je Verzeichnis unter docs/format_specs/,
    # und sie muss die Nachbarn mit SHA-256 DECKEN.
    import hashlib
    nach_verz: dict[str, list[str]] = {}
    for d in dat:
        if d.startswith(SPECS) and "/" in d[len(SPECS):]:
            nach_verz.setdefault(d.rsplit("/", 1)[0], []).append(d)
    for verz, dateien_ in sorted(nach_verz.items()):
        rm = wurzel / verz / "README.md"
        if not rm.exists():
            befunde.append(
                f"H3 {verz}: keine README.md — Fremddokumente ohne "
                f"Herkunftsangabe (Klasse C2 der Aufraeum-Anweisung)")
            continue
        text = rm.read_text(encoding="utf-8", errors="replace")
        for f in sorted(dateien_):
            name = f.rsplit("/", 1)[-1]
            if name.lower() == "readme.md":
                continue
            if f"`{name}`" not in text:
                befunde.append(
                    f"H3 {f}: steht nicht in {verz}/README.md")
                continue
            try:
                h = hashlib.sha256(
                    (wurzel / f).read_bytes()).hexdigest()
            except OSError:
                continue
            if h not in text:
                befunde.append(
                    f"H3 {f}: SHA-256 in {verz}/README.md stimmt nicht "
                    f"— gemessen {h[:16]}…")

    for d in dat:
        if A1.search(d):
            befunde.append(f"H4 {d}: Bauartefakt (Klasse A1) ist versioniert")
        elif A2.search(d):
            befunde.append(f"H4 {d}: Werkzeugabfall (Klasse A2) ist versioniert")
        else:
            art = magie(wurzel / d)
            if art:
                befunde.append(
                    f"H4 {d}: {art}-Programmdatei ist versioniert — "
                    f"Bauartefakt nach der MAGIE, nicht nach der Endung "
                    f"(A1 neu gefasst, MF-1091)")
    return befunde


def bericht(wurzel: Path) -> None:
    """H2 und H3 — gezaehlt, nicht erzwungen."""
    dat = versionierte(wurzel)
    spec = [d for d in dat if d.startswith(SPECS)]
    mit = [d for d in spec if d.rsplit("/", 1)[-1].lower()
           in ("readme.md", "readme.txt", "herkunft.md", "quellen.md")]
    kb = sum((wurzel / d).stat().st_size for d in spec
             if (wurzel / d).is_file()) // 1024
    print(f"H3  {SPECS}: {len(spec)} Dateien, {kb} KB, "
          f"{len(mit)} Herkunftsdatei(en) — blockierend seit MF-1091; "
          f"die Weitergabefrage bleibt offen (P3-372)")

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

    # H5 — zwei Zahlen, keine Schwelle. Siehe Kopf: eine berichtete Zahl
    # kann kein Motiv werden, eine erzwungene schon.
    gesamt = 0
    for d in dat:
        p = wurzel / d
        if p.is_file():
            try:
                gesamt += p.stat().st_size
            except OSError:
                pass
    stand_datei = wurzel / "docs" / "repo_groesse.json"
    vorher = None
    if stand_datei.exists():
        try:
            vorher = json.loads(stand_datei.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            vorher = None
    if vorher and isinstance(vorher.get("byte"), int):
        diff = gesamt - vorher["byte"]
        vz = "+" if diff >= 0 else "-"
        print(f"H5  versionierter Bestand: {gesamt // 1024} KB "
              f"({len(dat)} Dateien) — {vz}{abs(diff) // 1024} KB seit "
              f"{vorher.get('stand', '?')} ({vorher['byte'] // 1024} KB). "
              f"Keine Schwelle, keine Blockade.")
    else:
        print(f"H5  versionierter Bestand: {gesamt // 1024} KB "
              f"({len(dat)} Dateien) — kein frueherer Stand in "
              f"docs/repo_groesse.json")


# ── Selbsttest ──────────────────────────────────────────────────────────
#
# Gepflanzte Faelle in einem Wegwerfbaum mit eigenem git — ein Tor, das
# nicht feuert, beweist nichts (MF-1000).

def _selbsttest() -> int:
    import tempfile

    faelle = [
        ("sauberer Baum -> still", [], None, False),
        # H3 — die Herkunftsdatei muss die Nachbarn DECKEN.
        ("format_specs ohne README", ["docs/format_specs/x/A.TXT"],
         None, True),
        ("README, aber Datei nicht gelistet",
         ["docs/format_specs/x/A.TXT", "docs/format_specs/x/README.md"],
         None, True),
        ("README mit Name UND richtiger SHA-256 -> still",
         ["docs/format_specs/x/A.TXT", "!README-gut"], None, False),
        ("README mit Name, aber FALSCHER SHA-256",
         ["docs/format_specs/x/A.TXT", "!README-falsch"], None, True),
        ("Bauartefakt versioniert", ["src/foo.o"], None, True),
        # A1 nach MAGIE: der Fall, den die Endungsliste nicht sah.
        ("ELF OHNE Endung", ["!ELF:tests/werkzeug"], None, True),
        ("PE OHNE Endung", ["!MZ:tools/helfer"], None, True),
        ("Textdatei ohne Endung -> still", ["tools/shim"], None, False),
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
            import hashlib as _h
            for rel in extra:
                if rel.startswith("!README"):
                    ziel = baum / "docs" / "format_specs" / "x"
                    ziel.mkdir(parents=True, exist_ok=True)
                    echt = _h.sha256(b"x").hexdigest()
                    sha = echt if rel == "!README-gut" else "00" * 32
                    (ziel / "README.md").write_text(
                        "| `A.TXT` | 1 | `" + sha + "` |\n",
                        encoding="utf-8")
                    continue
                if rel.startswith("!ELF:") or rel.startswith("!MZ:"):
                    art, ziel_rel = rel[1:].split(":", 1)
                    kopf = (bytes([0x7F]) + b"ELF" if art == "ELF"
                            else b"MZ\x90\x00")
                    p = baum / ziel_rel
                    p.parent.mkdir(parents=True, exist_ok=True)
                    p.write_bytes(kopf + b"\x00" * 32)
                    continue
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
