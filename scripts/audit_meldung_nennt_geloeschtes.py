#!/usr/bin/env python3
"""Eine Fehlermeldung, die dem Benutzer einen Ausweg nennt, muss einen
Ausweg nennen, den es GIBT (MF-1025).

── Warum es dieses Tor gibt ────────────────────────────────────────────

Die Hardware-Schicht dieses Baums folgt dem Drei-Teile-Vertrag aus
Regel F-4: jede Fehlermeldung sagt **What**, **Why** und **Fix**. Der
Fix-Teil ist eine Handlungsanweisung an den Benutzer, und damit die
einzige Stelle im Werkzeug, an der eine veraltete Aussage nicht nur
falsch informiert, sondern **Arbeit kostet**.

Gemessen am Vorzustand von MF-1025 nannten vier solche Fix-Saetze in
`src/hardware_providers/scp_provider_v2.cpp` (Zeilen 74, 129, 247, 353)
den Ausweg:

    „use the V1 SCPHardwareProvider (serial path) until M3.1 lands"

Beides trug nicht. `SCPHardwareProvider` gibt es im ganzen Baum nicht
mehr — die V1-Hierarchie fiel in **P1.17 / MF-169**, und
`git grep "class.*HardwareProvider"` liefert **null** Treffer. Und M3.1
ist gelandet (MF-254, in CLAUDE.md so gefuehrt). Der Benutzer wurde
also auf eine geloeschte Klasse verwiesen, um auf ein Ereignis zu
warten, das eingetreten war.

**Der Satz war zugleich KOPIERT** — `tests/test_scp_provider_v2.cpp:194`
traegt ihn woertlich. Beim Aufschreiben dieses Kopfes stand hier zuerst
„ein gruener Test hat den Satz bewacht"; nachgelesen trifft das **nicht**
zu, und die Berichtigung gehoert hierher: der Test **behauptet** den Text
nicht, er benutzt ihn als Beispiel-Nutzlast fuer die Pruefung „ein
wohlgeformter `ProviderError` darf nicht werfen". Kein Test waere rot
geworden. Der Befund ist damit schwaecher als zuerst formuliert und
bleibt einer: eine falsche Aussage an zwei Stellen ueberlebt jede
Korrektur, die nur eine davon anfasst.

── Was hier gemessen wird ──────────────────────────────────────────────

Fuer jede Zeichenkette in `src/hal` und `src/hardware_providers`: nennt
sie einen Bezeichner in der EIGENEN Namensform dieses Baums, und gibt
es diesen Bezeichner im Baum ueberhaupt?

Zwei Namensformen, beide bewusst eng gewaehlt:

  * C++-Klassen dieses Projekts — `...Provider`, `...ProviderV2`,
    `...HardwareProvider`, `...Transport`, `...Runner`
  * C-Funktionen dieses Projekts — `uft_...()`, mit Klammern genannt

„Gibt es" heisst: der Bezeichner steht in irgendeiner versionierten
`.c`/`.cpp`/`.h`/`.hpp`-Datei **ausserhalb** einer Zeichenkette. Das ist
absichtlich KONSERVATIV in die sichere Richtung: wer irgendwo im Baum
als Code vorkommt — als Deklaration, Definition, Aufruf oder auch nur
in einer Vorwaertsdeklaration —, geht durch. Gemeldet wird nur, wer
NUR in Zeichenketten lebt. Genau das war der SCP-Fall.

── Was das Tor ausdruecklich NICHT sieht ───────────────────────────────

  * **Veraltete Aussagen ueber ZUSTAENDE.** „until M3.1 lands" nennt
    keinen Bezeichner; dass M3.1 gelandet ist, kann dieses Tor nicht
    wissen. Derselbe Fall: „does not yet have a production construction
    site" (`src/hal/uft_applesauce.c`, vor MF-1025) — gemessen waren es
    **drei** Konstruktionsstellen. Solche Saetze faengt nur eine
    Messung, kein Muster. Sie stehen als **P3-330**.
  * **Bezeichner in Kommentaren.** Ein Kommentar ist an Entwickler
    gerichtet, eine Fehlermeldung an Benutzer. Nur Letztere wird hier
    gemessen.
  * **`static_assert`-Meldungen.** Sie erreichen nie einen Benutzer —
    sie erscheinen im Bauprotokoll. Der erste Lauf dieses Tores hat
    genau daran seine Grenze gezeigt: `src/hardware_providers/
    usbfloppy_provider_v2.h:491` sagt in einem `static_assert`
    woertlich „`uft_ufi_start_stop()` does not exist in ufi.h" — eine
    RICHTIGE Aussage, gemeldet, weil sie richtig ist. Abgegrenzt wird
    deshalb an der ART der Zeichenkette, nicht an ihrer Prosa: haette
    das Tor auf Wendungen wie „does not exist" gehorcht, waere es
    dieselbe Falle wie in MF-1023 gewesen, wo ein Makroname in einem
    Zeichenketten-Literal ein Tor erfuellt hat.
  * **Fremdbezeichner.** `dtc`, `fcimage`, `fluxengine`, `hxcfe` sind
    externe Werkzeuge und haben im Baum zu Recht keinen Code.
  * **Andere Verzeichnisse.** Der Befund kam aus der Hardware-Schicht,
    und dort ist der Drei-Teile-Vertrag verbindlich. Ob `src/formats`
    dieselbe Klasse traegt, ist ungemessen und steht als P3-330.

Grundlinie **0** — darf nur fallen.
"""
from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

