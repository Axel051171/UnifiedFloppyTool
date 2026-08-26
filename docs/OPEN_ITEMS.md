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
| P0-11 | **Leck-Rueckstand — die Einordnung war falsch, zurueckgenommen (MF-592).** Hier stand: „im Produktionscode kein einziger Aufrufer, der eine gelesene Spur liegen laesst; der Rueckstand ist Testcode.“ Das stimmte fuer die Frage, die `audit_track_cleanup.py` stellt (fehlt ein AUFRAEUM-Aufruf?) — und ging an der Sache vorbei. Der geteilte Helfer `uft_format_add_sector_with_id()` alloziert einen Sektorpuffer, `uft_track_add_sector()` **kopiert** ihn (`uft_format_plugin.c:452-456`), und freigegeben wurde er nur im Fehlerfall. Also leckte **jeder gelesene Sektor** — 1,44 MB je Lesevorgang einer 1,44-MB-Diskette. Gemessen an `test_atr_512`: 2944 Byte in 8 Objekten. Wirkung im CI-Volllauf unter ASan: **58 von 266 auf 15 von 266**. Dabei gefunden und behoben: `uft_track_add_sector()` kopierte `confidence_map`, `weak_mask` und `timing_ns` **flach**, waehrend `uft_sector_cleanup()` sie freigibt — ein latentes doppeltes free im gemeinsamen Helfer | ⚠ **15 offen (MF-592).** Der Produktionsanteil ist weg und gemessen; was bleibt, ist noch nicht zugeordnet — 14 brechen unter LeakSanitizer ab, einer fiel unter UBSan und ist seit MF-594 behoben |
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
| **A1** | **57 von 88 tier-geführten Formaten stehen auf T3 — ungeprüft.** Kein Test, oder ein synthetischer Test ohne Abgleich gegen eine autoritative Quelle. Genau in dieser Lage waren die fünf fabrizierten Parser grün. Belegt: T1=2, T1b=12, T2=17 | gemessen | **offen**, Moratorium MF-363/498 |
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
