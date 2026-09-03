#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""
Nachschlagetabellen, die kleiner sind als das Enum, gegen das ihr
Aufrufer prueft (MF-842).

── Das Muster ──────────────────────────────────────────────────────────────

    typedef enum { A, B, C, ..., X_COUNT } x_t;

    static const char *namen[] = {          /* Laenge = letzter Eintrag+1 */
        [A] = "a", [B] = "b",               /* C fehlt                    */
    };

    if (t < X_COUNT) return namen[t];       /* prueft gegen COUNT!        */

Das Enum waechst, eine der abgeleiteten Tabellen wird nicht mitgezogen —
und die Schranke prueft weiter gegen `COUNT`. Der Compiler sagt nichts:
`namen[]` ohne Dimension ist so lang wie sein letzter benannter
Initialisierer, und ein Zugriff darueber hinaus ist erst zur Laufzeit
falsch.

Zwei Faelle, an denen dieses Tor entstanden ist:

  ufm_c64_scheme_detect.c   names[] mit 18 Eintraegen, Enum mit 25.
                            `ufm_c64_prot_type_name()` wird aus
                            `ProtectionAnalysisWidget.cpp` gerufen, und
                            die GUI hat fuer genau die sieben fehlenden
                            Werte bereits ein Spalten-Mapping.

  uft_pc_protection.c       vendor_names[] mit 22, Enum mit 35. Die
                            SCHWESTERTABELLE protection_names[] wurde
                            beim Erweitern mitgezogen, diese nicht.

── Wie gemessen wird ───────────────────────────────────────────────────────

Fuer jedes Enum mit einem `..._COUNT` als letztem Wert: die Zahl seiner
Werte. Fuer jede Tabelle mit designierten Initialisierern aus DIESEM
Enum: die Zahl ihrer Initialisierer. Gemeldet wird, wenn eine Tabelle
ohne feste Dimension weniger Eintraege hat als das Enum Werte — denn nur
dann ist ihre Laenge kuerzer als die Schranke.

Eine Tabelle mit AUSDRUECKLICHER Dimension (`[X_COUNT]`) faellt nicht auf:
dort erzwingt der Compiler die Laenge, fehlende Eintraege sind NULL, und
das ist die richtige Bauform. Das ist zugleich der empfohlene Fix — er
traegt auch beim naechsten Wachstum des Enums.

── Was dieses Tor NICHT kann ───────────────────────────────────────────────

* Es prueft nicht, ob der Aufrufer tatsaechlich gegen `COUNT` schrankt.
  Eine kurze Tabelle mit korrekter eigener Schranke wird gemeldet, obwohl
  sie sicher ist — ein falscher Alarm, aber in der sicheren Richtung.
* Es findet nur DESIGNIERTE Initialisierer (`[WERT] = ...`). Eine
  Tabelle in Enum-Reihenfolge ohne Klammern faellt durch.
* Es liest keine Praeprozessor-Bedingungen.

Diese Grenzen stehen hier, weil „0 gefunden" in diesem Baum schon einmal
als Entwarnung gelesen wurde und keine war.

── Dateimenge ──────────────────────────────────────────────────────────────

