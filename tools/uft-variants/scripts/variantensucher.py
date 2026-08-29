#!/usr/bin/env python3
"""variantensucher.py — Stufe 2: Wo verzweigen unabhaengige
Implementierungen auf Versionsfelder eines Formats?

  variantensucher.py <format> [-o work/<format>.evidenz.json]

Durchsucht die in config.json registrierten Referenz-Klone nach
versions-diskriminierendem Code fuer das Format: Magic-Strings,
Versions-/Revisions-Felder, Verzweigungen, Deprecation-Marker.
Zaehlt UNABHAENGIGE Quellen je Indiz (Zwei-Quellen-Regel).

Ausgabe ist EVIDENZ, kein Urteil: Fundstellen mit Datei:Zeile.
Das Urteil (echte Variante vs. interner Versionszaehler) faellt die
Tiefenpruefung im Dossier.

── Drei Korrekturen gegenueber dem ersten Entwurf ──────────────────────

Der erste HFE-Lauf hat `adf_ext/uft_adf_ext.c:157` als HFE-**Magic**
gemeldet. Die Zeile lautet `{ "Write", UFT_FEATURE_UNSUPPORTED,`. Sie
hat mit HFE nichts zu tun.

Der Schaden war nicht der Laerm. Das Indiz `magic` stand damit bei ZWEI
Quellen (samdisk + uft_selbst) und wurde als **MESSBAR-faehig**
ausgewiesen — der Fehltreffer hat genau die zweite Quelle ERZEUGT, die
die Zwei-Quellen-Regel verlangt. Ein Pruefer, der sich selbst die
Belege herstellt, prueft nichts.

Ursache, gemessen: `adf_ext/uft_adf_ext.c:9` erwaehnt HFE in einem
Prosakommentar („like the G64/HFE bitstream"). Die Dateiauswahl liess
die Datei damit zu, und danach feuerten die Zeilenmuster auf allem, was
darin steht. `"[A-Z0-9]{4,10}"` trifft baumweit 248-mal in
`src/formats/` — jedes grossgeschriebene Wort in Anfuehrungszeichen.

Daraus drei Regeln:

1. **Die Fundzeile muss selbst zum Format gehoeren.** Ein Suchwort des
   Formats muss in der Zeile oder ihrem unmittelbaren Umfeld stehen.
   Das erschlaegt beide Fehler auf einmal, ohne eine neue Heuristik zu
   erfinden.
2. **Der eigene Baum ist keine unabhaengige Quelle.** `uft_selbst`
   zaehlt nie zur Zwei-Quellen-Regel; es wird getrennt gefuehrt, weil
   es fuer eine ANDERE Frage gebraucht wird (widersprechen sich zwei
   Leser im eigenen Baum? — AGENT.md Regel 2).
3. **Was uebersprungen wird, wird gesagt.** Fehlende Klone und
   abgeschnittene Dateilisten stehen in der Ausgabe. Eine Quelle, die
   nicht existiert, darf keine Regel erfuellen; ein stilles Limit ist
   die Aufzaehlungsfalle in klein (MF-567/578/598/633/651/652).
"""
import json
import os
import re
import subprocess
import sys
from collections import defaultdict

HIER = os.path.dirname(os.path.abspath(__file__))
WURZEL = os.path.abspath(os.path.join(HIER, "..", "..", ".."))
CFG = json.load(open(os.path.join(HIER, "..", "config.json"),
                     encoding="utf-8"))

MAX_DATEIEN = 12
"""Wie viele Dateien je Quelle durchsucht werden. Die Grenze bleibt —
sie haelt den Lauf schnell —, aber sie wird jetzt GEMELDET."""

UMFELD = 2
"""Zeilen ober- und unterhalb, die noch als 'gehoert zum Format' gelten.
Zwei, weil ein Feldkommentar oft ueber der Zeile steht."""

# Indiz-Muster: (name, regex) — bewusst breit, die Tiefenpruefung siebt.
# Das Magic-Muster ist enger als im Entwurf: mindestens ein Ziffer- oder
# Sonderzeichenanteil ODER Laenge >= 6, damit "Write"/"READ" nicht mehr
# als Magic durchgehen. Der eigentliche Schutz ist aber die
# Format-Naehe-Regel unten, nicht dieses Muster.
MUSTER = [
    ("magic",       r'"[A-Z][A-Z0-9 _\-]{4,11}"'),
    ("versionfeld", r"\b(format_?rev(ision)?|version|revision)\b"),
    ("verzweigung", r"(if|switch|case|match).{0,40}\b(ver|rev)"),
    ("deprecated",  r"deprecat|obsolet|legacy|old.?format"),
    ("v_literal",   r"\bv[0-9]([._][0-9])?\b"),
]


def lauf(cmd, cwd):
    try:
        return subprocess.run(cmd, cwd=cwd, capture_output=True,
                              text=True, timeout=120)
    except (OSError, subprocess.SubprocessError):
        class _Leer:
            stdout = ""
        return _Leer()