GRUNDLINIE = 0

# Wo gesucht wird (die Schicht, aus der der Befund kam).
BEREICHE = ("src/hal", "src/hardware_providers")

BLOCK = re.compile(r"/\*.*?\*/", re.S)
ZEILE = re.compile(r"//[^\n]*")
# C/C++-Zeichenkette, Escapes beachtet.
LITERAL = re.compile(r'"((?:[^"\\\n]|\\.)*)"')
# `static_assert(...)` / `_Static_assert(...)` — compiler-gerichtet.
ASSERT_KOPF = re.compile(r"\b_?[Ss]tatic_assert\s*\(")

# Namensformen dieses Baums.
KLASSE = re.compile(
    r"\b([A-Z][A-Za-z0-9]*(?:HardwareProvider|ProviderV2|Provider"
    r"|Transport|Runner))\b")
FUNKTION = re.compile(r"\b(uft_[a-z0-9_]{3,})\s*\(")


def _dateien(repo: Path, *bereiche: str) -> list[Path]:
    """Dateimenge aus git, nie aus einer gepflegten Liste (MF-636)."""
    try:
        out = subprocess.run(
            ["git", "ls-files", "--cached", "--others", "--exclude-standard",
             *bereiche],
            cwd=repo, capture_output=True, text=True, check=False).stdout
    except OSError:
        return []
    return [repo / p for p in out.splitlines() if p.strip()]


def _code_ohne_zeichenketten(text: str) -> str:
    """Kommentare und Zeichenketten weg — es bleibt reiner Code."""
    return LITERAL.sub('""', ZEILE.sub("", BLOCK.sub("", text)))


def _assert_spannen(text: str) -> list[tuple[int, int]]:
    """Zeichenbereiche der `static_assert(...)`-Argumente, per Klammer-
    zaehlung — nicht per Zeile, denn diese Meldungen sind mehrzeilig."""
    spannen = []
    for m in ASSERT_KOPF.finditer(text):
        i = text.index("(", m.start())
        tiefe = 0
        for j in range(i, len(text)):
            if text[j] == "(":
                tiefe += 1
            elif text[j] == ")":
                tiefe -= 1
                if tiefe == 0:
                    spannen.append((i, j))
                    break
    return spannen


def _bezeichner_im_code(repo: Path) -> set[str]:
    """Jeder Bezeichner, der irgendwo im Baum als CODE vorkommt."""
    da: set[str] = set()
    for p in _dateien(repo):
        if p.suffix not in (".c", ".cpp", ".h", ".hpp", ".cc", ".hh"):
            continue
        try:
            t = p.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        code = _code_ohne_zeichenketten(t)
        da.update(re.findall(r"\b(\w{3,})\b", code))
    return da


def messe(repo: Path):
    """(Befunde, Notiz). Befund = (Datei, Zeile, Bezeichner, Auszug)."""
    if not (repo / ".git").exists():
        pass  # Selbsttest arbeitet in einem frischen git-Verzeichnis
    da = _bezeichner_im_code(repo)
    if not da:
        return None, "git nicht befragbar — Tor laesst durch und sagt es"

    befunde = []
    for p in _dateien(repo, *BEREICHE):
        if p.suffix not in (".c", ".cpp", ".h", ".hpp"):
            continue
        try:
            t = p.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        # Kommentare weg, damit nur BENUTZERTEXT gemessen wird.
        ohne = ZEILE.sub("", BLOCK.sub("", t))
        assert_bereiche = _assert_spannen(ohne)
        for m in LITERAL.finditer(ohne):
            if any(a <= m.start() <= b for a, b in assert_bereiche):
                continue  # compiler-gerichtet, nicht benutzergerichtet
            inhalt = m.group(1)
            zeile = ohne.count("\n", 0, m.start()) + 1
            genannt = set(KLASSE.findall(inhalt))
            genannt |= set(FUNKTION.findall(inhalt))
            for name in sorted(genannt):
                if name in da:
                    continue
                befunde.append((
                    p.relative_to(repo).as_posix(), zeile, name,
                    inhalt[:70]))
    return befunde, None


