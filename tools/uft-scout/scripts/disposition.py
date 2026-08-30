#!/usr/bin/env python3
"""disposition.py — aus Funden eine Arbeitsanweisung machen (MF-718).

    python tools/uft-scout/scripts/disposition.py <funde.json>
    python tools/uft-scout/scripts/disposition.py <funde.json> --md
    python tools/uft-scout/scripts/disposition.py --selbsttest

── Warum es diese Datei gibt ────────────────────────────────────────────

Die Pipeline endete bisher beim Gutachten. Ein Gutachten sagt, was ein
fremdes Repo kann und ob die Lizenz traegt — es sagt **nicht**, ob der
Fund heute dran ist, was vorher fertig sein muss und was er verdraengt.
Genau das war die Luecke: neun Gutachten aus Block 4 lagen wochenlang
als „Vorschlaege" da, und welcher zuerst zu greifen war, musste jedes
Mal neu ausgehandelt werden.

Diese Datei vergibt kein Urteil aus dem Bauch. Sie wendet die Regeln an,
die dieser Baum ohnehin schon hat, und macht sichtbar, was daraus folgt:

  Regel 9 (CLAUDE.md)   ohne Kennzahl ist ein Fund FUNDUS, nicht Auftrag
  MF-695                jeder Fund bekommt den staerksten LEGALEN Kanal;
                        ohne Kanal wartet er benannt, statt zu verfallen
  Lizenz vor Faehigkeit ROT/PRUEFEN sperrt den Port, nicht den Fund
  EINFRIER-REGEL        ein neues Format-Plugin ist gesperrt, AUCH als
                        Vorschlag — solche Funde werden zur Hebung
                        umgeschrieben oder sind Fundus

── Die drei Faecher ─────────────────────────────────────────────────────

  SOFORT   bewegt eine Kennzahl, Kanal offen, keine offene Abhaengigkeit,
           Aufwand klein — es gibt keinen Grund zu warten
  LISTE    bewegt eine Kennzahl, aber etwas fehlt noch: eine Beschaffung,
           eine Eigentuemer-Entscheidung oder ein anderer Fund zuerst
  FUNDUS   heute kein Kanal ODER keine Kennzahl. Benannt wartend, mit
           dem, was ihn oeffnen wuerde — nicht verworfen

── Reihenfolge ──────────────────────────────────────────────────────────

`abhaengig_von` nennt Kennungen anderer Funde oder offener Punkte. Die
Ausgabe ordnet topologisch: was nichts braucht, steht oben. Ein Ring
wird gemeldet, nicht stillschweigend aufgeloest.

── Eingabe ──────────────────────────────────────────────────────────────

    {"funde": [{
       "id":            "SCOUT-42",
       "repo":          "owner/name",
       "titel":         "…",
       "kennzahl":      "T3 runter" | "Pfade rauf" | "Lecks null" |
                        "Bench-Alter runter" | null,
       "kanal":         "Port|Nachbau|Helfer|Oracle|Spec|Daten|Fundus",
       "zone":          "GRUEN|GELB|ORANGE|PRUEFEN|ROT|?",
       "aufwand":       "klein|mittel|gross",
       "abhaengig_von": ["…"],
       "neues_plugin":  false,
       "beleg":         "Datei:Zeile oder Messung"
    }]}

Ein Fund ohne `beleg` wird abgewiesen. Ein Gutachten ohne Messung ist
in diesem Baum kein Gutachten.
"""
from __future__ import annotations

import argparse
import json
import sys

KENNZAHLEN = {"T3 runter", "Pfade rauf", "Lecks null", "Bench-Alter runter"}
KANAELE = {"Port", "Nachbau", "Helfer", "Oracle", "Spec", "Daten", "Fundus"}
ZONEN = {"GRUEN", "GELB", "ORANGE", "PRUEFEN", "ROT", "?"}
AUFWAND = {"klein": 1, "mittel": 2, "gross": 3}

# Was die Zone dem Kanal erlaubt. Der Agent stuft NIE selbst ein — die
# Zone kommt aus `vermessen.py`, hier wird sie nur angewandt (MF-679).
PORT_ERLAUBT = {"GRUEN"}


def pruefe(f: dict) -> list[str]:
    """Was an einem Fund fehlt, damit er ueberhaupt zaehlt."""
    mangel = []
    if not f.get("id"):
        mangel.append("keine Kennung")
    if not f.get("beleg"):
        mangel.append("kein Beleg — ohne Messung kein Fund")
    if f.get("kennzahl") not in KENNZAHLEN and f.get("kennzahl") is not None:
        mangel.append(f"unbekannte Kennzahl {f.get('kennzahl')!r}")
    if f.get("kanal") not in KANAELE:
        mangel.append(f"unbekannter Kanal {f.get('kanal')!r}")
    if f.get("zone") not in ZONEN:
        mangel.append(f"unbekannte Zone {f.get('zone')!r}")
    if f.get("aufwand") not in AUFWAND:
        mangel.append(f"unbekannter Aufwand {f.get('aufwand')!r}")
    return mangel


