#!/usr/bin/env python3
"""Was der Kopierschutz-Katalog IST, gegen das, was ueber ihn steht (MF-557)

── Warum es dieses Tor gibt ─────────────────────────────────────────────

`CLAUDE.md` und `README.md` fuehren "55+ Kopierschutz-Verfahren" als
**Kernfunktion**. Gemessen:

    Dateien in src/protection/            33
    Funktionen mit uft_-Praefix          369
    davon von ausserhalb gerufen           3   (uft_longtrack_get_def,
                                                uft_longtrack_type_name,
                                                uft_prot_config_init)
    davon von irgendeinem Test beruehrt   16
    von keinem Test beruehrt             353

Der Katalog ist also nicht bloss unverdrahtet, sondern **unbelegt**. Ihn
anzuschliessen hiesse, 353 ungeprueste Funktionen an ein forensisches
Urteil zu haengen — genau die Lage, aus der die fuenf fabrizierten Parser
kamen (FMT-2/3/10/11/12).

Die Oberflaeche sagt das bereits von sich aus
(`src/gui/ProtectionAnalysisWidget.cpp`):

    "Der eigentliche Erkenner (src/protection/, u.a.
     uft_protection_detect.c und uft_protection_classify.c) wird von
     NIEMANDEM aufgerufen ... ihn anzuschliessen, ohne ihn geprueft zu
     haben, waere derselbe Fehler noch einmal."

Was fehlte, war die ZAHL. Ohne sie liest sich "wird von niemandem
aufgerufen" wie ein Detail; mit ihr wie das, was es ist.

── Was das Tor prueft ───────────────────────────────────────────────────

Es misst die drei Zahlen neu und vergleicht sie mit dem, was in
`docs/BACKLOG.md` steht. Weicht die Doku ab, ist entweder etwas verdrahtet
worden (dann gehoert die Zahl aktualisiert) oder die Doku ist gedriftet.
Beides will man wissen.

Es verlangt NICHT, dass die Zahlen klein bleiben. Wer den Katalog
verdrahtet und prueft, aendert die Zahl — das Tor sagt dann nur, dass die
Doku nachziehen muss.

── Grenzen ──────────────────────────────────────────────────────────────

"Von einem Test beruehrt" heisst: der Name kommt in `tests/` vor. Das ist
eine Obergrenze, keine Aussage ueber Pruefqualitaet — ein Test, der eine
Funktion ruft und ihr Ergebnis nicht prueft, zaehlt hier mit. Die echte
Zahl der geprueften Funktionen ist also hoechstens 16, eher weniger.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

FUNC = re.compile(r"^[a-z][\w \t*]*?\b(uft_[a-z0-9_]+)\s*\(", re.M)


def measure(repo: Path) -> dict[str, int]:
    prot = repo / "src" / "protection"
    if not prot.exists():
        return {}

    names: set[str] = set()
    files = sorted(prot.rglob("*.c"))
    for p in files:
        try:
            names |= set(FUNC.findall(
                p.read_text(encoding="utf-8", errors="replace")))
        except OSError:
            continue

    def _corpus(root: Path, skip_prot: bool) -> str:
        out = []
        for sub in ("src", "tests") if not skip_prot else ("src",):
            d = root / sub
            if not d.exists():
                continue
            for q in d.rglob("*.c"):
                if skip_prot and "protection" in q.as_posix():
                    continue
                try:
                    out.append(q.read_text(encoding="utf-8", errors="replace"))
                except OSError:
                    pass
            for q in d.rglob("*.cpp"):
                if skip_prot and "protection" in q.as_posix():
                    continue
                try:
                    out.append(q.read_text(encoding="utf-8", errors="replace"))
                except OSError:
                    pass
        return "\n".join(out)

    outside = _corpus(repo, skip_prot=True)

    tests_text = []
    for q in (repo / "tests").rglob("*.c"):
        try:
            tests_text.append(q.read_text(encoding="utf-8", errors="replace"))
        except OSError:
            pass
    for q in (repo / "tests").rglob("*.cpp"):
        try:
            tests_text.append(q.read_text(encoding="utf-8", errors="replace"))
        except OSError:
            pass
    tests_all = "\n".join(tests_text)

    called = sum(1 for n in names
                 if re.search(r"\b" + re.escape(n) + r"\b", outside))
    tested = sum(1 for n in names
                 if re.search(r"\b" + re.escape(n) + r"\b", tests_all))

    return {"files": len(files), "funcs": len(names),
            "called": called, "tested": tested}


CLAIM = re.compile(
    r"(\d+)\s*Dateien,\s*(\d+)\s*Funktionen,\s*(\d+)\s*von aussen gerufen,"
    r"\s*(\d+)\s*von einem Test beruehrt")


def check(repo: Path) -> list[str]:
    m = measure(repo)
    if not m:
        return []
    doc = repo / "docs" / "BACKLOG.md"
    if not doc.exists():
        return []
    text = doc.read_text(encoding="utf-8", errors="replace")
    hit = CLAIM.search(text)
    if not hit:
        return [f"docs/BACKLOG.md: der C1-Eintrag nennt die Kopierschutz-"
                f"Zahlen nicht mehr im erwarteten Wortlaut. Gemessen: "
                f"{m['files']} Dateien, {m['funcs']} Funktionen, "
                f"{m['called']} von aussen gerufen, {m['tested']} von einem "
                f"Test beruehrt."]
    claimed = tuple(int(x) for x in hit.groups())
    real = (m["files"], m["funcs"], m["called"], m["tested"])
    if claimed != real:
        return [f"docs/BACKLOG.md C1 behauptet {claimed}, gemessen ist "
                f"{real} (Dateien, Funktionen, von aussen gerufen, von "
                f"einem Test beruehrt). Wurde etwas verdrahtet oder "
                f"geprueft, gehoert die Zahl aktualisiert — sonst ist die "
                f"Doku gedriftet."]
    return []


def main() -> int:
    repo = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    m = measure(repo)
    print(f"Kopierschutz-Katalog (root={repo}):")
    for k, v in m.items():
        print(f"  {k:8s}: {v}")
    errs = check(repo)
    for e in errs:
        print(f"  BEFUND: {e}")
    return 1 if errs else 0


if __name__ == "__main__":
    raise SystemExit(main())
