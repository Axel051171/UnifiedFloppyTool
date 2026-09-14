#!/usr/bin/env python3
"""Tor: der uebernommene Fremdbestand `src/dtc_components/` ist unveraendert.

Anlass (Eigentuemer-Weisung vom 2026-09-14, MF-1126): „Heute ist die
Unveraenderlichkeit eine Absicht in einer Markdown-Datei. Sie gehoert
gemessen. … Ohne dieses Tor ist D1 eine Bitte."

D1 lautet: der Beleg bleibt Beleg. Die SHA-256-Summen ueber die
git-Objekte sind der einzige Nachweis, dass der Bestand ist, was er zu
sein behauptet. Eine Aenderung an einer uebernommenen Datei vernichtet
ihn unwiederbringlich — verbessert wird durch Ableitung, nie durch
Bearbeitung.

Gemessen wird in VIER Richtungen, und die dritte ist die wichtigere:

  D1a  jede Zeile der Tafel in `UEBERNAHME.md` gegen `git show HEAD:<pfad>`
       — Summe UND Bytezahl. Zwei unabhaengige Groessen, damit eine
       vertauschte Zeile nicht durchkommt.

  D1b  jede Datei, die in der Tafel steht, MUSS im Baum liegen.

  D1c  jede Datei, die im Baum unter `src/dtc_components/` liegt, MUSS
       in der Tafel stehen. Ohne diese Richtung waere die Tafel eine
       gepflegte Liste, und die veraltet still — in diesem Baum vier
       Mal gemessen (MF-567, MF-578, MF-598, MF-633). Die Dateimenge
       kommt deshalb aus `git ls-files`, nicht aus der Tafel
       (Grundsatz MF-636).

  D1d  `.gitattributes` muss fuer jede dieser Dateien `text` auf `unset`
       stellen. Ohne `-text` hat derselbe Beleg auf zwei Rechnern zwei
       Summen (MF-1094/MF-1096); dann gaebe es die Zahl gar nicht, die
       D1a vergleicht.

AUSGENOMMEN ist genau eine Datei: `UEBERNAHME.md`. Sie gehoert UFT und
sagt das in ihrer ersten Zeile; sie ist die Akte, nicht der Beleg.

Gehasht wird das **git-Objekt**, nie der Arbeitsbaum. Mit
`core.autocrlf=true` sind die Bytes im Arbeitsbaum nicht die Bytes im
Blob, und eine Summe ueber den Arbeitsbaum ist eine Aussage ueber die
lokale Auscheckung statt ueber den Beleg (MF-1096, Sperre 3).

Dieses Skript schreibt nichts.
"""
from __future__ import annotations

import hashlib
import re
import subprocess
import sys
from pathlib import Path

WURZEL = Path(__file__).resolve().parent.parent
BESTAND = "src/dtc_components"
AKTE = "UEBERNAHME.md"          # gehoert UFT, ist kein Beleg

# | `src/crc.c` | 809 | `d09d3cac…` |  — die dritte Spalte mit 64 Hexzeichen
# macht die Zeile eindeutig; die anderen Tafeln der Datei haben zwei Spalten.
ZEILE = re.compile(
    r"^\|\s*`([^`]+)`\s*\|\s*(\d+)\s*\|\s*`([0-9a-fA-F]{64})`\s*\|")


def git_bytes(wurzel: Path, pfad: str) -> bytes | None:
    """Der Inhalt des git-OBJEKTS, nicht der Datei im Arbeitsbaum.

    Gefragt wird zuerst der **Index** (`git show :<pfad>`), dann `HEAD`.
    Der Grund ist gemessen: dieses Tor laeuft im Pre-Commit-Haken, und
    `HEAD` kennt eine gerade hinzugefuegte Datei noch nicht — beim
    ersten Lauf meldete es `D1b … liegt aber nicht im Baum` fuer das
    frisch aufgenommene `SOURCE_MAP.md`, also einen Befund ueber den
    Zeitpunkt statt ueber die Datei. Der Index ist, was der Commit
    wird, und sein Blob ist ein git-Objekt mit derselben Normalisierung
    — die Regel aus MF-1096 (nie der Arbeitsbaum) bleibt gewahrt.
    """
    for spec in (":" + pfad, "HEAD:" + pfad):
        r = subprocess.run(["git", "show", spec],
                           cwd=wurzel, capture_output=True)
        if r.returncode == 0:
            return r.stdout
    return None


