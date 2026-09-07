# Gutachten P3-247: aufgezeichnete Flussströme echter Geräte — Beschaffungsliste

**Auftrag:** P3-247 (`docs/OPEN_ITEMS.md` Z. 431), vom Eigentümer als Scout-Auftrag
gestellt. Priorität laut Auftrag: (1) KryoFlux-Stream `.raw` wie DTC ihn schreibt,
(2) Greaseweazle-Rohaufnahmen mit mehreren Umdrehungen und echten Index-Marken,
(3) SuperCard-Pro-`.scp` von echter Hardware mit Mehrfachumdrehungen.
**Datum:** 2026-09-07 · **Inventar:** `work/inv.json` auf HEAD `01a7ed26`
(88 Plugins, SSOT ok, 49 Korpus-Einträge) · **Kategorie:** Daten (Korpus) ·
**Aufwandsklasse:** S je Fund (Beschaffung), M für den ersten Differenzlauf.

---

## 0. Auflagen- und Werkzeug-Nachweis

**Heruntergeladen wurden ausschließlich einzeln benannte Dateien:**

| Datei | Bytes | SHA-256 | Zweck |
|---|---|---|---|
| `dbalsom/fluxfox` → `tests/images/sector_test/sector_test_kryoflux_360k.zip` | 2 387 173 | `26c0c077f9227e49e804a04bd8423542874905af81e4b0bd2a692d4977def00e` | Inhalt zählen, KFInfo lesen |
| `Datamuseum-DK/FloppyToolsExamples` → `q1/000/000_bin00.0.raw` | 254 404 | `304df9bcd6b026579582e98b78573a4202fa2303e275c3c1e206762019573606` | KFInfo, Index-Zahl |
| archive.org `dig-dogs_streetbusters_kryoflux` → ZIP-Mitglied `dig-dogs_streetbusters/track00.0.raw` | 455 613 | `a2be70fb4e5b81d4aa94e1d987a7bf28e84531ed0586f3cb79c1a8f284321e88` | KFInfo, Index-Zahl (Range-Request auf ZIP-Mitglieder ignoriert archive.org — die Datei kam ganz) |
| drei SCP-Dateiköpfe per HTTP-Range `0-15` | je 16 | — | Umdrehungszahl, Spurbereich, Flags |
| 96 Bytes von `yas-sim/fdc_bitstream/test_data/2019FM77AVDemo-4MHz.raw` | 96 | — | Formatprobe (Ergebnis: kein KryoFlux-Strom) |
| 16 archive.org-Metadatensätze (JSON), 2 Markdown-Dokus (Toshiba), 4 Suchantworten | < 1 MB | — | Katalogisierung |

Kein Fluss-Abbild über 2,4 MB wurde übertragen. Alles liegt unter
`tools/uft-scout/work/p3-247/` (nicht im Repo).

**Werkzeuglage, gemessen, nicht angenommen:** die MCP-Server `firecrawl`
(Search/Scrape) antworteten mit HTTP 401, `github` (search_code) mit
„Bad credentials" — beide in dieser Sitzung **nicht benutzbar**. Ersatz:
`gh api` (angemeldet als Axel051171), `curl` gegen die archive.org-APIs,
`WebSearch`. `forum.kryoflux.com` liefert 403 (Cloudflare) — der Thread
„Need sample Kryoflux raw stream files" (t=1717) konnte **nicht** gelesen
werden. `info-coach.fr` (Jean Louis-Guerin, Autor der im Parser-Header
zitierten Spec): die Aufit-/Preservation-Seiten liefern 404; nur
`FD-Hard.php` lädt und enthält keine Beispielströme.

## 1. Methode (jede Zahl mit Verfahren, MF-615)

* **archive.org, KryoFlux:** `advancedsearch.php?q=kryoflux AND mediatype:software`,
  Feld `numFound` = **2000**; davon mit gesetztem `licenseurl` = **1**
  (`dig-dogs_streetbusters_kryoflux`). Verfahren: Feld `licenseurl` nicht leer.
  Untergrenze — Objekte, deren Titel/Beschreibung „KryoFlux" nicht nennt,
  fallen durch.
