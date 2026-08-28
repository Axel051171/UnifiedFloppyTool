<!-- uebernommen: MF-648 -->
# Gutachten: Datamuseum-DK/FloppyTools

Stand: 2026-08-28 · Messung: `work/FloppyTools.messung.json`
(HEAD `769ca9f80b`, letzter Commit 2026-08-16) · Inventar: UFT `cf5fa96f`
(`work/inv.json`, gebaut 2026-08-28 17:41)

**Zwei Klone, eine Messbasis.** Der beauftragte GitHub-Spiegel
(`github.com/Datamuseum-DK/FloppyTools`) trägt seit dem letzten Commit
`3f8255f` (2026-03-25) nur noch einen Umzugshinweis: „FloppyTools have
moved … codeberg.org/Datamuseum-dk/FloppyTools" (README.md:1-3).
Gemessen wurde deshalb der lebende Upstream auf Codeberg
(`work/FloppyTools-codeberg`, HEAD `769ca9f`, 2026-08-16). Beide führen
dieselbe Lizenzdatei (SHA-256 beider `LICENSE.md` identisch:
`8b305ab1…`). Der Entwurf aus `gutachten.py` lief regulär durch
(keine Ratenbremse-Verweigerung); seine Mechanik ist hier eingearbeitet,
die Datei `out/FloppyTools-codeberg.gutachten.md` ist durch dieses
Gutachten ersetzt.

**Wer dahintersteht:** Datamuseum.dk (dänisches Computermuseum), Autor
Poul-Henning Kamp. Kein Hobby-Konverter, sondern das Arbeitswerkzeug
eines Archivs, das unbekannte Formate selbst reverse-engineert
(README.md:9-14) — dieselbe Mission wie UFT.

## Messwerte (aus `FloppyTools.messung.json`)

- 34 Dateien, davon 31 `.py` (4785 Zeilen, gemessen mit
  `wc -l $(find floppytools -name '*.py')`), 2 `.md`
- 16 Format-Module (`floppytools/formats/`), 8 Basis-Module
  (`floppytools/base/`), 1 CLI (`main.py`, 293 Zeilen)
- Domänen-Score 15 (Schwelle 3)
- Reine Python-Abhängigkeit: `crcmod` (setup.py)

## Lizenz (aus der Datei, nicht dem README)

- `LICENSE.md` (Repo-Wurzel, einzige Lizenzdatei; `find`-Lauf über den
  Klon, keine Unterverzeichnis-Lizenzen): **BSD-2-Clause**,
  © 2023 Poul-Henning Kamp
- **Zone: GRÜN** → Code portierbar mit Attribution (samdisk-Muster),
  Konzept-Nachbau frei, Oracle-Nutzung frei
- **Attribution (MF-636):** Jede Übernahme trägt
  „Quelle: Datamuseum-DK/FloppyTools (Codeberg), Commit `769ca9f`,
  Lizenz BSD-2-Clause" im Header. Für das Beispiel-Repo
  (FloppyToolsExamples): **BSD-2-Clause**, © 2024 Datamuseum-DK
  (LICENSE, Wurzel) — aber siehe Inhalts-Lizenzurteil unter
  „Beschaffung".

## Was das Archiv an VERFAHREN beiträgt

Das ist der eigentliche Wert dieses Repos — weniger die Decoder als die
dokumentierte Archiv-Praxis. Alles mit Fundstelle:

1. **Unread-Sentinel statt Null-Füllung.** Nicht gelesene Sektoren
   werden im ausgegebenen Abbild mit dem greppbaren Muster `_UNREAD_`
   gefüllt (`base/media.py:163`:
   `fill = (b'_UNREAD_' * (sector_length // 8 + 1))[:sector_length]`),
   und die Fehlliste wandert zusätzlich in die Metadaten. UFT füllt
   im D64-Pfad mit `0x00` (`src/formats/d64/uft_d64_parser_v3.c:1489`,
   `params->fill_pattern = 0x00`) — eine Null ist von echten Nulldaten
   nicht unterscheidbar. Das Sentinel-Verfahren ist die konsequentere
   Fassung von „keine erfundenen Daten".