def tafel(wurzel: Path) -> dict[str, tuple[int, str]]:
    """Die Tafel aus der Akte: Pfad -> (Bytezahl, SHA-256).

    Gelesen wird der Arbeitsbaum-Stand der AKTE, weil sie UFT gehoert
    und im selben Commit fortgeschrieben wird wie eine Aufnahme; die
    BELEGE dagegen kommen immer aus dem git-Objekt.
    """
    p = wurzel / BESTAND / AKTE
    if p.exists():
        roh = p.read_bytes()
    else:
        roh = git_bytes(wurzel, f"{BESTAND}/{AKTE}")
        if roh is None:
            return {}
    aus: dict[str, tuple[int, str]] = {}
    for z in roh.decode("utf-8", "replace").splitlines():
        m = ZEILE.match(z.strip())
        if m:
            aus[m.group(1)] = (int(m.group(2)), m.group(3).lower())
    return aus


def versionierte(wurzel: Path) -> list[str]:
    """Die Dateimenge kommt aus git, nicht aus der Tafel (MF-636)."""
    r = subprocess.run(
        ["git", "ls-files", "--cached", "--others", "--exclude-standard",
         "--", BESTAND],
        cwd=wurzel, capture_output=True, text=True)
    if r.returncode != 0:
        return []
    return [z.strip() for z in r.stdout.splitlines() if z.strip()]


def textattribut(wurzel: Path, pfade: list[str]) -> dict[str, str]:
    """`git check-attr text` je Pfad; leer, wenn git nicht befragbar ist."""
    if not pfade:
        return {}
    r = subprocess.run(["git", "check-attr", "-z", "text", "--"] + pfade,
                       cwd=wurzel, capture_output=True)
    if r.returncode != 0:
        return {}
    felder = r.stdout.split(b"\0")
    aus: dict[str, str] = {}
    for i in range(0, len(felder) - 2, 3):
        pfad, merkmal, wert = felder[i:i + 3]
        if merkmal == b"text":
            aus[pfad.decode("utf-8", "replace")] = \
                wert.decode("utf-8", "replace")
    return aus


def pruefe(eintraege: dict[str, tuple[int, str]],
           objekte: dict[str, bytes | None],
           attribute: dict[str, str],
           im_baum: list[str]) -> list[str]:
    """Der reine Kern, damit der Selbsttest ihn ohne git treiben kann.

    `eintraege`  Tafel: relativer Pfad -> (Bytezahl, SHA-256)
    `objekte`    relativer Pfad -> Inhalt des git-Objekts (None = fehlt)
    `attribute`  vollstaendiger Pfad -> Wert von `git check-attr text`
    `im_baum`    vollstaendige Pfade, die git unter dem Bestand fuehrt
    """
    befunde: list[str] = []

    # D1a / D1b
    for rel in sorted(eintraege):
        soll_n, soll_h = eintraege[rel]
        roh = objekte.get(rel)
        if roh is None:
            befunde.append(
                f"D1b {BESTAND}/{rel}: steht in der Tafel von {AKTE}, "
                f"liegt aber nicht im Baum — ein Beleg ohne Objekt")
            continue
        ist_h = hashlib.sha256(roh).hexdigest()
        if ist_h != soll_h:
            # Beide Summen VOLLSTAENDIG. Eine erste Fassung kuerzte auf
            # 16 Stellen, und der Rotbeweis zeigte prompt zweimal
            # dasselbe (`d09d3cac9395db05…`), weil die Verfaelschung am
            # ENDE sass. Eine Befundmeldung, die den Unterschied nicht
            # zeigt, ist keine.
            befunde.append(
                f"D1a {BESTAND}/{rel}: SHA-256 des git-Objekts weicht von "
                f"der Tafel in {AKTE} ab — eine uebernommene Datei wurde "
                f"bearbeitet, damit ist der Beleg vernichtet; verbessert "
                f"wird durch Ableitung.\n"
                f"      git-Objekt: {ist_h}\n"
                f"      Tafel     : {soll_h}")
        if len(roh) != soll_n:
            befunde.append(
                f"D1a {BESTAND}/{rel}: {len(roh)} Byte im git-Objekt, "
                f"die Tafel sagt {soll_n}")

    def relativ(voll: str) -> str:
        return voll[len(BESTAND) + 1:] if voll.startswith(BESTAND + "/") \
            else voll

    # D1c — die andere Richtung, aus git getrieben
    for voll in sorted(im_baum):
        rel = relativ(voll)
        if rel == AKTE:
            continue
        if rel not in eintraege:
            befunde.append(
                f"D1c {voll}: liegt im Bestand, steht aber in keiner Zeile "
                f"der Tafel von {AKTE} — ohne Eintrag ist die Datei kein "
                f"Beleg, sondern nur eine Datei")

    # D1d
    for voll in sorted(im_baum):
        rel = relativ(voll)
        if rel == AKTE:
            continue
        wert = attribute.get(voll)
        if wert is not None and wert != "unset":
            befunde.append(
                f"D1d {voll}: `git check-attr text` meldet '{wert}' statt "
                f"'unset' — ohne `-text` in .gitattributes hat derselbe "
                f"Beleg auf zwei Rechnern zwei Summen")
    return befunde


