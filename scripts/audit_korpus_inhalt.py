#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Tor 59: das Korpus-Manifest sagt, WAS auf einem Abbild steht — und die
Angabe wird gegen die Datei gehalten (MF-929, P3-197).

── DER ANLASS ────────────────────────────────────────────────────────

`tests/corpus_manifest/manifest.json` fuehrt je Abbild Herkunft, Hash,
Werkzeug und Format. Es fuehrt NICHT, was darauf steht.

Das ist kein Schoenheitsfehler. Gemessen (MF-923): `vice_c1541_35trk.d64`
traegt den Diskettennamen `UFTCORPUS`, `vice_c1541_35trk.g64` traegt
`UFTG64` — gleicher Namensstamm, ZWEI VERSCHIEDENE DISKETTEN. Beim Bau
eines Rotbeweises wurde daraus eine Zusicherung, die den einen BAM im
anderen Abbild erwartete. Sie fiel zu Recht, aber aus dem falschen
Grund, und die Fehlersuche kostete drei Runden.

Wer eine Korpusdatei als Vergleich heranzieht, muss wissen, was darauf
steht — sonst baut er eine Zusicherung auf eine Vermutung.

── WAS DIESES TOR PRUEFT, UND WAS AUSDRUECKLICH NICHT ────────────────

Geprueft wird das Feld `content` gegen die Datei selbst, fuer die zwei
Formate, deren Inhaltskennung an einer DOKUMENTIERTEN, festen Stelle
steht:

    D64   Diskettenname und ID im BAM, Spur 18 Sektor 0.
          Versatz = (Summe der Sektoren der Spuren 1..17) * 256
                  = 17 * 21 * 256 = 91392 (0x16500), Name bei +0x90,
          ID bei +0xA2. Das ist EINE Zahl aus der Zonenaufteilung,
          kein nachgebauter Leser.
    G64   Kopfbyte 9 (Zahl der Halbspur-Eintraege) und die Zahl der
          belegten Eintraege in der Offset-Tabelle ab Versatz 12.

Fuer alle anderen Formate wird `content` NICHT geprueft — dort waere
eine Pruefung ein zweiter Leser neben dem des Baums, und genau das ist
die Fehlerklasse, die dieser Baum sonst jagt (zwei Umsetzungen, eine
driftet). Solche Eintraege duerfen ein `content` tragen; das Tor sagt
dann ausdruecklich „nicht nachgeprueft".

