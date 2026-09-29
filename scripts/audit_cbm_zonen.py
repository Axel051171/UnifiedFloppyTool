#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Tor 60: die CBM-Zonenlaengen und ihre Zaehlweisen — gezaehlt, nicht
erinnert (MF-932, P3-150).

── DER ANLASS ────────────────────────────────────────────────────────

Eine 1541 hat vier Zonen: Spuren 1–17 tragen 21 Sektoren, 18–24 tragen
19, 25–30 tragen 18, 31–42 tragen 17. Diese vier Zahlen liegen im Baum
vielfach herum, und NICHT in einer gemeinsamen Zaehlweise.

P3-150 hielt seit MF-877 fest: „ELFFACH im Baum, in VIER Zaehlweisen."
Das war eine Handzaehlung, und sie ist abgedriftet. Gemessen (MF-932):
**23 Fundstellen in NEUN Zaehlweisen**.

Das ist der fuenfzehnte Fall von Aufzaehlung statt Messung in diesem
Baum — und der Punkt, den P3-150 selbst macht, wird dadurch nur
staerker: **erst das Tor, dann das Zusammenfuehren.** Vier (in Wahrheit
neun) Zaehlweisen sind nicht mechanisch ineinander ueberfuehrbar. Wer
ohne vorherige Messung aufraeumt, fuehrt still eine zehnte ein.

Zwei Kopien haben bereits nachweislich falsch gelesen: P3-148 und
P3-149.

── WAS GEZAEHLT WIRD ─────────────────────────────────────────────────

Feld-Initialisierer (`NAME[...] = { ... }`) im kommentar- und
zeichenkettenfreien Text, deren Zahlen

  * alle vier Zonenwerte 21/19/18/17 enthalten, UND
  * ausser diesen nur noch 0 enthalten (als Platzhalter fuer den
    unbenutzten Index 0 einer 1-basierten Spurtabelle).

Die zweite Bedingung ist der Filter, der traegt: ohne sie melden auch
`skew_6` (CP/M-Verzahnung), `gcr62_decode` (NIB), `twiggy_spt` (Lisa)
und eine 66-Eintraege-Tabelle mit 255 — Tabellen, die dieselben Zahlen
zufaellig enthalten. Gemessen: 27 Kandidaten roh, 23 nach dem Filter.

Die ZAEHLWEISE ergibt sich aus Laenge und erstem Wert:

    4 Eintraege, 21 zuerst    Zone 0 = Spuren 1–17   (aufsteigend)
    4 Eintraege, 17 zuerst    Zone 0 = Spuren 31–42  (absteigend)
    n Eintraege, 21 zuerst    Index 0 = Spur 1       (0-basiert)
    n Eintraege, 0 zuerst     Index 0 unbenutzt      (1-basiert)

── WAS DIESES TOR NICHT TUT ──────────────────────────────────────────

Es fuehrt nichts zusammen und urteilt nicht darueber, welche Zaehlweise
richtig ist — beides waere eine Aenderung, keine Messung. Es haelt nur
fest, WIE VIELE es gibt, damit eine neue auffaellt.

Es sieht auch keine Zonenlogik, die als `if`-Kette statt als Tabelle
geschrieben ist. Das steht hier, weil es eine echte Luecke ist: eine
zehnte Zaehlweise koennte sich so dem Tor entziehen.

── Grundlinie ────────────────────────────────────────────────────────

Die Grundlinie ist seit MF-1504 ein MANIFEST benannter Fundstellen
(`scripts/cbm_zonen_grundlinie.json`, Schluessel `datei::symbol`), nicht
mehr eine Anzahl. Der Grund ist gemessen, nicht ausgedacht: gegen eine
ZAHL besteht ein Baum das Tor auch dann, wenn eine alte Kopie
verschwindet und eine NEUE dazukommt — netto unveraendert, Regel
verletzt, niemand merkt es. Vorgefuehrt an einem Wegwerf-Baum:

    A  23 Fundstellen                                   -> OK
    B  23 Fundstellen, 1 NEU, 1 entfallen               -> OK   (!)
       namentlich: +frisch_angelegte_tafel, -kopie_22

