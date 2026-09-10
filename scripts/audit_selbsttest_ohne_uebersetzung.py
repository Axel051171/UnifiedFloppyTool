#!/usr/bin/env python3
"""Ein Selbsttest hinter einem Waechter, den niemand definiert, ist kein
Test — er ist ein Versprechen (MF-1000).

    python scripts/audit_selbsttest_ohne_uebersetzung.py
    python scripts/audit_selbsttest_ohne_uebersetzung.py --selbsttest
    python scripts/audit_selbsttest_ohne_uebersetzung.py --grundlinie-schreiben

── Warum es dieses Tor gibt ────────────────────────────────────────────

`#ifdef FOO_TEST` … `int main() { assert(...); }` … `#endif` sieht aus wie
Pruefung. Wird `FOO_TEST` von keinem Bausystem definiert, hat **kein
Compiler den Block je gesehen**: er kann nicht rot werden, er kann nicht
einmal syntaktisch falsch auffallen, und er driftet mit jeder Aenderung am
Code darueber weiter weg. In einem Baum, dessen Motto „Keine erfundenen
Daten" lautet, ist das eine Zusage ohne Deckung.

Die Klasse ist in diesem Baum schon einmal behandelt worden — und genau
daran haengt der Grund fuer dieses Tor:

  P3-89 / MF-845  fand **drei** Dateien mit `#ifdef UFT_UNIT_TESTS`
  MF-851          hob die 19 Zusagen nach `tests/` und entfernte die
                  Bloecke, „damit sie nicht driften"

Das war richtig und ist bis heute in Ordnung: die drei Dateien tragen an
der Stelle nur noch einen Kommentar, der den Vorgang erklaert.

**Aber gemessen wurde damals EIN Makroname.** Ueber alle Waechternamen
gerechnet sind es beim Bau dieses Tores **47 Dateien unter 44
verschiedenen Namen**, mit 3884 Zeilen und 606 `assert()`. `UFT_UNIT_TESTS`
war nicht die Klasse, sondern ein Mitglied davon.

Das ist die Signatur dieses Baums: **Aufzaehlung statt Messung.** Sie hat
hier dieselbe Form wie in MF-930, wo der Kopf eines TORES acht Verdaechtige
aufzaehlte und elf gemessen wurden. Ein Fix, der die bekannten Faelle
behebt statt die Klasse zu bewachen, laesst sie zurueckkommen — und der
Rueckweg ist unsichtbar, weil nichts rot wird.

── Was gemessen wird ───────────────────────────────────────────────────

Fuer jede Quelldatei aus `git ls-files` (Grundsatz MF-636 — nie eine
gepflegte Verzeichnisliste):

    ein `#ifdef M` / `#if defined(M)`-Block
    UND er enthaelt mindestens ein `assert(`
    UND `M` wird von KEINEM Bausystem definiert

Der dritte Punkt ist der wichtige, und er ist **abgeleitet**: gesucht wird
`M` in `*.pro`, jeder `CMakeLists.txt`, `*.cmake` und den CI-Ablaeufen —
als `DEFINES +=`, `add_definitions`, `target_compile_definitions`, `-DM`
oder `#define M`. Es gibt hier **keine Liste bekannter Testmakros**; wer
morgen `#ifdef WOZ4_TEST` schreibt, ist am selben Tag erfasst, ohne dass
jemand dieses Skript anfasst.

`assert(` ist das Merkmal, nicht `main(`. Ein `main()` hinter einem
Waechter kann ein Vorfuehrprogramm sein — unbenutzt, aber ehrlich. Eine
`assert`-Kette ist eine **Zusage ueber Verhalten**, und genau die zaehlt.

── Was dieses Tor ausdruecklich NICHT sagt ─────────────────────────────

Es sagt nicht, dass die 606 Zusagen falsch sind. MF-851 hat seine 19
gehoben und **alle 19 trugen**. Der Befund ist nicht „hier sind Fehler",
sondern „hier ist Ungeprueftes, das wie Geprueftes aussieht" — und der
Unterschied ist der ganze Punkt: richtig und unbewacht ist nicht dasselbe
wie richtig und bewacht.

Es misst auch keine Erreichbarkeit und keinen Inhalt. Ein Block, dessen
Waechter irgendwo definiert wird, geht durch, selbst wenn diese Definition
in einem Zweig steht, den niemand baut. Das ist bewusst konservativ: ein
Tor, das falsch meckert, wird abgeschaltet.

── Grundlinie ──────────────────────────────────────────────────────────

`docs/selbsttest_baseline.txt` haelt den Bestand beim Bau des Tores. Das
Tor faellt bei **jedem neuen** Eintrag; wer einen alten aufloest, nimmt
seine Zeile heraus. Die Zahl kann so nur sinken.
"""
import io
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

