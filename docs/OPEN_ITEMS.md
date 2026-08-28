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
