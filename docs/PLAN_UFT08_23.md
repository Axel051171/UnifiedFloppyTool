# Plan zu UFT-08/09/10 und UFT-19..23 (acht Berichte)

**Stand:** 2026-09-05, nach MF-903
**Filter:** „wir pushen nur was der Enduser braucht" + Regel 9 (MF-640)
**Rahmen, gemessen vor dem Planen:** das **Moratorium hält** — `nfd` steht
auf **T2**, nicht T1/T1b, die Bedingung aus MF-363/498 ist nicht erfüllt.
**Kein neues Format-Plugin aus diesen acht Berichten**, egal wie gut der
Fund ist. Erlaubt bleiben: Bugfixes an Bestehendem, Verifikations-, Test-
und Korpusarbeit, Spec-Korrekturen und die **Verdrahtung von vorhandenem,
aber unerreichbarem Code**.

**Kennzahlen am Tag des Plans:** T3 = **37** · Wandlungsmatrix = **16**
Einträge · leckende Tests = **0** · Bench-Alter = keine Hardware (MF-310).

---

## Phase 0 — Was die Messung von den Berichten übrig ließ

Drei Extraktions-Agenten haben die acht Berichte gelesen und **jede
tragende Behauptung am Baum nachgemessen**. Das Ergebnis ist der
eigentliche Wert dieses Durchgangs: **zwölf Behauptungen tragen nicht.**

| Behauptung | Messung | Urteil |
|---|---|---|
| „M2FM schließt eine offene Lücke" (19) | `uft_encoding_caps.c` führt M2FM als `can_decode=false`, mechanisch gehalten durch Tor 54; `docs/ORACLES.md` nennt fluxtoimd längst als Referenz | **trägt nicht** |
| „HDOS/Heathkit fehlt komplett" (20) | `src/formats/industrial/uft_heathkit.c` existiert, Kopf nennt HDOS | **trägt nicht** — es fehlt die *Dateisystem*ebene |
| „IMD→IMG verliert Skew" (20) | die Sektornummernkarte wird nach `sec->number` gelesen, geht nicht verloren; das Paar steht gar nicht in der Matrix | **trägt nicht** |
| `uft_st_measure_interleave()` (20) | existiert nicht; real ist `uft_st_interleave_messen()` | **trägt nicht** |
| „`opd` fehlt" (22) | Plugin `OPUS`, registriert in `uft_format_registry.c` | **trägt nicht** — aber deckt Phase 1 auf |
| „`1dd` fehlt" (22) | Plugin `D77` führt `d77;1dd`, T2 | **trägt nicht** |
| „Korg/Akai als Fundus" (23) | beide vorhanden, beide **T2**, beide mit echtem `fwrite` | **trägt nicht** |
| „variable Sektorgröße unmöglich" (22 §6) | `uft_format_add_sector()` nimmt die Größe je Sektor | **trägt nicht** |
| „`d64_is_valid_size()` kennt nur 35 und 40" (10) | seit MF-871 auch 41 und 42, inkl. Fehlerkarten | **trägt nicht** |
| „DEL-Einträge erzeugen Falschpositive" (08 F4) | es gibt **keine** Prüfung „Zeiger auf der Verzeichnisspur"; der reale Fund ist die **Divergenz zweier Türen** | **Prämisse trägt nicht** |
| „1581-Gaps fehlen, weil µPD765" (08 F1) | der Baum führt den 1581 als MFM/WD, nicht µPD765; die Gap-Tabelle hat **keinen Verbraucher** | **Begründung trägt nicht** |
| „DG-Nova-Fabrikation ist DRINGEND" (21) | **berichtigt MF-906/P3-183**: `77×26×1×128 = 256256` ist der **IBM-3740-Standard**, den vier Dateien führen und `mfm_detect.c` selbst so benennt — keine Fabrikation. Es bleibt: 0 CRC-Zeilen, kein Aufrufer | **trägt nicht** |

