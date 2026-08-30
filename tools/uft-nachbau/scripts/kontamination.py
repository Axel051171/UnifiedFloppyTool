#!/usr/bin/env python3
"""kontamination.py — die Brandmauer-Pruefung der Werkstatt (MF-696).

    kontamination.py --selbsttest
    kontamination.py --grundlinie <vorlage> [-o work/grundlinie.json]
    kontamination.py <neubau> <vorlage> [--weissliste f] [--baum <pfad>]

Vergleicht Neubau gegen Vorlage auf drei Ausdrucks-Ebenen: Bezeichner,
Kommentar-Wortfolgen (>=4 Woerter in Folge), String-Literale (>=8
Zeichen).

── Das Mass kommt aus dem Herkunftsaudit, nicht aus einer Liste ────────

`docs/QUARANTINE_PROCESS.md` §4 sagt seit MF-635, was zaehlt:

> **Beweiskraeftig sind Idiome, nicht Fakten.** Identische Tabellen,
> Konstanten, Feldreihenfolgen oder Formatgroessen beweisen nichts — die
> haette jede unabhaengige Implementierung auch, weil sie die
> Spezifikation sind. Beweiskraeftig sind: [...] **uebernommene
> Variablennamen**, Kommentar-Echos, [...]

Vorgefuehrt am Fall selbst: `carryshift` war deshalb der Beweis, weil
der Bezeichner **erfunden** ist — er steht in keiner Spezifikation und
in keinem dritten Baum. Die GCR-Tabellen derselben Dateien waren
wertlos als Indiz, weil sie Commodore-Spezifikation sind.

Genau dieses Mass laesst sich **ableiten** statt pflegen: Wie selten
ist ein geteilter Bezeichner **anderswo**? Ein Name, den nur diese
zwei Dateien kennen, ist die `carryshift`-Klasse. Ein Name, der in
zwanzig weiteren Dateien des eigenen Baums steht, ist Hausvokabular
und beweist nichts.

    andere Dateien = 0        BEWEISKRAEFTIG   (erfunden, nur hier)
    1 .. SCHWELLE-1           PRUEFEN
    >= SCHWELLE               Hausvokabular    (nachrichtlich)

Die Erstfassung fuehrte stattdessen eine Liste `ALLGEMEIN` mit
Bibliotheksvokabular (`fopen`, `memcpy`, `header`, `offset`, …). Das
ist die Aufzaehlung bekannter Faelle, die dieser Baum neunmal still
veralten sah — und sie wirkt in die **gefaehrliche** Richtung: was auf
der Liste steht, wird nicht mehr gemeldet. Ein vergessener Eintrag
kostet Rauschen, ein zuviel eingetragener kostet einen echten Befund.

Kommentar-Echos und Strings bleiben unabhaengig davon **scharf**: sie
sind nach §4 beweiskraeftig, egal wie verbreitet ihre Woerter sind.

── Die Weissliste: normalisiert, und sie meldet ihre eigenen Blinden ──

Dokumentierte Feldnamen duerfen geteilt sein. Die Erstfassung verglich
sie woertlich, und die HFE-Liste trug `number_of_side` und
`number_of_track` (Singular) sowie `formatrevision` (ohne Unterstrich),
waehrend der Code `number_of_sides`, `number_of_tracks` und
`format_revision` benutzt. Ergebnis: drei Spec-Fakten standen als
Befund da, und das README erklaerte sie zu „Spec-Fakten in
Zweitschreibung" — also zu etwas, das man uebergeht. Ein Tor, dessen
rote Zeilen man wegerklaert, ist keins mehr.

Verglichen wird darum **normalisiert** (klein, ohne Unterstriche, ohne
Plural-s) — das faengt alle drei realen Faelle.

Zusaetzlich meldet der Lauf, welche Weisslisten-Eintraege **nirgends**
getroffen haben: das ist ein Tippfehler oder ein veralteter Eintrag,
und er schuetzt derzeit nichts. Diese Richtung kann nichts
unterdruecken.

Was hier bewusst NICHT steht: eine „Beinah-Treffer"-Heuristik ueber
Teilzeichenketten. Sie war da und war gemessen eine LUECKE — `encoding`
und `offset` wurden von den laengeren Eintraegen
`track0s0_altencoding` und `track_list_offset` verschluckt und
verschwanden als Befund. Ein Mechanismus, der nur falsche Negative
erzeugt, gehoert weg.

── Was die Null NICHT heisst ───────────────────────────────────────────

Null Befunde sind die **notwendige**, nie die hinreichende Bedingung.
Nicht gesehen werden: Funktionszerlegung und Aufrufreihenfolge,
Fehlerbehandlungs-Idiome, charakteristische Schwellwerte ohne
Spec-Grundlage — drei der sieben beweiskraeftigen Klassen aus §4. Das
Herkunftsaudit bleibt Handarbeit; dieses Werkzeug nimmt ihm die zwei
mechanisch pruefbaren Klassen ab und sagt, welche fuenf offen bleiben.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from collections import Counter

HIER = os.path.dirname(os.path.abspath(__file__))

# Ab so vielen ANDEREN Dateien im eigenen Baum gilt ein geteilter
# Bezeichner als Hausvokabular. Die Zahl ist eine Konvention, keine
# Messung — sie steht hier als EINE Stelle, damit sie nicht an drei
# Orten driftet, und der Bericht nennt sie bei jedem Lauf.
SCHWELLE_HAUSVOKABULAR = 5

C_KEYWORDS = set("""auto break case char const continue default do
double else enum extern float for goto if inline int long register
restrict return short signed sizeof static struct switch typedef union
unsigned void volatile while uint8_t uint16_t uint32_t uint64_t int8_t
int16_t int32_t int64_t size_t bool true false NULL nullptr include
define ifdef ifndef endif pragma""".split())

IDENT_RX = re.compile(r"\b[A-Za-z_]\w{5,}\b")
STRING_RX = re.compile(r'"((?:[^"\\\n]|\\.){8,}?)"')
KOMM_RX = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)
QUELLEN = (".c", ".cc", ".cpp", ".h", ".hpp", ".rs", ".py")


def normal(w: str) -> str:
    """Klein, ohne Unterstriche, ohne Plural-s — fuer die Weissliste."""
    w = w.lower().replace("_", "")
    return w[:-1] if w.endswith("s") and len(w) > 3 else w


def dateien(pfad: str) -> list[str]:
    """Quelldateien unter @p pfad.

    Bewusst `os.walk` und NICHT `scripts/repo_scope.py`: die Vorlage
    ist ein FREMDER Baum, der nicht im Repo liegt und dort auch nicht
    liegen soll. `repo_scope` beantwortet die Frage „was sieht CI?" —
    hier lautet die Frage „was hat die Vorlage?". Zwei verschiedene
    Fragen, zwei verschiedene Mengen (MF-696).
    """
    if os.path.isfile(pfad):
        return [pfad]
    aus = []
    for dp, dn, fn in os.walk(pfad):
        dn[:] = [d for d in dn if d not in (".git", "__pycache__")]
        aus += [os.path.join(dp, f) for f in fn if f.endswith(QUELLEN)]
    return sorted(aus)


def extrahiere(pfade: list[str]):
    idents, strings, kommentare = Counter(), Counter(), set()
    for p in pfade:
        try:
            text = open(p, encoding="utf-8", errors="replace").read()
        except OSError:
            continue
        for m in KOMM_RX.finditer(text):
            worte = re.findall(r"[A-Za-zÄÖÜäöüß]{3,}", m.group(0))
            for i in range(len(worte) - 3):
                kommentare.add(" ".join(w.lower() for w in worte[i:i + 4]))
        code = KOMM_RX.sub(" ", text)
        for m in STRING_RX.finditer(code):
            strings[m.group(1)] += 1
        code2 = STRING_RX.sub('""', code)
        for m in IDENT_RX.finditer(code2):
            w = m.group(0)
            if w not in C_KEYWORDS:
                idents[w] += 1
    return idents, strings, kommentare


def hintergrund(baum: str, ausser: set[str]) -> dict[str, int]:
    """Wie viele ANDERE Dateien des eigenen Baums kennen den Namen?

    Das ist der Ersatz fuer die gepflegte `ALLGEMEIN`-Liste: abgeleitet,
    aktualisiert sich selbst, und misst genau das, worauf §4 abstellt —
    ist der Bezeichner erfunden oder verbreitet?
    """
    aus: dict[str, int] = Counter()
    if not baum or not os.path.isdir(baum):
        return aus
    r = subprocess.run(["git", "-C", baum, "ls-files", "--cached",
                        "--others", "--exclude-standard", "-z"],
                       capture_output=True, timeout=180)
    if r.returncode != 0:
        return aus
    for roh in r.stdout.split(b"\0"):
        if not roh:
            continue
        rel = roh.decode("utf-8", errors="replace")
        if not rel.endswith(QUELLEN):
            continue
        voll = os.path.abspath(os.path.join(baum, rel))
        if voll in ausser:
            continue
        try:
            text = open(voll, encoding="utf-8", errors="replace").read()
        except OSError:
            continue
        code = STRING_RX.sub('""', KOMM_RX.sub(" ", text))
        for w in set(IDENT_RX.findall(code)):
            aus[w] += 1
    return aus


def pruefe(neubau: str, vorlage: str, weissliste: set[str],
           baum: str | None = None):
    """@return (befunde, nachrichtlich, weissliste_ohne_treffer)."""
    n_pfade = dateien(neubau)
    ni, ns, nk = extrahiere(n_pfade)
    vi, vs, vk = extrahiere(dateien(vorlage))
    hg = hintergrund(baum, {os.path.abspath(p) for p in n_pfade}) \
        if baum else {}

    wl = {normal(w) for w in weissliste}
    befunde, nachricht = [], []

    for w in sorted(set(ni) & set(vi)):
        nw = normal(w)
        if nw in wl:
            continue
        andere = hg.get(w, 0)
        zeile = (w, f"Neubau {ni[w]}x, Vorlage {vi[w]}x, "
                    f"{andere} andere Datei(en) im eigenen Baum")
        if baum and andere >= SCHWELLE_HAUSVOKABULAR:
            nachricht.append(("BEZEICHNER (Hausvokabular)", *zeile))
        elif baum and andere > 0:
            befunde.append(("BEZEICHNER (pruefen)", *zeile))
        else:
            befunde.append(("BEZEICHNER (beweiskraeftig)", *zeile))

    # Strings und Kommentar-Echos: nach §4 beweiskraeftig, unabhaengig
    # von der Verbreitung ihrer Woerter.
    for s in sorted(set(ns) & set(vs)):
        if normal(s) in wl:
            continue
        befunde.append(("STRING", s[:60], ""))
    for k in sorted(nk & vk):
        if all(normal(t) in wl for t in k.split()):
            continue
        befunde.append(("KOMMENTAR-ECHO", k, ""))

    # Weisslisten-Eintraege OHNE jeden Treffer: Tippfehler oder veraltet
    # (MF-696).
    #
    # Die erste Anpassung hatte hier einen „Beinah-Treffer" ueber
    # Teilzeichenketten. Gemessen war er eine LUECKE: `encoding` und
    # `offset` wurden von den laengeren Eintraegen `track0s0_altencoding`
    # und `track_list_offset` verschluckt und verschwanden als Befund.
    # Ein Mechanismus, der nur falsche Negative erzeugt, gehoert weg —
    # und der normalisierte Vergleich faengt die drei realen Faelle
    # (Singular/Plural, fehlender Unterstrich) ohnehin schon.
    #
    # Was bleibt, ist dieselbe Frage aus der anderen Richtung, und die
    # kann nichts unterdruecken: welcher Eintrag hat NIRGENDS getroffen?
    alle = {normal(x) for x in list(ni) + list(vi) + list(ns) + list(vs)}
    ohne_treffer = sorted(q for q in weissliste if normal(q) not in alle)
    return befunde, nachricht, ohne_treffer


NICHT_GESEHEN = (
    "Was die Null NICHT heisst: sie ist die notwendige, nie die "
    "hinreichende Bedingung. Ungesehen bleiben drei der sieben "
    "beweiskraeftigen Klassen aus QUARANTINE_PROCESS.md §4 — "
    "Funktionszerlegung und Aufrufreihenfolge, "
    "Fehlerbehandlungs-Idiome, charakteristische Schwellwerte ohne "
    "Spec-Grundlage. Das Herkunftsaudit bleibt Handarbeit.")


def bericht(befunde, nachricht, beinah, wl_n, baum) -> None:
    print(f"{len(befunde)} Kontaminations-Befunde "
          f"(Weissliste: {wl_n} Fakten; "
          f"{'Hintergrund aus ' + baum if baum else 'OHNE Hintergrundbaum '
             '— jeder geteilte Name gilt als beweiskraeftig'})")
    for art, was, detail in befunde[:25]:
        print(f"- [{art}] {was}" + (f" ({detail})" if detail else ""))
    if len(befunde) > 25:
        print(f"- … {len(befunde) - 25} weitere")
    if beinah:
        print(f"\n{len(beinah)} Weisslisten-Eintraege ohne jeden Treffer "
              f"— Tippfehler oder veraltet. Sie schuetzen derzeit nichts; "
              f"bitte dort berichtigen:")
        print("  " + ", ".join(f"`{w}`" for w in beinah[:12]))
    if nachricht:
        print(f"\n{len(nachricht)} geteilte Namen sind Hausvokabular "
              f"(>= {SCHWELLE_HAUSVOKABULAR} andere Dateien im eigenen "
              f"Baum) — nachrichtlich, kein Befund:")
        for art, was, detail in nachricht[:10]:
            print(f"  {was} ({detail})")
    if befunde:
        print("\nJeder Befund => Herkunftsaudit nach "
              "`docs/QUARANTINE_PROCESS.md` §4.")
    print("\n" + NICHT_GESEHEN)


def selbsttest() -> int:
    basis = os.path.join(HIER, "..", "data", "selbsttest")
    ew = json.load(open(os.path.join(basis, "erwartung.json"),
                        encoding="utf-8"))
    wl = set(ew["weissliste"])
    baum = os.path.abspath(os.path.join(HIER, "..", "..", ".."))
    fehler = 0
    for fall in ew["faelle"]:
        b, _n, bh = pruefe(os.path.join(basis, fall["datei"]),
                           os.path.join(basis, ew["referenz"]), wl, baum)
        ist = len(b)
        soll_min = fall.get("mindestens")
        soll_genau = fall.get("genau")
        ok = (soll_genau is None or ist == soll_genau) and \
             (soll_min is None or ist >= soll_min)
        if not ok:
            fehler += 1
        print(f"  {'ok  ' if ok else 'FAIL'} {fall['datei']:22s} "
              f"{ist} Befund(e), Soll "
              f"{'genau ' + str(soll_genau) if soll_genau is not None
                 else '>=' + str(soll_min)}  — {fall['warum']}")
        if not ok:
            for x in b[:4]:
                print(f"        {x[0]}: {x[1]}")
        if bh:
            print(f"        (Weissliste ohne Treffer: {bh})")
    print(f"\nSelbsttest: {fehler} Abweichung(en) bei "
          f"{len(ew['faelle'])} Faellen")
    if fehler:
        print("Werkzeug nicht fertig — eine Brandmauer, deren Abnahme "
              "rot ist, darf keinen Nachbau freigeben.")
    return 3 if fehler else 0


def grundlinie(vorlage: str, out: str) -> int:
    """Die Vorlage allein inventarisieren — VOR der Uebergabe.

    AGENT.md Regel 6 verlangt sie seit der Erstfassung; ein Modus dafuer
    fehlte (MF-696). Ohne Grundlinie ist die Pruefung NACH Hand B nicht
    einordenbar: man sieht Treffer, aber nicht, wie gross der Heuhaufen
    war, aus dem sie stammen.
    """
    i, s, k = extrahiere(dateien(vorlage))
    paket = {"vorlage": vorlage,
             "bezeichner": len(i), "strings": len(s),
             "kommentar_wortfolgen": len(k),
             "top_bezeichner": [w for w, _ in i.most_common(40)]}
    os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
    json.dump(paket, open(out, "w", encoding="utf-8"),
              ensure_ascii=False, indent=1)
    print(f"Grundlinie: {len(i)} Bezeichner, {len(s)} Strings, "
          f"{len(k)} Kommentar-Wortfolgen -> {out}")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(add_help=False)
    ap.add_argument("stellen", nargs="*")
    ap.add_argument("--selbsttest", action="store_true")
    ap.add_argument("--grundlinie")
    ap.add_argument("--weissliste")
    ap.add_argument("--baum", default=os.path.abspath(
        os.path.join(HIER, "..", "..", "..")))
    ap.add_argument("-o", dest="out", default="work/grundlinie.json")
    ap.add_argument("-h", "--help", action="store_true")
    a = ap.parse_args()

    if a.help or (not a.selbsttest and not a.grundlinie
                  and len(a.stellen) < 2):
        print(__doc__)
        return 0 if a.help else 2
    if a.selbsttest:
        return selbsttest()
    if a.grundlinie:
        return grundlinie(a.grundlinie, a.out)

    wl = set()
    if a.weissliste:
        wl = {z.strip().lower() for z in
              open(a.weissliste, encoding="utf-8") if z.strip()}
    b, n, bh = pruefe(a.stellen[0], a.stellen[1], wl, a.baum)
    bericht(b, n, bh, len(wl), a.baum)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
