# Plan: Modernisierung — fremder Code herein, Schwachstellen heraus

> **Zweck.** Ein Fahrplan in Phasen, von denen jede in einer FRISCHEN
> Sitzung ausführbar ist. Jede Phase nennt, was sie liest, was sie baut,
> woran sie scheitert und wie man sieht, dass sie getragen hat.
>
> **Dies ist kein zweites Standdokument.** Der Gesamtstand steht in
> [`STAND.md`](STAND.md), die offenen Punkte in
> [`OPEN_ITEMS.md`](OPEN_ITEMS.md), die laufende Warteschlange in
> [`.claude/AUFGABEN.md`](../.claude/AUFGABEN.md). Zahlen, die auch dort
> stehen, sind hier **Verweis und nicht Quelle**.

**Auftrag, wörtlich:** „nim die Verbesserungen die schon gemacht wurden
mit auf nimm alle den fremd code aus `neue-ideen` und verbessere ihn,
gehe den gesamt code durch suche nach schwachstellen und verbesserungen,
das mein tool besser macht und nicht auf den selben alten code sitzen
bleibt es soll besser moderner und leistungsstärker machen, fange dann
zuerst mit IMD→IMG an."

---

## Phase 0 — Was schon gemessen ist

**Diese Phase ist abgeschlossen.** Sie steht hier, weil jede spätere
Phase auf ihren Zahlen aufbaut und eine neue Sitzung sie sonst neu
erheben müsste.

### 0.1 Was seit MF-1265 gebaut wurde

| MF | Was | Beleg |
|---|---|---|
| 1265 | Kopierplan-Quelle im Kern (`uft_copy_plan_set_quelle`) | `f3fdb428` |
| 1266–1271 | A-032-Register: `neue-ideen/` erfasst, 217 Einträge, **170 mit Urteil**, 47 offen | `docs/NEUE_IDEEN_REGISTER.md` |
| 1272 | **Das Zentrum `uft_disk2`** — vier Schichten mit Herkunft und Zuversicht | `7fd2fb02` |
| 1273 | Erster **Produktivleser**: der Disk-Analyzer zeigt den Trägerbericht | `095289e9` |
| 1274 | **Zweite Fassung** des Zentrums: Generationen, Ableitungsregister, Stimmen je Bit, mehrere Dateisysteme, `validate()` | `296c7835` |
| 1275 | **UFTD** — der Behälter, der alles trägt; Analyzer schreibt ihn | `04007f22` |
| 1276 | **FAT12-Schleifenbremse** sah nur 16 Glieder; Kettenläufer hatte 0 Tests | `87a2629f` |

**Die Linie dieser sieben Commits ist der Maßstab für alles Weitere:**
ein Stück wird erst eingebaut, wenn es (a) einen Produktivaufrufer hat,
(b) einen Test, der rot wird, wenn der Aufruf verschwindet, und (c) eine
Zahl, die gemessen und nicht geschätzt ist.

### 0.2 Die Wandlungspfade, vollständig vermessen

Gemessen mit einem Wegwerf-Prüfstand gegen die **echte** Produktions-API
`uft_convert_file()` (Job-Verzeichnis, kein Baumbestandteil):

```
Wandlungspfade in g_conversion_paths[]  : 46  (+1 UNKNOWN-Wächter)
davon mit Eintrag in g_matrix[]         : 17  (7 LL, 8 LD, 2 UNMÖGLICH)
davon ohne Eintrag                      : 29
```

Die 29 scheitern aus **drei verschiedenen Gründen**, und nur der erste
ist Gegenstand dieses Plans:

| Ursache | Pfade | rc | Deutung |
|---|---|---|---|
| **Preflight ABORT, kein Matrix-Eintrag** | **18** | −40 | Quelle öffnet sauber, Wandler wäre bereit. **Das ist die Arbeit.** |
| Quellformat mehrdeutig | 5 | −25 | Die Sonde weigert sich zu raten (MF-1153) — korrekt, eigener Posten |
| Kein Pfad von der ZUGEWIESENEN ID | 4 | −40 | `KRYOFLUX→*`: die Tabelle nennt `UFT_FORMAT_KRYOFLUX`, `uft_format_plugin_kfx` deklariert `UFT_FORMAT_DSK` |
| Keine Quelldatei, kein Plugin | 2 | — | `NBZ` — die ID trägt **keine** Registry |