def einordnen(f: dict) -> tuple[str, str]:
    """(Fach, Begruendung) — nach den Regeln oben, nicht nach Gefuehl."""
    # 1 · Die EINFRIER-REGEL hebt alles aus.
    if f.get("neues_plugin"):
        return ("FUNDUS",
                "neues Format-Plugin — die EINFRIER-REGEL sperrt das auch "
                "als Vorschlag; als Hebung eines vorhandenen Formats neu "
                "fassen, dann wieder vorlegen")

    # 2 · Lizenz vor Faehigkeit: ein Port ausserhalb GRUEN ist keiner.
    if f.get("kanal") == "Port" and f.get("zone") not in PORT_ERLAUBT:
        return ("FUNDUS",
                f"Port bei Zone {f['zone']} — Lizenz vor Faehigkeit. "
                f"Mit anderem Kanal (Nachbau, Oracle, Spec) neu vorlegen")

    # 3 · Regel 9: ohne Kennzahl ist es Bestand, kein Auftrag.
    if not f.get("kennzahl"):
        return ("FUNDUS", "bewegt keine der vier Kennzahlen — notiert, "
                          "nicht eingeplant (Regel 9)")

    # 4 · Kein Kanal heute.
    if f.get("kanal") == "Fundus":
        return ("FUNDUS", "heute kein offener Kanal — benannt wartend")

    # 5 · Etwas muss vorher fertig sein.
    offen = [d for d in f.get("abhaengig_von") or []]
    if offen:
        return ("LISTE", "wartet auf: " + ", ".join(offen))

    # 6 · Gross ist nie „sofort" — das ist die Scope-Regel.
    if f.get("aufwand") == "gross":
        return ("LISTE", "Aufwand gross — erst zuschneiden (Scope-Regel: "
                         "das groesste vollstaendig fertige Teilstueck)")

    return ("SOFORT", f"bewegt {f['kennzahl']}, Kanal {f['kanal']} offen, "
                      f"nichts steht davor")


def rang(f: dict) -> tuple:
    """Kleiner ist frueher. Kennzahl vor Aufwand, Aufwand vor Name."""
    kz = {"T3 runter": 0, "Pfade rauf": 1,
          "Lecks null": 2, "Bench-Alter runter": 3}
    return (kz.get(f.get("kennzahl") or "", 9),
            AUFWAND.get(f.get("aufwand"), 9),
            f.get("id", ""))


def reihenfolge(funde: list[dict]) -> tuple[list[str], list[str]]:
    """Topologisch. Rueckgabe: (Reihenfolge, Ringe)."""
    nach_id = {f["id"]: f for f in funde}
    fertig: list[str] = []
    stand: dict[str, int] = {}   # 0 = offen, 1 = in Arbeit, 2 = fertig
    ringe: list[str] = []

    def geh(i: str, pfad: list[str]) -> None:
        if stand.get(i) == 2:
            return
        if stand.get(i) == 1:
            ringe.append(" -> ".join(pfad + [i]))
            return
        stand[i] = 1
        for d in nach_id.get(i, {}).get("abhaengig_von") or []:
            if d in nach_id:
                geh(d, pfad + [i])
        stand[i] = 2
        fertig.append(i)

    for f in sorted(funde, key=rang):
        geh(f["id"], [])
    return fertig, ringe


def bericht(funde: list[dict], md: bool = False) -> str:
    z = []
    abgewiesen = []
    gut = []
    for f in funde:
        m = pruefe(f)
        if m:
            abgewiesen.append((f.get("id", "<ohne Kennung>"), m))
        else:
            gut.append(f)

    faecher: dict[str, list[tuple[dict, str]]] = {
        "SOFORT": [], "LISTE": [], "FUNDUS": []}
    for f in gut:
        fach, grund = einordnen(f)
        faecher[fach].append((f, grund))

    folge, ringe = reihenfolge(gut)
    platz = {i: n for n, i in enumerate(folge)}

    h = "## " if md else ""
    z.append(f"{h}Disposition: {len(gut)} Funde"
             + (f", {len(abgewiesen)} abgewiesen" if abgewiesen else ""))
    z.append("")

    for fach in ("SOFORT", "LISTE", "FUNDUS"):
        eintraege = sorted(faecher[fach], key=lambda t: platz.get(t[0]["id"], 99))
        z.append(f"{h}{fach} ({len(eintraege)})")
        if not eintraege:
            z.append("  —")
            z.append("")
            continue
        for n, (f, grund) in enumerate(eintraege, 1):
            z.append(f"  {n}. [{f['id']}] {f.get('titel', '')}")
            z.append(f"     Repo {f.get('repo', '—')} · Zone {f['zone']} · "
                     f"Kanal {f['kanal']} · Aufwand {f['aufwand']}")
            z.append(f"     Kennzahl: {f.get('kennzahl') or 'keine'}")
            z.append(f"     Warum hier: {grund}")
            z.append(f"     Beleg: {f['beleg']}")
            z.append("")

    if ringe:
        z.append(f"{h}RINGE in den Abhaengigkeiten — nicht aufloesbar")
        for r in ringe:
            z.append("  " + r)
        z.append("")

    if abgewiesen:
        z.append(f"{h}Abgewiesen (unvollstaendig)")
        for i, m in abgewiesen:
            z.append(f"  {i}: " + "; ".join(m))
        z.append("")

    sofort = len(faecher["SOFORT"])
    z.append(f"{h}Nächster Griff")
    if sofort:
        erst = sorted(faecher["SOFORT"],
                      key=lambda t: platz.get(t[0]["id"], 99))[0][0]
        z.append(f"  [{erst['id']}] {erst.get('titel', '')}")
        z.append(f"  — bewegt {erst['kennzahl']}, nichts steht davor.")
    else:
        z.append("  Kein Fund ist heute sofort greifbar. Was die LISTE "
                 "blockiert, steht je Eintrag unter 'Warum hier'.")
    return "\n".join(z)


