#!/usr/bin/env python3
"""inventar.py — baut den Ist-Stand des Zielprojekts als abfragbare Daten.

Antwort auf: "hat UFT X schon?" darf nie eine Vermutung sein.
Quellen (alle im Zielbaum, alle maschinenlesbar):
  - scripts/gen_format_list.py             SSOT der Format-Plugins
  - src/formats/*/ , src/parsers/*/        (Modulverzeichnisse)
  - include/uft/formats/*.h                (Format-Header)
  - docs/VERIFICATION_TIERS.md             (Tier je Format)
  - docs/CAPABILITIES.md                   (Controller + Bench-Stand)
  - tests/corpus_manifest/manifest.json    (welche Referenz-Abbilder liegen)
  - vendorte Fremdbibliotheken (src/samdisk, src/flux/fdc_bitstream, ...)

Aufrufe:
  inventar.py build <uft-pfad> [-o inventory.json]
  inventar.py query <inventory.json> <begriff> [begriff ...]

── Zwei gemessene Korrekturen (2026-08-26) ─────────────────────────────

**1. Registry kommt aus der SSOT, nicht aus einem eigenen Regex.**
Vorher las `build()` `uft_format_registry_v2.c` mit
`\\{\\s*"([^"]+)"\\s*,\\s*"([^"]+)"` und meldete **164** Eintraege — die
SSOT `scripts/gen_format_list.py` sagt **88** (plus 49 aus dem
DSK_PLUGIN-Makro = 137). Der Regex griff ueber die Plugin-Tabelle hinaus
und zog u. a. `Bitstream` als Format-Kuerzel heraus.

Das ist die gefaehrliche Richtung: AGENT.md Regel 4 lautet „Inventar vor
Vorschlag". Ein zu grosses Inventar laesst gueltige Kandidaten
**stillschweigend** fallen. Und dieser Baum hat drei belegte Faelle, in
denen von Hand gepflegte Zahlen gedriftet sind (MF-526/541/567) — die
Lehre daraus ist, an die SSOT anzuschliessen statt daneben zu messen.

**2. `vorhanden` unterscheidet jetzt starke von schwachen Treffern.**
Rotbeweis: `query "flux visualization"` meldete **vorhanden**, weil
`flux` (ein Decoder-VERZEICHNIS) als Teilwort passt. UFT hat keine
Fluss-Visualisierung. Eine mehrwortige Faehigkeitsfrage darf nicht als
erledigt gelten, weil ein Bestandteil auf einen Verzeichnisnamen passt.

Gegenprobe, die weiter gilt: `adfs` (Acorn) matcht NICHT auf `adf`
(Amiga) — die Laengenschranke traegt.
"""
import json, os, re, subprocess, sys
from datetime import datetime, timezone
from pathlib import Path


def _read(p):
    try:
        with open(p, encoding="utf-8", errors="replace") as f:
            return f.read()
    except OSError:
        return ""


