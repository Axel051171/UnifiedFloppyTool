# Gutachten: Beschaffung von Fluss-Aufnahmen für den UFT-Korpus

**Quelle 1:** archive.org, Konto `@floppyarchaeology` (1742 Objekte)
**Quelle 2:** archive.org-weite Suche nach FM-/SD-Flussabzügen
**Datum:** 2026-09-04 · **Inventar:** `work/inv.json` auf HEAD `274c81a8`
**Kategorie:** Daten (Korpus-Beschaffung) · **Aufwandsklasse:** S je Fund
**Auftrag:** P3-123c (FM-Flussabzug) zuerst; danach T3-Hebungen

---

## 0. Auflagen-Nachweis: gesichtet, nicht heruntergeladen

Die Auflage des Eigentümers („NICHT alle herunterladen") wurde eingehalten.
Geholt wurden ausschließlich:

| was | Größe | Zweck |
|---|---|---|
| 18 Seiten Katalog-Metadaten (Such-API) | ~1,7 MB JSON | Katalogisierung aller 1742 Objekte |
| 10 Objekt-Metadatensätze (`/metadata/<id>`) | je ~50 KB | Datei-Listen, md5, Rechte-Felder |
| 1 Imaging-Log (`.txt`) | 2,6 KB | Encoding-Nachweis DJ2D |
| 2 IMD-Dateien | 0,40 + 0,21 MB | FM/MFM-Modus-Auszählung |

Kein Disk-Image über 0,5 MB wurde übertragen. Alle Zwischenstände liegen im
Sitzungs-Scratchpad, nichts davon im Repo.

## 1. Methode (jede Zahl mit Verfahren, MF-615)

* **1742 Objekte:** archive.org `page_production`-API, Feld
  `uploads.hits.total`, abgefragt 2026-09-04. Verteilung aus derselben
  Antwort (Aggregation `mediatype`): software 1521, texts 207, movies 12,
  data 1, image 1.
* **64 Objekte mit Fluss-Bezug:** Regex
  `scp|kryoflux|a2r|applesauce|\.raw|flux|greaseweazle|hfe|woz`
  (case-insensitive) über die Felder title+description+subject aller 1742
  Katalog-Einträge. Treffer zählt, wenn der Begriff als Wort vorkommt.
  Untergrenze — Objekte, deren Beschreibung die Beigabe nicht nennt,
  fallen durch.
* **FM/MFM-Modi:** IMD-1.18-Spurköpfe (5 Byte: Mode, Cyl, Head, nSec,
  SSize) über die ganze Datei ausgezählt; Mode-Byte 0=FM 500k, 3=MFM 500k,
  5=MFM 250k nach IMD-Spezifikation.
* **Dateigrößen/md5:** archive.org-Metadata-API des jeweiligen Objekts.
* **T3-Raster:** Regex-Stichworte für alle 37 T3-Formate
  (`docs/VERIFICATION_TIERS.md` Z. 72–108) gegen die 1742 Katalogtitel +
  Beschreibungsanfänge. Ergebnis: **kein einziger** direkter T3-Titel-
  Treffer (die scheinbaren Treffer waren Rauschen: „Spectrum HoloByte",
  „Tandy 2000"). Der T3-Hebel dieser Sammlung läuft ausschließlich über
  Inhalte (CP/M-Dateisystem), nicht über Container.

## 2. Was das Inventar sagt (Abfragen zitiert)

`inventar.py query` gegen `work/inv.json` (Auszug, wörtlich):

```
"a2r":        { "vorhanden": true,  "tier": null, "plugin_liste_vollstaendig": true }
"atx":        { "vorhanden": true,  "tier": "T2" }
"mfi":        { "vorhanden": true,  "tier": "T2" }
"imd":        { "vorhanden": true,  "tier": "T2" }
"hardsector": { "vorhanden": true,  "tier": "T3" }
```

Korpus-Abgleich (`inv["korpus"]`, 40 Einträge): es liegt **genau ein**
SCP (`tests/corpus/gw_amigados.scp`, cross-tool **erzeugt**, AmigaDOS =
MFM) und **kein** A2R, **kein** ATX, **kein** MFI, **kein** reales
Fluss-Abbild mit FM-Inhalt. P3-123c ist damit gegen das Inventar
bestätigt, nicht nur gegen die Doku.

Drei Baum-Messungen, die die Auswahl steuern:

1. **`atx` T2** trägt den Vermerk „NICHT verifiziert: Verhalten an einem
   realen ATX-Abbild — keines im Korpus, daher T2 und nicht T1"
   (`docs/VERIFICATION_TIERS.md` Z. 54).
2. **`mfi` T2** trägt den Vermerk „dafür braucht es eine reale Datei, und
   im Korpus liegt keine. Deshalb T2 und nicht T1" (ebd. Z. 66).
3. **A2R ist eine Tür ohne Leser** (MF-726,
   `tests/test_apple_moof_a2r_no_door.c`): `uft_a2r_parser.c` (1111
   Zeilen) hat 0 Aufrufer, keine Plugin-Struktur, keinen Registry-Weg.
   **Konsequenz für jeden A2R-Fund:** die Messung läuft über eine
   Transkodierung A2R→SCP mit einem bereits gepinnten fremden Werkzeug
   (Kanal „Oracle": gw 1.23 oder hxcfe, beide im Korpus-Werkzeugkasten) —
   SCP ist T1b und im Flusspfad erreichbar. Die A2R-Datei selbst bleibt
   als Primärquelle liegen.

## 3. Beschaffungsliste nach Vorrang

### Nr. 1 — CP/M 2.2 „Disk Jockey 2D" 8-Zoll (1979) · **FM-Aufnahme, P3-123c**

archive.org-Objekt `cpm-ver-2.2-cbios-rev-3.1-disk-jockey-2-d-e-000h-1979`

| Feld | Befund |
|---|---|
| **Herkunft** | **Belegt.** Imaging-Log liegt im Objekt und wurde gelesen: „FAST IMAGER LOG / Performed by Applesauce v1.88.4 / Timestamp: May 30, 2024 / Media: 8\" Floppy Disk / Physical: 611K SS 500kbps **FM+MFM** encoded disk with IBM sector structure." Echte Aufnahme einer physischen Diskette, kein aus einem Sektorabbild erzeugter Fluss. Foto der Diskette liegt bei. |
| **Lizenz/Rechte** | Metadata-Felder `licenseurl: None`, `rights: None` — **es steht nichts da**. Inhalt ist CP/M 2.2 von 1979. **Zone PRÜFEN.** Präzedenz im Baum: reale historische Abbilder liegen in `tests/corpus/` (gitignored, nur SHA-256-Manifeste im Repo) — derselbe Weg wie dec0de/Bounty Bob; ob er hier trägt, entscheidet der Eigentümer. |
| **Kennzahl** | **FM-Aufnahme (P3-123c, Vorrang 1).** Gemessen an der beiliegenden IMD (77 Spuren ausgezählt): **Zylinder 0 = FM 500 kbps, 26 Sektoren × 128 Byte** — exakt das IBM-3740-Spur-Layout, gegen das `flux_decode_fm()` (MF-864) geschrieben ist; die übrigen 76 Spuren MFM 500k 8×1024. Ehrlich gesagt: **eine** FM-Spur, keine ganze FM-Diskette. Sie ist dafür die normgerechteste, die die Sichtung gefunden hat. Zusatznutzen: die IMD ist ein **reales IMD mit gemischten Modi** — Weg für `imd` T2→T1b (bewegt die T3-Zahl nicht, zahlt auf die 1:2-Hebungsregel ein). |
| **Aufwand** | 3 Dateien: `.a2r` 28,0 MB (md5 `3ec28534…`), `.imd` 0,40 MB (md5 `cf27a911…`), Log 2,6 KB. Zum Nachweis fehlt: Umdrehungszahl der A2R-Captures (ohne Download nicht bestimmbar) und der Transkodier-Schritt A2R→SCP. Geometrie ist benannt (Log + IMD). |

### Nr. 2 — F-15 Strike Eagle, Atari 8-bit (1984) · **FM-Aufnahme + reale ATX/MFI**

archive.org-Objekt `f-15-strike-eagle-f-15-strike-eagle`

| Feld | Befund |
|---|---|
| **Herkunft** | **Teilbelegt.** Beschreibung des Uploaders: „Atari Floppy with Copy Protection / Disk Images in MFI, ATX and A2R(flux) formats". Kein Imaging-Log im Objekt (Datei-Liste geprüft: a2r, atx, mfi, jpg — kein .txt). Derselbe Uploader dokumentiert sonst durchgängig mit Applesauce-Logs; hier ist die Kette **behauptet, nicht belegt**. Diskettenfoto liegt bei. |
| **Lizenz/Rechte** | `licenseurl: None`, `rights: None`. Inhalt MicroProse 1984, kopiergeschützt. **Zone PRÜFEN** (gleiche Lage wie Nr. 1). |
| **Kennzahl** | Dreifach: (a) **FM-Aufnahme (P3-123c, Vorrang 2** — Atari 810/1050, das Medienprofil `UFT_MEDIA_ATARI_FM` existiert im Baum); ATX ist per Formatdefinition Single Density, die ATX-Größe 100 344 B passt zu 40 Spuren SD. (b) **erstes reales ATX im Korpus** — schließt wörtlich die in `VERIFICATION_TIERS.md` Z. 54 benannte Lücke (atx T2→T1b-Weg). (c) **erstes reales MFI** — dito Z. 66 (mfi T2→T1b-Weg). (b) und (c) bewegen die T3-Zahl nicht; sie sind Hebungswährung nach der 1:2-Regel. Vorbehalt: Kopierschutz macht die Disk als **erstes** FM-Oracle unhandlich — dafür liefert das ATX die Sektor- und Statusreferenz gleich mit. |
| **Aufwand** | 3 Dateien: `.a2r` 7,81 MB (md5 `66062dde…`), `.atx` 100 344 B (md5 `7cb0bee0…`), `.mfi` 879 834 B (md5 `f94e84c5…`). Fehlt: Umdrehungszahl (ungeklärt), Log fehlt (s. o.). ATX und MFI sind **ohne Transkodierung** direkt von den vorhandenen T2-Lesern messbar. Reserve-Objekt gleicher Bauart: `operational-whirlwind-operational-whirlwind` (A2R 7,94 MB + ATX 80 176 B + MFI 989 402 B; laut Uploader „several copy protections", deshalb Zweitwahl). |

### Nr. 3 — OUT-THINK (Kamasoft), Kaypro CP/M · **T3-Hebung `cpm`**

archive.org-Objekt `out-think-kamasoft-out-think-for-cpm`

| Feld | Befund |
|---|---|
| **Herkunft** | **Belegt mit unabhängiger zweiter Hand.** Uploader wörtlich: „I have confirmed that this disk is a **40 track 400K Kaypro DSDD disk (10x512)**. I was also able to read the files and directory using **22disk**." Geometrie benannt, Verzeichnis durch ein fremdes, etabliertes CP/M-Werkzeug gegengelesen. Applesauce meldete Kopierschutz, deshalb liegen A2R+HFE+IMD bei. |
| **Lizenz/Rechte** | `licenseurl: None`. Kommerzielle Software (Kamasoft). **Zone PRÜFEN.** |
| **Kennzahl** | **ungeprüfte Formate (T3) → runter**: `cpm` steht auf T3 (`VERIFICATION_TIERS.md` Z. 77, einziger Test `test_cpm_fs`). Im Baum gemessen: `src/formats/cpm/uft_cpm_diskdefs.c:221-223` führt **`cpm_kaypro_4` („Kaypro 4 DSDD")** fest verdrahtet — exakt die vom Uploader benannte Geometrie. Messweg: IMD→Rohabbild, `cpm`-Plugin liest Verzeichnis, Abgleich gegen die von 22disk bestätigte Dateiliste. Das wäre der **erste** Fund dieser Sichtung, der die T3-Release-Kennzahl direkt bewegt. |
| **Aufwand** | 3 Dateien: `.a2r` 17,82 MB, `.hfe` 1,00 MB, `.imd` 0,13 MB. Fehlt: die 22disk-Dateiliste selbst ist nicht im Objekt — beim Nachweis einmal selbst mit cpmtools/22disk erzeugen (zweite Hand bleibt zweite Hand, Werkzeug austauschbar). Vorsicht: Kopierschutz gemeldet; falls das Verzeichnis betroffen ist, Befund dokumentieren statt biegen. |

### Nr. 4 (Reserve, kein eigener Vorschlag) — Operation Whirlwind, Atari (1983)

Gleiche Trias wie Nr. 2 (A2R+ATX+MFI, Felder s. dort). Wird erst
gezogen, wenn Nr. 2 am Kopierschutz scheitert oder ein zweites
FM-Realexemplar für den Regressionsschutz gebraucht wird.

## 4. Fundus (benannt wartend, bewegt heute keine Zahl)

| Fund | was es ist | was ihn öffnen würde |
|---|---|---|
| `hp-1000-backup-disks-scratch` | 14× 8-Zoll-A2R (je ~20 MB, echte Aufnahmen, Fotos beiliegend, **kein Log, Geometrie nirgends benannt** → als Oracle unbrauchbar). HP-1000-Medien sind mutmaßlich **M2FM** — das ist die offene Rückfrage **P3-123a** („Dekoder ergänzen oder als erkennbar-nicht-dekodierbar kennzeichnen"). Eine einzelne Datei wäre das Beweismaterial für diese Eigentümer-Entscheidung. **EINFRIER-REGEL:** ein M2FM-Dekoder wird hier ausdrücklich NICHT vorgeschlagen. | Eigentümer-Entscheid P3-123a |
| `altair-disk` | 2× 8-Zoll-A2R (19,7/21,4 MB), MITS Altair = **hardsektoriert**, ohne IBM-Adressmarken. `hardsector` steht auf T3 — aber das Plugin liest Sektor-Abbilder, kein Fluss; ob dieser Fund es je prüfen kann, ist ungeklärt. Kein Log, keine Sektorreferenz. | geklärter Messweg Fluss→hardsector |
| `trs80-model3-cpm-cheap` (anderer Uploader) | **CC-BY-4.0** (licenseurl im Metadatensatz: `https://creativecommons.org/licenses/by/4.0/`) — reales SCP (9,6 MB) + IMD derselben Diskette. Gemessen (IMD ausgezählt): **40 Spuren MFM 250k 10×512 — kein FM**, darum hier gefallen. Als frei lizenzierte reale SCP-Aufnahme mit benannter Geometrie trotzdem selten; `scp` ist aber schon T1b. | eine Frage, die ein reales freies SCP braucht |
| TRS-80-SD-Objekte (`rsm-2-d-…-1978`, `scripsit-…-1979`, `disk-scope-…`, u. a.) | DMK+IMD+IMG je Diskette, **kein Fluss** — für P3-123c unbrauchbar. Reale DMK-Paare wären ein `dmk`-T2→T1b-Weg (Kreuzlesung DMK gegen IMD derselben Diskette). | Hebungs-Slot nach der 1:2-Regel |
| Ianoid-Sammlungen (z. B. `2020-04-ianoid-misc-fluxes`: 144 A2R, 120 WOZ, 11 DMK, 2,8 GB) | Apple-II-GCR-Fluss in Masse — kein FM, aber ein Reservoir für spätere WOZ/GCR-Realdatei-Fragen. | konkrete GCR-Frage |
| FM-Towns-KryoFlux-Objekte (`fmt_*_kfraw`) | „FM" im Namen ist der Rechner, nicht die Kodierung (1,2-MB-MFM). Ausdrücklich notiert, damit der Name niemanden fängt. | — |

**Quelle-2-Bilanz:** Die archive.org-weite Suche (KryoFlux-Titel: 1652
Treffer, davon nach FM/SD-Raster 7 relevant; TRS-80+Flux: 7 Treffer) hat
**keinen** frei lizenzierten echten FM-Flussabzug zutage gefördert, der
die floppyarchaeology-Funde schlägt. Der einzige CC-lizenzierte Treffer
(trs80-model3-cpm-cheap) ist gemessen MFM.

## 5. Vorschlagsblock für `docs/OPEN_ITEMS.md` (3 von max. 5)

> Zur Übernahme durch einen Menschen. Ein Vorschlag ist kein Eintrag.

```
| KOR-a | FM-Flussabzug beschaffen: archive.org cpm-ver-2.2-cbios-rev-3.1-
        disk-jockey-2-d-e-000h-1979 (.a2r 28,0 MB md5 3ec28534 + .imd 0,40 MB
        md5 cf27a911 + Log). Echte Applesauce-1.88.4-Aufnahme, 8", Zyl 0 =
        FM 500k 26x128 (aus der IMD ausgezaehlt) = IBM-3740-Spurlayout.
        Kennzahl: FM-Aufnahme (P3-123c, Vorrang 1). Kanal: Daten/Fixture.
        Zone PRUEFEN (keine Rechteangabe am Objekt; Weg wie dec0de:
        tests/corpus/ gitignored + SHA-Manifest — Eigentuemer entscheidet).
        Messweg: A2R -> SCP via gw 1.23 ODER hxcfe (beide gepinnt; welches
        von beiden .a2r liest, ist die erste Messung), dann Flusspfad,
        flux_decode_fm() auf Zyl 0, 26 Sektoren byteweise gegen IMD-Spur 0. |
| KOR-b | Reales ATX+MFI+FM-Fluss beschaffen: archive.org f-15-strike-eagle-
        f-15-strike-eagle (.atx 100344 B md5 7cb0bee0, .mfi 879834 B md5
        f94e84c5, .a2r 7,81 MB md5 66062dde). Kennzahl: P3-123c Vorrang 2
        (Atari FM, UFT_MEDIA_ATARI_FM) + schliesst die in
        VERIFICATION_TIERS.md Z.54/66 woertlich benannten Luecken "kein
        reales ATX/MFI im Korpus" (Hebungswaehrung, T3-Zahl unberuehrt).
        ATX/MFI sind ohne Transkodierung von den T2-Lesern messbar.
        Zone PRUEFEN (keine Rechteangabe; Kopierschutz auf der Disk ist
        forensisch Nutzlast, kein Hindernis). Herkunftsluecke benannt:
        kein Imaging-Log im Objekt. |
| KOR-c | cpm von T3 heben: archive.org out-think-kamasoft-out-think-for-cpm
        (.imd 0,13 MB + .hfe 1,00 MB + .a2r 17,82 MB). Uploader-Beleg
        woertlich: "40 track 400K Kaypro DSDD disk (10x512)", Verzeichnis
        mit 22disk gegengelesen (unabhaengige zweite Hand). Im Baum:
        cpm_kaypro_4 in src/formats/cpm/uft_cpm_diskdefs.c:221-223 fuehrt
        exakt diese Geometrie. Kennzahl: ungepruefte Formate (T3) RUNTER
        (cpm, VERIFICATION_TIERS.md Z.77). Zone PRUEFEN. Messweg:
        IMD -> Rohabbild -> cpm-Plugin liest Verzeichnis -> Abgleich gegen
        selbst erzeugte cpmtools/22disk-Dateiliste. |
```

Nicht vorgeschlagen (Regel 9 — bewegt heute keine Zahl): HP-1000 (wartet
auf P3-123a-Entscheid), Altair (Messweg ungeklärt), trs80-model3-cpm-cheap
(kein FM, scp schon T1b), TRS-80-DMK-Paare, Ianoid-Masse. Alles in §4
benannt wartend.

## 6. Nächster Griff — EINE Datei, EINE Messung

**Die Datei:** `CPM Ver 2.2 CBIOS Rev 3.1 Disk Jockey 2D @ E000h (1979).a2r`
(28,0 MB, md5 `3ec28534…`) aus KOR-a — **plus** die schon gesichtete IMD
(0,40 MB), die als Referenz dient.

**Die Messung, Schritt für Schritt:**

1. SHA-256 beider Dateien, Eintrag ins Korpus-Manifest (Herkunft: Objekt-
   URL, Applesauce-Log wörtlich, Abrufdatum).
2. **Erste Teilmessung:** liest `gw 1.23` (liegt gepinnt vor) die A2R —
   `gw convert <datei>.a2r <datei>.scp`? Falls nein: hxcfe (Klon
   `05b53aa` liegt vor). Welches Werkzeug es tut, wird gemessen, nicht
   angenommen; das Ergebnis gehört ins Manifest.
3. SCP durch den UFT-Flusspfad, Zylinder 0: `flux_decode_fm()` muss 26
   Sektoren à 128 Byte liefern.
4. Byteweiser Vergleich der 26 Nutzlasten gegen Spur 0 der IMD
   (Applesauce' eigener Dekoder = fremde Hand; gw/hxcfe transkodieren nur
   Fluss, dekodieren nicht — die Unabhängigkeit bleibt erhalten).
5. Erfolgskriterium: 26/26 byteidentisch → P3-120 verliert seinen
   „NICHT belegt"-Vorbehalt, P3-123c ist geschlossen. Jede Abweichung
   wird benannt, nicht wegerklärt — sie wäre der erste echte
   FM-Realdaten-Befund des Dekoders und damit wertvoller als ein grüner
   Lauf.

## 7. UNGEKLÄRT

* **Umdrehungszahl je A2R-Capture** (alle Funde): ohne Download der
  Fluss-Datei nicht bestimmbar; die Dateigrößen (28 MB / 77 Spuren ≈ 364
  KB je Spur) *deuten* auf Mehrfach-Captures, das ist eine Schätzung.
* **Ob gw 1.23 A2R liest** — erste Teilmessung des nächsten Griffs, hier
  nicht vorweggenommen.
* **Ob die F-15-A2R tatsächlich FM ist** — aus der ATX-Größe geschlossen
  (ATX ist SD-definiert), nicht aus einem Log. Das Log fehlt im Objekt.
* **Kopierschutz-Wirkung auf das OUT-THINK-Verzeichnis** — 22disk konnte
  lesen, aber ob alle Spuren regulär sind, sagt erst die Messung.
* **HP-1000-Encoding** — M2FM ist Vermutung aus der Gerätegeschichte
  (HP 9885/9895), nirgends am Objekt belegt.
* **Rechtekette sämtlicher floppyarchaeology-Objekte** — kein einziges
  trägt ein Rechte-Feld; Identität des Uploaders nicht verifiziert.

## 8. Nicht geprüft

* Die übrigen ~1670 Objekte der Sammlung jenseits des Fluss-/FM-/T3-
  Rasters (v. a. ISO/CD/MSDN — außerhalb des Auftrags).
* Quellen außerhalb archive.org (a8preservation.com als bekannter
  ATX-Herausgeber, bitsavers `bits/`-IMD-Bestand, deramp.com-Altair-
  Material) — nur namentlich notiert, nicht gesichtet.
* Kein einziges Fluss-Abbild wurde heruntergeladen oder inhaltlich
  geprüft; alle Aussagen über A2R-Inhalte stützen sich auf Logs, IMDs
  und Metadaten.
* `data/known_negatives.json` führt Repos, keine Datenquellen — für
  archive.org-Objekte gab es nichts abzugleichen.