Zwei Berichte haben außerdem Zahlen, die der Baum selbst schon führt:
die Gutachten-Bibliothek hat **45**, nicht 43; `docs/format_specs/commodore/`
hat **47**, nicht 46. Und der DG-Nova-Befund stand seit **2026-08-28** im
eigenen Baum (`tools/uft-scout/out/FloppyTools.gutachten.md`) — der
Bericht hat ihn wiederentdeckt, nicht gefunden.

### Zwei Hebel, selbst gemessen

1. **Der Korpus-Engpass ist weg.** In WSL Ubuntu liegen `mformat`, `mdir`
   und über den Dispatcher `mtools_bin` auch `mcopy` aus mtools 4.0.49
   (MF-789). Probe gefahren: 720K formatiert, Datei hineinkopiert,
   Verzeichnis zurückgelesen. Damit ist **PLAN_UFT16_18 Phase 1** nicht
   mehr blockiert.
2. **SAMdisk als hauseigenes Oracle** wäre der größte Hebel auf Kennzahl 1
   — `src/samdisk/` ist ein vollständiger MIT-Quellbaum mit `main()`, und
   sein README nennt **acht** der 37 T3-Formate mit eigenem Handler
   (`cfi cpm fdi_pc98 ipf mgt sap_thomson scl udi`). Aber: **kein Compiler
   in beiden WSL-Distributionen** (gemessen: gcc/g++/make/cmake fehlen),
   dazu zlib/bzlib/lzma. → **Eigentümer-Entscheidung**, nicht eingeplant.

---

## Phase 1 — OPD liest eine Geometrie, die der Bootsektor widerlegt

> **✅ ERLEDIGT — MF-905.** Rotbeweis `tests/test_opd_geometrie.c`,
> **4 von 5 Prüfungen rot**; die sechste (Schreibpfad) kam dazu, **bevor**
> der Schreibpfad als geändert galt — die Lehre aus P3-178, gleich
> angewandt. Gegenprobe 5 Mutationen, jede fällt genau ihre Prüfungen.
>
> **Drei Stellen zogen dieselbe falsche Folge**, nicht nur die Sonde:
> `read_mem` legte mit Konstanten an, `read_track` wies Seite 1 ab und
> indizierte ohne Kopf, und `uft_opus_write()` hätte ein zweiseitiges
> Abbild auf 184320 Byte **abgeschnitten** (MF-877-Klasse).
>
> **Eine eigene Zusicherung fiel dabei durch**: `seite_eins_ist_lesbar`
> prüfte nur `== UFT_OK`, und mit `track_data[cyl]` liefert der Leser
> für beide Seiten dieselbe Spur — zweimal OK, Mutation überlebt.
> Verschärft auf den Inhalt; seither fällt sie.
>
> **Kennzahl bewegt:** `opus` von T3 auf **T2**, T3 von 37 auf **36**.
> `CLAUDE.md` und `README.md` mussten nachgezogen werden — das
> Drift-Tor hat beide gemeldet, wie es soll.


**Was der Benutzer davon hat:** eine doppelseitige Opus-Discovery-Diskette
wird heute **still abgelehnt**. `opd` ist über `uft_format_registry.c`
registriert, also erreichbar — und hat **null Tests**.

**Gemessen.** `include/uft/formats/uft_opus.h` verdrahtet 40×1×18×256 =
184320, und `uft_opus_probe()` beginnt mit
`if (size != OPUS_DISK_SIZE) return false;`. Der OPD-Bootsektor trägt die
Geometrie aber selbst.

**Die Referenz liegt im eigenen Baum**, MIT-lizenziert, und **Leser wie
Schreiber sagen dasselbe** — die Doppelbestätigung, die dieser Baum
verlangt:

```c
/* src/samdisk/opd.h — OPD_BOOT */
uint8_t jr_boot[2];   /* Z80 JR (0x18) */
uint8_t cyls;
uint8_t sectors;
uint8_t flags;        /* b7-6 FDC-Größencode · b4 Seiten (0=1, 1=2) */

/* src/samdisk/opd.cpp — ReadOPD() und WriteOPD() beide: */
fmt.cyls    = ob.cyls;
fmt.heads   = (ob.flags & 0x10) ? 2 : 1;
fmt.sectors = ob.sectors;
fmt.size    = ob.flags >> 6;
```

### Schritte

