# Plan zu UFT-04/06/07/08/09/10 (04.09.) und UFT-24

**Stand:** 2026-09-05, nach MF-910
**Filter:** „wir pushen nur was der Enduser braucht" + Regel 9 (MF-640)
**Rahmen:** Moratorium hält (`nfd` auf T2). Kennzahlen: T3 = **36** ·
Matrix = 16 · leckende Tests = **0** · Bench = keine Hardware (MF-310).

**Nicht aufgenommen:** `a8rawconv-full-analysis.md` — vollständig
abgearbeitet in `docs/PLAN_FLUXENGINE_A8RAWCONV.md` (MF-884/886/887,
Phase 5 ausdrücklich Fundus).

---

## Phase 0 — Was die Messung übrig ließ

Drei Extraktions-Agenten, jede tragende Behauptung am Baum nachgemessen.
**Rund zwanzig Aussagen tragen nicht** — die meisten, weil der Baum sie
zwischen MF-818 und MF-908 bereits behoben hat.

| Behauptung | Messung | Urteil |
|---|---|---|
| TD0: zwei Zeilen falsch (08) | seit MF-870 korrekt; die zitierten Zeilen sind heute der **Kommentar**, der den alten Fehler dokumentiert | **trägt nicht** |
| `d64_is_valid_size` kennt nur 35/40 (10) | 35/40/41/42 + Fehlerkarten seit MF-871; Schreibseite MF-908 | **trägt nicht** |
| Zonen-Invariante fehlt (08) | `tests/test_cbm_geometry.c` prüft 683/768/690/3200 | **trägt nicht** |
| einzelnes `$FF` gilt als Sync (04) | `G64_SYNC_MIN_BYTES = 5` — **strenger** als nibtools' 2 | **trägt nicht** |
| Bitverschiebung fehlt (04) | `find_pattern_shifted()`, 8 Durchläufe, gibt `shift` zurück | **trägt nicht** |
| RapidLok-Grammatik fehlt (04) | `uft_prot_detect_rapidlok()`: 21 Sync + `0x55` + 164× `0x7B` | **trägt nicht** |
| `gcr_encode_table` dreimal (04) | **fünfmal** — die Zahl ist zu niedrig | **trägt nicht** |
| DMS Modi 4/5 vertauscht (07) | MF-837 hat den ganzen Spurkopf als falsch belegt und behoben | **überholt** |
| MGT ohne Leser (04) | `uft_mgt.c` + `mgt_sad_sdf.c` | **trägt nicht** |
| `mfi.c` prüft 3 Byte Kennung (07) | 16 Byte mit Rückfall, seit MF-863, bewacht von `test_mfi_kennung.c` | **trägt nicht** |
| zwei Dateien doppelt in SOURCES (07) | `uniq -d` über alle `src/…\.c`-Zeilen der `.pro`: **leer** | **trägt nicht** |
| `uft_st.c` ohne Test (07) | `tests/test_st_bootpruefsumme.c` seit MF-873 | **trägt nicht** |
| ED-Geometrien fehlen (07) | alle vier in `uft_img.c` | **trägt nicht** |
| `load_fat_cache` vergleicht nie (06) | seit MF-829 mit `fat_compared`/`fat_diff_bytes` | **trägt nicht** |
| „keine Zeile für Textverarbeitung, Lanier fehlt" (24) | `supercopy_formats.h` führt LANIER, XEROX 820 II/3700, Olivetti ETV; dazu `cpm_wang`, `DSK_WNG`, `DSK_X820`, `DSK_OLI` | **trägt nicht** |
| „45 Spuren" wären ein Problem (10) | Baum ist konsistent: Zonen enden bei 42, `>42 → -1`, `G64_MAX_TRACKS 84` | **trägt nicht** |
| Generatortabelle = 745 Blöcke (08) | nachgerechnet aus dem **eigenen Schnipsel des Berichts**: **725** | **Zahl trägt nicht** |
| Schreibgrenze ~7700 als neue Konstante (09) | **7692** steht bereits dreifach und wurde MF-878 von gerundet auf gemessen korrigiert | **wäre eine zweite Zahl für denselben Fakt** |
| „Kernbefund neu" (04/08) | P3-38/P3-39 führen die Rittwage-Widerlegung seit MF-818 | **trägt nicht** |

