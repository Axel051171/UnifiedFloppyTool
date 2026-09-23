#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""
Plugin-Deklarationen ohne Definition — die Tuer mit Namensschild und
ohne Raum dahinter (MF-1332).

── Warum es dieses Tor gibt ───────────────────────────────────────────
Ein `extern const uft_format_plugin_t uft_format_plugin_X;` in einem
oeffentlichen Header ist eine ZUSAGE: dieses Plugin gibt es. Der Binder
widerspricht nicht, solange niemand das Symbol benutzt — die Zusage kann
also jahrelang falsch dastehen, ohne dass etwas rot wird.

Gemessen am 2026-09-21 standen SECHS solche Zeilen im Baum:

    uft_format_plugin_dfi     0 Quelldateien
    uft_format_plugin_ldbs    0
    uft_format_plugin_raw     0
    uft_format_plugin_simh    0
    uft_format_plugin_x68k    0
    uft_format_plugin_ydsk    0

Und es ist nicht folgenlos. Wer `dfi` verdrahten will, findet ein Symbol
vor, das aussieht, als gaebe es das Plugin schon; DFI-Code LIEGT im Baum,
aber in der verwaisten `FloppyDevice`-Schicht (`src/formats/flux/dfi.c`,
`uft_flx_dfi_*`) und nicht als Plugin. Zwei Dinge, ein Name. Das ist die
Klasse Phantom-API aus MF-366, wo die Audit-Trail-C-API zu 100 %
unimplementiert war.

── Warum nur HEADER nach Deklarationen durchsucht werden ─────────────
Die Zusage, um die es geht, ist die im oeffentlichen Header: sie sagt
einem fremden Uebersetzungsabschnitt „dieses Plugin gibt es". Ein
`extern` in einer `.c` ist dagegen lokale Verdrahtung — die Registry
`src/formats/format_registry/uft_format_registry.c` deklariert ihre
Plugins so und BENUTZT sie im selben Zug, also faellt ein fehlendes
Symbol dort beim Binden auf. Gemessen: 25 Deklarationen in Headern
gegen 138 Definitionen im Baum.

── Die Falle, an der eine naive Messung scheitert ─────────────────────
Meine erste, freihaendige Zaehlung meldete **56** statt 6, und der Fehler
steht hier, damit ihn niemand wiederholt: 49 Namen kommen aus dem Makro

    DSK_PLUGIN(INDEX, SUFFIX, NAME, DESC, EXT)
        -> const uft_format_plugin_t uft_format_plugin_dsk_##SUFFIX = {

in `src/formats/dsk_generic/uft_dsk_generic.c:301-318`. Wer nur nach
`const uft_format_plugin_t <name> =` sucht, sieht ihre Definitionen NICHT
und haelt sie alle fuer Phantome.

GENAU GESAGT, und das ist an der Mutationsprobe gemessen: DIESES Tor
kann der Falle gar nicht erliegen, weil es Deklarationen nur in Headern
sucht und die `dsk_*` dort nicht stehen. Die Makroaufloesung unten ist
also Vorsorge, kein Notbehelf — sie traegt in dem Augenblick, in dem
jemand den Deklarations-Umfang auf `.c` ausweitet. Der Selbsttestfall
„Makronamen kommen an" bewacht dieses Verhalten, nicht einen aktuellen
Fehlalarm; unter der Mutation wird er rot, das Tor selbst bleibt gruen.
Beides gehoert gesagt, sonst verspricht dieser Kopf eine Gefahr, die die
Bauart bereits ausschliesst.

Die Lehre bleibt dieselbe wie bei `teilstring_statt_zeichen` und
`measurement_hit_wrong_class`: eine Zahl aus einer Messung ist erst dann
ein Befund, wenn man weiss, was die Messung NICHT sehen kann.

── Zusicherung ───────────────────────────────────────────────────────
Fallende Grundlinie, Bauform Tor 57: die Zahl darf nur sinken. Ein NEUES
Phantom ist sofort rot; die sechs bestehenden sind beschriftet
(`include/uft/uft_format_plugin.h`) und warten auf eine
Eigentuemerentscheidung, weil ihr Entfernen eine Loeschung in der
Formatschicht waere (MF-1077).

Dateimengen kommen aus `git ls-files` (MF-636), nicht aus einer
Verzeichnisliste.

Aufruf:
    python3 scripts/audit_phantom_plugin.py
    python3 scripts/audit_phantom_plugin.py --selftest
