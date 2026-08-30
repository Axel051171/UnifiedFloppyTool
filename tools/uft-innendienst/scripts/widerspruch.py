#!/usr/bin/env python3
"""widerspruch.py — Rolle 5: haelt Doku-Aussagen gegen ihre Quellen.

    widerspruch.py --selbsttest
    widerspruch.py <uft-pfad>

Drei Pruefungen der Klasse "Vereinbarung ohne Leser / Behauptung ohne
Quelle" — die im Baum viermal aufgetreten ist (MF-633/651/671/678):

  1. "Generiert aus `X`" ⇒ X liegt im Baum UND wird ausserhalb der
     behauptenden Datei benutzt. Ein Erzeuger ohne Aufrufer ist eine
     Behauptung, kein Erzeuger.
  2. Deklarierte Marken (`config: marken_mit_leser`) ⇒ es gibt einen
     Leser in den genannten Pfaden. Eine Marke, die niemand liest, ist
     eine Vereinbarung mit sich selbst (MF-678: `<!-- stufe: 2 -->`).
  3. Ein Pfad, zwei Vereinbarungen: dieselbe `docs/`-Pfadangabe wird in
     mehr als einem Dokument mit "reserviert/gehoert/nur fuer" belegt
     (MF-689: der Namensraum-Konflikt um `docs/specs/`).
  4. Eine Agenten-Datei nennt einen Pfad, den es nicht gibt, ohne ihn
     als Vorschlag oder als Geschichte zu fuehren (MF-702).

── Pruefung 4 brauchte fuenf Anlaeufe, und das ist die Lehre ────────────

Der erste Lauf meldete **28** Befunde. Zehn davon waren echt. Die
uebrigen achtzehn waren Fehler des PRUEFERS, jeder von einer eigenen
Klasse:

| Anlauf | Fehlalarm-Klasse | Zahl |
|---|---|---|
| 1 | Bauplaene ("Optional als Nightly-Workflow …") | 3 |
| 2 | werkzeugkasten-relative Pfade (`playbook/…` unter `tools/uft-scout/`) | 4 |
| 3 | gitignorierte, aber vorhandene Verzeichnisse (`out/`, `work/`) | 5 |
| 4 | `tools/X/AGENT.md` — der Kasten IST das eigene Verzeichnis | 5 |
| 5 | Qualifier eine Zeile ueber dem Pfad (umbrochener Fliesstext) | 8 |

Danach kamen zwei NEUE Befunde dazu: die Berichtigungsnotizen, die den
alten falschen Pfad zitieren. Ein Pruefer, der das Protokoll seiner
eigenen Korrektur als Fehler meldet, zwingt dazu, die Korrektur
unkenntlich zu machen — darum die Historien-Klasse.

**28 → 10 echte.** Das Verhaeltnis ist die eigentliche Messung: wer
einen Zensus beim ersten Lauf glaubt, baut auf 64 % Rauschen.

── Warum der Selbsttest hier vorn steht (MF-693) ────────────────────────

Die Erstfassung rief `git ls-files` und `git grep` **im
Fixture-Verzeichnis**. Das ist kein Repository. git antwortete
`fatal: not a git repository`, die Dateiliste kam leer zurueck, der
Selbsttest fand **0 von 3** gepflanzten Faellen — waehrend das README
danebenstand und "Selbsttest 3/3" behauptete.

Damit war auch die Null des echten Laufs wertlos: ein Zaehlwerk, dessen
Beweis nicht feuert, kann nicht zwischen "nichts gefunden" und "nichts
gesucht" unterscheiden. Seit dieser Fassung kommt die Dateimenge aus
`baum.py` (git, mit sichtbarem Rueckfall), und die Suche laeuft
in-process statt ueber `git grep`.

Der Selbsttest laeuft daher **vor** jedem echten Lauf und bricht ihn ab.

── Was die Null nicht heisst ────────────────────────────────────────────

Diese drei Fragen sind dann leer — nicht die Nachbarfragen. Nicht
gesehen werden: eine Marke, deren Leser den Namen anders schreibt; ein
Erzeuger, der ueber eine Variable aufgerufen wird; eine Reservierung,
die ohne die drei Signalwoerter formuliert ist.
"""
from __future__ import annotations