**Lizenz, und sie entscheidet über den größten Teil der Berichte:**
`tools/uft-scout/work/nibtools/LICENSE` ist **GPL-3.0**, UFT ist GPL-2.
Der Anweisungsteil beider C64-Berichte — *„Konstanten übernehmen,
Fundstelle in den Kommentar"* — ist **kein zulässiger Kanal**. GPL-3
verbindet sich mit GPL-2-**or-later**, aber jede weitere GPL-3-Quelle
ist laut `CONTRIBUTING.md` **dieselbe Eigentümer-Entscheidung noch
einmal**, kein bereits erteiltes Präzedenz. Genau dafür hat **MF-635**
schon zwei Dateien entfernt; die Berichte wollen sie faktisch zurück.
Zulässig bleiben **Nachbau** (Hand B sieht die Quelle nie) und **Oracle**.

---

## Phase 1 — Die Signaturkonstanten haben keine Quelle, und ihr Test kann nicht scheitern

> **✅ ERLEDIGT — MF-913.** Der Rotbeweis lief als **Mutation vor dem
> Eingriff**, weil ein Test, der nicht scheitern kann, sich nicht
> anders widerlegen lässt: die Konstante durch `{DE,AD,BE,EF,99}`
> ersetzt — reinen Unsinn — und die Suite blieb **8 von 8 grün**.
> Nach dem Eingriff fällt dieselbe Mutation **zwei** Prüfungen.
>
> Der Test nennt seine Bytes jetzt selbst, und eine eigene Prüfung
> hält fest: die Konstante muss sein, was der Test erwartet — nicht
> umgekehrt. Der widerlegte Vermerk „(bit-shifted)“ ist gestrichen.
>
> **Keine nibtools-Bytes übernommen** (GPL-3 gegen GPL-2). Was der
> Kommentar jetzt sagt, ist eine **Beschreibung** des Unterschieds —
> Folge gegen Menge —, keine Übernahme von Werten. Befund als
> **P3-187**.


**Der schwerste Fund der Runde**, erstgeprüft gegen nibtools im Baum:

| | UFT (`uft_protection_detect.c`) | nibtools `prot.c` (erstgeprüft) |
|---|---|---|
| V-MAX | `{A5,1E,78,E1,87}`, **exakte Folge** | `{4b,69,49,5a,a5}`, **Byte-Menge**, Lauflänge > 5 |
| V-MAX Cinemaware | `{4B,3C,F0,C3}`, Kommentar *„(bit-shifted)"* | `{64,a5,a5,a5}` |
| PirateSlayer | `{07,07,FC,FC,01}` | `{d7,d7,eb,cc,ad}` |

**Von fünf Bytes überschneidet sich genau eines** (`0xA5`). Die Behauptung
„(bit-shifted)" ist **nachgerechnet falsch**: keine der 16 Verschiebungen
von `64A5A5A5` ergibt `4B3CF0C3`. Und die Suchform ist eine andere —
Folge gegen Menge.

Die Datei nennt **keine Quelle** (`@author UFT Team`, SPDX MIT).

**Und der Prüfstand kann den Fehler strukturell nicht sehen:**
`tests/test_schutz_erkennung_lebt.c` baut sein Fixture mit
`memcpy(track + 20, UFT_VMAX_MARKERS, …)` — aus **derselben Konstante**,
die er prüft. Er ist grün, egal welche Bytes dort stehen. Das ist die
Klasse „der Prüfstand war grün, WEIL der Fehler da war", zum ~14. Mal.