**Der dritte Grund ist der teuerste Befund dieser Messung.** Von **17**
`.img`-Korpusdateien werden nur **zwei** als `plugin='IMG'`
(`format-id=2`) erkannt; die übrigen holen sich MSX, Victor9K,
Micropolis, NorthStar oder HardSector — und die deklarieren **alle**
`format-id=5`, also `UFT_FORMAT_DSK`. Die Wandlungstabelle ist auf
Format-IDs verschlüsselt, aber der ID-Raum ist eingeschmolzen (MF-1087:
101 Plugins tragen `UFT_FORMAT_DSK`). Ein `IMG→ADF` kann für diese
Dateien nie feuern.

**Gemessen und festgehalten:** `accept_data_loss` hilft bei einem als
UNGEPRÜFT geführten Paar **nicht** — der Preflight sperrt bei jedem
Preis. Das ist die Bauart aus MF-263/UFT-A01 und richtig; der einzige
Weg ist der Eintrag mit Beleg.

### 0.3 Was `neue-ideen/` hergibt

Aus dem Register (MF-1266…1271) und den Messungen dieser Sitzung:

* **217 Einträge, 170 mit Urteil, 47 offen.**
* **Fünf Sammelordner** (`1`, `copy`, `floppy1`, `fertige`, `exsource`)
  halten **1789 von 2408** Dateien — eine offene Entwurfsfrage für den
  Eigentümer, kein technisches Hindernis.
* **Drei der sechs vermissten Module liegen im Baum** und wurden nur
  nicht gefunden, weil die Pakete **deutsche** Namen tragen und die
  Modulnamen INNEN stehen:
  * `neue-ideen/UFT-NN — FAT12 lesen.zip` → `uft_fat_robust` ✅ **eingebaut (MF-1276)**
  * `neue-ideen/Apple-Sektorordnung.zip` → `uft_a2_order` — offen
  * `neue-ideen/UFT-NN — Amiga.zip` → `uft_amiga_media` — offen
  * `uft_revolution`, `uft_protection_scan`, `uft_splice` — **kein Paket
    gefunden** (0 Treffer außerhalb der Aufgabenliste)
* **Gesperrt oder mit offener Lizenzfrage** (nicht anfassen, bis der
  Eigentümer entscheidet): OmniFlop-239-Format-Harvest, `dtc_code/`
  (Zone ROT), `x50conv.exe` (Disassemblierung verboten), `samdisk_plus`
  (nicht-kommerzielle Klausel, `P3-514`), `capsimage`, ST-Recover
  (Ms-RL), OpenCBM.

---

## Phase 1 — `IMD→IMG` (der erste Pfad)

**Diese Phase ist vollständig vorgemessen und kann sofort gebaut
werden.**

### Was gemessen ist

```
quelle : tests/corpus_free/hxcfe_pc160.imd  (164 785 Byte, von HxC erzeugt)
imd    : 40 Spuren, 40 Zylinder, 1 Kopf, 0 defekte, 0 nicht verfügbare
wandler: rc=0  ausgabe=163 840 Byte  spuren=40
VERGLEICH gegen tests/corpus_free/uft_pc160.img:
         0 von 163 840 Byte abweichend
```

**Warum das ein Beleg ist und keine Gleichheit ohne Aussage** (MF-1039):
die Quelle kommt aus **fremder Hand** (HxC), und ein flaches
Sektorabbild von 163 840 Byte = 40 × 1 × 8 × 512 hat keine
Wahlfreiheit — es ist der Sektorinhalt selbst. Dass HxCs IMD-Kodierung
über unseren Leser byteweise dieselben Nutzdaten ergibt wie das flache
Abbild, ist eine Aussage über den Leser.

### Die Verlustliste, aus den FELDERN belegt

