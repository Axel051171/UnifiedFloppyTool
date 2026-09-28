#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""„Habe ich das schon?" — ein Zulieferordner gegen den Korpus, nach INHALT.

Anlass (MF-1491): der Eigentuemer zeigt auf Ordner mit Diskettenabbildern,
und es kommen laufend neue hinzu. Eine Antwort in Prosa ist am naechsten
Tag falsch. Gemessen an einem Ordner, der waehrend der Arbeit von 23 auf
27,9 GB wuchs: `.a2r` ging von 356 auf 1213 Dateien.

Drei Dinge, die dieses Skript tut, und jedes hat an einem Tag einmal
gefehlt:

**1. Vergleich nach Inhalt, nicht nach Namen.** Derselbe Inhalt liegt im
Korpus unter `waitless_d1.a2r` und im Ordner unter `WaitLess - Disk 1.a2r`;
umgekehrt heissen zwei verschiedene Aufnahmen desselben Titels gleich.
Gemessen am ersten Lauf: 15 Dateien teilten eine Groesse mit einem
Korpuseintrag, **5** waren wirklich dieselben.

**2. Torsi erkennen, bevor sie Beleg werden.** Eine halbe Flussdatei liest
sich PLAUSIBEL — der Leser laeuft durch, was da ist. `Alge-Blaster Plus`
erklaerte im STRM-Kopf 19 919 745 Byte und hatte 4 194 304; sie stand mit
„26 Orten" und ordentlicher Geometrie auf einer Kandidatenliste (MF-1487).
Geprueft wird deshalb: Nullkopf, und bei A2R, dass die Blocklaengen GENAU
bei der Dateigroesse enden und ein Block hinter dem Flussblock steht.

**3. Die Luecke benennen** — welche Endungen der Ordner traegt, die der
Korpus nicht hat.

── Was es NICHT tut ─────────────────────────────────────────────────

Es hasht nicht 28 GB. Wer keine Groesse mit einem Korpuseintrag teilt,
kann kein Duplikat sein; nur Groessentreffer werden gehasht. Das ist eine
Aussage ueber Gleichheit, nicht ueber Aehnlichkeit — zwei Aufnahmen
derselben Diskette sind verschieden und sollen es sein.

Es entscheidet nichts ueber Lizenz oder Aufnahme. Ob eine Datei in den
Korpus DARF, ist eine Eigentuemerfrage (LIZ-1, docs/QUARANTINE_PROCESS.md);
dieses Skript sagt nur, ob sie schon drin ist und ob sie ganz ist.

