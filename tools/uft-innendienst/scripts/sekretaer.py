#!/usr/bin/env python3
"""sekretaer.py — Rolle 6: baut den Sitzungszettel der
Eigentuemer-Entscheidungen.

    sekretaer.py <uft-pfad> [-o out/sitzung.md]

Sammelt, was auf eine Entscheidung wartet, und formt jeden Posten auf
**Frage · Messung · Empfehlung · Folge**. Felder, die er nicht belegen
kann, bleiben als `AUSZUFUELLEN` stehen — er erfindet keine Empfehlung.
Ziel: vier Tore fallen in einer Sitzung statt in vierzehn Nachrichten.

Drei Quellen, alle bereits im Baum vorhanden:

  (a)  `docs/OPEN_ITEMS.md` — Tabellenzeilen, deren Text auf den
       Eigentuemer zeigt (Muster in `WARTE_RX`).
  (a2) die **Status-Marke** je Abschnitt (MF-694):
       `<!-- status: wartet-eigentuemer(2026-08-30) -->`. Das Datum ist
       Pflicht — daraus staffelt der Sekretaer nach Alter, und genau
       das ist seine Aufgabe. Abschnitte ohne Marke gelten als **noch
       nicht gesichtet** und erscheinen als EINE Rueckstands-Zeile,
       nicht als Posten.
  (b)  Gutachten **ohne Spur in OPEN_ITEMS.md** — geliefert von
       `scripts/scout_stand.py:ohne_spur()`, nicht selbst nachgebaut.

── Warum (b) importiert und nicht nachgebaut wird (MF-693) ──────────────

Die Erstfassung stellte dieselbe Frage ueber einen eigenen Ersatz:
"Gutachten ohne `<!-- stufe: -->`-Marke". Gemessen tragen **vier** von
rund dreissig Gutachten diese Marke. Der Ersatz haette also rund
fuenfundzwanzig Posten als offene Entscheidung auf den Zettel gesetzt,
von denen die meisten laengst abgearbeitet sind — ein Zettel, den man
nach dem dritten falschen Posten weglegt.

`scout_stand.py` beantwortet dieselbe Frage seit MF-675 schaerfer
(Spur in OPEN_ITEMS.md, ueber den vollen Bezeichner). Zwei Fassungen
einer Frage sind eine driftende Zahl; also gibt es hier nur eine.

── Was der Zettel NICHT sieht (gemessen, MF-693) ────────────────────────

Quelle (a) liest nur **Tabellenzeilen** aus `docs/OPEN_ITEMS.md`. Die
Liste besteht inzwischen zum groessten Teil aus Prosa-Abschnitten, und
ein Abschnitt wie `ORPH-5` mit der Ueberschrift „Entscheidung, die
ansteht (Eigentuemer)" faellt darum durch.

Der naheliegende Fix waere schlechter als die Luecke. Gemessen: von 65
`##`-Abschnitten enthalten **36** ein Eigentuemer-Signal — die meisten
davon in erledigten Vorgaengen („SCOUT-12 erledigt", „LIZ-3
entschieden", „FMT-13 geschlossen"). Ein Prosa-Scan haette den Zettel
von 21 auf ueber 50 Posten aufgeblaeht, groesstenteils mit
abgeschlossener Arbeit — genau der Anschlag-ohne-Treffer, wegen dessen
der `<!-- stufe: -->`-Ersatz schon verworfen wurde.

Geschlossen wurde die Luecke seit MF-694 nicht durch mehr Regex,
sondern durch eine **Konvention**: die Status-Marke (a2). Sie wird von
Hand vergeben, je Abschnitt, mit Begruendung im Commit — auch die
Ueberschrift taeuscht naemlich: 17 Koepfe enthalten „erledigt" oder
„geschlossen", aber „GUI-6 — 38 Bedienelemente in einer
**geschlossenen** Schleife" ist kein abgeschlossener Vorgang. Eine
falsch gesetzte Marke schliesst einen Vorgang still; das ist schlimmer
als keine.

Was der Zettel darum weiterhin nicht sieht: alles, was noch keine Marke
hat. Er sagt aber, **wie viel** das ist.

── Was der Zettel nicht ist ─────────────────────────────────────────────

Keine Rangfolge. Die Gewichtung ist ein Risiko-Urteil und bleibt beim
Menschen (AGENT.md, "bewusst NICHT gebaut"). Der Sekretaer staffelt
nach Herkunft und Reihenfolge im Dokument, nicht nach Wichtigkeit.
"""
from __future__ import annotations

