#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Eine GCR-Tafel auf das Codec-Register umhaengen — ohne Textmuster
(MF-1522).

── Warum es dieses Werkzeug gibt ─────────────────────────────────────────

Beim Umhaengen von P3-666 haben in einer Sitzung **vier** handgeschriebene
Textmuster je einen Teil ihrer Menge nicht gesehen, und jedes sah
vollstaendig aus:

  1. `sed` mit `$`-Anker — zwei CMake-Zeilen enden auf `)` statt auf `.c`
  2. Gegenprobe ueber `elseif`-Bloecke — die Sammelbibliothek ist keiner
  3. Tafelblock-Muster mit `/* */` — die Datei benutzt `//`
  4. Index-Muster `\\[([^\\]]+)\\]` — `tafel[(data[0] >> 4) & 0x0F]` hat
     eine VERSCHACHTELTE Klammer, das Muster stoppt am inneren `]`

Fall 4 hat acht Stellen zu `(data[0) >> 4) & 0x0F]` zerbrochen; gefangen
hat es der Compiler. Fall 2 hat 89 Link-Ziele ungeprueft gelassen;
gefangen hat es ein Blastradius-Lauf. Die Lehre ist nicht „sorgfaeltiger
mustern", sondern: **Klammern werden gezaehlt, nicht gematcht**, und jede
Ersetzung endet mit einer Gegenprobe, die zaehlt.

── Was es tut ────────────────────────────────────────────────────────────

    python scripts/gcr_umhaengen.py <datei> --tafel <name>:<art> [...]
                                   [--pruefen]

`art` ist `encode` oder `decode`. Je Tafel:

  1. die DEFINITION entfernen (zuerst, weil `tafel[16] = {…}` wie ein
     Zugriff aussieht) und durch einen Verweis ersetzen
  2. jeden ZUGRIFF ersetzen, den Index durch Klammerzaehlen bestimmt
  3. gegenpruefen: kein Bezeichner mehr im Code (Kommentare ausgenommen),
     Klammerbilanz der Datei unveraendert, Zahl der Ersetzungen gemeldet

`--pruefen` schreibt nichts und meldet nur, was es taete.