`IMD→IMG` ist **verlustbehaftet**, und zwar in genau neun Posten,
ablesbar an `uft_imd_image_t` / `uft_imd_track_t`
(`include/uft/formats/uft_imd.h`):

| # | Was verloren geht | Feld |
|---|---|---|
| 1 | Erzeugerstempel (Datum, Uhrzeit, Version) | `header.day/month/year/hour/minute/second`, `version_*` |
| 2 | Kommentartext | `comment` |
| 3 | Modus je Spur (Datenrate, FM/MFM) | `header.mode` |
| 4 | Sektornummernkarte (Interleave) | `smap[]` |
| 5 | Zylinder-/Kopfkarte | `cmap[]`, `hmap[]`, `has_cylmap`, `has_headmap` |
| 6 | Variable Sektorgrößen | `ssize[]`, `has_varsizes` |
| 7 | Sektortypen (normal/gepackt/gelöscht/defekt/fehlend) | `stype[]` |
| 8 | Die Zähler | `compressed_sectors`, `deleted_sectors`, `bad_sectors`, `unavail_sectors` |
| 9 | **Fehlende Sektoren werden mit `0xE5` gefüllt** | `uft_imd_to_raw(&imd, …, 0xE5)` |

**Posten 9 ist der forensisch wichtige.** Ein fehlender Sektor wird im
IMG zu `0xE5` und ist danach von einem Sektor, der wirklich `0xE5`
enthält, nicht mehr zu unterscheiden. Der Wandler **zählt und meldet**
ihn (`uftc_add_warning` bei `bad_sectors > 0 || unavail_sectors > 0`),
aber die Zieldatei kann den Unterschied nicht tragen. Genau das muss der
Matrix-Eintrag aussprechen — „Kein Bit verloren, keine erfundenen
Daten" heißt hier: die Erfindung ist unvermeidlich, also wird sie
benannt.

### Zu bauen

1. **Eintrag in `src/core/uft_roundtrip.c`, `g_matrix[]`:**
   `{ UFT_FORMAT_IMD, UFT_FORMAT_IMG, UFT_RT_LOSSY_DOCUMENTED, "…" }`
   mit der Notiz, die die neun Posten nennt und die Messung zitiert.
   Struktur: `uft_roundtrip_entry_t { from, to, status, note }`
   (`include/uft/core/uft_roundtrip.h:60-68`).
2. **Test `tests/test_convert_imd_img_belegt.c`** mit vier Gruppen:
   * die Wandlung liefert **163 840 Byte, 0 abweichend** gegen
     `uft_pc160.img`
   * `uft_convert_file()` läuft jetzt **durch** (vorher `rc=-40`)
   * die neun Verlustposten sind in der Matrixnotiz **genannt** (der
     Test liest `uft_roundtrip_note()` und prüft die Stichworte)
   * ein IMD **mit** defekten Sektoren erzeugt die Warnung, und die
     Zahl darin stimmt mit `imd.bad_sectors` überein
3. **Linkregel** in `tests/CMakeLists.txt` nach dem Muster von
   `test_convert_file_detection` (Zeile 2904 ff.).

### Rotbeweis (vor dem Bau)

| Mutation | Muss fallen |
|---|---|
| Matrix-Eintrag entfernt | „läuft durch" fällt, `rc=-40` kehrt zurück |
| `0xE5` im Wandler durch `0x00` ersetzt | die Byte-Identität fällt |
| Die Warnung bei `bad_sectors` unterdrückt | die vierte Gruppe fällt |
| Ein Stichwort aus der Notiz gestrichen | die dritte Gruppe fällt |

### Kennzahl

**Angebotene Wandlungspfade: 15 → 16.** Das ist die Zahl aus MF-640,
und sie bewegt sich hier, weil eine Messung vorliegt — nicht, weil ein
Eintrag hinzugefügt wurde (MF-1077: Kennzahlen sind Folgen, keine
Ziele).

### Anti-Muster, ausdrücklich verboten

* **Keinen Eintrag ohne Messung.** Bei `SCP↔HFE` hat genau das einmal
  `LOSSLESS` behauptet, und es war falsch (MF-527).
