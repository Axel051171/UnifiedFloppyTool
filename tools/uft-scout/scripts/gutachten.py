#!/usr/bin/env python3
"""gutachten.py — erzeugt den Gutachten-ENTWURF aus Messung + Inventar.

  gutachten.py <messung.json> <inventory.json> [-o out/]

Alles Mechanische wird gefüllt (Lizenzzone mit Konsequenz, Inventar-
Abgleich, Negativlisten-Prüfung, Ratenbremse). Alles Urteilende bleibt
als UNGEKLÄRT-Block stehen — das füllt die Tiefenprüfung (Mensch oder
LLM-Sitzung mit dem Playbook), nie dieses Skript. Ein Skript, das
Kategorien rät, wäre genau die Erfindung, die AGENT.md Regel 2 verbietet.
"""
import json, os, sys
from datetime import date

HIER = os.path.dirname(os.path.abspath(__file__))
CFG = json.load(open(os.path.join(HIER, "..", "config.json"),
                     encoding="utf-8"))
NEG = json.load(open(os.path.join(HIER, "..", "data",
                                  "known_negatives.json"),
                     encoding="utf-8"))["eintraege"]

_INV_MOD = None


def _inventar():
    """`inventar.py` als Modul — damit das Treffer-Urteil an EINER Stelle
    liegt (MF-610). Import per Pfad, weil `scripts/` kein Paket ist."""
    global _INV_MOD
    if _INV_MOD is None:
        import importlib.util
        p = os.path.join(HIER, "inventar.py")
        spec = importlib.util.spec_from_file_location("uft_inventar", p)
        _INV_MOD = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(_INV_MOD)
    return _INV_MOD

ZONEN_FOLGE = {
    "GRUEN": "Code portierbar mit Attribution (samdisk-Muster)",
    "GELB": "NUR Konzept-Nachbau nach Verhaltens-Spec + Oracle "
            "(GPL-2.0-inkompatibel)",
    "ORANGE": "nicht einlinkbar; Helper-Prozess-Weg möglich — "
              "Eigentümer-Vorlage",
    "ROT": "keine Lizenz = alle Rechte vorbehalten; nur Spec aus "
           "Doku/Blackbox + Oracle",
    "PRUEFEN": "Eigentümer-Vorlage vor jedem weiteren Schritt",
}


def inventar_abgleich(m, inv):
    zeilen = []
    kandidaten = set()
    name = os.path.basename(m["pfad"]).lower()
    kandidaten.add(name)
    kandidaten.update(b.lower() for b in m.get("domaenen_treffer", {}))
    # MF-610: hier stand der Treffer-Vergleich ein ZWEITES Mal, wortgleich
    # zur damaligen Fassung in inventar.query(). Damit fiel das Urteil in
    # der Kopie, und eine Korrektur an der Quelle erreichte es nicht —
    # genau das ist hier passiert: die Trennung von starken und schwachen
    # Treffern (Rotbeweis `flux visualization`) wirkte nur in `query`,
    # nicht im Gutachten, das AGENT.md Regel 4 anwendet.
    #
    # Jetzt eine Implementierung. Wer sie ändert, ändert beide Stellen.
    for k, r in sorted(_inventar().query(inv, sorted(kandidaten)).items()):
        tier = f" (Tier {r['tier']})" if r["tier"] else ""
        if r["vorhanden"]:
            zeilen.append(f"- `{k}` → **vorhanden**: "
                          f"{', '.join(r['treffer'][:5])}{tier}")
        elif r["schwache_treffer"]:
            zeilen.append(
                f"- `{k}` → nur Teilwort-Treffer: "
                f"{', '.join(r['schwache_treffer'][:5])} — **kein Beleg**, "
                f"von Hand prüfen (AGENT.md Regel 4 verwirft nur auf "
                f"starke Treffer){tier}")
    return zeilen or ["- keine Überschneidung mit dem Inventar gefunden"]


def zaehle_offene(outdir):
    """Wie viele Gutachten warten noch auf Uebernahme?

    MF-614 (W9): hier stand `sum(1 for f in os.listdir(outdir) if
    f.endswith(".gutachten.md"))` — also ALLE Gutachten, die je
    geschrieben wurden. Nach sechs Zyklen war die Bremse dauerhaft zu und
    meldete „bereits 5 offene Gutachten in diesem Zyklus", obwohl kein
    einziges aus dem laufenden Zyklus stammte. Die Bremse, die eine
    Flutung verhindern soll, hat sich selbst blockiert.

    AGENT.md Regel 5 begrenzt VORSCHLAEGE je Zyklus, nicht Dateien im
    Archiv. Ein Gutachten, dessen Befunde uebernommen sind, belegt keinen
    Platz mehr. Gezaehlt wird deshalb nur, was noch keine
    Uebernahme-Marke traegt.

    Die Marke setzt der Mensch, der die Vorschlaege nach OPEN_ITEMS
    uebernimmt — eine Zeile am Dateianfang:

        <!-- uebernommen: MF-NNN -->

    ── Zweite Marke: Entwuerfe (MF-678) ──────────────────────────────────

    Die Bremse zaehlte bis hierher jede Datei ohne Uebernahme-Marke, und
    das warf zwei sehr verschiedene Dinge zusammen:

      * ein fertiges Gutachten, das auf eine ENTSCHEIDUNG wartet
      * die mechanische Ausgabe dieses Skripts — Messwerte, Lizenzzone,
        Inventar-Abgleich, unausgefuellte UNGEKLAERT-Liste —, deren
        Tiefenpruefung nie lief

    Beim zweiten gibt es nichts zu uebernehmen. Er wartet nicht auf den
    Eigentuemer, sondern auf seine eigene Stufe 3. Gezaehlt wurde er
    trotzdem, und zwei solche Entwuerfe (`sector-cpc`, `superdiskindex`)
    haben zusammen mit fuenf echten Gutachten die Bremse ausgeloest — der
    Eigentuemer haette also Entscheidungen treffen sollen, die es gar
    nicht zu treffen gab.

    Ein Entwurf traegt darum:

        <!-- stufe: 2 -->

    Er belegt keinen Platz in der Bremse. Das ist keine Lockerung: die
    Bremse begrenzt Vorschlaege an den Menschen, und ein Entwurf enthaelt
    keine. Er bleibt offene Arbeit — nur eben Arbeit des Agenten, nicht
    des Eigentuemers.
    """
    if not os.path.isdir(outdir):
        return 0
    offen = 0
    for f in os.listdir(outdir):
        if not f.endswith(".gutachten.md"):
            continue
        try:
            with open(os.path.join(outdir, f), encoding="utf-8",
                      errors="replace") as fh:
                kopf = fh.read(400)
        except OSError:
            kopf = ""
        if "stufe: 2" in kopf:
            continue        # Entwurf: wartet auf Stufe 3, nicht auf einen Menschen
        if "uebernommen:" not in kopf and "übernommen:" not in kopf:
            offen += 1
    return offen


