# uft-scout

Zubringer-Agent für UnifiedFloppyTool: findet in fremdem Quellcode, was
dem Werkzeug fehlt oder es verbessert — und übergibt Funde als belegte
Gutachten an den bestehenden MF-Workflow. **Erzeugt niemals Code.**
Betriebsregeln: [`AGENT.md`](AGENT.md) · Lizenzregeln:
[`playbook/lizenzmatrix.md`](playbook/lizenzmatrix.md)

## Pipeline

    scout.py ──► vermessen.py ──► gutachten.py ──► [Tiefenprüfung] ──► OPEN_ITEMS
    (Suche)      (Messung)        (Entwurf)         (LLM/Mensch mit       (Ziel-Repo,
                                                     Playbook)             max 5/Zyklus)

Alles Mechanische ist Skript (deterministisch, offline testbar). Alles
Urteilende steht als UNGEKLÄRT-Block im Gutachten-Entwurf und wird in
der Tiefenprüfung gefüllt — von einer LLM-Sitzung, die das Playbook
lädt, oder von einem Menschen. Ein Skript, das Kategorien rät, wäre
erfundene Bewertung.

## Bedienung

```sh
# Stufe 0: Inventar des Zielprojekts (Antwort auf "haben wir X?")
python3 scripts/inventar.py build <uft-klon> -o work/inventory.json
python3 scripts/inventar.py query work/inventory.json amsdos pfs d64

# Stufe 1: Kandidaten suchen (GITHUB_TOKEN optional, erhöht Rate-Limit)
python3 scripts/scout.py [--limit 30] [--query "..."]

# Stufe 2: einen Kandidaten vermessen (URL oder lokaler Pfad)
python3 scripts/vermessen.py https://github.com/x/y.git

# Stufe 3: Gutachten-Entwurf erzeugen (verweigert bei Negativliste,
# Ratenbremse, Score unter Schwelle)
python3 scripts/gutachten.py work/y.messung.json work/inventory.json -o out

# Stufe 3b: Tiefenprüfung — Claude-Sitzung mit playbook/ + Gutachten-
# Entwurf + Klon; füllt die UNGEKLÄRT-Blöcke, zitiert Datei+Zeile.
```

## Verifikation des Agenten selbst (Stand: erster End-zu-End-Lauf)

- Inventar: 164 Registry-Einträge, 143 Format-Dirs, **88 Tier-Zeilen**
  — deckt sich mit den 88 tier-geführten Formaten der v4.1.6-Notes
- Negativfall geprüft: `misskey_mfm_parser` wird mit Grund verweigert
- Lizenz-Zonierung geprüft: sector-cpc → ROT (keine Datei),
  superdiskindex → GRÜN (MIT)
- Live-Suche geprüft: 5 echte Kandidaten, darunter ein aktiver nativer
  IPF-Decoder; eigene Repos werden vorgefiltert
- Zwei beim Test gefundene Fehler (Tier-Parser, unscharfer Abgleich)
  wurden gefixt und sind durch die obigen Läufe belegt

## Grenzen (ehrlich)

- Findet Greifbares (Formate, Dateisysteme, Daten, Oracles). „Besser
  als unsere PLL" erkennt kein Grep — dafür verlangt das Gutachten
  einen Differenzlauf-Plan.
- GitHub-Suche sieht nur GitHub; Foren/GitLab sind manuelle Zubringer.
- Der Gutachten-ENTWURF ist Vorarbeit, kein Urteil. Ohne Tiefenprüfung
  wandert nichts in OPEN_ITEMS.
