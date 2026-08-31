#!/usr/bin/env python3
"""Nennt jede Attribution die Lizenz IHRER Quelle? (MF-651)

Hintergrund. `scripts/audit_spdx_policy.py` listet seit MF-636 die
Fliesstext-Attributionen — „Based on X", „Port of X" —, faellt aber
bewusst kein Urteil. Die Frage „wie viele davon nennen keine Lizenz?"
wurde bis MF-651 von Hand beantwortet, und eine handgepflegte Zahl
driftet; in diesem Baum ist das dreimal belegt.

Die Falle, in die der erste Entwurf lief, steht hier als Warnung:
er suchte einen Lizenzbezeichner IRGENDWO im 60-Zeilen-Kopf. Damit
zaehlte er unsere EIGENE `SPDX-License-Identifier: GPL-2.0-or-later`
mit — die seit MF-621 in fast jeder Datei steht — und konnte die
gestellte Frage gar nicht beantworten. Gemessen: 29 geheilte
Attributionen bewegten die Zahl um **null**. Ein Mass, das sich durch
die Sache nicht bewegen laesst, misst sie nicht.

Deshalb hier: die Lizenz muss NEBEN der Attribution stehen (im selben
Kommentarblock, Fenster +/- FENSTER Zeilen), und die eigene
SPDX-Zeile zaehlt ausdruecklich NICHT.

Aufruf:
    python scripts/audit_attribution_licence.py [--liste]
"""
from __future__ import annotations

import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import audit_spdx_policy as A  # noqa: E402

FENSTER = 6
"""Zeilen ober- und unterhalb der Attribution, die noch als 'daneben'
gelten. Sechs, weil ein umbrochener Lizenzsatz in diesem Baum bis zu
vier Zeilen braucht (gemessen an den libdsk-Koepfen)."""

LIZENZ = re.compile(
    # MF-737: hier stand `\b(GPL|LGPL|AGPL|...)\b`. Das schliessende `\b`
    # nach `GPL` verlangt ein Nicht-Wortzeichen — und die HAEUFIGSTE
    # Schreibweise ueberhaupt ist `GPLv2`. Gemessen:
    #
    #     GPLv2+   FEHLT      GPL-2.0   trifft
    #     GPLv2    FEHLT      LGPL-2.1  trifft
    #     GPLv3    FEHLT      GPL 2     trifft
    #
    # Folge im Baum: `cbmconvert by Marko Maekelae (GPLv2+)` stand auf
    # der Liste „ohne Lizenz", obwohl die Lizenz danebensteht.
    r"\b(?:A|L)?GPL[-\sv]?\d"
    r"|\b(?:A|L)?GPL\b"
    r"|\bMIT\b|\bBSD\b|\bzlib\b|\bApache\b|\bCC0\b|\bISC\b|\bMPL\b"
    r"|[Pp]ublic [Dd]omain|proprietary|proprietaer|\bUnlicense\b")

EIGENE_SPDX = re.compile(r"SPDX-License-Identifier")


# ── Vier Klassen statt zweier Zahlen (MF-737) ────────────────────────────
#
# „171 Attributionen, 125 ohne Lizenz" war als Rueckstand unbrauchbar,
# weil die 125 drei voellig verschiedene Dinge in einen Topf warfen:
#
#   * Prosa, die gar keine Attribution ist („based on measured variance")
#   * ein Verweis auf DOKUMENTATION — der begruendet keine Ableitung und
#     ist damit keine Lizenzfrage. `docs/CLAUDE.md` (MF-636) sagt das
#     ausdruecklich: „wer eigenstaendig implementiert und nur fremde Doku
#     gelesen hat, schreibt das auch so"
#   * eine benannte fremde CODEBASIS — das ist der Rueckstand
#
# Gemessen: 125 -> 38. Die anderen 87 waren nie eine Entscheidung.
#
# GRUNDSATZ DER EINORDNUNG: im Zweifel die STRENGE Seite. Was sich nicht
# entscheiden laesst, wird UNKLAR und geht an einen Menschen — nie
# stillschweigend in die harmlose Klasse. Ein Rueckstand, der sich durch
# Wegsortieren verkleinern laesst, misst nichts.

AUSLOESER = re.compile(
    r"(?i)^(based on|adapted from|derived from|port(?:ed)? of|"
    r"portiert aus|nach dem Vorbild|taken from|originally (?:by|from)|"
    r"reference:|referenz:|verhalten nach)\s*")

# STARKE Marker: sie benennen selbst ein Artefakt und genuegen allein.
CODE_STARK = re.compile(
    r"\bsources?\b|\bemulator\b|\blibrar(?:y|ies)\b|"
    r"\blib[a-z]{2,}\b|\b\w+lib\b|\bSDK\b|\bdriver\b|\bassembly\b|"
    r"\bquelle\b|\bloader\b|"
    r"\b[\w./-]+\.(?:c|cpp|cc|cs|h|py|asm|s)\b",
    re.I)

