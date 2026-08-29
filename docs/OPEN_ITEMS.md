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
