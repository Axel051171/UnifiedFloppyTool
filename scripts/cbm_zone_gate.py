#!/usr/bin/env python3
"""Jede CBM-Zonentabelle im Baum muss mit der SSOT uebereinstimmen.

Warum es das gibt (ARCH-7, MF-434 / MF-459). Sektoren pro Spur sind eine
Eigenschaft des LAUFWERKS, nicht des Dateiformats: ein 1541 legt 21 Sektoren
auf Spur 1, egal ob das Ergebnis als D64, G64 oder NIB gespeichert wird. Der
Fakt stand trotzdem 24-mal im Baum, in drei unvertraeglichen Indexkonventionen
(1-basiert mit fuehrender 0, 1-basiert ohne, 0-basiert).

MF-434 gab dem Fakt ein Zuhause: `include/uft/formats/cbm/uft_cbm_geometry.h`
beschreibt vier Familien als Zonengrenzen statt als Arrays. Drei Dateien wurden
migriert; **21 Kopien blieben stehen**, weil mehrere davon in Lesern liegen,
die ARCH-6 ohnehin zusammenfuehren will — sie vorher zu migrieren waere Arbeit
an Code, der verschwinden soll.

Dieser Waechter schliesst die Luecke dazwischen. Er migriert nichts, er
**prueft**: jede Tabelle, die er findet, wird gegen die SSOT gerechnet. Damit
wird aus "24 Kopien, die zufaellig uebereinstimmen" ein "24 Kopien, deren
Uebereinstimmung nachgewiesen ist" — und eine spaetere Migration ist
verifizierbar statt riskant.

Die SSOT-Werte werden aus `src/formats/cbm/uft_cbm_geometry.c` GELESEN, nicht
hier wiederholt. Ein Waechter mit eigener Kopie der Wahrheit waere die 25.

Erkannte Konventionen, je Familie:
  1-basiert mit fuehrender 0   [0, 21, 21, ...]   Index == Spurnummer
  1-basiert ohne fuehrende 0   [21, 21, ...]      Index+1 == Spurnummer
  0-basiert                    [21, 21, ...]      Index+1 == Spurnummer
Die letzten beiden sind textgleich; unterschieden wird ueber die Laenge und
darueber, welche Lesart aufgeht. Geht KEINE auf, ist das der Befund.

Grenze, ausdruecklich: erkannt werden Array-Literale mit mindestens 15 Zahlen,
die 21/19/18/17 enthalten. Eine Tabelle, die zur Laufzeit berechnet oder aus
einer Datei geladen wird, faellt hier nicht auf.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

SKIP_DIRS = {".git", "build", "proto", ".claude", "release", "debug"}
SSOT_C = "src/formats/cbm/uft_cbm_geometry.c"

# Laufwerke, die aehnlich aussehen und keine sind.
#
# Die Apple Lisa "Twiggy" (FileWare) ist ebenfalls zonenweise aufgebaut und
# faengt bei 22 Sektoren an — sie hat mit Commodore nichts zu tun: 46 Zylinder,
# 512-Byte-Sektoren, andere Zonengrenzen. ARCH-7 nennt sie ausdruecklich als
# eine der beiden echten Abweichungen. Sie hier zu melden waere ein
# Falschbefund, sie stillschweigend zu erklaeren waere schlimmer.
FOREIGN_DRIVES: list[re.Pattern[str]] = [
    re.compile(r"^src/formats/lisa/"),
]

_ZONE_ROW = re.compile(r"\{\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\}")
_ZONE_BLOCK = re.compile(
    r"static const cbm_zone_t (zones_\w+)\[\]\s*=\s*\{(.*?)\n\};", re.S)
_ARRAY = re.compile(r"\{[^{}]{20,900}\}", re.S)
# Deklaration unmittelbar vor der Klammer: Typ, Name, optionale Laenge.
_DECL = re.compile(
    r"(?:static\s+)?(?:const\s+)?(\w[\w 	]*?)\s+(\w+)\s*\[\s*(\d*)\s*\]\s*=\s*$")

_BLOCK = re.compile(r"/\*.*?\*/", re.S)
_LINE = re.compile(r"//[^\n]*")


def strip_comments(text: str) -> str:
    def blank(m):
        return "".join(c if c == chr(10) else " " for c in m.group(0))
    return _LINE.sub(blank, _BLOCK.sub(blank, text))


def load_ssot(repo: Path) -> dict[str, list[int]]:
    """Sektoren pro Spur je Familie, 1-basiert: index 0 ist unbenutzt."""
    txt = strip_comments((repo / SSOT_C).read_text(encoding="utf-8", errors="replace"))
    out: dict[str, list[int]] = {}
    for m in _ZONE_BLOCK.finditer(txt):
        name = m.group(1)
        rows = [tuple(int(x) for x in r.groups()) for r in _ZONE_ROW.finditer(m.group(2))]
        if not rows:
            continue
        last = max(r[1] for r in rows)
        per = [0] * (last + 1)
        for first, lastt, sectors, _speed, _gap in rows:
            for t in range(first, lastt + 1):
                per[t] = sectors
        out[name] = per
    return out


def candidate_arrays(repo: Path):
    """(datei, zeile, zahlen) fuer jedes Array, das nach Zonentabelle aussieht."""
    for base in ("src", "include"):
        root = repo / base
        if not root.is_dir():
            continue
        for p in sorted(root.rglob("*")):
            if p.suffix.lower() not in {".c", ".cpp", ".h", ".hpp"}:
                continue
            if any(s in p.parts for s in SKIP_DIRS):
                continue
            rel = str(p.relative_to(repo)).replace("\\", "/")
            if rel == SSOT_C:
                continue
            if any(f.search(rel) for f in FOREIGN_DRIVES):
                continue
            clean = strip_comments(p.read_text(encoding="utf-8", errors="replace"))
            for m in _ARRAY.finditer(clean):
                nums = [int(x) for x in re.findall(r"\b\d+\b", m.group(0))]
                if len(nums) < 15:
                    continue
                if not {21, 19, 18, 17} <= set(nums):
                    continue
                if nums.count(21) < 5:
                    continue
                head = clean[:m.start()].rstrip()
                decl = _DECL.search(head[-160:])
                ident = decl.group(2) if decl else None
                ctype = " ".join(decl.group(1).split()) if decl else None
                yield rel, clean[:m.start()].count(chr(10)) + 1, nums, ident, ctype


def explains(nums: list[int], ssot: dict[str, list[int]]) -> str | None:
    """Name der Lesart, die diese Zahlenfolge erklaert — oder None."""
    for fam, per in ssot.items():
        maxt = len(per) - 1
        # 1-basiert mit fuehrender 0: nums[t] == per[t]
        if nums and nums[0] == 0:
            n = min(len(nums) - 1, maxt)
            if n >= 30 and all(nums[t] == per[t] for t in range(1, n + 1)):
                return f"{fam}, 1-basiert mit fuehrender 0"
        # 1-basiert ohne fuehrende 0: nums[i] == per[i+1]
        n = min(len(nums), maxt)
        if n >= 30 and all(nums[i] == per[i + 1] for i in range(n)):
            return f"{fam}, 1-basiert ohne fuehrende 0"
        # D71: zwei Seiten hintereinander, mit oder ohne fuehrende 0.
        # `d71_sectors[71] = { 0, <35 Spuren Seite 0>, <35 Spuren Seite 1> }`
        # ist die haeufigste Form im Baum — die fuehrende 0 verschiebt die
        # Haelften, weshalb sie vor dem Teilen abgezogen werden muss.
        body = nums[1:] if (nums and nums[0] == 0) else nums
        if len(body) >= 2 * 30 and len(body) % 2 == 0:
            half = len(body) // 2
            a, b = body[:half], body[half:]
            if a == b:
                n = min(half, maxt)
                if n >= 30 and all(a[i] == per[i + 1] for i in range(n)):
                    lead = " mit fuehrender 0" if nums[0] == 0 else ""
                    return f"{fam}, zwei Seiten hintereinander{lead}"
    return None


def check(repo: Path) -> list[str]:
    ssot = load_ssot(repo)
    if not ssot:
        return [f"{SSOT_C}: keine Zonentabellen gefunden — die SSOT wurde "
                f"umgebaut, und dieser Waechter liest sie nicht mehr"]

    errors: list[str] = []
    for rel, ln, nums, ident, ctype in candidate_arrays(repo):
        if explains(nums, ssot) is None:
            head = ", ".join(str(x) for x in nums[:12])
            errors.append(
                f"{rel}:{ln} sieht aus wie eine CBM-Zonentabelle ({len(nums)} "
                f"Zahlen: {head}, ...), stimmt aber mit keiner Familie und "
                f"keiner Indexkonvention der SSOT ueberein "
                f"(include/uft/formats/cbm/uft_cbm_geometry.h). Entweder ist "
                f"die Tabelle falsch, oder die SSOT kennt diese Familie nicht "
                f"— beides gehoert geklaert, bevor jemand daraus eine "
                f"Off-by-one macht (ARCH-7)")
    # Regel B: derselbe Bezeichner, zwei Header, verschiedene Typen.
    #
    # MF-459 gefunden: `c64_sectors_per_track` steht als
    # `static const int   c64_sectors_per_track[]`   in
    # include/uft/protection/uft_c64_protection.h und als
    # `static const uint8_t c64_sectors_per_track[41]` in
    # include/uft/uft_cbm_gcr.h. Geprueft: derzeit zieht keine
    # Uebersetzungseinheit beide, die Kollision ist also latent — genau wie
    # UFT_BIG_ENDIAN vor MF-455, und genau wie UFT_SCP_SIGNATURE in ARCH-2,
    # das viermal existierte, einmal mit anderem Typ. Wer beide Header
    # einbindet, bekommt einen Redefinitionsfehler oder, schlimmer, je nach
    # Include-Reihenfolge einen anderen Elementtyp.
    by_ident: dict[str, set[tuple[str, str]]] = {}
    for rel, ln, nums, ident, ctype in candidate_arrays(repo):
        if ident:
            by_ident.setdefault(ident, set()).add((ctype or "?", rel))
    for ident, uses in sorted(by_ident.items()):
        # Nur wenn mindestens EINE Definition in einem Header steht. `static
        # const` in einer .c ist dateilokal — zwei .c-Dateien duerfen denselben
        # Namen tragen, das ist keine Kollision. Erst geprueft, dann gemeldet:
        # die erste Fassung dieser Regel meldete drei Faelle, zwei davon
        # Fehlalarm.
        in_header = [f for _, f in uses if f.endswith((".h", ".hpp"))]
        types = {t for t, _ in uses}
        if in_header and len(uses) > 1 and len(types) > 1:
            where = "; ".join(f"{t} in {f}" for t, f in sorted(uses))
            errors.append(
                f"'{ident}' ist mehrfach mit VERSCHIEDENEN Typen definiert: "
                f"{where}. Wer beide Header einbindet, bekommt einen "
                f"Redefinitionsfehler oder je nach Reihenfolge einen anderen "
                f"Elementtyp — dieselbe Klasse wie ARCH-2. Einen Namen, eine "
                f"Definition")

    return errors


def main() -> int:
    repo = Path(__file__).resolve().parent.parent
    ssot = load_ssot(repo)
    rows = list(candidate_arrays(repo))
    print(f"SSOT-Familien aus {SSOT_C}: {', '.join(sorted(ssot))}")
    print(f"Zonentabellen im Baum      : {len(rows)}")
    ok = 0
    for rel, ln, nums, ident, ctype in rows:
        why = explains(nums, ssot)
        if why:
            ok += 1
            if "--verbose" in sys.argv:
                print(f"    OK  {rel}:{ln}  ({len(nums)}) — {why}")
    print(f"davon durch die SSOT erklaert: {ok}")
    errs = check(repo)
    for e in errs:
        print(f"  {e}")
    return 1 if errs else 0


if __name__ == "__main__":
    raise SystemExit(main())
