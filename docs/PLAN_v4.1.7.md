# Plan v4.1.7 — „Inhalt, nicht nur Struktur"

> **Verhältnis zu v4.1.6.** Die vorige Fassung hat bewiesen, dass jede
> *Aussage* des Werkzeugs stimmt. Diese soll beweisen, dass die
> *Interpretation* der wichtigsten Formate stimmt.
>
> Verbindlich bleiben: [`DESIGN_PRINCIPLES.md`](DESIGN_PRINCIPLES.md), die
> Einfrier-Regel in der Fassung MF-498
> ([`VERIFICATION_PLAN.md`](VERIFICATION_PLAN.md) §Einfrier-Regel) und die
> Provenienz-Regel ebendort. Offene Arbeit steht in
> [`OPEN_ITEMS.md`](OPEN_ITEMS.md), nicht hier.

---

## 0. Was zuerst nachgemessen wurde

Der Plan begann mit der Annahme, die fünf Kernformate müssten erst auf
T1/T1b gehoben werden. **Das stimmt nicht mehr.** Gemessen am Baum
(`docs/VERIFICATION_TIERS.md`, `tests/corpus_manifest/manifest.json`):

| Format | Stufe | Beleg im Manifest |
|---|---|---|
| `fdi` | **T1** | real — zxart.ee release 428381, SHA-256, Spectrofon #01 (1990er) |
| `d64` | **T1b** | cross-tool — VICE 3.10 `c1541 -format "uftcorpus,42" d64` |
| `adf` | **T1b** | cross-tool — amitools `xdftool create + format "UFTCORP…"` |
| `atr` | **T1b** | cross-tool — atrcopy 10.1, Template `dos2sd.atr` |
| `nfd` | **T2** | Spec gegen pc98.org nfdr0/nfdr1 — **kein Korpus-Image** |

**Vier von fünf sind erledigt.** P3-1 verlangt genau diese fünf; es fehlt
einer. Die Provenienz-Regel ist dabei eingehalten — jeder Eintrag nennt
Erzeuger-Werkzeug **mit Version** und einen reproduzierbaren
Erzeugungsweg, nicht nur „lag irgendwo".

### Aber: was T1b heute beweist, ist Struktur — nicht Inhalt

`tests/test_corpus_d64.c` prüft, stellvertretend für alle vier:

```
disk.geometry.cylinders >= 35
disk.geometry.sector_size == 256
t.sector_count == 21          /* Zone 1 */
t.sector_count == 19          /* Zone 2 */
track_contains(&t, bam_sig)   /* Bytefolge kommt in der Spur vor */
track_contains(&t, diskname)
track_contains(&t, entry)
```

Das belegt: **die Bytes liegen, wo das Format es sagt.** Es belegt
nicht: **was UFT daraus liest, ist dasselbe, was ein unabhängiges
Werkzeug daraus liest.** Ein Parser, der die Geometrie trifft und den
Verzeichnisbaum falsch verkettet, besteht diesen Test.

Genau diese Lücke schließt Phase 1. Sie ist der eigentliche Gewinn — die
Hebung selbst ist fast fertig.

---

## 1. Reihenfolge und Begründung

    Phase 0   nfd-r0 auf T1b            entsperrt die 1:2-Regel
    Phase 1   VFS-P1 lesend, je Format  macht T1b inhaltlich statt strukturell
    Phase 2   Hardware, vier Gleise     erzeugt T1-Korpora nebenbei
    Phase 3   Hebung auf T1             nur wo Phase 2 echte Dumps geliefert hat

Phase 1 und Phase 2 laufen **parallel** — sie füttern sich: jede
Bench-Sitzung erzeugt Referenz-Dumps, jede Formathebung gibt dem Gerät
ein Prüfziel.

---

## Was die Scout-Serie zu diesem Plan beigetragen hat (Stand MF-617)

Der Plan entstand vor den fünf Scout-Zyklen. Sie haben ihn an zwei
Stellen verschoben — und an einer bestätigt.

### Eine Formathebung ist schon passiert, ohne Phase 1

`mfi` steht auf **T2 statt T3** (MF-614). Nicht durch Korpus-Arbeit,
sondern weil ein echter Parser-Fehler gegen eine benannte Referenz
behoben wurde: das registrierte Plugin prüfte acht statt sechzehn
Signaturbytes, nahm echte MAME-Dateien deshalb **an** und las die
Spurtabelle vom falschen Versatz.

    T3   57 -> 56        T2   17 -> 18