def check(wurzel: Path, zaehler: dict[str, int] | None = None) -> list[str]:
    eintraege = tafel(wurzel)
    im_baum = versionierte(wurzel)
    if not eintraege and not im_baum:
        return []          # kein Bestand, nichts zu halten
    objekte = {rel: git_bytes(wurzel, f"{BESTAND}/{rel}") for rel in eintraege}
    attribute = textattribut(wurzel, im_baum)
    if zaehler is not None:
        zaehler["tafelzeilen"] = len(eintraege)
        zaehler["dateien"] = len([p for p in im_baum
                                  if not p.endswith("/" + AKTE)])
    return pruefe(eintraege, objekte, attribute, im_baum)


def _selbsttest() -> int:
    """Gepflanzte Faelle gegen den reinen Kern, plus EIN echter Lauf.

    Der reine Kern wird mit erfundenen Tafeln und Objekten getrieben —
    kein `git init`, keine Schreiboperation im Baum. Dass die
    git-Verrohrung wirklich greift, prueft der letzte Fall am ECHTEN
    Baum: dort muessen 0 Befunde stehen.
    """
    inhalt = b"abc\n"
    gut = hashlib.sha256(inhalt).hexdigest()
    n = len(inhalt)
    A = BESTAND + "/src/x.c"
    AK = BESTAND + "/" + AKTE

    faelle: list[tuple[str, list[str], bool]] = []

    def fall(name, eintraege, objekte, attribute, im_baum, erwartet_rot):
        b = pruefe(eintraege, objekte, attribute, im_baum)
        faelle.append((name, b, bool(b) == erwartet_rot))

    fall("sauber -> still",
         {"src/x.c": (n, gut)}, {"src/x.c": inhalt},
         {A: "unset"}, [A], False)

    fall("D1a falsche Summe",
         {"src/x.c": (n, "0" * 64)}, {"src/x.c": inhalt},
         {A: "unset"}, [A], True)

    fall("D1a falsche Bytezahl",
         {"src/x.c": (n + 1, gut)}, {"src/x.c": inhalt},
         {A: "unset"}, [A], True)

    fall("D1b Tafelzeile ohne Objekt",
         {"src/x.c": (n, gut)}, {"src/x.c": None},
         {A: "unset"}, [A], True)

    fall("D1c Datei ohne Tafelzeile",
         {}, {}, {A: "unset"}, [A], True)

    fall("D1d text gesetzt statt unset",
         {"src/x.c": (n, gut)}, {"src/x.c": inhalt},
         {A: "set"}, [A], True)

    fall("D1d text=auto ist auch zu wenig",
         {"src/x.c": (n, gut)}, {"src/x.c": inhalt},
         {A: "auto"}, [A], True)

    fall("die Akte selbst ist ausgenommen",
         {"src/x.c": (n, gut)}, {"src/x.c": inhalt},
         {A: "unset", AK: "set"}, [A, AK], False)

    echt = check(WURZEL)
    faelle.append(("echter Baum -> 0 Befunde", echt, not echt))

    rot = 0
    for name, befunde, ok in faelle:
        print(f"  {'ok  ' if ok else 'ROT '} {name}"
              + ("" if ok else f"  -> {befunde[:1]}"))
        if not ok:
            rot += 1
    print(f"SELBSTTEST {len(faelle) - rot}/{len(faelle)}")
    return 1 if rot else 0


def main() -> int:
    if "--selbsttest" in sys.argv:
        return _selbsttest()
    zaehler: dict[str, int] = {}
    befunde = check(WURZEL, zaehler)
    for b in befunde:
        print(b)
    print(f"dtc-Unveraendert: {len(befunde)} Befunde "
          f"({zaehler.get('tafelzeilen', 0)} Tafelzeilen gegen das "
          f"git-Objekt gehalten, {zaehler.get('dateien', 0)} Dateien im "
          f"Bestand aus `git ls-files`)")
    return 1 if befunde else 0


if __name__ == "__main__":
    raise SystemExit(main())