import json
import os
import re
import sys

HIER = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HIER)

import baum  # noqa: E402

GEN_RX = re.compile(r"[Gg]ener(?:iert aus|ated from)\s+`?([\w./-]+)`?")
# Pruefung 4 (MF-702): eine Pfadangabe, die ins Leere zeigt.
#
# Gemessen an den 26 Agenten-Dateien: 109 Pfadangaben, **28** ohne
# Ziel. Darunter `single-source-enforcer.md`, das im INDIKATIV sagt:
# "Generator: `scripts/generators/gen_version_h.py` liest VERSION,
# emittiert die Header" — die Datei gibt es nicht. Ein Agent, der
# einen Generator beschreibt, den es nicht gibt, ist derselbe Fall
# wie eine Marke ohne Leser, nur eine Ebene hoeher.
#
# UNTERSCHIEDEN wird dabei: eine Zeile, die den Pfad ausdruecklich
# als Vorschlag fuehrt ("optional", "geplant", "koennte", "waere"),
# ist KEIN Befund. Ohne diese Unterscheidung meldete die Pruefung
# jeden Bauplan als Fehler — und `must-fix-hunter.md` schreibt
# korrekt "Optional als Nightly-Workflow ...".
# Zweite Ausnahme-Klasse: ein Pfad, der als GESCHICHTE genannt wird,
# nicht als Zusage ("hier stand `x`", "bis MF-702 hiess es `y`").
# Gemessen: nach dem Berichtigen der zehn Befunde meldete der Lauf
# zwei neue — meine eigenen Berichtigungsnotizen, die den alten,
# falschen Pfad zitieren. Ein Pruefer, der das Protokoll seiner
# eigenen Korrektur als Fehler meldet, zwingt dazu, die Korrektur
# unkenntlich zu machen. Das waere der schlechtere Baum.
HISTORIE_RX = re.compile(
    r"\b(hier stand|stand hier|bis MF-\d+|frueher|früher|entfernt|nie gab|hiess es|hieß es|Berichtigt)\b", re.I)

VORSCHLAG_RX = re.compile(
    r"\b(optional|geplant|vorgesehen|koennte|könnte|waere|wäre|kuenftig|künftig|spaeter|später|sobald|Zielbild|Vorschlag)\b", re.I)
# Ein Agent nennt seinen Werkzeugkasten selbst. Das wird GELESEN,
# nicht aus dem Namen geraten — und es war noetig: der erste Lauf
# meldete `playbook/lizenzmatrix.md`, `data/known_negatives.json`,
# `scripts/korpus_zensus.py` und `data/bearbeitet.json` als fehlend.
# Alle vier existieren, nur relativ zu `tools/uft-scout/` bzw.
# `tools/uft-variants/`. Vier Fehlalarme von 25 — und ein Pruefer,
# der korrekte Verweise zu "Berichtigungen" macht, richtet mehr
# Schaden an als die Drift, die er sucht.
KASTEN_RX = re.compile(r"Werkzeugkasten:?\*{0,2}\s*`([^`]+)`")

PFAD_RX = re.compile(
    r"`([A-Za-z0-9_./-]+\.(?:py|c|h|cpp|hpp|md|json|ya?ml|txt|pro))`")

RESV_RX = re.compile(r"(docs/[\w/]+/?)\S*.{0,60}?"
                     r"(reserviert|gehört|nur für|only for)", re.I)