Ein Abbild OHNE `content` ist kein Fehler, sondern Rueckstand. Die Zahl
darf sinken, nicht steigen.
"""
import io
import json
import os
import sys

WURZEL = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MANIFEST = os.path.join(WURZEL, "tests", "corpus_manifest", "manifest.json")

# Grundlinie: so viele Abbilder tragen (noch) keine Inhaltsangabe.
# Sie darf SINKEN, nicht steigen.
OHNE_INHALT_MAX = 45

D64_BAM = 357 * 256           # Spur 18 Sektor 0
D64_NAME = D64_BAM + 0x90
D64_ID = D64_BAM + 0xA2


def lies(pfad):
    try:
        return io.open(pfad, "rb").read()
    except OSError:
        return None


def pruefe_d64(roh, behauptet):
    """(ok, gemessen) — None wenn die Datei zu kurz ist."""
    if len(roh) < D64_ID + 2:
        return None, "Datei zu kurz fuer einen BAM"
    name = roh[D64_NAME:D64_NAME + 16].rstrip(b"\xa0").decode("latin-1")
    kid = roh[D64_ID:D64_ID + 2].decode("latin-1")
    gemessen = "%s,%s" % (name, kid)
    return (behauptet == gemessen), gemessen


def pruefe_g64(roh, behauptet):
    if len(roh) < 12 + 84 * 4:
        return None, "Datei zu kurz fuer die Offset-Tabelle"
    halbspuren = roh[9]
    belegt = 0
    for i in range(halbspuren):
        o = 12 + i * 4
        if o + 4 > len(roh):
            break
        v = roh[o] | (roh[o + 1] << 8) | (roh[o + 2] << 16) | (roh[o + 3] << 24)
        if v:
            belegt += 1
    gemessen = "%d Halbspur-Eintraege, %d belegt" % (halbspuren, belegt)
    return (behauptet == gemessen), gemessen


PRUEFER = {"d64": pruefe_d64, "g64": pruefe_g64}


def messe(repo=None):
    """(bilder, ohne, geprueft, falsch, ungeprueft, fehlend)"""
    repo = repo or WURZEL
    manifest = os.path.join(repo, "tests", "corpus_manifest", "manifest.json")
    daten = json.load(io.open(manifest, encoding="utf-8"))
    bilder = daten.get("images", [])

    ohne, geprueft, falsch, ungeprueft, fehlend = [], 0, [], 0, 0

    for e in bilder:
        rel = e.get("file", "")
        fmt = (e.get("format") or "").lower()
        inhalt = e.get("content")
        pfad = os.path.join(repo, rel)

        if not inhalt:
            ohne.append(rel)
            continue

        roh = lies(pfad)
        if roh is None:
            fehlend += 1          # gitignored oder nicht beschafft — kein Fehler
            continue

        p = PRUEFER.get(fmt)
        if not p:
            ungeprueft += 1
            continue

        ok, gemessen = p(roh, inhalt)
        if ok is None:
            falsch.append((rel, inhalt, gemessen))
        elif ok:
            geprueft += 1
        else:
            falsch.append((rel, inhalt, gemessen))

    return bilder, ohne, geprueft, falsch, ungeprueft, fehlend


def check(repo):
    """Einhaengepunkt fuer scripts/check_consistency.py."""
    _, ohne, _, falsch, _, _ = messe(repo)
    fehler = []
    for rel, b, g in falsch:
        fehler.append("%s: Manifest sagt %r, die Datei sagt %r"
                      % (rel, b, g))
    if len(ohne) > OHNE_INHALT_MAX:
        fehler.append(
            "%d Korpus-Abbilder ohne Inhaltsangabe, Grundlinie ist %d — "
            "ein neues Abbild bekommt eine, sonst baut die naechste "
            "Zusicherung wieder auf eine Vermutung"
            % (len(ohne), OHNE_INHALT_MAX))
    return fehler


def main():
    sys.stdout.reconfigure(encoding="utf-8")
    bilder, ohne, geprueft, falsch, ungeprueft, fehlend = messe()

    print("Korpus-Inhalt (MF-929, P3-197)")
    print("  Abbilder im Manifest        : %3d" % len(bilder))
    print("  mit Inhaltsangabe, geprueft : %3d" % geprueft)
    print("  mit Angabe, nicht pruefbar  : %3d  (Format ohne feste Kennung)"
          % ungeprueft)
    print("  Datei nicht vorhanden       : %3d  (gitignored/nicht beschafft)"
          % fehlend)
    print("  OHNE Inhaltsangabe          : %3d  (Grundlinie %d)"
          % (len(ohne), OHNE_INHALT_MAX))

    schlecht = 0
    if falsch:
        print("\n  ABWEICHUNG — die Angabe stimmt nicht mit der Datei ueberein:")
        for rel, b, g in falsch:
            print("    %s" % rel)
            print("      behauptet: %s" % b)
            print("      gemessen : %s" % g)
        schlecht = 1

    if len(ohne) > OHNE_INHALT_MAX:
        print("\n  RUECKSTAND GESTIEGEN: %d ohne Angabe, Grundlinie ist %d."
              % (len(ohne), OHNE_INHALT_MAX))
        print("  Ein neues Korpus-Abbild bekommt eine Inhaltsangabe, sonst")
        print("  baut die naechste Zusicherung wieder auf eine Vermutung.")
        schlecht = 1

    print("\n%s" % ("FAIL" if schlecht else "OK"))
    return schlecht


if __name__ == "__main__":
    sys.exit(main())