Mit dem Manifest gilt:

  * Fundstelle NICHT im Manifest        -> FAIL, mit Namen
  * Fundstelle im Manifest verschwunden -> OK, plus Hinweis zum Kuerzen
  * neue Zaehlweise                     -> FAIL (eigene Klasse, s.o.)

Damit kann die Grundlinie nur sinken, und zwar sichtbar als Diff im
Commit — die Richtung aus MF-1077, ohne dass jemand eine Zahl anfasst.
Gekuerzt wird mit `--grundlinie-schreiben`; das ist Absicht und steht
dann im Diff.

**Das ist KEINE gepflegte Liste im Sinne von MF-636**, und der
Unterschied ist der ganze Punkt: die PRUEFMENGE kommt weiterhin aus
`git ls-files` — das Manifest waehlt nicht aus, was geprueft wird,
sondern haelt fest, was beim letzten Mal gefunden WURDE. Es wird
erzeugt, nicht getippt. Eine von Hand gepflegte Auswahlliste veraltet
still (viermal belegt); ein erzeugter Abzug, der nur schrumpfen darf,
kann das nicht.

Die frueheren Zahlen (23 Fundstellen, 9 Zaehlweisen) bleiben als
Anzeige erhalten, aber sie faellen kein Urteil mehr.
"""
from __future__ import annotations

import io
import json
import re
import subprocess
import sys
from pathlib import Path
from git_env import git_umgebung

# Nur noch Anzeige — das Urteil faellt das Manifest (MF-1504).
GRUNDLINIE_STELLEN = 23
GRUNDLINIE_WEISEN = 9

MANIFEST = Path(__file__).resolve().parent / "cbm_zonen_grundlinie.json"


def schluessel(rel: str, name: str) -> str:
    return "%s::%s" % (rel.replace("\\", "/"), name)


def manifest_lesen(pfad: Path = MANIFEST) -> dict | None:
    """-> {schluessel: zaehlweise}, oder None wenn es keins gibt."""
    try:
        roh = json.loads(pfad.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return None
    return {e["stelle"]: e.get("zaehlweise", "") for e in roh.get("stellen", [])}


def manifest_schreiben(stellen: list, pfad: Path = MANIFEST) -> int:
    inhalt = {
        "_": ("Grundlinie des CBM-Zonen-Tors (MF-1504). ERZEUGT, nicht "
              "getippt: `python scripts/audit_cbm_zonen.py "
              "--grundlinie-schreiben`. Die Pruefmenge kommt aus "
              "`git ls-files` (MF-636); diese Datei haelt nur fest, was "
              "beim letzten Mal gefunden wurde. Sie darf nur SCHRUMPFEN "
              "— die Richtung ist Zusammenfuehrung auf "
              "`uft_cbm_track_capacity()`."),
        "stellen": [{"stelle": schluessel(rel, name), "zaehlweise": w}
                    for rel, name, w in sorted(stellen)],
    }
    pfad.write_text(json.dumps(inhalt, indent=2, ensure_ascii=False) + "\n",
                    encoding="utf-8")
    return len(inhalt["stellen"])

ZONEN = {21, 19, 18, 17}
ERLAUBT = ZONEN | {0}

FELD = re.compile(r"(\w+)\s*\[[^\]]*\]\s*(?:\[[^\]]*\]\s*)?=\s*\{([^{}]*)\}",
                  re.S)


def entkerne(t: str) -> str:
    t = re.sub(r"/\*.*?\*/", " ", t, flags=re.S)
    t = re.sub(r"//[^\n]*", " ", t)
    t = re.sub(r'"(\\.|[^"\\])*"', '""', t)
    return t


def dateien(repo: Path):
    try:
        aus = subprocess.run(
            ["git", "ls-files", "--cached", "--others", "--exclude-standard"],
            cwd=str(repo), capture_output=True, text=True, timeout=60)
    except (OSError, subprocess.SubprocessError):
        return None
    if aus.returncode != 0:
        return None
    return [f for f in aus.stdout.split("\n")
            if f.endswith((".c", ".h", ".cpp", ".hpp"))]


def zaehlweise(zahlen: list) -> str:
    n = len(zahlen)
    erst = zahlen[0]
    if n == 4:
        if erst == 21:
            return "4 Zonen, aufsteigend (Zone 0 = Spur 1-17)"
        if erst == 17:
            return "4 Zonen, absteigend (Zone 0 = Spur 31-42)"
        return "4 Zonen, beginnt mit %d" % erst
    if erst == 0:
        return "%d Eintraege, 1-basiert (Index 0 unbenutzt)" % n
    if erst == 21:
        return "%d Eintraege, 0-basiert (Index 0 = Spur 1)" % n
    return "%d Eintraege, beginnt mit %d" % (n, erst)


def messe(repo: Path):
    """-> (fundstellen, fehler); fundstellen ist None ohne git."""
    pfade = dateien(repo)
    if pfade is None:
        return None, ["Guard-frei: `git ls-files` war nicht befragbar, "
                      "dieses Tor hat NICHTS geprueft."]

    gefunden = []
    for rel in pfade:
        try:
            roh = (repo / rel).read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        if "21" not in roh:
            continue
        t = entkerne(roh)
        for m in FELD.finditer(t):
            zahlen = [int(x) for x in re.findall(r"\b\d+\b", m.group(2))]
            if not zahlen:
                continue
            menge = set(zahlen)
            if not ZONEN.issubset(menge) or (menge - ERLAUBT):
                continue
            gefunden.append((rel, m.group(1), zaehlweise(zahlen)))
    return sorted(gefunden), []


def check(repo, hinweise: list | None = None,
          manifest: Path = MANIFEST) -> list:
    """-> Fehlerliste. `hinweise` nimmt, wenn gegeben, die nicht
    urteilsrelevanten Beobachtungen auf (verschwundene Stellen usw.)."""
    if hinweise is None:
        hinweise = []
    stellen, fehler = messe(Path(repo))
    if stellen is None:
        return fehler

    weisen = sorted({w for _, _, w in stellen})

    bekannt = manifest_lesen(manifest)
    if bekannt is None:
        # Kein Manifest -> das Tor sagt, dass es NICHTS geprueft hat,
        # statt stillschweigend durchzuwinken (Klasse MF-1000/Tor 64).
        fehler.append(
            "Grundlinien-Manifest `%s` fehlt oder ist unlesbar: dieses Tor "
            "hat NICHTS geprueft. Anlegen mit `python "
            "scripts/audit_cbm_zonen.py --grundlinie-schreiben`."
            % MANIFEST.name)
    else:
        ist = {schluessel(rel, name): w for rel, name, w in stellen}
        neu = sorted(set(ist) - set(bekannt))
        weg = sorted(set(bekannt) - set(ist))
        if neu:
            fehler.append(
                "%d NEUE Kopie(n) der CBM-Zonenlaengen, nicht im "
                "Grundlinien-Manifest: %s. Die Richtung ist "
                "Zusammenfuehrung auf `uft_cbm_track_capacity()` — jede "
                "neue Kopie geht dagegen. Zwei bestehende lesen "
                "nachweislich falsch (P3-148, P3-149). Das Manifest wird "
                "NICHT erweitert, um eine neue Kopie zuzulassen; es wird "
                "nur gekuerzt, wenn eine verschwindet."
                % (len(neu), ", ".join(neu)))
        # Ein Zaehlweisen-Wechsel an BEKANNTER Stelle ist keine neue
        # Kopie, aber er gehoert gesagt: dieselbe Tafel liest sich anders.
        gewechselt = sorted(s for s in set(ist) & set(bekannt)
                            if bekannt[s] and ist[s] != bekannt[s])
        if gewechselt:
            hinweise.append(
                "%d bekannte Stelle(n) haben die Zaehlweise gewechselt: %s. "
                "Kein Verstoss — aber `--grundlinie-schreiben` zieht es nach."
                % (len(gewechselt), ", ".join(gewechselt)))
        if weg:
            hinweise.append(
                "%d Stelle(n) aus dem Manifest sind verschwunden: %s. Das "
                "ist die gewuenschte Richtung — `python "
                "scripts/audit_cbm_zonen.py --grundlinie-schreiben` kuerzt "
                "das Manifest, damit der Fortschritt im Diff steht."
                % (len(weg), ", ".join(weg)))
    if len(weisen) > GRUNDLINIE_WEISEN:
        # Bewusst OHNE Liste der bekannten Zaehlweisen: die muesste
        # gepflegt werden, und eine gepflegte Liste driftet — genau die
        # Falle, aus der dieses Tor entstanden ist (P3-150 nannte 11
        # Stellen in 4 Zaehlweisen, gemessen waren es 23 in 9).
        # `--list` zeigt alle mit Fundstelle; welche neu ist, sagt der
        # Vergleich mit dem Vorzustand, nicht dieses Skript.
        fehler.append(
            "%d Zaehlweisen, Grundlinie %d. Eine NEUE Zaehlweise ist der "
            "teuerste Zuwachs: sie laesst sich mit den anderen nicht "
            "mechanisch verrechnen, und genau daran ist P3-148/149 "
            "gescheitert. Welche dazugekommen ist, zeigt "
            "`python scripts/audit_cbm_zonen.py --list` im Vergleich zum "
            "Vorzustand."
            % (len(weisen), GRUNDLINIE_WEISEN))
    return fehler


def _selbsttest() -> int:
    """Vor dem Nenner (MF-693) — ueber `check()` selbst, nicht ueber eine
    Hilfsfunktion."""
    import tempfile

    def baum(inhalt: str) -> int:
        with tempfile.TemporaryDirectory() as d:
            p = Path(d)
            subprocess.run(["git", "init", "-q"], env=git_umgebung(), cwd=d, capture_output=True)
            (p / "x.c").write_text(inhalt, encoding="utf-8")
            return len(messe(p)[0])

    faelle = [
        ("4-Zonen aufsteigend",
         "static int z[4] = { 21, 19, 18, 17 };", 1),
        ("4-Zonen absteigend",
         "static int z[4] = { 17, 18, 19, 21 };", 1),
        ("Spurtabelle 1-basiert",
         "static int z[6] = { 0, 21, 19, 18, 17, 17 };", 1),
        ("Skew-Tabelle mit denselben Zahlen -> KEIN Fund",
         "static int s[8] = { 1, 7, 13, 19, 21, 5, 17, 18 };", 0),
        ("nur drei der vier Zonen -> KEIN Fund",
         "static int z[3] = { 21, 19, 18 };", 0),
        ("im Kommentar -> KEIN Fund",
         "/* static int z[4] = { 21, 19, 18, 17 }; */\nint f(void){return 0;}",
         0),
        ("in einer Zeichenkette -> KEIN Fund",
         'const char *s = "{ 21, 19, 18, 17 }";', 0),
    ]

    gut = 0
    for name, inhalt, soll in faelle:
        ist = baum(inhalt)
        if ist == soll:
            gut += 1
        else:
            print("  ROT  %-46s erwartet %d, gemessen %d"
                  % (name, soll, ist))

    # ── Das Manifest gegen die Luecke, die eine ZAHL nicht sieht ────────
    # Vorgefuehrt statt behauptet: zwei Baeume mit GLEICH VIELEN Kopien,
    # einer davon mit einer neuen. Gegen eine Anzahl bestehen beide.
    def manifest_fall() -> list:
        ergebnis = []
        with tempfile.TemporaryDirectory() as d:
            p = Path(d)
            subprocess.run(["git", "init", "-q"], env=git_umgebung(),
                           cwd=d, capture_output=True)
            tafel = "static const int %s[6] = { 0, 21, 19, 18, 17, 17 };\n"
            for i, n in enumerate(("alt_a", "alt_b", "alt_c")):
                (p / ("f%d.c" % i)).write_text(tafel % n, encoding="utf-8")
            m = p / "grundlinie.json"
            manifest_schreiben(messe(p)[0], m)
            ergebnis.append(("Manifest deckt den Bestand", not check(p, None, m)))

            # Eine alte Stelle weg, eine NEUE dazu: Anzahl unveraendert.
            (p / "f2.c").write_text(tafel % "frisch", encoding="utf-8")
            stellen_neu = messe(p)[0]
            fehler = check(p, None, m)
            ergebnis.append(("Anzahl unveraendert (%d)" % len(stellen_neu),
                             len(stellen_neu) == 3))
            ergebnis.append(("neue Stelle faellt trotzdem auf",
                             any("frisch" in f for f in fehler)))

            # Nur eine Stelle entfaellt: kein Verstoss, aber ein Hinweis.
            (p / "f2.c").write_text("int leer(void){return 0;}\n",
                                    encoding="utf-8")
            hin: list = []
            ergebnis.append(("Wegfall ist kein Verstoss",
                             not check(p, hin, m)))
            ergebnis.append(("Wegfall wird gemeldet",
                             any("verschwunden" in h for h in hin)))

            # Fehlendes Manifest -> das Tor sagt, dass es nichts geprueft hat.
            ergebnis.append(("fehlendes Manifest ist ein FAIL",
                             any("NICHTS geprueft" in f
                                 for f in check(p, None, p / "gibtsnicht"))))
        return ergebnis

    for name, ok in manifest_fall():
        faelle.append((name, None, None))
        if ok:
            gut += 1
        else:
            print("  ROT  %-46s Manifest-Zusage haelt nicht" % name)

    print("Selbsttest: %d/%d" % (gut, len(faelle)))
    return 0 if gut == len(faelle) else 1


def main() -> int:
    sys.stdout.reconfigure(encoding="utf-8")
    repo = Path(__file__).resolve().parent.parent
    if "--selftest" in sys.argv:
        return _selbsttest()

    stellen, fehler = messe(repo)
    if stellen is None:
        print("\n".join(fehler))
        return 1

    weisen = {}
    for rel, name, w in stellen:
        weisen.setdefault(w, []).append((rel, name))

    if "-v" in sys.argv or "--list" in sys.argv:
        for w in sorted(weisen):
            print("%s  (%d)" % (w, len(weisen[w])))
            for rel, name in weisen[w]:
                print("    %-50s %s" % (rel, name))
            print()

    if "--grundlinie-schreiben" in sys.argv:
        n = manifest_schreiben(stellen)
        alt = manifest_lesen() or {}
        print("Grundlinien-Manifest geschrieben: %d Stellen (vorher %d) -> %s"
              % (n, len(alt), MANIFEST.name))
        print("Der Fortschritt steht jetzt im Diff, nicht in einer Zahl.")
        return 0

    bekannt = manifest_lesen() or {}
    print("CBM-Zonenlaengen: %d Fundstellen (Manifest %d), "
          "%d Zaehlweisen (Grundlinie %d)"
          % (len(stellen), len(bekannt), len(weisen), GRUNDLINIE_WEISEN))

    hinweise: list = []
    errs = check(repo, hinweise)
    for h in hinweise:
        print("  HINWEIS: %s" % h)
    if not errs:
        print("OK")
        return 0
    print("FAIL:")
    for e in errs:
        print("  %s" % e)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