Aus `git ls-files` (Grundsatz MF-636).
"""
from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

WURZEL = Path(__file__).resolve().parent.parent
GRUNDLINIE = WURZEL / "docs" / "enum_tables_baseline.txt"

ENUM = re.compile(r"typedef\s+enum\s*\{(.*?)\}\s*(\w+)\s*;", re.S)
# Tabelle: optionale Dimension, dann designierte Initialisierer.
TAB = re.compile(
    r"(?:static\s+)?(?:const\s+)?[A-Za-z_]\w*\s*\*?\s*(\w+)\s*"
    # Abschluss ohne erzwungenen Zeilenumbruch — die erste Fassung
    # verlangte `\n\s*\};` und fand deshalb einzeilige Tabellen nicht.
    # Vom Selbsttest gefangen.
    r"\[([^\]]*)\]\s*=\s*\{(.*?)\}\s*;", re.S)


def dateien(*muster: str) -> list[Path]:
    aus_alle: list[Path] = []
    for m in muster:
        try:
            r = subprocess.run(
                ["git", "ls-files", "--cached", "--others",
                 "--exclude-standard", m],
                cwd=WURZEL, capture_output=True, text=True, timeout=120)
            if r.returncode != 0:
                raise RuntimeError(r.stderr)
            aus_alle += [WURZEL / z for z in r.stdout.split() if z.strip()]
        except Exception as e:                               # noqa: BLE001
            print("  WARNUNG: git nicht befragbar (%s) — Tor laesst durch" % e,
                  file=sys.stderr)
            return []
    return aus_alle


def enums(text: str) -> dict[str, tuple[str, int]]:
    """COUNT-Name -> (Enum-Praefix, Anzahl Werte vor COUNT)."""
    out = {}
    for body, _name in ENUM.findall(text):
        # Ueber KOMMAS trennen, nicht ueber Zeilenanfaenge. Die erste
        # Fassung nahm `^\s*(\w+)` mit re.M und fand bei mehreren Werten
        # in einer Zeile nur den ersten — in echten Headern kommt das
        # vor, und der Selbsttest hat es gefangen.
        roh = re.sub(r"/\*.*?\*/", " ", body, flags=re.S)
        roh = re.sub(r"//.*", " ", roh)
        werte = []
        for teil in roh.split(","):
            m = re.match(r"\s*([A-Za-z_]\w*)", teil)
            if m:
                werte.append(m.group(1))
        if not werte:
            continue
        letzte = werte[-1]
        if not letzte.upper().endswith("_COUNT"):
            continue
        out[letzte] = (letzte, len(werte) - 1)
    return out


def pruefe_datei(quelle: Path, alle_enums: dict) -> list[str]:
    try:
        t = quelle.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return []
    t = re.sub(r"//.*", "", t)
    befunde = []
    lokal = dict(alle_enums)
    lokal.update(enums(t))

    for name, dim, body in TAB.findall(t):
        init = re.findall(r"\[\s*([A-Za-z_]\w*)\s*\]\s*=", body)
        if len(init) < 2:
            continue
        # Zu welchem COUNT gehoert diese Tabelle? Ueber den gemeinsamen
        # Praefix der Initialisierer — abgeleitet, nicht aufgezaehlt.
        kandidat = None
        for cnt in lokal:
            stamm = cnt[:-len("_COUNT")]
            if sum(1 for i in init if i.startswith(stamm)) >= max(2,
                                                                  len(init) // 2):
                kandidat = cnt
                break
        if not kandidat:
            continue
        _, n_enum = lokal[kandidat]
        if dim.strip():                 # ausdrueckliche Dimension: sicher
            continue
        if len(init) < n_enum:
            try:
                wo = quelle.relative_to(WURZEL).as_posix()
            except ValueError:
                wo = quelle.name
            befunde.append(
                "%s: %s[] hat %d Eintraege, %s = %d"
                % (wo, name, len(init), kandidat, n_enum))
    return befunde


def messen():
    hdr = dateien("include/*.h")
    src = dateien("src/*.c")
    if not src:
        return None
    alle = {}
    for h in hdr:
        try:
            alle.update(enums(h.read_text(encoding="utf-8", errors="replace")))
        except OSError:
            pass
    befunde = []
    for q in src:
        befunde += pruefe_datei(q, alle)
    return sorted(befunde)


def selbsttest() -> bool:
    import tempfile
    ok = 0
    kopf = ("typedef enum { P_A, P_B, P_C, P_D, P_COUNT } p_t;\n")
    kurz = ('static const char *n[] = { [P_A]="a", [P_B]="b" };\n')
    lang = ('static const char *n[] = { [P_A]="a", [P_B]="b",'
            ' [P_C]="c", [P_D]="d" };\n')
    fest = ('static const char *n[P_COUNT] = { [P_A]="a", [P_B]="b" };\n')
    with tempfile.TemporaryDirectory() as d:
        h = Path(d) / "e.h"; h.write_text(kopf, encoding="utf-8")
        e = enums(kopf)
        # 1: die zu kurze Tabelle faellt auf
        c1 = Path(d) / "a.c"; c1.write_text(kurz, encoding="utf-8")
        if pruefe_datei(c1, e):
            ok += 1
        else:
            print("  SELBSTTEST 1 ROT: kurze Tabelle nicht erkannt")
        # 2: GEGENBEWEIS — die vollstaendige Tabelle darf NICHT auffallen
        c2 = Path(d) / "b.c"; c2.write_text(lang, encoding="utf-8")
        if not pruefe_datei(c2, e):
            ok += 1
        else:
            print("  SELBSTTEST 2 ROT: vollstaendige Tabelle gemeldet")
        # 3: GEGENBEWEIS — ausdrueckliche Dimension ist die richtige
        #    Bauform und darf NICHT auffallen, auch wenn Eintraege fehlen
        c3 = Path(d) / "c.c"; c3.write_text(fest, encoding="utf-8")
        if not pruefe_datei(c3, e):
            ok += 1
        else:
            print("  SELBSTTEST 3 ROT: feste Dimension gemeldet")
    print("  Selbsttest %d/3" % ok)
    return ok == 3


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
        GRUNDLINIE = WURZEL / "docs" / "enum_tables_baseline.txt"
    g = grenze()
    if g is None:
        return []
    b = messen()
    if b is None or len(b) <= g:
        return []
    return ["%d zu kurze Tabellen > Grundlinie %d: %s"
            % (len(b), g, "; ".join(b[:3]))]


def main() -> int:
    print("audit_enum_tables (MF-842)")
    if not selbsttest():
        print("  ABBRUCH: Selbsttest rot — kein Nenner ohne Abnahme")
        return 2
    b = messen()
    if b is None:
        return 0
    print("  Tabellen kuerzer als ihr Enum: %d" % len(b))
    for x in b:
        print("    %s" % x)
    g = grenze()
    if g is None:
        print("  keine Grundlinie — nur Bericht")
        return 0
    print("  Grundlinie                   : %d" % g)
    if len(b) > g:
        print("  FEHLER: die Zahl ist gestiegen.")
        return 1
    if len(b) < g:
        print("  Hinweis: Grundlinie auf %d senken." % len(b))
    print("  OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
