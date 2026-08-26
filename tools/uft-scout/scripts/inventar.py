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
                        "werkzeug": (e.get("tool", "") or "")[:60],
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
        bl = b.lower().strip()
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

        out[b] = {
            "vorhanden": bool(stark),
            "treffer": stark[:8],
            "schwache_treffer": schwach[:8],
            "tier": inv["tiers"].get(bl),
            "hinweis": ("nur Teilwort-Treffer — von Hand pruefen, NICHT "
                        "als vorhanden verwerfen"
                        if schwach and not stark else ""),
        }
    return out


def main():
    if len(sys.argv) < 3:
        print(__doc__); return 2
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
        with open(sys.argv[2], encoding="utf-8") as f:
            inv = json.load(f)
        print(json.dumps(query(inv, sys.argv[3:]),
                         ensure_ascii=False, indent=1))
        return 0
    print(__doc__); return 2


if __name__ == "__main__":
    sys.exit(main())