def _nah_am_format(zeilen, nr, suchworte_klein):
    """Steht ein Suchwort des Formats in der Zeile oder ihrem Umfeld?

    Das ist die Regel, an der der erste Entwurf gescheitert ist. Sie
    kostet Treffer — und genau die, die nichts belegen.
    """
    lo = max(0, nr - 1 - UMFELD)
    hi = min(len(zeilen), nr + UMFELD)
    fenster = " ".join(zeilen[lo:hi]).lower()
    return any(w in fenster for w in suchworte_klein)


def suche(fmt):
    fmt_cfg = CFG["formate"].get(fmt.lower(), {})
    suchworte = fmt_cfg.get("suchworte", [fmt])
    suchworte_klein = [w.strip('"').lower() for w in suchworte if w.strip('"')]
    eigene = set(CFG.get("eigene_quellen", []))

    evidenz = defaultdict(lambda: defaultdict(list))
    quellen_je_indiz = defaultdict(set)
    fehlende_quellen = []
    abgeschnitten = {}
    verworfen = 0

    for quelle, pfad_rel in CFG["referenz_klone"].items():
        if quelle.startswith("_"):          # Kommentarschluessel
            continue
        pfad = os.path.join(WURZEL, pfad_rel)
        if not os.path.isdir(pfad):
            fehlende_quellen.append({"quelle": quelle, "pfad": pfad_rel})
            continue

        # 1) Dateien finden, die das Format ueberhaupt erwaehnen
        dateien = set()
        for w in suchworte:
            r = lauf(["git", "grep", "-il", w,
                      "--", "*.c", "*.cc", "*.cpp", "*.h", "*.hpp",
                      "*.py", "*.textpb", "*.md"], pfad)
            dateien.update(x for x in r.stdout.splitlines() if x)
        if not dateien:
            continue
        geordnet = sorted(dateien)
        if len(geordnet) > MAX_DATEIEN:
            abgeschnitten[quelle] = {"gefunden": len(geordnet),
                                     "durchsucht": MAX_DATEIEN}

        # 2) In diesen Dateien nach Versions-Indizien suchen —
        #    aber nur auf Zeilen, die zum Format gehoeren.
        for datei in geordnet[:MAX_DATEIEN]:
            voll = os.path.join(pfad, datei)
            try:
                zeilen = open(voll, encoding="utf-8",
                              errors="replace").read().splitlines()
            except OSError:
                continue
            for nr, zeile in enumerate(zeilen, 1):
                for name, rx in MUSTER:
                    if not re.search(rx, zeile, re.I):
                        continue
                    if not _nah_am_format(zeilen, nr, suchworte_klein):
                        verworfen += 1
                        continue
                    evidenz[name][quelle].append(
                        f"{datei}:{nr}: {zeile.strip()[:110]}")
                    quellen_je_indiz[name].add(quelle)

    # Kuerzen + Unabhaengigkeits-Zaehlung
    out = {
        "format": fmt,
        "suchworte": suchworte,
        "fehlende_quellen": fehlende_quellen,
        "abgeschnittene_quellen": abgeschnitten,
        "verworfen_weil_nicht_am_format": verworfen,
        "indizien": {},
    }
    for name, je_quelle in evidenz.items():
        alle = quellen_je_indiz[name]
        fremd = sorted(alle - eigene)
        out["indizien"][name] = {
            "unabhaengige_quellen": fremd,
            "eigener_baum": sorted(alle & eigene),
            # Die Regel zaehlt NUR fremde Quellen.
            "MESSBAR_faehig": len(fremd) >= 2,
            "fundstellen": {q: v[:6] for q, v in je_quelle.items()},
        }
    return out


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    fmt = sys.argv[1]
    ev = suche(fmt)
    out = os.path.join(HIER, "..", "work", f"{fmt.lower()}.evidenz.json")
    if "-o" in sys.argv:
        out = sys.argv[sys.argv.index("-o") + 1]
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    with open(out, "w", encoding="utf-8") as f:
        json.dump(ev, f, ensure_ascii=False, indent=1)

    n2 = sum(1 for i in ev["indizien"].values() if i["MESSBAR_faehig"])
    print("OK %s: %d Indiz-Klassen, %d mit >=2 UNABHAENGIGEN Quellen "
          "(eigener Baum zaehlt nicht) -> %s"
          % (fmt, len(ev["indizien"]), n2, out))
    print("   %d Treffer verworfen, weil die Zeile nicht zum Format gehoert"
          % ev["verworfen_weil_nicht_am_format"])
    if ev["fehlende_quellen"]:
        print("   FEHLENDE Quellen (zaehlen nirgends mit): %s"
              % ", ".join(q["quelle"] for q in ev["fehlende_quellen"]))
    if ev["abgeschnittene_quellen"]:
        for q, d in ev["abgeschnittene_quellen"].items():
            print("   ABGESCHNITTEN %s: %d Dateien gefunden, %d durchsucht"
                  % (q, d["gefunden"], d["durchsucht"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