* **Nicht `LOSSLESS` eintragen**, weil die eine Prüfdiskette 0
  Abweichungen zeigt. Sie hat 0 defekte und 0 fehlende Sektoren; die
  Verlustposten 1–8 fallen trotzdem an. `LOSSY_DOCUMENTED` ist die
  ehrliche Stufe.
* **Den Preflight nicht umgehen.** `accept_data_loss` ist gemessen
  wirkungslos bei UNGEPRÜFT, und das soll so bleiben.

---

## Phase 2 — Die übrigen 17 Pfade

Dieselbe Bauform wie Phase 1, **ein Pfad je Commit**. Reihenfolge nach
Stärke des möglichen Belegs:

**Stufe A — Sektorziel, Quelle aus fremder Hand** (Byte-Identität
möglich): `TD0→IMD`, `TD0→IMG`, `STX→IMG`, `IMD→IMG` ✓ (Phase 1).

**Stufe B — Flussziel oder Bitstromziel** (aufzählbare Verlustliste
statt Identität): `D64→SCP`, `D64→HFE`, `G64→SCP`, `G64→HFE`,
`SCP→G64`, `HFE→G64`, `IPF→SCP`, `STX→SCP`.

**Stufe C — Ziel-ID mehrdeutig** (`→DSK`, 101 Plugins tragen die ID):
`WOZ→DSK`, `NIB→DSK`, `STX→DSK`. **Diese drei bekommen KEINEN
Matrix-Eintrag**, sondern einen Posten in `OPEN_ITEMS.md`: ein
Wandlungsziel, das 101 Formate meint, ist kein Ziel. Das ist eine
Entwurfsfrage, keine Messfrage.

**Stufe D — Verlust benannt, Quelle aus fremder Hand**: `IPF→ADF`,
`IPF→IMG`, `WOZ→NIB`.

Je Pfad gilt: Beleg **zuerst** (Wegwerf-Prüfstand im Job-Verzeichnis),
Eintrag **danach**. Wer die Messung nicht besteht, bekommt einen Eintrag
als `UFT_RT_IMPOSSIBLE` **mit gemessener Begründung** — das bewegt die
Kennzahl nicht nach oben, macht aber aus „stillschweigend abgewiesen"
ein benanntes Urteil.

---

## Phase 3 — Die vier blockierten Pfade und der ID-Einschmelz

**Kein Wandlungsproblem, sondern ein Identitätsproblem.** Zu bauen ist
hier nichts, bevor eine Entscheidung fällt:

* `KRYOFLUX→SCP/HFE/D64/ADF`: die Tabelle nennt `UFT_FORMAT_KRYOFLUX`,
  der Leser `uft_format_plugin_kfx` deklariert `UFT_FORMAT_DSK`. Zwei
  Wege: (a) das Plugin trägt künftig `UFT_FORMAT_KRYOFLUX` — berührt
  die Erkennung; (b) die Tabelle nennt `UFT_FORMAT_DSK` — macht den
  Pfad für 101 Formate gültig, also falsch. **Weg (a) ist der
  richtige**, braucht aber eine Messung, was sich an der Erkennung
  ändert.
* `NBZ→D64/G64`: die ID trägt **kein** Plugin und **keine** Registry.
  Entweder ein Leser, oder die zwei Pfade werden als `IMPOSSIBLE` mit
  Begründung eingetragen.
* Die fünf Pfade mit mehrdeutiger Quelle sind ein **Sonden**-Posten,
  kein Wandlungsposten (MF-1153).

**Alles in diesem Abschnitt gehört nach `OPEN_ITEMS.md`, bevor eine
Zeile Code entsteht.**

---

## Phase 4 — `neue-ideen/` hereinholen

**Regel für jedes Paket, ohne Ausnahme:**

