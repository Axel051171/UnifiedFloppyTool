#!/usr/bin/env python3
"""sekretaer.py — Rolle 6: baut den Sitzungszettel der
Eigentuemer-Entscheidungen.

    sekretaer.py <uft-pfad> [-o out/sitzung.md]

Sammelt, was auf eine Entscheidung wartet, und formt jeden Posten auf
**Frage · Messung · Empfehlung · Folge**. Felder, die er nicht belegen
kann, bleiben als `AUSZUFUELLEN` stehen — er erfindet keine Empfehlung.
Ziel: vier Tore fallen in einer Sitzung statt in vierzehn Nachrichten.

Zwei Quellen, beide bereits im Baum vorhanden:

  (a) `docs/OPEN_ITEMS.md` — Zeilen, deren Text auf den Eigentuemer
      zeigt (Muster in `WARTE_RX`).
  (b) Gutachten **ohne Spur in OPEN_ITEMS.md** — geliefert von
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

Die Luecke bleibt darum **benannt statt zugekleistert**. Wer sie
schliesst, braucht ein Erledigt-Signal je Abschnitt, das der Zensus
lesen kann — das ist eine Aenderung an OPEN_ITEMS.md, nicht an diesem
Skript, und eine Eigentuemer-Entscheidung.

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

    oi_pfad = os.path.join(root, "docs", "OPEN_ITEMS.md")
    liste = ""
    if os.path.exists(oi_pfad):
        liste = open(oi_pfad, encoding="utf-8", errors="replace").read()
        for nr, zeile in enumerate(liste.splitlines(), 1):
            if (zeile.lstrip().startswith("|") and WARTE_RX.search(zeile)
                    and "✅" not in zeile):
                kurz = " ".join(zeile.split("|")[1:3]).strip()
                posten.append(("OPEN_ITEMS", f"Z.{nr}", kurz[:110]))
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

    z = [f"# Tore-Sitzung — Zettel vom {date.today().isoformat()}",
         f"{len(posten)} Posten aus zwei Quellen: `docs/OPEN_ITEMS.md` "
         f"(nur Tabellenzeilen) und `scripts/scout_stand.py:ohne_spur()`.",
         "",
         "**Was der Zettel nicht sieht:** Prosa-Abschnitte in "
         "OPEN_ITEMS.md. Gemessen tragen 36 von 65 `##`-Abschnitten ein "
         "Eigentümer-Signal, die meisten davon in **erledigten** "
         "Vorgängen — sie mitzunehmen hätte den Zettel verdreifacht und "
         "unbrauchbar gemacht. Die Lücke ist benannt, nicht "
         "geschlossen (siehe Kopf von `sekretaer.py`).",
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