Was es NICHT tut: es uebersetzt nicht und laeuft keine Tests. Beides
gehoert in den Aufrufer, und ohne beides ist eine Umhaengung nicht fertig.
"""
from __future__ import annotations

import argparse
import io
import re
import sys
from pathlib import Path

RUF = {
    "encode": "uft_gcr_kodieren(UFT_GCR_CBM_5_4, %s)",
    "decode": "uft_gcr_dekodieren(UFT_GCR_CBM_5_4, %s)",
}


def index_ab(text: str, pos: int) -> tuple[str, int]:
    """Inhalt der eckigen Klammer, die bei `pos` beginnt, und ihr Ende.

    Gezaehlt, nicht gematcht: `tafel[(data[0] >> 4) & 0x0F]` liefert
    `(data[0] >> 4) & 0x0F`, nicht `(data[0`.
    """
    if text[pos] != "[":
        raise ValueError("kein '[' an Position %d" % pos)
    tief = 0
    for i in range(pos, len(text)):
        if text[i] == "[":
            tief += 1
        elif text[i] == "]":
            tief -= 1
            if tief == 0:
                return text[pos + 1:i], i + 1
    raise ValueError("unbalancierte Klammer ab %d" % pos)


def ohne_kommentare(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def bilanz(text: str) -> tuple[int, int, int]:
    k = ohne_kommentare(text)
    return (k.count("(") - k.count(")"),
            k.count("[") - k.count("]"),
            k.count("{") - k.count("}"))


def definition_weg(text: str, name: str, verweis: str) -> tuple[str, bool]:
    """Die Tafeldefinition entfernen, samt vorangehender Kommentarzeile.

    Erkannt wird sie an `= {` — das unterscheidet sie von jedem Zugriff,
    unabhaengig von der Kommentarform davor.
    """
    #   Der Typ darf MEHRWORTIG sein (`unsigned char`, `const uint8_t`,
    #   `static const unsigned char`) — ein erster Versuch nahm `\w+\s+`
    #   und fand `unsigned char tab[16]` nicht, woraufhin `zugriffe_weg()`
    #   die Definition als Zugriff ersetzte. Der Selbsttest hat das
    #   gefangen; es ist derselbe Fehler wie die vier Musterverfehlungen
    #   im Kopf dieser Datei, nur eine Ebene tiefer.
    m = re.search(r"(?:^[ \t]*(?://[^\n]*|/\*(?:[^*]|\*(?!/))*\*/)\n)?"
                  r"^([ \t]*)(?:\w+[ \t]+){1,4}"
                  + re.escape(name) + r"[ \t]*\[[^\]]*\][ \t]*=[ \t]*"
                  r"\{[^}]*\};[ \t]*\n",
                  text, re.M)
    if not m:
        return text, False
    einzug = m.group(1)
    block = "".join("%s%s\n" % (einzug, z) if z else "\n"
                    for z in verweis.split("\n"))
    return text[:m.start()] + block + text[m.end():], True


def zugriffe_weg(text: str, name: str, art: str) -> tuple[str, int]:
    n = 0
    while True:
        m = re.search(r"\b%s\s*\[" % re.escape(name), text)
        if not m:
            return text, n
        idx, ende = index_ab(text, m.end() - 1)
        text = text[:m.start()] + (RUF[art] % idx.strip()) + text[ende:]
        n += 1
        if n > 200:
            raise SystemExit("ueber 200 Ersetzungen bei %s — abgebrochen" % name)


def umhaengen(pfad: Path, tafeln: list[tuple[str, str]],
              verweis: str, schreiben: bool = True) -> dict:
    roh = io.open(pfad, encoding="utf-8").read()
    vor = bilanz(roh)
    t = roh
    bericht = {"definitionen": [], "zugriffe": {}}

    for name, art in tafeln:                 # Definitionen ZUERST
        t, ok = definition_weg(t, name, verweis)
        bericht["definitionen"].append((name, ok))
    for name, art in tafeln:
        t, n = zugriffe_weg(t, name, art)
        bericht["zugriffe"][name] = n

    kern = ohne_kommentare(t)
    bericht["rest"] = sorted({n for n, _ in tafeln
                              if re.search(r"\b%s\b" % re.escape(n), kern)})
    bericht["bilanz_vorher"] = vor
    bericht["bilanz_nachher"] = bilanz(t)
    bericht["aufrufe"] = len(re.findall(r"uft_gcr_(?:de)?kodieren", kern))
    bericht["ok"] = (not bericht["rest"]
                     and bericht["bilanz_vorher"] == bericht["bilanz_nachher"])
    if schreiben and bericht["ok"]:
        io.open(pfad, "w", encoding="utf-8", newline="").write(t)
    return bericht


# ------------------------------------------------------------- Selbsttest

def _selbsttest() -> int:
    """Die vier Musterverfehlungen dieser Sitzung als Faelle (MF-1522)."""
    import tempfile
    faelle = []

    def lauf(inhalt: str, tafeln, schreiben=True):
        d = Path(tempfile.mkdtemp()) / "x.c"
        d.write_text(inhalt, encoding="utf-8")
        b = umhaengen(d, tafeln, "// umgehaengt", schreiben)
        return b, d.read_text(encoding="utf-8")

    # 1 VERSCHACHTELTE Klammer — der Fall, der acht Stellen zerbrach.
    b, neu = lauf(
        "static const unsigned char tab[16] = { 1,2,3,4 };\n"
        "void f(unsigned char *d, unsigned char *n) {\n"
        "    n[0] = tab[(d[0] >> 4) & 0x0F];\n"
        "    n[1] = tab[d[0] & 0x0F];\n}\n",
        [("tab", "encode")])
    faelle.append(("verschachtelte Klammer bleibt heil",
                   "(d[0] >> 4) & 0x0F)" in neu and "(d[0)" not in neu))
    faelle.append(("beide Zugriffe ersetzt", b["zugriffe"]["tab"] == 2))
    faelle.append(("Klammerbilanz unveraendert",
                   b["bilanz_vorher"] == b["bilanz_nachher"]))

    # 2 Die DEFINITION darf nicht als Zugriff gelten.
    faelle.append(("Definition ist weg, nicht ersetzt",
                   "tab[16]" not in neu
                   and "uft_gcr_kodieren(UFT_GCR_CBM_5_4, 16)" not in neu))

    # 3 `//`-Kommentar davor statt `/* */`.
    b3, neu3 = lauf(
        "// GCR-Tafel, alte Form\n"
        "static const unsigned char t2[4] = { 9,8,7,6 };\n"
        "void g(unsigned char *n) { n[0] = t2[1]; }\n",
        [("t2", "decode")])
    faelle.append(("//-Kommentar wird mit entfernt",
                   "alte Form" not in neu3 and b3["definitionen"][0][1]))
    faelle.append(("decode-Art ruft dekodieren",
                   "uft_gcr_dekodieren" in neu3))

    # 4 Kein Rest, und zwei Tafeln in einem Lauf.
    b4, neu4 = lauf(
        "static const unsigned char a[4] = { 1,2,3,4 };\n"
        "static const unsigned char b[4] = { 5,6,7,8 };\n"
        "void h(unsigned char *n) { n[0] = a[0]; n[1] = b[a[1]]; }\n",
        [("a", "encode"), ("b", "decode")])
    faelle.append(("zwei Tafeln, kein Rest", b4["rest"] == []))
    faelle.append(("Tafelzugriff IM Index wird auch ersetzt",
                   "uft_gcr_dekodieren(UFT_GCR_CBM_5_4, "
                   "uft_gcr_kodieren(UFT_GCR_CBM_5_4, 1))" in neu4))

    # 5 `--pruefen` schreibt nicht.
    b5, neu5 = lauf(
        "static const unsigned char c[4] = { 1,2,3,4 };\n"
        "void i(unsigned char *n) { n[0] = c[0]; }\n",
        [("c", "encode")], schreiben=False)
    faelle.append(("Pruefmodus laesst die Datei unberuehrt",
                   "c[4]" in neu5 and b5["zugriffe"]["c"] == 1))

    # 6 Unbalancierte Klammer wird gemeldet, nicht verschluckt.
    try:
        index_ab("tab[1 + 2", 3)
        offen = False
    except ValueError:
        offen = True
    faelle.append(("unbalancierte Klammer wirft", offen))

    gut = sum(1 for _, ok in faelle if ok)
    for name, ok in faelle:
        if not ok:
            print("  ROT  %s" % name)
    print("Selbsttest: %d/%d" % (gut, len(faelle)))
    return 0 if gut == len(faelle) else 1


def main() -> int:
    sys.stdout.reconfigure(encoding="utf-8")
    if "--selftest" in sys.argv:
        return _selbsttest()

    p = argparse.ArgumentParser()
    p.add_argument("datei")
    p.add_argument("--tafel", action="append", required=True,
                   help="<name>:<encode|decode>, mehrfach erlaubt")
    p.add_argument("--verweis", default=None,
                   help="Kommentar, der die Definition ersetzt")
    p.add_argument("--pruefen", action="store_true")
    a = p.parse_args()

    tafeln = []
    for spec in a.tafel:
        name, _, art = spec.partition(":")
        if art not in RUF:
            print("unbekannte Art %r (encode|decode)" % art)
            return 2
        tafeln.append((name, art))

    verweis = a.verweis or (
        "// Die GCR-Tafel stand hier bis MF-1522 — jetzt aus dem\n"
        "// Codec-Register (`uft/core/uft_gcr.h`): die Wortmenge folgt dort\n"
        "// einer gemessenen Regel, die Zuordnung steht genau einmal.\n"
        "#include \"uft/core/uft_gcr.h\"")

    b = umhaengen(Path(a.datei), tafeln, verweis, schreiben=not a.pruefen)
    print("Definitionen: %s"
          % ", ".join("%s %s" % (n, "entfernt" if ok else "NICHT GEFUNDEN")
                      for n, ok in b["definitionen"]))
    print("Zugriffe: %s" % b["zugriffe"])
    print("Klammerbilanz: %s -> %s"
          % (b["bilanz_vorher"], b["bilanz_nachher"]))
    print("Registeraufrufe danach: %d" % b["aufrufe"])
    print("Rest im Code: %s" % (b["rest"] or "keiner"))
    if not b["ok"]:
        print("NICHT GESCHRIEBEN — Gegenprobe gefallen")
        return 1
    print("geschrieben" if not a.pruefen else "Pruefmodus: nichts geschrieben")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
