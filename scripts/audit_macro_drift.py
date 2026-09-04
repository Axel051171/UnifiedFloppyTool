#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""
Gleicher Makroname, verschiedener Wert (MF-852).

── Woher das kommt ─────────────────────────────────────────────────────────

P3-92: der Baum trug ZWEI STX-Flagtabellen, die einander widersprachen.
`STX_TF_TRACK_IMAGE` war in `src/formats/atari/uft_stx_parser.c` **0x01**
und in `src/formats/stx/uft_stx_air.c` **0x40**. Eine von beiden musste
falsch sein, und es dauerte drei Sitzungen und eine fremde Quelle
(Hatari), bis feststand welche.

BERICHTIGT beim Bau (MF-852): **dieses Tor haette P3-92 NICHT gefangen.**
Der Gegenbeweis wurde gefahren — die alte Konstante 0x01 wieder
eingesetzt, das Tor blieb bei 32. Grund: die beiden Tabellen nannten
dasselbe Bit VERSCHIEDEN (`STX_TF_IMAGE` gegen `STX_TF_TRACK_IMAGE`).
Namensgleichheit gab es nie.

Das ist die im Baum belegte Lage „eine Messung kann die richtige Frage
stellen und die falsche Fehlerklasse treffen". P3-92 ist „gleiche
BEDEUTUNG, anderer Name, anderer Wert" — syntaktisch nicht erkennbar,
dafuer braucht es eine Quelle. Dieses Tor sieht „gleicher NAME, anderer
Wert", eine benachbarte, aber eigene Klasse.

Es bleibt trotzdem, weil die Messung dabei etwas anderes gefunden hat:
32 Faelle, darunter `ATX_SIGNATURE` byte-verdreht (ein Leser, dessen
Signaturpruefung nie zutrifft), `CMD_BAM_TRACK`/`CMD_BAM_SECTOR`
zwischen Header und Umsetzung vertauscht, und
`SCP_DISK_APPLE_400K` 0x30 gegen 0x24. Der Fund rechtfertigt das Tor —
die urspruengliche Begruendung tat es nicht.

── Was gemessen wird ───────────────────────────────────────────────────────

Jedes `#define NAME <ganze Zahl>` in `src/` und `include/`. Gruppiert
nach NAME; gemeldet wird jeder Name, dem mehr als ein Wert zugewiesen
wird.

── GESCHAERFT (MF-853): nur WIRKSAME Drift ─────────────────────────────────

Die erste Fassung meldete jeden Namen mit mehr als einem Wert: **31**.
Nachgemessen wurde dann, ob der Name in seiner Datei ueberhaupt BENUTZT
wird — und das Bild kippte:

    Drift gesamt        31
    davon wirksam       11   (in >= 2 Dateien benutzt, verschiedene Werte)
    davon toter #define 20   (in hoechstens einer benutzt)

Zwanzig von einunddreissig waren Definitionen, die ihre eigene Datei nie
liest. Sie koennen niemanden in die Irre fuehren, weil niemand sie
befragt — das ist eine eigene, geringere Klasse (Bestand, nicht
Wirkung).

Das kostete eine eigene Ueberzeichnung: MF-852 meldete
`CMD_BAM_TRACK`/`CMD_BAM_SECTOR` als „zwischen Header und Umsetzung
vertauscht". Gemessen sind es **tote Definitionen in einem unabhaengigen
Modul** (`uft_cmd_fd.c` definiert vier und benutzt keine; die Treffer
kamen aus `uft_cmd.c`, das seinen eigenen Satz hat). Kein Fehler, nur
Ballast — und ein Beleg dafuer, dass eine Zahl ohne Wirkungspruefung
groesser aussieht, als sie ist.

Gezaehlt wird seither nur die wirksame Menge. Die toten werden
BERICHTET, nicht gezaehlt: sie gehoeren weg, aber nicht in dieselbe
Zahl.

── Grenze des Verfahrens, ausdruecklich ────────────────────────────────────

**Auch ein wirksamer Treffer ist nicht automatisch ein Fehler.** Zwei
unverwandte Module duerfen ein lokales `MAX_SECTORS` oder `INITIAL_CAP`
mit verschiedenen Werten fuehren; der Name benennt dann eine Eigenschaft
des MODULS. Die Grundlinie ist deshalb WEICH, und jeder Abbau ist eine
Einzelentscheidung wie bei P3-76.

