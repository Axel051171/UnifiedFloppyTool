#!/usr/bin/env python3
"""SPDX-Bezeichner ausserhalb der erlaubten Menge (MF-621).

── Warum es dieses Tor gibt ─────────────────────────────────────────────

`src/formats/retro_image/uft_retro_image_detect.c` trug
`GPL-3.0-or-later` — in einem Projekt, dessen `LICENSE` der reine
GPL-2-Text ohne „or later"-Klausel ist. GPL-3 kombiniert nicht mit
GPL-2; die Lizenzmatrix des Projekts sagt das ausdruecklich.

Gefunden wurde das **nicht von einem Tor**, sondern nebenbei: beim
Probelauf eines gerade erst reparierten Scout-Werkzeugs gegen den
eigenen Baum (MF-620). Ein Zufallsfund ist kein Verfahren.

Die Politik steht in `CONTRIBUTING.md` §Licensing:

  * UFT-eigener Code:  GPL-2.0-or-later
  * portierter Code:   die Lizenz seines Ursprungs, benannt

── Was gemeldet wird ────────────────────────────────────────────────────

Jeder SPDX-Bezeichner in `src/` oder `include/`, der nicht in ERLAUBT
steht. Die Menge ist bewusst klein: was hinzukommt, ist eine
Eigentuemer-Entscheidung und gehoert hier eingetragen, nicht im
Vorbeigehen ergaenzt.

Nicht geprueft: Verzeichnisse mit eingekauftem Fremdcode
(`src/samdisk`, `src/a8rawconv`), die ihre eigene Lizenz mitbringen und
nicht gebaut werden.
"""
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]

# Mit GPL-2.0 vertraeglich, oder gemeinfrei-aequivalent.
ERLAUBT = {
    "GPL-2.0-or-later",          # die Vorgabe fuer eigenen Code
    "GPL-2.0",                   # Altbestand, gleichbedeutend nur enger
    "LGPL-2.1-or-later",
    "MIT",
    "BSD-2-Clause",
    "BSD-3-Clause",
    "ISC",
    "Zlib",
    "Unlicense",                 # gemeinfrei-aequivalent
    "CC0-1.0",
    "LicenseRef-PublicDomain-xDMS",   # formlose PD-Erklaerung, MF-614
}

AUSGENOMMEN = ("src/samdisk", "src/a8rawconv")

SPDX = re.compile(r"SPDX-License-Identifier:\s*([A-Za-z0-9.+\-]+)")


def scan(repo: pathlib.Path) -> list[tuple[str, int, str]]:
    treffer = []
    for basis in ("src", "include"):
        wurzel = repo / basis
        if not wurzel.is_dir():
            continue
        for p in sorted(wurzel.rglob("*")):
            if p.suffix.lower() not in (".c", ".cpp", ".h", ".hpp", ".cc"):
                continue
            rel = p.relative_to(repo).as_posix()
            if any(rel.startswith(a) for a in AUSGENOMMEN):
                continue
            try:
                text = p.read_text(encoding="utf-8", errors="replace")
            except OSError:
                continue
            for m in SPDX.finditer(text):
                kennung = m.group(1)
                if kennung not in ERLAUBT:
                    zeile = text[:m.start()].count("\n") + 1
                    treffer.append((rel, zeile, kennung))
    return treffer


def check(repo) -> list:
    """Schnittstelle fuer check_consistency.py."""
    try:
        rows = scan(pathlib.Path(repo))
    except Exception as exc:              # noqa: BLE001
        return ["SPDX-Politik nicht pruefbar: %s" % exc]
    return ["SPDX ausserhalb der Politik: %s:%d traegt `%s` — erlaubt ist "
            "fuer eigenen Code GPL-2.0-or-later (CONTRIBUTING.md "
            "§Licensing). Neue Bezeichner gehoeren in ERLAUBT in "
            "scripts/audit_spdx_policy.py, und das ist eine "
            "Eigentuemer-Entscheidung." % (f, ln, k)
            for f, ln, k in rows]


def main() -> int:
    rows = scan(ROOT)
    print("SPDX ausserhalb der Politik: %d" % len(rows))
    for f, ln, k in rows:
        print("  %-60s:%-5d %s" % (f, ln, k))
    return 1 if rows else 0


if __name__ == "__main__":
    sys.exit(main())
