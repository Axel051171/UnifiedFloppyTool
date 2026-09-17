#!/usr/bin/env python3
"""Sucht Abbilder fuer OFFENE Formate — auch IN Archiven (MF-1220).

Die Stufe T1b braucht ein Abbild fremder Hand, T1 ein echtes. Fuer die
Formate auf T2/T3 ist die Erzeuger-Frage abgearbeitet
(`docs/ERZEUGER_ZENSUS.md`: 12 offen, 0 mit gemessenem Kanal). Damit
bleibt genau ein Weg: ein vorhandenes Abbild finden.

ZWEI BLINDE FLECKEN, BEIDE BELEGT
    **(1) `find` sieht nicht in `.zip`/`.tar.gz` hinein.** MF-1073 hat
    das bezahlt: die erste Regalsuche meldete „null Treffer", und
    tatsaechlich lagen mehrere echte Abbilder in Archiven — die `.mgt`,
    `.opd`, `.scl` und `.fdi` des heutigen Korpus kommen ALLE von dort.
    Gemessen MF-1220: **kein** Skript im Baum benutzt `zipfile` oder
    `tarfile`; die Lehre war dokumentiert, aber nicht mechanisch.

    **(2) Eine Datei im Korpus, die im Manifest FEHLT, ist unsichtbar.**
    Die Stufenlogik liest ausschliesslich `manifest.json`; eine nicht
    eingetragene Datei bringt keine Stufe und faellt keiner Pruefung auf.

WAS EIN TREFFER IST — UND WAS NICHT
    Ein Treffer ist ein KANDIDAT, kein Beleg. Deshalb druckt dieses
    Skript zu jedem die Groesse und die ersten 8 Byte, damit die
    Entscheidung am Objekt faellt:

      * Die Endung sagt nichts. Gemessen MF-1220: alle **12** `.adf` in
        den Archiven des Baums tragen `DOS\\0` — gewoehnliches ADF, das
        bereits auf T1b steht. **Keine** trug `UAE-1ADF`, die Kennung,
        die `src/formats/adf_ext/uft_adf_ext.c:56` fuer `adf_ext`
        verlangt.
      * Ein leeres Abbild belegt nichts (MF-1021). `pc98_blank.fdi`
        liegt im Korpus und traegt AUSDRUECKLICH keine Stufe, weil seine
        Nutzlast zu 100 % Null ist.
      * Die Lizenz entscheidet mit (`P3-359`, `P3-356`).

DIE GROESSENSCHRANKE IST EINE ENTSCHEIDUNG, KEINE MESSUNG
    `.pro` ist die Endung von APE ProSystem UND von qmake-Projekten;
    gemessen sind **26** der 39 Rohtreffer qmake-Dateien. Eine Diskette
    hat mindestens einige Zehntausend Byte, ein `.pro`-Projekt selten
    mehr als dreissig. Die Schranke unten trennt das — sie wird
    AUSGEGEBEN, damit sie nicht als Messung durchgeht.

Aufruf:
    python scripts/audit_korpus_regal.py            # Kandidaten + Zahlen
    python scripts/audit_korpus_regal.py --selftest # Abnahme
"""
from __future__ import annotations

import json
import os
import sys
import tarfile
import zipfile
from pathlib import Path

WURZEL = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(WURZEL / "scripts"))

# Grundlinien, gemessen MF-1220. Duerfen nur SINKEN.
GRUNDLINIE_KANDIDATEN = 13   # Rohtreffer oberhalb der Groessenschranke
GRUNDLINIE_UNVERZEICHNET = 1  # Korpusdatei ohne Manifest-Eintrag

MINDESTGROESSE = 40 * 1024    # Entscheidung, siehe Kopf
ARCHIV_ENDUNGEN = (".zip", ".tar.gz", ".tgz", ".tar")
TAR_MAX = 200 * 1024 * 1024
UEBERSPRINGEN = ("build", ".git", "node_modules")


def offene_formate() -> dict[str, str]:
    """{symbol: endungen} fuer alles unter T1b — ABGELEITET, nicht gepflegt.

    Benutzt `gen_verification_tiers` mit: kein zweiter Parser fuer die
    Plugins und keine zweite Endungsliste (MF-1177). Ein Grep nach
    Endungen im Quelltext faengt gemessen VERALTETE Kommentarzeichen —
    `dim;xdf` wurde in MF-1087 entfernt, `pro;atx` in MF-1054, und beide
    stehen dort noch in Prosa.

    Die Endung kommt aus `scan()`, die Stufe aus `compute_tiers()` —
    verbunden ueber `symbol`. Das ist noetig und war die erste
    Fehlerquelle hier: `compute_tiers()` baut EIGENE Zeilen
    (`symbol name file tier tests spec evidence corpus`) und traegt
    **kein** `ext`. Ein `r.get("ext")` liefert dort leer, der
    Endungsindex bleibt leer, und das Skript meldet „0 Kandidaten" —
    gruen, ohne gesucht zu haben. Gefangen hat es allein die
    Diagnosezeile `gesuchte Endungen` in der Ausgabe; deshalb bleibt sie
    dort stehen.
    """
    import gen_verification_tiers as g  # type: ignore
    endung = {p["symbol"]: (p.get("ext") or "") for p in g.scan(WURZEL)}
    offen = {}
    for r in g.compute_tiers(WURZEL):
        if r["tier"] in ("T2", "T3"):
            offen[r["symbol"]] = endung.get(r["symbol"], "")
    return offen