import importlib.util
import os
import re
import sys
from datetime import date

HIER = os.path.dirname(os.path.abspath(__file__))

WARTE_RX = re.compile(
    r"(Eigent|deine Entscheidung|wartet auf dein|Vorlage|Owner|"
    r"Klick-?Abnahme|Lizenzkl)", re.I)

# Die Status-Marke aus `docs/OPEN_ITEMS.md` (MF-694). Sie steht
# unmittelbar unter der Abschnitts-Ueberschrift und traegt bei
# `wartet-eigentuemer` ein Pflicht-Datum — ohne das kann der Sekretaer
# Entscheidungsschulden nicht altersgestaffelt mahnen, und genau das ist
# seine Aufgabe.
MARKE_RX = re.compile(r"<!--\s*status:\s*([^>]*?)\s*-->")
WARTET_RX = re.compile(r"wartet-eigentuemer\s*\(\s*([^)]*?)\s*\)")


def abschnitte(text: str):
    """(Ueberschrift, Marke oder None) je `##`-Abschnitt.

    Gesucht wird die Marke NUR in den Zeilen direkt unter der
    Ueberschrift, bis zur ersten nicht-leeren Nicht-Marken-Zeile. Eine
    Marke tiefer im Fliesstext gehoert nicht zum Abschnitt — sonst
    faerbte ein zitiertes Beispiel den ganzen Vorgang um.
    """
    zeilen = text.splitlines()
    aus = []
    for i, z in enumerate(zeilen):
        if not z.startswith("## "):
            continue
        marke = None
        for w in zeilen[i + 1:i + 4]:
            if not w.strip():
                continue
            m = MARKE_RX.search(w)
            if m:
                marke = m.group(1)
            break
        aus.append((z[3:].strip(), marke))
    return aus


def alter_in_tagen(datum: str, heute: date) -> int | None:
    try:
        j, mo, t = (int(x) for x in datum.split("-"))
        return (heute - date(j, mo, t)).days
    except (ValueError, TypeError):
        return None