# SCHWACHE Marker: gewoehnliche Woerter, die auch in Prosa vorkommen. Sie
# zaehlen nur, wenn der Satz ueberhaupt etwas benennt. Gemessen: ohne
# diese Trennung landeten sechs reine Prosastellen in der Code-Klasse —
# „what the connect code actually does with", „the track number". Das
# Wort „code" allein macht keine Ableitung.
CODE_SCHWACH = re.compile(
    r"\bcode\b|\bimplementation\b|\btool(?:s|kit)?\b|\bprogram\b|"
    r"\bheaders?\b", re.I)

DOKU = re.compile(
    r"\b(?:specification|spec\b|manual|documentation|\bdocs?\b|wiki|"
    r"technical|notes?\b|article|book|disassembly|standard|protocol|"
    r"datasheet|whitepaper|handbuch|dokumentation|anleitung)\b"
    r"|\bFIPS\b|\bISO[- ]\d|\bECMA\b|\bRFC\s*\d"
    r"|Beneath Apple|Inside Macintosh|BeebWiki|nesdev|web\.archive\.org",
    re.I)

# Wer „Port of" schreibt, hat Code portiert. Der Ausloeser allein
# genuegt aber NICHT — die Quelle muss auch etwas benennen. Gemessen:
# „derived from" als Ausloeser zog sieben reine Prosastellen mit
# („derived from timing histograms", „derived from the track number").
# Eine Schaerfung, die ihre eigene Fehlerklasse erzeugt, ist keine.
CODE_AUSLOESER = re.compile(
    r"(?i)^(?:port(?:ed)? of|portiert aus|adapted from|"
    r"derived from|taken from)")

# Ein Repo-Pfad, GROSS-/kleinschreibungs-EMPFINDLICH. Mit `re.I` hielt
# das Muster `D77/D88` fuer `nutzer/repo` und schob eine reine
# Formatspezifikation in die Code-Klasse.
REPO_PFAD = re.compile(r"\b[a-z][a-z0-9_-]{2,}/[a-z][a-z0-9_.-]{2,}\b")

# Ein Titel in Anfuehrungszeichen ist ein Dokument, keine Codebasis.
TITEL = re.compile(r"\"[^\"]{4,}\"")

URL = re.compile(r"https?://\S+|\bwww\.\S+")

# Code-Ablagen. Bewusst eine kurze, geschlossene Menge — und eine
# Fehleinordnung landet in einer Liste, nicht im Schweigen. Ohne diese
# Regel hielt `REPO_PFAD` den Pfadteil JEDER Adresse fuer `nutzer/repo`,
# und `https://applesaucefdc.com/moof-reference` — eine reine
# Formatbeschreibung — galt als Codequelle.
CODE_ABLAGE = re.compile(
    r"github\.com|gitlab\.|bitbucket\.|sourceforge\.|codeberg\.|"
    r"git\.[a-z]+|/(?:tree|blob)/", re.I)

# `<Name> by <Person>` / `<Name> (<Person>)`: ein Programm mit seinem
# Autor. Wer einen Menschen nennt, nennt keinen Standard.
PROGRAMM_AUTOR = re.compile(
    r"^[\w.+-]{3,}(?:\s+[\d.]+)?\s+(?:by|von)\s+[A-Z]"
    r"|^[\w.+-]{3,}\s*\([A-Z][a-z]+\s+[A-Z]")

# Benennt der Satz ueberhaupt etwas? Grossbuchstabe, Ziffer,
# Punkt-im-Wort, Schraegstrich, Adresse oder ein Titel.
IDENT = re.compile(
    r"[A-Z]|\d|https?://|www\.|\"[^\"]{4,}\"|[a-z]\.[a-z]|/")

KLASSEN = ("KEINE", "DOKU", "CODE", "UNKLAR")


def _erster_satz(s: str) -> str:
    """Eine Attribution endet am Satzende — was danach kommt, ist Prosa."""
    return re.split(r"\.\s+(?=[A-Z@])|\.$", s, maxsplit=1)[0].strip()


def klassifiziere(text: str) -> tuple[str, str]:
    """(Klasse, Quelle) fuer eine Attribution."""
    m = AUSLOESER.match(text)
    ausloeser = m.group(1) if m else ""
    q = _erster_satz(AUSLOESER.sub("", text))

    d0 = bool(DOKU.search(q))
    if CODE_AUSLOESER.match(ausloeser) and not d0 and IDENT.search(q):
        return "CODE", q
    if TITEL.search(q) and not REPO_PFAD.search(q):
        return "DOKU", q
    if PROGRAMM_AUTOR.search(q) and not d0:
        return "CODE", q

    url = URL.search(q)
    if url:
        return ("CODE" if CODE_ABLAGE.search(url.group(0)) else "DOKU"), q

    benennt = bool(IDENT.search(q))
    # ERST die Marker, DANN die Identifizierbarkeit: ein
    # Bibliotheksname wie `libdsk` traegt keinen Grossbuchstaben und
    # bestuende die Identifizierbarkeits-Pruefung nicht — er ist
    # trotzdem eine Codequelle.
    c = bool(CODE_STARK.search(q)) or bool(REPO_PFAD.search(q))
    if not c and benennt and CODE_SCHWACH.search(q):
        c = True
    if c and not d0:
        return "CODE", q
    if d0 and not c:
        return "DOKU", q
    if c and d0:
        return "UNKLAR", q
    return ("KEINE" if not benennt else "UNKLAR"), q