def build(root):
    inv = {
        "erzeugt": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        "quelle": root,
        "head": "",
        "formate_registry": [],   # (anzeigename, kuerzel)
        "format_dirs": [],
        "parser_dirs": [],
        "format_header": [],
        "tiers": {},              # format -> tier
        "controller": [],
        "vendored": [],
        "alle_begriffe": [],      # flacher Suchindex, kleingeschrieben
    }
    try:
        inv["head"] = subprocess.run(
            ["git", "-C", root, "rev-parse", "--short", "HEAD"],
            capture_output=True, text=True).stdout.strip()
    except OSError:
        pass

    # Format-Plugins aus der SSOT des Zielprojekts (siehe Kopfkommentar).
    # Faellt sie aus, wird das GEMELDET, nicht durch eine Schaetzung
    # ersetzt — ein leises Ersatzinventar waere schlimmer als keines.
    inv["ssot"] = {"quelle": "scripts/gen_format_list.py", "ok": False,
                   "hinweis": ""}
    try:
        import importlib.util
        gfl_path = Path(root) / "scripts" / "gen_format_list.py"
        spec = importlib.util.spec_from_file_location("gfl", gfl_path)
        gfl = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(gfl)
        for p in gfl.scan(Path(root)):
            inv["formate_registry"].append([p["name"], p["symbol"]])
            inv.setdefault("format_ext", {})[p["symbol"]] = p.get("ext", "")
        inv["ssot"]["ok"] = True
    except Exception as exc:                      # noqa: BLE001
        inv["ssot"]["hinweis"] = "%s: %s" % (type(exc).__name__, exc)
        print("WARNUNG: SSOT nicht lesbar (%s) — die Plugin-Liste ist LEER, "
              "nicht geschaetzt. Jede 'vorhanden'-Antwort dazu ist damit "
              "ungueltig." % exc, file=sys.stderr)

    for base, key in (("src/formats", "format_dirs"),
                      ("src/parsers", "parser_dirs")):
        d = os.path.join(root, base)
        if os.path.isdir(d):
            inv[key] = sorted(x for x in os.listdir(d)
                              if os.path.isdir(os.path.join(d, x)))

    hdir = os.path.join(root, "include/uft/formats")
    if os.path.isdir(hdir):
        inv["format_header"] = sorted(
            f[:-2] for f in os.listdir(hdir) if f.endswith(".h"))

    # Tiers: Zeilen "| `plugin` | **T1b** | ..." (Format gemessen an
    # docs/VERIFICATION_TIERS.md Abschnitt "Pro Format")
    tiers = _read(os.path.join(root, "docs/VERIFICATION_TIERS.md"))
    for m in re.finditer(r"^\|\s*`([^`]+)`\s*\|\s*\*{0,2}(T1b?|T2|T3)"
                         r"\*{0,2}\s*\|", tiers, re.M):
        inv["tiers"][m.group(1).strip().lower()] = m.group(2)

    caps = _read(os.path.join(root, "docs/CAPABILITIES.md"))
    for name in ("greaseweazle", "supercard", "scp", "kryoflux",
                 "fluxengine", "adf-copy", "applesauce", "fc5025",
                 "xum1541", "zoomfloppy", "usb"):
        if name in caps.lower():
            inv["controller"].append(name)

    for cand in ("src/samdisk", "src/flux/fdc_bitstream", "src/a8rawconv"):
        if os.path.isdir(os.path.join(root, cand)):
            inv["vendored"].append(cand)

    # Welche Referenz-Abbilder liegen bereits? Ohne das erzeugt der
    # Gutachter Beschaffungslisten fuer Dateien, die schon da sind — und
    # die Beschaffung ist in diesem Projekt der Engpass, nicht der Code.
    inv["korpus"] = []
    man = os.path.join(root, "tests/corpus_manifest/manifest.json")
    if os.path.isfile(man):
        try:
            with open(man, encoding="utf-8") as f:
                raw = json.load(f)
            eintraege = raw if isinstance(raw, list) else raw.get(
                "images", raw.get("entries", []))
            for e in eintraege:
                if isinstance(e, dict):
                    inv["korpus"].append({
                        "format": e.get("format", ""),
                        "herkunft": e.get("origin", ""),
                        # MF-612 (W1): hier stand `[:60]`. Die
                        # Provenienz endete mitten im Wort
                        # („GTK3VI" statt „GTK3VICE-3.10-win64").
                        # Sie ist fuer T1/T1b konstitutiv
                        # (VERIFICATION_PLAN §Provenienz-Regel) —
                        # sie ausgerechnet in dem Werkzeug zu
                        # kuerzen, das Beschaffungslisten prueft,
                        # ist verkehrt herum. Gefunden im zweiten
                        # Scout-Lauf.
                        "werkzeug": e.get("tool", "") or "",
                        "datei": e.get("file", ""),
                    })
        except (OSError, ValueError) as exc:
            inv["korpus_fehler"] = str(exc)

    idx = set()
    for a, b in inv["formate_registry"]:
        idx.add(a.lower()); idx.add(b.lower())
    for coll in ("format_dirs", "parser_dirs", "format_header",
                 "controller", "vendored"):
        idx.update(s.lower() for s in inv[coll])
    idx.update(inv["tiers"].keys())
    inv["alle_begriffe"] = sorted(idx)
    return inv