def pruefe(root: str, cfg: dict) -> list[str]:
    befunde: list[str] = []
    alle = baum.dateien(root, mit_vendorten=True)
    md = [f for f in alle
          if f.endswith(".md") and (f.startswith(("docs/", "tools/"))
                                    or f == "README.md")]
    inhalt = baum.inhalt(root, md)

    # 1) "Generiert aus X" — Quelle da, und benutzt sie jemand?
    leser_pfade = tuple(cfg.get("erzeuger_leser_pfade",
                                ["scripts/", "tools/", ".github/"]))
    leser_dateien = [f for f in alle if f.startswith(leser_pfade)]
    leser_inhalt = baum.inhalt(root, leser_dateien)
    for f, text in inhalt.items():
        for m in GEN_RX.finditer(text):
            quelle = m.group(1)
            if not any(a.endswith(quelle) or quelle in a for a in alle):
                befunde.append(f"- {f}: behauptet 'generiert aus "
                               f"`{quelle}`' — Quelle NICHT im Baum")
                continue
            in_doku = any(quelle in t for g, t in inhalt.items() if g != f)
            in_werkzeug = any(quelle in t for g, t in leser_inhalt.items()
                              if g != f and not g.endswith(quelle))
            if not in_doku and not in_werkzeug:
                befunde.append(f"- {f}: 'generiert aus `{quelle}`' — die "
                               f"Quelle liegt im Baum, aber niemand ruft "
                               f"sie auf (Erzeuger nur behauptet)")

    # 2) Marken mit Leser
    for regel in cfg.get("marken_mit_leser", []):
        traeger = [f for f, t in inhalt.items() if regel["marke"] in t]
        if not traeger:
            continue
        pfade = tuple(regel["leser_pfade"])
        leser = [f for f in alle
                 if f.startswith(pfade) and f.endswith((".py", ".sh", ".yml"))
                 and regel["leser_muster"] in
                 baum.inhalt(root, [f]).get(f, "")]
        if not leser:
            befunde.append(f"- Marke `{regel['marke']}` steht in "
                           f"{len(traeger)} Datei(en) "
                           f"({', '.join(sorted(traeger)[:3])}), aber KEIN "
                           f"Leser unter {list(pfade)} — Vereinbarung ohne "
                           f"Leser")

    # 4) Pfadangabe ohne Ziel (MF-702)
    agenten = [f for f in alle
               if f.endswith(".md")
               and ("/agents/" in f or f.endswith("AGENT.md"))]
    im_baum = set(alle)
    verzeichnisse = {os.path.dirname(f) for f in alle}
    # Oberste Ebene des Baums: woran sich entscheidet, ob ein Pfad von
    # HIER aus ueberhaupt adressierbar ist.
    oberste = {f.split("/", 1)[0] for f in alle if "/" in f}
    for f, text in baum.inhalt(root, agenten).items():
        # Werkzeugkasten-relative Angaben: die Agenten nennen ihren
        # Kasten selbst ("**Werkzeugkasten:** `tools/uft-scout/`"), also
        # wird er gelesen und nicht aus dem Namen geraten.
        if f.endswith("AGENT.md") and "/" in f:
            # Die Betriebsanweisung LIEGT im Werkzeugkasten — der ist ihr
            # eigenes Verzeichnis, kein Textfund noetig. Ohne diese Zeile
            # meldete der Lauf fuenf eigene Skripte als fehlend, die
            # unmittelbar daneben liegen.
            kasten = os.path.dirname(f) + "/"
        else:
            m = KASTEN_RX.search(text)
            kasten = m.group(1).rstrip("/") + "/" if m else None
        zeilen = text.splitlines()
        for nr, zeile in enumerate(zeilen):
            # Fenster statt Einzelzeile: bei umbrochenem Fliesstext steht
            # der Qualifier oft eine Zeile vorher — "**Abgeleitet
            # (geplant):**" und dann die Pfade in der naechsten Zeile.
            # Gemessen: ohne Fenster meldete der Lauf acht Pfade, die
            # zwei Zeilen ueber sich ausdruecklich als geplant gefuehrt
            # sind.
            fenster = " ".join(zeilen[max(0, nr - 2):nr + 2])
            if VORSCHLAG_RX.search(fenster):
                continue          # ausdruecklich ein Bauplan, kein Ist
            if HISTORIE_RX.search(fenster):
                continue          # als Geschichte zitiert, nicht zugesagt
            for m2 in PFAD_RX.finditer(zeile):
                pf = m2.group(1)
                if "/" not in pf or "<" in pf or "*" in pf:
                    continue      # blosser Dateiname oder Glob
                if pf in im_baum or pf in verzeichnisse:
                    continue
                if kasten and (kasten + pf in im_baum
                               or kasten + pf in verzeichnisse):
                    continue      # relativ zum eigenen Werkzeugkasten
                if os.path.exists(os.path.join(root, pf)):
                    continue      # existiert auf der Platte, ist nur
                                  # gitignored (z.B. `out/`, `work/`) —
                                  # die Frage lautet "gibt es das", und
                                  # git-Verfolgung ist dafuer nur ein
                                  # Stellvertreter, der bei erzeugten
                                  # Verzeichnissen versagt
                if kasten and os.path.exists(
                        os.path.join(root, kasten + pf)):
                    continue
                if pf.split("/", 1)[0] not in oberste:
                    continue      # zeigt aus dem Baum hinaus (z.B.
                                  # `memory/` = Claude-Gedaechtnis) —
                                  # von hier aus nicht pruefbar, also
                                  # kein Befund
                befunde.append(f"- {f}: nennt `{pf}` — im Baum nicht "
                               f"vorhanden, und die Zeile fuehrt es nicht "
                               f"als Vorschlag")

    # 3) Ein Pfad, zwei Vereinbarungen
    resv: dict[str, set[str]] = {}
    for f, text in inhalt.items():
        for m in RESV_RX.finditer(text):
            resv.setdefault(m.group(1), set()).add(f)
    for pfad, quellen in resv.items():
        if len(quellen) > 1:
            befunde.append(f"- Pfad `{pfad}` wird in {len(quellen)} "
                           f"Dokumenten belegt: {sorted(quellen)} — "
                           f"Namensraum-Konflikt-Kandidat")
    return befunde


