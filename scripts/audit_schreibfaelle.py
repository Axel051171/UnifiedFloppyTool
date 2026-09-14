#!/usr/bin/env python3
"""Tor: eine Schreibzusage braucht einen Durchschreibfall (A7).

Auf Eigentuemer-Anweisung („Registry gegen diese Tabelle pruefen …
Fehlendes CAP_WRITE bzw. falsche Zusagen automatisch melden"). Der
T1b-Audit hat die Lage gemessen: Plugins fuehren
`UFT_FORMAT_CAP_WRITE` in ihrem Feld `.capabilities`, und nur eine
Handvoll hat einen Durchschreib- oder Persistenzfall.

── Was ein Durchschreibfall beweist, und was nicht ───────────────────────

`tests/test_capability_manifest.c` prueft `plugin->write_track != NULL`.
Das belegt, dass ein Funktionszeiger existiert — nicht:

  * dass `write_track()` Erfolg oder Fehler richtig meldet,
  * dass die Aenderung die DATEI erreicht,
  * dass sie nach `close()` und erneutem `open()` noch da ist,
  * dass ein fremdes Programm das Ergebnis annimmt,
  * dass bei einem Fehler das Original unbeschaedigt bleibt.

Genau das ist P3-154. Dieses Tor schliesst nur die erste Luecke: es
verlangt, dass es fuer jede Zusage UEBERHAUPT einen Fall gibt.

── Warum eine GRUNDLINIE und nicht sofort rot ────────────────────────────

Alle offenen Zusagen auf einmal rot zu faerben wuerde jede Arbeit
blockieren, und ein Tor, das man abschalten muss, um zu arbeiten, wird
abgeschaltet. Die Grundlinie haelt den heutigen Stand fest; rot wird
nur, was NEU hinzukommt oder was aus der Grundlinie verschwindet, ohne
einen Fall bekommen zu haben.

Das ist dieselbe Bauform wie `scripts/audit_schreibzusage.py` (Tor 57,
Grundlinie 0) und `verify_build_sources.py`.

── Wie gemessen wird ─────────────────────────────────────────────────────

Zusage:  das `.capabilities`-Feld der Plugin-Tafel enthaelt
         `UFT_FORMAT_CAP_WRITE`. Gelesen wird der Tafelrumpf bis zum
         ECHTEN Ende, nicht in einem festen Fenster — MF-1077 hat
         gemessen, dass ein 1200-Zeichen-Fenster fuenf Tafeln
         abschneidet.

         Und ausdruecklich NICHT per Datei-grep: `src/formats/mgt/
         uft_mgt.c` traegt einen MF-1006-Kommentar, der
         `UFT_FORMAT_CAP_WRITE` NENNT, neben dem Feld, das es setzt. Ein
         Datei-grep kann eine Zusage sehen, die im Feld nicht steht.

Fall:    eine Testdatei, deren Name eine Durchschreib-/Persistenz-/
         Rundlaufprobe nennt, erwaehnt `&uft_format_plugin_<name>`.
         Die Dateimenge kommt aus dem Verzeichnis, nicht aus einer
         gepflegten Liste (MF-636).

Beides ist absichtlich grob: dieses Tor fragt „gibt es einen Fall", nicht
„ist der Fall gut". Die Stufenleiter W0-W4 dafuer ist der naechste
Schritt.
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

HIER = Path(__file__).resolve().parent

PLUGIN_RE = re.compile(
    r"\b(?:const\s+)?uft_format_plugin_t\s+(uft_format_plugin_[a-z0-9_]+)\s*="
    r"\s*\{")

FALLDATEI_WORTE = ("durchschreib", "schreibt_in_die_datei", "persist",
                   "roundtrip", "rundlauf")


def _feldwert(body: str, feld: str) -> str | None:
    """Der Wert eines Feldes einer Struktur-Initialisierung.

    MF-1134: die erste Fassung suchte `\\.capabilities\\s*=([^;]*);` —
    also bis zum SEMIKOLON. In einem Struktur-Initialisierer endet ein
    Feld aber mit einem KOMMA; das einzige Semikolon steht hinter dem
    schliessenden `};`, und der Rumpf wird davor abgeschnitten. Der
    Ausdruck traf deshalb fast nie.

    Gemessen war die Folge eine Zahl, die nach etwas aussah: das Tor
    meldete **7** Plugins mit `CAP_WRITE`, waehrend ein Datei-grep 62
    findet. Gefangen hat es der eigene Selbsttest (4/6), nicht der Lauf —
    ein Lauf ohne rote Zusage haette die 7 weitergegeben.

    Jetzt zeilenweise: ab der Zeile mit dem Feld sammeln, bis eine Zeile
    ein NEUES Feld beginnt (`.name = …`) oder der Wert mit einem Komma
    endet, das nicht in einer Fortsetzung steht.
    """
    zeilen = body.split("\n")
    anfang = None
    for i, z in enumerate(zeilen):
        if re.match(r"\s*\." + re.escape(feld) + r"\s*=", z):
            anfang = i
            break
    if anfang is None:
        return None

    teile = [zeilen[anfang].split("=", 1)[1]]
    if teile[0].rstrip().endswith(","):
        return teile[0]
    for z in zeilen[anfang + 1:]:
        if re.match(r"\s*\.[A-Za-z_]\w*\s*=", z):      # naechstes Feld
            break
        teile.append(z)
        if z.rstrip().endswith(","):
            break
    return "\n".join(teile)


def _cap_write(text: str, m: re.Match) -> bool:
    ende = text.find("\n};", m.end())
    body = text[m.end():ende if ende > 0 else len(text)]
    wert = _feldwert(body, "capabilities")
    if wert is None:
        return False
    # Kommentare im Wert entfernen: ein Kommentar, der CAP_WRITE NENNT,
    # ist keine Zusage (gemessen an src/formats/mgt/uft_mgt.c, wo ein
    # MF-1006-Kommentar es neben dem Feld nennt).
    wert = re.sub(r"/\*.*?\*/|//[^\n]*", " ", wert, flags=re.S)
    return "UFT_FORMAT_CAP_WRITE" in wert


def zusagen(repo: Path) -> dict[str, str]:
    """Plugins mit `UFT_FORMAT_CAP_WRITE` im Feld `.capabilities`."""
    aus: dict[str, str] = {}
    for f in sorted((repo / "src" / "formats").rglob("*.c")):
        text = f.read_text(encoding="utf-8", errors="replace")
        for m in PLUGIN_RE.finditer(text):
            if _cap_write(text, m):
                kurz = m.group(1).replace("uft_format_plugin_", "")
                aus[kurz] = str(f.relative_to(repo)).replace("\\", "/")
    return aus


def faelle(repo: Path) -> dict[str, set[str]]:
    """Plugins mit einem Durchschreib-/Persistenzfall, je Testdatei."""
    aus: dict[str, set[str]] = {}
    for f in sorted((repo / "tests").glob("test_*.c")):
        n = f.name.lower()
        if not any(w in n for w in FALLDATEI_WORTE):
            continue
        text = f.read_text(encoding="utf-8", errors="replace")
        for m in re.finditer(r"&\s*uft_format_plugin_([a-z0-9_]+)", text):
            aus.setdefault(m.group(1), set()).add(f.name)
    return aus


def grundlinie_lesen(p: Path) -> set[str]:
    if not p.exists():
        return set()
    aus = set()
    for z in p.read_text(encoding="utf-8").splitlines():
        z = z.split("#", 1)[0].strip()
        if z:
            aus.add(z)
    return aus


def selbsttest() -> int:
    """Jeder Erkenner muss ROT werden koennen — und still bleiben.

    Die Formen hier stehen, weil der T1b-Audit eine Lehre geliefert hat:
    seine erste Fassung meldete 27 Speicher-Kandidaten, und ALLE 27
    waren Falschmeldungen. Ein Selbsttest, der nur die einfachste Form
    prueft, deckt nichts.
    """
    f = []

    def cap(body_text: str) -> bool:
        t = ("const uft_format_plugin_t uft_format_plugin_x = {\n"
             + body_text + "\n};\n")
        m = PLUGIN_RE.search(t)
        assert m
        return _cap_write(t, m)

    f.append(("Zusage erkannt",
              cap("    .capabilities = UFT_FORMAT_CAP_READ | "
                  "UFT_FORMAT_CAP_WRITE,"), True))
    f.append(("nur Lesen -> keine Zusage",
              cap("    .capabilities = UFT_FORMAT_CAP_READ,"), False))
    f.append(("Zusage ueber mehrere Zeilen erkannt",
              cap("    .capabilities = UFT_FORMAT_CAP_READ\n"
                  "                  | UFT_FORMAT_CAP_WRITE,"), True))
    # Der scharfe Fall: `src/formats/mgt/uft_mgt.c` traegt einen
    # MF-1006-Kommentar, der CAP_WRITE nennt, NEBEN dem Feld. Ein
    # Datei-grep kann dort eine Zusage sehen, die im Feld nicht steht.
    f.append(("CAP_WRITE im KOMMENTAR ist keine Zusage",
              cap("    /* UFT_FORMAT_CAP_WRITE entfernt, MF-930 */\n"
                  "    .capabilities = UFT_FORMAT_CAP_READ,"), False))

    def fall(dateiname: str, inhalt: str) -> bool:
        n = dateiname.lower()
        if not any(w in n for w in FALLDATEI_WORTE):
            return False
        return bool(re.search(r"&\s*uft_format_plugin_([a-z0-9_]+)", inhalt))

    f.append(("Fall in einer Durchschreibprobe erkannt",
              fall("test_durchschreibprobe.c",
                   '{ "po", &uft_format_plugin_po, "po", 35, 1, 16, 256 },'),
              True))
    f.append(("dieselbe Nennung in einem GEWOEHNLICHEN Test zaehlt nicht",
              fall("test_po_layout.c",
                   "rc = uft_format_plugin_po.open(&d, p, true);"), False))

    gruen = 0
    for name, ist, soll in f:
        ok = (bool(ist) == soll)
        gruen += ok
        print("  %s %-52s erwartet=%-5s ist=%s"
              % ("[ok ]" if ok else "[ROT]", name, soll, bool(ist)))
    print("\n  Selbsttest %d/%d" % (gruen, len(f)))
    return 0 if gruen == len(f) else 1


def check(repo) -> list:
    """Schnittstelle fuer `scripts/check_consistency.py`.

    Meldet nur, was NEU ist: eine Schreibzusage ohne Durchschreibfall,
    die nicht in der Grundlinie steht. Der Rueckstand selbst ist kein
    Befund — er ist benannte Schuld (P3-154).
    """
    repo = Path(repo)
    gl_pfad = repo / "docs" / "schreibfaelle_baseline.txt"
    z = zusagen(repo)
    fl = faelle(repo)
    ohne = set(z) - set(fl)
    gl = grundlinie_lesen(gl_pfad)

    fehler = []
    for n in sorted(ohne - gl):
        fehler.append(
            "%s sagt UFT_FORMAT_CAP_WRITE zu, hat aber keinen "
            "Durchschreibfall (%s) — entweder einen Fall ergaenzen oder "
            "die Zusage zuruecknehmen. `test_capability_manifest.c` "
            "prueft nur, dass write_track != NULL ist (P3-154)."
            % (n, z[n]))
    for n in sorted(gl - ohne):
        fehler.append(
            "%s steht in docs/schreibfaelle_baseline.txt, hat aber "
            "inzwischen einen Durchschreibfall — bitte die Zeile "
            "herausnehmen, sonst deckt die Grundlinie mehr, als sie "
            "muss." % n)
    return fehler


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=None)
    ap.add_argument("--selbsttest", action="store_true")
    ap.add_argument("--grundlinie", default=None)
    ap.add_argument("--schreibe-grundlinie", action="store_true")
    a = ap.parse_args()

    if a.selbsttest:
        print("Schreibfall-Tor — Selbsttest\n")
        return selbsttest()

    repo = Path(a.repo).resolve() if a.repo else HIER.parent
    if not (repo / "src" / "formats").is_dir():
        print("FEHLER: --repo <wurzel> angeben", file=sys.stderr)
        return 2

    gl_pfad = Path(a.grundlinie) if a.grundlinie else \
        repo / "docs" / "schreibfaelle_baseline.txt"

    z = zusagen(repo)
    fl = faelle(repo)
    ohne = sorted(set(z) - set(fl))
    mit = sorted(set(z) & set(fl))
    fall_ohne_zusage = sorted(set(fl) - set(z))

    print("Schreibfall-Tor (A7)\n")
    print("  Plugins mit CAP_WRITE im Feld .capabilities : %d" % len(z))
    print("  davon MIT Durchschreibfall                  : %d" % len(mit))
    print("  davon OHNE                                  : %d" % len(ohne))
    print("  Faelle ohne Zusage                          : %d"
          % len(fall_ohne_zusage))

    if mit:
        print("\n  MIT Fall: " + ", ".join(mit))
    if fall_ohne_zusage:
        print("\n  Fall ohne Zusage (durchgeschrieben getestet, aber keine")
        print("  CAP_WRITE-Zusage im Feld): " + ", ".join(fall_ohne_zusage))

    if a.schreibe_grundlinie:
        kopf = (
            "# Grundlinie: Plugins mit UFT_FORMAT_CAP_WRITE, die (noch)\n"
            "# keinen Durchschreibfall haben. Erzeugt von\n"
            "# scripts/audit_schreibfaelle.py --schreibe-grundlinie.\n"
            "#\n"
            "# Eine Zeile hier ist eine OFFENE SCHULD, kein Freibrief:\n"
            "# test_capability_manifest.c prueft nur, dass\n"
            "# plugin->write_track != NULL ist — nicht, dass die\n"
            "# Aenderung die Datei erreicht (P3-154). Wer einen Fall\n"
            "# ergaenzt, nimmt die Zeile heraus.\n"
            "#\n"
            "# NEUE Zusagen ohne Fall faerben das Tor rot.\n")
        gl_pfad.write_text(kopf + "\n".join(ohne) + "\n", encoding="utf-8")
        print("\n  Grundlinie geschrieben: %s (%d Zeilen)"
              % (gl_pfad, len(ohne)))
        return 0

    gl = grundlinie_lesen(gl_pfad)
    neu = sorted(set(ohne) - gl)
    erledigt = sorted(gl - set(ohne))

    print("\n  Grundlinie     : %s"
          % (gl_pfad if gl_pfad.exists() else "(fehlt)"))
    print("  akzeptiert     : %d" % len(gl))
    print("  NEU ohne Fall  : %d" % len(neu))
    print("  erledigt       : %d" % len(erledigt))

    if erledigt:
        print("\n  Erledigt — bitte aus der Grundlinie nehmen: "
              + ", ".join(erledigt))

    if neu:
        print("\nFAIL: %d neue Schreibzusage(n) ohne Durchschreibfall:"
              % len(neu))
        for n in neu:
            print("   %-16s %s" % (n, z[n]))
        print("\n  Entweder einen Fall ergaenzen (bevorzugt) oder die")
        print("  Zusage zuruecknehmen. Die Grundlinie zu erweitern ist")
        print("  die dritte Wahl und braucht einen Grund im Commit.")
        return 1

    print("\nOK: keine neue Schreibzusage ohne Durchschreibfall.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
