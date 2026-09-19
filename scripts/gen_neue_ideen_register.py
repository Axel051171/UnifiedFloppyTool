#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Das abgeleitete Register zu `neue-ideen/` (A-032, `P3-512`).

WOZU
────
`A-032` lautet „gehe hier alles gruendlich, schau ab was vergessen
wurde". Die Falle dabei ist nicht die Arbeit, sondern die Buchfuehrung:
**216 Eintraege kann niemand im Kopf halten**, und eine von Hand
gepflegte Liste veraltet still. Genau das hat dieser Baum fuenfmal
gemessen (MF-636, MF-567, MF-578, MF-598, MF-633) — zuletzt im Kopf
eines TORES, wo eine Aufzaehlung dessen Grenze beschrieb.

Deshalb ist das Register **abgeleitet, nicht gepflegt**: die
Verzeichnisebene ist die Wahrheit, `docs/neue_ideen_urteile.json` die
Meinung dazu, und dieses Skript meldet die Differenz in BEIDE
Richtungen —

  · ein Eintrag auf der Platte **ohne** Urteil  → noch zu sichten
  · ein Urteil **ohne** Eintrag auf der Platte  → verwaist, die
    Aufzaehlung ist der Wirklichkeit davongelaufen

Die zweite Richtung ist die wichtigere. Ohne sie waere dies wieder
genau die gepflegte Liste, gegen die es gebaut ist.