def selbsttest() -> int:
    """Gepflanzte Funde mit feststehender Antwort. Ein Sortierer, der
    sich selbst bestaetigt, sortiert nichts (MF-693, MF-710)."""
    fehler = []
    basis = {"repo": "x/y", "zone": "GRUEN", "aufwand": "klein",
             "beleg": "datei.c:1", "abhaengig_von": []}

    faelle = [
        # (Fund, erwartetes Fach, Stichwort in der Begruendung)
        ({**basis, "id": "A", "titel": "a", "kennzahl": "T3 runter",
          "kanal": "Oracle"}, "SOFORT", "nichts steht davor"),
        ({**basis, "id": "B", "titel": "b", "kennzahl": None,
          "kanal": "Spec"}, "FUNDUS", "Regel 9"),
        ({**basis, "id": "C", "titel": "c", "kennzahl": "T3 runter",
          "kanal": "Port", "zone": "GELB"}, "FUNDUS", "Lizenz vor"),
        ({**basis, "id": "D", "titel": "d", "kennzahl": "T3 runter",
          "kanal": "Oracle", "abhaengig_von": ["A"]}, "LISTE", "wartet auf"),
        ({**basis, "id": "E", "titel": "e", "kennzahl": "Pfade rauf",
          "kanal": "Spec", "aufwand": "gross"}, "LISTE", "Scope-Regel"),
        ({**basis, "id": "F", "titel": "f", "kennzahl": "T3 runter",
          "kanal": "Spec", "neues_plugin": True}, "FUNDUS", "EINFRIER"),
        ({**basis, "id": "G", "titel": "g", "kennzahl": "T3 runter",
          "kanal": "Fundus"}, "FUNDUS", "kein offener Kanal"),
    ]
    for f, soll_fach, stich in faelle:
        fach, grund = einordnen(f)
        if fach != soll_fach:
            fehler.append(f"{f['id']}: {fach} statt {soll_fach}")
        elif stich.lower() not in grund.lower():
            fehler.append(f"{f['id']}: Begruendung nennt {stich!r} nicht")

    # Unvollstaendige Funde muessen auffallen, nicht durchrutschen.
    if not pruefe({**basis, "id": "H", "kanal": "Spec", "beleg": ""}):
        fehler.append("ein Fund ohne Beleg wird angenommen")
    if not pruefe({**basis, "id": "I", "kanal": "Zauberei"}):
        fehler.append("ein erfundener Kanal wird angenommen")

    # Reihenfolge: D haengt an A, also muss A davor stehen.
    folge, ringe = reihenfolge([f for f, _, _ in faelle])
    if folge.index("A") > folge.index("D"):
        fehler.append("Reihenfolge stellt D vor A")
    if ringe:
        fehler.append(f"Ring erfunden: {ringe}")

    # Ein echter Ring MUSS gemeldet werden.
    ring = [{**basis, "id": "R1", "abhaengig_von": ["R2"], "kanal": "Spec",
             "kennzahl": "T3 runter"},
            {**basis, "id": "R2", "abhaengig_von": ["R1"], "kanal": "Spec",
             "kennzahl": "T3 runter"}]
    if not reihenfolge(ring)[1]:
        fehler.append("ein Ring wird stillschweigend aufgeloest")

    gesamt = len(faelle) + 5
    print(f"Selbsttest: {gesamt - len(fehler)}/{gesamt}")
    for f in fehler:
        print("  FAIL " + f)
    return 1 if fehler else 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("funde", nargs="?", help="JSON mit {'funde': [...]}")
    ap.add_argument("--md", action="store_true")
    ap.add_argument("--selbsttest", action="store_true")
    a = ap.parse_args()

    if a.selbsttest:
        return selbsttest()
    if not a.funde:
        ap.error("ohne Fund-Datei gibt es nichts einzuordnen "
                 "(oder --selbsttest)")
    with open(a.funde, encoding="utf-8") as f:
        d = json.load(f)
    funde = d.get("funde") or []
    if not funde:
        print("Keine Funde in der Datei — nichts einzuordnen.")
        return 1
    print(bericht(funde, a.md))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
