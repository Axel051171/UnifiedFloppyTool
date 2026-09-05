#!/usr/bin/env python3
"""Ein Include-Guard, zwei Bedeutungen — die Reihenfolge entscheidet (MF-881).

── Warum es dieses Tor gibt ────────────────────────────────────────────

Setzen zwei Header denselben `*_DEFINED`-Guard mit VERSCHIEDENEM Inhalt,
gewinnt der zuerst eingebundene. Der zweite wird stillschweigend
uebersprungen — kein Fehler, keine Warnung, nichts. Welche Fassung gilt,
haengt an der Include-Reihenfolge der Uebersetzungseinheit.

Das ist die einzige Fehlerklasse in diesem Baum, die ein FALSCHES Ergebnis
erzeugt statt eines fehlenden. `docs/orphan_baseline.txt` misst Module,
`tools/uft-innendienst/` misst Symbole; diese Klasse sieht keines von
beiden.

Gemessen am 2026-09-05 (der Anlass):

  `uft_format_t` existiert dreifach — `uft_types.h:122`,
  `uft_format_parsers.h:42`, `detect/uft_format_detect.h:39` — und die
  drei teilen sich **36 Namen, 34 davon mit verschiedenen Werten**:

      UFT_FORMAT_D64   detect=1   parsers=4   types=4
      UFT_FORMAT_ATR   detect=14              types=31
      UFT_FORMAT_ADF   detect=6   parsers=3   types=3

  Am Praeprozessor bewiesen, nicht abgeleitet — `gcc -E` ueber acht
  Uebersetzungseinheiten desselben Baus:

      UFT_FORMAT_D64 = 1  in src/core/uft_detect_format_impl.c
      UFT_FORMAT_D64 = 4  in sechs anderen

  Und es ist folgenreich: `uft_detect_format_impl.c:47` schreibt
  `result->format = (uft_format_t)plugin->format`. Der Wert kommt aus der
  Plugin-Einheit (4), gelesen wird er in `src/analysis/uft_format_suggest.c`
  gegen dessen eigene Konstanten (1). Beide Dialoge, die davon haengen —
  `src/gui/uft_recovery_dialog.cpp:509` und
  `src/gui/uft_smart_export_dialog.cpp:107` — bekommen die Formatfamilie
  falsch.

  Der Kommentar an genau dieser Stelle sagte, der Guard sorge dafuer, dass
  die beiden Aufzaehlungen ihre Werte TEILEN. Das ist die Umkehrung: er
  sorgt dafuer, dass nur EINE existiert.

── Was das Tor prueft ──────────────────────────────────────────────────

Ein Guard ist erst dann ein Befund, wenn DREI Bedingungen zusammenkommen:

  (a) zwei Header setzen ihn mit verschiedenem Rumpf
  (b) die Fassungen sind nicht durch die Reihenfolge festgelegt — bindet
      ein Header den anderen VOR seinem eigenen Guard ein, ist seine
      Fassung toter Text und die Wahl deterministisch
  (c) mindestens eine Uebersetzungseinheit sieht beide Header

Alle drei einzeln zu pruefen ist der Punkt. Eine Messung, die nur (a)
nimmt, meldet 22 Faelle; mit (b) und (c) bleiben 11. Ohne diese Trennung
waere das Tor ein Alarmgeber, den man abschaltet.

Zusaetzlich wird gemeldet, wenn zwei Fassungen denselben NAMEN an
verschiedene ZAHLEN vergeben — das ist die Stufe, auf der aus einer
Typfrage ein Rechenfehler wird.

Dateimenge aus `git ls-files` (MF-636), nie aus einer gepflegten Liste.
Die Grundlinie darf FALLEN und steigt nur mit Begruendung im Commit.
"""
from __future__ import annotations

import os
import re
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from repo_scope import repo_files  # noqa: E402

GRUNDLINIE = 11   # Guards, die wirklich wuerfeln, Stand MF-881

