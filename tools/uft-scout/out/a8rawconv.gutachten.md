<!-- uebernommen: MF-643 -->
# Gutachten: a8rawconv 0.95 (Axel051171/a8rawconv-0.95)

Zyklus 10, 2026-08-28 · Eigentümer-Auftrag (Sonderfall: Ursprung bereits
vendorten Codes, kein Fremdfund) · Messung: `work/a8rawconv.messung.json`
· Repo-Stand: `5db54b472a` (2026-08-22, „Lizenz richtigstellen")
· Inventar: `work/inv.json` (erzeugt 2026-08-28T15:41Z, head `cf5fa96f`,
SSOT ok — frisch übernommen, nicht neu gebaut; paralleler
atrcopy-Zyklus nutzt dasselbe)

> **Hinweis Werkzeugkette:** `gutachten.py` hat den Entwurf verweigert
> (RATENBREMSE: 6 Gutachten in `out/` ohne `<!-- uebernommen: MF-NNN -->`
> -Marke: ADFDiskBox, DiscImageManager, adfopus_hxc,
> hxcfe_amiga_copy_utility, sector-cpc, superdiskindex). Dieses Gutachten
> ist deshalb von Hand geschrieben; die Vorschläge unten stehen **hinter**
> der Abarbeitung der offenen Marken an.

---

## Kategorie

**Verbesserung + Oracle + Daten-Generator** — mit der Besonderheit, dass
die Quelle bereits als Referenz-Orakel im Baum liegt (`src/a8rawconv/`,
MF-467) und zwei Ports daraus in `src/core/` stehen. Drei Auftragsfragen
(A/B/C) plus Fähigkeits-Delta.

## Lizenzzone mit Konsequenz

**GRÜN (GPL-2.0-or-later) — portierbar in ein GPL-2.0-Projekt.**

Beleg aus den Dateien, nicht aus dem README:

* `COPYING` (Repo-Wurzel): wörtlicher Text „GNU GENERAL PUBLIC LICENSE
  Version 2, June 1991" (Zeile 1–2). `vermessen.py` meldete
  „GPL-2.0 + LGPL (mehrdeutig)" — das ist ein **Fehlalarm des Scanners**:
  die beiden „Lesser"-Treffer (COPYING:18, COPYING:338) sind Bestandteil
  des unveränderten FSF-Standardtexts der GPLv2 selbst. Kein
  Doppel-Lizenz-Fall, keine Eskalation nötig.
* **17 von 44** Quelldateien tragen den Kopf wörtlich: „Copyright (C)
  2014-2020 Avery Lee … either version 2 of the License, or (at your
  option) any later version" — Methode: `grep -l "either version 2"
  *.cpp *.h` in `a8rawconv/`, Treffer 17; die übrigen 27 Dateien tragen
  keinen Kopf und fallen unter die Projekterklärung.
* Darunter **beide Port-Quellen**: `interleave.cpp:1–16` und
  `compensation.cpp:1–16` (Kopf jeweils vollständig).
* Laufzeit-Banner des gebauten Binaries: „Licensed under GNU General
  Public License, version 2 or later."
* README.md:636 sagt dasselbe — als Hinweis, nicht als Beleg.

Fork-Historie: Commit `5db54b47` (2026-08-22) hat eine frühere
CC0-LICENSE entfernt und COPYING angelegt; CC0 war für fremden GPL-Code
ohnehin unwirksam. Der Fork ist die eigene Kopie des Eigentümers
(Original: Avery Lee/phaeron, 2014–2023).

## Attribution der Quelle

a8rawconv 0.95, Avery Lee (phaeron), 2014–2023, GPL-2.0-or-later.
9194 Zeilen C++ (Methode: `cat *.cpp *.h | wc -l` in `a8rawconv/`).

---

## Frage A — Stimmt die Lizenzangabe unserer Ports? **JA (Entlastungsbefund)**

Beide Ports behaupten „GPL-2-or-later" — **die Angabe ist richtig und
vollständig belegt**, es ist keine vierte Instanz der
GPL-3.0-Port-Klasse (MF-638):

| UFT-Datei | Behauptung | Quelle im Repo | Kopf dort |
|---|---|---|---|
| `src/core/uft_interleave.c:5` + `include/uft/core/uft_interleave.h:7–11` | „Port of a8rawconv 0.95's compute_interleave() … GPL-2-or-later" | `a8rawconv/interleave.cpp:21` (`compute_interleave`) | GPL-2.0-or-later, Zeilen 1–16 |
| `src/core/uft_write_precomp.c:5` + `include/uft/core/uft_write_precomp.h:16–22` | „Port of … postcomp_track_mac800k … GPL-2-or-later" | `a8rawconv/compensation.cpp:21` (`postcomp_track_mac800k`) | GPL-2.0-or-later, Zeilen 1–16 |

Beide Header nennen zusätzlich URL, Datei **und** Funktion — das
erfüllt die Attributionspflicht in der Form, die MF-636 anmahnt.
Stichprobe Algorithmus-Treue: Schwellenformel, 5/12-Faktor und
Halbdistanz-Klammer in `uft_write_precomp.c:40–70` stimmen mit
`compensation.cpp:41–56` überein (einzige Abweichung: durchgängig
`float` statt `double` im Zwischenschritt der Schwellenrechnung —
dokumentiert im Port-Kommentar, keine Lizenzfrage).

## Frage B — Ist die vendorte Kopie aktuell? **JA, byteidentisch**

Methode: Dateiliste beider Seiten (`ls | sort`, `comm -3`), dann je
Datei `cmp` (Byte-Vergleich, ohne CRLF-Toleranz).

* **44 von 44** Quelldateien in `src/a8rawconv/` sind **byteidentisch**
  mit dem Repo-Stand `5db54b47` (`byte-diffs: 0`).
* Nicht vendort, absichtlich: 5 Build-Artefakte (`a8rawconv` =
  224-KB-Binary, `a8rawconv.sln`, `.vcxproj`, `.vcxproj.filters`,
  `make`). Kein Quellcode fehlt.
* Zusätzlich vendort: `COPYING` + `README.md` — die Lizenz reist mit
  der Kopie, wie es sich gehört.
* Version: `version.h` → `A8RC_VERSION "0.95"` auf beiden Seiten.
* Seit dem Vendoring-Commit (MF-467, 2026-08-22) hat das Repo **keinen**
  Quellcode-Commit erhalten (einziger Commit danach ist die
  Lizenz-Richtigstellung am selben Tag; Quelldateien zuletzt 2025-06-13
  hochgeladen). Kein stiller Drift.
* SPDX-Tor: `scripts/audit_spdx_policy.py:54` führt `src/a8rawconv`
  in `AUSGENOMMEN` — bestätigt.

**UNGEKLÄRT bleibt nur die Aufwärtsrichtung:** ob Avery Lee je eine
Fassung > 0.95 veröffentlicht hat. Zwei Suchen + pigwa-Mirror
(endet bei 0.91, 2015) + virtualdub.org (keine a8rawconv-Seite) ergaben
keine autoritative Bezugsquelle; AtariAge-Forum ist nicht abrufbar
(HTTP 403). Kopfzeilen sagen 2014-2020, Banner 2014-2023.

## Frage C — Stand `docs/A8RAWCONV_INTEGRATION_TODO.md`

Der Statusblock im Plan (MF-421/466) stimmt; hier gegen den Baum
nachgemessen:

| Baustein | Stand | Beleg |
|---|---|---|
| TA1 Write-Precomp | **erledigt** | `src/core/uft_write_precomp.c` (96 LOC), `tests/test_write_precomp.c` |
| TA2 Interleave | **erledigt** | `src/core/uft_interleave.c` (91 LOC), `tests/test_interleave.c`; MF-479 nutzt ihn im ATX-Schreiber |
| TA3 ATX-Plugin | **erledigt und über den Plan hinaus** | `src/formats/atx/uft_atx.c` heute 726 LOC (Plan sah ~400): Weak (`:96,:288–292,:379`), Duplikate (`:395–397`), EXTD/Long (`:70,:330`); Schreiber MF-474, Winkelpositionen MF-479, Verschränkungs-Politik MF-485; Tier T2 |
| TA4 SCP-Direct | Code gelandet (MF-254), HW-Bench extern (UFT-008) | unverändert |
| TA5 FM/MFM-Parser-Review | **offen** — und jetzt mit Befund unten dringlicher | kein Beleg eines Reviews im Baum |
| TA6 Worktree-Skelette | gegenstandslos | Verzeichnis existiert nicht |
| Nachtrag 2: Testvektor-Generator | **offen** | `tests/vectors/gen/` existiert nicht (`ls` → No such file) |
| Nachtrag 3: Atari-Referenz-Orakel | **halb**: vendort (MF-467), aber nie gebaut/registriert | „Wird von keinem Build kompiliert" (Commit-Text MF-467); kein Aufruf in `tests/` |

**Plan-Drift, klein:** Der Abschnitt „Lizenz-Hinweis zum Fork" sagt noch
„die LICENSE-Datei im Fork nennt CC0" — seit `5db54b47` (2026-08-22)
korrigiert. Bei der Übernahme der Vorschläge mit nachführen.

---

## Fähigkeits-Delta (Datei+Zeile beidseitig)

### 1. FM-Flux→Sektor kann UFT nicht — der Pfad ist ein Torso

Gemessen im Baum: der Encoding-Verteiler
(`src/flux/uft_flux_decoder.c:1779`) leitet FM auf `flux_decode_fm`
(`:748`). Dessen Sektor-Schleife (`:801–810`) findet Syncs, decodiert
aber **keinen** Sektor („FM sector decoding would go here … For now,
just note we found a sync"); `sector_count` wird nie erhöht, die
Funktion kann nur `FLUX_ERR_NO_SYNC` liefern. Der Kommentar `:776`
dokumentiert zudem, dass der FM-Pfad nie mit dem 288-min⁻¹-Profil lief.
Kein anderer FM-Flux-Sektorpfad gefunden (Methode: `grep -rln
"fm_decode|decode_fm|FM sector"` über `src/`, Treffer sind Container-
oder Stub-Dateien; Restunsicherheit in UNGEKLÄRT).

Gegenseite: `a8rawconv/sectorparser.cpp` (615 LOC) decodiert FM **und**
MFM WD177x-genau (Address-Marks, Gap-Handling, CRC), gespeist aus
`rawdiskscp.cpp`/`rawdiskkf.cpp`. Für Atari 810 Single Density (FM,
288 min⁻¹) ist das genau der Pfad, den UFT nicht hat. Damit ist
**Flux→ATR für SD-Disketten heute unmöglich** — die Wandlungsmatrix
(`src/core/uft_roundtrip.c`) führt entsprechend **null** Atari-Einträge
(Methode: `grep -oE "UFT_FORMAT_[A-Z0-9_]+"` → nur ADF, D64, G64, HFE,
IMG, SCP).

### 2. Encoder-Rückweg: ATR→SCP mit bekanntem Inhalt (Generator)

a8rawconv schreibt SCP aus Sektor-Images (`-of scp`, `-e
ordered/precise`, `-p` Taktabweichung 50–200 %). **Heute gemessen**, mit
lokalem MinGW g++ 13.1.0, Build in einem Befehl (`make`-Einzeiler über
`compileall.cpp`, rc=0):

```
a8rawconv atrcopy_dos2sd.atr gen.scp   → 18 589 254 Byte SCP (FM, 5 Rev)
a8rawconv gen.scp back.atr             → cmp: BYTE-IDENTISCH zum Eingang
```

Das ist der in Plan-Nachtrag 2 geforderte Testvektor-Generator, als
funktionierender Kreis belegt — Erwartung per Konstruktion bekannt.

### 3. Zwei-Hand-Oracle für ATR steht — heute erstmalig gemessen

```
a8rawconv atrcopy_dos2sd.atr out.xfd
cmp tests/corpus_free/atrcopy_dos2sd.xfd out.xfd → BYTE-IDENTISCH
```

Avery Lees Leser bestätigt robmcmullens Erzeuger byteweise — zwei
unabhängige Hände über dem ATR-Korpus. atrcopy allein war dieselbe
Hand wie der Korpus (MF-426); diese Lücke ist damit geschlossen —
als Messung, noch nicht als Test im Baum.

### 4. Bereitschafts-Messung: Atari-DOS-Dateisystem liegt fertig da, ohne Tür (neunter Fall)

`src/formats/atari/`: `atari_dos2.c` (1006 LOC, vollständiges DOS 2.x
inkl. `dos2_read_directory` `:359` füllt `disk->directory[]`),
`atari_sparta.c` (410 LOC SpartaDOS), `atari_check.c` (680 LOC fsck:
`check_filesystem`, `check_print_report`), dazu **zweite** Implementierung
`uft_atari_dos.c` (510 LOC, `uft_atari_dir_count/get/find`). Aufrufer
außerhalb des Verzeichnisses (Methode: `grep -rln <symbol> src include
tests` minus `src/formats/atari`): **ausschließlich die eigenen
Prototypen** (`include/uft/formats/atari_dos.h`,
`include/uft/uft_atari_dos.h`). Exakt die Gestalt von MF-629 (D64) und
MF-641 Nachtrag 1. floptool kennt kein Atari-DOS (MF-623) — UFT hätte
hier etwas, das das Referenzwerkzeug nicht hat, wenn die Tür aufgeht.

### Kein Delta (geprüft, verworfen)

* ATX-Weak/EXTD/Duplikate: UFT gleichauf oder besser (`uft_atx.c`, s.o.).
* VFD: Inventar-Abfrage `vfd` → `vorhanden: true`,
  `plugin_liste_vollstaendig: true` (zitiert aus `inventar.py query`).
* KryoFlux/SCP-Lesen, SCP-Direct-HAL, Apple-GCR: im Baum vorhanden
  (src/flux/, MF-254, WOZ/NIB) — Plan hatte diska2 schon abgelehnt.
* `-analyze`-Kalibriertabelle (288/300/360 min⁻¹): Fundus; UFT-OTDR
  deckt Histogramm-Analyse, die RPM-Profile stehen seit MF-486ff im
  Decoder.
* `rawdiskscript.cpp` (DSL): bleibt abgelehnt (Prinzip 3) — extern
  nutzen, nicht portieren.

## Was das Inventar sagt (zitiert)

`inventar.py query work/inv.json`: `atr` → `vorhanden: true, tier: T1b`
· `atx` → `vorhanden: true, tier: T2` · `xfd` → T1b · `dcm`/`pro` → T3
(aus `tiers`) · `vfd` → `vorhanden: true` · `interleave`, `write
precompensation` → `abgedeckt: false` („INVENTAR DECKT DAS NICHT AB …
von Hand prüfen") — von Hand geprüft: beide als Ports vorhanden
(`src/core/uft_interleave.c`, `src/core/uft_write_precomp.c`) ·
`atari dos filesystem` → schwacher Treffer `atari` — von Hand geprüft:
vorhanden, aber unerreichbar (Delta 4). Korpus: ATR/XFD von atrcopy
liegen (`tests/corpus_free/atrcopy_dos2sd.{atr,xfd}`); **kein** Atari-
Flux-Abbild im Korpus (Methode: `korpus`-Liste in `inv.json`, 22
Einträge, kein Eintrag mit Format scp/kryoflux und Atari-Herkunft).

## Oracle-Kandidat

**a8rawconv selbst.** Konsole, hardwarefrei, ein Compile-Befehl
(bewiesen: MinGW 13.1.0, rc=0), deterministische, byteweise prüfbare
Ausgaben (bewiesen: zwei `cmp`-Identitäten heute), Zone GRÜN. Rollen:
ATR/XFD/ATX-Zweitoracle (andere Hand als atrcopy), SCP-FM-Generator,
Differenzlauf-Gegenpart für TA5. Es liegt seit MF-467 im Baum —
gebaut und registriert hat es nie jemand.

## Einhängepunkt

`docs/A8RAWCONV_INTEGRATION_TODO.md` (ankerfähig laut
`docs/plans/README.md:44`), Bausteine TA5, Nachtrag 2, Nachtrag 3;
für Vorschlag 3 zusätzlich `docs/PLAN_v4.1.7.md` Nachtrag 1
(„Erst Türen suchen, dann bauen").

## Differenzlauf-Plan (TA5 / „WD177x-genauer"-Behauptung)

* **Binaries:** a8rawconv (aus `src/a8rawconv/` gebaut, Befehl oben) vs.
  UFT-Konverterpfad (`uftc_convert_scp_to_mfm_sectors`,
  `src/formats/uft_format_convert_flux.c:1107`).
* **Korpus:** (a) generierte SCPs aus Vorschlag 2 (FM SD, MFM ED/DD, je
  mit `-p 98/100/102` Taktabweichung, weak-markierte ATX-Quellen),
  (b) reale Atari-Flux-Aufnahmen aus der Beschaffungsliste.
* **Metrik:** je Spur decodierte Sektoren, CRC-ok-Zahl, Payload-Hash;
  Vergleichsbasis das bekannte Quell-ATR.
* **Toleranzen:** Reihenfolge der Sektoren frei; Timing-Felder außer
  Betracht; FM zählt erst nach Vorschlag 1 (heute 0 erwartet — das IST
  der Rotbeweis).

## Beschaffungsliste

1. **Reale Atari-8-bit-Fluxaufnahme (SCP oder KryoFlux-Stream), FM SD
   und MFM ED** — liegt nicht im Korpus (s.o.). Kandidat:
   archive.org-Preservation-Sets mit KryoFlux-Rohstreams; Lizenz des
   Disketteninhalts vor Aufnahme klären (dieselbe Frage wie SCOUT-5).
2. Sonst nichts — Oracle-Quelle, ATR/XFD-Fixtures und Generator liegen
   bereits im Baum bzw. Korpus.

## Aufwandsklasse

Vorschlag 1: **M–L** (FM-Sektordecoder ist Format-/Decoder-Layer →
EINFRIER-REGEL: Rotbeweis zuerst, Referenz `sectorparser.cpp` +
generierte Fixtures im Header; GPL-2-kompatibel portierbar). Vorschlag
2: **S**. Vorschlag 3: **S–M** (Verdrahtung, kein Neubau). Vorschlag 4:
**S** (Skript + Registrierung). Vorschlag 5: **M** (Differenzlauf).

## OPEN_ITEMS-Vorschlagsblock (max. 5 — zur Übernahme durch den Menschen)

| # | Vorschlag | Kennzahl (Regel 9) |
|---|---|---|
| **SCOUT-30** | **FM-Flux→Sektor-Pfad real machen.** Rotbeweis zuerst: generiertes FM-SCP (bekannter Inhalt) gegen `flux_decode_fm` — heute 0 Sektoren (`uft_flux_decoder.c:801–810` decodiert nicht). Referenz: `src/a8rawconv/sectorparser.cpp` (GPL-2.0-or-later, Zone GRÜN, im Baum liegend); Stufe 4 nach EINFRIER-REGEL (benannte Referenz + Rotbeweis + Referenz im Header) | angebotene Wandlungspfade **rauf** (Voraussetzung für SCP→ATR); ungeprüfte Formate runter (FM-Pfad erstmals testbar) |
| **SCOUT-31** | **Testvektor-Generator (Plan-Nachtrag 2) bauen:** Skript unter `tests/vectors/gen/`, ruft a8rawconv (Build-Einzeiler belegt) für ATR→SCP mit definierten Defekten (`-p`-Taktabweichung, weak-ATX-Quellen); ohne Binary ehrlicher SKIP. Kreis ATR→SCP→ATR heute byteidentisch gemessen | ungeprüfte Formate **runter** (Decoder-Positiv-/Negativkontrollen, T2-Weg); leckende Tests **null halten** (SKIP statt PASS) |
| **SCOUT-32** | **Atari-DOS-Tür öffnen (neunter Fall MF-641-Gestalt):** `atari_dos2.c`/`atari_check.c`/`uft_atari_dos.c` (2606 LOC, 0 Aufrufer außerhalb — Messung im Gutachten) verdrahten; Rotbeweis: UFT-Verzeichnisliste gegen atrcopy-Liste, heute rot, weil kein erreichbarer Weg existiert. Vorher entscheiden, welche der ZWEI Implementierungen die Tür bekommt und was mit der anderen geschieht | ungeprüfte Formate **runter** (`atr` T1b→T1-Weg: Inhaltsbeweis statt Containervergleich) |
| **SCOUT-33** | **a8rawconv als Zweitoracle registrieren** (Plan-Nachtrag 3 abschließen): Build im Testharness aus `src/a8rawconv/`, ATR/XFD-Kreuzprüfung als Test (heute als Einzelmessung belegt: XFD-Byteidentität gegen atrcopy-Korpus). Damit erstmals eine **zweite Hand** über dem ATR-Korpus | ungeprüfte Formate **runter** (`atr`/`xfd`-Absicherung durch unabhängige Hand); Fundament für SCOUT-30/31 |
| **SCOUT-34** | **TA5 als Differenzlauf abschließen** (Plan oben, Messplan im Gutachten): a8rawconv vs. `uftc_convert_scp_to_mfm_sectors` auf generiertem + realem Korpus; Ergebnis entscheidet Merge/Dokumentation, nie Meinung | ungeprüfte Formate **runter** (MFM-Konverterpfad erhält Referenzmessung); schließt letzten offenen Plan-Baustein |

Reihenfolge-Empfehlung: 33 → 31 → 30 → 32 → 34 (Oracle zuerst, dann
Generator, dann der Rotbeweis, der beide braucht).

## UNGEKLÄRT

1. Existiert eine Avery-Lee-Fassung > 0.95? Keine autoritative
   Bezugsquelle gefunden (pigwa endet 0.91; virtualdub.org ohne
   a8rawconv-Seite; AtariAge 403). Folge falls ja: Diff der Fassungen.
2. Ob ein anderer Pfad im Baum FM-Flux zu Sektoren decodiert (God-Mode-
   Decoder, `uft_core_stubs.c`-Reste) — grep-negativ, aber nicht
   erschöpfend; der Rotbeweis aus SCOUT-30 klärt es endgültig.
3. Lizenz des Disketteninhalts realer Atari-Flux-Aufnahmen
   (Beschaffung 1) — Eigentümer-Entscheidung, wie bei SCOUT-5.
4. Ob `uft_atari_dos.c` und `atari_dos2.c` dieselbe DOS-2.x-Semantik
   implementieren oder divergieren (zwei Implementierungen, beide ohne
   Aufrufer) — vor SCOUT-32 zu messen.

<!-- uebernommen: ausstehend -->