1. **Lizenz an der DATEI messen**, nicht am Projektfeld (die
   GitLab/GitHub-API meldet die Einstellung, nicht die Datei — `fdtc`
   galt so als „ohne Lizenz" und trägt BSD-3).
2. **Namenskollisionen messen**, bevor eine Zeile geschrieben wird.
   Bei `uft_disk2` waren es sieben, bei `uft_fat_robust` drei — und in
   beiden Fällen entschied die Messung den Entwurf.
3. **Nicht danebenstellen, wenn es den Begriff schon gibt.** Bei
   `uft_fat_robust` hätte ein eigenes Modul den **dritten**
   FAT12-Kettenläufer ergeben. D3.
4. **Die Aussagen der Ausarbeitung über UNSEREN Baum nachmessen.** Bei
   `uft_fat_robust` trafen drei von vier zu; die vierte („854 Zeilen")
   war gedriftet (gemessen 884). Bei MF-1226 hat genau diese Klasse
   eine falsche Einstufung getragen.
5. **Kanal nach MF-695 benennen** und die Attribution in den Dateikopf
   (MF-636) — **ein NOTICE gibt es in diesem Baum nicht**.

**Nächste Griffe, gemessen:**

| Paket | Modul | Stand |
|---|---|---|
| `UFT-NN — FAT12 lesen.zip` | `uft_fat_robust` | ✅ MF-1276 |
| `Apple-Sektorordnung.zip` | `uft_a2_order` | offen — trifft `uft_sector_order.h` (MF-1175), **Kollision prüfen** |
| `UFT-NN — Amiga.zip` | `uft_amiga_media` | offen |
| `exsource/*.zip` (24 Stück) | diverse | offen, Lizenzen je Paket messen |
| `uft_revolution`, `uft_protection_scan`, `uft_splice` | — | **Paket nicht gefunden**, beim Eigentümer nachfragen |

### Der Zensus, gemessen (2026-09-20)

Volldokument:
`tools/uft-innendienst/out/inventar_neue-ideen_2026-09-20.md`.

**Bestand: 2409 Dateien** (`find neue-ideen -type f | wc -l`) — das
Register nennt 2408, die Abweichung von 1 ist ungeklärt und steht so da.
Davon: 70 eigene `UFT*`-Ausarbeitungen oberster Ebene, 10 eigene
Code-Zips, 19 Fremdklone oberster Ebene + 112 in Sammelordnern, 10
`.tar.gz`, 7 `.lha`, 33 Binärabbilder. Die fünf Sammelordner tragen
**exakt 1789** Dateien (143 + 176 + 92 + 1354 + 24) — deckungsgleich mit
dem Register; die 1002 HTML liegen **alle** in `fertige/`.

**Schon im Baum** (je Symbol mit `git grep -l … | wc -l`): `uft_scp_integrity`
(6 Dateien), `uft_sector_order` (6), `uft_track_layout` (5),
`uft_disk_compare` (6), `uft_floppy_reference` (7). **Damit ist
`P3-422`/MF-1173 beantwortet: 2 von 3 Layout-Modulen sind drin**, es
fehlt `uft_gap_drive`.

**Fehlt (0 Treffer):** `uft_gap_drive`, `uft_media_analysis`,
`uft_aard_artifact`, `uft_c64pp_catalog`, `uft_roland_s`, `uft_jv13`,
`uft_image_meta`, `disk_algorithms`, die `csrc`-Bezeichner. **Zwei
Nullen heißen dabei NICHT „fehlt":** TRS-80 ist über `jv3_jvc.h`
eingelöst (MF-1017), Roland über `uft_roland_ident.h` (MF-1176). Und ein
Treffer war ein Fehltreffer: `uft_vsn` traf das Makro `uft_vsnprintf`.

**Rangliste der nächsten Griffe** (Lizenz klar, Kennzahl benannt, kein
neues Format-Plugin; Aufwand in MF-Zyklen als Spanne):

| # | Paket | Lizenz | Kanal | Kennzahl | Aufwand |
|---|---|---|---|---|---|
| 1 | `akaiutil-4.6.8` | GPL-2 | Oracle | **T3 runter** (`akai_s900`, P3-403) | 0,5–1,5 |
| 2 | `uft_gap_drive`-Rest | eigen | Port | Wandlungspfade rauf | 1–2 |
| 3 | `AmigaDiskKit` | MIT | Daten/Fixture | Korpus | 0,5–1 |
| 4 | `KCemu` | GPL-2 | Oracle `td0` | T3 runter | 0,5–1,5 |
| 5 | `dmklib` | GPL-2 | Oracle `dmk`/RX02 | T3 runter | 0,5–1,5 |

Nachrücker nach je **einem** Lizenzschritt: `UFT_Media_Analysis` und
`UFT_Floppy_Reference_AARD` (beide „LGPL, Version unbestimmt", beide
HALB im Baum — Volltext lesen), `OpenCBM` nach Wurzellizenz-Messung.
`Roland_S` und `uft-trs80` scheiden unter der EINFRIER-REGEL aus.

### **Eigentümervorlage: zwei Urteile über dieselbe Codefamilie**

`neue-ideen/SPStudio_Dev` trägt eine **240-Byte-`LICENCE.txt` von
KryoFlux selbst (2002–2026) mit Apache-2.0** und dem Verweis „See the
LICENSE file" — **der Volltext fehlt im Paket**. Der Inhalt IST die
CAPS-Bibliothek (`CAPSImg/`, `LibIPF/`), die das Register unter
`capsimage-` als **nicht frei** führt. Zwei unverbundene Urteile über
denselben Code. **Falls die Erklärung echt ist, wäre die IPF-Quarantäne
neu zu bewerten** — bis dahin gilt *Lizenz vor Fähigkeit* und es wird
nichts übernommen.

**Weiterhin gesperrt:** `dtc_code/` (Zone ROT), `x50conv.exe`
(Disassemblierverbot), `PastiImgKit`, `FastCopy_3`, `ipfdec5` (SPS
non-commercial), `capsimage-`/`caps.zip`, `IPF-Format.zip`,
`xcopypro_xmas93` (HAND-A-Sperre). **Offen:** OmniFlop-Harvest
(§§ 87a ff. UrhG), `samdisk_plus` (P3-514), `OpenCBM`, `ST-Recover`
(Ms-RL — GPL-2-Vereinbarkeit VOR Übernahme klären), `pc98-disk-tools`
(keine Lizenz), 7 `.lha` ohne Lizenzdatei.

---

## Phase 5 — Schwachstellen im eigenen Baum

Gesucht werden **die fünf Krankheiten, die dieses Projekt an sich
selbst kennt**. Was in dieser Sitzung schon gefunden wurde, steht als
Muster dabei:

| Klasse | Gefundenes Beispiel | Wo |
|---|---|---|
| Türen ohne Leser | `uft_batch_run` 0 Aufrufer; die 4 HAL-Profilfunktionen | bekannt |
| **Stille Kürzung** | FAT12-Schleifenbremse sah 16 von N Gliedern | ✅ MF-1276 |
| Wissen doppelt | `uft_fat12.c` gegen `uft_msx.c:463-491` — **zwei FAT12-Kettenläufer** | offen |
| **Funktionen ohne Wächter** | `uft_fat_get_chain` hatte 0 Tests | ✅ MF-1276 |
| Aufzählung statt Messung | 14 Fälle im Protokoll | laufend |

**Zwei Befunde dieser Sitzung, die noch keinen Posten haben:**

* `tests/test_convert_leaves_no_ghost.c` legt Prüfdateien unter FESTEN
  Namen im Arbeitsverzeichnis an (`uft_ghost_out.hfe`,
  `uft_ghost_in.adf/.hfe`, Zeilen 100/215/245) und ruft `remove()`
  darauf (151/269/270) — zwei `ctest`-Läufe räumen einander ab.
  Eingetragen als **`P3-516`**.
* Der zweite FAT12-Kettenläufer in `uft_msx.c` hat dieselben vier
  Lücken, die MF-1276 in `uft_fat12.c` geschlossen hat. Er gehört
  zusammengeführt, **bevor** eine dritte Umsetzung entsteht.

### Die zwölf Funde, gemessen (2026-09-20)

Volldokument: `tools/uft-innendienst/out/innendienst_2026-09-20.bericht.md`,
Rohdaten in `out/_k1_*.json` und `out/_k4_ohne_waechter.json`. Alles
statisch gemessen — **kein Bau, kein Testlauf**.

**Die drei schwersten, in dieser Reihenfolge abzuarbeiten:**

**F1 — Der PLL-Kernpfad kappt still und meldet Erfolg.**
`src/core/uft_decode_pipeline.c:68-69,118` klemmt den Bitstrom auf
`UFT_SESSION_MAX_BITS = 524288` (`uft_decode_session.h:42`),
`uft_pll.c:58-72` wirft den Rest weg, die Rückgabe ist **bedingungslos
`UFT_OK`**, und `pll.quality` misst nur PLL-Aussetzer, nicht die
Kappung. Ein Mehrfach-Umdrehungs-HD-Fluss (> 131 072 Flanken) verliert
**still die hintere Spur**. **Und kein Test ruft die Funktion.**
Exakt die Klasse MF-1001/1022/1038/1040/1135/1224 — die teuerste dieses
Baums — an der zentralsten Stelle.

**F2 — Ein Vertrag, zwei Fassungen, und nur die Include-Reihenfolge
entscheidet.** `uft_track_set_flux` ist **zweimal öffentlich deklariert**:
`include/uft/uft_track.h:271` als `int(…, double sample_rate_mhz)`,
`include/uft/uft_format_plugin.h:1378` als
`uft_error_t(…, uint32_t tick_ns)`. Die Definition
(`uft_format_plugin.c:753`) ist die **ns-Fassung**. Verschiedene
Rückgabetypen **und ein physikalisch anderer vierter Parameter** —
MHz gegen Nanosekunden. Heutige Rufer sind zufällig sicher; **jede neue
Übersetzungseinheit, die `uft_track.h` zuerst zieht, ruft mit
double-ABI eine uint32-Funktion — ohne Compiler- oder Linkerfehler.**
Der Kommentar `track.h:269` nennt `format_plugin` kanonisch, und die
Zeile darunter widerspricht ihm.

**F3 — Die Träger von vier Produktivpfaden haben null Tests.**
`uft_disk_stream_tracks` (`src/core/uft_disk_stream.c:95`) trägt
`uft_disk_convert.c:81`, `uft_disk_stats.c:107`, `uft_disk_verify.c:229`;
`uft_disk_stream_pair` (`:227`) trägt `uft_disk_compare.c:166` und
`uft_disk_verify.c:134`. **0 Testaufrufer.** Exakt die Lage, in der
`uft_fat_get_chain` vor MF-1276 war — und *verify* und *compare* sind
die **Prüfwege**.

**Die übrigen neun:**

| # | Fund | Klasse |
|---|---|---|
| F4 | `uft_crc16_ccitt`: **1 Definition, 4 Deklarationen** — eine davon (`uft_decoder_plugin.h:372`) mit **drei** Parametern statt zwei — plus **18 bitweise 0x1021-Nachbauten** und 2 Tabellen. Neuer MF-1177-Fall, **größer als die GCR-Tafel** | Wissen doppelt |
| F5 | **Schreibpfade erfinden `0xE5`-Füllsektoren ohne Meldung** (`uft_d64_plugin.c:232,234`, `uft_d81.c:175`, `uft_dc42.c:499`, `uft_2img.c:445`, `uft_adf_arc.c:107`; 12+ Dateien ohne Kennzeichen). Die MF-980-Kur erreichte **nur die Leseseite** | stille Erfindung |
| F6 | `uft_hxcstream.c:172-198`: vier stille `continue`-Löcher, Flusszahl auf 2 000 000 geklemmt, am Ende `is_open = true`. **Aber 0 Aufrufer** — wird bei Anschluss zur Bedingung | stille Kürzung, geparkt |
| F7 | `uft_format_convert_flux.c:1661-1663`: HFE→ADF klemmt wortlos auf 80/2, während drei Zeilen höher für einen anderen Fehler eine Warnung gesetzt wird. Eine 82-Zylinder-HFE verliert Zyl. 80/81 spurlos | stille Kürzung im **angebotenen** Pfad |
| F8 | `uft_hw_register_backend`: 5 Backends registrieren darüber, **0 Tests** | ohne Wächter |
| F9 | `uft_copy_plan_set_quelle` (MF-1265): GUI-Lebenszeitkopplung, **0 Tests** | ohne Wächter |
| F10 | **Der Soft-Flux-Decoder** (`flux_decode_track` + MFM/FM/GCR-C64/GCR-Apple, 17 Exporte) ist **komplett NUR-TESTS** — gebaut, getestet, **nicht angeboten**. Produktiv hängt nur `flux_decode_amiga_bits` | **Wandlungspfade rauf, unmittelbar** |
| F11 | **AmigaDOS-Dateisystem** (`uft_amigados.c:262` ff., samt `salvage`/`repair_bitmap`) **komplett NUR-TESTS**, 0 produktive Rufer. Spiegelbild von `uft_fat_get_chain` | **Wandlungspfade rauf** |
| F12 | **168 im Build stehende Dateien, deren SÄMTLICHE Exporte Waisen sind** (Spitze: `tzx_wav` 36, `recovery_meta` 36, `trs80` 26, `msx` 21). Symbol-Frage, nicht Modul-Frage — `audit_orphan_modules.py` kann diese 168 nicht sehen | Entscheidungsgrundlage |

**Reihenfolge, aus den Kennzahlen abgeleitet:** F1 und F2 zuerst (beide
können still falsche Daten erzeugen), dann F3 und F5, dann F10/F11 als
**Tor-Entscheid** — anschließen oder ausdrücklich parken; beides ist
eine Antwort, Schweigen ist keine.

**Was die Messung NICHT gesehen hat** (steht so im Bericht): nichts
gebaut, nichts gelaufen; Fremdbäume ausgenommen; präprozessor-gebaute
Namen und Zeichenkettentafeln unsichtbar; die 23 frischen
`uft_disk2`-NUR-TESTS-Symbole aus MF-1272…1275 sind bewusst **nicht**
als Krankheit gemeldet.

---

## Phase 6 — Abnahme

Für **jede** Phase, nicht erst am Ende:

```
cmake --build build-tests-ci -j 8              # 0 Warnungen
ctest -j 8                                     # alle grün, Zahl NENNEN
python scripts/audit_attribution_licence.py    # Rückstand <= Grundlinie
python scripts/check_consistency.py            # alle Tore
```

**Vor jedem Commit**, in dieser Reihenfolge:
`gen_verification_tiers.py --write` → `gen_fs_tiers.py` →
`gen_erzeuger_zensus.py` → `update_inventory.py` → **`gen_stand.py`
zuletzt**.

**Fallen, die in dieser Sitzung zugeschnappt sind:**

* `git ls-files --others --exclude-standard` **versteckt ignorierte
  Dateien** — das Tor `[in-source build artifacts]` misst aber den
  **Arbeitsbaum**. Mit `ls` prüfen, nicht mit git.
* `release/` lässt sich nicht räumen, solange die Anwendung aus Qt
  Creator läuft. Erst schließen, dann committen.
* Zwei parallele `ctest`-Läufe im selben Bauverzeichnis lassen
  `test_convert_leaves_no_ghost` fallen (`P3-516`). Immer nur einer.
* Im Baum arbeiten **weitere Sitzungen** (`src/toolstab.cpp`,
  `forms/*.ui`, `tests/CMakeLists.txt`, `UnifiedFloppyTool.pro` tragen
  Fremdänderungen). Staging **hunkweise**, nie `git add -A`.

---

## Was dieser Plan NICHT verspricht

* **Keine neuen Format-Plugins.** Die EINFRIER-REGEL gilt; gemessen
  steht `nfd` auf T2, das Moratorium läuft weiter.
* **Keine Zahl, die sich ohne neue Messung bewegt** (MF-1077).
* **Keine Löschung** von Code, Tests oder Registereinträgen ohne
  ausdrückliche Eigentümerentscheidung.
* **Kein Eintrag in die Wandlungsmatrix ohne Beleg** — das ist der
  ganze Punkt von Phase 1 und 2.