Die Endungsmenge ist ABGELEITET, nicht gepflegt: sie kommt aus den
Endungen der registrierten Plugins (`gen_format_list.py --md`) plus den
Endungen, die im Manifest vorkommen. Eine Handliste waere die Aufzaehlung
aus MF-636, die in diesem Baum viermal veraltet ist — ein neues Format
waere sonst unsichtbar.
"""
from __future__ import annotations

import argparse
import collections
import hashlib
import io
import json
import os
import struct
import subprocess
import sys
from pathlib import Path

WURZEL = Path(__file__).resolve().parents[1]
MANIFEST = WURZEL / "tests" / "corpus_manifest" / "manifest.json"


# ── reine Funktionen ────────────────────────────────────────────────

def endungen_aus_md(md_text: str) -> set:
    """Endungen aus der Markdown-Tafel von `gen_format_list.py --md`.

    Die Spalte traegt sie mit `;` getrennt (`"dsk;img;raw"`), teils mit
    Komma (`"cpm,dsk"`), teils leer. Beides wird zerlegt; was kein
    Bezeichner ist, faellt weg.
    """
    aus = set()
    for z in (md_text or "").split("\n"):
        sp = z.split("|")
        if len(sp) < 4 or not sp[1].strip().startswith("**"):
            continue
        for e in sp[2].replace(",", ";").split(";"):
            e = e.strip().strip("`").lower()
            if e and e.isalnum():
                aus.add(e)
    return aus


def endungen_aus_manifest(bilder) -> set:
    """Endungen, die im Manifest vorkommen — auch ohne Plugin.

    Der Korpus fuehrt Dateien, fuer die es kein Plugin gibt (Berichte mit
    `format: "n/a"`, Schutz-Belege). Wer sie nicht mitnimmt, meldet sie
    spaeter als „neu", obwohl sie schon drin sind.
    """
    aus = set()
    for e in bilder or []:
        f = (e.get("file") or "")
        if "." in f:
            aus.add(f.rsplit(".", 1)[-1].lower())
    return aus


def ist_hohl(kopf: bytes) -> bool:
    """Nur Nullbytes im gelesenen Kopf — Platzhalter eines Downloads.

    Gemessen: 458 von 1213 `.a2r` in einem laufenden Torrent waren das.
    Ein leerer Puffer ist KEIN Abbild, auch wenn die Dateigroesse stimmt.
    """
    return len(kopf) > 0 and not any(kopf)


def a2r_bloecke(daten: bytes):
    """(bloecke, endversatz) eines A2R — oder (None, None), wenn kein A2R.

    Kopf: "A2R2"/"A2R3" + 0xFF 0x0A 0x0D 0x0A, danach
    <kennung:4><laenge:4 LE><nutzlast>. Quelle: applesaucefdc.com/a2r/
    und .../a2r2-reference/ (gelesen MF-1484).
    """
    if len(daten) < 16 or daten[:4] not in (b"A2R2", b"A2R3"):
        return None, None
    if daten[4] != 0xFF or daten[5:8] != b"\x0a\x0d\x0a":
        return None, None
    bl, i, ende = [], 8, 8
    while i + 8 <= len(daten):
        kz = daten[i:i + 4]
        if not all(32 <= x < 127 for x in kz):
            break
        ln = struct.unpack("<I", daten[i + 4:i + 8])[0]
        bl.append(kz.decode("ascii"))
        ende = i + 8 + ln
        i = ende
    return bl, ende


def a2r_urteil(bloecke, endversatz, dateigroesse: int):
    """('ganz'|'abgeschnitten'|'unklar', Grund).

    Ganz heisst: die Blocklaengen enden GENAU bei der Dateigroesse UND
    hinter dem Flussblock steht noch einer (das beweist, dass der
    Flussblock fertig geschrieben wurde). Fehlt der zweite Teil, ist es
    nicht „kaputt", sondern unklar — und Unklares wird nicht als Beleg
    genommen.
    """
    if bloecke is None:
        return "unklar", "kein A2R-Kopf"
    if endversatz > dateigroesse:
        return ("abgeschnitten",
                "Bloecke erklaeren %d Byte, die Datei hat %d"
                % (endversatz, dateigroesse))
    if endversatz < dateigroesse:
        return ("unklar", "%d Byte hinter dem letzten Block"
                % (dateigroesse - endversatz))
    fluss = [k for k in bloecke if k in ("STRM", "RWCP")]
    if not fluss:
        return "unklar", "kein STRM/RWCP"
    if bloecke[-1] in ("STRM", "RWCP"):
        return ("unklar",
                "Laengen gehen auf, aber hinter %s steht kein Block — "
                "dass er fertig ist, ist damit nicht belegt" % bloecke[-1])
    return "ganz", "Laengen gehen auf, %s steht hinter %s" % (
        bloecke[-1], fluss[-1])


def groessen_treffer(korpus_groessen, kandidaten):
    """Kandidaten, deren Groesse im Korpus vorkommt — nur die lohnen Hash.

    `korpus_groessen`: Menge von Groessen. `kandidaten`: [(groesse, pfad)].
    """
    g = set(korpus_groessen or ())
    return [(gr, p) for gr, p in (kandidaten or []) if gr in g]


# ── Messung am Baum ─────────────────────────────────────────────────

def sha256_datei(p: Path) -> str:
    h = hashlib.sha256()
    with io.open(p, "rb") as f:
        for s in iter(lambda: f.read(1 << 20), b""):
            h.update(s)
    return h.hexdigest()


def plugin_endungen() -> set:
    try:
        r = subprocess.run([sys.executable,
                            str(WURZEL / "scripts" / "gen_format_list.py"),
                            "--md"], capture_output=True, text=True,
                           encoding="utf-8", errors="replace", cwd=WURZEL)
        return endungen_aus_md(r.stdout)
    except OSError:
        return set()


def korpus_lesen():
    """sha256 -> (Pfad im Repo, Groesse oder None), plus die Bilderliste."""
    d = json.loads(MANIFEST.read_text(encoding="utf-8"))
    bilder = d.get("images", [])
    nach_hash = {}
    for e in bilder:
        p = WURZEL / e["file"].replace("/", os.sep)
        nach_hash[e["sha256"]] = (
            e["file"], p.stat().st_size if p.is_file() else None)
    return nach_hash, bilder


def main() -> int:
    ap = argparse.ArgumentParser(
        description="Zulieferordner gegen den Korpus, nach Inhalt")
    ap.add_argument("ordner", nargs="?", help="zu pruefender Ordner")
    ap.add_argument("--selbsttest", action="store_true")
    ap.add_argument("--kopf", type=int, default=4096,
                    help="Bytes fuer Kopf-/Hohlpruefung (Vorgabe 4096)")
    a = ap.parse_args()
    if a.selbsttest:
        return selbsttest()
    if not a.ordner:
        ap.error("ohne --selbsttest wird ein Ordner gebraucht")

    ordner = Path(a.ordner)
    if not ordner.is_dir():
        print("Ordner nicht vorhanden: %s" % ordner, file=sys.stderr)
        return 2

    nach_hash, bilder = korpus_lesen()
    endungen = plugin_endungen() | endungen_aus_manifest(bilder)
    groessen = {g for _, g in nach_hash.values() if g is not None}
    print("Korpus: %d Eintraege, %d verschiedene Hashes, %d Groessen"
          % (len(bilder), len(nach_hash), len(groessen)))
    print("Endungen abgeleitet: %d (Plugins + Manifest)" % len(endungen))

    kand, hohl, urteil = [], [], collections.Counter()
    for w, _, fs in os.walk(ordner):
        for f in fs:
            e = f.rsplit(".", 1)[-1].lower() if "." in f else ""
            if e not in endungen:
                continue
            p = Path(w) / f
            try:
                gr = p.stat().st_size
                kopf = io.open(p, "rb").read(a.kopf)
            except OSError:
                continue
            if ist_hohl(kopf):
                hohl.append(p)
                continue
            kand.append((gr, p, e))
            if e == "a2r":
                voll = io.open(p, "rb").read()
                bl, ende = a2r_bloecke(voll)
                urteil[a2r_urteil(bl, ende, gr)[0]] += 1

    print("\n%s" % ordner)
    print("  Abbild-Dateien (abgeleitete Endung): %d, %.2f GB"
          % (len(kand), sum(k[0] for k in kand) / 1e9))
    print("  davon HOHL (Nullkopf, nicht mitgezaehlt): %d" % len(hohl))
    if urteil:
        print("  A2R-Vollstaendigkeit: %s" % dict(urteil.most_common()))

    treffer = []
    gehasht = 0
    # `groessen_treffer` gibt (groesse, pfad) — zwei, nicht drei. Der
    # erste Entwurf entpackte drei und starb an der Aufrufstelle, obwohl
    # der Selbsttest die Funktion isoliert deckte: eine reine Funktion zu
    # pruefen ist nicht dasselbe wie ihren Aufruf zu pruefen.
    for _gr, p in groessen_treffer(groessen,
                                   [(g, q) for g, q, _ in kand]):
        gehasht += 1
        h = sha256_datei(p)
        if h in nach_hash:
            treffer.append((p.relative_to(ordner), nach_hash[h][0]))

    print("\n  Groessen-Treffer (also gehasht): %d" % gehasht)
    print("  INHALTSGLEICH mit dem Korpus: %d" % len(treffer))
    for t, k in sorted(treffer):
        print("    %-52s == %s" % (str(t)[:52], k))

    drin = {ordner / t for t, _ in treffer}
    neu = collections.Counter()
    neu_gr = collections.Counter()
    for gr, p, e in kand:
        if p in drin:
            continue
        neu[e] += 1
        neu_gr[e] += gr
    print("\n  NOCH NICHT im Korpus, nach Endung:")
    for e, n in neu.most_common(12):
        print("    %-10s %5d  %7.2f GB" % (e, n, neu_gr[e] / 1e9))
    print("    %-10s %5d  %7.2f GB" % ("Summe", sum(neu.values()),
                                       sum(neu_gr.values()) / 1e9))
    print("\nOB eine Datei in den Korpus DARF, sagt dieses Skript nicht — "
          "das ist eine\nEigentuemerfrage (LIZ-1, docs/QUARANTINE_PROCESS.md).")
    return 0


def selbsttest() -> int:
    f = []

    MD = ("| Format | Ext | Registrierung | Datei |\n"
          "|---|---|---|---|\n"
          "| **A2R** | a2r | auto | `x.c` |\n"
          "| **IMG** | dsk;img;raw | auto | `y.c` |\n"
          "| **CPM** | cpm,dsk | manuell | `z.c` |\n"
          "| nicht fett | zzz | auto | `n.c` |\n")
    e = endungen_aus_md(MD)
    f.append(("Endungen aus der Tafel, `;` und `,` zerlegt",
              e == {"a2r", "dsk", "img", "raw", "cpm"}))
    f.append(("eine Zeile ohne **Fettung** ist keine Formatzeile",
              "zzz" not in e))
    f.append(("leere Tafel gibt leere Menge",
              endungen_aus_md("") == set()
              and endungen_aus_md(None) == set()))

    f.append(("Manifest-Endungen kommen mit, auch ohne Plugin",
              endungen_aus_manifest(
                  [{"file": "a/b.txt"}, {"file": "c/d.A2R"},
                   {"file": "ohnepunkt"}]) == {"txt", "a2r"}))
    f.append(("Rand: kein Manifest",
              endungen_aus_manifest([]) == set()
              and endungen_aus_manifest(None) == set()))

    f.append(("lauter Nullen ist hohl", ist_hohl(b"\x00" * 64)))
    f.append(("ein Byte ungleich 0 ist nicht hohl",
              not ist_hohl(b"\x00" * 63 + b"\x01")))
    f.append(("leerer Kopf ist NICHT hohl — das waere eine Aussage ohne "
              "Messung", not ist_hohl(b"")))

    def a2r(bloecke, laengen, nutz=0):
        b = b"A2R2\xff\x0a\x0d\x0a"
        for k, ln in zip(bloecke, laengen):
            b += k.encode() + struct.pack("<I", ln) + b"\x00" * ln
        return b + b"\x00" * nutz

    d = a2r(["INFO", "STRM", "META"], [4, 8, 4])
    bl, ende = a2r_bloecke(d)
    f.append(("ganz: Laengen gehen auf und META steht hinter STRM",
              a2r_urteil(bl, ende, len(d))[0] == "ganz"))

    bl2, ende2 = a2r_bloecke(d)
    f.append(("abgeschnitten: Datei kuerzer als die Blocklaengen",
              a2r_urteil(bl2, ende2, len(d) - 4)[0] == "abgeschnitten"))

    d3 = a2r(["INFO", "STRM"], [4, 8])
    bl3, ende3 = a2r_bloecke(d3)
    f.append(("unklar: hinter STRM steht kein Block",
              a2r_urteil(bl3, ende3, len(d3))[0] == "unklar"))

    d4 = a2r(["INFO", "META"], [4, 4])
    bl4, ende4 = a2r_bloecke(d4)
    f.append(("unklar: kein STRM/RWCP vorhanden",
              a2r_urteil(bl4, ende4, len(d4))[0] == "unklar"))

    f.append(("kein A2R-Kopf -> unklar, nicht abgeschnitten",
              a2r_urteil(*a2r_bloecke(b"NOPE" + b"\x00" * 20), 24)[0]
              == "unklar"))
    f.append(("zu kurz fuer einen Kopf -> kein A2R",
              a2r_bloecke(b"A2R2") == (None, None)))

    f.append(("nur Groessen-Treffer werden zum Hashen vorgelegt",
              groessen_treffer({10, 20}, [(10, "a"), (11, "b"), (20, "c")])
              == [(10, "a"), (20, "c")]))
    f.append(("Rand: keine Groessen, keine Kandidaten",
              groessen_treffer(set(), [(1, "a")]) == []
              and groessen_treffer({1}, []) == []
              and groessen_treffer(None, None) == []))

    gut = 0
    for name, ok in f:
        print("  [%s] %s" % ("OK " if ok else "ROT", name))
        gut += bool(ok)
    print("Selbsttest %d/%d" % (gut, len(f)))
    return 0 if gut == len(f) else 1


if __name__ == "__main__":
    sys.exit(main())
