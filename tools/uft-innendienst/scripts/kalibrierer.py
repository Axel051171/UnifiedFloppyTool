#!/usr/bin/env python3
"""kalibrierer.py — Rolle 3: eicht Oracles (Laengensemantik zuerst).

    kalibrierer.py erzeugen            -> work/kalib_127.bin (Hausmass)
    kalibrierer.py pruefen <uft-pfad>  -> Register x Kalibrier-Stand

Ein Oracle-Wert, der Erfindung belohnt, ist schlimmer als keiner. Der
Anlass steht in `docs/ORACLES.md`: floptool meldete **254 Byte** fuer
eine 127-Byte-Datei, und dieser Wert stand dort als Messung. Seit MF-685
ist die Laengensemantik Pflichtfeld; dieses Werkzeug haelt das Register
gegen die Kalibriertabelle und meldet, wer noch keine hat.

── Struktur, gemessen statt geraten ─────────────────────────────────────

  `## Registrierte Oracles`      -> Tabelle | `kurzname` | ... |
  `### Stand der Kalibrierung`   -> Tabelle | Werkzeug | Semantik |
                                    gemessen | womit |

── Die Sammelzeile (Anpassung MF-693) ───────────────────────────────────

Die Kalibriertabelle fasst die ungemessenen Werkzeuge in EINER Zeile
zusammen:

    | `lsatr`, `a8rawconv`, `gw`, `cpmls`, `hxcfe`, `samdisk`, `dtc`
      | **ungemessen** | — | offen |

Die Erstfassung las aus dieser Zelle nur den ERSTEN Namen. Ergebnis:
`lsatr` bekam korrekt "ungemessen", `cpmls`/`hxcfe`/`samdisk` bekamen
"**keine** Kalibrierzeile" — obwohl eine dasteht. Beide Male ist der
Eichlauf faellig, das Urteil stimmte also zufaellig; die **Begruendung**
war falsch, und eine falsche Begruendung in einem Bericht, der
Entscheidungen traegt, ist ein Befund. Die Zelle wird jetzt an Kommas
zerlegt.

Zweite Folge davon: `gw` und `dtc` stehen in derselben Zeile. Die
Ausnahmeliste in `config_innendienst.json` erklaert sie fuer
gegenstandslos — ORACLES.md fuehrt sie als ungemessen. Dieser
Widerspruch wird jetzt **gemeldet**, statt dass die Ausnahme still
gewinnt. Wer sie fuer richtig haelt, traegt sie in ORACLES.md nach; das
ist eine Eigentuemer-Entscheidung, keine Werkzeugfrage.

── Was diese Messung nicht sieht ────────────────────────────────────────

Ob die eingetragene Semantik **stimmt**. Sie liest, was dasteht. Der
Eichlauf selbst — 127-Byte-Datei durchreichen, gemeldete Zahl ansehen —
bleibt eine Handbewegung; `erzeugen` legt die Kalibrierdatei dafuer an.
"""
from __future__ import annotations

import json
import os
import re
import sys

HIER = os.path.dirname(os.path.abspath(__file__))


def erzeugen() -> None:
    p = os.path.join(HIER, "..", "work", "kalib_127.bin")
    os.makedirs(os.path.dirname(p), exist_ok=True)
    daten = bytes((i * 7 + 13) & 0xFF for i in range(119)) + b"UFTKAL16"
    assert len(daten) == 127
    open(p, "wb").write(daten)
    print(f"OK: {p}")
    print("127 Byte — das Hausmass. Krumm, damit eine gepolsterte "
          "Antwort sofort auffaellt: 254 bei CBM-DOS, 488 bei "
          "Amiga-OFS, 512 bei FAT (MF-684/685).")


def tabelle_nach(text: str, ueberschrift_rx: str) -> list[list[str]]:
    """Erste Markdown-Tabelle nach der Ueberschrift, ohne Kopfzeile."""
    m = re.search(ueberschrift_rx, text, re.M)
    if not m:
        return []
    zeilen: list[list[str]] = []
    for z in text[m.end():].splitlines():
        if z.startswith("|"):
            zellen = [c.strip() for c in z.strip("|").split("|")]
            if all(set(c) <= set("-: ") for c in zellen):
                continue
            zeilen.append(zellen)
        elif zeilen:
            break
    return zeilen[1:] if zeilen else []