WURZEL = Path(__file__).resolve().parent.parent
GRUNDLINIE = WURZEL / "docs" / "selbsttest_baseline.txt"

QUELL_ENDUNGEN = (".c", ".cpp", ".cc", ".cxx")

# Wo ein Bausystem ein Makro definieren koennte. Bewusst breit.
BAU_MUSTER = ("*.pro", "*.pri", "CMakeLists.txt", "*.cmake", "*.yml",
              "*.yaml", "Makefile", "*.mk")


def _dateien():
    """Quelldateien aus git — inklusive der noch nicht hinzugefuegten.

    MF-636: wer in einem Skript entscheidet, WELCHE Dateien geprueft
    werden, fragt git. Eine gepflegte Verzeichnisliste veraltet still;
    das ist in diesem Baum viermal belegt.
    """
    try:
        r = subprocess.run(
            ["git", "ls-files", "--cached", "--others", "--exclude-standard"],
            cwd=WURZEL, capture_output=True, text=True, timeout=60)
        if r.returncode != 0:
            raise RuntimeError(r.stderr)
        namen = r.stdout.split("\n")
    except Exception as e:                       # noqa: BLE001
        print("  HINWEIS: git nicht befragbar (%s) — nehme den ganzen Baum."
              % e)
        namen = [str(p.relative_to(WURZEL)).replace("\\", "/")
                 for p in WURZEL.rglob("*") if p.is_file()]
    return [n for n in namen if n.endswith(QUELL_ENDUNGEN)]


def _ohne_kommentare(text):
    """C-Text, in dem Kommentare und Zeichenketten durch Leerzeichen
    ersetzt sind — an Ort und Stelle, damit Zeilennummern stimmen.

    Ohne das misst dieses Skript seine eigene Begruendung mit: der
    Kommentar in `uft_multiread_pipeline.c` enthaelt woertlich
    „`#ifdef UFT_UNIT_TESTS`" und waere sonst ein Treffer. Genau dieser
    Fehlalarm ist beim Bau des Tores aufgetreten.
    """
    ergebnis = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            while i < n and text[i] != "\n":
                ergebnis.append(" ")
                i += 1
        elif c == "/" and i + 1 < n and text[i + 1] == "*":
            while i < n and not (text[i] == "*" and i + 1 < n
                                 and text[i + 1] == "/"):
                ergebnis.append("\n" if text[i] == "\n" else " ")
                i += 1
            ergebnis.append("  ")
            i += 2
        elif c in "\"'":
            quote = c
            ergebnis.append(" ")
            i += 1
            while i < n and text[i] != quote:
                if text[i] == "\\":
                    ergebnis.append(" ")
                    i += 1
                    if i < n:
                        ergebnis.append("\n" if text[i] == "\n" else " ")
                        i += 1
                    continue
                ergebnis.append("\n" if text[i] == "\n" else " ")
                i += 1
            ergebnis.append(" ")
            i += 1
        else:
            ergebnis.append(c)
            i += 1
    return "".join(ergebnis)


_IFDEF = re.compile(r"^\s*#\s*ifdef\s+(\w+)\s*$", re.M)
_IFDEF2 = re.compile(r"^\s*#\s*if\s+defined\s*\(\s*(\w+)\s*\)\s*$", re.M)
_IF_IRGEND = re.compile(r"^\s*#\s*if")
_ENDIF = re.compile(r"^\s*#\s*endif")


def _bloecke(quelle):
    """(makro, zeilennr, blockzeilen) je bedingtem Block auf oberster Ebene."""
    zeilen = quelle.splitlines()
    aus = []
    i = 0
    while i < len(zeilen):
        m = _IFDEF.match(zeilen[i]) or _IFDEF2.match(zeilen[i])
        if not m:
            i += 1
            continue
        makro, start = m.group(1), i
        tiefe, block = 1, []
        i += 1
        while i < len(zeilen) and tiefe:
            if _IF_IRGEND.match(zeilen[i]):
                tiefe += 1
            elif _ENDIF.match(zeilen[i]):
                tiefe -= 1
                if not tiefe:
                    break
            block.append(zeilen[i])
            i += 1
        aus.append((makro, start + 1, block))
        i += 1
    return aus


