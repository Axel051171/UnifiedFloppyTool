# Gutachten: monkeyman79/dfsimage

- **Repo:** https://github.com/monkeyman79/dfsimage.git
- **Stand:** HEAD `de24cf0ff5`, letzter Commit **2021-02-20** („API
  documentation in progress"), Version `0.9rc3` (`setup.cfg:3`) —
  seit 5½ Jahren ruhend; für ein Oracle unschädlich (Verhalten friert
  mit dem Commit ein), für Upstream-Fixes relevant.
- **Messung:** `work/dfsimage.messung.json` (2026-08-28, 38 Dateien,
  23 `.py`, Domänen-Score 9) · Inventar `work/inv.json` (UFT
  `cf5fa96f`, erzeugt 2026-08-28T15:41Z, frisch).
- **Anlass (Regel 6):** Eigentümer-Auftrag Zyklus (2026-08-28) — Acorn
  ist die schwächste Ecke (`docs/PLAN_v4.1.7.md:524`, Nachtrag 5);
  `ssd`/`adl` auf T3 (`docs/VERIFICATION_TIERS.md:99,57`), Korpus ohne
  ein einziges Acorn-Abbild (`inv["korpus"]`: 22 Einträge, Formate
  `adf, atr, d64, d67, d71, d80, d81, d82, fdi, g64, g71, hfe, scp,
  xfd` + protection — kein ssd/dsd/adl; Methode: Format-Feld aller
  Einträge aufgezählt). **Neue Fragestellung gegenüber dem
  DiscImageManager-Zyklus (übernommen MF-642):** gibt es eine ZWEITE,
  von DIM unabhängige Hand für den Acorn-Differenzlauf — und eine
  lizenzfreundlichere Quelle für den selbst erzeugten Korpus? Repo
  nicht in `data/known_negatives.json` (grep `dfsimage` → 0 Treffer).
- **Werkzeug:** Python-Kommandozeilen- und Bibliotheks-Werkzeug für
  **BBC Micro Acorn DFS** `.ssd`/`.dsd`-Abbilder inkl. MMB-Container
  (readme.rst:1-25): list/cat/index, import/export, create/modify,
  format, digest, convert (linear↔verschränkt), MMB-Kommandos.

## 1. Lizenzurteil (Zone, mit Fundstelle)

**Wurzel: MIT → GRÜN.** `LICENSE:1-3`: „MIT License / Copyright (c)
2021 Tadeusz Kijkowski". Methode: `vermessen.py` hat alle 23
Quelldateien geprüft (`lizenz_je_datei_vollstaendig: true`).

**Vendoring: `dfsimage/wildparse/LICENSE` → vom Skript UNGEKLÄRT,
von Hand identifiziert als PSF License Agreement für Python 3.9.1**
(`wildparse/LICENSE:1-3`: „This LICENSE AGREEMENT is between the
Python Software Foundation…"). `wildparse/argparse.py:1-6` erklärt die
Herkunft: „modified version [of CPython argparse] to support
intermixed positional and optional parameters" mit Link auf den
eigenen CPython-Fork-Commit. Die PSF-Lizenz ist permissiv und
GPL-kompatibel — aber die **Zonen-Einstufung einer im Skript
unbekannten Lizenz ist per Regel 8 Eigentümer-Sache**, darum bleibt
die Gesamt-Zone formal **PRÜFEN** stehen.

**Konsequenz für diesen Zyklus: folgenlos**, denn kein Vorschlag unten
übernimmt Code — dfsimage wird ausschließlich als **Oracle** (nur
Ausgaben verglichen) und **Spec-Quelle** (Verhalten dokumentiert, von
Stufe 4 gegen Primärdoku neu belegt) verwendet. Selbst dafür gilt: die
Eigentümer-Vorlage (Punkt 1 der UNGEKLÄRT-Liste) vor der
Oracle-Registrierung abhaken.

## 2. Attribution der Quelle (Pflichtfeld, MF-636)

Methode: `grep -rniE "based on|port of|adapted from|derived from|
courtesy|originally|thanks"` über `dfsimage/` + `readme.rst` → 10
Zeilen, davon **1 relevante**:

- `dfsimage/wildparse/` = modifiziertes CPython-`argparse`
  (**PSF-2.0**, Autoren Steven J. Bethard / Raymond Hettinger,
  `wildparse/argparse.py:1-2`) — betrifft nur das CLI-Parsing, nicht
  die DFS-Logik.

Die DFS-Implementierung selbst trägt **keine** Fremdattribution —
eigenständige Implementierung von Tadeusz Kijkowski.
**Unabhängigkeit von DiscImageManager:** `grep -rni
"discimagemanager|holdsworth|beebem"` über den Klon → 0 Treffer
(einzige `mmc`-Treffer betreffen MMC-Karten, nicht DIM). Anderer
Autor, andere Sprache (Python vs. Free Pascal), älter als DIMs
aktuelle Fassung → als **zweite, unabhängige Hand** tauglich.

## 3. Inventar-Abgleich (Abfrage zitiert)

`python scripts/inventar.py query work/inv.json SSD DSD DFS "acorn
dfs" watford MMB` (2026-08-28):

| Begriff | Antwort | Konsequenz |
|---|---|---|
| `SSD` | `vorhanden: true`, Treffer `ssd`, `ssd_dsd`, **Tier T3**, `plugin_liste_vollstaendig: true` | Format da, ungeprüft — Hebungsziel, kein neues Plugin |
| `DSD` | `vorhanden: true`, Treffer `ssd_dsd` | dito |
| `DFS` | `vorhanden: true`, Treffer `uft_bbc_dfs` | Katalog-Bibliothek existiert — Tür-Frage unten |
| `acorn dfs`, `watford`, `MMB` | `abgedeckt: false` | von Hand im Baum nachgesehen — Ergebnis in F6/F7 |

Handprüfung MMB: `git grep -rli mmb -- src include` → 1 Datei,
`src/core/uft_disk_convert.c:142` — **falsch positiv** (Teilwort in
„bestimmbar"). UFT hat **keine** MMB-Unterstützung; da die
Formatliste aus der SSOT vollständig ist, heißt „kein Treffer" hier
wirklich „nicht vorhanden".

## 4. Befunde (jede Aussage mit Fundstelle)

### F1 · Oracle: skriptbare Datei-Hashes, alle drei Fragen mit JA beantwortet

- **Konsole:** `setup.py:5` registriert `console_scripts:
  dfsimage=dfsimage:cli`; nicht auf PyPI (readme.rst:72-75),
  Installation aus dem Klon (`python_requires >= 3.8`,
  `setup.cfg:18`). Kein Compiler, keine GUI-Toolchain — leichter zu
  beschaffen als das Lazarus-gebaute DIMConsole.
- **Hashes je Datei:** `dfsimage digest` mit `-a sha1|sha256|md5`
  (`cli.py:1650-1651`, Default SHA-1 in `misc.py:94-96`) und
  **Digest-Modi** `all|file|data` (`cli.py:160-168`): `data` = nur
  Dateiinhalt — exakt der `flophashes`-Differenzlauf-Standard
  (MF-629). Zusätzlich Hashes direkt in der Liste:
  `list --list-format="{fullname:12} {sha1}"` (readme.rst:467) und
  maschinenlesbares `index -f json` (readme.rst:43-45,469).
- **Unabhängig von DIM:** ja (Abschnitt 2).

**Kategorie: Oracle.**

### F2 · Korpus-Erzeugung aus einem MIT-Werkzeug

`create --new` (readme.rst:520-536, inkl. `-D -L` für linear
doppelseitig), `format` (readme.rst:725-737), `import --new`
(readme.rst:52-56), `create-mmb` (readme.rst:913). Damit ist der
fehlende Acorn-DFS-Korpus (heute 0 Abbilder) **selbst erzeugbar** —
und zwar aus einem **MIT**-Werkzeug statt aus den GPL-3-`Blank
Images/` von DIM, deren Kopieren das DIM-Gutachten (SCOUT-26) bewusst
ausschloss. **Kategorie: Daten.** Grenze: nur DFS — für
ADFS-S/M/L/D/E/F bleibt DIMConsole die einzige bekannte Quelle.

### F3 · Verhaltens-Spec: Katalog-Validierung

`side.py:606-671` prüft: Gesamtsektorzahl ∈ {400, 800}
(`SINGLE_SECTORS`/`DOUBLE_SECTORS`, `consts.py:11-12`),
Katalog-Sektorzahl ≤ physische Sektoren, Katalogende ≡ 0 (mod 8),
Opt-Byte `& 0xCC == 0`, je Eintrag Start-/Endsektor in Grenzen und
Einträge **absteigend geordnet ohne Überlappung** (`side.py:655-663`,
`entry.py:456-489`; bei Ordnungsverstoß zusätzliche
Belegungsprüfung). UFTs Probe rät stattdessen über Confidence-Stufen
(`uft_ssd_plugin.c:42-60`) — und deren Kommentar enthält eine falsche
Zahl: „Valid sector counts: 0x90=400, 0x20=800, **0xA0=1280**"
(`uft_ssd_plugin.c:53`); 1280 = 0x500 hat Low-Byte 0x00, nicht 0xA0,
und DFS-Kataloge tragen maximal 1023 Sektoren (10 Bit). Die
dfsimage-Prüfliste ist fertiges Spec-Futter für die SSD-Hebung —
jede Zahl von Stufe 4 gegen Acorn-Primärdoku (DFS-Handbuch/BeebWiki)
gegenzulesen. **Kategorie: Daten/Spec.**

### F4 · Verhaltens-Delta: 18-Bit-Adressen werden bei UFT nicht vorzeichenerweitert

dfsimage: sind die zwei High-Bits der Load-/Exec-Adresse `0b11`, wird
auf `0xFFxxxx` erweitert (`entry.py:257-262` bzw. `:272-277`:
`if high == 3: high = 255`) — die dokumentierte
Host/IO-Prozessor-Konvention des BBC. UFT setzt stur zusammen:
`entry->load_addr = load_lo | (hi << 16)` →
`0x3xxxx` (`src/formats/bbc/uft_bbc_dfs.c:59-61`). Die
High-Bit-Zerlegung selbst stimmt überein (`entry.py:492-506` Index
0=Start/1=Load/2=Länge/3=Exec ↔ `include/uft/formats/
uft_bbc_dfs.h:145-148` — Bit-für-Bit gleiche Zuordnung,
Cross-Tool-Bestätigung). Das Delta betrifft Exporte (`.inf`),
Digest-Modus `file` und jede Adressanzeige. **Kategorie:
Verbesserung** — Differenzlauf-Plan in Abschnitt 6.

### F5 · Lineare vs. verschränkte Doppelseiten-Abbilder

dfsimage modelliert **beide** Ablagen: verschränkt (Spur 0 Seite 0,
Spur 0 Seite 1, …) und **linear** (Seite 0 komplett, dann Seite 1) —
`image.py:79-94` (`linear`-Flag, Default nach Endung/Größe,
`cli.py:45-54`), zweiter Katalog-Offset `image.py:336`, Umwandlung
`dfsimage convert --from -D -L linear.img --to inter.dsd`
(readme.rst:51). UFT kennt nur die Verschränkung
(`uft_ssd_plugin.c:12`, `src/formats/bbc/ssd_dsd.c:63`). Ein lineares
409.600-Byte-Abbild würde heute still als verschränkte DSD gelesen —
Seiteninhalte vertauscht, ohne Fehler. Das beantwortet zugleich
UNGEKLÄRT-Punkt 3 des DIM-Gutachtens: die lineare Variante existiert
draußen, dfsimage führt sie als eigenen Abbild-Typ samt Konverter.
**Kategorie: Verbesserung** (Spec + Rotbeweis, kein neues Plugin).

### F6 · Baum-Befund (Bereitschafts-Messung, Nachtrag-1-Regel): drei Acorn-Module ohne Tür

Methode: `git grep -ln <symbol> -- src include tests` je exportierter
Funktion; `grep -rn ANKER src/formats/bbc/ src/formats/ssd/` → 0.

1. **`src/formats/bbc/uft_bbc_dfs.c`** (358 Z., kompiliert via
   `.pro:1801`): alle 5 Exporte (`uft_dfs_read_entry`,
   `uft_dfs_create_catalog`, `uft_dfs_add_file`,
   `uft_dfs_extract_file`, `uft_bbc_detect_format`) erscheinen
   außerhalb der Datei **nur** in den eigenen Prototypen
   (`include/uft/formats/uft_bbc_dfs.h`, `include/uft/uft_bbc_dfs.h`).
   Katalog-Lesen inkl. High-Bits ist fertig — es fehlt die Tür.
2. **`src/formats/bbc/ssd_dsd.c`** (85 Z., `.pro:1800`):
   FloppyDevice-Backend mit open/read/write; `uft_bbc_ssd_dsd_open`
   hat 0 Aufrufer, `include/uft/formats/ssd_dsd.h` inkludiert niemand.
3. **`src/formats/ssd/uft_ssd_parser_v2.c`** (555 Z., `.pro:1414`):
   „GOD MODE"-Datei mit eigenem `main()` und `TEST()`-Makros
   (`:482-540`), nicht als Plugin registriert; ihre
   Watford-„Erkennung" ist geraten (`:334`:
   `is_watford = looks_like_catalog && file_count == 31` statt des
   8×`$AA`-Markers aus dem DIM-Gutachten) — der Header verspricht
   „DFS variants (Watford, Opus, ADFS hybrids)" (`:22`), geliefert
   ist eine Vermutung.

Das ist dieselbe Gestalt wie die im Plan gezählten acht Fälle
(„Können im Baum, Zugang fehlt", `docs/PLAN_v4.1.7.md` Nachtrag 1) —
plus **dreifache** DFS-Katalog-Implementierung (1., 3. und die
Plugin-Probe) ohne gemeinsame Wahrheit und ohne `# ANKER:`-Zeile in
einem der drei Module (Verwaisten-Regel!). **Kategorie: Baum-Befund**
— der wertvollste dieses Zyklus.

### F7 · MMB-Container (Fundus, kein Vorschlag)

dfsimage adressiert `.mmb`-Container (511 SSDs je Datei,
`mmbfile.py`, readme.rst:15-25) inkl. `beeb.mmb:12`-Adressierung.
UFT: nicht vorhanden (Abschnitt 3). Ein MMB-Leser wäre ein **neues
Format** → Moratorium/1:2-Regel, und er bewegt heute keine der vier
Kennzahlen. **Fundus, nicht Auftrag.**

### Negativ-Ergebnis: Watford

`grep -rni watford dfsimage/` → **0 Treffer**; die Validierung
(`side.py:639`, Opt-Byte-Maske) kennt nur Standard-DFS mit 31
Einträgen (`consts.py`). dfsimage hilft SCOUT-29 (Watford-Spec)
**nicht** — dort bleibt DIM die einzige Quelle.

## 5. Oracle-Registrierung — Antwort auf die Kernfrage

| Frage | Antwort | Beleg |
|---|---|---|
| Konsolen-Einstieg? | ja, `dfsimage` | `setup.py:5` |
| Skriptbar? | ja, argparse-CLI + `-f json` | `cli.py`, readme.rst:43-45 |
| Hashes je Datei? | ja, `digest -m data -a sha256` | `cli.py:1645-1651`, `misc.py:85-96` |
| Unabhängig von DIM? | ja | Abschnitt 2 |
| Abbild-Erzeugung? | ja, `create`/`format`/`import --new`/`convert` | readme.rst:51,520-541,725-737 |

Damit stünde der Acorn-DFS-Differenzlauf auf **zwei unabhängigen
Händen** (dfsimage MIT/Python + DIMConsole GPL-3/Pascal) — stärker
als der D64-Standard (nur `c1541`).

## 6. Differenzlauf-Plan (Pflicht wegen F4/F5-„besser"-Aussagen)

- **Binaries:** UFT-Testtreiber über den künftig verdrahteten
  DFS-Leser (Stufe 4) · `dfsimage digest -m data -a sha256 --name`
  @ `de24cf0ff5` · DIMConsole @ `5ffe4796fe` als dritte Hand.
- **Korpus:** selbst erzeugt per dfsimage-Skript: S40/S80-SSD,
  D40/D80-DSD (verschränkt **und** linear, per `convert` als Paar mit
  identischem Inhalt), Dateien mit Load/Exec `&FFFFxxxx` (High-Bits
  `0b11`), Datei >64 KiB abgelehnt/Grenzfall 0x3FFFF, leerer und
  voller Katalog (31 Dateien); je Abbild Manifest (Werkzeug, Version,
  Commit, Skript, SHA-256).
- **Metrik:** je Datei SHA-256 des Inhalts identisch; je Eintrag
  Name, Verzeichnis, Lock, Load, Exec, Länge, Startsektor identisch.
- **Toleranzliste:** BBC-Zeichen `&60`→`£`-Übersetzung in
  dfsimage-Namen (`entry.py:179`) — Vergleich auf Roh-Bytes
  normalisieren; Adress-Darstellung 18 Bit vs. vorzeichenerweitert
  (genau das zu messende Delta F4 — im ersten Lauf als erwartete
  Abweichung führen, nach Spec-Entscheid als Fehler).

## 7. Einhängepunkt (im Baum auffindbar)

- `docs/PLAN_v4.1.7.md` **Nachtrag 5** („Acorn diszipliniert
  einreihen" — fünfter Phase-1-Kandidat; genau dessen Bedingung „sofern
  … skriptbar" ist hiermit für die zweite Hand belegt) und
  **Nachtrag 1** („erst Türen suchen" — F6 ist die geforderte
  Bereitschafts-Messung für den Acorn-Baustein).
- `docs/VERIFICATION_PLAN.md` (Tier-Hebung ssd T3 → T1b).
- `docs/OPEN_ITEMS.md`: Fortsetzung der SCOUT-Reihe; höchste vergebene
  Nummer bei Abfassung **SCOUT-37** (Methode:
  `grep -o "SCOUT-[0-9]*" docs/OPEN_ITEMS.md | sort -n | tail`).
  **Drei Zyklen laufen parallel — Nummern bei Übernahme neu vergeben.**

## 8. OPEN_ITEMS-Vorschläge (4 von max. 5 — der fünfte Platz bleibt bewusst leer, F7 ist Fundus)

| Nr. (vorläufig) | Vorschlag | Kennzahl, die er bewegt |
|---|---|---|
| SCOUT-38 | **dfsimage als zweites, MIT-lizenziertes Acorn-DFS-Datei-Hash-Oracle neben DIMConsole registrieren und den DFS-Korpus daraus erzeugen.** `dfsimage digest -m data -a sha256` liefert Datei-Hashes nach dem `flophashes`-Muster (`cli.py:1645-1651`, `misc.py:85-96`), `create`/`format`/`import --new` erzeugen Abbilder (readme.rst:520-541) — MIT (`LICENSE:1-3`) statt der GPL-3-Blanks aus SCOUT-26; unabhängig von DIM implementiert (0 Querverweise, Abschnitt 2). Heute liegen 0 Acorn-Abbilder im Korpus (`inv["korpus"]`, 22 Einträge geprüft). | ungeprüfte Formate runter (ssd: T3→T1b-Weg geöffnet); zusätzlich Korpus Acorn 0→n, hardwarefrei |
| SCOUT-39 | **Acorn-Türen-Verdrahtung mit Rotbeweis — drei Module, null Aufrufer, drei Katalog-Implementierungen.** `uft_bbc_dfs.c` (5 Exporte, Nennungen nur in eigenen Prototypen), `ssd_dsd.c` (Header inkludiert niemand), `uft_ssd_parser_v2.c` (eigenes `main()`, nicht registriert) — Methode: `git grep -ln` je Symbol über `src include tests` (F6). Rotbeweis zuerst: UFT-Dateiliste gegen `dfsimage list` auf einem erzeugten Abbild, rot weil UFT nichts liefert; dann EINE Katalog-Wahrheit statt drei, Rest nach Verwaisten-Regel (kein `# ANKER:` in keinem der drei). | ungeprüfte Formate runter (ssd-Inhalt wird beweisbar; Vorbedingung für jede DFS-Hebung) |
| SCOUT-40 | **DFS-Verhaltens-Spec: 18-Bit-Adress-Vorzeichenerweiterung + Katalog-Validierungsregeln.** dfsimage erweitert High-Bits `0b11` auf `0xFFxxxx` (`entry.py:257-262`), UFT nicht (`uft_bbc_dfs.c:59-61`); dazu die 5 Katalog-Prüfungen aus `side.py:606-671`/`entry.py:456-489` als Spec-Futter und die falsche Kommentar-Zahl „0xA0=1280" in `uft_ssd_plugin.c:53` tilgen. Stufe 4 belegt jede Regel gegen Acorn-DFS-Primärdoku (dfsimage ist Spec-Quelle, nicht Referenz). | ungeprüfte Formate runter (ssd-Hebung inhaltlich statt nur geometrisch) |
| SCOUT-41 | **Lineare Doppelseiten-Ablage erkennen statt still als verschränkt lesen.** dfsimage führt linear und verschränkt als getrennte Abbild-Typen mit Konverter (`image.py:79-94,336`; readme.rst:51); UFT kennt nur Verschränkung (`uft_ssd_plugin.c:12`, `ssd_dsd.c:63`) — ein lineares 409.600-B-Abbild wird heute mit vertauschten Seiteninhalten gelesen, ohne Fehler. Rotbeweis: per `dfsimage convert` erzeugtes Paar (gleicher Inhalt, beide Ablagen) gegen `uft_disk_open()`; Entscheid über beide Kataloge (Sektor 0/1 beider Seiten), Größe als Rückfall. | ungeprüfte Formate runter (stille Fehlklassifikation raus — Korrektheitsbedingung der T3-Hebung) |

**Bestätigung ohne Vorschlag:** die High-Bit-Zerlegung des
Mixed-Bytes stimmt zwischen UFT und dfsimage überein
(`uft_bbc_dfs.h:145-148` ↔ `entry.py:492-506`) — im Hebungs-Test
festhalten. F7 (MMB) bleibt Fundus.

## 9. Beschaffungsliste (gegen `inv["korpus"]` geprüft: nichts davon liegt)

1. **dfsimage-Klon @ `de24cf0ff5`** — liegt bereits unter
   `tools/uft-scout/work/dfsimage` (gitignored); für Stufe 4:
   `pip install .` aus dem Klon (nicht auf PyPI, readme.rst:72-75),
   Python ≥ 3.8 (`setup.cfg:18`). Keine weitere Toolchain.
2. **Acorn-DFS-Korpus, selbst erzeugt** (dfsimage-Skript, Inhalt aus
   Abschnitt 6) — je Abbild Manifest + Hashdatei.
3. **Acorn-DFS-Primärdokumentation** (Acorn DFS User Guide /
   BeebWiki-Katalogseiten) als benannte Referenz für SCOUT-40 —
   Stufe-4-Pflicht; dfsimage allein genügt der Einfrier-Regel nicht.
4. Für ADFS (S/M/L/D/E/F) bleibt die DIMConsole-Beschaffung aus dem
   DIM-Gutachten bestehen — dfsimage deckt nur DFS.

## 10. Aufwandsklassen

- SCOUT-38: **S–M** (pip-Install, Erzeugungs-Skript,
  Oracle-Registrierung, Manifest)
- SCOUT-39: **M** (Verdrahtung + Konsolidierung dreier
  Implementierungen, Rotbeweis, Verwaisten-Entscheid)
- SCOUT-40: **S** (Spec-Dokument + Kommentar-Korrektur)
- SCOUT-41: **S–M** (Probe-Verhalten + Fixture-Paar + Tests)

## 11. UNGEKLÄRT

1. **PSF-Zone (Regel 8):** Hand-Einstufung „PSF-2.0, permissiv" für
   `wildparse/LICENSE` ist eine Eigentümer-Vorlage, keine
   Scout-Entscheidung; bis dahin bleibt die Skript-Zone PRÜFEN
   maßgeblich. Praktisch folgenlos (nur Oracle/Spec-Nutzung).
2. **204.800-Byte-Mehrdeutigkeit** (80×1-SSD vs. 40×2-DSD): dfsimage
   entscheidet nach Endung und Größe (`cli.py:45-54`), nicht nach
   Katalog — als Referenz dafür ungeeignet; SCOUT-28 (DIM-Gutachten)
   bleibt die Quelle.
3. **Verbreitung linearer Doppelseiten-Abbilder** draußen: dfsimage
   modelliert sie als eigenen Typ samt Konverter; Häufigkeit in
   realen Archiven unbelegt.
4. **Datei >64 KiB / Längen-High-Bits:** ob dfsimage Längen über
   0xFFFF (High-Bits Index 2) beim Import begrenzt, wurde nicht bis
   zum Schreibpfad verfolgt — für den Differenzlauf als Grenzfall im
   Korpus vorgesehen (Abschnitt 6), Urteil offen.
5. **MMB-Bedarf** (F7): ob Archive MMB-Bestände anliefern, weiß nur
   der Eigentümer — Fundus bis dahin.
6. **dfsimage-Testtiefe:** nur eine Testdatei
   (`tests/test_create_image.py`, 2 Tests) — das Oracle-Vertrauen
   stützt sich auf Code-Lektüre + Differenzlauf mit dritter Hand
   (DIMConsole), nicht auf die eigene Suite.

## 12. Verdikt für die Negativliste

`bewertet` — Gutachten liegt vor; MIT (GRÜN) + PSF-Vendoring
(PRÜFEN, folgenlos); wertvollster Ertrag: **zweite unabhängige
Acorn-DFS-Hash-Hand** mit MIT-Korpus-Erzeugung + die
Bereitschafts-Messung F6 (drei türlose Acorn-Module im eigenen Baum).
