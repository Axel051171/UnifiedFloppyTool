# Gutachten: jfdelnero/AdfOpus_HxC
Stand: 2026-08-27 · Messung: `work/adfopus.messung.json`
(HEAD `bf82094caa`, letzter Commit 2017-01-22 „bak files removed.")
· Inventar: UFT HEAD `bb74f540`, `scout_inv5.json` (88 Plugins, SSOT ok)
· Zyklus 5, vom Eigentümer benannt. Gutachten von Hand nach Vorlage
(Ratenbremse in `gutachten.py` blockiert weiterhin, Befund Zyklus 4).

## Kategorie
**Oracle-Prüfung mit negativem Ergebnis + ein abgeleiteter
Hebungs-Vorschlag.** Aus diesem Repo ist nichts zu portieren; die Sichtung
hat aber einen belegten Weg ergeben, ein bestehendes UFT-Format (DMS, T3)
zu heben, und benennt den tatsächlichen Quellort für die MF-539-Lücke.

## Messwerte
- 1030 Dateien; .c=94, .h=69, Rest überwiegend Doxygen-HTML/Bilder
- Win32-GUI-Anwendung (MSVC .dsp-Projekte, WinAPI-Dialoge), Stand 2002,
  HxC-Anpassung 2013-2017 (jfdelnero)
- Vendorte Unterverzeichnisse: `ADFOpusSrc/ADFLib` (ADFlib 0.7.9d,
  `ADFLib/Lib/adf_defs.h:12`), `ADFOpusSrc/xdms` (xDMS 1.3, modifiziert:
  „Hacked to pieces by Dan Sutherland", `xdms/Xdms.c:2-6`),
  `ADFOpusSrc/Zlib`, dazu `ADFOpusSrc/fdi2raw.c` (UAE, Toni Wilen)
- `libhxcfe.dll` liegt **nur als Binärdatei** bei — kein
  libhxcfe-Quelltext in diesem Repo

## Lizenz (aus Dateien, je Unterverzeichnis — Handprüfung)
`vermessen.py` meldete **Zone ROT („keine LICENSE/COPYING-Datei
gefunden")** — das ist hier ein **Fehlurteil des Werkzeugs**: es sucht nur
auf oberster Ebene, die Lizenzdateien liegen in Unterverzeichnissen.
Handbefund:

| Unterverzeichnis | Beleg | Zone |
|---|---|---|
| `ADFOpusSrc/*.c` (ADFOpus selbst) | GPL-2.0-or-later, voller GPL-Block im Datei-Header `ADFOpus.c:13-26`; GPL-2-Volltext in `Help/license.rtf`, `Installers/Presetup/license.txt` | GRÜN |
| `ADFOpusSrc/ADFLib/` | `ADFLib/Docs/license.txt` = GPL-2.0-Volltext; `Lib/adflib.h:123-124` verweist darauf ohne „or later" → konservativ GPL-2.0-only | GRÜN |
| `ADFOpusSrc/fdi2raw.c` | GPL-2.0-or-later im Datei-Header (Zeilen 13-16, Toni Wilen 2001) | GRÜN |
| `ADFOpusSrc/xdms/` | KEINE Lizenzdatei im Unterverzeichnis; nur PD-Behauptung im Datei-Header `Xdms.c:3` („Public Domain") und in `ADFOpus.c:10`. `xdms.txt` enthält keinen Lizenzabschnitt | **PRÜFEN** (Datei-Header ≠ Lizenzdatei) |
| `ADFOpusSrc/Zlib/` | nicht geprüft (für die Funde irrelevant) | UNGEKLÄRT |
| `libhxcfe.dll` | Binärdatei, keine Quelle im Repo — nichts zu lizenzieren, nichts zu portieren | — |

Konsequenz: portierbar wäre aus ADFOpus/ADFLib/fdi2raw formal alles
(GRÜN) — es gibt nur **nichts, was UFT fehlt** (unten belegt).

## Befund je Einhängepunkt des Auftrags

### 1. AmigaDOS-Encoder (MF-539-Lücke) — NICHT in diesem Repo
Die gesamte MFM-Arbeit läuft über die **Binär-DLL**:
- `BatchConvert.c:273` — `hxcfe_readSectorData(..., AMIGA_MFM_ENCODING, ...)`
- `BatchConvert.c:435`, `ChildCommon.c:341`, `New.c:352` —
  `hxcfe_imgExport(..., "HXC_HFE")` (ADF→HFE)

Im Quelltext des Repos existiert **kein** AmigaDOS-MFM-Encoder:
ADFlib arbeitet ausschließlich auf Sektor-/Blockebene (ADF-Dumps,
`adf_dump.c`, `adf_raw.c` = Bootblock/Root, kein MFM); `fdi2raw.c` ist
ein **Decoder** (FDI→Raw-Bitstream), kein Encoder. Der eigentliche
Quellort des Encoders ist `jfdelnero/HxCFloppyEmulator` (libhxcfe) —
**anderes Repo, Lizenz dort aus deren Dateien zu bestimmen** (Vorschlag 3).
Zweiter Kandidat: `keirf/disk-utilities` (libdisk), in der Negativliste
bereits als „bewertet — Referenz-Oracle libdisk, Amiga-Decoder" geführt;
ob dort auch die **Encoder**-Seite liegt, ist ungemessen.

### 2. Drittes AmigaDOS-Oracle für PLAN_v4.1.7 Phase 1 — UNTAUGLICH
Zwei unabhängige Ausschlussgründe, beide gemessen:
1. **Nicht skriptbar.** Reine Win32-GUI: der Batch-Konverter ist ein
   Dialog (`BatchConvert.c:30` `BatchConvertProc(HWND dlg, ...)`,
   `:151` `void BCConvert(HWND dlg)`), es gibt keine Kommandozeile.
2. **Nicht unabhängig.** Die AmigaDOS-Engine IST ADFlib 0.7.9d — dieselbe
   Bibliothek, aus der `unadf` stammt (`ADFLib/Demo/unadf.c` liegt im
   selben Baum; `ADFLib/README`: „unADF is a unzip like for .ADF files").
   Der Plan warnt in `docs/PLAN_v4.1.7.md:122-127` genau vor Oracle und
   Korpus-Erzeuger aus derselben Hand — AdfOpus wäre dieselbe Messung mit
   demselben Messgerät wie das bereits benannte `unadf`.

Ein wirklich drittes, unabhängiges Werkzeug bleibt zu finden (Kandidaten
mit **eigener** AmigaDOS-Implementierung, z. B. FS-Code aus
disk-utilities oder ein Emulator-Mount; nicht hier belegbar).

### 3. Vendoring-Verdacht — BESTÄTIGT
ADFlib 0.7.9d vollständig vendort unter `ADFOpusSrc/ADFLib`
(`adf_defs.h:12` `ADFLIB_VERSION "0.7.9d"`, © 1997-2002 Laurent Clévy).
Lizenz aus DEREN Datei bestimmt (GPL-2.0, s. o.), nicht aus dem Wrapper.
Beifang: `ADFLib/Faq/adf_info.html` ist genau die Spec, die UFT bereits
als AmigaDOS-Referenz zitiert (`src/fs/uft_amigados.c:19`).

## Inventar-Abgleich (Abfragen zitiert)
- `DMS` → `{"vorhanden": true, "treffer": ["dms","uft_dms"], "tier": "T3", "plugin_liste_vollstaendig": true}`
- `ADZ` → `{"vorhanden": true, "treffer": ["adz"]}`
- `FDI` → `{"vorhanden": true, "treffer": ["fdi","fdi_pc98","uft_fdi"], "tier": "T1"}`
- `HFE` → `{"vorhanden": true, "tier": "T1b"}` · `ADF` → `{"vorhanden": true, "tier": "T1b"}`
- `amigados` → `{"vorhanden": true, "treffer": ["amiga"]}`
- `adflib`, `unadf`, `xdms` → `{"abgedeckt": false}` → **von Hand geprüft:**
  - ADFlib ist in UFT **nicht** vendort; UFT hat eine eigene
    AmigaDOS-Implementierung (`src/fs/uft_amigados.c`), die die
    ADFlib-Spec zitiert (Zeile 19, 521).
  - xDMS ist in UFT **bereits als Port vorhanden**:
    `src/formats/dms/uft_dms.c:5` — „Based on xDMS 1.3 by Andre Rodrigues
    de la Rocha (Public Domain)", alle vier Modi (Quick/Medium/Deep/Heavy).
  - Joguin-FDI ebenfalls vorhanden (`include/uft/uft_formats_extended.h:226`
    `FDI_MAGIC "Formatted Disk Image file\r\n"`, `src/formats/misc/fdi.c`)
    → `fdi2raw.c` wäre eine Dublette, kein Fund.
- Korpus (22 Einträge geprüft): **kein DMS-Abbild vorhanden** — die
  Beschaffungsliste unten fordert nichts an, was schon liegt.

## Abgeleiteter Fund: DMS liegt auf T3 mit rein synthetischem Test
- `src/formats/dms/uft_dms.c` (1008 Z.) + `uft_dms_plugin.c` (932 Z.):
  xDMS-1.3-Port, **kein SPDX-Header**, PD-Herkunft nur als Satz.
- `tests/test_uft_dms.c:7-9`: „Since we don't have real DMS files here,
  we construct minimal valid DMS structures in memory" — exakt die
  T3-Definition (synthetisch, ohne autoritative Quelle).
- Das Original-xDMS 1.3 ist paketiert (Debian `xdms`) und damit ein
  baubares, skriptbares Oracle. **Nicht** die Kopie aus diesem Repo
  verwenden — die ist modifiziert („Hacked to pieces", `Xdms.c:5`).

### Differenzlauf-Plan (Pflicht, da Hebung mit Oracle)
- **Binaries:** `xdms` 1.3 (Debian-Paket oder Quellbau aus
  Original-Distribution, Version dokumentieren) vs. UFT-`uft_dms`
  (Testtreiber über die Kernbibliothek, kein CLI — UFT ist GUI-only).
- **Korpus:** ≥1 reale historische DMS-Datei mit Provenienz (frei
  verteilbares Amiga-Scene-Demo, z. B. scene.org/Aminet), bevorzugt eine
  je Kompressionsmodus (Quick/Medium/Deep/Heavy), SHA-256-Manifest.
- **Metrik:** entpackte ADF byteidentisch (SHA-256 gleich); Track-CRCs
  beider Wege identisch gemeldet.
- **Toleranzliste:** leer — Dekompression ist deterministisch; jede
  Abweichung ist ein Befund.

## OPEN_ITEMS-Vorschläge (3 von max. 5, nach Priorität)

> Vorschlagsblock — Übernahme nach `docs/OPEN_ITEMS.md` durch einen
> Menschen; ein Vorschlag ist kein Eintrag.

1. **DMS von T3 heben (T1b-Weg) via xDMS-Differenzlauf.**
   `uft_dms.c` ist ein 1940-Zeilen-Port ohne Abgleich gegen die
   autoritative Quelle (`tests/test_uft_dms.c:7-9`, Tier-Liste: T3).
   Moratoriumskonform: Hebung, kein neues Format.
   *Einfrier-konformer Weg:* benannte Referenz = xDMS 1.3
   (Original-Distribution + Debian-Paket); Rotbeweis zuerst = Test gegen
   reale DMS-Datei muss heute mangels Fixture skippen (rc 77) bzw. bei
   verfälschtem Track-CRC am realen Abbild fallen; Referenz im Header =
   `uft_dms.c`-Kopf um Oracle-Version + Korpus-Manifest ergänzen.
   Einhängepunkt: Moratoriums-Rückstandsregel (VERIFICATION_PLAN
   §Einfrier-Regel, „Hebungen"); Aufwand **S** (nach Beschaffung).
2. **SPDX/Herkunft von `uft_dms.c` dokumentieren (Eigentümer-Vorlage).**
   Kein SPDX-Tag (gemessen: `grep -i spdx src/formats/dms/` leer); die
   PD-Behauptung stützt sich allein auf den xDMS-Datei-Header — dieselbe
   Fehlerklasse wie P0-5/LIC-1. Grenzfall nach Lizenzmatrix Zeile PRÜFEN
   (Datei-Header ≠ Lizenzdatei) → Entscheidung beim Eigentümer; Debians
   `xdms`-copyright-Datei als Zweitbeleg benennen. Aufwand **S**.
3. **Scout-Folgeziel für MF-539 (AmigaDOS-Encoder):**
   `jfdelnero/HxCFloppyEmulator` (libhxcfe-Quelltext — Amiga-MFM-Encoder
   und HFE-Writer aus einem Haus; dieses Gutachten belegt, dass AdfOpus
   ihn nur als DLL aufruft) sowie Nachmessung von `keirf/disk-utilities`
   gezielt auf die **Encoder**-Seite (Negativliste kennt bisher nur
   „Amiga-Decoder"). Lizenzzone jeweils erst dort aus deren Dateien.
   *Einfrier-konformer Weg (für Stufe 4, egal welche Quelle):* Referenz =
   ADFlib-FAQ `adf_info` (bereits UFT-Referenz, `uft_amigados.c:19`) +
   gewählte Quell-Implementierung mit Commit; Rotbeweis = der bestehende
   MF-539-Ablehnpfad `ADF→HFE` wird erst nach bitidentischem
   Rundlauf (ADF→HFE→ADF, Matrix-Eintrag in `uft_roundtrip.c`) geöffnet;
   Referenz im Header des neuen Encoders. Aufwand Scout-Zyklus **S**.

## Beschaffungsliste
- 1-4 reale DMS-Dateien mit Provenienz (frei verteilbare Scene-Demos;
  ideal je Kompressionsmodus eine) + SHA-256-Manifest — Korpus hat keine.
- `xdms`-Binary 1.3 (Debian-Paket, im CI installierbar; lokal optional).
- Nichts weiter — HFE/ADF/SCP-Amiga-Fixtures liegen bereits
  (`gw_amigados.hfe`, `gw_amigados.scp`, `xdftool_dd_ofs.adf`).

## UNGEKLÄRT
- Formaler PD-Status von xDMS 1.3 (keine Lizenzdatei in der
  Distribution; PD nur als Header-Satz) — Eigentümer-Vorlage, s. Vorschlag 2.
- Lizenz von `ADFOpusSrc/Zlib/` (nicht geprüft, kein Fund daran geknüpft).
- Lizenz von libhxcfe (Quelle nicht in diesem Repo) — im Folgeziel messen.
- Ob `keirf/disk-utilities` einen AmigaDOS-**Encoder** enthält (bisher
  nur „Decoder" belegt) — im Folgeziel messen.
- Ob UFTs Joguin-FDI-Pfad (`src/formats/misc/fdi.c`) je verifiziert wurde
  (das T1 der Tier-Liste hängt am Spectrum-FDI-Korpus `zxart_spectrofon01.fdi`,
  einer **anderen** FDI-Bedeutung) — außerhalb dieses Zyklus, notiert.

## Werkzeug-Befunde dieses Zyklus (zusätzlich zu den drei offenen aus Zyklus 4)
1. **`vermessen.py` Zone-Fehlurteil ROT:** sucht LICENSE/COPYING nur auf
   oberster Ebene; hier liegen drei GPL-2-Lizenzdateien in
   Unterverzeichnissen → „ROT" statt GRÜN. Die Lizenzmatrix verlangt
   Prüfung **je Unterverzeichnis** — das Werkzeug kann das nicht.
2. **`vermessen.py` schreibt `work/` relativ zum cwd**, nicht relativ zu
   `tools/uft-scout/` — die Messung landete in `<uft-root>/work/` und
   musste von Hand verschoben werden.
3. **`vermessen.py --help` stürzt ab** (behandelt `--help` als Pfad,
   unbehandelte Exception in `subprocess.run`).
4. `gutachten.py`-Ratenbremse blockiert weiterhin (Zyklus-4-Befund,
   unverändert) — dieses Gutachten daher von Hand.

## Regeln, die für diesen Fund gelten
- Kein Code aus diesem Agenten (AGENT.md Regel 1); Vorschlag 1/3 laufen
  durch Stufe 4 mit Einfrier-Regel.
- Zone GRÜN für ADFOpus/ADFLib/fdi2raw belegt, aber ohne Portier-Bedarf;
  xdms-Verzeichnis Zone PRÜFEN → Eigentümer-Vorlage.