1. **Rotbeweis zuerst**, `tests/test_opd_geometrie.c`: eine OPD, deren
   Bootsektor `cyls=40, sectors=18, flags` mit gesetztem Bit 4 (zwei
   Seiten) und Größencode 1 (256 B) trägt → 368640 Byte. Muss geöffnet
   werden, und die Geometrie muss dem Bootsektor folgen. Heute rot.
2. Wächter im selben Test: die einseitige 184320-Datei bleibt lesbar
   (Regression), und eine Datei, deren Bootsektor nicht zur Dateigröße
   passt, wird **abgelehnt** — sonst ist die Sonde ein Erkenner, der
   nicht „nein" sagen kann (MF-729).
3. `uft_opus_probe()` und der Öffnungspfad lesen die Geometrie aus dem
   Bootsektor; die Konstanten in `uft_opus.h` bleiben als **Vorgabe**
   stehen, nicht als Bedingung.
4. Die Referenz **in den Header** (EINFRIER-REGEL c), Verweis auf
   **Symbol**, nicht Zeile (P3-180).

### Abnahme
- [ ] Rotbeweis rot vor, grün nach · Gegenprobe: je Mutation genau ihre Prüfung
- [ ] `docs/spec_verification.json` um den `opus`-Beleg ergänzt, Tiers neu
- [ ] Bau ohne Warnung, `ctest` grün, alle Tore 0

**Kennzahl:** T3 runter (`opd` von T3 auf T2 — Oracle im Baum, aber noch
kein fremd erzeugtes Abbild).

---

## Phase 2 — Eine unerreichbare Zeile, und das Tor für ihre Klasse

> **✅ ERLEDIGT — MF-906 (Tor 58).** Gemessen über alle `src/**/*.c`
> aus `git ls-files`: **14** Tabellen dieser Gestalt, **3** Kollisionen.
> Der Bericht nannte **eine** — die anderen zwei (`g_hitachi_geom`
> 655360, `g_bk_geom` 409600) fand erst das Tor. Selbsttest 5/5,
> Gegenprobe beidseitig (neue Kollision → Exit 1; Grundlinie aufgelöst
> → als erledigt gemeldet).
>
> **Code bewusst NICHT angefasst.** Alle drei Dateien sind verwaist —
> kein Aufrufer, nicht in der Plugin-Liste, nicht in den Prüfstufen.
> Kein Benutzer trifft sie. Drei verwaiste Tabellen umzuschreiben wäre
> Bewegung ohne Gewinn; sie sind **benannt** eingefroren, und wer eine
> dieser Dateien verdrahtet, muss die Kollision vorher auflösen.
> Befund als **P3-182**.
>
> **BERICHTIGUNG (P3-183).** Die Phase-0-Tabelle unten führte die
> DG-Nova-Geometrie als *Fabrikation, die trägt*. Das ist falsch, und
> mein eigener Agent hat den Irrtum übernommen: `77 × 26 × 1 × 128 =
> 256256` ist der **IBM-3740-Standard** für 8-Zoll SSSD — vier Dateien
> führen ihn, und `src/detect/mfm/mfm_detect.c` benennt ihn selbst so.
> Vier Maschinen mit derselben Geometrie sind kein Kopierfehler,
> sondern derselbe Standard. Es bleibt: 0 CRC-Zeilen, kein Aufrufer.


**Gemessen.** `g_heathkit_geom[]` in `src/formats/industrial/uft_heathkit.c`:

```
{ 40, 10, 2, 512, 409600, "H8/H89 DS/DD Hard-Sector 400KB" }
{ 80, 10, 1, 512, 409600, "H89 SS/DD 80T 400KB" }        <-- nie erreichbar
```

`uft_heathkit_probe()` vergleicht `size == total_size` und kehrt beim
**ersten** Treffer zurück. Zeile 4 ist toter Code.

**Das ist eine Klasse, kein Einzelfall** — dieselbe Form wie die
Guard-Kollision aus MF-881. Der Einzelfall wird behoben; der Wert liegt im
**Tor**, das die Klasse baumweit festhält: zwei Zeilen derselben
Geometrietabelle mit identischer Gesamtgröße, wo die Sonde beim ersten
Treffer zurückkehrt.