2. **Manifest je Medium (DDHF-Bitstore `.meta`).**
   `base/media.py:181-232` schreibt pro Diskette: Metadaten-Version,
   Zugriffsstatus, gemessene Geometrie (aus den Lesungen, nicht aus der
   Formatdefinition), Transkription des physischen Etiketts
   (`labels.txt`-Mechanismus, media.py:236-254), Museums-Artefaktnummer
   (QR 50000000–50999999), Formatname, Liste der ungelesenen Sektoren.
   Das ist ein gelebtes Provenienz-Manifest — Vergleichsstoff für
   UFTs Provenienzkette (`src/forensic/uft_provenance.c`), die Hashes
   und Aktionen führt, aber keine physische Etikett-Transkription und
   keine Fehlstellenliste im Manifest.
3. **N:M-Modell physische/logische Adresse.** Jeder Sektor ist unter
   dem 6-Tupel `phys_chs + am_chs` abgelegt (`base/media_abc.py:59-73`,
   Kommentar nennt ausdrücklich Kopfversatz und Kopierschutz-Duplikate
   als Grund); eine AM/Phys-Abweichung wird geloggt, nie still
   korrigiert (media_abc.py:238-241, „AM MISMATCH").
4. **Supermajoritäts-Regel beim Voting.** Ein Mehrheitswert gilt erst
   bei `majority > 2 * minority` (`base/media_abc.py:142`), sonst
   bleibt der Sektor als Konflikt sichtbar (Statuszeichen `O`/`╬`).
   Konservativer als einfache Pluralität; UFTs gewichtetes Voting ist
   ein anderes Verfahren — kein Überlegenheitsurteil ohne
   Differenzlauf, darum nur Fundus.
5. **Gezieltes Nachlesen als Workflow.** Monitor-Modus (`main.py -m`)
   verarbeitet Streams, während DTC sie schreibt; das Beispiel-Korpus
   zeigt die Praxis: Lesung `000` deckt alle Spuren, die Folge-Lesungen
   enthalten nur noch Problem-Spuren (gemessen: 35
   Lesungs-Verzeichnisse, zusammen 173 `.raw`-Dateien — wären alle
   Lesungen vollständig, wären es 35×74). Abbruchkriterium ist
   menschlich dokumentiert: „After trying to read those three tracks
   35 times, I'm pretty sure more readings will not recover…"
   (FloppyToolsExamples README).
6. **Manuelle Sektor-Rettung als dokumentiertes Skript.** Die
   Reparatur einzelner Sektoren (Byte-Bruteforce gegen die CRC,
   fehlende Adressmarke aus Nachbarsektor rekonstruiert) liegt als
   kommentiertes, nachvollziehbares Skript neben dem Korpus
   (`q1_repair_c7s3.py`, `q1_repair_missing_am.py`) — der Eingriff ist
   Teil der Überlieferung, nicht versteckt.

## Funde

### F1 — DG-Nova-Geometrie: UFTs Plugin widerspricht dem Archiv (Kategorie: Daten/Korrektur)

FloppyTools dekodiert echte Nova-Disketten des Museums mit
**77 Zylinder × 1 Kopf × 8 Sektoren (0–7) × 512 Byte, FM**
(`formats/dg_nova.py:15-16`: `SECTOR_SIZE = 512`,
`GEOMETRY = ((0,0,0),(76,0,7),512)`), inkl. der dokumentiert kuriosen
CRC (`bogo_crc`, dg_nova.py:56ff, „The worlds second worst CRC-16").

UFTs `src/formats/minicomputer/uft_dg_nova.c:26-31` behauptet dagegen
u. a. `77 Trk × 26 Sek × 128 B = 256256` als „DG Nova 8\" SS/SD" —
**das ist byteidentisch die RX01/IBM-3740-Geometrie**
(`src/formats/pdp/uft_pdp.c:5`). Kein einziger der fünf UFT-Einträge
nennt eine Referenz (`grep -i "ref\|bitsavers\|source:"` über die Datei:
0 Treffer außer einem Opcode-Kommentar), und `dg_nova` fehlt in
`docs/VERIFICATION_TIERS.md` (`grep -i nova`: 0 Treffer). Das trägt die
Signatur der bekannten Copy-Paste-Fabrikation im Format-Layer
(FMT-2/3/10/11/12-Muster). Methode des Vergleichs: die zwei
Geometrie-Tabellen beider Dateien nebeneinandergelegt, Treffer =
identisches (Trk, Sek, Größe)-Tripel; Ergebnis: kein UFT-Eintrag
stimmt mit der Archiv-Messung überein.

Vorsicht in beide Richtungen: FloppyTools belegt die Geometrie **der
Museums-Disketten**, nicht aller je gebauten Nova-Formate. Aber ein
Plugin, das RX01-Abbilder mit Konfidenz 30+ als „DG Nova" annimmt und
keine Quelle nennt, ist mindestens irreführend.

### F2 — FloppyTools als freies KryoFlux-Strom-Oracle (Kategorie: Oracle)

- **Konsole/skriptbar:** ja — `python3 -m floppytools -d <dir>
  [streams…]` (`main.py:110-137`), kein GUI-Zwang, `-t` nur für
  TTY-Kosmetik.
- **Ausgeführt, nicht zugesichert:** heute auf dem Q1-Beispielkorpus
  gelaufen (3 von 35 Lesungen, `PYTHONUTF8=1`, rc=0); erzeugt
  `.status` mit Sektor-Statuskarte je CHS und `.cache`. Windows-Falle
  dokumentiert: ohne `PYTHONUTF8=1` stirbt der Trace-Writer an cp1252
  (`UnicodeEncodeError` in media.py:71) — gemessen, reproduzierbar.
- **Hashes je Datei:** nein; aber deterministische `.BIN`-Ausgabe
  (media.py:130-166) → Stufe „stärker" des Differenzlauf-Standards
  (Bytes herausholen und selbst hashen, `docs/ORACLES.md` §MF-629).
- **Herkunfts-Anker:** keine Versionsabfrage →
  `version_is_unaskable` + SHA-256 bzw. Git-Commit `769ca9f`.
- **Der eigentliche Hebel:** UFTs einziges KryoFlux-Strom-Oracle ist
  heute `dtc` — **proprietär, nur Ausführung** (`docs/ORACLES.md`
  Registrierte Oracles, Zeile `dtc`). FloppyTools ist ein
  unabhängiger, quelloffener Zweitleser desselben Stromformats
  (`base/kryostream.py`, eigene OOB-/sck-/ick-Auswertung) — eine
  „zweite Hand" im Sinn von MF-644 für den KryoFlux-Lesepfad
  (`src/flux/uft_kryoflux_stream.c`), der in
  `docs/VERIFICATION_TIERS.md` nicht vorkommt (`grep -i kryoflux`:
  0 Treffer) und dessen Tests bisher Emulator-generiert sind
  (`tests/emulators/kryoflux/`).

### F3 — Frei verteilbares Multi-Lesungs-Flux-Korpus (Kategorie: Daten)

`github.com/Datamuseum-DK/FloppyToolsExamples`: **35 Lesungen einer
realen, degradierten 8"-Diskette (Q1 MicroLite), 173
KryoFlux-Stream-Dateien, 44 MB** (gemessen: `ls q1 | grep -c '^0'` =
35, `find q1 -name '*.raw' | wc -l` = 173, `du -sh q1` = 44M). Die
Dateien sind echte KryoFlux-Streams (OOB-Block + „host_date=…,
name=KryoFlux…" im Hexdump der ersten 64 Byte). Herkunft dokumentiert
bis zum Museums-Wiki (https://datamuseum.dk/wiki/Q1_Microlite).

UFTs Korpus (22 Einträge in `inv["korpus"]`, vollständig durchgesehen)
enthält **keinen einzigen KryoFlux-Stream** und **kein einziges
Multi-Lesungs-Set** — alle Voting-/Multirev-Tests laufen auf
synthetischem Material. Dieses Set wäre beides zugleich, mit echter,
dokumentierter Degradation (3 bekannte Defekt-Sektoren: c39h0s18,
c41h0s77, c7h0s3 laut Beispiel-README).

**Inhalts-Lizenzurteil (nicht nur Repo-Lizenz):** Das Repo ist
BSD-2-Clause (LICENSE, © 2024 Datamuseum-DK) — das deckt die
Verteilung der Dateien durch das Museum. Der **Disketteninhalt** ist
aber Q1-Corporation-Software der 1970er; deren Urheberrecht hat das
Museum nicht lizenziert und kann es nicht. Zone: **PRÜFEN** →
Eigentümer-Vorlage (Regel 8). Gangbarer Weg innerhalb bestehender
UFT-Politik: **nicht vendoren**, sondern Manifest-Eintrag mit SHA-256 +
Bezugsweg (Korpus-Politik in `docs/VERIFICATION_PLAN.md:128-135`:
Dritte beschaffen selbst, prüfen gegen Hashes). Damit verteilt UFT
nichts, was es nicht verteilen darf.

### F4 — Zilog MCZ: benannte Referenz liegt vor, UFT hat keine (Kategorie: Daten)

FloppyTools nennt im Header die Primärquelle:
`formats/zilog_mcz.py:7`:
`ref: 03-3018-03_ZDS_1_40_Hardware_Reference_Manual_May79.pdf`.
UFTs `src/formats/zilog/zilogmcz.c` nennt **nichts** (`grep -i
"ref\|manual\|source"`: 0 Treffer) und ist ein reiner
Größen-Prüfer (Datei == 77×32×132 → Konfidenz 75). Dazu eine
Zahlen-Differenz, die nur die Quelle entscheiden kann: FloppyTools
speichert je Sektor 136 Byte (SECTOR_SIZE=136, zilog_mcz.py:20;
Nutzlast = 2 Adressbytes + 134, CRC über alles, zilog_mcz.py:36-44),
UFT rechnet mit 132. Ob 132 die Nutzlast ohne Adressbytes ist oder
eine Fabrikation, steht im Manual — nicht in einem der beiden Repos.
`zilogmcz` fehlt ebenfalls in `docs/VERIFICATION_TIERS.md`
(`grep -i zilog`: 0 Treffer).

### F5 — M²FM-Dekodierung existiert in UFT nicht (Kategorie: Innovation/Fundus)

UFT kennt `UFT_ENC_M2FM` nur als Enum und Namenstabelle
(`include/uft/formats/uft_format_params.h:169`,
`src/core/uft_ir_format.c:1605`, `src/core/uft_unified_types.c:126`;
`grep -rn -i m2fm src/decoder src/flux src/algorithms`: keine
Implementierung). FloppyTools hat zwei lauffähige M²FM-Pfade mit den
konkreten Parametern: `ClockRecoveryM2FM` mit den vier
Intervallklassen 1/1,5/2/2,5 × Rate (`base/fluxstream.py:104-112`),
Intel-ISIS-Marken `(0x87,0x70)`/`(0x85,0x70)` mit xmodem-CRC
(`formats/intel_isis.py:24-40`), DEC-RX02-Sonderweg (FM-Adressmarke
`(0xc7,0xfe)` im MFM-Fluss, `formats/dec_rx02.py:22-43`). BSD-2 →
später sogar portierbar. **Aber:** neuer Decoder-Code fällt unter die
Einfrier-Regel (Moratorium, 1:2) und bewegt heute keine Kennzahl →
Fundus, kein Vorschlag.

### F6 — Acht Formate, die UFT nicht hat (Kategorie: Fundus)

Inventar-Abfrage (SSOT-gedeckt, `plugin_liste_vollstaendig: true`,
daher heißt „kein Treffer" hier wirklich „nicht vorhanden"):
`RX02`→false*, `ISIS`→false, `ohio scientific`→false, `wang`→false**,
`q1`→false, `lexitron`→false, `philips`→false, `hp9885`→false,
`alpha lsi`→false; dagegen `zilog`→**vorhanden** (`zilog`, `zilogmcz`)
und `nova`→**vorhanden** (`uft_dg_nova`).
Handnachprüfung im Baum (Regel 4): *RX02 existiert als
Sektor-Abbild-Parser (`src/formats/pdp/uft_pdp.c:101-102`), nicht als
Flux-Decoder; **„Wang" in UFT ist der Wang Professional (CP/M-PC,
`src/formats/cpm/uft_cpm_diskdefs.c:1017-1020`), nicht die
WANG-WCS-Textmaschine. Alles Übrige: echt nicht vorhanden — und wegen
des Moratoriums bewusst **kein** Vorschlag; die Spec-Quellen liegen
mit diesem Gutachten benannt im Fundus.

### F7 — Experimentelle 2D-PLL (Kategorie: irrelevant, mit Grund)

Commit `11a9d45` (2026-04-05) fügt einen Parameter-Sweep-Leser in
`ibm.py` ein (Tripel `sigma_e/sigma_r/sigma_p`, handgestimmt je
Museums-Artefakt, hartkodierte `/tmp`-Pfade, `if False`-Blöcke).
Verwandt mit UFTs Adaptive Decode (±33 %-PLL-Re-Decode), aber als
Wegwerf-Experiment gekennzeichnet — nichts zum Nachbauen.

## Vorschläge (4 von max. 5 — Ratenbremse eingehalten)

> Nummerierung bewusst offen gelassen: drei weitere Scout-Zyklen laufen
> parallel; SCOUT-Nummern (zuletzt vergeben: SCOUT-37) vergibt der
> Übernehmende kollisionsfrei.

**V1 — DG-Nova-Plugin gegen die Archiv-Messung stellen.**
UFTs `uft_dg_nova.c:26-31` führt fünf Geometrien ohne Referenz; die
erste (77×26×128=256256) ist byteidentisch RX01
(`uft_pdp.c:5`), während das Datamuseum echte Nova-Disketten mit
77×1×8×512 FM dekodiert (`FloppyTools formats/dg_nova.py:15-16`,
BSD-2). Erlaubte Arbeit unter der Einfrier-Regel
(Spec-Korrektur gegen autoritative Quelle, `docs/VERIFICATION_PLAN.md`
§Einfrier-Regel „Erlaubt bleiben"); Weg für Stufe 4: dg_nova in den
Tier-Ledger aufnehmen, Rotbeweis = RX01-Abbild (256256 B) wird heute
als „DG Nova" mit Konfidenz ≥30 angenommen, dann Geometrie-Tabelle
gegen bitsavers-DG-Doku + FloppyTools-Verhalten berichtigen, Referenz
in den Header. **Kennzahl: ungeprüfte Formate (T3) runter** (setzt
Ledger-Aufnahme voraus — heute ist dg_nova nicht einmal gezählt).
Einhängepunkt: `docs/VERIFICATION_PLAN.md` (Register: ankerfähig).
Aufwand: S.

**V2 — FloppyTools als „zweite Hand" für den KryoFlux-Lesepfad
vormerken.** Eintrag unter „Vorgemerkt, noch nicht registriert" in
`docs/ORACLES.md`: entscheidet „liest UFTs
`src/flux/uft_kryoflux_stream.c` reale KryoFlux-Streams richtig?" —
heute hängt dieser Pfad allein am proprietären `dtc`. Beleg statt
Zusicherung: heute auf dem Q1-Korpus gelaufen (rc=0, `.status` mit
Sektorkarte; Auflage `PYTHONUTF8=1` unter Windows, gemessener
cp1252-Crash in media.py:71 sonst). Herkunfts-Anker:
`version_is_unaskable` + Commit `769ca9f`/SHA-256; Lizenz BSD-2,
verglichen wird nur Ausgabe. **Kennzahl: ungeprüfte Formate (T3)
runter** (Instrument für die Hebung des KryoFlux-Pfads, der im
Tier-Ledger fehlt — `grep -i kryoflux docs/VERIFICATION_TIERS.md` = 0).
Einhängepunkt: `docs/ORACLES.md` §Vorgemerkt (Register: ankerfähig
über VERIFICATION_PLAN). Aufwand: S.

**V3 — Q1-Multi-Lesungs-Korpus per Manifest aufnehmen (nicht
vendoren).** 35 Lesungen / 173 KryoFlux-Streams / 44 MB einer realen
degradierten 8"-Diskette mit dokumentierter Herkunft und drei bekannten
Defekt-Sektoren (FloppyToolsExamples, Repo BSD-2; **Inhalt Zone
PRÜFEN** → Eigentümer-Vorlage, darum Manifest+SHA-256+Bezugsweg nach
bestehender Korpus-Politik `docs/VERIFICATION_PLAN.md:128-135` statt
Kopie). Erster echter KryoFlux-Strom- UND erster echter
Multi-Lesungs-Bestand im Korpus (heute: 0 von 22 Einträgen, Methode:
`inv["korpus"]` vollständig durchgesehen). **Kennzahl: begründete
fünfte — Korpus-Umfang** (real-degradiertes Material für
Voting/Multiread, das heute nur synthetisch getestet wird).
Einhängepunkt: `docs/VERIFICATION_PLAN.md` §Phase 2 Korpus + Hebungen.
Aufwand: S.

**V4 — Zilog-MCZ-Referenz nachtragen und die 132/136-Frage
entscheiden.** UFTs `zilogmcz.c` hat keine benannte Referenz und
weicht in der Sektorgröße von FloppyTools ab (132 vs. 136 Byte
Nutzlast; Fundstellen oben, F4). Beschaffung: das von FloppyTools
zitierte Manual `03-3018-03_ZDS_1_40_Hardware_Reference_Manual_May79`
(bitsavers), dann Header-Referenz + ggf. Korrektur; FloppyTools dient
als Verhaltens-Zweitquelle (BSD-2). **Kennzahl: ungeprüfte Formate
(T3) runter** (mit Ledger-Aufnahme wie V1). Einhängepunkt:
`docs/VERIFICATION_PLAN.md` §Einfrier-Regel (Spec-Korrektur).
Aufwand: S.

## Differenzlauf-Plan (für V2, Pflicht nach Regel 7)

- **Binaries:** `python -m floppytools` (Codeberg `769ca9f`, BSD-2)
  gegen UFT-KryoFlux-Pfad (`uft_kryoflux_stream.c` + Decoder, über
  einen bestehenden Test-Treiber — kein CLI, UFT ist GUI-only).
- **Korpus:** Schnittmenge der Fähigkeiten ist IBM-FM/MFM, C64-GCR,
  Apple-GCR (FloppyTools-Formatliste README:29-53 vs. UFT-Decoder).
  Fixtures: bestehende gw-erzeugte SCP-Abbilder mit
  `gw convert` in KryoFlux-Stream-Sätze wandeln (Namensschema
  `…NN.H.raw`, das `kryostream.py:27-36` erwartet) — **ob gw-Ausgabe
  von FloppyTools akzeptiert wird, ist zu messen, nicht zuzusichern**
  (UNGEKLÄRT U3). Zusätzlich das Q1-Set aus V3 als Nur-FloppyTools-
  Regressionsanker für den Stream-Parser (OOB/sck/ick).
- **Metrik:** je Sektor Byte-Identität zwischen FloppyTools-`.BIN`
  (Sektorordnung = deklarierte Geometrie, media.py:130-166) und
  UFT-Sektorausgabe; Gesamtwert = Anteil identischer Sektoren.
- **Toleranzliste:** (a) FloppyTools verwirft Mehrheiten ≤ 2×Minorität
  → Sektoren mit `_UNREAD_`-Füllung aus der Wertung nehmen und
  gesondert zählen; (b) FloppyTools schreibt keine Flux-Metadaten
  zurück — verglichen wird nur die Sektorebene; (c) cp1252-Auflage
  unter Windows (PYTHONUTF8=1).

## Beschaffungsliste (gegen `inv["korpus"]` geprüft: nichts davon liegt bereits)

| Was | Woher | Lizenz Repo | Lizenz Inhalt | Zweck |
|---|---|---|---|---|
| Q1-Multi-Lesungs-Set (173 `.raw`, 44 MB) | github.com/Datamuseum-DK/FloppyToolsExamples | BSD-2 (LICENSE) | **PRÜFEN** — Q1-Corp-Inhalt, Eigentümer-Vorlage; Manifest-Weg umgeht Verteilung | V3, V2-Regressionsanker |
| ZDS-1/40-Hardware-Manual `03-3018-03` (Mai 1979) | bitsavers (von zilog_mcz.py:7 zitiert) | Scan, bitsavers-üblich | Doku, nur als Referenz gelesen | V4 |
| DG-Nova-Floppy-Doku (Controller-Handbuch) | bitsavers, Suchauftrag | dito | dito | V1 |
| KryoFlux-Stream-Sätze IBM/C64/Apple | selbst erzeugen aus vorhandenen gw-SCPs via `gw convert` | Unlicense (gw) | eigenerzeugt, frei | Differenzlauf V2 |

## UNGEKLÄRT

- **U1:** Ob 132 oder 136 Byte je Zilog-MCZ-Sektor die Manual-Wahrheit
  ist — entscheidet nur das ZDS-Manual (V4).
- **U2:** Ob es je DG-Nova-Varianten mit 26×128 gab (UFTs Tabelle wäre
  dann teilrichtig) — entscheidet nur DG-Primärdoku; die
  Archiv-Messung deckt nur die Museums-Disketten.
- **U3:** Ob `gw convert` KryoFlux-Streams erzeugt, die
  `kryostream.py` akzeptiert (Namensschema + OOB) — Messung vor dem
  Differenzlauf, nicht zugesichert.
- **U4:** Rechtsstatus des Q1-Disketteninhalts — Eigentümer-Vorlage
  (Regel 8); bis dahin gilt der Manifest-Weg.
- **U5:** Ob die `_UNREAD_`-Sentinel-Idee in UFT einen Träger hat
  (welcher Schreibpfad, welches Format verträgt Nicht-Null-Füllung
  ohne Nebenwirkung) — Verfahren notiert (Abschnitt Verfahren, Punkt
  1), bewegt heute keine Kennzahl → Fundus.
- **U6:** FloppyToolsExamples-README beschreibt teils veralteten Stand
  („Old outdated info", 20250728/phk) — Defekt-Sektorliste vor
  Verwendung gegen einen frischen Volllauf verifizieren.

## Negativlisten-Status

Repo war weder `verworfen` noch `bewertet` (grep über
`data/known_negatives.json`: 0 Treffer für floppytools/datamuseum) —
Erstbewertung, kein Neubesuchs-Anlass nötig. Eintrag `bewertet` wird
mit diesem Gutachten gesetzt.
