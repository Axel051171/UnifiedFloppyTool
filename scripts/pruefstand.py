#!/usr/bin/env python3
"""pruefstand.py — bauen UND testen, ohne den Rueckgabewert zu verlieren
(MF-667).

    python scripts/pruefstand.py [-b BUILD_DIR] [-j N] [-R MUSTER]

── Warum es dieses Skript gibt ─────────────────────────────────────────

Am 2026-08-29 stellte sich heraus, dass VIER Qt-Ziele laengere Zeit
nicht bauten — und niemand sah es. Zwei Dinge hatten es versteckt:

  1. `cmake --build . | grep -c error` liefert die Trefferzahl von
     `grep`, NICHT den Rueckgabewert des Baus. Wer so prueft, sieht
     immer 0, wenn die Fehlermeldungen anders aussehen als sein Muster.
     Diese Falle steht seit laengerem in den Projektnotizen; sie ist
     trotzdem zweimal zugeschnappt.

  2. `ctest` blieb gruen, weil die betroffenen Ziele ihre ausfuehrbaren
     Dateien aus einem FRUEHEREN Bau hatten. Der Pruefstand lief gegen
     veraltete Binaerdateien und meldete "278/278".

**"Tests gruen" und "Bau gruen" sind zwei Aussagen.** Dieses Skript
macht daraus einen Vorgang, in dem man die eine nicht fuer die andere
halten kann:

  * Der Bau laeuft mit `-k` (keep-going), damit ein kaputtes Ziel nicht
    150 andere mitreisst — aber sein Rueckgabewert wird FESTGEHALTEN.
  * `ctest` laeuft trotzdem, damit man die Ergebnisse der bauenden
    Ziele sieht.
  * Der Rueckgabewert am Ende ist **rot, sobald eines von beidem rot
    war**. Ein gruener Testlauf auf einem roten Bau ergibt hier kein
    Gruen.

Wer nur `ctest` braucht, ruft `ctest`. Wer wissen will, ob der Baum in
Ordnung ist, ruft dieses Skript.
"""
from __future__ import annotations

import argparse
import os
import pathlib
import re
import subprocess
import sys

WURZEL = pathlib.Path(__file__).resolve().parent.parent

ZIEL_RE = re.compile(r"CMakeFiles[/\\]([A-Za-z0-9_]+)\.dir")
FEHLER_RE = re.compile(r"^\S+:\d+:\d+: (?:fatal error|error):", re.M)
# Binderfehler sehen anders aus und haben KEINE Zeilennummer. Sie nur
# nicht mitzuzaehlen ergab beim ersten Lauf die irrefuehrende Zeile
# "0 Uebersetzungsfehler" direkt neben "Bau: ROT".
BINDER_RE = re.compile(r"undefined reference to|cannot find -l", re.I)


def lauf(cmd, cwd):
    """Fuehrt aus und gibt (rc, ausgabe) — die Ausgabe wird MITGESCHRIEBEN,
    nicht verschluckt, und der Rueckgabewert kommt vom Befehl, nicht von
    einer Pipe."""
    p = subprocess.Popen(cmd, cwd=str(cwd), stdout=subprocess.PIPE,
                         stderr=subprocess.STDOUT, text=True,
                         errors="replace", bufsize=1)
    zeilen = []
    assert p.stdout is not None
    for zeile in p.stdout:
        sys.stdout.write(zeile)
        zeilen.append(zeile)
    p.wait()
    return p.returncode, "".join(zeilen)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("-b", "--build-dir", default="build",
                    help="Bauverzeichnis (Vorgabe: build)")
    ap.add_argument("-j", "--jobs", default=str(os.cpu_count() or 4))
    ap.add_argument("-R", "--tests-regex", default=None,
                    help="nur Tests, die passen (wie ctest -R)")
    ap.add_argument("--no-tests", action="store_true",
                    help="nur bauen")
    a = ap.parse_args()

    bau = WURZEL / a.build_dir
    if not (bau / "CMakeCache.txt").is_file():
        print("FEHLER: %s ist kein CMake-Bauverzeichnis "
              "(CMakeCache.txt fehlt)." % bau)
        return 2

    print("== 1/2  bauen ==================================================")
    rc_bau, log = lauf(["cmake", "--build", ".", "--parallel", a.jobs,
                        "--", "-k"], bau)

    rc_test = 0
    if not a.no_tests:
        print("\n== 2/2  testen =============================================")
        cmd = ["ctest", "--output-on-failure", "--timeout", "300"]
        if a.tests_regex:
            cmd += ["-R", a.tests_regex]
        rc_test, _ = lauf(cmd, bau)

    print("\n== Ergebnis ====================================================")
    if rc_bau == 0:
        print("Bau    : GRUEN")
    else:
        print("Bau    : ROT (Rueckgabewert %d)" % rc_bau)
        ziele = sorted(set(ZIEL_RE.findall(log)))
        if ziele:
            print("  Ziele mit Fehlern: %s" % ", ".join(ziele[:20]))
        n_uebersetzung = len(FEHLER_RE.findall(log))
        n_binder = len(BINDER_RE.findall(log))
        print("  %d Uebersetzungsfehler, %d Binderfehler im Protokoll"
              % (n_uebersetzung, n_binder))
        if n_uebersetzung == 0 and n_binder == 0:
            print("  (keiner von beiden erkannt — das Protokoll steht oben; "
                  "die Muster hier sind eine Lesehilfe, kein Urteil)")

    if a.no_tests:
        print("Tests  : uebersprungen (--no-tests)")
    elif rc_test == 0:
        print("Tests  : GRUEN")
        if rc_bau != 0:
            print("\n  ACHTUNG: die Tests sind gruen, der Bau ist ROT.")
            print("  Das heisst NICHT, dass alles in Ordnung ist — die")
            print("  nicht gebauten Ziele liefen entweder gar nicht, oder")
            print("  ctest hat eine VERALTETE ausfuehrbare Datei aus einem")
            print("  frueheren Bau benutzt. Genau so blieben vier kaputte")
            print("  Ziele laengere Zeit unbemerkt (MF-667).")
    else:
        print("Tests  : ROT (Rueckgabewert %d)" % rc_test)

    gesamt = 0 if (rc_bau == 0 and rc_test == 0) else 1
    print("\nGESAMT : %s" % ("GRUEN" if gesamt == 0 else "ROT"))
    return gesamt


if __name__ == "__main__":
    raise SystemExit(main())
