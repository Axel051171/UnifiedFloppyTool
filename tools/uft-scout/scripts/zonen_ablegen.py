#!/usr/bin/env python3
"""Die Lizenzzonen der gesichteten Repos festschreiben (MF-705).

    python tools/uft-scout/scripts/zonen_ablegen.py

── Warum es diese Datei gibt ────────────────────────────────────────────

`docs/STAND.md` fuehrt die Lizenzzonen der gesichteten Fremd-Repos. Die
Quelle dafuer waren bis MF-705 die `*.messung.json` unter
`tools/uft-scout/work/` — und das Verzeichnis ist **gitignored**, weil
darin geklonte Fremdbaeume liegen (MF-633).

Damit war die Uebersichtsseite aus Daten abgeleitet, die es in CI nicht
gibt: dort waere die Tabelle leer, `STAND.md` daher anders als lokal und
das Frische-Tor 44 **dauerhaft rot**. Ein Tor, das in CI immer rot ist,
wird abgeschaltet — und dann faellt auch das weg, wofuer es gebaut war.

Aufgefallen ist es nicht beim Bauen, sondern eine Runde spaeter im
Echtbetrieb: der `pyAccess1581`-Zyklus legte eine neue Messung ab, Tor 44
schlug korrekt an, und beim Nachsehen kam heraus, dass die Quelle gar
nicht mitwandert.

Dieses Skript schreibt die Zonen als **verfolgte** Datei
(`tools/uft-scout/data/zonen.json`). Sie ist ab MF-705 die einzige
Quelle fuer die Tabelle in `STAND.md` — eine Wahrheit, ueberall
verfuegbar, statt zweier, die auseinanderlaufen.

Es ersetzt die Messungen nicht: `work/` bleibt der Arbeitsplatz, diese
Datei ist sein **Auszug**. Wer eine Zone anzweifelt, misst neu; wer die
Uebersicht liest, braucht den Klon nicht.
"""
from __future__ import annotations

import json
import os
from datetime import date

HIER = os.path.dirname(os.path.abspath(__file__))
WORK = os.path.join(HIER, "..", "work")
ZIEL = os.path.join(HIER, "..", "data", "zonen.json")


def main() -> int:
    if not os.path.isdir(WORK):
        print("FEHLER: tools/uft-scout/work/ fehlt — ohne Messungen gibt "
              "es nichts abzulegen. Erst einen Zyklus fahren.")
        return 1
    zonen: dict[str, str] = {}
    hinweise: dict[str, str] = {}
    for f in sorted(os.listdir(WORK)):
        if not f.endswith(".messung.json"):
            continue
        try:
            with open(os.path.join(WORK, f), encoding="utf-8") as fh:
                d = json.load(fh)
        except (OSError, ValueError):
            continue
        name = f[:-len(".messung.json")]
        zonen[name] = d.get("lizenz_zone", "?")
        h = d.get("lizenz_hinweis", "")
        if h:
            hinweise[name] = h[:160]

    paket = {
        "_kommentar": (
            "Auszug aus tools/uft-scout/work/*.messung.json (MF-705). "
            "VERFOLGT, weil docs/STAND.md daraus ableitet und work/ "
            "gitignored ist — sonst waere die Uebersicht in CI leer und "
            "das Frische-Tor dauerhaft rot. Erzeugt von "
            "tools/uft-scout/scripts/zonen_ablegen.py; nicht von Hand "
            "pflegen."),
        "_stand": date.today().isoformat(),
        "_was_die_zonen_heissen": {
            "GRUEN": "Port moeglich (MIT/BSD/GPL-2 …)",
            "GELB": "kein Port — Nachbau, Oracle, Spec, Helfer",
            "ORANGE": "nicht einlinkbar (BSD-4) — Helfer-Prozess",
            "PRUEFEN": "Lizenz ungeklaert oder mehrdeutig — Eigentuemer",
            "ROT": "keine Lizenz gefunden = alle Rechte vorbehalten",
        },
        "zonen": zonen,
        "hinweise": hinweise,
    }
    os.makedirs(os.path.dirname(ZIEL), exist_ok=True)
    with open(ZIEL, "w", encoding="utf-8", newline="\n") as fh:
        json.dump(paket, fh, ensure_ascii=False, indent=1, sort_keys=False)
        fh.write("\n")
    verteilung: dict[str, int] = {}
    for z in zonen.values():
        verteilung[z] = verteilung.get(z, 0) + 1
    print(f"{len(zonen)} Repos abgelegt -> "
          f"{os.path.relpath(ZIEL, os.path.join(HIER, '..', '..', '..'))}")
    print("  " + " · ".join(f"{k} {v}" for k, v in sorted(verteilung.items())))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
