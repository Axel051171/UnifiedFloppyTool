#!/usr/bin/env python3
"""korpus_zensus.py — Stufe 3: Welche Versionen liegen wirklich im
Korpus? Lücke je Variante = Beschaffungsauftrag.

  korpus_zensus.py <verzeichnis> [weitere ...] [-o work/korpus.json]

Liest je Datei die Erkennungsbytes und ordnet Format + Version zu,
nach der Tabelle in config.json ("erkennungen"). Unbekanntes wird als
unbekannt gezählt, nie geraten. Ergebnis: Histogramm Format×Version
mit Dateilisten — die Datenquelle für die Tier-Regel
„T1b nur mit Fixture je kursierender Version".
"""
import json, os, struct, sys
from collections import defaultdict

HIER = os.path.dirname(os.path.abspath(__file__))
CFG = json.load(open(os.path.join(HIER, "..", "config.json"),
                     encoding="utf-8"))


def erkenne(pfad):
    """-> (format, version, detail) oder (None, None, grund)"""
    try:
        with open(pfad, "rb") as f:
            kopf = f.read(64)
    except OSError as e:
        return None, None, f"unlesbar: {e}"
    for erk in CFG["erkennungen"]:
        off = erk.get("offset", 0)
        magic = erk["magic"].encode() if isinstance(erk["magic"], str) \
            else bytes(erk["magic"])
        if kopf[off:off + len(magic)] != magic:
            continue
        fmt = erk["format"]
        # Versionsableitung, wenn definiert
        v = erk.get("version")
        if v is None:
            return fmt, erk.get("version_fest", "?"), ""
        vb = kopf[v["offset"]:v["offset"] + v.get("len", 1)]
        if not vb:
            return fmt, "?", "Header zu kurz"
        wert = vb[0] if v.get("len", 1) == 1 else \
            struct.unpack("<H", vb[:2])[0]
        name = str(v.get("map", {}).get(str(wert), wert))
        return fmt, name, f"rohwert={wert}"
    # Endung als schwaches Indiz, ausdrücklich als solches markiert
    ext = os.path.splitext(pfad)[1].lower().lstrip(".")
    if ext in CFG.get("endung_nur_hinweis", []):
        return ext.upper(), "?", "nur Endung, kein Magic — pruefen"
    return None, None, "kein bekanntes Magic"


def main():
    args = [a for a in sys.argv[1:] if a != "-o" and
            (a == sys.argv[1] or sys.argv[sys.argv.index(a) - 1] != "-o")]
    if not args:
        print(__doc__); return 2
    out = os.path.join(HIER, "..", "work", "korpus.json")
    if "-o" in sys.argv:
        out = sys.argv[sys.argv.index("-o") + 1]

    hist = defaultdict(lambda: defaultdict(list))
    unbekannt = []
    n = 0
    for wurzel in args:
        for dp, dn, fn in os.walk(wurzel):
            dn[:] = [d for d in dn if d != ".git"]
            for f in fn:
                p = os.path.join(dp, f)
                n += 1
                fmt, ver, detail = erkenne(p)
                if fmt:
                    hist[fmt][str(ver)].append(
                        os.path.relpath(p, wurzel))
                else:
                    unbekannt.append([os.path.relpath(p, wurzel),
                                      detail])
    ergebnis = {
        "dateien_gesamt": n,
        "abdeckung": {f: {v: sorted(l)[:20] for v, l in vs.items()}
                      for f, vs in hist.items()},
        "unbekannt": unbekannt[:40],
        "hinweis": "Abdeckung je Version; fehlende Versionen aus dem "
                   "Dossier sind Beschaffungsauftraege.",
    }
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w", encoding="utf-8") as f:
        json.dump(ergebnis, f, ensure_ascii=False, indent=1)
    print(f"OK: {n} Dateien, {len(hist)} Formate erkannt, "
          f"{len(unbekannt)} unbekannt -> {out}")
    for fmt, vs in sorted(hist.items()):
        print(f"  {fmt}: " + ", ".join(
            f"{v}×{len(l)}" for v, l in sorted(vs.items())))
    return 0


if __name__ == "__main__":
    sys.exit(main())