# Die beiden BEKANNTEN Zahlkonflikte. Sie sind nicht "in Ordnung" — sie
# sind aufgenommen, benannt und in docs/OPEN_ITEMS.md als P3-155 gefuehrt.
#
# Sie hier zu dulden statt das Tor rot zu lassen, folgt dem Muster von
# `docs/fdc_gaps_baseline.txt` (MF-838): ein Tor, das am ersten Tag rot
# ist, wird abgeschaltet statt gelesen. Die Grundlinie darf FALLEN; ein
# DRITTER Konflikt faellt sofort auf.
#
# Aufloesen heisst hier: die drei `uft_format_t` auf eine Definition
# zusammenfuehren. Das ist kein Einzeiler — 20 Namen gibt es nur in
# `detect/uft_format_detect.h`, 10 nur in `uft_format_parsers.h`, und
# `src/analysis/uft_format_suggest.c` benutzt zwei davon
# (`UFT_FORMAT_KFX`, `UFT_FORMAT_MFI`). Die fehlenden muessen an
# `uft_types.h` ANGEHAENGT werden, damit bestehende Werte sich nicht
# verschieben.
KONFLIKT_GRUNDLINIE = {
    "UFT_FORMAT_ENUM_DEFINED",   # 34 Namen mit verschiedenen Zahlen
    "UFT_FORMAT_ID_T_DEFINED",   # 48
}

DEF = re.compile(r"^\s*#\s*define\s+([A-Z_][A-Z0-9_]*_DEFINED)\s*$", re.M)
INC = re.compile(r'^\s*#\s*include\s+[<"]([^>"]+)[>"]', re.M)
MITGLIED = re.compile(r"^([A-Z_][A-Za-z0-9_]*)")


def _lies(repo):
    dateien = repo_files(repo)
    if dateien is None:
        return None, None, None
    text, header, quellen = {}, [], []
    for p in sorted(dateien):
        try:
            rel = p.relative_to(repo).as_posix()
        except ValueError:
            continue
        if not rel.endswith((".c", ".cpp", ".h", ".hpp")):
            continue
        try:
            text[rel] = p.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        (header if rel.endswith((".h", ".hpp")) else quellen).append(rel)
    return text, header, quellen


def _rumpf(t, start):
    """Text vom #define bis zum passenden #endif."""
    zeilen = t[start:].splitlines()
    tiefe, aus = 1, []
    for z in zeilen[1:]:
        s = z.strip()
        if re.match(r"#\s*(if|ifdef|ifndef)\b", s):
            tiefe += 1
        elif re.match(r"#\s*endif\b", s):
            tiefe -= 1
            if tiefe == 0:
                break
        aus.append(z)
    return "\n".join(aus)


def _norm(s):
    s = re.sub(r"/\*.*?\*/", " ", s, flags=re.S)
    s = re.sub(r"//[^\n]*", " ", s)
    return re.sub(r"\s+", "", s)


def _werte(rumpf):
    """name -> Zahl fuer eine Aufzaehlung; leer, wenn es keine ist."""
    if "enum" not in rumpf:
        return {}
    k = rumpf.split("{", 1)
    if len(k) < 2:
        return {}
    koerper = k[1].rsplit("}", 1)[0]
    koerper = re.sub(r"/\*.*?\*/", " ", koerper, flags=re.S)
    koerper = re.sub(r"//[^\n]*", " ", koerper)
    m, lauf = {}, 0
    for teil in koerper.split(","):
        teil = teil.strip()
        if not teil:
            continue
        if "=" in teil:
            name, aus = [x.strip() for x in teil.split("=", 1)]
            try:
                lauf = int(aus, 0)
            except ValueError:
                pass
        else:
            name = teil
        g = MITGLIED.match(name)
        if not g:
            continue
        m[g.group(1)] = lauf
        lauf += 1
    return m


