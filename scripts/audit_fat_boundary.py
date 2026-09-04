#!/usr/bin/env python3
"""Die FAT12/16-Grenze steht an fuenf Stellen. Sie muss EINE bleiben (MF-875).

── Der Befund ──────────────────────────────────────────────────────────

Gemessen im Baum: die Entscheidung „FAT12 oder FAT16" faellt an vier
Stellen mit der **fest verdrahteten** Zahl 4085, und daneben liegt eine
benannte Konstante `UFT_FAT12_MAX_CLUSTERS 4084`, die **niemand**
benutzt.

    src/detect/mfm/mfm_detect.c            clusters < 4085
    src/formats/fat/uft_fat_bootsector.c   cluster_count < 4085
    src/formats/legacy/uft_fdi.c           data_clusters >= 4085
    src/fs/uft_fat12.c                     zweimal, < 4085
    include/uft/uft_fat12.h                #define … 4084   (0 Aufrufer)

Das ist die Form aus MF-870: derselbe Wert unter verschiedenen Namen,
und Tor 52 (`audit_macro_drift.py`) faengt genau das NICHT — es
vergleicht gleiche NAMEN mit verschiedenen Werten.

── Warum es hier besonders zaehlt ──────────────────────────────────────

Die Grenze ist NICHT selbstverstaendlich. Es gibt zwei hergeleitete
Werte aus zwei Systemen:

  MS-DOS / FAT-Spezifikation        < 4085
      reserviert $FF7..$FFF (9) + $000,$001 (2) -> 4096-11 = 4085

  TOS 2.06/3.06/4.0x                <= 4078
      reserviert $FF0..$FFF (16) + $000,$001 (2) -> 4096-18 = 4078
      Quelle: Harun Scheutzow, FLOP_FIX.TXT (1992), Abschnitt
      „ED-Disketten" — mit ausgeschriebener Herleitung:
      „Bis einschliesslich 4078 Datenclustern wird von einer 12Bit-FAT
       ausgegangen. Ab 4079 Datenclustern wird eine 16Bit-FAT verwendet."

Eine Diskette mit **4079 bis 4084** Datenclustern wird von TOS als
FAT16 und von MS-DOS als FAT12 gelesen. Dieselben Bytes, zwei
Ergebnisse.

UFT bleibt bewusst bei 4085 — fuer ein Werkzeug, das ueberwiegend
PC-Medien liest, ist das der richtige Wert, und die Entscheidung ist
Eigentuemer-Sache. Dieses Tor aendert sie nicht. Es haelt nur fest,
dass sie an ALLEN Stellen dieselbe ist.

── Was das Tor prueft ──────────────────────────────────────────────────

1. Jede FAT12/16-Entscheidung im Baum benutzt 4085 (bzw. 4084 als
   „letzter FAT12-Cluster"). Taucht ein dritter Wert auf — etwa 4078 —
   ohne dass jemand die Grundlinie bewusst aendert, wird das gemeldet.
2. Die Zahl der Fundstellen darf nicht wachsen. Eine sechste Kopie ist
   eine Entscheidung, kein Versehen.

Die Grundlinie darf FALLEN (Kopien zusammenfuehren ist gut) und nur mit
Begruendung im Commit steigen.

Dateimenge aus `git ls-files` (MF-636), nie aus einer gepflegten Liste.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from repo_scope import repo_files  # noqa: E402

# Die erlaubten Werte und was sie bedeuten.
ERLAUBT = {
    4085: "MS-Grenze: erster FAT16-Clusterzaehler",
    4084: "MS-Grenze: letzter FAT12-Cluster",
}

# Werte, die auftauchen KOENNTEN und dann eine Entscheidung waeren.
BEKANNT_FREMD = {
    4078: "TOS 2.06+ (FLOP_FIX.TXT 1992) — andere Plattform, andere Grenze",
    4079: "TOS 2.06+ erster FAT16-Clusterzaehler",
    4086: "SED 5.68 HLP, ohne Herleitung",
}

# Gezaehlt werden ENTSCHEIDUNGSSTELLEN. Ein Test, der die Schwelle
# prueft, nennt die Zahl ebenfalls — er ist aber keine Kopie der
# Entscheidung, sondern ihre Absicherung. Der WERT wird auch dort
# geprueft (ein Test, der 4078 zusichert, waere ein Befund); nur in die
# Kopienzahl geht er nicht ein.
#
# Das ist eine EINORDNUNG, keine Ausschlussliste im Sinne von MF-636:
# die Dateimenge kommt weiterhin vollstaendig aus `git ls-files`, und
# keine Datei entzieht sich der Wertpruefung.
IST_PRUEFUNG = ("tests/",)

GRUNDLINIE = 6   # Entscheidungsstellen ausserhalb tests/, Stand MF-875

ENDUNGEN = (".c", ".h", ".cpp", ".hpp")

# Eine Zeile zaehlt, wenn sie eine der Zahlen traegt UND im Umfeld von
# Clustern/FAT die Rede ist. Reine Zahlenkollisionen (CRC-Tabellen!)
# fallen so heraus — `0x4084` in einer CCITT-Tabelle ist keine Grenze.
ZAHL = re.compile(r"\b(4078|4079|4084|4085|4086)\b")
KONTEXT = re.compile(r"cluster|fat1[26]|fat_type|is_fat16|FAT12|FAT16",
                     re.IGNORECASE)


def _fundstellen(repo: Path):
    dateien = repo_files(repo)
    if dateien is None:
        return None, ["git nicht befragbar — dieses Tor kann nichts sagen "
                      "(MF-636: lieber laut unbrauchbar als still blind)"]

    treffer = []
    for pfad in sorted(dateien):
        if pfad.suffix not in ENDUNGEN:
            continue
        try:
            text = pfad.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        if "4078" not in text and "4084" not in text and "4085" not in text \
           and "4086" not in text and "4079" not in text:
            continue
        # Kommentare zaehlen NICHT. Eine Zeile, die den anderen Wert
        # nennt, um ihn zu erklaeren, ist Dokumentation — genau das
        # soll dieses Tor foerdern, nicht bestrafen. Der Strip ist
        # bewusst einfach gehalten und kennt seine Grenze: ein "/*" in
        # einem String-Literal wuerde ihn taeuschen. Fuer diese Zahlen
        # gibt es im Baum keinen solchen Fall, und ein zu vorsichtiger
        # Strip meldete lieber zu viel als zu wenig.
        im_block = False
        for nr, roh in enumerate(text.splitlines(), 1):
            zeile = roh
            if im_block:
                if "*/" in zeile:
                    zeile = zeile.split("*/", 1)[1]
                    im_block = False
                else:
                    continue
            while "/*" in zeile:
                vor, rest = zeile.split("/*", 1)
                if "*/" in rest:
                    zeile = vor + rest.split("*/", 1)[1]
                else:
                    zeile = vor
                    im_block = True
                    break
            if "//" in zeile:
                zeile = zeile.split("//", 1)[0]

            m = ZAHL.search(zeile)
            if not m or not KONTEXT.search(zeile):
                continue
            try:
                rel = pfad.relative_to(repo).as_posix()
            except ValueError:
                rel = pfad.as_posix()
            treffer.append((rel, nr, int(m.group(1)), zeile.strip()))
    return treffer, []


def check(repo: Path) -> list[str]:
    treffer, fehler = _fundstellen(repo)
    if treffer is None:
        return fehler

    for rel, nr, wert, zeile in treffer:
        if wert in ERLAUBT:
            continue
        grund = BEKANNT_FREMD.get(wert, "unbekannter Grenzwert")
        fehler.append(
            f"{rel}:{nr} entscheidet FAT12/16 mit {wert} — {grund}. "
            f"Der Baum benutzt sonst 4085. Ein zweiter Wert im selben "
            f"Baum heisst: dieselben Bytes bekommen je nach Codepfad "
            f"verschiedene Antworten. Entweder begruenden und die "
            f"Grundlinie in scripts/audit_fat_boundary.py anheben, oder "
            f"auf 4085 vereinheitlichen.  [{zeile[:70]}]")

    entscheidungen = [x for x in treffer
                      if not x[0].startswith(IST_PRUEFUNG)]
    if len(entscheidungen) > GRUNDLINIE:
        neue = "\n      ".join(f"{r}:{n}" for r, n, _, _ in entscheidungen)
        fehler.append(
            f"{len(entscheidungen)} Entscheidungsstellen der "
            f"FAT12/16-Grenze, Grundlinie "
            f"{GRUNDLINIE}. Eine weitere Kopie ist eine Entscheidung, kein "
            f"Versehen — zusammenfuehren, oder die Grundlinie mit "
            f"Begruendung anheben.\n      {neue}")
    return fehler


def _selbsttest(repo: Path) -> int:
    """Vor dem Nenner. Ein Tor, das seine eigene Abnahme nicht besteht,
    darf nicht zaehlen (MF-693)."""
    import tempfile
    import subprocess

    faelle = [
        # (Inhalt, soll das Tor meckern?)
        ("if (clusters < 4085) fat12();\n", False),
        ("#define UFT_FAT12_MAX_CLUSTERS 4084  /* cluster */\n", False),
        ("if (clusters <= 4078) fat12();  /* TOS */\n", True),
        ("if (cluster_count < 4086) x();\n", True),
        # Gegenprobe: dieselbe Zahl OHNE FAT-Kontext (CRC-Tabelle)
        ("static const uint16_t t[] = { 0x4084, 0x50A5 };\n", False),
        # Gegenprobe: FAT-Kontext ohne Grenzzahl
        ("if (cluster_count < 12) x();\n", False),
        # Kommentare sind Dokumentation, keine Entscheidung — sonst
        # bestrafte das Tor genau das Verhalten, das es foerdern will.
        ("/* TOS schaltet bei 4078 Clustern auf FAT16 um. */\n", False),
        ("// cluster_count < 4078 waere die TOS-Grenze\n", False),
        ("/* mehrzeilig\n * cluster 4078 FAT12\n */\n", False),
        # ... aber Code NACH einem Blockkommentar wird wieder gesehen.
        ("/* Hinweis */ if (cluster_count < 4078) x();\n", True),
        # ... und Code VOR einem Zeilenkommentar ebenfalls.
        ("if (cluster_count < 4078) x();  // TOS\n", True),
    ]
    gut = 0
    for i, (inhalt, soll) in enumerate(faelle, 1):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d)
            subprocess.run(["git", "init", "-q"], cwd=d,
                           capture_output=True)
            (p / "f.c").write_text(inhalt, encoding="utf-8")
            errs = check(p)
            hat = any("entscheidet FAT12/16" in e for e in errs)
            if hat == soll:
                gut += 1
            else:
                print(f"  Selbsttest {i}: erwartet meckern={soll}, "
                      f"bekommen {hat}  [{inhalt.strip()[:50]}]")
    print(f"Selbsttest: {gut}/{len(faelle)}")
    return 0 if gut == len(faelle) else 1


def main() -> int:
    repo = Path(__file__).resolve().parent.parent
    if "--selftest" in sys.argv:
        return _selbsttest(repo)

    treffer, _ = _fundstellen(repo)
    if treffer is not None and ("-v" in sys.argv or "--list" in sys.argv):
        ent = [x for x in treffer if not x[0].startswith(IST_PRUEFUNG)]
        print(f"FAT12/16-Grenze: {len(ent)} Entscheidungsstellen "
              f"(Grundlinie {GRUNDLINIE}), "
              f"{len(treffer) - len(ent)} Nennungen in Tests:")
        for rel, nr, wert, zeile in treffer:
            art = "PRUEFUNG " if rel.startswith(IST_PRUEFUNG) else "ENTSCHEID"
            print(f"  {art} {rel}:{nr}  {wert}   {zeile[:60]}")
        print()

    errs = check(repo)
    if not errs:
        print("OK: die FAT12/16-Grenze ist im ganzen Baum dieselbe.")
        return 0
    print(f"FAIL: {len(errs)} Befund(e)")
    for e in errs:
        print(f"  {e}")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