**Gemessen zur Schwere:** die ganze `uft_prot_detect_*`-Familie ist
**unerreichbar** — nur Test-Aufrufer, `uft_detect_all_protections()` wird
außerhalb von `src/protection/` nirgends genannt. Kein Benutzer bekommt
heute eine falsche Schutzaussage. Der grüne Test ist aber eine
Falschauskunft **an uns**.

### Schritte
1. Rotbeweis: der Test muss mit **fremden** Bytes arbeiten, nicht mit den
   eigenen Konstanten — dann fällt er, sobald die Konstante nicht das
   trifft, was sie zu treffen behauptet.
2. Die Konstanten **als quellenlos kennzeichnen**: der Kommentar
   „(bit-shifted)" wird gestrichen (er ist widerlegt), und an ihre Stelle
   tritt, was gemessen ist — inklusive dessen, was nibtools stattdessen
   prüft, **als Beschreibung, nicht als Übernahme**.
3. **Keine Bytes aus nibtools übernehmen.** Lizenz vor Fähigkeit.

**Kennzahl:** keine der vier. Begründung ausgesprochen: eine Zusicherung,
die nicht feuern kann, ist keine — und dieser Baum führt genau darüber
Buch (P3-178).

---

## Phase 2 — 313 CP/M-Formate ohne Tür, ohne SPDX, ohne Lizenzurteil

**Gemessen.** `include/uft/formats/supercopy_formats.h` (411 Z.) und
`src/formats/cpm/uft_supercopy_detect.c` (440 Z.) führen **313
CP/M-Diskettenformate**. Der Kopf nennt die Herkunft:

> *„SuperCopy v3.40 SELECT.DAT — CP/M-Format-Datenbank … Quelle:
> SuperCopy v3.40 von Oliver Müller"*

Das ist eine **extrahierte fremde Datentabelle** aus einem Programm von
1991. Gemessen:

- **SPDX: 0** in beiden Dateien — entgegen `CONTRIBUTING.md`
- **Lizenz: nirgends genannt**, weder Datei noch `docs/QUARANTINE.md`
- **`scripts/audit_attribution_licence.py` findet sie nicht** — das
  Herkunfts-Tor hat eine Lücke
- **alle fünf Symbole: 0 Aufrufer** außerhalb der eigenen Datei

Das ist der eigentliche Befund hinter UFT-24: die Formatklasse **fehlt
nicht**, sie ist nicht verdrahtet — und sie darf nicht verdrahtet werden,
bevor die Lizenz geklärt ist. **Lizenz vor Fähigkeit** (Konfliktordnung 2,
belegt an IPF/MF-638).

### Schritte
1. Eintrag in `docs/QUARANTINE.md` nach `docs/QUARANTINE_PROCESS.md`.
2. SPDX **nicht** blind setzen — erst die Herkunft klären; solange gilt
   der Quarantäne-Vermerk.
3. **Die Lücke im Tor schließen**: warum sieht
   `audit_attribution_licence.py` eine Datei nicht, die ihre fremde
   Quelle im Kopf nennt? Das ist die wertvollere Hälfte — ein Tor, das
   den größten Fall übersieht, schützt nichts.

**Kennzahl:** die fünfte — **Dateien mit ungeklärter Herkunft**,
Befund-Stufe. Rückstand steht bei 30 (Grundlinie 30).

---

## Phase 3 — `zones_1541` beansprucht eine Verifikation, die nur 35 Spuren deckt