* **archive.org, SCP/Greaseweazle/Flux mit Lizenzfeld:**
  `q=("supercard pro" OR "scp" OR greaseweazle OR "flux image") AND
  mediatype:software AND licenseurl:[* TO *]` → **49** Treffer, davon per
  Titel **23** Disketten-Objekte, **3** unklar, **23** Namensrauschen
  (Spiel „SCP – Containment Breach"). Von den 23 wurden **9** per
  Metadaten-API (`/metadata/<id>`, Dateiliste + `licenseurl` + Beschreibung)
  geprüft, dazu **7** weitere Objekte des Uploaders `dave@max.bungi.com`
  (Abfrage `uploader:(dave@max.bungi.com)` → 14 Treffer, 13 mit CC-BY-4.0).
* **GitHub:** `gh api search/repositories q="kryoflux stream"` (6 Repos),
  `search/code q='"00.0.raw" kryoflux extension:md'` (15 Treffer);
  Repo-Bäume von 10 Kandidaten rekursiv auf `\.(raw|scp|flux|hfe|a2r|woz|kfx)$`
  gefiltert. Ergebnis: **2** Repos mit Fluss-Dateien (`dbalsom/fluxfox`,
  `yas-sim/fdc_bitstream`); FluxEngine, Greaseweazle, disk-utilities,
  kryoflux-stream-checker, kryotools, DiskFormatID, archivists-guide, pcjs:
  **0**.
* **Umdrehungen in SCP:** Byte 5 des 16-Byte-Kopfs
  (`SCP`-Spezifikation v2.5, cbmstuff, Feld „number of revolutions").
* **Umdrehungen in KryoFlux-Strömen:** Anzahl der Index-OOB-Blöcke
  (Typ `0x02`) beim vollständigen Durchlauf eines Stroms mit einem
  20-zeiligen Python-Zähler nach dem Opcode-Schema aus
  `src/flux/uft_kryoflux_stream.c:11-22`. *n* Index-Marken = *n − 1* volle
  Umdrehungen.
* **Herkunft der Ströme:** KFInfo-OOB (Typ `0x04`) im Wortlaut — `name=`,
  `version=`, `host_date=`.

## 2. Was das Inventar sagt (Abfragen zitiert)

`inventar.py query` gegen `work/inv.json`, wörtlich:

```
"kryoflux":        { "vorhanden": true,  "treffer": ["flux","kryoflux","kryoflux_checker"], "tier": null }
"kryoflux stream": { "vorhanden": false, "abgedeckt": true, "schwache_treffer": ["flux","kryoflux"],
                     "hinweis": "nur Teilwort-Treffer — von Hand pruefen, NICHT als vorhanden verwerfen" }
"kfraw":           { "vorhanden": true,  "treffer": ["kfraw"], "tier": null }
"scp":             { "vorhanden": true,  "tier": "T1b" }
"86f":             { "vorhanden": true,  "tier": "T3" }
"pri":             { "vorhanden": true,  "tier": "T3", "treffer": ["apridisk","pri","uft_apridisk","uft_prime"] }
"cpm":             { "vorhanden": true,  "tier": "T3" }
"imd":             { "vorhanden": true,  "tier": "T2" }
"mfi":             { "vorhanden": true,  "tier": "T2" }
"td0":             { "vorhanden": true,  "tier": "T2" }
"multi revolution":{ "vorhanden": false, "abgedeckt": false }   -> von Hand geprueft: UFT hat Multirev (src/core, MF-611)
```

**Von Hand nachgesehen (Regel 4):**

* `inv["korpus"]` (49 Einträge, vollständig durchgesehen): **0** KryoFlux-
  Stream-Dateien; **1** SCP (`gw_amigados.scp`). Dessen Manifest-Eintrag
  sagt wörtlich: `gw convert --format=amiga.amigados tests/corpus_free/
  xdftool_dd_ofs.adf out.scp` — ein aus einem Sektorabbild **synthetisierter**
  Fluss mit 2 Umdrehungen (`tests/test_corpus_scp.c:9`), **keine Aufnahme**.
  Das schärft die Prämisse von P3-247: der 99,71-%-Befund aus MF-950
  wurde an einem Strom erzielt, den nie ein Gerät gesehen hat. Ein
  aufgezeichneter Strom fehlt damit **auch** für den Greaseweazle-Pfad, nicht
  nur für KryoFlux. Echte Aufnahmen im Korpus gibt es heute nur von
  **Applesauce** (kor_a/b/c).
* `grep -i kryoflux docs/VERIFICATION_TIERS.md` = **0** Zeilen; `kfraw` und
  `gwraw` stehen **nicht** in `scripts/gen_format_list.py`. Die beiden
  Dateien `src/formats/flux/kfraw.c` / `gwraw.c` (Kopf: „@version 3.8.0",
  alte `FloppyDevice`-API) sind vom Tier-Ledger und von der Registry nicht
  erfasst. Der KryoFlux-Lesepfad ist also **kein tier-geführtes Format** —
  das entscheidet die Kennzahl-Frage in §5.
* `uft_kf_decode()` hat außerhalb der eigenen Datei **drei** Aufrufer
  (`src/hardware_providers/kryoflux_provider_v2.cpp:334`,
  `src/analysis/uft_protection_probe.c:103`,
  `tests/emulators/kryoflux/test_kryoflux_emulator.c:52`); die Tests
  `tests/test_kryoflux_stream.c` und `tests/test_external_integration.c`
  füttern **handgeschriebene Byte-Felder** — wie P3-247 sagt. Der
  Verzeichnis-Leser für Stream-Sätze liegt in `src/hal/uft_kryoflux_dtc.c:1412`
  (`%s/track%02d.%d.raw`).
* Nebenbefund an der Negativliste: `data/known_negatives.json` führt
  `yas-sim/fdc_bitstream` als „integriert (vendored in src/flux/fdc_bitstream)".
  `CLAUDE.md` sagt: „nach MF-626 (fdc_bitstream entfernt)". Der Eintrag ist
  **veraltet**; hier nicht geändert, nur benannt.

## 3. Die Funde

### Nr. 1 — `dbalsom/fluxfox` `tests/images/sector_test/` · **eine Diskette in 19 Containern**, darunter KryoFlux-Strom **und** SCP

| Feld | Befund |
|---|---|
| **URL / Abruf** | `https://github.com/dbalsom/fluxfox/tree/main/tests/images/sector_test` — Einzeldateien per `raw.githubusercontent.com/dbalsom/fluxfox/main/tests/images/sector_test/<name>`. Kein LFS (Range-Request auf die SCP lieferte HTTP 206). |
| **Dateien (Größe in Bytes, aus dem Git-Baum)** | `sector_test_kryoflux_360k.zip` **2 387 173** (80 × `trackNN.S.raw`, 9 109 711 B entpackt) · `sector_test_360k.scp` **18 175 764** · `.img` 368 640 · `.imd` 2 622 · `.86f` 2 153 628 · `.hfe` 1 054 720 · `.mfi` 2 049 889 · `.td0` 4 680 · `.pri` 1 003 228 · `.psi` 25 948 · `.pfi` 9 108 152 · `.mfm` 1 024 722 · `.tc` 1 073 620 · `.imz` 2 024 — plus ein 1,2-MB-Satz (`sector_test_1200k.{imd,img,mfi}`). |
| **Herkunft** | **Gemessen am Strom selbst:** KFInfo-Block wörtlich `name=Greaseweazle, version=1.18, host_date=2024.11.09, host_time=11:57:59, sck=24027428.5714286, ick=3003428.5714286`. **Die „KryoFlux"-Dateien sind von Greaseweazle 1.18 geschrieben, nicht von DTC** — ein zweiter, unabhängiger *Schreiber* des KryoFlux-Stromprotokolls. `track00.0.raw`: 127 987 B, **4 Index-OOBs = 3 volle Umdrehungen**. SCP-Kopf: `SCP`, Version-Byte 0x00, Disk-Typ 0x80, **3 Umdrehungen**, Spuren 0–79 (40 Zyl. × 2), Flags 0x23 (Index-synchron, 96 tpi, Footer), Kopf-Feld 0 = beide Seiten. Commit `2b852552` (2024-11-09) „reorganize tests, add 360k sector test images in all formats." Diskette: 5,25" 360K PC-MFM (40×2×9×512 = 368 640 = `.img`-Größe), Inhalt ein Sektor-Testmuster des Autors. Aufnahmegerät: Greaseweazle (aus KFInfo); Firmware/Drive nicht benannt. |
| **Lizenz im Wortlaut** | `LICENSE` (Repo): „MIT License / Copyright (c) 2024 Daniel Balsom / Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction …". **Zone GRÜN** nach Matrix (MIT). Vorbehalt benannt: der Autor legt für `tests/images/transylvania/` eine **eigene** `LICENSE.txt` bei (Wortlaut: „Transylvania is copyright 1982-1986 Polarware / Penguin Software. Mark Pelczarski … donate this game to the public … You are allowed to copy this game as long as you do not charge any money for it …") — er unterscheidet also je Ordner. `sector_test/` trägt **keine** eigene Datei und fällt damit unter das Repo-MIT. |
| **Unabhängiges Gegenstück** | **Ja, mehrfach.** `.img` (368 640 B) als Sektor-Referenz; `.imd`, `.td0`, `.86f`, `.mfi`, `.hfe`, `.pri`, `.psi` derselben Diskette. Wer die Container erzeugt hat (gw / fluxfox selbst / 86Box / MAME), ist **nicht** benannt (UNGEKLÄRT U1) — für die Sektor-Bytes ist das zweitrangig, für „fremde Hand je Container" nicht. |
| **Kennzahl (Regel 9)** | **ungeprüfte Formate (T3) → runter, bis zu zwei:** `86f` (T3, `docs/VERIFICATION_TIERS.md` Z. 74 — heute nur `test_86f_spec_conformance`, gegen die 86Box-Spec, ohne reale Datei) und `pri` (T3, Z. 96, **kein Test**; Plugin `src/formats/pri/uft_pri.c:270`). Dazu Hebungswährung: `td0`/`imd`/`mfi` T2 → T1b mit realem Paar. Und der Kern von P3-247: erster **aufgezeichneter** Strom für den Greaseweazle-Pfad (SCP, 3 Umdrehungen, echte Index-Marken) **und** erster KryoFlux-Protokoll-Strom im Korpus. |
| **Aufwand** | S. Alles liegt frei; das ZIP liegt bereits unter `work/p3-247/` (SHA oben). |

### Nr. 2 — archive.org `dig-dogs_streetbusters_kryoflux` · **DTC-geschriebener Strom, 84 Spuren, 5 Umdrehungen, mit Sektorabbild**

| Feld | Befund |
|---|---|
| **URL / Abruf** | `https://archive.org/details/dig-dogs_streetbusters_kryoflux` — `dig-dogs_streetbusters_kryoflux.zip` **28 258 736 B**, md5 laut Archiv `3d96081d41cc9c0120807f21303d9623`; Einzelströme ohne ZIP-Download über `…/dig-dogs_streetbusters_kryoflux.zip/dig-dogs_streetbusters%2FtrackNN.S.raw`. Gegenstück `dig-dogs_streetbusters.img` **1 474 560 B**, md5 `b614ac59615ce21b54beffb392cde664`. |
| **Inhalt (aus der archive.org-Listenansicht des ZIP, ohne Download)** | **168** Dateien `track00.0.raw … track83.1.raw` (84 Zylinder × 2 Seiten — DTC-Voreinstellung für 3,5"), Einzelgrößen um 455–539 KB. |
| **Herkunft** | **Gemessen an `track00.0.raw`:** KFInfo wörtlich `host_date=2023.08.04, host_time=15:50:29, hc=0` und `name=KryoFlux DiskSystem, version=3.00s, date=Mar 27 2018, time=18:25:55, hwid=1, hwrv=1, hs=1, sck=24027428.5714285, ick=3003428.5714285625`. **Echter DTC-3.00s-Strom.** 455 613 B, **6 Index-OOBs = 5 volle Umdrehungen**, 14 StreamInfo-Blöcke, 455 095 Flusswerte, EOF-Block an Byte 455 606. Diskette: 3,5" HD, MS-DOS-Spiel, Fassung 1995, Uploader `mail@schanzer.eu`, Beschreibung wörtlich: „KryoFlux Image der Originaldiskette / MFM Sektor Image (.img) / JPG Scan der Diskette in 300 DPI". Ersteller laut Metadaten: Deutscher Verkehrssicherheitsrat e.V. |
| **Lizenz / Rechte im Wortlaut** | `licenseurl` = `https://creativecommons.org/publicdomain/mark/1.0/`; Beschreibung: „PC Freeware - frei kopierbar." — Das ist eine **Public-Domain-*Markierung* durch den Uploader**, keine Lizenz des Rechteinhabers; „Freeware" sagt nichts über Ableitungen. Nicht in der Matrix → **Zone PRÜFEN**, Eigentümer-Vorlage (Regel 3/8). Praktisch: die Schutzhöhe eines Sicherheitsrats-Lernspiels ist eine Frage, die der Eigentümer stellt, nicht der Scout. Weg wie kor_a: `tests/corpus/` (gitignored) + Manifest. |
| **Unabhängiges Gegenstück** | `.img` 1 474 560 B — Erzeuger nicht benannt (mutmaßlich DTC `-i4`, also derselbe Aufnehmer, anderer Dekoder als UFT — dieselbe Lage wie kor_a: A2R + IMD aus Applesauces eigenem Dekoder, MF-869). Zusätzlich existiert eine **1994-Fassung** derselben Software von einem **anderen** Uploader (`dig-dogs-streetbusters-1994-3.5de`, `_raw.zip` 29 957 155 B + `_img.zip`, **kein** Lizenzfeld) — andere Pressung, keine Referenz für dieselbe Diskette, aber ein zweiter DTC-Hand-Strom derselben Spurstruktur. |
| **Kennzahl** | Keine der vier direkt (KryoFlux-Pfad nicht tier-geführt, §2). **Das ist der Fund, für den P3-247 gestellt wurde:** der einzige frei erreichbare **DTC**-Strom einer gewöhnlichen PC-MFM-Diskette mit Sektorreferenz. Fünfte Zahl siehe §5. |
| **Aufwand** | S (28 MB). |

### Nr. 3 — `Datamuseum-DK/FloppyToolsExamples` `q1/` · **DTC-Ströme, 35 Lesungen, quelloffener Zweitleser** — Neubesuch mit neuem Anlass

| Feld | Befund |
|---|---|
| **Anlass des Neubesuchs (Regel 6)** | Vorgeschlagen im FloppyTools-Gutachten (2026-08-28, V3) als **Multi-Lesungs-Korpus für Voting**; in `docs/OPEN_ITEMS.md` Z. 2845 als „Fundus — bewusst nicht eingeplant" geführt. **Neue Frage seit P3-247 (2026-09-07):** nicht Voting, sondern *liest der geprüfte Stream-Parser echte DTC-Bytes* — Stufe Strom, nicht Stufe Sektor. Dafür ist die Q1-Kodierung gleichgültig; die Diskette darf für UFT undekodierbar sein. |
| **URL / Abruf** | `https://github.com/Datamuseum-DK/FloppyToolsExamples` — 177 Blobs, davon 173 `.raw` unter `q1/<lesung>/<lesung>_binNN.0.raw` (Größen 226 877–281 195 B, aus dem Git-Baum); Einzelabruf per `raw.githubusercontent.com/Datamuseum-DK/FloppyToolsExamples/main/q1/000/000_bin00.0.raw`. Letzter Commit `c0574fda` 2025-07-28 („Update README"), Daten-Commit `4b931fa4` 2024-04-14. |
| **Herkunft** | **Gemessen an `000_bin00.0.raw`:** KFInfo `host_date=2024.04.11, host_time=16:24:23, hc=0` / `name=KryoFlux DiskSystem, version=3.00s, date=Mar 27 2018 …` — **DTC 3.00s**, 254 404 B, **6 Index-OOBs = 5 Umdrehungen**, 8 StreamInfo, 253 976 Flusswerte. 8"-Diskette Q1 MicroLite (Museums-Wiki `datamuseum.dk/wiki/Q1_Microlite`), einseitig, 77 Spuren; README nennt drei bekannte Defekt-Sektoren (c39h0s18, c41h0s77, c7h0s3) und markiert diesen Abschnitt selbst als „Old outdated info (20250728/phk)". |
| **Lizenz im Wortlaut** | `LICENSE`: „BSD 2-Clause License / Copyright (c) 2024, Datamuseum-DK / Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met: 1. Redistributions of source code must retain the above copyright notice … 2. Redistributions in binary form must reproduce the above copyright notice …". **Zone GRÜN** für die Verteilung durch das Museum; der **Disketteninhalt** (Q1-Corporation-Software, 1970er) ist davon nicht erfasst → **PRÜFEN** wie im Vorgutachten (U4 dort). Für den Strom-Parser-Test ist der Inhalt irrelevant — die Messung vergleicht Flusswerte, nicht Nutzdaten. |
| **Unabhängiges Gegenstück** | Für die **Strom-Ebene:** FloppyTools `base/kryostream.py` (BSD-2, Codeberg `769ca9f`), ein quelloffener Zweitleser desselben Protokolls, im Vorgutachten bereits ausgeführt (rc=0, `PYTHONUTF8=1`-Auflage). Für die Sektor-Ebene: FloppyTools-`.BIN` (Q1-Format, für UFT ohne Dekoder — Moratorium, bewusst **nicht** vorgeschlagen). |
| **Kennzahl** | Keine der vier; fünfte Zahl (§5). Zusatzwert gegenüber Nr. 2: **35 Lesungen derselben Spuren** = das einzige reale Material für Multi-Capture-Voting und für P3-125 (StreamInfo-Positionsabgleich, MF-867) an degradiertem Medium. |
| **Aufwand** | S (44 MB, oder gezielt einzelne Lesungen). |

### Nr. 4 — archive.org `trs80-model3-cpm-cheap` · **Greaseweazle-SCP, 3 Umdrehungen, + IMD, CC-BY-4.0 im Wortlaut**

| Feld | Befund |
|---|---|
| **URL / Abruf** | `https://archive.org/details/trs80-model3-cpm-cheap` — `disks/model-iii-cpm.1.15.scp` **9 648 021 B**, md5 `8dab1046502fc62958a5dfed6b74073c`, sha1 `a300e38b48b03f7f30535d991470a7546c8c6aaa`; `disks/model-iii-cpm.1.15.imd` **205 876 B**, md5 `7397980042d9631fd637210e07ce8dee`, sha1 `da0c6ba7a3793038cfea8cdf3cf40163deb3bc47`. GitHub-Spiegel `davidlrand/mame-system-media/trs80-model3-cpm/` führt **nur die IMD** (SCP nicht im Baum, gemessen). |
| **Herkunft** | Beschreibung wörtlich: „`disks/model-iii-cpm.1.15.scp` — the Greaseweazle **flux master** of that system disk." Geometrie wörtlich: „40-track SS, 10×512 MFM". **SCP-Kopf gemessen:** `SCP`, Version 0x00, Disk-Typ 0x80, **3 Umdrehungen**, Spuren 0–80, Flags 0x23, Kopf-Feld 1 (nur Seite 0), Auflösung 0. Datum der Diskette 1983 (Feld `date`), Aufnahme 2026 (Objekt `addeddate` 2026-06-02). gw-Version, Firmware, Laufwerk **nicht benannt** (U2). Wie die IMD entstand, steht nicht am Objekt; das Schwesterobjekt (Nr. 5) dokumentiert das Rezept `gw read … disk.scp; gw convert disk.scp disk.imd` — also gws Dekoder, fremde Hand gegenüber UFT, dieselbe wie der Aufnehmer (Lage wie kor_a). |
| **Lizenz im Wortlaut** | `licenseurl` = `https://creativecommons.org/licenses/by/4.0/`; Beschreibung: „**License.** Material I authored (this description and the README, my Alpha One CP/M 2.2 BIOS on the system disk, the disks I built and curated, the MAME driver patch) is licensed CC-BY-4.0, © 2026 David L. Rand. Third-party material preserved for emulation and not mine to license: any third-party CP/M utilities on `mycpm-model3.imd` … and — by reference only, not included here — the stock Model III ROMs … CP/M is a product of Digital Research, Inc." — **CC-BY-4.0 steht nicht in der Matrix → PRÜFEN**; zudem trägt die Systemdiskette CP/M-2.2-BDOS/CCP von Digital Research, die der Uploader ausdrücklich nicht lizenziert. Eigentümer-Vorlage. Als Fluss-Aufnahme (Timing, Index, Umdrehungen) ist die Datei unabhängig vom Inhalt nutzbar; der Manifest-Weg (gitignored + SHA) bleibt. |
| **Unabhängiges Gegenstück** | IMD derselben Diskette (s. o.). |
| **Kennzahl** | Erster **aufgezeichneter** Greaseweazle-Strom (Nr. 1 ist auch einer; Nr. 4 ist die *historische* Diskette von 1983, Nr. 1 ein Testmuster). Hebungswährung `imd` T2 → T1b (reales Paar). **Nicht** T3: `cpm` bräuchte eine Diskdef — TRS-80-„Alpha One" ist ein Eigenbau-BIOS, in `uft_cpm_diskdefs.c` nicht vorhanden (gemessen: kein Treffer für trs80/model iii). |
| **Aufwand** | S (9,6 MB). |

### Nr. 5 — archive.org `toshiba-t250-t200-cpm` `T-200/boot-v2.30a-1024` · **GW-SCP 3 Umdrehungen, FM-Bootspur + MFM, IMD + MFI + Formatdoku aus BIOS-Quelle**

| Feld | Befund |
|---|---|
| **URL / Abruf** | `https://archive.org/details/toshiba-t250-t200-cpm` — `disks/T-200/boot-v2.30a-1024.scp` **17 159 065 B**, md5 `016c8a84d1c51b2ed6fbd3216e02be66`; `boot-v2.30a-1024.imd` **340 114 B**, md5 `8e84ccbc86918bab1ce906935b7ca5a0`; `boot-v2.30a-1024.mfi` (527 891 B, im GitHub-Spiegel `davidlrand/mame-system-media/toshiba-t250-t200/disks/T-200/`); `docs/disk-formats.md` 18 796 B. |
| **Herkunft** | Beschreibung wörtlich: „Three T-200 disks (ser-10133, V2.30a, Trianex V3.0) additionally include their original Greaseweazle .scp flux masters — the deepest, format-independent capture." **SCP-Kopf gemessen:** 3 Umdrehungen, Spuren 0–69 (**35 Zylinder** × 2 — `disk-formats.md` Z. 14: „T-200 uses 35 tracks, not the more common 40. This is unusual."), Flags 0x23, beide Seiten. `disk-formats.md` Z. 17 wörtlich: „Boot track (cyl 0, head 0) is always **FM 128-byte sectors**, regardless of disk format" — 18 Sektoren, Skew 2 (Z. 245); Rest MFM (Typ 6/7: 1024-B-Sektoren, 5 je Spur). Die Doku ist „derived directly from the CP/M BIOS source (`bios64.asm` rev 2.12, 1982)" und enthält fertige **cpmtools-`diskdefs`** (Z. 277 ff.). Rezept am Objekt: `gw read --drive=A --tracks=c=0-34:h=0-1 --format=ibm.scan disk.scp` / `gw convert disk.scp disk.imd`. |
| **Lizenz im Wortlaut** | `licenseurl` CC-BY-4.0; Beschreibung: „Material I authored (the READMEs and docs, my replacement boot ROMs and CP/M 2.2 BIOS, the bootable disks I built and curated, my photographs) is licensed CC-BY-4.0, © 2026 David L. Rand. Preserved for emulation and not mine to license: Toshiba Corporation's stock CP/M BIOS, as contained in … the toshiba-v2.21-original-ser-10133 variant (including its .imd and .scp representations)." — **V2.30a ist die Diskette mit dem BIOS des Uploaders**, also die rechtlich sauberste der drei; CP/M-BDOS/CCP von DRI bleibt wie bei Nr. 4. **Zone PRÜFEN** (CC-BY-4.0 nicht in der Matrix). |
| **Unabhängiges Gegenstück** | IMD (gw-Dekoder) **und** MFI (MAME-Weg) derselben Diskette, plus Geometrie aus Primärquelle. |
| **Kennzahl** | **ungeprüfte Formate (T3) → runter: `cpm`** (Z. 78, einziger Test `test_cpm_fs`): die Diskdef steht am Objekt, `cpmls` ist registriertes Oracle (`docs/ORACLES.md`), die Geometrie ist aus BIOS-Quelle belegt. Zweitnutzen **P3-123c**: FM-Spur auf 5,25" (Zyl. 0, 18×128) in einer echten GW-Aufnahme — `flux_decode_fm()` (MF-864) hat bisher nur die 8"-Applesauce-Aufnahme (kor_a). Dazu `imd`/`mfi` T2 → T1b. |
| **Aufwand** | S (17 MB + 2 kleine Dateien). |

## 4. Fundus — benannt wartend (bewegt heute keine Zahl oder scheitert an einer Vorbedingung)

| Fund | was es ist (gemessen) | was ihn öffnen würde |
|---|---|---|
| `z-100-pc-disk-based-diagnostics-rev-r1.4`, `ms-dos-version-2-for-the-z-100-pc`, `microsoft-fortran-compiler-v3.20_202206` (Uploader `supaseibz`) | GW-SCP „(flux 0-39)" 12,8–19,2 MB **plus** `.img` 327 680/368 640 B je Diskette; PD-Mark. Z-100 = Zenith, 5,25" MFM. Inhalt Zenith/Microsoft → PRÜFEN | Eigentümer-Entscheid; wäre eine zweite gw-Hand mit `.img` |
| `color-flex-5.0.4-…`, `ed-editor-flex-…` (Uploader `n6il`) | ZIP mit SCP „made from a physical disk using a GreaseWeazle" + DMK-Konversion + SDF; PD-Mark auf FHL-Software → PRÜFEN | `dmk` T2 → T1b braucht reales Paar; DMK ist aber Konversion, keine Zweitlesung |
| `lexitroncpm` (PD-Mark) | 9 GW-SCPs (8,7–9,0 MB), Herkunft wörtlich: „written to physical media using a kyroflux … re-imaged with greaseweazle" — ein Schreib-/Lese-Rundlauf über **zwei** Geräte; Quellabbilder im Objekt `lexitron-vt-disks` | Lexitron VT1303 ist eines der acht 8"-Formate unter Moratorium (FloppyTools-Gutachten) |
| `davidlrand`: `colonial-data-sb80-cpm`, `xerox-820-cpm` (49 IMD), `xerox-16-8-cpm`, `ade-elettronica-mk3000-cpm`, `siemens-pc-mx2-sinix-s3510` (18 IMD) | **nur IMD/TD0/MFI, kein Fluss** — außerhalb des Auftrags; als reale IMD-Bestände für `imd`/`td0`-Hebung notiert | Hebungs-Slot |
| `dig-dogs-streetbusters-1994-3.5de` | `_raw.zip` 29 957 155 B + `_img.zip`, anderer Uploader, **kein** Lizenzfeld | zweite DTC-Hand derselben Software (andere Pressung) — nur nach Nr. 2 sinnvoll |
| `amiga_amc_kfraw` (Astro Marine Corps), `kings_quest_vi_de_kfx`, King's-Quest-1985-Objekt (pcjs-Blog) | DTC-Ströme kommerzieller, teils kopiergeschützter Titel; **kein** Lizenzfeld → ROT | Kopierschutz-Fragen mit benannter Diskette |
| `dbalsom/fluxfox` `tests/images/monster_disk/` | `monster_disk_kryoflux_360k.zip` 15 747 426 B + `.pri`; laut `tests/prolok.rs` eine **Prolok**-Kopierschutz-Testdiskette; Commit `c96ce5b9` 2024-11-12 | konkrete Prolok-Frage (Katalog `src/protection/` hat keinen Aufrufer, P0-2) |
| `dbalsom/fluxfox` `tests/images/transylvania/` | 7 Container, **kein Fluss**, eigene Rechte-Notiz (siehe Nr. 1) | — |
| `yas-sim/fdc_bitstream` `test_data/*.raw` | **kein KryoFlux-Strom** — Textkopf `**TRACK_RANGE 0 79 / **SAMPLING_RATE 4000000 / **BIT_RATE 500000` = hauseigenes Format. Verworfen, damit der Name niemanden fängt | — |
| **SuperCard-Pro-Hardware-SCP** (Auftrag Prio 3 im engen Sinn) | **Nicht gefunden mit Lizenztext.** Alle gemessenen SCP-Köpfe tragen Version-Byte 0x00 + Flags 0x23 — die Greaseweazle-Signatur. Atari-ST-SCP-Sätze (atari-forum, DrCoolZic) existieren, Seite nicht lesbar (Cloudflare/404), keine Rechteangabe | ein Objekt mit Rechte-Feld **und** SCP-Software-Version im Kopf |

## 5. Kennzahl-Frage — ehrlich gestellt

Regel 9 verlangt eine der vier Zahlen. Gemessen:

* **T3 runter** bewegen **Nr. 1** (`86f`, `pri`) und **Nr. 5** (`cpm`) direkt.
* **Nr. 2, 3, 4** bewegen keine der vier: der KryoFlux-Strompfad ist kein
  tier-geführtes Format (§2), und „Bench-Alter" (`docs/CAPABILITIES.md`
  §MF-589) zählt Termine an **echtem Gerät** — ein aufgezeichneter Strom
  ist keiner.

Vorschlag einer **fünften Zahl**, begründet aus P3-247 selbst und
**gemessen** am Manifest: *Gerätepfade mit mindestens einem aufgezeichneten
echten Strom im Korpus.* Heute **1 von 9** (Applesauce, kor_a/b/c); der
Greaseweazle-Eintrag zählt **nicht**, weil `gw_amigados.scp` per
`gw convert` aus einem ADF synthetisiert ist. Nach Nr. 1–5: **3 von 9**
(Applesauce, Greaseweazle, KryoFlux). Ob diese Zahl geführt wird,
entscheidet der Eigentümer; bis dahin sind Nr. 2–4 formal **Fundus mit
Auftragsbezug** und stehen hier trotzdem, weil P3-247 sie ausdrücklich
anfordert.

## 6. Messplan — Differenzlauf je Ebene (kein Code, nur der Weg für Stufe 4)

**Ebene Strom (Nr. 1, 2, 3):**

* **Beide Hände:** UFT `uft_kf_decode()` (`src/flux/uft_kryoflux_stream.c`,
  über einen Test nach dem Muster von `tests/test_kryoflux_stream.c`, der
  statt Byte-Feldern eine Datei liest) gegen (a) `gw` 1.23 — registriertes
  Oracle, Unlicense — dessen KryoFlux-Leser die Flusswerte in `sck`-Ticks
  liefert (Ausführung: `gw convert trackNN.S.raw out.scp` und Rücklesen, oder
  die Python-Klasse direkt), und (b) FloppyTools `kryostream.py` (BSD-2) als
  zweite Hand.
* **Metrik:** Anzahl Flusswerte, Anzahl Index-Marken, Werte byteweise
  (Ticks) identisch; Index-Positionen ± 0 Zellen. Erwartung aus §3:
  `track00.0.raw` von Nr. 1 → 4 Index; Nr. 2 → 6 Index / 455 095 Werte;
  Nr. 3 → 6 Index / 253 976 Werte. **Weicht UFT von der eigenen Vorzählung
  hier ab, ist das der Befund.**
* **P3-125 (MF-867):** an Nr. 3 prüfen, ob der StreamInfo-Positionsabgleich
  auf echten DTC-Blöcken (8 bzw. 14 je Spur) still bleibt.
* **Toleranzliste:** (a) Nop-Bytes zählen nicht als Fluss; (b) FloppyTools
  liest nur `.raw`-Namensschema `…NN.H.raw` (Vorgutachten U3); (c) Nr. 1 ist
  gw-geschrieben — Abweichungen zwischen gw-Schreiber und DTC-Schreiber
  (z. B. Reihenfolge KFInfo/StreamInfo, `hc=`-Feld fehlt bei gw) sind
  Befund, nicht Fehler.

**Ebene Sektor (Nr. 1, 2, 4, 5):**

* UFT-Flusspfad (SCP-Plugin T1b bzw. KF-Strom → `flux_decode_track()`)
  → Sektorbytes gegen `.img` (Nr. 1: 368 640 B; Nr. 2: 1 474 560 B) bzw.
  IMD (Nr. 4, 5) **byteweise**; Erfolgskriterium 100 % oder benannte
  Sektoren.
* Nr. 5 zusätzlich: Zylinder 0/Kopf 0 durch `flux_decode_fm()` — 18 × 128 B
  gegen IMD-Spur 0; danach `cpmls` mit der Diskdef aus `disk-formats.md`
  gegen UFTs `cpm`-Verzeichnis (Oracle vorher kalibrieren, §ORACLES
  „Stand der Kalibrierung": `cpmls` ist ungemessen).
* Nr. 1 zusätzlich: `.86f` und `.pri` durch die T3-Plugins, Sektorbytes
  gegen dasselbe `.img` — das ist die T3-Hebung.

**Einhängepunkt (im Baum auffindbar):** `docs/VERIFICATION_PLAN.md`
§„Korpus-Politik" (Z. 171) und §„Phase 2 — Korpus + Hebungen" (Z. 203);
für Nr. 5 zusätzlich `docs/OPEN_ITEMS.md` P3-123c; für die Oracle-Seite
`docs/ORACLES.md` §„Vorgemerkt" (FloppyTools, Vorgutachten V2).

## 7. Vorschlagsblock für `docs/OPEN_ITEMS.md` (5 von max. 5)

> Zur Übernahme durch einen Menschen. Ein Vorschlag ist kein Eintrag.

```
| KOR-d | fluxfox sector_test beschaffen: github.com/dbalsom/fluxfox
        tests/images/sector_test/ — sector_test_kryoflux_360k.zip 2 387 173 B
        (SHA-256 26c0c077…def00e, 80 Stroeme, KFInfo woertlich
        "name=Greaseweazle, version=1.18, host_date=2024.11.09", 4 Index =
        3 Umdrehungen) + sector_test_360k.scp 18 175 764 B (Kopf gemessen:
        3 Umdr., Spuren 0-79, Flags 0x23) + .img 368 640 B + .86f/.pri/.imd/
        .td0/.mfi/.hfe derselben Diskette. Lizenz MIT (LICENSE woertlich,
        Zone GRUEN; Ordner ohne eigene Rechte-Notiz, anders als transylvania/).
        Kennzahl: T3 RUNTER — 86f (Z.74, nur Spec-Test) und pri (Z.96, kein
        Test) gegen dasselbe .img; dazu erster AUFGEZEICHNETER GW-Strom und
        erster KryoFlux-Protokoll-Strom im Korpus. UNGEKLAERT: Erzeuger je
        Container nicht benannt. |
| KOR-e | DTC-Strom beschaffen: archive.org dig-dogs_streetbusters_kryoflux —
        ZIP 28 258 736 B md5 3d96081d (168 Stroeme track00..83 x2, KFInfo
        woertlich "KryoFlux DiskSystem, version=3.00s", 6 Index = 5 Umdr.,
        455 095 Flusswerte in track00.0.raw) + .img 1 474 560 B md5 b614ac59
        als Sektorreferenz. Rechte: PD-Mark 1.0 + "PC Freeware - frei
        kopierbar" (Uploader-Aussage, nicht in der Matrix) -> ZONE PRUEFEN,
        Weg wie kor_a (gitignored + Manifest). Kennzahl: keine der vier;
        fuenfte vorgeschlagen (Geraetepfade mit echtem Strom 1/9 -> 3/9).
        Der einzige frei erreichbare DTC-Strom einer PC-MFM-Diskette mit
        Gegenstueck. |
| KOR-f | Q1-Stroeme fuer die STROM-Ebene (Neubesuch, Anlass P3-247):
        Datamuseum-DK/FloppyToolsExamples q1/ — 173 .raw, BSD-2 woertlich;
        000_bin00.0.raw 254 404 B SHA-256 304df9bc…3606, KFInfo "KryoFlux
        DiskSystem, version=3.00s, host_date=2024.04.11", 6 Index, 8
        StreamInfo, 253 976 Flusswerte. Zweite Hand: FloppyTools
        kryostream.py (BSD-2). Inhalt Q1 = Moratorium, wird NICHT dekodiert;
        gemessen wird nur uft_kf_decode() gegen zwei fremde Leser, plus
        P3-125 an 35 Lesungen. Kennzahl: fuenfte (s. KOR-e). |
| KOR-g | GW-Aufnahme einer historischen Diskette: archive.org
        trs80-model3-cpm-cheap — model-iii-cpm.1.15.scp 9 648 021 B md5
        8dab1046 (Kopf gemessen: 3 Umdr., Spuren 0-80, Seite 0, Flags 0x23;
        Uploader woertlich "Greaseweazle flux master", "40-track SS, 10x512
        MFM") + .imd 205 876 B md5 73979800. Lizenz CC-BY-4.0 im Wortlaut
        fuer das Material des Uploaders; CP/M-2.2 (DRI) ausdruecklich nicht
        lizenziert -> ZONE PRUEFEN. Kennzahl: imd T2->T1b (Hebungswaehrung);
        fuenfte. UNGEKLAERT: gw-Version/Firmware/Laufwerk nicht benannt. |
| KOR-h | cpm von T3 heben + FM auf 5,25": archive.org toshiba-t250-t200-cpm
        T-200/boot-v2.30a-1024.scp 17 159 065 B md5 016c8a84 (Kopf gemessen:
        3 Umdr., 35 Zyl. x 2, Flags 0x23) + .imd 340 114 B md5 8e84ccbc +
        .mfi + docs/disk-formats.md (Geometrie aus bios64.asm rev 2.12,
        fertige cpmtools-diskdefs; Zyl 0/Kopf 0 = FM 18x128, Rest MFM 5x1024).
        BIOS des Uploaders selbst -> sauberste der drei T-200-Disketten;
        CC-BY-4.0 im Wortlaut, DRI-Anteil wie KOR-g -> ZONE PRUEFEN.
        Kennzahl: T3 RUNTER (cpm, Z.78) via cpmls (vorher kalibrieren);
        Zweitnutzen P3-123c (FM-Spur in echter GW-Aufnahme, 5,25"). |
```

Nicht vorgeschlagen (Regel 9 / Vorbedingung): Z-100-Trio, FLEX-Paare,
Lexitron, Rand-IMD-Bestände, Dig-Dogs 1994, Amiga/Sierra-Ströme,
monster_disk — alles in §4 benannt wartend.

## 8. Nächster Griff — EINE Datei, EINE Messung

**Die Datei liegt schon:** `tools/uft-scout/work/p3-247/sector_test_kryoflux_360k.zip`
(SHA-256 `26c0c077…def00e`), darin `track00.0.raw` (127 987 B).

1. SHA-256 des ZIP und der entpackten `track00.0.raw` ins Manifest
   (Herkunft: Repo-URL + Commit `2b852552` + KFInfo wörtlich + Abrufdatum
   2026-09-07; `rechte`: MIT, `LICENSE` zitiert).
2. **Erste Messung:** `uft_kf_decode()` über die Dateibytes; Erwartung aus
   der unabhängigen Vorzählung dieses Gutachtens: **4 Index-OOBs**, KFInfo
   `name=Greaseweazle, version=1.18`, `sck=24027428.5714286`. Jede
   Abweichung ist ein Befund gegen den Parser oder gegen meinen Zähler —
   beides wird benannt, nicht wegerklärt.
3. **Zweite Messung:** `gw convert track00.0.raw t0.scp` mit dem gepinnten
   gw 1.23, dann Flusswerte in Ticks gegen UFTs Ausgabe. Identisch → der
   Parser hat zum ersten Mal einen Strom gelesen, den eine fremde Hand
   geschrieben hat.
4. Erst danach Nr. 2 (DTC-Strom) auf demselben Weg.

## 9. UNGEKLÄRT

* **U1:** Wer die 17 Nicht-Fluss-Container in fluxfox `sector_test/` erzeugt
  hat (gw, fluxfox selbst, 86Box, MAME) — Commit-Nachricht nennt es nicht.
  Für Sektorbytes unerheblich, für „fremde Hand je Container" nicht.
* **U2:** gw-Version, Firmware und Laufwerk bei Nr. 4 und Nr. 5 — am Objekt
  nicht benannt; nur das Rezept (`gw read … --format=ibm.scan`) steht.
* **U3:** Rechtsstatus des Dig-Dogs-Inhalts (PD-Mark ist Uploader-Aussage;
  Rechteinhaber Deutscher Verkehrssicherheitsrat e.V.) — Eigentümer-Vorlage.
* **U4:** CC-BY-4.0 ist in `playbook/lizenzmatrix.md` nicht geführt —
  Eigentümer-Einordnung für Daten (nicht Code); der Scout ordnet nicht ein.
* **U5:** Ob DRI-CP/M-2.2-Anteile auf Nr. 4/5 die Manifest-Politik
  berühren — dieselbe Frage wie bei kor_a (dort am 2026-09-04 entschieden:
  beschaffen und messen, Weitergabe nicht).
* **U6:** Ob es überhaupt ein frei lizenziertes SCP gibt, das von
  **SuperCard-Pro-Software** (nicht gw) geschrieben wurde — nicht gefunden;
  der KryoFlux-Forum-Thread und die Atari-Forum-Seiten waren nicht lesbar.
* **U7:** Erzeuger der Dig-Dogs-`.img` (mutmaßlich DTC `-i4`) — nicht benannt.

## 10. Nicht geprüft

* Kein Strom wurde durch UFT geschickt; alle Zahlen stammen aus eigenen
  Zählern und Dateiköpfen.
* archive.org-Objekte **ohne** Lizenzfeld (1999 von 2000 KryoFlux-Treffern)
  nur stichprobenhaft (2 Objekte) angesehen.
* Die 1,2-MB-Sätze in fluxfox und `monster_disk` nicht heruntergeladen.
* Quellen außerhalb archive.org/GitHub (SPS-Forum, a8preservation, c64pp-
  KryoFlux-Bestände) — nicht erreichbar oder nicht gesichtet.
