#!/usr/bin/env python3
"""Was hat der Scout gesichtet, und was ist daraus geworden? (MF-675)

    python scripts/scout_stand.py [--offen]

── Warum es dieses Skript gibt ──────────────────────────────────────────

Auf die Frage „hat der Scout alle Listen abgearbeitet" liess sich aus
dem Baum **keine Antwort** geben, und das war der eigentliche Befund:

  * Die eingereichten Repo-Listen standen nur im Gespraechsverlauf.
    Nirgendwo im Baum stand, was beauftragt war — also konnte niemand
    pruefen, ob etwas fehlt.
  * Gemessen werden konnte nur, was ANGEKOMMEN ist: geklonte Repos,
    Messdateien, Gutachten. Das beantwortet „was wurde getan", nicht
    „wurde alles getan".

Der Unterschied ist derselbe wie zwischen einem gruenen Test und einem
Test, der scheitern kann. „26 Gutachten" klingt nach Vollstaendigkeit
und ist eine Zahl ohne Nenner.

Dieses Skript liefert den Nenner, soweit er messbar ist, und sagt
ausdruecklich, wo er fehlt. Es zaehlt NICHT aus einer gepflegten Liste,
sondern aus dem, was auf der Platte liegt — dieselbe Regel wie
`repo_scope.py`: Verzeichnisse fragen, nicht Aufzaehlungen pflegen.

── Was es NICHT sagen kann ──────────────────────────────────────────────

Ob ein eingereichtes Repo nie geklont wurde. Dafuer muesste der Auftrag
im Baum stehen. Wer eine Liste uebergibt, traegt sie nach
`tools/uft-scout/data/auftraege.json` ein — dann ist die Frage beim
naechsten Mal beantwortbar.
"""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

WURZEL = Path(__file__).resolve().parent.parent
SCOUT = WURZEL / "tools" / "uft-scout"
WORK = SCOUT / "work"
OUT = SCOUT / "out"
AUFTRAEGE = SCOUT / "data" / "auftraege.json"
NEGATIVE = SCOUT / "data" / "known_negatives.json"
LISTE = WURZEL / "docs" / "OPEN_ITEMS.md"


def gemessene() -> list[str]:
    return sorted(p.name[: -len(".messung.json")]
                  for p in WORK.glob("*.messung.json"))


def gutachten() -> dict[str, str]:
    """Name -> Volltext."""
    return {p.name[: -len(".gutachten.md")]:
            p.read_text(encoding="utf-8", errors="replace")
            for p in OUT.glob("*.gutachten.md")}


def norm(x):
    return re.sub(r"[^a-z0-9]", "", x.lower())


def klon_slugs(work):
    """Welche Repos liegen wirklich als Klon da — nach ihrer HERKUNFT.

    Vorher wurde der Verzeichnisname verglichen, und das hat still
    danebengegriffen: `joncampbell123/floppytools` galt als geklont, weil
    `Datamuseum-DK/FloppyTools` unter dem Namen `FloppyTools` liegt. Zwei
    Projekte, ein Name, eine falsche Entwarnung.

    `git remote get-url origin` sagt, was ein Verzeichnis wirklich
    enthaelt. Antwortet git nicht (kein Repo, kein git im Pfad), faellt
    der Eintrag auf den Verzeichnisnamen zurueck — schwaecher, aber
    besser als gar keine Zuordnung; der Rueckfall ist in der Menge nicht
    unterscheidbar, darum wird er im Zweifel als schwaechster Beleg
    behandelt.
    """
    import subprocess
    aus = set()
    for d in sorted(x for x in work.iterdir() if x.is_dir()):
        try:
            u = subprocess.run(["git", "-C", str(d), "remote", "get-url",
                                "origin"], capture_output=True, text=True,
                               timeout=10).stdout.strip()
        except (OSError, subprocess.SubprocessError):
            u = ""
        if u:
            m = re.search(r"[:/]([^/:]+)/([^/]+?)(?:\.git)?$", u)
            if m:
                aus.add((m.group(1) + "/" + m.group(2)).lower())
                continue
        aus.add(norm(d.name))
    return aus



def status_von(e, texte, negativ, klone):
    """Wie weit ist dieser Auftrag?

    ── Warum es fuenf Stufen sind und nicht zwei (MF-677) ────────────────

    Der erste Entwurf kannte nur "begutachtet" oder "OFFEN" und zaehlte
    einen Treffer des REPO-NAMENS in irgendeinem Gutachten als erledigt.
    Die Stichprobe hat zwei Fehlerarten aufgedeckt, beide in die
    gefaehrliche Richtung — sie melden Arbeit als getan, die offen ist:

      * **Namensgleichheit.** `joncampbell123/floppytools` galt als
        begutachtet, weil `Datamuseum-DK/FloppyTools` eines hat. Zwei
        verschiedene Projekte, ein Name.
      * **Erwaehnung statt Begutachtung.** `tonioni/WinUAE` und
        `lipro-cpm4l/cpmtools` kommen in fremden Gutachten VOR — als
        Verweis, nicht als Gegenstand.

    Darum traegt nur der volle `owner/repo`-Bezeichner ein Urteil. Ein
    Namenstreffer ohne Bezeichner heisst "nur erwaehnt" und ist eine
    FRAGE an den Menschen, kein Ergebnis.

    Reihenfolge vom staerksten zum schwaechsten Beleg:
      1. `owner/repo` steht in einem Gutachten            -> begutachtet
      2. `owner/repo` steht in known_negatives            -> frueher bewertet
      3. ein Klon liegt da                                -> geklont, kein Gutachten
      4. nur der blosse Name kommt irgendwo vor           -> nur erwaehnt
      5. nichts davon                                     -> OFFEN
    """
    slug = e.get("slug", "")
    name = e.get("name", "")
    if slug and slug.lower() in texte.lower():
        return "begutachtet"
    if slug and slug in negativ:
        return "frueher bewertet"
    if slug and slug.lower() in klone:
        return "geklont, kein Gutachten"
    if name and re.search(r"\b" + re.escape(name) + r"\b",
                          texte, re.I):
        return "nur erwaehnt (pruefen)"
    return "OFFEN"


