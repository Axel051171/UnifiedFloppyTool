#!/usr/bin/env python3
"""Bedeutet derselbe Name in zwei Uebersetzungseinheiten dasselbe? (MF-540/559)

── Was hier schiefstehen kann ───────────────────────────────────────────

Der Baum hat Header, die sich einen `#ifndef`-Waechter TEILEN, aber
verschiedenen Inhalt darunter stellen. Wer zuerst eingebunden wird,
gewinnt; die uebrigen werden still uebersprungen. Welche Zahl eine
Konstante in einer Uebersetzungseinheit bedeutet, entscheidet damit die
Include-Reihenfolge — ohne Warnung, ohne Fehler.

`scripts/shared_guard_gate.py` zaehlt diese Kollisionen (40, davon 37 mit
abweichendem Inhalt). Es sagt aber nicht, ob der Unterschied etwas
BEDEUTET. Zwei Enums mit denselben Namen und denselben Werten sind
Redundanz; dieselben Namen mit anderen Werten sind eine stille
Bedeutungsverschiebung.

Gemessen (MF-559): von 37 Kollisionen haben **fuenf** echte Wert-Konflikte,
und **drei davon sind in gebautem Code scharf**:

    UFT_FORMAT_ADF        3 in sieben Einheiten, 6 in uft_format_suggest.c
    UFT_PLATFORM_AMIGA    1 / 2 / 5 in drei Einheiten
    UFT_PROT_COPYLOCK     1 / 10 / 22 / 512 / 4096 in fuenf Einheiten

Fuenf Bedeutungen fuer einen Namen, alle in derselben Binaerdatei.

Der urspruengliche Fall (MF-540, `UFT_FMT_D64`) ist NICHT scharf: alle
messbaren Einheiten sehen dieselbe Zahl. Er bleibt hier, weil die Kollision
besteht und der naechste Include ihn scharf machen kann.

── Wie gemessen wird ────────────────────────────────────────────────────

Nicht per Textvergleich, sondern mit dem PRAEPROZESSOR: fuer jede
Uebersetzungseinheit, die eine Sonden-Konstante benutzt, wird ermittelt,
welchen Wert sie dort wirklich hat. Das ist die einzige Messung, die zaehlt
— alles andere ist eine Aussage ueber Text, nicht ueber das Programm.

── Was das Tor durchlaesst, und warum ───────────────────────────────────

Die drei scharfen Faelle stehen mit ihrer gemessenen Werteverteilung in
`ARMED_BASELINE`. Ein Eintrag dort heisst NICHT "harmlos", sondern
"gemessen, und die Verteilung ist genau diese". Aendert sich die
Verteilung — eine neue Einheit, eine umsortierte Include-Zeile — roetet das
Tor.

Sie aufzuloesen heisst, die Enums zusammenzulegen. Das ist Arbeit am ABI
und gehoert nicht in eine Release-Vorbereitung; siehe KNOWN_ISSUES ID-1
und ID-2.

── Grenzen ──────────────────────────────────────────────────────────────

Gemessen wird mit dem Praeprozessor des lokalen gcc. Ist keiner
auffindbar, gibt das Tor NICHTS zurueck statt zu raten — ein Tor, das ohne
Messung urteilt, ist schlimmer als keines. Die CI misst mit ihrem eigenen
Compiler.

Gemessen werden nur Einheiten, die die Konstante tatsaechlich benutzen.
Eine Einheit, die den Typ nur durchreicht, kann sich nicht verrechnen.
"""

from __future__ import annotations

import re
import shutil
import subprocess
import sys
from pathlib import Path

# (Waechter, Sonden-Konstante). Die Sonde ist ein Name, der in mindestens
# zwei konkurrierenden Fassungen desselben Waechters vorkommt.
PROBES = [
    ("UFT_FORMAT_ID_T_DEFINED",       "UFT_FMT_D64"),
    ("UFT_FORMAT_ENUM_DEFINED",       "UFT_FORMAT_ADF"),
    ("UFT_PLATFORM_T_DEFINED",        "UFT_PLATFORM_AMIGA"),
    ("UFT_PROTECTION_TYPE_T_DEFINED", "UFT_PROT_COPYLOCK"),
    ("UFT_SECTOR_STATUS_DEFINED",     "UFT_SECTOR_DELETED"),
]

# Gemessene Verteilungen, MF-559. Sonde -> {Wert: Anzahl Einheiten}.
# Nur Sonden, die HEUTE scharf sind, stehen hier. Aendert sich die
# Verteilung, will man es wissen.
ARMED_BASELINE: dict[str, dict[int, int]] = {
    "UFT_FORMAT_ADF":     {3: 7, 6: 1},
    "UFT_PLATFORM_AMIGA": {1: 1, 2: 1, 5: 1},
    "UFT_PROT_COPYLOCK":  {1: 1, 10: 1, 22: 1, 512: 1, 4096: 1},
}

