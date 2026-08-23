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
| Tests | **245/245 grün** |
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

| # | Punkt | Stand |
|---|---|---|
| P0-1 | **Erfundene Konfidenzzahlen** in der Kopierschutz-Anzeige: `85 %`, `70 %`, `60 %` waren hartkodierte Literale unter der Spaltenüberschrift „Confidence" | ✅ **behoben (MF-508)** |
| P0-2 | **„55+ Kopierschutz-Schemes"** steht als Kernfunktion in `CLAUDE.md`/`README` — das Erkennungs-Subsystem (`src/protection/`, 20 Dateien, ~200 Funktionen) hat **keinen Aufrufer**. Die Oberfläche zeigt stattdessen eine Heuristik | **offen** |
| P0-3 | **57 von 88 Formaten sind T3.** ~~Die Formatliste nennt sie ohne diese Unterscheidung.~~ **Diese Einstufung war zu hart:** `README.md` sagt es bereits ausdruecklich („Honest verification status … T1=2, T1b=12, T2=17, T3=57“). Ohne den Hinweis waren nur `CLAUDE.md` und `SHOWCASE.md` — letzteres nannte zusaetzlich eine veraltete Zahl (80 statt 137 Plugins) und „100 % Prinzip-7-Compliance“ ohne den Zusatz, dass das Metadaten-Vollstaendigkeit ist und keine Verifikation | ✅ **behoben (MF-509)** |
| P0-6 | **Drei Speicherfehler im DSK-Oeffnungspfad** — Heap-Ueberlauf (44830 B, gemessen), Stapel-Ueberlauf (1800 B) und ein **doppeltes free bei jeder abgeschnittenen DSK-Datei**. Gefunden vom neuen Fuzzer ueber `uft_disk_open()`, nicht durch Lesen | ✅ **behoben (MF-513)** |
| P0-7 | **Zwoelf `read_track` schrieben durch einen Nullzeiger** — `track->sectors[s] = ...` auf einem Zeiger, den `uft_track_init()` nicht anlegt. Woertlich derselbe kopierte Rumpf in 12 Plugins; alle zwoelf konnten nie funktionieren. Dazu MF-515: `edsk_parser_read_track` las ~29 KB hinter sein Feld | ✅ **behoben (MF-515/516)**, Tor gesetzt |
| P0-8 | **45 `read_track` indizierten mit `cyl`, ohne auf negativ zu pruefen** — `-1 >= tracks` ist falsch, `track_data[-1]` ein Zugriff vor dem Feld. Gefunden an OPUS, als der Fuzzer die Dateigroessen anbot, die die Sonden verlangen (aus den Sonden gelesen, 46 Werte) | ✅ **behoben (MF-518/519)** |
| P0-9 | **Der Schreibpfad nahm Koordinaten an, die es nicht gibt** — ADF und D81 ohne jede Schranke (aus 880 KB wurden 11 MB; `cyl=-1,head=2` schrieb ueber **Spur 0**), D64/ATR/XFD mit Schranke, die `UFT_OK` meldet ohne zu schreiben. 29 Befunde, kein einziger ein Absturz | ✅ **behoben (MF-522)** |
| P0-10 | **Ein eingeschlepptes `snprintf` ueberschrieb das der C-Bibliothek** — Parser von 1995 ohne `%z`; 10 Aufrufstellen unter `src/` betroffen, eine stuerzte ab. Dazu drei weitere Sanitizer-Befunde (Lesen hinter einer Zeichenkette, Verschiebung um 32 Stellen, Linksschieben auf `int`) | ✅ **behoben (MF-523/524)** |
| P0-11 | **Leck-Rueckstand jetzt sichtbar, nicht behoben** — LeakSanitizer meldet im Volllauf Lecks bis **8 132 856 Byte in 321 Allokationen** je Test. Das scharfe Tor laeuft deshalb mit `detect_leaks=0` | **offen**, gemessen ab MF-517 |
| P0-12 | **„44 Konvertierungspfade“ beschreibt eine Tabelle, keine Faehigkeit** — angeboten werden **8** (2 verlustfrei, 6 mit Zustimmung); 33 weist das Preflight-Tor als UNGEPRUEFT ab, 3 als unmoeglich. Die Abweisung ist Absicht und richtig; die Zusage war es nicht | ✅ **Zusagen korrigiert (MF-526)**, 33 Hebungen offen |
| P0-13 | **Zwei Speicherfehler im Wandlungspfad** — die HFE-Spurtabelle ohne jede Schranke, und `lut[].length` (Gesamtlaenge beider Seiten) dreimal als Laenge je Seite verwendet: Absturz auf einer **gueltigen** Datei, 25080 Byte hinter dem Dateiende | ✅ **behoben (MF-526)** |
| P0-14 | **Die einzige Zusage ohne Zustimmung war unbelegt — und sie ist falsch.** `SCP<->HFE` stand als LOSSLESS; der jetzt existierende Bit-Identitaets-Test misst 25336 → 6400 Byte je Spur. Herabgestuft auf LOSSY_DOCUMENTED. **Folge: es gibt keine Wandlung mehr, die ohne Zustimmung laeuft** | ✅ **korrigiert (MF-527)**; offen: wo genau der Verlust entsteht |
| P0-4 | **POL-1: das Schreib-Sicherheitstor hat keinen Aufrufer.** Ein Tor, das nie läuft, ist eine Sicherheitszusage, die niemand einlöst | **offen** (braucht Hardware-Sitzung) |
| P0-5 | **LIC-1:** `uft_multiread_pipeline.c` trägt `SPDX: MIT`, dokumentiert sich aber als Nachbau von a8rawconvs `sift_sectors` (GPLv2+) | **offen** (Entscheidung des Eigentümers) |