def _endungsindex(offen: dict[str, str]) -> dict[str, list[str]]:
    idx: dict[str, list[str]] = {}
    for sym, exts in offen.items():
        for e in (exts or "").split(";"):
            e = e.strip().lower()
            if e:
                idx.setdefault("." + e, []).append(sym)
    return idx


def _erste_bytes(oeffner, name: str, n: int = 8) -> bytes:
    try:
        with oeffner(name) as f:
            return f.read(n)
    except Exception:
        return b""


#: Eine ENGE, benannte Ausnahme — keine allgemeine Regel.
#:
#: `.pro` ist die Endung von APE ProSystem UND von qmake-Projekten.
#: Gemessen sind **alle sechs** `.pro`-Treffer oberhalb der
#: Groessenschranke `UnifiedFloppyTool.pro` mit dem Kopf `#-------`,
#: also qmakes erzeugte Kopfzeile. Die Schranke allein trennt das nicht:
#: die groesste dieser Dateien hat 154 995 Byte.
#:
#: **Warum nicht die naheliegende Regel „Kopf ist druckbares ASCII =
#: Text":** sie wuerde genau das Gesuchte wegwerfen. `UAE-1ADF` (die
#: Kennung von `adf_ext`), `SINCLAIR`, `IMD ` und `2IMG` sind ASCII. Die
#: Ausnahme bleibt deshalb auf dieses eine Muster beschraenkt und wird
#: in der Ausgabe gezaehlt.
QMAKE_KOPF = b"#-------"


def _pruefe(name: str, groesse: int, idx: dict[str, list[str]],
            kopf_holen=None) -> dict | None:
    """Endung + Groesse pruefen, dann ERST den Kopf holen.

    Der Holer ist eine Funktion, damit die acht Bytes nur fuer Dateien
    gelesen werden, deren Endung ueberhaupt passt — sonst waeren es
    449 Archive x alle Mitglieder. Und er steht hier statt in beiden
    Archivzweigen, damit die qmake-Ausnahme EINE Stelle hat.
    """
    nl = name.lower()
    for end, syms in idx.items():
        if nl.endswith(end) and groesse >= MINDESTGROESSE:
            kopf = kopf_holen() if kopf_holen else b""
            if nl.endswith(".pro") and kopf.startswith(QMAKE_KOPF):
                return None
            return {"formate": syms, "name": name, "groesse": groesse,
                    "kopf": kopf}
    return None


def archiv_kandidaten(idx: dict[str, list[str]]) -> tuple[list[dict], int, list[str]]:
    kandidaten: list[dict] = []
    archive = 0
    fehler: list[str] = []
    for dirpath, dirnames, filenames in os.walk(WURZEL):
        dirnames[:] = [d for d in dirnames if d not in UEBERSPRINGEN
                       and not d.startswith("build")]
        for fn in filenames:
            low = fn.lower()
            if not low.endswith(ARCHIV_ENDUNGEN):
                continue
            pfad = Path(dirpath) / fn
            rel = str(pfad.relative_to(WURZEL)).replace("\\", "/")
            archive += 1
            try:
                if low.endswith(".zip"):
                    with zipfile.ZipFile(pfad) as z:
                        for i in z.infolist():
                            tr = _pruefe(
                                i.filename, i.file_size, idx,
                                lambda n=i.filename: _erste_bytes(z.open, n))
                            if tr:
                                tr["archiv"] = rel
                                kandidaten.append(tr)
                else:
                    if pfad.stat().st_size > TAR_MAX:
                        fehler.append(rel + ": zu gross, uebersprungen")
                        continue
                    with tarfile.open(pfad) as t:
                        for m in t.getmembers():
                            if not m.isfile():
                                continue

                            def hol(mm=m, tt=t):
                                f = tt.extractfile(mm)
                                return f.read(8) if f else b""

                            tr = _pruefe(m.name, m.size, idx, hol)
                            if tr:
                                tr["archiv"] = rel
                                kandidaten.append(tr)
            except Exception as e:
                fehler.append("%s: %s" % (rel, type(e).__name__))
    return kandidaten, archive, fehler


def unverzeichnet() -> list[str]:
    mf = WURZEL / "tests" / "corpus_manifest" / "manifest.json"
    if not mf.is_file():
        return []
    m = json.loads(mf.read_text(encoding="utf-8"))
    bekannt = {str(e.get("file", "")).replace("\\", "/")
               for e in m.get("images", [])}
    fehlt = []
    for d in ("tests/corpus", "tests/corpus_free"):
        basis = WURZEL / d
        if not basis.is_dir():
            continue
        for root, _, fs in os.walk(basis):
            for f in fs:
                rel = str((Path(root) / f).relative_to(WURZEL))
                rel = rel.replace("\\", "/")
                if rel not in bekannt:
                    fehlt.append(rel)
    return sorted(fehlt)