def check(repo: Path) -> list[str]:
    befunde, notiz = messe(repo)
    if befunde is None:
        return ["HINWEIS: %s" % notiz]
    if len(befunde) <= GRUNDLINIE:
        return []
    return [
        "%s:%d nennt `%s` — im Baum gibt es diesen Bezeichner nur in "
        "Zeichenketten: \"%s...\"" % (rel, zl, name, txt)
        for rel, zl, name, txt in befunde
    ]


# ── Selbsttest ──────────────────────────────────────────────────────────
_FALL_SCHLECHT = '''
#include "x.h"
uft_error_t f(void) {
    return mach("Use the V1 GhostHardwareProvider until it lands");
}
'''
_FALL_GUT = '''
#include "x.h"
class RealProviderV2 { };
uft_error_t f(void) {
    return mach("Construct RealProviderV2 with a valid handle");
}
'''
_FALL_KOMMENTAR = '''
/* GhostHardwareProvider was deleted in P1.17 — historical note. */
uft_error_t f(void) { return 0; }
'''
_FALL_FUNKTION = '''
uft_error_t f(void) {
    return mach("call uft_niemals_existiert() first");
}
'''
# Der Fall, an dem der erste Lauf seine Grenze gezeigt hat: eine
# `static_assert`-Meldung, die ausdruecklich die ABWESENHEIT feststellt.
_FALL_STATIC_ASSERT = '''
static_assert(!Kann<X>,
    "X must NOT satisfy Kann "
    "(current surface: uft_niemals_existiert() does not exist in x.h. "
    "Omit until the backend exposes it)");
'''
# Und die Gegenprobe dazu: eine ECHTE Meldung in derselben Datei muss
# weiter gemeldet werden, damit die Ausnahme nicht die Datei freistellt.
_FALL_ASSERT_UND_ECHT = '''
static_assert(!Kann<X>, "uft_niemals_existiert() does not exist");
uft_error_t f(void) {
    return mach("use the V1 GhostHardwareProvider instead");
}
'''


def _selbsttest(_repo: Path) -> int:
    import tempfile
    faelle = [
        ("geloeschte Klasse in Meldung", _FALL_SCHLECHT, True),
        ("vorhandene Klasse in Meldung", _FALL_GUT, False),
        ("nur im Kommentar genannt", _FALL_KOMMENTAR, False),
        ("geloeschte Funktion in Meldung", _FALL_FUNKTION, True),
        ("static_assert stellt Abwesenheit fest", _FALL_STATIC_ASSERT,
         False),
        ("static_assert daneben, echte Meldung bleibt",
         _FALL_ASSERT_UND_ECHT, True),
    ]
    gut = 0
    for name, quelle, soll in faelle:
        with tempfile.TemporaryDirectory() as d:
            p = Path(d)
            subprocess.run(["git", "init", "-q"], cwd=d, capture_output=True)
            (p / "src").mkdir()
            (p / "src" / "hal").mkdir()
            (p / "src" / "hal" / "probe.c").write_text(quelle,
                                                       encoding="utf-8")
            befunde, _ = messe(p)
            hat = bool(befunde)
            if hat == soll:
                gut += 1
            else:
                print("  Selbsttest '%s': erwartet meckern=%s, bekommen %s"
                      % (name, soll, hat))
                if befunde:
                    for b in befunde:
                        print("      %s" % (b,))
    print("Selbsttest: %d/%d" % (gut, len(faelle)))
    return 0 if gut == len(faelle) else 1


def main() -> int:
    repo = Path(__file__).resolve().parent.parent
    if "--selftest" in sys.argv:
        return _selbsttest(repo)

    if "-v" in sys.argv or "--list" in sys.argv:
        befunde, notiz = messe(repo)
        if notiz:
            print("HINWEIS: %s" % notiz)
        else:
            print("Meldungen, die Geloeschtes nennen: %d (Grundlinie %d)"
                  % (len(befunde), GRUNDLINIE))
            for rel, zl, name, txt in befunde:
                print("  %-52s :%-5d %-26s %s" % (rel, zl, name, txt))
            print()

    errs = check(repo)
    if not errs:
        print("OK: keine Fehlermeldung nennt einen Ausweg, den es nicht "
              "gibt.")
        return 0
    print("FAIL: %d Befund(e)" % len(errs))
    for e in errs:
        print("  %s" % e)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