def erzeuge(m, inv, outdir):
    name = os.path.basename(m["pfad"])
    voll = next((k for k in NEG if k.lower().endswith("/" + name.lower())),
                None)
    if voll and NEG[voll]["status"] in ("verworfen", "integriert"):
        return None, (f"ÜBERSPRUNGEN: {voll} steht auf der Negativliste "
                      f"({NEG[voll]['status']}: {NEG[voll]['grund']})")
    if zaehle_offene(outdir) >= CFG["max_vorschlaege_je_zyklus"]:
        return None, ("RATENBREMSE: bereits "
                      f"{CFG['max_vorschlaege_je_zyklus']} Gutachten ohne "
                      "Uebernahme-Marke (AGENT.md Regel 5). Erst "
                      "abarbeiten, dann `<!-- uebernommen: MF-NNN -->` in "
                      "die betroffenen Dateien in out/ setzen.")
    if m["domaenen_score"] < CFG["mindest_domaenen_score"]:
        return None, (f"ÜBERSPRUNGEN: Domänen-Score {m['domaenen_score']} "
                      f"< {CFG['mindest_domaenen_score']}")

    zone = m["lizenz_zone"]
    liz = "\n".join(f"  - `{l['datei'].split(name + '/', 1)[-1]}` → "
                    f"{l['kennung']} ({l['zone']})"
                    for l in m["lizenzen"]) or "  - KEINE Lizenzdatei"
    treffer = "\n".join(
        f"  - {b}: {', '.join(p for p in pf)}"
        for b, pf in sorted(m.get("domaenen_treffer", {}).items()))
    inventar = "\n".join(inventar_abgleich(m, inv))

    md = f"""# Gutachten-Entwurf: {name}
Stand: {date.today().isoformat()} · Messung: `{name}.messung.json`
(HEAD `{m.get('head', '?')}`, letzter Commit {m.get('letzter_commit', '?')})
· Inventar: UFT `{inv['head']}`

## Messwerte
- Dateien: {m.get('dateien')}, Sprachen: {m.get('sprachen')}
- Letzte Commit-Botschaft: {m.get('letzte_botschaft', '?')}
- Domänen-Score: {m['domaenen_score']} (Schwelle {CFG['mindest_domaenen_score']})

## Lizenz (aus Dateien, nicht README)
{liz}
- **Zone: {zone}** → Konsequenz: {ZONEN_FOLGE[zone]}

## Inventar-Abgleich (Abfrage, kein Urteil)
{inventar}

## Domänen-Fundstellen
{treffer}

## UNGEKLÄRT — von der Tiefenprüfung zu füllen, NIE zu raten
- [ ] Kategorie: Innovation / Verbesserung / Daten / Oracle / irrelevant
- [ ] Was genau fehlt UFT bzw. was wäre besser? (Datei+Zeile zitieren)
- [ ] Bei "besser": Differenzlauf-Plan (Binaries, Korpus, Metrik, Toleranz)
- [ ] Einhängepunkt in bestehende Pläne (welcher Baustein?)
- [ ] Oracle-Kandidat + Baubarkeit
- [ ] Beschaffungsliste (Fixtures, Referenzdateien, Specs)
- [ ] Aufwandsklasse S/M/L
- [ ] OPEN_ITEMS-Vorschlagstext (max. 3 Sätze, mit Messquelle)

## Regeln, die für diesen Fund gelten
- Zone {zone}: {ZONEN_FOLGE[zone]}
- Kein Code aus diesem Agenten (AGENT.md Regel 1)
"""
    out = os.path.join(outdir, f"{name}.gutachten.md")
    os.makedirs(outdir, exist_ok=True)
    with open(out, "w", encoding="utf-8") as f:
        f.write(md)
    return out, f"OK: {out} (Zone {zone}, Score {m['domaenen_score']})"


def main():
    if len(sys.argv) < 3:
        print(__doc__); return 2
    m = json.load(open(sys.argv[1], encoding="utf-8"))
    inv = json.load(open(sys.argv[2], encoding="utf-8"))
    outdir = "out"
    if "-o" in sys.argv:
        outdir = sys.argv[sys.argv.index("-o") + 1]
    pfad, meldung = erzeuge(m, inv, outdir)
    print(meldung)
    return 0 if pfad or meldung.startswith(("ÜBERSPRUNGEN", "RATENBREMSE")) \
        else 1


if __name__ == "__main__":
    sys.exit(main())