### Schritte
1. Rotbeweis: `uft_heathkit_probe(409600)` muss die **beabsichtigte** Zeile
   melden, oder die Doppeldeutigkeit muss benannt werden.
2. `scripts/audit_geometrie_kollision.py` (Tor 58): findet alle
   Geometrietabellen mit gleicher Gesamtgröße in mehreren Zeilen.
   **Selbsttest vor dem Nenner** (Regel aus `uft-innendienst`), Grundlinie
   aus dem ersten Lauf, Gegenprobe verpflichtend.
3. Ist die Grundlinie > 0, wird sie **benannt** eingefroren, nicht
   stillschweigend.

**Kennzahl:** keine der vier direkt — aber es ist ein **Tor**, und Tore
sind die Währung, in der dieser Baum Rückfälle verhindert. Wird als solches
begründet, nicht kaschiert.

---

## Phase 3 — G64→D64 ist angeboten und verschweigt die Spurabschneidung

**Gemessen.** `src/core/uft_roundtrip.c` führt bei `UFT_FORMAT_G64 →
UFT_FORMAT_D64` das Urteil `UFT_RT_LOSSY_DOCUMENTED` mit der Verlustliste
*„680 von 683 Sektoren bitgleich; ab: Spur 17/0, Spur 18/0 (BAM), Spur
18/1 (Verzeichnis); GCR-Kodierung und Fehlerinfo gehen bauartbedingt
verloren"*.

**Nicht in der Liste:** die Spuren jenseits der Sonde. Zwei Türen, zwei
verschiedene Grenzen, **beide ohne Meldung**:

| Tür | Grenze | Fundstelle |
|---|---|---|
| angebotener Wandlerpfad | Spuren 36..40 sondiert, 41/42 nie gesehen | `uft_d64_g64.c`, Sondier-Schleife im Plugin- und im Blob-Pfad |
| zweite Tür | `t <= 35` | `g64_export_d64()` in `uft_g64_parser_v3.c`, erreichbar über `uft_v3_bridge.c` |

Eine G64 mit 42 Spuren verliert sie **still** — auf einem Pfad, dessen
Verlustliste den Anspruch erhebt, vollständig zu sein.

### Schritte
1. Rotbeweis: eine G64 mit Spuren > 40 (synthetisch konstruiert, der
   Aufbau ist im Testkopf beschrieben) → die Wandlung muss die
   Abschneidung **melden**, nicht verschweigen.
2. Beide Türen melden; die Verlustliste in `uft_roundtrip.c` wird um die
   Spurgrenze ergänzt — **gemessen**, nicht geschätzt.
3. Muster dafür steht im Baum: `d64_diagnosis_add()`.

**Kennzahl:** angebotene Wandlungspfade (die Ehrlichkeit eines
angebotenen Pfades ist Teil seines Angebots).

---

## Phase 4 — Zwei Verzeichnistüren, zwei Antworten auf denselben Eintrag

**Gemessen.** `src/fs/uft_cbmdos.c` **überspringt** jeden DEL-Eintrag;
`src/formats/d64/uft_d64_parser_v3.c` **nimmt ihn auf** (`ftype != 0 &&
first_track > 0`). Dieselbe Diskette, zwei Antworten — die Klasse, die
dieser Baum sonst jagt (MF-897: drei Strukturen, drei Aussagen).

Die **Prämisse** des Berichts trägt dabei nicht: einen Falschpositiv gibt
es nicht, weil es überhaupt keine Prüfung „Zeiger liegt auf der
Verzeichnisspur" gibt. Der Fund ist die Divergenz.

### Schritte
1. Rotbeweis mit einer D64, in die ein DEL-Eintrag geschrieben wurde
   (aus dem Korpus-Abbild kopiert, Bytes benannt gesetzt).
2. Beide Türen auf dieselbe Antwort bringen — und die Antwort **begründen**:
   ein DEL-Eintrag ist forensisch Bestand, kein Nichts. Wer ihn ausblendet,
   verschweigt; wer ihn wie eine Datei zeigt, behauptet. Der Mittelweg ist
   „zeigen und als gelöscht kennzeichnen" — das Muster steht in
   `cbm_entry_plausibel()`.