Woran man die echten erkennt: ein **formatgebundener Praefix**
(`ATX_`, `SCP_`, `G64_`, `CMD_`, `PRO_`, `GEOS_`, `ADF_`) benennt eine
Eigenschaft des FORMATS, und ein Format hat nur eine. Ein generischer
Name (`MAX_SECTORS`, `INITIAL_CAP`) benennt eine Eigenschaft des MODULS.

Was das Tor NICHT sieht:
  * `enum`-Werte (nur `#define`)
  * berechnete Ausdruecke (`#define A (B+1)`) — nur ganze Zahlen
  * Praeprozessor-Bedingungen: steht derselbe Name in zwei `#if`-Zweigen
    DERSELBEN Datei, ist das gewollt und faellt hier nicht auf, weil je
    Datei nur der letzte Wert gezaehlt wird
  * Definitionen in `.cpp`-Dateien werden mitgezaehlt, aber Qt-Header
    ausserhalb des Baums nicht

Alle drei erzeugen zu WENIG, nicht zu viel — das Tor ist eine untere
Schranke.
"""
from __future__ import annotations

import collections
import re
import subprocess
import sys
from pathlib import Path

WURZEL = Path(__file__).resolve().parent.parent
GRUNDLINIE = WURZEL / "docs" / "macro_drift_baseline.txt"

DEF = re.compile(
    r"^\s*#\s*define\s+([A-Z_][A-Z0-9_]{3,})\s+"
    r"((?:0[xX][0-9a-fA-F]+|\d+)[uUlL]*)\s*(?:/\*|//|$)")


def dateien(wurzel: Path):
    """Dateimenge aus git, nie aus einer gepflegten Liste (MF-636)."""
    try:
        r = subprocess.run(
            ["git", "ls-files", "--cached", "--others", "--exclude-standard"],
            cwd=wurzel, capture_output=True, text=True, timeout=120)
        if r.returncode != 0:
            print("  WARNUNG: git nicht befragbar — Tor laesst durch",
                  file=sys.stderr)
            return None
    except Exception as e:                                   # noqa: BLE001
        print("  WARNUNG: git nicht befragbar (%s) — Tor laesst durch" % e,
              file=sys.stderr)
        return None
    return [z for z in r.stdout.split("\n")
            if z.endswith((".c", ".h", ".cpp"))
            and (z.startswith("src/") or z.startswith("include/"))]


def _benutzt(name: str, txt: str) -> bool:
    """Kommt @p name im Text vor — ausserhalb seiner eigenen Definition?

    MF-853: die entscheidende Frage. Ein `#define`, das seine eigene
    Datei nie liest, kann keinen Leser in die Irre fuehren."""
    for z in txt.split("\n"):
        if name in z and not DEF.match(z):
            return True
    return False


def pruefe_texte(paare, mit_toten=False):
    """paare: [(pfad, text)] -> {name: {pfad: wert}}.

    Gemeldet wird nur WIRKSAME Drift: der Name muss in mindestens zwei
    Dateien benutzt werden UND dort verschiedene Werte tragen.
    Mit @p mit_toten kommt zusaetzlich die Liste der toten Definitionen
    zurueck (zweiter Rueckgabewert)."""
    werte = collections.defaultdict(dict)
    texte = dict(paare)
    for pfad, txt in paare:
        for z in txt.split("\n"):
            m = DEF.match(z)
            if not m:
                continue
            try:
                v = int(m.group(2).rstrip("uUlL"), 0)
            except ValueError:
                continue
            werte[m.group(1)][pfad] = v

    wirksam, tot = {}, {}
    for n, d in werte.items():
        if len(set(d.values())) <= 1:
            continue
        aktiv = {f: v for f, v in d.items() if _benutzt(n, texte[f])}
        if len(set(aktiv.values())) > 1:
            wirksam[n] = aktiv
        else:
            tot[n] = d
    return (wirksam, tot) if mit_toten else wirksam


def selbsttest() -> bool:
    """Vor dem Nenner, mit Gegenproben."""
    ok = 0

    # MF-853: die Werte muessen BENUTZT werden, sonst ist es toter
    # Ballast. Deshalb traegt jede Probe eine Verwendungszeile.
    A = "#define FOO_BAR 0x01\nint f(void){return FOO_BAR;}\n"
    B = "#define FOO_BAR 0x40\nint g(void){return FOO_BAR;}\n"
    if list(pruefe_texte([("a.c", A), ("b.c", B)])) == ["FOO_BAR"]:
        ok += 1
    else:
        print("  SELBSTTEST 1 ROT: wirksame Drift nicht erkannt")

    # GEGENPROBE A: derselbe Wert in zwei Dateien ist KEIN Fund.
    if not pruefe_texte([("a.c", A), ("b.c", A)]):
        ok += 1
    else:
        print("  SELBSTTEST 2 ROT: gleicher Wert gemeldet")

    # GEGENPROBE B: 0x40 und 64 sind DERSELBE Wert — verglichen wird die
    # Zahl, nicht ihre Schreibweise. Ohne das waere jede Hex/Dezimal-
    # Mischung ein Fehlalarm.
    if not pruefe_texte([("a.c", "#define FOO_BAR 0x40\nint f(void){return FOO_BAR;}\n"),
                         ("b.c", "#define FOO_BAR 64\nint g(void){return FOO_BAR;}\n")]):
        ok += 1
    else:
        print("  SELBSTTEST 3 ROT: 0x40 != 64 gemeldet")

    # GEGENPROBE D (MF-853): verschiedene Werte, aber EINE der beiden
    # Dateien benutzt ihren Wert nicht — toter Ballast, kein Fund. Ohne
    # diese Probe waere die Schaerfung selbst unbewacht.
    if not pruefe_texte([("a.c", "#define FOO_BAR 0x01\nint f(void){return FOO_BAR;}\n"),
                         ("b.c", "#define FOO_BAR 0x40\n")]):
        ok += 1
    else:
        print("  SELBSTTEST 5 ROT: toter #define als Drift gemeldet")

    # GEGENPROBE C: ein Makro mit Ausdruck statt Zahl faellt nicht auf —
    # das ist die im Kopf genannte Grenze, und sie wird hier BELEGT,
    # nicht behauptet.
    if not pruefe_texte([("a.c", "#define FOO_BAR (BAZ + 1)\n"),
                         ("b.c", "#define FOO_BAR (BAZ + 2)\n")]):
        ok += 1
    else:
        print("  SELBSTTEST 4 ROT: Ausdruck wurde doch gelesen")

    print("  Selbsttest %d/5" % ok)
    return ok == 5


def sammle(wurzel: Path):
    ds = dateien(wurzel)
    if ds is None:
        return None
    paare = []
    for z in ds:
        p = wurzel / z
        try:
            paare.append((z, p.read_text(encoding="utf-8", errors="replace")))
        except OSError:
            continue
    return pruefe_texte(paare, mit_toten=True)


def grenze():
    if not GRUNDLINIE.exists():
        return None
    for z in GRUNDLINIE.read_text(encoding="utf-8").splitlines():
        z = z.split("#")[0].strip()
        if z.isdigit():
            return int(z)
    return None


def check(repo=None):
    global WURZEL, GRUNDLINIE
    if repo:
        WURZEL = Path(repo)
        GRUNDLINIE = WURZEL / "docs" / "macro_drift_baseline.txt"
    g = grenze()
    if g is None:
        return []
    r = sammle(WURZEL)
    if r is None:
        return []
    d, _tot = r
    if len(d) <= g:
        return []
    return ["%d wirksame Makro-Drifts > Grundlinie %d: %s"
            % (len(d), g, ", ".join(sorted(d)[:5]))]


def main() -> int:
    print("audit_macro_drift (MF-852)")
    if not selbsttest():
        print("  ABBRUCH: Selbsttest rot — kein Nenner ohne Abnahme")
        return 2
    r = sammle(WURZEL)
    if r is None:
        return 0
    d, tot = r
    print("  WIRKSAME Drift (>=2 Nutzer)       : %d" % len(d))
    for n in sorted(d):
        print("    %-32s %s" % (n, " | ".join(
            "%s=%s" % (f.rsplit("/", 1)[-1], hex(v)) for f, v in d[n].items())))
    print("  tote #defines (hoechstens 1 Nutzer): %d  — berichtet, nicht "
          "gezaehlt" % len(tot))
    g = grenze()
    if g is None:
        print("  keine Grundlinie — nur Bericht")
        return 0
    print("  Grundlinie                        : %d" % g)
    if len(d) > g:
        print("  FEHLER: die Zahl ist gestiegen.")
        return 1
    if len(d) < g:
        print("  Hinweis: Grundlinie auf %d senken." % len(d))
    print("  OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
