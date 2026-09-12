#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Tor: eine Faehigkeitsabsage, die der eigene Baum widerlegt (MF-1045).

Eingehaengt in `scripts/check_consistency.py` als Tor 67, ueber den
Torvertrag `check(repo) -> list[str]` (wie Tor 66, MF-1044). Schreibt
nichts; liest `docs/CAPABILITIES.md` und `git ls-files`.

── Woher dieses Tor kommt ───────────────────────────────────────────────

`docs/CAPABILITIES.md` fuehrt je Controller eine Spalte **Write**. Das
Zeichen `-` bedeutet dort laut Legende:

    Capability ist fuer diesen Controller protokoll-bedingt nicht
    vorgesehen (z.B. KryoFlux Write — read-only by design).

Das ist eine Aussage ueber das **Geraet**. Gemessen war sie nie.

Fuer KryoFlux widerlegt sie der eigene Baum: `src/hal/uft_kryoflux_dtc.c`
enthaelt einen vollstaendigen Schreiber — `uft_kf_write_track()` baut
`dtc -w -p -i0 -e<t> -s<s> -g<n> -t<t> "<datei>"` und fuehrt es aus, mit
Flusswandlung, Zwischendatei und Fehlerauswertung. Nur ruft ihn niemand.

Die Absage stimmt also im **Ergebnis** (UFT kann mit einem KryoFlux nicht
schreiben) und irrt in der **Begruendung** (nicht das Geraet sieht es
nicht vor — UFT erreicht seinen eigenen Schreiber nicht). Das ist die
Gestalt von MF-930, einen Stock hoeher: dort waren es elf Format-Plugins
mit fertigem, unerreichbarem `uft_<fmt>_write()`.

── Was dieses Tor misst ─────────────────────────────────────────────────

Genau eine Frage: **sagt die Tafel „protokoll-bedingt nicht vorgesehen",
waehrend im Baum ein ausgefuehrter Schreiber fuer diesen Controller
steht?** Wenn ja, behauptet das Dokument etwas ueber die Hardware, das
die eigene Quelle bestreitet.

Die Controller-Menge ist **abgeleitet, nicht gepflegt** (MF-636): sie
kommt aus der Tabelle in `docs/CAPABILITIES.md` selbst, die Dateimenge
aus `git ls-files`. Wer einen Controller hinzufuegt, bekommt den Schutz
ohne Zutun.

Ein Schreiber gilt als **ausgefuehrt** (nicht als Rumpf), wenn sein Koerper
eine wirkliche Ausgabehandlung enthaelt — `fwrite`, `popen`, `system`,
`libusb_*_transfer`, `QProcess`, `write(`. Ein Stub, der nur
`UFT_ERR_NOT_IMPLEMENTED` liefert, ist keine widerlegte Absage, sondern
eine gedeckte.

── Was es NICHT sehen kann ──────────────────────────────────────────────

* Es entscheidet **nicht**, ob das Geraet schreiben kann. Das waere eine
  Messung am Geraet (es gibt keines, MF-310) oder am echten DTC (nicht im
  Baum; `tools/hw_simulators/` haelt nur Simulatoren). Es misst nur, ob
  der Baum seiner eigenen Absage widerspricht.
* Es prueft **nicht** die Erreichbarkeit ueber Dateigrenzen hinweg — es
  fragt, ob ein Schreiber **existiert**, nicht ob er ankommt. Die
  Erreichbarkeitsfrage stellt Tor 57 fuer die Format-Schicht.
* Zeilen mit `✅`, `🟡` oder `⬜` in der Write-Spalte laesst es ganz in
  Ruhe: die behaupten keine Protokollgrenze.