WAS ES NICHT TUT
────────────────
Es faellt kein Urteil. Lizenz, Kanal und Kennzahl stehen in der JSON
und kommen von einer Messung am Paket — nie von diesem Skript und nie
vom API-Feld eines Hosters (`measurement_hit_wrong_class`: GitHub
meldet die Projekteinstellung, nicht die Lizenzdatei; `fdtc` galt
danach als „ohne Lizenz" und traegt BSD-3).

WENN `neue-ideen/` FEHLT
───────────────────────
Dann sagt es **„Umfang nicht feststellbar"** und endet mit Kode 2 —
*nicht* mit „alles beurteilt". Das Verzeichnis ist gitignoriert, in CI
also gar nicht da; ein Register, das dort Vollstaendigkeit meldet,
waere eine Falschaussage im Ruhezustand. Das ist die Lehre aus
MF-1171: eine Rueckfallebene, die schweigt, ist schlimmer als eine,
die ihre Grenze nennt.

AUFRUF
──────
    python3 scripts/gen_neue_ideen_register.py              # melden
    python3 scripts/gen_neue_ideen_register.py --write      # + Doku
    python3 scripts/gen_neue_ideen_register.py --selbsttest # Abnahme
"""
from __future__ import annotations

import io
import json
import pathlib
import sys

WURZEL = pathlib.Path(__file__).resolve().parent.parent
QUELLE = WURZEL / "neue-ideen"
URTEILE = WURZEL / "docs" / "neue_ideen_urteile.json"
ZIEL = WURZEL / "docs" / "NEUE_IDEEN_REGISTER.md"

PFLICHTFELDER = ("lizenz", "lizenz_beleg", "kanal", "kennzahl",
                 "naechster_griff")


# ── dateilesender Teil ──────────────────────────────────────────────

def lies_ebene(quelle: pathlib.Path):
    """Die Namen der obersten Ebene, oder None wenn es sie nicht gibt.

    None heisst „Umfang nicht feststellbar" und ist von „leer"
    ausdruecklich zu unterscheiden — ein leeres Verzeichnis waere eine
    Aussage, ein fehlendes ist keine.
    """
    if not quelle.is_dir():
        return None
    return sorted((p.name for p in quelle.iterdir()), key=str.lower)


def lies_urteile(pfad: pathlib.Path):
    d = json.loads(pfad.read_text(encoding="utf-8"))
    return (d.get("eintraege", {}), d.get("_kanaele", []),
            d.get("_kennzahlen", []))


# ── reiner Teil: pruefbar ohne Platte ───────────────────────────────

def pruefe(namen, urteile, kanaele, kennzahlen):
    """Die Differenz zwischen Platte und Meinung — in beide Richtungen.

    `namen` ist die Verzeichnisebene, `urteile` die JSON-Abbildung.
    Gibt ein Woerterbuch mit fuenf Listen zurueck; jede leer bedeutet
    „in dieser Hinsicht nichts offen".
    """
    auf_platte = set(namen)
    beurteilt = set(urteile)

    ohne_urteil = sorted(auf_platte - beurteilt, key=str.lower)
    verwaist = sorted(beurteilt - auf_platte, key=str.lower)

    unvollstaendig, falscher_kanal, falsche_kennzahl = [], [], []
    for name in sorted(beurteilt & auf_platte, key=str.lower):
        u = urteile[name]
        fehlend = [f for f in PFLICHTFELDER if not str(u.get(f, "")).strip()]
        if fehlend:
            unvollstaendig.append((name, fehlend))
        if kanaele and u.get("kanal") not in kanaele:
            falscher_kanal.append((name, u.get("kanal")))
        if kennzahlen and u.get("kennzahl") not in kennzahlen:
            falsche_kennzahl.append((name, u.get("kennzahl")))

    return {
        "gesamt": len(auf_platte),
        "beurteilt": len(beurteilt & auf_platte),
        "ohne_urteil": ohne_urteil,
        "verwaist": verwaist,
        "unvollstaendig": unvollstaendig,
        "falscher_kanal": falscher_kanal,
        "falsche_kennzahl": falsche_kennzahl,
    }


def offen(befund) -> int:
    """Wie viele Punkte den Posten offen halten."""
    return (len(befund["ohne_urteil"]) + len(befund["verwaist"])
            + len(befund["unvollstaendig"]) + len(befund["falscher_kanal"])
            + len(befund["falsche_kennzahl"]))


# ── Doku ────────────────────────────────────────────────────────────

def render_md(namen, urteile, befund) -> str:
    z = []
    z.append("# Register `neue-ideen/` — je Eintrag ein Urteil\n")
    z.append("> **ERZEUGT — nicht von Hand aendern.** Quelle sind die\n"
             "> Verzeichnisebene von `neue-ideen/` und die Urteile in\n"
             "> [`neue_ideen_urteile.json`](neue_ideen_urteile.json).\n"
             "> Erzeugen mit\n"
             "> `python3 scripts/gen_neue_ideen_register.py --write`.\n")
    z.append("")
    z.append("Der Posten `A-032` ist so lange **offen**, wie auch nur ein\n"
             "Eintrag ohne Urteil dasteht. Das Register sagt das von selbst;\n"
             "es muss niemand zaehlen.\n")
    z.append("")
    z.append("| | Zahl |")
    z.append("|---|---|")
    z.append("| Eintraege auf der obersten Ebene | %d |" % befund["gesamt"])
    z.append("| davon mit Urteil | %d |" % befund["beurteilt"])
    z.append("| **ohne Urteil** | **%d** |" % len(befund["ohne_urteil"]))
    z.append("| Urteile ohne Eintrag (verwaist) | %d |"
             % len(befund["verwaist"]))
    z.append("| Urteile mit fehlendem Feld | %d |"
             % len(befund["unvollstaendig"]))
    z.append("")

    if befund["verwaist"]:
        z.append("## Verwaiste Urteile\n")
        z.append("Ein Urteil ueber etwas, das es nicht mehr gibt. Genau die\n"
                 "Richtung, in der eine gepflegte Liste still veraltet.\n")
        for n in befund["verwaist"]:
            z.append("- `%s`" % n)
        z.append("")

    if befund["unvollstaendig"]:
        z.append("## Urteile mit fehlendem Feld\n")
        for n, f in befund["unvollstaendig"]:
            z.append("- `%s` — fehlt: %s" % (n, ", ".join(f)))
        z.append("")

    if befund["falscher_kanal"] or befund["falsche_kennzahl"]:
        z.append("## Werte ausserhalb der erlaubten Menge\n")
        for n, v in befund["falscher_kanal"]:
            z.append("- `%s` — Kanal `%s` ist keiner der sieben nach MF-695"
                     % (n, v))
        for n, v in befund["falsche_kennzahl"]:
            z.append("- `%s` — Kennzahl `%s` ist keine der vier" % (n, v))
        z.append("")

    z.append("## Gefaellte Urteile\n")
    if not befund["beurteilt"]:
        z.append("*(noch keines)*\n")
    else:
        z.append("| Eintrag | Lizenz | Beleg | Kanal | Kennzahl "
                 "| naechster Griff |")
        z.append("|---|---|---|---|---|---|")
        auf_platte = set(namen)
        for n in sorted(urteile, key=str.lower):
            if n not in auf_platte:
                continue
            u = urteile[n]
            z.append("| `%s` | %s | %s | %s | %s | %s |" % (
                n,
                str(u.get("lizenz", "")).replace("|", "/"),
                str(u.get("lizenz_beleg", "")).replace("|", "/"),
                str(u.get("kanal", "")).replace("|", "/"),
                str(u.get("kennzahl", "")).replace("|", "/"),
                str(u.get("naechster_griff", "")).replace("|", "/"),
            ))
        z.append("")

    z.append("## Noch ohne Urteil\n")
    if not befund["ohne_urteil"]:
        z.append("*(keiner — der Posten kann geschlossen werden)*\n")
    else:
        z.append("%d Eintraege. Sie stehen hier vollstaendig, weil eine\n"
                 "gekuerzte Liste genau die Aufzaehlung waere, gegen die\n"
                 "dieses Register gebaut ist.\n" % len(befund["ohne_urteil"]))
        for n in befund["ohne_urteil"]:
            z.append("- `%s`" % n)
        z.append("")

    return "\n".join(z) + "\n"


# ── Abnahme ─────────────────────────────────────────────────────────

def _selbsttest() -> int:
    """Neun Zusagen an den reinen Teil. Jede kann fallen."""
    K = ["Spec", "Fundus"]
    Z = ["T3 runter", "Fundus"]
    voll = {f: "x" for f in PFLICHTFELDER}

    faelle = []

    # 1-2: ohne Urteil wird gefunden, und zwar genau einmal
    b = pruefe(["a", "b"],
               {"a": dict(voll, kanal="Spec", kennzahl="Fundus")}, K, Z)
    faelle.append(("ohne Urteil gefunden", b["ohne_urteil"] == ["b"]))
    faelle.append(("beurteilt gezaehlt", b["beurteilt"] == 1))

    # 3: verwaistes Urteil wird gefunden — die wichtigere Richtung
    b = pruefe(["a"], {"a": dict(voll, kanal="Spec", kennzahl="Fundus"),
                       "weg": dict(voll, kanal="Spec", kennzahl="Fundus")},
               K, Z)
    faelle.append(("verwaistes Urteil gefunden", b["verwaist"] == ["weg"]))

    # 4: ein verwaistes Urteil macht NICHT zugleich „ohne Urteil"
    faelle.append(("verwaist nicht doppelt gezaehlt", b["ohne_urteil"] == []))

    # 5: fehlendes Pflichtfeld faellt auf
    b = pruefe(["a"], {"a": dict(voll, kanal="Spec", kennzahl="Fundus",
                                 lizenz_beleg="")}, K, Z)
    faelle.append(("leeres Pflichtfeld gefunden",
                   b["unvollstaendig"] == [("a", ["lizenz_beleg"])]))

    # 6: nur-Leerzeichen zaehlt als fehlend, nicht als gesetzt
    b = pruefe(["a"], {"a": dict(voll, kanal="Spec", kennzahl="Fundus",
                                 naechster_griff="   ")}, K, Z)
    faelle.append(("Leerzeichen zaehlt als fehlend",
                   b["unvollstaendig"] == [("a", ["naechster_griff"])]))

    # 7-8: Werte ausserhalb der Menge
    b = pruefe(["a"], {"a": dict(voll, kanal="Zauberei", kennzahl="Fundus")},
               K, Z)
    faelle.append(("falscher Kanal gefunden",
                   b["falscher_kanal"] == [("a", "Zauberei")]))
    b = pruefe(["a"], {"a": dict(voll, kanal="Spec", kennzahl="mehr Ruhm")},
               K, Z)
    faelle.append(("falsche Kennzahl gefunden",
                   b["falsche_kennzahl"] == [("a", "mehr Ruhm")]))

    # 9: alles sauber -> offen() ist 0, und NUR dann
    b = pruefe(["a"], {"a": dict(voll, kanal="Spec", kennzahl="Fundus")}, K, Z)
    faelle.append(("sauberer Fall meldet 0 offen", offen(b) == 0))

    gut = sum(1 for _, ok in faelle if ok)
    for name, ok in faelle:
        print("  [%s] %s" % ("ok " if ok else "ROT", name))
    print("Selbsttest %d/%d" % (gut, len(faelle)))
    return 0 if gut == len(faelle) else 1


def main(argv) -> int:
    # Windows-Konsole ist cp1252; die Kaesten und Gedankenstriche
    # wuerden sonst mit UnicodeEncodeError abbrechen.
    if hasattr(sys.stdout, "buffer"):
        sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8",
                                      errors="replace", line_buffering=True)

    if "--selbsttest" in argv:
        return _selbsttest()

    namen = lies_ebene(QUELLE)
    if namen is None:
        print("Umfang nicht feststellbar: %s gibt es hier nicht." % QUELLE)
        print("Das Verzeichnis ist gitignoriert; in CI ist es nie da.")
        print("KEINE Aussage ueber Vollstaendigkeit — weder ja noch nein.")
        return 2

    urteile, kanaele, kennzahlen = lies_urteile(URTEILE)
    befund = pruefe(namen, urteile, kanaele, kennzahlen)

    print("neue-ideen/ oberste Ebene : %d Eintraege" % befund["gesamt"])
    print("  mit Urteil               : %d" % befund["beurteilt"])
    print("  OHNE Urteil              : %d" % len(befund["ohne_urteil"]))
    print("  verwaiste Urteile        : %d" % len(befund["verwaist"]))
    print("  Urteil mit fehlendem Feld: %d" % len(befund["unvollstaendig"]))
    print("  Kanal ausserhalb Menge   : %d" % len(befund["falscher_kanal"]))
    print("  Kennzahl ausserhalb      : %d" % len(befund["falsche_kennzahl"]))

    if "--write" in argv:
        ZIEL.write_text(render_md(namen, urteile, befund), encoding="utf-8")
        print("-> %s" % ZIEL)

    n = offen(befund)
    print("\nA-032 offen: %d Punkte" % n)
    return 1 if n else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