# Die Kennzahl: benannte fremde CODEBASIS ohne Lizenz daneben. Sie darf
# sinken, nicht steigen — wie `audit_todo_without_plan`. Gemessen am
# 2026-08-31 nach der Einordnung.
CODE_OHNE_LIZENZ_MAX = 34


def messe(repo: pathlib.Path):
    """(gesamt, ohne_lizenz) — ohne_lizenz als Liste von (datei, zeile, text)."""
    treffer = A.scan_attributions(repo)
    ohne = []
    for rel, zeile, text in treffer:
        try:
            zeilen = (repo / rel).read_text(
                encoding="utf-8", errors="replace").splitlines()
        except OSError:
            continue
        lo = max(0, zeile - 1 - FENSTER)
        hi = min(len(zeilen), zeile + FENSTER)
        umfeld = "\n".join(
            l for l in zeilen[lo:hi] if not EIGENE_SPDX.search(l))
        if not LIZENZ.search(umfeld):
            ohne.append((rel, zeile, text))
    return treffer, ohne


def einordnen(repo: pathlib.Path):
    """{Klasse: [(datei, zeile, quelle, hat_lizenz)]}."""
    treffer, ohne = messe(repo)
    ohne_schl = {(a, b) for a, b, _ in ohne}
    aus: dict[str, list] = {k: [] for k in KLASSEN}
    for rel, zeile, text in treffer:
        k, quelle = klassifiziere(text)
        aus[k].append((rel, zeile, quelle, (rel, zeile) not in ohne_schl))
    return aus, len(treffer)


def check(repo) -> list:
    """Schnittstelle fuer check_consistency.py.

    Bis MF-737 gab dieses Werkzeug bewusst `[]` zurueck: eine Attribution
    ohne Lizenz ist nichts Verbotenes, sondern etwas
    Entscheidungsbeduerftiges (MF-636). Das gilt weiter — aber nur fuer
    die Frage OB.

    Neu ist eine Sperrklinke: die Zahl der benannten fremden CODEBASEN
    ohne Lizenz daneben darf **sinken, nicht steigen**. Wer eine neue
    Ableitung erklaert, nennt ihre Lizenz mit — das ist die Regel aus
    MF-636, jetzt mit Zaehler. Doku-Verweise und Prosa beruehrt sie nicht.
    """
    try:
        aus, _ = einordnen(pathlib.Path(repo))
    except Exception as exc:                        # noqa: BLE001
        return ["Attributions-Einordnung nicht pruefbar: %s" % exc]
    offen = [x for x in aus["CODE"] if not x[3]]
    if len(offen) <= CODE_OHNE_LIZENZ_MAX:
        return []
    fehler = [
        "%d benannte fremde Codebasen ohne Lizenz daneben — die "
        "Grundlinie ist %d (gemessen MF-737). Sie darf sinken, nicht "
        "steigen. Eine Ableitungserklaerung nennt die Lizenz ihrer "
        "Quelle (MF-636); wer nur fremde Doku gelesen hat, schreibt das "
        "auch so." % (len(offen), CODE_OHNE_LIZENZ_MAX)]
    for rel, zeile, q, _ in offen[CODE_OHNE_LIZENZ_MAX:]:
        fehler.append("  %s:%d: %s" % (rel, zeile, q[:70]))
    return fehler


def main() -> int:
    repo = pathlib.Path(__file__).resolve().parent.parent
    aus, gesamt = einordnen(repo)
    print("Attributionen gesamt          : %d" % gesamt)
    print()
    print("%-8s %8s %9s %9s" % ("Klasse", "gesamt", "m. Lizenz", "o. Lizenz"))
    for k in KLASSEN:
        m = sum(1 for x in aus[k] if x[3])
        o = len(aus[k]) - m
        print("%-8s %8d %9d %9d" % (k, m + o, m, o))
    print()
    print("Rueckstand (CODE ohne Lizenz) : %d  (Grundlinie %d)"
          % (sum(1 for x in aus["CODE"] if not x[3]), CODE_OHNE_LIZENZ_MAX))
    ziel = None
    for k in KLASSEN:
        if "--" + k.lower() in sys.argv:
            ziel = k
    if "--liste" in sys.argv and ziel is None:
        ziel = "CODE"
    if ziel:
        nur_ohne = "--ohne" in sys.argv or ziel == "CODE"
        print()
        print("=== %s%s ===" % (ziel, " (ohne Lizenz)" if nur_ohne else ""))
        for rel, zeile, q, hl in aus[ziel]:
            if nur_ohne and hl:
                continue
            print("  %s:%d  %s" % (rel, zeile, q[:70]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