* Ist git nicht befragbar, laesst es alles durch **und sagt es**.
"""
import os
import re
import subprocess
import sys

TAFEL = os.path.join("docs", "CAPABILITIES.md")

# Eine Zeile der Controller-Tabelle: | Name | Read | Write | ...
ZEILE = re.compile(r"^\|\s*([A-Za-z0-9][A-Za-z0-9 _/-]*?)\s*\|"
                   r"\s*([^|]*?)\s*\|"
                   r"\s*([^|]*?)\s*\|")

# Das Zeichen, das eine Protokollgrenze behauptet.
ABSAGE = "-"

# Eine Ausgabehandlung — daran erkennt man einen Schreiber, der TUT.
TAT = re.compile(r"\b(fwrite|popen|system|libusb_[a-z_]*transfer|"
                 r"QProcess|_write|write)\s*\(")

# Der Vorspann ist NICHT gierig und muss auf einem Trenner enden — sonst
# frisst er den Namensanfang, und der Fund heisst `_write_track` statt
# `uft_kf_write_track`. Ein Instrument, das seinen eigenen Fund falsch
# benennt, ist genau die Klasse, die dieser Baum jagt (gemessen im ersten
# Lauf von MF-1045).
KOPF = re.compile(r"^[A-Za-z_][A-Za-z0-9_ \*\t]*?[ \t\*]"
                  r"([A-Za-z_][A-Za-z0-9_]*write[A-Za-z0-9_]*)\s*\([^;]*?\)\s*\{",
                  re.M | re.S | re.I)

SCHLUESSEL = {"if", "for", "while", "switch", "do", "else", "return",
              "sizeof", "case"}


def _git(repo, *args):
    try:
        r = subprocess.run(["git"] + list(args), cwd=str(repo),
                           capture_output=True, text=True)
        if r.returncode != 0:
            return None
        return r.stdout
    except OSError:
        return None


def controller_mit_absage(repo):
    """(Name, Kuerzel) je Controller, dessen Write-Spalte `-` traegt."""
    pfad = os.path.join(str(repo), TAFEL)
    try:
        with open(pfad, "r", encoding="utf-8", errors="replace") as f:
            text = f.read()
    except OSError:
        return None
    treffer = []
    for roh in text.splitlines():
        m = ZEILE.match(roh)
        if not m:
            continue
        name, lesen, schreiben = m.group(1), m.group(2), m.group(3)
        if name.lower() in ("controller", "---"):
            continue
        if set(name) <= set("-: "):
            continue
        # Nur echte Controller-Zeilen: die Read-Spalte traegt eine Marke.
        if not lesen.strip():
            continue
        if schreiben.strip() != ABSAGE:
            continue
        kuerzel = re.sub(r"[^a-z0-9]", "", name.lower())
        treffer.append((name.strip(), kuerzel))
    return treffer


def dateien_zu(repo, kuerzel, alle):
    """Quelldateien, deren Dateiname das Controller-Kuerzel traegt."""
    raus = []
    for rel in alle:
        if not rel.endswith((".c", ".cpp")):
            continue
        flach = re.sub(r"[^a-z0-9]", "", os.path.basename(rel).lower())
        if kuerzel and kuerzel in flach:
            raus.append(rel)
    return raus


def schreiber_in(pfad):
    """Namen der Funktionen, die schreiben HEISSEN und schreiben TUN."""
    try:
        with open(pfad, "r", encoding="utf-8", errors="replace") as f:
            text = f.read()
    except OSError:
        return []
    raus = []
    for m in KOPF.finditer(text):
        name = m.group(1)
        if name.lower() in SCHLUESSEL or len(name) < 4:
            continue
        i = text.index("{", m.start())
        tiefe, j = 0, i
        while j < len(text):
            if text[j] == "{":
                tiefe += 1
            elif text[j] == "}":
                tiefe -= 1
                if tiefe == 0:
                    break
            j += 1
        rumpf = text[i:j + 1]
        if TAT.search(rumpf):
            raus.append((name, rumpf.count("\n") + 1))
    return raus


def messe(repo=None):
    if repo is None:
        repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    aus = _git(repo, "ls-files")
    if aus is None:
        return None, None
    alle = [z.strip() for z in aus.splitlines() if z.strip()]
    ctrl = controller_mit_absage(repo)
    if ctrl is None:
        return None, None
    befunde = []
    for name, kuerzel in ctrl:
        for rel in dateien_zu(repo, kuerzel, alle):
            for fn, zeilen in schreiber_in(os.path.join(str(repo), rel)):
                befunde.append((name, rel, fn, zeilen))
    return ctrl, befunde


def check(repo):
    """Einhaengepunkt fuer scripts/check_consistency.py."""
    ctrl, befunde = messe(repo)
    if ctrl is None:
        return ["git oder docs/CAPABILITIES.md ist nicht lesbar — dieses "
                "Tor kann nichts sagen und laesst alles durch"]
    fehler = []
    for name, rel, fn, zeilen in befunde:
        fehler.append(
            "%s traegt in docs/CAPABILITIES.md ein `-` in der Write-Spalte "
            "(Legende: „protokoll-bedingt nicht vorgesehen\") — aber %s:%s "
            "ist ein ausgefuehrter Schreiber von %d Zeilen. Die Absage "
            "behauptet etwas ueber das GERAET, das der eigene Baum "
            "bestreitet (MF-1045). Abhilfe: entweder die Zeile berichtigen "
            "(die Faehigkeit fehlt in UFT, nicht im Geraet) oder den "
            "Schreiber entfernen."
            % (name, rel, fn, zeilen))
    return fehler


def selbsttest():
    """Die Regel an synthetischer Eingabe, ohne git und ohne Baum."""
    import tempfile
    faelle = [
        # (Quelltext, erwartete Zahl ausgefuehrter Schreiber)
        ("int uft_x_write_track(void* c) {\n  FILE* f = fopen(p,\"wb\");\n"
         "  fwrite(d,1,n,f);\n  return 0;\n}\n", 1),
        # Stub: heisst schreiben, tut nichts -> keine widerlegte Absage
        ("int uft_x_write_track(void* c) {\n"
         "  return UFT_ERR_NOT_IMPLEMENTED;\n}\n", 0),
        # popen zaehlt als Tat
        ("int uft_x_write_disk(void* c) {\n  FILE* p = popen(cmd,\"r\");\n"
         "  pclose(p);\n  return 0;\n}\n", 1),
        # Funktion ohne 'write' im Namen wird nicht betrachtet
        ("int uft_x_read_track(void* c) {\n  fwrite(d,1,n,f);\n"
         "  return 0;\n}\n", 0),
        # zwei Schreiber in einer Datei
        ("int uft_x_write_a(void* c) {\n  fwrite(d,1,n,f);\n  return 0;\n}\n"
         "int uft_x_write_b(void* c) {\n  system(cmd);\n  return 0;\n}\n", 2),
    ]
    gut = 0
    for text, erwartet in faelle:
        fd, pfad = tempfile.mkstemp(suffix=".c")
        os.close(fd)
        with open(pfad, "w", encoding="utf-8") as f:
            f.write(text)
        ist = len(schreiber_in(pfad))
        os.unlink(pfad)
        if ist == erwartet:
            gut += 1
        else:
            print("  FEHLER: erwartet %d, gemessen %d fuer:\n%s"
                  % (erwartet, ist, text))
    print("Selbsttest: %d/%d" % (gut, len(faelle)))
    return 0 if gut == len(faelle) else 1


def main():
    sys.stdout.reconfigure(encoding="utf-8")
    if "--selftest" in sys.argv:
        return selbsttest()

    repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    ctrl, befunde = messe(repo)

    print("Faehigkeitsabsage gedeckt? (MF-1045)")
    if ctrl is None:
        print("  git/Tafel nicht lesbar — nichts geprueft.")
        print("\nOK")
        return 0
    print("  Controller mit `-` in der Write-Spalte : %3d" % len(ctrl))
    for name, kuerzel in ctrl:
        print("      %s" % name)
    print("  davon vom eigenen Baum widerlegt       : %3d" % len(befunde))
    for name, rel, fn, zeilen in befunde:
        print("      %-12s %s:%s (%d Zeilen)" % (name, rel, fn, zeilen))

    if befunde:
        print("\n  Eine Absage, die eine Protokollgrenze behauptet, waehrend")
        print("  der eigene Baum einen ausgefuehrten Schreiber fuehrt, sagt")
        print("  etwas ueber die Hardware, das sie nie gemessen hat.")
        print("\nFAIL")
        return 1
    print("\nOK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