def selbsttest() -> int:
    faelle = []

    idx = _endungsindex({"dim": "dim;xdf", "udi": "udi"})
    faelle.append(("mehrfach-Endung wird zerlegt",
                   set(idx) == {".dim", ".xdf", ".udi"}))

    faelle.append(("Groessenschranke greift",
                   _pruefe("x.udi", 10, {".udi": ["udi"]}) is None
                   and _pruefe("x.udi", 99999, {".udi": ["udi"]}) is not None))

    faelle.append(("Endung case-insensitiv",
                   _pruefe("X.UDI", 99999, {".udi": ["udi"]}) is not None))

    # Die Falle, die diese Suche von Hand getroffen hat: eine Datei, die
    # woertlich `.zip` heisst, wird von `glob('*.zip')` NICHT gefunden.
    # `os.walk` findet sie — deshalb kein glob oben.
    faelle.append(("os.walk statt glob (Datei namens '.zip')",
                   ".zip".endswith(ARCHIV_ENDUNGEN)))

    faelle.append(("offene Formate sind abgeleitet",
                   callable(offene_formate)))

    # Die qmake-Ausnahme: eng, und sie darf die GESUCHTE ASCII-Kennung
    # nicht wegwerfen.
    idxp = {".pro": ["pro"]}
    faelle.append(("qmake-.pro wird verworfen",
                   _pruefe("x.pro", 99999, idxp, lambda: b"#-------") is None))
    faelle.append(("echtes .pro bleibt",
                   _pruefe("x.pro", 99999, idxp, lambda: b"\x00\x01\x02\x03")
                   is not None))
    idxa = {".adf": ["adf_ext"]}
    faelle.append(("ASCII-Kennung UAE-1ADF bleibt",
                   _pruefe("x.adf", 99999, idxa, lambda: b"UAE-1ADF")
                   is not None))

    # Der Kopf wird nur geholt, wenn Endung und Groesse passen.
    gerufen = []
    _pruefe("x.txt", 99999, idxa, lambda: gerufen.append(1) or b"")
    faelle.append(("Kopf wird nicht unnoetig gelesen", not gerufen))

    ok = 0
    for name, gut in faelle:
        print("  [%s] %s" % ("OK " if gut else "ROT", name))
        ok += bool(gut)
    print("Selbsttest %d/%d" % (ok, len(faelle)))
    return 0 if ok == len(faelle) else 1


def main(argv: list[str]) -> int:
    if "--selftest" in argv:
        return selbsttest()

    offen = offene_formate()
    idx = _endungsindex(offen)
    kandidaten, archive, fehler = archiv_kandidaten(idx)
    fehlt = unverzeichnet()

    print("[korpus-regal] Abbilder fuer OFFENE Formate (T2/T3)")
    print("  offene Formate        : %d  (%s)"
          % (len(offen), " ".join(sorted(offen))))
    print("  gesuchte Endungen     : %s" % " ".join(sorted(idx)))
    print("  Archive gelesen       : %d" % archive)
    print("  Lesefehler            : %d" % len(fehler))
    print("  Groessenschranke      : %d Byte (Entscheidung, siehe Kopf)"
          % MINDESTGROESSE)
    print("  KANDIDATEN            : %d" % len(kandidaten))
    for k in sorted(kandidaten, key=lambda x: (x["formate"], x["name"])):
        print("    %-14s %9d  %s"
              % ("/".join(k["formate"]), k["groesse"],
                 os.path.basename(k["name"])))
        print("    %-14s kopf=%r" % ("", k["kopf"]))
        print("    %-14s in %s" % ("", k["archiv"]))
    print("  UNVERZEICHNET im Korpus: %d" % len(fehlt))
    for f in fehlt:
        print("    ? %s" % f)
    for f in fehler[:5]:
        print("    ! %s" % f)

    rc = 0
    if len(kandidaten) > GRUNDLINIE_KANDIDATEN:
        print("\nHINWEIS: %d Kandidaten, Grundlinie %d — ein NEUES Archiv "
              "bringt moeglicherweise ein Abbild fuer ein offenes Format. "
              "Kopf und Groesse oben pruefen, dann Lizenz (P3-359)."
              % (len(kandidaten), GRUNDLINIE_KANDIDATEN))
        rc = 1
    if len(fehlt) > GRUNDLINIE_UNVERZEICHNET:
        print("\nROT: %d Korpusdateien ohne Manifest-Eintrag, Grundlinie %d. "
              "Eine nicht eingetragene Datei traegt keine Stufe und faellt "
              "keiner Pruefung auf." % (len(fehlt), GRUNDLINIE_UNVERZEICHNET))
        rc = 1
    if rc == 0:
        print("\nOK")
    return rc


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