---

## P1 — stille Verfälschung oder wartender Build-Bruch

| # | Punkt | Stand |
|---|---|---|
| P1-1 | `session->audit_entries++` auf einem `void *` — schob den Listenzeiger je Logzeile um ein Byte, statt `audit_count` zu zählen. Heap-Korruption im Audit-Pfad | ✅ **behoben (MF-507)** |
| P1-2 | `uft_td0_to_imd`: `struct uft_imd_image_t` nur in der Parameterliste deklariert → der Prototyp beschrieb einen Typ, den es nirgends gibt, also konnte die Typprüfung **keinen** Aufrufer prüfen | ✅ **behoben (MF-507)** |
| P1-3 | `GetTempPathA` ohne `<windows.h>` — implizite Deklaration, bricht unter GCC 14+/Clang | ✅ **behoben (MF-507)** |
| P1-4 | `uft_geos_protection.c:367`: `%d` mit `size_t` — verschiebt alle folgenden Argumente; `info->name` würde als Zeiger von der falschen Stelle gelesen | ✅ **behoben (MF-509)**; der ganze Baum ist jetzt frei von Formatfehlern |
| P1-5 | ARCH-3: 22 Banner-Header sind wirklich unfertig, der Skelett-Audit sieht sie nicht | **offen** |
| P1-6 | ARCH-21: 20 Altfälle von Header-Prototypen, die niemand einbindet | **offen** |
| P1-7 | ARCH-2/ARCH-4: 7 Header-Duplikate brauchen echte Zusammenführung. `UFT_SCP_SIGNATURE` ist **kein** Fall mehr — nachgemessen: zwei wertgleiche `#ifndef`-Defines, keine Abweichung | **offen** (verkleinert, MF-510) |
| P1-8 | **ARCH-26: 38 geteilte Include-Wächter mit abweichendem Inhalt** — zwei Header, ein `#ifndef`-Name, verschiedener Inhalt; die Include-Reihenfolge entscheidet still, welchen eine Übersetzungseinheit sieht. 14 davon verschlucken eine ganze Datei, 24 geben einem Typ zwei Layouts | **1 behoben, 37 offen; Tor gesetzt (MF-511)** |

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