ENUM_BODY = re.compile(r"enum[^{]*\{([^{}]*)\}", re.S)


def _find_gcc() -> str | None:
    for cand in (shutil.which("gcc"),
                 r"C:\Qt\Tools\mingw1310_64\bin\gcc.exe",
                 "/usr/bin/gcc"):
        if cand and Path(cand).exists():
            return cand
    return None


def _value_in(text: str, name: str) -> int | None:
    """Wert von `name` im praeprozessierten Text, ueber alle enums.

    `[^{}]*` und nicht `.*?`: der Rumpf darf keine Klammer enthalten. Die
    erste Fassung dieses Tors (MF-540) benutzte `.*?` mit re.S, verschluckte
    damit mehrere Enums am Stueck und fand den Namen an einer Position, die
    niemandem gehoert. Sie meldete daraufhin "alle Einheiten sehen dieselbe
    Zahl" — und der Rotbeweis feuerte nicht. Aufgefallen ist es nur, weil
    der Rotbeweis ZUERST gelaufen ist.
    """
    for m in ENUM_BODY.finditer(text):
        v = -1
        for raw in m.group(1).split(","):
            tok = raw.split("/*")[0].split("//")[0].strip()
            if not tok:
                continue
            if "=" in tok:
                lhs, rhs = tok.split("=", 1)
                lhs = lhs.strip()
                try:
                    v = int(rhs.strip(), 0)
                except ValueError:
                    continue
            else:
                lhs = tok
                v += 1
            if lhs == name:
                return v
    return None


def _users(repo: Path, probe: str) -> list[Path]:
    pat = re.compile(r"\b" + re.escape(probe) + r"\b")
    out: list[Path] = []
    for p in (repo / "src").rglob("*.c"):
        try:
            if pat.search(p.read_text(encoding="utf-8", errors="replace")):
                out.append(p)
        except OSError:
            continue
    return sorted(out)


def _incs(repo: Path) -> list[str]:
    out: list[str] = []
    for d in ("include", "include/uft", "include/uft/core",
              "include/uft/formats", "include/uft/flux",
              "include/uft/protection", "src", "."):
        out += ["-I", str(repo / d)]
    return out


def measure(repo: Path, probe: str, gcc: str,
            incs: list[str]) -> dict[int, int]:
    seen: dict[int, int] = {}
    for src in _users(repo, probe):
        try:
            r = subprocess.run([gcc, "-E", "-P", str(src)] + incs,
                               capture_output=True, text=True,
                               errors="replace", timeout=120)
        except (OSError, subprocess.TimeoutExpired):
            continue
        if not r.stdout:
            continue
        v = _value_in(r.stdout, probe)
        if v is None:
            continue
        seen[v] = seen.get(v, 0) + 1
    return seen


def check(repo: Path) -> list[str]:
    gcc = _find_gcc()
    if not gcc:
        return []          # keine Messung -> kein Urteil

    incs = _incs(repo)
    errors: list[str] = []

    for guard, probe in PROBES:
        seen = measure(repo, probe, gcc, incs)
        base = ARMED_BASELINE.get(probe)

        if base is None:
            if len(seen) > 1:
                parts = ", ".join(f"{v} ({n}x)"
                                  for v, n in sorted(seen.items()))
                errors.append(
                    f"{probe} (Waechter {guard}) bedeutet in gebautem Code "
                    f"{len(seen)} verschiedene Zahlen: {parts}. Die "
                    f"Include-Reihenfolge entscheidet still, welche gilt. "
                    f"Siehe docs/KNOWN_ISSUES.md ID-1/ID-2.")
            continue

        if seen != base:
            errors.append(
                f"{probe}: die Werteverteilung hat sich geaendert. Gemessen "
                f"{dict(sorted(seen.items()))}, eingefroren "
                f"{dict(sorted(base.items()))}. Entweder ist eine "
                f"Uebersetzungseinheit dazugekommen, eine Include-Zeile "
                f"umsortiert worden, oder jemand hat die Enums "
                f"zusammengelegt. Alle drei will man wissen — die Zahl in "
                f"scripts/audit_format_id_drift.py::ARMED_BASELINE gehoert "
                f"dann nachgezogen.")

    return errors


def main() -> int:
    repo = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    gcc = _find_gcc()
    if not gcc:
        print("kein gcc auffindbar — nicht gemessen, kein Urteil")
        return 0

    incs = _incs(repo)
    print(f"Bedeutungs-Drift je Uebersetzungseinheit (root={repo}):\n")
    for guard, probe in PROBES:
        seen = measure(repo, probe, gcc, incs)
        mark = ">>> SCHARF" if len(seen) > 1 else "ok"
        print(f"  {probe:24s} {dict(sorted(seen.items()))}  {mark}")

    errs = check(repo)
    if errs:
        print("\nBEFUNDE:")
        for e in errs:
            print(f"  {e}")
        return 1
    print("\nOK: alle Verteilungen wie gemessen eingefroren")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