> **✅ ERLEDIGT — MF-916, und beim Messen genauer geworden als der
> Bericht.** Er sprach pauschal von einer ungedeckten „extended zone“.
> Nachgemessen berührt die 35-Spur-Diskette **alle vier** Zonenzeilen —
> Zone 0 über die Spuren 31–35. Die vier **Gap-Werte** sind also je
> gedeckt.
>
> Ungedeckt ist die **Reichweite**: dass Zone 0 bis 42 weiterläuft, ist
> eine Fortschreibung. Und `tests/test_cbm_geometry.c` schließt das
> nicht — er hält die Tabelle gegen die **Alttabellen**, also gegen die
> 24 Kopien, die sie abgelöst hat. Einigkeit unter Kopien ist keine
> äußere Instanz.
>
> Ausdrücklich festgehalten, weil es naheliegt und nicht trüge: ein
> Rundlauf mit einer 42-Spur-D64, die UFT **seit MF-908 selbst
> erzeugen kann**, beläge Selbstkonsistenz — nicht die Zonenaufteilung
> einer echten Diskette. Befund als **P3-189**.


**Gegen mich selbst.** Der Kopf von `src/formats/cbm/uft_cbm_geometry.c`
beschreibt die Spuren 36–42 als „extended range" und belegt die
Gap-Werte mit *„byte-identical G64 output against the c1541 reference
image"*. **MF-910 hat diesen Vermerk gestern präzisiert — und die Lücke
nicht gesehen.**

Gemessen: `tests/corpus_free/vice_c1541_35trk.d64` ist **174 848 Byte =
683 Blöcke = 35 Spuren**. Die Messung deckt die erweiterten Spuren
**nicht**; für 36–42 gibt es im Baum kein Referenzmaterial.

Der Vermerk sagt „gemessen wirksam" ohne zu sagen **wofür**. Das ist
dieselbe Überdehnung, die MF-910 an anderer Stelle behoben hat.

**Kennzahl:** keine — reine Ehrlichkeit, und billig.

---

## Fundus — benannt, nicht eingeplant

| Fund | warum nicht jetzt |
|---|---|
| `uft_prot_detect_fat_track()` verdrahten (04) | existiert, 0 Aufrufer, ist genau der Spurvergleich, den beide Berichte vermissen — aber eine Erkennung zu verdrahten, deren **Schwesterkonstanten quellenlos sind**, wäre die falsche Reihenfolge. Nach Phase 1 neu bewerten |
| Ringpuffer über `2*tracklen` (04) | neuer Decoder-Code → EINFRIER-REGEL |
| `gcr_decode_table[0x1F] = 0xFF` (04) | Sync von 15 Fehlmustern nicht unterscheidbar — trägt, aber neuer Decoder-Code |
| Key Track als eigene Metrik (09) | kein geschütztes C64-Abbild im Korpus → kein Rotbeweis möglich |
| Spur-Schrittweite (09/10) | UFT modelliert keinen Kopiervorgang; bewegt keine Kennzahl |
| Xerox 860, CPT 8000, NBI 3000, HSC (24) | neue Plugins → Moratorium. Und 8″-SSSD wird von `DSK_X820` bereits bedient |
| elf `.boo`-Bootsektoren als Fixture (06) | **erlaubt und wertvoll** (`st` T3→T2), aber die Lizenz des ECOPY-Materials ist ungemessen — erst Kanal klären |
| Schutz-Korpus @floppyarchaeology (07) | erlaubt; misst die Falschpositivrate von `g64_detect_protection()` an ausdrücklich schutzfreien Objekten |
| `$55`-Key-Track-Rolle an P3-39 (09) | zwei Doku-Zeilen; mitnehmen, wenn P3-39 ohnehin angefasst wird |

---

## Reihenfolge

| # | Phase | Nutzen | beweisbar heute? |
|---|---|---|---|
| 1 | Signaturkonstanten + Test | ein Test, der scheitern **kann** | **✅ erledigt, MF-913** |
| 2 | Herkunft der 313 Formate + Torlücke | Lizenzklarheit, und ein Tor, das sieht | **ja** |
| 3 | `zones_1541`-Vermerk | Ehrlichkeit gegen die eigene Überdehnung | **✅ erledigt, MF-916** |
