# Offene Punkte — eine Liste (MF-508)

**Stand:** 2026-08-23 · **Basis:** HEAD nach MF-507
**Zweck:** alle offenen Listen dieses Baums an *einer* Stelle, nach einem
Maßstab sortiert, der nicht Geschmack ist.

Zusammengeführt aus: `KNOWN_ISSUES.md` (49 offene Einträge),
`MASTER_PLAN.md`, `MAMMUT_PLAN.md`, `VERIFICATION_PLAN.md`,
`STUB_ELIMINATION_PLAN.md`, den beiden Integrations-TODOs, 58
`TODO`/`FIXME`-Marken im Quelltext — plus den Befunden eines
Übersetzerlaufs und eines Erreichbarkeits-Audits, die hier zum ersten Mal
stehen.

---

## Status-Marke je Abschnitt (MF-694)

Jeder `##`-Abschnitt darf **eine** maschinenlesbare Statuszeile tragen,
unmittelbar unter der Ueberschrift:

```
<!-- status: offen -->
<!-- status: wartet-eigentuemer(2026-08-30) -->
<!-- status: erledigt(MF-693) -->
```

**Warum sie existiert.** Der Tore-Sekretaer
(`tools/uft-innendienst/scripts/sekretaer.py`) las bis MF-693 nur
Tabellenzeilen und sah Prosa-Abschnitte nicht — auch nicht solche, die
ausdruecklich eine Eigentuemer-Entscheidung ueberschreiben. Der
naheliegende Ausweg, ein Prosa-Scan nach Woertern wie „Eigentuemer",
ist gemessen **schlechter als die Luecke**: von 65 Abschnitten tragen
36 ein solches Signal, die meisten davon in **erledigten** Vorgaengen.
Der Zettel waere von 21 auf ueber 50 Posten gewachsen.

**Warum nicht automatisch vergeben.** Auch die Ueberschrift taeuscht:
17 Koepfe enthalten ein Wort wie „erledigt" oder „geschlossen", aber
„GUI-6 — 38 Bedienelemente in einer **geschlossenen** Schleife" ist
kein abgeschlossener Vorgang, und „**Uebernommen** aus BACKLOG.md" sagt
nichts ueber die uebernommenen Punkte. Eine Marke, die falsch gesetzt
wird, ist schlimmer als keine — sie schliesst Vorgaenge still.
Vergeben wird darum **von Hand, je Abschnitt, mit Begruendung im
Commit**.

**Das Datum bei `wartet-eigentuemer`** ist Pflicht: ohne es kann der
Sekretaer Entscheidungsschulden nicht altersgestaffelt mahnen, und
genau das ist seine Aufgabe.

**Unmarkiert heisst „noch nicht gesichtet"**, nicht „offen" und nicht
„erledigt". Der Sekretaer meldet die Zahl der unmarkierten Abschnitte
als **eine** Zeile — sichtbar und zaehlbar, statt still.

**Zwei Leser stehen ab Tag eins bereit**, damit dies nicht die fuenfte
„Vereinbarung ohne Leser" wird: der Sekretaer wertet die Marke aus, und
`tools/uft-innendienst/scripts/widerspruch.py` schlaegt an, sobald sie
in einem Dokument steht und niemand sie liest.

## 0. Der gemessene Ist-Stand

| | |
|---|---|
| Tests | **248/248 grün** |
| Skelett-Header | **0** |
| Konsistenz-Tore | **25 Kategorien, 0** |
| Fuzz ueber `uft_disk_open()` | 431 Eingaben, **0 Abstürze** (nach 7 Korrekturen) — **103 von 137** Plugins erreicht, 34 nie |
| Fuzz ueber `write_track()` | 360 Schreibversuche, **0 ausserhalb der Geometrie** (vorher 29) |
| qmake-Release | **baut und linkt** |
| Gebaute Module mit exportierten Funktionen | 537 |
| davon **von niemandem aufgerufen** | **228 (42 %)** |
| Format-Plugins | 88 |
| davon **Stufe T3 = ungeprüft** | **57 (65 %)** |
| Geteilte Include-Wächter mit abweichendem Inhalt | **37** (von 40) |
| `TODO`/`FIXME`-Marken | 58 |
| Warnungen unter strengen Klassen (C-Kern) | 311 |

---

## 1. Warum die Frage „sind alle Stubs weg?" am Problem vorbeizielt

Skelett-Header: 0. Stubs im engeren Sinn: praktisch keine. Und trotzdem
sind **zwei Fünftel der Modulfläche unerreichbar**.

**Stubs sind nicht das Problem — sie sind das Gegenteil davon.** Ein Stub
sagt die Wahrheit über sich: „nicht implementiert". Was dieser Baum hat,
ist die gefährlichere Sorte: Code, der fertig aussieht, übersetzt, grüne
Tests hat — und den nichts erreicht.

Daraus folgt der Maßstab, nach dem diese Liste sortiert ist. Nicht
„Aufwand", nicht „Alter", sondern:

> **Priorität = Risiko, dass das Werkzeug dem Benutzer etwas Unwahres
> sagt.**

Das ist der einzige Maßstab, der zur Mission passt („Kein Bit verloren.
Keine stille Veränderung. **Keine erfundenen Daten.**") — und er ordnet
die Liste anders, als eine Feature-Liste es täte.

Eine grüne Testsuite über unerreichbarem Code ist kein Qualitätsnachweis.
Sie ist der Beweis, dass Tests und Produkt verschiedene Codepfade
benutzen.

---

## P0 — das Werkzeug behauptet etwas, das nicht stimmt

> **Stand der Abarbeitung:** 10 von 14 geschlossen. Offen sind 4, davon
> **keiner** aus Mangel an Arbeit: einer braucht Hardware, einer eine
> Entscheidung des Eigentuemers, einer ist ein gemessener Rueckstand, und
> einer haengt am Verifikations-Moratorium (MF-498) — unerreichbaren Code
> anzuschliessen waere genau die Wette, die dieses Kapitel aufraeumt.
>
> Sortiert nach Nummer; die Nummer ist die Reihenfolge der Entdeckung,
> nicht der Wichtigkeit.

| # | Punkt | Stand |
|---|---|---|
| P0-1 | **Erfundene Konfidenzzahlen** in der Kopierschutz-Anzeige: `85 %`, `70 %`, `60 %` waren hartkodierte Literale unter der Spaltenüberschrift „Confidence" | ✅ **behoben (MF-508)** |
| P0-2 | **„55+ Kopierschutz-Schemes"** steht als Kernfunktion in `CLAUDE.md`/`README` — das Erkennungs-Subsystem (`src/protection/`, 20 Dateien, ~200 Funktionen) hat **keinen Aufrufer**. Die Oberfläche zeigt stattdessen eine Heuristik | ⚠ **ueberwacht, nicht verdrahtet.** Die Doku ist seit MF-508/509 ehrlich — `README.md` sagt woertlich „a function that exists is not a feature until something calls it", `CLAUDE.md` traegt die Ehrlichkeits-Notiz. Gemessen (MF-557): **38 Dateien, 350 Funktionen, 4 Aufrufer, 16 von einem Test beruehrt**. Seither haelt `scripts/audit_protection_claims.py` die Zahl fest. Den Katalog anzuschliessen hiesse, 334 ungeprueste Funktionen an ein forensisches Urteil zu haengen — die Lage, aus der die fuenf fabrizierten Parser kamen |
| P0-3 | **57 von 88 Formaten sind T3.** ~~Die Formatliste nennt sie ohne diese Unterscheidung.~~ **Diese Einstufung war zu hart:** `README.md` sagt es bereits ausdruecklich („Honest verification status … T1=2, T1b=12, T2=17, T3=57“). Ohne den Hinweis waren nur `CLAUDE.md` und `SHOWCASE.md` — letzteres nannte zusaetzlich eine veraltete Zahl (80 statt 137 Plugins) und „100 % Prinzip-7-Compliance“ ohne den Zusatz, dass das Metadaten-Vollstaendigkeit ist und keine Verifikation | ✅ **behoben (MF-509)** |
| P0-15 | **`SCP→G64` meldete 35 gewandelte Spuren und schrieb 789 Byte** | ✅ **behoben (MF-537)**: `d64_gcr_to_flux()` liefert SCP-Ticks, `scp_writer_add_track()` will Nanosekunden — 4000 ns wurden zu 150 ns; jetzt 252782 statt 789 Byte |
| P0-16 | **`HFE→ADF` liefert eine LEERE ADF.** 160 von 160 Spuren scheitern beim `read_track`, kein Sektor wird platziert — und heraus kommt eine Datei der richtigen Groesse. Die Zaehler melden es korrekt; wer sie nicht liest, haelt das Ergebnis fuer eine Wandlung | ✅ **behoben (MF-539)**: der Fehler lag in `ADF→HFE`, nicht in der Rueckrichtung — der HFE-Schreiber kodierte gar nicht (rohe Bytes statt Zellen, CRC nie berechnet, keine Bit-Spiegelung). `ADF→HFE` lehnt jetzt ab (kein AmigaDOS-Encoder im Baum); der IBM-Pfad ruft den vorhandenen `uft_mfm_encode_track()` und ist bitgleich |
| P0-4 | **POL-1: das Schreib-Sicherheitstor hat keinen Aufrufer.** Ein Tor, das nie läuft, ist eine Sicherheitszusage, die niemand einlöst | ✅ **behoben (MF-573)** — und die Einordnung war falsch. `uft_write_gate_precheck()` nimmt `image_data`, `image_len`, `snapshot_dir`, `snapshot_prefix`: **keine Hardware in der Signatur** (MF-572 gemessen). Sechs Stellen rufen es jetzt — `ExplorerTab::{onImportFiles,onImportFolder,onRename,onDelete,onNewFolder}` und `MainWindow::onSave()`. Schnappschuesse in `.uft-snapshots` neben dem Abbild; belegt durch `tests/test_write_gate_snapshot_survives.c` (Abbild zerstoert, Schnappschuss vollstaendig und byteweise gleich). **Nur der Laufwerks-Pfad bleibt hardwareblockiert.** Ein Posten, der als blockiert gefuehrt wird, wird nicht angefasst |
| ~~P0-5~~ | **LIC-1:** `uft_multiread_pipeline.c` trug `SPDX: MIT`, dokumentierte sich aber als Nachbau von a8rawconvs `sift_sectors` (GPLv2+) | ✅ **MF-580** — korrigiert auf `GPL-2.0-or-later`. Es war keine Präferenzfrage: die Angabe war auf **zwei** unabhängigen Wegen falsch — gegen die belegte Herkunft (Zeilenverweise bis `disk.cpp:331-343` stehen in der Datei) **und** gegen das Projekt selbst (`LICENSE` ist die GNU GPL v2). Sichtung über alle MIT-Dateien: keine weitere mit fremder Herkunft |
| P0-6 | **Drei Speicherfehler im DSK-Oeffnungspfad** — Heap-Ueberlauf (44830 B, gemessen), Stapel-Ueberlauf (1800 B) und ein **doppeltes free bei jeder abgeschnittenen DSK-Datei**. Gefunden vom neuen Fuzzer ueber `uft_disk_open()`, nicht durch Lesen | ✅ **behoben (MF-513)** |
| P0-7 | **Zwoelf `read_track` schrieben durch einen Nullzeiger** — `track->sectors[s] = ...` auf einem Zeiger, den `uft_track_init()` nicht anlegt. Woertlich derselbe kopierte Rumpf in 12 Plugins; alle zwoelf konnten nie funktionieren. Dazu MF-515: `edsk_parser_read_track` las ~29 KB hinter sein Feld | ✅ **behoben (MF-515/516)**, Tor gesetzt |
| P0-8 | **45 `read_track` indizierten mit `cyl`, ohne auf negativ zu pruefen** — `-1 >= tracks` ist falsch, `track_data[-1]` ein Zugriff vor dem Feld. Gefunden an OPUS, als der Fuzzer die Dateigroessen anbot, die die Sonden verlangen (aus den Sonden gelesen, 46 Werte) | ✅ **behoben (MF-518/519)** |
| P0-9 | **Der Schreibpfad nahm Koordinaten an, die es nicht gibt** — ADF und D81 ohne jede Schranke (aus 880 KB wurden 11 MB; `cyl=-1,head=2` schrieb ueber **Spur 0**), D64/ATR/XFD mit Schranke, die `UFT_OK` meldet ohne zu schreiben. 29 Befunde, kein einziger ein Absturz | ✅ **behoben (MF-522)** |
| P0-10 | **Ein eingeschlepptes `snprintf` ueberschrieb das der C-Bibliothek** — Parser von 1995 ohne `%z`; 10 Aufrufstellen unter `src/` betroffen, eine stuerzte ab. Dazu drei weitere Sanitizer-Befunde (Lesen hinter einer Zeichenkette, Verschiebung um 32 Stellen, Linksschieben auf `int`) | ✅ **behoben (MF-523/524)** |
| P0-11 | **Leck-Rueckstand — die Einordnung war falsch, zurueckgenommen (MF-592).** Hier stand: „im Produktionscode kein einziger Aufrufer, der eine gelesene Spur liegen laesst; der Rueckstand ist Testcode.“ Das stimmte fuer die Frage, die `audit_track_cleanup.py` stellt (fehlt ein AUFRAEUM-Aufruf?) — und ging an der Sache vorbei. Der geteilte Helfer `uft_format_add_sector_with_id()` alloziert einen Sektorpuffer, `uft_track_add_sector()` **kopiert** ihn (`uft_format_plugin.c:452-456`), und freigegeben wurde er nur im Fehlerfall. Also leckte **jeder gelesene Sektor** — 1,44 MB je Lesevorgang einer 1,44-MB-Diskette. Gemessen an `test_atr_512`: 2944 Byte in 8 Objekten. Wirkung im CI-Volllauf unter ASan: **58 von 266 auf 15 von 266**. Dabei gefunden und behoben: `uft_track_add_sector()` kopierte `confidence_map`, `weak_mask` und `timing_ns` **flach**, waehrend `uft_sector_cleanup()` sie freigibt — ein latentes doppeltes free im gemeinsamen Helfer | ✅ **0 offen (MF-599).** Im CI gemessen, jeder Schritt: **58 → 15** (MF-592, Produktionsleck im geteilten Sektor-Helfer) **→ 2** (MF-595 Steckplatz-Vorbelegung + `owns_data` an drei Erzeugern; MF-598 HFE) **→ 0** (MF-599: `uft_track_t` führt drei Flux-Zeiger, und die beiden Aufräumer deckten je einen anderen Satz ab — `uft_track_release()` gab ein nie belegtes Feld frei und ließ das belegte stehen). UBSan steht seit MF-594 ebenfalls bei **0 von 266**, beide scharfen Tore bei 9/9 |
| P0-12 | **„44 Konvertierungspfade“ beschreibt eine Tabelle, keine Faehigkeit** — angeboten werden **8** (2 verlustfrei, 6 mit Zustimmung); 33 weist das Preflight-Tor als UNGEPRUEFT ab, 3 als unmoeglich. Die Abweisung ist Absicht und richtig; die Zusage war es nicht | ⚠ **Zahlen seit MF-541 ABGELEITET, nicht gepflegt** (`scripts/update_inventory.py`, DERIVED_CLAIMS gegen `src/core/uft_roundtrip.c`). Stand nach MF-567: **12 angeboten**, davon **4 verlustfrei je mit Messung** (D64→D64, ADF→ADF, D64→G64, IMG→HFE), **8 nur mit Zustimmung**; 30 als UNGEPRUEFT abgewiesen, 2 als unmoeglich. Die von Hand gefuehrten Zahlen daneben („15 angeboten", „11 mit Zustimmung") waren nach MF-567 still falsch geworden und haengen seither ebenfalls an der Ableitung. MF-567 hat drei Matrix-Urteile ohne Wandler entfernt (SCP→IMD, IPF→ADF, STX→ST) |
| P0-13 | **Zwei Speicherfehler im Wandlungspfad** — die HFE-Spurtabelle ohne jede Schranke, und `lut[].length` (Gesamtlaenge beider Seiten) dreimal als Laenge je Seite verwendet: Absturz auf einer **gueltigen** Datei, 25080 Byte hinter dem Dateiende | ✅ **behoben (MF-526)** |
| P0-14 | **Die einzige Zusage ohne Zustimmung war unbelegt — und sie ist falsch.** `SCP<->HFE` stand als LOSSLESS; der jetzt existierende Bit-Identitaets-Test misst 25336 → 6400 Byte je Spur. Herabgestuft auf LOSSY_DOCUMENTED. **Folge: es gibt keine Wandlung mehr, die ohne Zustimmung laeuft** | ✅ **korrigiert (MF-527)**; die Ursache ist gefunden und behoben (MF-528: feste Spurlaenge 6400 statt aus dem Fluss — Datenrate statt Zellrate, dazu Seiten- statt Gesamtlaenge; Ergebnis 1025024 → 2008064 B) |

## P1 — stille Verfälschung oder wartender Build-Bruch

| # | Punkt | Stand |
|---|---|---|
| P1-1 | `session->audit_entries++` auf einem `void *` — schob den Listenzeiger je Logzeile um ein Byte, statt `audit_count` zu zählen. Heap-Korruption im Audit-Pfad | ✅ **behoben (MF-507)** |
| P1-2 | `uft_td0_to_imd`: `struct uft_imd_image_t` nur in der Parameterliste deklariert → der Prototyp beschrieb einen Typ, den es nirgends gibt, also konnte die Typprüfung **keinen** Aufrufer prüfen | ✅ **behoben (MF-507)** |
| P1-3 | `GetTempPathA` ohne `<windows.h>` — implizite Deklaration, bricht unter GCC 14+/Clang | ✅ **behoben (MF-507)** |
| P1-4 | `uft_geos_protection.c:367`: `%d` mit `size_t` — verschiebt alle folgenden Argumente; `info->name` würde als Zeiger von der falschen Stelle gelesen | ✅ **behoben (MF-509)**; der ganze Baum ist jetzt frei von Formatfehlern |
| P1-5 | ARCH-3: 22 Banner-Header sind wirklich unfertig, der Skelett-Audit sieht sie nicht | ⚠ **Zahl war falsch (MF-558).** Der Audit zaehlte 78, nicht 22 — und **67 davon waren leere Aufhaenger**, die sich nur als unfertig beschrifteten. Jetzt **11**, davon 8 mit wirklich offenen Prototypen. Die Zahl, um die es ging, war nie 22 und nie 78 |
| P1-6 | ARCH-21: 20 Altfälle von Header-Prototypen, die niemand einbindet | **offen** |
| P1-7 | ARCH-2/ARCH-4: 7 Header-Duplikate brauchen echte Zusammenführung. `UFT_SCP_SIGNATURE` ist **kein** Fall mehr — nachgemessen: zwei wertgleiche `#ifndef`-Defines, keine Abweichung | **offen** (verkleinert, MF-510) |
| P1-8 | **ARCH-26: 38 geteilte Include-Wächter mit abweichendem Inhalt** — zwei Header, ein `#ifndef`-Name, verschiedener Inhalt; die Include-Reihenfolge entscheidet still, welchen eine Übersetzungseinheit sieht. 14 davon verschlucken eine ganze Datei, 24 geben einem Typ zwei Layouts | ⚠ **15 behoben, 23 offen (MF-593).** Ein toter Header geloescht (`src/crc/uft_crc_polys.h`, null Einbinder), neun private Waechter auf `UFT_SRC_*` umbenannt — beide Bauten gruen, 266/266. **Klasse DATEI: 14 → 4 → 0 (MF-593).** Die letzten vier: zwei tote Header in `src/tools/` geloescht (kein `.c` daneben, kein Einbinder, nicht in der `.pro`); der leere IMD-Aufhaenger `formats/imd.h` trug den Waechter des echten `formats/uft_imd.h` und verschluckte ihn — mit einem Uebersetzungsvorgang belegt, der in dieser Reihenfolge `'UFT_IMD_MODE_500K_MFM' undeclared` meldet und in der umgekehrten durchlaeuft; und `uft_platform.h` trug einen zweiten Satz Endian-Funktionen unter dem Waechter von `uft_endian.h`, dem die vier 64-Bit-Funktionen fehlten (belegt: `implicit declaration of 'uft_read_be64'` + Linkfehler). Kein Aufrufer im Baum nutzte die Kopie — nachgemessen —, also gibt es jetzt eine Quelle statt zwei. Die 23 der Klasse TYP bleiben: dort haengen `UFT_FORMAT_ENUM_DEFINED` (vier Header) und `UFT_FORMAT_ID_T_DEFINED` (fuenf) — Zusammenlegen ist ABI-Arbeit, siehe C4 |

---

## P2 — beworbene Fähigkeit ist nicht erreichbar

**P2-1 ist der größte Posten dieser Liste: 228 gebaute Module ohne
Aufrufer.** Sie sind im Binary, kosten Bauzeit und Vertrauen, und
niemand kann sie benutzen.

> **Die Blutung ist gestoppt (MF-509).** `scripts/orphan_module_gate.py`
> friert die 228 als Grundlinie ein und ist die **23. Kategorie** in
> `check_consistency.py`. Ein *neues* verwaistes Modul lässt den Commit
> jetzt rot werden; ein behobenes meldet sich mit Namen zur Streichung.
> Aus einem unbegrenzten Problem ist ein begrenztes geworden.
>
> Warum nicht mehr: unerreichbarer Code ist per Definition ungeprüfter
> Code. Fünf Parser dieses Baums waren gegen erfundene Specs gebaut
> (FMT-2/3/10/11/12) — sie einfach anzuschließen hieße, dieselbe Wette in
> größerem Maßstab einzugehen. Der Weg führt über
> `VERIFICATION_PLAN.md`, nicht über einen Aufruf.

Die dichtesten Stellen:

| Verzeichnis | ohne Aufrufer | darunter |
|---|---|---|
| `src/protection/` | 20 | `uft_protection_detect.c` (20 Fn), `uft_protection_classify.c` (21 Fn), `uft_pc_protection.c` (18 Fn) |
| `src/formats/misc/` | 20 | |
| `src/formats/commodore/` | 13 | |
| `src/formats/atari/` | 12 | |
| `src/formats/flux/` | 11 | |
| `src/recovery/` | mehrere | `uft_recovery_meta.c` (**37 Fn**) |

Für jedes gilt genau eine von drei Entscheidungen — **verdrahten**,
**löschen**, oder **als unerreichbar dokumentieren**. Was nicht geht:
liegenlassen und weiter als Fähigkeit führen.

Dazu: ARCH-18 (`uft_xdf_api_impl.c`, zweite Formatschicht ohne Aufrufer),
ARCH-6 (zwei parallele Formatschichten), ARCH-23 (vier Schalter ohne
Verbraucher), ARCH-7 (CBM-Zonentabelle, 20 geprüfte Kopien, Migration
offen).

---

## P3 — unbelegte Zusagen

| # | Punkt |
|---|---|
| P3-1 | **57 Formate auf T3.** Das Moratorium (MF-363/498) verlangt: erst ATR, D64, ADF, FDI, NFD-r0 auf T1/T1b, danach 1:2 |
| P3-2 | Korpus-Beschaffung — **blockiert beim Eigentümer**: `cpmtools`, SAMdisks `tc.cpp`, `sector-cpc` (siehe `MAMMUT_PLAN.md` §5) |
| P3-3 | GUI-Rauchtest für MF-496/MF-501 — **blockiert beim Eigentümer** (kein Bediener in dieser Sitzung) |
| P3-4 | Tier-3-Hardware-Bench — **kein Gerät vorhanden** (MF-310), an die Gemeinschaft delegiert |
| P3-5 | Teilaufnahme-Karte nach ddrescue-Vorbild (Mammut §1.3, letzter offener Teil) |
| P3-6 | **ATR Enhanced Density:** beide ATR-Fassungen rechnen fest mit 18 Sektoren/Spur. Für ED (1040 Sektoren à 128 B) ergäbe das 58×18 statt 40×26. **Kein Datenverlust** — ATR ist ein lineares Sektorabbild, jeder Sektor bleibt erreichbar; falsch wäre nur die *gemeldete* Geometrie. Nicht geändert, weil keine **benannte Referenz** im Baum liegt (MF-498(a)) — gehört in die ATR-Hebung auf T1/T1b |

---

## P4 — Hygiene

- 58 `TODO`/`FIXME`-Marken; die ohne Issue-Verweis verstoßen gegen die
  eigene Regel in `.claude/CLAUDE.md`
- 311 Warnungen unter `-Wconversion`/`-Wsign-conversion` im C-Kern
  (überwiegend Rauschen, aber 41 × `-Wmissing-prototypes` deuten auf
  Funktionen, die `static` sein sollten — also auf P2-1)
- 10 × `-Wduplicated-branches` — die drei verdächtigsten **einzeln
  geprüft, keiner ist ein Fehler**:
  - `mac_dsk.c:43` `(sz==409600)?80:80` — Mac 400K *und* 800K haben 80
    Zylinder; der Ternär steht nur parallel zur `heads`-Zeile darunter
  - `uft_frz.c:428` `(status & 0x20) ? '-' : '-'` — Bit 5 ist im
    6502-Statusregister das **unbenutzte**; die übliche `NV-BDIZC`-Anzeige
    zeigt dort immer `-`
  - `atr.c:51` `(sectorSize==256)?18:18` — liegt in einer **zweiten,
    unerreichbaren** ATR-Fassung (die registrierte ist
    `src/formats/atr/uft_atr.c`); siehe P3-6
- **Ausdrücklich kein Fehler:** die drei identischen PETSCII-Zweige in
  `uft_d64_file.c`, `uft_t64.c`, `uft_bam_editor.c`. Redundant, aber
  korrekt — `'A'`–`'Z'` bleibt unverändert. Steht hier, damit niemand sie
  ein zweites Mal „findet"

---

## Was das für v4.1.6 heißt

Eine Version ist eine **Menge von Zusagen**. Dieser Baum kann heute
belegen:

- der Lesepfad für AmigaDOS-Flux (SCP→ADF) — mit gemessenen Grenzen
- 14 Formate auf T1/T1b
- der Aufnahme-Speicher samt Herkunft in beide Richtungen
- 238 grüne Tests, 0 Konsistenzverstöße, drei Plattformen im CI

Er kann heute **nicht** belegen:

- dass die 57 T3-Formate lesen, was sie zu lesen behaupten
- dass die beworbene Kopierschutz-Erkennung erreichbar ist
- dass das Schreib-Sicherheitstor je läuft

**Die ehrliche v4.1.6 sagt beides.** Ein Release, das nur die erste Liste
nennt, ist genau der Fehler, gegen den die Einfrier-Regel (MF-498)
geschrieben wurde — nur eine Ebene höher.


---

## Übernommen aus BACKLOG.md — Risiko-Ordnung (MF-588)

**`docs/OPEN_ITEMS.md` ist ab hier die EINZIGE Liste.** `BACKLOG.md`
verweist nur noch hierher; neue Befunde kommen hierhin, nirgendwo sonst.

Diese Einträge sind nicht nach Subsystem oder Aufwand sortiert, sondern
nach der Frage: **was passiert, wenn es falsch ist und niemand merkt es?**

| Stufe | Kriterium |
|---|---|
| **A** | Das Werkzeug sagt etwas Falsches über gesicherte Daten, und der Benutzer kann es nicht sehen |
| **B** | Das Werkzeug beschädigt oder verliert Daten, sichtbar |
| **C** | Das Werkzeug behauptet eine Fähigkeit, die es nicht hat |
| **D** | Das Werkzeug ist unvollständig, sagt es aber |

Ein Absturz ist Stufe **B**, nicht A — und das ist Absicht: ein Absturz
ist ein ehrlicher Fehler. Eine Diskette, die als „vollständig gesichert"
gemeldet wird und es nicht ist, wird nie bemerkt.

**Stand: 14 offen, 29 abgetragen** (die abgetragenen stehen mit ihrer
Messung in `BACKLOG.md`, das als Archiv erhalten bleibt).

| # | Sache | Herkunft | Stand |
|---|---|---|---|
| **A1** | **56 von 88 tier-geführten Formaten stehen auf T3 — ungeprüft.** (War 57; `mfi` ist mit MF-614 auf T2 gehoben.) Kein Test, oder ein synthetischer Test ohne Abgleich gegen eine autoritative Quelle. Genau in dieser Lage waren die fünf fabrizierten Parser grün. Belegt: T1=2, T1b=12, T2=17 | gemessen | **offen**, Moratorium MF-363/498 |
| **A2** | **29 von 44 Wandlungspfaden ungeprüft.** Das Preflight-Tor weist sie ab — richtig. Aber die Doku nennt weiter „44 Pfade" als Fähigkeit | gemessen | **offen**; Zahlen seit MF-541 abgeleitet |
| **A3** | Header-Prototypen ohne Definition | gemeldet, dann gemessen | ⚠ **294 → 237 (MF-549)**: die 2 gefaehrlichen Faelle (von `static inline` im selben Header gerufen) sind weg, dazu zwei Header mit 809 Zeilen und 0 Implementierung. **237 bleiben** — davon 40 in zwei Tests, die in EXCLUDED_TESTS stehen |
| **C1** | **„55+ Kopierschutz-Schemes“ steht als Kernfunktion in `CLAUDE.md` und `README`.** Gemessen: **38 Dateien, 350 Funktionen, 4 von aussen gerufen, 16 von einem Test beruehrt** — und eine der vier ist ein CRC-Helfer. **334 Funktionen sind weder verdrahtet noch geprueft** | gemessen (MF-557) | ⚠ **ueberwacht, nicht verdrahtet.** Den Katalog anzuschliessen hiesse, 334 ungeprueste Funktionen an ein forensisches Urteil zu haengen — genau die Lage, aus der die fuenf fabrizierten Parser kamen. Die Oberflaeche sagt es bereits von sich aus (`ProtectionAnalysisWidget.cpp`); seit MF-557 haelt `scripts/audit_protection_claims.py` die Zahl fest |
| **C12** | **10 von 22 Wandler-Funktionen haben null Tests** (`imd→img`, `img→imd`, `kryoflux→{adf,d64,hfe,scp}`, `nbz→{d64,g64}`, `td0→{imd,img}`) | gemessen (MF-567) | ⚠ **nicht angeboten** — keins der zehn hat einen Matrix-Eintrag, das Preflight-Tor weist alle ab. Das ist die Konstruktion aus MF-263, und sie trägt. Ein Test wird erst nötig, wenn eines davon angeboten werden soll |
| **C14** | **15 216 Zeilen Oberfläche.** Bei Beginn der Prüfsitzung: 2 Qt-Tests, beide auf demselben Reiter (Hardware). Wandel-, Explorer-, Status-, Workflow-, XCopy-Reiter: null Tests | gemessen (MF-569) | ⚠ **offen.** Das ist der Grund, warum MF-568/569 in einer einzigen Prüfrunde vier Klasse-A-Befunde ergaben. Vor einem Release muss diese Schicht so weit durchgemessen sein, dass zwei Runden hintereinander nichts Neues finden. **MF-574: der erste Reiter jenseits der Hardware hat jetzt einen Test** (`test_tools_tab_convert`, ToolsTab/Konvertieren). **MF-574…577: alle fünf Reiter jenseits der Hardware haben jetzt einen Test.** Was bleibt, ist Menschenarbeit: Aussehen, Bedienfluss, und alles hinter einem modalen Dialog |
| **D10** | **Fünf weitere `write()` ohne Prüfung** (Presets, Log, Text-Ausgabe, zwei Serial-Kommandos) | gemessen (MF-571) | ⚠ **offen, geringe Tragweite.** Keiner davon schreibt Abbild-Daten oder behauptet einen forensischen Befund. Der Vollständigkeit halber notiert, damit die Zahl nicht als „alle erledigt“ missverstanden wird |
| **C10** | **Zwei Zählweisen für dieselbe SCP-Datei.** `scp_writer_add_track()` nimmt den Zylinder und rechnet `*2 + side`; `uft_scp_get_track_flux()` nimmt den fertigen SCP-Index | gemessen (MF-565) | ⚠ **offen.** Kostete beim Bau des Rotbeweises einen halben Durchgang: mit dem Halbspur-Index landeten die Spuren doppelt so weit auseinander, und nur Spur 1 traf — **21 von 683**. Dazu prüft der Schreiber `track_num < 84`, rechnet dann aber bis `167`. Keine Datenverfälschung gemessen, aber eine Falle für jeden nächsten Aufrufer |
| **D1** | Tier-3-Hardware-Bench | **kein Gerät** (MF-310) — an die Gemeinschaft delegiert |
| **D3** | Korpus-Beschaffung (`cpmtools`, SAMdisk `tc.cpp`, `sector-cpc`) | **Eigentümer** |
| **D4** | GUI-Bedien-Nachweis | ⚠ **teilweise auflösbar, Einordnung war zu pessimistisch (MF-574).** „Nur der Eigentümer kann klicken“ stimmt nicht: der Baum hat kopflose Qt-Tests (`QT_QPA_PLATFORM=offscreen`), die in ctest laufen. Dieselbe Fehleinordnung wie bei C2. **`test_tools_tab_convert.cpp` treibt seit MF-574 den Konvertieren-Knopf wirklich an** — drei Zusicherungen, jede einzeln rotbewiesen. **MF-585: die Leitung ist bewacht** — MF-496 und MF-501 fassen keine GUI-Datei an, sie melden über `uftc_add_warning()`; bis MF-568 hat diese Ausgabe **nie einen Benutzer erreicht** (der Knopf war eine Dateikopie). `converterWarningsReachTheOutputPane` hält jetzt fest, dass Wandler-Meldungen im Ausgabefeld ankommen — rotbewiesen durch Kappen der Leitung. **Was ein Mensch noch leisten muss**, auf fünf Punkte eingedampft in [`docs/CLICK_SESSION_v4.1.6.md`](CLICK_SESSION_v4.1.6.md): Lesbarkeit der Meldung, ob der Feineinsteller-Vorschlag auf einen erreichbaren Regler zeigt, Verständlichkeit der Schadenslage, der modale Zustimmungs-Dialog (kopflos nicht prüfbar), und ob die zurückgenommenen Anzeigen wie eine ehrliche Auskunft klingen statt wie ein Fehler |
| **D5** | 12 Wandlungspfade brauchen Korpusdateien, 10 einen Wandler, der nicht existiert | teils Beschaffung, teils Neubau |
| **D6** | ATR Enhanced Density: gemeldete Geometrie falsch, **kein Datenverlust** | braucht benannte Referenz (MF-498a) |
| **D7** | Teilaufnahme-Karte nach ddrescue-Vorbild | offen |

---

## Übernommen aus KNOWN_ISSUES.md (MF-607)

`docs/KNOWN_ISSUES.md` ist ein **Prinzipien-Register**, kein
Arbeitsvorrat — es hält fest, wo das Werkzeug seine eigenen
Design-Prinzipien nicht einhält, und das meiste darin ist abgeschlossene
Geschichte (973 Zeilen, davon **genau zwei** mit Status `OPEN`,
nachgemessen).

Diese zwei stehen ab hier hier, damit es bei EINER Liste bleibt. Das
Register bleibt als Archiv erhalten und verweist für offene Arbeit
hierher.

| # | Sache | Stand |
|---|---|---|
| KI-6.1 | **Keine CI-Prüfung durch echte Emulatoren.** Prinzip 6 verlangt, dass Exporte durch einen Emulator laufen. `.github/workflows/emulator.yml` **existiert**, nennt sich aber im eigenen Kopf „SCAFFOLD ONLY": es belegt, dass VICE auf `ubuntu-22.04` installierbar und `c1541` kopflos aufrufbar ist, und liest eine feste D64-Vorlage. Was fehlt, sagt die Datei selbst: der echte Rundlauf braucht ein UFT-Werkzeug, das Fluss/Sektoren entgegennimmt und eine kanonische D64/ADF ausgibt — **das ist derselbe Blocker wie der verschobene Punkt 10** (`uft-decode`-CLI, Eigentümer-Entscheidung). Der Eintrag nannte das Gerüst bisher nicht | **bleibt** — hängt an einer Eigentümer-Entscheidung, nicht an Arbeit |
| KI-7.4 | **ADF-Schreibseite: zwei ehrliche Stubs.** `uft_adf_add_file()` (`src/formats/uft_adf.c:896`) und `uft_adf_delete()` (`:909`) geben `-1` zurück; ein dritter abgeleiteter Stub ab Zeile 1017 fällt auf dieselben zurück. Die Oberfläche bietet die Schreibseite nicht an | **bleibt, und das ist richtig so** — ein Stub, der `-1` zurückgibt statt zu tun als ob, ist releasefähig. Plan v4.2 (AmigaDOS-Bitmap-Belegung + Verzeichnis-Hash + Blockprüfsummen als ein Patch), gesteuert über `docs/STUB_ELIMINATION_PLAN.md` Phase 5 |

Fünf weitere Einträge stehen auf `MITIGATED` — sie beschreiben Zustände
mit Abmilderung, keine offene Arbeit, und bleiben im Register.

---

## Aus dem ersten Scout-Zyklus (MF-611, 2026-08-26)

Erster Lauf des neuen `uft-scout` gegen ein benanntes Repository
(`JYewman/Greaseweazle-Floppy-Disk-Restorer`).

**Ergebnis aus dem Repo: kein Fund** — und das ist das Ergebnis, kein
Ausweichen. Das Gutachten liegt in
`tools/uft-scout/out/greaseweazle-floppy-disk-restorer.gutachten.md`,
das Repo steht in der Negativliste. Nachgeprüft, weil eine Absage über
fremden Code genauso belegt gehören muss wie eine Zusage:

| Behauptung des Scouts | Nachgemessen |
|---|---|
| Lizenz MIT | ✅ `LICENSE`, „Copyright (c) 2026 Joshua Yewman" |
| Head-Alignment misst nicht, was es anzeigt | ✅ `src/floppy_formatter/analysis/head_alignment.py:444` — wörtlich `# For now, we simulate by reading at the nominal position`; dazu `:256` „rough approximation" |
| PLL-Sweep bewertet gegen einen Decoder ohne CRC | ✅ `.../recovery/pll_tuning.py:570` — `crc_valid = len(set(data_bytes)) > 1  # Basic sanity check`, zwei Zeilen darüber `# Real implementation would calculate CRC-CCITT` |

Die Zeilenangabe zum PLL-Fall stimmte auf sechs Zeilen genau; die
Dateipfade nannte der Scout ohne das `src/floppy_formatter/`-Präfix —
beim Nachschlagen gefunden, ohne Folge für den Befund.

**Der Ertrag dieses Laufs sind zwei Befunde über den Werkzeugkasten
selbst**, beide hier nachgemessen:

| # | Sache | Stand |
|---|---|---|
| SCOUT-1 | **Das Scout-Inventar ist für Fähigkeitsfragen strukturell falsch-negativ — ohne jede Warnung.** Gemessen: `jitter`, `weak bits`, `multi capture voting`, `bit slip` liefern alle `vorhanden: false` **und leere schwache Treffer**, obwohl UFT die Fähigkeiten hat (nachgeprüft: `src/hardwaretab.cpp`, `src/recovery/uft_bitstream_recovery.c` — 93 Treffer, `src/algorithms/advanced/uft_multi_rev_fusion.c`, `src/recovery/uft_multiread_pipeline.c`, alle vorhanden). Ursache: der Suchindex (`tools/uft-scout/scripts/inventar.py`) kennt nur Formatnamen, Verzeichnisse, Controller und vendorte Bibliotheken — keine Fähigkeiten. Folge: `AGENT.md` Regel 4 („Inventar vor Vorschlag") ist für alles außerhalb der Formatfrage **nicht einlösbar**, und der Scout schlüge Dubletten vor. In diesem Lauf hat ihn nur seine manuelle Gegenprobe davor bewahrt | ✅ **behoben (MF-611).** Feld `abgedeckt`; `false` heißt jetzt nicht mehr `fehlt`. Nachbesserung MF-613: Bindestrich und Unterstrich umgingen die Trennung weiterhin |
| SCOUT-2 | `python tools/uft-scout/scripts/inventar.py query --help` stürzt mit `FileNotFoundError` ab, weil `argv[2]` ungeprüft als Datei geöffnet wird — statt die Hilfe zu zeigen. Gemessen 2026-08-26 | ✅ **behoben (MF-611).** rc=2 mit Hilfe; ein fehlendes Inventar bekommt eine Meldung statt eines Tracebacks |

### Warum SCOUT-1 mich besonders angeht

Am selben Tag habe ich an genau dieser Abfrage die **umgekehrte**
Richtung repariert: `flux visualization` galt als vorhanden, weil `flux`
ein Verzeichnisname ist (MF-610). Der Rotbeweis dazu deckte den
Falsch-Positiv ab — und ließ den Falsch-Negativ **ohne Netz**.

Das ist dieselbe Bauart wie MF-531 gegen MF-592: eine Messung, die ihre
Frage beantwortet und die andere Hälfte des Problems nicht sieht. Der
Unterschied ist, dass es diesmal ein Fremder gefunden hat, und zwar
sofort im ersten Lauf.

**Der billigere von zwei Wegen ist der ehrlichere:** statt den Index um
Modul- und Funktionsnamen zu erweitern (viel Arbeit, neue Fehlalarme),
soll die Abfrage für Begriffe außerhalb ihrer Abdeckung sagen
„**Inventar deckt das nicht ab**" statt „vorhanden: false". Eine Antwort,
die ihre eigene Reichweite kennt, ist hier mehr wert als eine, die mehr
zu wissen vorgibt.

---

## Aus dem zweiten Scout-Zyklus (MF-612, 2026-08-27)

`yas-sim/fdc_bitstream` — vom Eigentuemer benannt, **steht bereits als
`integriert` in der Negativliste**. Der Scout hat das zuerst geprueft, eine
Neubewertung als Uebernahme-Kandidat nach Regel 6 **verweigert** und
stattdessen die Frage gedreht: nicht „uebernehmen?", sondern **„ist unsere
Kopie aktuell, und weicht sie ab?"**

Gutachten: `tools/uft-scout/out/fdc_bitstream.gutachten.md`.

**Antwort auf die Divergenzfrage: die Kopie ist aktuell.** Upstream-HEAD
`0178992` vom 2024-06-04 — seit dem Vendoring **null** Commits
(nachgemessen ueber die GitHub-API). Abweichungen sind ausschliesslich
lokale Haertungen. Kein Update noetig.

**Vier Befunde am eigenen Baum**, alle nachgeprueft:

| # | Sache | Stand |
|---|---|---|
| SCOUT-3 | **Die MIT-Pflicht der vendorten Kopie ist nicht erfuellt.** Upstream ist MIT (`LICENSE.md`, © 2022 Yasunori Shimura). In `src/flux/fdc_bitstream/` liegt **keine LICENSE-Datei**, kein Permission Notice, kein gepinnter Upstream-Commit — nachgemessen. MIT verlangt beides in jeder Kopie. Dieselbe Familie wie P0-5 (SPDX), nur umgekehrt: dort stand eine falsche Angabe, hier fehlt sie ganz. Weg: `LICENSE.md` ins Verzeichnis, Herkunftszeile „yas-sim/fdc_bitstream @ 0178992 (2024-06-04), MIT" in die Verzeichnis-README. Reine Doku, einfrier-frei | ✅ **behoben (MF-614).** `LICENSE.md` liegt im Verzeichnis, die README nennt Upstream-Commit `0178992` (2024-06-04) — Einzelheiten im Abschnitt „Abgearbeitet aus den Scout-Zyklen |
| SCOUT-4 | **2795 Zeilen werden gebaut und von niemandem gerufen.** `UnifiedFloppyTool.pro:400-414` uebersetzt die Kopie; nachgemessen: **0 Einbinder** ausserhalb des Verzeichnisses fuer `fdc_bitstream.h`, `bit_array.h`, `fdc_vfo_base.h`, und der einzige Test-Treffer ist eine Kommentarzeile. „Integriert" in der Negativliste beschreibt **Vendoring, keine Faehigkeit**. Entscheidung des Eigentuemers: (a) als Differenzlauf-Orakel gegen den Produktionspfad verdrahten — Plan mit Korpus, Metrik, Toleranzliste und Rotbeweis (eine 1-Bit-Verschiebung muss BEIDE Decoder roeten) steht im Gutachten; (b) aus dem Bau nehmen. **Nicht** verdrahten, damit es benutzt aussieht — das ist ein ausdrueckliches Nicht-Ziel | **offen, Eigentuemer** |
| SCOUT-5 | **Ein Wahrheitspaar fuer D77 liegt beim Upstream und ist nicht mitgekommen.** `test_data/`: `2019FM77AVDemo-4MHz.raw` und `-8MHz.raw` (je 3 649 229 B, dieselbe Diskette mit zwei Abtastraten) plus `2019FM77AVDemo.d77` (348 848 B) — reale Aufnahme mit unabhaengig erzeugtem Sektorabbild. Nachgemessen: `d77` steht auf **T3 ohne alles** (kein Test, keine Spec-Quelle, kein Korpus), und der Korpus fuehrt kein D77/FM-7/RAW. **BLOCKIERT bis Lizenzklaerung:** das Repo ist MIT, der Disketten-INHALT (ein Demo-Programm) hat eigene Urheber. Eigentuemer-Vorlage nach Regel 8 | **offen, Lizenzklaerung** |
| SCOUT-6 | **Zwei Doku-Stellen sagen Unwahres ueber die Kopie.** (a) `KNOWN_ISSUES.md` FLUX-12 nennt sie „zweiter Decoder, der eigene Tests hat" — es gibt keinen. (b) `src/flux/fdc_bitstream/README.md:65` fuehrt `vfo_fixed.cpp` als gebaut, `UnifiedFloppyTool.pro:419` sagt seit dem Entfernen das Gegenteil. Dazu die Negativlisten-Formulierung „integriert" → „vendored, 0 Aufrufer" (betrifft auch den samdisk-Eintrag) | ✅ **behoben (MF-614).** Beide Sätze berichtigt — Einzelheiten im Abschnitt „Abgearbeitet aus den Scout-Zyklen |

### Werkzeugkasten: ein dritter eigener Fehler

**W1 — die Provenienz wurde mitten im Wort abgeschnitten.**
`inventar.py` kappte das Herkunftsfeld des Korpus auf 60 Zeichen: aus
`GTK3VICE-3.10-win64` wurde `GTK3VI`, aus einem ganzen Satz
`byte-identical t`. Provenienz ist fuer T1/T1b **konstitutiv**
(`VERIFICATION_PLAN.md` §Provenienz-Regel) — sie ausgerechnet in dem
Werkzeug zu verstuemmeln, das Beschaffungslisten dagegen prueft, ist
verkehrt herum. Eingebaut am selben Tag beim Ergaenzen des Korpus-Felds
(MF-610), gefunden im zweiten Lauf. **Behoben.**

Was im zweiten Lauf **funktionierte**: die Korrekturen aus MF-611. Das
Feld `abgedeckt` hat den Scout dreimal zur Handnachschau gezwungen und
bei „wd179x fdc emulation" einen Fehlschluss verhindert — die Faehigkeit
liegt in der vendorten Kopie selbst.

---

## Abgearbeitet aus den Scout-Zyklen (MF-614, 2026-08-27)

Fünf Zyklen haben dreizehn Befunde über den eigenen Baum geliefert. Was
ohne Eigentümer-Entscheidung machbar war, ist erledigt:

| # | Sache | Ergebnis |
|---|---|---|
| **SCOUT-8** | **Drei MFI-Fassungen, keine las eine echte Datei** | ✅ **behoben.** Das registrierte Plugin prüfte **8 Byte** `"MAMEFLOP"` — ein Präfix der echten 16-Byte-Kennung. Es wies echte MAME-Dateien deshalb nicht ab, sondern **nahm sie an** und las die Spurtabelle ab `0x10`, wo `cyl_count`/`head_count` liegen; die Einträge beginnen bei `0x20`. `form_factor` kam aus `0x08`, also aus der Kennung, und die Eintragszahl aus der Dateigröße statt aus dem Kopf. Alle vier berichtigt gegen `mfi_dsk.h`/`.cpp` (BSD-3), zusätzlich die Schranken aus `identify()`. Rotprobe zuerst: beide Zusicherungen fielen auf der alten Fassung. **`mfi` steht damit auf T2 statt T3 — die Kennzahl sinkt von 57 auf 56** |
| **SCOUT-12** | Die zweite MFI-Fassung `uft_mame_mfi.c` trägt eine **erfundene** 17-Byte-Kennung `"MAME FLOPPY IMAGE"` mit Leerzeichen, die nie zutrifft; sie wird gebaut, hat keinen Aufrufer und keine Registry-Struktur | ⚠ **benannt, nicht berichtigt.** Die Kennung zu korrigieren würde einen **zweiten lebenden** MFI-Leser neben dem registrierten schaffen — das wäre schlimmer. Was ansteht, ist eine Löschentscheidung. Im Code steht jetzt, dass der Wert falsch ist, statt ihn als Spec auszugeben |
| **SCOUT-3** | MIT-Pflicht der vendorten `fdc_bitstream`-Kopie unerfüllt | ✅ **behoben.** `LICENSE.md` liegt im Verzeichnis, die README nennt Upstream-Commit `0178992` (2024-06-04), Lizenz und die Art der lokalen Abweichungen |
| **SCOUT-10** | `uft_dms.c` behauptete „Public Domain" ohne Beleg, 0 SPDX | ✅ **behoben.** Belegt gegen Debians `xdms`-`copyright` (machine-readable 1.0), mit dem Wortlaut des Autors. `SPDX-License-Identifier: LicenseRef-PublicDomain-xDMS` — eine formlose PD-Erklärung wird **zitiert**, nicht in eine Lizenz umgedeutet, die der Autor nie gewählt hat |
| **SCOUT-6** | Zwei Doku-Stellen behaupteten Unwahres über `fdc_bitstream` | ✅ **behoben.** `KNOWN_ISSUES.md` FLUX-12 sagte „der eigene Tests hat" — die Kopie hat **0 Aufrufer und 0 Tests**; die README führte eine gelöschte Datei als gebaut |
| **W5/W6/W7/W9/W10/W11** | Sechs Fehler im Scout-Werkzeugkasten | ✅ **behoben.** Der schwerste: `vermessen.py` stufte MAMEs **GPL-2.0-`COPYING` als MIT** ein (First-Match, MIT-Regex zuerst, die Datei zitiert ab Zeile 64 einen MIT-Text). Dort zonengleich — bei GPL-3 mit MIT-Zitat wäre aus „nicht portierbar" ein „portierbar" geworden. Jetzt gewinnt bei mehreren Treffern die **strengste** Zone, und eine Datei mit zwei Lizenztexten ist ein PRÜFEN-Fall |

### Nebenbei berichtigt

Ein Kommentar in `tests/test_disk_open_fuzz.c` nannte seit MF-539 die
8-Byte-Kennung als den *richtigen* Wert. Mein eigener Fix hat ihn
veralten lassen — mitgezogen.

### Offen geblieben

`SCOUT-4` (2795 Zeilen ohne Aufrufer: verdrahten oder aus dem Bau
nehmen), `SCOUT-5` (D77-Wahrheitspaar, Lizenzklärung), `SCOUT-12`
(Löschentscheidung) — alle drei brauchen den Eigentümer.
`SCOUT-7` (HFE-Fixture) und `SCOUT-11` (DMS-Differenzlauf) brauchen
Material von außen. `SCOUT-9` (floptool-Oracle) ist seit MF-623
**erledigt** — Werkzeug beschafft, am Korpus gemessen, Ergebnis am Ende
dieser Datei.

**Die Zahl `28 von 57` zur floptool-Abdeckung ist die Messung des
Scouts, nicht meine** — sie gehört nachgerechnet, bevor sie in einen
Release-Text kommt. Nachgerechnet in MF-615 (`22 von 56` über Namen),
und in MF-623 durch eine Fähigkeits-Messung ersetzt: **1 von 5**
Phase-1-Zielen.

---

## SCOUT-9 nachgerechnet: die Zahl war keine Messung (MF-615, 2026-08-27)

Der vierte Scout-Zyklus meldete **„floptool deckt 28 von 57 T3-Formaten
ab"** und nannte das den wertvollsten Fund der Serie. Ich habe die Zahl
im Bericht ausdrücklich als ungeprüft markiert. Jetzt ist sie geprüft.

**Sie stimmt nicht — und wichtiger: sie war nie überprüfbar.**

### Wie ich gemessen habe

Quelle ist MAMEs Hauptliste `src/lib/formats/all.cpp` (234 `en.add()`-
Aufrufe, 154 verschiedene `FLOPPY_*_FORMAT`-Symbole), gegen die
56 T3-Zeilen aus `docs/VERIFICATION_TIERS.md`. Drei Stufen, jede
nachvollziehbar:

| Stufe | Zahl | Beispiel |
|---|---|---|
| exakte Namensgleichheit | **12** | `86f`, `apridisk`, `img`, `ipf`, `nib`, `trd`, `vdk` … |
| Alias im MAME-Namen | **+3** | `ssd` → `acorn_ssd`, `opus` → `opus_ddos`, `cpm` → `poly_cpm` |
| semantisch begründbar | **+7** | `2img` → `apple_2mg`, `victor9k` → `victor_9000`, `fdi_pc98` → `pc98fdi`, `d77` → `d88` |
| | **= 22 von 56** | belegbare Untergrenze |
| möglich, aber unbelegt | +4 | `do`/`po` → `apple_gcr`?, `v9t9` → `ti99_sdf`? |
| | **≤ 26** | Obergrenze dieser Methode |

**28 liegt außerhalb.** Und die Methode des Scouts steht nirgends — ohne
sie ist auch seine Zahl nicht falsifizierbar, sondern nur unbelegt. Das
ist dieselbe Klasse wie die Zahlen, die dieser Baum dreimal driften sah
(MF-526/541/567), nur diesmal aus einer neuen Quelle.

### Was auch meine 22 NICHT sind

Ein Papier-Abgleich über Namen. Er sagt nichts darüber, ob floptool
**unsere** Dateien lesen kann — nur, dass MAME ein Format dieses Namens
kennt. Für ein Oracle zählt aber genau das Gegenteil: es muss dieselbe
Datei lesen und dasselbe herausbekommen.

**Entschieden wird das mit dem Binary, nicht mit einer Liste.** Der
nächste Schritt ist deshalb nicht „22 heben", sondern:

1. `floptool` beschaffen (liegt der offiziellen MAME-Distribution bei,
   kein Bau nötig) — Eigentümer, steht auf der Beschaffungsliste
2. `floptool flopdir`/`identify` gegen die 22 Kandidaten laufen lassen,
   mit unseren Korpus-Dateien wo vorhanden
3. Erst was dabei übrig bleibt, ist ein Oracle-Kandidat für Phase 1

### Der Befund über den Scout

Er hat in fünf Zyklen dreizehn belegte Funde geliefert, jeden mit
Datei und Zeile. Dieser eine war eine **Schätzung im Gewand einer
Messung** — und sie stand ausgerechnet an der Stelle, die er selbst als
den wertvollsten Fund bezeichnet hat.

Für `AGENT.md` heißt das: Regel 2 („kein Fund ohne Messung") verlangt
bislang eine Quelle. Sie verlangt nicht, die **Methode** dazuzusagen.
Bei einer Zählung über zwei Listen ist die Methode aber die halbe
Aussage. Nachgetragen.

---

## SCOUT-12 erledigt, ein vierter MFI-Leser gefunden (MF-616, 2026-08-27)

`src/formats/mame/uft_mame_mfi.c` ist **gelöscht** (Freigabe des
Eigentümers). Sie trug eine erfundene 17-Byte-Kennung
`"MAME FLOPPY IMAGE"` mit Leerzeichen, die in keiner MFI-Datei vorkommt,
wurde gebaut und hatte keinen Aufrufer.

### Die Löschpipeline hat einen Fehler in meiner eigenen Prüfung gefangen

Sechs Stufen nach MF-369. Fünf waren sauber — Stufe 6 nicht:

| Stufe | Ergebnis |
|---|---|
| 1 · bedingt in der `.pro`? | nein, einfacher `SOURCES`-Eintrag |
| 2 · beide Endungen, alle Verzeichnisse | 4 Treffer: eigener `@file`-Kopf, zwei Kommentarzeilen, `.pro` |
| 3 · Kommentar-Strip | die zwei Testtreffer sind Kommentar, kein Aufruf |
| 4 · Symbolrauschen | `uft_mfi_probe` mit **2 Treffern ausserhalb** → siehe unten |
| 5 · Skript-Referenzen | keine |
| 6 · Vollbau | **qmake grün, CMake ROT** |

Stufe 6 fiel, weil `tests/CMakeLists.txt` die Datei nicht beim Namen
nennt: drei `GLOB_RECURSE ${CMAKE_SOURCE_DIR}/src/formats/*.c` ziehen
**jede** Format-Datei ein. Eine Namenssuche findet das nicht, und das
Glob-Ergebnis liegt zwischengespeichert im Bauverzeichnis — der erste
Bau nach dem Löschen meldete `AutogenInfo.json … does not exist`.

**Der Fehler danach war meiner:** ich habe `ctest` auf dem gescheiterten
Bau laufen lassen und „267/267" gelesen. Das waren veraltete
Binärdateien. Erst `cmake ..` (neu erzeugen, nicht nur bauen) macht die
Zahl gültig. Dieselbe Falle steht in den Projektnotizen als
„`cmake .` gegen `cmake ..`" — ich bin trotzdem hineingelaufen.

### Der Fund bei Stufe 4: es gab vier MFI-Leser, nicht drei

| # | Ort | Zustand |
|---|---|---|
| 1 | `src/formats/mfi/uft_mfi.c` | registriert — **behoben** (MF-614) |
| 2 | `src/formats/mame/uft_mame_mfi.c` | erfundene Kennung — **gelöscht** (MF-616) |
| 3 | `include/uft/profiles/uft_mfi_format.h` | `static inline uft_mfi_probe`, **gleicher Name wie #2 bei anderer Signatur** (2 statt 3 Parameter) |
| 4 | `src/samdisk/mfi.cpp` | eingekauft, weist die moderne Kennung ab, nicht gebaut |

**#3 ist ebenfalls tot, nur eine Ebene tiefer.** Es wird ausschließlich
von `include/uft/profiles/uft_format_registry.h` eingebunden, und die
bindet **niemand** ein (nachgemessen). Auch dessen Spec ist erfunden:
`UFT_MFI_SIGNATURE_V1 = "MAMEFLOP"` — wieder acht statt sechzehn Byte —
plus eine „v2" mit der Kennung `"MFI2"`, die MAME nicht kennt, und ein
`signature[8]`-Feld in einer selbstgebauten v1/v2-Aufteilung.

| # | Sache | Stand |
|---|---|---|
| **SCOUT-13** | **Ein fünfter Ort mit erfundener MFI-Spec, tot:** `include/uft/profiles/uft_mfi_format.h` (v1/v2-Aufteilung, `"MAMEFLOP"` 8 Byte, `"MFI2"`) samt seinem einzigen Einbinder `profiles/uft_format_registry.h` (471 Zeilen, **kein Einbinder**). Löschkandidat nach derselben Pipeline — der Fall liegt gleich, die Freigabe fehlt | **offen, Eigentümer** |
| **SCOUT-14** | **Vier Dateien heißen `uft_format_registry.h`** — `src/core/unified/`, `include/uft/core/`, `include/uft/profiles/`, `include/uft/`. Verwandt mit der Wächter-Kollisionsfamilie (P1-8): wer welche sieht, entscheidet die Include-Reihenfolge | **offen** |

---

## SCOUT-13 erledigt: 26 tote Header, und was dabei sichtbar wurde (MF-617)

`include/uft/profiles/` hatte **28 Header. Zwei sind lebendig.** Gelöscht
sind die anderen 26, einschließlich `uft_mfi_format.h` (fünfter Ort mit
erfundener MFI-Spec) und `uft_format_registry.h` (471 Zeilen, kein
Einbinder).

Geblieben: `uft_imd_data_modes.h` (2 echte Einbindungen) und
`uft_ipf_format.h` (1).

### Die Pipeline, sechs Stufen

| Stufe | Ergebnis |
|---|---|
| 1 · Include-Pfad | `profiles/` steht auf **keinem** — weder `.pro` noch CMake |
| 2 · Einbinder, pfadgenau | nur die zwei lebenden |
| 3 · Kommentar-Strip | die Treffer bei `uft_mfi_format.h`/`uft_td0_format.h` waren Kommentare |
| 4 · Symbolrauschen | **58 Namen kommen auch außerhalb vor** — aber als eigenständige Definitionen |
| 5 · namenlose Einbindungen | jede einzelne liegt **innerhalb** von `profiles/` |
| 6 · Vollbau | qmake grün (Binary unverändert 5 018 112 Byte), cmake grün, 267/267 |

Stufe 4 war der Grund, überhaupt so genau zu messen: hätte auch nur
einer der 58 Namen von außen *gebraucht* statt nur *auch definiert*
worden, wäre die Löschung ein Linkfehler geworden.

### Was die Löschung bewirkt hat — und was nicht

**Vier Konflikte sind weg**, und das Tor hat sie selbst gemeldet:

    UFT_IMD_MAX_COMMENT     Makro-Konflikt  aufgeloest
    UFT_IMD_MAX_SECTORS     Makro-Konflikt  aufgeloest
    UFT_IMD_MAX_TRACKS      Makro-Konflikt  aufgeloest
    UFT_FORMAT_COUNT        Enum-gegen-Makro aufgeloest

Alle vier aus den Grundlinien gestrichen und unter `MF-617` als
aufgelöst vermerkt — die Grundlinie soll etwas bedeuten.

**Meine Vermutung war dagegen falsch, und das ist gemessen.** Ich hatte
erwartet, dass die Wächter-Kollisionen sinken, weil `profiles/` Träger
von `UFT_FORMAT_ENUM_DEFINED` und `UFT_PLATFORM_T_DEFINED` war:

    geteilte Waechter      26 -> 25
    davon ABWEICHEND       23 -> 23     unveraendert

Nur eine **redundante** Kollision fiel weg. Die 23 der Klasse TYP (P1-8)
bleiben unberührt — die toten Header waren nicht ihre zweite Seite.

### Ein Toter hielt einen Toten am Leben

| # | Sache | Stand |
|---|---|---|
| **SCOUT-15** | **`src/formats/86box/uft_86box.c` — 268 Zeilen, gebaut, keine Aufrufer.** Eine ZWEITE 86F-Fassung neben dem registrierten `uft_86f_plugin.c`, dasselbe Muster wie bei MFI. Sie war schon vorher unerreichbar; der Verwaisten-Detektor zählte den `static inline uft_86f_probe` aus dem toten `uft_86f_format.h` als Benutzung. Mit dessen Löschung wurde sie **sichtbar**, nicht tot. In `docs/orphan_baseline.txt` mit Begründung eingetragen — das heißt „gesehen und benannt", nicht „erledigt" | **offen, Eigentümer** |

Das ist inzwischen das dritte Mal dieses Musters: **MFI hatte vier
Fassungen** (eine registriert, drei tot), **86F hat zwei**. Wert einer
eigenen Suche — nicht „welches Format fehlt", sondern „welches Format
haben wir mehrfach, und welche Fassung gewinnt".

---

## SCOUT-14 nachgemessen: die Prämisse war falsch, der Fund ein anderer (MF-619)

SCOUT-14 lautete: „vier Dateien heißen `uft_format_registry.h` —
verwandt mit der Wächter-Kollisionsfamilie P1-8". **Beides stimmt nicht
mehr.**

Es sind **drei** (die vierte war die mit MF-617 gelöschte), und sie
tragen **verschiedene** Wächter:

    src/core/unified/uft_format_registry.h    UFT_SRC_FORMAT_REGISTRY_H
    include/uft/core/uft_format_registry.h    UFT_CORE_UFT_FORMAT_REGISTRY_H
    include/uft/uft_format_registry.h         UFT_FORMAT_REGISTRY_H

Sie kollidieren also nicht — das hat MF-591 erledigt, als die privaten
Wächter umbenannt wurden. Meine Einordnung war eine Vermutung, und sie
war falsch.

**Zwei der drei haben keinen Einbinder** (nachgemessen, pfadgenau UND
namenlos; `include/uft` liegt auf dem Include-Pfad, deshalb war die
zweite Prüfung nötig). Lebendig ist nur
`include/uft/core/uft_format_registry.h`, eingebunden von
`uft_imd_adapter.h:24`.

### Der eigentliche Fund liegt eine Ebene tiefer

Es gibt **drei** `.c`-Dateien mit „registry" im Namen, und keine bindet
einen der drei Header ein:

| Datei | Zeilen | Stand |
|---|---|---|
| `src/formats/format_registry/uft_format_registry.c` | 471 | **lebt** — definiert `uft_register_all_formats()` (`:434`), gerufen von `src/main.cpp:43` |
| `src/formats/uft_format_registry.c` | 482 | erreichbar |
| `src/formats/uft_format_registry_v2.c` | 587 | **in der Verwaisten-Grundlinie** — kein Aufrufer |

| # | Sache | Stand |
|---|---|---|
| **SCOUT-16** | **Drei Dokumente zitieren eine Kennzahl aus einer Datei ohne Aufrufer.** `CAPABILITIES.md:110`, `FORMAT_GROUPS.md:11` und `FORMAT-CLASSIFICATION.md:3` führen **„161 Format-IDs im Katalog"** und nennen `uft_format_registry_v2.c` als Quelle. Die Datei hat 162 Tabelleneinträge — und **keinen Aufrufer**; ihre sechs exportierten Funktionen ruft niemand. Registriert wird über `format_registry/uft_format_registry.c`, dessen `all_plugins[]` aus Gruppen gespeist wird, nicht aus dieser Tabelle. Die 161 beschreiben einen **Katalog**, nicht das Verhalten — dieselbe Unterscheidung wie bei „55+ Kopierschutz-Schemes" (P0-2). `KNOWN_ISSUES.md` hat die Datei zweimal als unbenutzt vermerkt (`:2772`, `:9072`), ohne dass die Zahl berichtigt wurde | ⚠ **zwei von drei Stellen markiert** (MF-619). `FORMAT-CLASSIFICATION.md` ist generiert — dort muss der Erzeuger geändert werden, nicht die Ausgabe |
| **SCOUT-17** | Zwei Registry-Header ohne Einbinder (`src/core/unified/`, `include/uft/`) und eine Registry-`.c` ohne Aufrufer (`_v2.c`, 587 Zeilen). Löschkandidaten nach derselben Pipeline wie MF-616/617 | **offen, Eigentümer** |

SCOUT-14 selbst ist damit **erledigt** — als widerlegte Vermutung, nicht
als behobener Fehler.

---

## Lizenz-Zensus je Datei — und was er sofort fand (MF-620)

Der letzte offene Werkzeugfehler des Scouts (W8) war: `vermessen.py`
sieht **Datei-Lizenzen** nicht, nur Lizenzdateien. Bei MAME war genau
das die maßgebliche Ebene (441× BSD-3 neben 6× GPL-2.0+ neben 6×
LGPL-2.1+, je SPDX-Kopfzeile) — der vierte Zyklus musste den Zensus von
Hand machen.

Jetzt zählt das Werkzeug SPDX- und `// license:`-Kopfzeilen mit. Beim
Probelauf **gegen den eigenen Baum**:

| Bezeichner | Dateien |
|---|---|
| `GPL-2.0-or-later` | 7 |
| `MIT` | 6 |
| `LicenseRef-PublicDomain-xDMS` | 1 |
| `Unlicense` | 1 |
| **`GPL-3.0-or-later`** | **1** |

### Der Fund

| # | Sache | Stand |
|---|---|---|
| **SCOUT-18** | **`GPL-3.0-or-later` in einem GPL-2.0-Projekt.** `src/formats/retro_image/uft_retro_image_detect.c` und `include/uft/formats/uft_retro_image_detect.h` tragen diesen Bezeichner. Nachgemessen: unsere `LICENSE` ist der reine GPL-2-Text **ohne** „or later"-Klausel; die Datei nennt **keinen** fremden Ursprung (kein „based on", kein Copyright Dritter) — es ist UFT-eigener Code. Sie steht in `docs/orphan_baseline.txt`, ist also unerreichbar; die gebaute Binärdatei ist damit nicht betroffen, die Quell-Release aber schon. Die Lizenzmatrix des Scouts sagt: „GPL-3.0 ist für ein GPL-2.0-Projekt NICHT portierbar" | **offen, Eigentümer.** Zwei Auslegungen, beide plausibel: (a) der Kopf ist schlicht falsch — jemand schrieb GPL-3, wo GPL-2 gemeint war; (b) die Datei war als GPL-3 gedacht und gehört dann nicht in diesen Baum. Beides ist eine **Lizenzentscheidung**, keine Codeänderung — deshalb nicht von mir angefasst |

`Unlicense` (`flashfloppy/uft_ff_formats.c`) ist unproblematisch —
gemeinfrei-äquivalent und mit GPL-2 verträglich.

### Zwei Einschränkungen des Zensus, die im Ergebnis stehen

**Stichprobe, keine Vollzählung:** 600 Quelldateien, und das Ergebnis
sagt das selbst (`lizenz_je_datei_vollstaendig: false`).

**Der erste Anlauf war blind.** Er lief die Verzeichnisse in
Walk-Reihenfolge ab und verbrannte sein Budget in `.claude/skills/` und
`.agents/` — 600 Dateien geprüft, **null** Lizenzkopfzeilen gefunden,
obwohl es sie gibt. Erst als `src/`, `include/`, `lib/` Vorrang bekamen,
fand er die sieben GPL-2-Köpfe, die ich teils selbst heute gesetzt habe.
Ein Zensus, der in einem Skriptordner anfängt, misst Skriptordner.

---

## Die dritte Doku-Stelle zur „161" (MF-620)

`FORMAT-CLASSIFICATION.md` sagte „_Generiert 2026-07-05 aus
`uft_format_registry_v2.c`_". **Es gibt keinen Erzeuger** — nachgemessen:
`grep -rl "FORMAT-CLASSIFICATION" scripts/` ist leer. Die Datei ist eine
von Hand erstellte Momentaufnahme, die sich als erzeugt ausgibt, und ihre
Quelle hat keinen Aufrufer. Beides steht jetzt darin.

Dazu zwei falsche Zuschreibungen berichtigt:

* `CAPABILITIES.md:100` schrieb die 161 der SSOT `gen_format_list.py`
  zu — **die liefert 88.**
* `FORMAT-CLASSIFICATION.md:32` nannte `uft_format_registry_v2.c` die
  „SSOT-Registry". Sie ist weder SSOT noch erreichbar.

Damit ist SCOUT-16 **erledigt**: alle drei Stellen nennen jetzt die
wahre Quelle und ihren Zustand.

---

## Zwei Regeln beschlossen — vier Entscheidungen mechanisch erledigt (MF-621/622)

Statt fünf Einzelfälle zu entscheiden, hat der Eigentümer zwei Regeln
festgelegt. Vier der fünf lösen sich damit von selbst; eine bleibt als
echte Untersuchung.

### Regel 1 — SPDX-Politik (`CONTRIBUTING.md` §Licensing)

**UFT-eigener Code trägt `GPL-2.0-or-later`.** Portierter Code behält die
Lizenz seines Ursprungs und nennt sie (samdisk-Muster). Das `-or-later`
hält die Tür zu GPL-3-Quellen offen, falls das Projekt je hochzieht.

| # | | |
|---|---|---|
| **SCOUT-18** | GPL-3.0 in einem GPL-2.0-Projekt | ✅ **erledigt.** Die Datei ist UFT-eigener Code (kein fremder Ursprung, nachgemessen) — also entscheidet der Urheber, und die Entscheidung steht jetzt geschrieben. Kopf auf `GPL-2.0-or-later` berichtigt, in `.c` und `.h` |

**Und der Zufallsfund wird ein Verfahren.** Neues Tor 38,
`scripts/audit_spdx_policy.py`: jeder SPDX-Bezeichner in `src/` oder
`include/` außerhalb einer kleinen erlaubten Menge lässt den Commit
scheitern. Rotbeweis gelaufen — mit dem alten Kopf meldet es beide
Dateien mit Zeilennummer, danach 0. Neue Bezeichner einzutragen ist
ausdrücklich eine Eigentümer-Entscheidung, keine Nebenbei-Ergänzung.

### Regel 2 — Verwaisten-Regel (`docs/orphan_baseline.txt`)

**Verwaister Code bleibt nur mit benanntem Plan-Anker** — welcher
Baustein wird ihn verdrahten? Ohne Anker wird gelöscht; Git vergisst
nichts. Das ist die MF-271-Linie, ausgeschrieben, samt Form des Ankers.

| # | | |
|---|---|---|
| **SCOUT-15** | `src/formats/86box/uft_86box.c`, 268 Zeilen | ✅ **gelöscht.** Kein Plan-Anker. Dabei ein weiterer Fund: das registrierte Plugin behauptet im Kopfkommentar, es umhülle `uft_86f_probe()`/`uft_86f_read()` — **es ruft sie nirgends.** Der einzige Treffer im ganzen Baum war dieser Kommentar. Berichtigt |
| **SCOUT-17** | zwei Registry-Header | ✅ **gelöscht** — `src/core/unified/` und `include/uft/`, beide ohne Einbinder, ohne Plan-Anker, ohne Symbolkollision |

**Nebenwirkung, vom Tor gemeldet:** zwei Einträge in
`extern_decl_baseline.json` lösen sich auf — die abweichende Deklaration
von `uft_format_detect` und `uft_format_can_convert` saß im gelöschten
Header. Gestrichen und unter `MF-622` vermerkt.

### Was ich NICHT gelöscht habe, und warum

| # | | |
|---|---|---|
| **SCOUT-17b** | `src/formats/uft_format_registry_v2.c`, 587 Zeilen | ✅ **erledigt in MF-624** — Tabelle nach `docs/FORMAT_CATALOG.md` überführt, Datei gelöscht. Der damalige Vorbehalt steht unten unverändert, weil er richtig war: ⚠ **angehalten.** Regel 2 träfe zu — kein Plan-Anker, keine Symbolkollision. Aber die Datei hat einen Konsumenten, den die Regel nicht kennt: **sieben Verweise in vier Dokumenten.** `FORMAT-CLASSIFICATION.md` ist im Kern eine Klassifikation genau dieser 162-Einträge-Tabelle; `CAPABILITIES.md` und `FORMAT_GROUPS.md` beziehen ihre „161 Format-IDs" daraus. Sie zu löschen entzieht einem ganzen Dokument den Gegenstand |

Diese Information lag bei der Regelentscheidung noch nicht vor. Zwei
Wege, beide sauber:

* **Tabelle in ein Dokument überführen**, dann die `.c` löschen — die
  161 sind dann Doku, was sie faktisch ohnehin sind.
* **Anker eintragen** („Katalogquelle für FORMAT-CLASSIFICATION") und
  behalten — dann sagt die Grundlinie die Wahrheit über den Grund.

Was **nicht** geht: löschen und die vier Dokumente auf eine gelöschte
Datei zeigen lassen.

---

## Die neue SPDX-Politik gilt für 4,2 % des Baums (MF-622)

Regel 1 steht seit MF-621 in `CONTRIBUTING.md`, und Tor 38 setzt sie
durch. Bevor das als erledigt gilt, die Reichweite — gemessen, nicht
angenommen:

| Quelldateien in `src/` + `include/` (ohne eingekaufte Bäume) | |
|---|---|
| mit SPDX-Kopf | **50** |
| ohne | **1132** |
| Anteil | **4,2 %** |

**Das Tor fängt einen *falschen* Bezeichner, nie einen *fehlenden*.** Die
Regel ist damit bindend für neue und angefasste Dateien — und für den
Rest eine Absichtserklärung. Das steht jetzt in `CONTRIBUTING.md`, damit
niemand den Abschnitt für eine Beschreibung des Baums hält.

1132 Köpfe nachzutragen ist **eine** Masseänderung mit echten Folgen und
gehört dem Eigentümer, nicht dem Vorbeigehen.

| # | Sache | Stand |
|---|---|---|
| **SCOUT-19** | 1132 Quelldateien ohne SPDX-Kopf. Masseänderung, Eigentümer-Entscheidung. Ein Tor gegen *fehlende* Köpfe wäre erst danach sinnvoll — vorher meldete es 1132 Zeilen und würde am nächsten Tag abgeschaltet | **offen, Eigentümer** |

### Die D77-Untersuchung: erster Schritt gemacht, Ergebnis unbequem

Der Auftrag war: Herkunft messen, dann entscheiden. Gemessen an
`src/formats/d77/uft_d77.c`, `src/formats/d88/uft_d88.c` und
`uft_d88_parser_v2.c`:

**Keine einzige Herkunftsangabe.** Kein SPDX, kein „based on", kein
fremdes Copyright, keine Spec-Referenz — in keiner der drei Dateien.

Das ist **nicht** der harmlose Fall („Ursprung gefunden, Lizenz klar,
Attribution nachtragen"), aber auch nicht der schlimme („erkennbar
fremder Code ohne Lizenz"). Es ist der dritte: **nichts deutet auf
fremden Ursprung, aber nichts schließt ihn aus.** Genau die
Unentscheidbarkeit, die `VERIFICATION_PLAN.md` §Provenienz-Regel als
konstitutiv beschreibt — „ohne sie ist ‚Parser falsch' von ‚Datei
beschädigt' nicht unterscheidbar".

Der nächste Schritt wäre ein Ähnlichkeitsvergleich gegen die üblichen
Verdächtigen. Der braucht deren Quelltext im Zugriff und ist ein eigener
Arbeitsgang — **nicht** nebenbei erledigt, und ich habe ihn nicht
gemacht, statt ihn zu behaupten.

`d77` steht auf **T3** ohne Test, ohne Spec-Quelle, ohne Korpus. Die
Neuimplementierung gegen die öffentliche D88-Spezifikation wäre also
ohnehin kein Verlust — sie wäre eine Hebung.

---

## Korpus-gebundene Tests — Beschaffungsliste (MF-588)

**Gemessen auf diesem Rechner: 266/266, ein Skip** (`test_freezer`, siehe unten). Auf einem frischen
Klon ist das anders, und der Unterschied gehört in den Release-Text.

`tests/corpus/` ist **gitignored** (`.gitignore:105`) — 10 Dateien liegen
hier lokal, keine davon im Repo. Sechs Testdateien hängen daran:

| Test | braucht |
|---|---|
| `test_corpus_scp.c` | `gw_amigados.scp` |
| `test_corpus_fdi.c` | `zxart_spectrofon01.fdi` |
| `test_corpus_protection_copylock.c` | dec0de-Referenzen (`*.BIN`) |
| `test_c64_protection_real_corpus.c` | `c64pp_*.g64` |
| `test_convert_scp_adf.c` | `gw_amigados.scp` |
| `test_scp_legacy_adapter.c` | `gw_amigados.scp` |
| `test_freezer.c` (8 von 14) | ein echtes Action-Replay-Einfrierabbild |

`tests/corpus_free/` (12 Dateien) **liegt im Repo** — die darauf gestützten
Tests laufen überall.

**Das ist keine Lücke im Werkzeug, sondern eine in der Verteilung.** Ein
Release-Text, der „266/266" sagt, ohne diesen Unterschied zu nennen,
behauptet für den Leser etwas, das bei ihm nicht gilt. Deshalb steht die
Zahl mit ihrer Bedingung in den Release Notes.

**Zu beschaffen** (Eigentümer): Lizenz-/Verteilungsklärung für die 10
Dateien, oder Ersatz aus frei verteilbaren Quellen.

---

## Was die Prüfstands-Reparatur zutage gefördert hat (MF-596…598)

**32 Testläufe konnten nicht scheitern.** `RUN_TEST` zählte den Erfolg
hinter dem Aufruf, bedingungslos; `ASSERT` kehrte bei Fehlschlag nur aus
der Testfunktion zurück. `main()` gab `(tests_passed == tests_run) ? 0 : 1`
— also immer 0. Nach der Reparatur (MF-596) fielen **sechs Tests mit 17
Prüfungen**, die als bestanden gemeldet worden waren.

Die Einordnung, jede mit ihrer Ursache:

| Prüfung | Ursache | Erledigt |
|---|---|---|
| `test_gcr_ops::gcr_count_syncs_bytealigned` | Puffer 200 Byte; die Vorlage legt Syncs erst ab 500 an | ✅ MF-597, Test |
| `test_gcr_ops::gcr_detect_density` | rief mit `NULL` und traf damit die Wache statt der Größenlogik | ✅ MF-597, Test |
| `test_genesis::calculate_checksum` | Vorlage ohne Nutzlast → Prüfsumme korrekt 0 | ✅ MF-597, Test |
| `test_genesis::verify_checksum` | dieselbe Ursache: 0 == 0 stimmt überein | ✅ MF-597, Test |
| `test_sms` (3 Prüfungen) | **Code falsch:** Region liegt im OBEREN Halbbyte von `$7FFF`, gelesen wurde das untere. Jedes Game-Gear-Abbild wurde als Master System gemeldet; der Größencode kam aus dem falschen Byte | ✅ MF-598, Referenz im Header |
| `test_gameboy::get_gb_info` | **Code falsch:** roher Struktur-Überwurf über einen Kopf, dessen Felder überlappen. `sizeof` = 86 statt 80, `cartridge_type` landete auf `$014C` statt `$0147`, `global_checksum` sechs Byte hinter dem Kopfende | ✅ MF-598, Pan Docs im Header |
| `test_floppy_formats::spec_detect_tap` | **Code falsch:** die kennungslose Z80-Vermutung stand vor TAP und DSK und schluckte beide | ✅ MF-598 |
| `test_freezer` (8 Prüfungen) | **unentscheidbar:** weder Vorlage noch Erkenner haben einen Nachweis | ⚠ ausgelassen, siehe unten |

**Drei davon waren echte Fehler im Format-Layer**, keine Testfehler. Sie
standen unbemerkt, weil ihre Tests sich nicht rot melden konnten.

### Der offene Fall: Action Replay

`freezer_detect()` verlangt `size >= 66816 && size <= 67584`. Die
Testvorlage ist 66 664 Byte groß (`0x80 + 1000 + 65536`). Woher das
Fenster stammt, sagt niemand: `include/uft/formats/c64/uft_freezer.h:58`
nennt seine Versatztabelle wörtlich „typical layout", `docs/` kennt keine
Quelle, und ein AR-Einfrierabbild ist kein genormtes Dateiformat, sondern
das, was die Steckmodul-Firmware auf die Diskette schreibt.

Beide Seiten aneinander anzupassen wäre Erfindung — dieselbe Bauart wie
bei den fünf fabrizierten Parsern (FMT-2/3/10/11/12). Die EINFRIER-REGEL
verlangt eine **benannte** Referenz; es gibt keine. Die acht Prüfungen
sind deshalb ausgelassen (`SKIP_TEST`, Rückgabe 77), die sechs
referenzfreien laufen weiter.

**Zu beschaffen** (Eigentümer): ein echtes Action-Replay-Einfrierabbild.
Dann lässt sich der Erkenner dagegen messen statt gegen sich selbst.

### Nebenbei aufgelöst

Die Liste in `tests/CMakeLists.txt`, die sieben Testnamen für
`SKIP_RETURN_CODE 77` aufzählte, gilt jetzt für alle Tests. 77 ist ein
absichtlich gewählter Sentinel; eine Liste, die Sonderfälle nennt statt
die Regel, veraltet still — in diesem Baum zum dritten Mal belegt
(MF-567, MF-578, hier).

---

## SCOUT-9 entschieden: floptool ist ein Oracle für EIN Phase-1-Ziel (MF-623, 2026-08-28)

Die drei Schritte von MF-615 sind gegangen. Entschieden hat es das
Binärprogramm, nicht die Liste.

**Beschafft:** `floptool.exe` (3 041 280 Byte) aus `mame0289b_x64.exe`
(87 626 249 Byte, Release `mame0289` vom 2026-07-30). Die SHA-256 der
Distribution wurde gegen die offizielle `SHA256SUMS` des Release geprüft
und ist identisch (`a1aa7912…150b7d`). Das Werkzeug selbst trägt
`6973d1b5…20ac21`.

### Was floptool überhaupt kann — die Dateisystem-Liste, nicht die Format-Liste

`identify` rät über die Dateigröße und ist als Oracle unbrauchbar: es
hielt `dec0de_README.txt` für ein Acorn-DSD. Der Phase-1-Mechanismus ist
`flopdir <container> <dateisystem> <datei>` mit **ausdrücklich genanntem**
Format — damit entfällt das Raten.

Damit zählt aber die Dateisystem-Liste, und die ist klein: **62 Einträge
in 19 Familien** (`floptool help all`). Darunter CBM DOS, ProDOS, PC-FAT,
HP-LIF, CoCo RS-DOS/OS-9, Adam-EOS, ISIS, Oric-Jasmin, vtech.
**Kein Amiga-Dateisystem. Kein Atari DOS.**

### Der Differenzlauf über den freien Korpus (12 Dateien)

| Ergebnis | Dateien | Anzahl |
|---|---|---|
| echte Auflistung mit Volumename + Disk-ID | `.d64`, `.d71`, `.g64` | **3** |
| **schweigender Fehlgriff** (rc=0, leer) | `.adf` | **1** |
| lauter Fehler | `.d81`, `.hfe`, `.d67`, `.g71`, `.xfd`, `.atr` | 6 |
| hängt (>9 min, abgebrochen) | `.d80`, `.d82` | 2 |

Gelesen wird echt: aus `vice_c1541_35trk.d64` kommt
`Volume: name=UFTCORPUS disk_id=42 os_version=2A` und der Eintrag
`UFT MARKER PRG` — genau das, was die Herkunftsangabe des Korpus
(`c1541 -format "uftcorpus,42"`) verspricht. MAME ist eine von VICE
unabhängige Implementierung, also eine echte Zweitmeinung.

### Gegen die fünf Phase-1-Ziele: 1 von 5

D64 **ja**. ADF, ATR, FDI, NFD-r0 **nein** — nicht aus Schwäche des
Abgleichs, sondern weil floptool für keines davon ein Dateisystem
mitbringt. Die frühere Zahl `22 von 56` war ein Abgleich über Namen; sie
ist damit nicht widerlegt, sondern **ersetzt**: gemessen wurde jetzt, was
das Werkzeug an unseren Dateien tut.

### Der Fallstrick, und wo er festgeschrieben ist

floptool prüft den **Container**, nicht das **Dateisystem**. Rotbeweis in
beide Richtungen:

* 174 848 Byte Zufall als `d64`/`cbmdos` → lauter Fehler
  (`Block number overflow`). Es erfindet nichts.
* falscher Container (`d64` auf einer ADF-Datei) → lauter Fehler.
* **richtiger Container, falsches Dateisystem** (`adf`/`cbmdos` auf einem
  880-KB-AmigaDOS-Abbild) → `rc=0`, leerer Volumename, leere Liste, keine
  Warnung.

Wer dieses Oracle benutzt, wertet eine leere Auflistung deshalb als
**„kein Ergebnis"**, nie als „leere Diskette" — sonst bestätigt das
Oracle stillschweigend einen Lesefehler von UFT. Jeder Aufruf braucht
außerdem ein Zeitlimit (siehe die zwei Hänger). Das steht jetzt am
Registry-Eintrag selbst, nicht nur hier.

### Was das an der Registry geändert hat

`floptool` ist der **sechste** Eintrag in `tests/differential/oracles.py`
— und der erste, der auf dieser Maschine tatsächlich vorhanden ist
(vorher: 0 von 5).

Er nennt allerdings keine Version: weder `--version` noch `-version` noch
der argumentlose Aufruf geben eine aus. Nach der Provenienz-Regel wäre er
damit dauerhaft `complete: false` und für kein T1b-Manifest brauchbar.
Das ist die falsche Folgerung — die Regel schützt die
**Nachbeschaffbarkeit**, und die SHA-256 des Binärprogramms leistet das
strenger als eine Versionszeile. Der Eintrag wird also nicht aufgeweicht,
sondern verschärft: jeder aufgelöste Oracle-Pfad trägt ab jetzt seinen
Hash im Manifest, und `version_is_unaskable` muss am Eintrag **erklärt**
sein — ein Werkzeug mit Versionsabfrage, das sich für stumm erklärt,
weist der Selbsttest ab.

### Nebenbei: der Testläufer meldete Erfolg für Prüfungen, die er nie sah

`tests/differential/test_oracles.py` läuft unter ctest **ohne** pytest,
über ein eigenes `main()`. Dessen Startblock stand mitten in der Datei;
alles darunter existierte beim Aufruf noch nicht. Vier neu angehängte
Prüfungen wurden übersprungen, gemeldet wurde „10 von 10 bestanden".
Dieselbe Klasse wie MF-596 (32 Testdateien, die nicht rot werden
konnten), nur eine Ebene höher.

Der Läufer zählt jetzt die `def test_`-Zeilen im Quelltext und bricht ab,
wenn er weniger einsammelt als dort stehen (rc=1, gemessen). Fixturen mit
einem Parameter bekommt er nachgebildet. Beide Wege — ctest-Direktlauf
und pytest — melden jetzt 14 von 14.

### Offen

Für ADF und ATR braucht Phase 1 ein anderes Oracle; `unadf` und
`atrcopy` stehen dafür schon im Plan. Ob D71 und G64 über D64 hinaus
etwas zur Hebung beitragen, ist noch nicht bewertet.

---

## SCOUT-17b: die Tabelle war nie Code (MF-624, 2026-08-28)

`src/formats/uft_format_registry_v2.c` ist **gelöscht**, ihre 162 Einträge
stehen als [`FORMAT_CATALOG.md`](FORMAT_CATALOG.md) im Baum.

Die Verwaisten-Regel traf zu — alle acht exportierten Funktionen hatten
baumweit **0 Aufrufer** (einzeln nachgemessen). Der Vorbehalt aus MF-621
war aber ebenfalls richtig: drei Dokumente beziehen ihre Zahl „161
Format-IDs" von hier, und „Katalogquelle" ist kein *Plan*-Anker im Sinne
der Regel. Beides zusammen ergibt nur einen sauberen Weg: die Tabelle ist
faktisch immer Dokumentation gewesen, also wird sie eine.

`FORMAT-CLASSIFICATION.md` nennt sich „generiert" — im Baum liegt kein
Erzeuger (MF-620 hatte das schon festgestellt). Das Löschen brach deshalb
nichts Mechanisches, nur Zitate, und die zeigen jetzt auf den Katalog.

### Die neue Spalte — und was sie nicht sagt

Der Katalog trägt eine gemessene Spalte „Plugin?": steht dieser Name oder
eine seiner Endungen in der Plugin-SSOT (88 ausgeschriebene + 49
`DSK_PLUGIN()`-Ausprägungen = 137)?

**95 von 162 ja, 67 nein.**

Der erste Anlauf dieser Zahl war ein reiner Namensabgleich und ergab
„100 ohne Plugin". Er war falsch: `2MG` gegen `2IMG`, `A2R` gegen einen
tatsächlich vorhandenen Eintrag. Über Endungen nachgemessen blieben 67 —
und für `CRT`, `EDD`, `FDX` liegt sogar Code im Baum, der gebaut, aber
nie registriert wird und über `uft_disk_open()` deshalb unerreichbar ist.
Genau die Lage aus MF-446/447.

Es ist derselbe Fehler, den ich bei SCOUT-9 am Werkzeug gemessen habe:
**ein Abgleich über Namen ist kein Abgleich über Fähigkeiten** — hier
einmal in die andere Richtung, mit einer zu hohen Fehlzahl statt einer zu
hohen Erfolgszahl.

### Zwei Fallen beim Überführen, beide vom Wächter gefangen

Der Auslese-Code bekam eine Zusicherung „erkannte Zeilen == öffnende
Klammern". Sie hat zweimal gefeuert:

* `{"D81", "d81", "1581 3.5\" MFM", …}` — das maskierte Anführungszeichen
  brach das Muster. **Eine Zeile wäre stillschweigend verschwunden.**
* `{NULL, NULL, NULL, NULL, 0}` — der Abschluss-Eintrag ist keine
  Datenzeile; meine Zählung war zu grob, nicht die Tabelle falsch.

Ohne die Zusicherung hätte der Katalog 161 statt 162 Zeilen gehabt, und
niemand hätte es gemerkt.

### Nebenbei gefunden: eine dritte Registry, die der Detektor nicht sieht

`src/formats/uft_format_registry.c` (15 KB, 16 `.name`-Einträge) definiert
`uft_scp_probe`, `uft_hfe_probe`, `uft_a2r_probe`, `uft_woz_probe` und
weitere. Gemessen: `uft_hfe_probe` hat **keinen** Treffer außerhalb der
Datei; die anderen drei nur **Header-Deklarationen**, keinen Aufrufer.

Trotzdem steht die Datei **nicht** in `docs/orphan_baseline.txt`.

> **Berichtigt in MF-626 — dieser Absatz war falsch.** Ich hatte daraus
> geschlossen, der Detektor zähle Header-Deklarationen als Benutzung, und
> das als `ORPH-2` notiert. Nachgemessen mit der Logik des Skripts selbst
> (`scan_tree` + `reference_index` + `exported_names`): die Datei hat 22
> exportierte Namen, und drei davon werden wirklich gerufen —
> `uft_format_detect` aus `src/gui/ProtectionAnalysisWidget.cpp`,
> `uft_d64_probe` und `uft_format_get_handler` aus
> `src/core/uft_advanced_mode.c`. **Die Datei ist erreichbar, und der
> Detektor hat recht.** Mein Befund beruhte auf einem einfachen `grep`
> über vier `*_probe`-Namen — ein Ausschnitt, nicht die Menge.
>
> `CALL_REF` verlangt ohnehin eine eingerückte Zeile, genau damit ein
> Prototyp am Zeilenanfang nicht als Aufruf zählt
> (`audit_orphan_modules.py:150-160`). Die Lücke, die ich beschrieben
> habe, ist dort bereits geschlossen.
>
> Die **echte** Lücke steht weiter unten und ist eine andere — siehe
> `ORPH-2` in der MF-626-Notiz.

---

## SCOUT-5 zweiter Schritt: die Herkunftsfrage hat einen Fehler ausgegraben (MF-625, 2026-08-28)

Der Auftrag war ein Ähnlichkeitsvergleich gegen die üblichen Verdächtigen,
nachdem Schritt 1 in `uft_d77.c`, `uft_d88.c` und `uft_d88_parser_v2.c`
**keine einzige Herkunftsangabe** gefunden hatte.

### Das Ergebnis zur Herkunft

**Kein Anhaltspunkt für abgeschriebenen Code — und die Frage war falsch
gestellt.** Verglichen wurde gegen MAMEs `src/lib/formats/d88_dsk.cpp`:

* MAME ist **BSD-3-Clause** (`copyright-holders: Miodrag Milanovic` /
  `Olivier Galibert`), nicht GPL. Selbst eine Ableitung wäre für dieses
  Projekt zulässig und bräuchte nur Attribution — der schlimmste Fall ist
  also deutlich harmloser als angenommen.
* Strukturell verschieden: MAME hat `D88_HEADER_LEN`, `d88_dsk_identify`,
  eine Klasse `d88_format`; wir haben eine Tabellenform im Kopfkommentar
  und eine Konfidenz-Staffelung, die es dort nicht gibt.
* Die übereinstimmenden Zahlen (`0x2B0`, Medium 00/10/20/30/40, Dichte
  00/40) stammen aus einer **öffentlichen Spezifikation** —
  <https://www.pc98.org/project/doc/d88.html>, dieselbe Quelle, die schon
  MF-336 entschieden hat. Übereinstimmung mit einer publizierten Spec ist
  kein Indiz für Abschrift; sie ist das erwartete Ergebnis.

Damit ist der Fall von „unentscheidbar" auf „kein positiver Befund gegen
eine benannte Vergleichsquelle" gehoben. Die Neuimplementierung, die als
Ausweichweg im Raum stand, ist gegenstandslos.

### Was der Vergleich stattdessen fand

Die Spec nennt **zwei** Kopfgrößen: „Total Size: 688 **or 672** bytes",
164 bzw. 160 Spureinträge. MAME führt dasselbe Paar („der erste
Spur-Versatz muss 0x02A0 oder 0x02B0 sein").

**Unsere beiden Erkenner kannten nur 688.** `uft_d88.c:53` und
`uft_d77.c:84` prüften jeden Spur-Versatz gegen die feste Untergrenze
`0x2B0`. Ein Abbild mit 160-Eintrag-Kopf hat seinen ersten Versatz bei
`0x2A0` — acht Byte darunter. Folge: **`probe()` gab `false` zurück, und
`uft_disk_open()` behandelte eine vollständig gültige Datei als
unbekanntes Format.** Kein Teilverlust, kein stiller Fehler: die Tür ging
gar nicht auf.

Dazu las `d77_open()` alle 164 Einträge auch dann, wenn nur 160 existieren
— die letzten vier waren der Sektorkopf der ersten Spur, gelesen als
Versätze, was `track_count` auf 164 setzte und dem Aufrufer eine
82-Zylinder-Geometrie für eine 80-Zylinder-Diskette gab.

### Wie die Fassung unterschieden wird — erzwungen, nicht geraten

Die Kopfgröße steht nirgends in der Datei. Sie folgt aber aus der Anordnung:
Einträge 160..163 lägen bei `0x2A0..0x2AF`. Beginnen dort Spurdaten, können
dieselben Bytes nicht zugleich Tabelleneinträge sein. Der **kleinste
Nicht-Null-Versatz unter den ersten 160** entscheidet deshalb exakt. 160
Einträge sind eine volle 80-Zylinder-Diskette mit zwei Köpfen — die kürzere
Fassung verliert keine erreichbare Spur.

### Der Rotbeweis war beim ersten Anlauf grün — und damit wertlos

`tests/test_d88_header_variants.c` rief zuerst nur `plug->open()`. Vier
Prüfungen, alle grün, Fehler unverändert vorhanden: `open()` liest den Kopf
selbst und kommt an der Schranke gar nicht vorbei. Erst über `plug->probe()`
— das Tor, an dem die Erkennung wirklich entscheidet — fielen genau die
beiden 672er-Fälle um, die fallen mussten. Ein Beweis, der die fragliche
Stelle nicht durchläuft, beweist nichts; das steht jetzt im Testkopf.

### Ertrag

`d77` steigt **T3 → T2** (Test + benannte Spec-Quelle in
`docs/spec_verification.json`). Damit T2=19, T3=**55** von 88 — nachgezogen
in README und CLAUDE.md, weil das Konsistenz-Tor die Abweichung selbst
gemeldet hat.

**Offen:** `src/formats/d88/uft_d88_parser_v2.c` trägt dieselben festen 164
Einträge, hat aber kein Plugin und keinen Aufrufer. Er fällt damit unter die
Verwaisten-Regel, nicht unter diesen Fix — geprüft wird er hier nicht, und
verdrahtet wird er nicht, damit er benutzt aussieht.

---

## SCOUT-4 nach der Waisen-Regel entschieden: fdc_bitstream ist raus (MF-626, 2026-08-28)

`src/flux/fdc_bitstream/` (Yasunori Shimura, MIT, mit `LICENSE.md` und
Autorennennung im Kopf jeder Datei) ist **entfernt**, samt seiner vierzehn
Header unter `include/uft/flux/`. Zusammen **6483 Zeilen**, in jeden Bau
übersetzt.

### Die Messung, die es entschieden hat

| Frage | Antwort |
|---|---|
| Einbinder außerhalb des Subsystems? | **keiner** — alle 14 Header einzeln geprüft |
| Eintrag in `docs/orphan_baseline.txt`? | nein — siehe ORPH-2 unten |
| Benannter Plan-Anker? | **keiner** |

Die einzige Plannennung ist `MASTER_PLAN.md:435`, und die führt die
Bibliothek als Teil von **„1626 LOC Redundanz"** dreier paralleler
Decoder-Pfade — also als Problem, nicht als Vorhaben.

### Der Vorbehalt, den ich selbst hatte — und warum er nicht trägt

Ich hatte notiert, Phase 1 wolle einen unabhängigen Decoder als Oracle,
und fdc_bitstream sei genau das. Nachgelesen: **der Plan nennt seine
Oracles ausdrücklich und ausschließlich als externe Programme** —
`c1541`, `unadf`, `xdftool`, `atrcopy`, `mtools`, `cpmls`
(`PLAN_v4.1.7.md:198-199, 216, 381`). Eine in den eigenen Binärcode
übersetzte Bibliothek ist in dem Sinn, auf den es ankommt, gerade nicht
unabhängig — und ein Oracle braucht ohnehin einen Prüfstand, der sie
aufruft. Genau das war die Option „(a) verdrahten mit Differenzlauf-Plan",
also ein eigenes Vorhaben, kein Regelfall.

Einen Anker zu schreiben, um den Code zu behalten, wäre das Gegenteil der
Regel gewesen: dem Bestand nachträglich einen Zweck andichten. MIT plus
Git heißt, dass die Rücknahme billig und lizenzrechtlich sauber ist —
`git revert` auf diesen Commit, falls Phase 1 den Prüfstand doch will.

### Was die Tore beim Löschen zusätzlich fanden

* **`A:UFT_HAS_EXPERIMENTAL_VFO`** stand in
  `scripts/define_parity_baseline.json` als geprüfte Abweichung. Der
  Wächter meldete von sich aus, dass sie keine mehr ist — der Schalter kam
  nur aus dem gelöschten `experimental_vfo`-Block. Eintrag entfernt.
* **`verify_build_sources.py:88`** trug ein Ausnahmemuster für
  `vfo_experimental.cpp`. Ein Muster, das auf nichts mehr passt, ist eine
  stille Ausnahme für Code, den es nicht gibt — und lässt später
  versehentlich etwas Neues durch. Entfernt.
* Die Quelldatei-Zahlen in `CLAUDE.md` waren **in beide Richtungen**
  abgedriftet: gemessen 715 Quellen und 481 Header gegen die dort
  geführten „~693 / ~515". Nachgezogen, mit dem Zählbefehl daneben.

### Nebenbei, zum dritten Mal in dieser Sitzung

Meine erste Entfernung der `INCLUDEPATH`-Zeile schlug **stillschweigend
fehl**: im Heredoc wurde `\n` zu einem echten Zeilenumbruch, das Muster
passte nicht, und der Ersetzungsaufruf meldete nichts. Aufgefallen ist es
nur, weil ich hinterher im erzeugten `Makefile.Release` nachgesehen habe.
Die Wiederholung lief über einen Schnitt mit Zusicherung („genau eine
Zeile erwartet"), der bei Nichttreffer laut geworden wäre.


### ORPH-2, neu gefasst: der Detektor misst Dateien, nicht Subsysteme

Warum standen 6483 Zeilen ohne jeden äußeren Aufrufer **nicht** in der
Verwaisten-Grundlinie? Nicht wegen Header-Deklarationen — die Vermutung
aus MF-624 ist oben berichtigt. Der Grund steht in
`scripts/audit_orphan_modules.py:178-185`:

```python
def used_elsewhere(names, idx, own):
    for n in names:
        hits = idx.get(n)
        if hits and (hits - {own}):     # nur die EIGENE Datei fällt raus
            return True
```

Ausgeschlossen wird genau eine Datei: die geprüfte. **Geschwister zählen
mit.** `fdc_bitstream.cpp` ruft `bit_array`, `mfm_codec` ruft
`fdc_vfo_base`, und so weiter — jede der zwölf Dateien hatte damit einen
„Aufrufer" und galt als benutzt. Von außen rief keine einzige jemand.

Das ist keine Nachlässigkeit, sondern eine Grenze der gewählten Einheit:
der Wächter fragt „ruft jemand diese **Datei**?", nicht „ruft jemand in
dieses **Subsystem** hinein?". Für einzelne verwaiste Module ist das
richtig; für einen geschlossenen Ring von Dateien ist es blind, und zwar
umso zuverlässiger, je vollständiger der Ring ist.

**Offen (`ORPH-2`):** eine zweite Messung auf Verzeichnisebene — welche
Verzeichnisse unter `src/` werden von außerhalb ihrer selbst nie
referenziert? Das ist eine kleine Ergänzung derselben Indexstruktur und
würde genau die Klasse finden, die hier durchrutschte. Bis dahin gilt:
die 227 Grundlinien-Einträge sind eine Untergrenze für **Einzeldateien**
und sagen über geschlossene Subsysteme nichts.

---

## ORPH-2 gebaut — und der erste Lauf legte einen Widerspruch offen (MF-627, 2026-08-28)

`scripts/audit_orphan_modules.py --dirs` ist neu: die Messung auf
Verzeichnisebene, die MF-626 als Lücke benannt hat. **Kein Tor, ein
Bericht** — das Ergebnis braucht Auslegung, und ein Wächter, der daraus
Löschungen erzwingt, wäre falsch.

### Warum erst messen, dann verdrahten

Der erste Entwurf indizierte nur `src/` und `include/` und meldete
prompt `src/formats/nintendo` als geschlossen — obwohl `test_rom_headers`
es aufruft (MF-598). Tests gehören dazu; ein Verzeichnis, das wenigstens
ein Test benutzt, ist geprüft, nicht abgeschnitten. Dieselbe
Unterscheidung trifft der Wächter für Einzeldateien längst.

Ebenso ausgenommen: Dateien direkt unter `src/`. Dort liegt `main.cpp`,
und ein Programmstart hat definitionsgemäß keinen Aufrufer — ohne diese
Ausnahme stünden 14 513 Zeilen als „verwaist" im Bericht.

### Das Ergebnis

**40 Verzeichnisse, 22 110 Zeilen**, in die von außen nie hineingerufen
wird. Zerfällt in zwei sehr verschiedene Hälften:

| | Anzahl | Zeilen | Bedeutung |
|---|---|---|---|
| `src/formats/*` | 35 | 18 051 | **nicht registriert**, nicht überflüssig → ARCH-11/12, Antwort ist verdrahten |

Die Zuordnung „nicht registriert" ist nachgemessen, nicht gefolgert:
verglichen wurden die 35 geschlossenen Verzeichnisse gegen die 88
Verzeichnisse, in denen `scripts/gen_format_list.py` ein Plugin findet
(Schnittmenge über den Verzeichnispfad). **Die Schnittmenge ist leer.**
Kein einziges geschlossenes Verzeichnis enthält ein registriertes Plugin
— und umgekehrt landet kein registriertes Plugin fälschlich im Bericht.
Das ist zugleich die Gegenprobe, dass die Messung Plugin-Registrierungen
als Erreichbarkeit erkennt: sonst stünden Verzeichnisse wie
`src/formats/d64` mit in der Liste.

Stichprobe dazu: `src/formats/pc98` exportiert `uft_fdi98_open`,
`uft_fdi98_read_sector`, `uft_pc98_analyze` und weitere — freistehende
Funktionen, **keine** `uft_format_plugin_*`-Struktur. Es ist also nicht
ein Plugin, das der Registrierung harrt, sondern Code, der nie an den
Plugin-Weg angeschlossen wurde.
| übrige | 5 | 4 059 | einzeln anzusehen |

Die fünf übrigen: `src/analysis/profiles` (1573), `src/parsers/a2r`
(1073), `src/analysis/deepread` (979), `src/algorithms/recovery` (400),
`src/compat` (34).

### Der Befund, der weh tut

> **Zur Genauigkeit (nachgetragen MF-628):** der Bericht hat diese fünf
> Dateien **nicht entdeckt** — sie stehen seit MF-509 einzeln in
> `docs/orphan_baseline.txt`, alle fünf. Der Datei-Detektor hatte sie
> längst. Neu ist nicht der Waisenstatus, sondern der **Widerspruch**:
> dieselben Module standen in `CLAUDE.md` unter den Kernfunktionen, und
> niemand hatte die beiden Stellen nebeneinandergelegt.
>
> Der echte ORPH-2-Fall ist `src/flux/fdc_bitstream/`: davon stand
> **keine einzige** der zwölf Dateien in der Grundlinie (nachgemessen an
> `70d2e0e7^`). Genau dafür ist die Verzeichnis-Messung da — für
> geschlossene Ringe, nicht für Einzeldateien.

**`src/analysis/deepread/` hat keinen Aufrufer.** Alle **13** exportierten
Funktionen der fünf Forensik-Module — Write-Splice-Erkennung,
Magnetic-Aging-Profil, Cross-Track-Korrelation, Revolution-Fingerprint,
Soft-Decision-LLR — werden außerhalb ihres Verzeichnisses **nirgends**
genannt: nicht in `src/`, nicht in der GUI, nicht in `tests/`. Die
einzigen Treffer sind ihre eigenen Prototypen unter
`include/uft/analysis/`.

Gegengeprüft mit einfachem `grep` über alle neun `uft_deepread_*`-Namen,
unabhängig von der Skript-Logik — nach dem ORPH-2-Fehlschluss von MF-624
gehört das dazu.

Das ist dieselbe Klasse wie der Kopierschutz-Katalog (P0-2): **Bestand,
nicht Fähigkeit.** CLAUDE.md führte „5 DeepRead Forensik-Module" und „8
DeepRead-Module" unter den Kernfunktionen; beide Stellen tragen jetzt die
Messung. Die drei **Decode-Booster** sind davon nicht betroffen — sie
haben mit `src/gui/uft_otdr_panel.cpp` einen echten Aufrufer. Erreichbar
sind also **3 von 8**.

**Offen (`DEEP-1`):** verdrahten oder als unerreichbar dokumentieren — die
dritte Möglichkeit, es weiter als Kernfunktion zu führen, ist seit dieser
Messung keine mehr.

Ein Plan-Anker im Sinne der Verwaisten-Regel liegt **nicht** vor.
`MASTER_PLAN.md:332` nennt DeepRead nur unter den **Nicht-Zielen**: „die
5 DeepRead-Module sind genug, bis M2 fertig ist und tatsächlich ein
Nutzer sie ausprobiert hat". Der Satz setzt voraus, dass ein Nutzer sie
ausprobieren *kann* — er kann nicht, es ruft sie nichts. Das ist keine
Verdrahtungszusage, sondern eine dritte Stelle, an der der Baum eine
Fähigkeit annimmt, die er nicht hat.

### Was der Bericht ausdrücklich nicht sagt

Dass die 35 Format-Verzeichnisse weg können. Ein Format-Plugin ohne
Außenreferenz ist der bekannte Registrierungs-Rückstand (MF-446/447), und
die Antwort darauf steht im Plan, nicht in einer Löschliste.

---

## Zweiter ORPH-2-Fall: 1886 Zeilen Plattform-Wissen ohne Aufrufer (MF-628, 2026-08-28)

`src/analysis/profiles/` — sechs Dateien, **1886 Zeilen**, **null**
Aufrufer von außen. Gemessen mit der Logik von `audit_orphan_modules.py`
über `src/`, `include/` **und** `tests/`; jede der sechs Dateien einzeln
geprüft, keine hat eine Nennung außerhalb des Verzeichnisses.

### Warum der Datei-Detektor nur ein Drittel davon sah

In `docs/orphan_baseline.txt` standen **2 von 6**:
`uft_profile_xdf.c` und `uft_profiles_all.c`. Die vier übrigen —
`uft_profile_japanese.c`, `_misc.c`, `_uk.c`, `_us.c`, zusammen 1384
Zeilen — versteckten sich hinter ihrem eigenen Sammler: `uft_profiles_all.c`
nennt sie, also galten sie als benutzt. Dass der Sammler selbst niemanden
hat, fiel auf; dass damit der ganze Ring unerreichbar ist, nicht.

Das ist der zweite Beleg für ORPH-2 nach `src/flux/fdc_bitstream/` — und
diesmal ein Ring mit einem sichtbaren und vier unsichtbaren Gliedern
statt zwölf unsichtbaren.

### Was da liegt

Plattform-Profile: Sync-Muster, Geometrie und Encoding-Hinweise für
TI-99/4A, TRS-80 Model I/III/4, Victor 9000/Sirius 1, Kaypro, Osborne
(`uft_profile_us.c`) sowie UK-, japanische und sonstige Plattformen.
Exportiert werden unter anderem `uft_detect_profile_by_size`,
`uft_get_all_profiles`, `uft_get_profiles_by_category`,
`uft_format_requires_track_copy`.

**Kein Duplikat.** `src/flux/uft_media_profile.c` (166 Zeilen) klingt
ähnlich, ist aber etwas anderes und **lebt**: magnetisches Zeitverhalten,
gerufen aus `src/flux/uft_flux_decoder.c`,
`src/formats/uft_format_convert_flux.c` und `tests/test_media_profile.c`.
Hier geht es um Plattform-Erkennung, nicht um Zellzeiten.

### Warum ich es nicht gelöscht habe

Ein Plan-Anker liegt **nicht** vor — weder `MASTER_PLAN.md` noch
`PLAN_v4.1.7.md` nennen dieses Verzeichnis. Nach der Verwaisten-Regel
wäre das ein Löschfall.

Es ist aber nicht dieselbe Lage wie bei `fdc_bitstream`. Dort war der
Code **belegte Redundanz** (`MASTER_PLAN.md:435`, „1626 LOC Redundanz"
dreier paralleler Decoder-Pfade); hier dupliziert er nichts, und es ist
genau die Art Wissen, die ein forensisches Werkzeug für unbekannte
Disketten braucht. Löschen wäre regelkonform und trotzdem womöglich der
teurere Weg.

**Offen (`PROF-1`), Eigentümer-Entscheidung, dieselbe Form wie `DEEP-1`:**
verdrahten (dann Anker eintragen), oder löschen. Was nicht geht: weiter
liegen lassen, ohne dass eine der beiden Zeilen gezogen wird — dafür ist
die Grundlinie da, und dort stehen bislang nur zwei der sechs Dateien.

### Ein Versuch, der sofort widerlegt wurde

Ich habe die vier fehlenden Dateien in `docs/orphan_baseline.txt`
nachgetragen — und das Tor hat im selben Lauf widersprochen: es meldete
sie als „weniger als in der Grundlinie, bitte dort streichen".

Zu Recht. Die Grundlinie ist definiert als das, was der **Datei**-Detektor
meldet, und der hält die vier für benutzt, weil ihr Sammler sie nennt.
Ein Verzeichnis-Befund in einer Datei-Grundlinie erzeugt einen dauerhaften
Fehlstand, den niemand auflösen kann. Zurückgenommen.

Daraus folgt etwas für `ORPH-2` selbst: **eine Verzeichnis-Messung braucht
ihre eigene Grundlinie**, wenn sie je ein Tor werden soll — die
bestehende lässt sich dafür nicht mitbenutzen. Bis dahin ist der Bericht
das, was er ist: ein Bericht, und diese Notiz hier sein Gedächtnis.

---

## Scout-Zyklus 6: FloppyControl — und was er über unser eigenes Oracle verriet (MF-629, 2026-08-28)

Untersucht: **https://github.com/imqqmi/FloppyControl** (HEAD `0633bc7`,
letzter Commit 2026-01-26). Gutachten im Volltext:
`tools/uft-scout/out/FloppyControl.gutachten.md`.

### Lizenz — und eine Genauigkeit, die zählt

**GPL-3.0**, aus der Wurzel-`LICENSE` gelesen, nicht aus dem README
(selbst nachgeprüft). Die Lizenzmatrix führt GPL-3.0 als „für ein
GPL-2.0-Projekt nicht portierbar". Der Scout hat zu Recht ergänzt, dass
das hier eine Nuance hat: UFT steht seit MF-621 auf
`GPL-2.0-**or-later**`, und eine Verbindung mit GPL-3.0-Code ist damit
rechtlich möglich — sie hebt das Ganze aber auf GPLv3. Das ist eine
Eigentümer-Entscheidung, keine Nebenbei-Übernahme. **Bis dahin gilt: kein
Code, nur Verhaltens-Spec und Oracle-Nutzung.**

### Der wertvollste Ertrag kam aus der Gegenprobe, nicht aus dem Fund

Der Scout schlug `dskx` vor — eine echte .NET-Konsolen-CLI im Fremd-Repo
(`FloppyControlApp.net7/WindowsFormsApplication2/FAT12Extractor/src/DskX.Cli/Program.cs`;
`list`/`extract`, `--deleted`, `--deleted-only`, `--version`; selbst
gelesen, existiert). Begründung: sie entscheide den FAT12-Inhalt, „für
den floptool seit MF-623 blind ist".

**Das stimmt nicht, und die Prüfung hat sich gelohnt.** Ich habe ein
minimales 720K-FAT12-Abbild gebaut und floptool darauf angesetzt:

```
floptool flopdir pc pc_fat fat12.img
  Volume: name=UNTITLED oem_name=UFTPROBE
  file HELLO.TXT ... 0xd
floptool flophashes pc pc_fat fat12.img
  file HELLO.TXT 13 crc32 2cc7187a sha1 1b1ca0d5744a10fc96cb61...
```

floptool liest FAT12 vollständig. Es ist dieselbe Klasse Fehler wie in
MF-615, nur andersherum: dort wurde floptools Abdeckung **über**schätzt,
hier **unter**schätzt — beide Male ohne Messung am Werkzeug.

Was `dskx` wirklich hinzufügt, ist schmaler und immer noch interessant:
**gelöschte Verzeichniseinträge** (`--deleted-only`) und
Bad-Cluster-Behandlung (`0xFF7`). floptools Auflistung zeigt so etwas
nicht. Für ein forensisches Werkzeug ist genau das die interessante
Hälfte — aber es ist ein P3-Fund, kein P2.

### Was die Gegenprobe an unserem Oracle geändert hat

MF-623 hat floptool mit `identify` und `flopdir` verzeichnet. Das war
unvollständig. Das Werkzeug kann außerdem `flophashes`, `flopread`,
`flopblocks`, `flopconvert`, `flopwrite` — und **`flophashes` ist die
stärkste Oracle-Form, die dieser Baum hat**: CRC32 und SHA-1 **je Datei**.

Am Korpus gemessen:

| Aufruf | Ergebnis |
|---|---|
| `flophashes d64 cbmdos vice_c1541_35trk.d64` | `UFT MARKER`, 254 B, sha1 `56fea729e9e37c473b12c3b76fc0d3e387b39b5a` |
| `flophashes g64 cbmdos vice_c1541_35trk.g64` | `MARKER`, 254 B, sha1 `a25b5799b3708c84ede2d27fa722d69320514487` |
| `flopread d64 cbmdos … "UFT MARKER"` | 254 Byte herausgeschrieben |

Damit heißt der Phase-1-Differenzlauf nicht mehr „das Verzeichnis sieht
gleich aus", sondern **„derselbe Inhalt, byteweise nachgerechnet"**. Der
Registry-Eintrag in `tests/differential/oracles.py` nennt jetzt diese
Form als die bevorzugte.

### Die drei Vorschläge, neu gewichtet

| # | Fund | Zone | Bewertung |
|---|---|---|---|
| **SCOUT-F1** | `dskx` als Oracle für **gelöschte** FAT12-Einträge und Bad-Cluster | GELB | **P3** statt P2 — floptool deckt normalen FAT12-Inhalt bereits ab (oben gemessen). Bleibt interessant, weil gelöschte Einträge forensisch die interessante Hälfte sind. Baut auf .NET 7, ungebaut |
| **SCOUT-F2** | Fluss-Scatterplot (`Graphics.cs:1942-2160`) — Strom über Zeit, 4/6/8-µs-Farbbänder, Index-Marken | GELB | **P2, Eigentümer-Vorlage.** UFT hat gemessen keine Fluss-Visualisierung; die Datenquellen (Intervalle, MF-488-Peaks) liegen im Baum. GUI-Folge → Regel 8 |
| **SCOUT-F3** | Analog-Oszilloskop-Rettungspfad (`WaveformEdit.cs:334/636-744/796`) + 18-MB-Fixture | GELB | **P3, Eigentümer-Vorlage.** UFT hat keinerlei Analog-Ingest. Urheberrecht am Fixture-Inhalt UNGEKLÄRT — Beschaffung nur nach Vorlage |

Als Fundus abgelegt und **nicht** vorgeschlagen: DiskSpare-Spec und
„2M"-Format (Einfrier-Regel), AddNoise-Dither-Mining (Forensik-Vorbehalt:
erfundene Daten), StepStick-Mikroschritte und die Frage nach einem
siebten Controller (Hardware-Folge, MF-310).

FloppyControl selbst taugt **nicht** als Decoder-Oracle: WinForms-GUI,
nicht skriptbar. Einzig `dskx` ist ein Konsolenprogramm.

### Zur Verfahrenslage

Das Repo stand bereits als `bewertet` in `data/known_negatives.json`
(2026-08-23). Gemessen: **0 Commits seit der Bewertung**. Regel 6
verlangt für einen erneuten Vorschlag eine gemessene wesentliche
Änderung — die gibt es nicht. Die drei Funde stehen hier trotzdem, weil
sie Bereiche betreffen, die in keinem Baum-Dokument als abgedeckt
nachweisbar sind; die Übernahme ist ausdrücklich Eigentümer-Entscheid.

### Werkzeug-Befunde am Scout selbst

* **W12:** `vermessen.py` zählt Domänen-Treffer in
  Webseiten-Mitschnitten (`*_files/`) und PDFs mit — der Score 24 für
  dieses Repo ist überwiegend Rauschen („GCR" in `dashicons.css`).
* **W13:** Die Ratenbremse in `gutachten.py` liest nur die ersten 400
  Byte einer Datei; vier längst abgearbeitete Gutachten trugen deshalb
  keine Übernahme-Marke. Nachgetragen mit gemessenem MF-Anker
  (fdc_bitstream→MF-626, mame→MF-623, greaseweazle-restorer→MF-611,
  hxcfe_file_selector→MF-614). Vier bleiben echt offen.

### UNGEKLÄRT

`dskx` ist **nicht gebaut und nicht ausgeführt** — der Versionsstring
stammt aus der Quelle, nicht aus einer Prozessausgabe. Vor einem
Registry-Eintrag muss er laufen, sonst wäre es ein Oracle auf
Zusicherung. Ebenfalls offen: das Urheberrecht am `.wvfrm`-Fixture, und
ob FloppyControls SCP-/KryoFlux-Import als Zweitmeinung taugt (GUI-only,
nicht geprüft).

---

## Phase-1-Bereitschaft für D64: der Leser ist da, die Tür fehlt (MF-629, 2026-08-28)

Aus `flophashes` folgt eine Frage, die der Plan bislang nicht gestellt
hat: für einen Verzeichnis-Differenzlauf braucht es **zwei** Seiten.
floptool liefert seine — was liefert UFT?

### Gemessen

`src/formats/d64/uft_d64_parser_v3.c` **liest das Verzeichnis
vollständig**. Ab Zeile 1085 läuft es über die Einträge der
Verzeichnisspur, füllt `d64_dir_entry_t directory[]` mit Dateityp,
Startspur/-sektor, entpacktem Dateinamen und REL-Feldern. Die Datei steht
**nicht** in der Verwaisten-Grundlinie; ihr `main()` ist mit
`#ifdef D64_V3_TEST` gekapselt, kollidiert also nicht.

Erreichbar ist davon aber nichts. Der Weg nach draußen führt über
`src/formats/uft_v3_bridge.c`, und dort haben von vier
`uft_d64_v3_*`-Funktionen **drei keinen Aufrufer**:

| Funktion | Aufrufer |
|---|---|
| `uft_d64_v3_detect_protection` | `src/core/uft_advanced_mode.c` |
| `uft_d64_v3_get_diagnosis` | **keiner** |
| `uft_d64_v3_handler` | **keiner** |
| `uft_d64_v3_write` | **keiner** |

Und selbst die Diagnose würde nicht helfen: `uft_v3_bridge.c:332-350`
setzt eine Zeichenkette aus **Spurzahl und Dateigröße** zusammen — das
Verzeichnis kommt darin nicht vor.

Das registrierte Plugin (`uft_d64_plugin.c:220-229`) bietet
`probe/open/close/read_track/write_track/verify_track`. Spur und Sektor,
keine Datei-Ebene. Auch `src/fs/` hat keinen CBM-DOS-Treiber — dort
liegen AmigaDOS, FAT12 und Hilfsmodule.

### Was das für den Plan heißt

Phase 1 nennt als ersten Schritt „CBM DOS (D64), Oracle `c1541`, kleinstes
Format, Korpus liegt bereits". Der Schritt ist **kleiner** als er
aussieht, aber ein anderer als beschrieben:

* **Nicht** „einen CBM-DOS-Leser schreiben" — es gibt einen, und er
  arbeitet.
* **Noch nicht** „Verzeichnisse vergleichen" — auf UFT-Seite kommt
  nichts heraus, was man vergleichen könnte.
* Sondern: **das bereits Gelesene nach außen führen.** Eine Funktion, die
  `disk->directory[]` in eine vergleichbare Form bringt, plus ein
  Aufrufer. Danach steht der Differenzlauf gegen
  `floptool flophashes d64 cbmdos` sofort offen — mit SHA-1 je Datei,
  nicht nur mit Namen.

Das ist dieselbe Gestalt wie bei DeepRead (`DEEP-1`) und den
Plattform-Profilen (`PROF-1`): **das Können liegt im Baum, der Zugang
fehlt.** Drei Fälle in einer Sitzung, aus drei ganz verschiedenen
Richtungen gefunden.

**Offen (`PH1-1`):** `uft_d64_v3_get_diagnosis` um die Verzeichnisliste
erweitern oder eine eigene Ausleitung schreiben, dann Aufrufer setzen.
Erst danach ist der D64-Differenzlauf Arbeit statt Vorarbeit. Vor dem
Code steht wie immer der Rotbeweis: ein Test, der die UFT-Liste gegen
die floptool-Liste stellt und **rot** ist, weil UFT nichts liefert.

---

## Einspruch des Eigentümers zu F2 — und was dahinter lag (MF-630, 2026-08-28)

Der Eigentümer hat dem Satz „UFT hat gemessen keine Fluss-Visualisierung"
widersprochen und zwei mögliche Auflösungen benannt: entweder sind die
Widgets in der Verwaisten-Bereinigung gefallen, oder gemeint war präzise
„keine **Zeitbereichs**-Darstellung".

**Beides trifft nicht zu. Es ist ein dritter Fall.**

### Gemessen

| Datei | Zeilen | im Bau | instanziiert |
|---|---|---|---|
| `src/widgets/fluxvisualizerwidget.cpp` | 1092 | ja | **nein** |
| `src/uft_flux_histogram_widget.cpp` | 821 | ja | **nein** |
| `src/gui/uft_otdr_panel.cpp` | 942 | ja | ja |

`fluxvisualizerwidget.h` definiert `FluxViewMode` mit **fünf** Modi —
`WAVEFORM`, `HISTOGRAM`, `SPECTROGRAM`, `CELL_VIEW`, `COMPARISON`
(Multi-Revolutions-Vergleich) — plus `drawSyncPatterns()`,
`drawWeakBits()`, `drawRegions()`, `drawRuler()`, `drawMarker()`.
`WAVEFORM` **ist** eine Strom-über-Zeit-Darstellung, und `COMPARISON`
geht über FloppyControls Scatterplot hinaus.

Instanziiert wird davon nichts: Suche nach `FluxVisualizerWidget` und
`UftFluxHistogramWidget` über alle `*.cpp`/`*.h` unter `src/` liefert
Treffer **nur in den Widget-Dateien selbst**. 1913 Zeilen Fluss-Anzeige
werden in jeden Bau übersetzt und nie erzeugt.

### Der Zuschnitt ändert sich damit

F2 ist **kein neues Widget und keine neue Ansicht**, sondern:

1. den vorhandenen Widget-Stapel verdrahten (Ansicht im bestehenden
   Stack, wie der Eigentümer vorgibt),
2. **erst danach** prüfen, ob `WAVEFORM` um Periodendarstellung mit
   µs-Farbbändern und Index-Marken ergänzt werden muss.

Ein neues Widget hätte genau die „fünf Bedeutungen für einen Namen"-Klasse
erzeugt, vor der der Eigentümer gewarnt hat.

Die Vorgabe zur Datenquelle bleibt: MF-501-Winkellage plus
Multirev-Klassifikation, **ein Datenmodell, zwei Ansichten** (später die
Polarkarte, Welle 3.2). Arbeitsteilung: Logik und kopflose Qt-Tests hier,
Klick-Abnahme beim Eigentümer, Protokoll ins CLICK_SESSION-Dokument.

### ORPH-3 — warum die Werkzeuge geschwiegen haben

`exported_names()` in `scripts/audit_orphan_modules.py` liefert für
klassenbasierte C++-Dateien eine **leere Menge**, und das Skript
überspringt Dateien ohne Exporte (`if not names: continue`).

Gemessen über `src/`:

| | Anzahl | Zeilen |
|---|---|---|
| `.cpp` **ohne** erkannte Exporte | **50 von 56** | **26 918** |
| davon GUI/Widget | 15 | 9 661 |
| `.c` ohne erkannte Exporte | 3 von 531 | — |

Die C-Seite ist also sauber erfasst, **die C++-Seite praktisch gar nicht**.
Damit haben weder die Datei- noch die Verzeichnis-Messung je etwas über
die Oberflächenschicht ausgesagt — dieselbe Schicht, die
`gui_layer_was_unaudited` als „15 216 Zeilen Oberfläche, 2 Qt-Tests"
führt.

**Offen (`ORPH-3`):** `exported_names()` um Klassenmethoden und
Qt-Metaobjekte erweitern, oder eine eigene Messung „welche
QWidget-Ableitung wird nie instanziiert?". Bis dahin gilt für beide
Waisen-Messungen: **sie sprechen über C, nicht über C++.** Das steht
jetzt hier, statt als stille Annahme.

### Zum Muster

Das ist der vierte Fall an einem Tag, in dem das Können im Baum liegt und
der Zugang fehlt — nach DeepRead (`DEEP-1`), den Plattform-Profilen
(`PROF-1`) und dem D64-Verzeichnis (`PH1-1`). Alle vier aus verschiedenen
Richtungen gefunden, keiner durch dieselbe Messung.

---

## Entscheidungen des Eigentümers zu Scout-Zyklus 6 (MF-630)

**SCOUT-F1 — angenommen, Reihenfolge bindend.** .NET-7-SDK auf die
Dev-Maschine (optionale CI-Spur, **kein** Pflichtpfad), `dskx` bauen,
gegen **dasselbe** selbstgebaute 720K-Fixture laufen lassen, das schon
floptool vermessen hat — **erst danach** Registry-Eintrag, eng
geschnitten auf **gelöschte Einträge und Bad-Cluster**, denn dafür ist es
laut Messung das einzige Werkzeug. Zeitpunkt: **mit dem FAT-VFS-Baustein,
nicht vorher** — es ist das forensische Gegenstück zur
Datei-Schadenskarte und hat allein keinen Abnehmer. Begründung des
Eigentümers: „Oracle auf Zusicherung abzulehnen ist exakt die Disziplin,
die den Baum trägt."

**SCOUT-F2 — angenommen als P2**, mit dem Zuschnitt oben. Der Scatterplot
ist nicht nur Anzeige, sondern **das fehlende Bedienelement der geplanten
Rettungskette**: Fensterwahl für das CRC-Orakel und die
Overlay-Differenzkarte brauchen genau diese Zeitbereichs-Sicht.

**SCOUT-F3 — P3 bestätigt, Fixture abgelehnt.** 18 MB mit ungeklärtem
Urheberrecht sind für **Daten** dieselbe ROT-Zone wie für Code; ein
Fixture, das nicht verteilt werden darf, vergiftet den Korpus. Bessere
Beschaffung: mit dem Start des Analog-Import-Bausteins die
Referenzaufnahme **selbst** erzeugen — eigenes Scope, eigene Diskette,
Provenienz im README, und nebenbei die erste echte Messung für die offene
FLUX-15-Winkelfrage. Bis dahin Fundus.

**Regel 6 präzisiert** (steht jetzt in `tools/uft-scout/AGENT.md`): sie
gilt streng nur für `verworfen`. `bewertet` heißt nicht `erschöpft` —
Neubesuch zulässig bei neuer Fragestellung, und das Gutachten muss den
Anlass benennen.

**`flophashes` ist ab jetzt der Standard** für **jeden** VFS-Differenzlauf,
nicht nur für D64: verglichen wird der Inhalt byteweise über CRC32/SHA-1
je Datei, nicht die Verzeichnisdarstellung. Eingetragen am
Registry-Eintrag und in `PLAN_v4.1.7.md`.

---

## Scout-Zyklus 7: nibtools — der schärfste „Können ohne Tür"-Fall bisher (MF-634, 2026-08-28)

Untersucht: **https://github.com/rittwage/nibtools** (HEAD `0abdc11`,
2025-06-26). Gutachten: `tools/uft-scout/out/nibtools.gutachten.md`.
Fünf Funde, alle gemessen. Ich habe die tragenden Behauptungen
nachgeprüft; drei davon musste ich präzisieren.

### Lizenz — meine Annahme im Auftrag war falsch

Ich hatte dem Scout mitgegeben, nibtools sei „vermutlich GPL-2, also die
grüne Zone". **Das stimmt nicht.** Selbst nachgesehen: `LICENSE` ist der
GPL-3-Volltext, hinzugefügt in Commit `a549c18` („Create LICENSE"). Davor
führte das Repo **keine** Lizenzdatei.

**Zone GELB: kein Code portierbar.** Erlaubt sind Verhaltens-Spec und
Oracle-Nutzung — und genau da liegt hier der Wert.

### Die Lizenzfrage, die daraus folgt (SCOUT-23)

Vier Dateien in unserem Baum behaupten Ableitung:

| Datei | Wortlaut |
|---|---|
| `src/formats/c64/uft_d64_g64.c:5` | „Based on nibtools by Pete Rittwage" |
| `src/formats/c64/uft_gcr_ops.c:5` | „Based on nibtools **gcr.c**" |
| `src/formats/c64/uft_nib_format.c:5` | „Based on nibtools"; dazu `:42` „LZ77 Compression (from nibtools by Marcus Geelnard)" |
| `src/protection/c64/uft_track_align.c:5` | „Based on nibtools by Pete Rittwage and Markus Brenner"; `:479` nennt `prot.c align_rl_special()` |

Zwei weitere Treffer sind **keine** Ableitungsbehauptung und gehören
nicht in dieselbe Schublade: `src/formats/g71/uft_g71.c:15` nennt
nibtools als *Referenz*, `src/hardware_providers/xum1541_provider_v2.h`
erwähnt es in Prosa.

**Das Datum entscheidet, und ich habe es gemessen:** alle vier kamen am
**2026-02-08** (Commit `4d622192`) in den Baum — über ein Jahr **nach**
der GPL-3-Lizenzierung des Upstream (2025-01-30). Der Scout nannte
2026-01-16; das ist das Datum im Dateikopf, nicht das der Aufnahme.

Damit ist die Frage nicht akademisch: sind das **Ports** oder
**Verhaltens-Nachbauten**? Bei Port müsste das Ergebnis GPL-3 sein —
unser Baum steht seit MF-621 auf `GPL-2.0-or-later`, was eine Verbindung
erlaubt, aber das **Ganze auf GPLv3 hebt**. Das ist eine
Eigentümer-Entscheidung, keine Nebenbei-Übernahme. Optionen: Herkunft
klären, Freigabe bei Rittwage erbitten, oder neu schreiben. Der
SPDX-Zensus aus MF-620/621 konnte das nicht sehen — die Dateien tragen
keinen fremden SPDX-Bezeichner, nur Fließtext.

### SCOUT-22: UFT liest C64-NIB überhaupt nicht

`src/formats/c64/uft_nib_format.c` hat **1100 Zeilen**, liest NIB/NB2/NBZ
und hat einen grünen Test. Nachgemessen über alle 27 exportierten
Funktionen:

* **15** werden ausschließlich von Tests gerufen,
* **11** von niemandem,
* **1** von der Produktion — `nib_format_name`, ein Namens-Helfer, aus
  `src/formats/nib/uft_nib_parser_v2.c`. **Und diese Datei steht selbst in
  `docs/orphan_baseline.txt`.** Der einzige Produktions-Aufrufer ist also
  auch tot.

Das registrierte `nib`-Plugin ist eine **andere** Datei und etwas
anderes: `src/formats/nib/uft_nib.c` heißt „Apple II Nibble", `:10`
setzt `NIB_FILE_SIZE 232960` (35 × 6656). Eine Commodore-MNIB-Datei
trifft diese Probe nie.

Die Formatliste führt „NIB". Für den Benutzer heißt das heute: **eine
C64-NIB-Datei wird nicht gelesen**, und die 1100 Zeilen, die es könnten,
sind unerreichbar. Das ist der fünfte Fall dieser Gestalt an einem Tag —
nach DeepRead (`DEEP-1`), den Plattform-Profilen (`PROF-1`), dem
D64-Verzeichnis (`PH1-1`) und dem Fluss-Widget (MF-630/632, inzwischen
verdrahtet) — und der einzige, bei dem die Formatliste eine Fähigkeit
verspricht, die der Öffnungspfad nicht hat.

Dazu drei gemessene Spec-Abweichungen gegen `fileio.c` (Halbspur 84 wird
still verworfen, abweichende Dichte-Flag-Behandlung, Kapazitätsschranken
ohne Entsprechung). **Spec-Korrekturen gegen eine autoritative Quelle
sind einfrier-frei** und können sofort gemacht werden; die
*Registrierung* als Plugin ist „neue Registrierung" im Sinne von MF-363
und wartet auf das Moratorium (danach 1:2).

### SCOUT-20 und SCOUT-21: ein hardwarefreies Zweitoracle

Der Scout hat `nibconv`, `nibscan` und `nibrepair` **heute gebaut** —
mit MinGW 13.1.0, ohne OpenCBM-Bibliothek, ohne Patches — und auf dem
Korpus laufen lassen. `nibscan` liefert Dichteprofil je Spur,
Fehlerkarte, Bad-GCR-Zählung, Killer/Fat/RapidLok-Signale und
**BAM/DIR-CRC plus Full-CRC über dekodierte Spuren**: das Spur-Analogon
zu `flophashes`, und die zweite Hand neben `c1541`, die
`PLAN_v4.1.7.md:224-230` wörtlich verlangt.

Wichtiger noch: `nibconv G64→D64` weicht auf `vice_c1541_35trk.g64` von
der VICE-Referenz in **exakt den drei Sektoren und 143 Byte-Positionen
aus MF-536** ab (T17/0: 127, T18/0: 6, T18/1: 10). Die dritte Quelle, die
der Matrix-Kommentar dort als fehlend führt, **existiert also** — und sie
bestätigt, dass die Abweichung im G64-Inhalt steckt (PETSCII-Fall), nicht
im Wandler.

Vorbehalt, den der Scout selbst benennt und der bleibt: `uft_d64_g64.c`
ist „Based on nibtools" — als Oracle ist das eine **verwandte Hand**,
derselbe Einwand wie im AdfOpus/ADFlib-Fall. Für die MF-536-Frage
entscheidet es trotzdem etwas, weil dort VICE die Gegenseite ist.

### SCOUT-24: eine Definition, die wir vermissen, steht dort

`docs/KNOWN_ISSUES.md:1267ff` führt `density_deviation` als undefiniert.
`nibscan.c:532-536` definiert es: gespeicherte Dichte ≠ `speed_map`-Vorgabe.
Dazu zwei benannte GCR-Fehllese-Modelle (`nibrepair.c:228-230`,
Tri-Bit- und Low-Frequency-Verwechslung), die unsere Recovery-Schicht
nicht führt.

### Beschaffungsliste: leer

Das Oracle ist der Klon plus ein dokumentiertes Build-Rezept (heute
verifiziert), der G64/D64-Korpus liegt, und das NIB-Rotbeweis-Fixture ist
aus liegendem Material erzeugbar. Zum ersten Mal in dieser Serie kostet
ein Fund den Eigentümer **keine** Beschaffung.

### Offen

`SCOUT-20` (Oracle-Eintrag; Version ist nur ein Build-Datum, also
`version_is_unaskable` + SHA-256-Anker), `SCOUT-21` (MF-536 nachmessen),
`SCOUT-22` (drei Spec-Korrekturen sofort, Plugin-Frage später),
`SCOUT-23` (Lizenz-Vorlage, Eigentümer), `SCOUT-24` (Definition +
Fehlermodelle).

Ungeklärt bleibt unter anderem, ob UFTs Ausgabe **wertgleich** zu
nibconv ist — belegt ist bisher nur Positions- und Anzahlgleichheit gegen
VICE.

---

## SCOUT-23 entschieden: ein Port, zwei Eigenständige, zwei Löschungen (MF-635, 2026-08-28)

### Vorweg eine Faktenkorrektur

Die Vorgabe sagte „solange v4.1.6 nicht getaggt ist, gehört das vor den
Tag". **v4.1.6 ist getaggt** (`e1b56dee`) und seit dem 2026-08-26
öffentlich released — und **alle vier Dateien sind darin enthalten**
(einzeln mit `git cat-file -e v4.1.6:<pfad>` geprüft). SCOUT-23 ist damit
kein Blocker *vor* einem Tag, sondern ein Befund *in einem
veröffentlichten Release*. Am Vorgehen ändert das nichts; an der
Dringlichkeit schon.

### Das Audit, pro Datei entschieden

Methode wie vorgegeben: Struktur, Funktionszerlegung, Fehlerbehandlung,
charakteristische Schwellwerte, Kommentar-Echos. **Nicht** die
GCR-Tabellen — die sind Commodore-Spezifikation und beweisen nichts.
Beweiskräftig sind Idiome.

| Datei | Urteil |
|---|---|
| `src/protection/c64/uft_track_align.c` | **PORTIERT** |
| `src/formats/c64/uft_gcr_ops.c` | eigenständige Rümpfe, nibtools-**Vokabular** |
| `src/formats/c64/uft_d64_g64.c` | **eigenständig** |
| `src/formats/c64/uft_nib_format.c` | Audit entfällt — gelöscht (s.u.) |

**`uft_track_align.c` — der Beweis.** `shift_buffer_left` steht zeichen-
gleich da:

| | nibtools `prot.c:157` | UFT `uft_track_align.c:253` |
|---|---|---|
| | `int carryshift = 8 - n;` | `int carryshift = 8 - bits;` |
| | `tempbuf[length] = 0x00;` | `temp[length] = 0x00;` |
| | `buffer[i] = (tempbuf[i] << n) \| (carry >> carryshift);` | `buffer[i] = (temp[i] << bits) \| (carry >> carryshift);` |

Der Bezeichner `carryshift` ist erfunden, nicht abgeleitet — zwei
unabhängige Autoren wählen ihn nicht beide. Dazu **sieben wörtliche
Kommentar-Echos** („back up a little", „set first byte to shift", „shift
buffer left to edge of sync marks", „36 - 42 (non-standard)") und eine
Funktion-für-Funktion-Entsprechung: **alle 14** Funktionen aus `prot.c`
haben ein Gegenstück, **acht namensgleich** (`align_pirateslayer`,
`align_vmax`, `align_vmax_new`, `fix_first_gcr`, `fix_last_gcr`,
`search_fat_tracks`, `shift_buffer_left/right`).

**`uft_gcr_ops.c` — warum es kein Port ist.** Drei Rümpfe verglichen,
alle drei anders gebaut: `strip_runs` (nibtools kompaktiert in-place über
`source`/`buffer` mit `run`-Zähler und den Parametern
`length_max, minrun, target`; hier eigener Ausgabepuffer,
Lauflängen-Scan, getrennte Zweige, Parameter `min_sync, min_gap`),
`kill_partial_sync` (drüben vier feste 1000er-Felder und ein
`locked`-Automat mit 10-Bit-Sync-Idiom; hier eine Suchschleife),
`reduce_runs` (drüben `do/while` mit fünf Parametern, hier ein Einzeiler
mit vier). Ein einziges Kommentar-Echo, und das ist der Fachbegriff
„header checksum". **Übernommen ist das Vokabular** — die Zerlegung folgt
nibtools' Funktionsnamen mit `gcr_`-Präfix. Das steht jetzt so im Kopf.

**`uft_d64_g64.c`** — keine Entsprechung zu `convert_GCR_sector`, keine
der auffälligen Konstanten (Blockmarke `0x07`, `0x4b` „Original Format
Pattern"), keine Tri-Bit-/Low-Frequency-Notizen, ein Echo („header
checksum").

**Die Köpfe der beiden Eigenständigen sind berichtigt.** „Based on X" ist
eine Ableitungserklärung, keine Höflichkeit; sie lauten jetzt „Verhalten
nach der nibtools-Dokumentation und -Quelle — EIGENSTÄNDIGE
Implementierung, kein Port" samt der Messung, die das trägt, und dem
Oracle (`nibscan`/`nibconv`).

### Zwei Löschungen

**`uft_nib_format.c` (1100 Zeilen) + `tests/test_nib_format.c` (557) +
`include/uft/formats/c64/uft_nib_format.h` (gelöscht).** Verwaisten-Regel:
kein erreichbarer Weg dorthin.

> **Berichtigung meiner eigenen Zahl von MF-634:** ich hatte geschrieben,
> eine der 27 Funktionen habe einen Produktions-Aufrufer
> (`nib_format_name` aus `uft_nib_parser_v2.c`). **Falsch.** Jene Datei
> definiert bei `:233` eine **eigene `static`**-Funktion gleichen Namens.
> Es waren **null** Produktions-Aufrufer. Der Referenz-Index ist
> namensbasiert und hat einen `static`-Namensgleichklang als Aufruf
> gezählt — dieselbe Klasse Fehler wie überall heute, diesmal in meiner
> eigenen Messung.

**`uft_track_align.c` (1175) + Header (622) + `tests/test_track_align.c`
(602) — Quarantäne.** Auch hier kostet es **keine Fähigkeit**: gemessen
null Produktions-Aufrufer. Der einzige Kandidat war wieder ein
Namensgleichklang — `find_sync`, und `uft_mfm_sector_parser.c:104` wie
`uft_g64.c:223` führen je ein **eigenes `static find_sync`** mit anderer
Signatur. (Nebenbei: `uft_track_align.c:128` exportierte ein
nicht-statisches `find_sync` in den globalen Namensraum.)

Der Neubau kommt Clean-Room und Oracle-first gegen `nibscan`, wenn der
Baustein drankommt. **Nicht** gewählt wurde der Ausweg „Projekt auf GPLv3
heben": das wäre Relizenzierung unter Kontaminationsdruck, verbaut die
geplanten FluxEngine-GPL-2.0-Ports — und heilt den schlimmsten Fall
nicht, denn vor 2025-01-30 war nibtools **lizenzlos**, und lizenzlos ist
unter keiner Lizenz einbaubar.

### SCOUT-22, Listen-Teil: die Oberfläche versprach fünf Formate zu viel

Der Registry-Eintrag war schon ehrlich („Apple II Nibble"). Die Unwahrheit
stand in `src/formattab.cpp:69`: die Auswahlliste **„Commodore 64/128"**
bot `NIB`, `NBZ`, `P64`, `X64`, `T64`, `TAP` an. Gegen die Plugin-SSOT
gemessen: **fünf davon haben gar kein Plugin**, und `NIB` trifft nur ein
gleichnamiges — das Apple-II-Plugin, dessen Probe exakt 232 960 Byte
verlangt. **Sechs von zehn Einträgen waren ein Versprechen ohne Deckung.**

Eine Beschriftung wie „NIB (nicht lesbar)" war keine Option: der gewählte
Text wandert als `p.format` weiter (`:1137`). Die fünf sind gestrichen,
mit der Messung als Begründung im Code. Die Plugin-Beschreibung heißt
jetzt „Apple II Nibble (nicht Commodore MNIB)" — ein Name mit zwei
Bedeutungen ist die MF-559-Klasse.

### GUI-1: das ist nicht auf Commodore beschränkt

Weil das Geschwister-Muster heute schon zweimal zugeschlagen hat, habe
ich die **ganze** GUI-Formatdatenbank gemessen statt nur die eine Liste:

| | |
|---|---|
| Systeme in `m_systemFormats` | **33** |
| angebotene Format-Einträge | **156** |
| davon ohne jedes Plugin | **32 (21 %)** |

Die schwersten: PC/DOS 7 von 14 (`DMF, 2M, 360K, 720K, 1.2M, 1.44M,
2.88M` — bei den Kapazitätsangaben ist es zusätzlich ein
Kategorienfehler), Flux (raw) 4 von 12 (`KFRAW, GWRAW, A2R, MFM`), ZX
Spectrum 3 von 10, Amiga 2 von 5 (`ADZ, HDF`).

**Offen (`GUI-1`), Eigentümer:** ich habe nur das System bereinigt, nach
dem gefragt war — die übrigen 27 Einträge brauchen Urteil (Kapazität vs.
Format) und sind kein Nebenbei-Schnitt. Der strukturelle Weg wäre, die
Liste aus der SSOT abzuleiten statt sie zu pflegen; ein Tor darauf würde
heute 32-mal feuern, und ob man die als Grundlinie einfriert oder
abarbeitet, ist eine Entscheidung, keine Mechanik.

---

## LIZ-1: der Attributions-Zensus — SCOUT-23 war die Spitze, nicht der Fall (MF-636, 2026-08-28)

Die Werkzeug-Lehre aus SCOUT-23 lautete: der SPDX-Zensus war blind für
Fließtext-Attributionen, und genau daran hat der Fall ein Jahr lang
vorbeigehangen. `scripts/audit_spdx_policy.py` hat jetzt eine zweite
Stufe, die Kopfkommentare auf „based on / adapted from / derived from /
port of / portiert aus / nach dem Vorbild / taken from / originally by"
absucht.

**Bewusst kein Tor, sondern eine Liste.** Eine Attribution ist nichts
Verbotenes; sie ist etwas Entscheidungsbedürftiges. Ein Tor würde
erzwingen, dass jemand sie wegdefiniert.

### Der erste Lauf

**88 Attributionen** (unsere eigenen Erklärkommentare aus MF-635
abgezogen). Klassifiziert:

| Klasse | Anzahl | Bedeutung |
|---|---|---|
| **A — ausdrückliche Port-Erklärung** | **7** | „Port of …" / „portiert" |
| **B — nennt fremde Codebasis** | **43** | davon **2 mit genannter Lizenz**, **41 ohne** |
| C — Spezifikation, Doku, Messung | 38 | rechtlich unkritisch |

**Klasse A im Wortlaut:**

* `src/core/uft_interleave.c` — „Port of a8rawconv 0.95's
  `compute_interleave()` by Avery Lee"
* `src/core/uft_write_precomp.c` — „Port of a8rawconv 0.95's
  `postcomp_track_mac800k` by Avery Lee"
* `src/formats/amiga/uft_amiga_protection.c` — „port of XCopy Pro
  (1989-2011) 68000 Assembly algorithms" (und `:47` „Port of `ROL.L #1,D0`")
* `src/formats/ipf/uft_ipf_air.c` — „port of AIR `IPFReader.cs` /
  `IPFStruct.cs` / `IPFWriter.cs` to C"
* `src/formats/kfx/uft_kfstream_air.c` — „port of AIR `KFReader.cs` to C"
* `src/formats/stx/uft_stx_air.c` — „port of AIR `PastiRead.cs` /
  `PastiStruct.cs` / `PastiWrite.cs` to C"

Bemerkenswert: `src/a8rawconv/` ist als vendortes Verzeichnis ausgenommen
— diese beiden Dateien liegen in `src/core/`, also **außerhalb** der
Ausnahme.

**Klasse B, die mit Lizenz** (also vorbildlich): `uft_cbm_formats.c`
(cbmconvert, GPLv2+), `uft_dms.h` (xDMS 1.3, Public Domain).

**Klasse B ohne Lizenz**, Auswahl: hactool (SciresM), bbctapedisc, SPS
CAPS Library, DrCoolZic/Aufit, dec0de (Orion ^ The Replicants), MAME
`lib/formats` (Olivier Galibert), HxCFloppyEmulator (dreimal),
msa-to-zip, qbarnes/catweasel-cw und /gw2dmk, libdsk `diskdefs`, opencbm
`xum1541.c`, ipflib 4.2 / spsdeclib 5.1, RTCExtractor, Super-Kit 1541
V2.0.

### Was das heißt

SCOUT-23 hat vier Dateien betroffen, von denen eine ein Port war. Hier
stehen **50 Dateien mit einer Ableitungs- oder Codebasis-Erklärung, davon
48 ohne Lizenzangabe** — und alle sind in v4.1.6 veröffentlicht.

Das ist **kein** Vorwurf an den Code: die meisten dieser Attributionen
sind vermutlich korrekt und harmlos (MAME ist BSD-3, HxC ist GPL-2, xDMS
ist Public Domain — alles verträglich). Der Befund ist, dass **niemand es
weiß**, weil die Lizenz nicht dabeisteht.

**Offen (`LIZ-1`), Eigentümer.** Vorschlag zur Reihenfolge, nach Risiko:

1. **Klasse A zuerst** (7 Erklärungen, 6 Dateien) — dieselbe Prüfung wie
   SCOUT-23: Idiom-Vergleich gegen die Quelle, dann entweder Kopf
   berichtigen oder Quarantäne. Die AIR-Dateien (`.cs` → C) sind der
   klarste Fall, weil eine Sprachübersetzung eine Ableitung bleibt.
2. **Klasse B: Lizenz nachtragen**, nicht neu bewerten. Für die meisten
   ist sie in einer Minute nachschlagbar; die Arbeit ist, sie
   hinzuschreiben. Wo sie sich nicht klären lässt, wird daraus ein
   eigener Fall.
3. Klasse C braucht nichts.

Was **nicht** geht: die Erklärungen entfernen, um die Liste zu leeren.
Eine Attribution zu löschen macht aus einer offenen Frage eine
verschwiegene.

---

## LIZ-1 Klasse A vermessen: drei selbsterklärte GPL-3.0-Ports (MF-638, 2026-08-28)

Die sieben Port-Erklärungen aus dem Zensus (MF-636) sind einzeln geprüft.
Die Lizenz stand in **fünf von sieben Fällen im Kopf selbst** — mein
Klassifikator hatte sie nur nicht gesehen, weil sein Fenster 60 Zeichen
maß und die Lizenz meist in der Folgezeile steht. Die Zahl „41 ohne
Lizenz" aus MF-636 ist damit **zu hoch**; für Klasse A gemessen:

| Datei | Quelle | Lizenz | erreichbar? |
|---|---|---|---|
| `src/core/uft_interleave.c` | a8rawconv 0.95 (Avery Lee) | **GPL-2-or-later** | — |
| `src/core/uft_write_precomp.c` | a8rawconv 0.95 (Avery Lee) | **GPL-2-or-later** | — |
| `src/formats/ipf/uft_ipf_air.c` | AIR, Jean Louis-Guerin | **GPL-3.0** | **ja** |
| `src/formats/kfx/uft_kfstream_air.c` | AIR, SPS & Louis-Guerin | **GPL-3.0** | nein |
| `src/formats/stx/uft_stx_air.c` | AIR, Jean Louis-Guerin | **GPL-3.0** | nein |
| `src/formats/amiga/uft_amiga_protection.c` | XCopy Pro (1989-2011), 68000-Assembler | **keine genannt** | nein |

**Die beiden a8rawconv-Ports sind sauber** — GPL-2-or-later ist mit
unserem Baum verträglich, die Attribution nennt Autor und Lizenz, und
`include/uft/core/…​.h` trägt laut Kopf die volle Zuschreibung. Kein
Handlungsbedarf. (Nebenbei: `src/a8rawconv/` ist als vendortes
Verzeichnis vom SPDX-Tor ausgenommen — diese beiden liegen in
`src/core/`, also außerhalb, und sind trotzdem in Ordnung.)

**Die drei AIR-Dateien sind der Kern.** Sie erklären sich selbst als
„**Full port** of AIR `<Datei>.cs` to C" und nennen „Original: Copyright
(C) 2013-2015 SPS & Jean Louis-Guerin (**GPL-3.0**)". Das ist derselbe
Fall wie nibtools, nur **besser belegt** — hier steht die Lizenz im
eigenen Kopf, es braucht kein Ähnlichkeitsaudit. Eine Sprachübersetzung
(`.cs` → C) bleibt eine Ableitung.

### Erreichbarkeit — und ein Schein-Test

`uft_ipf_air.c` ist **erreichbar**: `uft_ipf_plugin.c` ruft
`ipf_air_alloc()` (`:70`), `ipf_air_free()` (`:78`),
`ipf_air_get_geometry()` (`:85`) und `ipf_air_get_track_meta()` (`:135`).
Keine statischen Gleichnamen (nachgeprüft, 0). Quarantäne würde hier
**IPF-Lesen kosten** — anders als bei allen bisherigen Fällen.

`uft_kfstream_air.c` und `uft_stx_air.c` haben **null** Aufrufer. Beide
stehen in `docs/orphan_baseline.txt`.

Dabei fiel ein eigener Befund an: **`tests/test_air_cross_validate.c`**
(745 Zeilen, läuft in ctest) heißt „Cross-validation test harness for AIR
enhanced parsers" und ruft **keine einzige AIR-Funktion**. Gemessen über
alle 20 exportierten Namen der drei Module: 0 Aufrufe; der einzige
Treffer war sein eigenes `main`. Der CMake-Eintrag bindet keine der drei
Quellen ein. Der Test erzeugt synthetische STX/IPF/KF-Dateien und prüft
sie gegen seine **eigene** Inline-Logik. Er ist damit kein Beleg für die
AIR-Parser — dieselbe Klasse wie MF-596, nur diesmal nicht „kann nicht
scheitern", sondern „prüft etwas anderes als der Name sagt".

(Die `main()` in allen drei Modulen sind sauber gekapselt —
`STX_AIR_TEST`, `KF_AIR_TEST`, `IPF_AIR_TEST`. Dort ist kein Fehler.)

### Was ich NICHT getan habe

**Nichts gelöscht.** Dein Verfahren aus SCOUT-23 („übersetzt/portiert →
Quarantäne") träfe auf die drei AIR-Dateien zu, und bei zweien wäre es
kostenlos. Aber: das sind neue Befunde aus meinem eigenen Zensus, nicht
die vier Dateien, über die du entschieden hast — und der dritte Fall
kostet eine Fähigkeit, die dein Verfahren nicht vorsah. Vorlage statt
Alleingang.

**Offen (`LIZ-2`), Eigentümer, drei Entscheidungen:**

1. **`uft_kfstream_air.c` + `uft_stx_air.c`** — GPL-3.0-Port, unerreichbar,
   kein Test hängt daran. Quarantäne kostet nichts. Analog zu
   `uft_track_align.c`.
2. **`uft_ipf_air.c`** — GPL-3.0-Port, **erreichbar**. Quarantäne nimmt
   IPF-Lesen aus dem Werkzeug. Alternativen: Neubau Clean-Room gegen die
   öffentliche CAPS/SPS-Spezifikation (aufwendig), oder Freigabe bei
   Jean Louis-Guerin erbitten. Die dritte Möglichkeit — behalten und
   nichts sagen — ist nach der Release-Zusage keine.
3. **`uft_amiga_protection.c`** — XCopy Pro war kommerzielle
   Amiga-Software; eine Lizenz nennt der Kopf nicht. Nach deiner Regel
   („unklar → wie portiert behandeln") ein Quarantänefall; er ist
   unerreichbar, kostet also nichts. **Achtung:** es gibt **zwei**
   Dateien dieses Basisnamens (`src/formats/amiga/` und
   `src/protection/`) — nur die erste trägt die XCopy-Erklärung.

Unabhängig davon und ohne Lizenzbezug: `test_air_cross_validate.c`
gehört entweder an die AIR-Module angeschlossen oder umbenannt. Ein Test,
dessen Name etwas anderes verspricht als er prüft, ist eine
Erfolgsmeldung ohne Tat.

---

## Scout-Zyklus 9: DiscImageManager — ein erfundener Formatkopf, und das erste Acorn-Oracle (MF-642, 2026-08-28)

Untersucht: **https://github.com/geraldholdsworth/DiscImageManager**
(HEAD `5ffe4796fe`, 2026-08-24). Gutachten:
`tools/uft-scout/out/DiscImageManager.gutachten.md`.

**Lizenz GPL-3.0** (Wurzel-`LICENSE`, bestätigt in `DIMConsole.lpr:4-19`).
Zone GELB — kein Port, kein Vendoring; Pascal wäre ohnehin keiner.
Verhaltens-Spec und Oracle sind das Maximum.

### Der Fehler im eigenen Baum — selbst nachgemessen

`src/formats/acorn/uft_adl.c:1-13` behauptet:

> „ADL (Acorn **DFS** Large) … Headerless, 80 tracks × 1 head × 16
> sectors × 256 bytes = 327,680."

mit `#define ADL_SIZE 327680` und `disk->geometry.heads = 1`.

**Das ist zweifach falsch.** Gegen zwei unabhängige Belege im
DIM-Klon geprüft:

| Beleg | Aussage |
|---|---|
| `LazarusSource/DiscImage_ADFS.pas:73-75` | `163840` = ADFS **S**, `327680` = ADFS **M**, `655360` = ADFS **L** |
| beigelegtes `ADFS_L.adl` | **655360 Byte** (`stat` gemessen) |

Also: `.adl` ist **ADFS L**, 655 360 Byte, **zwei** Seiten — nicht „DFS
Large" mit 327 680 und einem Kopf. Und die Zahl, die wir führen, ist die
von ADFS **M**.

Folge für den Benutzer: ein echtes `.adl` fällt durch unsere
größenexakte Sonde und wird nicht gelesen; eine 327 680-Byte-Datei
bekäme den falschen Namen. **Fabrikations-Klasse** — dieselbe Bauart wie
FMT-2/3/10/11/12: eine plausible, in sich stimmige, erfundene Spec.

### Das erste skriptbare Acorn-Oracle

DIM hat ein eigenes Konsolen-Ziel (`DIMConsole.lpi/.lpr`,
`{$DEFINE DIMCONSOLE}`), ist skriptbar (`-c <script>` + `runscript`) und
hardwarefrei. Entscheidend: `GetFileCrc`/`GetFileMD5` hashen den per
`ExtractFile` gewonnenen **Inhalt byteweise**
(`DiscImage_Published.pas:98-115`), und `savecsv` schreibt sie je Zeile.

**Das ist exakt das `flophashes`-Muster** — und floptool hat **kein
einziges Acorn-Dateisystem** (MF-623: 62 Dateisystem-Einträge, keines
davon). DIM ist damit die fehlende zweite Hand auf **Datei-Ebene** für
die schwächste Ecke des Baums, und eine andere Hand als MAME und UFT.

### Die fünf Vorschläge, je mit Kennzahl

| # | Vorschlag | Kennzahl |
|---|---|---|
| **SCOUT-25** | `uft_adl.c`-Identitätsfehler beheben. Rotbeweis: ein 655 360-Byte-L-Abbild gegen `uft_disk_open()` — heute NULL | ungeprüfte Formate runter (`adl` T3→T2) |
| **SCOUT-26** | DIMConsole als Acorn-Datei-Hash-Oracle registrieren **und den Acorn-Korpus selbst erzeugen** (`new` + `add` + `savecsv`). Heute: **0** Acorn-Abbilder im Korpus | Korpus 0→n; T3→T1b-Weg für `ssd` und `adl`, **ohne Bench** |
| **SCOUT-27** | ADFS-Verhaltens-Spec: Old-Map-Prüfsummen `$0FF/$1FF`, Root-Checkbytes, New-Map-Disc-Record, Zonen-CrossCheck-XOR=`$FF`, acht Broken-Directory-Codes. Stufe 4 liest gegen RISC-OS-Primärdoku gegen | ungeprüfte Formate runter (Grundlage) |
| **SCOUT-28** | größenexakte Sonden tolerant gegen gekürzte Abbilder; die 204 800-Byte-Mehrdeutigkeit über den Katalog entscheiden statt still 80×1 zu wählen | `ssd` T3→T2; stille Fehlklassifikation raus |
| **SCOUT-29** | Watford-DFS in die SSD-Hebungs-Spec — **kein neues Plugin** (Moratorium) | SSD-Spec vollständig |

### Was der Zyklus **bestätigt** hat

Die DSD-Verschränkungsformel in DIM ist **deckungsgleich** mit unserer.
Das ist kein Fund, sondern ein Beleg — und genau so verzeichnet.

### Attribution im Fremd-Repo (Pflichtfeld)

`DiscImage_ADFS.pas:4422` — „Adapted from the RISC OS RamFS ARM code
procedure `InitDiscRec` in RamFS50". Quelle: RISC OS Open,
**Apache-2.0**. Wer die Disc-Record-Erzeugung je als Spec nutzt, muss
auf die **RISC-OS-Primärquelle** ausweichen, nicht auf DIMs Nachbau —
Apache-2.0 ist für diesen Baum nicht portierbar, und ein Nachbau eines
Nachbaus verdoppelt die Unsicherheit.

### Beschaffung

Lazarus/FPC (`lazbuild`, nur fürs Oracle) · DIMConsole-Binary mit SHA-256
· **selbst erzeugter** Acorn-Korpus (DFS S/D 40/80, WDFS, ADFS
S/M/L/D/E/F mit Inhalt, `savecsv`-Hashes, Manifest) · RISC-OS-Primärdoku
für SCOUT-27.

**Die beigelegten GPL-3-Blanks werden nicht kopiert** — und sie sind
ohnehin gekürzt (651 264 / 323 584 Byte gemessen, statt 655 360 /
327 680). Der Scout hat daraus SCOUT-28 abgeleitet: unsere Sonden lehnen
alle vier ab.

### Offen

Ob `lazbuild` DIMConsole unter MinGW ohne Widgetset durchbaut — das
erweist erst der Bau. Vorher kein Registry-Eintrag: **kein Oracle auf
Zusicherung.**

---

## Zyklus 10 (atrcopy) und 11 (a8rawconv): Atari hat einen Leser, ein Oracle-Paar — und keinen FM-Decoder (MF-643, 2026-08-28)

Zwei Zyklen auf derselben Plattform, unabhängig gelaufen, mit
zusammenpassendem Ergebnis.

### Der Entlastungsbefund zuerst (a8rawconv, Frage A)

Unsere zwei Ports behaupten „Port of a8rawconv 0.95 … **(GPL-2-or-later)**".
**Die Angabe stimmt.** Beide Quelldateien tragen den GPL-Kopf wörtlich —
`interleave.cpp:1-16`, `compensation.cpp:1-16`, „either version 2 …, or
(at your option) any later version". 17 von 44 Quelldateien tragen ihn,
`COPYING` ist der GPLv2-Volltext. Der Scanner meldete „GPL-2.0 + LGPL
(mehrdeutig)" — **Fehlalarm**: die „Lesser"-Treffer stehen im
unveränderten FSF-Standardtext.

Nach drei Lizenz-Überraschungen an einem Tag ist das die vierte Prüfung
— und die erste, die nichts findet. Das gehört genauso deutlich gesagt.

**Und die vendorte Kopie ist aktuell:** 44/44 Quelldateien
**byteidentisch** mit HEAD `5db54b47` (`cmp` je Datei). Seit MF-467 kein
Quellcode-Commit im Repo.

### Der schwere Befund: UFT kann FM-Fluss nicht dekodieren

`src/flux/uft_flux_decoder.c`, `flux_decode_fm()` — selbst nachgelesen:

```c
    /* FM sector decoding would go here - similar to MFM */
    /* For now, just note we found a sync */
    pos = sync_pos + 16;
}
free(bits);
return (track->sector_count > 0) ? FLUX_OK : FLUX_ERR_NO_SYNC;
```

`sector_count` wird in der ganzen Funktion **kein einziges Mal erhöht**
(gemessen: 0 Treffer). Die Rückgabe kann deshalb **nur**
`FLUX_ERR_NO_SYNC` sein.

Das ist schlimmer als ein Stub: die Funktion setzt `detected_encoding`,
rechnet `avg_bitrate`, findet Syncs — sie **sieht implementiert aus** und
kann nie gelingen. Ein Lazy-Stub mit Fassade.

Folge: **Flux→ATR für Atari-SD ist heute unmöglich.** Und die
Rundlauf-Matrix führt **kein einziges Atari-Paar** — der einzige
ATR-Bezug in `src/core/uft_roundtrip.c` ist ein Kommentar, der sagt, ein
echter Wandler fehle noch.

> **Korrektur meiner eigenen Messung:** mein erster Zähllauf meldete 19
> Atari-Treffer in der Matrix. Falsch — `grep -i atr` trifft auch
> „m**atr**ix". Genau die Falle, vor der AGENT.md Regel 2 warnt, diesmal
> in meiner Zählung.

Die Referenz liegt im Baum: `src/a8rawconv/sectorparser.cpp` (615 Zeilen,
WD177x-genau), Zone **GRÜN**.

### Der neunte „Tür fehlt"-Fall — und er ist doppelt

Beide Zyklen fanden ihn unabhängig: `src/formats/atari/` trägt **zwei
parallele** Atari-DOS-Implementierungen, zusammen rund **3 400 Zeilen**
(`atari_dos2.c` 1006, `atari_check.c` 680, `atari_atr.c` 459,
`atari_sparta.c` 410, `atari_util.c` 314, plus `uft_atari_dos.c` 510).
Inhaltlich decken sie DOS 2.0/2.5, MyDOS mit VTOC2 und SpartaDOS ab —
also alles, was atrcopy kann.

**0 Aufrufer außerhalb des Verzeichnisses. 0 Tests. Kein `ANKER:`.** Der
ExplorerTab kennt nicht einmal `*.atr` als Filter.

Nach der Verwaisten-Regel ist das ein Löschfall — **oder** die Tür für
Phase 1. Zusätzlich zur Türfrage steht hier eine **Dubletten-Frage**:
welche der zwei Fassungen trägt?

### Das Oracle-Problem für ATR ist gelöst

atrcopy hat unseren ATR-Korpus **erzeugt** (Manifest: „atrcopy 10.1").
Als alleiniges Oracle wäre der T1b-Eintrag **zirkulär** — dieselbe Falle
wie AdfOpus/ADFlib. floptool fällt aus (kein Atari-Dateisystem).

**Zwei unabhängige zweite Hände gefunden:**

| Werkzeug | Lizenz | liefert |
|---|---|---|
| `lsatr` aus dmsc/mkatr | GPL-2.0 | eigener C-Leser für DOS 1/2.0/2.5/MyDOS/SpartaDOS/BW-DOS; Extraktion → SHA-1 extern |
| a8rawconv selbst | GPL-2.0-or-later | heute gebaut; `ATR→XFD` **byteidentisch** zum Korpus-XFD, Kreis `ATR→SCP(FM)→ATR` byteidentisch |

Erzeuger ≠ Prüfer ist damit für ATR gesichert.

**atrcopy als Oracle hat eine Auflage:** unter NumPy 2.5.1 stürzt 10.1
bei **jedem** Abbild-Open ab (drei gemessene Bruchstellen: `np.alen`
entfernt seit NumPy 1.23, zwei NEP-50-uint8-Überläufe). Der
Registry-Eintrag muss `numpy<1.23` pinnen, sonst „prüft" ein Absturz.
Auf dieser Maschine (nur Python 3.13) nicht herstellbar.

### Vorschläge

`SCOUT-30` FM-Fluss real machen (Kennzahl: **Wandlungspfade rauf** —
Voraussetzung für SCP→ATR) · `SCOUT-31` Testvektor-Generator ·
`SCOUT-32` Atari-DOS-Tür öffnen **oder löschen**, samt Dubletten-Entscheid ·
`SCOUT-33` a8rawconv als Zweitoracle registrieren · `SCOUT-34`
ATR↔XFD-Wandlungspfad (der einzige Vorschlag, der eine der **vier**
Release-Zahlen direkt bewegt) · `SCOUT-35` atrcopy mit Pinnungs-Auflage ·
`SCOUT-36` lsatr als zweite Hand · `SCOUT-37` 25 MPL-2.0-Fixtures,
darunter die reale ED-Referenz, die P3-6 als fehlend führt.

### Nebenbefund

Der Fork-Commit `5db54b47` hat eine unwirksame CC0-LICENSE entfernt —
der Plan-Abschnitt „Lizenz-Hinweis zum Fork" in
`A8RAWCONV_INTEGRATION_TODO.md` ist dadurch überholt.

Aus demselben Plan sind **TA1, TA2, TA3, TA4-Code erledigt** (TA3 sogar
über den Plan hinaus: `uft_atx.c` mit 726 statt geplanter ~400 Zeilen).
**Offen:** TA5 (FM/MFM-Parser-Review) und Nachtrag 2 (Generator).

---

## Vier liegengebliebene Gutachten abgearbeitet — zwei Befunde, zwei Entwürfe (MF-646, 2026-08-28)

Die Ratenbremse zählte vier Gutachten ohne Übernahme-Marke. Nachgesehen:
**zwei** davon sind vollständig und tragen Befunde, **zwei** sind
mechanische Entwürfe, deren Tiefenprüfung nie lief.

### `adfopus_hxc` — drei Befunde, einer davon ein `LIZ-1`-Fall

Zone **GRÜN** für ADFOpus/ADFlib/fdi2raw. Drei Ergebnisse:

1. **Der AmigaDOS-Encoder ist dort nicht.** Die `MF-539`-Lücke (`ADF→HFE`
   lehnt ab, weil uns ein Encoder fehlt) schließt dieses Repo nicht —
   AdfOpus ruft libhxcfe nur als DLL. **Folgeziel benannt:**
   `jfdelnero/HxCFloppyEmulator` (libhxcfe-Quelltext: Amiga-MFM-Encoder
   und HFE-Writer aus einem Haus) sowie `keirf/disk-utilities` gezielt
   auf die **Encoder**-Seite — die Negativliste kennt bisher nur den
   Decoder.
2. **Als drittes ADF-Oracle untauglich** — ADFlib, wieder dieselbe Hand.
   Bestätigt die Zirkularitätsregel ein drittes Mal.
3. **`uft_dms.c` ist ein 1940-Zeilen-Port ohne Abgleich** gegen die
   autoritative Quelle, `dms` steht auf **T3**. Und: **kein SPDX-Tag**;
   die Public-Domain-Behauptung stützt sich allein auf den
   xDMS-Datei-Header. Das ist ein **`LIZ-1`-Fall** — Klasse B, fremde
   Codebasis, Lizenz nur aus einem Datei-Header statt aus einer
   Lizenzdatei. Debians `xdms`-copyright wäre der Zweitbeleg.

   Kennzahl: **ungeprüfte Formate runter** (`dms` T3→T1b über einen
   xDMS-Differenzlauf) — moratoriumskonform, weil Hebung, nicht neues
   Format.

### `hxcfe_amiga_copy_utility` — ein Fixture, das einen toten Zweig belebt

Zone **PRÜFEN**. Der Fund ist ein HxC-**natives** HFE-v1-Abbild, und
sein Wert liegt in einem Feldunterschied, den ich nachgemessen habe:

| Feld | unser `gw_amigados.hfe` | das HxC-native |
|---|---|---|
| `track_encoding` | **0xFF (unknown)** | **0x01 (AMIGA_MFM)** |
| `interface_mode` | **0xFF (unknown)** | 0x04 |
| Spuren | 80 | 84 |

Selbst nachgeprüft am Korpus-Abbild (Bytes 0–17): `track_encoding` und
`interface_mode` stehen beide auf `0xFF`. **Folge:** der Zweig
`case HFE_ENC_AMIGA_MFM:` in `src/formats/hfe/uft_hfe.c:137` wird von
unserem einzigen HFE-Fixture **nie durchlaufen**. Wir haben eine
Abbildung, die niemand prüft.

Kennzahl: **ungeprüfte Formate runter** (`hfe`) — ein Fixture, das einen
bestehenden Codepfad erstmals ausführt, ist billiger als jede neue
Fähigkeit. Lizenz des Fixtures ist Zone PRÜFEN und vor Übernahme zu
klären (Regel aus MF-630: ein nicht verteilbares Fixture vergiftet den
Korpus).

### `sector-cpc` und `superdiskindex` — Entwürfe, keine Gutachten

Beide sind die **mechanische Ausgabe** von `gutachten.py`: Messwerte,
Lizenzzone, Inventar-Abgleich — und eine unausgefüllte
UNGEKLÄRT-Liste. Die Tiefenprüfung, die Kategorie, Einhängepunkt und
Vorschlag festlegt, **lief nie**.

* `sector-cpc` (39 Zeilen): **Zone ROT** — keine Lizenzdatei. Damit ist
  ohnehin nur Verhaltens-Spec und Oracle möglich.
* `superdiskindex` (61 Zeilen): **Zone GRÜN**.

Sie bekommen **keine** Übernahme-Marke — es gibt nichts zu übernehmen.
Sie sind als **offene Zyklen** gekennzeichnet und warten auf ihre
Stufe 3, nicht auf eine Entscheidung.

> **Werkzeug-Befund:** die Ratenbremse zählt Entwürfe wie fertige
> Gutachten. Das ist nicht falsch — ein Entwurf ist unerledigte Arbeit —
> aber der Zähler unterscheidet nicht zwischen „wartet auf den
> Eigentümer" und „wartet auf die eigene Stufe 3". Ein Feld im Kopf
> (`<!-- stufe: 2 -->`) würde das trennen, ohne die Bremse zu lockern.

---

## Zyklen 12–15: vier Aufklärer, ein Lesefehler, und eine sechsfache Zahl (MF-648, 2026-08-28)

Vier Scout-Zyklen parallel gelaufen: **lib1541img** (BSD-2), **dfsimage**
(MIT), **Apple-II-Disk-Tools + DskToWoz2** (beide GPL-3.0), **Datamuseum
FloppyTools** (BSD-2). Drei der vier Lizenzurteile stehen in Zone GRÜN —
zum ersten Mal ist für CBM und für 8"-Minicomputer eine portierbare
Referenz da, nicht nur eine lesbare.

Die drei Befunde unten sind **selbst nachgemessen**, nicht vom Aufklärer
übernommen. Bei einem davon fiel die eigene Messung breiter aus als der
gemeldete Fund.

### Befund 1 — der 40-Spur-D64 liest den Disknamen als Belegungskarte

`src/formats/d64/uft_d64_parser_v3.c:1029ff`. Die Schleife läuft bis
`disk->tracks` mit

```c
size_t entry_off = 4 + (track - 1) * 4;
```

Für Spur 36 ergibt das `4 + 35*4 = 144 = 0x90`. Vierzehn Zeilen tiefer
steht in derselben Funktion:

```c
d64_copy_filename(disk->disk_name, bam + 0x90, 16);
```

**Dieselbe Adresse.** Für jedes 40-Spur-Abbild werden die Spuren 36–40
aus dem Disknamen und der Disk-ID gelesen; `free_blocks` ist damit
falsch. Und die Größe wird angenommen: `d64_is_valid_size()` (:524ff)
führt `D64_SIZE_40` und `D64_SIZE_40_ERR` ausdrücklich.

Der Fehler sitzt **genau in dem Leser**, den die Umsetzungsliste unter
A2 als Phase-1-Erstcommit ausleiten will. Wäre er ausgeleitet worden,
hätte der Differenzlauf gegen `flophashes` eine falsche Belegung
verglichen — grün oder rot, in beiden Fällen wertlos.

**Referenz für den Fix, benannt und in Zone GRÜN:** lib1541img
(BSD-2-Clause, excess-c64) sondiert die erweiterten Belegungskarten
statt zu extrapolieren — DolphinDOS `bam+0x1c+4*track`, SpeedDOS
`bam+0x30+4*track`, PrologicDOS inline mit Namensversatz
(`cbmdosvfsreader.c:88-115, 395-435`). Der Aufklärer hat die
Eigenständigkeit belegt: 0 Treffer für „vice|nibtools|based on|port of"
über den ganzen Fremdbaum, also weder Korpus-Erzeuger noch
nibtools-Verwandtschaft — die Zirkularitätsfrage aus `ORACLES.md` ist
beantwortet, bevor sie gestellt wurde.

**Zusatzwiderspruch, mitgemessen:** `src/formats/c64/uft_d64_plugin.c:68`
nimmt Abbilder ab 205312 Byte als **42 Spuren** an; der v3-Parser lehnt
dieselbe Datei als ungültige Größe ab. Zwei Wege, zwei Antworten.

→ **SCOUT-42**, Kennzahl: ungeprüfte Formate runter (`d64`-Hebung ist
Moratoriums-Bedingung). Als **A5** in die Umsetzungsliste, **vor** A2.

### Befund 2 — dieselbe 8"-Geometrie steht sechsmal unter sechs Namen

Gemeldet war ein Widerspruch bei DG Nova. Die eigene Messung
(`grep -rn 256256 src/`) fand die Klasse dahinter: **77 × 26 × 1 × 128 =
256 256** liegt sechsmal im Baum, jedes Mal als eigenes „System":

| Datei | Name in der Tabelle |
|---|---|
| `src/detect/mfm/mfm_detect.c:1517` | „IBM 8" SSSD 250K" |
| `src/formats/dsk_generic/uft_dsk_generic.c:63` | „Xerox 820" |
| `src/formats/industrial/uft_cromemco.c:28` | „Cromemco 8" SSSD 250KB" |
| `src/formats/japanese_ext/uft_hitachi_s1.c:25` | „S1 8" SSSD 250KB" |
| `src/formats/minicomputer/uft_dg_nova.c:25` | „DG Nova 8" SS/SD 250KB" |
| `src/formats/motorola/uft_versados.c:34` | „VersaDOS SS 128b 250KB" |

Das ist **nicht automatisch falsch** — viele 8"-Systeme benutzten das
IBM-3740-Layout, und `mfm_detect.c` nennt es auch so. Falsch ist, dass
**keine** der fünf Formatzeilen eine Referenz trägt und alle fünf
Formate denselben Bytebestand als ihr eigenes System beanspruchen. Die
Erkennung nach Dateigröße ist damit für alle sechs mehrdeutig, und
welches Plugin gewinnt, entscheidet die Registrierungsreihenfolge.

Datamuseum FloppyTools (BSD-2) dekodiert echte Nova-Disketten mit
**77 × 1 × 8 × 512 FM** (`dg_nova.py:15-16`) — eine andere Geometrie.
Das entscheidet die Frage noch nicht: eine unbelegte Zahl durch eine
zweite unbelegte Zahl zu ersetzen wäre keine Verbesserung. Was
entschieden ist: die sechs Zeilen sind eine **Fabrikations-Klasse**
(FMT-2/3/10/11/12) und gehören auf Referenz oder auf eine ehrliche
Sammelzeile „IBM-3740-kompatibel, System nicht unterscheidbar".

→ **SCOUT-47**, Kennzahl: ungeprüfte Formate runter (fünf T3-Einträge
auf einmal). Aufwand M, weil die Entscheidung eine Quelle je System
verlangt.

### Befund 3 — A2R behauptet Schreiben und kann nicht einmal proben

`src/formats/uft_format_registry.c:232-244` führt A2R mit
`supports_read = true` **und** `supports_write = true`. Verdrahtet ist
ein einziges Feld: `.probe = uft_a2r_probe`. Die Probe vergleicht das
Magic `"A2R2"` (`:24`) — **A2R-v3-Dateien proben nie**.

Der eigentliche Leser, `src/parsers/a2r/uft_a2r_parser.c` (~1100 Zeilen,
beherrscht v3 mit RWCP/SLVD), ist in der `.pro` und hat außerhalb seines
Verzeichnisses **null** Nennungen. Selbst gegengeprüft: die einzigen
Treffer für `uft_a2r_` außerhalb `src/parsers/a2r/` sind die
Registry-Zeile und die Probe selbst.

Das ist dieselbe Klasse wie GUI-1 (MF-635) — eine Zusicherung, hinter
der nichts steht — **und** der zehnte „Können im Baum, Zugang fehlt"-Fall.
`docs/plans/FLUXENGINE.md` §FE-5 stellt genau diese Frage; sie ist damit
beantwortet, die Entscheidung (verdrahten mit Rotbeweis **oder**
ehrliche Registry-Zeile) steht aus.

→ **SCOUT-46**, Kennzahl: Ehrlichkeit der Formatliste. Schließt FE-5.

### Was die vier Zyklen sonst geliefert haben

| # | Fund | Kennzahl | Reife |
|---|---|---|---|
| SCOUT-38 | `dfsimage` (MIT, Hashes je Datei, `list`/`digest`/`index -f json`) als **zweites** Acorn-Oracle neben DIMConsole — zwei unabhängige Hände, stärker als der D64-Standard | T3 runter | B (`pip install` aus dem Klon) |
| SCOUT-39 | **Drei** türlose Acorn-Module: `src/formats/bbc/uft_bbc_dfs.c` (5 Exporte, nur eigene Prototypen), `src/formats/bbc/ssd_dsd.c` (Header inkludiert niemand), `src/formats/ssd/uft_ssd_parser_v2.c` (eigenes `main()`, unregistriert) — dreifache Katalog-Implementierung, kein `# ANKER:` | T3 runter | B |
| SCOUT-40 | 18-Bit-Adress-Vorzeichenerweiterung (`dfsimage entry.py:257-262`) fehlt in `uft_bbc_dfs.c:59-61`; falsche Kommentarzahl „0xA0 = 1280" in `uft_ssd_plugin.c:53` | T3 runter | B |
| SCOUT-41 | Lineare vs. verschränkte DSD-Ablage: UFT liest ein lineares 409 600-B-Abbild still mit **vertauschten Seiten** | T3 runter | B |
| SCOUT-43 | Zweiter türloser CBM-Verzeichnisleser: `src/formats/cbm/uft_cbm_formats.c` (959 Z., cbmconvert-Ableitung **mit** genannter GPLv2+-Lizenz, ohne SPDX, D64/D71/D81/T64, null Aufrufer). Zwei Leser, eine Tür — einer bekommt sie, der andere Anker oder Löschung. Nebenbei: der Kopf **behauptet** LyNX und implementiert es nicht (`src/formats/misc/lnx.c` ist **Atari** Lynx, selbst nachgesehen) | Tier-Leiter D71/D81 | C (Eigentümer) |
| SCOUT-44 | DSK→WOZ2: der Pfad ist seit jeher registriert (`uft_format_convert_tables.c:214`), hat aber **keinen Arbeiter** und keinen Matrix-Eintrag. Vollständige Verhaltens-Spec aus `dsktowoz2.c` liegt im Gutachten (TMAP, TRKS 14 Blöcke, Spurlänge 0xC5C0/0xBB30, Prologe, 4-and-4, 6-and-2, Interleave) | Wandlungspfade rauf | B |
| SCOUT-45 | Apple-Oracle-Paar: `to_woz2` (Konsole, deterministisch, deckt `.do`/`.d13`) + `floptool prodos_140k` (liegt registriert, deckt `.po`). **Korpus kostenlos**: `to_woz2` prüft den Inhalt nicht, seed-erzeugte DSKs genügen — heute hat `inv["korpus"]` **null** Apple-Einträge | T3 runter (4 Formate) | B |
| SCOUT-48 | FloppyTools (BSD-2) als quelloffene **zweite Hand** für `src/flux/uft_kryoflux_stream.c` — heute ist die einzige KryoFlux-Referenz das proprietäre `dtc`. Lauf belegt (`python -m floppytools -d q1`, rc=0); Windows braucht `PYTHONUTF8=1` | T3 runter | B |

### Lizenz-Nebenbefund mit Folgen für Stufe 4

Beide Apple-Repos sind **GPL-3.0** — kein Code in unseren Baum. Aber
ihre Nibblize-Routinen erklären selbst: „Based on code by Andy McFadden,
from CiderPress", und CiderPress ist **BSD-3-Clause** (am Original
geprüft). Wer die Apple-GCR-Kodierung je portieren will, nimmt die
grüne Quelle mit Attribution — nicht die gelbe Ableitung. Das gehört
notiert, bevor jemand den bequemeren Weg nimmt.

### Fundus — bewusst nicht eingeplant

M²FM-Decode-Pfad (BSD-2, UFT hat nur das Enum) · acht fehlende
8"-Formate aus dem Archiv (ISIS-II, WANG WCS, Q1, Lexitron, Philips
P5002, HP9885, Ohio Scientific, Alpha LSI) — Moratorium · ZipCode
4/5-pack · MMB-Container · `_UNREAD_`-Sentinel und DDHF-Manifest als
Verfahren · Q1-Multi-Lesungs-Korpus (44 MB, Inhalt Zone PRÜFEN).

---

## Zyklen 16–19: LIZ-1 schrumpft von 48 auf 14, und die Tür bleibt offen (MF-650, 2026-08-28)

Vier Aufklärer, ausgewählt nach einem einzigen Kriterium: löst er einen
**im Baum benannten Blocker**? HxCFloppyEmulator (LIZ-1), OpenCBM
(`KNOWN_ISSUES` §M.4), libdsk (Oracle + die Sechsfach-Geometrie aus
MF-648), mkatr/atrip (`lsatr` für B1).

Alle vier haben geliefert, und drei davon haben eine Aussage **dieses
Baums** korrigiert — zweimal meine eigene.

### Die Tür zu GPLv3 bleibt offen, und zwar doppelt gesichert

`docs/plans/FLUXENGINE.md` hielt seit MF-644 fest: heute liegt kein
GPL-2.0-only-Code im Baum — **mit dem Vorbehalt**, dass die 48
ungeklärten Attributionen aus `LIZ-1` das umwerfen könnten, und
HxCFloppyEmulator stand dreimal darin.

Selbst nachgemessen am Klon:

```
either version 2 : 478 Dateien
either version 3 :   0 Dateien
```

Die `COPYING`-Dateien legen zwar den GPL-3.0-Volltext bei, aber **kein
einziger** Dateikopf erteilt einen Version-3-Grant; die operative
Erklärung ist durchgängig „either version 2 … or (at your option) any
later version". Formal ist das Matrix-Zeile PRÜFEN (COPYING ≠ Kopf),
operativ **GRÜN**.

Und die drei Attributionen bei uns sind gar keine Ableitungen. Das Audit
lief idiombasiert in **beide** Richtungen — neun HxC-Idiome bei uns
gesucht, acht unserer Idiome dort — je **0 Treffer**. Der schärfste
Einzelbeleg ist eine Bedeutungsdifferenz, die ein Port mitgeschleppt
hätte: unser `calculateFormatValue()` (`src/rawformatdialog.cpp:368`)
baut `(tracks<<16)|(sides<<8)|sectors`, während HxCs `formatvalue` das
**Füllbyte** ist (`cb_rawfile_loader_window.cxx:104`). Wer abschreibt,
erfindet nicht dieselbe Zahl neu mit anderer Bedeutung.

**Verdikt: freigesprochen, keine Quarantäne.** Die Köpfe werden nach
MF-636 berichtigt, nicht gelöscht. Selbst wenn es Ableitungen wären,
hätte die Tür offen gestanden — GPLv2+ verengt nichts.

### 31 der 48 Verdachtsfälle haben denselben Ursprung

Der libdsk-Zyklus hat den größten Brocken von `LIZ-1` aufgelöst, und
zwar in unserem eigenen Baum: **32 Dateien** tragen eine
„Reference: libdsk drv*.c"-Attribution, **keine davon mit Lizenz**.

Gemessen am Klon (Fassung 1.5.12): **146** Dateien mit
„any later version", Lizenztext `doc/COPYING` = „GNU **Library** General
Public License Version 2" → **LGPL-2.0-or-later**, Zone GRÜN.

> **Eigene Fehlmessung, festgehalten weil sie eine Methode betrifft.**
> Mein erster Grep suchte `either version 2` einzeilig und fand **eine**
> Datei — ich hielt den Bericht deshalb für widerlegt. Der Grant steht
> im libdsk-Kopf über zwei Zeilen umbrochen („either\n * version 2 of
> the License"). Der Bericht hatte recht, meine Messung war die falsche.
> Dieselbe Klasse wie `grep -i atr` auf „m**atr**ix" (MF-643): ein
> Suchmuster, das die Frage nur scheinbar stellt.

Die Rechnung für die fünfte Kennzahl, Verdachtsstufe:

| | |
|---|---|
| Stand MF-636 | **48** |
| − libdsk (Lizenz jetzt bestimmt) | 31 |
| − HxCFloppyEmulator (drei, freigesprochen) | 3 |
| **verbleibend** | **14** |

Zwei der 32 libdsk-Verweise nennen Treiber (`drvmgt.c`, `drvopus.c`),
die es in **diesem** Fork nicht gibt — nur upstream (1.5.22). Für die
beiden trägt die Messung nicht; sie bleiben offen und werden auch so
gekennzeichnet.

### Ein Attributionsfehler, der eine falsche Rechtsaussage war

`src/hal/uft_xum1541.c:6` schrieb: „based on opencbm xum1541.c
(https://github.com/OpenCBM/OpenCBM, **BSD-2**)".

Am Original nachgelesen — `opencbm/lib/plugin/xum1541/xum1541.c:10-13`:

> „This program is free software; … under the terms of the GNU General
> Public License … either version 2 of the License, or (at your option)
> any later version."

**GPL-2.0-or-later, nicht BSD-2.** Im ganzen OpenCBM-Baum tragen fünf
Dateien eine BSD-Notiz; diese ist keine davon. Berichtigt.

Das ist keine Kleinigkeit: eine Attribution ist eine rechtliche Aussage
(MF-636). Eine zu großzügige Lizenz zu behaupten ist genau der Fehler,
den `LIZ-1` sucht — nur hier mit Namen und Zeile.

### M.4 war seit einem Monat erledigt, und zwei Dokumente wussten es nicht

Ich habe den Aufklärer mit der Prämisse losgeschickt, zwei HIGH-Befunde
blockierten die M3.2-Verdrahtung. Er hat die Prämisse korrigiert:
`docs/KNOWN_ISSUES.md:427` führt §M.4 seit **MF-301** als „RESOLVED IN
CODE". Alle acht Protokoll-Behauptungen wurden trotzdem am frischen
Klon nachgeprüft — sie stimmen (`XUM_STATUSBUF_SIZE=3`,
`xum1541_types.h:64`; Header `[opcode, proto|flags, size_lo, size_hi]`,
`xum1541.c:998-1001`; IEC-Adressierung als WRITE+ATN, `archlib.c:337ff`).

Selbst nachgezählt: **16** `UFT_HAS_LIBUSB`-Stellen in
`src/hal/uft_xum1541.c`. Das Wiring **ist** da. Offen sind **sieben**
Funktionen mit unbedingtem `UFT_ERR_NOT_IMPLEMENTED` —
`identify_drive`, `get_status`, `read_track`, `read_track_gcr`,
`read_disk` und zwei weitere —, und die brauchen kein USB, sondern die
**CBM-DOS-Kommandoebene**: `U1:`, `M-R`, Kanal 15.

`docs/MASTER_PLAN.md` sagte „13/26, Multi-Session-libusb-Wiring offen",
`CLAUDE.md` an zwei Stellen „USB I/O pending libusb wiring". Beide in
**beide** Richtungen falsch: zu pessimistisch beim Draht, zu optimistisch
bei der Vollständigkeit. Berichtigt.

### Der Entlastungsbefund zur Sechsfach-Geometrie

MF-648 fand 77 × 26 × 1 × 128 sechsmal unter sechs Systemnamen und ließ
offen, ob das falsch ist. libdsk antwortet, dreifach gemessen:

* kennt **keinen** der sechs Systemnamen (0 Treffer im ganzen Baum),
* führt **keine** 26 × 128-Geometrie (die einzige 128er-Zeile ist ein
  MYZ80-Festplattencontainer, `dsksgeom.c:78`),
* urteilt **nie nach Dateigröße**: `dskid` auf 256 256 Byte Zufall
  antwortet wörtlich **„Bad format."** Identifikation läuft über Inhalt
  (`dskgeom.c:236ff`: Sektor-IDs → Boot-BPB → PCW-Spec-Byte →
  DFS-Katalog → CP/M-86) oder über einen ausdrücklich genannten
  Formatnamen.

Damit ist die ehrliche Sammelzeile aus SCOUT-47 nicht bloß zulässig,
sondern **Stand der Referenzpraxis**. Und sie sagt zugleich, wie es
richtig ginge: Geometrie aus Inhalt, nie aus Größe.

### `lsatr` ist registriert — und der Korpus dahinter ist leer

Selbst gebaut, selbst gelaufen, selbst gehasht: `mingw32-make CC=gcc`,
gcc 13.1.0, rc=0; `lsatr.exe` SHA-256
`dc7f90046d833b7b122865f8d9347bb90526d05bda02e145ae663fea043ed1d5`;
`-v` → „mkatr version 1.4".

```
atrcopy_dos2sd.atr: 720 sectors of 128 bytes, DOS 2.0s, 707 sectors free of 707 total.
atrcopy_dos2sd.xfd: 720 sectors of 128 bytes, DOS 2.0s, 707 sectors free of 707 total.
```

Zeichengleich — die dritte unabhängige Bestätigung, dass XFD das ATR
ohne den 16-Byte-Kopf ist (**A3**). Registriert als siebtes Oracle;
`oracle_registry` grün.

**Der Nebenbefund wiegt schwerer als der Fund:** 707 von 707 Sektoren
frei — unser einziges ATR-Abbild ist **leer**. `B1` wollte damit über
3 400 Zeilen Atari-DOS entscheiden. Auf einem leeren Verzeichnis
entscheidet dieser Differenzlauf **nichts**. B1 braucht zuerst ein
nicht-leeres Abbild; das steht jetzt in der Umsetzungsliste.

`atrip` ist **kein** Oracle: `README.rst:6` nennt es wörtlich „The
successor to atrcopy" — dieselbe Hand wie der Korpus-Erzeuger, fünfte
Registrierungsfrage. Unabhängig davon auf dieser Maschine nicht
lauffähig (`pkg_resources` fehlt unter Python 3.13, `np.fromstring`
neunmal). Als Nicht-Oracle eingetragen.

### Was sonst anliegt

| # | Fund | Kennzahl |
|---|---|---|
| SCOUT-49 | `hxcfe` v2.16.15.2 gebaut und gemessen: ADF→HFE→ADF byteidentisch, liest unser SCP (1760/1760 Sektoren), FDI, ATR; **nicht** G64. Schließt das Drei-Hände-Dreieck xdftool/gw/hxcfe | T3 runter |
| SCOUT-50 | **B4 ist gelöst und kostet nichts:** Fixture mit `track_encoding=0x01`/`interface_mode=0x04` selbst erzeugt — der tote Zweig `uft_hfe.c:137` läuft erstmals, die Lizenzfrage am Fremd-Fixture entfällt | T3 runter (`hfe`) |
| SCOUT-51 | AmigaDOS-MFM-Encoder `tg_addAmigaSectorToTrack()` (`amiga_mfm_track.c:356-485`, GPLv2+) schließt die MF-539-Lücke — ADF→HFE lehnt heute ab, weil uns der Encoder fehlt | Wandlungspfade rauf |
| SCOUT-55 | `libcbmimage` gebaut und gelaufen, auch gegen **unser** `vice_c1541_35trk.d64`; trennt SpeedDOS/Dolphin/ProLogic-BAM ausdrücklich — die zweite Hand für genau die MF-649-Zone, und sie löst die VICE-Zirkularität aller acht CBM-Korpuszeilen | T3 mittelbar |
| SCOUT-59 | libdsk-Trio als Oracle: 13 Treiber mit gemessenem byteidentischem Schreib-Rundlauf, davon sieben auf T3 (edsk, jv3, apridisk, myz80, nanowasp, logical, qrst) | T3 runter |
| SCOUT-60 | TD0/CQM/IMD-Korpus aus den seltenen libdsk-**Schreibern** — Formate, für die sonst kaum ein Erzeuger existiert | Tier-Leiter T2→T1b |
| SCOUT-65 | Nicht-leeres DOS-2-Trio (SD/ED/DD) für B1; Urteil bleibt beim unabhängigen `lsatr`, deshalb unbedenklich | T3 runter (`atr`) |
| SCOUT-66 | `lsatr` kennt **beide** Ablagen der ersten drei Sektoren im DD-ATR (`src/atr.c:76-90`), unser `atr_sector_offset()` (`uft_atr.c:19-25`) nur die 128-Byte-Form → Verdacht auf 384 Byte Versatz. **Nur quelltextbelegt, nicht ausgeführt** | T3 runter (`atr`) |

**Fundus:** 181 HxC-Loader als Spec-Quelle · 86 XML-DiskLayouts
(sui-generis-Vorsicht) · M3.2-DOS-Schicht-Spec (bewegt keine Zahl) ·
MyDOS/SpartaDOS/LiteDOS-Lesewissen (Moratorium) · mkatr als
SpartaFS-Generator (erst nach B1).

**Nebenfund zur Inventar-Härtung:** `src/formats/d1m/` ist ein leeres,
ungetracktes Verzeichnis und erzeugt trotzdem `vorhanden: true` —
dieselbe Index-Falle wie seinerzeit die Fluss-Visualisierung (MF-630).
Ein Verzeichnisname ist keine Fähigkeit.

---

## Berichtigung zu MF-650: die 14 waren falsch, und warum (MF-651, 2026-08-28)

**Die Zahl „48 → 14" aus MF-650 ist zurückgenommen.** Sie war eine
Rechnung auf einer Handklassifikation, die ich nicht nachgefahren habe —
genau die Bauart, gegen die dieser Baum seine Kennzahlen abgeleitet
statt gepflegt führt. Beim Nachmessen fiel sie auf beiden Beinen um.

### Das erste Bein: das Maß konnte die Frage nicht stellen

Mein erster Messversuch suchte einen Lizenzbezeichner **irgendwo** im
60-Zeilen-Kopf. Damit zählte er unsere **eigene**
`SPDX-License-Identifier: GPL-2.0-or-later` mit, die seit MF-621 in fast
jeder Datei steht. Ergebnis: 29 frisch geheilte Attributionen bewegten
die Zahl um **null**.

Ein Maß, das sich durch die Sache nicht bewegen lässt, misst sie nicht.
Das Kriterium heißt jetzt: die Lizenz muss **neben** der Attribution
stehen (±6 Zeilen), und die eigene SPDX-Zeile zählt ausdrücklich nicht.

### Das zweite Bein: der Zensus war blind für die größte Gruppe

`scripts/audit_spdx_policy.py` erkannte „based on", „port of", „derived
from", „taken from" — aber **nicht** `Reference:`. Und genau so sind die
32 libdsk-Zuschreibungen formuliert.

Die Folge, gemessen: der Prüfer, der Attributionen finden soll, sah die
größte zusammenhängende Gruppe im Baum nicht. Die 48 waren nie die
Gesamtlage, sondern das, was durch ein Muster passte.

**Das ist die fünfte Auflage desselben Fehlers.** Die Tabelle in
`CLAUDE.md` führt vier: MF-567 (Abbruch-Codes), MF-578 (Offscreen-Tests),
MF-598 (`SKIP_RETURN_CODE`-Namen), MF-633 (`SKIP_DIRS`). Nun MF-651:
aufgezählt wurden **Formulierungen**, durchgefallen ist eine neue.

Der Unterschied zu den vier: dort ließ sich die Aufzählung durch eine
Abfrage ersetzen (`git ls-files`). Hier geht das nicht — für „nennt
dieser Satz eine fremde Codebasis?" gibt es keine autoritative Quelle.
Was bleibt, ist die Regel, sie zu erweitern, **sobald** eine neue
Formulierung auffällt, und den Fund als Fund zu notieren statt als
Betriebsunfall.

### Die Zahlen, jetzt abgeleitet statt gepflegt

`scripts/audit_attribution_licence.py` (neu) beantwortet die Frage
reproduzierbar:

| | |
|---|---|
| Attributionen gesamt | **170** (mit `Reference:`; vorher sichtbar: 90) |
| davon ohne Lizenz daneben, vor heute | **156** |
| **heute** | **124** |

Die 32 Heilungen dieses Commits sind damit **belegt sichtbar** —
156 → 124, und die Differenz ist genau die Zahl der bearbeiteten Zeilen.

Was inhaltlich richtig bleibt aus MF-650: die libdsk-Lizenz **ist**
bestimmt (LGPL-2.0-or-later, 146 Dateien mit „any later version"), die
HxC-Attributionen **sind** freigesprochen, und die GPLv3-Tür **ist**
offen. Falsch war nur die Behauptung, wie viel davon `LIZ-1` schließt.

### Was dabei sonst gefunden wurde

**Ein Dateiname, den es nicht gibt.** Elf Attributionen nannten Treiber;
drei davon liegen nicht in der geprüften Fassung 1.5.12. Bei einem ist
der Grund ein Tippfehler mit Folgen: `drvapdsk.c` heißt in libdsk
**`drvadisk.c`**. Eine Attribution, deren Datei niemand findet, ist
nicht nachprüfbar und damit keine. Berichtigt; `drvmgt.c` und
`drvopus.c` sind ausdrücklich als **unverifiziert** gekennzeichnet
statt stillschweigend mit der Projektlizenz versehen.

**Eine zweite Quelle, die niemand gemessen hat.** Sechs Zeilen nennen
„libdsk diskdefs, **cpmtools**". cpmtools wurde in keinem Zyklus
geklont; seine Lizenz steht in `ORACLES.md` als GPL-3.0 für `cpmls`,
aber das ist ein anderes Artefakt. Die Zeilen tragen jetzt libdsks
Lizenz **und** den ausdrücklichen Vermerk, dass cpmtools offen ist.
Lieber eine halbe Antwort, die sagt, dass sie halb ist.

---

## Die Oberfläche hat 27 Fenster ohne Tür und 21 tote Menüpunkte (MF-663, 2026-08-29)

Der Eigentümer hat entschieden: das Analyse-Fenster **verdrahten**, und
messen, ob mehr davon herumliegt. Beides erledigt — das Ergebnis der
Messung ist erheblich größer als der Anlass.

### Die Tür war halb schon da

`actionAnalyze` steht seit jeher im Menü und war mit **nichts**
verbunden. Gleichzeitig lag `DiskAnalyzerWindow` gebaut
(`UnifiedFloppyTool.pro:291/367`), aber unerreichbar da.

**Zwei Hälften derselben Lücke, die einander gesucht haben** — ein
Menüpunkt ohne Ziel und ein Fenster ohne Aufrufer. Verbunden mit
`MainWindow::onAnalyze()`; ohne geladenes Abbild sagt es das, statt ein
leeres Fenster zu zeigen, das Geometrie aus dem Nichts schätzt.

### Messung 1 — Fenster ohne Tür: **27 von 46**

Methode: Erreichbarkeits-Wanderung ab `main.cpp` und `mainwindow.cpp`.
Kante = „Klasse A erzeugt Klasse B"; Kommentare werden vorher entfernt.

> **Die erste Fassung der Messung war falsch, und das gehört dazu.**
> Sie fragte „konstruiert jemand die Klasse außerhalb ihrer eigenen
> Datei?" — und zählte damit Unterfenster als türlos, die ihr Besitzer
> in derselben Datei erzeugt (die fünf Assistentenseiten von
> `UftRecoveryDialog`). Ein Unterfenster, das sein Besitzer erzeugt, ist
> verdrahtet; unerreichbar ist es nur, wenn der **Besitzer**
> unerreichbar ist. Die Erreichbarkeits-Wanderung kommt auf dieselben
> 27 — aber jetzt mit einer Methode, die trägt.

Darunter, nach Gewicht:

| | |
|---|---|
| **vier ganze Einstellungs-Tabs** | `XCopyTab`, `NibbleTab`, `ForensicTab`, `ProtectionTab` |
| ein Wiederherstellungs-Assistent | `UftRecoveryDialog` + 5 Seiten |
| Sektor-Editor-Teile | `UftHexEdit`, `UftFindReplaceDialog` |
| Dialoge | `UftCompareDialog`, `UftSmartExportDialog`, `PresetManagerDialog` |
| Ansichten | `DiskVisualizationWindow`, `VisualDiskWindow`, `TrackGridWidget`, drei OTDR-Ansichten |

**Der schärfste Einzelbefund:** `src/mainwindow.cpp:146` kommentiert

> `// Tab 4: Settings - All settings as Sub-Tabs (Flux, Format, XCopy, Nibble, Forensic, Protection)`

und erzeugt darunter **einen** davon (`FormatTab`). Fünf der sechs
genannten Unter-Tabs existieren im laufenden Programm nicht. Der
Kommentar beschreibt eine Absicht und liest sich wie ein Zustand.

### Messung 2 — tote Menüpunkte: **21 von 30**

`forms/mainwindow.ui` führt 30 `<action>`-Einträge; 21 werden in keinem
`connect()` genannt. Darunter nicht Randsachen, sondern:

    actionReadDisk · actionWriteDisk · actionVerifyDisk
    actionConnect  · actionDisconnect · actionMotorOn/Off
    actionConvert  · actionCompare    · actionRepair
    actionBAMViewer · actionBootblockViewer · actionLabelEditor
    actionProtectionAnalyzer · actionChecksumDatabase
    drei Sprachumschaltungen + actionLoadLanguage

Jeder davon ist anklickbar und tut nichts. Das ist dieselbe Klasse wie
die 38 toten Bedienelemente aus Stufe 5 des Plans — nur eine Ebene
höher, im Menü.

### Und ein Tor, das falsch gezählt hat

`scripts/audit_orphan_modules.py` ist für C geschrieben und kannte
`namespace { … }` nicht. Es zählte jede Funktion in einem **anonymen
Namensraum** als exportiert — obwohl die interne Bindung hat, genau wie
`static`.

Gemessen: **fünf von sechs** Dateien mit anonymem Namensraum standen
als „ohne jeden Aufrufer" da, weil ihre einzigen „Exporte" dateilokale
Helfer waren.

**Das betrifft meine eigene Meldung von gestern.** In MF-662 habe ich
geschrieben, das Waisen-Tor habe `diskanalyzerwindow.cpp` gemeldet und
ich hätte daraufhin die fehlende Tür gefunden. Die fehlende Tür war
echt und ist unabhängig per `grep` belegt — aber **das Tor meinte etwas
anderes**: es sah `metadatum`, den Helfer, den ich im selben Commit in
einen anonymen Namensraum gelegt hatte. Der Befund stimmt, die
Begründung war ein Zufallstreffer.

Das ist die Aufzählungsfalle in der Grammatik: der Prüfer kannte eine
Schreibweise nicht und hat sie deshalb nicht übersehen, sondern
**falsch gezählt** — was schlimmer ist. Behoben mit
`strip_anon_namespace()`, Klammerzählung statt Regex, weil ein anonymer
Namensraum verschachtelte Blöcke enthält.

### Was daraus folgt

Die drei Zahlen gehören zusammen und beschreiben **eine** Lage:

    38 Bedienelemente ohne Wirkung   (MF-660, Stufe 5 offen)
    27 Fenster ohne Tür              (hier)
    21 Menüpunkte ohne Verbindung    (hier)

Das ist kein Ausreißer, sondern der Regelfall in dieser Oberfläche: sie
wurde gebaut, aber nicht verbunden. Der Plan
`docs/plans/VARIANTEN_UND_FAEHIGKEITEN.md` behandelt bisher nur die
Bedienelemente; die anderen beiden brauchen dieselbe Behandlung —
**verdrahten oder entfernen, kein Drittes.**

**Nicht getan und bewusst nicht:** die übrigen 26 Fenster und 20
Menüpunkte anzufassen. Das ist keine Nacharbeit, sondern eine
Entscheidung je Fall — und 27 Fenster in einem Durchgang zu verdrahten
wäre genau die Sorte Massenänderung, die dieser Baum nicht verträgt.

---

## Ein Header-Name, drei Dateien — und der Bau war rot, ohne dass es auffiel (MF-666)

Beim Anschließen des Varianten-Wählers ist ein Bau fehlgeschlagen, der
**vorher schon rot war** und den niemand bemerkt hatte.

### Wie er sich versteckt hat

Zwei Dinge zusammen:

1. **Ich habe den Rückgabewert selbst verdeckt.** `cmake --build . | grep -c error` liefert die Trefferzahl von `grep`, nicht den Rückgabewert des Baus. Genau diese Falle steht in meinen eigenen Projektnotizen, und ich bin ein zweites Mal hineingelaufen. Erst `cmake --build . > log; echo $?` hat `rc=2` gezeigt.
2. **`ctest` blieb grün.** Die betroffenen vier Ziele hatten ihre ausführbaren Dateien aus einem früheren Bau; der Prüfstand lief gegen veraltete Binärdateien und meldete 278/278. „Tests grün" und „Bau grün" sind zwei Aussagen.

### Die Ursache

`src/formats/bbc/uft_bbc_dfs.c` inkludiert **unqualifiziert**:

```c
#include "uft_bbc_dfs.h"
```

Diesen Namen gibt es **dreimal**:

| Datei | `uft_dfs_file_entry_t` |
|---|---|
| `include/uft/uft_bbc_dfs.h` | 3× definiert |
| `include/uft/formats/uft_bbc_dfs.h` | 3× definiert |
| `include/uft/fs/uft_bbc_dfs.h` | **0×** |

Welche Datei ein Ziel bekommt, entscheidet allein die **Reihenfolge der
Suchpfade**. Vier Qt-Ziele hatten `include/uft/fs` auf dem Pfad und
`include/uft/formats` nicht — sie griffen die Fassung ohne den Typ und
bauten nicht.

Der Beweis dafür steckt in der Behebung: mit `include/uft/fs` **zuerst**
blieb der Bau rot; erst als der formats-Pfad davor stand, wurde er grün.
Die Richtigkeit hing an einer Zeilenreihenfolge.

### Was daran der eigentliche Befund ist

Die Notabhilfe (Pfad ergänzt, Reihenfolge korrigiert) macht den Bau
grün. Sie beseitigt die Falle nicht: **drei Dateien gleichen Namens,
aufgelöst über Suchpfad-Reihenfolge**, bleiben ein Zufallsgenerator.
Jedes neue Ziel kann wieder die falsche erwischen, und diesmal
vielleicht ohne Übersetzungsfehler — mit einer Struktur, die nur *fast*
passt.

Das ist die SSOT-Verletzung in ihrer unangenehmsten Form: nicht zwei
Wahrheiten in zwei Dateien, sondern zwei Wahrheiten unter **einem
Namen**.

Dieselbe Datei ist bereits als SCOUT-39 gemeldet — eines von drei
türlosen Acorn-Modulen mit dreifacher Katalog-Implementierung. Jetzt
kommt heraus: auch der Header liegt dreifach. Wer SCOUT-39 anfasst,
entscheidet beides zusammen.

**Offen:** welcher der drei Header ist der kanonische, und die anderen
beiden weg — oder die Includes qualifizieren
(`#include "uft/formats/uft_bbc_dfs.h"`). Beides ist eine
SSOT-Entscheidung, kein Beifang eines Varianten-Wählers.

### Und eine Lehre für die Tore

`check_consistency.py` und `verify_build_sources.py` waren die ganze
Zeit grün. Kein Tor prüft, **ob der Prüfstand überhaupt baut** — die
Annahme war, dass `ctest` das mitbringt. Tut es nicht, wenn alte
Binärdateien herumliegen. Ein Tor „`cmake --build` mit Rückgabewert 0"
wäre billig und hätte das hier sofort gezeigt.

### ✅ Nachgezogen (MF-667)

**In der CI stand der Grund schwarz auf weiß**, und der Kommentar
daneben widersprach ihm:

```yaml
cmake --build build-tests --parallel $(nproc) -- -k 2>&1 || true
```

Der ctest-Schritt darunter erklärte gleichzeitig, `--no-tests=ignore`
sei so gewählt, „so a build breakage surfaces **in the build step**".
Genau das konnte der Bau-Schritt nicht — er trug `|| true` und war nicht
rot zu bekommen. Dieselbe Maskierung, die CI-1 am 2026-07-04 für `ctest`
entfernt hat; für den **Bau** blieb sie stehen.

Behoben in **beiden** Aufträgen (Linux und Windows), und zwar so, dass
der gute Grund für `-k` erhalten bleibt:

* Der Bau läuft weiter mit keep-going und bleibt selbst grün — damit
  `ctest` danach noch läuft und man die Ergebnisse der bauenden Ziele
  sieht.
* Sein Rückgabewert wird **festgehalten** statt verworfen.
* Ein eigener Schritt **nach** `ctest` bewertet ihn, nennt die
  gescheiterten Ziele beim Namen und färbt den Auftrag rot.

Dazu `scripts/pruefstand.py` für lokal: baut und testet in einem Zug,
verliert den Rückgabewert nicht, und sagt ausdrücklich etwas, wenn die
Tests grün und der Bau rot sind — der Zustand, der heute eine Stunde
gekostet hat.

**Rotbeweis für das Tor selbst:** ein Ziel absichtlich gebrochen
(undefinierter Aufruf in `uft_bbc_dfs.c`). Ergebnis: `Bau: ROT
(Rückgabewert 2)`, zwanzig Ziele namentlich, `GESAMT: ROT`,
Rückgabewert 1. Zurückgesetzt → alles grün.

Beim Rotbeweis fiel gleich eine Ungenauigkeit auf: die erste Fassung
meldete „0 Übersetzungsfehler" neben „ROT", weil es **Binder**fehler
waren. Beide werden jetzt getrennt gezählt, und wenn keines der Muster
greift, sagt das Skript das — die Muster sind eine Lesehilfe, kein
Urteil.

---

## GUI-6 — 38 Bedienelemente in einer geschlossenen Schleife (MF-668)

**Kennzahl:** keine der vier. Das ist ein Ehrlichkeits-Befund, kein
Rückstand — nach Regel 9 (MF-640) also **Fundus mit Entscheidungsbedarf**,
nicht eingeplante Arbeit. Die Entscheidung gehört dem Eigentümer, weil
sie sichtbare Oberfläche entfernt.

### Gemessen

Die drei „Advanced…"-Dialoge (`src/advanceddialogs.{h,cpp}`, 530 Zeilen)
bieten zusammen **38 Bedienelemente** an: Flux 15, PLL 9, Nibble 14.

Ihr vollständiger Weg:

```
Dialog  ->  FormatTab::m_fluxAdvParams  ->  Dialog
```

`git grep` nach den drei Parameter-Typen findet **außerhalb der Dialoge
selbst genau einen** Konsumenten: `src/formattab.h`, wo sie als
Mitglieder liegen. In `formattab.cpp` werden sie an exakt sechs Stellen
berührt — dreimal `setParams`, dreimal `getParams`, je Dialog ein Paar.
**Niemand liest sie sonst.** Kein Wandler, kein Dekoder, keine HAL.

Die Schleife ist geschlossen: der Benutzer stellt etwas ein, es wird
gemerkt, es wird ihm beim nächsten Öffnen wieder gezeigt, und es wirkt
sich auf nichts aus.

### Warum „verdrahten" für die meisten nicht in Frage kommt

Zwei Messungen, die die naheliegende Reparatur ausschließen:

**Erstens:** die Oberfläche baut **nirgends** ein
`flux_decoder_options_t`. Nur vier Kerndateien tun das, intern. Es gibt
keinen Parameterweg von einem Dialog zum Dekoder — für kein einziges
der 38.

**Zweitens:** von 34 systematisch geprüften Elementen nennen **19** einen
Namen, den es im ganzen Baum als Feld nicht gibt — `preserveGaps`,
`preserveSync`, `ignoreBadGCR`, `rawNibble`, `adaptiveGain`,
`lockThreshold`, `weakBitWindow` und weitere. Sie sind nicht
unverdrahtet, sie sind **erfunden**. Für sie gibt es unten keine
Schraube, an der eine Leitung enden könnte.

**Drittens:** von den Feldern, die es gibt, sind mehrere im Dekoder tot.
Gezählt in `src/flux/uft_flux_decoder.c` — Lesestellen, nicht
Setzstellen:

| Feld | Lesestellen | |
|---|---:|---|
| `use_pll` | 7 | lebt |
| `media` | 6 | lebt |
| `pll_gain` | 6 | lebt |
| `media_adjust_pct` | 3 | lebt |
| `bitcell_ns` | 2 | lebt |
| `sync_patterns` / `sync_count` | 2 | lebt |
| **`tolerance`** | **0** | dreimal geschrieben, nie gelesen |
| **`revolution`** | **0** | tot |
| **`decode_all_revs`** | **0** | tot |

Der PLL-Dialog hat einen Toleranz-Regler. Eine Leitung dorthin war im
Zuge dieser Arbeit **gebaut und wieder ausgebaut**: ein Regler mit
Verkabelung und ohne Wirkung ist kein Fortschritt gegenüber einem Regler
ohne Verkabelung — nur schwerer zu erkennen.

### Die drei Wege, und was gegen sie spricht

1. **Löschen.** 530 Zeilen Dialog plus drei Menü-Einträge. Ehrlich und
   billig; nimmt aber eine Oberfläche weg, die jemand vielleicht als
   Zusage gelesen hat.
2. **Ausblenden/sperren**, mit sichtbarem Grund. Behält die Elemente als
   Landkarte dessen, was einmal kommen soll — aber nur, wenn es dafür
   einen benannten, `git grep`-findbaren Plananker gibt (Anker-Regel).
   Ohne Anker ist das die Verwaisten-Regel, und die sagt löschen.
3. **Verdrahten.** Für die sechs lebenden Felder machbar, je Feld
   ungefähr wie MF-480: eine öffentliche Option, eine Zeile im
   Verteiler, ein Verbraucher, ein Rotbeweis. Für die 19 erfundenen
   Namen und die drei toten Felder **nicht** machbar, ohne vorher den
   Dekoder zu erweitern — und das ist Format-/Decoder-Layer, also
   Einfrier-Regel: nur gegen eine benannte Referenz.

Empfehlung: 2 für die sechs lebenden Felder mit Anker auf diesen
Eintrag, 1 für den Rest. Die Entscheidung ist nicht getroffen.

### Was in MF-668 tatsächlich behoben wurde

Nicht die 38 — eine **halbe Naht**, die die Messung nebenbei aufdeckte.

`decode_cell_adjust_pct` (MF-480) war der einzige Wert mit einem
vollständigen Weg von außen bis zum Dekoder. Er stand in
`uftc_convert_scp_to_adf_via_plugin` und **fehlte** in
`uftc_convert_hfe_to_adf_via_plugin` — dieselbe Aufzählungs-Falle wie
sechsmal zuvor in diesem Baum: ein Verhalten an einer Stelle gepflegt,
an der zweiten still vergessen. Wer den Feineinsteller auf einem
HFE-Abbild benutzte, bekam sein Ergebnis, als hätte er nichts
eingestellt.

Die naheliegende Reparatur wäre falsch gewesen. `media_adjust_pct` wird
nur dort gelesen, wo **Zeiten** in Zellen umgerechnet werden;
`flux_decode_amiga_bits()` — der Weg für ein HFE — liest aus den
Optionen nur die Sync-Muster. Ein HFE ist ein fertig getakteter
Bitstrom, die Zellgrenzen sind beim Aufzeichnen gefallen. Der Regler ist
dort nicht unverdrahtet, sondern **bedeutungslos**.

Behoben also so: eine Funktion `uftc_apply_decode_options()` mit zwei
Aufrufern, die je nach Quelle den Wert anwendet **oder sagt, dass er
hier nicht gilt**. Drei Zustände sind möglich, und nur der dritte war
verboten — wirken, sich erklären, oder schweigen.

Rotbeweis: `tests/test_decode_options_reach.c`, absichtlich über den
HFE-Pfad (über SCP wäre er von Anfang an grün gewesen und hätte die
Lücke zugedeckt). Gegenprobe durch Sabotage des HFE-Aufrufs: 2
Abweichungen, danach wieder grün.

---

## GUI-7 — der achte Fall, in einem Teil der als funktionierend gilt (MF-669)

**Kennzahl:** keine. Ehrlichkeits-Befund, Entscheidung beim Eigentuemer.

Der PLL-Toleranz-Regler im OTDR-Panel (`m_pllTolerance`,
`src/gui/uft_otdr_panel.cpp:210-219`) hat denselben geschlossenen Weg wie
die 38 aus GUI-6, nur ueber einen Umweg:

```
Regler -> QSettings("pllTolerance") -> Regler
```

Gemessen: `m_pllTolerance->value()` wird an **einer** Stelle benutzt
(`:697`, Speichern), und der Schluessel wird an **einer** Stelle gelesen
(`:675`, Laden). Sonst nirgends.

Das wiegt schwerer als GUI-6, weil das OTDR-Panel zu den *arbeitenden*
Teilen gehoert — es ist der Aufrufer, der die drei DeepRead-Booster
ueberhaupt erreichbar macht.

### Was daran haengt: die beworbenen ±33 %

`CLAUDE.md` beschreibt Adaptive Decode als „aggressiver PLL-Re-Decode
(±33 %)". Gemessen in `uft_otdr_adaptive_decode.c`:

```c
agg_opts.tolerance = UFT_ADAPTIVE_PLL_TOLERANCE;  /* 0.33 — 0 Lesestellen */
agg_opts.pll_gain  = UFT_ADAPTIVE_PLL_GAIN;       /* 6 Lesestellen */
```

Die Aggressivitaet kam **allein aus der Verstaerkung**. Die ±33 %
Zeitgeber-Toleranz sind nie angewandt worden. Die wirkungslose Zuweisung
ist mit MF-669 entfernt — eine wirkungslose Zuweisung neben einer
wirksamen ist schwerer zu erkennen als gar keine.

### Drei Wege

1. **Regler entfernen.** Ehrlich; der Rest des Panels bleibt.
2. **Mechanismus bauen**, Oracle-first: eine Lesestelle fuer eine
   Zeitgeber-Toleranz im Dekoder, gegen eine benannte Referenz, mit
   Rotbeweis vor dem Code. Erst danach das Feld, erst danach der Regler
   (Regel aus `docs/SETTINGS_ROADMAP.md`).
3. **Regler auf einen lebenden Traeger umhaengen** — `pll_gain` waere
   der Kandidat, aber er bedeutet etwas anderes. Eine Umbenennung, die
   die Bedeutung verschiebt, ist die naechste stille Falschaussage.

Nicht entschieden. `CLAUDE.md` nennt die ±33 % bis dahin weiterhin
falsch — das ist mit diesem Eintrag benannt, nicht behoben.

---

## OPT-1 — sechs Wandlungsoptionen, die nirgends ankommen (MF-672)

**Kennzahl:** keine der vier direkt. Der Eintrag steht trotzdem, weil er
eine **Zusage** betrifft: `preserve_weak_bits` und `verify_after` sind
Namen, die in einem Forensik-Werkzeug etwas versprechen.

### Gemessen

`uft_convert_default_options()` setzt zehn Werte. Sechs davon werden in
den erweiterten Typ kopiert und dort von **niemandem** gelesen:

| Feld | verspricht |
|---|---|
| `verify_after` | Prüfung nach der Wandlung |
| `preserve_errors` | Fehler bleiben erhalten |
| `preserve_weak_bits` | Schwachbit-Information bleibt erhalten |
| `interpolate_errors` | Fehlstellen werden interpoliert |
| `synthetic_cell_time_us` | Zellendauer erzeugter Fluss-Abbilder |
| `synthetic_jitter_percent` | Jitter erzeugter Fluss-Abbilder |

Jedes hat drei Schreibstellen (Vorgabe, Weitergabe, Ziel der Weitergabe)
und **null** echte Lesestellen. Von Hand nachgeprüft für
`preserve_weak_bits` und `verify_after`: alle weiteren Treffer im Baum
gehören zu **anderen** Strukturen (`uft_merge_engine.h`, die
`*_parser_v3.c`-Parameter, `uft_snapshot.h::verify_after_write`).

### Warum das Tor sie nicht sofort fand

`scripts/audit_setting_wiring.py` hat sie beim ersten Lauf **freigegeben**,
und der Grund ist eine eigene Lehre. Die Zeile

```c
ext_opts.verify_after = options->verify_after;
```

enthält ein Schreiben *und* ein Lesen. Das Tor sah das Lesen und gab
Entwarnung — obwohl die Kette danach im Nichts endet. Eine reine
**Weitergabe ist kein Verbrauch**; sie verschiebt die Frage nur eine
Struktur weiter.

Seit MF-672 unterscheidet das Tor beides und fasst je Feldnamen über alle
Einstellungs-Strukturen zusammen — weit genug, um eine Weitergabe nicht
fälschlich als Ende zu lesen, eng genug, um die Namensverwechslungen von
MF-669 zu vermeiden. Rotbeweis geführt.

### Stand

Die sechs stehen in `ERLAUBT_UNGELESEN` **mit Verweis auf diesen
Eintrag** — nicht weil sie in Ordnung sind, sondern weil ein dauerhaft
rotes Tor übergangen wird und dann auch neue Fälle nicht mehr fängt. Die
Liste wächst nicht: ein siebter Eintrag bedeutet, dass jemand einen
Mechanismus zu bauen oder ein Feld zu löschen hat.

**Ende dieses Eintrags:** wenn jedes der sechs entweder eine Lesestelle
hat (Oracle-first, benannte Referenz, Rotbeweis vor dem Code) oder
gelöscht ist. Löschen ist eine öffentliche API-Änderung — deshalb
Eigentümer-Entscheidung, nicht Wartungsarbeit.

---

## Behoben in MF-672: die Oberfläche nullte ihre Optionen

Kein offener Punkt, sondern der Grund, warum OPT-1 überhaupt auffiel.

Alle drei Stellen, an denen die Oberfläche Wandlungsoptionen baute
(`toolstab.cpp`, `decodejob.cpp`, `uft_save_image.cpp`), taten es mit
`memset(&opts, 0, sizeof(opts))`. Das sieht nach „keine besonderen
Wünsche" aus und ist etwas anderes:

`use_multiple_revs` steht per Vorgabe auf **true**, und
`uft_format_convert_flux.c:964` liest `(!opts || opts->use_multiple_revs)`.
Ein Aufrufer mit `NULL` bekam also die Verschmelzung über alle
Umdrehungen — ein Aufrufer mit genullter Struktur nicht.

**Die Oberfläche war damit schlechter als gar keine Angabe.** Sie hat
SCP-Abbilder mit fünf Umdrehungen dekodiert, als läge eine vor, und
niemand konnte es sehen. Alle drei rufen jetzt
`uft_convert_default_options()`; `accept_data_loss` bleibt ausdrücklich
ungesetzt (UFT-A05).

Damit sich das nicht wiederholt, prüft das Tor seither: wer eine
`uft_convert_options_t` anlegt, muss die Vorgabefunktion **aufrufen**.
Beim Rotbeweis schwieg diese Regel zunächst — der Kommentar, der die
Reparatur begründet, nennt die Funktion im Fließtext, und das zählte als
Aufruf. Ein Tor, das sich von einer Erklärung besänftigen lässt, prüft
nichts; Kommentare werden jetzt vorher entfernt.

---

## SET-1 — die einstellbare Fläche, Rest (MF-674)

**Kennzahl:** keine der vier direkt. Die Gruppe ist trotzdem geführt,
weil sie eine **Zusage** betrifft: was die Oberfläche anbietet, muss
wirken oder sagen, warum nicht.

**Wo die Einzelheiten stehen:** [`docs/SETTINGS_ROADMAP.md`](SETTINGS_ROADMAP.md).
Dieser Eintrag ist der **Arbeits**-Zeiger, die Roadmap die **Fach**-Liste
— Träger, Lesestelle und Einheit je Regler stehen dort und werden hier
nicht wiederholt. Eine Zahl an zwei Stellen driftet; das ist in diesem
Baum dreimal belegt.

### Erledigt

| | |
|---|---|
| MF-671 | vier OTDR-Schwellen, über einen Trichter |
| MF-672 | Umdrehungs-Gruppe — drei von vier gestrichen, dafür ein echter Fehler behoben |
| MF-673 | Abstimm-Strenge; zwei Attrappen entfernt statt verdrahtet |

### Offen, in der Reihenfolge ihres Aufwands

**a) `synthetic_revolutions` — vollständiger Weg, keine Oberfläche.**
Wie viele Umdrehungen in ein *erzeugtes* Abbild geschrieben werden. Vier
Lesestellen, alle in `scp_writer_create()`; die öffentliche Option
existiert und wird durchgereicht. Es fehlt nur der Regler. Billigster
offener Punkt der Gruppe.

Dazu gehört ein **ungeklärter Nebenbefund**: dieselbe Option hat in
derselben Datei zwei Rückfallwerte — `convert_bitstream.c:65` nimmt 1
(HFE→SCP), `:281` nimmt 3 (G64→SCP), ohne genannten Grund. Welcher
richtig ist, sagt keine Referenz im Baum. **Vor** dem Regler zu klären,
sonst stellt der Benutzer eine Zahl ein, deren Vorgabe niemand
begründen kann.

**b) Bitstrom-Gruppe.** `ignoreBadGCR`, `includeHalfTracks`,
`includeQuarterTracks` — Träger und Lesestellen sind in der Roadmap
belegt. Sie liegen in drei verschiedenen Format-Plugins, also drei
kleine Nähte statt einer.

**c) Fluss-Gruppe.** `bitcell_ns`, `pll_gain`, `keep_raw_bits`,
`sync_patterns`. Wirkt auf die meisten Formate — und hat als einzige
Gruppe **noch gar keinen Ort**. Die drei „Advanced…"-Dialoge sind
gelöscht bzw. zur Löschung vorgesehen (GUI-6), der Werkzeug-Reiter
kennt nur Wandlungsoptionen. **Erst die Frage „wohin", dann die Naht** —
sonst entsteht die vierte Stelle, an der jemand Optionen zusammenbaut,
und damit die nächste Aufzählungs-Falle.

`sync_patterns` hat dabei einen eigenen Wert: ohne die Amiga-Sync-Liste
dekodiert eine geschützte Diskette zu **null** Sektoren (MF-453). Das
ist kein Feinschliff, sondern der Unterschied zwischen lesbar und nicht
lesbar.

### Wovon dieser Eintrag NICHT handelt

Die 38 Attrappen aus GUI-6 und der PLL-Toleranz-Regler aus GUI-7 sind
**Löschungs**-Entscheidungen, keine Verdrahtungs-Aufgaben. Sie stehen
dort und bleiben dort.

---

## SCOUT-25 — der Scout hatte keinen Nenner (MF-675)

**Kennzahl:** keine direkt; betrifft die Verlässlichkeit der
Scout-Priorisierung, an der nach Regel 9 (MF-640) die MF-Reihenfolge
hängt.

### Der Befund

Auf die Frage „hat der Scout alle Listen abgearbeitet" ließ sich aus
dem Baum **keine Antwort geben**. Die eingereichten Repo-Listen standen
nur im Gesprächsverlauf; nirgendwo im Baum stand, was beauftragt war.

Messbar war nur, was **angekommen** ist — und das sieht vollständig aus:

| | |
|---|---|
| geklont und gemessen | 31 |
| Gutachten geliefert | 26 (30 der 31 Repos abgedeckt, mehrere gebündelt) |
| früher schon bewertet | 57 (`known_negatives.json`) |

„26 Gutachten" ist aber eine Zahl **ohne Nenner**. Sie klingt nach
Vollständigkeit und beantwortet nur „was wurde getan", nicht „wurde
alles getan" — derselbe Unterschied wie zwischen einem grünen Test und
einem Test, der scheitern kann.

### Zwei echte Lücken, die dabei sichtbar wurden

* **`apple2-disk-tools`** ist geklont und gemessen, hat aber **kein**
  Gutachten. Der einzige „Apple II"-Treffer in `out/` steht im
  nibtools-Gutachten und meint etwas anderes.
* **Sieben Gutachten haben keine Spur in dieser Liste**: `ADFDiskBox`,
  `adf_zweitmeinung`, `amigadx`, `atari_st_werkzeuge`, `cbm_erzeuger`,
  `mac_fs_dfxml`, `mkatr_atrip`. Ein Gutachten ohne Entscheidung ist
  Arbeit, die niemand aufgenommen hat.

  Der Vergleich ist ein **grober Namensabgleich** und liefert Fragen,
  keine Urteile: `settings_triage` erscheint dort ebenfalls, obwohl es
  aufgenommen ist — als `SET-1`, unter anderem Namen.

### Behoben, teilweise

`scripts/scout_stand.py` zählt den Stand aus dem, was auf der Platte
liegt — keine gepflegte Aufzählung, dieselbe Regel wie `repo_scope.py`.
Es sagt **ausdrücklich**, dass der Nenner fehlt, statt eine
Vollständigkeit zu suggerieren.

### Teilweise geschlossen (MF-676): die Auftragsliste existiert

`tools/uft-scout/data/auftraege.json` ist angelegt und trägt ihren
ersten Eintrag (`gwnbd`, übergeben 2026-08-29). `scout_stand.py` zeigt
seither eine Zeile „beauftragt / davon noch offen" — **ab jetzt** ist
die Frage also beantwortbar.

Die Datei hält je Auftrag auch die **Erwartung** fest, mit der er
übergeben wurde. Das ist Absicht: nur so lässt sich später sagen, ob
sich eine Vermutung bestätigt hat, statt das Ergebnis rückwirkend zur
Absicht zu erklären.

### Geschlossen (MF-677): der Nenner steht

Die ursprünglichen drei Blöcke sind **wörtlich nachgereicht** — aus den
alten Nachrichten kopiert, nicht aus dem Gedächtnis rekonstruiert. Der
Unterschied ist der ganze Punkt: nur eine Original-Liste belegt, dass
nichts fehlt.

**78 Aufträge. Und die Antwort auf die Ausgangsfrage lautet: nein.**

| | |
|---|---|
| begutachtet | 22 |
| früher bewertet (`known_negatives`) | 19 |
| **noch nicht angefasst** | **37** |

Vier Repos waren mehrfach genannt (`cpmtools`, `flux-analyze`,
`superdiskindex`, `LinuxAtariFloppyFormatter`); die Dubletten sind
zusammengefasst und vermerkt.

### Drei Fehler in der eigenen Zuordnung, gefunden beim Nachprüfen

Der erste Abgleich meldete **51** offen, der zweite **34** — beide
falsch, und beide Male irrte das Werkzeug in die *gefährliche* Richtung
oder verschleierte echte Arbeit:

1. **Namensgleichheit.** `joncampbell123/floppytools` galt als geklont,
   weil `Datamuseum-DK/FloppyTools` unter diesem Verzeichnisnamen liegt.
   Zwei Projekte, ein Name. Behoben: die Klon-Zuordnung fragt jetzt
   `git remote get-url origin`, nicht den Verzeichnisnamen — der
   Rückfall auf Namen ist ersatzlos gestrichen, denn eine falsche
   Entwarnung ist schlechter als eine fehlende Zuordnung.
2. **Erwähnung ≠ Begutachtung.** Ein Treffer des bloßen Repo-Namens
   zählte als erledigt. Jetzt trägt nur der volle `owner/repo`-Bezeichner
   ein Urteil; ein Namenstreffer heißt „nur erwähnt" und ist eine Frage.
3. **Der eigene erste Eintrag.** `gwnbd` wurde als OFFEN gemeldet,
   obwohl sein Gutachten vorlag — er entstand vor der
   Bezeichner-Konvention und hatte keinen `slug`. Die Liste, die den
   Nenner liefern soll, hatte selbst eine Lücke im ersten Datensatz.

Sie lässt sich nicht rückwirkend rekonstruieren, ohne zu raten: die
eingereichten URLs stehen nirgends im Baum, und aus 31 Klonen auf die
ursprüngliche Liste zu schließen hieße, die Antwort aus der Frage
abzuleiten. **Nächster Schritt gehört dem Eigentümer:** die Listen
erneut übergeben, damit sie eingetragen werden können — dann sagt
`scout_stand.py` beim nächsten Lauf, was fehlt.

---

## SCOUT-26 — die vier liegengebliebenen Gutachten, übernommen (MF-678)

Erst der Ausfluss, dann der Zufluss: vier fertige Gutachten lagen ohne
Übergabe in `out/` und haben zusammen mit zwei **Entwürfen** die
Ratenbremse ausgelöst.

### Zuerst ein Werkzeug-Fehler, der die Lage verzerrt hat

Die Bremse zählte sieben. Zwei davon — `sector-cpc` und
`superdiskindex` — sind gar keine Entscheidungen, sondern die
mechanische Ausgabe von `gutachten.py`, deren Tiefenprüfung nie lief.
Sie warten auf ihre eigene Stufe 3, nicht auf den Eigentümer.

Beide trugen die Marke `<!-- stufe: 2 -->` **seit MF-646** — nur hat
`gutachten.py` sie nie gelesen. Die Vereinbarung war da, der Leser
fehlte. Seit MF-678 zählt die Bremse Entwürfe nicht mit: sie begrenzt
Vorschläge an den Menschen, und ein Entwurf enthält keine.

Ergebnis: fünf statt sieben — die vier unten plus `gwnbd`.

### Die vier zerfallen in zwei Sorten

**Sorte A — das sind keine Entscheidungen, das sind Fehler.** Drei
Befunde stehen gegen benannte Referenzen und fallen damit unter die
erlaubte Arbeit der EINFRIER-REGEL (Bugfix gegen benannte Quelle):

| Befund | Beleg | Kennzahl |
|---|---|---|
| **`dim_atari` prüft das Magic nicht.** `grep -c "0x4242"` in `src/formats/dim_atari/uft_dim_atari.c` = **0** — von mir nachgemessen. Jacknife (`dllmain.c:591`) und Hatari (`src/floppies/dim.c`) prüfen es beide. Byte 3 (used-sectors) steht in unserem Kopfkommentar und wird nirgends gelesen. | zwei unabhängige Referenzen | ungeprüfte Formate ↓ |
| **`docs/ORACLES.md:48-49,156` behauptet Falsches.** Die Prämisse, ADFlib sei „dieselbe Bibliothek, die über xdftool auch unser Korpus-Abbild erzeugt hat", ist gemessen falsch — amitools 0.8.1 enthält **0** ADFlib. Damit ist unser ADF-Oracle nicht das, was die Doku sagt. | Messung im Gutachten | Ehrlichkeit |
| **floptool-Padding: `ORACLES.md`/`PLAN_v4.1.7` führen einen gepaddeten Wert als Messung.** Ein Differenzlauf dagegen misst das Padding, nicht das Format. | Messung im Gutachten | ungeprüfte Formate ↓ |

**Empfehlung: alle drei beheben, keine Vorlage nötig.** Ein Tippfehler
in einer Referenz ist keine Eigentümer-Frage.

**Sorte B — echte Vorlagen, weil Lizenz und Verteilung betroffen sind:**

| Vorlage | Worum es geht | Empfehlung |
|---|---|---|
| **adfrescue als ADF-Oracle** | Genau die gesuchte zweite Hand: ADFlib-**unabhängig**, skriptbar, extrahiert Inhalte. Gemessen gegen `tests/corpus_free/xdftool_dd_ofs.adf`: `marker.txt`, 127 Byte, **byteidentisch** zur xdftool-Extraktion. Blocker: **keine Lizenzdatei** → Zone ROT. | **Ja, aber nur als Oracle** — ein Werkzeug ausführen und seine Ausgabe vergleichen verteilt nichts. Kein Code-Port, kein Vendoring. |
| **40-Spur-D64-Fixtures aus mkd64** (SCOUT-82) | Drei Abbilder liegen verifiziert bereit (35-Spur, 40-Spur Dolphin, 40-Spur Speed), Erzeuger mkd64 1.4b, deterministisch, durch libcbmimage gegengelesen. Sie würden die D64-Hebung erstmals gegen **fremd erzeugte** 40-Spur-Abbilder absichern — heute prüft `test_d64_bam_40track.c` nur selbstgebaute Bytes. | **Ja** — genau der Fixture-Typ, der MF-649 rückwirkend härtet. Aufnahme in den Baum heißt Verteilung, darum deine Entscheidung. |
| **PD-lizenziertes MFS-Abbild** | Ein echtes Macintosh-MFS-Abbild in Public Domain; die MFS-Lücke im Baum ist gemessen real. | **Ja, wenn PD belegt ist** — Fixture-Lizenz ist Code-Lizenz (ROT-Zone für Daten). |
| **EUPL-1.2 in die Lizenzmatrix** | Aus dem gwnbd-Zyklus; siehe eigener Eintrag. | siehe dort |

### Was ausdrücklich Fundus bleibt

* **DFXML** (`hfs2dfxml`): sauber recherchiert, bewegt keine Kennzahl.
  UFT hat heute **0** DFXML-Erwähnungen; die Sechs-Formate-Liste in
  `CLAUDE.md` §6 ist laut MF-366 ohnehin Zielbild, nicht Ist-Stand.
* **disk-peek**, **DiskToolC64**, **floppydiskimagetool**: Oracle-Reserve,
  alle Zone ROT, keine Kennzahl-Bewegung.
* **Jacknife/st2disk/atari-st-tools**: kein Port möglich (ROT bzw. GELB,
  ein Bau gescheitert). Der Wert des Zyklus war Befund J-1, nicht der
  Code.

### Ein Fund, der niemandem nützt und trotzdem hierher gehört

`libcbmimage` liest die 40-Spur-BAM **aus dem Disknamen** — exakt der
Fehler, den MF-649 in unserem eigenen D64-Parser behoben hat. Zwei
Projekte, unabhängig, derselbe Off-by-one an derselben Stelle. Das ist
kein Auftrag, aber es sagt etwas über die Fehlerklasse: die
40-Spur-Erweiterung lädt zu genau dieser Verwechslung ein.

---

## LIZ-3 — EUPL-1.2 in der Matrix, und die Regel dahinter (MF-679)

Aus dem gwnbd-Zyklus. Die Lizenz stand **nicht** in der Matrix, und der
Scout hat sie richtigerweise nicht selbst eingeordnet, sondern
vorgelegt. Entschieden vom Eigentümer, jetzt kodifiziert.

### Die Zeile

**EUPL-1.2 → Zone GELB-GRÜN.** Verhaltens-Spec und Oracle ohne
Rückfrage; ein Port **nur** mit Eigentümer-Vorlage je Fall.

Nicht GRÜN, obwohl die Kompatibilitätsklausel (Art. 5) mit ihrem Anhang
GPL-2.0 nennt — aus zwei Gründen:

* Die Wirkung ist **einseitig**: sie erlaubt, eine *Kombination* unter
  GPL-2.0 weiterzugeben, macht EUPL-Code aber nicht allgemein
  GPLv2-kompatibel.
* Die Auslösebedingung ist eine Rechtsfrage am Einzelfall und wirkt
  **stromabwärts** auf jeden, der das Ergebnis weitergibt.

### Die Meta-Regel, die der Fall lehrt

**Eine Kennung, die nicht in der Matrix steht, ist automatisch Zone
PRÜFEN** — auch wenn sie permissiv aussieht. Der Agent ordnet nie selbst
ein.

EUPL-1.2 ist genau das Gegenbeispiel zum Augenmaß: sie sieht aus wie
eine gewöhnliche freie Lizenz, und ihre Verträglichkeit hängt an einer
Klausel mit Anhang. Ein Fehler dieser Art fällt erst beim Verteilen auf
— und dann bei jemand anderem.

Steht in `playbook/lizenzmatrix.md` als eigene Zeile plus Fußnote (1)
und (2), und als Verschärfung von Regel 3 in `AGENT.md`. Der Scout hat
es im gwnbd-Zyklus von sich aus richtig gemacht; diese Einträge machen
daraus eine Regel, damit es nicht Ermessen bleibt.

---

## HAL-3 — zwei gwnbd-Befunde am Greaseweazle-Pfad, beide gestoppt (MF-680)

Schritt 3 des Plans wollte den NAK-Resync sofort bauen. **Beide Hälften
sind blockiert**, und zwar aus zwei unabhängigen Gründen — keiner davon
„es ist schwer".

### Befund 1 — NAK ohne Wiederherstellung

`uft_gw_command()` (`src/hal/uft_greaseweazle_full.c:396-448`) kehrt bei
einem NAK sofort zurück, nachdem es zwei Kopfbytes gelesen hat. Es
**leert den Strom nicht**. gwnbd hat dafür ein `resync()`.

Das ist im Baum nachgemessen und unstrittig. Was daraus folgt, ist es
nicht:

**Grund A — die Behauptung ist einquellig.** Dass reale
Greaseweazle-Firmware nach einem NAK noch Bytes im Strom lässt, stammt
aus **einer** Quelle (Dritt-Firmware-Beobachtung in gwnbd). Nach der
Zwei-Quellen-Regel ist das „unbelegt", nicht „baubar". Ohne Hardware
(MF-310) können wir es nicht selbst messen.

**Grund B — der Rotbeweis ist mit dem heutigen Emulator unmöglich.**
Nicht schwierig — unmöglich. Der GW-Emulator modelliert die
Zustandsmaschine als Funktionsaufrufe (`gw_fw_cmd_*`), es gibt keinen
Bytestrom. `DIVERGENCES.md` §D-2 sagt das selbst, samt der Zeile, auf
die es hier ankommt:

> **Detection: none** — this is an architectural split. The HAL's
> `serial_*` helpers are the only place that touches the wire.

Und diese Helfer sind `static`, in zwei plattformbedingten Fassungen
(POSIX `:204-234`, Windows `:313-348`). **Es gibt keinen
Einspeisepunkt.** Ein Rotbeweis bräuchte also zuerst eine Änderung an
genau der Datei, die er absichern soll — und das ist eine geschützte
Datei.

### Befund 2 — Pin 2 / REDWC

gwnbd misst, dass Pin 2 auf 5,25″ REDWC ist und nicht Density-Select:
28/30 gegen 1/23 gelesene Sektoren. Zwei unabhängige Pinout-Quellen
stützen die Semantik.

Bei uns: `uft_gw_set_pin` hat **null Aufrufer** — Deklaration
(`uft_greaseweazle_full.h:384`) und Definition
(`uft_greaseweazle_full.c:689`), sonst nichts. Von mir nachgemessen.
`HAL_CAP_DENSITY_CTRL` bleibt unbedient. Wir fahren Pin 2 nie.

Nach der Verwaisten-Regel wäre das ein Löschkandidat — aber die
Löschung fasst dieselbe geschützte Datei an, und die Funktion ist ein
plausibler künftiger Anker. **Kein Verdrahten auf Zuruf**: wir fahren
Pin 2 nicht, also gäbe es nichts zu prüfen.

### Was entschieden werden muss

1. **Darf `uft_greaseweazle_full.c` angefasst werden**, um die
   `serial_*`-Helfer einspeisbar zu machen? Das ist die Voraussetzung
   für **jeden** künftigen Protokoll-Rotbeweis, nicht nur für diesen —
   der Emulator kann diese ganze Fehlerklasse sonst nie sehen. Die Datei
   ist als „production-tested C-API" geschützt; eine Ops-Indirektion
   ändert kein Verhalten, aber sie ändert die Datei.
2. **Reicht eine Quelle** für den NAK-Resync? Meine Empfehlung: nein.
   Erst eine zweite unabhängige Bestätigung (Greaseweazle-Firmware-
   Quelltext oder Protokoll-Doku), dann bauen.
3. `uft_gw_set_pin`: löschen, oder mit Anker stehen lassen?

Bis dahin bleibt der Befund dokumentiert und ungebaut. Das ist die
richtige Reihenfolge, nicht die bequeme.

### Der Emulator-Befund ist der eigentliche Ertrag

Dass eine ganze Fehlerklasse — alles, was zwischen Kopfbytes und
Nutzlast schiefgeht — bei unserem einzigen Tier-3-gebenchten Controller
**strukturell unprüfbar** ist, wusste `DIVERGENCES.md` bereits und
niemand hatte es als Auftrag gelesen. gwnbd hat es sichtbar gemacht,
indem es einen konkreten Fall dieser Klasse mitbrachte.

---

## SCOUT-27 — das Zählwerk hat jetzt einen Test (MF-681)

Schritt 4 des Plans. Der Nenner (beauftragt / begutachtet / offen)
steuert die gesamte Restarbeit am Scout-Rückstand — und er lag beim
ersten Lauf **dreimal** falsch, jedes Mal so, dass Arbeit verschwand.

`tests/test_scout_stand.py` stellt alle drei als Fixtures nach:
namensgleiches Fremd-Repo, bloße Erwähnung statt Begutachtung,
Alteintrag ohne Bezeichner. Dazu zwei Normalfälle und eine Prüfung, dass
jeder echte Auftrag einen Bezeichner trägt — die einzige Prüfung am
Bestand, und sie schreibt bewusst **keine Zahl** fest: ein Test, der
`78` festnagelt, wäre beim nächsten übergebenen Repo rot, ohne dass
etwas kaputt ist.

Registriert als 41. Kategorie in `check_consistency.py`. Rotbeweis
geführt: Zuordnung sabotiert → 6 Befunde, zurückgesetzt → 0.

### Der Test hat auf seinem ersten Lauf einen vierten Fehler gefangen

Beim Umbau auf fünf Stufen war aus `` ein `\b` geworden — das
Muster suchte einen echten Backslash statt einer Wortgrenze. Die
Erwähnungs-Erkennung war damit tot, und **niemand hätte es gemerkt**:
sie meldet still zu wenig, nicht zu viel.

Nach der Korrektur ändert sich der Stand: **34 offen statt 37**, plus
**3 „nur erwähnt (prüfen)"** — `amigadev/trackloader`,
`gonk23/HXCFE_Amiga_copy_utility`, `joncampbell123/floppytools`. Diese
drei kommen in fremden Gutachten vor, ohne deren Gegenstand zu sein.
Sie sind eine Frage, kein Ergebnis: entweder mitbehandelt und nur nicht
sauber benannt, oder übersehen.

Ein Zählwerk ohne Test ist eine Zahl, der man glaubt.

---

## FMT-13 — `dim_atari`: ein Magic, das wir nicht prüfen (MF-684)

**Kennzahl:** ungeprüfte Formate ↓ (`dim_atari` steht auf T3).
**Stand: gemessen, NICHT geändert** — und der Grund ist der wichtigste
Teil dieses Eintrags.

### Was gemessen ist

`dim_atari_probe()` prüft **kein** Magic. `grep -c "0x4242\|'B'"` in
`src/formats/dim_atari/uft_dim_atari.c` = **0**. Die Probe prüft eine
X68000-Abgrenzung und dann Geometrie-Plausibilität — mehr nicht.

Jacknife prüft es: `dllmain.c:590`

```c
else if (*(unsigned short *)disk_image.buffer == 0x4242)  // "BB"
{
    // Fastcopy DIM image, unpack it to flat buffer if needed
```

Von mir im Klon gelesen, nicht übernommen. (Das Gutachten nannte
Zeile 591; gemessen ist 590.)

### Warum der Parser trotzdem unverändert bleibt

**Unser eigener Header widerspricht dieser Quelle**, und zwar direkt:

| Offset | unser Header sagt | Jacknife liest |
|---|---|---|
| 0x00 | „Flags (unused, often 0)" | `'B'` |
| 0x01 | „Reserved" | `'B'` |

Zwei Möglichkeiten, und sie führen zu entgegengesetzten Änderungen:
Entweder unsere Spec ist falsch — dann gehört das Magic geprüft. Oder
Jacknife behandelt eine **Fastcopy-Variante**, während wir das
allgemeinere DIM meinen — dann würde eine Magic-Prüfung gültige Dateien
abweisen. Der Kommentar in Jacknife sagt wörtlich „Fastcopy DIM image",
was für die zweite Lesart spricht.

Das Gutachten nennt Hatari als zweite Quelle. **Ich konnte sie nicht
verifizieren**: Hatari ist nicht geklont, und `raw.githubusercontent.com`
liefert für die genannten Pfade 404. Damit steht **eine** verifizierte
Quelle gegen unsere eigene Dokumentation.

Genau in dieser Lage sind hier fünf Parser gegen erfundene Specs gebaut
worden (FMT-2/3/10/11/12). Eine Änderung am Format-Layer auf eine
Quelle, die der eigenen Doku widerspricht, ist die Wiederholung dieses
Fehlers — nur diesmal mit einer echten fremden Datei als Anlass statt
mit einer erfundenen.

### Was den Eintrag schließt

Eine **zweite unabhängige Quelle**, die sagt, ob das Magic zum Format
oder zur Fastcopy-Variante gehört. Kandidaten: Hatari `src/` (Klon
nötig), die Fastcopy-Dokumentation, oder eine reale `.dim`-Datei aus
zwei verschiedenen Erzeugern. Erst danach entscheidet sich, ob die
Prüfung dazukommt oder unser Header korrigiert wird.

**Bis dahin ist der Befund mehr wert als der Fix**, weil er die Frage
festhält, die vorher niemand gestellt hatte.

---

## FS-1 — `uft_amiga_entry_t.blocks`: ein Ergebnisfeld ohne Erzeuger (MF-685)

**Kennzahl:** keine direkt; Ehrlichkeits-Befund am Dateisystem-Leser.

`uft_amiga_entry_t` führt ein Feld `blocks` („Blocks used"). In
`src/fs/uft_amigados.c` wird es **nirgends zugewiesen** — gemessen, alle
`blocks`-Treffer dort gehören zu `uft_amiga_chain_t`. Es meldet immer 0.

Eine 127-Byte-OFS-Datei belegt mindestens einen Datenblock. **0 ist also
nicht ungenau, sondern falsch.**

Aufgefallen ist es, weil `test_adf_directory_crosstool` es beim ersten
Lauf als Ergebnis ausgab: „belegt 0 Blöcke" — eine gedruckte Null sieht
aus wie eine Messung. Der Test nennt es jetzt beim Namen und schreibt
den falschen Wert **nicht** fest; ihn zu behaupten hieße, den Fehler
einzufrieren.

### Die Fehlerklasse ist neu

`audit_setting_wiring.py` (MF-669/672) fängt *Einstellungen*, die
geschrieben und nie gelesen werden. Hier ist es die Spiegelseite: ein
**Ergebnis**feld, das gelesen werden kann und nie geschrieben wird. Das
Tor sieht diese Klasse nicht — es prüft Einstellungs-Strukturen, nicht
Ausgabe-Strukturen.

Ob sich das lohnt zu automatisieren, ist offen. Ein Ausgabefeld ohne
Erzeuger ist in einem Forensik-Werkzeug aber genau so gefährlich wie
ein Regler ohne Wirkung: beide sehen aus wie eine Aussage.

### Drei Wege

1. **Füllen** — aus der Blockkette (`uft_amiga_get_chain` existiert).
   Braucht einen eigenen Rotbeweis und kostet je Verzeichniseintrag
   einen Kettenlauf; das ist eine Aufwandsentscheidung, keine
   Kleinigkeit.
2. **Entfernen** — öffentliche Struktur, also API-Änderung.
3. **Als unbelegt dokumentieren** — der heutige Zustand, aber im Header
   statt nur im Test.

Nicht entschieden.

---

## FS-2 — was das ADF-Korpus NICHT bezeugen kann (MF-685)

Der Kreuzvergleich aus MF-685 prüft ein **flaches OFS mit einer Datei**.
Mehr trägt `xdftool_dd_ofs.adf` nicht. Ausdrücklich **nicht** geprüft:

* **FFS** — anderes Datenblock-Format (roh 512 statt 488 mit Kopf).
* **Unterverzeichnisse** — die Hash-Kette über mehr als eine Ebene.
* **Hard- und Softlinks** — der Leser kann sie laut Header
  (`is_hardlink`, `is_softlink`, `real_entry`, `link_target`); ob er sie
  richtig kann, weiß niemand.
* **Lange Dateinamen** (LFS) und internationale Groß-/Kleinschreibung —
  `uft_amiga_hash_name(…, bool intl)` hat den Schalter, das Korpus
  keinen Fall dafür.

Das steht hier als **benannte Grenze**, nicht als stille Lücke. Jeder
dieser Fälle braucht ein fremd erzeugtes Abbild; `xdftool` kann sie
erzeugen, und die Erzeugungsbefehle gehören dann ins Manifest — genau
wie beim vorhandenen.

---

## HAL-3 geschlossen — und die Messung gab gwnbd recht (MF-686)

Alle drei Teile des Pakets erledigt, nach ausdrücklicher Freigabe für die
geschützte Datei.

### (a) Die Byteebenen-Naht

`uft_gw_stream_ops_t` + `uft_gw_set_stream_ops()` + `uft_gw_open_stream()`.
Bauform bewusst konservativ: die plattformnahen Funktionen sind
**unverändert** und heißen nur `*_platform`; die Weiche davor reicht ohne
eingespeiste Ops exakt dorthin durch. **Ohne Einspeisung ist der
Produktionsweg Zeile für Zeile derselbe.** Die Kommando- und
Dekodierlogik ist nicht berührt.

Das Feld in der Gerätestruktur ist kein ABI-Bruch: die Struktur ist opak,
im Header steht nur `typedef struct uft_gw_device uft_gw_device_t`.

### (b) Das Experiment — und sein Ergebnis

`tests/test_gw_nak_resync.c`. **Gemessen, vor jedem Eingriff:**

```
Befehl 1 (NAK erwartet): rc=-7
danach ungelesen im Puffer: 3 Byte
Befehl 2 (sauber):       rc=-5
[GW] Echo mismatch: expected 0x02 got 0xDE
```

Der Treiber räumte den Strom nicht ab und las die Reste als **Kopf des
nächsten Befehls**. Danach ist die Sitzung verloren: jeder weitere Befehl
liest um einen Rahmen versetzt.

**Damit ist die Einquelligkeit aufgelöst, ohne die Regel zu verbiegen.**
gwnbds Firmware-Beobachtung war eine Quelle und hätte allein nicht
gereicht. Die zweite ist diese Messung am eigenen Code — und sie ist die
stärkere, weil sie nicht davon abhängt, *warum* Bytes im Strom liegen.
Ein flackerndes Kabel genügt.

Behoben durch Abräumen nach NAK (50 ms, es wird nichts erwartet).
Danach: 3 Byte abgeräumt, Folgebefehl grün.

### (c) `uft_gw_set_pin` entfernt

Null Aufrufer, Verwaisten-Regel. gwnbds REDWC-Messung (Pin 2 ist auf
5,25″ REDWC statt Density-Select, 28/30 gegen 1/23 Sektoren) steht als
Kommentar an der Stelle der früheren Definition — samt der Begründung,
warum nicht verdrahtet wurde: einen Pin zu setzen, den niemand braucht,
wäre ein Schalter ohne Wirkung, und wo wir ihn brauchen würden, ist die
richtige Belegung eine Messung an Hardware, die wir nicht haben.

### Was jetzt anders ist

Die Fehlerklasse, die `DIVERGENCES.md` §D-2 als **„Detection: none"**
führte, ist prüfbar. Nicht nur dieser eine Fall — jede Frage, die
zwischen Kopfbytes und Nutzlast liegt, lässt sich jetzt am einzigen
Controller mit Bench-Erfahrung stellen, ohne Gerät.

Prüfstand: Bau grün, 283/283 mit benanntem Skip.

---

## FMT-13 geschlossen — die zweite Quelle war beschaffbar (MF-687)

MF-684 hat angehalten, weil **eine** verifizierte Fremdquelle gegen die
eigene Dokumentation stand. Ein Varianten-Zyklus hat die fehlenden
Quellen beschafft. Jetzt sind es **drei unabhängige Implementierungen**,
alle drei von mir im Klon nachgelesen:

| Quelle | Fundstelle | was sie tut |
|---|---|---|
| Hatari (GPLv2+) | `src/floppies/dim.c:75-76` | lehnt ab, wenn `[0]`/`[1]` ≠ `0x42` |
| Hatari, Spec-Kommentar | `dim.c:36` | `0x0000 Word ID Header (0x4242('BB'))` |
| HxC libhxcfe | `dim_loader/dim_loader.c:74`, `:110` | `id_header != 0x4242` → ablehnen |
| Jacknife | `dllmain.c:590` | `== 0x4242 // "BB"` |

**`BB` gehört zum Format, nicht zu einer Fastcopy-Variante.** Der
Verdacht aus MF-684 löst sich auf: das Atari-`.dim` *ist* das
FastCopy-Format. Unser Header-Kommentar („Flags (unused, often 0)" /
„Reserved") war von keiner Quelle gedeckt.

### Was gemessen wurde, bevor etwas geändert wurde

Zwei Abbilder, Unterschied **genau zwei Byte**. Vorher:

```
mit  `BB`: angenommen=ja Konfidenz=90
ohne `BB`: angenommen=ja Konfidenz=90
```

Jede Datei passender Länge wurde als DIM angenommen und ihre ersten 32
Byte als Geometrie gelesen. Nachher: `ohne BB → angenommen=nein`.

### Eine Feinheit, die ich NICHT übernommen habe

Hatari lehnt zusätzlich `[0x03] != 0` und `[0x0A] != 0` ab. Das sind
**Hataris eigene Grenzen** — es kann keine „used sectors only"-Abbilder
und keine Startspur ≠ 0 —, nicht Eigenschaften des Formats. Unsere
Header-Tabelle führt beide Felder als zulässig, und sie hier abzulehnen
würde eine Leser-Beschränkung in ein Urteil über die Datei verwandeln.

Nur das Magic ist von allen drei Quellen gedeckt, also wird nur das
Magic geprüft. Das ist der Unterschied zwischen einer Referenz folgen
und sie abschreiben.

### Was offen bleibt

* **`dim_atari` steht weiter auf T3.** Ein Proben-Test hebt kein Format;
  dafür braucht es ein reales Abbild aus fremder Hand. Der
  Varianten-Zyklus hat ein Fixture-Paar erzeugt (`tools/uft-variants/
  work/fixtures/`) — die Aufnahme in den Baum ist Verteilung und damit
  Eigentümer-Entscheidung.
* **E-Copy** ist die einzige kopflose `.dim`-Fassung und bleibt
  **unbelegt**: nur Jacknife setzt sie um, Foren-Zeugnisse sind keine
  zweite Quelle. Für sie wäre unsere Geometrie-Lesart ebenfalls falsch.
* **Der X68000-Zweig** in der Probe ist seit dem Magic-Fix nahezu tot.
  Er bleibt stehen: seine Medientyp-Tabelle wird laut Varianten-Zyklus
  von MAME und HxC widersprochen, und das zu entfernen ist eine eigene
  Änderung mit eigenem Beleg. Als Fundus notiert, nicht gehandelt.
* **Der Schreibpfad** prüft das Magic weiterhin nicht — `write_track`
  schreibt in fremde Dateien passender Größe. Eigener Prüfauftrag.

---

## FMT-14 — der Schreibpfad prüfte sein Ziel nicht (MF-688)

**Kennzahl:** keine der vier direkt — aber der Befund läuft gegen
Prinzip 1, und das steht über der Kennzahl-Regel.

### Der Fehler

`dim_atari_open()` prüfte kein Magic. Es liest den Kopf, prüft
Geometrie-Plausibilität und öffnet **`"r+b"`**, wenn der Aufrufer
schreiben will. Wer das Plugin unmittelbar wählt — ausdrückliche
Formatwahl, ein Fuzzer, ein Aufrufer an der Registry vorbei — bekam für
**jede Datei passender Länge** ein Schreibziel, und `write_track()`
schrieb hinein.

Gemessen, bevor etwas geändert wurde:

```
open(schreibend) auf Fremddatei: rc=0   <- angenommen
→ die Datei kam VERÄNDERT zurück
```

Das ist stille Veränderung fremder Daten. Ein Benutzer mit einer
Nicht-DIM-Datei richtiger Länge im falschen Dialog überschreibt sie
kommentarlos.

Behoben: `open()` prüft das Magic vor allem anderen, mit denselben drei
Referenzen wie die Probe.

### Der Rotbeweis hat sich selbst korrigiert

Der erste Entwurf bestand — **aus dem falschen Grund**. Er setzte die
ersten 64 Byte der Fremddatei auf `0x5A`, um sie wiedererkennbar zu
machen, und zerstörte damit den Geometrie-Kopf. `open()` scheiterte an
der Geometrie, nicht am fehlenden Magic. Grün, und nichts gezeigt.

Jetzt bleibt der Kopf vollständig gültig; **einzig die zwei Magic-Bytes
fehlen**. Damit kann `open()` nur noch aus einem Grund ablehnen. Und der
Test schreibt absichtlich echt und sieht danach nach: eine Ablehnung,
die vorher schon geschrieben hat, ist keine Ablehnung.

### Der Zensus — und was er über Messungen lehrt

`scripts/audit_open_vs_probe_magic.py`, registriert als 42. Kategorie.

Die naheliegende Frage wäre „hat dieses Plugin ein Magic?" gewesen, und
sie ist falsch: von 81 Plugins mit Schreibpfad sind die meisten
**kopflos** — ADF, D64, IMG, XFD werden an der Dateigröße erkannt und
*können* nichts prüfen. Ein Tor, das 70-mal falsch anschlägt, wird
übergangen.

Die richtige Frage ist enger: **prüft die Probe ein Magic, das `open()`
nicht prüft?** Wer beim Erkennen auf feste Bytes besteht, hält sie für
wesentlich; beim Öffnen darauf zu verzichten ist ein Widerspruch in sich.

Der Weg dahin ging über zwei eigene Fehlmessungen, beide von Hand
gefunden:

| Lauf | Befunde | was falsch war |
|---|---:|---|
| 1 | 10 | zählte jeden Byte-Vergleich — auch Konfidenz-**Hinweise**. `img_probe` prüft `data[0] == 0xEB`, aber nur um die Zahl zu erhöhen; erkannt wird über die Größe. Dazu `scp` mit `write_track = NULL`. |
| 2 | 1 | der Rückgabewert-Teil kannte nur `UFT_ERROR`, nicht `UFT_ERR_` — damit galt `cqm_open` als prüfungslos, obwohl es prüft. |
| 3 | **0** | — |

Ein Vergleich, der bei Nichtübereinstimmung **ablehnt**, ist ein Magic.
Einer, der eine Zahl hochzählt, ist ein Hinweis — und ein Hinweis darf
im `open()` fehlen.

Rotbeweis: die Prüfung in `dim_atari_open` wieder entfernt → 1 Befund,
zurückgesetzt → 0.

### Was die Null NICHT heißt

`dim_atari` war die einzige Instanz **dieses Musters**. Sie heißt nicht
„alle Schreibpfade sind sicher": die kopflosen Formate erkennen allein
an der Größe, und eine Größenprüfung ist dünn. Ob das ein eigener Befund
ist, ist eine andere Messung — und eine andere Frage.

---

## ORPH-4 — 42 unerreichbare Format-Leser, alle schreibend geöffnet (MF-691)

**Kennzahl:** keine der vier direkt — aber es ist die größte einzelne
Menge toten Codes, die dieser Baum seit MF-369 gesehen hat, und sie
trägt ein Risiko.

### Was der Modus-Zensus gefunden hat

Gesucht war das Muster aus MF-688 in allgemeiner Form: **wer öffnet
schreibbar, obwohl der Aufruf nur lesen will?** Von 468 `fopen`-Stellen
in `src/formats/` öffnen 48 unbedingt `"r+b"`.

Der erste Blick sah nach 48 Modus-Fehlern aus. Der zweite zeigte etwas
anderes:

| | |
|---|---|
| Dateien mit unbedingtem `"r+b"` | 48 |
| davon mit **null** externen Aufrufern | **42** |
| Zeilen darin | **5 479** |
| erreichbar | 3 (`msx`, `pc98`, `trs80`) |
| ohne erkennbare Funktionen | 3 |

Die 42 gehören einer **zweiten, älteren Leser-Familie** an: sie benutzen
`FloppyDevice` statt `uft_disk_t` und eine Signatur ohne
`read_only`-Parameter. Ihr Öffnungsmuster ist überall dasselbe:

```c
FILE *fp = fopen(path, "r+b");
bool ro = false;
if (!fp) { fp = fopen(path, "rb"); ro = true; }
```

Also: *schreibbar öffnen, wenn es irgend geht.* Eine Datei anzusehen
hieße hier, sie beschreibbar in der Hand zu halten.

### Warum das jetzt nicht gefährlich ist — und wann es das wird

Solange niemand sie aufruft, richtet das Muster nichts an. Wer diese
Familie aber verdrahtet, bekommt **42 Schreibvektoren auf einen
Schlag** — und zwar solche, die schon beim bloßen Öffnen greifen, nicht
erst beim Schreiben. Das ist derselbe Fehler wie MF-688, nur
zweiundvierzigfach und vorbereitet.

Sie werden dabei **gebaut**: die Dateien stehen in
`UnifiedFloppyTool.pro`. Toter Code, der übersetzt wird, ist teurer als
toter Code, der wegfällt — er wird bei jeder Änderung mitgeschleppt und
sieht in jeder Statistik nach Fähigkeit aus.

### Warum hier nicht gelöscht wird

Eine Löschung dieser Größe braucht die **sechsstufige Beweiskette** aus
MF-369 (`.pro`-Bedingungen, Regex-Falle, Kommentar-Strip,
Symbol-Rauschen, Skript-Referenzen, qmake-Vollbau-Abnahme). Das ist eine
eigene Sitzung, kein Tagesrand — und die Erfahrung aus MF-369 sagt, dass
genau die Abkürzung teuer wird.

**Nächster Schritt:** die Kette auf diese 42 anwenden, nicht auf
Verdacht löschen. Die drei erreichbaren (`msx`, `pc98`, `trs80`) bleiben
davon unberührt und brauchen stattdessen die Modus-Korrektur aus MF-688:
`read_only` respektieren.

---

## FMT-15 — kopflose Formate erkennen allein an der Größe (MF-691)
<!-- status: offen -->

**Kennzahl:** keine. Ehrlichkeits-Befund, kein Handlungsdruck.

Die Null aus Tor 42 (MF-688) heißt „dieses Muster kommt nicht mehr vor",
nicht „alle Schreibpfade sind sicher". Der Rest ist eine **andere
Frage**, und sie gehört benannt, bevor sie wieder zu Stillschweigen
wird.

Von 81 Plugins mit Schreibpfad sind die meisten **kopflos** — ADF, D64,
IMG, XFD und Verwandte sind rohe Sektorabbilder ohne Magic. Sie werden
allein an der **Dateigröße** erkannt. Das ist dünn:

* Jede Datei von 174 848 Byte ist für uns eine D64. Ein ZIP, ein
  Abschnitt eines Videos, eine fremde Sicherung — die Größe entscheidet.
* Beim Schreiben heißt das: ein mehrdeutiges Ziel wird ohne Rückfrage
  angenommen.

### Schließbedingung

Zwei Wege, einer davon genügt je Format:

1. **Inhalts-Plausibilität** statt Größe allein — BAM-Struktur für D64,
   Root-Block-Prüfsumme für ADF, Bootsektor-Plausibilität für IMG. Das
   ist je Format eine eigene, belegbare Prüfung mit eigenem Rotbeweis.
2. **Ausdrückliche Bestätigung** bei mehrdeutigen **Schreib**zielen. Die
   Leseseite darf großzügig bleiben; die Schreibseite ist die, an der
   Daten verloren gehen.

Beides ist Arbeit mit Referenzbedarf, keine Aufräumaktion. Der Eintrag
steht, damit die Frage nicht mit der beantworteten verwechselt wird.

---

## ORPH-5 — `uft_convert_memory()` ist öffentlich und wird nur von Tests gerufen (MF-693)
<!-- status: wartet-eigentuemer(2026-08-30) -->

**Kennzahl:** keine unmittelbar. Der Punkt ist die **Anker-Regel**: eine
öffentliche C-API ohne Produktions-Tür braucht einen benannten Plan-Anker
oder sie geht. Bis das entschieden ist, ist er Fundus mit Verfallsdatum.

Gemessen mit dem Tür-Sucher (`tools/uft-innendienst/scripts/tuersucher.py`,
Kommentare und String-Literale gestrippt) und unabhängig mit `git grep`
gegengeprüft. Alle Nennungen des Symbols im Baum:

| Fundstelle | Art |
|---|---|
| `include/uft/uft_format_convert.h:147` | Deklaration |
| `src/formats/uft_format_convert_dispatch.c:725` | Definition |
| `tests/test_convert_atr_xfd.c:80` | Aufruf |
| `tests/test_convert_table_has_dispatch.c:58` | Aufruf |
| `tests/test_convert_table_has_dispatch.c:124` | Aufruf |

**Kein einziger Aufruf außerhalb von `tests/`.** Die GUI wandelt über
`uft_convert_file()`; der Speicher-Weg hat im Baum keinen Verbraucher.

### Warum das trotzdem kein „dann weg damit" ist

Die Funktion steht in einem **öffentlichen Header**. Wer die Bibliothek
einbindet, kann sie aufrufen, ohne in diesem Baum vorzukommen — sie ist
damit nicht dasselbe wie eine verwaiste `static`-Funktion. Genau deshalb
war die Reparatur aus **MF-567** richtig und nicht überflüssig: bis dahin
ging `uft_convert_memory()` vollständig am Preflight-Tor vorbei, weil es
keine Dateipfade übergibt und die Prüfung ohne Pfade sofort mit
`ABORT_INVALID_ARG` zurückkehrte. Gemessen kamen aus 4096 Byte Zufall
3 712 758 Byte SCP heraus, bei einem Paar, das die Matrix wörtlich
Fabrikation nennt.

Der Befund ist also nicht „toter Code", sondern: **eine öffentliche
Zusage, die im eigenen Baum niemand einlöst.** Das ist die Lage, in der
eine Regression unbemerkt bleibt, weil kein Produktionspfad darüber
läuft — dieselbe Lage wie bei MF-567, nur diesmal vorher benannt.

### Entscheidung, die ansteht (Eigentümer)

Genau eine von drei, keine vierte:

1. **Anker setzen** — der Speicher-Weg gehört zur öffentlichen API und
   bleibt; der Plan-Anker wird benannt (welcher Verbraucher, welche
   Fassung). Dann gilt „nur Tests" als beabsichtigt und der Eintrag
   schließt.
2. **Tür bauen** — die GUI bekommt den Speicher-Weg dort, wo sie ihn
   ohnehin bräuchte (Wandlung ohne Zwischendatei). Bewegt dann
   **angebotene Wandlungspfade** nicht, aber macht die bestehende Zusage
   erreichbar.
3. **Zurückziehen** — aus dem öffentlichen Header nehmen, `static` im
   Dispatch, Tests auf `uft_convert_file()` umstellen. Braucht die
   sechsstufige Beweiskette aus MF-369 nicht (ein Symbol, kein Modul),
   aber eine ABI-Prüfung: das Symbol ist exportiert.

**Was diese Messung nicht sieht:** Aufrufe über Funktionszeiger, aus
Makros und hinter `#if`. Der Fehler zeigt in die vorsichtige Richtung —
eher wird etwas fälschlich als benutzt gemeldet als fälschlich als
türlos. Für dieses Symbol wurde die Fundstellenliste von Hand
nachgesehen; sie ist vollständig.

---

### Was seither geschah, Teil 1: die Delegations-Messung (MF-693)

**Die Frage war: sind `uft_convert_file()` und `uft_convert_memory()`
zwei parallele Wandlungs-Implementierungen?** Gemessen — und die Antwort
verschiebt den Befund, statt ihn zu bestätigen.

**Nein, es gibt einen Kern.** Beide enden bei `dispatch_conversion()`
(`src/formats/uft_format_convert_dispatch.c:150`), das **23**
Format-Paare bedient. `uft_convert_file_inner()` ruft es an `:780`,
`uft_convert_memory()` an `:1098`. `uft_convert_file()` delegiert
**nicht** an `uft_convert_memory()` — das stimmt —, aber daraus folgt
keine zweite Wandlungs-Implementierung.

**Die Doppelung sitzt enger, und der Code sagt es selbst.**
`uft_convert_memory()` trägt eine eigene **Abkürzungskette für 7 dieser
23 Paare** — ATR↔XFD, IMD→IMG, IMG→IMD, TD0→IMG, D64→G64, G64→D64 —
nach dem Preflight-Tor, vor dem Dispatch-Rückfall. Der Kommentar an
`:973` steht seit MF-655 dort:

> *„ACHTUNG: diese Kette steht ZWEIMAL in dieser Datei … Dieselbe
> Doppelung war die Ursache von MF-567."*

**Und die zwei Fassungen divergieren heute schon — ohne Bau messbar.**
Am Paar D64→G64:

| | `dispatch_conversion` | `uft_convert_memory` |
|---|---|---|
| Weg | `uftc_convert_d64_to_g64()` (`:225`) | von Hand, `:1030-1055` |
| öffnet über | Format-Plugin (`uft_disk_open`) | `d64_load_buffer()` |
| `opts` | benutzt (Fortschritt, `convert_options_t`) | **fasst `opts` nicht an** |
| Warnungen | `uftc_add_warning` bei Fehlern | keine |

Was `uftc_convert_d64_to_g64()` mit den Optionen tut, überspringt der
Speicher-Weg still. Das ist die Divergenz — nicht als Risiko, sondern
als Ist-Stand.

### Der vorgeschlagene Weg wird von der Messung nicht getragen

„`uft_convert_file()` auf `uft_convert_memory()` aufsetzen" scheitert an
der Bedingung, die im Vorschlag selbst als Abbruchkriterium steht:
`uft_convert_memory()` schreibt für die **16 Nicht-Abkürzungspaare** in
eine **Temp-Datei** und liest sie zurück (`:1085-1110`). Heute schreibt
`dispatch_conversion` im Dateiweg direkt nach `dst_path`. Die Vereinigung
in dieser Richtung kaufte jedem Datei-Wandlungslauf eines dieser 16 Paare
einen vollständigen Schreib-Lese-Umweg über die Ausgabedatei ein — bei
SCP-Größen ist das genau der Streaming-Einwand.

### Die Gegenrichtung, drei Fassungen (Eigentümer)

1. **Kette einmal, zwei Enden** *(Empfehlung)* — die 7 Abkürzungen
   wandern in **einen** Helfer mit zwei Abschlüssen (in einen Puffer /
   in eine Datei), den beide Einstiege rufen. Eine Implementierung je
   Paar, `opts` wirkt auf beiden APIs gleich, kein Temp-Datei-Preis.
   Aufwand: mittel, rein mechanisch, keine Verhaltensänderung beabsichtigt.
2. **Abkürzungen streichen** — `uft_convert_memory()` geht für alle 23
   Paare durch `dispatch_conversion()`. Am einfachsten und am
   ehrlichsten, kostet aber den 5 reinen In-Memory-Paaren einen
   Temp-Datei-Umweg, den sie heute nicht haben. Leistungsfrage, keine
   Korrektheitsfrage — und Forensik schlägt Performance.
3. **Stehen lassen, Tor bauen** — ein Zensus, der anschlägt, sobald ein
   Paar in nur einer der beiden Ketten steht. Behebt die Ursache nicht,
   verhindert aber den nächsten MF-567.

**Rotbeweis für 1 und 2 — vor der Änderung:** dieselbe D64 durch beide
APIs schicken und die Ausgaben byteweise vergleichen, mit gesetztem
`opts` (etwa `target_geometry`). Wenn sie heute schon abweichen, ist das
der rote Test; wenn nicht, ist er nach der Änderung der grüne
Regressionsschutz. Dazu der MF-567-Fall über die öffentliche API: das
Preflight-Tor muss auf beiden Wegen greifen.

### Berichtigung zur Divergenz-Behauptung (MF-694)

Der Abschnitt oben sagt, die zwei Ketten „divergieren heute schon", und
belegt das damit, dass die Speicher-Abkürzung `opts` nicht anfasst. Das
erste stimmt so **nicht**, das zweite schon — und was darunter liegt,
ist schlimmer.

Nachgemessen an `uftc_convert_d64_to_g64()`
(`src/formats/uft_format_convert_sector.c:119-175`):

* Der Encoder bekommt seine Einstellungen aus
  `convert_options_t conv_opts; convert_get_defaults(&conv_opts);` —
  **nicht** aus `opts`.
* `opts` wird dort ausschließlich für `uftc_report_progress()` benutzt.
* Die Speicher-Abkürzung ruft `d64_to_g64(d64, &g64, NULL, NULL)`, und
  `d64_to_g64` fällt bei `NULL` auf **genau dieselben**
  `convert_get_defaults()` zurück (`src/formats/c64/uft_d64_g64.c:967-971`).

**Folge: beide Ketten erzeugen für D64→G64 dieselben Bytes.** Was sie
unterschiedlich machen, ist Buchhaltung — Fortschrittsmeldungen,
Warntexte, `result`-Felder — nicht Nutzdaten. Meine Formulierung war zu
stark; die Doppelung ist ein **Divergenz-Risiko**, keine gemessene
Divergenz.

**Der Rotbeweis aus dem Plan kann darum nicht feuern.** Er lautete:
„D64→G64 über die Speicher-API mit gesetztem `opts`-Nudge muss sich vom
Nudge-losen Lauf unterscheiden." Es gibt kein Feld in
`uft_convert_options_ext_t`, das den Encoder erreicht — auf **keinem**
der beiden Wege. Ein Beweis, der nicht feuern kann, hätte hier die
Vereinigung als „belegt" ausgewiesen, obwohl er nichts gesehen hätte.

### Der Befund, der stattdessen darunter liegt: OPT-2

`convert_options_t` (der C64-Encoder: `extended_tracks`,
`include_halftracks`, `gap_fill`, `sync_length`, `generate_errors`) wird
**nirgends** aus `uft_convert_options_ext_t` befüllt — gemessen, kein
einziger Treffer für eine Zuweisung an `conv_opts.*` außerhalb von
`convert_get_defaults()`.

Das heißt konkret: `preserve_errors` aus der Oberfläche erreicht die
Fehlerkarten-Erzeugung nicht, und die 40/42-Spur-Wahl erreicht
`extended_tracks` nicht. Das ist dieselbe Klasse wie `OPT-1` (sechs
Wandlungsoptionen, die nirgends ankommen, MF-672) — nur eine Schicht
tiefer und vom bestehenden Tor 40 nicht gesehen, weil dort geprüft wird,
ob ein Feld **gelesen** wird, nicht ob der Wert **ankommt**.

**Rotbeweis, der feuert:** `opts.preserve_errors = true` bei D64→G64
setzen und die Ausgabe mit dem Lauf ohne vergleichen. Byteidentisch ⇒
die Einstellung kommt nicht an. Das ist rot **vor** jeder Änderung und
grün, sobald die Übersetzung gebaut ist.

### Was das für die Vereinigung heißt

Sie bleibt richtig, aber ihre **Begründung** ändert sich: nicht „die
Ketten rechnen verschieden" (tun sie heute nicht), sondern „zwei Ketten
für dieselben sieben Paare, von denen eine die Optionen-Übersetzung
bekommen müsste, die andere aber nicht". Wer OPT-2 behebt, ohne vorher
zu vereinigen, baut die Übersetzung an genau einer der beiden Stellen
ein — und **dann** divergieren sie, gemessen und mit Datenfolge.

**Reihenfolge daher:** OPT-2 rot machen ⇒ vereinigen ⇒ Übersetzung
einmal einbauen ⇒ OPT-2 grün. Nicht umgekehrt.

### Was seither geschah, Teil 2: die Verallgemeinerung wurde geschärft

Der Auftrag war, die Klasse als Marke `PUBLIC_PROMISE` in den Tür-Sucher
zu heben: *in `include/uft/**` deklariert und ohne Produktionsaufrufer.*
Die Messung hat die Marke widerlegt, bevor sie ausgeliefert war:

| Fassung der Frage | Treffer |
|---|---|
| WAISE oder NUR_TESTS | 3559 |
| davon in `include/uft/**` deklariert | **1777** |
| davon: Header wird von einem **fremden** `src/`-Verzeichnis eingebunden | **289** |

1777 von 3559 trennen nichts. Der Grund ist gemessen: **UFT ist
`TEMPLATE = app`** (`UnifiedFloppyTool.pro:122`), keine Bibliothek. Es
gibt keine Header-Installationsliste, keine API-Fassade, und das
Export-Makro `UFT_API` — **zweimal** definiert, `uft_compiler.h:231` und
`uft_platform.h:208` — steht an **null** Symbolen. `include/uft/` ist
schlicht *das* Kopfverzeichnis einer Anwendung.

**Dritte, denen man etwas versprechen könnte, gibt es nicht.** Die
Klasse überlebt, ihr Adressat ändert sich: nicht „Zusage an Dritte",
sondern **Angebot an den übrigen Baum**. Die Marke heißt deshalb
`ANGEBOT_OHNE_ABNEHMER` und trifft **289**.

Die Aufteilung zeigt sofort, dass sie diskriminiert — an der Spitze
steht ein Befund, den `CLAUDE.md` bereits als P0-2 führt:

| Anzahl | Ort |
|---|---|
| 46 | `src/fs/uft_fat12.c` |
| 25 | `src/analysis/otdr` |
| 19 | `src/hal/uft_kryoflux_dtc.c` |
| 13 | `src/formats/uft_adf.c` |
| 13 | `src/hal/uft_greaseweazle_full.c` |
| 12 | `src/protection/c64` — der Kopierschutz-Katalog, „Bestand, nicht Fähigkeit" |

Rotbeweise am gepflanzten Baum, alle drei gelaufen: Kommentar-Strip
abgeschaltet ⇒ eine **erfundene** Zusage aus einer auskommentierten
Deklaration; Verzeichnis-Prüfung abgeschaltet ⇒ das Hausmittel des
eigenen Teilsystems erscheint fälschlich; unverändert ⇒ genau der eine
richtige Fall. Selbsttest 13/13.

**Nebenbefund, eigener Eintrag wert:** `UFT_API` ist an zwei Stellen
definiert und wird nirgends verwendet — eine Vereinbarung ohne Leser in
zwei Fassungen. Als Fundus notiert, nicht eingeplant: es bewegt keine
der vier Kennzahlen.


## ORAK-1 — zwei Oracles tragen einen Test, ohne registriert zu sein (MF-693)
<!-- status: wartet-eigentuemer(2026-08-30) -->

> **Teilerledigt am selben Tag.** `xdftool` ist seit MF-693 registriert
> (`tests/differential/oracles.py`, Register jetzt **8**); die fünfte
> Frage ist für `adfrescue` **gemessen** und fällt auf *verschiedene
> Hände*. Offen bleibt allein die Lizenz-Entscheidung zu `adfrescue`
> — siehe § *Was seither geschah* am Ende dieses Eintrags.

**Kennzahl:** **ungeprüfte Formate (T3) ↓**, aufschiebend. Solange die
beiden nicht im Register stehen, kann kein T1b-Manifest sie zitieren —
und der nächste ADF-Schritt (**FS-2**: FFS, Unterverzeichnisse, Links)
ist genau auf sie geplant.

`docs/ORACLES.md` führt zwei Tabellen, und sie widersprechen sich:

| Werkzeug | in „Stand der Kalibrierung" | in „Registrierte Oracles (7)" |
|---|---|---|
| `amitools xdftool` | **roh**, 2026-08-29, MF-685 | **nein** |
| `adfrescue` | **roh**, Scout-Zyklus `adf_zweitmeinung` | **nein** |
| `a8rawconv` | in der Sammelzeile **ungemessen** | **nein** |

Gemessen: `tests/differential/oracles.py` registriert genau sieben Namen
— `gw`, `cpmls`, `hxcfe`, `samdisk`, `dtc`, `floptool`, `lsatr`. Die
drei oben sind nicht darunter. ORACLES.md sagt zu unregistrierten
Werkzeugen selbst: *„Sie zählen deshalb für kein T1b-Manifest."*

### Warum das mehr ist als ein fehlender Tabelleneintrag

`tests/test_adf_directory_crosstool.c` läuft im Baum und stützt sich in
seinem Kopfkommentar ausdrücklich auf beide:

* Zeile 24–25: das Korpus-Abbild `xdftool_dd_ofs.adf` stammt aus
  `amitools xdftool` — nachgewiesen in
  `tests/corpus_manifest/manifest.json:27`.
* Zeile 48–49: `adfrescue` als **zweite unabhängige Hand**, 127 Byte,
  byteidentisch zur xdftool-Extraktion.

Der Test trägt heute **kein** Tier-Urteil — die ADF-Zeile in
`docs/VERIFICATION_TIERS.md` nennt ihn nicht. Das ist der einzige Grund,
warum hier nichts Falsches behauptet wird. Ein Beleg, den man nicht
zitieren darf, ist aber ein Beleg, der nicht zählt: der Test misst gegen
zwei fremde Hände und darf es nirgends geltend machen.

`a8rawconv` liegt anders und wird hier nur mitgeführt, damit die
Sammelzeile nicht als „drei gleiche Fälle" gelesen wird: es ist weder
gemessen noch registriert und steht in der Kalibriertabelle nur auf der
Liste der offenen Eichläufe. Für ATX wäre es der naheliegende
Beschaffungsweg (`tools/uft-innendienst/out/korb.md`, Posten 3), aber
das ist eine andere Frage.

### Schließbedingung

Je Werkzeug ein Registry-Eintrag in `tests/differential/oracles.py` mit
dem, was `ORACLES.md §Was ein Eintrag braucht` verlangt:
Herkunfts-Anker (Version oder SHA-256, wenn `--version` nicht
beantwortbar ist), Lizenz, Entscheidungsbereich, **und** die
Längensemantik, die für beide bereits gemessen ist (roh, MF-685 bzw.
Scout-Zyklus `adf_zweitmeinung`).

Für `adfrescue` ist dabei die **fünfte Frage** (MF-644, „dieselbe
Hand?") ausdrücklich zu beantworten: es soll die *unabhängige*
Zweitmeinung gegen xdftool sein. Trüge es dieselbe Codebasis, wäre die
Byte-Identität von 127 kein Beleg, sondern eine Tautologie — und die
ADF-Kalibrierung stünde auf einer einzigen Hand.

Gefunden vom `uft-innendienst`-Kalibrierer
(`tools/uft-innendienst/scripts/kalibrierer.py pruefen .`), der die
Kreuzung Register × Kalibriertabelle misst.

### Was seither geschah (MF-693, derselbe Tag)

**Die fünfte Frage ist beantwortet — gemessen, nicht abgewogen.**
`adfrescue` (dschwen, Commit `0cbc5ff`, 2015-12-21) sind **225 Zeilen
eigenständiges C++** mit `stdio.h`, `stdlib.h`, `string.h` und sonst
nichts: kein ADFlib, kein amitools, kein gemeinsamer Unterbau. `xdftool`
ist Python aus `amitools`. Geteilt ist allein die **Dokumentation** —
die ADF-FAQ von Laurent Clévy, die adfrescue als Kopie mitliefert. Eine
Spec ist keine Hand.

**Damit ist die 127-Byte-Identität ein Beleg und keine Tautologie.** Was
sie *nicht* ausschließt, steht im Registry-Eintrag: einen Fehler, den
beide aus derselben FAQ übernommen hätten.

**`xdftool` ist registriert.** Der Eintrag brauchte eine kleine
Erweiterung der Registry, weil das Werkzeug seine Version nicht nennt:

* Neues Feld `version_via` — ein **vollständiges Kommando**, das die
  Version liefert, wenn sie nicht im Startprogramm steckt. Für
  `xdftool` ist das `importlib.metadata.version("amitools")` → `0.8.1`.
* Warum nicht `version_is_unaskable` + SHA-256, der naheliegende Weg:
  die aufgelöste Datei ist bei einem Python-Einstiegspunkt ein
  **Startprogramm-Rumpf**. Sein Hash pinnt den Rumpf, nicht `amitools`
  — der schwächere Anker in der Gestalt des stärkeren, also genau der
  Fehler, vor dem die Beschreibung von `version_is_unaskable` warnt.
  Die SHA-256 steht trotzdem im Manifest daneben: sie sagt, **welches
  Programm lief**, während `version_via` sagt, **welches Paket
  dahinterliegt**. Erst beide zusammen pinnen den Lauf.
* Der Selbsttest lässt jetzt **genau einen** der drei Wege zu
  (`version_args` / `version_via` / `version_is_unaskable`). Rotbeweis
  gelaufen: zwei Wege gesetzt ⇒ rc=1 mit benanntem Grund, keiner
  gesetzt ⇒ rc=1, unverändert ⇒ rc=0.
* Live nachgemessen an `tests/corpus_free/xdftool_dd_ofs.adf`:
  `marker.txt` wird mit **127** gemeldet, nicht mit 488. Semantik
  **roh**, bestätigt.

`manifest_entry("xdftool")` liefert damit `complete: true` mit Version
`0.8.1` und SHA-256 — der erste Registry-Eintrag, der auf dieser
Maschine tatsächlich auflösbar ist.

**Was sich dadurch NICHT bewegt hat, und warum das richtig so ist.**
`docs/VERIFICATION_TIERS.md` ist generiert; `gen_verification_tiers.py`
ordnet Tests über ihre `uft_format_plugin_<sym>`-Verweise einem Format
zu. `test_adf_directory_crosstool.c` nennt keinen — es prüft die
**Dateisystem**-Schicht, nicht das Plugin. Nach der Registrierung neu
erzeugt: **T1=2, T1b=13, T2=21, T3=52, unverändert.**

Das ist kein Fehlschlag, sondern die genauere Fassung des Befunds: die
Tier-Tabelle misst **Format-Plugins**. Für die **Dateisystem**-Schicht
gibt es keine Stufe — und genau das ist, worum es bei `FS-2` geht. Die
Registrierung war die *Vorbedingung* dafür, nicht die Bewegung selbst.

### Offen: die `adfrescue`-Lizenzentscheidung (Eigentümer)

Nicht registriert, und zwar bewusst: **Lizenz vor Fähigkeit.** Gemessen
hat das Repo **keine** Lizenzdatei — Zone ROT, alle Rechte vorbehalten
(`tools/uft-scout/work/adfrescue.messung.json`). Dazu liefert es kein
Binärprogramm; der Bau braucht unter MinGW zwei Zusätze, heute
nachgemessen:

```
g++ -include cstdint -Du_int32_t=uint32_t -o adfrescue.exe adfrescue.cc
```

Ohne sie bricht g++ 13.1.0 in `checksum()` ab (`'u_int32_t' was not
declared`) — `u_int32_t` ist ein BSD-Typname.

Die drei Wege stehen bereits im Scout-Gutachten
`tools/uft-scout/out/adf_zweitmeinung.gutachten.md` als SCOUT-71 und
werden hier auf die EINE Liste geholt, statt eine zweite ID zu eröffnen:

1. **Upstream fragen** — dschwen um eine Lizenzdatei bitten. Das Repo
   ist seit 2015 unverändert, der Autor auf GitHub aktiv. Bester
   Ausgang, längste Laufzeit, Ergebnis nicht in unserer Hand.
2. **Lokal als nicht-weitergebbares Zweit-Oracle** — Registry-Eintrag
   mit `version_is_unaskable`, SHA-256 des Eigenbaus, Quell-Commit
   `0cbc5ff` und dem Baurezept oben. Präzedenz: `dtc` steht mit
   „proprietär, nur Ausführung" im Register. Verglichen wird ohnehin
   nur die **Ausgabe**; es wandert kein Code ein.
3. **Verwerfen** — nur als Verhaltens-Referenz im Testkommentar führen,
   nie zitierfähig. Kostet die dritte Hand.

**Empfehlung: (2), mit (1) parallel.** Die Präzedenz trägt, die Messung
liegt vollständig vor, und (1) kann (2) später nur verbessern.

### Offen: `a8rawconv`

Steht in der Kalibriertabelle auf der Liste der **offenen Eichläufe**
und nicht im Register — also weder gemessen noch zitierfähig. Für ATX
wäre es der naheliegende Beschaffungsweg
(`tools/uft-innendienst/out/korb.md`, Posten 3). Bis dahin ist es eine
halbe Zusage und wird vom Kalibrierer seit MF-693 als **Befund**
gemeldet, nicht mehr als Fußnote: kalibriert-aber-unregistriert ist ab
jetzt gleichrangig mit registriert-aber-ungeeicht. ORAK-1 ist der
Beleg, dass beide Richtungen kosten.

---

## Scout-Block 4 — neun Gutachten, zwei Aufträge, ein Fundus (MF-694)
<!-- status: offen -->

**Kennzahl:** zwei Posten bewegen **ungeprüfte Formate (T3) ↓**, der
Rest bewegt keine und steht darum als Fundus da — nicht als Auftrag.

Der Zyklus zu Block 4 (zwölf Repos, MF-692) ist abgeschlossen: drei
Gutachten lagen aus einem am Sitzungslimit abgebrochenen Lauf vor, neun
sind nachgeliefert. Alle in `tools/uft-scout/out/`.

### Die zwei Aufträge, nach Regel 9

**1 · `hardsector`: Rotbeweis gegen die 3740-Falschzuschreibung, dann
Tabellen-Korrektur.** Kennzahl **T3 ↓**. Quelle:
`out/hardsector_tool.gutachten.md` §2-3. Aufwand klein — zwei
synthetische Dateigrößen, **keine Beschaffung**. Regelkonform unter der
EINFRIER-REGEL: Bugfix an Bestehendem plus Verifikation, die Referenzen
stehen im Gutachten. Einhängepunkt `docs/VERIFICATION_PLAN.md`
§Einfrier-Regel.

**2 · `86f`: T3-Hebung per Differenzlauf.** Kennzahl **T3 ↓**. Aufwand
mittel. Oracle-Kandidat `fftool` (aus fluxfox, MIT) — **Bau und
Kalibrierung sind Bedingung**, nicht Zugabe: `cargo` liegt auf dieser
Maschine nicht, und ein Oracle ohne Längensemantik ist eine Zusicherung
(ORAK-1). Zweitreferenz DiskImageTool (GPL-3.0, nur Verhaltensabgleich,
kein Code). Beschaffung: **ein 86Box-erzeugtes 86F-Abbild** — der
Korpus hat 0 von 24. Quellen: `out/fluxfox.gutachten.md` §4,
`out/DiskImageTool.gutachten.md` §3.

### Was ausdrücklich Fundus bleibt

`ipf-flux` kam **ohne Übernahmeweg** zurück — erwartet ROT, gemessen
PRÜFEN mit derselben Folge: BSD-3 durchgehend, aber die Dreiglied-Kette
ipf-flux ← MAME ← CAPS/SPS ist nicht messbar, und „Clean room" ist eine
Selbstauskunft. Das ist der MF-638-Präzedenzfall, nicht seine Ausnahme.
Wert hat das Gutachten als **Anlage zu LIZ-2, Entscheidung 2**: es
belegt, dass es inzwischen **zwei** blob-freie IPF-Implementierungen
gibt (ipf-flux BSD-3-Kette, fluxfox MIT) — der Neubau-Preis in der
Vorlage ist damit neu zu schätzen. Die Lizenzfrage selbst bleibt beim
Eigentümer.

Ebenfalls Fundus, weil ohne Kennzahl-Bezug: FS-2-Fixture-Zulieferung
(fuseadf — gehört an den bestehenden Punkt, dort schon beschrieben),
FMT-15-Referenzpaar fluxfox+DiskImageTool (FMT-15 führt selbst
„Kennzahl: keine"), M2FM-Decode aus fluxtoimd (Moratorium), WOZ2-Erzeuger
picturedsk, BK-0010-Referenz mfmdisk, `FluxBridge`/DrawBridge-HAL (Hardware ohne
Gerät, MF-310), `fluxpy` mit dem offenen Brother-WP-1-Verhältnis,
flux-analyze-Schadensspuren und `apple-ii-fluxdoctor` als
Community-Bench-Hinweis (6502-Assembler auf echter Hardware, für
diesen Baum gegenstandslos).

### Zwei Werkzeugkasten-Befunde des Scouts

1. `tools/uft-scout/scripts/vermessen.py` erkennt nur `LICENSE` und
   `COPYING` als Lizenzdateinamen und stellte `fluxtoimd` deshalb
   **fälschlich ROT**, obwohl `gpl-3.0.txt` im Wurzelverzeichnis liegt.
   Ein falsches ROT unterdrückt Funde still — dieselbe Fehlerklasse wie
   das falsche „vorhanden" aus MF-610, nur in die andere Richtung.
2. `scripts/scout_stand.py` zählt „Name kommt in einem Gutachten vor"
   als begutachtet; bei `DiskImageTool` waren die Treffer teils ein
   **anderes** Repo (`FloppyDiskImageTool`, markusC64). Für diesen Fall
   durch das neue Gutachten geheilt, das Zählverfahren bleibt anfällig —
   es ist dieselbe Namens-statt-Identitäts-Zuordnung, die MF-677 schon
   einmal gekostet hat.

---

## Drei Eigentümer-Handgriffe, die Fundus in Nutzung verwandeln (MF-695)
<!-- status: wartet-eigentuemer(2026-08-30) -->

**Kennzahl:** alle drei bewegen die fünfte (**Dateien mit ungeklärter
Herkunft**, Befund-Stufe); (1) bewegt zusätzlich **T3 ↓**, sobald sie
fällt.

Drei Zeilen des Eigentümers stehen zwischen dem heutigen Fundus und
seiner Nutzung. Sie stehen hier zusammen, weil sie **eine** Sitzung
sind, nicht drei Nachrichten — und mit Datum, damit der Sekretär sie
altersgestaffelt mahnen kann.

### 1 · capsimg-Lizenzprüfung — die einzige, die eine Fähigkeit zurückbringt

**Frage:** Ist `capsimg` so lizenziert, dass IPF-Lesen im Baum wieder
zulässig ist?

**Messung:** liegt vor. `LIZ-2` führt die Quarantäne; der
Block-4-Zyklus hat zusätzlich `out/ipf-flux.gutachten.md` geliefert —
BSD-3 durchgehend, aber die Dreiglied-Kette *ipf-flux ← MAME ←
CAPS/SPS* ist **nicht messbar**, und „Clean room" ist eine
Selbstauskunft. Zugleich belegt es, dass es inzwischen **zwei**
blob-freie IPF-Implementierungen gibt (ipf-flux BSD-3, fluxfox MIT) —
der Neubau-Preis in der `LIZ-2`-Vorlage ist damit neu zu schätzen.

**Folge bei Ja:** `ipf-flux` wird **sofort** zweites Oracle
(Ausführung ist frei, wie bei `dtc`), sein Varianten-Wissen wird
geerntet, und die IPF-Fähigkeit kommt auf legalem Weg zurück.
**Bei Nein:** die Quarantäne bleibt, das Gutachten bleibt als Anlage —
nichts verfällt.

### 2 · `adfrescue`: eine E-Mail

**Frage:** Autor (dschwen) um eine Lizenzdatei bitten — ja oder nein?

**Messung:** Repo seit 2015 unverändert, ein Autor, 225 Zeilen, **keine
Lizenzdatei** (Zone ROT). Unabhängigkeit gegen `xdftool` ist gemessen
(MF-693): eigenständiges C++, kein gemeinsamer Unterbau — die zweite
Hand für ADF trägt.

**Folge bei Ja:** wahrscheinlich Zone ROT → GRÜN, dann sogar vendorbar;
`uft_amigados` bekäme seine zweite **registrierte** Hand.
**Bei Nein:** Weg (2) aus `ORAK-1` bleibt — lokales, nicht
weitergebbares Oracle nach `dtc`-Präzedenz. Beide Wege sind gangbar;
(1) ist nur besser.

### 3 · EUPL-1.2-Zeile bestätigen

**Frage:** Steht EUPL-1.2 als Zeile in der Lizenz-Matrix?

**Messung:** `LIZ-3` hat den Fall entschieden (MF-679); die
gwnbd-Konzepte (NAK-Resync, REDWC-Messung) sind eingeplant und in
MF-686 teils gelandet. Offen ist nur die **Verallgemeinerung**.

**Folge bei Ja:** der nächste EUPL-Fund ist Routine statt Einzelfall.
**Bei Nein:** jeder Fall wird wieder einzeln verhandelt.

### Warum diese drei zusammen stehen

Sie sind der Beleg für die Regel, die mit ihnen festgeschrieben wurde
(`CLAUDE.md` §*Der stärkste legale Kanal*): **kein Fund verlässt die
Pipeline ungenutzt.** Was heute keinen Kanal hat, wartet **benannt** —
mit dem, was ihn öffnen würde. Alle drei sind genau solche Öffner.

---

## LIZ-4 — „allen Code mit Lizenzproblem nachbauen": was die Messung daraus macht (MF-697)
<!-- status: offen -->

> **Teil A entschieden und vollzogen (MF-698): die GPL-3-Bindung ist**
> **angenommen, gelöscht wird nichts.** Was daraus folgte, steht am
> Ende dieses Eintrags unter *Vollzug A*. Teil B bleibt offen.

**Kennzahl:** fünfte (**Dateien mit ungeklärter Herkunft**, Befund-Stufe).
Der Auftrag lautete, allen lizenzproblematischen Code sauber neu zu
implementieren. Die Messung trägt das in dieser Form **nicht** — sie
trägt etwas Kleineres und etwas Größeres.

### Was die Messung widerlegt

**1 · UFT steht unter GPL-2.0-*or-later*** (`LICENSE`, `CONTRIBUTING.md`
§Licensing). Die drei AIR-Ports nennen **GPL-3.0**. Unter „or later" ist
das **kompatibel**: die verteilbare Kombination wird GPL-3. Das ist eine
Bindung und eine Entscheidung — **keine Rechtsverletzung**. Ein Nachbau
löst die Bindung, ist aber nicht nötig, um legal zu bleiben.

**2 · Drei der vier tragen keine Fähigkeit.** Mit dem Tür-Sucher
gemessen (`tools/uft-innendienst`):

| Datei | Zeilen | Lizenz im Kopf | Exporte ohne Tür |
|---|---|---|---|
| `src/formats/stx/uft_stx_air.c` | 914 | GPL-3.0 | **4 von 4 WAISE** |
| `src/formats/kfx/uft_kfstream_air.c` | 908 | GPL-3.0 | 2 WAISE + 2 nur eigenes Verzeichnis |
| `src/formats/amiga/uft_amiga_protection.c` | 766 | **keine** | **16 WAISE + 1 nur Test** |
| `src/formats/ipf/uft_ipf_air.c` | 975 | GPL-3.0 | 2 WAISE + 7 nur eigenes Verzeichnis — **aber das Plugin ruft vier Funktionen: IPF-Lesen hängt daran** |

Diese drei nachzubauen hieße **2588 Zeilen** zu schreiben, die niemand
ruft. Nach Regel 9 bewegt das keine Kennzahl; es wäre
`ANGEBOT_OHNE_ABNEHMER` im Großformat. Der vorgesehene Weg steht schon
in `QUARANTINE.md`: *Clean-Room-Neubau, **wenn ein Baustein ihn
verlangt***.

### Was die Messung stattdessen findet — und das ist der eigentliche Defekt

**Keine der vier trägt einen SPDX-Kopf, und nichts hat das bemerkt.**
`CONTRIBUTING.md` sagt seit MF-580: *„Ported or adapted code keeps its
origin's licence and names it."* `audit_spdx_policy.py` prüfte bis
MF-697 nur, ob **vorhandene** SPDX-Kennungen in der Politik stehen — es
verlangte keine. Die Regel war unbewacht, und zwar genau bei den
Dateien, die sie am nötigsten haben.

Seit MF-697 listet das Tor die Klasse: **6 Port-Erklärungen im Kopf, 4
ohne SPDX.** Bewusst als Liste, nicht als Tor — eine Attribution ist
nichts Verbotenes, sondern etwas Entscheidungsbedürftiges (MF-636).

**Zwei davon sind seither erledigt**, weil sie keine Entscheidung
brauchten: `src/core/uft_interleave.c` und `src/core/uft_write_precomp.c`
sind Ports aus **a8rawconv**, dessen Lizenz am vendorten Baum gemessen
ist (`src/a8rawconv/a8rawconv.cpp:5-7` — *„version 2 … or (at your
option) any later version"*), also **GPL-2.0-or-later** wie UFT selbst.
SPDX gesetzt, Sache geschlossen. Die Lizenz stammt aus der **Quelle**,
nicht aus dem Kopfkommentar — der behauptete sie nur.

### Die zwei Entscheidungen, die bleiben

**A · Die drei AIR-Dateien (GPL-3.0).** `GPL-3.0` steht **nicht** in
`ERLAUBT` (`scripts/audit_spdx_policy.py:44-52`). Einen SPDX-Kopf zu
setzen, ohne die Politik zu ändern, macht das Tor rot und blockiert
jeden Commit; die Politik zu ändern, **ist** die Annahme der
GPL-3-Bindung. Beides ist eine Lizenzentscheidung und darum nicht meine
(MF-679). Drei Wege:

1. **Bindung annehmen** — `GPL-3.0` in `ERLAUBT`, SPDX in die drei
   Dateien, `LICENSE`-Frage für die Distribution klären. Kostet nichts
   an Fähigkeit, bindet die Kombination an GPL-3.
2. **Die zwei ohne Abnehmer löschen** (`stx_air`, `kfstream_air`, 1822
   Zeilen) und nur für `ipf_air` Weg 1 gehen. Nach der sechsstufigen
   Löschkette aus MF-369.
3. **Nachbau** — nur für `ipf_air` sinnvoll, und der hängt ohnehin an
   der capsimg-Entscheidung (siehe *Drei Eigentümer-Handgriffe*, Nr. 1).

**Empfehlung: 2.** Sie entfernt 1822 Zeilen GPL-3-Bindung, die nichts
tragen, und lässt die Entscheidung auf die eine Datei zusammenschmelzen,
die eine Fähigkeit hat.

**B · `uft_amiga_protection.c` — der einzige echte Lizenzmangel.**
766 Zeilen, „C99 port of XCopy Pro (1989-2011) 68000 Assembly
algorithms", **keine Lizenz genannt** — und XCopy Pro hat keine, die
jemand gemessen hätte. Gemessen: 16 von 17 Exporten ohne Tür, der
siebzehnte nur von einem Test gerufen. **Fähigkeit: keine.**

Das ist der Fall, der weder Bindung noch Nachbau braucht, sondern
**Löschung** — die Verwaisten-Regel und die Lizenzfrage zeigen zum
selben Ergebnis, und der Nachbau hätte keinen Abnehmer. Auch hier gilt
die sechsstufige Kette aus MF-369.

### Was daraus für den Auftrag folgt

„Allen Code mit Lizenzproblem sauber neu implementieren" wird nach
dieser Messung zu:

* **2 Dateien** — erledigt, SPDX gesetzt (a8rawconv, kompatibel).
* **3 Dateien, 2588 Zeilen** — *nicht* nachbauen, sondern löschen oder
  binden; sie tragen nichts. Eigentümer-Entscheidung A und B.
* **1 Datei** — `ipf_air`, die einzige mit Fähigkeit. Ihr Weg hängt an
  capsimg und steht bereits auf der Liste.
* **1 Tor** — die Regel, die das alles hätte verhindern sollen, steht
  seit MF-697 unter Beobachtung.

Kein Fund verfällt dabei: die drei AIR-Dateien bleiben als **Spec-Quelle
und Oracle-Kandidat** im Fundus, falls ein Baustein die Fähigkeit später
verlangt — genau die Kanal-Regel aus `CLAUDE.md` §*Der stärkste legale
Kanal*.

### Vollzug A (MF-698): Bindung angenommen, nichts gelöscht

Der Eigentümer hat **Weg 1** gewählt und ausdrücklich ergänzt: *nichts
löschen*. Damit bleiben alle vier Dateien im Baum, und die Folgen sind
zu **sagen**, nicht nur zu tragen.

**1 · SPDX gesetzt, konservativ gelesen.** Die drei AIR-Ports tragen
jetzt `SPDX-License-Identifier: GPL-3.0-only`. Warum `-only` und nicht
`-or-later`: der Quellkopf nennt „GPL-3.0" ohne Zusatz. `-or-later`
wäre eine Behauptung über Rechte, die niemand gemessen hat; `-only`
irrt, wenn überhaupt, zu unseren Lasten. Der Grund steht in jedem der
drei Köpfe, nicht nur hier.

**2 · Politik erweitert — als Einzelfall, nicht als Freibrief.**
`GPL-3.0-only` steht in `ERLAUBT` (`scripts/audit_spdx_policy.py`), mit
dem ausdrücklichen Vermerk: die Vorgabe für **eigenen** Code bleibt
`GPL-2.0-or-later`, und wer eine weitere GPL-3-Quelle aufnimmt, trifft
dieselbe Entscheidung erneut.

**3 · Die Bindung steht jetzt, wo ein Verteiler sie liest.** `README.md`
§License sagte „GPL-2.0 — see LICENSE". Das war ab dem Moment der
Annahme **unvollständig**: die verteilbare Kombination ist GPL-3.0. Eine
Bindung, die niemand lesen kann, ist keine — das ist derselbe Fehler wie
eine Lizenz, die nur als Kommentarsatz existiert (P0-5). README nennt
jetzt die drei Dateien, die Folge für Weiterverteiler und den
Entscheidungsanker.

**4 · Eine Berichtigung an `CONTRIBUTING.md`.** Dort stand: *„Not
permitted without an owner decision: GPL-3.0, AGPL, Apache-2.0 and
BSD-4-Clause — none of them combine with GPL-2.0."* Der Nachsatz war
**zu grob**: GPL-3 kombiniert sehr wohl mit GPL-2-**or-later**, und
genau diese Tür hat der `-or-later`-Halbsatz zwei Absätze weiter oben
offengehalten. Die Regel nennt jetzt den Unterschied.

**Gemessen nach dem Vollzug:** `audit_spdx_policy.py` — 6
Port-Erklärungen, **5 mit SPDX**, „SPDX außerhalb der Politik: 0".

### Was offen bleibt: LIZ-4 B

`src/formats/amiga/uft_amiga_protection.c` (766 Z., „C99 port of XCopy
Pro (1989-2011) 68000 Assembly algorithms") ist die **einzige** Datei
mit einer Port-Erklärung und **ohne jede Lizenzangabe** — weder im Kopf
noch in einer messbaren Quelle. Sie ist damit von der README-Aussage
oben ausdrücklich **nicht gedeckt**.

Da nichts gelöscht wird, bleibt sie im Baum, und ihr SPDX-Kopf bleibt
leer: einen zu setzen hieße, eine Lizenz zu erfinden. Das ist die
Fehlerklasse, die dieser Baum als P0-5 bezahlt hat (`SPDX: MIT` auf
einem GPLv2+-Port).

**Weg 3 ist seit MF-699 vollzogen — als Zwischenstand, nicht als
Ende.** Die Datei ist aus dem Verteilpaket (`UnifiedFloppyTool.pro`
auskommentiert, Eintrag in `verify_build_sources.py:
`NOT_BUILT_BY_DESIGN` mit benanntem Ende), bleibt aber im Baum und wird
von ihrem Test weiter gebaut. Die ausgelieferte Fassung trägt damit
keine ungeklärte Herkunft mehr; der Rückweg bleibt messbar.

**Die Reihenfolge ist Eigentümer-Vorgabe (MF-699): erst der
funktionierende Nachbau, dann die Löschung.** Sie steht jetzt als Regel
in `docs/QUARANTINE_PROCESS.md` §5, zusammen mit der Registerpflicht.
Der Grund ist messbar: eine gelöschte Vorlage ist als **Messpunkt** weg
— der Nachbau braucht sie nicht als Quelltext (die Brandmauer verbietet
das ohnehin), wohl aber als Verhaltensreferenz für Blackbox-Läufe.

Die zwei Enden dieses Zwischenstands, eines muss eintreten:

1. **XCopy-Pro-Lizenz messen** — sie ist bisher nirgends belegt. Fällt
   sie auf etwas Kombinierbares, ist die Sache mechanisch erledigt.
2. **Als `LicenseRef-` führen** mit dem, was tatsächlich bekannt ist —
   ehrlich, aber es bleibt eine Datei ungeklärter Herkunft im
   Verteilpaket.
3. ~~**Vom Verteilpaket ausnehmen**~~ — **vollzogen MF-699.** Die
   Fähigkeit ist ohnehin keine: 16 von 17 Exporten ohne Tür, der
   siebzehnte nur von einem Test gerufen.

**Kennzahl:** fünfte, Befund-Stufe. Weg 3 bewegt sie ohne Verlust; Weg 1
löst sie ganz.

---

## FMT-16 — `86f` verfehlt die Spezifikation an vier Stellen und kündigt es als „SUPPORTED" an (MF-707)
<!-- status: wartet-eigentuemer(2026-08-30) -->

**Kennzahl:** **ungeprüfte Formate (T3) ↓**, sobald die Neufassung
steht. Der Rotbeweis allein hebt nichts — T2 verlangt eine autoritative
Referenz-**Implementierung**, und `fftool` ist mangels `cargo` nicht
baubar.

Der zweite T3-Posten aus Scout-Block 4 war als **Differenzlauf** geplant.
Der ist gesperrt, beide Blocker nachgemessen: `cargo` fehlt auf dieser
Maschine (kein `fftool`), und der Korpus hat **0 von 24** 86F-Abbildern.
Also der andere Weg — gegen die **veröffentlichte Spezifikation**.

### Die Quelle

`docs/dev/formats/86f.rst` aus dem **eigenen Dokumentations-Repositorium
von 86Box** (github.com/86Box/docs, abgerufen 2026-08-30). Keine
Sekundärquelle, keine Rückentwicklung: die Beschreibung durch den
Urheber des Formats.

```
00000000: Magic 4 bytes ("86BF")
00000004: Minor version (0C)
00000005: Major version (02)
00000006: Disk flags (16-bit)
00000008: Offsets of tracks
```

### Vier Widersprüche, gemessen

| Stelle | `uft_86f_plugin.c` | Spezifikation |
|---|---|---|
| Magic | `"86BX"` (`:20`) | **`"86BF"`** |
| Kopfgröße | 32 (`:22`) | **8** |
| Byte 6/7/8 | `disk_type` / `sides` / `tracks` (`:66-68`) | Disk flags (16 bit), dann **Beginn der Spur-Offset-Tabelle** |
| Spurtabelle | ab 32, Einträge 12 Byte mit `offset(4)+length(4)+flags(1)+sectors(1)+rpm(2)` (`:105-107`) | ab 8, Einträge sind **32-Bit-Offsets** |
| Geometrie | CHS aus einem „disk type byte" (`:32-43`) | 86F speichert FM-/MFM-**Transitionen**; einen solchen Typ gibt es nicht |

Keine dieser Annahmen ist aus der Spezifikation herleitbar. Die
12-Byte-Eintragsstruktur mit benannten Unterfeldern trägt die Signatur
aus **FMT-2/3/10/11/12**: plausibel aussehend und erfunden.

### Die praktische Folge

Weil die Probe auf `"86BX"` besteht, **weist sie jede echte 86F-Datei
ab** — gemessen in `tests/test_86f_spec_conformance.c`: Spec-Kopf →
NEIN, erfundener Kopf → JA mit **Konfidenz 98**. Genau umgekehrt.

Dabei kündigt `uft_format_plugin_86f` an: `Read SUPPORTED`, `Write
SUPPORTED`, `Flux SUPPORTED`, dazu `CAP_READ | CAP_WRITE | CAP_FLUX |
CAP_VERIFY`, und `.write_track` ist verdrahtet. Das ist „Bestand, nicht
Fähigkeit" (P0-2) in der unangenehmsten Form: nicht bloß unerreichbar,
sondern **angekündigt**.

### Warum kein Ein-Zeilen-Fix

Das Magic zu berichtigen wäre eine Zeile — und **schlimmer als der
jetzige Zustand**. Heute lehnt das Plugin echte Dateien ab; mit richtigem
Magic nähme es sie an und zerlegte sie mit Kopf-Offset 32 statt 8 und
einer erfundenen Spurtabelle, bei verdrahtetem Schreibpfad. Aus
„wirkungslos" würde „nimmt an und zerlegt falsch".

Dieselbe Kopplung wie bei `hardsector` (MF-706): **erst die Struktur,
dann das Erkennungsmerkmal.**

### Drei Wege (Eigentümer)

1. **Neufassung gegen die Spezifikation** — Magic, Kopf, Spurtabelle,
   Transitionsmodell. Regelkonform unter der EINFRIER-REGEL: benannte
   Referenz (86Box-Doku), Rotbeweis liegt, Referenz gehört in den
   Header. Aufwand groß — 86F ist ein Oberflächenformat, kein
   Sektorformat, und der jetzige Leser ist als Sektorleser gebaut.
2. **Fähigkeit ehrlich zurücknehmen** — Features auf `UNSUPPORTED`,
   Capabilities entsprechend, Plugin bleibt als Platzhalter. Kostet
   nichts, macht die Registry wahr, und `docs/STAND.md` zeigt es sofort.
3. **Aus dem Verteilpaket** wie `uft_amiga_protection.c` (MF-699) —
   nicht löschen, nicht bauen, mit benanntem Ende.

**Empfehlung: 2 sofort, 1 als eigener Baustein.** Weg 2 ist eine
Ehrlichkeitskorrektur und in Minuten gemacht; Weg 1 braucht Fixture und
Oracle, also die Beschaffung, die dieser Posten ohnehin schon nennt.

### Nachtrag MF-708 — die zweite Quelle, und ein zweiter Leser

**Anlass:** der Eigentümer gab `Digitoxin1/DiskImageTool` zur Prüfung.
Das Repo war aus Scout-Block 4 bereits begutachtet
(`tools/uft-scout/out/DiskImageTool.gutachten.md`, Zone **GELB**,
GPL-3.0) — mit einem dort vermerkten Neubesuch-Anlass. Der ist jetzt
eingetreten.

**1 · Die Zwei-Quellen-Regel ist erfüllt.** Bis MF-707 ruhten die vier
Widersprüche auf **einer** Quelle (der 86Box-Spezifikation).
DiskImageTool bringt einen eigenständigen 86F-Leser (871 Zeilen,
VB.NET), der von diesem Baum und von jenem Spec-Text nichts weiß.
Gemessen im Klon:

```
86FImage.vb:8    Private Const FILE_SIGNATURE = "86BF"
86FImage.vb:348  _MinorVersion = Buffer(4)
86FImage.vb:349  _MajorVersion = Buffer(5)
86FImage.vb:350  _DiskFlags    = BitConverter.ToUInt16(Buffer, 6)
86FImage.vb:359  Dim Pos = 8                    ' Beginn der Tabelle
86FImage.vb:363  Offset = BitConverter.ToUInt32(Buffer, Pos)
```

Feld für Feld dieselbe Aussage wie die Spezifikation. Alle vier
Widersprüche stehen damit auf zwei unabhängigen Quellen.

**2 · Der Baum hat ZWEI 86F-Leser.** Beim Nachprüfen des
Tabellenbeginns bei Byte 8 fiel auf:

| | Spec + DiskImageTool | `86box/uft_86f_plugin.c` | `pc/uft_86f.c` |
|---|---|---|---|
| im Build | — | ja (`.pro:906`) | ja (`.pro:3249`) |
| **registriert** | — | **ja** | **nein** |
| Aufrufer | — | über Plugin-Zeiger | **keiner**, gemessen |
| Magic | `86BF` | `86BX` ✗ | `86BF` ✓ |
| Byte 4/5 | Minor / Major | — ✗ | Version LE16 ✓ |
| Byte 6 | Disk flags LE16 | `disk_type` u8 ✗ | Flags LE16 ✓ |
| Byte 8 | **Spur-Offset-Tabelle** | `tracks` u8 ✗ | `disk_type` u8 ✗ |
| Byte 9-13 | (Tabelle) | — | encoding/rpm/tracks/sides/bitcell ✗ |

Der Leser mit dem **richtigen** Erkennungsmerkmal ist der **ohne Tür**.
Er kommt acht Byte weit korrekt und erfindet dann einen verlängerten
Kopf aus sechs Feldern genau dort, wo die Offset-Tabelle beginnt —
dieselbe Fabrikationsklasse, einen Schritt später.

Gemessen im Rotbeweis (`tests/test_86f_spec_conformance.c`, Abschnitt
3b), beide Leser mit demselben Kopf:

```
registriertes Plugin:  "86BF" -> NEIN          |  "86BX" -> JA (98)
pc/uft_86f.c        :  "86BF" -> JA (97)       |  "86BX" -> NEIN
```

Eine vollständige Umkehrung. Die Registry hat den falschen gewählt.

**3 · Und wieder die Aufzählung statt der Messung.** Der Kopf von
`uft_86f_plugin.c` sagte bis MF-708, es trage die 86F-Unterstützung
„allein". MF-622 hatte **einen** unerreichbaren 86F-Leser gelöscht und
daraus geschlossen, es sei der letzte gewesen — statt zu messen, welche
Dateien 86F lesen. Der zweite stand die ganze Zeit im Build. **Elfter
belegter Fall** dieses Musters (MF-567/578/598/633/651/652/668/671/
678/703). Die Kopfzeile ist mit MF-708 berichtigt.

### Was das für die Entscheidung ändert

**Der Weg zu T2 ist damit NICHT frei** — und das ist gemessen, nicht
angenommen: DiskImageTool ist `OutputType=WinExe` auf .NET Framework
4.7.2, eine reine WinForms-Anwendung ohne `Sub Main` und ohne
Kommandozeile. Als **Oracle** taugt sie nicht (nicht automatisierbar,
und `dotnet` allein baut kein Framework-4.7.2-Projekt); als
**Verhaltens-Referenz zum Lesen** ist sie belegt und hat hier ihren
Dienst getan. Kanal: **Spec**, nicht Port, nicht Oracle.

**Die Empfehlung wird dadurch eindeutiger, nicht anders.** Weg 2
(Merkmale auf UNSUPPORTED, bis eine Neufassung steht) war schon vorher
die Empfehlung; jetzt steht sie auf zwei Quellen statt einer. Neu
hinzu kommt eine **zweite Frage an den Eigentümer**:

> `src/formats/pc/uft_86f.c` — 477 Zeilen, gebaut, unregistriert, ohne
> Aufrufer, mit richtigem Magic und falschem Kopfaufbau ab Byte 8.
> Anker setzen, Tür geben oder zurückziehen? (Klasse ORPH-5 /
> Verwaisten-Regel.)

Eine Tür zu geben wäre **falsch**, solange der Kopfaufbau ab Byte 8
erfunden ist: aus „weist alles ab" würde „nimmt an und zerlegt falsch".
Dieselbe Kopplung wie bei `hardsector` (MF-706) — erst die Struktur,
dann das Erkennungsmerkmal.

**Was die Neufassung braucht, wenn sie kommt:** sie folgt der
86Box-Spezifikation. DiskImageTool ist GPL-3.0; gelesen wurden
**Tatsachen** (welches Byte welche Bedeutung trägt), kein Ausdruck,
und übernommen wurde nichts. Wer die Neufassung schreibt, arbeitet aus
der Spezifikation — die dafür vollständig ist.

## FS-3 — die PC-Seite: Leser echt, Schreiber elf Attrappen, Tür auf dem Nebengleis (MF-709)

<!-- status: offen -->

**Kennzahl:** **ungeprüfte Dateisystem-Leser ↓** (`uft_fat12` steht auf
**FS-T1** — zirkulär) und mittelbar **T3 ↓** (`img` ist T3).

**Anlass:** der Eigentümer legte die Fähigkeitsliste von
`Digitoxin1/DiskImageTool` vor, mit der Frage „haben wir das auch, sind
wir besser?". Die Antwort verlangte eine Messung der eigenen PC-Seite.
Sie fiel anders aus, als die Export-Liste vermuten ließ — und die erste
Fassung dieser Antwort saß darauf auf. Berichtigt nach Blick in die
Rümpfe.

### Was gemessen wurde

| | Befund | Quelle |
|---|---|---|
| FAT-**Lese**pfade | echt — `uft_fat_open` 21 Z., `read_dir` 49, `extract` 43, `get_chain` 37 | `src/fs/uft_fat12.c` |
| FAT-**Schreib**pfade | **11 von 12 sind ehrliche `UFT_FAT_ERR_READONLY`-Attrappen** (3-5 Zeilen): `inject`, `inject_from_file`, `inject_path`, `mkdir`, `rmdir`, `delete`, `rename`, `set_label`, `set_attr`, `save`, `write_cluster` | ebd. |
| Erreichbarkeit | **57 von 59 Funktionen ohne Aufrufer** außerhalb der eigenen Datei | grep über `src/` + `tests/` |
| Test-Tiefe | `test_fatfs` fasst **genau eine** Funktion an: `uft_fat12_detect()` | `tests/test_fatfs.c` |
| Stufe | **FS-T1** — alle Eingaben selbst gebaut, gegen den eigenen Erzeuger geprüft | `docs/VERIFICATION_TIERS_FS.md` |
| Korpus | **0 von 10** Dateien sind PC-/FAT-Abbilder | `tests/corpus/` |

**Und die Tür führt aufs Nebengleis.** `src/explorertab.cpp` ruft nicht
den 854-Zeilen-Leser, sondern `src/formats/uft_fat12_legacy.c` — **98
Zeilen**, dessen Schreibpfade `-1` zurückgeben. Der Kopf jener Datei ist
dabei vorbildlich ehrlich: er benennt sich als Übergang, nennt seinen
einzigen Aufrufer und die Bedingung für die eigene Löschung. Kein
Vorwurf an sie — der Befund ist, dass der Übergang seit Commit `db6897e`
so steht.

Das ist **dieselbe Form wie MF-708, einen Stock tiefer**: die fähigere
Implementierung hat keine Tür, die Tür führt zur dünneren. Zweiter Fall
an einem Tag.

### Was daraus folgt

**Nicht:** „FAT-Schreiben implementieren". Das wäre neuer Code im
FS-Layer ohne benannte Referenz — die EINFRIER-REGEL trifft es, und zwar
zu Recht: ein Schreiber lässt sich nicht rotbeweisen, solange kein fremd
erzeugtes Abbild existiert, gegen das man liest.

**Sondern, in dieser Reihenfolge:**

1. **Beschaffung** — ein FAT12-Abbild von fremder Hand. Bewegt
   `uft_fat12` FS-T1 → FS-T1b sofort, und mit Register-Eintrag der
   erzeugenden Hand → FS-T2. Gemessen: `mtools` / `mkfs.fat` sind auf
   dieser Maschine **nicht** vorhanden; der Kanal ist also
   **Daten/Fixture** (frei verteilte DOS-Diskettenabbilder) oder eine
   registrierbare Werkzeug-Hand. Dasselbe Abbild gibt `img` (T3) seine
   erste echte Prüfvorlage.
2. **Erst danach** ist über Schreibpfade zu reden — mit dem geprüften
   Leser als Gegenprobe.

### Was DiskImageTool dazu beiträgt

Nichts Portierbares (GPL-3.0, Zone GELB), und als Oracle taugt es nicht
(WinForms, keine Kommandozeile — gemessen, MF-708). Sein Beitrag ist
**die Frage**: es belegt, dass genau diese Fähigkeit auf dieser
Formatfamilie erwartet wird. Kanal: **Spec/Anregung**.

## ORAK-2 — `to_woz2`: das erste Oracle, das baut, läuft und dessen Ausgabe wir erkennen (MF-711)

<!-- status: offen -->

**Kennzahl:** **ungeprüfte Formate (T3) ↓** — Voraussetzung für die
Hebungen von `do`, `po` und `d13`; mittelbar **Wandlungspfade ↑**
(erster Apple-Eintrag der Rundlauf-Matrix).

**Anlass:** der Eigentümer gab `cmosher01/DskToWoz2` und
`cmosher01/Apple-II-Disk-Tools` zur Prüfung. Gutachten:
`tools/uft-scout/out/{DskToWoz2,Apple-II-Disk-Tools}.gutachten.md`.
Die Zone `?` von `apple2-disk-tools` ist dabei per Messung aufgelöst —
beide Lizenzdateien sind wörtlicher GPL-3.0-Text → **GELB** (kein Port;
Spec und Oracle zulässig).

### Warum das mehr ist als ein weiteres Gutachten

Diesem Baum fehlen Oracles chronisch, und die letzten drei Anläufe sind
je an einer anderen Hürde gescheitert:

| Kandidat | scheiterte an |
|---|---|
| `fftool` (fluxfox) | kein `cargo` auf dieser Maschine |
| DiskImageTool | WinForms, keine Kommandozeile (MF-708) |
| `cpmls`, `hxcfe`, `samdisk`, `lsatr` | nicht installiert |

`to_woz2` scheitert an keiner. **Gemessen, in dieser Reihenfolge:**

1. **Es baut.** MinGW gcc 13.1.0, eine Zeile, rc=0, 0 Warnungen
   (Scout-Messung).
2. **Es läuft.** `to_woz2.exe` ohne Argumente gibt seinen
   Gebrauchstext; 16-Sektor, 13-Sektor und Blank erzeugen je ein
   252 416-Byte-Abbild (eigene Gegenprüfung).
3. **Es ist deterministisch.** Drei Läufe — zwei vom Scout, einer
   unabhängig danach — liefern **byteidentische** SHA-256
   (`0015aa1e2024717…`). Ein Oracle, das bei jedem Lauf etwas anderes
   sagt, ist keins.
4. **Sein Kopf stimmt.** `WOZ2 FF 0A 0D 0A` — die Signatur der
   WOZ-2.0-Spezifikation (applesaucefdc.com), Byte für Byte.
5. **Und wir erkennen seine Ausgabe.** Der Scout ließ das ausdrücklich
   als UNGEKLÄRT stehen; die Wegwerf-Messung gegen
   `uft_format_plugin_woz.probe()` beantwortet es:

```
out_a.woz  (16-Sektor)  Kopf WOZ2 FF 0A 0D 0A  ->  probe: JA (Konfidenz 98)
out13.woz  (13-Sektor)  Kopf WOZ2 FF 0A 0D 0A  ->  probe: JA (Konfidenz 98)
blank.woz  (35 leer)    Kopf WOZ2 FF 0A 0D 0A  ->  probe: JA (Konfidenz 98)
```

**Was dieses JA NICHT heißt:** dass wir richtig dekodieren. Es heißt,
dass die Tür aufgeht. Der Beweis für T2 ist der Differenzlauf —
Sektorinhalte byteweise zurück — und der steht aus.

### Was zu tun ist, in dieser Reihenfolge

1. **`to_woz2` in `docs/ORACLES.md` registrieren**, nach dem Muster von
   `dtc`: nur Ausführung, kein Weitergeben; gepinnt über Quell-Commit
   `639dc1c` + Bau-Rezept + Binary-SHA-256. Es hat **kein**
   `--version` — die Pinnung muss das tragen. Rotbeweis: der
   Fünf-Fragen-Prüfstand, mit benanntem Skip, wenn das Binary fehlt.
2. **`do` T3 → T2** per Differenzlauf: Muster-DSK →(`to_woz2`)→ WOZ
   →(UFT-Leser + 6-and-2-Decode)→ byteweiser Sektorvergleich.
   Rotbeweis **zuerst**, und er muss den heute grün-falschen Fall
   enthalten: `prodos_po_do.c:71` entscheidet DO gegen PO **an einem
   Buchstaben der Dateiendung**. Ein PO-Inhalt mit `.dsk`-Endung wird
   still falsch gelesen — dieselbe Klasse wie FMT-15.
3. **`d13` T3 → ?** über den 13-Sektor-Pfad. `d13` hat heute
   **keinen einzigen Test** (`VERIFICATION_TIERS.md:63`). Das erste
   fremd erzeugte WOZ geht dabei in den Korpus — dieselbe Bewegung wie
   `dim_atari` (MF-690).
4. **Erst danach** `DO→WOZ` als erster Apple-Eintrag der
   Rundlauf-Matrix. Braucht einen 6-and-2-**Encoder**, den der Baum
   gemessen nicht hat (`uft_nib_parser_v2.c:13` sagt selbst
   „decoding"). Der EINFRIER-konforme Weg ist benannt: Referenzen
   (WOZ-2.0-Spec, *Beneath Apple DOS*) im Header, `to_woz2` als
   Differenz-Oracle, Rotbeweis `DO→WOZ→DO` bitidentisch **vor** dem
   Code.

### Grenzen, gemessen statt angenommen

* **Kein Port.** GPL-3.0, Zone GELB. Kanal ist **Oracle** (Ausführung
  frei, Weitergabe nicht) und **Spec**.
* `to_woz2` kann **kein `.po`**; der Kopf-CRC bleibt 0 (spec-legal).
* Der Scout meldet einen **1-Byte-Heap-Überlauf** in `parse_filename`
  (`to_woz2.c:367-369`). Für ein Oracle, das wir mit selbst gewählten
  Dateinamen füttern, ist das tragbar — es gehört aber in den
  Registry-Eintrag, nicht in eine Fußnote.
* Die CiderPress-Kette (`nibblize`, „Based on code by Andy McFadden",
  BSD-3) ist eine **zweistufige** Attribution und in beiden Gutachten
  ausgewiesen. Sie betrifft uns nur, falls je portiert würde — was
  Zone GELB ausschließt.

**Fundus ohne Auftrag** (benannt, nicht verfallen): das
META-Provenienz-Muster aus DskToWoz2 (Quell-CRC im Abbild) und
`a2catalog` als DOS-3.3-Katalog-**Generator**.

### Nachtrag MF-712 — Schritt 1 erledigt, mit zwei Berichtigungen

`to_woz2` ist registriert: `tests/differential/oracles.py` (9. Eintrag),
`docs/ORACLES.md` mit eigenem Abschnitt, Eichung in
`tests/differential/test_oracles.py`. Registry-Selbsttest grün,
ctest 287/287.

**Berichtigung 1 — die Stufe.** Oben stand „`do` T3 → **T2**". Die
Leiter aus `scripts/gen_verification_tiers.py` sagt etwas anderes:
T1b ist das *Fremdwerkzeug-Abbild*, T2 die *Spec-Verifikation des
Byte-Layouts*. `to_woz2` liefert ein Fremdwerkzeug-Abbild, also
**T1b** — und T1b steht auf dieser Leiter **über** T2
(`order = {"T1": 0, "T1b": 1, "T2": 2, "T3": 3}`). Die Korrektur ist
also eine Verbesserung, keine Rücknahme.

**Berichtigung 2 — der Anker ist nicht der Binärhash.** Der Vorschlag
oben nannte „Binary-SHA". Gemessen: zwei unabhängige Baue aus demselben
Quellstand `639dc1c` ergaben **verschiedene** Binärhashes
(`434cfbda…` / `4dbb8def…`) und **byteidentische** Ausgabe
(`0015aa1e…` / `a5ff575f…`). Ein Binärhash hätte das Oracle nach jedem
Neubau zu einem anderen gemacht. Zitierfähig ist **Quellstand +
Baurezept + Ausgabe-SHA**; der Manifest-Hash bleibt eine *Bau*-Angabe.

**Der Überlauf ist real und feuert.** Der Scout meldete ihn als
Randnotiz. Gemessen: mit einem **absoluten** Pfad bricht `to_woz2` mit
`0xC0000374` (STATUS_HEAP_CORRUPTION) ab — die erste Fassung des
Eich-Tests hat ihn ausgelöst. Benutzungsregel jetzt am Eintrag: **immer
aus dem Arbeitsverzeichnis mit relativen Namen rufen.**

**Und ein Test, der nicht scheitern konnte.** `test_a_missing_tool_
gives_an_incomplete_manifest_entry` kannte den Fall
`version_is_unaskable` nicht und kippte, sobald zum ersten Mal
überhaupt ein Oracle auflöste. Er war nie aufgefallen, weil auf dieser
Maschine **keines** liegt — der Zweig `path is not None` wurde nie
betreten. Dieselbe Klasse wie die 32 Testdateien aus MF-596. Berichtigt.

**Offen bleiben** die Schritte 2–4 (Hebung `do`, `d13`, dann der
Wandlungspfad). Und die fünfte Frage steht ehrlich im Eintrag: eine
zweite unabhängige Hand für WOZ 2.0 gibt es hier nicht — der Abgleich
stützt sich auf die Spezifikation, nicht auf ein zweites Werkzeug.

## FMT-17 — `do` und `po` entscheiden ohne hinzusehen (MF-713)

<!-- status: wartet-eigentuemer(2026-08-30) -->

**Kennzahl:** **ungeprüfte Formate (T3) ↓** — `do` und `po` stehen beide
auf T3. Der Rotbeweis hebt nichts; er macht aus einer stillen
Falschaussage eine gemessene.

**Anlass:** Schritt 2 aus ORAK-2 (`do` heben). Vor dem Differenzlauf
stand die Messung — und die traf eine andere, größere Sache.

### Der Befund

Ein Apple-II-Abbild mit 35 × 16 × 256 ist **143 360 Byte** groß, ob die
Sektoren in **DOS-** oder in **ProDOS-Reihenfolge** liegen. Die
Reihenfolge entscheidet, wo jedes Byte landet; die Größe verrät sie
nicht. Beide Sonden entscheiden trotzdem allein an ihr:

```c
src/formats/do/uft_do.c   do_probe(...) { (void)d; (void)s;
                            if (fs == DO_SIZE) { *c = 60; return true; } }
src/formats/po/uft_po.c   po_probe(...) { (void)d; (void)s;
                            if (fs == PO_SIZE) { *c = 55; return true; } }
```

`DO_SIZE` und `PO_SIZE` sind **beide 143360**. Der Inhalt wird
ausdrücklich weggeworfen. Gemessen
(`tests/test_do_po_probe_ignores_content.c`):

```
Abbild A (DOS 3.3 im Inhalt)    do: JA (60)   po: JA (55)
Abbild B (ProDOS im Inhalt)     do: JA (60)   po: JA (55)
143360 Byte Nullen              do: JA (60)
```

Zwei Abbilder, die der Inhalt eindeutig trennt, bekommen **dasselbe**
Urteil. Der Gleichstand wird an zwei fest verdrahteten Zahlen gebrochen:
**60 > 55**. `do` gewinnt immer — ein ProDOS-geordnetes Abbild wird
still in DOS-Reihenfolge gelesen, jeder Sektor am falschen Platz, ohne
Warnung.

Das ist die FMT-15-Klasse, hier verschärft: **zwei** Plugins auf
**denselben** Bytes, entschieden von einer Konstanten.

### Dass es auch anders geht, ist belegt

In DOS-Reihenfolge liegt die VTOC auf Spur 17, Sektor 0 — Versatz
`0x11000`. Zwei Bytes sind für DOS 3.3 festgelegt:

| Versatz | Bedeutung | Wert |
|---|---|---|
| `0x11001` | Katalog-Spur | `0x11` (17) |
| `0x11027` | max. Spur/Sektor-Paare | `0x7A` (122) |

**Quelle 1:** *Beneath Apple DOS* (Worth/Lechner), VTOC-Aufbau.
**Quelle 2, unabhängig:** `cmosher01/DskToWoz2`, `conversion.cpp:47-62`
prüft **genau diese beiden Versätze auf genau diese beiden Werte** (und
für 13 Sektoren `0x0DD01`/`0x0DD27` = 17 × 13 × 256). Übernommen wurden
Tatsachen über ein Format, kein Ausdruck — GPL-3.0, Zone GELB, kein Port.

**Zwei Bytes hätten gereicht.** Keines wird gelesen.

### Die Eigentümer-Entscheidung

Eine inhaltsbasierte Unterscheidung ändert, **welches Plugin eine Datei
beansprucht** — Verhalten an der Registry-Tür, kein Tagesrand. Sie
gehört in einen eigenen Schritt, mit diesem Rotbeweis als Grundlage;
dieselbe Ordnung wie bei `hardsector` (MF-706) und `86f` (MF-707/708).

Was dafür spricht, es zu tun: die Referenz ist benannt und
zweitbestätigt, der Rotbeweis steht, und die Änderung ist klein.
Was zu bedenken ist: eine Datei, die heute `do` beansprucht, könnte
danach `po` beanspruchen — für Bestandsabbilder ohne VTOC an der
DOS-Stelle ist das die *Korrektur*, aber es ist eine Verhaltensänderung.

### Nicht Gegenstand dieses Eintrags

`src/formats/apple/prodos_po_do.c:71` entscheidet die Reihenfolge am
**ersten Buchstaben der Dateiendung** (`ext[1]=='d'`) — schlimmer als
die Größe: ein `.dsk` wird immer DOS, ein `.img` immer ProDOS.
Gemessen hat die Datei aber **0 Aufrufer**, **keine** Plugin-Struktur
und **keinen** Registry-Eintrag; der Fehler ist dort **latent**. Beide
Türen wurden geprüft — Symbol *und* Funktionszeiger, die Lehre aus
MF-706. Sie gehört in die ORPH-5-Klasse (Anker / Tür / Rückzug).

### Was das für ORAK-2 Schritt 2 heißt

Der geplante Differenzlauf (WOZ → dekodieren → Sektoren vergleichen)
ist **heute nicht fahrbar**: der Baum hat gemessen **keinen
6-and-2-Dekoder** (`grep` über `src/` und `include/` findet Apple-GCR
nur als CRC-Tabellen-Treffer). Das ist eigener Bau im Decoder-Layer und
fällt unter die EINFRIER-REGEL — erlaubt nur mit benannter Referenz,
Rotbeweis zuerst und Referenz im Header. Dieser Eintrag liefert das
erste Drittel davon.

### Berichtigung MF-714 — die Prämisse von FMT-17 war falsch

**Der Auftrag lautete „FMT-17 umsetzen". Beim Umsetzen hat sich die
Begründung als falsch erwiesen — und der Baum hatte recht.**

Der Eintrag oben behauptete, zwei Bytes hätten genügt: die VTOC auf
Spur 17 Sektor 0 (`0x11001` = `0x11`, `0x11027` = `0x7A`). Beim Anfassen
der Sonde kam heraus:

**1 · Der Baum sagte es bereits.** `src/formats/do/uft_do.c` trägt seit
**MF-463** die Analyse im Kopf, und sie ist richtig: DO und PO
unterscheiden sich **nur in den Sektoren 1..14**; Sektor 0 und 15 liegen
in beiden Ordnungen gleich. Die VTOC *ist* Sektor 0 — sie steht bei DO
und bei PO auf demselben Versatz und sagt „DOS-3.3-Diskette", nicht
„DOS-Reihenfolge". Unabhängig nachgemessen an `a8rawconv`,
`diska2.cpp:3-9`:

```
DOS:    0, 13, 11, 9, 7, 5, 3, 1, 14, 12, 10, 8, 6, 4, 2, 15
ProDOS: 0,  2,  4, 6, 8, 10, 12, 14, 1, 3, 5, 7, 9, 11, 13, 15
         ^                                                  ^
         beide stimmen genau an 0 und 15 überein
```

**2 · Und der angeführte Beleg belegte etwas anderes.**
`DskToWoz2/conversion.cpp:47-62` heißt `readDos()` und liefert die
Zeichenkette „DOS 3.x" — es prüft die **DOS-Ausgabe**, nicht die
**Reihenfolge**. Zwei Tatsachen verwechselt.

**3 · Die richtige Zweitreferenz stand schon im Kopf von `uft_do.c`:**
SAMdisk löst es genauso und sagt es offen — `src/samdisk/do.cpp:5-13`,
`ReadDO()` ist mit „not used" gekennzeichnet und fällt auf Größe **plus
Dateiendung** zurück. Ich habe eine vorhandene, korrekte Analyse
übergangen, statt sie zu lesen.

### Was von FMT-17 bleibt — und es ist weniger

Der Befund schrumpft, verschwindet aber nicht. Gemessen bleibt:

* Beide Sonden nehmen **jede** 143 360-Byte-Datei an — auch 143 360
  Nullbytes — mit voller Konfidenz.
* Der Gleichstand fällt an zwei fest verdrahteten Zahlen (60 > 55).
* `do` gewinnt damit **immer**, auch wenn im Puffer positive Belege für
  die andere Ordnung lägen.

Der letzte Punkt ist der einzige, den MF-463 **nicht** betrachtet hat,
und er ist echt: das **ProDOS-Datenträgerverzeichnis** liegt in Block 2,
also auf Versatz **`0x400`** — mitten im Sondenpuffer. In DOS-Reihenfolge
steht dort etwas anderes (ProDOS-Block 2 landet über physischen Sektor
8/10 in der DOS-Datei nicht auf `0x400`). Ein Fund dort wäre ein
**positiver Beleg für PO** — die eine Richtung, die im Puffer erreichbar
ist.

### Warum es trotzdem noch kein Code ist

**Die zweite unabhängige Quelle fehlt.** Für den Aufbau des
Datenträgerverzeichnisses (Speichertyp `0xF` im oberen Nibble bei
`0x404`, Vorgänger-Zeiger `0x0000`, Nachfolger `0x0003`) liegt in diesem
Baum bislang **eine** Quelle vor. Die Zwei-Quellen-Regel ist nicht
erfüllt, und ohne sie wäre es genau die plausibel aussehende Annahme,
die FMT-2/3/10/11/12 erzeugt hat. Beschaffung: ProDOS-8-Referenz oder
ein zweites Werkzeug, das Block 2 prüft — der Scout hat den Auftrag
noch nicht.

**Offen bleibt zusätzlich die Frage, ob es überhaupt gewollt ist.**
MF-463 nennt „zwei Plugins mit fast gleicher Konfidenz auf denselben
Bytes" ausdrücklich *die ehrliche Antwort*. Eine PO-Bevorzugung bei
gefundenem Verzeichnis wäre eine Verhaltensänderung an der Registry-Tür
— das bleibt eine Eigentümer-Entscheidung, jetzt nur mit richtiger
Begründung statt mit falscher.

**Kennzahl:** unverändert. Diese Runde hat nichts gehoben; sie hat eine
falsche Aussage aus dem Baum genommen.

## GCR-1 — der 6-and-2-Dekoder steht, 560 von 560 Sektoren belegt (MF-715)

<!-- status: erledigt(MF-715) -->

**Kennzahl:** Voraussetzung für **T3 ↓** bei `do`, `po`, `d13`, `nib`,
`woz` — und für **Wandlungspfade ↑** (Apple hat bis heute **null**
Einträge in der Rundlauf-Matrix). Die Hebung selbst steht noch aus;
dieser Eintrag schafft ihre Grundlage.

**Der Ausgangsbefund:** Der Baum konnte keinen einzigen Apple-Sektor aus
einem Bitstrom zurückholen. `grep` über `src/` und `include/` fand
Apple-GCR nur als CRC-Tabellen-Treffer. WOZ, NIB und A2R waren Container
ohne Inhalt.

### Die Messung stand vor dem Code

Die EINFRIER-REGEL erlaubt neuen Decoder-Code nur mit benannter Referenz
**und** Rotbeweis bzw. Messung **vor** dem Code. Beides ist erfüllt:

**Zuerst die Tabelle geprüft, blackbox.** Die 64 Diskettenbytes der
6-and-2-Kodierung wurden nicht aus dem Gedächtnis übernommen, sondern
gegen die **Ausgabe** des registrierten Oracles `to_woz2` gehalten —
ohne eine Zeile fremden Quelltext zu lesen:

```
Spur 0 eines to_woz2-WOZ : 50 624 Bits, 6224 Nibbles
Adressfelder (D5 AA 96)  : 16
Datenfelder  (D5 AA AD)  : 16
Datenbytes geprüft       : 5488  (16 × 343)
nicht in der Tabelle     : 0
```

Ein einziges fremdes Nibble hätte die Tabelle widerlegt, bevor sie Code
wurde. Es gab keines.

**Dann der Differenzlauf über die ganze Diskette:**

```
Eingabe  : 143 360-Byte-DSK, deterministisches Füllmuster
Weg      : to_woz2 → WOZ 2.0 → woz_get_track_525()
           → uft_apple_gcr_scan_track() → Vergleich gegen die DSK
Ergebnis : 560 von 560 Sektoren gefunden
           Adress-Prüfsumme  560/560
           Datenfeld dekodiert und byteidentisch  560/560
```

**Und eine unabhängige Bestätigung, die niemand eingebaut hat.** Aus dem
Vergleich fiel die Zuordnung physisch→logisch heraus:

```
0, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 15
```

Das ist genau die Umkehrung der DOS-3.3-Interleave-Tabelle aus
`a8rawconv` (`diska2.cpp:3-5`) — gemessen, nicht angenommen, und aus
einer Quelle, die der Dekoder nicht kennt.

### Was gebaut wurde

`include/uft/formats/apple/uft_apple_gcr.h` + `src/formats/apple/uft_apple_gcr.c`:

| Funktion | tut |
|---|---|
| `uft_apple_gcr_denibblize_6_2()` | 343 Diskettenbytes → 256 Nutzbytes, mit laufender XOR-Prüfsumme |
| `uft_apple_gcr_decode_4_4()` | „odd-even" des Adressfelds |
| `uft_apple_gcr_scan_track()` | Spur als **Ring** absuchen, Adress-/Datenfeld-Paare |

Referenzen im Header: *Beneath Apple DOS* (Worth/Lechner) Kapitel 3 für
das Verfahren, WOZ-2.0-Spezifikation für den Bitstrom, plus die
Oracle-Messung oben.

**Zwei Entscheidungen, die im Code begründet stehen:**

1. **Bei abgewiesenem Feld bleibt das Ziel unberührt.** Ein halb
   dekodierter Sektor wäre eine stille Veränderung
   (`DESIGN_PRINCIPLES`). Der Test prüft das ausdrücklich.
2. **Genau eine Umdrehung.** Die erste Fassung las
   `bit_count + 8×400` und lieferte **595** statt 560 Sektoren — 17 je
   Spur, weil der erste hinter der Naht ein zweites Mal kam. Alle 595
   waren korrekt dekodiert; die Zahl war trotzdem falsch. Eine stille
   Doppelung ist schlimmer als eine Lücke: sie sieht aus wie ein Fund.

### Was der Dekoder ausdrücklich NICHT tut

Er ordnet nichts um. `scan_track()` liefert die **physischen**
Sektornummern aus dem Adressfeld. Ob daraus DOS- oder ProDOS-Reihenfolge
wird, entscheidet der Aufrufer — die beiden Ordnungen sind eine
Eigenschaft der **Datei**, nicht der Spur (MF-463, MF-714).

### Was jetzt möglich wird

* **ORAK-2 Schritt 2** (`do` → T1b): der Differenzlauf ist gefahren, es
  fehlt die Verankerung als Korpus-Eintrag mit Manifest.
* **ORAK-2 Schritt 3** (`d13`): der 13-Sektor-Weg braucht die
  5-and-3-Tabelle — `to_woz2` erzeugt sie bereits (`probe13.d13` →
  `neu13.woz`, SHA `a5ff575f…`), der Dekoder kennt sie noch nicht.
* **Ein Apple-Wandlungspfad**: erst mit einem *Encoder*; der Dekoder ist
  die eine Hälfte.

**Was NICHT belegt ist:** dass echte, historische Disketten gelesen
werden. Gemessen ist eine von `to_woz2` erzeugte, fehlerfreie
Aufzeichnung. Schwache Bits, Halbspuren, Kopierschutz und beschädigte
Felder sind hier nicht vorgekommen.

### ORAK-2 Schritt 2 erledigt (MF-716) — `do` steht auf T2, T3 faellt 52 -> 51

**Zum ersten Mal in dieser Kette bewegt sich eine Release-Kennzahl.**

**Und der Weg war ein anderer als gedacht.** Ich hatte MF-711 dahin
korrigiert, `to_woz2` liefere T1b. Das gilt fuer `woz` — nicht fuer
`do`: der Generator ordnet Korpus-Eintraege je **Plugin-Symbol** zu, und
`to_woz2` *erzeugt* WOZ, *verbraucht* DSK. Ein Fremdwerkzeug-Abbild fuer
`do` gibt es also nicht. Der Weg ist `T2` ueber
`docs/spec_verification.json` — genau, was der Scout urspruenglich sagte.
Zweite Korrektur an mir selbst in dieser Sache; der Scout lag beide Male
richtig.

**Der Differenzlauf lief durch das Plugin, nicht nur durch einen Puffer.**
Das war der Punkt, an dem ein blosser JSON-Eintrag zu wenig gewesen
waere:

```
linke Seite : uft_format_plugin_do liest die DSK
              (Versatz (cyl*16+s)*256, Sektor-ID = logisch s)
rechte Seite: dieselbe Diskette, to_woz2 -> WOZ 2.0
              -> uft_apple_gcr_scan_track() -> physische Sektoren
Verbindung  : DOS-3.3-Interleave, a8rawconv diska2.cpp:3-5

rechte Seite dekodiert : 560
verglichen             : 560
byteidentisch          : 560
fehlend                :   0
```

Waere der Versatz falsch, die Nummerierung anders oder die Tabelle
verkehrt herum, koennte das nicht aufgehen.

**Regressionsschutz:** `tests/test_do_layout_verified.c` haelt die
gepruefte Anordnung fest und braucht kein Fremdwerkzeug — faellt der
Versatz um, ist der Differenzlauf hinfaellig, und das faellt auf.

**Nebenbefund: die README trug drei verschiedene T3-Zahlen** (56, 55,
52) an drei Stellen. Das Tor „inventory drift" hat beim Heben genau eine
davon gefunden; die anderen zwei kamen beim Nachsehen. Alle drei stehen
jetzt auf dem abgeleiteten Wert.

**Was ausdruecklich NICHT gehoben ist:** `po` bleibt T3 — der
Differenzlauf lief ueber die DOS-Ordnung, die ProDOS-Ordnung war nicht
beteiligt. Und belegt ist eine fehlerfreie, maschinell erzeugte
Aufzeichnung, keine historische Diskette.

**Offen bleibt Schritt 3** (`d13`, heute ohne einen einzigen Test): er
braucht die 5-and-3-Tabelle im Dekoder. `to_woz2` erzeugt sie bereits
(`neu13.woz`, SHA `a5ff575f...`).

## GCR-2 — `d13`: 5-and-3, der Beweisweg steht bereits offen (MF-717)

<!-- status: offen -->

**Kennzahl:** **ungeprüfte Formate (T3) ↓**. `d13` hat heute **keinen
einzigen Test** (`docs/VERIFICATION_TIERS.md`) und ist damit der
billigste noch offene Posten der Apple-Reihe.

**Warum es jetzt dran ist:** ORAK-2 Schritt 2 (MF-716) hat den Weg
vollständig ausgetreten. Für `d13` muss nichts Neues erfunden werden —
dieselben vier Teile, ein Tabellenwechsel:

| Teil | Stand für `do` (MF-716) | Stand für `d13` |
|---|---|---|
| Oracle | `to_woz2`, registriert | **dasselbe** — es liest `.d13` |
| Fremd erzeugte Vorlage | `neu16.woz` | **`neu13.woz` liegt vor**, SHA `a5ff575f7f82592c80e6…` |
| Dekoder | 6-and-2 ✓ | **5-and-3 fehlt** — das ist die ganze Arbeit |
| Brücke logisch↔physisch | DOS-3.3-Interleave | 13-Sektor-Interleave, noch zu belegen |

### Was zu tun ist, in dieser Reihenfolge

1. **Die 5-and-3-Tabelle blackbox prüfen**, genau wie bei 6-and-2
   (MF-715): Nibbles aus `neu13.woz` ziehen und gegen die
   Kandidaten-Tabelle halten. Ein fremdes Nibble widerlegt sie, **bevor**
   sie Code wird. Erwartet werden 13 Adress- und 13 Datenfelder je Spur.
2. **`uft_apple_gcr_denibblize_5_3()`** dazu, nach demselben Muster:
   bei abgewiesenem Feld bleibt das Ziel unberührt.
3. **Differenzlauf durch das Plugin**, wie MF-716: linke Seite der
   `d13`-Leser, rechte Seite `to_woz2` → WOZ → Dekoder. Erwartet:
   35 × 13 = **455** Sektoren byteidentisch.
4. **`spec_verification.json`-Eintrag** + Regressionstest, dann steigt
   die Stufe.

### Was dabei zu beachten ist

* **13 Sektoren, nicht 16.** Die Dateigröße ist `116 480` (= 35 × 13 ×
  256), und `DskToWoz2/conversion.cpp` prüft die VTOC entsprechend bei
  `0x0DD00` (= 17 × 13 × 256) statt `0x11000`. Zwei unabhängige Quellen
  für die Geometrie liegen damit vor.
* **5-and-3 ist nicht 6-and-2 mit anderer Tabelle.** Es packt 5 Datenbits
  je Diskettenbyte; ein Sektor braucht **410** statt 342 Nibbles. Die
  Zahl ist zu **messen**, nicht anzunehmen — sie fällt bei Schritt 1 ab.
* **Die EINFRIER-REGEL gilt.** Erlaubt ist das hier, weil es eine
  **Hebung** ist und kein neues Plugin: benannte Referenz (*Beneath
  Apple DOS*), Messung vor dem Code, Referenz im Header.

**Was das NICHT wird:** ein Beleg, dass echte 13-Sektor-Disketten
gelesen werden. Wie bei `do` ist die Vorlage maschinell erzeugt und
fehlerfrei.

### Vormessung erledigt (MF-719) — die 5-and-3-Tabelle ist vollstaendig gemessen

Schritt 1 aus der Liste oben ist gefahren, blackbox gegen die **Ausgabe**
von `to_woz2`, ohne eine Zeile fremden Quelltext. Vier Befunde, drei
davon widerlegen eine Annahme dieses Eintrags:

**1 · Der Adress-Vorspann ist ein anderer.** 13-Sektor benutzt
**`D5 AA B5`**, nicht `D5 AA 96`. Gemessen an Spur 0 von `neu13.woz`:
13 × `D5 AA B5`, 13 × `D5 AA AD`, **null** `D5 AA 96`. Der Abtaster aus
MF-715 fände auf einer 13-Sektor-Spur also **keinen einzigen** Sektor —
und würde dabei nicht scheitern, sondern schweigend 0 melden.

**2 · Die Nutzlänge ist 411, nicht 410.** Der Eintrag oben schrieb
„**410** statt 342" und mahnte, die Zahl sei zu messen. Sie ist es
jetzt: vom Vorspann bis zum Epilog `DE AA` liegen **exakt 411** Nibbles,
bei allen 13 Sektoren und bei beiden Prüfvorlagen. 410 Datenbytes plus
**ein** Prüfbyte — dieselbe Bauform wie bei 6-and-2 (342 + 1 = 343).

**3 · Die Tabelle hat 32 Einträge, und alle 32 sind belegt.** Der erste
Lauf mit dem deterministischen Füllmuster löste nur **24** verschiedene
Datenbytes aus — eine Teilmenge, die eine Tabelle bestätigen, aber nicht
vervollständigen kann. Mit einer Vorlage, die alle 256 Bytewerte enthält
(`voll13.d13`, 116 480 Byte, 256 verschiedene Werte), kamen **32 von 32**:

```
AB AD AE AF B5 B6 B7 BA BB BD BE BF D6 D7 DA DB
DD DE DF EA EB ED EE EF F5 F6 F7 FA FB FD FE FF
```

Das ist dieselbe Methode wie bei 6-and-2 (MF-715) — nur dass sie hier
nicht nur bestätigt, sondern die Tabelle **erzeugt**. Kein fremder Code
gelesen, kein Gedächtnis bemüht.

**4 · Der Abstand Adressfeld → Datenfeld ist 19 Nibbles**, bei allen 13
Sektoren gleich.

### Was davon in den Code muss

| | Wert | Herkunft |
|---|---|---|
| Adress-Vorspann | `D5 AA B5` | gemessen |
| Daten-Vorspann | `D5 AA AD` | gemessen, wie 16-Sektor |
| Epilog | `DE AA` | gemessen |
| Nutzlänge | **411** (410 + 1 Prüfbyte) | gemessen |
| Alphabet | 32 Werte, s. o. | gemessen, vollständig |
| Sektoren je Spur | 13 | gemessen |

**Offen bleibt genau eines:** die Zuordnung logisch↔physisch für 13
Sektoren. Sie fällt bei 6-and-2 aus dem Differenzlauf heraus (MF-715)
und wird das hier genauso tun — aber sie braucht wie dort eine
**zweite, unabhängige** Quelle, bevor sie als belegt gilt. Der
Beschaffungsauftrag steht beim Streif-Scout.

**Der Aufwand sinkt damit von „mittel" auf „klein":** die
Verhaltens-Spec ist vollständig, es fehlt die Umsetzung plus der
Differenzlauf (erwartet 35 × 13 = **455** Sektoren byteidentisch).

### Beschaffung erfüllt (MF-720) — die 13-Sektor-Zuordnung ist die Identität

Der Streif-Scout hat den Auftrag aus MF-719 erledigt. Damit ist die
Verhaltens-Spec für `d13` **vollständig**; die Hebung kann beginnen.

**Zwei unabhängige Quellen, plus eine Nachprüfung von mir:**

| | Aussage | Fundstelle |
|---|---|---|
| Quelle 1 | `logical_sector_index(int physical) { return physical; }` | `mamedev/mame@c0d3677674` `src/lib/formats/ap2_dsk.cpp:410`, BSD-3 |
| Quelle 2 | „13-sector floppies use physical sector order" | ciderpress2.com/formatdoc/Unadorned-notes.html |
| Nachprüfung | dieselbe Datei, Zeile 410 gelesen; 16-Sektor steht bei `:666` und benutzt `dos_skewing[]`/`prodos_skewing[]` | lokal gemessen |

Der Kontrast ist der eigentliche Beleg: **16-Sektor benutzt Tabellen,
13-Sektor gibt `physical` unverändert zurück.** Die Identität ist
Absicht, kein Versehen.

**Der Scheinwiderspruch, den der Scout aufgelöst hat.** `to_woz2` führt
eine Skew-10-Tabelle (`to_woz2.c:145-151`), MAME sagt Identität. Beides
stimmt: der Skew bestimmt, *an welcher Winkelposition* ein Sektor auf
der synthetisierten Spur landet — Adressfeld **und** Dateiversatz laufen
durch **dieselbe** Tabelle (`to_woz2.c:164-169` und `:287-290`).
Adressfeld-Sektor `s` trägt also die Daten von Datei-Sektor `s`. In
MAMEs `load()` steht dasselbe Muster: `int sector = (i*10) % 13` für die
Platzierung, und `sdata = sector_data + 256 * sector` für die Daten —
derselbe `sector` auf beiden Seiten.

**Und MAME bestätigt die Blackbox-Messung aus MF-719 Byte für Byte:**
`raw_w(…, 0xd5aab5)` Adress-Vorspann, `0xd5aaad` Daten-Vorspann,
`0xdeaaeb` Epilog, `translate5[nval ^ pval]` als laufende XOR über eine
32-Werte-Tabelle. Dritte unabhängige Übereinstimmung, aus einer Quelle,
die die Messung nicht kannte.

**Damit steht in GCR-2 nichts mehr offen außer der Umsetzung.**

## FMT-17 — die zweite Quelle liegt vor (MF-720)

<!-- status: offen -->

**Kennzahl:** **T3 runter** (`po`).

Die Zwei-Quellen-Regel für das ProDOS-Datenträgerverzeichnis ist
erfüllt. `mamedev/mame@c0d3677674` `src/lib/formats/fs_prodos.cpp:269-271`
(Dateikopf `license:BSD-3-Clause`) schreibt in Block 2 genau die drei
Werte, die MF-714 als unbelegt zurückgestellt hatte:

```
w16l(0x00, 0x0000)      Rückzeiger
w16l(0x02, 0x0003)      Vorzeiger
w8  (0x04, 0xf0 | len)  Speichertyp 0xF im oberen Nibble
```

Das ist eine **Referenz-Implementierung**, unabhängig von der ersten
Quelle im Baum. Die Sperre aus MF-714 („ohne zweite Quelle wäre es genau
die plausible Annahme, die FMT-2/3/10/11/12 erzeugt hat") ist damit
aufgehoben.

**Was jetzt zu tun ist:** `po_probe()` darf bei gefundenem Verzeichnis
auf `0x400` einen positiven Beleg werten. **Es bleibt eine
Verhaltensänderung an der Registry-Tür** — welches Plugin eine Datei
beansprucht, ändert sich. Die Eigentümer-Entscheidung steht also weiter
aus, jetzt aber mit vollständiger Begründung statt mit einer falschen
(MF-713) oder einer halben (MF-714).

## FMT-18 — sechs Formate ohne jedes fremde Gegenstück (MF-720)

<!-- status: offen -->

**Kennzahl:** **T3 runter** — aber die Frage geht tiefer.

Die Abdeckungskarte des Streif-Scouts (51 T3-Formate gegen 45 gesichtete
Repos **und** floptools 154 Formate) findet für sechs Formate **nirgends**
ein Gegenstück:

```
edk · posix · syn · t1k · tan · xdm86
```

Kein Werkzeug, keine Spec, kein Abbild. Für die übrigen 45 gibt es
mindestens einen Kandidaten.

**Die Frage, die sich daraus stellt, ist nicht „wo finden wir eine
Referenz", sondern:** *woher haben diese sechs Plugins ihre Layouts?*
Das ist wörtlich die Frageform, die FMT-2/3/10/11/12 aufgedeckt hat —
fünf Parser, gegen erfundene Spezifikationen gebaut und grün getestet.

**Zu tun, bevor irgendetwas anderes an diesen sechs geschieht:** je
Plugin den Header und die Commit-Geschichte lesen und feststellen, ob
eine benannte Referenz existiert. Findet sich keine, gehören sie in
dieselbe Behandlung wie `86f` (MF-707/708): messen, was sie
tatsächlich tun, und die Ankündigung in `features` auf den belegten
Stand bringen.

`posix` ist dabei vermutlich harmlos (Sammelname, kein historisches
Format) — das ist zu prüfen, nicht anzunehmen.

### Umsetzung erledigt (MF-721) — 454 von 455, und der eine ist benannt

Der 5-and-3-Dekoder steht: `uft_apple_gcr_denibblize_5_3()`, und der
Abtaster erkennt den 13-Sektor-Vorspann `D5 AA B5` selbst — der Aufrufer
muss die Kodierung nicht mitgeben.

**Der Differenzlauf gegen `to_woz2`:**

```
Eingabe : 116 480-Byte-D13 mit allen 256 Bytewerten
gefunden           : 455 von 455
Adress-Prüfsumme   : 455
byteidentisch      : 454
andere Kodierung   :   1  (benannt, siehe unten)
```

Die Zuordnung physisch↔logisch fiel wieder aus dem Lauf heraus und ist
die **Identität** — dritte Bestätigung von MF-720 (MAME, CiderPress),
diesmal aus dem eigenen Messaufbau. Die Abtast-Reihenfolge ist
`0, 10, 7, 4, 1, 11, 8, 5, 2, 12, 9, 6, 3` = `(i·10) mod 13`, genau
MAMEs Skew.

### Der eine Sektor — und warum er der wichtigste ist

**Spur 0, Sektor 0 trägt eine andere 5-and-3-Variante.** DOS 3.2
schreibt den Bootsektor anders als die übrigen 454. Im Oracle steht das
wörtlich:

```c
deduce_encoding(dos33, track, sector) {
    if (dos33)            return ENC_62;
    if (track || sector)  return ENC_53;
    return ENC_53A;                 // genau T0S0
}
```

Die beiden Kodierer sind verschiedene Dateien (`nibblize_5_3.c` 244
Zeilen, `nibblize_5_3_alt.c` 271).

**Und die Prüfsumme trennt sie nicht.** Beide benutzen dieselbe Tabelle
und dieselbe laufende XOR. Der erste Entwurf hat den Bootsektor darum
mit `data_checksum_ok = true` und **255 von 256 falschen Bytes**
zurückgegeben — ein Inhalt, der nirgends auf der Diskette stand. Das ist
wörtlich „stille Veränderung" und „erfundene Daten"
(`DESIGN_PRINCIPLES`), und es wäre durchgegangen: der Sektor sah aus wie
jeder andere gelesene.

Seit MF-721 meldet die Einheit stattdessen `alt_encoding`, lässt `data`
**unberührt** und setzt `data_checksum_ok = false`. Ein Feld wurde
gefunden, es ist nur nicht lesbar — das ist etwas anderes als „defekt"
und etwas anderes als „gelesen".

**Wie der Fall gefunden wurde**, weil die Methode wiederverwendbar ist:
455 gefunden, 454 identisch — die eine Abweichung ließ sich nicht mit
„Rundungsfehler" abtun. Der Reihe nach ausgeschlossen: kein Nahtproblem
(Drehen des Startpunkts ändert nichts), keine Verwechslung (der Inhalt
steht **nirgends** in der Datei), keine falsche Zuordnung (die anderen
zwölf stimmen). Erst danach der Blick in die Oracle-Quelle — und dort
stand es in vier Zeilen.

### Was jetzt noch fehlt, damit `d13` steigt

Der Dekoder ist fertig und belegt. Die **Hebung** braucht denselben
Schritt wie `do` (MF-716): einen `spec_verification.json`-Eintrag mit
diesem Differenzlauf als Beleg, plus einen Regressionstest über den
`d13`-Leser. Der Testkopf und `tests/test_apple_gcr_6and2.c`
Abschnitt 4b tragen die Belege bereits.

**Nicht belegt bleibt:** die ENC_53A-Variante selbst. Sie zu dekodieren
wäre eine eigene Aufgabe mit eigener Referenz — heute wird sie
**benannt**, nicht geraten.

### GCR-2 erledigt (MF-722) — `d13` steht auf T2, T3 faellt 51 auf 50

<!-- status: erledigt(MF-722) -->

**Zweite Hebung der Kette.** `d13` hatte vorher **keinen einzigen Test**
und ist damit der billigste Posten gewesen, der noch offen war.

**Differenzlauf durch das Plugin:**

```
links : uft_format_plugin_d13 liest die .d13
        (Versatz (cyl*13+s)*256, Sektor-ID = s)
rechts: dieselbe Diskette, to_woz2 -> WOZ 2.0
        -> uft_apple_gcr_scan_track() auf dem 5-and-3-Weg (MF-721)
Bruecke: Identitaet, zweifach belegt (MF-720)

rechte Seite dekodiert : 454  (+1 andere Kodierung)
verglichen             : 454
byteidentisch          : 454
```

Der 455. ist der Bootsektor — **benannt, nicht geraten** (MF-721).

### Eine Berichtigung an mir selbst

Beim Lesen von `d13_read_track()` fiel auf, dass es einen
unvollstaendigen Sektor mit `0xE5` fuellt und **ausgibt** — genau das,
was `uft_do.c` seit MF-463 mit Begruendung im Quelltext verbietet
(„invented data as if it had been read"). Derselbe Layer,
entgegengesetztes Verhalten: ich hielt das fuer einen scharfen Fehler
und wollte ihn beheben.

**Gemessen ist er es nicht.** `d13_open()` lehnt jede Datei ab, deren
Groesse nicht **genau** `D13_SIZE` ist — der Zweig ist durch die
Plugin-Tuer unerreichbar. Der Test haelt statt dessen die Pruefung fest,
die wirklich schuetzt: abgeschnitten **und** ein Byte zu lang werden
beide abgewiesen.

Dritte Wiederholung derselben Lehre in dieser Kette (`hardsector`
MF-706, `prodos_po_do` MF-713): **erst die Erreichbarkeit messen, dann
urteilen.** Sie sitzt offenbar noch nicht fest genug.

### Was offen bleibt

* **`po`** — braucht die Eigentuemer-Entscheidung aus FMT-17 (zweite
  Quelle liegt seit MF-720 vor).
* **Die ENC_53A-Variante** — eigene Aufgabe mit eigener Referenz.
* **`nib`, `2img`, `moof`, `a2r`** — die uebrigen Apple-Formate auf T3.
  Der Dekoder steht jetzt fuer beide Kodierungen; was fehlt, ist je ein
  Differenzlauf.

## GCR-3 — `nib` ist auf Unabhaengigkeit gesperrt, nicht auf Code (MF-723)

<!-- status: wartet-eigentuemer(2026-08-31) -->

**Kennzahl:** T3 runter — aber erst, wenn eine zweite Hand existiert.

**Anlass:** `nib` war als naechster Posten vorgesehen, mit der
Begruendung „kein neuer Code, nur ein Differenzlauf". Der Eigentuemer
verlangte, **vorher die fuenfte Frage zu klaeren** (MF-644): bei rohen
Nibbles ist die Gefahr besonders gross, dass Korpus-Erzeuger und Oracle
dieselbe Hand sind — und ein Rundlauf wird dabei fast zwangslaeufig
gruen, also unauffaellig falsch.

**Die Messung faellt schaerfer aus als die Sorge.** Es geht nicht um
dieselbe *Familie*, sondern um **dieselben Quelldateien**:

| | |
|---|---|
| `.nib` im Korpus | **0** — es gibt nichts, dessen Herkunft messbar waere |
| Erzeuger, verfuegbar | `a2nibblize` (Apple-II-Disk-Tools) |
| dessen Kodierer | `nibblize_6_2.c`, `nibblize_5_3.c` |
| unser Dekoder geeicht gegen | `to_woz2` — **dieselben** `nibblize_*.c` |
| zweite Hand | MAMEs `a2_nib_format` hat `load`, aber **kein `save`** |

Ein Differenzlauf `a2nibblize → unser Dekoder` pruefte damit **denselben
Code gegen sich selbst durch zwei Huellen**. Gruen waere er mit
Sicherheit; belegen wuerde er nichts.

**Was ihn oeffnen wuerde**, in dieser Reihenfolge nach Aufwand:

1. **MAMEs `a2_nib_format` als Leser bauen** (BSD-3, `ap2_dsk.cpp:821+`)
   — echte zweite Hand, aber MAME ist gross; ob sich die Datei mit
   vertretbarem Aufwand einzeln uebersetzen laesst, ist **nicht
   gemessen**.
2. **Ein `.nib` fremder Herkunft beschaffen** (Daten/Fixture-Kanal) —
   dann ist der Erzeuger unbekannt und `a2nibblize` faellt als Vergleich
   weg.
3. **Ein dritter Erzeuger** (CiderPress, AppleCommander) — beide nicht
   gemessen, beide nicht auf dieser Maschine.

**Was NICHT hilft:** unser eigener Encoder. Er existiert nicht, und
wenn er existierte, waere er die dritte Huelle um denselben Kern.

### Was das fuer die Reihenfolge heisst

Die geplante Folge war `nib → a2r → FMT-17 → 2img → moof`. `nib` faellt
vorerst heraus, und `a2r` steht vor derselben Frage (Erzeuger ist
Applesauce, hier nicht vorhanden). Was bleibt und **heute** geht:

**FMT-17 zuerst** — es braucht keine neue Vorlage, nur eine Inhaltsprobe
und eine ehrliche Kandidaten-Ausgabe.

### Und ein Befund, der dabei abfiel: `2img` ist schon jetzt betroffen

`src/formats/2img/uft_2img.c` liest das Sortierungs-Feld bei `0x0C` und
kennt drei Werte:

```c
#define IMG2_FMT_DOS    0   /* definiert, NIRGENDS benutzt */
#define IMG2_FMT_PRODOS 1   /* definiert, NIRGENDS benutzt */
#define IMG2_FMT_NIB    2   /* einzige Verwendung, :72     */
```

Der Leser trennt NIB vom Sektor-Abbild und behandelt **DOS und ProDOS
identisch**. Ein `.2mg`, das ProDOS-Sortierung deklariert, wird gelesen,
als spielte die Sortierung keine Rolle — dieselbe stille Falschaussage
wie FMT-17, eine Schicht hoeher, und sie laeuft bereits. `2img` gehoert
darum hinter FMT-17, nicht daneben.

### FMT-17 erledigt (MF-724) — Kandidaten statt Konstante

<!-- status: erledigt(MF-724) -->

Der Eigentuemer hat den Ausgang vorgegeben und er trifft: `.do` gegen
`.po` ist **kein Erkennungs-, sondern ein Mehrdeutigkeitsproblem**.
Beide Deutungen bleiben Kandidaten; wo der Inhalt es hergibt,
entscheidet er; wo nicht, wird der Gleichstand **gemeldet statt
geraten**.

**Gemessen, ueber die Registry:**

```
ohne Verzeichniskopf         Gewinner DO   Konf 55   tied 2   Bewerber 2
mit ProDOS-Verzeichniskopf   Gewinner PO   Konf 90   tied 1   Bewerber 2
```

Die zweite Zeile war vorher **unmoeglich**: `do` gewann mit 60 gegen 55,
immer, an einer Konstanten.

**Der Gleichstand ist der eigentliche Gewinn.** Vorher blieb
`uft_probe_ranking.tied` bei 1 — die Registry meldete Eindeutigkeit, wo
keine war. Jetzt wird sie 2, und `uft_smart_open()` reicht das als
`equally_ranked` weiter (`:422`) samt Warnung (`:430`). Der Kopf von
`uft_probe_ranking` sagt selbst, was das heisst: „der Gewinner steht
durch Registrierungsreihenfolge fest, nicht durch Evidenz." Genau das
war vorher der Fall — nur hat es niemand erfahren.

**Die Probe**, in `include/uft/formats/apple/uft_apple_order.h` mit
Begruendung und beiden Quellen:

```
0x400  Rueckzeiger   == 0x0000
0x402  Vorzeiger     == 0x0003
0x404  Speichertyp   oberes Nibble == 0xF
```

ProDOS-Block 2 liegt in einer `.po` auf `N*512` = **0x400**, mitten im
Sondenpuffer. In einer `.do` steht dort der DOS-logische Sektor 4;
ProDOS-Block 2 landet ueber die physischen Sektoren 8/10 auf 0xB00 und
0xA00. Ein gueltiger Verzeichniskopf auf 0x400 ist damit ein **positiver
Beleg fuer ProDOS-Anordnung**.

Quelle 1: ProDOS-8-Beschreibung. Quelle 2, unabhaengig:
`mamedev/mame@c0d3677674` `fs_prodos.cpp:269-271` (BSD-3-Clause),
beschafft in MF-720. Bis dahin war der Weg ausdruecklich gesperrt, weil
**eine** Quelle nicht genuegt (MF-714) — die Sperre hat gehalten und ist
regulaer gefallen.

**Nicht gebaut, mit Absicht:** ein Leser, der den Benutzer fragt. Ein
Leser, der fragt, haengt in CI. Er reicht `tied` weiter; die Oberflaeche
fragt, ein Skript bricht ab.

**Verhalten praktisch unveraendert:** bei Gleichstand gewinnt weiterhin
`do`, es steht in der Registry vorn. Geaendert hat sich die **Aussage
darueber**.

**Kennzahl:** keine bewegt. `po` bleibt T3 — die Sonde ist ehrlich
geworden, das Layout ist damit nicht geprueft. Dafuer braucht es einen
Differenzlauf wie bei `do` (MF-716), und der braucht ein
ProDOS-geordnetes Abbild fremder Hand.

**Naechster Posten:** `2img`. Es liest das Sortierungs-Feld bei `0x0C`,
benutzt aber nur `IMG2_FMT_NIB` — `DOS` und `PRODOS` sind definiert und
werden **identisch** behandelt (MF-723). Dieselbe Frage, eine Schicht
hoeher, und mit dieser Probe jetzt beantwortbar.

## FMT-19 — `2img` verwirft, was sein Kopf sagt (MF-725)

<!-- status: offen -->

**Kennzahl:** T3 runter (`2img`) — aber die offene Haelfte braucht einen
Kanal, den es noch nicht gibt.

Der Eigentuemer hatte `2img` hinter FMT-17 eingeordnet, weil sein Kopf
bei `0x0C` traegt, welche Sortierung innen liegt (0 = DOS, 1 = ProDOS,
2 = NIB) — und weil ein Leser dem Feld entweder blind glauben oder
dieselbe Mehrdeutigkeit aufloesen muss. Die Messung fiel schaerfer aus:
**der Leser tut keines von beidem.** Er liest das Feld und verwirft es.

### Erledigt in MF-725: der NIB-Fall

`IMG2_FMT_NIB` war der **einzige** benutzte Wert — und er wurde falsch
benutzt. `img2_open()` setzte auch dort `spt = 16`, `sector_size = 256`,
und `img2_read_track()` rechnete `cyl * 4096`. Eine NIB-Spur ist
**6656 Byte** gross (`mamedev/mame` `ap2_dsk.h:150`
`nibbles_per_track = 0x1a00`; `src/formats/nib/uft_nib.c:9`).

Gemessen im Rotbeweis: Spur 1 wurde bei Versatz **4160 statt 6720**
gelesen — mitten in Spur 0, erstes Byte `0xFF` statt `0x01`.

**Die zweite Folge wog schwerer:** die Bytes wurden als **Sektoren
0..15** ausgegeben. Rohe Nibbles sind keine Sektoren; sie sind
GCR-kodiert und tragen ihre Nummern in Adressfeldern. Das ist erfundene
Struktur — und sie waere auch bei richtiger Schrittweite falsch. Kein
Rechenfehler, sondern eine Bedeutungsverwechslung.

Seit MF-725 wird NIB **abgewiesen** (`UFT_ERROR_NOT_SUPPORTED`), mit
Begruendung im Quelltext. Nicht dekodiert, obwohl der Baum seit
MF-715/721 einen GCR-Dekoder hat: `nib` steht nach GCR-3 auf
Unabhaengigkeit gesperrt, ein Differenzlauf waere tautologisch.

### Offen: die Sortierung wird gelesen und weggeworfen

`IMG2_FMT_DOS` (0) und `IMG2_FMT_PRODOS` (1) sind definiert und
**nirgends benutzt**. `img2_read_track()` liest sequenziell und vergibt
`id.sector = s`.

Das ist nicht schlicht falsch: in beiden Faellen **ist** `s` die
logische Sektornummer — nur in verschiedenen Nummernkreisen. Falsch ist,
dass der Aufrufer nicht erfaehrt, **welcher**. Fuer ein
ProDOS-geordnetes Abbild sind alle Nummern ausser 0 und 15 anders
gemeint, als ein DOS-Leser sie versteht.

**Warum es hier nicht behoben wird:** dem Leser fehlt der Kanal.
`uft_disk_t` traegt Geometrie, aber keine Angabe zur Sektor-Nummerierung,
und `uft_track_t.sectors[].id` hat kein Feld dafuer. Das ist eine
Architekturfrage (wo steht, in welchem Nummernkreis eine Sektor-ID
gemeint ist?), kein Tagesrand — und sie betrifft nicht nur `2img`,
sondern jedes Format, das beide Ordnungen kennt.

**Was daran anschliesst:** FMT-17 hat fuer `do`/`po` gezeigt, wie man
Mehrdeutigkeit meldet statt sie zu raten (`tied`). Hier liegt der Fall
umgekehrt — die Datei **sagt** es, und niemand hoert zu. Der naechste
Schritt waere, die Aussage des Kopfes gegen die Inhaltsprobe aus
`uft_apple_order.h` zu halten: stimmen sie ueberein, ist die Sache klar;
widersprechen sie sich, ist **das** der Befund.

## ORPH-6 — MOOF und A2R: kein Zugang, aber die falsche Tuer geht auf (MF-726)

<!-- status: wartet-eigentuemer(2026-08-31) -->

**Kennzahl:** T3 runter, mittelbar — beide sind heute nicht einmal in
der Tier-Tabelle, weil `gen_verification_tiers.py` ueber
`uft_format_plugin_<sym>` zuordnet und es keines gibt.

**Anlass:** `moof` war der naechste Posten der Apple-Reihe. Vor dem
Differenzlauf stand die Erreichbarkeitsmessung — die Lehre aus
`hardsector` (MF-706), `prodos_po_do` (MF-713) und `d13` (MF-722).

### Was gemessen wurde

`CLAUDE.md` fuehrt acht Apple-Formate. **Sechs** tragen ein Plugin und
stehen in der Registry. **Zwei nicht:**

| Format | Zeilen | Plugin | Registry | Aufrufer |
|---|---|---|---|---|
| `moof` | 597 | keins | 0 | **1** — und das ist ein Test |
| `a2r` | 1111 | keins | 0 | **0** |

Beide werden gebaut. Der einzige `moof`-Aufrufer,
`tests/test_moof_roundtrip.c`, baut seine Struktur im Speicher und liest
sie zurueck: **unser Schreiber gegen unseren Leser**, ohne jede fremde
Referenz.

`a2r` hat zusaetzlich eine Besonderheit: der Baum baut **zwei**
Registries. In der aelteren (`src/formats/uft_format_registry.c`,
Format-ID-basiert) steht `a2r` mit Magic, Endung und `uft_a2r_probe()`.
Gemessen ruft diese Probe **niemand** ausserhalb ihrer eigenen Datei.
Ein Eintrag in einer Tabelle, die keiner liest, ist keine Tuer.

### Und dann kam es schlimmer als erwartet

Die Erwartung war „niemand beansprucht sie". Mit allen 137 registrierten
Plugins gemessen:

```
WOZ2 (Gegenprobe)   Bewerber 3   Gewinner WOZ (98)   Zweiter KFX (40)
MOOF                Bewerber 2   Gewinner KFX (40)   Zweiter XFD (25)
A2R2                Bewerber 2   Gewinner KFX (40)   Zweiter XFD (25)
A2R3                Bewerber 2   Gewinner KFX (40)   Zweiter XFD (25)
```

**`uft_disk_open()` uebergibt eine MOOF-Datei dem KryoFlux-Strom-Leser.**
Ein fehlender Zugang waere ehrlich; ein falscher ist es nicht.

### Die Ursache liegt nicht bei Apple — FMT-20

`kfx_probe()` (`src/formats/kfx/uft_kfx.c:57`) zaehlt Vorkommen des
Bytes `0x0D` in den ersten 512 Byte:

```c
if (oob_count >= 2) { *confidence = 80; return true; }
if (oob_count >= 1) { *confidence = 40; return true; }
```

**Ein einzelner Wagenruecklauf genuegt.** Gemessen: 512 Nullbytes mit
einem einzigen `0x0D` an beliebiger Stelle werden von KFX mit 40
beansprucht. Die Applesauce-Familie hat ihn im Kopf (`FF 0A 0D 0A`).

Betroffen ist **jedes** Format, dessen eigene Sonde unter 40 meldet oder
das gar kein Plugin hat. Das ist die FMT-15-Klasse in ihrer breitesten
Form: nicht „erkennt an der Groesse", sondern „erkennt an einem Byte,
das ueberall vorkommt".

**Warum hier nicht geschaerft wird:** die richtige Bedingung ist die
Struktur eines KryoFlux-OOB-Blocks (`0x0D`, Typbyte, 16-Bit-Groesse) —
und dafuer braucht es die Stream-Spezifikation als benannte Referenz.
`dtc` ist registriert, aber nicht installiert. Eine Sonde blind zu
verengen, waere derselbe Fehler in der anderen Richtung.

### Die Eigentuemer-Entscheidung

Fuer `moof` und `a2r` je eines: **Anker** (Plan benennen), **Tuer**
(registrieren) oder **Rueckzug**.

„Tuer" ist **nicht** die naheliegende Wahl: ein unerreichbarer Leser,
der erreichbar wird, ohne geprueft zu sein, ist genau die Lage von `86f`
(MF-707/708) — angekuendigt und falsch. Und geprueft werden koennen
beide heute nicht: MOOF-Vorlagen fehlen, A2R-Vorlagen erzeugt
Applesauce, das hier nicht vorhanden ist. Damit stehen sie neben `nib`
und `a2r` unter GCR-3.

Regressionsschutz: `tests/test_apple_moof_a2r_no_door.c` haelt den
gemessenen Stand fest — samt Gegenprobe, dass WOZ sehr wohl gewinnt
(ohne sie waere der Test auch bei leerer Registry gruen, MF-447).

## FMT-20 — `kfx_probe()` beansprucht jede Datei mit einem `0x0D` (MF-726)

<!-- status: offen -->

**Kennzahl:** keine unmittelbar — aber es verfaelscht die Zuordnung fuer
eine unbekannte Zahl von Formaten, und das ist schlechter als eine Zahl.

Siehe ORPH-6 fuer Messung und Begruendung. Kurz: `>= 1` Vorkommen von
`0x0D` in 512 Byte gibt Konfidenz 40. Gemessen an 512 Nullen mit einem
einzigen gesetzten Byte.

**Was es braucht:** die KryoFlux-Stream-Spezifikation als benannte
Referenz fuer den OOB-Blockaufbau. Ohne sie waere jede Verengung
geraten — dieselbe Fehlerklasse, nur enger statt breiter.

**Was zuerst zu messen waere:** wie viele der 137 Plugins eine Sonde
haben, die unter 40 meldet. Jedes davon verliert heute an KFX, sobald
irgendwo ein Wagenruecklauf steht.

### FMT-20 vermessen (MF-727) — und der Befund ist groesser als KFX

Der Eigentuemer wollte die Zahl, die niemand kennt: wie viele Plugins
verlieren an KFX? Sie ist da — und die Messung hat den Befund
**erweitert**.

**Statischer Zensus** ueber alle Plugin-Quellen aus `git ls-files`
(nicht aus einer Liste, CLAUDE.md §Dateimengen): von **82** Quellen mit
erkennbarer Konfidenz-Zuweisung melden **13** hoechstens 40 — sie
koennen KFX nie ueberbieten:

```
25  t1k          35  jv1          40  dsk_generic
30  edk          35  sam          40  jvc
30  tan          35  syn          40  korg
35  adf_arc      35  xdm86        40  pdp
                                  40  v9t9
```

**Fuenf davon — `t1k`, `edk`, `tan`, `syn`, `xdm86` — sind genau die
Formate, fuer die FMT-18 draussen kein Gegenstueck gefunden hat.** Die
beiden Befunde treffen dieselben Plugins: kein fremdes Werkzeug, keine
Spec, kein Abbild — und eine Sonde, die im Gedraenge untergeht.

### Die dynamische Messung korrigiert meine Erwartung

Ich hielt es fuer einen Zweikampf `v9t9` gegen KFX. Gemessen an einer
92 160-Byte-Datei (eine der drei Groessen aus `v9t9_probe`):

```
ohne 0x0D :  7 Bewerber,  5 gleichauf bei 40,  Sieger XFD
mit  0x0D :  8 Bewerber,  6 gleichauf bei 40,  Sieger XFD
```

**Der Sieger stand schon vorher durch die Registrierungsreihenfolge
fest.** KFX ist nicht die Ursache, sondern der sechste im Gedraenge.

Das 40er-Band ist ueberfuellt: mehrere kopflose Formate beanspruchen
dieselbe Groesse mit identischer Konfidenz. Das ist die
**FMT-15-Klasse**, und sie reicht weiter als FMT-20 — die
Wagenruecklauf-Sonde macht ein bestehendes Problem sichtbar, sie
verursacht es nicht.

### Was daraus folgt

**Nicht** „kfx_probe verengen und fertig". Zwei getrennte Aufgaben:

1. **FMT-20 (KFX):** die Schaerfung braucht den OOB-Blockaufbau aus der
   KryoFlux-Stream-Spezifikation als benannte Referenz. `dtc` ist
   registriert, aber nicht installiert. Blind verengen waere derselbe
   Fehler in der anderen Richtung — echte Stroeme fielen durch.
2. **Neu, und das ist der groessere:** fuenf Plugins gleichauf bei 40 auf
   derselben Groesse. Solange `uft_probe_ranking.tied` niemanden
   erreicht, sieht das aus wie eine Entscheidung. FMT-17 (MF-724) hat
   fuer `do`/`po` gezeigt, wie es geht — dieselbe Behandlung fuer das
   40er-Band waere die Verallgemeinerung.

**Was zuerst zu messen waere:** wie viele der 137 Plugins auf **jeder**
gaengigen Diskettengroesse gleichauf liegen. Die 92 160-Byte-Messung ist
ein Stichprobenwert; die Verteilung ueber alle Groessen kennt niemand.

Regressionsschutz: `tests/test_kfx_probe_overclaims.c`.

## FMT-21 — Konfidenz ohne Skala: 35 bis 85 für dieselbe Erkenntnis (MF-728)

<!-- status: wartet-eigentuemer(2026-08-31) -->

**Kennzahl:** keine unmittelbar. Es verfälscht die Format-Zuordnung für
eine unbekannte Zahl von Dateien — und das ist schlechter als eine Zahl,
weil niemand weiß, wie oft es zuschlägt.

**Anlass:** der Eigentümer bat, Ordnung in den Befund aus MF-727 zu
bringen (fünf Plugins gleichauf bei 40). Die Messung hat ihn verschoben:
der Gleichstand ist das kleinere Problem.

### Die Verteilung, gemessen

Über 25 gängige Diskettengrößen, jeweils mit einem Puffer aus lauter
Nullen:

* **4 von 25** Größen haben einen Gleichstand, der größte mit **fünf**
  Bewerbern (92 160 Byte).
* Bei den übrigen 21 gibt es einen eindeutigen Sieger — **aber die
  Eindeutigkeit ist ein Artefakt.**

### Der eigentliche Befund

Ein Puffer aus lauter Nullen trägt **keine Signatur**. Das Einzige, was
eine Sonde darin finden kann, ist die Dateigröße. Gemessen melden sie
darauf:

```
DIM  1474560  85     D71   349696  70
TRD   163840  82     ADF   901120  70
D81   819200  80     DO    143360  55
NIB   232960  80     PO    143360  55
MSX   184320  75     IMG   204800  40
D64   174848  75     V9T9   92160  40
D13   116480  75     JV1   102400  35
```

**35 bis 85 für exakt dieselbe Erkenntnis.** Es gibt keine gemeinsame
Skala; jede Sonde vergibt ihre Zahl für sich, und nirgends steht, was
sie bedeutet.

Zwei Beispiele, warum das kein Zufall ist:

```c
/* trd_probe: Größe passt -> 70 */
if (data[0x227] == 0x10)      *confidence = 92;   /* echte Signatur */
else if (data[0x8E4] <= 128)  *confidence = 82;   /* <-- Bereichsprüfung */
```

Die zweite Bedingung trifft auf **die Hälfte aller Bytewerte** zu, auf
Nullen immer. Sie hebt 70 auf 82, ohne etwas erkannt zu haben.

```c
/* d81_probe: Größe passt -> 80, ohne jede Prüfung des Inhalts */
```

### Die Folgen, gemessen

* Ein **PC-160K**-Abbild verliert gegen **TRD** (82).
* Ein **Macintosh-800K**-Abbild verliert gegen **D81** (80).
* Ein **PC-180K/360K/720K**-Abbild verliert gegen **MSX** (75).

Keines dieser Plugins hat mehr erkannt als „die Größe passt". Sie haben
nur größere Zahlen gewählt.

**Und schlimmer als der Gleichstand ist die falsche Eindeutigkeit:** bei
verschiedenen Zahlen meldet `uft_probe_ranking.tied` = **1**. Der
Aufrufer erfährt „eindeutig", wo mehrere Plugins dieselbe
größenbasierte Vermutung anstellen. Der Gleichstand ist wenigstens
ehrlich; die falsche Eindeutigkeit ist es nicht.

### Wie andere Werkzeuge es halten — zwei belegte Quellen

1. **SAMdisk** (simonowen.com/samdisk/formats/, abgerufen 2026-08-31)
   führt seine kopflosen Formate ausdrücklich als
   „**RAW — raw sector dumps, identified by file size only.**" Die
   Größen-Erkennung wird *benannt*, nicht als Erkennung ausgegeben. Der
   Baum kennt das bereits: `uft_do.c` zitiert seit MF-463 SAMdisks
   `ReadDO()`, das mit „not used" markiert ist.
2. **MAME floptool** (docs.mamedev.org/tools/floptool.html, abgerufen
   2026-08-31): das Eingabeformat darf `auto` sein, das
   **Ausgabeformat muss immer genannt werden**. Wo es darauf ankommt,
   wird nicht geraten.

Beide lösen es **nicht durch bessere Zahlen**, sondern indem sie die
Unsicherheit sichtbar machen — derselbe Weg wie FMT-17 (MF-724) für
`do`/`po`.

### Vorschlag: eine geschriebene Skala

Heute fehlt die eine Stelle, an der steht, was eine Konfidenz bedeutet.
Vorschlag:

| Band | heißt | Beispiel |
|---|---|---|
| **0–29** | kein Anspruch | — |
| **30–49** | **nur die Größe** passt, sonst nichts | `img`, `v9t9`, `do`/`po` |
| **50–79** | eine Struktur wurde gelesen und ist plausibel | Verzeichnis-Kopf, BPB |
| **80–100** | ein **Erkennungsmerkmal** wurde getroffen | Magic, Signatur |

Zwei Regeln daraus, beide mechanisch prüfbar:

1. **Wer den Inhalt nicht liest, bleibt unter 50.** Prüfbar am Quelltext
   (`(void)d`) *und* an der Messung: was auf einem Nullpuffer über 49
   meldet, hat nichts erkannt.
2. **Sind alle Bewerber im 30–49-Band, ist die Antwort „mehrdeutig"** —
   unabhängig von kleinen Zahlenunterschieden. Das verallgemeinert
   FMT-17 vom Sonderfall `do`/`po` auf alle kopflosen Formate.

### Warum hier nichts geändert wurde

Die Umsetzung berührt **über zwanzig Plugins**. Solange nicht
geschrieben steht, welcher Wert richtig ist, wäre jede Änderung geraten
— und die EINFRIER-REGEL verlangt für Format-Layer-Code eine benannte
Referenz. Die beiden Quellen oben tragen das **Verfahren**, nicht die
einzelnen Zahlen.

Der Regressionsschutz steht: `tests/test_probe_confidence_on_zeros.c`
hält alle 14 gemessenen Werte fest. Sinkt einer, ist das vermutlich
richtig — der Test ist dann nachzuziehen, nicht wegzudrücken.

### Die Eigentümer-Entscheidung

1. Wird die Skala oben übernommen (dann ist es ein Bauauftrag, keine
   Entscheidung mehr)?
2. Soll `tied` auf „alle im selben Band" erweitert werden, statt auf
   „exakt dieselbe Zahl"?
3. In welcher Reihenfolge werden die Plugins angepasst — und wird eine
   Anpassung als Verhaltensänderung behandelt (wie FMT-17) oder als
   Bugfix?

### FMT-21 umgesetzt (MF-729) — Vertrag mit Messinstrument statt zwanzig Zahlen

Der Eigentümer hat alle drei Fragen entschieden, und die Antworten
hängen zusammen: die Bänder tragen **Bedeutung statt Rang**, ihr Wert
liegt in den **zwei mechanischen Regeln**, und der Nullpuffer ist das
Eichgerät dafür.

### Was gebaut wurde

**1 · Die Bänder als Vertrag** (`include/uft/uft_format_plugin.h`):

| Band | heißt |
|---|---|
| 0–29 | kein Anspruch |
| **30–49** | nur die Größe passt |
| 50–79 | Struktur gelesen, plausibel |
| 80–100 | Erkennungsmerkmal getroffen |

**2 · Das Urteil** — `uft_probe_ranking` trägt jetzt `band`, `verdict`
und `band_claimants`:

```
eindeutig  <=>  genau EIN Bewerber im Band des Gewinners
                UND das Band ist mindestens „Struktur gelesen"
```

Alles andere ist **mehrdeutig**, und dann ist die Kandidatenliste die
Antwort. Das verallgemeinert FMT-17 (MF-724) vom Sonderfall `do`/`po`
auf alle kopflosen Formate. Die Felder sind **angehängt**, nicht
eingefügt — ABI-sicher.

**3 · Eichung 1** (`tests/test_probe_confidence_on_zeros.c`): auf einem
Nullpuffer darf **keine** Sonde 50 oder mehr melden. Läuft über
**alle 137** Plugins × 25 Größen — die erste Fassung führte 14 in einer
Tabelle und war damit ein Protokoll, kein Tor.

**4 · Eichung 2** (`tests/test_probe_confidence_on_random.c`): wer
50–79 beansprucht, darf bei höchstens **5 %** zufälliger Puffer
zustimmen. Fester Generator, fester Startwert — ein Tor, das bei jedem
Lauf etwas anderes misst, ist keins.

**5 · Enumeration** — `uft_registered_format_plugin_at()`. Ohne sie
lässt sich kein Tor bauen, das *jede* Sonde prüft.

### Was gemessen wurde, vorher und nachher

```
                        vorher            nachher
Nullpuffer, höchste     85 (DIM)          45 (D13)
Verstöße Eichung 1      —                 0 von 137
Verstöße Eichung 2      —                 0  (vorher: KFX bei 61,7 %)

PC 160K   ->  TRD 82  eindeutig      ->  TRD 45  MEHRDEUTIG (9 von 10)
Mac 800K  ->  D81 80  eindeutig      ->  D81 45  MEHRDEUTIG (12 von 12)
PC 360K   ->  MSX 75  eindeutig      ->  MSX 45  MEHRDEUTIG (11 von 12)
WOZ2-Kopf ->  WOZ 98  eindeutig      ->  WOZ 98  eindeutig   (1 von 3)
```

**Der Sieger ist derselbe wie vorher. Geändert hat sich, was über ihn
behauptet wird.**

### Die Plugins, die gesenkt wurden

Mechanisch, kein Ermessen: geändert wurde ausschließlich der Zweig, der
**nur die Größe** geprüft hatte. Signatur- und Strukturwerte blieben.

`dim` 85→45 · `trd` 70→45 · `d81` 80→45 · `nib` 80→45 · `msx` 75→45 ·
`d64` 75→45 · `d13` 75→45 · `d71` 70→45 · `adf` 70→45 · `sad` 70→45 ·
`adl` 50→45 · `do`/`po` 55→45 (über `UFT_A2_CONF_UNKLAR`)

**Zwei Bereichsprüfungen sind gestorben**, nicht gesunken:

* `trd`: `data[0x8E4] <= 128` hob 70 auf 82 — wahr für die **Hälfte
  aller Bytewerte**. Gesenkt wäre sie von der reinen Größe nicht mehr zu
  unterscheiden gewesen; entfernt ist ehrlicher.
* `kfx`: `oob_count >= 2` gab 80 für zwei `0x0D` in 512 Byte — in
  Zufallsdaten der Erwartungswert. Eichung 2 mass **61,7 %** Zustimmung.
  Beide Zweige jetzt 45/35, mit der relativen Ordnung erhalten.

### Was das für die zwei Kennzahlen heißt

Der Eigentümer hat die Verbindung benannt, und sie trägt: **was löst
einen Gleichstand im 30–49-Band ehrlich auf? Ein Inhaltsleser.** Ein
FAT12-Leser, der einen gültigen Bootsektor und ein plausibles
Wurzelverzeichnis findet, hebt genau einen Kandidaten legitim ins
50–79-Band. Die VFS-Leser sind die natürlichen Schiedsrichter der Probe
— „Dateisystem wird nicht gelesen" und „Erkennung rät" sind dasselbe
Problem von zwei Seiten und fallen mit derselben Arbeit.

### Offen: die Kandidatenliste muss ankommen

`verdict` und `band_claimants` stehen bereit, aber **niemand zeigt sie
her**. Das ist der Baustein für die nächste Klick-Sitzung:

* **GUI:** bei MEHRDEUTIG die Kandidatenliste zur Wahl anbieten — der
  DiskFlashback-Befund („Erkenner liefert nie mehr als einen
  Kandidaten") von der anderen Seite.
* **CLI:** bei MEHRDEUTIG `--format` verlangen und die Kandidaten
  auflisten — exakt das floptool-Muster (Eingabe darf `auto` sein, die
  Entscheidung wird verlangt statt geraten).

Bis dahin gewinnt bei Gleichstand weiterhin der zuerst registrierte —
das Verhalten ist unverändert, nur die Aussage darüber nicht mehr
falsch.

### Ehrlichkeits-Eintrag (Release-Notes)

> **Kopflose Formate werden nur an der Größe erkannt — das Werkzeug
> sagt das jetzt, statt zu raten.** Wer bisher ein PC-Abbild geöffnet
> hat, das (falsch, aber bequem) als TR-DOS aufging, bekommt künftig
> eine Rückfrage. Die Erkennung ist nicht schlechter geworden; sie war
> vorher nur zuversichtlicher, als sie durfte.

### FMT-18 beantwortet (MF-730) — woher die sechs ihre Layouts haben

Die Frage aus MF-720 lautete: für sechs Formate (`edk`, `posix`, `syn`,
`t1k`, `tan`, `xdm86`) findet sich draußen **kein Gegenstück** — woher
haben ihre Plugins dann ihr Wissen? Die Antwort braucht kein Werkzeug,
nur Lesen.

### Fünf haben keine benannte Referenz. Einer schon.

**Entlastet:** `posix` trägt im Kopf *„Reference: libdsk drvposix.c
(LGPL-2.0-or-later; Fassung 1.5.12 geprueft)"* — benannt, versioniert,
geprüft. Es ist auch kein historisches Format, sondern libdsks Schema
„Rohabbild plus `.geom`-Datei". Dass draußen kein Gegenstück gefunden
wurde, war ein Suchartefakt: das Gegenstück heißt `libdsk` und steht
schon im Baum.

**Die anderen fünf** tragen nur eine Geometrie-Behauptung in Prosa:

```
edk    "80 × 2 × 10 × 512 = 819200 (DD) oder 80 × 2 × 20 × 512"
syn    "77 cyl × 2 heads × 16 spt × 256 = 634880 bytes"
xdm86  "40 cyl × 1-2 heads × 9 spt × 256"
```

Keine Quelle, keine Spec, kein Werkzeug. Genau die Lage der fünf
fabrizierten Parser (FMT-2/3/10/11/12).

### Zwei sagen selbst, dass sie nichts unterscheiden

```
t1k  "Tandy 1000 used standard IBM PC format ... Same geometry as IMG"
tan  "Same as JV1 but from Tandy-era tools"
```

Ein Format, das mit einem anderen **byteidentisch** ist und sich nur
durch *„welches Werkzeug es gemacht hat"* unterscheidet, ist aus der
Datei **nicht erkennbar**. Das sind keine Formate, sondern
Herkunftsetiketten — und als Plugins sind sie reine Bewerber im
Größen-Band, die jeden Gleichstand vergrößern (`t1k` 25, `tan` 30, im
FMT-21-Zensus). Sie können nie gewinnen und nie richtig liegen.

### Und die Herkunft ist ein einziger Commit

Alle fünf stammen aus **`4d8883cc`** (2026-04-14). Der Titel ist der
Befund:

> **feat: final 11 Plugin-B parsers — 0 active stubs remaining**
> […] **Format count: 201 → 212. Zero active stubs in build.**

Elf Plugins, 802 Zeilen, ein Commit — **um zwei Zahlen zu schließen**.
Und derselbe Commit-Text kündigt an:

> 25. DCM — header/metadata parsed, **decompression placeholder
>     (returns E5-filled sectors)**

Ein Plugin, das ausdrücklich erfundene Daten zurückgibt, ausgeliefert,
damit „0 aktive Stubs" dasteht. *(Entlastet: `dcm` ist inzwischen
repariert — der Kopf sagt „full decompression", und `dcm_decompress()`
ist echt.)*

**Von den elf stehen heute acht im Niedrig-Band** (`adf_arc` 35, `edk`
30, `pdp` 40, `sam` 35, `syn` 35, `t1k` 25, `tan` 30, `xdm86` 35) —
reine Größenvermutungen, die im FMT-21-Zensus die Gleichstände füllen.
Drei sind belastbar geworden (`2img` 95, `dcm` 90, `xfd` 82).

### Was das über den Baum sagt

Dies ist die **Vorgeschichte** der Regeln, unter denen er heute
arbeitet. Die EINFRIER-REGEL (MF-363/498) und Regel 9 („jeder Baustein
nennt seine Kennzahl; was keine bewegt, ist Fundus, nicht Auftrag",
MF-640) sind die Antwort auf genau diesen Commit: **eine Kennzahl trieb
die Code-Produktion, und die Prüfung kam nicht mit.**

Der Unterschied ist messbar. Damals: 11 Parser an einem Tag, um
201→212 zu erreichen. Heute: zwei Formate in einer Woche gehoben
(`do`, `d13`), jedes mit Differenzlauf gegen eine fremde Hand, und
`nib` **nicht** gehoben, weil der einzige verfügbare Erzeuger dieselben
Quelldateien benutzt wie das Oracle (GCR-3).

### Die Eigentümer-Entscheidung

Für die fünf ohne Referenz, je eines — **und `t1k`/`tan` sind der
klarste Fall:**

1. **Rückzug.** `t1k` ist IMG, `tan` ist JV1; ihre eigenen Köpfe sagen
   es. Sie entfernen heißt: zwei Bewerber weniger in jedem Gleichstand,
   und keine Fähigkeit verloren — die Dateien werden weiterhin gelesen,
   nur unter dem Namen, der stimmt.
2. **Belegen.** Für `edk`, `syn`, `xdm86` eine benannte Quelle
   beschaffen (Ensoniq-, Synclavier-, TI-99-Doku) und die Geometrie
   dagegen halten. Der Streif-Scout hat den Auftrag noch nicht.
3. **Ankündigung senken.** Solange weder das eine noch das andere
   geschehen ist, gehört in `features` und `spec_status`, was wirklich
   belegt ist — die 86f-Behandlung (MF-707/708).

**Kennzahl:** T3 runter für die drei belegbaren; für `t1k`/`tan` sinkt
bei Rückzug die Zahl der Bewerber im Größen-Band, was FMT-21 direkt
zuarbeitet.

### Nachtrag MF-732 — wie viele Sonden den Inhalt gar nicht ansehen

Eine Zahl, die zu FMT-21 gehoerte und niemand hatte. Zensus ueber
**alle** `.c`-Dateien aus `git ls-files src` (nicht ueber eine Liste),
gesucht wurde jeder Sondenrumpf und darin die ausdrueckliche
Verwerfung des Datenzeigers (`(void)d;` / `(void)data;`):

```
Sondenrumpfe im Baum   : 171
verwerfen den Inhalt   :  24   (14 %)
sehen ihn an           : 147
```

**Die 24 blinden:**

```
adf_arc · adl · akai · d13 · d67 · dim · edk · hardsector · jv1
jvc · korg · lisa · micropolis · northstar · pdp · posix · sam
syn · t1k · tan · v9t9 · victor9k · xdf · xdm86
```

### Was die Liste zeigt

**Sie ist fast deckungsgleich mit den drei vorherigen Befunden.** Neun
der elf Plugins aus `4d8883cc` („0 active stubs remaining", MF-730)
stehen darin; alle fuenf ohne benannte Referenz aus FMT-18; die
Mehrheit der dreizehn aus dem FMT-20-Niedrigband.

Der schaerfste Einzelfall ist `dim`: eine Sonde, die den Inhalt
**ausdruecklich verwirft** — und die bis MF-729 mit **85** die
**hoechste** Konfidenz auf einem Nullpuffer meldete. Blindheit und
Selbstsicherheit fielen zusammen.

### Was sie NICHT zeigt

Dass 14 % ein Missstand waeren. Fuer ein **wirklich** kopfloses Format
ist die Groessenpruefung die einzige ehrliche Antwort — `hardsector`,
`v9t9` oder `jv1` haben nichts, was man lesen koennte. Die Frage ist
nicht, ob eine Sonde blind ist, sondern **ob sie das zugibt**. Genau
das erzwingt seit MF-729 Eichung 1: wer blind ist, bleibt unter 50.

Der Zensus ist damit kein neuer Auftrag, sondern die **Bestaetigung,
dass das Band die richtige Groesse hat**: 24 blinde Sonden gehoeren ins
Groessen-Band, 147 sehende koennen es verlassen — wenn sie es
verdienen (Eichung 2).

**Und eine Einschraenkung der Methode:** ein erster Zensus fand nur 50
Rumpfe, weil das Muster je Plugin-Struktur suchte — 39 Sonden stehen in
**anderen** Dateien als ihre Struktur (`uft_adf.c` gegen
`uft_adf_plugin.c`). Die Zahl haette 36 % statt 14 % gelautet und waere
nicht gedeckt gewesen. Gesucht wird darum ueber den ganzen Baum, nicht
je Struktur.

### Nachtrag MF-733 — die CLI-Haelfte von FMT-21 gibt es nicht

Der Plan zu FMT-21 sah zwei Anlaufstellen fuer die Kandidatenliste vor:
die Oberflaeche soll sie zur Wahl anbieten, und **die Kommandozeile
soll bei Mehrdeutigkeit `--format` verlangen** (das floptool-Muster).

**Die zweite Haelfte ist gegenstandslos: UFT hat keine Kommandozeile.**
Gemessen:

```
UnifiedFloppyTool.pro   TARGET = UnifiedFloppyTool
                        TEMPLATE = app          (ein Ziel)
ungeschuetztes main()   genau EINES: src/main.cpp
```

47 der 50 `main()` im Baum stehen in `#ifdef`-Bloecken
(`IMD_PARSER_TEST`, `ADF_PARSER_TEST` …) — es sind Selbsttests der
Parser, keine Programme. Das deckt sich mit der festen Projektregel
„GUI-only, kein CLI-Modus".

**Was daraus folgt, ist keine Luecke, sondern eine Vereinfachung:** der
Baustein fuer die Klick-Sitzung hat **eine** Anlaufstelle statt zwei.
Wo ein Skript stuende, steht hier der Rueckgabewert — `verdict` und
`band_claimants` sind bereits Teil von `uft_probe_ranking`, und ein
Aufrufer, der `UFT_PROBE_VERDICT_MEHRDEUTIG` ignoriert, trifft dieselbe
falsche Annahme wie vorher. Das floptool-Muster bleibt also gueltig, es
wirkt nur an der API-Grenze statt an einer Kommandozeile.

**Berichtigung an mir selbst, weil die Methode wichtiger ist als die
Zahl:** dieser Nachtrag entstand aus zwei nacheinander **falschen**
Zaehlungen. Erst meldete ein Muster „24 ungeschuetzte main()", dann „21
davon im Build" — beides waere ein Link-Fehler gewesen, den es nicht
gibt. Der Grund: das Muster suchte die Wache in den 600 Zeichen vor dem
`main()`, und in `uft_imd_parser_v2.c` steht sie **89 Zeilen** davor
(`#ifdef IMD_PARSER_TEST`, Zeile 639, `main()` bei 728). Gefunden wurde
der Fehler nur, weil ein einzelner Fall konkret nachgesehen wurde,
statt der Zahl zu glauben. Die richtige Messung zaehlt die
Praeprozessor-Verschachtelung mit.

### LIZ-1 geordnet (MF-737) — der Rückstand war 125, die Entscheidung ist 37

Die Zahl stand seit MF-651 als **„171 Attributionen, 125 ohne Lizenz"**
da. Als Rückstand war sie unbrauchbar, weil sie drei völlig verschiedene
Dinge in einen Topf warf. Gemessen, nachdem sie getrennt sind:

| Klasse | gesamt | m. Lizenz | **o. Lizenz** | ist das eine Lizenzfrage? |
|---|---|---|---|---|
| **KEINE** — gar keine Attribution | 23 | 1 | 22 | nein, das ist Prosa |
| **DOKU** — ein Dokument gelesen | 36 | 2 | 34 | **nein** (MF-636) |
| **CODE** — fremde Codebasis benannt | 75 | 38 | **37** | **ja — das ist LIZ-1** |
| **UNKLAR** — nennt beides | 37 | 6 | 31 | Mensch entscheidet |

**87 der 125 waren nie eine Entscheidung.** `docs/CLAUDE.md` sagt es
seit MF-636: *„wer eigenständig implementiert und nur fremde Doku gelesen
hat, schreibt das auch so"* — eine Doku zu lesen begründet keine
Ableitung. Und „based on measured variance" ist überhaupt keine
Attribution, sondern ein zu Ende geschriebener Satz.

#### Ein Prüfer-Fehler, der eine korrekte Attribution beschuldigt hat

`\b(GPL|LGPL|...)\b` traf **`GPLv2` nicht** — das schließende `\b`
verlangt ein Nicht-Wortzeichen, und `GPLv2` hat dort ein `v`. Gemessen:

```
GPLv2+  FEHLT      GPL-2.0   trifft
GPLv2   FEHLT      LGPL-2.1  trifft
GPLv3   FEHLT      GPL 2     trifft
```

Folge: `cbmconvert by Marko Mäkelä (GPLv2+)` stand auf der Liste „ohne
Lizenz", obwohl die Lizenz danebensteht. Die häufigste Schreibweise
überhaupt war die eine, die der Prüfer nicht kannte.

#### Sechs Schärfungen — drei davon gegen selbst erzeugte Fehler

Eine Regel über Fließtext liegt zuerst daneben. Festgehalten, weil das
Muster wiederkehrt:

| # | was | Wirkung |
|---|---|---|
| 1 | Marker vor Identifizierbarkeit | `libdsk` trägt keinen Großbuchstaben und ist trotzdem Code |
| 2 | `derived from`/`taken from` sind Code-Auslöser | — |
| 3 | Titel in Anführung = Dokument; `<Name> by <Person>` = Code | — |
| 4 | starke vs. schwache Code-Marker | 6 Prosastellen raus („the track number") |
| 5 | Adresse: Doku, außer bei einer Code-Ablage | `applesaucefdc.com/moof-reference` galt als `nutzer/repo` |
| 6 | der Auslöser allein genügt nicht | **Schärfung 2 hatte 7 Prosastellen hereingezogen** |

Schärfung 6 korrigiert Schärfung 2. `REPO_PFAD` lief mit `re.I` und hielt
**`D77/D88`** für einen Repo-Pfad — eine reine Formatspezifikation in der
Code-Klasse.

**Grundsatz der Einordnung: im Zweifel die strenge Seite.** Was sich
nicht entscheiden lässt, wird UNKLAR und geht an einen Menschen — nie
stillschweigend in die harmlose Klasse. Ein Rückstand, der sich durch
Wegsortieren verkleinern lässt, misst nichts.

Alle acht Fälle stehen als Selbsttest in `scripts/audit_selbsttest.py`.

#### Die Zahl hat jetzt eine Sperrklinke

Tor 43 in `check_consistency.py`: **37 darf sinken, nicht steigen.** Es
prüft nicht *ob* — eine Attribution ohne Lizenz ist nichts Verbotenes,
sondern etwas Entscheidungsbedürftiges. Es prüft, dass niemand eine neue
Ableitung erklärt, ohne ihre Lizenz zu nennen.

### Die Belege — gemessen, nicht angenommen

Über die GitHub-API abgefragt (`gh api repos/<r>/license`), Lizenzdateien
im Wortlaut gelesen:

| Quelle | Nennungen | Beleg | Lizenz |
|---|---|---|---|
| **XCopy Pro** | **4** | kein Repo; kommerzielles Amiga-Kopierprogramm | **keine** |
| **nibtools** | **3** | `OpenCBM/nibtools`: keine `LICENSE`, kein Lizenzkopf in `gcr.c`, nichts im `readme.txt` | **nirgends genannt** |
| **dec0de** | 3 | kein GitHub-Treffer | offen |
| **bbctapedisc** | 3 | kein GitHub-Treffer | offen |
| **msa-to-zip** | 2 | `obruchez/msa-to-zip`: keine `LICENSE` | **keine genannt** |
| hactool | 3 | `SciresM/hactool` | ISC |
| VICE | 3 | `VICE-Team/svn-mirror`, `vice/COPYING` | GPL-2.0 |
| **FluxEngine** | 2 | `davidgiven/fluxengine`, `COPYING.md` | **GPL-2.0**, nicht MIT |
| MAME | 2 | `mamedev/mame`, `COPYING` | GPL-2.0 |
| **Aaru** | 1 | `aaru-dps/Aaru`, `LICENSE` | **GPL-3.0** |
| 86Box | 1 | `86Box/86Box` | GPL-2.0 |
| HxCFloppyEmulator | 1 | `jfdelnero/…`: keine `LICENSE` | offen |
| cbmconvert | 1 | Kopf im eigenen Baum | GPLv2+ |

Eingeordnet wird hier nichts — das ist Eigentümer-Sache (MF-679). Was
sich geändert hat: die Frage steht nicht mehr als „laut README", sondern
als Zitat aus der Lizenzdatei.

### Vier Entscheidungen, nach Dringlichkeit

**1 · XCopy Pro — vier Stellen, kommerzielle Vorlage, keine Lizenz.**

```
analysis/uft_track_analysis.c/.h      "derived from XCopy Pro (1989-2011)"
formats/amiga/uft_amiga_protection.c  "port of XCopy Pro 68000 Assembly algorithms"
formats/amiga/uft_amiga_protection.c  "Port of ROL.L #1,D0"
```

*„Port of … Assembly algorithms"* ist eine **ausdrückliche
Ableitungserklärung** von einem kommerziellen Programm ohne jede
Lizenz. Das ist die Lage, die bei IPF eine ganze Fähigkeit gekostet hat
(P0-5, MF-638) — und dort war der Parser wenigstens erreichbar.

Nach MF-695 sind die Kanäle: **Nachbau** (`uft-nachbau`, Weg 2 —
Verhalten belegen, Code neu schreiben), **Zurücknehmen**, oder
**Umformulieren**, falls die Erklärung zu weit greift und tatsächlich
nur Verhalten nachgebaut wurde. Das lässt sich am Code prüfen, und es
ist die erste Frage: *steht dort wirklich portierter Code, oder hat die
Kopfzeile übertrieben?*

**2 · FluxEngine ist GPL-2.0, nicht MIT.**

`core/unified/uft_flux_decoder.h` („Based on FluxEngine's proven flux
decoding algorithms"), `micropolis`, `victor9k`. GPL-2.0 ist mit unserem
`GPL-2.0-or-later` **vereinbar** — die Erklärung muss die Lizenz nur
nennen. Billigster Fall auf der Liste; drei Kopfzeilen.

**3 · Aaru ist GPL-3.0.**

`include/uft/formats/modern/uft_aaru.h`. GPL-3.0 lässt sich mit
`GPL-2.0-or-later` verbinden, aber **das Ergebnis ist dann GPL-3.0**.
Das ist eine Lizenzentscheidung über den Baum, keine Kopfzeile. Zu
prüfen: ist es überhaupt eine Ableitung, oder nur ein Formatverweis?

**4 · nibtools und msa-to-zip nennen nirgends eine Lizenz.**

Damit gilt „unbekannte Lizenz = Zone PRÜFEN" (MF-679). Für nibtools ist
das besonders scharf: `uft_gcr_ops.h` nennt **`nibtools gcr.c:`**
namentlich, und diese Datei trägt nur einen Copyright-Vermerk. Der
stärkste offene Kanal ist eine **Anfrage an die Autoren** — Pete
Rittwage und Markus Brenner sind erreichbar, und eine schriftliche
Freigabe kostet nichts als eine Mail.

### Was bewusst offen bleibt

Die **31 UNKLAR ohne Lizenz** nennen Doku *und* Code zugleich — „MSX
Resource Center wiki, openMSX source", „VICE emulator, Commodore
2040/4040 technical docs". Welche der beiden die Vorlage war, kann keine
Regel entscheiden; das weiß nur, wer die Datei geschrieben hat. Sie
stehen als Liste bereit (`--unklar`), nicht als Zahl im Rückstand.

**Kennzahl:** „Dateien mit ungeklärter Herkunft", Verdachts-Stufe:
**125 → 37**, und zum ersten Mal mit einer Sperrklinke statt einer
gepflegten Zahl.

### Die vier Lizenzentscheidungen, einzeln beantwortet (MF-739)

Der Eigentümer hat jede getrennt entschieden. Hier steht, was die
Prüfung ergeben hat — inklusive der einen, bei der die Antwort das
Gegenteil der Erwartung war.

#### 2 · FluxEngine — erledigt

Drei Kopfzeilen. GPL-2.0 laut `COPYING.md`, MAME GPL-2.0 laut
`COPYING`, beide vereinbar mit `GPL-2.0-or-later`. Lizenzen aus den
Lizenzdateien gelesen, nicht aus README-Prosa. Rückstand **37 → 35**.

#### 3 · Aaru — keine Ableitung, und dabei ein Korrektheitsverdacht

Die Frage war: enthält die Stelle Aaru-**Code**, oder bildet sie nur
Aaru-**Verhalten** nach? Gemessen an Aarus eigenem
`Aaru.Images/AaruFormat/Structs.cs`:

| | Aaru | unser Header |
|---|---|---|
| Fassung | `Version` (Byte-Array), `ApplicationVersion` | `major_version`, `minor_version` (uint8) |
| Zeitstempel | **Windows FILETIME**, 100 ns seit 1601-01-01 | „Unix timestamp" |
| Längen | `ManufacturerLength`, `ModelLength`, … | kommen nicht vor |

Andere Namen, andere Typen, **andere Semantik**. Eine Transkription
hätte die FILETIME-Bedeutung mitgenommen. Die GPL-3.0-Frage löst sich
damit auf; die Kopfzeile sagt jetzt „Verhalten nach der
Formatbeschreibung, eigenständige Implementierung".

**Nebenbefund, der bleibt:** wenn Aaru FILETIME schreibt und wir „Unix
timestamp" dokumentieren, ist entweder die Beschreibung falsch oder die
Felder liegen woanders. Das Format steht auf **T3** — kein Test, nie
geprüft. Ein billiger, konkreter Prüfauftrag.

#### 4 · nibtools — der Brief steht, und er ist nicht die Lösung

`docs/LIZENZ_ANFRAGEN.md`. Fehlende Lizenz heißt **„alle Rechte
vorbehalten"**, nicht „frei verwendbar"; bis eine Antwort kommt, bleibt
die Stelle in Zone PRÜFEN. Die Tabelle dort führt jede Anfrage mit
Datum und Stand, damit eine *gestellte* Frage nicht als erledigt
durchgeht.

Der Brief fragt nach **`gcr.c`**, nicht allgemein: eine allgemeine
Frage verlangt vom Empfänger eine Entscheidung über sein ganzes
Projekt, eine Frage nach einer benannten Datei mit fertigem Vorschlag
ist in zwei Minuten beantwortet.

### 1 · XCopy Pro — die Kopfzeile hat nicht übertrieben

Die entscheidende Frage lautete: steht dort portierter Code, oder
klingt die Zeile nur beeindruckend? **Es steht portierter Code, und der
Beweis liegt in unserem eigenen Baum.**

`src/formats/amiga/uft_amiga_protection.c` enthält den originalen
68000-Quelltext in Kommentarblöcken, **mitsamt seinen deutschen
Originalkommentaren**:

```
 *   move.w  #RLEN-2,D0      ; Read LEN in Bytes
 *   lea     RLEN*2(A3),A1   ; Ende des Puffers
 * 1$ tst.w   -(A1)          ; letztes Word im Puffer suchen
 *   moveq   #0,D6           ; Bruchstellenzaehler
 *   cmp.w   #5,D6           ; max 5 Bruchstellen
```

Und daneben die Transliteration, Befehl für Befehl:

```c
/* swap D0 / move.w (A2)+,D0 / swap D0 */
d0 = (d0 >> 16) | ((uint32_t)read_be16(data + pos) << 16);
```

18 solcher Stellen. Der Kopf nennt sechs Routinen bei ihren
Originalnamen — *Analyse, Search, gapsearch, Neuhaus, getracklen,
dostest*. „Neuhaus" ist ein **Personenname als Routinenname**; so etwas
errät man nicht.

#### Die anderen zwei Dateien: die Textähnlichkeit täuscht

`uft_track_analysis.c/.h` hat **kein** Assembly, keine Routinennamen,
und die Textähnlichkeit zur Amiga-Datei beträgt **1,8 %**. Das sieht
nach Unabhängigkeit aus — bis man das richtige Maß anlegt. Der Baum hat
eines (MF-696: *beweiskräftig sind Idiome, nicht Fakten*):

> **Zwölf Bezeichner kommen in diesen beiden Dateien vor und in keiner
> einzigen der übrigen 715.**
>
> `detect_breakpoints` · `breakpoint_count` · `bp_count` ·
> `has_breakpoints` · `gap_index` · `gap_sector_index` ·
> `unique_lengths` · `unique_lens` · `unique_count` · `rol32` ·
> `bytes_left` · `Breakpoint`

„Breakpoint" ist die Übersetzung von „Bruchstelle" aus dem
Original-Assembly-Kommentar, und der Kopf nennt die Routine ausdrücklich
„Neuhaus (**breakpoint** detection)". Das ist dieselbe Ableitung, eine
Stufe verallgemeinert — keine unabhängige Umsetzung.

**Die Textähnlichkeit war das falsche Instrument.** 1,8 % hätte
Entwarnung bedeutet; der Idiom-Test sagt das Gegenteil. Genau dafür
steht die Regel im Baum.

### Kann man das nachbauen — und würde es uns helfen?

**Nachbauen: ja, die Konzepte sind nicht geschützt.** Multi-Sync-Suche
mit Bitrotation, Spurlängenmessung, GAP-Erkennung über
Häufigkeitsanalyse — das macht jeder Flux-Dekoder so. Ein Algorithmus
ist nicht geschützt, eine konkrete Umsetzung schon, und wir haben die
konkrete Umsetzung.

Aber der Nachbau hätte eine Hürde, die hier besonders scharf ist: die
Zwei-Hände-Brandmauer verlangt, dass die bauende Hand die Vorlage nie
sieht — **und die Vorlage steht in unserem eigenen Baum**, in
Kommentaren. Wer die Datei öffnet, ist kontaminiert. Ein Nachbau
verlangt also, sie **zuerst zu entfernen**.

**Helfen: nein, gemessen.**

```
37 exportierte Funktionen in den vier Stellen
 1 wird von ausserhalb gerufen — aus einem TEST
 0 Aufrufer in src/, keine GUI, keine Pipeline
```

Und die eine gerufene Funktion, `uft_amiga_identify_sync()`, ist ein
**Vier-Fall-`switch`** von Sync-Wort auf Spielname (AmigaDOS, Arkanoid,
Beyond the Ice Palace, Mercenary). Eine Tatsachentabelle. Tatsachen sind
nicht geschützt, und sie ist in zehn Minuten unabhängig geschrieben.

Im größeren Bild ist das nicht die Ausnahme:

| | |
|---|---|
| CODE-Dateien mit exportierten Funktionen | **51** |
| davon mit Aufrufer in `src/` | 45 |
| davon **ohne jeden Aufrufer** | **6** |

Die beiden XCopy-Dateien sind mit 20 und 17 Funktionen die **zwei
größten toten Blöcke** unter allen Dateien, die eine Ableitung
erklären.

#### Empfehlung

**Entfernen, nicht nachbauen.** Ein Clean-Room-Nachbau kostet das volle
Zwei-Hände-Ritual und produziert am Ende Code, den weiterhin niemand
ruft — das wäre die vierte Runde „Bestand statt Fähigkeit" nach dem
Kopierschutz-Katalog (P0-2) und den DeepRead-Modulen (MF-627).

Nach MF-695 ist der richtige Kanal damit **Fundus**: benannt wartend.
Was ihn öffnen würde, ist kein Rechtsproblem, sondern ein Aufrufer —
eine GUI oder Pipeline, die Amiga-Kopierschutzanalyse tatsächlich
braucht. Kommt die, sind die Konzepte frei und direkt aus der
Standardtechnik implementierbar; die Vorlage wird dafür nicht gebraucht.

Zu erhalten wäre einzig die Sync-Tabelle (vier Konstanten plus Namen)
und der Test, der sie benutzt.

### Der Punkt, der über die vier hinausgeht

**Eine Attributionszeile ist eine Tatsachenbehauptung.** Sie kann in
beide Richtungen falsch sein, und heute waren beide Richtungen im Baum:

* **Aaru** behauptete eine Ableitung, die es nicht gibt — und hätte
  beinahe eine GPL-3.0-Entscheidung über das ganze Projekt ausgelöst.
* **`uft_track_analysis.h`** sagte „derived from" und untertrieb dabei:
  gemessen ist es dieselbe Ableitung wie die Assembly-Datei daneben.

Das ist dieselbe Klasse wie die fabrizierten Parser und wie „Das
Merkmal ist **gemessen, nicht geraten**" neben einem geratenen
Schwellwert (MF-738, eine Stunde alt). Eine Zeile, die niemand prüft,
sagt irgendwann etwas, das nicht stimmt — und hier erzeugt sie ein
Rechtsproblem aus dem Nichts.

**Offen bleibt die Frage in ihrer allgemeinen Form:** von den 75
CODE-Fällen sind 45 lebendig, und wie viele davon eine *echte*
Ableitung erklären, ist bei keinem einzigen geprüft. Der Idiom-Test hat
heute in einer Viertelstunde eine Antwort geliefert, wo die
Textähnlichkeit danebenlag — er ist das Werkzeug für diese Frage, und
er ist billig.

### Der Nachbau ist beantwortet, bevor er begonnen hat (MF-740)

Der Eigentümer hat die Frage an der richtigen Stelle angegriffen:
*„Nachbauen, damit es mit den anderen Komponenten harmoniert" setzt
voraus, dass es eine Komponente gibt, mit der harmoniert werden soll.*
Die gibt es nicht — 20 und 17 Funktionen, null Aufrufer, die zwei
größten toten Blöcke unter allen Ableitungserklärungen im Baum.

Der erste Schritt ist deshalb keine Bauplanung, sondern die Frage:
**welcher Aufrufer soll das benutzen?** Sie ist offen, und ohne
Antwort ist jede Integration Vermutung.

#### Aber die Belegfrage steht vor der Codefrage — und die ist beantwortet

Der Eigentümer hat den Weg selbst benannt: Sync-Wörter sind auf dem
Medium beobachtbar, also in einer unabhängigen Quelle zu belegen. Damit
würde aus „nachgebaut" ein „unabhängig belegt". Diese Prüfung braucht
keinen Aufrufer, und sie ist billig. Gemessen:

| Wert | Anspruch im Baum | unabhängig belegt? |
|---|---|---|
| `0x4489` | AmigaDOS-Standard | **ja** — Amiga Hardware Reference Manual |
| `0x9521` | Arkanoid | **nein** |
| `0xA245` | Beyond the Ice Palace | **nein** |
| `0xA89A` | Mercenary, Backlash | **nein** |
| `0x448A` | „ohne belegten Namen" | — |

Die drei spielspezifischen Werte kommen **auf GitHub nirgends** vor.
Und der stärkste Gegenprobe-Fall: [`keirf/Disk-Utilities`
(libdisk)](https://github.com/keirf/Disk-Utilities) führt rund 200
einzeln gepflegte Amiga-Schutzhandler — und hat für **keines** der vier
Spiele einen.

#### Zwei der drei Zuordnungen werden ausdrücklich widersprochen

* **Arkanoid – Revenge of Doh** benutzte laut Cracking-Dokumentation
  *Single Track Protection – Copylock* — ein benanntes Schema, kein
  nacktes Sync-Wort.
* **Beyond the Ice Palace**: die
  [SPS-WIP-Notiz](http://www.softpres.org/wip:2004-02-20) nennt
  *Cyberblast-Schutzspuren*, dieselben wie bei den Garrison-Titeln, vom
  selben Autor geschrieben.

Damit ist die Tabelle nicht nur von ungeklärter Herkunft — **ihre
Fakten sind unbelegt und teils widerlegt.** Sie hat genau eine Quelle,
und das ist XCopy Pro.

**Das erledigt die Nachbau-Frage endgültig.** Man kann nichts sauber
nachbauen, dessen Fakten falsch sind; man würde eine ungeprüfte
Behauptung durch eine unabhängig gebaute ungeprüfte Behauptung
ersetzen. Genau der Mechanismus, der die 88 Plugins ohne Ground Truth
erzeugt hat.

#### Was stattdessen gebaut wurde: die Brandmauer als Vorrichtung

Der Eigentümer verlangt, dass der Agent protokolliert, welche Dateien
er in welchem Lauf geöffnet hat — *„ohne das ist die Trennung eine
Behauptung"*. Diese Vorrichtung gab es nicht.
`tools/uft-nachbau/scripts/sichtprotokoll.py`, Selbsttest **9/9**.

`kontamination.py` prüft am **Ergebnis** (stehen Ausdrücke der Vorlage
im Neubau?). Das Sichtprotokoll prüft am **Vorgang**, und es sagt
ausdrücklich, welche seiner zwei Hälften trägt:

* **Mechanisch — der Arbeitsbaum.** Hand B arbeitet in einem Baum ohne
  die Vorlage. Geprüft nach Pfad **und** SHA-256; ein Selbsttestfall
  ist genau die umbenannte Vorlage, die ein Pfadvergleich durchlässt.
  Wer nicht lesen kann, was nicht da ist, ist nicht kontaminiert.
* **Deklarativ — das Protokoll.** Was eine Hand geöffnet hat, kann das
  Werkzeug nicht messen; es sitzt nicht im Agenten. Ein Lauf B, der
  eine Vorlagendatei meldet, ist rot. Ein Lauf B, der **schweigt**, ist
  *unbelegt* — nicht bestanden.

Die Unterscheidung steht im Kopf des Werkzeugs, weil es sonst mehr
verspräche, als es hält.

#### Und sie gilt für mich

Ich habe `uft_amiga_protection.c` gelesen, um festzustellen, *was*
dort steht. Damit bin ich **Hand A** und als Hand B ausgeschlossen.
Das ist keine Formalie: die naheliegendste Handlung am Ende eines
Nachbaus ist, die Vorlage zu öffnen „um zu prüfen, ob es stimmt" — und
genau die hebt die Trennung auf.

Die Verhaltensbeschreibung, die Hand A liefern dürfte, ist damit auch
schon geschrieben und besteht aus fünf Zeilen: vier Sync-Wörter, ein
Name je Wort. **Drei davon sind unbelegt, zwei widerlegt.** Es gibt
nichts zu übergeben.

#### Stand

| | |
|---|---|
| Aufrufer benannt? | **nein** — offen beim Eigentümer |
| Fakten belegt? | **1 von 4** |
| Empfehlung | entfernen, nicht nachbauen (MF-739) |
| Kanal | **Fundus** — was ihn öffnet, ist ein Aufrufer |