def messe(repo: Path):
    text, header, quellen = _lies(repo)
    if text is None:
        return None, ["git nicht befragbar — dieses Tor kann nichts sagen "
                      "(MF-636: lieber laut unbrauchbar als still blind)"]

    nach_suffix = defaultdict(list)
    for h in header:
        teile = h.split("/")
        for i in range(len(teile)):
            nach_suffix["/".join(teile[i:])].append(h)

    def aufloesen(ziel, von):
        k = nach_suffix.get(ziel)
        if k:
            return k
        kand = os.path.normpath(
            os.path.join(os.path.dirname(von), ziel)).replace("\\", "/")
        return [kand] if kand in text else []

    cache = {}

    def abschluss(p, tiefe=0):
        if p in cache:
            return cache[p]
        if tiefe > 40 or p not in text:
            return set()
        cache[p] = set()
        aus = set()
        for m in INC.finditer(text[p]):
            for z in aufloesen(m.group(1), p):
                if z not in aus:
                    aus.add(z)
                    aus |= abschluss(z, tiefe + 1)
        cache[p] = aus
        return aus

    wo = defaultdict(list)
    for p in header:
        t = text[p]
        for m in DEF.finditer(t):
            r = _rumpf(t, m.start())
            wo[m.group(1)].append(
                (p, t[:m.start()].count("\n") + 1, _norm(r), r))

    # (a) verschiedene Rumpfe
    kand = {g: st for g, st in wo.items()
            if len(st) > 1 and len({x[2] for x in st}) > 1}

    sichten = {q: abschluss(q) | {q} for q in quellen}

    befunde = []
    for g, st in sorted(kand.items()):
        pfade = {x[0] for x in st}

        # (b) durch Reihenfolge festgelegt?
        def geschuetzt(p, zeile):
            t = text[p]
            grenze = 0
            for i, z in enumerate(t.splitlines(True), 1):
                if i >= zeile:
                    break
                grenze += len(z)
            for m in INC.finditer(t[:grenze]):
                for z in aufloesen(m.group(1), p):
                    if z in (pfade - {p}):
                        return True
                    if z in text and (pfade - {p}) & abschluss(z):
                        return True
            return False

        frei = [(p, z) for p, z, _, _ in st if not geschuetzt(p, z)]
        if len(frei) < 2:
            continue

        # (c) gemeinsam erreichbar?
        tus = [q for q, s in sichten.items() if len(pfade & s) > 1]
        if not tus:
            continue

        # Nutzlast: gleicher Name, andere Zahl?
        karten = [(p, _werte(r)) for p, _, _, r in st]
        konflikte = set()
        for i in range(len(karten)):
            for j in range(i + 1, len(karten)):
                ka, kb = karten[i][1], karten[j][1]
                for n in set(ka) & set(kb):
                    if ka[n] != kb[n]:
                        konflikte.add(n)
        befunde.append((g, [x[0] for x in st], tus, sorted(konflikte)))
    return befunde, []


def check(repo) -> list:
    befunde, fehler = messe(Path(repo))
    if befunde is None:
        return fehler

    for g, pfade, tus, konflikte in befunde:
        if not konflikte or g in KONFLIKT_GRUNDLINIE:
            continue
        fehler.append(
            "%s: %d Fassungen mit verschiedenem Inhalt, %d Uebersetzungs"
            "einheiten sehen zwei davon, und %d NAMEN tragen dabei "
            "VERSCHIEDENE ZAHLEN (z.B. %s). Welche Fassung gilt, entscheidet "
            "die Include-Reihenfolge — der Compiler sagt dazu nichts. "
            "Zusammenfuehren auf eine Definition, oder in JEDEM zweiten "
            "Header den kanonischen VOR dem eigenen Guard einbinden.\n"
            "      %s"
            % (g, len(pfade), len(tus), len(konflikte),
               ", ".join(konflikte[:3]), "\n      ".join(pfade)))

    # Ein Guard, der aus der Grundlinie faellt, gehoert dort gestrichen —
    # sonst deckt sie stillschweigend etwas, das es nicht mehr gibt.
    noch_da = {g for g, _, _, k in befunde if k}
    verschwunden = KONFLIKT_GRUNDLINIE - noch_da
    if verschwunden:
        fehler.append(
            "Diese Guards stehen in KONFLIKT_GRUNDLINIE, haben aber keinen "
            "Zahlkonflikt mehr: %s. Bitte dort streichen — eine Grundlinie, "
            "die Geloestes weiter deckt, verliert ihre Aussage."
            % ", ".join(sorted(verschwunden)))

    if len(befunde) > GRUNDLINIE:
        fehler.append(
            "%d wuerfelnde Guards, Grundlinie %d. Ein weiterer ist eine "
            "Entscheidung, kein Versehen — zusammenfuehren, oder die "
            "Grundlinie mit Begruendung anheben."
            % (len(befunde), GRUNDLINIE))
    return fehler