def ohne_spur(gut: dict, liste: str) -> list[str]:
    """Gutachten, die in `docs/OPEN_ITEMS.md` keine Spur hinterlassen.

    Ausgelagert (MF-693), weil der Innendienst-Sekretaer dieselbe Frage
    stellt. Er hatte dafuer zunaechst einen eigenen, schwaecheren Ersatz:
    "Gutachten ohne `<!-- stufe: -->`-Marke". Gemessen tragen genau vier
    der rund dreissig Gutachten diese Marke — der Ersatz haette also
    fuenfundzwanzig Posten als offene Entscheidung gemeldet, von denen
    die meisten laengst erledigt sind. Ein Tor, das so oft falsch
    anschlaegt, wird uebergangen.

    Der Vergleich ist bewusst grob (Name, nicht `owner/repo`): ein
    Treffer ist eine FRAGE an den Menschen, kein Urteil.
    """
    return sorted(n for n in gut
                  if n not in liste
                  and not re.search(re.escape(n), liste, re.I))


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--offen", action="store_true",
                    help="nur zeigen, was noch aussteht")
    a = ap.parse_args()

    if not WORK.is_dir():
        print("FEHLER: tools/uft-scout/work/ fehlt — nichts zu messen.")
        return 2

    mess = gemessene()
    gut = gutachten()
    texte = "\n".join(gut.values())

    # Ein Repo gilt als begutachtet, wenn sein Name in IRGENDEINEM
    # Gutachten vorkommt — mehrere Gutachten buendeln Repos (etwa
    # `cbm_erzeuger` fuer fuenf), darum trifft ein Namensvergleich
    # Datei-zu-Datei daneben.
    ohne_gutachten = [n for n in mess if n not in texte]

    liste = LISTE.read_text(encoding="utf-8", errors="replace") \
        if LISTE.is_file() else ""
    ohne_entscheidung = ohne_spur(gut, liste)

    neg = 0
    if NEGATIVE.is_file():
        try:
            neg = len(json.loads(NEGATIVE.read_text(encoding="utf-8"))
                      .get("eintraege", {}))
        except (ValueError, OSError):
            neg = -1

    auftraege = None
    if AUFTRAEGE.is_file():
        try:
            auftraege = json.loads(AUFTRAEGE.read_text(encoding="utf-8"))
        except (ValueError, OSError):
            auftraege = None

    if not a.offen:
        print("Scout-Stand")
        print("=" * 60)
        print(f"  geklont und gemessen   : {len(mess)}")
        print(f"  Gutachten geliefert    : {len(gut)}")
        print(f"  davon ohne Gutachten   : {len(ohne_gutachten)}")
        print(f"  frueher schon bewertet : "
              f"{neg if neg >= 0 else 'known_negatives.json unlesbar'}")

    negativ = {}
    if NEGATIVE.is_file():
        try:
            negativ = json.loads(NEGATIVE.read_text(encoding="utf-8")).get(
                "eintraege", {})
        except (ValueError, OSError):
            negativ = {}
    klone = klon_slugs(WORK)
    # KEIN Rueckfall auf Verzeichnis- oder Messdateinamen: genau der
    # hat `joncampbell123/floppytools` als geklont gemeldet, weil
    # Datamuseums gleichnamiges Repo dort liegt. Lieber eine Zuordnung
    # weniger als eine falsche Entwarnung.

    if auftraege is None:
        print("\n  KEIN NENNER: tools/uft-scout/data/auftraege.json fehlt.")
        print("  Damit ist \"alles abgearbeitet?\" NICHT beantwortbar — es")
        print("  laesst sich nur zaehlen, was angekommen ist, nicht was")
        print("  beauftragt war. Wer eine Liste uebergibt, traegt sie dort")
        print("  ein.")
    else:
        eintraege = auftraege.get("eintraege", [])
        nach_status = {}
        for e in eintraege:
            nach_status.setdefault(
                status_von(e, texte, negativ, klone), []).append(e)
        print("")
        print("  beauftragt             : %d" % len(eintraege))
        for k in ("begutachtet", "frueher bewertet",
                  "geklont, kein Gutachten",
                  "nur erwaehnt (pruefen)", "OFFEN"):
            if k in nach_status:
                print("    %-26s: %d" % (k, len(nach_status[k])))
        for k in ("geklont, kein Gutachten", "nur erwaehnt (pruefen)", "OFFEN"):
            for e in nach_status.get(k, []):
                print("      [%s] %s" % (k, e.get("slug", e.get("name"))))

    if ohne_gutachten:
        print("\n  gemessen, aber ohne Gutachten:")
        for n in ohne_gutachten:
            print(f"      {n}")

    if ohne_entscheidung:
        print("\n  Gutachten ohne Spur in OPEN_ITEMS.md:")
        for n in ohne_entscheidung:
            print(f"      {n}")
        print("  (Hinweis: grober Namensvergleich. Ein Gutachten kann unter")
        print("   anderem Namen aufgenommen sein — `settings_triage` etwa")
        print("   als SET-1. Ein Treffer hier ist eine Frage, kein Urteil.)")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