**Kennzahl:** keine direkt — aber es ist eine **stille Falschaussage
gegenüber dem Benutzer**, und die Anweisung „nur was der Enduser braucht"
schlägt Regel 9, wenn zwei Ansichten desselben Abbilds sich widersprechen.

---

## Phase 5 — Billige Ehrlichkeit (kein Code)

1. **P3-79 Statuswechsel.** `docs/OPEN_ITEMS.md` führt die
   Burst-Nibbler-`$01`-Behauptung als *vom Eigentümer zurückgezogen /
   UNRESOLVED*, Grund: das untersuchte Binary war ein gepackter Crack.
   Bericht 09 liefert genau das Fehlende — das unverpackte Original,
   `$1846-$18AD`, sechs Masken. **Statuswechsel, kein Code.** Lizenz:
   unlizenzierte Szene-Software → nur Verhalten dokumentieren
   (Clean-Room), Kanal „Nachbau/Spec".
2. **Drei verwaiste Dateien mit erfundener oder widersprüchlicher
   Geometrie** benennen, statt sie stumm liegen zu lassen:
   `uft_dg_nova.c` führt `{77,26,1,128,256256}` — **RX01-identisch**, 0
   CRC-Zeilen; `zilogmcz.c` sagt 132 Byte/Sektor, FloppyTools sagt 136.
   Beide in `docs/orphan_baseline.txt`. Der Eintrag sagt künftig **warum**
   sie dort stehen.
3. **Herkunftsvermerk** an den 1541-Gaps `{9, 19, 13, 10}` in
   `uft_cbm_geometry.c`: der Kommentar sagt „gleiche Quelle wie der
   Encoder", nennt sie aber nicht.

---

## Fundus — notiert, nicht eingeplant (Moratorium)

Kein neues Format-Plugin, solange `nfd` auf T2 steht.

`d2m` `d4m` `s24` `sbt` `dti` `mbd` `dsc` `ds2` `pdi` `cwtool` `2d` ·
AMSDOS · Ohio Scientific · OS65U · Q1 MicroLite · HP 9885 ·
Ensoniq Mirage (450560, 0 Treffer im Baum) · M2FM-Decoder ·
HDOS-Dateisystemebene · WANG WCS (Achtung Namensfalle:
`uft_cpm_diskdefs.c` führt `cpm_wang` = *Wang Professional Computer*,
ein anderes Gerät).

Dazu drei Beobachtungen ohne Auftrag:
- **`src/formats/uft_cw_raw.c`** wird gebaut (`.pro`) und hat **null
  Aufrufer** — kein Plugin-Struct, keine `.extensions`.
- **Größenkollision 819200**: vier Plugins beanspruchen sie (`d81` 45,
  `korg_dss1` 40, `akai_s900` 40, `edk` 30). MF-729 hat die **Skala**
  geeicht, die **Reihenfolge** innerhalb 30–49 nicht.
- **`edk`** beansprucht `.ede`, überspringt aber keinen 512-Byte-Kopf.
  Das wäre ein Bugfix — aber der Beleg ist **eine** Quelle
  (chickensys-Doku, im Baum nicht nachprüfbar). Nach der Regel aus
  MF-901/Phase 4 des Vorplans: **zwei unabhängige Quellen oder gar
  nicht.**

---

## Reihenfolge

| # | Phase | Nutzen | Beweisbar heute? |
|---|---|---|---|
| 1 | OPD-Geometrie | still abgelehnte Disketten werden lesbar | **✅ erledigt, MF-905** |
| 2 | Unerreichbare Zeile + Tor 58 | verhindert die Klasse baumweit | **✅ erledigt, MF-906** |
| 3 | G64→D64-Verlustliste | ein angebotener Pfad sagt die Wahrheit | **ja**, synthetisch |
| 4 | DEL-Divergenz | zwei Ansichten, eine Antwort | **ja**, synthetisch |
| 5 | Ehrlichkeit ohne Code | schließt P3-79 | **ja** |