def _selbsttest(repo: Path) -> int:
    """Vor dem Nenner (MF-693)."""
    import tempfile

    faelle = [
        # (Dateien, soll das Tor meckern?)
        ({"a.h": "#ifndef G_DEFINED\n#define G_DEFINED\n"
                 "typedef enum { X, Y } t;\n#endif\n",
          "b.h": "#ifndef G_DEFINED\n#define G_DEFINED\n"
                 "typedef enum { Y, X } t;\n#endif\n",
          "u.c": '#include "a.h"\n#include "b.h"\n'}, True),
        # gleiche Rumpfe -> kein Befund
        ({"a.h": "#ifndef G_DEFINED\n#define G_DEFINED\n"
                 "typedef enum { X, Y } t;\n#endif\n",
          "b.h": "#ifndef G_DEFINED\n#define G_DEFINED\n"
                 "typedef enum { X, Y } t;\n#endif\n",
          "u.c": '#include "a.h"\n#include "b.h"\n'}, False),
        # verschieden, aber keine TU sieht beide -> kein Befund
        ({"a.h": "#ifndef G_DEFINED\n#define G_DEFINED\n"
                 "typedef enum { X, Y } t;\n#endif\n",
          "b.h": "#ifndef G_DEFINED\n#define G_DEFINED\n"
                 "typedef enum { Y, X } t;\n#endif\n",
          "u.c": '#include "a.h"\n'}, False),
        # verschieden und co-erreichbar, aber b bindet a vorher ein
        ({"a.h": "#ifndef G_DEFINED\n#define G_DEFINED\n"
                 "typedef enum { X, Y } t;\n#endif\n",
          "b.h": '#include "a.h"\n#ifndef G_DEFINED\n#define G_DEFINED\n'
                 "typedef enum { Y, X } t;\n#endif\n",
          "u.c": '#include "b.h"\n#include "a.h"\n'}, False),
        # verschieden, co-erreichbar, aber disjunkte Namen -> kein
        # ZAHLEN-Konflikt, also keine Meldung
        ({"a.h": "#ifndef G_DEFINED\n#define G_DEFINED\n"
                 "typedef enum { AA, AB } t;\n#endif\n",
          "b.h": "#ifndef G_DEFINED\n#define G_DEFINED\n"
                 "typedef enum { BA, BB, BC } t;\n#endif\n",
          "u.c": '#include "a.h"\n#include "b.h"\n'}, False),
    ]

    gut = 0
    for i, (dateien, soll) in enumerate(faelle, 1):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d)
            subprocess.run(["git", "init", "-q"], cwd=d, capture_output=True)
            for name, inhalt in dateien.items():
                (p / name).write_text(inhalt, encoding="utf-8")
            errs = check(p)
            hat = any("VERSCHIEDENE ZAHLEN" in e for e in errs)
            if hat == soll:
                gut += 1
            else:
                print("  Selbsttest %d: erwartet meckern=%s, bekommen %s"
                      % (i, soll, hat))
    print("Selbsttest: %d/%d" % (gut, len(faelle)))
    return 0 if gut == len(faelle) else 1


def main() -> int:
    repo = Path(__file__).resolve().parent.parent
    if "--selftest" in sys.argv:
        return _selbsttest(repo)

    if "-v" in sys.argv or "--list" in sys.argv:
        befunde, _ = messe(repo)
        if befunde is not None:
            print("Wuerfelnde Guards: %d (Grundlinie %d)"
                  % (len(befunde), GRUNDLINIE))
            for g, pfade, tus, konflikte in befunde:
                print("  %-32s %d Fassungen, %2d TUs, %d Zahlkonflikte"
                      % (g, len(pfade), len(tus), len(konflikte)))
                for p in pfade:
                    print("        %s" % p)
            print()

    errs = check(repo)
    if not errs:
        print("OK: kein Guard vergibt denselben Namen an verschiedene Zahlen.")
        return 0
    print("FAIL: %d Befund(e)" % len(errs))
    for e in errs:
        print("  %s" % e)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