def _definierte_makros():
    """Jedes Makro, das irgendein Bausystem definiert — gemessen.

    KEINE Liste bekannter Testmakros. Wer morgen einen neuen Waechter
    erfindet, ist am selben Tag erfasst.
    """
    gefunden = set()
    kandidaten = []
    for muster in BAU_MUSTER:
        kandidaten += [p for p in WURZEL.rglob(muster)
                       if ".git" not in p.parts]
    fang = re.compile(
        r"(?:DEFINES\s*\+?=|add_definitions|target_compile_definitions|"
        r"COMPILE_DEFINITIONS|#\s*define|[-/]D)\s*[(\s]*([A-Za-z_]\w*)")
    for p in kandidaten:
        try:
            t = p.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        for m in fang.finditer(t):
            gefunden.add(m.group(1))
        # `-DFOO=1` und `DEFINES += FOO BAR` mit mehreren Namen je Zeile
        for zeile in t.splitlines():
            if "DEFINES" in zeile or "-D" in zeile:
                for w in re.findall(r"[-/]D\s*([A-Za-z_]\w*)", zeile):
                    gefunden.add(w)
    return gefunden


def messe(wurzel=None):
    """-> sortierte Liste 'pfad:zeile:MAKRO:anzahl_asserts'."""
    global WURZEL
    alt = WURZEL
    if wurzel:
        WURZEL = Path(wurzel)
    try:
        definiert = _definierte_makros()
        treffer = []
        for name in _dateien():
            p = WURZEL / name
            try:
                roh = p.read_text(encoding="utf-8", errors="replace")
            except OSError:
                continue
            if "assert" not in roh:
                continue
            sauber = _ohne_kommentare(roh)
            for makro, zeile, block in _bloecke(sauber):
                if makro in definiert:
                    continue
                # Aufrufe zaehlen, nicht Zeilen: der Selbsttest fiel hier,
                # weil zwei asserts auf EINER Zeile als 1 galten.
                n = sum(len(re.findall(r"\bassert\s*\(", b)) for b in block)
                if n:
                    treffer.append("%s:%d:%s:%d" % (name, zeile, makro, n))
        return sorted(treffer)
    finally:
        WURZEL = alt


# ── Selbsttest ──────────────────────────────────────────────────────────
#
# Vor dem Nenner, nach der Hausregel: eine Erstfassung eines Werkzeugs in
# `tools/uft-innendienst/` meldete „Selbsttest 3/3" und lieferte gemessen
# 0/3. Ein Tor, dessen Messung nicht vorgefuehrt ist, ist eine Behauptung.

_FIXTURES = {
    # 1. der Fall, den das Tor fangen soll
    "src/faengt.c": """
#ifdef NIE_DEFINIERT_TEST
int main(void) { assert(1 == 1); assert(2 == 2); return 0; }
#endif
""",
    # 2. derselbe Block, aber das Makro wird gebaut -> kein Treffer
    "src/gebaut.c": """
#ifdef WIRD_GEBAUT_TEST
int main(void) { assert(1 == 1); return 0; }
#endif
""",
    # 3. Waechter ohne assert -> Vorfuehrprogramm, kein Treffer
    "src/nur_demo.c": """
#ifdef DEMO_TEST
int main(void) { printf("hallo"); return 0; }
#endif
""",
    # 4. der Fehlalarm, der beim Bau wirklich auftrat: die Erklaerung
    #    eines FRUEHEREN Blocks im Kommentar
    "src/geheilt.c": """
/* MF-851: hier stand ein
 * #ifdef UFT_UNIT_TESTS
 * Block mit assert(x); — er wurde nach tests/ gehoben.
 */
int echt(void) { return 1; }
""",
    # 5. assert ausserhalb jedes Waechters -> kein Treffer
    "src/offen.c": """
int f(void) { assert(1); return 0; }
""",
    "bau.pro": "DEFINES += WIRD_GEBAUT_TEST\n",
}


def selbsttest():
    print("=== Selbsttest ===")
    gruen = rot = 0

    def pruefe(name, bedingung, hinweis=""):
        nonlocal gruen, rot
        if bedingung:
            print("  [OK]   %s" % name)
            gruen += 1
        else:
            print("  [ROT]  %s%s" % (name, ("  — " + hinweis) if hinweis
                                     else ""))
            rot += 1

    with tempfile.TemporaryDirectory() as td:
        w = Path(td)
        for name, inhalt in _FIXTURES.items():
            ziel = w / name
            ziel.parent.mkdir(parents=True, exist_ok=True)
            ziel.write_text(inhalt, encoding="utf-8")
        ergebnis = messe(w)
        wo = " ".join(ergebnis)

        pruefe("undefinierter Waechter mit assert wird gefunden",
               "faengt.c" in wo, wo)
        pruefe("  ... und zaehlt beide Zusagen",
               ":NIE_DEFINIERT_TEST:2" in wo, wo)
        pruefe("gebautes Makro wird NICHT gemeldet",
               "gebaut.c" not in wo, wo)
        pruefe("Waechter ohne assert wird NICHT gemeldet",
               "nur_demo.c" not in wo, wo)
        pruefe("Kommentar ueber einen frueheren Block loest nicht aus",
               "geheilt.c" not in wo, wo)
        pruefe("assert ausserhalb eines Waechters loest nicht aus",
               "offen.c" not in wo, wo)

    print("\n  %d gruen, %d rot" % (gruen, rot))
    return 0 if rot == 0 else 1


