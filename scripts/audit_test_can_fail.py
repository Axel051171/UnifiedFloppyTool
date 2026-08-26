#!/usr/bin/env python3
"""Tests, die nicht scheitern koennen (MF-605).

── Warum es dieses Tor gibt ─────────────────────────────────────────────

In 32 Testdateien stand wortgleich dies:

    #define RUN_TEST(name) do { \\
        printf("  Running %s... ", #name); \\
        tests_run++; \\
        test_##name(); \\
        tests_passed++;                     <- bedingungslos
        printf("PASSED\\n"); \\
    } while(0)

    #define ASSERT(condition) do { \\
        if (!(condition)) { \\
            printf("FAILED at line %d: %s\\n", __LINE__, #condition); \\
            return;                         <- nur aus der Testfunktion
        } \\
    } while(0)

`ASSERT` kehrt bei Fehlschlag aus der Testfunktion zurueck. `RUN_TEST`
zaehlt danach trotzdem einen Erfolg und druckt „PASSED" — in die Zeile
DIREKT HINTER „FAILED at line N". `main()` gibt
`(tests_passed == tests_run) ? 0 : 1` zurueck, also immer 0.

Diese Tests konnten nicht rot werden. Dahinter lagen sieben Tests mit 18
Pruefungen, davon DREI echte Fehler im Format-Layer — jedes
Game-Gear-Abbild wurde als Master System gemeldet; der Game-Boy-Kopf
wurde als Speicherabbild ueberworfen; die kennungslose Z80-Vermutung
verschluckte TAP und DSK. Alle drei standen jahrelang hinter gruenen
Tests (MF-596 … MF-600).

Ein Test, der nicht scheitern kann, ist schlimmer als kein Test: er
belegt einen Platz in der Bilanz und beruhigt.

── Was gemeldet wird ────────────────────────────────────────────────────

Eine Datei faellt auf, wenn BEIDES zutrifft:

  1. ihr Aufruf-Makro erhoeht einen Erfolgszaehler bedingungslos hinter
     dem Aufruf der Testfunktion, UND
  2. ihre Zusicherungs-Makros haben keinen Fluchtweg, der das aufwiegt.

Als Fluchtweg gilt:

  * `exit(...)` im Fehlerpfad — der Prozess endet, der Zaehler wird nie
    erreicht (so macht es `test_mega65_fat32.c`);
  * das Erhoehen eines FEHLER-Zaehlers, den `main()` auswertet (so macht
    es `test_libdsk_formats.c` mit `tests_failed`);
  * das Setzen einer Fehler-Fahne, die das Aufruf-Makro abfragt (so
    machen es die 32 seit MF-596).

Beides zusammen — bedingungsloser Erfolgszaehler UND blosses `return` im
Fehlerpfad — ist der Fehler.
"""
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]

DEFINE = re.compile(r"^[ \t]*#[ \t]*define[ \t]+(\w+)\s*\(", re.M)
PASS_INC = re.compile(r"\b(\w*pass\w*|\w*ok\w*|\w*success\w*)\s*\+\+")
FAIL_INC = re.compile(r"\b(\w*fail\w*|\w*error\w*|\w*bad\w*)\s*\+\+")
FLAG_SET = re.compile(r"\b\w*fail\w*\s*=\s*(?:1|true)\b")
EXITS = re.compile(r"\bexit\s*\(|\babort\s*\(|\bassert\s*\(")
BRANCH = re.compile(r"\bif\b|\?|&&|\|\|")


def macro_body(lines: list[str], i: int) -> str:
    """Makro-Koerper ab Zeile i einsammeln (Fortsetzung mit \\)."""
    out = [lines[i]]
    j = i
    while out[-1].rstrip().endswith("\\") and j + 1 < len(lines):
        j += 1
        out.append(lines[j])
    return "\n".join(out)


def scan() -> list[tuple[str, int, str]]:
    findings = []
    tdir = ROOT / "tests"
    files = sorted(list(tdir.rglob("*.c")) + list(tdir.rglob("*.cpp")))
    for p in files:
        text = p.read_text(encoding="utf-8", errors="replace")
        lines = text.split("\n")

        runner = None          # (Zeile, Koerper)
        asserts = []
        for i, line in enumerate(lines):
            m = DEFINE.match(line)
            if not m:
                continue
            body = macro_body(lines, i)
            if "test_##" in body:
                runner = (i + 1, body)
            elif re.search(r"ASSERT|CHECK|EXPECT|REQUIRE", m.group(1), re.I):
                asserts.append(body)

        if not runner:
            continue
        ln, body = runner

        inc = PASS_INC.search(body)
        if not inc:
            continue                       # kein Erfolgszaehler: nichts zu holen
        call_at = body.find("test_##")
        if inc.start() < call_at:
            continue                       # zaehlt VOR dem Aufruf: anderer Bau
        if BRANCH.search(body[call_at:inc.start()]):
            continue                       # bedingt — in Ordnung

        # Fluchtwege in den Zusicherungen?
        joined = "\n".join(asserts)
        if EXITS.search(joined):
            continue                       # Prozess endet im Fehlerfall
        if FAIL_INC.search(joined) and FAIL_INC.search(text):
            continue                       # Fehlerzaehler, den main auswertet
        if FLAG_SET.search(joined) and FLAG_SET.search(body) is None:
            # Fahne gesetzt UND vom Aufruf-Makro abgefragt?
            if re.search(r"\b\w*fail\w*\b", body):
                continue

        findings.append((p.relative_to(ROOT).as_posix(), ln, inc.group(1)))
    return findings


def check(repo) -> list:
    """Schnittstelle fuer check_consistency.py."""
    try:
        rows = scan()
    except Exception as exc:               # noqa: BLE001
        return ["Test-Scheiterbarkeit nicht pruefbar: %s" % exc]
    return ["Test kann nicht scheitern: %s:%d — `%s++` steht bedingungslos "
            "hinter dem Aufruf, und die Zusicherungen kehren nur zurueck"
            % (f, ln, c) for f, ln, c in rows]


def main() -> int:
    rows = scan()
    print("Tests, die nicht scheitern koennen: %d" % len(rows))
    for f, ln, c in rows:
        print("  %-52s:%-6d %s++" % (f, ln, c))
    return 1 if rows else 0


if __name__ == "__main__":
    sys.exit(main())