def _scout_stand(root: str):
    p = os.path.join(root, "scripts", "scout_stand.py")
    if not os.path.exists(p):
        return None
    spec = importlib.util.spec_from_file_location("uft_scout_stand", p)
    if spec is None or spec.loader is None:
        return None
    m = importlib.util.module_from_spec(spec)
    try:
        spec.loader.exec_module(m)
    except Exception:
        return None
    return m


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    root = sys.argv[1]
    out = os.path.join(HIER, "..", "out", "sitzung.md")
    if "-o" in sys.argv:
        out = sys.argv[sys.argv.index("-o") + 1]

    posten: list[tuple[str, str, str]] = []
    hinweise: list[str] = []
    unmarkiert = 0

    oi_pfad = os.path.join(root, "docs", "OPEN_ITEMS.md")
    liste = ""
    if os.path.exists(oi_pfad):
        liste = open(oi_pfad, encoding="utf-8", errors="replace").read()
        for nr, zeile in enumerate(liste.splitlines(), 1):
            if (zeile.lstrip().startswith("|") and WARTE_RX.search(zeile)
                    and "✅" not in zeile):
                kurz = " ".join(zeile.split("|")[1:3]).strip()
                posten.append(("OPEN_ITEMS", f"Z.{nr}", kurz[:110]))

        # Quelle (a2): die Status-Marke je Abschnitt (MF-694).
        heute = date.today()
        for kopf, marke in abschnitte(liste):
            if marke is None:
                unmarkiert += 1
                continue
            m = WARTET_RX.match(marke)
            if not m:
                continue                      # offen / erledigt: kein Posten
            tage = alter_in_tagen(m.group(1), heute)
            alt = f"{tage} Tage" if tage is not None else \
                  f"Datum `{m.group(1)}` unlesbar"
            posten.append((f"wartet auf Eigentuemer ({alt})", kopf[:70],
                           "Entscheidung steht im Abschnitt"))
    else:
        hinweise.append("`docs/OPEN_ITEMS.md` fehlt — Quelle (a) leer.")

    st = _scout_stand(root)
    if st is None:
        hinweise.append("`scripts/scout_stand.py` nicht ladbar — Quelle "
                        "(b) leer. Der Zettel ist damit unvollstaendig, "
                        "nicht kurz.")
    else:
        try:
            gut = st.gutachten()
            for name in st.ohne_spur(gut, liste):
                posten.append(("Gutachten ohne Spur in OPEN_ITEMS", name,
                               "uebernehmen, ablehnen oder als Fundus "
                               "ablegen?"))
        except Exception as e:                      # noqa: BLE001
            hinweise.append(f"scout_stand.py lief nicht durch: {e}")

    # Aelteste Entscheidungsschuld zuerst — das ist die Staffelung,
    # fuer die das Pflicht-Datum in der Marke da ist.
    def rang(p):
        m = re.match(r"wartet auf Eigentuemer \((\d+) Tage\)", p[0])
        return (0, -int(m.group(1))) if m else (1, 0)
    posten.sort(key=rang)

    z = [f"# Tore-Sitzung — Zettel vom {date.today().isoformat()}",
         f"{len(posten)} Posten aus zwei Quellen: `docs/OPEN_ITEMS.md` "
         f"(nur Tabellenzeilen) und `scripts/scout_stand.py:ohne_spur()`.",
         "",
         f"**Triage-Rückstand:** {unmarkiert} `##`-Abschnitte in "
         f"OPEN_ITEMS.md tragen **keine** Status-Marke. Unmarkiert "
         f"heisst „noch nicht gesichtet\" — weder offen noch erledigt. "
         f"Sie stehen hier bewusst als EINE Zeile und nicht als "
         f"{unmarkiert} Posten: ein Prosa-Scan nach „Eigentümer\" "
         f"träfe 36 von 65 Abschnitten, die meisten davon erledigt, "
         f"und machte den Zettel unbrauchbar (MF-693/694).",
         "",
         "Format je Posten: **Frage · Messung · Empfehlung · Folge**. "
         "`AUSZUFUELLEN` heisst: steht in der verlinkten Quelle, ist "
         "aber nicht maschinell entnehmbar — der Sekretaer erfindet "
         "nichts.",
         "",
         "**Dies ist keine Rangfolge.** Die Gewichtung ist ein "
         "Risiko-Urteil und bleibt beim Menschen; gestaffelt wird nach "
         "Herkunft, nicht nach Wichtigkeit.",
         ""]
    for h in hinweise:
        z += [f"> HINWEIS: {h}", ""]
    for i, (art, ort, kurz) in enumerate(posten, 1):
        z += [f"## {i} · [{art}] {ort}",
              f"- Frage: {kurz}",
              "- Messung: AUSZUFUELLEN (aus der Quelle uebernehmen)",
              "- Empfehlung: AUSZUFUELLEN (aus der Quelle uebernehmen)",
              "- Folge bei Ja / bei Nein: AUSZUFUELLEN", ""]

    os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
    open(out, "w", encoding="utf-8").write("\n".join(z) + "\n")
    for h in hinweise:
        print("HINWEIS: " + h)
    print(f"OK: {len(posten)} Entscheidungs-Posten -> {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
