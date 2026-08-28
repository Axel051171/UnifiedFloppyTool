# Gutachten: pbakota/amigadx
<!-- uebernommen: MF-639 (Commit 778fd307 — Ringschluss-Fix beider Ketten in src/fs/uft_amigados.c + tests/test_amigados_cycle.c; exakt der Gutachten-Befund SCOUT-20) -->

- **Quelle:** https://github.com/pbakota/amigadx.git, HEAD `165ed3e` (2014-07-18, „Update README.md")
- **Gemessen:** 2026-08-28, `work/amigadx.messung.json` (123 Dateien, Zone ROT, 14 Domänen-Begriffe)
- **Inventar:** `work/inv.json` (SSOT ok, 88 Plugins, HEAD `cf5fa96f`)
- **Neubesuch-Anlass:** entfällt — Erstbesuch, kein Eintrag in `data/known_negatives.json` (geprüft: `grep amigadx\|pbakota` = 0 Treffer)
- **Ratenbremse:** `gutachten.py` hat den mechanischen Entwurf **verweigert** — 5 Gutachten in `out/` ohne Übernahme-Marke (adfopus_hxc, hxcfe_amiga_copy_utility, nibtools, sector-cpc, superdiskindex). Dieses Gutachten ist die Stufe-3-Tiefenprüfung von Hand; die 2 Vorschläge unten stehen ausdrücklich **hinter** dem bestehenden Rückstand.

## Was es ist

Ein **Total-Commander-Packer-Plugin (WCX-DLL)** von 2002–2007 (Quellfreigabe 2013),
das ADF (rw), ADZ (ro), DMS (ro) und HDF (rw) im Dateimanager öffnet.
Eigener Code: **1708 Zeilen** (Methode: `wc -l src/*.c src/*.h`, 11 Dateien,
`lib/` ausgenommen). Der gesamte Dateisystem-Teil ist **vendorte ADFlib**
(`lib/adflib/`, 0.7.10 laut Changelog `src/AmigaDX.c:22`) mit kleinen
eigenen Patches (Marker `/* BV */`). Dazu vendort: `lib/xdmslib/` (xDMS,
DMS-Dekompression) und `lib/zlib/` (1.2.3, für ADZ=gzip-ADF).

## Die wichtigste Einzelfrage: ADFlib-unabhängige ADF-Zweitmeinung?

**Nein, zweifach.**

1. **Gleiche Hand.** `lib/adflib/adf_dir.c:2` „(C) 1997-2002 Laurent Clevy …
   This file is part of ADFLib". amigadx ist derselbe Motor wie `unadf` und
   AdfOpus — exakt der Ausschlussgrund, den `docs/PLAN_v4.1.7.md:129-132`
   für AdfOpus bereits festgehalten hat.
2. **Nicht skriptbar.** Es ist eine Windows-DLL mit WCX-API
   (`src/wcxapi.c`, `src/wcxhead.h` — OpenArchive/ReadHeader/ProcessFile),
   Wirt ist Total Commander. Kein Konsolen-Einstieg, kein hardwarefreier
   Standalone-Lauf.

**Konsequenz:** Als ADF-Oracle **verworfen**. Die Suche nach einer von
ADFlib unabhängigen ADF-Zweitmeinung bleibt **offen** — amigadx schließt
diese Lücke nicht und kein Fund dieses Zyklus tut es.

## Lizenz (aus Dateien, je Unterverzeichnis — Handprüfung)

`vermessen.py`: Zone **ROT** — „keine LICENSE/COPYING-Datei gefunden =
alle Rechte vorbehalten". Handprüfung je Ebene:

