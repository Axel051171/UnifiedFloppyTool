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



# ── Zweite Stufe: Attributionen im Fliesstext (MF-636) ───────────────────
#
# Der SPDX-Zensus oben sieht nur, was einen SPDX-Bezeichner traegt. SCOUT-23
# hing genau daran vorbei: vier Dateien erklaerten im KOPFKOMMENTAR eine
# Ableitung — "Based on nibtools by Pete Rittwage" — und trugen keinen
# fremden SPDX. Der Zensus aus MF-620/621 meldete sie deshalb nicht, und
# die Frage "ist das ein Port?" blieb ein Jahr lang ungestellt, waehrend
# der Upstream unter GPL-3 stand.
#
# Eine Ableitungserklaerung ist eine RECHTLICHE Aussage. Sie gehoert
# gesehen, unabhaengig davon, ob jemand daneben einen Bezeichner gesetzt
# hat. Diese Stufe meldet sie — als LISTE, nicht als Fehler: eine
# Attribution ist nichts Verbotenes, sie ist etwas Entscheidungsbeduerftiges.
ATTRIBUTION = re.compile(
    r"(based\s+on|adapted\s+from|derived\s+from|port(?:ed)?\s+of|"
    r"portiert\s+aus|nach\s+dem\s+Vorbild|taken\s+from|"
    r"originally\s+(?:by|from)|reference:|referenz:|"
    r"verhalten\s+nach)\s+(.{0,60})",
    re.IGNORECASE)
# MF-651: `reference:` fehlte hier — und das war die fuenfte Auflage
# desselben Fehlers wie MF-567/578/598/633: eine Aufzaehlung bekannter
# Faelle, die still veraltet. Gemessen: 32 Dateien tragen
# "Reference: libdsk drv*.c", nennen also eine fremde Codebasis, und
# KEINE davon war dem Zensus sichtbar — der Pruefer, der Attributionen
# finden soll, konnte die groesste zusammenhaengende Gruppe im Baum
# nicht sehen. Die Formulierung war neu, nicht der Sachverhalt.

# Was KEINE Fremd-Attribution ist: Verweise auf den eigenen Baum und auf
# Spezifikationen. "Based on the D88 spec" begruendet kein Urheberrecht.
ATTRIB_HARMLOS = re.compile(
    r"^(the\s+)?(spec|specification|documentation|docs?|format|layout|"
    r"standard|MF-\d+|uft_|src/|include/|docs/)", re.IGNORECASE)


def scan_attributions(repo: pathlib.Path) -> list[tuple[str, int, str]]:
    """Fliesstext-Attributionen in Quellkoepfen — Liste, kein Urteil."""
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
            # Nur der Kopf: eine Ableitungserklaerung steht oben, nicht in
            # Zeile 800. Das haelt die Liste bei den rechtlich gemeinten
            # Aussagen statt bei jedem "based on" im Prosatext.
            kopf = "\n".join(text.splitlines()[:60])
            for m in ATTRIBUTION.finditer(kopf):
                quelle = m.group(2).strip().strip("*/ ").strip()
                if not quelle or ATTRIB_HARMLOS.match(quelle):
                    continue
                zeile = kopf[:m.start()].count("\n") + 1
                treffer.append((rel, zeile, "%s %s" % (m.group(1), quelle)))
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


# ── Die schaerfere Frage: PORT ohne SPDX-Kopf (MF-697) ──────────────────
#
# `scan_attributions` meldet jede Fremd-Attribution — 171 Stueck, und das
# ist als Uebersicht richtig. Fuer eine Entscheidung ist es zu grob:
# „Reference: libdsk drv*.c" begruendet keine Ableitung, „Full port of AIR
# IPFReader.cs to C" schon.
#
# Diese Stufe fragt nur nach der ENGEREN Klasse: wer im Kopf eine
# **Portierung oder Ableitung von fremdem CODE** erklaert, muss einen
# SPDX-Kopf tragen. `CONTRIBUTING.md` §Licensing sagt das seit MF-580
# („Ported or adapted code keeps its origin's licence and names it") — es
# stand nur nirgends unter Beobachtung.
#
# Gemessen beim ersten Lauf: 5 solche Erklaerungen, **keine** mit
# SPDX-Kopf. Zwei davon (a8rawconv-Ports) sind mit MF-697 gesetzt; die
# drei AIR-Dateien nennen GPL-3.0, und GPL-3.0 steht NICHT in `ERLAUBT`.
# Das ist eine Eigentuemer-Entscheidung (LIZ-2), kein Skriptfehler —
# darum LISTE, nicht Tor.
#
# Der Detektor ist bewusst eng. Eine erste, weitere Fassung meldete 20
# Dateien und war zu einem Viertel Artefakt: „a compact identifier
# derived from timing histograms" (Prosa ueber einen Fingerabdruck),
# „uint16_t expected_len; /* derived from N */" (ein Feldkommentar) und
# ein deutsches „mit" als Lizenztreffer. Ein Detektor, der Prosa fuer
# eine Rechtsaussage haelt, erzeugt eine Liste, die niemand liest.
PORT_ERKLAERUNG = re.compile(
    r"(full\s+port\s+of|C99\s+port\s+of|port\s+of\s+\w+[\w.]*['’]?s?\s|"
    r"portiert\s+aus|adapted\s+from\s+the\s+source|"
    r"uebernommen\s+aus)\s*(.{0,70})", re.IGNORECASE)


def scan_ports_ohne_spdx(repo: pathlib.Path) -> list[tuple[str, str, bool]]:
    """(Datei, Erklaerung, hat_SPDX) je Port-Erklaerung im Kopf."""
    aus = []
    for basis in ("src", "include"):
        wurzel = repo / basis
        if not wurzel.is_dir():
            continue
        for pfad in sorted(wurzel.rglob("*")):
            if pfad.suffix not in (".c", ".h", ".cpp", ".hpp"):
                continue
            rel = pfad.relative_to(repo).as_posix()
            if rel.startswith(AUSGENOMMEN):
                continue
            try:
                kopf = pfad.read_text(encoding="utf-8", errors="replace")[:4000]
            except OSError:
                continue
            m = PORT_ERKLAERUNG.search(kopf)
            if not m:
                continue
            aus.append((rel, m.group(0).strip()[:75],
                        bool(SPDX.search(kopf))))
    return aus


def main() -> int:
    attrib = scan_attributions(ROOT)
    print("Fliesstext-Attributionen (MF-636, Liste, kein Fehler): %d" % len(attrib))
    for f, ln, q in attrib:
        print("  %s:%d  %s" % (f, ln, q))
    print()
    ports = scan_ports_ohne_spdx(ROOT)
    ohne = [p for p in ports if not p[2]]
    print("Port-Erklaerungen im Kopf: %d, davon OHNE SPDX: %d "
          "(MF-697, Liste — die Lizenzfrage ist Eigentuemer-Sache)"
          % (len(ports), len(ohne)))
    for f, erk, hat in ports:
        print("  %s %-52s %s" % ("SPDX " if hat else "OHNE ", f, erk))
    print()
    rows = scan(ROOT)
    print("SPDX ausserhalb der Politik: %d" % len(rows))
    for f, ln, k in rows:
        print("  %-60s:%-5d %s" % (f, ln, k))
    return 1 if rows else 0


if __name__ == "__main__":
    sys.exit(main())