"""
from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

WURZEL = Path(__file__).resolve().parent.parent

# Gemessen 2026-09-21: dfi, ldbs, raw, simh, x68k, ydsk.
GRUNDLINIE = 6

DEKL = re.compile(
    r"\bextern\s+const\s+uft_format_plugin_t\s+uft_format_plugin_"
    r"([A-Za-z0-9_]+)\s*;")
DEFI = re.compile(
    r"\bconst\s+uft_format_plugin_t\s+uft_format_plugin_"
    r"([A-Za-z0-9_]+)\s*=")
# `DSK_PLUGIN(23, rld, "DSK_RLD", ...)` -> uft_format_plugin_dsk_rld
MAKRO = re.compile(r"\bDSK_PLUGIN\s*\(\s*\d+\s*,\s*([A-Za-z0-9_]+)\s*,")


def _repo_dateien() -> list[str]:
    """Versionierte und neue Dateien — MF-636, nie eine Verzeichnisliste."""
    try:
        p = subprocess.run(
            ["git", "ls-files", "--cached", "--others", "--exclude-standard"],
            cwd=WURZEL, capture_output=True, text=True,
            errors="replace", timeout=120)
    except Exception:                       # noqa: BLE001 - Messwerkzeug
        return []
    if p.returncode != 0:
        return []
    return [z.strip() for z in p.stdout.splitlines() if z.strip()]


def _lies(rel: str) -> str:
    try:
        return (WURZEL / rel).read_text(encoding="utf-8", errors="replace")
    except OSError:
        return ""


def _fremd(p: str) -> bool:
    """Fremdcode zaehlt nicht — dort gelten andere Regeln, und ein Klon
    bringt seine eigenen Header mit."""
    return (p.startswith("tools/") or p.startswith("proto/")
            or p.startswith("src/samdisk/") or p.startswith("neue-ideen/"))


def messen() -> dict:
    dateien = _repo_dateien()
    header = [p for p in dateien if p.endswith(".h") and not _fremd(p)]
    quellen = [p for p in dateien
               if p.endswith((".c", ".cpp")) and not _fremd(p)]

    deklariert: dict[str, str] = {}     # name -> erste Fundstelle
    for p in header:
        for m in DEKL.finditer(_lies(p)):
            deklariert.setdefault(m.group(1), p)

    definiert: set[str] = set()
    aus_makro: set[str] = set()
    for p in quellen:
        t = _lies(p)
        definiert |= set(DEFI.findall(t))
        for suffix in MAKRO.findall(t):
            aus_makro.add("dsk_" + suffix)
    definiert |= aus_makro

    phantome = sorted(set(deklariert) - definiert)
    return {
        "header": len(header), "quellen": len(quellen),
        "deklariert": len(deklariert), "definiert": len(definiert),
        "aus_makro": len(aus_makro),
        "phantome": phantome, "wo": deklariert,
    }


def selbsttest() -> int:
    faelle: list[tuple[str, bool]] = []

    faelle.append(("eine Deklaration wird erkannt",
                   DEKL.findall("extern const uft_format_plugin_t "
                                "uft_format_plugin_abc;") == ["abc"]))
    faelle.append(("eine Definition wird erkannt",
                   DEFI.findall("const uft_format_plugin_t "
                                "uft_format_plugin_abc = {") == ["abc"]))
    # DIE FALLE: ohne Makroaufloesung waeren alle dsk_* Phantome.
    faelle.append(("DSK_PLUGIN liefert den erzeugten Namen",
                   MAKRO.findall(
                       'DSK_PLUGIN(23, rld, "DSK_RLD", "Roland DSK", "dsk")')
                   == ["rld"]))
    faelle.append(("eine Deklaration ist keine Definition",
                   DEFI.findall("extern const uft_format_plugin_t "
                                "uft_format_plugin_abc;") == []))
    faelle.append(("ein Praefix allein ist kein Name",
                   DEKL.findall("extern const uft_format_plugin_t "
                                "uft_format_plugin_;") == []))

    # Die Makroaufloesung am ECHTEN Baum. VORSORGE, nicht Notbehelf:
    # an der Mutationsprobe gemessen bleibt das Tor auch ohne sie gruen,
    # weil `dsk_*` nur in der Registry-`.c` deklariert ist und dieses Tor
    # Deklarationen nur in HEADERN sucht. Der Fall traegt in dem
    # Augenblick, in dem jemand den Umfang auf `.c` ausweitet — dann
    # waeren es 49 Fehlalarme.
    m = messen()
    faelle.append(("Makronamen kommen an (Vorsorge, siehe Kopf)",
                   m["aus_makro"] >= 40))
    faelle.append(("kein dsk_* unter den Phantomen",
                   not any(n.startswith("dsk_") for n in m["phantome"])))

    gut = 0
    for name, ok in faelle:
        print("  [%s] %s" % ("OK " if ok else "ROT", name))
        gut += bool(ok)
    print("Selbsttest %d/%d" % (gut, len(faelle)))
    return 0 if gut == len(faelle) else 1


def main(argv: list[str]) -> int:
    if "--selftest" in argv:
        return selbsttest()

    m = messen()
    if not m["header"]:
        print("ABBRUCH — keine Header gefunden; `git ls-files` nicht "
              "befragbar? Ein Freispruch waere hier eine Falschaussage.",
              file=sys.stderr)
        return 2

    print("[phantom-plugin] Deklaration ohne Definition (MF-1332)")
    print(f"  Header geprueft            : {m['header']}")
    print(f"  Quellen geprueft           : {m['quellen']}")
    print(f"  deklarierte Plugins        : {m['deklariert']}")
    print(f"  definierte Plugins         : {m['definiert']}"
          f"  (davon {m['aus_makro']} aus DSK_PLUGIN)")
    print(f"  PHANTOME                   : {len(m['phantome'])}")
    for n in m["phantome"]:
        print(f"    ? uft_format_plugin_{n}   <- {m['wo'][n]}")

    if len(m["phantome"]) > GRUNDLINIE:
        print(f"\nROT: {len(m['phantome'])} Phantome, Grundlinie "
              f"{GRUNDLINIE}. Sie darf nur sinken (MF-1077). Ein neues "
              f"`extern` ohne Definition ist eine Zusage, die der Binder "
              f"nicht prueft.")
        return 1
    if len(m["phantome"]) < GRUNDLINIE:
        print(f"\nHINWEIS: {len(m['phantome'])} < Grundlinie "
              f"{GRUNDLINIE} — Grundlinie im selben Commit nachziehen.")
    print("\nOK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