def query(inv, begriffe):
    """Beantwortet „hat UFT X schon?" — mit dem Unterschied zwischen
    „ja" und „ein Wort davon kommt vor".

    Rotbeweis, der zu dieser Trennung gefuehrt hat: `flux visualization`
    galt als vorhanden, weil `flux` ein Decoder-Verzeichnis ist. Ein
    schwacher Treffer ist ein Hinweis zum Nachsehen, kein Urteil — und
    AGENT.md Regel 4 darf nur auf STARKE Treffer hin verwerfen.
    """
    out = {}
    for b in begriffe:
        # MF-613: `bl.split()` trennt NUR an Leerzeichen. Damit umging
        # jeder Bindestrich und jeder Unterstrich die Stark/Schwach-
        # Trennung:
        #
        #     "flux visualization"  -> schwacher Treffer  (richtig)
        #     "flux-visualization"  -> vorhanden: true    (falsch)
        #     "flux_visualization"  -> vorhanden: true    (falsch)
        #
        # Und `gutachten.py` fuettert Repo-Basisnamen direkt hinein —
        # `hxcfe_amiga_copy_utility` galt als vorhanden, wegen `amiga`.
        #
        # Das ist derselbe Fehler wie MF-610, zum dritten Mal in einer
        # neuen Variante. Die ersten beiden Korrekturen haben den
        # Einzelfall repariert (erst nur mehrwortig, dann null Treffer);
        # diese repariert die REGEL: Trennzeichen werden vor dem
        # Zerlegen vereinheitlicht, egal welches.
        bl = re.sub(r"[-_./]+", " ", b.lower()).strip()
        mehrwortig = len(bl.split()) > 1

        stark, schwach = [], []
        for t in inv["alle_begriffe"]:
            if t == bl:
                stark.append(t)
            elif not mehrwortig and (bl in t or (len(t) >= 4 and t in bl)):
                # Einwortige Frage: Teilstring zaehlt weiter als stark
                # (adf/adfs bleibt durch die Laengenschranke getrennt).
                stark.append(t)
            elif mehrwortig and len(t) >= 4 and t in bl:
                # Mehrwortige Frage: ein passendes Bestandteil ist ein
                # Hinweis, kein Beleg.
                schwach.append(t)

        # ── MF-611 (SCOUT-1): die Abfrage muss ihre Reichweite kennen ──
        #
        # Der Index kennt Formatnamen, Verzeichnisse, Controller und
        # vendorte Bibliotheken. Er kennt KEINE Faehigkeiten. Bis hierher
        # antwortete er auf `jitter`, `weak bits`, `multi capture voting`
        # und `bit slip` mit `vorhanden: false` und leeren schwachen
        # Treffern — obwohl UFT alle vier hat (`src/hardwaretab.cpp`,
        # `src/recovery/uft_bitstream_recovery.c`,
        # `src/algorithms/advanced/uft_multi_rev_fusion.c`).
        #
        # Das ist die Gegenrichtung zu der Korrektur eine Stunde davor
        # (MF-610, `flux visualization` galt faelschlich als vorhanden).
        # Deren Rotbeweis deckte den Falsch-Positiv ab und liess den
        # Falsch-Negativ ohne Netz — gefunden hat es der erste
        # Scout-Lauf, sofort.
        #
        # Der ehrliche Weg ist billiger als der vollstaendige: statt den
        # Index um Modul- und Funktionsnamen zu erweitern (viel Arbeit,
        # neue Fehlalarme), sagt die Antwort jetzt, wenn die Frage
        # ausserhalb ihrer Abdeckung liegt. Eine Antwort, die ihre
        # Reichweite kennt, ist mehr wert als eine, die mehr zu wissen
        # vorgibt.
        # Kein einziger Treffer heisst: der Index hat zu diesem Begriff
        # NICHTS — und er kann nicht wissen, ob das „gibt es nicht" oder
        # „kenne ich nicht" bedeutet. Der erste Anlauf dieser Korrektur
        # prueste nur mehrwortige Fragen; `jitter` (einwortig, in UFT
        # vorhanden) fiel weiter als blankes `false` durch. Derselbe
        # Testlauf hat das sofort gezeigt.
        #
        # Ausnahme, und sie ist belegbar: fuer FORMATNAMEN ist die Liste
        # vollstaendig — sie kommt aus der SSOT. Dort heisst „kein
        # Treffer" wirklich „nicht vorhanden". Das steht als eigenes Feld
        # in der Antwort, damit der Aufrufer es unterscheiden kann.
        abgedeckt = bool(stark) or bool(schwach)
        if stark:
            hinweis = ""
        elif schwach:
            hinweis = ("nur Teilwort-Treffer — von Hand pruefen, NICHT "
                       "als vorhanden verwerfen")
        elif not abgedeckt:
            hinweis = ("INVENTAR DECKT DAS NICHT AB — der Index kennt "
                       "Formate, Verzeichnisse, Controller und vendorte "
                       "Bibliotheken, keine Faehigkeiten. `false` heisst "
                       "hier NICHT `fehlt`. Von Hand im Baum pruefen, "
                       "bevor etwas vorgeschlagen wird (AGENT.md Regel 4)")
        else:
            hinweis = ""

        out[b] = {
            "vorhanden": bool(stark),
            "abgedeckt": abgedeckt,
            "treffer": stark[:8],
            "schwache_treffer": schwach[:8],
            "tier": inv["tiers"].get(bl),
            # Nur hierfuer ist die Liste vollstaendig (SSOT): wenn der
            # Begriff ein Formatname ist, heisst „kein Treffer" wirklich
            # „nicht vorhanden".
            "plugin_liste_vollstaendig": bool(
                inv.get("ssot", {}).get("ok")),
            "hinweis": hinweis,
        }
    return out


