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
| P3-1 | **37 Formate auf T3** (gemessen 2026-09-02, MF-796; MF-786 mass 39, seither `dim_atari` und `edsk` gehoben; hier stand **57** — um 18 daneben, ausgerechnet in dem Punkt, der das Moratorium führt). **Und das Moratorium ist EIN FORMAT von seinem eigenen Ende entfernt:** MF-363/498 verlangt ATR, D64, ADF, FDI, NFD-r0 auf T1/T1b — `fdi` steht auf **T1**, `adf`/`atr`/`d64` auf **T1b**, allein **`nfd` ist noch T2**. Es hat bereits Test und benannte Spec-Quelle; was fehlt, ist ein **cross-tool-Abbild**, und NFD ist ein Container mit Kopf — also blockiert von **P3-14**. Danach gilt nicht „alles erlaubt", sondern **1:2** (ein neues Format = zwei Hebungen). Plan: `docs/PLAN_NAECHSTE_STRECKE.md`. **GEMESSEN (MF-795) — `nfd` ist gesperrt, und zwar nicht aus Mangel an Werkzeugen, sondern an der fünften Frage.** Keines der drei verfügbaren Oracles schreibt NFD: `gw` kennt PC-98 nur als Rohgeometrie (`pc98.2d/2dd/2hd/2hs`), `samdisk` nennt es nicht, `hxcfe` führt `NEC_FDI` und `NEC_D88` — beide **ohne** NFD. Die beiden Werkzeuge, die NFD beherrschen, sind **FIVEC** (pc98.org) und **d88split** (tomari) — und genau diese zwei stehen im Kopf von `src/formats/nfd/uft_nfd_plugin.c` als die Quellen, gegen die MF-358 den Leser geschrieben hat. Ein Abbild von ihnen bestätigte nur, wovon UFT abgeleitet ist — dieselbe Lage, die MF-780 bei `openMSX`/`msx_disk` erkannt und verworfen hat. **Was es öffnen würde:** ein PC-98-Emulator, der NFD selbst schreibt — der Format-Urheber **T98-Next** oder ein gleichwertiger. Das ist eine Beschaffungsentscheidung des Eigentümers, wie Amiga Forever bei X-Copy. Bis dahin bleibt das Moratorium in Kraft, und der Grund ist benannt statt vermutet |
| P3-2 | Korpus-Beschaffung — **blockiert beim Eigentümer**: `cpmtools`, SAMdisks `tc.cpp`, `sector-cpc` (siehe `MAMMUT_PLAN.md` §5) |
| P3-3 | GUI-Rauchtest für MF-496/MF-501 — **blockiert beim Eigentümer** (kein Bediener in dieser Sitzung) |
| P3-4 | Tier-3-Hardware-Bench — **kein Gerät vorhanden** (MF-310), an die Gemeinschaft delegiert |
| P3-5 | Teilaufnahme-Karte nach ddrescue-Vorbild (Mammut §1.3, letzter offener Teil) |
| P3-6 | **ATR Enhanced Density:** beide ATR-Fassungen rechnen fest mit 18 Sektoren/Spur. Für ED (1040 Sektoren à 128 B) ergäbe das 58×18 statt 40×26. **Kein Datenverlust** — ATR ist ein lineares Sektorabbild, jeder Sektor bleibt erreichbar; falsch wäre nur die *gemeldete* Geometrie. Nicht geändert, weil keine **benannte Referenz** im Baum liegt (MF-498(a)) — gehört in die ATR-Hebung auf T1/T1b |
| P3-7 | **Der Schreibstartpunkt hat keinen Verbraucher (P6, MF-769).** Der Produzent steht: `uft_track_verdikt_t` trägt seit MF-769 `splice_lage`, `splice_pos_ns`, `splice_abstand_ns` und `splice_alt_ns` — gemessen unterscheidet er „Sync nach dem Gap da" (Abstand 11 994 000 ns) von „Sync direkt hinter dem Index" (16 000 ns, Faktor 750) von „kein Sync = Code 3" von „nur eine Umdrehung aufgenommen". **Gelesen wird davon nichts.** Der Verbraucher wäre der Schreibpfad, und `src/hal/uft_greaseweazle_full.c` ist ein **geschützter Pfad** — Änderung nur nach Rückfrage. Die Entscheidung lautet: soll der Startpunkt über `uft_hal.h` in die HAL (so der Plan), oder bleibt es beim `index_sync` des Greaseweazle-Protokolls? Bis dahin ist auch dies **Bestand, nicht Fähigkeit** — dieselbe Lage wie bei „gerettet" (P3/B8). Die **Schwelle**, ab der ein Abstand zu klein ist, hängt am Laufwerk und bleibt ungemessen (MF-310); deshalb liefert der Produzent einen zweiten Kandidaten statt einer Zahl |
| P3-8 | **Drei Aminet-Pakete gesichtet, Lizenzurteil offen (MF-773).** `SuperDuper 3.0/3.13` — Weitergabe nur „without additions, deletions, or modifications of any kind", also **kein Bearbeitungsrecht**, dieselbe Klasse wie LIZ-4 B (MF-744). `AllowBad 0.7` — Freeware mit ausdrücklichem Modifikations- **und Reverse-Engineering-Verbot**, kein Quellcode im Archiv. `badformat 0.2` — **keinerlei Lizenzaussage** → Zone ROT. Alle drei: Urteil ist Eigentümerentscheidung (MF-679). Gutachten: `docs/scout/GUTACHTEN_SUPERDUPER_ALLOWBAD_BADFORMAT.md` |
| P3-9 | **SuperDuper als Oracle für den fehlenden AmigaDOS-MFM-Encoder.** Der einzige der vier Funde, der eine Kennzahl bewegt (**Wandlungspfade ↑**): `src/formats/uft_format_convert_bitstream.c` lehnt ADF→HFE ab, weil dem Baum ein AmigaDOS-Encoder in `src/` fehlt (MF-539; der in `tests/flux_gen/` ist testseitig). SuperDuper wäre die **zweite unabhängige Hand**, die MF-646 verlangt — unabhängig von ADFlib und libhxcfe. Kanal: **Oracle** (ausführen frei, weitergeben nicht), nicht Port. **ERLEDIGT (MF-776):** `gw` 1.23 ist installiert (Archiv-SHA-256 `f409ae24…d146464`) und **kodiert AmigaDOS ohne Hardware** — `gw convert --format amiga.amigados x.adf x.scp`. Beide Richtungen gegen UFT gemessen: **1760/1760 Sektoren byteidentisch**. Die zweite Hand für MF-539 ist damit da; SuperDuper wird dafür nicht mehr gebraucht und bleibt **Fundus, nicht Auftrag**. — Der Weg dorthin, für den nächsten Leser: **NICHT MEHR BLOCKIEREND (MF-774):** MF-646s „dieselbe Hand" meint die *Abstammung des Codes*, nicht das Werkzeug — `gw` (Keir Fraser) und `hxcfe` (jfdelnero) sind zwei getrennte Linien und genügen einander. Offen bleibt nicht die Zahl der Hände, sondern deren Unabhängigkeit **von UFT**: beide tragen im Register `abstammung` = offen, und bei `hxcfe` nennt `uft_hxcstream.h` das HxC-Projekt als Code-Erklärung ohne Lizenz (MF-743). Gemessen sind zudem **0 von 10** Oracles installiert. Damit ist P3-9 eine Wahlmöglichkeit, kein Engpass |
| P3-10 | **43 Testblöcke listen dieselben Quellen einzeln (MF-773).** Jede neue Abhängigkeit des Flussdekoders muss heute 43-mal eingetragen werden; MF-765/766/768 haben genau das verpasst und den Bau vier Commits lang gebrochen. Der Hook fängt es jetzt, aber er behandelt das Symptom. Die Ursache wäre eine **Objektbibliothek**, gegen die alle Testziele linken — aus 43 Stellen wird eine. **Prüfauftrag für den Innendienst**, kein Nebenfix: er muss erst messen, welche Ziele wirklich denselben Satz brauchen und welche bewusst schlank sind |
| P3-11 | **Lizenz-Klasse statt Einzelurteil — Eigentümer-Vorlage (MF-774).** Vorschlag zur Entscheidung über P3-8: **nicht je Paket urteilen, sondern die Klasse festlegen.** „**Oracle-Kanal** = unverändert ausführen, Ergebnisse als Fakten festhalten, weder Binary noch Text ins Repo." Die X-Copy-Binaries laufen bereits über genau diese Klasse. SuperDupers Text verbietet die Veränderung der *Verteilung* und sagt nichts gegen Ausführung; `badformat` ohne Lizenz braucht dafür nur, was der Aminet-Upload ohnehin bezweckt. Der einzige harte Fall ist **AllowBads Reverse-Engineering-Verbot** — ob ein Black-Box-Differenztest darunter fällt, ist die eine Frage, die wirklich zu beantworten ist. **Und sie muss nicht beantwortet werden:** AllowBad bewegt keine Kennzahl und kann draußen bleiben. Urteil bleibt beim Eigentümer (MF-679) |
| P3-12 | **Ein Hardwaretag erledigt drei wartende Punkte (MF-774).** `P6`/`P3-7` (Schreibstartpunkt am echten Laufwerk), `P1` (HIL) und `P7` (ob der Default-an-Zustand am echten Laufwerk trägt) warten alle auf **dasselbe Tor** — es hat im Mai v4.1.4 blockiert. Ein Tag mit Greaseweazle, Laufwerk und frischen Disketten deckt alle drei. **Danach öffnet sich ein zweiter Oracle-Pfad:** dieselben Fixtures per Greaseweazle auf echte Disketten, echtes X-Copy auf echtem Amiga — Emulator- und ROM-Achse fallen weg. Das geht erst **nach** P6, weil es den Schreibpfad braucht. **Reihenfolge: Emulator zuerst, Hardware zweitens.** Kein Gerät im Haus (MF-310) — an die Gemeinschaft delegierbar |
| P3-18 | **`atrcopy` taugt für `dcm` — aber als PRÜFER, nicht als Erzeuger (MF-788).** Gemessen: `atrcopy` 10.1 (MPL-2, bereits im Register) bringt ein `dcm`-Modul mit, und UFTs `src/formats/dcm/uft_dcm.c` nennt es **nicht** — für DCM ist es also eine **unabhängige Hand** (anders als für `atr`/`xfd`, die es selbst erzeugt hat). **Aber:** das Modul ist **nur lesend** (`DCMContainer`, kein `encode`/`save`/`write`). Als Erzeuger fällt es damit aus. Der Weg wäre umgekehrt: eine DCM-Datei aus **dritter** Quelle, dann UFT und atrcopy dasselbe entpacken lassen und die Sektoren vergleichen. Braucht also zuerst eine Fixture — Kennzahl T3 ↓ für `dcm`, aber blockiert auf Beschaffung |
| P3-19 | **`COPY130.M65` (Antic 85-09) — Fundus, kein Auftrag (MF-788).** Gemessen: ein **Kopierprogramm** für den Atari 130XE, kein Format-Parser; es beschreibt keine Datenträgerstruktur, sondern nutzt die SIO-Schnittstelle und bankgeschaltete Puffer (255 Sektoren, dann 84 weitere). **Rechtelage: „(c) 1985, ANTIC PUBLISHING"** — urheberrechtlich geschützt, Zone PRÜFEN, kein Port. **Bewegt keine der vier Kennzahlen:** das Atari-Format, das es berührt, steht mit `atr` bereits auf T1b, und ein Kopierablauf ist ohne Hardware nicht nachmessbar (MF-310). Nach Regel 9 damit **notiert, nicht eingeplant** — mit dem, was ihn öffnen würde: eine belegte Verhaltensfrage, die UFT heute falsch beantwortet |
| P3-23 | **Kodierte Disketten: strukturell heil, inhaltlich zufällig — der zweite Falschbefund (MF-791).** `SanityCopy` (Chaos/Sanity) ist kein Kopierer mit Zusatz, sondern ein **Diskettenverschlüssler**: die Sektorinhalte werden beim Kopieren mit einem aus einem Passwort erzeugten Muster EOR-verknüpft, und ein Dekoder-Bootblock hängt sich **unterhalb des Dateisystems** an `trackdisk.device`. Für AmigaDOS ist eine kodierte Diskette damit von einer normalen ununterscheidbar. **Die Fehlerklasse:** MFM-Ebene korrekt, Sektorköpfe korrekt, Prüfsummen gültig — nur der Inhalt ist verwürfelt. UFT liest bis zur Dateisystemebene fehlerfrei und meldet dort **Unsinn oder einen defekten Rootblock**. Beides ist falsch: die Diskette ist nicht beschädigt, sie ist **kodiert**. Dieselbe Klasse wie P3-20 (`dummy.bad`). **Die Signatur ist benennbar ohne das Binary zu zerlegen:** *gültige Sektorprüfsummen bei hochentropischem Inhalt und einem Dateisystem ohne Sinn.* Korruption sieht anders aus — sie trifft Struktur **und** Inhalt. **Gemessen im Baum:** Entropie gibt es (`src/analysis/uft_ml_protection.c`, Shannon über das **Fluss-Zeithistogramm**), aber **nie über Sektorinhalt**; DMS erkennt Verschlüsselung über ein **Kopf-Flag** (`uft_dms.c:805`), nicht über den Inhalt. Für Amiga-Datenträger: nichts. **Es ist eine Familie, kein Einzelfall** — die Doku nennt `CoderBoard` als Konkurrenz, also mindestens zwei Verwürfelungsschemata. Zensus-Auftrag für `uft-variants`, keine Einzelerkennung |
| P3-24 | **Die Virus-Signaturdatenbank hat 48 Namen und NULL Muster (MF-791).** Beim Nachmessen zu P3-23 aufgefallen: `src/fs/uft_amiga_virus_db.c` (508 Zeilen) führt **48** benannte Einträge — und **alle 48** haben `.pattern = NULL`. Der Scanner überspringt sie ausdrücklich (`uft_bootblock_scanner.c:91`, Kommentar `/* PENDING — skip */`). Die **Mechanik** steht also (`looks_like_m68k_code`, `uft_bb_compute_checksum`, `classify_dos_type`) — sie hat nur **nichts zum Vergleichen**. Das ist dieselbe Lage wie beim Kopierschutz-Katalog (P0-2): **Bestand, nicht Fähigkeit.** Ehrlicherweise ist es bereits notiert — `docs/XCOPY_INTEGRATION_TODO.md` nennt die Muster als „PENDING (brauchen xvs.library-Extraktion)". **Und genau die liegt in der Sammlung des Eigentümers:** `xvslibrary.lha` (103 129 B) und `.zip` (109 888 B). Dazu kommt: der Scanner hat **keinen Code-Aufrufer** (gemessen, nur `.pro` und Doku nennen ihn). Ein Dekoder-Bootblock nach P3-23 wäre damit **der erste wirksame Eintrag** dieser Datenbank — nicht ein weiterer unter vielen |
| P3-25 | **`pasti.dll` (Jorge Cwik, 2004/2006) — Zone PRÜFEN, Kanal: Oracle, praktisch versperrt.** Gemessen am Archiv `PastiDll_02h.zip`: `pasti.dll` 102 400 B plus zwei Textdateien. Der Lizenzabschnitt lautet vollständig *„Pasti.Dll is copyright (c) 2004 by Jorge Cwik. This software is provided as is. No warranty of any kind is given or implied."* — **eine Rechteeinräumung fehlt ganz**: keine Weitergabe, keine Änderung, kein Rückbau. Damit ist Port, Nachbau-Vorlage und Weitergabe ausgeschlossen; offen bleibt allein der Oracle-Kanal (unverändert ausführen, Ergebnisse als Fakten festhalten, weder Binärdatei noch Text ins Repo). **Und der ist praktisch versperrt:** die DLL ist kein Werkzeug, sondern ein Emulator-Zusatz — sie aktiviert sich nur in einem Pasti-fähigen Emulator (Steem ab 3.2, SainT ab 1.85), also eine weitere Beschaffung. Die API steht nicht in der beiliegenden Doku. **Zwei Fakten aus der lesbaren Doku sind trotzdem sofort verwertbar:** (a) *„Writes to Pasti images are lost and discarded on eject"* — belegt aus erster Hand, dass STX ein **Emulations**-, kein Preservationsformat ist; (b) Pasti würfelt die Zeitmessung absichtlich („Disable Randomize"), ein Oracle-Lauf müsste das abschalten, sonst ist er nicht reproduzierbar. **Fünfte Frage:** Cwik ist der Format-**Urheber**; UFTs STX-Leser ist ein erklärter GPL-3-Port von Louis-Guérins AIR, das seinerseits aus Rückbau plus Rücksprache mit Cwik entstand. Zwei unabhängige Implementierungen, aber eine gemeinsame Beschreibungslinie — als Oracle wäre das Grundwahrheit, nicht zweite Hand | gemessen (MF-797) | ⚠ **Fundus** — benannt wartend. Was ihn öffnen würde: ein Pasti-fähiger Emulator plus die Entscheidung des Eigentümers zum Oracle-Kanal (P3-11) |
| P3-26 | **`MSA2_3.PRG` (AZ Software) — Zone PRÜFEN, Kanal: Oracle, Emulator nötig.** Gemessen: gültiger GEMDOS-Kopf `0x601A`, 556 B Text / 25 262 B Daten / 21 378 B BSS, 25 850 B gesamt. Das Datensegment ist gepackt (die Zeichenketten liegen zerhackt), lesbar sind `D:\MSA_2_3+.PRG` und ein Copyright-Vermerk von „AZ Software". **Keine Lizenzdatei, kein Rechtevermerk** — Copyright behauptet, nichts eingeräumt. Zerlegt wurde nichts. Der Wert wäre erheblich: MSA steht auf **T2**, und seine einzige Spec-Quelle ist **SAMdisk** (`src/samdisk/msa.cpp`), also dieselbe Hand. Der Magic Shadow Archiver ist das Werkzeug, das das Format geschaffen hat — eine dritte, vollständig unabhängige Hand. Braucht einen ST-Emulator | gemessen (MF-797) | ⚠ **Fundus** — dieselbe Beschaffungsklasse wie Amiga Forever und T98-Next |
| P3-27 | **`msa` lässt sich HEUTE von T2 auf T1b heben — mit einem bereits installierten Oracle.** Gemessen: `msa` steht auf T2, seine Spec-Quelle ist SAMdisk (dieselbe Hand, MF-785 führt `msa` ausdrücklich auf SAMdisks Sperrliste). **hxcfe 2.16.13 führt `ATARIST_MSA` als RW** und steht für MSA **nicht** auf der Sperrliste (dort nur `dim_atari`, `hfe`, `hxcstream`). Damit ist der Weg derselbe wie bei `edsk` in MF-796: hxcfe erzeugt, SAMdisk liest zurück, `gen_container_corpus.py` schreibt nur bei byteidentischem Rundlauf. **Kennzahl: ungeprüfte Formate ↓.** Aufgefallen bei der Prüfung von P3-26 — die gesperrte Datei zeigte den offenen Weg | gemessen (MF-797) | ⚠ **offen, nicht blockiert** — nächster Kandidat nach `edsk` |
| P3-28 | **Vier bestätigte Greaseweazle-Befunde, nicht angefasst (MF-799).** Der Umfang war ausdrücklich auf `get_pin`+Spur-0 begrenzt; die anderen vier sind Stelle für Stelle nachgemessen und stehen offen. (a) **ClearComms fehlt unter POSIX** — `serial_reset_comms()` gibt es zweimal, die Win32-Fassung setzt `DCB.BaudRate = 10000` und zurück (`:271`), die POSIX-Fassung leert nur Puffer und ruft `tcflush()` (`:387`). Der Dateikopf führt „BUG10: Missing baud-rate reset (ClearComms)" als behoben — für Windows stimmt das, für Linux/macOS nicht, also dort, wo die CI läuft. Wirkung: nach einem mitten im Flussstrom abgebrochenen Lauf sendet das Gerät weiter, Abräumen hilft nicht, und der Benutzer sieht „Device not found" bei angeschlossenem Gerät. (b) **Stille Kürzung bei 4 MB** (`:967`) — läuft der Puffer voll, bevor der Terminator `0x00` kommt, verlässt die Schleife ohne ihn, danach wird GET_FLUX_STATUS gesendet, während das Gerät noch sendet. Gekürzter Fluss plus versetzter Strom, gemeldet wird nichts. (c) **`uft_gw_seek(…, uint8_t cylinder)`** (`:860`) — Zylinder −1 ist nicht ausdrückbar, obwohl der Flippy-Sonderweg (`NO_CLICK_STEP`, `int8_t`-Umdeutung) darunter bereits gebaut ist. Der Code ist da, die Tür fehlt. Dazu fehlt die 16-Bit-Form des Seek-Befehls. (d) **`write_precomp_ns` sechsmal gesetzt, nie gelesen** — alle Fundstellen in `uft_hal_profiles.c`, ein Profil auf 125. Alles in der **geschützten** Datei außer (d) | gemessen (MF-799) | ⚠ **offen** — Reihenfolge nach Eigentümer-Vorschlag: (a), (b), dann (c). Erst danach ist ein Bench-Lauf sinnvoll |
| P3-29 | **`pySuperCardPro` (NF6X, GPL-3.0) — der SCP-Transport ist vermutlich die Ursache, warum UFT-008 nie grün wurde.** Drei unabhängige Quellen sagen dasselbe: `scpdev.py:114` öffnet `serial.Serial(port, 250000)`; SAMdisks `SCP_USB.cpp` nutzt `/dev/ttyUSB0`; `SCP_FTDI.cpp` ruft `ftdi_usb_open_desc(0x0403, 0x6015, …, "SCP-JIM")`. **Die beiden SAMdisk-Dateien liegen im eigenen Baum**, unter `src/samdisk/`, und der Kopf von `uft_scp_direct.c` nennt SAMdisk als Gegenprobe fürs Protokoll — für den TRANSPORT hat sie nie stattgefunden. UFT öffnet stattdessen `libusb_open_device_with_vid_pid(0x16D0, 0x0F8C)` mit Bulk 0x01/0x81 (`:222-250`). PID `0x6015` ist ein FT230X/FT231X, ein FTDI-Seriellwandler: er stellt jedem 64-Byte-IN-Paket **zwei Modemstatusbytes** voran, und 250000 Baud lassen sich über rohe Bulk-Transfers gar nicht einstellen. Die Antwortlesung erwartet `[CMD_ECHO, STATUS]` und bekäme die FTDI-Statusbytes — `UFT_ERR_IO` beim ersten Befehl. **Nicht geprüft:** ob `0x16D0:0x0F8C` stimmt (EEPROM-Umbau denkbar); die Transportart steht davon unabhängig fest. `uft_scp_direct.c` ist **nicht** geschützt | gemeldet vom Eigentümer, Fundstellen nachgesehen (MF-799) | ⚠ **offen.** Erster Schritt ist transportunabhängig und kostet eine Stunde: `:157-161` wirft das Statusbyte weg („caller can read the raw status via a future status-query API" — die es nicht gibt), obwohl die Firmware **19 unterscheidbare Codes** liefert (`PR_NODISK`, `PR_WPENABLED`, `PR_NOINDEX`, `PR_NOTRK0`, …). Danach der Transportumbau nach SAMdisks drei Backends (247 Zeilen, MIT) |
| P3-30 | **`pyRT11` (NF6X, GPL-3.0) — Fundus, bewusst nachrangig.** RT-11-Dateisystem-Werkzeug für PDP-11, 1.484 Zeilen, Stand 2014, **ohne Test und ohne Spec-Dokument im Baum**. Drei Gründe für die Nachrangigkeit, und der erste wiegt: **der Autor sagt selbst, dass Daten verloren gehen** — die Speicherdarstellung führt Bad-Block-Tabellen und Blockersetzung nicht mit, „so some information is lost when manipulating many images". Das ist wörtlich die Fehlerklasse, gegen die dieses Projekt antritt; als Vorlage für einen Schreiber vom Autor selbst disqualifiziert. Zweitens steht über der Datenträgertabelle im Quelltext `# Some of these parameters are a bit questionable...` — eine Quelle, die ihre eigenen Zahlen anzweifelt, trägt keinen T2-Eintrag. Drittens fehlt die zweite Quelle ganz. **Was ich trotzdem mitnähme:** `rad50.py` (138 Zeilen, DEC Radix-50, drei Zeichen in ein 16-Bit-Wort) ist in sich abgeschlossen und die 40er-Zeichentabelle ist DEC-Dokumentation, kein Rückbau — gegen jede PDP-11-Quelle nachprüfbar. RAD50 gibt es im Baum **nirgends**. **Ausdrücklich KEIN Befund:** die Beobachtung, `src/formats/pdp/uft_pdp.c` prüfe nur die Größe, stimmt (`fs == 256256 oder 512512`, kein Blick in den Inhalt) — aber die Probe meldet dafür `*c = 40`, also Band „nur die Größe" nach MF-729, und ein struktureller Treffer schlägt sie. Sie sagt, was sie ist. `pdp`s T1b bezieht sich auf die **Geometrie** gegen `gw` (MF-784), nicht auf die Probe | gemeldet vom Eigentümer, `pdp_probe` nachgemessen (MF-799) | ⚠ **Fundus.** Was es öffnen würde: DECs *RT-11 Volume and File Formats Manual* (bitsavers) plus `simh` oder `putr` als zweite unabhängige Hand — dann liesse sich Block 1 (Home-Block: Segmentzahl, Bandname, Prüfsumme) lesen und aus „Größe passt" ein „Inhalt bestätigt" machen |
| P3-31 | **ADF-Copy: das Protokoll, das UFT spricht, gibt es nicht.** Vom Eigentümer gegen die Original-Firmware gehalten (`github.com/Niteto/ADF-Drive-Firmware`, GPLv3, Dominik Tonn, v1.111). UFT nimmt ein **binäres Ein-Byte-Opcode-Protokoll** an — im eigenen Baum nachgemessen: `adfcopy_serial_runners.h` führt `CMD_INIT (0x01)`, `CMD_READ_FLUX (0x06)`, `CMD_GET_STATUS (0x0B)`, Antworten `O`/`E`/`D`, Status-Bitmaske. Die Firmware liest laut Befund eine **ASCII-Zeile**, splittet am ersten Leerzeichen und vergleicht per Stringgleichheit gegen 67 Kommandos (`read <n>`, `get <n>`, `put <n>`, `ver`, `flux`, `weak`, `exterr`, …). Kein Byte-Opcode, keine Bitmaske; `O` gibt es nur als erstes Zeichen von `"OK
| P3-32 | **Der SCP-v3-Parser beschriftet Disk-Types systematisch falsch — und der Pfad ist erreichbar.** Vom Eigentümer gegen die offizielle Spec v2.5 (cbmstuff.com, 2024-02-11) gehalten; die Konstanten im eigenen Baum nachgemessen. `src/flux/uft_scp_parser.c:595` behauptet, die doppelte Tabelle sei entfernt und „diese hier ist die einzige" — **sie ist es nicht**: `src/formats/scp/uft_scp_parser_v3.c:562` hält eine zweite, und die ist ab der Atari-Klasse durchgehend verschoben. Gemessen: `ATARI_ST 0x10` (Spec 0x14), `PC_360K 0x40` (Spec 0x30 — 0x40 ist TRS-80 SS/SD), `TRS80 0x60` (Spec 0x40), `TI99 0x70` (Spec 0x50), `ROLAND 0x80` (Spec 0x60), `AMSTRAD 0x90` (Spec 0x70), `OTHER 0xC0` (Spec 0x80). Das ist keine Lücke, sondern **aktive Fehlbeschriftung**: ein echtes TRS-80-Abbild wird als „PC 360K" ausgegeben, ein echtes PC-1.44 (0x33) fällt in `default` → „Unknown". `scp_get_expected_sectors()` hängt an denselben Konstanten. **Der Pfad ist nicht tot:** `UnifiedFloppyTool.pro:753` baut die Datei, `uft_v3_bridge.c:302` hängt sie als `uft_scp_v3_handler` ein, `uft_advanced_mode.c` ruft daraus `open`, `detect_protection`, `get_geometry`. **Warum das Tor es nicht fing:** `tests/test_scp_readers_agree.c` (MF-439/440) ist genau gegen diese Drift gebaut, vergleicht aber **Flussintervalle** — Disk-Type, Auflösung, Overflow, Seiten und Prüfsumme liegen außerhalb seiner Zusicherung. Da ist die Lücke, und sie gehört geschlossen, bevor die Tabelle repariert wird | gemeldet vom Eigentümer, Konstanten nachgemessen (MF-809) | ⚠ **offen, vier weitere Befunde am selben Parser, alle nachgemessen.** (a) **Der Overflow-Marker wird zum Kurzimpuls.** Ein Bitcell-Wert `0x0000` heißt laut Spec: ≥ 65536 × 25 ns ohne Übergang, der nächste Wert wird um 65536 erhöht. `uft_scp_parser.c:564` macht das richtig; `scp_calc_flux_stats()` in `uft_scp_parser_v3.c:845` behandelt es **gar nicht** — die 0 geht in `min_flux`, in Summe und Mittel, landet in Histogramm-Bin 0 und zählt als `short_count`. Ab `short_count > flux_count/10` feuert `SCP_DIAG_FLUX_TOO_SHORT` und zieht `flux_score`. **Das kehrt die Bedeutung um:** ausgerechnet die geschützten Spuren, die das Werkzeug erkennen soll, werden als Schrott bewertet — je stärker der NFA-Schutz, desto schlechter die Note. (b) **`side_count` hängt am falschen Bit.** `:1345` rechnet `(flags & 0x01) ? 2 : 1`; FLAGS Bit 0 ist laut Spec **INDEX**. Die Seitenzahl steht in Byte 0x0A (0 = beide, 1 = nur Seite 0, 2 = nur Seite 1) und wird zwei Zeilen vorher als `disk->heads` eingelesen — **gemessen: `disk->heads` erscheint an genau zwei Stellen, `:1109` lesen und `:1433` zurückschreiben, nie auswerten.** Dazu die Regel fürs Iterieren: bei einseitigen Abbildern sind die TDH-Einträge nur jede zweite belegt, Seite 0 gerade, Seite 1 ungerade. (c) **Auflösung ignoriert.** Byte 0x0B ist ein Multiplikator, Tick = 25 ns × (Wert+1). `uft_scp_parser.c:233` rechnet das; der v3-Parser liest `disk->resolution` (`:1110`) und rechnet dann überall mit festem `SCP_TICK_NS` — gemessen an `:531`, `:538`, `:555` (`scp_calc_rpm`). Für jedes Fremdgerät-Abbild mit `resolution > 0` ist damit jede Zeitangabe und jede RPM falsch. (d) **Extended Mode und Prüfsumme.** `SCP_FLAG_EXTENDED` ist `:86` definiert und wird **nirgends abgefragt** — bei gesetztem Bit 6 beginnt die TDH-Tabelle bei 0x80 statt 0x10, `scp_parse_offsets()` startet unbedingt bei 16 und läse 0x70 Byte reservierten Raum als Offset-Tabelle. Die Prüfsumme wird `:1113` eingelesen und nie geprüft; `uft_scp_verify_checksum()` in `uft_scp_parser.c:692` hat sogar die Spec-Ausnahme (bei gesetztem MODE-Bit ist das Feld definitionsgemäß 0) |
| P3-33 | **Zwei SCP-Regeln, die nur im Herstellerforum stehen — beide für ein Forensikwerkzeug tragend.** (1) **Ein Firmware-Fehlerfenster im Bestand.** Jim Drew bestätigt im Thread „SCP format – disk type confusion" (tid=861), dass die SCP-Software zwischen **2020-06-16 und 2021-11-13** IBM-1.44-Abbildern `0x45` statt `0x33` in den Disk-Type schrieb — eingeschleppt mit der Unterstützung für MFM-Festplatten und Bandlaufwerke. **`0x45` ist ein gültiger Wert, kein erkennbarer Müll.** Jedes PC-Abbild aus diesen siebzehn Monaten trägt also einen falschen Disk-Type. Das ist eine **Provenienz-Regel für den Korpus**, keine Codefrage: ein SCP-Abbild unbekannten Alters mit `0x45` ist nicht zwingend defekt. Drews eigene Einordnung: der Disk-Type sei für Programme gedacht, die ihn brauchen, aber jedes Format lasse sich allein aus den Daten dekodieren. (2) **„Bitcell-Summe > Index-zu-Index" ist NORMAL** (Thread „Reference of first flux transition", tid=868). Die aufsummierten Flusszeiten einer Umdrehung dürfen die Index-zu-Index-Zeit über- wie unterschreiten; Ursache sind ungültiger Fluss und Drehzahlschwankungen. Start und Stopp hängen am Indexpuls, das Lesen beginnt und endet **mitten in einer Bitcell** — erste und letzte fallen deshalb typischerweise deutlich kürzer aus. **Das ist eine VERBOTSREGEL:** eine Plausibilitätsprüfung „Summe muss zur Indexzeit passen" würde jedes zweite echte Abbild als defekt melden. Der Parser prüft es heute nicht — gut so, und die Regel steht hier, damit sie niemand später einbaut. Nebenbei zum SPLICE-Modus: er sucht in der zweiten Umdrehung einen ungültigen Flusswert und endet dort; findet er keinen, schreibt er die volle Umdrehung | gemeldet vom Eigentümer aus dem Herstellerforum (MF-809) | ⚠ **Fundus, zum Festhalten.** Nicht angesehen und ausdrücklich offen: „SuperCard Pro Advanced" (tid=1223 Dez 2025, tid=1240 Feb 2026) — eine neue Geräterevision, die den SCP-Direct-Provider und die Opcode-Behauptung berührt; das SDK (tid=93); „Device initialisation" (tid=295); Keir Frasers „Weird problem writing weak flux pattern" (tid=282), das zum gesperrten Schreibpfad passt |
| P3-34 | **Vier STX-Leser im selben Bau, drei davon erfunden — und der MSA-Zwilling liest RLE verkehrt.** Vom Eigentuemer gegen Louis-Guerins *Pasti File Documentation* v0.5 (2014) gehalten. `src/formats/stx/uft_stx_air.c` ist der belegte Port desselben Autors (GPL-3, MF-698) und **korrekt**. Daneben: `src/formats/atari/uft_stx_parser.c:14` beruft sich auf eine *„Pasti STX specification v1.0"* — **die es nicht gibt**. Der 16-Bit-Flags-Wert im Track-Deskriptor wird dreifach widersprüchlich gelesen; Bit 0 ist der **strukturelle Schalter** des Formats („Sektor-Deskriptoren folgen"), und beide falschen Parser hängen dort etwas anderes auf. `uft_stx_parser_v2.c` liest die Seite aus Byte 15 (laut Spec `trackType`, unbenutzt) statt aus Bit 7 der Tracknummer, addiert beim Weiterschalten 16 + Fuzzy-Umfang **zusätzlich** zu `recordSize`, liest nie einen Sektor-Deskriptor — und meldet `valid = true`. `uft_stx_plugin.c:54` liest `track_count` als LE16 an Offset 10; das ist **ein Byte**, Byte 11 ist `revision` — bei Revision 2 werden aus 160 Spuren 672. `src/formats/atari/stx.c:30` prüft auf `"STX"`, die Kennung ist `"RSY\0"` — die Funktion weist damit **jede echte STX-Datei ab** und setzt im Erfolgsfall `flux_supported = true`, was für ein Sektor/Track-Image-Format ohnehin falsch ist. **MSA:** `uft_msa_parser_v2.c:180` liest Zähler und Datenbyte **vertauscht** (Spec: Marker, Datenbyte, Längenwort). Gemessen: `uft_msa_plugin.c:43` macht es RICHTIG, und seit MF-810 ist das an einer wirklich komprimierten Fremddatei belegt (15 484 statt 774 490 B). Der v2-Parser steht im Bauplan (`.pro:1416`) und hat **keinen Aufrufer** — Bestand, nicht Fähigkeit. Der Kompressor daneben schreibt in derselben falschen Reihenfolge, ein Rundlauf-Test merkte also nichts | gemeldet vom Eigentümer, MSA-Teil nachgemessen (MF-810) | ⚠ **offen.** Nächster Schritt ist Aufräumen, nicht Recherche: prüfen, welche der drei überhaupt Aufrufer haben, die toten entfernen, die lebenden auf den AIR-Port umhängen. **Forumswissen zum Festhalten:** Bit 5 der Track-Flags gilt in der Spec als unbenutzt, ist aber in den meisten neueren STX gesetzt, und wer es beim Schreiben weglässt, bringt Pasti.DLL zum Fehllesen — also durchreichen statt verwerfen. Und die Bytes vor dem ersten Sync sind **phasenabhängig mehrdeutig** (0x4E kann als 16 verschiedene Werte dekodieren); bei Barbarian und Falcon ist genau dieser Wert Teil des Schutzes, ein Dump-Vergleich darf ihn also nicht als Abweichung werten |
| P3-35 | **HFE: die Schreibschutz-Polarität ist in BEIDE Richtungen invertiert — an einer von HxC selbst erzeugten Datei gemessen (MF-811).** `hxcfe 2.16.13` schreibt: `write_allowed = 0xFF`, `single_step = 0xFF`, alle vier `track0`-Bytes `0xFF`, Füllbytes `0xFF`. Die Spec sagt `0x00 = write protected, 0xFF = unprotected` und „Unused header bytes must be set to 0xFF". UFT: `uft_hfe.c:513` liest `read_only = (write_allowed == 0xFF)` — markiert also genau die **freigegebenen** Abbilder als schreibgeschützt; `:577` schreibt `0x00` mit dem Kommentar „Schreiben erlaubt", erzeugt also für HxC und Gotek **schreibgeschützte** Dateien. Beide Fehler heben sich innerhalb von UFT auf, ein Rundlauf sieht nichts. **Der Gegenbeleg liegt im eigenen Baum:** `src/samdisk/hfe.cpp:274` schreibt `0xff` und hat `:24` den richtigen Kommentar — der Port ist spec-konform, die native Umsetzung nicht. Drittes Format mit genau dieser Konstellation. **Weiter:** `hfe_header_t header = {0}` (`:565`) setzt alle vier `track0`-Bytes auf `0x00` = „alternative Kodierung anwenden" mit `encoding 0x00` = ISOIBM_MFM — UFTs HFEs weisen den Emulator also **aktiv an**, Spur 0 beidseitig auf IBM-MFM zu zwingen; bei FM- oder Amiga-Spuren wird sie still umgebogen. `altencoding` wird beim Lesen **nirgends ausgewertet** (der TRS-80-Fall im HxC-Forum, t=1587, ist genau dafür da). `format_revision` wird nicht geprüft, obwohl der eigene Variantenkommentar (`:1024`) damit argumentiert, dass beide unabhängigen Leser `revision != 0` ablehnen — eine ExtHFE liefe als v1 durch, samt ihrer eingebetteten Opcodes als Zelldaten. `HFEV3_SETBITRATE` (`:197`) wird als Zwei-Byte-Opcode übersprungen und der Parameter weggeworfen: eine v3-Spur mit Bitratenwechseln kommt als gleichförmiger Bitstrom heraus, **ohne Diagnose** — anders als bei den Weak-Bits, wo die Ehrlichkeit ausdrücklich eingebaut ist | gemeldet vom Eigentümer, Kopf an HxC-Datei nachgemessen (MF-811) | ⚠ **offen.** Der Weg ist benannt: ein **Polaritätstest, der nicht über den Rundlauf geht** — die HxC-erzeugte Referenzdatei einlesen und `write_allowed`, `single_step` und die vier `track0`-Bytes gegen die Spec-Bedeutungen prüfen. Genau die Klasse, die der Korpus-Ansatz fängt und ein Selbstvergleich nie. **Zum Schützen, nicht als Fund:** die veröffentlichte Spec beschreibt `RAND 0xF4` fälschlich als „Do nothing" (hineinkopiert von NOP); UFTs `HFEV3_RAND = weak/fuzzy` folgt dem Loader und ist RICHTIG. Das gehört im Code vermerkt, damit es niemand später „korrigiert" |
| P3-36 | **G64: die Speed-Offsets werden weggemaskt, und beide Tabellen sind um eine Halbspur verschoben.** `uft_g64_parser_v3.c:1331` rechnet `speed_zones[half_track] & 0x03`. Die Spec (Schepers/Brenner/Moser Rev. 1.6) ist eindeutig: **< 4 ist eine Zone, > 4 ist ein echter Dateioffset** auf einen 1982-Byte-Block, in dem jedes Byte in vier 2-Bit-Feldern die Geschwindigkeit von vier GCR-Bytes trägt. Ein Offset `0x00005E1C` wird so zu Zone 0 — ohne Diagnose. Die Fallunterscheidung fehlt **vollständig**. Und weil `expected_size` (`:1333`) aus derselben falschen Zone kommt, meldet die Größenprüfung anschließend noch einen „kurzen" Track als Zugabe. Genau die Abbilder, für die das Format existiert, verlieren hier ihre Information. **Indizierung:** die Leseschleife (`:1285`) füllt Indizes 0..83, der Verbraucher (`:1331`, `:1335`) liest `[half_track]` mit 1..84. Dateislot 0 ist Track 1.0 — Halbspur 1 liest also Track 1.5, und Halbspur 84 liest einen nie beschriebenen Index. Der Writer (`:1518`) macht denselben Fehler, also schweigt der Rundlauf; bei `ht = 84` schreibt er zudem über das Tabellenende in den ersten Speed-Eintrag bzw. in die Trackdaten. Der Kommentar darüber feiert, dass MF-119 genau so einen Off-by-one in der **Leseschleife** behoben hat — der Verbraucher blieb 1-basiert. **Bericht:** `:823` zeigt `g64_get_speed_zone(full)`, also die aus der Tracknummer abgeleitete SOLL-Zone, nicht den in der Datei stehenden Wert aus `track->speed_zone`. Für eine geschützte Diskette meldet der Bericht damit, was dort stehen sollte, nicht was dort steht. **D64:** `uft_d64_parser_v3.c:526` kennt vier Größen (35/40 Spuren, je mit und ohne Fehlerbytes); **42 Spuren (205 312 bzw. 206 114 B) fehlen und werden abgewiesen** — obwohl `src/formats/cbm/uft_cbm_geometry.c:25` die Spuren 36–42 ausdrücklich als den Bereich führt, den kopiergeschützte Disketten nutzen. Das Geometriemodul weiß es, der Parser nimmt die Datei nicht an; der GMA88-Schutz auf Spur 38 ist der belegte Anwendungsfall (forum64 t=83663). Dazu `:553` `d64_file_type_str()` mit `& 0x07`: die CBM-DOS-Bits 6 und 7 sind **Sperr- und Abschlusskennung** — ein nicht geschlossener Splat-File und ein gesperrter Eintrag sind für eine forensische Verzeichnisausgabe genau die interessanten Fälle | gemeldet vom Eigentümer, Fundstellen nachgemessen (MF-811) | ⚠ **offen.** Wieder ein Test, der **nicht** über den Rundlauf geht: eine mit `nibconv` oder `g64conv` erzeugte G64 einlesen und prüfen, ob Track 1.0 in Slot 0 landet und ob ein Speed-Eintrag > 4 als Offset erkannt wird. **Haltungsvorbild im eigenen Baum:** `uft_cbm_geometry.c` lässt den 2040-Gap-Wert bewusst auf 0 mit der Begründung „no authoritative source found" — dieselbe Haltung auf die Speed-Offsets angewandt heißt: ein Offset, den man nicht auflöst, ist **kein Zonenwert 0**, sondern ein „nicht ausgewertet" mit Diagnose |
| P3-37 | **TD0: die Kompressionserkennung ist invertiert, und das Kommentar-Flag steht am falschen Byte — beides gegen den eigenen Baum belegt.** `uft_td0_parser_v2.c:411` rechnet `advanced_compression = (signature == TD0_SIG_NORMAL)` mit `TD0_SIG_NORMAL 0x4454 /* "TD" - advanced compression */`. `src/samdisk/td0.cpp:10-11` sagt das Gegenteil: `"TD" // Normal compression (RLE)`, `"td" // Huffman compression`. UFT jagt also jede gewöhnliche **TD**-Datei durch den LZSS-Dekompressor und liest jede komprimierte **td**-Datei roh — beide Richtungen falsch. Dieselbe Datei sagt es an **drei Stellen unterschiedlich**: der Dateikopf (`:15`) hat es richtig, der Konstantenname sagt „NORMAL", der Kommentar daneben „advanced". **Zeile 412:** `has_comment = (drive_type & TD0_FLAG_COMMENT)` — das Bit sitzt laut samdisk (`:28`) in **bTrackDensity**, nicht im Laufwerkstyp. Und fünfzehn Zeilen darüber steht es bereits richtig: der **MF-460-Kommentar** hält fest, dass dasselbe Byte im Baum dreifach gelesen wurde, entscheidet sich für die samdisk-Lesart und nennt `uft_td0_lzss.c:469` als Bestätigung — **und die macht es tatsächlich richtig** (`if (img->header.stepping & 0x80)`). Der Befund wurde aufgeschrieben, das Feld umbenannt, der Kommentar gesetzt, **und die Codezeile darunter nie angefasst.** Nebenwirkung: `drive_type &= ~TD0_FLAG_COMMENT` löscht Bit 7 dort, wo nichts zu löschen ist, und verfälscht den Laufwerkstyp, sobald er ≥ 0x80 ist. **Drittens:** samdisk leitet die Kodierung aus `tt.head & 0x80` ab (FM/MFM); `uft_td0_parser_v2.c` führt `head` unmaskiert und wertet das Bit nicht aus, definiert stattdessen `TD0_ENC_FM/MFM`, als gäbe es ein eigenes Kodierungsbyte — das es nicht gibt. Eine FM-Spur bekommt Kopfnummer 0x80 statt 0, und Einfach-/Doppeldichte geht verloren | gemeldet vom Eigentümer, alle drei im Baum nachgemessen (MF-813) | ⚠ **offen.** Für den Nachweis braucht es **keine externe Quelle**: `src/samdisk/td0.cpp` liegt neben `src/formats/td0/uft_td0_parser_v2.c`, und der eigene MF-460-Kommentar zitiert es bereits. **Vierter Punkt, NICHT belegt:** `:423` rechnet `sides = header.sides + 1`; samdisk dokumentiert das Byte als `bSurfaces` („sides stored in the image"), also bereits eine Anzahl — wertet es aber nirgends aus, es gibt also keinen internen Gegenbeleg. Gehört an einer echten TD0-Referenz geprüft, nicht geraten. **Und zum Festhalten:** die LZH-Ringpuffer-Initialisierung unterscheidet sich zwischen Teledisk vor und ab 1.5 — ein klassischer Stolperstein, `uft_td0_lzss.c` einen Blick wert. **IMD dagegen ist sauber:** `uft_imd_parser_v2.c` gegen die Formatbeschreibung durchgesehen — Sektortypen 0x00–0x08 inklusive der Unterscheidung ungerade = Volldaten / gerade = Füllbyte, Modi 0x00–0x05, `CYL_MAP 0x80` und `HEAD_MAP 0x40` mit Maskierung vor der Weiterverwendung, `128 << size_code` mit Obergrenze. **Kein Befund** — das erste Format dieser Reihe ohne einen |
| P3-38 | **Die C64-Schutzerkennung rät Markennamen, und das Unterscheidungsmerkmal fehlt ganz.** Der physikalische Fakt (Anti-Cracker-Buch, C64-Wiki): der Lesekopf der 1541 kennt 80 Halbspuren, der **Schreibkopf nur 40 ganze** — er ist zu breit und überschreibt beim Schreiben immer eine benachbarte Halbspur. Das Merkmal ist also die **Nachbarschaft**, nicht die Existenz von Halbspuren: 1.5/17.5/35.5 ist etwas völlig anderes als 17.0/17.5/18.0, und nur das zweite ist der unschreibbare Fall. `uft_g64_parser_v3.c:1606` zählt `half_tracks_with_data > 2` und vergibt „Half-track protection" mit 0,80 — **nachgemessen: es gibt im ganzen Baum keine Stelle, die Nachbarschaft oder Kopfbreite kennt.** Drei verstreute Halbspuren bekommen dasselbe Etikett. **`g64_detect_protection()` (`:1549`) vergibt fünf Markennamen an Schwellwerten ohne jede Quelle** — dieselbe Klasse wie die fünf erfundenen Format-Parser (FMT-2/3/10/11/12), nur in der Schutzerkennung: „Vorpal/RapidLok" bei 0,90 aus `weak_tracks > 0 && half_tracks > 0` (zwei verschiedene Systeme unter einem Label, und die zwei Zähler könnten sie nicht unterscheiden); „Epyx FastLoad" bei 0,75 — ein **Modul-Schnelllader**, kein Diskettenschutz, also ein Kategorienfehler; „V-Max!" bei 0,85 aus **einem** Weak-Bit, gelesen aus `tracks[g64_full_to_half(20)]` = Index 39, was mit dem Off-by-one aus P3-36 die Spur **20.5** ist, nicht 20. Die Reihenfolge macht spätere Regeln unerreichbar: V-Max läuft vor „General weak bit" und vor „Half-track". Schwellen (>5, >3, >2, >0) und Konfidenzen (0,90/0,85/0,80/0,75/0,70/0,65) sind nirgends hergeleitet — auffälliger Bruch in einem Baum, der den 2040-Gap-Wert lieber auf 0 lässt, als ihn plausibel zu raten | gemeldet vom Eigentümer, alle Stellen nachgemessen (MF-815) | ⚠ **offen.** **Zwei weitere Befunde, beide bestätigt:** (a) `uft_format_convert_bitstream.c:690` rechnet `halftrack = track_num * 2` — der v3-Parser rechnet `n*2-1` und hält **gerade** Nummern für Halbspuren. Das ist die **dritte inkompatible Halbspur-Indizierung** im Baum; der Kopf von `uft_cbm_geometry.c` beklagt genau das für die Geometriearrays („three incompatible indexing conventions"), für die Halbspuren wurde die Aufräumarbeit nie gemacht. (b) Der MF-555-Kommentar an derselben Stelle sagt, `g64_set_track()` weise „Halbspuren und zu lange Spuren ab" — nachgemessen prüft `uft_d64_g64.c:722` **nur** `halftrack < 2 || halftrack >= G64_MAX_TRACKS`. Keine Halbspur-Abweisung, keine Längenprüfung. MF-555 hat richtig behoben, dass der Rückgabewert verworfen wurde; die **Begründung** dafür beschreibt ein Verhalten, das es nicht gibt. **Der Weg ist benannt:** Beobachtungen statt Namen — „Halbspuren belegt: 17.5, 18.0, 18.5 (benachbart — mit einer 1541 nicht rückschreibbar)" sagt mehr und behauptet weniger als „V-Max!, 0.85". Die Nachbarschaftsprüfung sind drei Zeilen; die Information liegt bereits in `track->is_half_track`. **Und eine Provider-Lücke dazu:** geschützte Spuren lassen sich mit einer 1541 nur über **Parallelkabel** vollständig lesen, mit einer 1571 auch seriell — nibtools prüft das zur Laufzeit. Der XUM1541-Provider kennt die Unterscheidung nicht. Ein Benutzer mit serieller 1541 bekäme ein Urteil über Daten, die sein Aufbau nicht erfassen konnte |
| P3-39 | **Die Belegbasis gegen P3-38: alle drei Markennamen sind an der Quelle widerlegt, und eine Regel feuert auf UNGESCHÜTZTEN Originalen.** Quelle: Peter Rittwage (Autor von **NIBTools**), `rittwage.com/floppy/dp.php?pg=protection_cbm` — **16 Klassen** mit Mechanismus und benannten Urhebern. **(a) „Vorpal/RapidLok" 0,90** aus `weak_tracks > 0 && half_tracks_with_data > 0`: Rapidloks Merkmal sind laut Quelle **Bad GCR / Weak Bits auf allen Titeln**, von Halbspuren keine Rede; Vorpal (spät) ist durch **Long Tracks und Spuren ohne Sync-Marken** gekennzeichnet — weder Weak Bits noch Halbspuren. Die Bedingung passt auf **keine von beiden**. Und Halbspuren sind ausdrücklich selten („sehr wenige Titel"), also ist die zweite Hälfte der Konjunktion ein **Ausschluss**kriterium für fast alles. **(b) „V-MAX!" 0,85** aus einem Weak-Bit auf Spur 20: der Schutz (Harald Seeley, 1985–1988) hat mindestens **drei Versionen**; die späten nutzen **extrem kurze Syncs**, die Nibbler übersehen — das **Gegenteil** dessen, was `has_long_sync` misst. Von Spur 20 steht nichts. **(c) „Epyx FastLoad" 0,75:** Rittwage nennt für Epyx den **Vorpal**-Loader, und zwar für Long Tracks und Spuren ohne Sync. FastLoad ist ein **Steckmodul**-Schnelllader — Name, Zuordnung und Richtung des Merkmals stimmen alle drei nicht. **(d) DER SCHÄRFSTE PUNKT — `weak_tracks > 3` → „Weak bit protection" 0,70 (`:1599`) meldet auf normalen Originalen.** Bad GCR ist dasselbe wie unformatiert, und weil Originale nicht auf einer 1541 dupliziert wurden und die Spuren nicht voll ausgeschrieben sind, hinterlassen die **Duplizierautomaten am Ende praktisch aller Spuren** unformatierte Bereiche. Mehr als drei Spuren mit Weak-Bit-Befund ist damit der **Normalfall einer kommerziell duplizierten Diskette**, nicht ein Schutzhinweis. Die brauchbare Unterscheidung liefert die Quelle mit: **am Spurende = Artefakt, mitten in der Spur an gezielt abgefragter Stelle = Schutz.** Der Detektor kennt nur die Anzahl | gemeldet vom Eigentümer aus der NIBTools-Quelle, Regel im Baum nachgemessen (MF-818) | ⚠ **offen — hier liegt der Ersatz für die fünf Etiketten.** **Vier Zahlen zum Eichen statt Erfinden:** (1) **~7700 Byte/Spur** ist das physikalisch Schreibbare bei Höchstdichte und 300 U/min; alles darüber ist Long-Track-Schutz und **nicht bei Normaldrehzahl zurückschreibbar**. `G64_MAX_TRACK_SIZE 7928` ist die **Containergrenze** der G64-Spec, eine andere Größe — und die einzige Längenprüfung (`:953`, `gcr_size > expected * 1.1f`) fragt „länger als das Zonen-Soll", nicht „länger als schreibbar". (2) **298,5 U/min oder darunter** lässt EA-/XEMAG-Skew-Schutz durch — Bench-Parameter, gehört in `BENCH_PROTOCOL.md`, nicht in den Parser. (3) **Sync: mindestens 10 Eins-Bits, üblich 40, keine Obergrenze** — damit hat `has_long_sync` erstmals eine Bezugsgröße. (4) **≥ 3 Null-Bits in Folge** ist die Auslöseschwelle für das Phantom-Bit. **Zwei Herkunftssignale, die der Baum bisher gar nicht hat:** `0x01`-Läufe dort, wo Weak Bits sein müssten, sind **Burst-Nibbler-Ersatz** — also **kein Original**, und sie dürfen nicht als Weak-Bit-Beleg zählen; durchgehend `0x55`-gefüllte Gaps verraten den **Kopierer-Nachbau**. Zusammen ergibt das eine **Echtheitsprüfung des Abbilds**, unabhängig von der Schutzerkennung. **Referenzkorpus:** Rittwages Disk Database (3625 archiviert, 1419 verifiziert) sagt, welcher Titel welchen Schutz trägt |
| P3-40 | **Zwei stille Kappungen, beide im jeweils GUTEN Port.** `uft_stx_air.c:289` und `:514` klemmen auf `STX_MAX_SECTORS 32` (`if (nsec > STX_MAX_SECTORS) nsec = STX_MAX_SECTORS;`) — ohne Warnung, ohne Diagnose, ohne Rückgabewert. Louis-Guérin nennt für NOS ausdrücklich **Sherman M4 mit 70 Sektoren je Spur** und hält fest, dass 12 oder mehr Sektoren zwingend auf Tricks hindeuten. Ein Sherman-M4-STX verlöre **38 von 70** Sektoren, und die Datei gälte als gelesen — fünf Zeilen unter dem „Kein Bit verloren" im Dateikopf. Der STX-Track-Deskriptor führt `sectorCount` als `uint16`, die Grenze liegt also **nicht im Format**. Dieselbe Klemme ein zweites Mal: `uft_ipf_air.c:615` mit `IPF_MAX_BLOCKS 16`, während die IPF-Doku zum IMGE-Feld ausdrücklich sagt *„Usually (but not necessarily) this is the number of sectors"* — „not necessarily" ist der ganze Punkt. Beide Male trifft es den `_air`-Port, also die jeweils **korrekte** Implementierung, aus derselben Portierungsarbeit | gemeldet vom Eigentümer, beide Stellen nachgemessen (MF-824) | ⚠ **offen.** Nicht die Zahl ist der Fehler, sondern die **stille** Kappung. Der Weg ist derselbe wie bei `EDSK_TRACK_LEER` (MF-796): melden statt verschweigen |
| P3-41 | **`uft_fuzzy_bits.c` ist das bestbelegte Schutzmodul des Baums — mit vier Mängeln.** Vom Eigentümer vollständig gegen seine Quelle verifiziert (`atari.8bitchip.info`, Fassung 1.3 vom 2020-12-20, aufgebaut auf Louis-Guérins Discovery-Cartridge-Analyse): **acht Konstanten, alle korrekt** — PACE/FB-Marker mit Längenpräfix, „Seri", Seriennummer bei 0x0D, CRC bei 0x11, CRC-8 poly 0x01 init 0x2D, Fuzzy-Bytes 0x68/0xE8, Spur 0 Sektor 7 und Sektor 247, Endmarke bei 0x1FE/0x1FF. **Aber:** (F1) das TOR davor liest nur ZWEIMAL — `uft_has_fuzzy_bits()` vergleicht zwei Lesungen und wertet jeden Unterschied als Fuzzy; die 5-Lesungen-Analyse läuft nur, wenn dieser schwächste Test schon ja gesagt hat. Genau die Konstruktion, vor der ijor warnt (Soft Errors sind häufiger als gedacht). Fehlgeschlagene Lesungen werden per `continue` verworfen, ohne Vermerk. (F2) `variation_count` erhöht sich nur, wenn sich das **Intervall erweitert** — ein Byte, das zwischen zwei bekannten Werten springt, zählt nie wieder; praktisch zwei Inkremente, dann steht der Zähler. Und `min`/`max` erfassen die Spannweite, nicht die **Maske** — die Pasti-Doku gibt die Formel `Mask[o] = ~(read1[o] ^ read2[o])`, und genau die braucht man, um einen Fuzzy-Sektor nach STX zu schreiben. (F3) `DM_FUZZY_BYTE_NORMAL 0x68` und `FLIPPED 0xE8` sind **definiert und nirgends benutzt** (nachgemessen: 2 Treffer, beide die Definitionen). Das Spiel selbst akzeptiert nur diese zwei Werte — damit wäre die Trennung Fuzzy/Lesefehler geschenkt. (F4) der Fuzzy-Bereich steht **im Sektor deklariert**: Wort bei 0x12 = `E9 01` = 489 Byte ab 0x14; der Code durchsucht stattdessen alle 512 gleichförmig. Die 20 Kopfbytes und drei Schlussbytes sind per Definition nicht fuzzy | gemeldet vom Eigentümer, F3 nachgemessen (MF-824) | ⚠ **offen, mit fertigem Material.** Die Quelle veröffentlicht **86 (Seriennummer, CRC)-Paare** — eines davon ist seit MF-821 im Test (`CA 08 00 00 -> EF`, DM 1.2 English) und **trifft**; die Stelle für die übrigen 85 ist markiert. Dazu drei fertige Fixtures: der vollständige 512-Byte-Bootsektor (Prüfsumme über 256 BE-Worte muss $1234 ergeben — direkter Vektor für `st_boot_checksum()`), der geschriebene Sektor 7 und ein gelesener Sektor 7 mit den 68→E8-Kippungen. **Zwei Reichweiten-Befunde:** die Amiga-Fassungen von DM/CSB tragen denselben Schutzsektor auf Spur 0 Seite 1 als **10-Sektoren-Atari-ST-Spur**, Sektor 1 (DM 2.x) bzw. 2 (CSB 3.x) — die fest verdrahtete Sonde auf T0/S7 verfehlt sie alle. Und daraus folgt ein ADF-Befund: **eine Amiga-DM-Diskette enthält eine Spur, die kein Amiga-MFM ist** — ADF kann sie nicht tragen, sie fällt beim Erzeugen durch oder bleibt still leer, und der ADF-Pfad hat dafür keine Diagnose. Nebenbei erkennt die Sonde **Oids** mit, ohne es zu nennen |
| P3-42 | **DIM und STT: drei Befunde aus DrCoolZics „Atari Image File Formats" V1.0 (09/2014).** (a) `uft_dim_atari.c` nennt 0x0E–0x1F „Reserved (zero padded)"; das Dokument sagt **18 Byte BPB-Kopie** — 0x0E + 18 = 0x20 füllt den Header exakt. Das ist eine **zweite, unabhängige Geometriequelle in der Datei**, genau die Sorte In-Band-Wahrheit, die `uft_st.c` seit MF-462 als Erstquelle nutzt, und sie liegt ungelesen herum. (b) `hdr[0x03]` wird nirgends ausgewertet. Es gibt „komprimierte" DIMs, die **nur belegte Sektoren** enthalten; zum Entpacken muss die FAT ausgewertet werden. Der MF-687-Kommentar argumentiert **richtig** gegen Hataris Ablehnung („refusing them here would turn a reader limitation into a false verdict") — nur folgt daraus nicht, die Datei als Linearabbild zu lesen, und genau das tut `dim_atari_open()`. Es gibt einen dritten Zweig zwischen Ablehnen und Falschlesen: annehmen, Flag lesen, mit Diagnose verweigern. (c) **`stt.c:70` speichert die Offsettabelle vermutlich transponiert:** der Code rechnet `t * num_sides + side` (spurweise verschränkt), das Dokument listet drei ausgeschriebene Einträge **alle für Seite 0** und schließt mit `Track NumTracks-1 Side NumSides-1` — also seitenweise. Bei einseitigen Abbildern fallen beide Konventionen zusammen; bei zweiseitigen ist ab der zweiten Spur jeder Zugriff auf der falschen Seite. Der Kommentar darüber („sequential by track/side") deckt die Zweideutigkeit zu, statt sie aufzulösen. **Bemerkenswert:** .ST und .MSA speichern nachweislich spurweise abwechselnd — zwei Formate desselben Umfelds, zwei Konventionen | gemeldet vom Eigentümer (MF-824) | ⚠ **offen.** (c) ist der einzige Punkt mit Restzweifel — die drei ausgeschriebenen Zeilen sind der einzige direkte Beleg, danach steht „….". An einem **zweiseitigen STT** in zehn Minuten entschieden; FloImg schreibt STT. **Und ein ungenutzter Mechanismus:** das Format definiert für Bits 2–15 je einen „Undefined section"-Block mit `W Offset to End of Section`, damit ein Leser Unbekanntes überspringen kann. `stt.c` liest nur den Sektorabschnitt und gibt sonst `UFT_ENOTSUP` — es kann also einen Sektorabschnitt **hinter** einem unbekannten nicht erreichen. Das Format wurde ausdrücklich für Vorwärtskompatibilität gebaut. **Namenskollision:** „DIM" heißt auf dem Atari zweierlei — FastCopy (32-Byte-Header, `BB`-Magic, dreifach belegt) und **E-Copy** (byteweise identisch mit .ST, ohne Header). Das Modul deckt nur FastCopy ab; der Schaden ist Fehlbenennung, kein Datenverlust, weil die Erkennung inhaltsbasiert läuft und ein E-Copy-DIM beim ST-Plugin landet |
| P3-43 | **Alle WD1772-Zeiten hängen am Takt — und „der WD1772" gibt es nicht.** Aus „Build your own DD-HD switch for the Atari ST/STE" (Martin Graiter 1997/98, überarbeitet von Vezz): ein auf 16 MHz übertakteter WD1772 halbiert **alle** Zeiten. *„Instead of getting 3ms we get 1.5ms … To rectify this we set the steprate to 6ms."* Damit gilt David Smalls Steprate-Tabelle (00 = 6 ms, 01 = 12, 10 = 2, 11 = 3) **nur bei 8 MHz**, und seine Empfehlung „für 3,5″ immer 11" ist auf einer umgebauten Maschine falsch. Ebenso die 32 µs je Byte beim Write Track — bei 16 MHz sind es 16. **Und Revisionen unterscheiden sich:** *„If your chip has the marking WD1772 02-02 it is OK … but if your chip says VLSI or WD1772 00-00 it will most likely NOT function."* Das begrenzt die gesamte Atari-Messbasis: DPLL-Toleranz 9–10 %, CRC-Vorladung auf `$CDB4`, der Read-Track-Durchlaufbug, „genau drei $A1, aber sieben gehen auch" — all das ist an **einem bestimmten** WD1772 gemessen | gemeldet vom Eigentümer (MF-824) | ⚠ **offen, ein latenter Befund im Baum.** `include/uft/protection/uft_fuzzy_bits.h:27-33` definiert `UFT_MFM_BIT_CELL_US 2.0` und `UFT_MFM_FLUX_4US/6US/8US` als *„Standard"* — das sind **DD**-Werte; bei HD-MFM ist die Zelle 1 µs und die Abstände 2/3/4. Darauf sitzt `uft_is_valid_mfm_timing()`, ein **generisch benannter** Prüfer in einem öffentlichen Header, der gegen HD-Daten jeden gültigen Wechsel abwiese. Heute folgenlos (erreicht nur über den einseitigen DD-Pfad von Dungeon Master), aber Name und Platzierung laden zur Wiederverwendung ein. **Der Gegensatz im selben Baum:** `uft_ipf_plugin.c:145` setzt dieselbe Konstante mit Begründung („IPF uses 2µs MFM cells universally — DD bitcell") und liegt richtig. Gleiche Zahl, einmal belegt, einmal nicht. **Für ein künftiges ST-Laufwerksprofil** (es gibt keines in `uft_hal_profiles.c`): Schritt- und Beruhigungszeit an den FDC-Takt binden, nicht absolut — MegaSTE, TT und Falcon können HD ab Werk, also mindestens drei Takt-Konstellationen in einer Rechnerfamilie |
| P3-44 | **„Read Track → Write Track klont keine Spur" — die dritte, älteste Bestätigung, dass STX ein Emulationsformat ist.** David Small, START Vol. 1 Nr. 2, Herbst 1986, wörtlich: *„This is why you can't do a Read Track, then a Write Track with the same data to clone a track."* Zwei Gründe: Read Track zerstört die Sektoranfänge (der Datenseparator rastet dort jedes Mal neu ein), und Write Track behandelt **jedes** `$Fx`-Byte gesondert — auch mitten im Sektor. Sein Verfahren: leere Spur formatieren, dann per Write Sector füllen. **Für UFT:** das STX-Trackimage ist per Definition ein Read-Track-Ergebnis; es zurückzuschreiben ist hardwareseitig ausgeschlossen, nicht implementierungsabhängig. Damit steht die Tier-Aussage aus drei unabhängigen Richtungen — Louis-Guérins Tabelle, sein Satz *„real preservation"* gegen *„emulation preservation"*, und jetzt der Mann, der den Chip programmiert hat. `README.md:339` führt STX weiterhin flach neben ST, MSA und ATX | gemeldet vom Eigentümer (MF-824) | ⚠ **offen.** **Drei verwertbare Sachfakten dazu:** (1) Read Track ist konstruktionsbedingt unzuverlässig — *„the data will not appear cleanly and will vary even from read to read"*; ein byteweiser Vergleich zweier STX-Trackimages oder Trackimage gegen Sektordeskriptoren zeigt **erwartete** Abweichungen. Dritte Quelle für dieselbe Aussage nach ijors Soft-Error-Punkt und Louis-Guérins „das erste $A1 wird immer falsch gelesen". (2) **DMA-FIFO-Falle:** der Atari-DMA hat einen 32-Byte-Puffer; ein Read Address überträgt 6 Byte, die **nie im Speicher ankommen**, bis weitere Lesevorgänge sie durchspülen. Ergänzt Louis-Guérins „nur Vielfache von 16 Byte" — zwei Effekte, gleiche Richtung: das Ende eines Read Track oder Read Address fehlt möglicherweise. (3) **Ein drittes Herkunftssignal:** `$6D $B6` ist die empfohlene Formatfüllung („maximum stress on the north-north and north-south magnetic field patterns") — ein damit gefüllter Sektor ist **formatiert, aber nie beschrieben**. Im Baum kommt `6DB6` nirgends vor. Damit sind es drei: `0x55` (Kopierer-Gaps), `0x01` (Burst-Nibbler-Ersatz), `$6DB6` (nie beschrieben). **Und zwei Gap-Tabellen statt einer:** Small beschreibt die IBM-System-34-Variante (80×4E, IAM, 50×4E, Gap4 54), Louis-Guérin die Atari-TOS-Formatierung (60×4E, kein IAM, Gap4 40). Kein Widerspruch — zwei Fälle, am IAM unterscheidbar. Ein Track-Rekonstruktor, der eine fest annimmt, liegt bei der anderen daneben |
| P3-45 | **Copylock ST sitzt auf Spur 0 Sektor 6 — belegt durch den Herstellerquelltext.** `info-coach.fr/atari/software/_preservation/copylock.s`, `keydisk.s` vom 12.07.1990, „(c)1988-90 Rob Northen Computing". Im Kopf in Großbuchstaben: *„DO NOT OVERWRITE SECTOR 6 ON TRACK 0"*, und weiter unten *„All 21080 sectors may be used … with the exception of sector 6 on track 0."* Deckungsgleich mit Louis-Guérins LGS/SHS-Eintrag (Populous T0-S6, Back to the Future T0-S6) — einmal aus der Analyse, einmal vom Urheber. **Damit war `uft_atarist_protection.c:66` `track = 79 /* Typically on last track */` nicht nur ein Platzhalter, sondern nachweislich FALSCH** — es zeigte auf die andere Seite der Diskette, und `uft_atarist_prot_print()` gab es als `Track/Side: 79/0` aus. Seit MF-820 zurückgezogen, der Ausdruck hängt an `copylock.detected` und das steht fest auf `false`. **Und `lfsr_seed` ist konzeptionell falsch benannt:** die Aufrufschnittstelle liefert `d0.l = serial no.`, also eine **32-Bit-Seriennummer**, keinen LFSR-Startwert. Dasselbe Muster wie bei Dungeon Master (4-Byte-Seriennummer plus CRC) — zwei unabhängige Schutzsysteme, beide mit einer je Diskette eindeutigen Kennung | gemeldet vom Eigentümer (MF-824) | ⚠ **offen — aber jetzt mit belegter Regel.** Copylock ST: **Spur 0, Sektor 6, kurzer Sektor (Bitzelle ≈ 4,2 µs, 4–5 % schneller), erste ~32 Byte bei Normalgeschwindigkeit, Träger einer 32-Bit-Seriennummer.** Das ersetzt die zurückgezogene ASCII-Suche. **Ein zweiter Beleg für den `uft_st.c`-Entwurf:** die Keydisk ist physisch 80×2×10 (819 200 B), der Bootsektor deklariert **einseitig** *„for compatibility with single sided drives"*. `st_geometry_from_bpb()` verwirft ihn (409 600 ≠ 819 200) und trifft über den Größen-Scan die richtige Geometrie. Zusammen mit Dungeon Masters 804-statt-800-Sektoren sind das **zwei echte Disketten, die absichtlich falsch deklarieren, und MF-462 fängt beide** — beide ohne Originalmedium synthetisierbar. **Kollisionstabelle:** 819 200 Byte ist der vierte Anwärter (D81, „KC 85/4 DS 800KB", ST-800K, Copylock-Keydisk). **Und eine Erwartung an Korpusvergleiche:** RNCs eigene Anti-Tamper-Anleitung empfiehlt, die Prüfung zwei- bis dreimal einzubetten, arithmetisch mit Programmvariablen zu verrechnen statt per IF-THEN-ELSE und in Interruptroutinen zu legen — eine gecrackte Fassung unterscheidet sich also **an vielen Stellen**, nicht an einer. Ein Differenzlauf, der einen einzelnen Patchpunkt sucht, sucht das Falsche. **Rechtlich:** die Tatsachen sind frei, der `dc.l`-Block trägt „All Rights Reserved" — aus dieser Datei geht nichts in den Baum außer Fakten und Fundstelle |
| P3-46 | **Drei dokumentierte Index-Randfälle: der AIR-Port hat alle drei, der native Parser keinen.** Nachgemessen über `src/flux/uft_kryoflux_stream.c` — `overflow_cnt`, `nfidx`, `incomplete`: **null Treffer**. Der Portierungszwilling `src/formats/kfx/uft_kfstream_air.c` behandelt sie einzeln und benannt: **(1) Sample-Counter-Überlauf vor dem Index** (`:460`, `pre_index_time = (ic_overflow_cnt - pre_overflow_cnt) << 16 + sample_counter`) — der 16-Bit-Zähler läuft über, bevor der Index kommt, und ohne die Überlaufzählung fehlen 65 536 Ticks je Überlauf. **(2) Index zeigt hinter den letzten Flusswechsel** (`:492`, `if (index_int[iidx-1].stream_pos >= flux_count) flux_count++`) — die Spezifikation nennt das ausdrücklich („Use additional cell if last index had incomplete flux"); ohne die Zusatzzelle fehlt der Aufnahme ihr Ende. **(3) Index vor dem ersten Flusswechsel** (`:415-427`, `nfidx = 0`-Sonderfall) — bis zum ersten Index lässt sich keine Index-Zeit bilden, weil es immer eine Teilumdrehung ist. Zusätzlich prüft der Port die StreamEnd-Position gegen die eigene Zählung (`KF_WRONG_POSITION`) und erkennt einen beschädigten letzten Index | gemeldet vom Eigentümer, beide Seiten nachgemessen (MF-826) | ⚠ **offen.** **Vierter Fall desselben Musters:** wo ein Port neben einem nativen Parser steht, ist der Port richtig — nach SCP (P3-32), HFE (P3-35) und TD0 (P3-37). Das ist kein Zufall mehr, sondern eine Aussage über die Entstehung: portierter Code trägt die Randfälle seiner Vorlage, selbst geschriebener trägt die, an die jemand gedacht hat. **Der Weg:** die drei Fälle sind im Port bereits ausformuliert und benannt — es ist eine Übertragung, kein Entwurf. Sie gehören zusammen mit MF-825 gemessen, weil sie dieselbe Rechnung betreffen |
| P3-47 | **Ovl16-Ketten sind NFA-Kandidaten und werden nicht als Befund gemeldet.** Die Spezifikation (Louis-Guérin, „KryoFlux Stream File Protocol") sagt zu Flusswerten über `0xFFFF`, sie seien ungewöhnlich, aber *„have been found in games that attempt to fool the AGC (Automatic Gain Control) of the drive electronics."* Das ist die **Erzeugerseite** dessen, was derselbe Autor im Schutzdokument als **No Flux Area** (NFA) führt. `uft_kryoflux_stream.c:250-265` addiert die Überläufe **korrekt** (`v + overflow`, Zähler danach zurückgesetzt), zählt sie aber nicht und meldet nichts. Eine Ovl16-Kette geht damit als gewöhnlicher langer Flusswert durch | gemeldet vom Eigentümer, Zählung im Baum nachgemessen (MF-826) | ⚠ **offen, geringe Tragweite, aber die richtige Richtung.** Der Befund lautet nicht „hier ist ein Schutz", sondern „hier war der Fluss länger als 65 536 Ticks, *n*-mal in dieser Spur" — eine Beobachtung mit Zahl, keine Benennung. Genau die Form, die P3-38/39 für die C64-Seite fordern. **Und das Regelpaar dazu ist jetzt vollständig:** Jim Drew erklärt, warum die Summe der Flusszeiten die Index-zu-Index-Zeit **über**schreiten kann (ungültiger Fluss, Drehzahlschwankung — normal, P3-33); Louis-Guérin erklärt, warum sie bei NFA über dem Index **unter**schreitet: SCP übermittelt keine Lage des Indexpulses relativ zu den Datenpulsen und beginnt die Abtastung immer am Index, weshalb eine NFA über dem Index in der ersten Umdrehung gar nicht übertragen wird — KryoFlux kann es, weil es **vor** dem Index zu sampeln beginnt. Beide Richtungen belegt, beide gehören in die Analystenbasis, damit niemand eine Plausibilitätsprüfung baut, die in eine der beiden Fallen läuft. Sein pragmatischer Nachsatz gehört dazu: in allen von ihm getesteten Spielen mit NFA war die relative Lage zum Index letztlich **irrelevant**, das Zurückschreiben funktionierte trotzdem. Formatgrenze belegt, praktische Auswirkung begrenzt — die Nuance, die ein MESSUNG/FOLGERUNG-Bericht tragen kann und eine Konfidenzzahl nicht |
| P3-48 | **Zwei Formataussagen der jeweiligen Autoren, die die Formattabelle nicht führt.** **(1) KryoFlux Stream:** *„Note that Stream files are hardware specific (to the KryoFlux device) and therefore are not intended for long term preservation."* — vom Autor der Protokollbeschreibung, samt der Warnung, das Format könne sich ändern und die Beschreibung beziehe sich auf **DTC 2.2**. **(2) Pasti/STX:** *„real preservation"* (genug, um zu emulieren **und die Diskette physisch wiederherzustellen** — DC, KryoFlux, SuperCard Pro) gegen *„emulation preservation"* (genug für das Verhalten, keine Diskette daraus — Pasti/STX). Dazu die dritte, älteste Bestätigung aus P3-44: Read Track → Write Track klont hardwareseitig keine Spur. `README.md:339` führt STX weiterhin **flach** neben ST, MSA und ATX; Stream taucht in der Formattabelle ohne Einordnung auf | gemeldet vom Eigentümer (MF-826) | ⚠ **offen.** Für ein Werkzeug, dessen **Provider**-Tabelle sehr genau zwischen Tier-1, -2 und -3 trennt, fehlt dieselbe Trennung auf der **Format**seite — und die Quellen, die dieser Baum für STX und Stream ohnehin nutzt, liefern sie fertig. Zwei Spalten genügten: „trägt genug zum Emulieren" gegen „trägt genug, um die Diskette wiederherzustellen" |
| P3-49 | **SCHUTZNOTIZ: drei Stellen, an denen der Baum GENAUER ist als die Quelle.** Zum ersten Mal in dieser Reihe läuft der Abgleich andersherum — und das gehört genauso festgehalten, sonst „repariert" ein späterer Durchgang etwas Richtiges kaputt. Quelle: `info-coach.fr/atari/software/FD-Soft.php` (Louis-Guérin). **(1) Die FAT12/16-Schwelle.** Die Seite sagt *„Drives with more than 4086 clusters have a 16-bit FAT"*. `src/fs/uft_fat12.c:133` rechnet `clusters < 4085` — das ist der Wert der **Microsoft-FAT-Spezifikation** und der kanonische; 4086 ist eine der beiden verbreiteten Falschangaben. Für Atari-Disketten (~350 Cluster) ohne Belang, aber das Modul kann auch FAT16 und größere Medien, und dort wären ein bis zwei Cluster an der Grenze eine echte Fehlklassifikation. **NICHT anpassen.** **(2) Die Bootbedingung.** Die Seite verlangt *zusätzlich* zur Prüfsumme den Text „Loader" ab Byte 3. Das widerspricht der Dungeon-Master-Analyse **desselben Autors** auf dmweb (*„If the result is $1234 then the BIOS assumes that the sector is bootable"*) und David Smalls START-Artikel — beide: **Prüfsumme allein**. Das OEM-Feld bei $02 ist eine Konvention des TOS-Loaders, keine Bedingung des BIOS; eine selbstgeschriebene Bootdiskette mit anderem OEM-String bootet trotzdem. `uft_st.c:51-56` prüft **nur** die Prüfsumme über 256 BE-Worte gegen `0x1234`, mit Quellenangabe. **NICHT ergänzen** — als **UNRESOLVED** geführt: eine Quelle sagt ja, zwei sagen nein, darunter dieselbe Hand. **(3) Der FAT-Startwert.** Die Seite: *„the first byte contains the media descriptor (usually $F9F)"* — in sich widersprüchlich, ein Byte kann nicht `$F9F` sein. Gemeint ist vermutlich der erste 12-Bit-Eintrag (`FAT[0] = $FF9`, `FAT[1] = $FFF` bei Medienkennung `$F9`); die Schreibweise sieht nach vertauschten Nibbles aus. **Als Prüfwert unbrauchbar** | gemeldet vom Eigentümer, alle drei im Baum nachgemessen (MF-827) | ✅ **Schutznotiz, kein Auftrag.** Gleiche Form wie die HFE-`RAND`-Notiz (P3-35): die veröffentlichte Spec beschreibt `0xF4` fälschlich als „Do nothing", UFTs Konstante folgt dem Loader und ist richtig. Zwei Fälle jetzt, in denen eine Quelle den Baum verschlechtern würde |
| P3-50 | **Der Atari-Bootsektor trägt eine 24-Bit-Seriennummer bei $08 — sie wird nirgends gelesen.** Die BPB-Tabelle der Quelle: `SERIAL $08, 4 Bytes: The low 24-bits of this long represent a unique disk serial number`. **BERICHTIGUNG zur Meldung:** der Eigentümer vermutete, `uft_fat12.c:191` lese für ST-Abbilder eine Zufallszahl als Volume-Serial. Nachgemessen trifft das **nicht** — der Lesevorgang bei `0x27` steht hinter `if (d[0x26] == UFT_FAT_EXT_BOOT_SIG)`, also der Kennung `0x29` des erweiterten Bootsatzes. Eine Atari-Diskette ohne erweiterten Bootsektor liefert schlicht **keine** Seriennummer; das Feld bleibt 0. Es ist also kein Fehler, sondern eine **fehlende Funktion** | gemeldet vom Eigentümer, Schutzbedingung nachgemessen (MF-827) | ⚠ **offen, und der Nutzen ist größer als das Feld.** Der 24-Bit-Serial ist eine **Herkunftskennung**: zwei Abbilder mit gleichem Serial stammen sehr wahrscheinlich von derselben physischen Diskette, zwei unabhängig formatierte haben verschiedene. Für Korpusarbeit — Duplikaterkennung, „ist das derselbe Abzug" — ist das genau die Sorte Feld, die dieser Baum sonst mühsam über Hashes annähert, und ein Hash über das ganze Abbild kann das nicht: er ändert sich bei jedem Bit. **Und es reiht sich ein:** Copylock trägt eine 32-Bit-Seriennummer (P3-45), Dungeon Master eine 4-Byte-Seriennummer plus CRC (P3-41). Drei unabhängige Systeme, alle mit einer je Diskette eindeutigen Kennung — das ist ein wiederkehrendes Merkmal und gehört in eine Klassifikation, nicht in drei Einzelfälle |
| P3-51 | **Drei belegte Regeln aus der Gap-Analyse, die der Baum noch nicht führt.** **(1) Elf Sektoren je Spur ⇒ Sektorschreiben verweigern.** Zwischen Datenblock und nächstem ID-Block bleiben bei 11 Sektoren nur **7 Byte** (Gap 4 + Gap 2); ein Sektorschreibvorgang muss perfekt kalibriert sein, sonst kollidiert er mit dem folgenden ID-Block. Deshalb führt die Discovery-Cartridge-Dokumentation solche Spuren als **schreibgeschützt**, und deshalb taugt das überhaupt als Schutzmechanismus. Für das Write-Gate ist das eine belegte Regel: bei 11 Sektoren nur **Trackschreiben** anbieten, kein Sektorschreiben. **(2) Gap 3a und 3b werden NIE gekürzt.** Bei 11 Sektoren werden Gap 1 (60→10), Gap 2 (15→6) und Gap 4 (40→1) gekürzt — Gap 3a/3b nicht, weil der FDC zwischen ID- und Datenblock Zeit braucht. Damit ist ein Layout mit gekürztem Gap 3 **entweder fehlerhaft dekodiert oder absichtlich kaputt** — nie ein legitimes dichtes Format. Eine Plausibilitätsregel für jede Trackrekonstruktion. **(3) Eine Sektorreihenfolge ≠ aufsteigend ist KEIN Schutz.** *„Normally the sector number is incremented by 1 for each record … however sectors can be written in any order."* Ergänzt Louis-Guérins NOS-Klasse: nicht nur eine Sektorzahl ≠ 9 ist kein Schutz, auch eine abweichende Reihenfolge nicht. Ohne diese Regel meldet ein Analyst irgendwann eine interleavte Spur als Auffälligkeit | gemeldet vom Eigentümer (MF-827) | ⚠ **offen.** (1) ist die einzige mit unmittelbarer Wirkung — **aber nicht dort, wo ich sie zuerst verortet hatte.** Nachgemessen: `src/policy/uft_write_gate.c` arbeitet auf **Dateisignaturen und -größen**, kennt also keine Sektorzahl je Spur. Die Regel ist eine **Spur**-Entscheidung und gehört an den Schreibpfad selbst — `st_write_track()` (`uft_st.c:304`) bzw. die HAL-Ebene `uft_gw_write_track()`. Sie bleibt fail-closed, nur eine Ebene tiefer als gedacht. (2) und (3) sind Regelbasis für den Analysten, kein Code. **Was der Baum an dieser Stelle schon richtig macht:** `uft_fat12.c:173` **rechnet** die Wurzelverzeichnisgröße aus `root_entry_count` statt die „7 Sektoren" der Quelle fest zu verdrahten — bei Dungeon Master mit `NDIRS = 16` ist es **einer**, und wer 7 annimmt, liest sechs Sektoren Müll als Verzeichniseinträge. Und `uft_fat12.c:468` behandelt den `$05`-Sonderfall (erstes Namenszeichen ist wirklich `$E5`, nicht die Löschmarke) — den vergessen die meisten Umsetzungen |
| P3-52 | **Die Primaerquelle zur HDG/ISS-Klasse liegt vor — und sie nennt die Ausloeser vollstaendig.** Claus Brod, „Copy me - I want to travel", ST NEWS Vol. 2 Issue 7 (1987), auf `st-news.com` unter dem Titel **„The Track 41 Protector"**. Louis-Guerin verweist bei „Hidden Data into GAP" (HDG) und „Invalid Sync-mark Sequence" (ISS) auf genau diesen Artikel, nennt aber keine Bytes. Hier stehen sie. **(a) Die Ausloesertabelle — vier Regeln, drei davon bisher nirgends im Baum:** `$29` mit vorherigem **geraden** Byte · `$52`/`$53` mit vorherigem **durch 4 teilbaren** Byte · `$A4`–`$A7` mit vorherigem **durch 8 teilbaren** Byte · `$14` mit **gesetztem MSB des folgenden** Bytes. Dazu ausdruecklich: „Some other byte combinations can cause the error, too (shifts of the byte sequences above)". **(b) Der Pruefvektor:** geschrieben `FE 29 00 01` — mit Read-Track gelesen `FE 14 7F FE`. Vier Byte hin, vier zurueck, deterministisch. Direkt implementierbarer Prueffall fuer jeden WD1772-Datenseparator. **(c) Die verfeinerte Signatur:** gerades Byte, `$29`, dann ein gewoehnliches `$A1` (weil die Synchronisation nach `$29` um ein **halbes Byte** verschoben ist) — gelesen als **`$14 $0B`**, „which no copy program recognizes as correct sync sequence". Das ist die konkrete Bytesignatur statt der Heuristik „Sync-Marken im Gap suchen". **(d) Die einfache Vorstufe:** `4E 4E 4E A1 A1 A1 <Text> 4E 4E …` — die Nutzlast ist **druckbarer ASCII-Text** („(C) 1987 by Claus Brod"). Damit ist die HDG-Nutzlast viel spezifischer beschreibbar als „Nicht-Gap-Bytes". **(e) Warum Spur 41:** `41` dezimal ist `$29` hex, und die Spurnummer steht im **ID-Feld jedes Sektors** — der Effekt ist neunmal je Spur eingebaut, ohne dass etwas Besonderes geschrieben werden muesste. Louis-Guerins Randnotiz „This error occurs everywhere on track 41" ist damit erklaert | Primaerquelle abgerufen und Zitate woertlich gegengeprueft (MF-828) | ⚠ **offen — und mit einer BERICHTIGUNG an MF-820.** Der Eigentuemer haelt entgegen, die **Idee** von `detect_flaschel()` sei richtig: „Nicht-Gap-Bytes im Gap zaehlen" ist die passende Erkennung fuer HDG, und Brod bestaetigt das indirekt („Most programmers therefore suppose that gaps consist of $4E-s and $00-s and nothing else, basta" — geschrieben 1987 als Beschreibung genau der Annahme, die der Schutz ausnutzt). Das stimmt, und ich ziehe die Empfehlung „wegwerfen" zurueck: **umsetzen statt loeschen**, auf ein echtes Trackabbild, mit korrekter Satzlaenge, mit `$14 $0B` als zweitem Kriterium. Der **Name** „Flaschel" bleibt gestrichen, solange er unbelegt ist. **Und die Primaerquelle liefert einen STAERKEREN Ruecknahmegrund, als MF-820 hatte:** „This error only occurs when reading a complete track … When reading sectors, the controller switches off the sync unit, so that it can read without the sync bug confusing it." Die ganze Klasse ist ein **Read-Track-Phaenomen**; ein Detektor auf Sektordaten kann sie **prinzipbedingt nie sehen**, nicht bloss wegen des falschen Rasters. **Dritter belegter Fall im Baum, in dem eine Messung nicht misst, was sie zu messen vorgibt** — nach dem selbstbezueglichen CRC-Test und dem KryoFlux-Erzeuger mit vertauschten Zaehlern |
| P3-53 | **Zwei Gap-Zahlen, die wie ein Widerspruch aussahen, rechnen sich exakt auf — und der Rechenweg belegt eine Tatsache, die der Baum nirgends fuehrt.** Brods Write-Track-Tabelle (Erzeugerseite, 1987) gegen Louis-Guerins Messtabelle (Leserseite, 2015). Brod je Sektor: 12×`$00`, 3×`$F5`, `$FE`, 4 Byte ID, `$F7`, 22×`$4E`, 12×`$00`, 3×`$F5`, `$FB`, 512 Byte Daten, `$F7`, 40×`$4E`. Naiv summiert sind das **612** Byte — nicht 614. Die Differenz ist die **Steuersprache des WD1772 beim Write Track**, die Brod ausschreibt und die im ganzen Baum nicht vorkommt: Bytes ueber `$F4` sind Steuercodes, `$F5` = Syncbyte `$A1` mit fehlenden Taktbits, `$FB` = Datenmarke, `$FE` = ID-Marke — und **`$F7` schreibt eine Pruefsumme aus ZWEI Byte**. Mit den zwei `$F7` je Sektor: 612 + 2 = **614**, also **byteweise Louis-Guerins Sektorabstand**. Und weiter: 60 Byte Vorspann + 9×614 = 5586; eine DD-Spur bei 250 kbit/s und 300 U/min fasst 6250 Byte; 6250 − 5586 = **664** — genau Louis-Guerins Gap 5. Brods „normally 1400 bytes of $4E, which is more than enough to fill the track" ist damit ebenfalls erklaert: 1400 ist, was man dem FDC **fuettert**, 664 ist, was **draufpasst**; der Ueberschuss wird vom Indexpuls abgeschnitten | aus der Primaerquelle abgeleitet und nachgerechnet (MF-828) | ⚠ **offen als Regelbasis.** Zwei unabhaengige Quellen, 28 Jahre auseinander, eine von der Schreib- und eine von der Leseseite, treffen sich **auf das Byte** — sobald man weiss, dass `$F7` sich verdoppelt. Ohne dieses Wissen erscheinen 612 und 614 als Widerspruch, und wer eine Trackrekonstruktion gegen die falsche der beiden Zahlen prueft, meldet einen Fehler, wo eine Konvention ist. **Das bestaetigt zugleich MF-820 nachtraeglich mit einer Herleitung statt einer Zitatstelle:** der Sektorabstand ist 614, nie 512. Der Baum braucht die Steuersprache nicht — sein `write_track` arbeitet auf Spurdaten, nicht auf FDC-Kommandos (gemessen: `$F5`/`$F7` als Steuerbytes kommen in `src/` und `include/` nicht vor) —, aber wer je einen mitgeschnittenen Write-Track-Puffer auslegt, braucht sie |
| P3-54 | **Formatfuellwort: beide Bytes muessen ≤ `$F4` sein — zwei Quellen greifen ineinander.** Brod: „you must not format your disks (using the XBIOS call) with certain „virgin" values (the virgin word-s bytes must not exceed $F4)" — Begruendung ist die Steuersprache aus P3-53. David Small empfiehlt im START-Artikel `$6DB6` als Formatfuellung statt Nullen (weil eine formatierte, nie beschriebene Zelle so erkennbar bleibt); `$6D` und `$B6` liegen **beide** unter `$F4` ✓. Small sagt, **welchen** Wert man nehmen soll, Brod sagt, **warum nicht jeder** geht | beide Quellen woertlich, Rechnung trivial (MF-828) | ⚠ **offen.** Gemessen: `0x6DB6` kommt im ganzen Baum **nicht vor** (`src/`, `include/`, alle Endungen). Fuer einen ST-Formatpfad ist das eine harte Pruefbedingung — und die Zahl `$6DB6` ist zugleich das Herkunftssignal „formatiert, nie beschrieben", das dieser Baum an anderer Stelle bereits fuehrt (neben `0x55` fuer Kopierluecken und `0x01` fuer Burst-Nibbler-Ersatzbits) |
| P3-55 | **Ein FAT-Dialekt, den ein Zeitzeuge 1990 mit einem Patchprogramm geradebiegen musste — und den UFT ohne Patch liest.** `clausbrod.de/…/Atari/MsxSoftware`: Disketten des Yamaha-Samplers **TX16W** tragen ein Format, das Brod „MSX-DOS" nennt — „almost DOS/TOS-like, but a few minor differences gave TOS a really hard time": **(1) Dateinamen mit Leerzeichen**, **(2) fehlende FAT-Sicherungskopie**. Sein `MsxPatch` hat genau diese zwei Abweichungen wegpatchen muessen, damit TOS die Disketten lesen konnte. **Beides im Baum nachgemessen — beides wird korrekt behandelt:** `src/fs/uft_fat12.c:124` und `:170` weisen nur `num_fats == 0` ab, **nicht** `num_fats == 1`, und beide Folgerechnungen (`:130` `data_sec`, `:175` `root_dir_sector`) multiplizieren mit `num_fats` statt eine Zwei zu unterstellen; `trim_padded()` (`:47-52`) schneidet nur **nachlaufende** Leerzeichen ab, eingebettete ueberleben unveraendert, und es gibt keinen Zeichenfilter, der sie abweist | Quelle gelesen, beide Aussagen im Baum gemessen (MF-828) | ✅ **kein Befund — und deshalb festgehalten.** Ein unabhaengiger Zeitzeuge stolpert 1990 ueber genau zwei Abweichungen und muss ein Werkzeug dagegen schreiben; UFTs Leser nimmt sie beide ohne Sonderfall. Das ist die Art Bestaetigung, die eine T-Stufe **nicht** hebt (kein Korpus, kein Gegenlauf) und trotzdem etwas wert ist: sie benennt zwei Bruchstellen, die ein spaeterer „Aufraeumer" leicht einbaut — etwa `num_fats == 2` als Plausibilitaetspruefung. **Zwei Anschluesse, beide Fundus:** Brod stellt selbst die Frage, ob „MSX-DOS" dasselbe Format wie bei **MSX-Rechnern** sei („Can anybody confirm this hypothesis?") — UFT haelt mit `msx_disk` (T1b seit MF-782) und dem FAT12-Leser beide Haelften und koennte es beantworten. Und der **Yamaha Protector**: die Samplerdisketten trugen ein Schutzverfahren, das der Sampler **verlangte** — unformatierte DOS/TOS-Disketten wurden abgewiesen. Umkehrung des ueblichen Falls: der Schutz sperrt nicht das Kopieren, sondern die **Annahme** durch das Geraet. Passt zu Korg/Akai/Twiggy (Sampler auf Standardgeometrie, unterschieden am Dateisystem). **Eine Quelle, Erstautor** — nach der Zwei-Quellen-Regel bleibt es Fundus, der Code ist zudem verkauft und unveroeffentlicht |
| P3-56 | **Welche der beiden FAT-Kopien die massgebliche ist, haengt vom schreibenden System ab — und der Baum begruendet seine Wahl nirgends.** Quelle: `msxpatch.txt` (Claus Brod, ZOO-Archiv vom 23.11.1994, vom Eigentuemer entpackt): *„MSXDOS disks have two FATs, but only the first of them contains valid data. On a PC, that's no problem since MS-DOS uses the second FAT only as a backup of the first FAT. TOS, however, always tries to look up its file allocation data in the second FAT."* Zwei Aussagen: **(1)** MSXDOS legt zwei FATs an und fuellt nur die erste; **(2)** TOS liest die **zweite**. Aussage (2) laeuft gegen die Erwartung jedes PC-orientierten Werkzeugs — die FAT-Spezifikation kennt keine Vorrangregel. Im Baum liest **alles** die erste: `uft_fat12.c` `load_fat_cache()` (`fat_start_sector` = `reserved_sectors`) und `uft_msx.c:467` (`ctx->fat_start_sector + …`). Fuer eine MSXDOS-Samplerdiskette ist das die **richtige** Wahl, fuer eine TOS-geschriebene mit auseinander gelaufenen Kopien die falsche — und an keiner der beiden Stellen steht, warum | Quelle gelesen, beide Codestellen gemessen (MF-829) | ⚠ **offen — und ausdruecklich EINE Quelle.** Nach Aussage (2) wurde unabhaengig gesucht und **nichts Belastbares gefunden**; sie bleibt unbelegt, bis eine zweite Hand sie traegt. **Genau deshalb entscheidet die Umsetzung in MF-829 sie NICHT:** `uft_fat12_detect()` meldet seit MF-829 die **Beobachtung** („die Kopien unterscheiden sich in *n* Byte"), nicht die **Deutung** („Kopie *k* ist massgeblich"). Dieselbe Form wie P3-47 fuer Ovl16-Ketten. **Zur .prg ausdruecklich entschieden: NICHT disassemblieren.** `msxpatch.prg` stammt von **derselben Hand** wie das Readme — sie kann fuer dessen Aussage kein unabhaengiger zweiter Beleg sein, sondern nur zeigen, wie **dieser eine Autor** es gemacht hat. Das ist die fuenfte Frage (MF-644/760) in Reinform: ist das Orakel dieselbe Hand wie das, was es pruefen soll. Sie ist es. Fuer die Regelableitung reicht das Readme, und was die .prg zusaetzlich haette klaeren koennen (prueft es vor dem Kopieren?), bewegt keine der vier Kennzahlen |
| P3-57 | **Herkunftssignal Nr. 4 — und dafuer liegt die Messung seit MF-829 vor, die Deutung noch nicht.** Die Beobachtung „FAT 2 weicht von FAT 1 ab" traegt je nach Gestalt eine andere Aussage: **FAT 2 leer oder Muell bei gueltiger FAT 1** deutet auf ein System, das die Sicherungskopie nicht pflegt (MSXDOS/Yamaha-Sampler nach P3-56); **beide gueltig, aber verschieden** deutet auf Schreibabbruch oder Medienfehler; **identisch** ist der Normalfall. Damit steht es neben den drei bereits gefuehrten Signalen: `0x55` in Luecken (Kopierprogramm), `0x01`-Laeufe (Burst-Nibbler-Ersatzbits), `$6DB6` (formatiert, nie beschrieben — P3-54) | Regel abgeleitet, Messgrundlage seit MF-829 im Baum | ⚠ **offen, aber die teure Haelfte ist erledigt.** `uft_fat_detect_t` fuehrt seit MF-829 `fat_compared` und `fat_diff_bytes`; die Unterscheidung „leer/Muell gegen gueltig" braucht nur noch eine Auswertung der bereits verglichenen Bytes, keinen neuen Lesevorgang. **Was dabei NICHT passieren darf:** aus `fat_diff_bytes > 0` eine Herkunft zu **benennen**. Die Deutungstabelle oben hat drei Zeilen und eine Quelle; sie gehoert in die Analystenbasis, nicht in eine Konfidenzzahl. Nebenbefund aus derselben Quelle, im Baum bereits in Ordnung: ein `$20` **innerhalb** der acht Namensbytes ist gueltig und darf nicht als Namensende gelesen werden — nur nachlaufende sind Fuellung (`trim_padded()` in `uft_fat12.c:47-52` macht genau das, gemessen in MF-828/P3-55). Fuer Samplerdisketten ist das kein Randfall, sondern der Normalfall |
| P3-58 | **BERICHTIGT MF-847: die Klemme war erreichbar, sie stand nur woanders.** Hier stand: „`STX_MAX_SECTORS` ist 32; Atari-Spuren tragen 9 bis 11 Sektoren, geschuetzte selten ueber 20 — die Klemme ist real, aber nach heutigem Wissen nicht im Alltag erreichbar." **Beide Haelften tragen nicht.** (1) Der belegte Extremfall liegt weit darueber: „Sherman M4" fuehrt **70 Sektoren je Spur** (DrCoolZic, Atari Copy Protection Rev 1.4, Klasse NOS), Theme Park Mystery 12, Computer Hits Vol. 2 elf. (2) Vor allem aber zielte die Erreichbarkeitsfrage auf `uft_stx_air.c` — eine Datei **ohne Produktionsaufrufer**. Der Leser, den Benutzer wirklich bekommen, ist `uft_stx_plugin.c`, und der klemmte bei **26**, nicht bei 32, ohne jede Begruendung im Code | Extremfaelle vom Eigentuemer aus dem Schutzkatalog belegt; die Erreichbarkeit der vier STX-Leser im Baum ueber `git ls-files` nachgemessen (MF-847) | ✅ **der erreichbare Teil ist behoben** (P3-91): die 26er-Klemme ist ersatzlos weg — `uft_track_add_sector()` waechst per realloc, es gab also nie einen Kapazitaetsgrund. **Und der urspruengliche Punkt ist mit MF-854 erledigt:** die Verdrahtung machte die Klemme scharf, also meldet der Adapter sie — `uft_stx_air_track_announced()` gegen `uft_stx_air_track_stored()`, und bei Abweichung eine `UFT_WARN` mit beiden Zahlen und dem Vermerk „Grenze der Umsetzung, nicht des Formats". Genau die in dieser Zeile vorgeschlagene Form (`sectors_stored` neben `sector_count`, schmale Zugriffs-API nach dem Muster von `ipf_air_get_track_loss()`). Frueher offen war: in `uft_stx_air.c` traegt `trk->sector_count` weiter die ANGEKUENDIGTE Zahl, waehrend nur `nsec` Deskriptoren gelesen werden — eine falsche Zahl im Datensatz. Das wiegt jetzt allerdings leichter, weil die Datei keinen Aufrufer hat; die Reihenfolge haengt an P3-93 (wird der Port verdrahtet oder nicht) |
| P3-59 | **Die vier Positionsfelder auf `uft_sector_t` sind bereits da — und bis auf eines tot.** Anlass: der Vorschlag, nach dem Vorbild von SED 5.68 (`sec%(x,y)`, Anton Stepper / Claus Brod, 1996) vier Feldgrenzen anzuhaengen. Vorher gemessen, je Bezeichner ueber `git ls-files`, mit Blick auf die **kanonische** Struktur in `include/uft/uft_types.h` („This is the ONE definition of `uft_sector_t` used across the entire project"): `id_offset` (`:361`, „Bit offset of ID field") hat im ganzen Baum **eine einzige Fundstelle — die Deklaration**. `gap_before` (`:366`) hat einen Treffer, und der steht in `uft_atarist_macrodos.c:116` auf einer **eigenen** Struktur. `data_offset` (`:362`) und `bit_offset` (`:364`): alle 44 bzw. 14 Schreibstellen gehoeren anderen Strukturen (2IMG-Kopf, IR-Format, `uft_mfm_sector_t`, `positions[]`, `splices[]`) — und die eine, die dem Namen nach passt (`uft_mfm_sector_parser.c:339`), setzt einen **Pufferindex** (`pool_used`), nicht eine Bitposition. **Genau ein** Positionsfeld wird je gefuellt: `angular_position`, und zwar von `uft_atx.c:366` — dem einzigen Format, das es kann | alle fuenf Felder einzeln gemessen (MF-831) | ✅ **ERLEDIGT MF-832 — und der Vorschlag hat sich umgekehrt.** Vier weitere Felder auf vier tote zu setzen macht die Struktur **irrefuehrender**, nicht reicher: ein nie geschriebenes Feld ist schlimmer als ein fehlendes, weil es wie eine Zusage aussieht und sein Nullwert oft gueltig ist — `id_offset == 0` ist eine legitime Bitposition. **Der erste Schritt ist deshalb nicht Erweitern, sondern Einloesen:** entweder fuellt ein Leser die vier, oder sie bekommen das Gueltigkeitsflag-Muster, das MF-474 mit `has_angular_position` bereits vorgemacht hat. **Was der Vorschlag richtig sieht:** die Aufteilung selbst. Zwei unabhaengige Entwuerfe — ein Atari-Sektoreditor von 1996 und das IPF-Blockformat fuer neun Plattformen — sind bei derselben Struktur gelandet (Gap und Daten als Spannen mit Offsets, Einheit mitgefuehrt, physikalische Reihenfolge als Index). Das ist ein besseres Argument als jede Ableitung. **Und die drei Uebertragungsgrenzen sind belegt:** Apple hat keinen Index (Positionen nur als Differenzen aussagekraeftig, `has_angular_position` faengt das korrekt ab); Amiga schreibt die Spur in einem Zug (ein Gap je SPUR, nicht je Sektor — die tragfaehige Form ist eine geordnete Liste typisierter Bereiche, aus der sich die Sektorgruppierung ERGIBT); und die Einheit wechselt zwischen Byte, Bit und Flusstakt und muss mitgefuehrt werden | **Abgeschlossen MF-832:** die Messung steht als Kommentarblock ueber den sechs Feldern in `uft_types.h`, Feld fuer Feld mit Fundstellen. Ein Gueltigkeitsflag `has_bit_positions` war der erste Entwurf und ist **zurueckgenommen** — es haette niemand gesetzt, waere also selbst ein totes Feld gewesen und genau der Fall, den das Tor aus MF-831 fangen soll. Es gehoert hierher, sobald ein Erzeuger die Werte fuellt, zusammen mit ihm. **Und beim Nachsehen, wer die Felder wenigstens weiterreicht, fiel der eigentliche Fehler auf** — siehe P3-61.
| P3-60 | **SED 5.68 meldet zehn benannte Beobachtungen und keinen einzigen Schutznamen — 1996.** Aus dem Quelltextkommentar: *„Fehler in ANALYSE: Gap Header-Datamark, Datamark not found, Sector length, Tracknr., Double Sector, Gap Sector-Header, Wrong Side, Header/Sector, Sectornr., Differenz"* — dazu die Aufloesung von „Differenz": *„(Diff: Trackpos. − Headerpos.)"*. Keine Marke, keine Konfidenzprozente, jede Meldung mit Position. Gegen Louis-Guerins rund 30 Klassen gehalten, lassen sich beide auf **sechs formatunabhaengige Klassen** reduzieren: ID widerspricht Position (ITN/ISN/IHN) · Identitaet mehrdeutig (DSN/SWS) · Groesse deklariert ≠ gemessen (ISL, LGS/SHS) · Feld fehlt (SND/SNI) · Zwischenraum unerwartet (HDG/IDG/TLP) · Positionsversatz (DOI/DBI/IOI/IBI) | Quelltextkommentare gelesen, Zuordnung gegen die Klassenliste gefuehrt (MF-831) | ⚠ **offen — und es ist die Ausgabeform, die MF-820 gesucht hat.** Dort wurden drei Detektoren zurueckgezogen, weil sie Marken meldeten, die sie nicht messen konnten (Copylock aus drei ASCII-Zeichen, ein Platzhalter-Seed als Messwert). Was an ihre Stelle gehoert, steht hier: sechs Klassen, alle aus Feldgrenzen und Positionen ABLEITBAR, keine davon eine Benennung. **Die Voraussetzung dafuer ist P3-59** — ohne gefuellte Feldgrenzen ist keine der sechs messbar. Reihenfolge also: erst die vorhandenen Felder einloesen, dann die Klassen darauf. Nebenbefund aus derselben Quelle, ebenfalls offen: `src/fs/uft_fat12.c` kennt keine **verlorenen Ketten** und keine **Querverkettung**, obwohl `uft_amigados_extended.c` genau das fuer AmigaDOS baut und im Kommentar begruendet („data still on disk = forensically valuable, count as orphan_count") — SEDs FAT-Graph fuehrt fuenf Zustaende (frei, Verwaltung, verklebt, Leiche, defekt), UFTs FAT12 drei |
| P3-61 | **`uft_sector_copy()` verlor 98 von 192 Byte je Sektor — behoben MF-832.** Die Funktion zaehlte **neun Skalarfelder von Hand auf** (`id`, `crc_stored`, `crc_calculated`, `crc_ok`, `error`, `retry_count`, `bit_offset`, `byte_offset`, `confidence`) und kopierte dann die vier eigenen Puffer. Alles, was seit dem Schreiben dieser Funktion zu `uft_sector_t` hinzukam, fiel **still** weg. Namentlich: `angular_position` samt `has_angular_position` (MF-474) — eine kopierte Spur verlor die Winkelposition eines ATX-Sektors und meldete danach „nicht gemessen", wo gemessen wurde —, dazu `id_crc_ok`, `gap_before` und die Kompatibilitaets-Aliasse. `uft_track_copy()` (`uft_unified_types.c:358`) ruft sie fuer **jeden** Sektor auf, der Weg war also erreichbar | Rotbeweis zuerst: byteweises Muster durch die Kopie, **98 von 192 Byte abweichend, erstes bei Offset 12** (`tests/test_sector_copy_vollstaendig.c`, MF-832) | ✅ **behoben.** Nicht die Liste ergaenzt, sondern **abgeschafft**: `*dest = *src`, danach die vier Zeiger auf NULL und neu anlegen. Eine Strukturzuweisung kann kein Feld vergessen, auch kein kuenftiges — zu pflegen ist nur die kurze Liste der Zeiger, die **nicht** geteilt werden duerfen, und die steht unmittelbar darunter. **Siebzehnter belegter Fall** der Aufzaehlung statt Messung in diesem Baum, und der erste an **Nutzdaten** statt an einer Kennzahl. Der Test prueft bewusst keine Feldliste — das waere derselbe Fehler eine Ebene hoeher — sondern vergleicht die ganze Struktur byteweise ausser den vier Zeigerfeldern; ein kuenftiges Feld ist damit automatisch gedeckt |
| P3-62 | **Der erste Pruefvektor fremder Hand fuer die FAT12-Nibbelentpackung — und er ist der einzige Test im Baum, der eine vertauschte Schiebung faengt.** Quelle: `SED_568.HLP`, Disk-Monitor SED 5.68 (Anton Stepper / Claus Brod, August 1996). Acht Byte, fuenf Erwartungswerte: `F9 FF FF 03 40 00 FF 0F` -> Eintrag 0 = `$FF9`, 1 = `$FFF`, 2 = `$003`, 3 = `$004`, 4 = `$FFF`. Alle fuenf vor dem Schreiben nachgerechnet (Eintrag *n* liegt bei *n* + *n*/2). Der Wert des Vektors liegt in der **Ueberlappung**: Eintrag 2 und 3 teilen sich das Byte an Versatz 4 (`$40`) — der gerade nimmt dessen untere vier Bit als seine oberen, der ungerade dessen obere vier als seine unteren | eingebaut als `fat12_nibbel_gegen_sed_568` in `tests/test_fat12_fremd.c` (MF-832) | ✅ **erledigt, mit einer Messung als Zugabe.** Mutationsprobe: `(cluster & 1) ? (w >> 4) : (w & 0x0FFF)` vertauscht, neu gebaut — **alle anderen Tests des Baums bleiben gruen**, nur dieser faellt um. Bis MF-832 hatte die FAT12-Entpackung also **keinen** Test, der sie haette falsifizieren koennen; die bisherigen Tests dieser Datei pruefen ein `mtools`-Abbild, also die Geometrie aus fremder Hand, die Entpackung aber weiter gegen die eigene Rechnung. **Zweiter Punkt derselben Quelle, im Baum bereits richtig:** die HLP („Eintraege 2 bis Clusterzahl + 1") und Rainer Seitels Aenderungsprotokoll 1994/95 („Groesster Clusterindex ist jetzt cpd%+1") nennen dieselbe Regel; `uft_fat12.c:210` rechnet `data_clusters + 2 - 1` — genau das. Mit im Test festgehalten |
| P3-63 | **ST Recover (Bertrand, Ms-RL) fuehrt zwei Groessen, die UFT nicht hat — und faellt bei vier Stellen hinter UFT zurueck.** Was **fehlt**: `bloc.trouve_par_le_controleur` — je Sektor der Vermerk, ob der Controller ihn gemeldet hat oder ob er aus dem Rohspurbild rekonstruiert wurde (MEASURED gegen INFERRED auf **Sektorebene**, 2015 umgesetzt); und `bloc.duree_espace_libre_en_1er` — die Luecke als **Messgroesse** in Mikrosekunden statt als Restmenge, samt Abschlussblock hinter dem letzten Sektor. Dazu ein vollstaendiges **Rekonstruktionsverfahren** fuer desynchronisierte Spuren: mittlerer Sektorabstand nur aus Paaren zwischen einem Mindestwert und 1,5× Sektordauer (sonst verfaelscht jede Luecke den Mittelwert), Platzhalter in jede Luecke ≥ Mindestdauer, Nachnummerierung rueckwaerts ueber das aus den Nachbarn bestimmte Interleave-Inkrement modulo Sektorzahl | vom Eigentuemer gelesen und gegenuebergestellt (MF-832) | ⚠ **offen, und die Reihenfolge steht: nach P3-59.** Die zwei Groessen setzen gefuellte Feldgrenzen voraus; heute gibt es keinen Erzeuger dafuer. **Vier belegte Bestaetigungen aus derselben Quelle:** 32 µs je Rohbyte (`200000/6250`, mit Herleitung im Kommentar — vierte unabhaengige Bestaetigung von David Smalls Wert); *„11 Sektoren/Spur, in 2 Umdrehungen zu lesen"* — zweiter Beleg fuer P3-51(1), diesmal aus laufendem Code; 50-Byte-Fenster zwischen ID und Datenmarke (Louis-Guerin: 43 ab ID-CRC, von `A1A1A1` gerechnet 47 — stimmig); und `128 << (n & 3)`, also **zwei** Bit Laengenkennung. **UNRESOLVED bleibt** die Breite der Maske: der Eigentuemer nimmt seine eigene Notiz „nur die untersten drei Bits, `$03` ≡ `$FF`" zurueck, weil sie in sich widersprueckt (`0xFF & 7 = 7`). **UFTs Behandlung ist davon unabhaengig richtig:** `uft_mfm_sector_parser.c:249` speichert den ROHWERT, maskiert nicht, und verweigert die Datensuche bei *n* > 3 mit Begruendung, statt eine Laenge zu erfinden — der Sektor bleibt als Befund erhalten. ST Recover verliert ihn (dort erbt ein Datenfeld ohne eigenen ID die Laenge des zuletzt gesehenen, scheitert an der CRC und verschwindet ohne Vermerk) |
| P3-64 | **ATR ist bei 256-Byte-Sektoren DREIFACH ambig — UFT nahm eine Variante an. Behoben MF-833.** Die ersten drei Sektoren belegen physisch 256 Byte, tragen aber nur 128 Byte Nutzdaten. Drei Ablagevarianten sind im Umlauf: **LOGICAL** (3×128, dann 256er), **PHYSICAL** (3×256, je 128 genutzt), **WEIRD** (3×128 + 3×128 Nullen). `atr_sector_offset()` rechnete LOGICAL **fest verdrahtet**; ein PHYSICAL-Abbild wurde ab Sektor 4 um **384 Byte verschoben** gelesen — ohne Fehler, ohne Warnung, nur mit falschen Daten. `total_sectors` rechnete mit derselben Annahme. Quelle: Joe Allen, `atari-tools` `readme.md` §Image formats, abgeleitet aus „Structure of an SIO2PC Atari disk image"; dort steht auch das Unterscheidungsverfahren (Teilbarkeit durch 128 aber nicht 256 → LOGICAL; sonst Byte 384–767 pruefen) | Rotbeweis: dieselbe Diskette dreimal abgelegt, Marke in Sektor 4 und im letzten Sektor — **3 von 5 Pruefungen rot**, LOGICAL und der 128-Byte-Gegenbeweis gruen (`tests/test_atr_boot_layout.c`, MF-833) | ✅ **behoben.** Vor dem Schreiben nachgerechnet: bei 720 Sektoren ist LOGICAL 183936 Byte (%256 = 128), PHYSICAL und WEIRD sind **beide** 184320 — die Laenge allein trennt die letzten zwei also **nicht**, deshalb der Byte-Bereich. Der WEIRD-Fall bleibt ausdruecklich unsicher (ein PHYSICAL mit leeren Bootsektoren 4–6 sieht identisch aus) und wird als `UFT_WARN` **gemeldet, nicht behauptet** — die Quelle sagt selbst „probably". **Bemerkenswert: `atari-tools` kann es selbst nicht.** Sein `atr.c` akzeptiert allein `size-16 == 128*3 + 256*717` und weist PHYSICAL und WEIRD als „Unknown disk size" ab. Das Verfahren ist dort **beschrieben und nirgends umgesetzt**; dies ist die erste dem Baum bekannte Umsetzung. Nebenbei behoben: der Rueckfall unbekannter Sektorgroessen auf 128 war **still** und meldet jetzt den Kopfwert | **NACHTRAG MF-834 — die Trichotomie ist erklaert, und MF-833 war unvollstaendig.** Nick Kennedys SIO2PC-Quelltext (der Autor von ATR; das Magic `0x0296` ist die Summe der ASCII-Werte von „NICKATARI") ist in sich **widerspruechlich**: `8BUSCMND.S` adressiert Sektor *n*>3 bei `0x190 + (n-4)*256` — `0x190 = 16 + 3×128`, also **LOGICAL** —, waehrend die Groessentabelle in `2SIOTEXT.S:1125` die 180-K-Diskette mit `(256/16)*720 = 11520` Absaetzen fuehrt, also 184320 Byte, also **PHYSICAL**. Adressiert werden davon nur 183936; die letzten **384 Byte sind Schlacke**. Damit sind die drei Varianten nicht drei Deutungen einer klaren Spezifikation, sondern **die zwei Haelften einer widerspruechlichen Referenzimplementierung** plus eine Mischform: wer ihrer Groessenangabe folgte, baute PHYSICAL, wer ihrer Adressrechnung folgte, LOGICAL. **Folge fuer MF-833:** die Regel „durch 256 teilbar -> PHYSICAL oder WEIRD" deckt den Fall der Referenz selbst nicht ab — und mein Probelauf hat SIO2PC-Originaldateien deshalb auf PHYSICAL abgebildet, also 384 Byte verschoben gelesen. **Ein Rueckschritt, den ich selbst eingebaut habe**, behoben in MF-834 mit einer Entscheidungstafel aus ZWEI Zeugen (Anfang *und* Ende) statt einem |
| P3-65 | **Fuenf Strukturregeln aus derselben Quelle, noch nicht im Baum.** **(1) VTOC2 Byte 0–83 ist ein Duplikat, dem nicht zu trauen ist** — *„Repeat VTOC bitmap for sectors 48..719 (write these, do not read them)"*. DOS 2.5 pflegt den Bereich, maßgeblich ist er nicht. **Drittes Vorkommen desselben Musters** nach TOS-liest-FAT-2 (P3-56) und MSXDOS ohne gepflegte zweite FAT (P3-55): eine redundante Struktur, bei der die Autoritaet **nicht** dort liegt, wo ein naiver Leser sie vermutet. **(2) Der Next-Sektor-Zeiger hat nur 10 Bit** — harte Obergrenze 1023 fuer jede Kettenverfolgung; bei 40×26 = 1040 physischen Sektoren sind 16 per Dateisystem **unerreichbar**. **(3) Verzeichniseintraege hinter der Endmarke** — DOS 2.0s bricht die Suche beim ersten nie benutzten Eintrag ab (Flagbits 6 und 7 beide 0); Eintraege dahinter sind fuer DOS unsichtbar, **die Daten liegen aber noch da**. Forensisch dieselbe Klasse wie `orphan_count` in `uft_amigados_extended.c` und die „Leichen" in SEDs FAT-Graph. `atari-tools` prueft es, `atari_check.c` nicht. **(4) VTOC-Freiplatz mit Toleranz ±1** — DOS 2.5 belegt auf neuen Disketten Sektor 720 vor, obwohl er unbenutzbar ist; 1010 **und** 1011 sind gueltige Anfangswerte. Ein Pruefer, der exakt 1010 verlangt, meldet legale Disketten als defekt. **(5) Interleave-Tabellen Atari 8-Bit** (`atr2imd.c`): Interleave 2 mit Umbruch — erst alle ungeraden, dann alle geraden, fuer 18 und 26 Sektoren | Quelle gelesen, Gegenstellen im Baum gemessen (MF-833) | ⚠ **offen — aber (3) laengst nicht mehr.** Hier stand „(3) ist der wertvollste — ein reiner Zugewinn an Fundstellen, ohne Risiko"; **behoben ist (3) seit MF-835**, und diese Bewertungsspalte ist einfach stehen geblieben. Aufgefallen beim naechsten Zugriff auf denselben Eintrag (MF-850) — dieselbe Klasse wie die dreimal belegte Zahlendrift, nur in Prosa statt in einer Zahl. **Und der Nachzug hat mehr gekostet als der Zugewinn:** MF-835 hat `check_directory()` beigebracht, hinter die Endmarke zu sehen, drei Geschwister aber nicht mitgezogen — einer davon gab die Sektoren dieser Eintraege FREI (P3-100). (5) haengt an P3-63: ohne die physikalische Reihenfolge laesst sich einem aus der Lueckengeometrie erschlossenen Sektor keine Nummer zuordnen. (1), (2) und (4) sind Regelbasis fuer den Analysten, kein Code. **Bereits behoben in MF-833:** die sechste Regel — *„any sector can be short, not just the last one"* (Bill Wilkinson, „Inside Atari DOS", 1982). `atari_check.c:381` meldete einen kurzen Sektor mitten in der Kette mit der **erfundenen** Begruendung „sollte voll sein"; jetzt CHECK_INFO mit Quellenangabe und dem ausdruecklichen Vermerk, die Stufe nicht zu heben. Eine Warnung mit erfundener Begruendung wandert mit der Zeit nach oben, nicht nach unten |
| P3-66 | **`atari-tools` ist als Orakel NICHT brauchbar — fuenf gemessene Maengel, festgehalten damit niemand danach greift.** **(1)** `atr.c` liest den ATR-Kopf **gar nicht**: kein Magic `0x0296`, keine Sektorgroesse aus Byte 4/5, keine Absatzzahl — es rechnet allein aus der Dateilaenge. Folge: eine **XFD**-Datei (kopfloses Rohabbild) passender Groesse wird als ATR angenommen und mit 16 Byte Versatz gelesen, ohne Meldung. Pikant: `atr2imd.c` im **selben Repo** prueft das Magic, liest Sektorgroesse und Absatzzahl mit High-Byte und misstraut der Kopfangabe ausdruecklich („Get actual image size, don't trust size from header"). Zwei Disziplinen, ein Autor, ein Repo. **(2)** `getsect`/`putsect` pruefen nur `sect != 0`, keine Obergrenze — ein korrupter 10-Bit-Zeiger schreibt jenseits des Abbildendes und **verlaengert die Datei** still. **(3)** Der beworbene Anspruch *„will not crash when manipulating damaged images"* ist gebrochen: `check_file()` beendet mit `exit(-1)`, wenn ein Kettenzeiger ueber das Ende hinauszeigt — der klassische Schadensfall. **(4)** Die Querverkettungserkennung **zerstoert ihren eigenen Befund**: nach der Warnung wird `map[sector]` mit der neuen Dateinummer ueberschrieben, der Ersteigentuemer ist aus dem Protokoll gelöscht; eine Dreifachverkettung erscheint als zwei Zweifachverkettungen mit falschem Eigentuemer. Die Zyklenerkennung greift nur bei Ruecklauf auf einen Sektor **derselben** Datei; ein Zyklus durch fremde Sektoren laeuft bis zum globalen Zaehler 2048, bei 1024 Sektoren also zweimal um die Diskette. **(5)** `fopen(disk_name, "r+")` — schreibend geoeffnet auch fuer `ls`, `cat`, `get`, `check`, und unter Windows im Textmodus (vom Readme eingeraeumt, nicht behoben) | vom Eigentuemer gelesen, gegen UFT gestellt (MF-833) | ✅ **kein Auftrag, ein Vermerk.** Der Wert dieser Zeile ist die **Sperre**: die Quelle ist fuer Regeln und Testvektoren ausgezeichnet (P3-64/65 stammen daraus), als Vergleichsimplementierung fuer einen Differenzlauf **nicht**. Genau die fuenfte Frage (MF-644/760) in ihrer anderen Richtung — nicht „dieselbe Hand", sondern „schwaechere Hand". **UFT ist an sechs von sechs Stellen besser, gemessen:** Magic geprueft (`uft_atr.c:44`), Absatzzahl mit High-Byte (`:55`), Kettenzeiger-Schranke (`atari_check.c:324,463`), Kettenlaenge je Datei aus der Deklaration statt global 2048, Querverkettung in einem eigenen Durchgang mit `sector_owner[]`, und Befund statt `exit()`. Die vier Atari-Module fuehren „Inside Atari DOS" (Wilkinson 1982) als Referenz — eigenstaendige Umsetzung, keine Lizenzfrage |
| P3-67 | **Drei weitere ATR-Kopffehler aus der Primaerquelle — behoben MF-834.** **(1) `HI_XSIZE` ist ein WORT bei Offset 6-7**, nicht ein Byte; `uft_atr.c:55` las nur `header[6]`. Praktisch harmlos (ein gesetztes `header[7]` waere ein 268-GB-Abbild) — aber es machte (2) unsichtbar. **(2) SIO2PC VOR Revision 3.00 schrieb MUELL in `HI_XSIZE`.** Der Autor hat dafuer eine versteckte Reparaturoption eingebaut, die das Wort schlicht nullt (`A_DISKS.S:1256`, „This fixes it!!!") mit dem Bildschirmtext *„PRESS Y TO FORCE FILE SIZE TO STANDARD (FIX BUG)"* — er nennt es selbst einen Bug. Erkennung nach seinem eigenen Verfahren (`A_DISKS.S:1265`): passt das Low-Wort EXAKT auf eine der vier Standardgroessen (4096 / 5760 / 8320 / 11520 Absaetze) und ist das High-Wort ungleich 0, ist das High-Wort der bekannte Muell. **(3) Enhanced Density hat 26 Sektoren je Spur**, nicht 18. `uft_atr.c` verdrahtete 18 fest und meldete fuer eine 1040-Sektor-Diskette **58 Zylinder statt 40** — die Sektorzahl stimmte, die Spuraufteilung nicht | (3) mit Test gemessen (`enhanced_density_hat_26_sektoren_je_spur`, MF-834); (1) und (2) durch Inspektion, siehe Nachsatz | ✅ **behoben.** **Ehrlich zu (2):** die Wirkung ist seit MF-833 nur noch eine WARNUNG, kein geaenderter Wert — die Sektorzahl kommt seither aus der tatsaechlichen Dateilaenge, nicht aus der Kopfangabe. Ein Test darauf waere leer, und deshalb steht hier keiner. Was bleibt: die latente Ueberlaufquelle ist weg, und der Befund wird benannt. **Bemerkenswerter Nebenfund:** `include/uft/formats/atari_dos.h` fuehrt `atr_header_t` mit `size_high` als `uint16_t`, `flags` und `first_bad_sector` — die RICHTIGE Struktur stand im Baum, nur das Format-Plugin las an ihr vorbei. Dasselbe Muster wie „Port neben nativem Parser", diesmal innerhalb desselben Formats |
| P3-68 | **P3-65 (3) erledigt — und der Prueflauf dafuer existierte schon, er konnte nur nie feuern.** Ich hatte gestern notiert, `atari_check.c` pruefe Eintraege hinter der Verzeichnis-Endmarke nicht. **Das war falsch**, uebernommen ohne Messung: `atari_check.c:232` meldet „Eintrag #%d nach Ende-Marker" seit jeher. Gemessen ist der wirkliche Fehler schlimmer — `atari_dos2.c:391-400` hoerte beim ersten Nullstatus auf zu LESEN und ueberschrieb **alle restlichen Plaetze** mit „nie benutzt": `for (rest = idx; rest < MAX_FILES; rest++) directory[rest].status = DIR_FLAG_NEVER_USED;`. Danach ist die Bedingung `found_end && status != NEVER_USED` auf **jeder** Diskette falsch. **Fuenfter belegter Fall einer Pruefung, die gruen ist WEIL sie unmoeglich ist — und der erste, bei dem nicht die Pruefung fehlt, sondern ihr Gegenstand zerstoert wird.** Dazu zweitens: die Endmarke war als `status == 0x00` geprueft, die Quelle sagt „flag byte bits 6 and 7 both 0" — ein Eintrag mit `0x02` beendet die DOS-Suche ebenfalls, wurde von UFT aber weitergezaehlt; UFT sah damit Dateien, die DOS nicht sieht, und sagte es nicht | Rotbeweis dreiteilig, alle drei rot: Parser verwarf den Eintrag, Pruefer meldete nichts, Endmarke falsch erkannt (`tests/test_atari_dir_past_end.c`, MF-835) | ✅ **behoben.** `dir_entry_count` merkt die **Sichtgrenze** einmal, gelesen wird weiter bis zum Verzeichnisende; `DIR_IST_ENDMARKE()` prueft b6/b7 statt Gleichheit. Der Befund ist CHECK_**INFO** mit Sektor, Startsektor und Sektorzahl — also wiederherstellbar — und laesst `is_valid` unberuehrt: ein Fund, kein Mangel. Damit fuehrt die Atari-Seite dieselbe Klasse wie `uft_amigados_extended.c` fuer AmigaDOS (`orphan_count`, „data still on disk = forensically valuable") und SEDs FAT-Graph („Leiche"). **Offen aus P3-65 bleiben (1), (2), (4) und (5)** — VTOC2-Duplikat, 10-Bit-Grenze, ±1-Toleranz, Interleave-Tabellen |
| P3-69 | **HADFS-Disketten wurden als DFS-Volumes gemeldet — mit Konfidenz 85. Behoben MF-836.** HADFS (J. G. Harston, BBC/Electron/Master) schreibt beim ANLEGEN einer Diskette einen **echten DFS-Katalog** in die Sektoren 0/1, als Kompatibilitaetsmassnahme. Quelle: HADFS 6.10 Quelltext, `S.HADFS8` §InstDFS (`LDA FSM+31:AND #&41:BNE InstNoDFS` — nur bei Flag-Bit 0 „NoDFS" oder Bit 6 „Locked" bleiben 0/1 unberuehrt) und `S.HADFS9` §BootCode (Titel „HADFS", Eintraege `!Boot`, `$`, `HADFSROM`). `uft_ssd_plugin_probe()` prueft genau diesen Katalog — Sektorzahl bei 0x107, Bootoption bei 0x106 — und meldete **85**, auf der Skala aus MF-729 also „Merkmal getroffen". Getroffen wurde aber ein Merkmal, das HADFS **absichtlich** hinterlegt hat; das eigentliche Dateisystem beginnt bei Sektor 71 und blieb unsichtbar. Kein Absturz, keine Warnung, ein plausibles falsches Ergebnis | Rotbeweis mit drei Gegenproben: dasselbe Abbild ohne Kennung muss weiter ≥ 80 melden, ein einzelnes verdrehtes Kennungsbyte darf nichts herabsetzen, und ein zu kurzer Sondenpuffer auch nicht — die drei hielten schon, nur der Hauptfall war rot (`tests/test_ssd_hadfs_nicht_dfs.c`, MF-836) | ✅ **behoben, und ausdruecklich OHNE neuen Leser.** Traegt Sektor 70 die Kennung, die HADFS zur Selbsterkennung benutzt (`S.HADFS9` §ChkJGH gegen §JGHName: Byte 16..23 = `00 28 43 29 4A 47 48 00`, also „\0(C)JGH\0", Dateioffset **0x4610** — innerhalb der 65536 Byte Sondenpuffer), faellt die Konfidenz auf **30** = „nur die Groesse". Bewusst **kein** `return false`: die Datei IST ein Acorn-Abbild mit 10 Sektoren zu 256 Byte, ihr den Zugriff zu verweigern waere schlechter als eine ehrliche niedrige Zahl. **Ein HADFS-Leser wird NICHT eingefuehrt** — das waere ein neues Dateisystem und fiele unter das Moratorium (EINFRIER-REGEL MF-363/498), auch als Vorschlag. Diese Aenderung nimmt allein einen falschen Anspruch zurueck |
| P3-70 | **Vier belegte Acorn-Fakten, die der Baum nicht fuehrt — plus das HADFS-Plattenlayout als Fundus.** **(1) Geometrie:** `HADFS8:1310` „Multiply tracks by 10 to get sectors" — 10 Sektoren zu 256 Byte je Spur, 2560 Byte/Spur. Also DFS-Geometrie, nicht ADFS (16×256). `uft_ssd_plugin.c` hat das richtig (`SSD_SPT 10`), aber ohne Quellenangabe. **(2) 102 Spuren ist die groesste einseitige DFS-Groesse** — `HADFS8:4740` (`CMP #103:BCC FormDsk5`), darueber wird auf doppelseitig umgeschaltet und die Spurzahl je Seite halbiert. **(3) Seitenwahl ueber die Laufwerksnummer:** `HADFS8:4960` `ORA #2 :\ Set to drive 2/3 for side 1` — die Acorn-Konvention „Laufwerk +2 = Seite 1". Fuer `ssd_dsd.c` heisst das: die Seitenverschraenkung im `.dsd` folgt der Laufwerksnummer, nicht einem Kopfbit. **(4) Formatierungs-Versatz 7 Sektoren:** `HADFS8:4880` `LDA #7:STA &F93 :\ Set initial skew` — ein Parameter, den der Baum fuer Acorn nicht kennt. **Das Plattenlayout** dazu, aus §Install und §Format rekonstruiert: Sektor 0-1 DFS-Kompatibilitaetskatalog, 2-5 frei, 6-69 frei oder eingebettetes HADFS-ROM (Systemdiskette), **70 = FSM** (Kennungssektor: Titel 0-15, Kennung 16-23, DiskID 24-25, Groesse 28-29, Typ 30, Flags 31 mit b7 BigDisk / b6 Locked / b0 NoDFS, FSM-Eintraege ab 32), 71-73 Wurzelverzeichnis `$`, ab 74 frei | vom Eigentuemer aus dem Quelltext rekonstruiert (MF-836) | ⚠ **Fundus, kein Auftrag.** (4) haengt an P3-63: ohne den Versatz laesst sich einem aus der Lueckengeometrie erschlossenen Acorn-Sektor keine Nummer geben — dasselbe, was die Atari-Interleave-Tabellen aus P3-65 (5) leisten. (1) bis (3) sind Regelbasis. Das FSM-Layout ist der Bauplan fuer einen HADFS-Leser, **wenn** das Moratorium ihn je zulaesst; bis dahin ist es die Begruendung dafuer, warum die Kennung in MF-836 an genau dieser Stelle steht. Vom Autor selbst dokumentierte Grenze, gehoert dazu: HADFS 6 adressiert bis 4 GB, kann aber Dateien ab 16 MB nicht loeschen, weil `AddToFSM` nur Luecken < 16 MB zurueckgeben kann |
| P3-71 | **Das DMS-Plugin las den Track-Kopf VOLLSTAENDIG falsch und lieferte fuer im Wesentlichen jede echte DMS-Datei ein 0xE5-gefuelltes ADF. Behoben MF-837.** Der Baum hatte **zwei** DMS-Umsetzungen: `src/formats/dms/uft_dms.c` — die gegen **xDMS 1.3** (Andre Rodrigues de la Rocha, 24.03.1999) verifizierte Portierung, reentrant ueber `dms_ctx_t` (was xDMS selbst nicht ist), mit allen sieben Betriebsarten, beiden Entpackstufen, den drei Tracklaengen und allen vier Integritaetspruefungen — und `uft_dms_plugin.c`, die **registrierte** Fassung mit eigenem Entpacker. Gemessen: die Bibliothek hatte ausserhalb ihres Verzeichnisses genau **einen** Aufrufer, und der war ein Test (`tests/test_uft_dms.c`). Kein Produktionspfad. **Der Track-Kopf, Feld fuer Feld:** richtig ist `2-3 number · 6-7 pklen1 · 8-9 pklen2 · 10-11 unpklen · 12 flags · 13 cmode · 14-15 usum · 16-17 dcrc · 18-19 Kopf-CRC`. Das Plugin las `comp_mode` aus **10-11** (das ist `unpklen`, auf einer Amiga-Spur typisch 5632 = 0x1600), `unpacked_size` als BE32 aus **12-15** (= `flags|cmode|usum`) und `packed_size` als BE32 aus **16-19** (= zwei CRC-Felder). **Nur die Tracknummer stimmte.** Folge: `comp_mode` traf immer den `default:`-Zweig, die Spur behielt ihre 0xE5-Fuellung, und die Schranke `packed_size > file_size - pos` beendete die Schleife meist sofort | Rotbeweis mit von Hand gebautem, in allen vier Pruefsummen stimmigem DMS (NOCOMP — braucht keinen Entpacker, misst also allein den Kopf): **2 von 5 rot**, darunter die Gegenprobe „dieselbe Datei durch die Bibliothek" **gruen** (`tests/test_dms_plugin_gegen_bibliothek.c`, MF-837) | ✅ **behoben durch ENTFERNEN: 668 Zeilen Entpacker weg, 932 -> 229 Zeilen.** Das Plugin ruft jetzt `dms_read_info()` und `dms_unpack()`. Erster Lauf **streng** (alle vier Pruefungen zaehlen), bei Abbruch ein zweiter mit `override_errors` — dann sind die Daten da UND der Mangel ist benannt; ist nichts wiederherstellbar, wird die Datei **abgelehnt** statt als leere Diskette ausgegeben. **0xE5 ist die Formatfuellung von AmigaDOS** — der alte Kommentar nannte das „forensic integrity", es war das Gegenteil: der Verlust wurde als Datum ausgegeben. Nebenbei behoben: HD-Archive (geninfo Bit 4) sind 1760 KB, das Plugin rechnete immer mit 880 KB |
| P3-72 | **Sechster Fall des Musters „Port richtig, native Umsetzung falsch" — mit einer Verschaerfung.** Nach SCP (P3-32), HFE (P3-35), TD0 (P3-37), KryoFlux (P3-46) und STX ist DMS der sechste belegte Fall. Neu ist die Verschaerfung: hier stand die richtige Fassung nicht bloss **daneben**, sie war **abgekoppelt** — registriert war die falsche. Das Muster ist damit nicht mehr nur eine Aussage ueber Entstehung (portierter Code traegt die Randfaelle seiner Vorlage, selbst geschriebener die, an die jemand gedacht hat), sondern auch eine ueber **Verdrahtung** | sechs Faelle, jeder einzeln gemessen (MF-837) | ⚠ **offen als Frage, nicht als Auftrag: wie viele weitere Doppelumsetzungen gibt es, und welche ist jeweils registriert?** `audit_orphan_modules.py` misst Module ohne Aufrufer, `tuersucher.py` Symbole ohne Leser — beide finden `uft_dms.c` **nicht**, weil sie einen Aufrufer sieht: den Test. Genau diese Luecke hat den Fall vier Monate getragen. Ein Tor dafuer waere: **Modul, dessen einzige Aufrufer Tests sind, waehrend eine zweite Umsetzung derselben Sache registriert ist.** Das ist messbar (Aufrufer je Modul, Registrierungen je Format) und traegt nach Regel 9 auf die Kennzahl „ungepruefte Formate": eine registrierte Zweitumsetzung ist per Definition ungeprueft, wenn die gepruefte daneben liegt |
| P3-73 | **11 von 17 Eintraegen der FDC-Gap-Tabelle beschreiben eine Spur, die physisch nicht passt — und `gap3_fmt = 84` stand in VIER Eintraegen mit DREI verschiedenen Sektorzahlen.** Berichtigt wurde in MF-838 nur PC 1.44M, weil nur dafuer eine benannte Quelle vorliegt: die DDPT des PC fuehrt **zwei** Gap-3-Werte getrennt (FreeDOS FORMAT 0.92, `floppy.c:402` `ddptPrinter` zeigt `gap3_length_rw` und `gap3_length_xmat` nebeneinander), und Ch. Hochstaetters Tabelle (FDFORMAT/88 1.8, ueber `floppy.c:952`) nennt fuer 18 Sektoren Format-Gap `0x6C = 108` und BIOS-R/W-Gap `0x1B = 27`. Der Eintrag trug `gap3_rw = 108` — den FORMAT-Gap — und `gap3_fmt = 84`, den Wert von 1.2M. **`gap4b` wurde von 400 auf 78 ABGELEITET**, weil die Spur mit dem belegten Format-Gap sonst nicht passt (12806 > 12500); das ist Rechnung, keine Quelle, und steht so im Code | Rotbeweis ueber die Mutationsprobe: alter Zustand wiederhergestellt -> **4 von 4 Pruefungen rot** und das Tor 11 -> 12 -> FEHLER (`tests/test_fdc_gaps_1440k.c` + `scripts/audit_fdc_gaps.py`, MF-838) | ✅ **teils behoben, der Rest SICHTBAR statt uebertuencht.** Die zehn uebrigen Eintraege (BBC, PC-98, Amstrad, MSX, 8-Zoll-FM, Atari) haben keine Quelle. Sie mit abgeleiteten `gap4b`-Werten stimmig zu machen waere arithmetisch moeglich und **schlechter**: sie wuerden konsistent, ohne richtig zu werden, und der Widerspruch fiele nicht mehr auf. **Milderung, gemessen:** `gap3_fmt` wird im ganzen Baum an **null** Stellen gelesen, und das Modul `src/formats/uft_fdc_gaps.c` steht als Orphan in `docs/orphan_baseline.txt:213` — alle sechs oeffentlichen Funktionen haben null Aufrufer. Der Fehler war also **latent**. Latent ist nicht harmlos: die Tabelle ist genau das, was ein kuenftiger Formatierer benutzen wuerde |
| P3-74 | **Woher die 664 kommt — eine Herleitung, die MF-828 von der anderen Seite bestaetigt.** Die PC-, MSX- und Amstrad-Eintraege tragen `gap4b = 664`. Das ist der **Atari-ST-Wert**, und er laesst sich Byte fuer Byte nachrechnen: die Satzlaenge ohne Gap 3 betraegt 574 Byte (12 Sync + 3×A1 + FE + 4 CHRN + 2 CRC + 22 Gap 2 + 12 Sync + 3×A1 + FB + 512 + 2 CRC). Mit dem ST-Gap 3 von 40 ergibt das **614** — genau Louis-Guerins Sektorabstand —, und `60 + 9 × 614 = 5586`, also `6250 − 5586 = 664`. Mit dem IBM-Gap 3 von 80 ergibt es 654, und `80 + 50 + 9 × 654 = 6016`, also `6250 − 6016 = **234**`. Der ST-Eintrag ist damit **richtig** (sein Gap 3 von 40 ergibt exakt 614); die 664 ist nur in die falschen Zeilen kopiert. **Und die Atari-Eintraege liegen um exakt 60 Byte daneben**, weil sie den 60-Byte-Vorspann DOPPELT fuehren (`gap4a = 60` UND `gap1 = 60`), wo Brods Write-Track-Tabelle nur einen nennt: „Intro: 60 bytes of $4E" | aus den Tabellenwerten selbst hergeleitet, gegen MF-828 gehalten (MF-838) | ⚠ **offen als Herleitung, nicht als Quelle** — deshalb steht sie in `docs/fdc_gaps_baseline.txt` und nicht im Code. Bemerkenswert ist die Richtung: MF-828 hat 614 und 664 aus Brods Write-Track-Tabelle **hergeleitet** und damit Louis-Guerins Messung getroffen; hier tauchen dieselben zwei Zahlen als **Fehlerspur** in einer fremden Tabelle wieder auf. Zwei unabhaengige Wege, dieselbe Arithmetik — das ist der beste Beleg dafuer, dass die Rechnung aus MF-828 stimmt |
| P3-75 | **Die SED-Medienbyte-Tabelle ist feldgenau bestaetigt — und das Medienbyte identifiziert das Format trotzdem nicht.** FreeDOS FORMAT 0.92 (`floppy.h:25`) fuehrt 17 PC-Formate mit allen BPB-Feldern. Gegen die Tabelle aus SEDs Hilfedatei (Stepper/Brod 1996, Atari) gehalten, decken sich **alle fuenf gemeinsamen Formate feldgenau**: 360K (`0xFD`, SPF 2, Root 112, SPC 2), 720K (`0xF9`/3/112/2), 1.2M (`0xF9`/7/224/1), 1.44M (`0xF0`/9/224/1), 2.88M (`0xF0`/9/240/2). Zwei unabhaengige Quellen, **28 Jahre** auseinander, verschiedene Plattformen. **Die Qualifikation dazu:** `0xF9` steht fuer FD720, FD1200, FD800 **und** FD1494; `0xF0` fuer FD1440, FD2880, DMF, FD1680x, FD3360, FD1743 **und** FD3486. Die SED-Regel „`$FD` = Tabellenwerte nehmen, `$F9` = BPB lesen" bleibt richtig, sagt aber nur, **ob** man den BPB braucht, nicht **welches** Format vorliegt | vom Eigentuemer feldweise gegengerechnet (MF-838) | ✅ **Bestaetigung, kein Auftrag** — mit einem UNRESOLVED und vier Fundus-Punkten. **UNRESOLVED: DMF gegen FD1680x.** Beide 80×2×21 = 3360 Sektoren = 1 720 320 Byte, Unterschied allein in der Wurzelverzeichnisgroesse (112 gegen 224) — und der Autor selbst ist unsicher (`floppy.h:56`: *„is this really 16 root dir entries? changing to 112!"*). Drei Werte im Umlauf: 16, 112, 224. Ueber die Groesse sind die beiden Formate **nicht trennbar**; `src/formats/img/uft_img.c:44` fuehrt den Namen „1.68MB 3.5\" DMF" — eine von zwei Moeglichkeiten. **Fundus:** (a) Skew und Interleave klassenweise — Standardformate 0/0, Ueberformate 3/3, DMF 3/2 (`floppy.h:51`), mit der Begruendung in `floppy.c:941` („over-formats always have to use interleave … to help gap size"); das ist die PC-Haelfte von P3-65 (5) und P3-70 (4). (b) `0xF6` als **fuenftes** Herkunftssignal (DDPT `fill_char_xmat`) — und die Querverbindung traegt: Brods Regel „beide Bytes des Fuellwortes ≤ `$F4`" schliesst `0xF6` fuer eine Atari-Format-Track-Operation **aus**, damit trennt das Fuellbyte PC- von ST-Formatierung unabhaengig von Geometrie und BPB. (c) Harte Grenze **23 Sektoren/Spur** bei 500 kbps (`floppy.c:967`). (d) Sechs fehlende Formate, davon zwei mit Groessenkollision (409 600 = PC FD400 gegen Atari ST 80×1×10; 819 200 = PC FD800 gegen D81 gegen ST 800K gegen Copylock-Keydisk) |
| P3-76 | **35 Funktionsnamen sind in oeffentlichen Headern mit WIDERSPRUECHLICHEN Signaturen deklariert — Tor gebaut MF-839.** C prueft Deklarationen nicht ueber Uebersetzungseinheiten hinweg: zwei Header duerfen dasselbe Symbol verschieden deklarieren, und es **linkt trotzdem**. Ein ueberzaehliges Argument wird auf x86-64 stillschweigend verworfen, ein fehlendes aus einem Register gelesen, das niemand gesetzt hat. Kein Compilerfehler, kein Linkerfehler, kein Absturz — nur ein falscher Wert. **Die drei schwersten:** `uft_crc16_ccitt` steht in **vier** Headern; drei nennen `(const uint8_t*, size_t)`, `uft_decoder_plugin.h:361` nennt zusaetzlich `uint16_t init`. Es gibt **eine** Definition (`uft_protection_ext.c:171`, zwei Parameter) — wer jenen Header einbindet und einen Startwert uebergibt, bekaeme eine CRC, die ihn **ignoriert**. `uft_disk_free` steht in drei Headern mit **drei verschiedenen Zeigertypen** (`uft_disk_image_t*`, `uft_disk_t*`, `uft_disk_unified_t*`) — ein `free()` auf dem falschen Typ ist die teuerste Variante dieser Klasse. Und **15 `bam_*`-Namen** durchgehend `bam_context_t*` gegen `bam_editor_t*`: ein halb vollzogener Umbau | `scripts/audit_decl_conflicts.py`, Selbsttest 3/3 mit zwei Gegenbeweisen; Gegenprobe am Baum: ein neuer widerspruechlicher Name -> 35 -> 36 -> FEHLER (MF-839) | ⚠ **offen, Grundlinie 35.** Jeder Fall braucht eine **Entscheidung**, welche Signatur gilt, und die betrifft Aufrufer — Eigentuemerarbeit, keine Sammelkorrektur. Eine Sammelkorrektur waere hier besonders gefaehrlich, weil sie stillschweigend Aufrufer umdeutet. Das Tor haelt die Zahl fest, damit keine 36. dazukommt. **Der Baum fuehrt fuer diese Klasse einen Agenten** (`abi-bomb-detector`) **und ein Prinzip** (`single-source-enforcer`) **— und hatte bis MF-839 kein Tor.** Grenze, ausgeschrieben: keine Praeprozessor-Bedingungen, keine Typaliase, und **kein Abgleich gegen die Definition** — ein Name mit nur EINER Deklaration faellt nicht auf, selbst wenn die Definition abweicht |
| P3-77 | **`0xCDB4` und `0xFFFF` sind kein Widerspruch — nachgerechnet, und der Baum liegt mit beiden richtig.** Terry Ritter, „The Great CRC Mystery", Dr. Dobb's Journal 11(2), Feb. 1986, erklaert, warum ein Startwert noetig ist: bei einem Nullregister aendert ein verarbeitetes Nullbit den Rest nicht, vorangestellte Nullbits waeren unerkennbar. Selbst gerechnet (CRC-CCITT, Poly `0x1021`, init `0xFFFF`): **`0xFFFF` ueber `A1 A1 A1` ergibt `0xCDB4`**. `0xCDB4` ist also kein anderer Startwert, sondern **derselbe nach der Syncfolge**. Gegenprobe: `IDAM {A1,A1,A1,FE,00,00,01,02}` ab `0xFFFF` = `0xCA6F`; dieselben Daten ohne die drei `A1` ab `0xCDB4` = `0xCA6F` — identisch. **Zwei weitere Eigenschaften, die der Baum nicht nutzt:** laesst man Daten UND angehaengte CRC gemeinsam durchlaufen, ergibt sich **`0x0000`** (Residuum) — eine zweite, strukturell verschiedene Pruefform, die das CRC-Feld nicht als separat adressiertes Objekt braucht und deshalb bei unsicheren Feldgrenzen (Rohspur, STX) robuster ist als der Wertvergleich. Und **`0x1D0F` ist eine ANTI-KONSTANTE**: sie ergibt sich nur mit **komplementiert** angehaengter CRC (HDLC, X.25, XMODEM-CRC), nie bei einem FDC. Nachgerechnet: mit Komplement `0x1D0F`, ohne `0x0000` | alle Werte selbst gerechnet, nicht uebernommen (MF-839) | ⚠ **offen als Schutznotiz und als zwei ungenutzte Werkzeuge.** Der Baum traegt `0xCDB4` an fuenf Stellen; nur `uft_86f.c:408` und `src/samdisk/CRC16.h:10` nennen die Herkunft. `uft_dmk_parser_v2.c:200` traegt sie ohne Begruendung — **genau so entsteht spaeter ein „Fix", der etwas Richtiges kaputtmacht** (vgl. P3-49, P3-35). Nebenbefund zur Benennung: `dmk_crc_init(bool double_density)` kodiert die **Dichte**, wo die eigentliche Groesse die **Anzahl der Syncbytes** ist. Startwerte je Anzahl, gerechnet: 1×`0x443B` · 2×`0x968B` · 3×`0xCDB4` · 4×`0x192A` · 7×`0x35F7` · 11×`0x6B84`. **UNRESOLVED**, ob der WD1772 bei den von Louis-Guerin belegten sieben Syncbytes alle sieben in die CRC nimmt oder bei jedem `$F5` neu vorlaedt — „preset at each $F5" spricht fuer Letzteres, dann bleibt `0xCDB4` immer richtig und die Tabelle ist nur Rueckfallebene |
| P3-78 | **Die CRC-16-Entwurfsgrenze: 4095 Byte — und der Baum prueft weit darueber, ohne es zu sagen.** Ritter (1986): 16-Bit-Polynome sind fuer Bloecke bis 2^15−1 Bit entworfen; darueber entfallen zugesicherte Eigenschaften, namentlich die Erkennung **aller** 2-Bit-Fehler. Innerhalb der Grenze garantiert CRC-CCITT: jeden zusammenhaengenden Fehlerburst kuerzer als das Polynom, jede ungerade Fehleranzahl, jeden 2-Bit-Fehler — also **jede** Anordnung von 1, 2 oder 3 Bitfehlern. Angewandt: 512-B-Sektor 4096 Bit ✓ (und das ist kein Zufall — das Polynom stammt aus der IBM-8-Zoll-Diskettenspezifikation), 1024-B-Sektor ✓, **XDF-8-KB-Sektor ✗**, **ganze Spur 6250 B ✗**, **DMS-Trackblock bis 32 000 B ✗ (achtfach darueber)**. Zur Einordnung nennt Ritter die Zahlen: Einzelbyte-Summe ≈ 99,29 %, CRC-16 ≈ 99,998 % — die CRC laesst einen Fehler rund **460-mal seltener** durch | Quelle gelesen, Bitrechnung trivial (MF-839) | ⚠ **offen, und es ist eine Formulierungsfrage mit Substanz.** Es ist **kein Fehler**, die CRC dort zu pruefen — das Format schreibt sie vor. Aber „CRC ok" traegt dort eine **schwaechere** Aussage, und ein forensischer Bericht darf sie nicht als die starke ausgeben. Konkret betroffen: der DMS-Pfad prueft seit MF-837 beides (CRC ueber die gepackten, Pruefsumme ueber die entpackten Daten) — zwei unterschiedlich starke Aussagen, die der Bericht heute gleich behandelt. **Nebenbefund, gemessen:** der Baum traegt **36** eigenstaendige CRC-16-Definitionen. Die beiden gleichnamigen `sap_crc16` in `uft_sap_plugin.c:40` (bitweise, 2 Parameter) und `uft_sap_parser_v2.c:144` (tabellengestuetzt, 3 Parameter) wurden gegeneinander gerechnet: **2000 Zufallspuffer, null Abweichungen**. Also Wartungsschuld und latente Gefahr, **kein Fehler heute** — beide `static`, deshalb kein Linkerfehler, deshalb faellt es nicht auf |
| P3-79 | **Artefaktsignaturen identifizieren CODEFAMILIEN, nicht Produkte — an einem Korpus von 17 C64-Kopierprogrammen belegt.** „Halbspurcopy" (deutsch, Banner „C U B") und „Mr. Nibble 40 Tracks" (italienisch) teilen **851 Byte byteidentischen 1541-Laufwerkscode**, gleicher SHA1. Eine von beiden geschriebene Diskette traegt dieselbe Signatur; sie sind **an der Diskette nicht unterscheidbar**. Das ist die Umkehrung des Fehlers, den MF-820 in `uft_atarist_protection.c` zurueckgezogen hat: dort wurde einer Beobachtung ein Produktname zugewiesen. Ein Bericht darf „Werkzeug der Turbo-Nibbler-Familie" schreiben, nicht „Halbspurcopy" | vom Eigentuemer disassembliert und gehasht (MF-839) | ⚠ **offen, mit vier belegten Einzelheiten.** **(1) Die `$55`-Gap-Signatur, jetzt mit Laengen:** Turbo Nibbler schreibt je Sektor `5×$FF | 12 B Header | 7×$55 | 5×$FF | 138×$07 | 8×$55`, identisch ueber V1.0 (`$0ECD`, `JSR $06D5`) und V2.0 (`$149E`, `JSR $0681`). Das **robustere** Merkmal ist dabei nicht das Fuellbyte, sondern dass **alle Gaps einer Spur exakt gleich lang** sind — ein Original hat das nie. **Voraussetzung dafuer sind die Feldgrenzen aus P3-59**, die heute kein Erzeuger fuellt. **(2) Killer-Track gegen unformatiert:** „nur Sync" und „kein Sync" sind **gegensaetzliche** Befunde; die Zaehlung in `uft_g64_parser_v3.c` wirft sie zusammen. Verfahren aus Turbo Nibbler `$0DDB`: 5 Versuche, 256 Iterationen Zeitfenster. **(3) Halbspuren sind die NATIVE Stepperaufloesung** (`$1C00` Bit 0-1, vier Phasen = zwei Vollspuren) — Vollspuren sind der Sonderfall. Und zwei Grenzen, die verwechselt werden: **Positionieren** ist trivial, **Schreiben** neben einer belegten Nachbarhalbspur ist physisch unmoeglich. **(4) `$1C0C` Bit 1 schaltet Byte-Ready waehrend der Kopfbewegung AB** — Rauschen an dieser Stelle im Flussabzug ist **kein Befund**. Dazu: die sechs CBM-DOS-Fehlercodes (20, 21, 22, 23, 27, 29), die die D64-Fehlerkarte traegt, kennt der Baum **nicht** (gemessen: null Treffer in `src/formats/d64/` und `src/formats/g64/`) — waehrend `g64_export_d64()` sie fest verdrahtet verwirft. **ZURUECKGEZOGEN vom Eigentuemer:** die `0x01`-Ersetzung durch Burst Nibbler — die Quelle (Rittwage) ist gut, das vorliegende Binary ist ein gepackter Crack, die Behauptung also **nicht am Erzeuger geprueft**. UNRESOLVED, nicht implementieren |
| P3-80 | **Ein spurgenauer ADF-Teilabzug war „kein ADF" — und ein naiver Fix haette die fehlenden Spuren erfunden. Behoben MF-840.** `TransDisk` (Amiga, ueber `trackdisk.device`) zieht mit `-s`/`-e` Spurbereiche ab; das ist das **dokumentierte Beispiel seines Autors** (`transdisk >RAM:df1.adf.1 -d trackdisk 1 -s 0 -e 39`), und die Endung `.1` zeigt, dass eine `.2` folgt. Auf Amigas mit knapper RAM-Disk oder bei serieller Uebertragung war das der Normalfall. `adf_plugin_probe()` und `adf_open()` verlangten **exakte** Gleichheit mit 901 120 oder 1 802 240 Byte — ein solcher Abzug war damit **gar nichts**, nicht „ADF, unvollstaendig". Die Erkennung fiel durch und die Datei ging an den naechsten Kandidaten | Rotbeweis dreiteilig, drei Gegenproben hielten schon (`tests/test_adf_teilabzug.c`, MF-840) | ✅ **behoben, und der Patch hat ZWEI Haelften.** Die Groessenpruefung bloss zu lockern waere ein **Rueckschritt** gewesen: `adf_read_track()` fuellte einen kurzen Lesevorgang mit `memset(buf, 0xE5, …)` und meldete `UFT_OK` — die 80 fehlenden Spuren waeren als **leere, formatierte Diskette** erschienen. `0xE5` ist die AmigaDOS-Formatfuellung; dieselbe Klasse, die MF-837 aus dem DMS-Plugin entfernt hat. Jetzt: Teilabzuege annehmen (Konfidenz 35, mit Bootblock 70 — nie 95), fehlende Spuren mit `UFT_ERROR_NOT_FOUND` und einer Meldung, welche von wie vielen Spuren die Datei traegt. **Zwei eigene Fehler unterwegs, beide vom Test gefangen:** (1) 901 120 ist **auch** ein Vielfaches der HD-Spurlaenge (÷ 11 264 = 80) — meine „HD zuerst"-Regel stufte die **vollstaendige** DD-Diskette als HD-Teilabzug ein; vollstaendige Groessen muessen zuerst geprueft werden. (2) Dateien, die nicht einmal ein 512er-Vielfaches sind, hatte ich angenommen — `test_plugin_probe_real` verlangt zu Recht, dass 901 119 durchfaellt, denn an der Groesse ist so eine Datei nicht mehr erkennbar |
| P3-81 | **„Spur 79" stand an ZWEI Stellen — die zweite ist lebender Code. Zurueckgezogen MF-841.** MF-820 hat `uft_atarist_protection.c` entschaerft; `src/formats/ipf/uft_ipf_ctraw_v2.c:399` trug dieselbe falsche Annahme und leitete daraus aktiv einen Namen ab: `has_extra_track && has_weak_track` → `IPF_PROT_COPYLOCK`, also aus „es gibt eine Spur ≥ 80" **und** „irgendwo mehr als 100 Weak Bits". **Rob Northen Copylock (Atari ST) liegt auf Spur 0, Sektor 6** — drei unabhaengige Quellen: der Herstellerquelltext `keydisk.s` (1990, „DO NOT OVERWRITE SECTOR 6 ON TRACK 0"), Louis-Guerins Katalog (Populous und Back to the Future: T0-S6) und das entschluesselte Laufzeitdisassemblat der Pruefroutine | vom Eigentuemer disassembliert, beide Codestellen gemessen (MF-841) | ✅ **Benennung zurueckgezogen, Beobachtung erhalten.** Der eigentliche Erkenntnisgewinn ist der Mechanismus: der Schutz liest Sektor 6 und misst ihn **gegen Sektor 5 als Bezugsgroesse**, verlangt ≥ 1 % mehr Lesezeit und misst am Original `0xC66` gegen `0xCEB` = **4,19 %** — was Louis-Guerins „4–5 %" **unabhaengig bestaetigt**. Copylock braucht also **zwei** Sektoren: der Schutz bildet ein VERHAELTNIS, weil die absolute Lesezeit mit der Laufwerksdrehzahl schwankt. Ein Erkenner ohne Bezugssektor hat keinen Nenner. Nachgerechnet und bestaetigt sind auch die zwei Konstanten der Schluesselableitung: `"Rob Northen Comp"` als vier Langworte von 0 subtrahiert ergibt `0xB34C4FDC`, minus `0x6CC60666` und `0xA6AB2ADF` ergibt `0x9FDB1E97`. **Zweitens ist die Heuristik fuer IPF ueberfluessig:** das Dichtefeld im Track-Datensatz NENNT „Copylock ST" (Wert 5, seit MF-823 ueber `ipf_air_density_name()` durchgereicht). Eine Mustererkennung neben einer Formatangabe ist die schwaechere Hand. **Nebenbefund, offen:** dieselbe Datei rechnet `protection_confidence = 0.5f + 0.1f * count` — eine Zahl, die wie eine Wahrscheinlichkeit aussieht und aus einer Abzaehlung stammt. Erreichbar ist sie heute nicht (`detected_protections` hat ausserhalb der Datei keinen Leser), aber sie gehoert durch die Nennung der zutreffenden Merkmale ersetzt, nicht durch eine andere Formel |
| P3-82 | **Zwei Lesezugriffe hinter dem Feld: das Enum wuchs, eine der abgeleiteten Tabellen nicht. Behoben MF-842, Tor gebaut.** `ufm_c64_scheme_detect.c` fuehrt `names[]` mit **18** designierten Initialisierern; das Enum in `ufm_c64_protection_taxonomy.h` ist auf **25** Werte gewachsen (`UFM_C64_PROT_WEAK_BITS` … `LONG_TRACK`), und die Schranke lautet `type < UFM_PROT_COUNT` — die Indizes 18–24 lesen hinter dem Feld. `uft_pc_protection.c` fuehrt `vendor_names[]` mit **22** Eintraegen gegen ein Enum mit **35**; die dreizehn neueren (`ARMADILLO` … `SUBCODE`) fehlen. **Die Schwestertabelle `protection_names[]` ist korrekt mitgewachsen** — nur die Herstellertabelle blieb zurueck | gemeldet vom Eigentuemer (cppcheck, jeder Fund von Hand gegengeprueft), im Baum feldweise nachgemessen (MF-842) | ✅ **behoben — strukturell, nicht durch Nachtragen.** Beide Tabellen tragen jetzt eine ausdrueckliche Dimension (`[UFM_PROT_COUNT]`, `[UFT_PCPROT_COUNT]`): der Compiler erzwingt die Laenge, fehlende Eintraege sind NULL, und die Zugriffsfunktionen fangen das bereits ab. **Erfundene Herstellernamen waeren die falsche Reparatur gewesen** — sie haetten den Zugriff sicher gemacht und dabei Angaben behauptet, fuer die keine Quelle vorliegt. **Erreichbarkeit, gemessen:** `ufm_c64_prot_type_name()` wird aus `src/gui/ProtectionAnalysisWidget.cpp` gerufen, und die GUI hat fuer genau die sieben fehlenden Werte bereits ein Spalten-Mapping — dass heute kein Detektor sie SETZT, ist der einzige Grund, warum der Pfad noch nicht zuendet. `uft_pcprot_vendor()` ist nicht-statisch und im Header deklariert |
| P3-83 | **Der tote Zwilling mit zwei Fehlern — entfernt MF-842.** `uft_protection_params.c:102` trug ein zweites, gleichnamiges `uft_protection_type_name_local()`: `static`, im ganzen Baum **ohne Aufrufer**, und mit zwei Fehlern darin. **(1)** Die Schleife `while (t > 1 && idx < 14) { t >>= 1; idx++; }` zaehlt bis `t <= 1` statt bis `t == 0` — jedes Flag bekam einen um 1 zu niedrigen Index: `UFT_PROT_FUZZY_BITS` (0x0001) lief null Mal durch und lieferte „None" statt „Fuzzy Bits". **(2)** Fuer `UFT_PROT_CUSTOM` (0x8000, Bit 15) reichte der Deckel `idx < 14` nicht, um `t` zu reduzieren → `g_type_names[14]` bei 14 Eintraegen | vom Eigentuemer gemeldet und als tot erkannt, im Baum gegengeprueft (MF-842) | ✅ **geloescht statt repariert.** Eine korrekte Fassung desselben Namens steht in `uft_protection_detect.c:701` und hat dort einen Aufrufer. Ein zweiter Namensvetter ermoeglicht genau die Verwechslung, die den Fehler entstehen liess — und er ist derselbe Fall wie die zwei gleichnamigen `sap_crc16` aus P3-78, nur diesmal mit einem echten Fehler im toten Zwilling statt einer blossen Doppelung |
| P3-84 | **21 Dateihandles, die bei einem Fehler offen blieben. Behoben MF-846, hartes Tor gebaut.** Das Muster ist immer dasselbe: `fopen` gelingt, dann bricht die `fseek`-Schranke ab und die Funktion kehrt zurueck, **ohne** `fclose` — waehrend der **Nachbarzweig in derselben Funktion** sauber aufraeumt (`if (!data) { fclose(f); return UFT_ERR_MEMORY; }`). Bei allen 21 Stellen war es so; es ist eine vergessene Klammer in einer kopierten Vorlage, keine Entscheidung. **Vier davon standen nicht auf der gemeldeten Liste** (`uft_adf.c` 767/771/802, `uft_cbm_formats.c:842`) | gemeldet vom Eigentuemer (cppcheck, 17 Stellen), im Baum nachgemessen und auf 21 erweitert, jede Fundstelle von Hand gegengeprueft (MF-846) | ✅ **behoben.** **Zwei der 21 trugen einen zweiten, schwereren Fehler:** `uft_hdf_read_block()` gab bei fehlgeschlagenem Seek **`0` zurueck — in dieser Funktion ERFOLG** (`return (read == 512) ? 0 : -1;`); der Aufrufer haette einen **unberuehrten Puffer als gelesenen Block** bekommen. Das ist keine Ressourcen-, sondern eine Datenfrage — nur heute nicht erreichbar, weil die Funktion im ganzen Baum **keinen Aufrufer** hat (Tuer ohne Leser). Und `uft_cbm_t64_read()` verlor im selben Zweig das Handle, den gerade gebauten Eintrag samt `file->data` **und die ganze bereits aufgebaute `*files`-Liste**, und meldete `-1` in einer Funktion, die sonst `UFT_CBM_*` oder die Anzahl liefert — repariert nach dem **Zweig unmittelbar darunter**, der den Eintrag ueberspringt statt abzubrechen. **Ehrlich zur Erreichbarkeit:** `fseek(f, 0, SEEK_SET)` auf einer regulaeren Datei schlaegt praktisch nie fehl; der Zweig zuendet bei einer FIFO oder einem Geraetepfad (`ESPIPE`). Der Rotbeweis ist deshalb **das Tor, nicht ein Laufzeittest** — dieselbe Bauform wie MF-842 |
| P3-85 | **Der Belegtyp — als Zahl, nicht als Struktur. Tor gebaut MF-843.** Vorschlag des Eigentuemers, nachdem er in dieser Reihe **zweimal** eine eigene Behauptung zuruecknehmen musste (Aussagekraft der FAT-Volumeseriennummer; `0x01`-Ersetzung durch Burst Nibbler) — beide Male, weil eine **Beschreibung** als **Messung** weitergegeben wurde. Sein Schluss: eine Regel, die ihren Belegtyp mitfuehrt, kann nicht versehentlich zu einem Befund aufsteigen, und `may_assert = false` ist maschinell durchsetzbar, wo Vorsicht es nicht ist. **Dieselbe Klasse ist mir unterlaufen:** P3-65 (3) hielt fest, `atari_check.c` pruefe Eintraege hinter der Verzeichnis-Endmarke nicht — uebernommen ohne Messung, und falsch (MF-835: der Prueflauf gab es, er konnte nur nicht feuern) | 84 Eintraege ausgezaehlt, 19 ohne Belegtyp; Gegenprobe 19 -> 20 -> FEHLER (`scripts/audit_evidence_marker.py`, MF-843) | ✅ **umgesetzt — aber NICHT als C-Struktur.** Der Entwurf sah `uft_rule_meta_t` mit `uft_evidence_t` und `may_assert` im Code vor. Als C-Struktur waere das ein Feld, das niemand liest — genau der Fall, den `audit_dead_fields.py` seit MF-831 misst (1231 im Bestand). Ein Belegtyp, den kein Verbraucher abfragt, schuetzt niemanden. Die tragfaehige Fassung ist die **Belegspalte dieses Dokuments**, wo der Beleg bereits steht — das Tor macht daraus eine Zahl. **Die 19 bleiben absichtlich stehen:** fast durchweg fruehe Eintraege aus der Zeit vor der Gewohnheit. Sie nachtraeglich zu beschriften hiesse, ihren Beleg zu REKONSTRUIEREN — genau die Bewegung, gegen die das Tor steht. **Grenze, ausgeschrieben:** die Erkennung arbeitet mit einem Wortschatz und prueft die FORM, nicht die Wahrheit; wer „gemessen" schreibt, ohne gemessen zu haben, faellt nicht auf |
| P3-86 | **P3-32 und P3-33 standen DOPPELT im SSOT der offenen Punkte — bereinigt MF-843.** Gefunden beim Bau des Belegtyp-Tors, bevor es verdrahtet war. Beide Paare: eine kuerzere Fassung an der numerisch richtigen Stelle, eine laengere am Tabellenende. Vor dem Loeschen geprueft, dass nichts verloren geht — alle Zahlen, Hexwerte und Dateinamen der kurzen Fassung kommen auch in der langen vor, bei P3-32 zusaetzlich elf weitere. Behalten wurde der VOLLE Text an der numerisch richtigen Stelle | beide Fassungen ausgezaehlt und Kernbegriffe gemessen (MF-843) | ✅ **bereinigt: 86 Zeilen -> 84, keine doppelte Kennung mehr.** Die beiden Fassungen **widersprachen** sich nicht — es war Redundanz, nicht Konflikt, und damit die mildere Form des Fehlers. Offen bleibt eine verwandte Kleinigkeit: zwei Zeilen sind aus der Tabelle **gebrochen** (Prosafragmente zwischen den Eintraegen, entstanden durch ein `|` oder einen Zeilenumbruch im Text). Sie stoeren die Anzeige, nicht die Auswertung — die Tore zaehlen ueber `^\| P3-\d+ \|` |
| P3-87 | **Ein `fprintf(stderr)` in `read_track()` — meine eigene Regression aus MF-823, behoben MF-844.** Die Stelle meldet die Schutzart aus dem IPF-Dichtefeld und sitzt in `ipf_plugin_read_track()`. Sie feuert damit **einmal je Spur**: bei einer Amiga-Diskette bis zu **160 Zeilen auf stderr**, unabhaengig davon, ob der Aufrufer sie sehen will — und an einem Kanal vorbei, den niemand abschalten kann. MF-823 hatte die Meldung richtig eingefuehrt und den Kanal uebersehen | im Baum gemessen: 10 `fprintf(stderr` in `src/formats/`, 33 in `src/` insgesamt (MF-844) | ✅ **behoben** — `UFT_INFO`, und das ist zugleich die richtige Stufe: eine Feststellung **aus der Datei**, keine Warnung. Beide Testziele, die `uft_ipf_plugin.c` linken, fuehrten `uft_log.c` bereits, die Linkfalle aus MF-833/836/840 blieb diesmal aus. **Die uebrigen neun im Formatlayer sind NICHT angefasst** — sie sind nicht meine und stehen ueberwiegend in Fehlerpfaden, wo stderr vertretbar ist. Gegen ein Tor spricht heute die Zahl: 33 Stellen im ganzen Baum sind zu wenige, um eine Grundlinie zu rechtfertigen, und zu viele, um sie ungefragt umzuschreiben |
| P3-88 | **Das Multi-Read-Voting konnte einen Sektor ERFINDEN, den keine Lesung enthielt. Behoben MF-845.** `multiread_execute()` baute den Ausgabepuffer ueber `vote_buffer()` — eine **byteweise unabhaengige** Mehrheitsabstimmung — und rief `classify_passes()` erst **danach** auf (Zeile 480 gegen 526). Die Einstufung konnte den fertigen Puffer also nicht mehr beeinflussen. Bei zwei Lesungen, die BEIDE ihre CRC bestehen und sich trotzdem unterscheiden (`MULTIREAD_CLASS_AMBIGUOUS_GOOD`, der Normalfall bei Fuzzy Bits), ergibt die byteweise Mehrheit eine **dritte** Bytefolge — bei Gleichstand gewinnt in `vote_byte()` der kleinere Bytewert, stillschweigend. Das Ergebnis landet ueber `uftc_adf_place_voted()` / `uftc_d64_place_voted()` unveraendert im Zielabbild, sobald `res.recovered` gilt, und das ist bei zwei CRC-validen Lesungen trivial erfuellt | Rotbeweis mit drei Gegenproben, gemessen: `Pass A 01 FF 02 FF 03 FF 04 FF` und `Pass B FF 01 FF 02 FF 03 FF 04` ergaben `01 01 02 02 03 03 04 04` — weder A noch B (`tests/test_multiread_kein_mischbyte.c`, MF-845) | ✅ **behoben.** Erst einstufen, dann entscheiden: bei `AMBIGUOUS_GOOD` wird eine **ganze beobachtete Lesung** uebernommen statt eines Mischbytes. `vote_buffer()` laeuft weiter fuer die **Kennzahlen** (Konfidenz, Weak-Maske) — die kommen aus den Lesungen, nicht aus der Ausgabe. **Fuer ein Werkzeug mit dem Grundsatz „Keine erfundenen Daten" ist das die schwerste Klasse: hier ging nichts verloren, hier ENTSTAND etwas.** Das Referenzverhalten stand im eigenen Kommentar bei `classify_passes()` — „a8rawconv behaelt einen und warnt" —, nur eben nicht getan; FluxEngine (`readerwriter.cc::collectSectors()`) markiert zwei abweichende `Sector::OK` als `Sector::CONFLICT`. Beide mischen nie auf Byteebene. **Nebenbei gegen die Zwillingsdrift:** die Auswahl der Bezugslesung ist als `multiread_reference_pass()` ausgeklammert und wird von Einstufung UND Ausgabe benutzt — zwei Fassungen derselben Regel driften (P3-78, P3-83). Und der Bezugspass wird nur uebernommen, wenn er mindestens so lang ist wie der Puffer: eine halbe Lesung mit gevotetem Rest zu verkleben waere dieselbe Fabrikation in klein |
| P3-89 | **Drei `#ifdef UFT_UNIT_TESTS`-Bloecke, die nie uebersetzt wurden. Behoben MF-851.** `UFT_UNIT_TESTS` wird im ganzen Baum **nirgends definiert** — weder im qmake-`.pro`, noch in einer `CMakeLists.txt`, noch als Compilerschalter. Die Selbsttests in `src/recovery/uft_multiread_pipeline.c` (108 Zeilen), `src/protection/uft_protection_detect.c` (98) und `src/parsers/a2r/uft_a2r_parser.c` (49) waren toter Code: nie gebaut, nie rot | ueber `git ls-files` gemessen, drei Dateien (MF-845), alle 19 Zusagen ausgefuehrt (MF-851) | ✅ **gehoben und entfernt.** Die Faelle laufen jetzt als `test_multiread_selbsttests_leben`, `test_schutz_erkennung_lebt` und `test_a2r_hilfsfunktionen_leben` in CI; die Bloecke sind weg, damit sie nicht driften. **Ergebnis der ersten Ausfuehrung: alle 19 Zusagen tragen.** Das macht den Befund nicht kleiner — richtig und ungeprueft ist nicht dasselbe wie richtig und bewacht; die Schutzerkennung war vier Monate unbewacht, waehrend MF-842 daneben einen gleichnamigen Zwilling mit zwei Fehlern fand. **Fuenf Gegenproben ergaenzt**, die im Original fehlten: beide Erkenner muessen auf einem Puffer OHNE ihr Muster schweigen (ein Erkenner, der nur auf seinem eigenen Muster geprueft wird, kann einer sein, der immer anschlaegt), schwache Bits duerfen bei Einigkeit nicht melden, die Viertelspur-Umrechnung muss ueber den ganzen Bereich umkehrbar sein statt an einem Wertepaar, und Einigkeit muss volle Konfidenz geben. **Zwei Zusagen liessen sich NICHT heben:** `uft_protection_type_name_local()` und `read_le16`/`read_le32` sind `static` — ein Test ausserhalb der Uebersetzungseinheit kommt nicht heran (am Binder gemessen). Das ist die Kehrseite solcher Bloecke: sie erreichen, was sonst niemand sieht, und duerfen genau deshalb nicht die einzige Pruefung sein. Nicht getan: die Funktionen oeffentlich machen — MF-842 hat den Zwilling geloescht, WEIL zwei gleiche Namen die Verwechslung ermoeglichen |
| P3-90 | **Warum ein grober Durchlauf hier 467 Fehlalarme liefert — und was der Trennstrich wirklich ist.** Beim Bau des Tors zu P3-84 lieferte „irgendein `return`, waehrend eine Datei offen ist" **467** Kandidaten; die engere Form „`return` an einer `fseek`/`fread`-Schranke ohne `fclose`" immer noch **347**. Beide Zahlen sind unbrauchbar, und der Grund ist lehrreich: die ueberwaeltigende Mehrheit steht auf einem **langlebigen** Handle (`p->file`, `ctx->fp`), das dem Plugin gehoert und in dessen `close()` geschlossen wird — dort ist `return` ohne `fclose` **richtig** | im Baum gemessen, drei Fassungen des Suchmusters nacheinander (467 / 347 / 21), die letzte Menge vollstaendig von Hand gegengeprueft (MF-846) | ✅ **als Verfahren festgehalten.** Der Trennstrich ist nicht die Schranke, sondern die **Herkunft des Handles**: stammt der `FILE*` aus einem `fopen` in DERSELBEN Funktion, ist er lokal und muss vor jedem `return` geschlossen werden. Damit fielen 347 auf 21, und alle 21 waren echt — **keine Ausschussquote**. Das ist die allgemeine Lehre fuer die naechsten Tore dieses Baums: ein Tor, dessen Fundmenge zu 94 % aus richtigem Code besteht, wird nicht gelesen, und ein ungelesenes Tor ist ein fehlendes Tor. Die Grenzen des Verfahrens stehen ausdruecklich im Kopf von `scripts/audit_fd_leaks.py` — alle drei erzeugen zu VIEL, nie zu wenig, was bei Grundlinie 0 die richtige Richtung ist |
| P3-91 | **Der STX-Leser, den Benutzer wirklich bekommen, hatte fuenf Feldfehler — darunter einen, der jeden Sektor jeder geschuetzten Spur falsch lieferte. Behoben MF-847.** Der Baum fuehrt **vier** STX-Leser; nur `src/formats/stx/uft_stx_plugin.c` (179 Zeilen) haengt an der Registry. Gemessen, je Fehler mit Rotbeweis: **(1) Die Fuzzy-Maske wurde nicht uebersprungen.** Zwischen den Sektordeskriptoren und den Nutzdaten liegen `fuzzyCount` Byte; `dataOffset` zaehlt ab DANACH. Der Leser rechnete `sec_desc_off + sec_count*16` und lieferte die **Maske als Sektorinhalt** — still, mit `UFT_OK`. Das trifft nicht den Randfall, sondern den Zweck des Formats: Spuren mit Fuzzy-Maske sind die geschuetzten. **(2) `trackCount` wurde als LE16 an 0x0A gelesen**, wo ein Byte steht — die `revision` an 0x0B rutschte mit hinein; bei revision=2 wurden aus 1 Spur 0x0201 = 513, nach der Klemme 200, gemeldete Geometrie **100 Zylinder statt 1**. **(3) Spursaetze wurden nach ihrer REIHENFOLGE in der Datei zugeordnet**, nicht nach ihrer eigenen Koordinate in Byte 0x0E (Spur in Bit 0-6, Seite in Bit 7) — bei einem Abzug, der nur einen Teil der Spuren enthaelt (der Normalfall bei STX), wanderte jede folgende Spur unter fremde Koordinaten. **(4) Das Flag SECT_DESC wurde mit `(void)` kassiert** und immer Deskriptoren angenommen. **(5) Stille Klemme `s < 26`** ohne Begruendung | Rotbeweis mit vier Faellen gegen den Plugin-Struct, gemessen: erstes Sektorbyte `0x5C` (Fuellbyte der Fuzzy-Maske) statt `0xA7` (Nutzdaten), 8 Byte zu frueh — `tests/test_stx_registrierter_leser.c`; Referenz benannt: AIR `pasti/PastiRead.cs:199-210` (GPL-3, J. Louis-Guerin, MF-698), im Baum als `uft_stx_air.c:333-346` portiert (MF-847) | ✅ **behoben.** Die Klemme ist **ersatzlos** weg statt gemeldet: `uft_track_add_sector()` waechst per `realloc` (Verdopplung ab 32), es gab nie einen Kapazitaetsgrund; die Formatgrenze ist `sectorCount` als uint16, die Dateigrenze faengt die Schleife ab. **Und der vorher gruene `test_stx_error_marks` wurde dabei rot** — seine Vorrichtung setzte Spur-Flags `0` und legte trotzdem drei Deskriptoren an, beschrieb also eine Datei, die es nicht geben kann. Er war gruen, WEIL der Fehler da war: solange das Flag kassiert wurde, konnte eine Vorrichtung ohne Flag nicht auffallen. Dieselbe Bauform wie MF-830 und MF-596; die Vorrichtung ist korrigiert, nicht der Test aufgeweicht. Mutationsprobe gemessen: `data_off` ohne `+ fuzzy_count` macht den Rotbeweis wieder rot |
| P3-92 | **Zwei einander widersprechende STX-Flagtabellen im selben Baum. Entschieden MF-852 durch eine zweite Hand.** `uft_stx_air.c:57-60` fuehrte `SECT_DESC 0x01 · PROT 0x20 · IMAGE 0x40 · SYNC 0x80`, `src/formats/atari/uft_stx_parser.c:38-49` fuer dieselben Bits `TRACK_IMAGE 0x01 · TIMING_DATA 0x20 · FUZZY_BITS 0x40`; auch die Sektor-Flags gingen auseinander. Eine der beiden musste falsch sein, und MF-847 hing an der ersten | **Hatari**, `src/includes/floppies/stx.h:40-45,83-85` (GPL-2-or-later), im bereits vorhandenen Klon unter `tools/uft-scout/work/hatari` gelesen (MF-852) | ✅ **entschieden — zugunsten der AIR-Tabelle, und die Atari-Fassung ist berichtigt.** Uebereinstimmung Hatari ↔ AIR: **alle fuenf Sektor-Flags** und **drei von vier Spur-Flags** identisch; `STX_TF_PROT` (0x20) modelliert Hatari nicht — kein Widerspruch, nur keine Aussage. Die Atari-Fassung war bei **sieben von neun** Konstanten falsch und ist nachgezogen, samt der Verwendungen: `id_crc_error` kam dort aus Bit `0x02`, das in **keiner** der beiden Quellen vorkommt — richtig ist `CRC (0x08) UND RNF (0x10)`, woertlich in AIR („data if RNF=0, ID if RNF=1") und bei Hatari als zwei getrennte Bits. Ebenso: ein Spur-Flag fuer „Fuzzy vorhanden" gibt es nicht, massgeblich ist `fuzzy_size`. **Zur Unabhaengigkeit, ausdruecklich:** Hataris Kopf nennt vier Rueckentwickler — Markus Fritze (Sarnau), P. Putnik, **Jean Louis Guerin**, Nicolas Pomarede. Louis-Guerin ist also UNTER den Quellen; die Hand ist **eingeschraenkt** unabhaengig. Was sie hinzufuegt, sind drei weitere Namen und **Verhaltensbeleg** — der Emulator faehrt damit reale geschuetzte Spiele, was eine Beschreibung nicht leistet. Die Atari-Fassung hatte gar keine genannte Quelle. **Nebenbefund:** Hatari `stx.c:1080-1083` bestaetigt unabhaengig den Kern von MF-847 (`pTrackData = pFuzzyData + FuzzySize`, also `sec_desc_off + sec_count*16 + fuzzy_count`) |
| P3-93 | **Der vollstaendige STX-Port hatte keinen Aufrufer — der 179-Zeilen-Leser war der ausgelieferte. Verdrahtet MF-854, nach Eigentuemer-Entscheidung.** `uft_stx_air.c` (~950 Z.) liest Fuzzy-Masken, Timing-Saetze, Spurbilder mit Sync-Offset, Standardspuren und rechnet CRC nach; ausserhalb der eigenen Datei nannten ihn nur ein Test und ein Kommentar | ueber `git ls-files` je Bezeichner gemessen (MF-847), Verdrahtung gegen die bestehenden Rotbeweise abgenommen (MF-854) | ✅ **verdrahtet.** Der Eigentuemer hat beide Folgen ausdruecklich angenommen: **(a)** eine **GPL-3.0-only**-Uebersetzungseinheit liegt jetzt im REGISTRIERTEN Pfad — MF-698 hatte die Bindung fuer das verteilbare Gesamtwerk bereits angenommen, mit MF-854 waechst die Reichweite; **(b)** `STX_MAX_SECTORS 32` ist **scharf** und wird deshalb GEMELDET statt verschwiegen (siehe P3-58). Gebaut wurde eine **schmale Tuer** (`include/uft/formats/stx/uft_stx_air.h`: undurchsichtiges Handle + Sektorsicht), nicht die offengelegte Struktur — ein Aufrufer, der ein Feldlayout von Hand nachdeklariert, ist der Fehler aus MF-796. Dieselbe Bauform wie MF-837 (DMS). Beide bestehenden Rotbeweise (`test_stx_registrierter_leser`, `test_stx_error_marks`) bleiben gruen — sie pruefen jetzt den Port. **Die drei uebrigen Leser** (`uft_stx_parser_v2.c`, `atari/uft_stx_parser.c`) haben weiterhin keinen Aufrufer; ihre Loeschung ist eine eigene Entscheidung |
| P3-94 | **Zwei gemeldete AIR-Befunde tragen nicht mehr — nachgemessen statt uebernommen.** Die Zulieferung des 26. Durchgangs fuehrte `IPF_MAX_BLOCKS 16` als „klemmt still" (A2) und `STX_MAX_SECTORS 32` in `uft_stx_air.c` als hoechste Schwere (A1). **A2 ist seit MF-830 erledigt**: `uft_ipf_air.c:586` haelt den Verlust schon an der ANKUENDIGUNG fest (`blocks_truncated = img.block_count > IPF_MAX_BLOCKS`) und `:905` gibt ihn ueber `ipf_air_get_track_loss()` heraus — das Feld hat also einen Leser, es ist keine tote Zusage. **A1 zeigt auf eine Datei ohne Produktionsaufrufer** (P3-93); die erreichbare Klemme war `26` in `uft_stx_plugin.c` (P3-91) | beide im Baum nachgemessen, `blocks_truncated` und `uft_stx_air` je ueber `git ls-files` auf Aufrufer geprueft (MF-847) | ✅ **festgehalten, weil die Richtung zaehlt.** Der Plan sagte „entschaerfe die Klemme im Port", die Messung sagte „der Port wird nicht aufgerufen, und der aufgerufene Leser ist schlechter" — Messung vor Plan, wie beim Fluss-Widget (MF-630/632). Der Rest der Zulieferung bleibt gueltig und offen: der 16-Byte-Deskriptor in `uft_stx_parser_v2.c` (A3) und die drei KryoFlux-Auslassungen (A4/A5/A6) sitzen samt und sonders in Dateien ohne Aufrufer und haengen damit ebenfalls an P3-93 |
| P3-95 | **Der Firmware-Automat und die Byteebenen-Naht lagen seit MF-686 im selben Baum, ohne einander zu kennen. Verbunden MF-848.** `tests/emulators/greaseweazle/firmware_state_machine.c` modelliert die GW-Firmware vollstaendig — Zustaende, ACK-Bytes, TRK0, Schreibschutz, Motor — und kann Rahmen **bauen** (`gw_fw_build_packet()`), aber keine **beantworten**: der Hinweg war da, der Rueckweg nicht. `uft_gw_open_stream()` ist die Gegenseite und wurde von genau **einem** Test benutzt (`test_gw_nak_resync.c`), der eine **von Hand geschriebene** Bytefolge einspeist | ueber `git ls-files` je Bezeichner gemessen: ein Nutzer der Naht, null Verbindungen zum Automaten (MF-848) | ✅ **verbunden.** `gw_wire_bridge.c` ist die fehlende Haelfte: Rahmen rein, Automat befragen, `[cmd, ack]` + Nutzlast raus. **Der Gewinn ist nicht Bequemlichkeit, sondern eine zweite Hand.** Eine Bytefolge, die im selben Test steht wie die Erwartung, teilt jedes Missverstaendnis ihres Autors ueber das Protokoll — das ist die fuenfte Frage dieses Baums (MF-644/760): *ist dieses Orakel dieselbe Hand wie das Gepruefte?* Der Automat ist eine andere: eigener Durchgang, eigenes Abweichungsregister. **Erstes Ergebnis:** die beidseitige TRK0-Pruefung aus MF-799 — die Erkennung des dekalibrierten Kopfes, also des Laufwerks, das glaubt auf Spur 40 zu stehen und auf 0 steht — war bisher nur an echter Hardware pruefbar, und dieses Projekt hat keine (MF-310). Mutationsprobe gemessen: nimmt man die zweite Richtung heraus, meldet der Pruefstand `seek(40) bei anliegendem /TRK0 gab 0, erwartet -7` |
| P3-96 | **Greaseweazle nahm jede Firmware an, auch eine zu alte. Behoben MF-849 — mit Freigabe fuer den geschuetzten Pfad.** `usb.py` fuehrt `EARLIEST_SUPPORTED_FIRMWARE = (0, 31)`; UFT las `fw_major`/`fw_minor` in `uft_gw_get_info()` und **protokollierte sie nur** — gemessen: die Zeichenketten `MIN_FW` und `EARLIEST` kamen in `uft_greaseweazle_full.{c,h}` **null**mal vor. Eine zu alte Firmware fiel erst beim ersten nicht unterstuetzten Befehl auf, als `UFT_GW_ERR_UNSUPPORTED` ohne Bezug zur Ursache | vom Eigentuemer aus `keirf/greaseweazle` gemeldet, im Baum durch Zaehlung nachgemessen; vier Mutationsproben gegen den Pruefstand gefahren (MF-849) | ✅ **behoben, und an einer Stelle besser als die Vorlage.** Die Pruefung sitzt NICHT als Zweig in `uft_gw_open()`, sondern als eigene Zusage `uft_gw_firmware_supported(const uft_gw_info_t*)`. Grund: eine Regel, die nur im Transportpfad steht, braucht ein Geraet, um befragt zu werden — und dieses Projekt hat keins (MF-310). Getrennt laesst sie sich mit einem `uft_gw_info_t` auf dem Stapel pruefen, und ueber die Bruecke aus MF-848 auch mit einer Antwort, die der Firmware-Automat gegeben hat. **Zweite bewusste Abweichung:** `usb.py` weist nicht ab, sondern setzt `update_needed` und laesst das Geraet fuer `GetInfo` offen. Das setzt einen Begriff voraus, den UFT nicht hat — ein eingeschraenkt nutzbares Geraet; in einem forensischen Werkzeug ist ein halb brauchbares Geraet schlechter als eine benannte Absage, die Ist- und Sollversion nennt. **Vier Mutationsproben, drei gefangen** (immer wahr · `>` statt `>=` · NULL gilt als in Ordnung). Die vierte (`major*256+minor >= MIN_MINOR`) besteht — richtigerweise: sie ist **gleichwertig**, solange `UFT_GW_MIN_FW_MAJOR == 0` ist (Schwellwert 0*256+31 = 31). Sie wuerde erst falsch, wenn jemand die Hauptzahl anhebt (`0.200` kaeme durch); kein Testfall kann das heute zeigen, weil es unterhalb Hauptzahl 0 keine Version gibt. Als benannte Grenze im Pruefstand festgehalten statt als Testfall, der nichts beweisen kann |
| P3-97 | **Greaseweazle: der `dev`-Schutz galt nur im Naht-Zweig, nicht im Rueckfall. Behoben MF-849.** `serial_write_all()`, `serial_read_exact()` und `serial_read_available()` pruefen `dev && dev->stream_ops && ...` und reichten bei NULL trotzdem `serial_*_platform(dev, ...)` weiter, wo `dev->handle` bzw. `dev->fd` ungeprueft dereferenziert wird. Der `dev &&`-Test suggerierte einen Schutz, den der Rueckfallzweig nicht hatte | vom Eigentuemer (cppcheck) gemeldet, an allen drei Funktionen im Baum nachgelesen (MF-849) | ✅ **behoben — und ehrlich zur Schwere.** Heute nicht erreichbar: jeder Aufrufer prueft vorher, `uft_gw_command()` beginnt damit. Das ist **Haertung, kein Bugfix mit Symptom**, und es steht so in der Commit-Nachricht. **Bewusst ohne eigenen Test:** die drei Funktionen sind `static`, und kein oeffentlicher Weg fuehrt mit `dev == NULL` dorthin — ein Pruefstand, der die Stelle nicht erreichen kann, waere ein Schein-Test, und davon hat dieser Baum genug gesehen (MF-596, MF-830). Der Grund steht als Kommentar an jeder der drei Stellen |
| P3-98 | **Zwei native Treiberplaene (KryoFlux, FluxEngine). Eigentuemer-Entscheidung: FluxEngine JA. Gutachten liegt vor (MF-856).** | `uft-scout` gegen `davidgiven/fluxengine` HEAD `909fac72`, jede Angabe mit Datei:Zeile belegt; Rahmengroessen an einem uebersetzten Testprogramm GEMESSEN (MF-856) | ⚠ **offen, aber die Grundlagen stehen — und drei Annahmen sind umgestossen.** **(1) Die Lizenz war falsch angenommen.** „Public-Domain-artig" ist widerlegt: `COPYING.md` sagt woertlich „GPL 2.0-licensed … **not** GPL 2.0-or-later". Die Lizenzmatrix fuehrt GPL-2.0-only als GRUEN; das Urteil bleibt beim Eigentuemer (MF-679). **(2) Meine abgeschriebene Flux-Kodierung war um eins daneben.** Die Fortsetzungsschwelle ist **63, nicht 62** — Quelle ist der Hardware-Sampler (`Sampler.v:70-76`: Byte = `{pulse, index, counter[5:0]}`, emittiert bei Flanke ODER `counter == 0x3f`). Die 62 stammt aus dem Client-**Encoder** (`fluxmap.cc:42-47`); die Hardware sendet sehr wohl Event-Bytes mit 63 Ticks, `0xBF` ist gueltig. **Genau dafuer war die Quelle noetig.** **(3) Die Naht liegt woanders, als der Plan sagte.** `FluxEngineRunner` ist eine **argv**-Naht (`fluxengine_provider_v2.h:200-202`); ein USB-Runner dahinter muesste Kommandozeilen-Argumente PARSEN und erbte damit genau das Dialektrisiko aus FE-3. Tragfaehig ist eine Byteebenen-Naht eine Ebene tiefer, im HAL, nach dem Muster von `uft_gw_stream_ops_t` (MF-848). Die Reihenfolge Automat → Treiber traegt; nur der Einhaengepunkt aendert sich. **Was jetzt feststeht:** Protokollfassung **17**, seit 2022-03-26 unveraendert; VID/PID `1209:6e00`; vier Endpunkte; **kein `#pragma pack` im ganzen Baum** — natuerliche Ausrichtung, Fuellbytes gehen mit ueber die Leitung, der Automat muss Groessen MESSEN statt rechnen (`read_frame` 8, `write_frame` 12); **kein Zeitlimit** im Client, falscher Antworttyp wirft; Versionsabweichung ist harter Abbruch ohne Aushandlung. **Eine Quelle, keine zweite.** Zweifach gesucht, keine unabhaengige Umsetzung des Board-Protokolls gefunden — das gehoert als Erstzeile in das `DIVERGENCES.md` des kuenftigen Automaten. **Pikant:** die aktuelle Firmware sendet **nie** `F_FRAME_DEBUG` (`print()` geht auf UART, `main.c:127-138`); die Debug-Behandlung im Client ist Altbestand. **KryoFlux bleibt unberuehrt** — dort steht die SPS-Lizenz entgegen, Zone PRUEFEN |
| P3-99 | **SCOUT-F1 widerlegt: `dskx` kann keines der beiden Dinge, fuer die es als FAT12-Oracle vorgeschlagen war.** `tools/uft-scout/out/FloppyControl.gutachten.md` §F1 begruendete den Oracle-Vorschlag mit geloeschten Verzeichniseintraegen (`list --deleted`) und getrennt gefuehrten Bad-Clustern 0xFF7 — **aus dem Quelltext zitiert, nie gebaut** (dort als UNGEKLAERT #3 vermerkt) | vom Eigentuemer nachgeholt: `dskx` mit .NET 7 gebaut, FAT12-Abbild mit `mtools` erzeugt, eine Datei geloescht, der 0xE5-Marker roh nachgeprueft (Offset 3680: `e5 45 4c 45 54 45 44 20`), `dskx list --deleted-only` liefert LEER (MF-848) | ⚠ **offen — wartet auf das Fremdprojekt, nicht auf uns.** Ursache im Quelltext: `Fat12Extractor.cs` dokumentiert selbst „deferred as a future enhancement" — `DiscUtils.Fat` legt geloeschte Eintraege nicht offen, ein eigener Parser dafuer existiert nicht. Vorhanden sind nur das CLI-Flag, `FileEntry.IsDeleted` und die Filterlogik: **Fassade mit funktionsfaehigem Rahmen und funktionsloser Mitte**. Die zweite Haelfte ist umgekehrt gelagert — `SectorMapper.MapBadSectors()` ist ECHT implementiert, aber `DskX.Cli/Program.cs` ruft `GetSectorMap()` nirgends, also ueber die Kommandozeile nicht erreichbar. **Beides zusammen ist genau unser eigenes Muster in fremdem Code**: eine Tuer ohne Leser neben einer Zusage ohne Umsetzung. Die Registrierung als siebtes Oracle in `tests/differential/oracles.py` bleibt liegen; fuer normale FAT12-Inhalte deckt `floptool` (MF-629) dasselbe bereits ab |
| P3-100 | **Der Reparaturlauf gab genau die Sektoren frei, die derselbe Lauf als „vermutlich noch vorhanden" meldete. Behoben MF-850.** MF-835 hat `check_directory()` beigebracht, Verzeichniseintraege HINTER der Endmarke zu melden — fuer DOS unsichtbar, die Daten aber noch da. **Drei Geschwister im selben Pruefer wurden nicht mitgezogen** und brachen weiter beim ersten nie benutzten Eintrag ab: `check_sector_chains` (:338), `check_cross_links` (:501) und `check_lost_sectors` (:576). Der dritte wiegt am schwersten, weil er **schreibt**: er baut aus den Verzeichniseintraegen die Menge `used_by_files[]`, nennt jeden in der VTOC belegten Sektor ausserhalb dieser Menge „verloren" und ruft bei `fix = true` `dos2_free_sector()` + `dos2_write_vtoc()`. Die Sektoren einer versteckten Datei fehlten in der Menge — **und wurden freigegeben** | Rotbeweis mit fuenf Gegenproben, gemessen: „Sektor 401 ist nach dem Reparaturlauf FREI" (`tests/test_atari_verlorene_sektoren.c`, MF-850) | ✅ **behoben.** Das `break` ist an allen drei Stellen ersatzlos weg — die Folgezeile filtert ohnehin ueber `is_valid` (b6), das bei einer Endmarke per Definition frei ist. **Das ist keine verpasste Meldung, sondern eine stille Veraenderung am Beweismittel**, und damit die schwerste Klasse dieses Baums: „Kein Bit verloren. Keine stille Veraenderung." **Warum es erst jetzt entstand:** vor MF-835 hat der PARSER alle Eintraege ab der Marke ueberschrieben — es gab keine versteckten Eintraege, die jemand haette uebersehen koennen. Der Fehler entstand in dem Moment, als der Parser ehrlich wurde. Eine Reparatur, die eine zweite Stelle noetig macht und sie nicht mitzieht, ist im Baum belegt (MF-794 `sad`, MF-847 STX). **Nebenwirkung, gemessen:** die beiden anderen Stellen liefern jetzt zusaetzliche Befunde — eine Querverkettung, an der eine versteckte Datei beteiligt ist, wurde vorher gar nicht gemeldet |
| P3-101 | **Die Mutationsprobe fand eine Luecke in meinem EIGENEN Rotbeweis — geschlossen MF-850.** Nach dem Fix wurden die drei entfernten `break` einzeln zurueckgesetzt, um zu messen, ob der Pruefstand jedes einzelne faengt. **Er fing zwei von drei.** `check_sector_chains` (:338) hatte keinen Fall, der ihn beruehrt: die versteckte Datei im Aufbau war widerspruchsfrei, ihre Kette also unauffaellig | vier Faelle gegen drei Mutationen gefahren, 2/3 gefangen (MF-850) | ✅ **geschlossen.** Sechster Fall ergaenzt: die versteckte Datei behauptet 5 Sektoren, ihre Kette hat einen — ein Widerspruch, der auch hinter der Endmarke auffallen muss. Danach 3/3. **Der Punkt ist nicht der eine Fall, sondern das Verfahren:** ein Fix an *n* Stellen braucht *n* Mutationen, sonst deckt der Rotbeweis nur die Stelle, an die man beim Schreiben gedacht hat. Ohne die Probe waere `check_sector_chains` als „mitrepariert" durchgegangen — richtig repariert, aber unbewacht, und beim naechsten Umbau still zurueckgefallen. Dasselbe Verfahren hat in MF-849 den umgekehrten Dienst getan: dort bestand eine Mutation zu Recht, weil sie eine gleichwertige Schreibweise war |
| P3-102 | **Kein Tor gegen die naechste tote Testschranke — und der Grund gehoert aufgeschrieben.** Nach MF-851 ist die Zahl 0. Die naheliegende Frage lautet: warum kein Tor, wo dieser Baum sonst fuer jede solche Klasse eines baut? | die beiden moeglichen Bauformen gegeneinander abgewogen, keine gemessen (MF-851) | ⚠ **bewusst nicht gebaut, mit Begruendung.** Die **allgemeine** Form — „welche `#ifdef`-Bezeichner werden in keiner Bauvorschrift definiert" — traegt nicht: die grosse Mehrheit sind Plattform- und Compilermakros (`_WIN32`, `__linux__`, `__GNUC__`), die der Compiler setzt und keine `CMakeLists.txt` nennt. Sie auszunehmen hiesse, eine Liste bekannter Faelle zu pflegen — genau das Muster, das dieser Baum **siebzehnmal** als Fehlerquelle belegt hat. Die **enge** Form — nur Bezeichner, deren Name `TEST` enthaelt — ist ebenfalls eine Aufzaehlung, nur eine von Namenskonventionen statt von Fundstellen. Sie faengt `UFT_UNIT_TESTS`, aber nichts, was jemand anders nennt. **P3-89 hatte es schon richtig entschieden** („bei drei Fundstellen ist das Verschieben billiger als das Messen"), und diese Zeile haelt die Begruendung fest, damit die Frage nicht bei jedem Durchgang neu aufgeworfen wird. Was den Rueckfall tatsaechlich verhindert, ist billiger: die Bloecke sind **entfernt**, nicht nur umgangen — es gibt keine Vorlage mehr, von der jemand kopiert |
| P3-103 | **BERICHTIGT MF-853: nicht 31 Makro-Drifts, sondern 11 wirksame und 20 tote Definitionen.** Die erste Messung (MF-852) zaehlte jeden Namen mit mehr als einem Wert. Nachgemessen wurde dann, ob der Name in seiner Datei ueberhaupt BENUTZT wird — und das Bild kippte: **20 von 31** sind `#define`s, die ihre eigene Datei nie liest. Sie koennen niemanden in die Irre fuehren, weil niemand sie befragt | ueber `git ls-files` gemessen, Wirkungspruefung je Datei (MF-853) | ⚠ **offen, weiche Grundlinie 11 (war 31).** **Eine eigene Ueberzeichnung zurueckgenommen:** MF-852 meldete `CMD_BAM_TRACK`/`CMD_BAM_SECTOR` als „zwischen Header und Umsetzung vertauscht". Gemessen sind es **tote Definitionen in einem unabhaengigen Modul** — `uft_cmd_fd.c` definiert vier und benutzt keine; die Treffer kamen aus `uft_cmd.c`, das seinen eigenen Satz hat. Kein Fehler, nur Ballast. **Von den 11 wirksamen sind vier generische Namen** (`INITIAL_CAP`, `MAX_SEGMENTS`, `MAX_FORMATS`, `MAX_TRACKS`) in unverwandten Modulen — zulaessig. **Format-gebunden und damit entscheidungsbeduerftig bleiben:** `D2M_TRACKS`/`D4M_TRACKS` (81 gegen 80), `PRO_MAX_TRACKS` (80 gegen 77), `ATX_MAX_SECTORS`/`ATX_MAX_TRACKS` (26/40 gegen 32/48), `ADF_TRACKS` (80 gegen 160 — vermutlich Spuren gegen Spurseiten) und `STX_MAX_TRACKS`. **Bereits behoben (MF-852):** `ATX_SIGNATURE` byte-verdreht — die Signaturpruefung in `uft_atx_parser_v2.c` traf nie |
| P3-104 | **Das Tor aus MF-852 haette den Fund, aus dem es entstand, NICHT gefangen — gemessen, nicht vermutet.** `audit_macro_drift.py` sucht „gleicher Name, anderer Wert". P3-92 war „gleiche BEDEUTUNG, anderer Name, anderer Wert": die beiden Tabellen nannten dasselbe Bit `STX_TF_IMAGE` und `STX_TF_TRACK_IMAGE`. Namensgleichheit gab es nie | Gegenbeweis gefahren: die alte Konstante `0x01` wieder eingesetzt, das Tor blieb bei 32 (MF-852) | ✅ **als Grenze festgehalten, Tor bleibt.** Das ist die im Baum belegte Lage „eine Messung kann die richtige Frage stellen und die falsche Fehlerklasse treffen" — und diesmal war ich es selbst, im Rechtfertigungstext des eigenen Werkzeugs. **Die Berichtigung steht jetzt im Kopf des Skripts, wo sie jeder liest, der es benutzt.** Das Tor bleibt, weil die Messung 32 Faelle einer BENACHBARTEN Klasse fand (P3-103), darunter eine nie zutreffende Signaturpruefung — **der Fund rechtfertigt es, die urspruengliche Begruendung tat es nicht.** Was die Klasse von P3-92 faengt, ist syntaktisch nicht erreichbar: dafuer braucht es eine Quelle, die sagt, welche Bedeutung welchem Wert gehoert. Genau dafuer gibt es die Zwei-Quellen-Regel |
| P3-105 | **Vier STX-Leser, vier verschiedene Spurdecken — und keine davon ist eine Formatzahl.** `uft_stx_air.c` 85 · `atari/uft_stx_parser.c` 168 · `uft_stx_parser_v2.c` 168 · `uft_stx_plugin.c` 128. Das Format erlaubt ueber die sieben Bit in Spursatz-Byte 0x0E genau **128** Spuren je Seite; nur der REGISTRIERTE Leser benutzt diese Zahl, und zwar erst seit MF-847 | im Baum gemessen, Formatgrenze aus dem Feldaufbau abgeleitet (MF-853) | ⚠ **offen, aber ausdruecklich KEIN Datenverlust.** Nachgesehen: `uft_stx_air.c:291` gibt bei Ueberschreitung `STX_AIR_TRACK_ERROR` zurueck — er WEIST AB, schneidet nicht still ab. Das ist die ehrliche Fehlerart; die Grenze ist nur niedriger als das Format erlaubt. Die 85 und die 168 sind auch keine echte Meinungsverschiedenheit, sondern zwei Feldformen (2D `[85][2]` gegen flach `84*2`). **Zu tun ist hier nichts, solange P3-93 offen ist** — drei der vier Leser haben keinen Aufrufer, und die Decken in ihnen bewegen nach Regel 9 keine Kennzahl. Notiert, damit die Zahl 128 nicht spaeter als Willkuer erscheint: sie ist die einzige der vier, die aus dem Format stammt |
| P3-106 | **Die Spurdecke des STX-Ports war 85 und wurde mit der Verdrahtung scharf — jetzt 128. MF-854.** `uft_stx_air.c` fuehrte `STX_MAX_TRACKS 85` und gab bei einer hoeheren Spur `STX_AIR_TRACK_ERROR` zurueck. Solange die Datei keinen Aufrufer hatte, war das folgenlos; mit MF-854 haette eine Diskette mit Spur 85 oder darueber **die ganze Datei verloren**, nicht nur die Spur | Formatgrenze aus dem Feldaufbau abgeleitet (`trackNumber` Bit 0-6 = 0..127), im Baum gemessen (MF-854) | ✅ **auf 128 gehoben — die einzige der vier Decken, die aus dem Format stammt** (P3-105 stellte 85 · 168 · 168 · 128 gegenueber). Kosten: das Feld waechst von ~350 KB auf ~530 KB je Diskette, auf dem Haufen, einmal je geoeffneter Datei. Das ist der Preis dafuer, dass keine Zahl mehr im Weg steht, die niemand begruendet hat |
| P3-107 | **FluxEngine nativ: Rahmenschicht und Drahtautomat stehen, fuenf Kommandos. MF-857.** Nach der Eigentuemer-Entscheidung (P3-98) und dem Gutachten (MF-856) gebaut: `src/hal/uft_fluxengine.c` mit Byteebenen-Naht `uft_fe_stream_ops_t` von der ersten Zeile an, `tests/emulators/fluxengine/wire_state_machine.c` als Gegenhand, verbunden im Pruefstand | neun Faelle gegen den Automaten, alle Angaben aus `protocol.h`/`main.c` Commit 909fac72 (MF-857) | ⚠ **offen — vier von neun Kommandos fehlen, mit Grund.** Umgesetzt: `GET_VERSION`, `SEEK`, `RECALIBRATE`, `SET_DRIVE`, `MEASURE_SPEED`. Offen: `READ`, `WRITE`, `ERASE`, `MEASURE_VOLTAGES` — **jedes kommt mit seinem eigenen Prueffall, nicht als Vorrat**; der Automat ZAEHLT unbekannte Rahmentypen (`unknown_cmds`), damit die Luecke sichtbar wird statt geerbt. **Drei Stellen ausdruecklich anders als die Vorlage:** (1) **ausgeschriebene Byteversaetze** statt Struct-Kopien — der Original-Client verlaesst sich darauf, dass Host- und PSoC-Compiler identisch ausrichten, und die ARM-Seite hat nie jemand gemessen; (2) **ein Zeitlimit** (5000 ms) — der Client hat keins, bei einer READ-Antwort ueber 1 MiB droht laut Quelltext eine wechselseitige Blockade, und ein forensisches Werkzeug darf nicht haengen; (3) **Laengenpruefung** — die Firmware prueft weder Laenge noch `size`-Feld, ein verkuerzter Rahmen ginge sonst als gueltig durch. Alle drei stehen in `tests/emulators/fluxengine/WIRE_DIVERGENCES.md`, dessen ERSTE Zeile lautet: **eine Quelle, keine zweite** (FEW-0). **Zwei Fehlschlaege beim Bauen lagen an meiner VORRICHTUNG, nicht am Treiber** — die erste Bruecke erzeugte die Antwort beim Schreiben; ein Geraet sendet auf EIN Kommando aber MEHRERE Rahmen (die Debug-Rahmen sind genau dieser Fall). Umgebaut auf Antwort beim Lesen |
| P3-108 | **Der ST-Pfad kannte weder Interleave noch Spiralfaktor. Behoben MF-858.** Gemessen ueber `git ls-files`: `src/formats/st/`, `src/formats/msa/`, `src/formats/stx/` und `src/fs/` hatten **null Treffer** auf `skew|spiral|interleav`. Fuer den Atari ST ist das keine Kleinigkeit — der Spiralfaktor ist dort **TOS-versionsabhaengig** und damit ein Herkunftssignal | Quelle: Juergen Stessun, „Wie schnell sind Disketten zu laden?", ST-Computer 12/1989, abgedruckt in `LESETEST.HLP` (FCopy Pro 1.2); alle 15 Werte der Fastload-Messreihe gegen die Formel gerechnet, groesste Abweichung **0,020 kB/s** (MF-858) | ✅ **behoben, und an zwei Stellen ueber die Quelle hinaus.** **(1) Die Konstante ist keine Konstante.** Die Quelle setzt 2,5 ein und sagt dazu, woraus sie entsteht (Sektorlaenge in kB mal Umdrehungen je Sekunde); hier ist sie ausgerechnet, damit die Formel auch fuer 1024-Byte-Sektoren und 360-U/min-Laufwerke gilt. **(2) Die zweite Messreihe hat eine Struktur, die die Quelle nie ausgerechnet hat.** Sie druckt „mit" und „ohne Fastload" nebeneinander und erklaert den Sprung in Worten. Rechnet man die Zeitdifferenz aus (Zeit = Nutzdaten/Geschwindigkeit, eine Umdrehung = 200 ms), wird daraus eine Regel: **ab Spiralfaktor 2 kostet der Spurwechsel nichts (0,00 Umdrehungen), darunter eine ganze (0,99)** — fuer 9 wie fuer 10 Sektoren. **UNRESOLVED mit Zahl:** 9 SpT bei SPIR 1 kostet **0,88** statt 0,99 Umdrehungen, elf Prozent weniger, waehrend alle anderen auf 0,99 oder 0,00 liegen. Zu gross fuer Messrauschen, unerklaert — als eigener Testfall festgehalten, der **rot wird, wenn jemand das Modell so aendert, dass es passt** (dann gehoert der Fall neu geschrieben, nicht stillschweigend weggemittelt). Vier Mutationsproben, alle vier gefangen |
| P3-109 | **Der „zusaetzliche Header" ist keine Schutzmarke — aber heute nicht unterscheidbar.** Ein ID-Feld ohne Datenfeld auf einer ST-Spur ist meist eine Geschwindigkeitsoptimierung: ohne Fastload muss das System nach einem Spurwechsel die Spurnummer verifizieren; fehlt dafuer ein Header, kostet das eine volle Umdrehung. Gemessener Effekt: 9 SpT ohne Fastload steigt von 11,31 auf **22,49 kB/s** — auf das Niveau MIT Fastload. FCopy Pro fuehrt es als Schalter „Fast headers"; die Quelle sagt, „von dieser Loesung machen viele Kopier- und Formatierprogramme Gebrauch" | Quelle: `LESETEST.HLP`, Abschnitt „Mit Koepfchen gehts besser", mit gemessener Platzierungsregel (Ende des Zusatz-Headers mindestens 10 Byte vor dem Vorspann von Sektor 1, sonst wird Sektor 1 ueberlesen); Umsetzbarkeit im Baum gemessen (MF-858) | ⚠ **offen — und der Grund ist gemessen, nicht Bequemlichkeit.** Die Unterscheidung geht **nur ueber die POSITION**: unmittelbar vor Sektor 1 oder am Trackende hinter dem hoechstnummerierten Sektor = Geschwindigkeits-Header; anderswo = Befund. Dafuer braucht es Feldgrenzen, und die gibt es nicht: `uft_sector_t.id_offset` hat im ganzen Baum **eine einzige Fundstelle — die Deklaration** (P3-59, gemessen MF-831/832). **Das haengt also an P3-59, nicht an dieser Zeile.** Bis dahin darf kein Erkenner ein ID-ohne-Daten als Schutz melden — es waere eine Falschpositivquelle, die viele schnell formatierte ST-Disketten betrifft |
| P3-110 | **Der D64-Erzeuger schrieb auf 18 von 35 Spuren Sektoren DOPPELT und liess andere WEG. Behoben MF-859.** `uft_d64_writer.c` fuehrte eine Tabelle mit 21 Eintraegen und wandte sie auf jede Spur an: `sector = standard_interleave[i % 21]; if (sector >= sector_count) sector = i;`. Die Tabelle ist fuer Zone 0 (21 Sektoren, Spuren 1-17) gerechnet; eine D64 hat vier Zonen (21/19/18/17). In den anderen drei greift der Rueckfall — und er ersetzt einen zu grossen Wert durch den **Laufindex**, der spaeter selbst noch in der Tabelle steht | gemeldet vom Eigentuemer als „sequentiell statt interleavt", im Baum nachgerechnet und **schwerer befunden** (MF-859) | ✅ **behoben.** Nachgerechnet, Zone fuer Zone: **Spur 18-24** fehlend 1, 11 · doppelt 2, 4 — **Spur 25-30** fehlend 1, 11, 12 · doppelt 2, 4, 6 — **Spur 31-35** fehlend 1, 11, 12 · doppelt 4, 6, 8. Das ist nicht eine andere Reihenfolge, sondern eine **strukturell unmoegliche Spur**: ein echter 1541 faende auf Spur 18-24 die Sektoren 1 und 11 nie. Die Regel steht jetzt als eigene Zusage `uft_d64_sektor_an_position()` — eine Regel, die nur in der Schleife steht, ist ohne die Schleife nicht pruefbar (dieselbe Lehre wie `uft_gw_firmware_supported()`, MF-849). **Zwei unabhaengige Haende fuer die Versatzwerte:** die 1541-DOS-ROM-Quelle 901229-05 (`dskintsf.src` setzt `secinc` = 10; `tst4.src::nxdrbk` sichert den Wert, setzt **3** fuer die Directory-Spur, vergibt, setzt zurueck) und `lib1541img` von Felix Palmen (`cbmdosfs.c:21-22`: `.dirInterleave = 3`, `.fileInterleave = 10`), im Scout-Verzeichnis vorgefunden. Damit ist die Zwei-Quellen-Regel erfuellt. **Zone 0 bleibt Byte fuer Byte, wie sie war** — sie war korrekt, und ein eigener Fall haelt das fest. Drei Mutationsproben, alle gefangen. **BERICHTIGT MF-861: die Reparatur stand auf der falschen Ebene.** Der behobene Fehler war echt — doppelte und fehlende Sektoren —, aber er sass in einer Funktion **ohne Aufrufer**, und der Rotbeweis schrieb die **Vergabe**folge als physisches Soll der Spur fest. Ein Rotbeweis, der das Falsche beweist, ist schlimmer als keiner: er macht den Irrtum haltbar. Seit MF-861 belegt `uft_d64_sektor_an_position()` die Spur **aufsteigend**, und die Vergaberegel steht unter ihrem eigenen Namen (`uft_d64_vergabe_an_position()`) mit der **DOS**-Regel statt der modularen |
| P3-111 | **Legt ein 1541 die Sektoren physisch interleavt auf die Spur? NEIN — aufsteigend. Geklaert MF-861.** MF-859 hat `d64_write_track_gcr()` interleavt und die Frage offen gelassen. Der Verdacht war richtig: die Zitatstelle beschreibt **Blockvergabe** (`fndnxt` — „returns next AVAILABLE track & sector"; `nxdrbk` — „allocate … and mark as used in bam"), nicht Spurbelegung | `uft-scout`: fuenf unabhaengige Umsetzungen gelesen und eine reale G64 bitgenau gemessen (MF-861) | ✅ **entschieden, und zwar breit belegt.** ROM 901229-01, Formatierroutine **$FC36-$FD1C**: Sektorzaehler startet bei 0, Header sequenziell gebaut, Schreibschleife rueckt den Zeiger linear weiter — **kein Interleave-Code im ganzen Pfad**. Dazu OpenCBM `cbmformat.a65` (laeuft auf echter Hardware), VICE `fsimage-dxx.c:262`, nibtools `fileio.c:760`, 1541ultimate `disk_image.cc:251` — und eine bitgenaue Messung an `cbm_fixtures/fdit_uft35.g64`: **35 von 35 Spuren aufsteigend**. Fuenf Haende plus Messung statt der geforderten zwei. **Und der eigene Baum sagte es die ganze Zeit:** der ANGEBOTENE Wandlungspfad D64→G64 laeuft ueber `uft_d64_g64.c::build_gcr_track()` und schreibt dort seit jeher `for (int s = 0; s < num_sectors; s++)`. `d64_write_track_gcr()` war die einzige Stelle mit einer anderen Meinung — **und hat ausserhalb ihres Moduls keinen Aufrufer**. **Zweite Frage gleich mit beantwortet:** die DOS-Vergaberegel ist NICHT modular. ROM FNDNXT **$F189-$F193** zieht beim Ueberlauf die Sektorzahl ab und danach noch eins, sofern das Ergebnis nicht 0 ist — byte-fuer-byte `lib1541img`s Vorgabe (`cbmdosfs.c:126-134`). Fuer 21 Sektoren `0,10,20,8,18,6,…`; UFTs modulare Fassung entsprach dort dem Schalter `CFF_SIMPLEINTERLEAVE`. SECINC belegt: **$EBCD** setzt 10, **NXDRBK $D497** setzt 3 |
| P3-112 | **MF-845 nahm die ERSTE ueberlebende Lesung — der Referenzalgorithmus nimmt die HAEUFIGSTE. Verfeinert MF-860.** MF-845 hat das erfundene Mischbyte beseitigt und dafuer „eine ganze beobachtete Lesung" uebernommen. Richtige Richtung, zu grob: bei drei Lesungen, von denen ZWEI uebereinstimmen und eine abweicht, kann die erste die Einzelgaengerin sein — dann setzt sich eine Minderheit durch | Referenz im Baum vendoriert: `src/a8rawconv/disk.cpp:267-320` (`sift_sectors`, Avery Lee, GPL-2.0-or-later), Zeile fuer Zeile gelesen; Rotbeweis gemessen (MF-860) | ✅ **verfeinert.** `multiread_popular_pass()` gruppiert die ueberlebenden Lesungen nach INHALT und nimmt die groesste Gruppe — genau wie a8rawconv („find the most popular; count is small, so we'll just do this linearly"). Die Zusage aus MF-845 bleibt: die Ausgabe ist IMMER eine wirklich gelesene Bytefolge, nie eine zusammengesetzte. Kein Hash noetig, `pass_count` ist ein `uint8_t`. **Rotbeweis gemessen:** unter der MF-845-Regel liefert der neue Fall `01 FF 02 FF …` — Pass A, die Einzelgaengerin, obwohl B zweimal vorkam. Drei Mutationsproben, alle rot |
| P3-113 | **`distinct_contents` zaehlte nicht, was sein Name sagt. Behoben MF-860.** `classify_passes()` bestimmte den Wert als „Anzahl der Lesungen, die vom BEZUG abweichen, plus eins". Das ist nicht die Zahl verschiedener Inhalte: bei den Lesungen A, B, B ergab es **3** statt 2, weil B zweimal gegen A verglichen wurde | beim Lesen von `classify_passes()` fuer MF-860 aufgefallen und nachgerechnet (MF-860) | ✅ **behoben.** Die Zahl kommt jetzt aus derselben Gruppierung, die auch die Ausgabe waehlt — eine Groesse, eine Rechnung. **Warum das mehr ist als Kosmetik:** `distinct_contents` ist die Zahl, an der ein Verbraucher ablesen kann, ob die Wahl MEHRHEITLICH oder WILLKUERLICH war (`distinct_contents == good_reads` heisst: jede Lesung ist einzigartig, es gab keine Mehrheit). Ueberzaehlt sie, sieht eine Mehrheitsentscheidung wie ein Gleichstand aus. a8rawconv unterscheidet die beiden Faelle nur im **Text auf stdout** („Keeping the most popular one" gegen „Keeping one of them"); im Ergebnis steht dort nichts. Hier steht es im Datensatz |
| P3-114 | **Zwei Punkte aus der a8rawconv-Zulieferung waren bereits erledigt — nachgemessen statt uebernommen.** **(1) `mWeakOffset`:** der Vorschlag, nach a8rawconvs Vorbild das gemeinsame Praefix zu bestimmen und alles danach als schwach zu markieren, ist in `classify_passes()` **seit jeher da** — die Variable `common` berechnet genau das und landet in `out_offset`. **UFT ist an dieser Stelle sogar feiner:** a8rawconv markiert ab dem ersten Unterschied ALLES als schwach (`best_sector->mWeakOffset = max_match`), waehrend UFTs `weak_mask` **byteweise** markiert, welche Stellen wirklich abweichen. Die groebere Regel zu uebernehmen waere ein Rueckschritt. **(2) `compute_interleave`:** MF-479 hat den Algorithmus bereits wortgleich portiert und angeschlossen (`src/core/uft_interleave.c`), samt der 8-%-Spurversatz-Konstante und der krummen `(n*15+17)/18`-Rundung | beide Stellen im Baum gelesen (MF-860) | ✅ **festgehalten, damit sie nicht ein drittes Mal vorgeschlagen werden.** Nebenbefund des Eigentuemers, nicht UFT betreffend: a8rawconvs README nennt im Fliesstext „9:1 interleave" fuer beide Dichten, waehrend Code und Kommentar 15:1 fuer 256-Byte-Sektoren rechnen. UFT hat den **Code** portiert, nicht die Prosa |
| P3-115 | **MF-859 hat einen echten Fehler in totem Code behoben — und dabei die falsche Ebene festgeschrieben. Berichtigt MF-861.** Die Kette im Einzelnen: (1) der gemeldete Interleave-Fehler war real und schwerer als gemeldet (doppelte UND fehlende Sektoren auf 18 von 35 Spuren); (2) er sass in `d64_write_track_gcr()`, das **keinen Aufrufer** hat; (3) der angebotene Wandlungspfad war die ganze Zeit korrekt; (4) die Reparatur schrieb die Vergabefolge als physisches Soll fest | vom Scout gemessen, im Baum gegengeprueft (MF-861) | ✅ **berichtigt — und die Lehre gehoert festgehalten.** MF-859 hat P3-111 als offene Frage NOTIERT und trotzdem gebaut. Das ist die Stelle, an der es schiefging: eine offene Frage zu benennen ersetzt nicht, sie zu klaeren, bevor man ein Soll festschreibt. **Die Frage war klaerbar** — fuenf Quellen und eine Messung lagen bereit, drei davon bereits im Scout-Verzeichnis. Der Aufwand fuer die Klaerung war kleiner als der fuer die Reparatur. Reihenfolge kuenftig: erst die Ebene klaeren, dann den Rotbeweis schreiben |
| P3-116 | **`d64_write_track_gcr()` hatte keinen Aufrufer — er war nicht unfertig, sondern der REICHERE. Verdrahtet MF-862.** Der Baum trug zwei Erzeuger fuer GCR-Spuren aus D64-Sektoren: `build_gcr_track()` (angeschlossen, der angebotene Pfad D64→G64) und `d64_write_track_gcr()` (kein Aufrufer). Der unbenutzte kann MEHR: Gap- und Sync-Laengen je Spur einstellbar, eigene Sektorordnung, Ergebnisdatensatz je Spur — genau, was Kopierschutz braucht (lange Spuren, eigene Syncs) | vor der Umstellung gemessen: beide liefern ueber die volle Laenge des reicheren **byte-identische** Ausgabe (`tests/test_d64_gcr_zwei_erzeuger.c`, MF-862) | ✅ **verdrahtet — und der Beweis kam ZUERST.** Den angebotenen Pfad umzustellen heisst, eine **gemessen verlustfreie** Wandlung anzufassen (MF-532/533). Das darf nur, wer vorher zeigt, dass dieselben Bytes herauskommen; ohne diese Messung waere die Umstellung nicht zu rechtfertigen gewesen. Beide legen dieselbe Struktur an — 5×`0xFF` Sync, Kopf, 9×`0x55` Gap, 5×`0xFF` Sync, Daten, 9×`0x55` Gap —, nur fuellt der angeschlossene danach auf Zonenkapazitaet auf. Diese Auffuellung bleibt in `build_gcr_track()`. **Der unvollstaendige Fall behaelt die alte Schleife**, und zwar begruendet: der reichere Erzeuger verlangt einen zusammenhaengenden Puffer und die volle Sektorzahl — fehlende Sektoren muesste man dort ERFINDEN. Die alte Schleife ueberspringt sie. Damit ist der Erzeuger erreichbar, ohne dass irgendwo Daten entstehen |
| P3-117 | **Der ZWEITE MFI-Leser verglich drei Byte statt sechzehn — er wies jede echte Datei ab und nahm jede an, die zufaellig mit „MFI" beginnt. Behoben MF-863.** `src/formats/flux/mfi.c:29-33` pruefte `memcmp(sig, "MFI", 3)`. Das ist weder die Kennung noch ihr Anfang: echt sind `"MAMEFLOPPYIMAGE\0"` und `"MESSFLOPPYIMAGE\0"`, je 16 Byte. Der Fehler ging in BEIDE Richtungen, und die zweite war die schlimmere — eine angenommene Fremddatei bekam `dev->flux_supported = true`, also die Zusage, aus ihr Flusswerte lesen zu koennen. Der `fread`-Fehlerzweig darunter war ein leerer Block | Rotbeweis `tests/test_mfi_kennung.c`: gegen den alten Leser **4 von 6 Faellen rot**, gegen den neuen 6/6 gruen (MF-863) | ✅ **behoben, und die URSACHE mit.** Die richtige Kennung lag seit MF-614 als Code im Baum — im ANDEREN Leser (`src/formats/mfi/uft_mfi.c:69-70`), zwei Verzeichnisse weiter, mit derselben benannten Quelle (MAME `src/lib/formats/mfi_dsk.cpp:81-82`). Zwei Fassungen derselben Sache driften; das ist im Baum als P3-103 11-mal gemessen. Seit MF-863 steht die Kennung **einmal** in `include/uft/formats/mfi.h`, und beide Leser holen sie dort. **Ehrlich zur Schwere:** `uft_flx_mfi_open()` hat keinen Aufrufer — der Fehler war nicht erreichbar. Er bleibt einer: eine Tuer, die das Falsche tut, faellt beim Verdrahten niemandem auf. **Ehrlich zur Quelle:** die 16 Byte kommen aus der benannten Referenz und dem zweiten Leser, **nicht** aus einer Messung an einer echten `.mfi`-Datei — im Korpus liegt keine |
| P3-118 | **44 % der Bauliste war Wiederholung: 1499 von 3389 Zeilen. Behoben MF-863.** `UnifiedFloppyTool.pro` fuehrte **2081** Quellzeilen fuer **582** verschiedene Dateien. 27 Pfade standen mehrfach da; 16 davon **94-mal**, weil derselbe 16-Zeilen-Block (`src/analysis/events/`, `src/analysis/denoise/`) jedem `SOURCES +=`-Block vorangestellt worden war | vor UND nach der Bereinigung mit qmake gemessen: **624 Objekte, Menge identisch** (MF-863) | ✅ **behoben, ohne Verhaltensaenderung.** Gebaut wurde vorher dasselbe — der Schaden war ein anderer: wenn eine Zeile 94-mal dasteht, kann niemand mehr durch Ansehen entscheiden, ob eine Datei absichtlich in einem bestimmten Block steht. Genau das war die Frage, als der zweite MFI-Leser auffiel (P3-117). **Vor dem Loeschen geprueft, und das war die eigentliche Arbeit:** eine Dublette ist nur dann ein Befund, wenn beide Vorkommen UNBEDINGT gelten — steht eine Datei einmal in `win32 { }` und einmal in `unix { }`, ist das richtig. Gemessen: alle 27 standen an jeder Fundstelle unbedingt. Wer nach Pfadgleichheit allein entdoppelt, ohne das zu pruefen, nimmt eine Datei aus genau dem Zweig, der sie braucht | Tor 53 `scripts/audit_pro_duplicates.py`, Grundlinie **0**, Selbsttest 5/5 (zwei seiner fuenf Faelle pruefen ausdruecklich, dass zwei Plattformzweige KEIN Befund sind) |
| P3-119 | **Soll `src/formats/flux/mfi.c` entfallen? Und was ist `HSC`?** Zwei Rueckfragen aus dem archive.org-Bericht, die MF-863 ausdruecklich NICHT selbst entschieden hat | zu (a) ueber `git ls-files` gemessen: der kleine Leser hat keinen Aufrufer; zu (b) im ganzen Baum gesucht, kein Treffer (MF-863) | ⚠ **offen, Eigentuemer-Entscheidung.** (a) Der Baum hat zwei MFI-Leser. Der grosse (`src/formats/mfi/uft_mfi.c`, mit Plugin-Registrierung, Spurtabelle, Kompression) ist der erreichbare; der kleine kann Kennung und `fclose` und hat keinen Aufrufer. MF-863 hat ihn REPARIERT statt entfernt, weil Loeschen eine Entscheidung ist und Reparieren keine — dieselbe Lage wie bei den vier STX-Lesern (P3-93) und `d64_write_track_gcr` (P3-116). Inzwischen ist das der **dritte** Fall in Folge; die Frage „zwei Umsetzungen derselben Sache, eine angeschlossen" verdient eine Regel statt drei Einzelentscheidungen. (b) `HSC` ist im Baum nicht auffindbar und im Bericht nicht aufgeloest — ohne Auskunft bleibt es Fundus. Nach Regel 9 bewegt (a) keine Kennzahl, (b) moeglicherweise die T3-Zahl |
| P3-120 | **Der FM-Weg war erreichbar und legte keinen Sektor an — `/* FM sector decoding would go here */`. Behoben MF-864.** `flux_decode_fm()` wird vom Verteiler `flux_decode_track()` gerufen, suchte Syncs und zaehlte sie; gemessen ueber den ganzen Rumpf schrieb **keine** Zeile `track->sectors[...]` oder erhoehte `sector_count` — im MFM-Zweig daneben gibt es beide. Jede FM-Diskette ueber den Flusspfad (Atari 810/1050, TRS-80 SD, IBM 3740) lieferte null Sektoren, waehrend `CLAUDE.md` „Flux→Sektor" als Faehigkeit fuehrt | Rotbeweis `tests/test_flux_fm_sektoren.c`: **0 von 7** gegen den Vorzustand, 9/9 danach; **7 von 7 Mutationsproben gefangen** (MF-864) | ✅ **behoben, mit einer fremden Hand am Fixture.** Der Baum hat KEINEN FM- und keinen MFM-Encoder — an vier Stellen als Blocker vermerkt. Eine Spur, die ich baue, gelesen von einem Dekoder, den ich baue, waere EINE Hand zweimal (die fuenfte Frage, MF-644/760). Deshalb ist die Pruefspur an **vier** Stellen von `fluxtoimd` (Eric Smith 2016, GPL-3-only) abgenommen: Adressmarken gegen seine vorberechneten Klassenwerte, ID- und Datenfeld zurueck durch sein `FM.decode()`, beide Pruefsummen mit seiner CRC-Klasse. Uebernommen ist **nichts** — es wurde ausgefuehrt (Kanal „Oracle"). Layout aus **ECMA 54 / ISO 5654 / ANSI X3.73**. Erzeugt hat die CRCs die fremde Klasse, **nachgerechnet UFTs eigene** `flux_crc16_ccitt()` — darin liegt der Kreuzcheck. **Der Vorbehalt ist eingeloest (MF-869).** Hier stand: „das Verhalten an einer echten Aufnahme ist nicht belegt; im Korpus liegt kein FM-Flussabzug". Seit MF-866 liegt einer darin und seit MF-868 laesst er sich oeffnen. Gemessen an der Applesauce-Aufnahme von 1979 (KOR-a, Zylinder 0): **32 Sektoren, ID-CRC falsch 0, Daten-CRC falsch 0** — und **26 von 26 byteidentisch** zur IMD desselben Objekts, die Applesauces EIGENER Dekoder erzeugt hat. Die Zellenzeit kam dabei aus dem Intervall-Histogramm (~2125 ns, zwei Berge bei rund 2000 und 4000 ns), nicht aus einer Annahme |
| P3-121 | **Drei von fuenf Befunden des Flusspfad-Berichts (28. Durchgang) tragen so nicht — nachgemessen MF-864.** (F1) „PLL ohne Periodenklemme": **falsch**, die Klemme steht in `uft_flux_decoder.c:417-421` (±20 % um `bitcell_ns`), dazu eine Phasenschranke bei ±½ Zelle (:410) — der Bericht hat die STRUKTUR angesehen, die Klemme steht im REGELCODE. (F2) „`opts->pll_gain` moeglicherweise tot": **veraltet**, es erreicht `pll.freq_gain` an fuenf Stellen; genau dieser Fehler wurde in **MF-487** gefunden und behoben, zwei PLL-Profile gibt es seit **MF-808**. (F3) „FM-Dekoder verwirft Clockbits, Schwere hoch": **richtig, aber Tuer ohne Leser** — `flux_fm_decode_byte()` hatte keinen Aufrufer, und `FM_SYNC_PATTERN` ist bereits das clock-korrekte Wort | je Fundstelle im Baum gemessen (MF-864) | ✅ **berichtigt, und der Bericht hat den kleineren Fehler gefunden.** Neben F3 lag P3-120 — ein erreichbarer Pfad, der gar keine Sektoren lieferte. **Zwei Dinge sind trotzdem hinzugewonnen:** (a) F1s eigentlicher Gehalt bleibt gueltig — die Klemmtreffer werden nicht GEZAEHLT, und diese Zahl waere ein Detektor fuer DrCoolZics SBV-Klasse (→ P3-122). (b) Die im Bericht empfohlene Byte-Aufspaltung ist nachweislich **unnoetig**: ein Nutzbyte `0xFE` wird zu `0xFFFE` und unterscheidet sich damit im Kanalwort von `0xF57E`; gemessen ueber alle 1- bis 3-Byte-Folgen kann `0xF57E` in wohlgeformten Nutzdaten an **keiner** Bitposition stehen. Die Vorlage selbst verwirft die Clockbits ebenso — ihr `Modulation.decode()` traegt dazu einen ausdruecklichen Vorbehalt, dass die Clockbits nicht gegen die Kodierregeln geprueft werden |
| P3-122 | **Die PLL-Klemme greift, aber niemand zaehlt mit — und diese Zahl waere ein Messwert.** `uft_flux_decoder.c:417-421` begrenzt die Periode auf ±20 %; erreicht sie die Grenze, ist das ein BEFUND (Bitratenvariation, Write Splice, No-Flux-Area) und kein Fehler. Heute geht er verloren | Fundstelle gemessen (MF-864); Aussage zur SBV-Klasse aus dem Bericht, nicht nachgemessen | ⚠ **offen, klein und lohnend.** Rein additiv: ein `clamp_hits` im `flux_pll_t` (ans ENDE, ABI-sicher) und eine Meldung am Spurende. Kein Verhaltenswechsel. Nach Regel 9 bewegt es heute keine der vier Kennzahlen — es waere die Vorstufe zu einem Schutz-Detektor. ✅ **gebaut MF-866.** `period_nominal` und `clamp_hits` stehen am ENDE von `flux_pll_t` (ABI: ein Einschub mittendrin waere eine binaere Aenderung ohne Compiler-Warnung). Gezaehlt wird an der Klemmstelle, gemeldet an **einer** Stelle — `flux_to_bitstream()`, durch die alle fuenf Dekoder laufen; fuenf Meldestellen waeren fuenf Gelegenheiten zum Auseinanderdriften. Rotbeweis `tests/test_pll_klemme.c`: 5/5, Mutation (Zaehler entfernt) gefangen. Gemessen an einer um 30 % driftenden Spur: **535 Treffer**, bei einer exakt getakteten **0**, bei abgeschalteter Regelung **0**. **Nicht belegt** bleibt, dass die Zahl eine Schutzklasse ausweist — sie sagt „die Regelung wollte weiter, als erlaubt ist"; die Deutung braucht eine Aufnahme mit bekannter Ratenaenderung, und im Korpus liegt keine |
| P3-123 | **Drei Rueckfragen aus dem Flusspfad-Bericht, die MF-864 ausdruecklich NICHT selbst entschieden hat.** (a) **M2FM**: `ENCODING_M2FM` steht in fuenf Aufzaehlungen, `grep M2FM` ueber `src/flux/` findet **null** — Dekoder ergaenzen oder als „erkennbar, nicht dekodierbar" kennzeichnen? Der Bericht markiert das selbst als Rueckfrage. (b) **KryoFlux-StreamInfo**: die Position wird gelesen (`uft_kryoflux_stream.c:318-322`, dort `data_count` genannt) und nie gegen die mitgezaehlte abgeglichen — **aber** ein frueheres Dokument des Eigentuemers sagt „`src/flux/uft_kryoflux_stream.c` nicht anfassen", das neue verlangt die Aenderung. Ein Widerspruch zwischen zwei Anweisungen entscheidet nicht der Agent. (c) **FM-Aufnahme**: P3-120 ist gegen eine ERZEUGTE Spur belegt; fuer T1b braucht es einen echten FM-Flussabzug im Korpus | im Baum gemessen (MF-864) | ✅ **(a) und (b) entschieden am 2026-09-04, (c) offen.** (a) M2FM: **erst ehrlich machen** — eine Faehigkeitstabelle statt eines Dekoders, umgesetzt in MF-865 (→ P3-124 fuer den Dekoder selbst). (b) KryoFlux: **Abgleich ergaenzen**, die neuere und speziellere Anweisung gilt (→ P3-125). (c) FM-Aufnahme fuer den Korpus: ✅ **geschlossen MF-869**. Der Scout fand sie unter 1742 Objekten (KOR-a), der Eigentuemer gab sie frei, MF-868 machte den A2R-Leser faehig sie zu oeffnen — und die Messung ergab **26 von 26 byteidentisch** gegen Applesauces eigenen Dekoder. Als Test festgehalten (`tests/test_fm_echte_aufnahme.c`), der sich benannt ueberspringt, wenn der Korpus fehlt. Damit sind alle drei Rueckfragen dieses Eintrags erledigt |
| P3-124 | **M2FM war nicht „erkennbar, aber nicht dekodierbar" — es war beides nicht, und die GUI gab die Auswahl des Benutzers als Erkennung zurueck. Teilbehoben MF-865.** Gemessen: `M2FM` steht in **elf** Aufzaehlungen (nicht fuenf, wie gemeldet); `grep M2FM` ueber `src/flux/` findet null; die Aufzaehlung des Flusspfads `flux_encoding_t` kennt es **gar nicht**; und **keine Zuweisung im Baum** setzt `UFT_ENC_M2FM` je als ERGEBNIS — alle Fundstellen sind `case`-Zweige, Namenstabellen oder ein GUI-Eintrag. Der auslesende Fehler sass in der Oberflaeche: `setEncodingHint()` schrieb die Auswahl direkt nach `m_detectedEncoding`, und die Anzeige meldete danach „Encoding: M2FM" — die eigene Vorgabe, zurueckgegeben als Befund. Die Auto-Erkennung liefert dabei ausschliesslich MFM oder FM | ueber `git ls-files` je Bezeichner gemessen; `tests/test_encoding_caps.c` 6/6; Tor 54 mit **4 von 4** Mutationsproben (MF-865) | ⚠ **Ehrlichkeit hergestellt, Faehigkeit offen.** Neu: `uft_encoding_caps()` erklaert je Kodierung `can_detect` und `can_decode` samt Begruendung, die GUI unterscheidet **„(erkannt)" von „(vorgegeben)"** und haengt bei fehlendem Dekoder „— nicht dekodierbar" an. **Die Tabelle wird nicht gepflegt, sondern gemessen:** Tor 54 prueft `can_decode` gegen die `switch`-Zweige von `flux_decode_track()` UND dagegen, dass jede geroutete Funktion wirklich Sektoren anlegt — genau der Fall MF-864. Dabei fiel nebenbei auf: `UFT_ENC_GCR_VICTOR` kommt in **keiner** Quelldatei vor, und GCR/Amiga sind zwar dekodierbar, werden aber von keiner Erkennung gesetzt. **Offen bleibt der Dekoder** (Intel-SBC-202 und HP-7902/9885/9895, Muster in fluxtoimd `modulation.py:165-168`) — Umfang deutlich ueber der 150-Zeilen-Grenze, zwei Varianten, zwei Pruefspuren |
| P3-125 | **KryoFlux-StreamInfo: die gemeldete Position wird gelesen und nie gegen die mitgezaehlte geprueft.** `src/flux/uft_kryoflux_stream.c:316` liest `payload[0]` als `data_count`; laut Spezifikation ist das die **Stream-Position**. Sie wird fuer `avg_bps` verwendet (Z459-461) und nie mit dem selbst mitgezaehlten `stream_pos` (Z251) abgeglichen. Weicht beides ab, ist die Blockkette durcheinander — meist, weil OOB-Bytes faelschlich mitgezaehlt wurden | Fundstellen gemessen (MF-865); der Leser ist **erreichbar** (OTDR-Bruecke `uft_otdr_bridge.c:190`, DTC-HAL `uft_kryoflux_dtc.c:1424`, Provider) | ✅ **gebaut MF-867.** Der Eigentuemer hat den Widerspruch am 2026-09-04 aufgeloest: der Abgleich wird ergaenzt, als **harter** Fehler (`UFT_DIAG_FORMAT_ERROR`), **nicht korrigierend** — eine stillschweigend berichtigte Position verdeckt den eigentlichen Fehler. Vorbild ist `kryoflux-stream-checker`, wo die weiche Variante („resetting") auskommentiert danebensteht |
| P3-126 | **Der A2R-Leser meldet `A2R_OK` und liefert nichts — an einer echten Datei gemessen.** `a2r_is_valid_file()` sagt ja, `a2r_get_file_version()` liefert richtig 3, aber `a2r_get_info()` gibt `OK` mit **lauter Nullen** zurueck (Version 0, leerer Erzeuger, Disk-Typ „Unknown"), und `a2r_read_track()` liefert fuer die Spuren 0..11 keine einzige Aufnahme. Ursache gemessen: `parse_info_chunk()` verlangt `size >= 60`; die echte Datei hat einen **37-Byte**-INFO-Chunk. Dazu liest der Parser den Erzeuger ab `data[0]` — dort steht aber die **INFO-Version**, also ist jedes Feld um ein Byte verschoben | am Korpus-Objekt KOR-a gemessen (Applesauce 1.88.4, A2R3), Chunk-Aufbau byteweise ausgelesen (MF-866) | ✅ **behoben MF-868 — und es war der dritte Fall derselben Klasse.** Das echte A2R3-INFO: `[0]` INFO-Version=1, `[1:33]` Erzeuger „Applesauce v1.88.4", `[33]` Disk-Typ **6** (8 Zoll), `[34]` schreibgeschuetzt, `[35]` synchronisiert, `[36]` Hartsektoren. Wie bei MF-863 (MFI: 3 statt 16 Byte) und MF-864 (FM: Zweig ohne Rumpf) ist die Tuer da und tut das Falsche — hier sogar mit `OK` daran. **Blockiert P3-123c:** der Weg zur FM-Messung fuehrt ueber A2R, und weder `gw` noch `hxcfe` sind auf dieser Maschine verfuegbar. Kennzahl: keine der vier direkt; mittelbar P3-123c. **Der Umfang war groesser als der gemeldete Befund.** Nicht nur INFO war falsch: `parse_rwcp_chunk()` traf die Struktur an KEINER Stelle — es nahm `location` in Byte 0 an (dort steht die **Mark** `0x43`), `side` in Byte 2 (dort steht das untere Byte der uint16-Location), `data_len` in Byte 6 (dort steht das Index-Array) und eine feste Eintragsgroesse von 10 Byte, obwohl sie von der Zahl der Index-Signale abhaengt. Den **16-Byte-RWCP-Kopf** uebersprang es gar nicht. Dazu rechnete es mit der festen Konstante `A2R_TICK_NS` (125), waehrend die Referenz die Aufloesung als Feld im Kopf fuehrt — in Pikosekunden je Tick. Und `disk_type_strings[]` benannte ab Index 2 durchgehend etwas anderes als die Referenz, wobei die Werte 6..8 ganz fehlten; die Schranke `if (disk_type > 4)` stand fest und haette jede Erweiterung still ins Leere laufen lassen. **Zwei unabhaengige Haende, gleiche Zahlen:** die veroeffentlichte „A2R 3.x Disk Image Reference" (applesaucefdc.com/a2r/) und die byteweise Messung an der echten Aufnahme. **Rotbeweis** `tests/test_a2r_layout.c`: **4 von 6 rot** gegen den Vorzustand, mit exakt den Meldungen der Messung an der echten Datei („Version 0, Typ 0, Erzeuger leer", null Aufnahmen); 5/5 danach. **Abnahme an der echten 28-MB-Aufnahme:** Erzeuger „Applesauce v1.88.4", Disk-Typ 6 = „8\" DS", 19 Aufnahmen ueber sieben Locations, Timing-Aufnahmen alle bei **208 000 us +- 4 us** — eine Streuung, die ein falsch geparster Puffer nicht erzeugt. **P3-123c ist damit entsperrt** |
| P3-127 | **Der Korpus hat jetzt eine echte FM-Spur — und die MFI-Kennung ist an realen Daten belegt.** Eigentuemer-Entscheidung 2026-09-04: KOR-a, KOR-b und KOR-c beschaffen. Neun Dateien liegen in `tests/corpus/` (ungetrackt), Manifest mit SHA-256 und Herkunft im Baum | selbst nachgemessen, nicht vom Scout uebernommen (MF-866) | ✅ **beschafft, erste Messung eingeloest.** (1) **KOR-a**, IMD selbst ausgezaehlt: Spur 0 = **FM 500 kbps, 26 Sektoren a 128 Byte**, alle Typ „normal" — exakt das IBM-3740-Layout, gegen das `flux_decode_fm()` geschrieben ist; die uebrigen 76 Spuren MFM 500k 8×1024, also eine gemischte Diskette, wie das Applesauce-Log sagt. (2) **KOR-b** loest den Vorbehalt aus MF-863 ein: dort stand „Kennung nicht an einer echten `.mfi` gegengeprueft — im Korpus liegt keine". Jetzt liegt eine, und sie beginnt mit `4D 41 4D 45 46 4C 4F 50 50 59 49 4D 41 47 45 00` = `MAMEFLOPPYIMAGE\0`. Die **alte** 3-Byte-Pruefung haette diese echte Datei ABGEWIESEN, die neue liest sie — durch den echten Leser gefahren, `rc=0`. (3) **Rechtslage:** an keinem der drei Objekte steht ein Rechte-Feld — ueber die archive.org-Metadaten-API gemessen, nicht vermutet. Zone PRUEFEN bleibt; die Dateien sind ungetrackt |
| P3-128 | **Der neue Positionsabgleich fand beim ersten Lauf VIER stillschweigend ungueltige Pruefstaende — darunter den Emulator selbst.** Drei Pruefstroeme meldeten im Endblock Position 0, waehrend 1, 4 bzw. 5 Flussbytes gelaufen waren (`test_kryoflux_stream.c` zweimal, `test_kryoflux_provider_v2.cpp` einmal). Der vierte ist der schwerere: `tests/flux_gen/kryoflux/flux_gen.c` schrieb an drei Stellen die rohe **Pufferposition** statt der Zellenstrom-Position — er zaehlte OOB-Bloecke mit | beim Einbau von MF-867 aufgefallen, je Fundstelle nachgemessen | ✅ **alle vier berichtigt, und der vierte ist der Lehrsatz.** Der Emulator erzeugte damit **wortwoertlich die Fehlerklasse, die der Abgleich fangen soll** — der Bericht nennt sie als haeufigste Ursache: „OOB-Bytes wurden faelschlich in die Stream-Position eingerechnet". Eine Zeile gab es sogar zu (`/* NB: our stream_pos proxy */`), und zwanzig Zeilen darueber steht die Lehre aus einem frueheren Fall bereits ausformuliert (MF-825): **„Ein Emulator, der die Hardware falsch nachbildet, kann die Fehlerklasse nicht fangen, fuer die er gebaut wurde."** Genau das war hier wieder der Fall. Die drei Pruefstroeme waren fuer das, wofuer sie geschrieben wurden (Fluss, Index, Positionszaehlung), weiterhin brauchbar — sie waren nur als STROM ungueltig, und das faellt erst auf, wenn jemand die Gueltigkeit prueft |
| P3-129 | **Der A2R-INFO-Leser WAR der WOZ-INFO-Leser, umbenannt — und die Kopie verdeckte tote Felder in zwei anderen Modulen.** Nicht „aehnlich": dieselben neun Feldnamen, dieselbe Groessenannahme. `include/uft/formats/apple/uft_woz.h:126` sagt woertlich „WOZ INFO chunk (60 bytes)" — und genau 60 Byte verlangte `parse_info_chunk()` von einem A2R-Chunk, der 37 hat. Die Felder `cleaned`, `optimal_timing`, `disk_sides`, `boot_sector_format`, `data_format`, `optimal_bit_timing`, `compatible_hw`, `required_ram`, `largest_track` stehen so in `uft_woz.h:139-142`; die A2R-Spezifikation kennt **keines** davon. Der Leser fuellte sie auf einer echten Datei mit Werten, die er HINTER dem Chunk-Ende auflas | Feldnamen und Chunk-Groesse je Fundstelle gemessen; Schreibstellen ueber `git ls-files` gezaehlt (MF-868) | ✅ **Kopie entfernt, und der Nebenbefund ist der groessere.** Das ist das Muster, wegen dem es die EINFRIER-REGEL gibt (FMT-1/2/3: fuenf Parser gegen erfundene Specs) — hier kein ERFUNDENER Parser, sondern ein **FREMDER**. In der Wirkung dasselbe und schwerer zu sehen, weil alles uebersetzt und plausibel aussieht. **Der Nebenbefund:** `scripts/audit_dead_fields.py` vergleicht Feldnamen BAUMWEIT, nicht je Struktur. Die Schreibzugriffe des A2R-Lesers hielten damit **fuenf** Felder in `uft_woz.h` und `uft_moof.h` von der Liste fern, die dort NIE gefuellt werden — nach dem Entfernen gemessen: **null** Schreibstellen im ganzen Baum fuer `cleaned`, `compatible_hw`, `disk_sides`, `required_ram`, `largest_track`. Sie waren immer tot; sichtbar sind sie erst, seit die Kopie weg ist. Deshalb steigt die Grundlinie 1163 → **1168**: eine Erhoehung, bei der die MESSUNG ehrlicher wurde, nicht der Baum schlechter. Die Grundlinien-Datei warnt genau davor, nur in der anderen Richtung („0 gefunden" ist keine Entwarnung) | ⚠ **Folgefrage offen:** dass WOZ und MOOF diese fuenf Felder deklarieren und nie fuellen, ist jetzt sichtbar, aber nicht behoben. Fuer WOZ 2 stehen sie in der Spezifikation — der Leser fuellt sie nur nicht. Eigene Aufgabe, eigener Commit |
| P3-130 | **TD0: „TD" und „td" waren vertauscht, das Kommentarflag stand im falschen Byte — und die Datei war in sich WIDERSPRUCHSFREI falsch. Behoben MF-870.** `uft_td0_parser_v2.c:411` setzte `advanced_compression = (signature == TD0_SIG_NORMAL)` mit `TD0_SIG_NORMAL = "TD"` — invertiert. `:412` las `header.drive_type` statt `header.stepping`. Die Folge ist nicht kosmetisch: eine **„TD"-Datei lief durch den LZSS-Dekoder, eine „td"-Datei wurde roh gelesen** — beide liefern Unsinn | Rotbeweis `tests/test_td0_signatur.c`: **4 von 6 rot** gegen den Vorzustand, 6/6 danach; die Gegenprobe zeigt, dass Bit 7 in `drive_type` den Kommentarblock ausloeste (MF-870) | ✅ **behoben, und die Ursache lag eine Ebene tiefer.** Nicht die zwei Zeilen waren die Wurzel, sondern **drei** Stellen in derselben Datei, die alle dasselbe Falsche sagten: der Dateikopf (`"td" (none/RLE) and "TD" (advanced LZSS)`), beide Konstantenkommentare (`TD0_SIG_NORMAL 0x4454 // "TD" - advanced compression`) und die Auswertung. **Wer nur diese Datei liest, findet keinen Fehler** — sie ist in sich schluessig, nur eben gegen den Rest des Baums. Dazu ist der Name `TD0_SIG_OLD` fuer die Kleinbuchstaben-Kennung irrefuehrend: „td" ist nicht aelter, sondern die komprimierte Fassung; er bleibt als Alias stehen, damit keine Fundstelle stillschweigend die Bedeutung wechselt |
| P3-131 | **Vier Gegenquellen, drei davon im eigenen Baum — und keine hat den Fehler verhindert.** `include/uft/formats/uft_td0.h:31,34` (der KANONISCHE Header) fuehrt `UFT_TD0_SIG_ADVANCED 0x6474` richtig; `src/formats/td0/uft_td0_lzss.c:295` — **dieselbe Formatfamilie, dasselbe Verzeichnis** — wertet es richtig aus; `src/samdisk/td0.cpp:10-11` ebenfalls. Die vierte kam von aussen (`uft_extract_v19`, gemeldet in UFT-08) | je Fundstelle gemessen (MF-870) | ✅ **festgehalten als Muster, nicht als Einzelfall.** `uft_td0_parser_v2.c` fuehrte **eigene private Konstanten** mit denselben Werten und gegensaetzlichen Kommentaren. **Tor 52 (`audit_macro_drift.py`) faengt genau das NICHT**: es vergleicht gleiche NAMEN mit verschiedenen Werten; hier waren es verschiedene Namen (`TD0_SIG_*` gegen `UFT_TD0_SIG_*`) mit gleichem Wert. Diese Luecke steht seit MF-853 ausdruecklich im Dateikopf jenes Tores — sie ist jetzt zum zweiten Mal aufgetreten (nach P3-92). Ein Tor, dessen bekannte Luecke sich wiederholt, ist ein Kandidat fuer eine Erweiterung: gleicher WERT unter verschiedenen Namen im selben Themenraum |
| P3-132 | **Die Titel-zu-Spur-Tabelle aus der Parameterdiskette mischt Schreiber und Nicht-Schreiber.** Der zugrunde liegende Bericht (UFT-09) sagt: „Jede davon ist ein kleines 1541-Laufwerksprogramm, das die Schutzspur des jeweiligen Titels neu schreibt." Gemessen ueber `STA $1C01` (GCR schreiben) und `LDA $1C01` (lesen) trifft das auf die HAELFTE zu: `winter games`, `summer games ii`, `jet combat sim.`, `t.apshai-trilogy` und `sideways` schreiben **null** und lesen **null** — sie setzen einen Jobcode und eine Spur, beauftragen also das LAUFWERK, statt selbst GCR zu schreiben | je Datei gezaehlt (MF-870) | ⚠ **offen, als Einordnung.** Die Spurangaben bleiben belastbar; was nicht traegt, ist die Gleichsetzung von „Parameter" mit „Erzeugungscode". Zwei verschiedene Bauarten unter einem Namen — dieselbe Unterscheidung, die der Bericht selbst zwischen Beschreibung, Erkennung und Erzeugung zieht, nur eine Ebene tiefer |
| P3-133 | **„Bauform B" ist nicht ungeklaert — sie ist gruppierbar, ohne ein einziges Disassemblat.** UFT-09 fuehrt sie als offen („ohne `param- main.prg` nicht bestimmbar") und sieht zwei Dateien. Gemessen sind es **16** (Ladeadresse `$2000`, <= 64 Byte) in **7 byteidentischen Gruppen**. Die groesste umfasst **sechs Titel** mit Byte-fuer-Byte demselben Parameter: `creative calc`, `creative filer`, `creative writer`, `karateka`, `music shop`, `trolls & tribul.` Drei weitere — `maxwell manor` und zweimal `raid over moscow`, alle **Access Software** — stimmen in **44 von 56 Byte** ueberein | SHA-256 je Nutzlast, Spaltenvergleich (MF-870) | ⚠ **offen, aber mit Strukturmodell.** Die Nutzlast ist nullgepolstert; in den laengeren Faellen stehen **3-Byte-Saetze** (`maxwell manor`: `48 01 02 | 48 01 02 | 46 01 00` … `11 00 01 | 11 00 01 | 50 01 02 | 50 01 01`). Das ist kein vollstaendiger Dekode, macht aber aus „Format unbestimmbar" eine **pruefbare Hypothese**. **Und es ist der eigene Grundsatz als Messung:** sechs Titel, ein Parameter — „Artefaktsignaturen identifizieren Codefamilien, nicht Produkte". Der Grundsatz stand im Bericht; angewandt wurde er nicht, weil nur zwei Dateien angesehen wurden |
| P3-134 | **D64 hat ZWEI Leser im selben Verzeichnis, und sie nahmen verschiedene Dateigroessen an. Behoben MF-871.** `uft_d64_plugin.c:36-41` (ueber die Registry erreichbar) nimmt seit MF-350 **acht** Groessen — 35/40/41/42 Spuren, je mit und ohne Fehlerkarte — und fuehrt sie im Kommentar ausgeschrieben auf. `uft_d64_parser_v3.c::d64_is_valid_size()` (ueber `uft_v3_bridge` -> `uft_advanced_open()`) kannte **vier**: 41 und 42 Spuren fielen durch. Das ist das Muster aus MF-870 (TD0) und MF-796 (EDSK), einen Commit spaeter und in einer anderen Formatfamilie: zwei Umsetzungen im selben Verzeichnis, jede in sich schluessig, keine gegen die andere gemessen | Rotbeweis `tests/test_d64_42_spuren.c::beide_d64_tueren_nehmen_dieselben_groessen`: rot gegen den Vorzustand mit der Meldung „200960 Byte: v3=0, Plugin=1 — die Tueren sind sich uneinig", gruen danach; beide Tueren im selben Binaer gebunden (MF-871) | ✅ **behoben, und die Zahlen sind ABGELEITET.** Keine der vier neuen Groessen ist eingetippt: `uft_cbm_total_blocks()` liefert 683/768/785/802, mal 256 ergibt 174848/196608/200960/205312, plus die Blockzahl ergibt die Fehlerkarten-Fassungen. Alle acht stimmen byteweise mit denen des Plugins ueberein — dass dieselbe Formel die BESTEHENDEN, funktionierenden Werte reproduziert, ist die Begruendung fuer die neuen. **Ehrlich zur Tragweite:** die reparierte Tuer hat heute keinen Leser (`uft_advanced_open()` ohne Aufrufer, gemessen; `uft_v3_bridge.c:28` sagt es seit MF-442 selbst), also bewegt MF-871 **keine** der vier Release-Kennzahlen. Es entfernt einen Auseinanderlauf auf einem mitgebauten, exportierten Pfad |
| P3-135 | **Die Stufe `d64` = T1b stuetzte sich auf einen Test, der nur EINE der zwei Tueren anfasst.** `docs/VERIFICATION_TIERS.md` fuehrt `d64` auf T1b mit dem Beleg „VICE D64 sizes incl. error-block trailer + 40/42-track variants" und nennt dafuer u. a. `test_d64_42track` (MF-350). Dieser Test bindet `uft_d64_plugin.c` — und nur den. Der zweite Leser derselben Familie war von der Stufe nie erfasst und lag vier Groessen daneben | Bindungsliste in `tests/CMakeLists.txt:314-316` gegen die Aufruferkette gemessen (MF-871) | ⚠ **offen als Frage an die Stufen-Logik.** Der Fall ist nicht auf D64 beschraenkt: `scripts/gen_verification_tiers.py` leitet die Stufe aus den Testnamen je Format ab, nicht aus der Frage, **welche** Umsetzung ein Test bindet. Wo ein Format zwei Leser hat, kann die Stufe des einen die des anderen mit ausweisen. Der naechste Schritt waere zu messen, **wie viele** Formate ueberhaupt mehr als eine Umsetzung haben — solange diese Zahl unbekannt ist, ist unbekannt, wie viele Stufen dieselbe Luecke tragen. Das ist ein Fund ohne Kennzahl-Bezug und damit nach Regel 9 **Fundus, nicht Auftrag** — notiert, nicht eingeplant |
| P3-136 | **Die ED-Spezialformate 41-44 SpT fehlten — und die Luecke war nicht leer. Behoben MF-872.** `known_geometries[]` in `src/formats/img/uft_img.c` endete bei 36 SpT (2.88 MB). Gemessen ueber den ECHTEN Erkennungsweg — alle 137 registrierten Sonden gegen einen Puffer mit gueltigem PC-Bootsektor — beanspruchte die vier Groessen **allein `DMK`, mit Konfidenz 55**: ein TRS-80-Format fuer ein PC-Abbild, und 55 liegt im Band „Struktur gelesen" (MF-729). Ein Benutzer mit einem 3,5-MB-ED-Abbild bekam also nicht „unbekannt", sondern eine falsche, selbstbewusste Antwort | Rotbeweis `tests/test_img_ed_spezialformate.c`: **2 von 4 rot** gegen den Vorzustand, mit der Meldung „41 SpT: IMG=0(0), DMK=1(55) — IMG gewinnt nicht"; 4/4 danach, IMG meldet 90 (MF-872) | ✅ **behoben, Referenz doppelt belegt.** FLOPFIX `version.txt:40` („ED-Formate mit 41, 42, 43 und 44 Sektoren zugaenglich") und FreeDOS FORMAT 0.92 (GPL-2-only, **nur Zahlen entnommen, kein Code**) mit FD3360/FD3486 bis 42 SpT. Die Groessen sind gerechnet, nicht abgeschrieben: SpT x 2 x 80 x 512 — dieselbe Rechnung trifft die BESTEHENDE Zeile 36 SpT -> 2949120 exakt. Vor dem Eintragen gemessen (MF-784): keine der vier Zahlen kommt sonst im Baum vor, und der generische Rueckfall faengt sie nicht |
| P3-137 | **`dmk_probe()` prueft `file_size < implied` statt `!=` — und gibt darauf die Grundkonfidenz 55.** Gemessen in `src/formats/dmk/uft_dmk.c:53-56`: nach drei Bereichspruefungen (`tracks <= 96`, `tlen` in [1000,20000], `file_size >= implied`) steht die Konfidenz auf 55, und die eigentlichen Belege sind erst die Zuschlaege darueber — `+30` traegt sogar den Kommentar „exact, no trailing data". Der Autor wusste also, dass die exakte Groesse das richtige Mass ist, und hat sie zum Bonus gemacht statt zur Bedingung. Folge: **jede** hinreichend grosse Datei mit vier plausiblen Kopfbytes wird beansprucht, im Band „Struktur gelesen" | am belegten Pfad gemessen: ein 3 604 480-Byte-Puffer mit PC-Bootsektor ergibt tracks=60, tlen=19856, sides=1, implied=1 191 376 — angenommen, weil 3 604 480 groesser ist (MF-872) | ⚠ **offen.** Die Eichung `test_probe_confidence_on_random` faengt es nicht, weil sie eine RATE ueber Zufallspuffer misst und DMK die 95-%-Huerde nimmt; hier geht es um die Form der Pruefung, nicht um die Rate. Ein Fix braucht eine **benannte** DMK-Referenz zur Frage, ob eine DMK-Datei ueberhaupt Anhangsdaten haben darf — ohne die waere das Aendern von `<` auf `!=` genau der ungeprueft-Fall, den die EINFRIER-REGEL sperrt. Nach Regel 9 bewegt der Fund keine der vier Kennzahlen: **Fundus, nicht Auftrag**, bis die Referenz da ist |
| P3-138 | **Die TOS-Bootpruefsumme 0x1234 hing an EINER fremden Datei. Geschlossen MF-873.** Die Stufennotiz zu `st` sagte es selbst: „nur bei SAMdisk belegt, nicht doppelt gegengelesen". Zwei unabhaengige Quellen schliessen das: **disktype**, Dokumentation §3.3.1 „The GEMDOS File System" — „the boot sector must have a checksum of $1234 (computed from 16-bit words in **big-endian** byte order)" — und **Richard Karsmakers, „The Ultimate Virus Killer Book"**, Anhang I, Schritt 9. disktype ist die tragende, weil es als einzige die Byte-Reihenfolge ausspricht | beide Quellen abgerufen und woertlich zitiert; `docs/spec_verification.json` nachgezogen (MF-873) | ✅ **geschlossen.** Dazu `tests/test_st_bootpruefsumme.c` mit einer Gegenspur: ein Bootsektor, dessen LITTLE-Endian-Summe 0x1234 trifft und dessen Big-Endian-Summe nicht — er unterscheidet sich vom gueltigen ausschliesslich im Ausgleichswort an $1FE. **Der Ertrag ist die Quelle, nicht der Test** — siehe P3-139 |
| P3-139 | **Eine eigene Behauptung, die die Messung widerlegt hat — und die Datei sagt es jetzt in ihrem Kopf.** Die erste Fassung von `test_st_bootpruefsumme.c` begruendete sich damit, `test_st_geometry.c` koenne Big- von Little-Endian nicht unterscheiden, weil sein Erbauer dieselbe Formel rechne wie der Pruefer (die Fehlerklasse aus MF-644/760). Gemessen ist das **falsch**: `build_st()` ruft `st_boot_checksum()` nicht auf, sondern fuehrt eine ZWEITE, unabhaengige Big-Endian-Schreibweise — zwei getrennte Haende, und die Mutation bricht das Paar | zwei Mutationen am belegten Pfad: (1) Pruefer little-endian → **beide** Tests rot; (2) Pruefer nimmt BE ODER LE → `test_st_geometry` **gruen**, nur der neue Test rot (MF-873) | ✅ **festgehalten als Muster.** Nur Fall (2) rechtfertigt die neue Datei, und genau das steht in ihrem Kopf statt der urspruenglichen Begruendung. Das Muster dahinter ist allgemein: **„derselbe Algorithmus" ist nicht „dieselbe Hand".** Zwei unabhaengige Schreibweisen derselben Formel fangen eine Mutation an einer von beiden sehr wohl; was sie NICHT fangen, ist eine Aufweichung, die beide Lesarten zulaesst — dafuer braucht es einen Fall, der die FALSCHE Antwort vorlegt. Der Verdacht „Orakel = gepruefte Hand" gehoert also gemessen wie jede Zahl, nicht aus der Form des Codes geschlossen |
| P3-140 | **Der Baum hat DREI FAT-Leser, und der einzige, der die Kopien vergleicht, ist der einzige ohne Produktionsaufrufer.** Gemessen ueber `git ls-files`: `src/fs/uft_fat12.c` (`uft_fat12_detect`) vergleicht seit MF-829 — **nur Tests** rufen ihn; `src/formats/uft_fat12_legacy.c` (`uft_fat12_init`) ist von `explorertab.cpp:677,772` erreichbar und vergleicht **nicht**; `src/fileops/uft_file_ops_extended.c` (`fat12_extract_file`) ist von `explorertab.cpp:1172,1319` erreichbar und verglich **nicht**. Beide erreichbaren lasen unbedingt FAT 1. Und `explorertab.cpp:1165` schickt auch **`.st`** durch den dritten — genau der Fall, fuer den Brods MSXPATCH sagt, dass **TOS die zweite FAT fuehrt** | Aufruferkette je Symbol gemessen; Rotbeweis `tests/test_fat_kopien_kette.c` (MF-874) | ✅ **teilbehoben.** `fat12_extract_file()` verfolgt die Clusterkette jetzt in BEIDEN Kopien und meldet eine Abweichung mit Rueckgabe 1 („gelesen, zweideutig"), waehrend der Inhalt weiter FAT 1 folgt — gemeldet wird die Beobachtung, nicht ihre Deutung (P3-56, MF-829). Beide GUI-Aufrufstellen pruefen jetzt `>= 0` und zeigen eine Warnung. **Offen bleibt der zweite erreichbare Leser** `uft_fat12_legacy.c`, der ueber `uft_fat12_init/-delete` auch SCHREIBT — dort ist die Frage nicht „welche Kopie lese ich", sondern „welche schreibe ich", und die ist ohne zweite Quelle zu P3-56 nicht entscheidbar |
| P3-141 | **Pro Datei, nicht per memcmp — und das ist gemessen, nicht behauptet.** Ein Vergleich der ganzen FAT waere einfacher und faelschlich alarmierend: zwei Kopien duerfen sich in Bereichen unterscheiden, die eine bestimmte Datei gar nicht beruehrt (etwa Eintraege geloeschter Dateien), ohne dass ihr Inhalt zweideutig waere | zwei Mutationen an `fat12_extract_file()`: (a) kein Vergleich → `eine_abweichende_kette_wird_gemeldet` **rot**, Gegenprobe gruen; (b) `memcmp` ueber die ganze FAT → `abweichung_ausserhalb_der_kette_meldet_nichts` **rot**, Meldefall gruen (MF-874) | ✅ **festgehalten.** Jede der beiden Mutationen faellt bei genau EINEM Fall um — die vier Faelle sind damit nicht austauschbar, sondern decken je eine eigene Aussage. Der Einzel-FAT-Fall traegt eine eigene Referenz: Harun Scheutzow, FLOP_FIX.TXT (1992), Fehler 2 — Einzel-FAT ist auf dem Atari eine **benannte, unterstuetzte** Eigenschaft (Bit 1 der BPB-Flags an Offset $10), kein Defekt; hinter FAT 1 liegt dann das Wurzelverzeichnis, und wer dort „die zweite FAT" laese, verglaeche Verzeichniseintraege mit Clustereintraegen |
| P3-142 | **Die FAT12/16-Grenze hat ZWEI hergeleitete Werte, und der Baum meldet das Fenster nicht.** MS-DOS/FAT-Spezifikation: FAT12 bis 4084 (reserviert `$FF7..$FFF` = 9, plus `$000`/`$001` → 4096−11 = **4085**). TOS 2.06/3.06/4.0x: FAT12 bis 4078 (reserviert `$FF0..$FFF` = 16, plus zwei → 4096−18 = **4078**) — Quelle **Harun Scheutzow, FLOP_FIX.TXT (1992)**, Abschnitt „ED-Disketten", **mit ausgeschriebener Herleitung**, was die MS-Seite nicht bietet. Ein Medium mit **4079 bis 4084** Datenclustern wird von TOS als FAT16 und von MS-DOS als FAT12 gelesen — dieselben Bytes, zwei Ergebnisse. Keine akademische Luecke: eine 2,88-MB-ED-Diskette mit einem Sektor je Cluster landet je nach Geometrie genau dort, und FLOP_FIX ist deswegen geschrieben worden | beide Herleitungen nachgerechnet; sechs Entscheidungsstellen im Baum gemessen (MF-875) | ⚠ **offen, und zwar absichtlich.** Der Wert wird **nicht** geaendert — fuer ein Werkzeug, das ueberwiegend PC-Medien liest, ist 4085 richtig, und die Wahl ist Eigentuemer-Sache. Festgehalten ist jetzt, **dass es eine Wahl ist**: die Herleitung beider Werte steht bei `UFT_FAT12_MAX_CLUSTERS`, und **Tor 55** (`scripts/audit_fat_boundary.py`) haelt die sechs Stellen auf demselben Wert — taucht 4078 oder 4086 auf, ist das ein Befund. **Was offen bleibt:** das Fenster wird nirgends GEMELDET. Fuer ein forensisches Werkzeug ist „FAT12" darin unvollstaendig — die Aussage gilt nur fuer eine der beiden Plattformen |
| P3-143 | **Der Selbsttest-Pruefstand mass die Tore in einem Zustand, den sie in Wirklichkeit nie haben. Behoben MF-875.** `audit_selbsttest.py` legt seine Attrappen in Temp-Verzeichnissen an — **ohne `.git`**. Werkzeuge, die ihre Dateimenge ueber `scripts/repo_scope.py` holen (MF-636), fragen dort `git ls-files`, bekommen `fatal: not a git repository` und fallen richtigerweise auf „alles durchlassen" zurueck. Damit war der **git-gestuetzte Pfad — der einzige, den CI benutzt — vom Pruefstand nicht gedeckt**: ein Werkzeug, das nur dort bricht, waere gruen geblieben | `repo_scope.py` instrumentiert, Grund gemessen: `DIAG cwd: …\uft_grundrauschen_*` mit rc=128 (MF-875) | ✅ **behoben** — die Attrappen bekommen ein `git init`. Nebenwirkung, die den Fund ueberhaupt sichtbar machte: **jeder** Konsistenzlauf begann mit **zehn** Zeilen „`git ls-files` nicht verfuegbar — es wird der GANZE Verzeichnisbaum geprueft". Das liest sich wie MF-633 und war harmlos; **genau dadurch erzieht es dazu, die Meldung zu ueberlesen** — wovor `repo_scope.py` im eigenen Kopf warnt. Jetzt 0 Zeilen, und wenn eine kommt, bedeutet sie etwas |
| P3-144 | **Der EINZIGE erreichbare Schutzerkenner des Baums bestand aus drei Heuristiken, und eine davon war ein Muenzwurf. Behoben MF-876.** `ForensicTab::detectProtection()` haengt an einer Checkbox im Forensik-Tab — der Katalog in `src/protection/` hat keinen Aufrufer (P0-2), `g64_detect_protection()` haengt am toten `uft_advanced_open()`. Gemessen: **(a)** `data[0x1e0] == 0x36` → „RapidLok-style loader detected" feuerte auf **6 von 2000 Zufallspuffern = 0,3 %**, also 1/256 — die Pruefung traegt **keine Information**, und keine Quelle im Baum nennt diesen Offset. **(b)** `contains("V-MAX!")` traf in einer Sammlung von 146 C64-Kopierprogrammen `vmax 2 copy -dsd.prg` und `vmax 3.1 cpy-dsd.prg` — **V-MAX-KOPIERER, keine geschuetzten Disketten**; die Variante `"\x52\x52\x52\x52"` zusaetzlich `rmr nibbler copy.prg`, einen Nibbler. **(c)** Der Erkenner lief auf ALLES, was der Benutzer geladen hatte, auch auf ein `.prg` | 2000 Zufallspuffer und 146 Programme ausgezaehlt; das echte Abbild `T64COPY.D64` (35 Spuren, gewoehnlich) als Gegenprobe (MF-876) | ✅ **ersetzt durch `uft_schutz_aus_d64()`.** Jede Aussage aus `docs/format_specs/commodore/D64.TXT` (Schepers Rev. 1.11, Abschnitt „*** Error codes", dort nach Immers/Neufeld). Die 0x1e0-Heuristik ist **ersatzlos entfernt**; die V-MAX-Zeichenkette bleibt, aber als **Inhaltshinweis**, nicht als Schutzbefund. Auf `T64COPY.D64` meldet der neue Erkenner **0 Befunde und 11 uebersprungene Verfahren mit Begruendung** — vorher stand dort ein gruenes „nothing matched" |
| P3-145 | **`uft_schutzbefund` (MF-792) hatte keinen Produktionsaufrufer — jetzt schon, und C1s Kennzahl bewegt sich zum ERSTEN MAL.** Der Eintrag C1 in `docs/BACKLOG.md` fuehrt vier Zahlen; die dritte („von aussen gerufen") stand ueber **drei** Fortschreibungen hinweg unveraendert auf **4**, mit der ausdruecklichen Bemerkung „wer die Zahl bewegen will, muss verdrahten, nicht bauen". MF-876 verdrahtet: **4 → 9** | `scripts/audit_protection_claims.py` gemessen: (38,358,4,30) → (39,361,9,33) (MF-876) | ✅ **verdrahtet, und die Grenze steht dabei.** Angeschlossen ist der **Berichtsvertrag**, nicht der Katalog: die 55+ benannten Verfahren bleiben unerreichbar, und die Differenz 361−33 = 328 ungeprueft ist unveraendert. Was der Benutzer davon hat, ist der zweite Teil des Berichts — **was NICHT geprueft werden konnte**, statt einer leeren Liste, die sich als Unbedenklichkeitsbescheinigung liest |
| P3-146 | **Ein benannter Grund wurde als „unbekannt" angezeigt — seit MF-793, und kein Tor hat es gesehen.** `uft_uebersprungen_name()` in `src/protection/uft_schutzbefund.c` hatte fuer `UFT_UEBERSPRUNGEN_ZU_WENIG_LESUNGEN` **keinen `case`**; der Wert fiel in `default: return "unbekannt"`. Ausgerechnet in der Datei, deren ganzer Zweck es ist, „nicht gefunden" von „nicht geprueft" zu trennen: der zweite Teil des Berichts sagte „unbekannt" | am Quelltext gemessen; `scripts/audit_enum_tables.py` meldet dafuer **0** (MF-876) | ✅ **Zweig ergaenzt, Torluecke notiert.** Tor „Enum-Tabellen zu kurz" prueft **Tabellen** (Arrays), nicht **Verzweigungen** — ein `switch` ohne `default`-Warnung entzieht sich ihm. Dazu neu: `UFT_UEBERSPRUNGEN_KEINE_FEHLERINFO`, abgegrenzt gegen `NICHT_DEKODIERBAR`. Der Unterschied ist nicht kosmetisch — dort war eine Spur nicht LESBAR, hier ist sie vollstaendig gelesen und die Angabe steht im Format ueberhaupt nicht. Ein Benutzer, dem „nicht dekodierbar" gemeldet wird, sucht einen Defekt, den es nicht gibt (dieselbe Verwechslung wie MF-769) |
| P3-147 | **G64 hat DREI Leser, und der mit den Pruefungen ist der ohne Aufrufer — vierter Fall desselben Musters.** `src/formats/g64/uft_g64.c` ist das registrierte Plugin (Sonde/Oeffner/`read_track`) und **erreichbar**; `uft_g64_parser_v2.c` fuehrt ein eigenes statisches `g64_parse`; `uft_g64_parser_v3.c` traegt `g64_detect_protection()` und haengt an `uft_v3_bridge` → `uft_advanced_open()` — **ohne Aufrufer** (gemessen MF-871). Nach D64 (MF-871, zwei Tueren), TD0 (MF-870, Geschwisterdatei) und FAT (MF-874, drei Leser) ist das der **vierte** Fall | Aufruferkette je Symbol ueber `git ls-files` (MF-876) | ⚠ **offen, und die Frage ist inzwischen groesser als der Einzelfall.** `g64_detect_protection()` selbst ist heuristische Benennung der Sorte, die MF-508 meint: `weak_tracks > 0 && half_tracks > 0` → **„Vorpal/RapidLok", Konfidenz 0,90**. Ein generisches Signal bekommt einen Produktnamen. Ein Ersatz durch gemessene Signaturen ist vorbereitet — UFT-09 belegt die Turbo-Nibbler-Schreibsignatur byte-exakt (`A9 55 A2 07 20 81 06` … nachgeprueft an `turbonibbler 2.prg`, $14DE/$14E5/$14EC/$14F6) — **aber das sind SCHREIBER-Signaturen: sie sagen „dieses Abbild hat ein Turbo Nibbler erzeugt", nicht „diese Diskette ist geschuetzt".** Zwei verschiedene Fragen, die der heutige Code vermengt. Solange die Tuer keinen Leser hat, bewegt ein Umbau dort keine Kennzahl: nach Regel 9 **Fundus**. Der naechste Griff waere P3-135 — messen, WIE VIELE Formate mehr als einen Leser haben |
| P3-148 | **Das registrierte G64-Plugin schrieb eine INVERTIERTE Speed-Zone-Tabelle in jede erzeugte Datei. Behoben MF-877.** `src/formats/g64/uft_g64.c` fuehrte drei eigene Zonentabellen; `g64_track_speed[43]` ordnete den Spuren 1-17 die Speed-Zone **0** zu und den Spuren 31-42 die **3**. Die Spezifikation sagt das Gegenteil (`docs/format_specs/commodore/G64.TXT:285-291`: „1-17 … 3 (slowest writing speed)", „31-4x … 0 (fastest)"), und die echte VICE-Aufnahme `tests/corpus_free/vice_c1541_35trk.g64` bestaetigt es — von mir selbst ausgelesen: Spur 1 → Speed 3 / 7692 Byte, Spur 31 → Speed 0 / 6250. Die Kommentare waren mit invertiert („Tracks 1-17 (schnellste)"). Geschrieben wurde das an `:446` in jede ueber `.create` angelegte Datei, und `g64_create` haengt am **registrierten** Plugin | Rotbeweis `tests/test_g64_speedzonen.c`: unter der zurueckgedrehten Zuordnung fallen **3 von 5** Faellen um, danach 5/5 (MF-877) | ✅ **behoben durch LOESCHEN, nicht Korrigieren.** Die drei Tabellen sind weg; der Wert kommt aus der SSOT `uft_cbm_speed_zone(UFT_CBM_1541, track)`. Der zweite G64-Schreiber des Baums (`src/formats/c64/uft_d64_g64.c:58`, bedient die Wandlung D64→G64) holte ihn bereits von dort und war richtig — **fuenfter Fall des Musters „mehrere Umsetzungen, eine falsch", diesmal auf der SCHREIBSEITE**. Der Test liest die Zonen aus einer wirklich erzeugten Datei zurueck und vergleicht sie mit der Aufnahme; zwei Gegenproben sichern, dass er nicht gruen sein kann, wenn alle Zonen gleich waeren |
| P3-149 | **`{6250, 6667, 7143, 7692}` mit der Zusage „A VICE-written G64 stores exactly these lengths" — VICE schreibt 6666 und 7142. Behoben MF-878.** Es sind die aufgerundeten Nominalwerte (7142,85 → 7143), also gerechnet statt gemessen. Zwei Kopien: `src/protection/ufm_c64_metrics.c:35` und `include/uft/uft_c64_gcr.h:147` (dort ohne jeden Aufrufer). Die SSOT fuehrt seit MF-434 die gemessenen Werte (`src/formats/cbm/uft_cbm_geometry.c:64`). **Ein Test nagelte die falschen Werte fest** — `tests/test_c64_metrics_corpus.c:79-83`, unter dem Kommentar „Pinned against the lengths VICE actually wrote into the G64". Er benannte die Quelle, der er widersprach | an `tests/corpus_free/vice_c1541_35trk.g64` ausgelesen; Rotbeweis: unter den alten Werten faellt der Fall um (MF-878) | ✅ **behoben, und ehrlich zur Tragweite:** `track_length_ratio` war fuer echte Spuren der Zonen 1 und 2 um **1,4e-4** zu klein. Die Schwelle fuer „lange Spur" liegt bei 1,02 (`ufm_c64_scheme_detect.c:102`), die Anzeige rundet auf drei Nachkommastellen — **nichts hat sich falsch verhalten, falsch war die Aussage**. Der Test liest die Aufnahme jetzt wirklich, statt Zahlen einzutippen; eine erneute Drift ist nicht mehr eintippbar |
| P3-150 | **Dieselben vier Zonenlaengen liegen ELFFACH im Baum, in VIER Zaehlweisen.** Gemessen: `uft_g64_parser_v3.c:62-65` und `:68-71` (die zweite unbenutzt), `uft_g64_parser_v2.c:190-195`, `uft_cbm_geometry.c:64`, `uft_gcr_ops.c:83`, `uft_g64.c` (bis MF-877), `uft_d64_writer.c:43-48`, `uft_c64_protection.c:365-369`, `uft_xdf_dxdf.h:49-52`, `tests/flux_gen/xum1541/flux_gen.c:62-65`, dazu die zwei aus P3-149. Zwei Kopien nennen „Zone 0" die Spuren 1-17, die anderen die Spuren 31-42 | je Fundstelle mit Werten und Indexrichtung erhoben (MF-877) | ⚠ **offen, und die Reihenfolge ist wichtig.** Zusammenfuehren auf `uft_cbm_track_capacity()` ist richtig, aber **erst nach einem Tor**: vier Zaehlweisen sind nicht mechanisch ineinander ueberfuehrbar, und ohne vorherige Messung fuehrt das Aufraeumen still eine fuenfte ein. Muster fuer das Tor: `scripts/audit_fat_boundary.py` (MF-875) — Kommentar-Strip, Selbsttest, Grundlinie. Nach Regel 9 bewegt der Fund keine Kennzahl: **Fundus**, bis eine weitere Kopie nachweislich falsch liest. Zwei taten das bereits (P3-148, P3-149). Plan: [`PLAN_C64_SCHREIBSEITE.md`](PLAN_C64_SCHREIBSEITE.md) §3 |
| P3-151 | **Der Gap-Laengen-Zusage im SSOT-Header fehlt der Pruefstand.** `src/formats/cbm/uft_cbm_geometry.c:27-29` sagt, der Erzeuger liefere „byte-identical G64 output against the c1541 reference image". Der Test, der das belegen soll, vergleicht `tests/test_convert_via_plugin.c:140-157` UFTs Blob-Pfad gegen UFTs Plugin-Pfad — **zwei eigene Haende**. Kein Test im Baum haelt ein ERZEUGTES G64 byteweise gegen `tests/corpus_free/vice_c1541_35trk.g64`. Nebenher gemessen: die Gap-Laengen 9/19/13/10 der SSOT messen sich an dieser Aufnahme als **8/17/12/9**; nibtools fuehrt wieder andere (`gcr.c:37-45`: 10/17/11/8) | Bytelaeufe zwischen Datenblockende und naechstem Sync ausgezaehlt (MF-877) | ⚠ **offen — und ausdruecklich NICHT als „Zahlen anpassen".** G64.TXT sagt selbst (`:433-440`), die Tail-Gap-Laenge haenge von Laufwerk, Drehzahl und Formatierprogramm ab. Eine Abweichung gegen EINE Aufnahme belegt nicht, dass 9/19/13/10 falsch sind — sie belegt, dass die **Zusage** nicht traegt. Zuerst gehoert der fehlende Byte-Vergleich gebaut; sein Ergebnis entscheidet, ob die Zusage oder die Zahlen fallen. Das beruehrt die Wandlungsmatrix: D64→G64 steht als verlustfreier Pfad (MF-532), und diese Einstufung haengt an genau dieser Byte-Identitaet |
| P3-152 | **Strg+S schrieb das geladene Abbild neu — und konnte es dabei kuerzen. Behoben MF-879.** `MainWindow::onSave()` ruft `uftSaveImageAs(m_currentFile, m_currentFile)`; ohne Identitaets-Erkennung lief das in `kopiere()` (`src/uft_save_image.cpp:47`), und dort ist die Reihenfolge fuer Quelle==Ziel gefaehrlich: `out.open(WriteOnly)` **kuerzt die Quelle**, bevor etwas geschrieben ist, und bei Kurzschreibung entfernt `QFile::remove(target)` genau das Original. `kopiere()` ist fuer Quelle != Ziel geschrieben — dort ist das Entfernen einer halben Zieldatei richtig (MF-571) | Rotbeweis `tests/test_save_image_converts.cpp::speichern_auf_sich_selbst_schreibt_nichts` mit einer SCHREIBGESCHUETZTEN Datei: ohne den Zweig wird das Speichern abgelehnt („Das Ziel ist nicht beschreibbar"), mit ihm gelingt es (MF-879) | ✅ **behoben.** Quelle und Ziel werden ueber aufgeloeste Pfade verglichen (`canonicalFilePath()`, nicht Zeichenketten); bei Gleichheit wird nichts geschrieben und das gesagt. Der Rotbeweis ist zugleich der sprechendste Symptomnachweis: eine Datei, die BEREITS gespeichert ist, liess sich nicht speichern, weil der Code sie neu schreiben wollte. **Aenderungen gehen ohnehin nicht hier durch** — der Explorer-Reiter schreibt sie unmittelbar und hinter dem Schreibtor (`gateBeforeModify()`, `explorertab.cpp:442,540,661,746,846`, alle fuenf geprueft) |
| P3-153 | **Das PRO-Plugin meldete jeden Schreibvorgang als Erfolg, ohne dass ein Byte die Platte erreichte. Behoben MF-880.** `src/formats/atari/uft_pro_plugin.c` ist ueber die Registry erreichbar. `pro_open()` liest die Datei mit `uft_read_file()` in den Speicher und behaelt **weder Pfad noch `FILE*`**; `pro_write_track()` schrieb in diese Kopie und meldete `UFT_OK`; `pro_close()` macht `free()`. Im ganzen File steht **kein** `fwrite`, `fopen` oder `fseek` (gezaehlt: 0), und die Plugin-Struktur hat kein `.flush`. Die Zusage stand an **drei** Stellen: im Rueckgabewert, in der Merkmalstabelle (`"Write", SUPPORTED`) und in `.capabilities` (`UFT_FORMAT_CAP_WRITE`) | Rotbeweis `tests/test_pro_schreibt_nicht.c`, **1 von 3 rot** gegen den Vorzustand mit der Meldung „write_track meldete UFT_OK, die Datei ist unveraendert" (MF-880) | ✅ **behoben durch Ehrlichkeit, nicht durch einen Schreiber.** Die EINFRIER-REGEL sperrt letzteres: die Datei nennt **keine Quelle**, im Korpus liegt **kein PRO-Abbild**, `PRO_MAX_TRACKS` ist hier 77 und in `uft_pro_parser_v2.c` 80, und der Kopfkommentar sagt „header bytes 4-5", waehrend `pro_open()` `raw[7]`/`raw[6]` liest. Alle drei Stellen sagen jetzt dasselbe; `write_track` bleibt gesetzt und antwortet `UFT_ERROR_NOT_SUPPORTED` — ein NULL-Zeiger gaebe dem Aufrufer keine Begruendung |
| P3-154 | **Das Faehigkeits-Tor prueft, ob ein Funktionszeiger EXISTIERT — nicht, ob die Funktion etwas tut.** `tests/test_capability_manifest.c:147` misst `"Write"` gegen `p->write_track != NULL`. Ein `write_track`, das seinen Puffer wegwirft und `UFT_OK` meldet, erfuellt diese Bedingung vollstaendig — deshalb hat das Tor P3-153 nie gesehen, obwohl es seit MF-658 laeuft | am Torquelltext gemessen (MF-880) | ⚠ **offen als Torluecke.** Der Prueffall aus MF-880 zeigt die Form, die traegt: *ein `write_track`, das `UFT_OK` meldet, muss die Datei geaendert haben* — formatunabhaengig und am Ergebnis gemessen statt am Zeiger. Ihn fuer **alle** registrierten Plugins mit `CAP_WRITE` zu fahren waere das eigentliche Tor; es braucht je Format ein Pruefabbild, das `open()` annimmt. Nach Regel 9 bewegt das keine der vier Kennzahlen — aber es ist die Klasse, die MF-522 (D64/D81) und MF-880 (PRO) bereits zweimal geliefert hat, und beim zweiten Mal hat kein Tor gewarnt **Nachtrag MF-883: teilweise geschlossen.** `scripts/audit_schreibzusage.py` (Tor 57) misst die scharfe Teilmenge — ein Plugin mit `CAP_WRITE` und `write_track`, in dessen Datei **keine** Schreiboperation steht — und hätte PRO (MF-880) wie alle neun aus MF-883 gefangen. **Offen bleibt die unscharfe Hälfte:** Dateien, in denen ein echter `uft_<fmt>_write()` **ohne Aufrufer** neben einem Speicher-Pfad liegt (u.a. `mgt`, `udi`, `apridisk`, `cfi`, `posix`, `qrst`, `rcpmfs`, `hardsector`) — Tor 57 lässt sie bewusst durch, weil eine Erreichbarkeitsanalyse ratender wäre als das Tor tragen darf. Dafür bleibt der hier beschriebene Ergebnistest die richtige Form: schreiben → `close()` → **neu öffnen** → lesen → vergleichen, je Format mit einem Prüfabbild. |
| P3-155 | **Derselbe Aufzaehlungsname hat im selben Bau verschiedene Zahlen — und der Kommentar an der Zuweisung nannte das "safe".** `uft_format_t` ist dreimal definiert (`uft_types.h:122`, `uft_format_parsers.h:42`, `detect/uft_format_detect.h:39`), alle drei unter dem Guard `UFT_FORMAT_ENUM_DEFINED`. Ein Guard laesst nicht zwei Aufzaehlungen ihre Werte TEILEN — er sorgt dafuer, dass je Uebersetzungseinheit nur EINE existiert; welche, entscheidet die Include-Reihenfolge. Am Praeprozessor gemessen (`gcc -E`, derselbe Bau): `UFT_FORMAT_D64 = 1` in `src/core/uft_detect_format_impl.c`, `= 4` in sechs weiteren Einheiten darunter `src/core/uft_format_plugin.c` und `src/formats/d64/uft_d64_plugin.c`. 34 Namen tragen so verschiedene Werte, bei `uft_format_id_t` (`UFT_FORMAT_ID_T_DEFINED`, 5 Definitionsstellen) sind es 48. **Erreichbar:** `uft_detect_format_impl.c:83` schreibt `result->format = (uft_format_t)plugin->format`; `src/analysis/uft_format_suggest.c:333` liest es und vergleicht in `is_flux_format()` (`:96`) und `native_sector_format()` (`:109`) gegen die detect-Fassung — von der Oberflaeche ueber `src/gui/uft_recovery_dialog.cpp:509` und `src/gui/uft_smart_export_dialog.cpp:107` | am Praeprozessor je Uebersetzungseinheit gemessen, nicht gelesen; Namens-/Wertkonflikte je Guardpaar ausgezaehlt (MF-881) | ⚠ **offen, und bewusst nicht hier behoben.** Die Aufloesung ist das Zusammenfuehren der drei `uft_format_t` auf EINE Definition, und das ist kein Einzeiler: 20 Namen gibt es nur in `detect/uft_format_detect.h`, 10 nur in `uft_format_parsers.h`, und `uft_format_suggest.c` benutzt zwei davon. Die fehlenden muessen an `uft_types.h` ANGEHAENGT werden, damit bestehende Werte sich nicht verschieben. Was MF-881 liefert: die **Berichtigung der falschen Zusage** an der Zuweisung (der Kommentar sagt jetzt, dass sie NICHT sicher ist) und **Tor 56** (`scripts/audit_guard_kollision.py`, in `check_consistency.py` verdrahtet), das die Zahl bei 11 festhaelt, damit kein zwoelfter Fall dazukommt. Der Torlauf zaehlt einen Guard nur, wenn drei Bedingungen zugleich gelten: mehrfach vergeben, **verschiedene** Koerper, und die Reihenfolge ist nicht durch einen gegenseitigen Include geschuetzt — von 23 mehrfach vergebenen Guards bleiben so 11 uebrig, die wirklich wuerfeln |
| P3-156 | **Ein Erkenner, der niemals „nein“ sagen kann — und er war der einzige, den die Oberfläche zeigte. Zurückgenommen MF-882.** `src/analysis/uft_ml_protection.c` verglich einen 8-stelligen Merkmalsvektor per Cosinus gegen **15 handgesetzte Signaturen** und meldete ab 0.70 einen PRODUKTNAMEN mit Prozentzahl. Gemessen am übersetzten Modul, drei unabhängige Abtastungen: **441** GUI-erreichbare Punkte → 0 freigesprochen; **6561** Rasterpunkte über den ganzen 8D-Raum → 0; **500 000** Zufallspunkte → 0. Der Zweig `is_protected = false` war für **jede** Eingabe unerreichbar. Eine mustergültig **saubere** Diskette bekam „Long Track Generic, 99,1 %“; ein **leerer** Vektor — also „nichts gemessen“ — „Unknown protection scheme suspected (weirdness: 50,0 %)“, weil `fabsf(blended[1] - 1.0f) * 5.0f` eine ungemessene Null als maximale Abweichung vom Nennwert liest. **Ursache:** alle 15 Signaturen tragen an Stelle [1] (Längenverhältnis) einen Wert um 1.0 — die größte Komponente jedes Vektors; der Cosinus ist skaleninvariant und maß danach die gemeinsame Großkomponente gegen sich selbst. **Erschwerend:** der einzige Aufrufer `src/gui/uft_otdr_panel.cpp:1004-1012` übergibt **sechs der acht** Merkmale als Festwerte — der Vergleich lief gegen die eigenen Platzhalter, und das Ergebnis wurde fett rot gefärbt | drei Abtastungen am übersetzten Modul, Rotbeweis 4 von 8 Prüfungen rot gegen den Vorzustand (MF-882) | ✅ **zurückgenommen, nicht nachgebessert.** Die Signaturtabelle hatte **keine Quelle** — kein Spezifikationsverweis, kein Korpus, kein Trainingslauf; Schwellen oder Geometrie zu justieren hieße, einen zweiten Satz unbelegter Zahlen über den ersten zu legen. Tabelle, Cosinus und Rangfolge sind entfernt; `uft_ml_detect_protection()` meldet `UFT_ML_PROT_NICHT_GEPRUEFT` (bewusst weder 0 noch -1: ein `0` mit `is_protected == false` wäre ein **stiller Freispruch**). Die Oberfläche zeigt das grau als „nicht geprüft“ mit Begründung — weder grün noch rot. `uft_ml_extract_features()` bleibt: sie normiert nur. **Offen bleibt der Wiederaufbau:** er braucht eine **belegte** Signaturtabelle UND eine Geometrie, die beide Antworten geben kann; solange die GUI sechs Merkmale nicht erhebt, wäre auch die beste Tabelle von dort aus nicht befragbar. Vorbild im selben Baum: `uft_protection_extended.c:522`, `uft_speedlock.c`, `uft_c64_protection_enhanced.c` |
| P3-157 | **Neun registrierte Plugins meldeten jeden Schreibvorgang als Erfolg, ohne dass ein Byte die Platte erreichte. Behoben MF-883.** Betroffen: **86F, CP/M, CQM, DCM, DMS, IMD, MSA, SAP, SCL**. In jeder der neun Dateien steht **keine einzige** Schreiboperation (`fwrite`/`fputc`/`fprintf`/`ftruncate`/`WriteFile`), das Plugin hat kein `.flush`, und `close()` gibt den Puffer frei. `write_track` änderte die Speicherkopie und meldete `UFT_OK`. Alle neun beanspruchten zugleich `{ "Write", SUPPORTED }` **und** `UFT_FORMAT_CAP_WRITE`. **Und es gibt keinen allgemeinen Rückweg:** `plugin->flush` wird im ganzen Baum von **niemandem** gerufen — `uft_disk_close()` ruft nur `close`; selbst ein Plugin MIT Flush käme nicht durch. **Der Wandlungspfad war mitbetroffen:** `uft_disk_convert.c:41` zählt bei `UFT_OK` ein `tracks_converted++` und schreibt danach nichts hinaus — eine Wandlung nach IMD meldete umgewandelte Spuren, die nirgends ankamen (keiner der neun steht als Ziel in der Rundlauf-Matrix, der Weg war also nicht angeboten) | Rotbeweis: `tests/test_imd_write_roundtrip.c` um `close()` + **Neu-Öffnen** ergänzt → `FAIL @ 127: t3.sectors[0].data[0] == 0xAB`; Tor 57 gegen den Vorzustand: **9 Befunde**, am heutigen Baum **0** (MF-883) | ✅ **behoben durch Ehrlichkeit, nicht durch neun Schreiber.** Die EINFRIER-REGEL sperrt letzteres: neun Container-Schreiber gegen ungeprüfte Lage wären neun Wetten. Dieselbe Entscheidung wie MF-880 (PRO). `write_track` bleibt **gesetzt** und antwortet `UFT_ERROR_NOT_SUPPORTED` — ein Nullzeiger gäbe dem Aufrufer keine Begründung; der MF-529-Wächterblock bleibt unverändert stehen. **Es war eine VIERTE Zusagestelle, die MF-880 nicht kannte:** `src/policy/uft_write_gate.c:65` führte für SCL ein eigenes `UFT_FMT_CAP_WRITE` — gefunden hat sie nicht meine Aufzählung, sondern das bestehende Tor `[write-gate caps vs plugins]`. Neu: **Tor 57** (`scripts/audit_schreibzusage.py`, Selbsttest 6/6, Grundlinie 0) |
| P3-158 | **Die Zusage, an der der Fusionspfad haengt, stand nirgends. Ausgesprochen MF-884.** `multiread_execute()` baut seinen Ausgabepuffer ueber eine **byteweise** Mehrheit. In der Klasse `MULTIREAD_CLASS_WEAK` (mehrere Inhalte, KEINE gueltige CRC) ist das Ergebnis fast immer eine Bytefolge, die **keine** Lesung geliefert hat — gemessen **99,9 %** bei zwei Lesungen, **100,0 %** bei drei und vier (je 20 000 Zufallslaeufe). Ursache: jede abweichende Byteposition ist dann ein Gleichstand, und `vote_byte()` behaelt stillschweigend den kleineren Bytewert. **Folgenlos bleibt das nur wegen einer Kopplung zweier getrennter Regeln:** `recovered = (confidence >= min_confidence) && (good_reads > 0)` (MF-466) und der Umstand, dass `good_reads` in der WEAK-Klasse per Definition 0 ist. Beide Verbraucher schreiben nur bei `recovered` (`uft_format_convert_flux.c:190`, `:541`). Die daraus folgende dritte Regel — *meldet `execute()` `recovered`, ist `data` eine wirklich gelesene Bytefolge* — war nirgends aufgeschrieben und nirgends geprueft | Streifzug ueber **2 000 000** Zufallseingaben: 948 434 mit `recovered`, 380 014 Fabrikate, **0** zugleich; Gegenprobe: ohne `good_reads > 0` **625** Verletzungen in 20 000 Runden (MF-884) | ✅ **ausgesprochen und gepruefte Zusage, kein Verhalten geaendert.** Der Header fuehrt jetzt eine Tafel, was `data` je Klasse ist; `tests/test_multiread_kein_mischbyte.c` haelt die Kopplung mit einem 20 000-Runden-Streifzug fest (samt Pruefung, dass der Streifzug beide Seiten erreicht hat). **Das Byte-Voting im WEAK-Zweig bleibt** — MF-845 hat es begruendet stehen lassen, und die Begruendung traegt: ohne CRC ist die byteweise Mehrheit die beste verfuegbare Schaetzung und bei einem echt schwachen Sektor mit vielen Lesungen **besser** als jede einzelne Lesung. **Offen bleibt nichts** — aber wer MF-466 je lockert, macht die Schaetzung still schreibbar; genau dafuer steht der Test |
| P3-159 | **Zwei Sektoren mit derselben Nummer an verschiedenen Stellen der Spur sind für UFT ein Widerspruch — für das Referenzwerkzeug zwei Sektoren.** Gemessen: in `src/recovery/uft_multiread_pipeline.c` kommt kein Begriff für Winkelposition vor (`position`/`angular`/`winkel`/`phantom` → ein einziger Treffer, und der ist ein Doxygen-Wort). a8rawconv gruppiert **vor** jedem Inhaltsvergleich nach angularer Position mit 3 % Toleranz (`src/a8rawconv/disk.cpp`, `if (fabsf(poserr) > 0.03f) break;`) und trennt so **Phantomsektoren** — ein bekannter Kopierschutztrick — von echten Mehrfachlesungen desselben Sektors. UFT behandelt beide gleich: als widersprüchliche Lesungen eines Sektors. Bei geschützten Disketten ist das die falsche Antwort | kommentarfreie Symbolsuche über `git ls-files`; Referenzverhalten am vendorierten `src/a8rawconv/disk.cpp` gelesen (MF-886) | ⚠ **Fundus mit Entscheidungsbedarf — bewusst nicht umgesetzt.** Die Eingangsstruktur `multiread_pass_t` trägt **keine** Positionsangabe. Sie zu ergänzen ist eine Änderung an einer öffentlichen Struktur (ABI) und damit Eigentümer-Entscheidung, nicht Nebenarbeit. **Und ohne Prüfstück wäre jede Umsetzung ungeprüft:** im Korpus liegt keine Aufnahme mit doppelter Sektornummer. **Was es öffnen würde:** ein solches Abbild — dann in dieser Reihenfolge: Feld anhängen (nie einfügen, ABI), Gruppierung vor den Inhaltsvergleich, Rotbeweis am echten Abbild |
| P3-160 | **`uft_precomp_apply()` ist geprüft und unverdrahtet — und es gibt keinen Anschluss, an den es gehörte.** Der Moduleingang in `src/core/uft_write_precomp.c` hat **0** Aufrufer in `src/`; nur `tests/test_write_precomp.c` ruft die Rechenfunktion darunter. **Berichtigung einer Zulieferung:** der Bericht `a8rawconv-full-analysis.md` §4 sagte, `uft_precomp_track_mac800k` habe keinen Aufrufer — sie **hat** einen (`uft_write_precomp.c:95`), der Bericht hatte ihn per `grep -v uft_write_precomp` selbst ausgeschlossen. Verwaist ist der Eingang eine Ebene darüber. Schluss richtig, Beleg falsch | je Bezeichner kommentarfrei über `git ls-files` **gemessen**, nicht aus der Zulieferung übernommen (MF-886) | ⚠ **offen als benannte Wartestellung, und ausdrücklich NICHT verdrahtet.** Es gibt in UFT keinen Mac-800K-**Flusspfad**: `src/formats/apple/mac_dsk.c` (77 Zeilen) ist ein Container-Leser für bereits dekodierte Abbilder. Eine Aufrufstelle zu erzwingen wäre eine Verdrahtung ohne Verbraucher (MF-630/632). Steht bewusst **nicht** in `docs/orphan_baseline.txt` — jene Liste führt Module, die *niemand* ruft, auch kein Test. **Was es öffnen würde:** ein Flusspfad, der Mac-800K-GCR dekodiert; dann gehört der Aufruf auf die **rohen Übergangszeiten**, bevor die Bits dekodiert werden (a8rawconv `compensation.cpp`, `-P mac800k`). Ein `-P`-äquivalenter Regler wird bis dahin nicht angelegt — ein Schalter ohne Wirkung ist ein Workaround-Stub. Der Modulkopf sagt das jetzt selbst |
"`. **Und der Baum bezeugt es selbst:** `audit/adfcopy/mock_adfcopy.py:34` trägt wörtlich `# Opcodes — needs-source (see extract_ref.py): values from UFT comments only.` — nachgemessen. **Die Prüfkette ist geschlossen:** Mock spiegelt `adfcopy_provider_v2.cpp`, `firmware_state_machine.c` spiegelt den Mock, `test_adfcopy_runners_protocol.cpp` prüft die Runner gegen den Mock. Die 15 Tier-1-Tests sind grün, weil **UFT gegen UFT** geprüft wird — dieselbe Klasse wie MF-301 | gemeldet vom Eigentümer, In-Baum-Teile nachgemessen (MF-807) | ⚠ **offen, in drei Schritten.** (1) **Die Probe:** `teensy_probe.cpp:67` sendet als ADF-Copy-Identify ein einzelnes `0x00` (nachgemessen). Die Firmware nimmt das als Kommandozeile, findet keinen Treffer und antwortet gar nicht — kein Default-Zweig. `ping_ok` ist damit **immer** false, die Einordnung fällt auf `Unknown`, und `Unknown` löst absichtlich keine Warnung aus. Die Zusage in `RELEASE_NOTES.md` („prevents disk-corrupting cross-wire") besteht so nicht. Fix: `ver
` statt `0x00` — das Kommando ist reines `Serial.printf`, kein Motor, kein Kopf, keine Medienberührung — **und die Einordnung auf INHALT umstellen** (Teilzeichenkette `ADF-Drive`), sonst antwortet die Applesauce mit ihrer Unknown-Command-Zeile, beides ist „plausibel", und man landet wieder bei `Unknown`. (2) **Die Statuszeile berichtigen:** Read/Write stehen auf `—`, aber `get <n>`+`download` liest und `upload`+`put <n>` schreibt sektorbasiert — ADF-Copy ist ein **Decoded-Image**-Provider wie USB-Floppy, kein Flux-Provider. `kAdfcStatusFluxCapable` und die 25-ns-Angabe sind unbelegt: `flux` liefert 256 × uint32 LE **Histogramm-Bins**, also eine Timing-Verteilung, keinen Transitionsstrom; der Tick-Takt von `capf`/`getcells` über FTM0 auf einem Teensy 3.2 ist zu **messen**, nicht aus SCP abzuschreiben. (3) **Die Runner sind ein Neuschrieb, kein Bump** — Zeilenprotokoll statt Byte-Opcodes, und Mock plus Emulator müssen mit, sonst bleibt die Zirkularität. Wegen **GPLv3** gehört (3) in die Nachbau-Werkstatt (`uft-nachbau`): die Firmware ist Referenz für das **Wire-Protokoll**, nicht für Code — Kommandonamen und Antwortformate sind Schnittstellenfakten, aber die Runner gehören gegen ein destilliertes Protokoll-Gutachten geschrieben, nicht neben den offenen Firmware-Puffer. **NACHTRAG MF-856 — derselbe Denkfehler ein drittes Mal, und diesmal steht er als `static_assert` im Code.** `adfcopy_provider_v2.h:493` verbietet die Faehigkeit ausdruecklich: `static_assert(!MeasuresRPM<ADFCopyProviderV2>, "… V1 measureRPM() returns constant 300.0 with no serial dialog …")`. Die Firmware hat dafuer ein Kommando: `index` antwortet `"%d microseconds\nOK\n"`, bei fehlender Diskette `"NO DISK"`. Die Pruefung hat also **UFTs eigene, unvollstaendige V1-Umsetzung** begutachtet und daraus geschlossen, die HARDWARE koenne es nicht — dieselbe Verwechslung wie bei `WritesRawFlux` („Honest audit: hardware cannot do this", waehrend die Firmware `write`, `testwrite`, `upload`, `enc`/`dec` fuehrt). **Ein `static_assert` mit falscher Begruendung ist schlimmer als ein fehlendes Mixin:** er zementiert den Irrtum und macht die Korrektur zu einer Aenderung an einer Zusage, nicht an einer Auslassung. Gehoert in denselben Neuschrieb wie (3), also in die Nachbau-Werkstatt. **Nebenbefund, forensisch wertvoll:** `weak` gibt die Retry-Zahl des letzten Lesevorgangs zurück, `exterr` eine Klartext-Fehlerursache — die Provenienz-Metadaten-Schicht, die die anderen Provider nicht liefern |
| P3-20 | **`dummy.bad`: eine Diskette, die gesund aussieht und es nicht ist (MF-790).** **Der wichtigste offene Punkt aus dem Hinweis des Eigentümers, weil er die Mission unmittelbar trifft.** `AllowBad 0.7` (1996) und `badformat 0.2` sind **Anti-Preservation-Werkzeuge**: sie schreiben absichtlich auf beschädigte Medien und **verstecken** den Schaden. AllowBad belegt die defekten Spuren mit einer Scheindatei `dummy.bad`, gegen Lesen/Ändern/Löschen geschützt — das Dateisystem meidet sie, und die Diskette ist wieder benutzbar. **Für ein Werkzeug, das den Marker nicht kennt, liest sich so eine Diskette als gesund:** 880 K, eine Datei, Bitmap stimmig. Der Befund wäre falsch, und zwar in genau der Richtung, die dieses Projekt ausschließt — „keine stille Veränderung" gilt auch für den *Bericht*. Im Baum findet sich dazu **nichts** (gemessen). **Klar umrissener Prüfauftrag:** `dummy.bad` erkennen, benennen, und die belegten Bereiche als **schadensverdächtig** statt als belegt führen. Der Marker ist dafür gut geeignet — benannt, geschützt, an Spurgrenzen ausgerichtet. Kennzahl: keine der vier direkt, aber es ist ein **Falschbefund**, und die stehen in diesem Baum über Kennzahlen |
| P3-21 | **`diskspare.device` — ein Amiga-Format ohne Eintrag im Baum (MF-790).** AllowBad nennt es neben `trackdisk.device` als getestetes Gerät; es hat abweichende Geometrie und **mehr Sektoren je Spur**. Im Baum: kein Treffer. Zensus-Eintrag für den **Varianten-Zubringer** (`uft-variants`), nicht für ein neues Plugin — die EINFRIER-REGEL gilt. Nebenbefund des Eigentümers: derselbe Klaus Deppisch steht hinter `CrunchDisk.c` in LibXAD |
| P3-22 | **P9 hat eine zweite unabhängige Quelle (MF-790).** AllowBad prüft `HighCyl` höchstens **81**, also **82 Zylinder** — dieselbe Obergrenze, die das X-Copy-Handbuch nennt (`endtrack DC.W 79 ; 0 - 81`). Zwei voneinander unabhängige Werkzeuge, dieselbe Zahl. Damit ist die Grenze **belegt** statt aus einer Quelle übernommen |
| P3-15 | **FAT12 hat jetzt einen fremden Erzeuger — `mformat`, und er lässt sich **bootcodefrei** stellen (MF-788).** Gemessen vom Scout: `mtools 4.0.49` (GPL-3) baut unter MinGW (ein Patch nötig: `HAVE_ICONV_H` austragen, Qt-MinGW hat kein `langinfo.h`). Befehlszeile mit **expliziter** Geometrie: `mformat -C -i x.img -t 80 -h 2 -n 9 -N 12345678 -v UFTFAT12 ::`. **Fremdcode-Befund, gemessen an fünf Abbildern:** ohne `-B` **44 Bytes ≠ 0** hinter dem BPB (36 aus `bootprog[]`, 8 aus einem Schein-Partitionseintrag) — **mit `-B zero.bin` oder `-k`: 0 Bytes**, dann fehlen aber auch Sprung und `0xAA55`, die Vorlage muss sie tragen. Zwei Läufe, identische SHA-256. **Das löst Schritt 4 des Plans ohne blueMSX und ohne Nextor.** Hindernis: Windows Defender quarantänisiert das gebaute `mtools.exe` als `Trojan:Win32/Bearfoos.A!ml` (ML-Heuristik) — Eigentümer-Entscheidung: Ausnahme oder signiertes Paket (msys2 und WSL sind vorhanden) |
| P3-16 | **Zweite FAT-Hand: Flopgen — und sie bringt einen Prüffall mit (MF-788).** GPL-3, vendort FatFs R0.16 (deren Lizenz „1-clause BSD-style" **nicht in der Matrix → PRÜFEN**). Baut mit MinGW, erzeugt ein 720K-Abbild mit `EB FE 90`, **0 Bytes Code**, und dessen BPB ist **bytegleich** mit dem von mtools — zwei unabhängige Hände, dasselbe BPB. **Gemessener Makel, der ein Testfall wird:** `FAT[0] = F8` bei BPB-Media `F9`. Das ist ein echter Widerspruch im Erzeugnis, und UFTs FAT12-Leser sollte ihn **bemerken**, nicht stillschweigend überlesen. `dosfstools` (GPL-3) taugt nicht als Erzeuger (Bootcode **immer** 448 B) — wohl aber als **Prüfer** über `fsck.fat` |
| P3-17 | **Blockade 2 bleibt: keines der sieben Werkzeuge schreibt Container mit expliziter Geometrie (MF-788).** Geprüft: `TotalImage` (MIT, GUI) erzeugt **nur Raw**; `Flopgen` Raw; `DiskFormatID` ist ein KryoFlux-Frontstück; `EmptyFlops` sind Daten; `DiskFlashback` GUI. **`CreateXDF` ist ausdrücklich NICHT zu verwenden:** keine Lizenzdatei und ein eingebetteter proprietärer IBM/Backup-Technologies-Bootsektor („Duplication prohibited"). Damit bestätigt sich der Weg aus P3-14: **die Geometrie an SAMdisk und hxcfe explizit übergeben** — und der erste Versuch in dieser Richtung hat bereits einen echten Lesefehler gefunden (MF-787) |
| P3-14 | **Container-Formate brauchen ein anderes Verfahren als das gw-Rezept (MF-785).** SAMdisk 4.0 und hxcfe 2.16.13 sind installiert und im Register (3 von 14 verfügbar). Beide **können** Container mit Kopf schreiben, was gw nicht kann — SAMdisk `SAD`, `MGT`, `DSK`; hxcfe `V9T9`, `JV3`. **Gemessen scheiterten trotzdem alle fünf**, und die Ursache stand in SAMdisks eigener Ausgabe: *„input format guessed from file size — please check"*. Beide Werkzeuge **legen den Inhalt neu aus**, statt ihn durchzureichen; die Prüfmarke „Sektor *n* trägt seine Nummer" überlebt das nicht: `sad` und `v9t9` verloren die Kennung, `mgt` lieferte keine Daten, `edsk` meldete 9 statt 10 Sektoren, `jv3` 40/1/18 statt der erwarteten 40/2/9. **Das ist kein Fehler der Werkzeuge**, sondern die Folge einer *geratenen* Eingangsgeometrie. Der nächste Schritt ist deshalb nicht „mehr Formate", sondern: **die Geometrie explizit angeben** statt raten zu lassen (SAMdisk und hxcfe haben beide Optionen dafür). Erst danach ist die Prüfmarke wieder aussagekräftig. **BERICHTIGUNG (MF-794): die Prämisse trägt nicht.** Gemessen mit expliziter Geometrie (`SAMdisk copy -c80 -s10 -z2 -b1`) kommt für `sad` eine Datei heraus, die mit der geratenen **byteidentisch** ist — das Raten war also nicht die Ursache. Die Ursache war die **Erwartung**: der Korpus-Test verglich gegen eine *lineare* Anordnung, SAD legt aber **kopf-dur** ab (alle Zylinder von Seite 0, dann Seite 1). Belegt von zwei unabhängigen Händen — SAMdisk schreibt so, hxcfe liest es so zurück, Rundlauf `img→sad→img` byteidentisch. Für `mgt` und `edsk` schließt sich derselbe Rundlauf ebenfalls byteidentisch. **Die Werkzeuge zerstören nichts; sie legen den Inhalt nach dem Zielformat ab.** Was der Differenzlauf stattdessen fand, ist ein echter Lesefehler in UFT (MF-794): `uft_sad.c` rechnete zylinder-dur und las damit **158 von 160 Spuren an der falschen Stelle** — bei einem Format, das auf **T1b** stand. Offen bleiben `v9t9` (SAMdisk schrieb nichts) und `jv3` |
| P3-13 | **Nextor als Spec- und Ground-Truth-Quelle für MSX — die Vorfrage ist gemessen und fällt günstig aus (MF-779).** Die entscheidende Frage lautete: liest UFT MSX-DOS schon? **Ja** — `src/formats/msx/uft_msx.c`, `src/formats/dsk_msx/uft_dsk_msx.c`, und `msx_disk` steht auf **T3: null Tests, keine Spec-Quelle**. Damit ist Nextor **kein neues Format**, sondern eine Hebungsquelle für ein vorhandenes — die EINFRIER-REGEL erlaubt das ausdrücklich („Verifikations-/Test-/Korpus-Arbeit"). **Kanal (MF-695): Spec + Daten/Fixture, NICHT Port.** Der Code ist Z80-Assembler für MSX-Hardware und für UFT ohnehin unbrauchbar; wertvoll sind `docs/` (Programmers Reference, Driver Development Guide, `DOS2-PIS.TXT` zum MSX-DOS-Dateisystem) — eine **autoritative** Quelle statt Reverse Engineering, genau die Klasse, deren Fehlen die fünf Fabrikationen erzeugt hat. **Der praktisch wertvollere Teil:** Nextor läuft in **blueMSX** — im Emulator formatieren, eigene Dateien schreiben, Abbild herausziehen. Das ist **T1b** (cross-tool), die stärkste hier erreichbare Klasse; T1 verlangt Hardware (MF-310). **Zwei Vorbedingungen aus MF-779, bevor ein Korpus-Eintrag entstehen kann:** (a) blueMSX braucht einen **Registereintrag** in `oracles.py`, sonst weist das neue Tor „Korpus-Herkunft" den Eintrag ab; (b) der **Fremdcode-Befund** wird hier zum ersten Mal scharf — eine formatierte MSX-DOS-Diskette trägt mit hoher Wahrscheinlichkeit einen **Bootsektor**, also `ja (was)` statt `nein`. Das ist kein Hindernis, sondern genau der Zweck des Feldes: es zwingt die Frage, bevor fremder Code im Repository liegt. **Lizenz UNGEPRÜFT** → Zone PRÜFEN, Urteil beim Eigentümer (MF-679); Quelle: `github.com/Konamiman/Nextor`  — **Q7 BEANTWORTET (MF-780): kein neues Format, und der Gewinn ist größer als MSX.** Gemessen: `msx_disk` ist **kein Dateisystem-Plugin**, sondern ein **Container**-Leser (`probe/open/read_track/write_track` auf einen kopflosen Sektordump); sein eigener Kopf sagt „MSX uses IBM-PC compatible FAT12 format". Das Dateisystem dahinter ist **FAT12**, und das hat der Baum: `src/fs/uft_fat12.c`. Damit greift die EINFRIER-REGEL **nicht** — es entsteht kein Plugin, es wird ein vorhandenes belegt. **Und der eigentliche Gewinn liegt bei FAT12, nicht bei MSX:** `uft_fat12` steht auf FS-T1 mit dem Vermerk *„alle Tests bauen ihre Eingabe selbst — geprüft gegen den eigenen Erzeuger"*. Ein Nextor-formatiertes Abbild wäre das **erste fremd erzeugte FAT12** in diesem Baum — und das bedient **jedes** FAT12-tragende Format, nicht nur MSX. Die Klick-Sitzung ist damit **jetzt** dran, nicht nach der nächsten Hebung. — **FRAGE 2 BEANTWORTET (MF-781), und sie fällt scharf aus:** `FORMAT` schreibt Code, und zwar Nextors eigenen. Dreifach belegt — User Manual, Kopfkommentar von `source/kernel/bank2/bootsect.mac`, und **gemessen am Release-Abbild: Offset 0x1E–0xB7 = 154 Bytes**, Rest null, mit den Strings „Boot error" und „MSXDOS SYS". Damit ist ein Nextor-formatiertes Abbild **in diesen 154 Bytes ein Derivat**. Eine Formatier-Option ganz ohne Bootcode gibt es praktisch nicht. **Folge:** das Abbild geht nach `tests/corpus/` (gitignored), Manifest-Feld `fremdcode` lautet **„ja (154 B Bootcode, Offset 0x1E–0xB7)"** — genau die in diesem Punkt vorausgesagte Antwort. **Für den FAT12-Zweck ist das folgenlos:** FAT, Verzeichnis und Cluster liegen außerhalb dieser Bytes. **Lizenz (Bericht, kein Urteil, MF-679):** eine `LICENSE.md`, MIT-ähnlich mit zwei Klauseln — keine kommerzielle Nutzung und **keine Derivate ohne ausdrückliche Erlaubnis** (Forks ausdrücklich eingeschlossen). Zone PRÜFEN; der Oracle-Weg bleibt offen, weil Ausführen keine Ableitung ist. **Achtung bei `nextor.dsk` aus dem Release:** laut Makefile von **mtools** formatiert, nur der Bootcode eingepatcht — als „Nextor-formatiertes FAT12" **untauglich**, als bootfähige Werkzeugdiskette für die Sitzung dagegen der kürzeste Weg (bringt COMMAND3 mit FORMAT mit)|

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

### LIZ-4 B entschieden (MF-744) — die XCopy-Lizenz liegt vor, und sie verbietet es

Der Eigentümer hat `xcopypro_source_2011.zip` beigebracht
(SHA-256 `0047220e40ae5441…`, eingefroren über
`tools/uft-nachbau/scripts/sichtprotokoll.py`). Das Archiv enthält zwei
Dateien: `xcopy_licence.txt` und ein 880K-Amiga-Abbild mit dem
Quelltext.

**Gelesen wurde nur die Lizenzdatei.** Das Abbild ist bewusst
ungeöffnet und steht nicht im Sichtprotokoll — die Lizenz entscheidet,
ob es überhaupt geöffnet werden darf, und sie entscheidet es mit Nein.

#### Der Wortlaut

| Klausel | Text |
|---|---|
| **1. Usage** | *„You are entitled to use the enclosed software free for **private, non-commercial** use. Commercial or **governmental use is not permitted**."* |
| **2. Distribution** | *„…only if you keep **all the files in it intact**, and provided you do it **free of any charge at all**."* |
| **3. Copyrights** | *„© 1988-2011 Anguilla Software International Ltd., Bletchley Manor, Long Ground, Anguilla, BWI."* |
| **Bearbeitung** | **kommt nicht vor** |

#### Drei Folgen, alle zwingend

**1 · Kein Recht auf Bearbeitung.** Die Lizenz gewährt *Nutzung* und
*unveränderte Weitergabe*. Ein Bearbeitungsrecht wird nirgends
eingeräumt. `uft_amiga_protection.c` erklärt sich im eigenen Kopf als
*„C99 port of XCopy Pro (1989-2011) 68000 Assembly algorithms"* und
trägt den Original-Assemblertext mitsamt seinen deutschen
Originalkommentaren in den Kommentarblöcken. Das ist ein
**Bearbeitungswerk ohne Erlaubnis**.

**2 · Unvereinbar mit GPL-2.0-or-later.** Die GPL *verlangt*, dass
kommerzielle Nutzung gestattet wird (§0, §6: keine weiteren
Beschränkungen). Diese Lizenz *verbietet* sie. Die Dateien können unter
unserer Lizenz nicht stehen — und einen SPDX-Kopf zu setzen hieße, eine
Lizenz zu erfinden. Das ist die Fehlerklasse, die dieser Baum als P0-5
bezahlt hat.

**3 · Unsere Zielgruppe ist ausdrücklich ausgeschlossen.** `CLAUDE.md`
nennt „Archive, Museen, digitale Forensiker". *„Governmental use is not
permitted"* schließt einen erheblichen Teil davon aus — Staatsarchive,
Landesmuseen, Behörden.

#### Damit ist LIZ-4 B entschieden

Nicht mehr „unklar ⇒ wie portiert" (§4), sondern **belegt portiert unter
einer Lizenz, die Bearbeitung nicht gestattet**. Es ist keine
Abwägungsfrage mehr.

Betroffen sind vier Stellen in zwei Übersetzungseinheiten:

```
src/formats/amiga/uft_amiga_protection.c    seit MF-699 aus dem qmake-Bau
src/analysis/uft_track_analysis.c + .h      steht NOCH im Bau (.pro:1219)
```

#### Die Entfernung ist kleiner als befürchtet — gemessen

`uft_track_analysis.h` wird von sieben Dateien eingebunden, was nach
viel aussieht. Gemessen, was sie daraus benutzen:

| | |
|---|---|
| exportierte Funktionen in `.c` | **20** |
| davon von den Einbindern gerufen | **0** |
| gebrauchte Typen | **2** — `uft_platform_t`, `uft_platform_profile_t` |

Die sieben Profil-Dateien brauchen **nur zwei Typdeklarationen**. Ein
Plattform-Enum und eine Profilstruktur sind Fakten, keine
Ausdrucksform — sie lassen sich behalten, ohne die Vorlage zu berühren.
Die 20 abgeleiteten Funktionen haben null Abnehmer.

#### Was ausdrücklich NICHT folgt

**Kein Nachbau.** MF-740 hat das schon entschieden, und die Lizenz
ändert daran nichts: von den vier Sync-Wörtern ist eines belegt, zwei
werden von unabhängiger Preservation-Dokumentation widersprochen, und
der Baum selbst führte `0xA245` unter zwei verschiedenen Namen. Man
kann nichts sauber nachbauen, dessen Fakten falsch sind.

**Das Archiv gehört nicht in den Baum** — auch nicht ins Korpus.
Klausel 2 erlaubt Weitergabe nur **vollständig**; ein einzelnes ADF als
Fixture wäre die Weitergabe eines Teils. Das 880K-Abbild wäre als
AmigaDOS-Fixture reizvoll gewesen; es ist nicht verfügbar.

**Das Abbild bleibt zu.** Es zu öffnen brächte nichts, was die
Entscheidung ändert, und würde die Kontamination nur vertiefen. Und es
kann die Sync-Wörter auch nicht belegen: XCopys eigenen Quelltext zu
lesen bestätigt, *was XCopy behauptet* — nicht, ob es stimmt. Genau
darum ging es in MF-740.

### BERICHTIGUNG zu MF-744 (MF-746) — ein Bearbeitungsrecht gibt es, und es hilft trotzdem nicht

Der Eigentümer hat ein drittes Archiv beigebracht: den **entpackten
Inhalt** des Abbilds, mit `Source/xcopypro_src_original_1992.lha`,
`Source/xcopypro_src_fixed_2011.lha` — und einer Datei, die ich in
MF-744 nicht hatte: **`Readme 2011`**.

Gelesen wurde ausschließlich diese Datei, weil sie lizenzrelevant ist.
Die beiden `.lha`-Quellen sind **ungeöffnet**.

#### Was darin steht

> *„The intention of this release is document the development process of
> the software. This software is released as is, without any guarantees
> or warranties. **You are welcome to enhance it or develop further
> versions, just keep it free (don't sell it) and release your source as
> well.**"*
>
> *„Regarding all other matters please respect the licence that came
> with this disk image."*
>
> — Christian Bartsch für Anguilla Software International, 26.12.2011

#### Was ich in MF-744 falsch geschrieben habe

Dort stand:

> **„Kein Recht auf Bearbeitung.** Die Lizenz gewährt *Nutzung* und
> *unveränderte Weitergabe*. Ein Bearbeitungsrecht wird nirgends
> eingeräumt."

**Das ist falsch.** Der Rechteinhaber räumt ausdrücklich ein, die
Software zu erweitern und weiterzuentwickeln. Ich hatte nur
`xcopy_licence.txt` und habe aus dessen Schweigen auf ein Verbot
geschlossen — ein Schluss aus fehlender Erwähnung, und genau die Art
Schluss, die dieser Baum sonst als unbelegt zurückweist.

Die Datei, die es widerlegt, lag in einem Archiv, das ich zu dem
Zeitpunkt nicht hatte. Das entschuldigt den Fehler nicht: die richtige
Formulierung wäre „in den mir vorliegenden Unterlagen findet sich kein
Bearbeitungsrecht" gewesen, nicht „es wird keines eingeräumt".

#### Das Ergebnis bleibt — aus einem anderen, engeren Grund

Die Freigabe ist an **Bedingungen** geknüpft, und zusammen mit der
Lizenz, auf die sie ausdrücklich verweist, ergibt sich:

| Bedingung | Quelle | Vereinbar mit GPL-2.0-or-later? |
|---|---|---|
| „keep it free (**don't sell it**)" | Readme 2011 | **nein** — GPL §6 verbietet zusätzliche Beschränkungen, und die GPL erlaubt ausdrücklich, für die Weitergabe Geld zu nehmen |
| „release your source as well" | Readme 2011 | ja, das ist Copyleft-Geist |
| „private, **non-commercial** use" | `xcopy_licence.txt` | **nein** — Nutzungszweck-Beschränkung |
| „**governmental use is not permitted**" | `xcopy_licence.txt` | **nein** — schließt Staatsarchive und Landesmuseen aus, unsere Zielgruppe |
| Weitergabe nur vollständig und kostenlos | `xcopy_licence.txt` | **nein** |

**Der entscheidende Punkt ist der Verkaufsvorbehalt.** Man kann etwas
nicht unter GPL stellen, wenn man die Erlaubnisse, die die GPL
weitergeben muss, selbst nicht hat. Eine „don't sell it"-Auflage ist
eine zusätzliche Beschränkung im Sinne von §6 — sie macht die Lizenz
GPL-**inkompatibel**, unabhängig davon, wie wohlwollend sie gemeint ist.

Damit gilt unverändert: `uft_amiga_protection.c` und
`uft_track_analysis.c`+`.h` können **nicht** unter `GPL-2.0-or-later`
stehen. Der Weg bleibt **entfernen**.

Was sich ändert, ist der Ton der Begründung: es ist **keine
Rechtsverletzung**, sondern eine **Lizenz-Unverträglichkeit**. Die
Autoren haben die Weiterentwicklung ausdrücklich erlaubt — nur unter
Bedingungen, die unsere eigene Lizenz nicht tragen kann. Das ist ein
Unterschied, der in einem Projekt zählt, das Attributionen als
Tatsachenbehauptungen führt.

#### Ein Nebenbefund, der ein Rätsel schließt

> *„X-Copy was originally developed by **Frank Neuhaus** and H.G. Berg,
> and later maintained by Hans Kurent and Holger Vocke."*

In MF-739 stand: *„«Neuhaus» ist ein **Personenname als
Routinenname**; so etwas errät man nicht."* Jetzt ist belegt, wessen
Name: der des ursprünglichen Entwicklers. Der Kopf unseres
`uft_amiga_protection.c` führt die Routine als *„Neuhaus (breakpoint
detection)"* — das ist der Name aus der Vorlage, übernommen.

Der Idiom-Befund aus MF-739 ist damit unabhängig bestätigt.

#### Was weiterhin nicht folgt

**Kein Nachbau.** Unverändert: von vier Sync-Wörtern ist eines belegt,
zwei werden widersprochen, die Dateien haben null Aufrufer (MF-740).
Das Bearbeitungsrecht ändert nichts an fehlenden Fakten.

**Die Quellen bleiben zu.** `xcopypro_src_original_1992.lha` und
`…_fixed_2011.lha` sind ungeöffnet und stehen nicht im Sichtprotokoll.
Sie zu öffnen brächte nichts, was die Entscheidung ändert.

**Und die Verwahrung wird leichter.** Der Readme erlaubt Weitergabe
ausdrücklich („just keep it free") — die Sorge aus MF-744, ein
einzelnes ADF als Korpus-Fixture wäre unzulässige Teil-Weitergabe,
trägt so nicht mehr. Ob wir ein Abbild unter diesen Bedingungen ins
Korpus nehmen wollen, ist eine **Eigentümer-Entscheidung**; es wäre das
erste Fixture mit einer nicht-freien Lizenz, und `LIZENZ_ANFRAGEN.md`
wäre der Ort, das zu führen.