**Das ist ein Weg, den der Plan nicht kannte:** ein Format hebt sich
auch dadurch, dass man seinen Leser gegen eine Referenz richtigstellt.
Die Einfrier-Regel erlaubt das ausdrücklich („Bugfixes an Bestehendem,
Spec-Korrekturen gegen autoritative Quellen"), und es braucht kein
Korpus-Abbild — nur eine benannte Quelle und einen Rotbeweis.

Wie viele der 56 auf diesem Weg erreichbar sind, ist **nicht gemessen**.
Es wäre eine eigene Frage wert.

### Der floptool-Hebel ist kleiner als gemeldet

Der vierte Zyklus nannte „28 von 57" und bezeichnete das als wertvollsten
Fund. Nachgerechnet (MF-615): **22 von 56** als belegbare Untergrenze,
höchstens 26 — und die Methode des Scouts stand nirgends.

Wichtiger noch: es ist ein Abgleich über **Namen**. Er sagt nichts
darüber, ob floptool *unsere* Dateien liest. Für Phase 1 zählt nur das.
Der erste Schritt ist deshalb das Binary, nicht die Liste.

### Bestätigt: Oracle und Korpus-Erzeuger dürfen nicht dieselbe Hand sein

Phase 1 warnt davor. Der fünfte Zyklus hat den Fall geliefert: AdfOpus
schied als drittes ADF-Oracle aus, **nicht** weil es schlecht wäre,
sondern weil seine Engine ADFlib ist — dieselbe Bibliothek hinter
`unadf`, das der Plan bereits führt. Die Warnung war also nicht
theoretisch.

### Was dabei sonst herauskam

Fünf Zyklen, **keine einzige Übernahme** aus fremdem Code — aber
fünfzehn belegte Funde über den eigenen Baum — plus **elf** über das
Werkzeug des Scouts selbst (SCOUT-1/2 und W1/W4…W11), von denen zehn
behoben sind.

Im Baum: ein Klasse-A-Fehler im Format-Layer (MFI las jede echte Datei
falsch **und nahm sie an**), drei Lizenz-/Herkunftspflichten, zwei
falsche Doku-Sätze, **27 gelöschte tote Dateien** (1 + 26), vier
aufgelöste Makro-/Enum-Konflikte und ein sichtbar gewordener Verwaister.

Vier der elf Werkzeugfehler hatte ich selbst am Tag der Einpflege
eingebaut. Der Scout hat sie im jeweils nächsten Lauf gefunden.

**Der Scout ist kein Import-Trichter. Er ist ein Auditor von außen.**
Jedes fremde Repository zwingt zu einer Frage über den eigenen Baum, die
sonst niemand stellt: „haben wir das?", „ist unsere Kopie aktuell?",
„erfüllen wir die Lizenz?"

Für die Planung heißt das: Scout-Zyklen gehören **nicht** als
Beschaffungsquelle für Phase 1 eingeplant, sondern als eigenständige
Prüfschleife neben ihr.

---

## Phase 0 — `nfd-r0` auf T1b (klein, entsperrend)

**Warum zuerst:** P3-1 ist die einzige Bedingung des Moratoriums. Solange
sie offen ist, gilt für neue Formate ein Verbot statt der 1:2-Regel. Ein
Format trennt uns davon.

**Aufgabe.** Ein NFD-r0-Abbild mit Provenienz beschaffen und einen
Korpus-Test dagegen schreiben.

| | |
|---|---|
| **Oracle-Weg** | ein NFD-r0-Erzeuger nach pc98.org-Spec — die Spec ist bereits als T2-Quelle im Baum benannt |
| **Rotbeweis zuerst** | der neue Test muss auf dem *heutigen* Parser fallen, wenn man im Abbild ein Kopf-Feld verfälscht; feuert er nicht, misst er nichts |
| **Abnahme** | Manifest-Eintrag mit Werkzeug + Version + Erzeugungsweg; `gen_verification_tiers.py` meldet `nfd` als T1b |
| **Beweist nicht** | dass NFD-r1 stimmt — andere Variante, eigener Eintrag |

**Wenn sich kein Erzeuger findet:** Phase 0 endet mit „bleibt T2, Grund
dokumentiert". Das ist ein zulässiges Ergebnis und **keine** Umgehung —
aber dann bleibt das Moratorium, und das ist die Konsequenz.

> **Eskalation:** Die Beschaffung ist Eigentümer-Sache. Bis das Abbild da
> ist, kann Phase 0 nicht abgeschlossen werden.

---

## Phase 1 — VFS-P1, lesend (der eigentliche Hebel)

**Was es ist:** ein nur lesender Dateisystem-Zugriff — Verzeichnisliste,
Dateigrößen, Belegungskarte. Kein Schreiben, kein `fsck`, keine
Reparatur.

**Warum es die Verifikation stärkt:** die stärkste Belegform für ein
Sektor-Abbild ist nicht „Kopf stimmt", sondern „ein unabhängiges Werkzeug
liest denselben Inhalt heraus". Das ist ein **Oracle-Differenztest**, und
damit ist neuer Dateisystem-Code einfrier-konform baubar: benannte
Referenz, Rotbeweis zuerst, Referenz im Header.

**Warum die Oberfläche schon wartet:** MF-575/576 haben Datei-Browser und
Belegungskarte ehrlich gemacht — sie zeigen heute graues `?` und sagen,
dass nicht gelesen wird. Die Anzeige-Verdrahtung existiert, die
Wahrheits-Tore (34./35.) existieren. Es fehlt nur die Datenquelle.

### Reihenfolge innerhalb Phase 1

| # | Dateisystem | Oracle | Warum an dieser Stelle |
|---|---|---|---|
| 1 | **CBM DOS** (D64) | `c1541` aus VICE | kleinstes Format, skriptbares Oracle, BAM überschaubar, Korpus liegt bereits |
| 2 | **AmigaDOS** (ADF) | `xdftool` (amitools) — derselbe Erzeuger wie der Korpus; **zusätzlich** `unadf` als unabhängige Zweitmeinung | zweitkleinster Schritt; Korpus liegt |
| 3 | **Atari DOS 2** (ATR) | `atrcopy` | Korpus liegt |
| 4 | **FAT12** (IMG/DSK) | `mtools` (`mdir`) | größte Verbreitung, aber kein Korpus-Eintrag → braucht erst einen |

> **Warnung zur Nr. 2.** Wenn Oracle und Korpus-Erzeuger dasselbe
> Werkzeug sind (`xdftool`), prüft der Test die Selbstkonsistenz eines
> fremden Werkzeugs, nicht die Richtigkeit von UFT. Deshalb ist die
> Zweitmeinung (`unadf`) dort **Pflicht**, nicht Kür. Bei D64 ist die
> Lage dieselbe (`c1541` erzeugt und liest) — auch dort gehört ein
> zweites Werkzeug dazu.

### Aufgabenschnitt je Dateisystem (identisch, viermal)

1. **Rotbeweis zuerst.** Ein Differenztest, der die Verzeichnisliste von
   UFT gegen die des Oracles stellt. Er muss **rot sein**, bevor Code
   entsteht — es gibt ja noch keine Leseseite. Ein Rotbeweis, der nicht
   feuert, ist wertlos (in dieser Sitzung dreimal belegt).
2. **Oracle in den Prüfstand.** `c1541`, `unadf`, `xdftool`, `atrcopy`,
   `mtools` sind paketierbar; im CI installierbar, lokal optional. Fehlt
   das Oracle: **Skip mit Rückgabe 77 und gedrucktem Grund** — nie ein
   stiller Erfolg.
3. **Leseseite implementieren**, Referenz im Header.
4. **Abnahme:** Verzeichnisliste byteweise gleich, über den ganzen
   Korpus. Abweichungen sind ein Befund, kein Toleranzfall.
5. **Anzeige verdrahten** — Datei-Browser und Belegungskarte bekommen
   ihre Datenquelle. **Klick-Sitzung erforderlich** (GUI ist
   Eigentümer-Territorium), Protokoll nach dem Muster von
   `CLICK_SESSION_v4.1.6.md`.

### Was Phase 1 ausdrücklich NICHT beweist

- **Nicht**, dass UFT beschädigte Dateisysteme richtig liest — die
  Oracles lesen gesunde Abbilder. Schadensfälle sind eigene Arbeit.
- **Nicht**, dass Schreiben funktioniert — Phase 1 ist lesend.
- **Nicht**, dass exotische Varianten stimmen (OFS vs. FFS, DOS 2.5 vs.
  MyDOS, 40 vs. 42 Spuren) — jede Variante braucht ihren Korpus-Eintrag.

### Was Phase 1 entsperrt

Laut den vorhandenen Plänen hängen daran: Datei-Schadenskarte,
`fsck`-Operation, Katalogmodus, `HOST_OS_CONTAMINATION`-Zuordnung, die
DiskFlashback-Mount-Kette und der Known-Disk-Abgleich. **Vor** dem Bauen
ist je Baustein zu prüfen, ob die Abhängigkeit real ist — der Fächer ist
das Argument für Phase 1, kein Freibrief für sechs Folgeprojekte.

---

## Phase 2 — Hardware, vier Gleise

**Die ordnende Randbedingung:** MF-310 — kein Gerät hinter diesem
Projekt. Sechs der zehn offenen Punkte brauchen Hardware. Gemessener
Stand: **1 von 9 Controllern** hat je einen Tier-3-Bench (Greaseweazle,
2026-05-15); die übrigen acht: nie.

### Gleis A — Geräte, nach Verhältnis von Kosten zu offenem Code

| # | Controller | Code-Stand (gemessen) | Was fehlt | Weg |
|---|---|---|---|---|
| 1 | **USB-Floppy (UFI)** | Linux-SG_IO fertig; Emulator-Zustandsautomat treibt den Produktions-HAL | Windows-Backend, Tier-3 | Kaufgerät, günstig |
| 2 | **ADF-Copy** | Provider mit echten Runnern | Tier-3 | Selbstbau (Teensy + Shield, offene Pläne) |
| 3 | **XUM1541** | libusb verdrahtet, Drahtprotokoll gegen OpenCBM-Quelle geprüft (MF-301), Emulator 56/56 | Tier-3 + opencbm | Kaufgerät oder selbst flashen |
| 4 | **SCP-Direct** | der fertigste: 22/22 Opcodes byte-exakt, Lesepfad komplett, Schreiben bis zum Bench gesperrt | **nur** Tier-3 | reine Beschaffung |
| 5 | **Applesauce** | `?vers` verdrahtet; **`?disk` kommt in `src/` nicht vor** (gemessen) | erst Code, dann Gerät | Community |

**Begründung der Reihenfolge:** 1 ist das billigste Gerät mit dem meisten
offenen Code — das Windows-Backend ist Arbeit ohne Gerätezwang. 2 trifft
vorhandene Fähigkeiten und erzeugt **am selben Nachmittag** den
ADF-Referenzkorpus. 3 hat die stärkste Kopplung an Phase 1: eine
ZoomFloppy liest echte 1541-Disketten, `c1541` ist das Oracle, D64 wird
**T1 mit Hardware-Bench** statt nur gegen Dateien. 4 ist reine
Beschaffung — der Code wartet fertig. 5 zuletzt, weil dort als einzigem
noch Code fehlt.

### Gleis B — Code, der ohne Gerät vorankommt

Alles moratoriumskonform, weil HAL- bzw. Verifikationsarbeit:

- **Applesauce-`?disk`-Automat** oracle-first gegen
  `docs/M3_APPLESAUCE_TRANSPORT.md`, plus Emulator-Erweiterung. Danach
  wartet auch dieser Provider nur noch auf ein Gerät statt auf Code.
- **UFI-Windows-Backend** (`ufi_win.c`) gegen den vorhandenen
  Firmware-Emulator.
- **Emulator-Härtung** — Fehlerinjektion in alle Geräte-Emulatoren:
  Zeitüberschreitung, Kurz-Read, Stall, Diskettenwechsel mitten im
  Lesen. Genau die Fälle, die ein echter Bench später findet, vorab ins
  Netz.

> **Grenze, die gilt:** Emulator-Ergebnisse sind **niemals** Tier-3.
> „SIMULATED ist niemals PASS." Gleis B macht Benches billiger, es
> ersetzt sie nicht.

### Gleis C — das Bench-Programm formalisieren

Die Release-Notes delegieren Tier-3 an die Gemeinschaft. Dann braucht die
Gemeinschaft ein Werkzeug: ein Bench-Läufer nach
[`BENCH_PROTOCOL.md`](BENCH_PROTOCOL.md), der ein **maschinenlesbares
Protokoll** erzeugt, das als PR nach `releases/` einwandern kann.

**Maßstab:** ein Fremder mit einem SCP auf dem Tisch liefert in 30
Minuten einen gültigen Tier-3-Nachweis, ohne zu fragen. Das skaliert auf
alle neun Controller — auch auf die, die nie gekauft werden.

**Abnahme:** ein Tor, das ein eingereichtes Protokoll gegen sein Schema
prüft und ein unvollständiges ablehnt. Ein Bench-Bericht ohne
Geräte-Seriennummer, Firmware-Version und Diskettenherkunft ist kein
Nachweis.

### Gleis D — eigene Hardware (UFI v2.1, CM5 + STM32H723)

Nicht zu verwechseln mit der USB-Floppy-UFI-Klasse in UFT. Sobald die
Karte Flux liefert, ist ein eigener Provider der logische Schritt: der
einzige Controller im Feld, bei dem Werkzeug- und Hardware-Autor
dieselbe Person sind — und damit das beste Prüfziel für den Bench-Läufer
aus Gleis C.

**Bedingung:** derselbe Maßstab wie für fremde Geräte. Eigene Hardware
bekommt kein leichteres Tier-3.

---

## Phase 3 — Hebung auf T1, nur wo Phase 2 geliefert hat

T1 verlangt **dokumentierte archivalische Herkunft** (Quelle/URL +
SHA-256) **plus** unabhängigen Spec-Pin: die Assert-Bytes werden **vor**
dem UFT-Lauf per unabhängigem Parse gepinnt, weil der historische
Erzeuger unbekannt ist.

| Format | Weg zu T1 | Abhängigkeit |
|---|---|---|
| `adf` | eigene Amiga-Disketten mit Greaseweazle dumpen | Gleis A #2 oder vorhandener GW |
| `d64` | echte 1541-Diskette über ZoomFloppy | Gleis A #3 |
| `atr` | Original-Atari-Diskette | Beschaffung offen |
| `nfd` | erst Phase 0 (T1b), dann echtes PC-98-Medium | Beschaffung offen |

**Provenienz ist konstitutiv, nicht dekorativ.** Ein Dump ohne
dokumentierte Herkunft ist bestenfalls T2-Material — „irgendwo lag ein
Image" ist keine Ground Truth.

---

## Was ausdrücklich NICHT in dieser Fassung passiert

| Nicht | Grund |
|---|---|
| **Kopierschutz-Katalog verdrahten** | Die Freigabe-Abrechnung hat das als *Entscheidung* eingefroren: 334 ungeprüfte Funktionen an ein forensisches Urteil zu hängen wäre genau die Wette, aus der die fabrizierten Parser kamen. Erst Prüfung, dann Aufrufer. |
| **Weitere neue Formate** | Die 1:2-Regel greift erst nach Phase 0. |
| **Tier-3-Benches erzwingen** | Acht Geräte kauft man nicht, um eine Tabelle zu färben. Gleis C skaliert stattdessen. |
| **VFS schreibend** | Phase 1 ist lesend. Schreiben braucht das Sicherheitstor, Zustimmung und einen eigenen Plan. |

---

## Abnahme für v4.1.7

Nach dem Muster von v4.1.6 — messbar, nicht „sicher gefühlt":

- [ ] **Phase 0 entschieden**: `nfd` auf T1b **oder** „bleibt T2" mit
      dokumentiertem Grund. Beides zulässig, Schweigen nicht.
- [ ] **Mindestens zwei Dateisysteme** lesend, je gegen **zwei**
      unabhängige Oracles, Verzeichnisliste byteweise gleich über den
      ganzen Korpus.
- [ ] **Fehlendes Oracle führt zu Skip mit Grund**, nie zu stillem Erfolg
      — durch ein Tor erzwungen.
- [ ] **Klick-Sitzung** für jede neu verdrahtete Anzeige, protokolliert.
- [ ] **Alle Tore 0**, keines aufgeweicht; Sanitizer weiter 0/0.
- [ ] **Die vier Kennzahlen** neu gemessen im Release-Text. Erwartung:
      ungeprüfte Formate sinken von 56 (Stand nach MF-614); **gemessen
      schrumpfen schlägt
      behauptet verschwinden.**
- [ ] **Bench-Alter je Controller** neu — jeder Tier-3 aus Phase 2 mit
      Datum und Protokoll-Verweis.
- [ ] Kein Satz in README, CLAUDE.md oder Release-Text ohne Test, Skript
      oder benannten Bench.

---

## Engpass

Derselbe wie zuletzt: **Beschaffung**, und sie ist Eigentümer-Sache.

| Was | Wofür |
|---|---|
| NFD-r0-Abbild oder -Erzeuger | Phase 0 — entsperrt das Moratorium |
| Oracle-Binaries im Prüfstand | Phase 1 — `c1541`, `unadf`, `xdftool`, `atrcopy`, `mtools` |
| USB-Floppy, ZoomFloppy, Teensy | Gleis A #1–#3 |
| Action-Replay-Einfrierabbild | `test_freezer`, 8 ausgelassene Prüfungen |
| Klärung für die 10 Dateien in `tests/corpus/` | Lizenz/Verteilung |

**Der Vorteil, den dieses Projekt hat:** dreihundert eigene
Amiga-Disketten und ein Greaseweazle. Ein Nachmittag Dumpen liefert den
ADF-Referenzkorpus aus erster Hand, mit Provenienz im Manifest — und
hebt `adf` von T1b auf **T1**, ohne auf irgendjemanden zu warten.

Das ist der Punkt, an dem ich anfangen würde.