def selbsttest() -> int:
    fx = os.path.join(HIER, "..", "data", "widerspruch_fixtures")
    soll = json.load(open(os.path.join(fx, "erwartung.json"),
                          encoding="utf-8"))
    b = pruefe(fx, soll["config"])
    for w in baum.warnungen():
        print("  HINWEIS: " + w)
    print(f"  Selbsttest: {len(b)} Befunde (Soll {soll['soll']})")
    for z in b:
        print("   " + z)
    if len(b) != soll["soll"]:
        print("  Der Beweis feuert nicht wie erwartet — das Werkzeug "
              "kann 'nichts gefunden' nicht von 'nichts gesucht' "
              "unterscheiden (AGENT.md Regel 3).")
        return 3
    return 0


def main() -> int:
    cfgp = os.path.join(HIER, "..", "config_innendienst.json")
    cfg = json.load(open(cfgp, encoding="utf-8")) \
        if os.path.exists(cfgp) else {}
    if "--selbsttest" in sys.argv:
        return selbsttest()
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    print("Selbsttest an den gepflanzten Faellen:")
    if selbsttest() != 0:
        return 3
    print()
    b = pruefe(sys.argv[1], cfg)
    for w in baum.warnungen():
        print("HINWEIS: " + w)
    if b:
        print(f"{len(b)} Widerspruchs-Befunde:")
        for z in b[:15]:
            print(z)
        if len(b) > 15:
            print(f"… {len(b) - 15} weitere")
    else:
        print("0 Widerspruchs-Befunde. Was die Null NICHT heisst: diese "
              "drei Fragen sind leer, nicht die Nachbarfragen — eine "
              "Marke mit anders geschriebenem Leser, ein Erzeuger hinter "
              "einer Variablen und eine Reservierung ohne die drei "
              "Signalwoerter bleiben unsichtbar.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