def check(repo) -> list:
    """Hausschnittstelle fuer scripts/check_consistency.py.

    Meldet NUR Neuzugaenge gegen die Grundlinie. Aufgeloeste Eintraege
    sind kein Fehler — sie sind das Ziel; sie werden hier trotzdem
    genannt, damit die Grundlinie nicht auseinanderlaeuft.
    """
    global WURZEL, GRUNDLINIE
    alt_w, alt_g = WURZEL, GRUNDLINIE
    WURZEL = Path(repo)
    GRUNDLINIE = WURZEL / "docs" / "selbsttest_baseline.txt"
    try:
        jetzt = set(messe())
        basis = set()
        if GRUNDLINIE.exists():
            basis = {z.strip() for z in
                     GRUNDLINIE.read_text(encoding="utf-8").splitlines()
                     if z.strip() and not z.startswith("#")}
        fehler = []
        for z in sorted(jetzt - basis):
            pfad, zeile, makro, n = z.rsplit(":", 3)
            fehler.append(
                "%s:%s: %d Zusage(n) hinter `#ifdef %s` — dieses Makro wird "
                "von KEINEM Bausystem definiert, der Block wurde nie "
                "uebersetzt und kann nicht rot werden. Entweder den "
                "Waechter im Bausystem definieren, oder die Zusagen nach "
                "tests/ heben (Muster: MF-851, P3-313)."
                % (pfad, zeile, int(n), makro))
        for z in sorted(basis - jetzt):
            fehler.append(
                "docs/selbsttest_baseline.txt fuehrt `%s`, gemessen ist er "
                "weg — bitte die Zeile aus der Grundlinie nehmen." % z)
        return fehler
    finally:
        WURZEL, GRUNDLINIE = alt_w, alt_g


def main():
    if "--selbsttest" in sys.argv:
        return selbsttest()

    if selbsttest() != 0:
        print("\nABBRUCH: der Selbsttest ist rot — die Messung unten waere "
              "eine Behauptung.")
        return 2

    print("\n=== Selbsttests hinter einem nie uebersetzten Waechter ===")
    jetzt = messe()

    if "--grundlinie-schreiben" in sys.argv:
        GRUNDLINIE.parent.mkdir(parents=True, exist_ok=True)
        kopf = [
            "# Selbsttests hinter einem Waechter, den kein Bausystem "
            "definiert (MF-1000).",
            "#",
            "# Erzeugt von scripts/audit_selbsttest_ohne_uebersetzung.py.",
            "# Format: pfad:zeile:MAKRO:anzahl_asserts",
            "#",
            "# Diese Zeilen sind BESTAND, kein Freibrief. Das Tor faellt bei",
            "# jedem NEUEN Eintrag. Wer einen Block nach tests/ hebt (Muster:",
            "# MF-851), nimmt seine Zeile heraus — die Zahl kann nur sinken.",
            "",
        ]
        GRUNDLINIE.write_text("\n".join(kopf + jetzt) + "\n",
                              encoding="utf-8")
        print("Grundlinie geschrieben: %d Eintraege" % len(jetzt))
        return 0

    alt = set()
    if GRUNDLINIE.exists():
        alt = {z.strip() for z in
               GRUNDLINIE.read_text(encoding="utf-8").splitlines()
               if z.strip() and not z.startswith("#")}

    neu = sorted(set(jetzt) - alt)
    weg = sorted(alt - set(jetzt))

    summe = sum(int(z.rsplit(":", 1)[1]) for z in jetzt)
    print("  Bloecke: %d   Zusagen darin: %d   Grundlinie: %d"
          % (len(jetzt), summe, len(alt)))

    if weg:
        print("\n  %d Eintraege sind aufgeloest — bitte aus der Grundlinie "
              "nehmen:" % len(weg))
        for z in weg[:20]:
            print("    - %s" % z)

    if neu:
        print("\n  NEU (%d) — ein Selbsttest, den kein Compiler sieht:"
              % len(neu))
        for z in neu:
            print("    + %s" % z)
        print("\n  Entweder den Waechter im Bausystem definieren, oder die")
        print("  Zusagen nach tests/ heben (Muster: MF-851).")
        return 1

    print("\n  0 neu.")
    return 0


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    sys.exit(main())