| Bereich | Beleg | Zone |
|---|---|---|
| Repo-Wurzel | keine LICENSE/COPYING (geprüft: `find . -iname "*license*" -o -iname "*copying*"` → nur `readme_win32.html`, `README.md`) | **ROT** |
| `src/` (eigener Code) | nur „Copyright (C) 2002 Peter Bakota" (`AmigaDX.c:2`), kein Grant | **ROT** |
| `lib/adflib/` | Datei-Header GPL-2.0-or-later (`adf_dir.c:11-15`), keine COPYING im Verzeichnis | GRÜN dem Header nach — irrelevant: ADFlib nehmen wir nicht (gleiche-Hand-Problem) |
| `lib/xdmslib/` | `xdms.txt:273-279` „released as public domain software" | PD — identische Codelinie wie unser `src/formats/dms/uft_dms.c` (dort belegt, MF-614) |
| `lib/zlib/` | zlib-Lizenz im Header `zlib.h:1-20` (1.2.3, 2005) | GRÜN — irrelevant, UFT braucht kein zweites zlib |

**Konsequenz Zone ROT für den eigenen Code:** kein Port, kein Vendoring.
Maximum wäre Verhaltens-Spec/Blackbox — und selbst das lohnt nicht, weil
das Verhalten ADFlib ist.

### Attributionen (Pflichtfeld)

| Fundstelle | Wortlaut | Lizenz der Quelle |
|---|---|---|
| `src/support.c:10` | „This routines is converted from ADFOpus" | ADF Opus = GPL-2.0-or-later (belegt in `out/adfopus_hxc.gutachten.md`, dort aus `ADFOpus.c:13-26`) — d. h. `support.c` ist mutmaßlich GPL-abgeleitet, ohne dass das Repo es ausweist |
| `src/wcxapi.h:11` | „This code based on Christian Ghisler (support@ghisler.com) sources" | WCX-SDK-Beispielcode, **keine Lizenz genannt** — ungeklärt |
| `lib/adflib/*` Header | „This file is part of ADFLib" | GPL-2.0-or-later (Datei-Header) |
| `lib/xdmslib/xdms.txt:273-279` | PD-Erklärung des xDMS-Autors | Public Domain (formlos) |

## Inventar-Abfragen (zitiert)

```
"DMS": { "vorhanden": true, "treffer": ["dms","uft_dms"], "tier": "T3" }
"ADZ": { "vorhanden": true, "treffer": ["adz"] }
"HDF": { "vorhanden": true, "treffer": ["hdf"] }
"ADF": { "vorhanden": true, "treffer": ["adf","adf-copy","adf_adl","adf_arc","adf_ext","extadf","uft_adf"], "tier": "T1b" }
"xdms" / "bootblock" / "salvage" / "undelete": { "abgedeckt": false, hinweis: "von Hand im Baum pruefen" }
```

Starke Treffer DMS/ADZ/HDF/ADF → als Format-Funde **verworfen** (Regel 4).
Die `abgedeckt:false`-Begriffe wurden von Hand geprüft, Ergebnis unten.

## Befunde

### F1 — Oracle-Frage negativ beantwortet (Kategorie: irrelevant als Oracle)
Siehe oben. Wert des Zyklus: die Negativantwort ist jetzt belegt, samt
dem Muster „Amiga-Freeware-Ökosystem = fast immer ADFlib darunter".

### F2 — UFT weist reale 81–83-Zylinder-ADFs ab (Kategorie: Verbesserung, S)

**Fremdbeleg:** amigadx hat 2004/2005 auf Benutzermeldungen hin Über-
größen-ADFs nachgerüstet — Changelog `src/AmigaDX.c:20` („non standard
81 cylinder floppy", 18.08.2004) und `:24` („up to 83 cylinder",
22.06.2005); Umsetzung im ADFlib-Patch `lib/adflib/adf_hd.c:56-59`:
akzeptiert `512*11*2*{80,81,82,83}` (Marker `/* BV */`). Solche Abbilder
existieren also im Feld — Amiga-Laufwerke konnten 81–83 Spuren schreiben.

**UFT-Messung (Handprüfung, alle vier ADF-Leser):**
- `src/formats/adf/uft_adf_plugin.c:19` — `file_size != 901120 && != 1802240 → false` (das registrierte Plugin)
- `src/formats/misc/adf.c:28` — nur `80*2*11*512`
- `src/formats/uft_adf.c:260-261` — nur DD_SIZE/HD_SIZE exakt
- `src/formats/adf_ext/uft_adf_ext.c` — anderes Format (UAE-1ADF), fängt Roh-ADFs nicht

