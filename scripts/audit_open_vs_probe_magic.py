#!/usr/bin/env python3
"""Wer beim Erkennen ein Magic prueft, muss es beim Oeffnen auch pruefen
(MF-688).

    python scripts/audit_open_vs_probe_magic.py [--liste]

── Warum es diese Messung gibt ──────────────────────────────────────────

`dim_atari` prueft seit MF-687 in der Probe ein Magic (`0x4242`) — und
prueft es in `open()` nicht. Das ist nicht bloss unsauber, es ist ein
Datenverlust-Vektor:

  * `open()` oeffnet "r+b", wenn der Aufrufer schreiben will.
  * Wer das Plugin unmittelbar waehlt — ausdrueckliche Formatwahl, ein
    Fuzzer, ein Aufrufer an der Registry vorbei — bekommt fuer JEDE
    Datei passender Laenge ein Schreibziel.
  * `write_track()` schreibt dann hinein.

Gemessen (tests/test_dim_atari_magic.c): eine Fremddatei mit gueltigem
32-Byte-Kopf ohne Magic wurde mit rc=0 geoeffnet und kam veraendert
zurueck.

── Warum es NICHT "hat dieses Plugin ein Magic" fragt ───────────────────

Weil die naheliegende Frage die falsche ist. Von 81 Plugins mit
Schreibpfad sind viele **kopflos**: ADF, D64, IMG, XFD und Verwandte
sind rohe Sektorabbilder, die allein an der Groesse erkannt werden. Sie
KOENNEN kein Magic pruefen; das als Befund zu melden waere ein Tor, das
70-mal falsch anschlaegt und darum uebergangen wird.

Gefragt wird also die engere, beantwortbare Frage:

    **Prueft die Probe ein Magic, das `open()` nicht prueft?**

Ein Plugin, das beim ERKENNEN auf feste Bytes besteht, hat damit
erklaert, dass es sie fuer wesentlich haelt. Beim OEFFNEN darauf zu
verzichten ist ein Widerspruch in sich — und genau der Widerspruch, der
oben Daten kostet.

── Was diese Messung nicht sehen kann ──────────────────────────────────

Ehrlich benannt, damit eine Null hier nicht als Entwarnung gelesen wird:

  * Ein Magic, das ueber einen Hilfsaufruf geprueft wird
    (`ist_gueltig(hdr)`), statt im Funktionskoerper.
  * Ein `memcmp` gegen eine Konstante, die anderswo definiert ist.
  * Kopflose Formate, deren Schreibpfad aus anderen Gruenden gefaehrlich
    ist — die Groessenpruefung allein ist duenn, aber das ist eine
    andere Frage und braucht eine andere Messung.

Ein Treffer hier ist darum ein **Befund zum Nachsehen**, kein Urteil.
Eine leere Liste heisst „dieses Muster kommt nicht mehr vor", nicht
„alle Schreibpfade sind sicher".
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

WURZEL = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(WURZEL / "scripts"))

from repo_scope import make_filter  # noqa: E402

# Ein Magic-TOR, nicht ein Magic-Hinweis.
#
# Der erste Entwurf suchte irgendeinen Byte-Vergleich gegen eine
# Konstante und meldete zehn Befunde. Von Hand nachgesehen waren
# mindestens zwei falsch: `img_probe` prueft `data[0] == 0xEB` und
# `data[510] == 0x55`, aber nur um die KONFIDENZ zu erhoehen — erkannt
# wird das Format ueber die Dateigroesse. Dasselbe bei `d64_plugin_probe`.
#
# Der Unterschied ist genau der, auf den es ankommt: ein Vergleich, der
# bei Nichtuebereinstimmung ABLEHNT, ist ein Magic. Einer, der eine Zahl
# hochzaehlt, ist ein Hinweis — und ein Hinweis darf im `open()` fehlen,
# ohne dass etwas falsch ist.
#
# Gesucht wird darum der Vergleich ZUSAMMEN mit seiner Folge: ein
# `return false` (oder ein Fehler-Return) im selben Bedingungsblock,
# innerhalb weniger Zeilen.
#
# Zweiter Korrekturgang: der Rueckgabewert-Teil kannte nur `UFT_ERROR`,
# nicht `UFT_ERR_`. Damit galt `cqm_open` als pruefungslos, obwohl es
# `h[0] != 'C' || h[1] != 'Q' || h[2] != 0x14` prueft und
# `UFT_ERR_FORMAT_INVALID` zurueckgibt. Von Hand nachgesehen, nicht dem
# Skript geglaubt — der Praefix ist jetzt `UFT_ERR`.
MAGIC = re.compile(
    r"(?:\w+)\s*\[\s*(?:0x)?[0-9A-Fa-f]+\s*\]\s*(?:!=|==)\s*"
    r"(?:0x[0-9A-Fa-f]{2}|'[^']')"
    r"[^;{}]{0,200}?\)\s*(?:\{[^{}]{0,120}?)?return\s+(?:false|UFT_ERR|-)"
    r"|memcmp\s*\(\s*\w+\s*,\s*\"[^\"]+\"[^;{}]{0,80}?\)\s*"
    r"(?:!=|==)\s*0[^;{}]{0,60}?\)\s*(?:\{[^{}]{0,120}?)?return\s+"
    r"(?:false|UFT_ERROR|-)",
    re.S)

# Feld -> Grund. Wer hier eintraegt, nennt auch das Ende.
ERLAUBT: dict[str, str] = {}


def koerper(text: str, name: str) -> str | None:
    """Der Funktionskoerper von @p name, ueber Klammerzaehlung."""
    m = re.search(r"^[A-Za-z_][\w \t\*]*\b" + re.escape(name) + r"\s*\(",
                  text, re.M)
    if not m:
        return None
    auf = text.find("{", m.end())
    if auf < 0:
        return None
    tiefe = 0
    for i in range(auf, len(text)):
        if text[i] == "{":
            tiefe += 1
        elif text[i] == "}":
            tiefe -= 1
            if tiefe == 0:
                return text[auf + 1:i]
    return None


def pruefe_datei(pfad: str, text: str) -> str | None:
    """Ein Befund, oder None."""
    m_probe = re.search(r"\.probe\s*=\s*(\w+)", text)
    m_open = re.search(r"\.open\s*=\s*(\w+)", text)
    m_write = re.search(r"\.write_track\s*=\s*(\w+)", text)
    if not (m_probe and m_open and m_write):
        return None            # ohne Schreibpfad ist die Frage gegenstandslos
    if m_write.group(1) == "NULL":
        return None            # kann nicht schreiben, also nichts zu verlieren

    k_probe = koerper(text, m_probe.group(1))
    k_open = koerper(text, m_open.group(1))
    if k_probe is None or k_open is None:
        return None

    if not MAGIC.search(k_probe):
        return None            # kopflos oder Magic anderswo — nicht dieses Muster
    if MAGIC.search(k_open):
        return None            # beides prueft, in Ordnung

    return (f"{pfad}: `{m_probe.group(1)}` prueft ein Magic, `{m_open.group(1)}` "
            f"nicht — und `{m_write.group(1)}` schreibt. Eine Fremddatei "
            f"passender Laenge wird zum Schreibziel (MF-688).")


def sammle(repo: Path):
    behalten, warnung = make_filter(repo)
    if warnung:
        print("HINWEIS: " + warnung)
    aus = []
    for p in sorted((repo / "src" / "formats").rglob("*.c")):
        if not behalten(p):
            continue
        try:
            aus.append((p.relative_to(repo).as_posix(),
                        p.read_text(encoding="utf-8", errors="replace")))
        except OSError:
            pass
    return aus


def check(repo: Path) -> list[str]:
    befunde = []
    for pfad, text in sammle(repo):
        if pfad in ERLAUBT:
            continue
        b = pruefe_datei(pfad, text)
        if b:
            befunde.append(b)
    return befunde


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--liste", action="store_true",
                    help="auch die unauffaelligen Plugins zeigen")
    a = ap.parse_args()

    dateien = sammle(WURZEL)
    mit_schreibpfad = 0
    mit_magic = 0
    befunde = []
    for pfad, text in dateien:
        if not re.search(r"\.write_track\s*=", text):
            continue
        mit_schreibpfad += 1
        m_probe = re.search(r"\.probe\s*=\s*(\w+)", text)
        k = koerper(text, m_probe.group(1)) if m_probe else None
        if k and MAGIC.search(k):
            mit_magic += 1
        b = pruefe_datei(pfad, text)
        if b:
            befunde.append(b)
        elif a.liste and k and MAGIC.search(k):
            print("  ok   " + pfad)

    print("=" * 70)
    print(f"{len(dateien)} Plugin-Quellen, {mit_schreibpfad} mit Schreibpfad.")
    print(f"{mit_magic} davon pruefen in der Probe ein Magic an fester "
          f"Stelle;")
    print(f"die uebrigen sind kopflos oder pruefen anders — beides ist "
          f"kein Befund.")
    if not befunde:
        print("\nProbe prueft, open() nicht: 0")
        return 0
    print(f"\nROT: {len(befunde)}")
    for b in befunde:
        print("    " + b)
    print("""
Wer beim Erkennen auf feste Bytes besteht, haelt sie fuer wesentlich.
Beim Oeffnen darauf zu verzichten ist ein Widerspruch — und einer, der
Daten kostet: `open()` liefert ein Schreibziel, `write_track()` schreibt
hinein.
""")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