def _hilfe():
    """Docstring ausgeben, ohne an der Konsolen-Kodierung zu sterben.

    MF-611: `query --help` druckte die Hilfe und starb dann mit
    UnicodeEncodeError — die Windows-Konsole laeuft unter cp1252, der
    Kopfkommentar enthaelt Gedankenstriche und Pfeile. Eine Hilfe, die
    beim Helfen abstuerzt, ist keine.
    """
    txt = __doc__ or ""
    enc = (sys.stdout.encoding or "utf-8")
    sys.stdout.write(txt.encode(enc, errors="replace").decode(enc))
    sys.stdout.write(os.linesep)


def main():
    if len(sys.argv) < 3:
        _hilfe(); return 2
    if sys.argv[1] == "build":
        root = sys.argv[2]
        out = "inventory.json"
        if "-o" in sys.argv:
            out = sys.argv[sys.argv.index("-o") + 1]
        inv = build(root)
        with open(out, "w", encoding="utf-8") as f:
            json.dump(inv, f, ensure_ascii=False, indent=1)
        ssot = "SSOT ok" if inv.get("ssot", {}).get("ok") else "SSOT FEHLT"
        print(f"OK: {len(inv['formate_registry'])} Plugins ({ssot}), "
              f"{len(inv['format_dirs'])} Format-Dirs, "
              f"{len(inv['tiers'])} Tier-Zeilen, "
              f"{len(inv.get('korpus', []))} Korpus-Abbilder, "
              f"HEAD {inv['head']} -> {out}")
        return 0 if inv.get("ssot", {}).get("ok") else 1
    if sys.argv[1] == "query":
        # MF-611 (SCOUT-2): hier stand `open(sys.argv[2])` ungeprueft —
        # `query --help` endete mit einem FileNotFoundError-Traceback
        # statt mit der Hilfe. Gefunden im ersten Scout-Lauf.
        if len(sys.argv) < 4 or sys.argv[2] in ("-h", "--help"):
            _hilfe()
            return 2
        try:
            with open(sys.argv[2], encoding="utf-8") as f:
                inv = json.load(f)
        except OSError as exc:
            print("Inventar nicht lesbar: %s\n"
                  "Zuerst `inventar.py build <uft-pfad> -o <datei>` "
                  "laufen lassen." % exc, file=sys.stderr)
            return 2
        print(json.dumps(query(inv, sys.argv[3:]),
                         ensure_ascii=False, indent=1))
        return 0
    _hilfe(); return 2


if __name__ == "__main__":
    sys.exit(main())