**Folge:** Ein 912 384-Byte-ADF (81 Zyl. DD) liefert aus `uft_disk_open()`
NULL — eine reale Dateiklasse wird kommentarlos nicht geöffnet.

**Stufe-4-Weg (einfrier-konform, Bugfix an Bestehendem):**
1. **Rotbeweis zuerst:** Test, der ein 912 384-Byte-AmigaDOS-Abbild an
   `uft_disk_open()` gibt und heute NULL erhält.
2. **Benannte Referenz:** WinUAE `disk.cpp` (Präzedenz im Baum:
   `uft_adf_ext.c` ist bereits „verified byte-for-byte against the WinUAE
   source, disk.cpp"). Dort die akzeptierten Roh-ADF-Größen nachschlagen —
   ob 81–83 die richtige Obergrenze ist oder WinUAE die Spurzahl aus der
   Größe ableitet, ist UNGEKLÄRT-Punkt 1 und muss VOR dem Code beantwortet
   sein. ADFlib-Upstream als Zweitreferenz.
3. Referenz in den Header, jede Größe gemessen.

**Einhängepunkt:** `docs/PLAN_v4.1.7.md` ADF-Schiene (adf steht T1b,
Korpus liegt); Einfrier-Regel erlaubt Bugfixes an bestehenden Formaten.

### F3 — Zyklus-Schutz: im Baum vorhanden, im benutzten Walker fehlend (Kategorie: Verbesserung, S — „Können im Baum, Zugang fehlt")

**Fremdbeleg (nur als Existenznachweis der Fehlerklasse):** amigadx-Changelog
`src/AmigaDX.c:27` (26.06.2005): „Maked workaround for circular references
on disk" — zirkuläre Verkettungen auf realen/korrupten Amiga-Disketten
sind eine 2005 dokumentierte Feldbedingung. (Wo der Workaround im Code
liegt, war nicht auffindbar — UNGEKLÄRT-Punkt 4; die UFT-Seite trägt den
Befund allein.)

**UFT-Messung:** Der Baum hat den Schutz bereits — aber nur im einen von
zwei Walkern:
- `src/fs/uft_amigados_extended.c:63,82-129` — Visited-Bitmap (1 Bit je
  Block) + Tiefendeckel 32, Zykluszähler. Vorbildlich.
- `src/fs/uft_amigados.c:406-417` (Hash-Ketten-Walker) — bricht nur bei
  **Selbst**-Schleife (`e.hash_chain == head`); ein 2er-Zyklus A→B→A läuft
  endlos und `dir_append` realloziert unbegrenzt.
- `src/fs/uft_amigados.c:518-565` (Extension-Ketten-Walker) — dieselbe
  Lage: nur `next == cur_hdr` bricht (Zeile 562).

Der registrierte FS-Treiber (`src/fs/uft_fs_amigados_driver.c:3` „wraps
uft_amiga_*") sitzt auf dem **ungeschützten** Basis-Walker.

**Stufe-4-Weg:** Rotbeweis zuerst — synthetisches ADF mit A→B→A-Hash-Kette
bzw. Extension-Zyklus, Test muss heute hängen/OOM (mit Timeout-Harness).
Fix = das **baumeigene** Muster aus `uft_amigados_extended.c` (benannte
Referenz im eigenen Baum) auf beide Schleifen anwenden. Kein fremder Code
nötig.

**Einhängepunkt:** Fuzz-Schiene `docs/OPEN_ITEMS.md` (Fuzzer deckt
`uft_disk_open()`, die FS-Schicht ist dort nicht erfasst); Nachbarschaft
KI-7.4 (ADF-Schreibseite, v4.2).

### F4 — amigadx' Nutzwert = UFTs unverdrahtete FS-Schicht (Kategorie: Bestätigung, kein neuer Vorschlag)

Der gesamte Gebrauchswert von amigadx (ADF im Dateimanager durchblättern
und entpacken) existiert in UFT als Code ohne Zugang: `explorertab.cpp:330-377`
(MF-569) zeigt „(no directory listing - filesystem reading is not wired)";
`uft_amiga_foreach_entry()` hat dort gemessen **0 Aufrufer**. Heute
nachgemessen: die FS-Registry (`src/core/uft_fs_registry.c`) exportiert
`uft_fs_driver_get`, `uft_fs_mount_auto`, `uft_fs_driver_count` — Suche
über `src/`, `include/`, `tests/` (alle `.c*`/`.h`, Registry-Datei selbst
ausgenommen) ergibt **genau einen** Treffer, den Prototyp
`include/uft/uft_integration.h:320`. **Null Aufrufstellen.** Ein
Freeware-Plugin von 2002 liefert Endnutzern, was unser Baum hält, aber
nicht anschließt. Bereits getrackt: `docs/MASTER_PLAN.md:379` („GUI
features ohne Backend: explorertab …") — deshalb **kein** neuer
OPEN_ITEMS-Eintrag, nur diese Bestätigung mit Messung. Wer verdrahtet:
F3 **vorher** fixen (der Walker, der dann läuft, ist der ungeschützte).

### Verworfen (mit Grund)

| Fund | Grund |
|---|---|
| DMS/ADZ/HDF-Lesen | starke Inventar-Treffer (oben zitiert); UFTs DMS ist zudem In-Memory-Port derselben xDMS-Linie — amigadx' Temp-Datei-Umweg (`support.c:76-99`) ist der schlechtere Weg |
| xdmslib als DMS-Zweitmeinung | gleiche Hand wie `uft_dms.c` (beides xDMS); SCOUT-11 braucht das **Original**-xDMS als Oracle, das ist bereits benannt |
| Bootblock-Installation / „make bootable" (`src/bbk.c`, `adfInstallBootBlock` `AmigaDX.c:783`) | Schreibseite; UFTs ADF-Schreibstubs sind KI-7.4 (v4.2, STUB_ELIMINATION Phase 5) — dort einsortiert, kein eigener Posten. `res/Boot/stdboot3.bbk` ist Amiga-OS-Bootcode, Rechte nicht beim Repo-Autor (UNGEKLÄRT 3) |
| Erkennungslogik | reine Endungs-Erkennung (`support.c:34-112`) — UFT sondiert inhaltsbasiert, nichts zu lernen |
| ADFlib-Salvage (`adf_salv.c`) | liegt zwar im Vendoring, amigadx **ruft es nicht** (geprüft: `grep adfUndel\|Salv src/*.c` = 0); für Salvage-Konzepte ist ADFlib-Upstream die Adresse, nicht dieses Repo |

## Differenzlauf-Plan

Entfällt — dieses Gutachten behauptet an keiner Stelle Überlegenheit
eines amigadx-Verfahrens (Regel 7 greift nicht).

## Beschaffungsliste

Gegen `inv["korpus"]` geprüft (liegen: `xdftool_dd_ofs.adf`,
`gw_amigados.hfe` u. a. — 22 Abbilder):

| Was | Wofür | Weg | Status |
|---|---|---|---|
| 81-Zylinder-DD-ADF (912 384 B), AmigaDOS-formatiert | F2-Rotbeweis + Fixture | bevorzugt cross-tool: prüfen, ob `xdftool` (bereits Korpus-Erzeuger) freie Geometrie kann; sonst WinUAE/FS-UAE-erzeugt mit Provenienz | **fehlt**, UNGEKLÄRT 2 |
| Zyklus-ADF (A→B→A-Hash-Kette) | F3-Rotbeweis | synthetisch im Test erzeugen (Standard-ADF + 8 Byte patchen + Prüfsummen) — keine Beschaffung | — |

Aus amigadx selbst ist **nichts** zu beschaffen: keine ADF-Testdateien im
Repo (einzige Binärdaten: `res/Boot/stdboot3.bbk`, `bin/premake4.exe`).

## UNGEKLÄRT

1. Akzeptiert WinUAE (`disk.cpp`) genau 80–83 Zylinder für Roh-ADF, oder
   leitet es die Spurzahl aus der Dateigröße ab? Muss vor F2-Implementierung
   aus der WinUAE-Quelle beantwortet werden (Referenzpflicht).
2. Kann `xdftool` ein 81-Zylinder-ADF erzeugen/formatieren? Entscheidet
   den Fixture-Weg für F2.
3. Rechtestatus von `res/Boot/stdboot3.bbk` (Amiga-OS-Bootblock, 68k) —
   nur relevant, falls je ein Standard-Bootblock-Referenzhash gewünscht
   wird; nicht weiterverfolgt.
4. Wo amigadx' „circular references workaround" (Changelog 2005) im Code
   liegt — nicht auffindbar (`grep -i circular` trifft nur den Changelog).
   Für F3 ohne Belang: der Befund ruht auf UFT-seitiger Messung.
5. Lizenzstatus von `src/support.c` (ADFOpus-Konvertit in unlizenziertem
   Repo — GPL-Ableitung ohne Ausweis). Für uns folgenlos, da nichts
   übernommen wird.

## OPEN_ITEMS-Vorschläge (2 von max. 5 — hinter der Ratenbremse)

> Die Ratenbremse meldet 5 unverarbeitete Gutachten in `out/`. Diese zwei
> Vorschläge sind übernahmefertig formuliert, stehen aber ausdrücklich
> **nach** dem Abarbeiten des Rückstands.

```
| SCOUT-19 | **Alle vier ADF-Leser weisen reale 81–83-Zylinder-ADFs ab.** Sonden nur für exakt 901120/1802240 B (`src/formats/adf/uft_adf_plugin.c:19`, `misc/adf.c:28`, `uft_adf.c:260-261`); ein 912384-B-Abbild (81 Zyl. DD) liefert aus `uft_disk_open()` NULL. Feldbeleg: amigadx rüstete 2004/2005 auf Benutzermeldung 81–83 Zyl. nach (`AmigaDX.c:20,24`; `adf_hd.c:56-59`: `512*11*2*{80..83}`). Weg: Rotbeweis (912384-B-Abbild → heute NULL), Referenz WinUAE `disk.cpp` (Präzedenz: `uft_adf_ext.c`-Header), dann Sonden erweitern | offen |
| SCOUT-20 | **Zyklus-Schutz existiert im Baum, fehlt aber im benutzten AmigaDOS-Walker.** `uft_amigados_extended.c:82-129` hat Visited-Bitmap + Tiefendeckel; der Basis-Walker, auf dem der registrierte FS-Treiber sitzt, bricht nur bei Selbst-Schleifen — ein A→B→A-Zyklus läuft endlos mit unbegrenztem realloc (`uft_amigados.c:406-417` Hash-Kette, `:518-565` Extension-Kette). Feldbedingung 2005 dokumentiert (amigadx `AmigaDX.c:27`). Weg: Rotbeweis (synthetisches Zyklus-ADF, Test hängt), dann das baumeigene Bitmap-Muster auf beide Schleifen. Vor jeder FS-Verdrahtung (MF-569/MASTER_PLAN:379) zu erledigen | offen |
```

## Urteil

amigadx selbst: **bewertet, nichts zu portieren** — Zone ROT, Motor ist
ADFlib (gleiche Hand wie unadf/AdfOpus), Nutzwert-Delta gegenüber UFT
liegt in zwei kleinen, UFT-seitig zu schließenden Lücken (F2, F3) und
einer Bestätigung des bekannten Verdrahtungs-Rückstands (F4). Als Oracle
untauglich (WCX-DLL, nicht skriptbar, ADFlib-abhängig). Die Suche nach
einer ADFlib-unabhängigen ADF-Zweitmeinung bleibt offen.