def namen_in(zelle: str) -> list[str]:
    """Alle Werkzeugnamen einer Zelle — auch aus einer Sammelzeile.

    Erst an Kommas trennen, DANN je Teil den ersten Backtick-Namen
    nehmen. Andernfalls wird aus `` `floptool` (`flophashes`) `` ein
    zweites, erfundenes Werkzeug — gemessen beim ersten Lauf: der
    Bericht meldete `flophashes` als "kalibriert, aber nicht im
    Register". Ein Klammerzusatz ist ein Aufrufname, kein Oracle.
    """
    namen = []
    for teil in zelle.split(","):
        m = re.search(r"`([^`]+)`", teil)
        if m:
            namen.append(m.group(1).split()[0])
        elif teil.split():
            namen.append(teil.split()[0])
    return namen


def pruefen(root: str) -> int:
    om = os.path.join(root, "docs", "ORACLES.md")
    if not os.path.exists(om):
        print("FEHLER: docs/ORACLES.md fehlt")
        return 1
    text = open(om, encoding="utf-8", errors="replace").read()

    register = tabelle_nach(text, r"^##\s+Registrierte Oracles")
    kalib = tabelle_nach(text, r"^###\s+Stand der Kalibrierung")
    if not register:
        print("FEHLER: Register-Tabelle nicht gefunden — Struktur von "
              "ORACLES.md geaendert? Parser dagegen neu messen, nicht "
              "raten.")
        return 1

    reg_namen: list[str] = []
    for z in register:
        if z:
            reg_namen += namen_in(z[0])[:1]      # Kurzname = erste Spalte

    kal: dict[str, dict[str, str]] = {}
    for z in kalib:
        if len(z) >= 3:
            for name in namen_in(z[0]):
                kal[name] = {"semantik": z[1], "gemessen": z[2]}

    cfgp = os.path.join(HIER, "..", "config_innendienst.json")
    cfg = json.load(open(cfgp, encoding="utf-8")) \
        if os.path.exists(cfgp) else {}
    ausnahmen = {k: v for k, v in cfg.get("eich_ausnahmen", {}).items()
                 if not k.startswith("_")}

    befunde, ok, hinweise = [], [], []
    for name in reg_namen:
        eintrag = kal.get(name)
        ausnahme = ausnahmen.get(name)
        if eintrag and "ungemessen" in eintrag["semantik"].lower():
            if ausnahme:
                hinweise.append(
                    f"`{name}`: ORACLES.md fuehrt es als **ungemessen**, "
                    f"die Ausnahmeliste haelt es fuer gegenstandslos "
                    f"({ausnahme}). Eine der beiden Stellen muss "
                    f"nachziehen — Eigentuemer-Entscheidung.")
                ok.append(f"{name} (Ausnahme, mit Widerspruch)")
            else:
                befunde.append(f"- `{name}`: Kalibrierzeile sagt "
                               f"**ungemessen** — Eichlauf faellig")
        elif eintrag:
            if not re.search(r"\d{4}-\d{2}-\d{2}|MF-\d+|Zyklus",
                             eintrag["gemessen"]):
                befunde.append(f"- `{name}`: Semantik "
                               f"`{eintrag['semantik']}` ohne "
                               f"Eichdatum/Anker — wer hat wann gemessen?")
            else:
                ok.append(f"{name} ({eintrag['semantik'].strip('*')})")
        elif ausnahme:
            ok.append(f"{name} (Ausnahme: {ausnahme})")
        else:
            befunde.append(f"- `{name}`: registriert, aber **keine "
                           f"Kalibrierzeile** — Laengensemantik "
                           f"unbekannt. Vor jedem Differenzlauf eichen "
                           f"(Hausmass `work/kalib_127.bin`)")

    unregistriert = [k for k in kal if k not in reg_namen]

    print(f"Register: {len(reg_namen)} · geeicht/ausgenommen: {len(ok)} · "
          f"Befunde: {len(befunde)}")
    for b in befunde:
        print(b)
    if ok:
        print("\ngeeicht/ausgenommen: " + ", ".join(ok))
    for h in hinweise:
        print("\nWIDERSPRUCH: " + h)
    if unregistriert:
        print("\nHinweis — kalibriert, aber NICHT im Register (zaehlt "
              "fuer kein T1b-Manifest): " +
              ", ".join(f"`{k}`" for k in sorted(unregistriert)))
    print("\nWas die Zahlen nicht heissen: gelesen wird, was dasteht — "
          "nicht, ob es stimmt. Der Eichlauf selbst bleibt eine "
          "Handbewegung.")
    return 0


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    if sys.argv[1] == "erzeugen":
        erzeugen()
        return 0
    if sys.argv[1] == "pruefen" and len(sys.argv) > 2:
        return pruefen(sys.argv[2])
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main())
