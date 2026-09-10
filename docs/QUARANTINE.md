# Quarantäne-Liste

Verfahren: [`QUARANTINE_PROCESS.md`](QUARANTINE_PROCESS.md).
Schema der Felder dort in §3.

> **Kennzahl.** Die Zahl **offener** Zeilen (Weg noch nicht beschritten)
> ist die Datenquelle für „Dateien mit ungeklärter Herkunft" — die
> begründete fünfte Release-Kennzahl (CLAUDE.md, MF-640).
>
> **Stand 2026-09-10: 2 vollzogen, 6 vorgemerkt, 2 aufgelöst.**
>
> MF-1002: `uft_caps_ipf.c` ist von „vorgemerkt" nach „vollzogen"
> gewandert — gelöscht. Die Zeile galt als die teuerste der Liste,
> weil sie als einzige eine **Fähigkeit** kostete. Gemessen kostete
> sie keine: die eine erreichbare Funktion antwortete verkehrt herum
> (`false` für ein echtes IPF, `true` für vier Bytes `00 00 00 01`).
>
> MF-742: hier stand **0 aufgelöst**. Gemessen an den Tabellen sind
> es **zwei** — `uft_gcr_ops.c` und `uft_d64_g64.c` unter
> „Rehabilitiert (Weg 1)". Die Zahl unterberichtete **erledigte
> Arbeit**; das ist die Richtung, in der ein Fehler am längsten
> unbemerkt bleibt, weil niemand nachfragt, warum etwas noch offen
> ist.
>
> Diese Zeile ist jetzt für Menschen da, nicht als Quelle: die
> Zahlen leitet `scripts/quarantine_stand.py` aus den Tabellen ab,
> und Tor 44 hält Prosa und Messung zusammen.
>
> MF-741: die fünfte vorgemerkte Zeile ist `uft_track_analysis.c`+`.h`.
> Sie kam nicht über eine Selbsterklärung herein wie die vier davor,
> sondern über den **Idiom-Test** — ihre Textähnlichkeit zur
> XCopy-Nachbarzeile beträgt 1,8 % und hätte sie freigesprochen.
>
> **Diese Zahl ist von Hand gepflegt** und speist über
> `scripts/gen_stand.py:139` eine der Release-Kennzahlen. Sie ist
> damit ein Kandidat für dieselbe Behandlung wie die
> Wandlungsmatrix (MF-541): ableiten statt zählen. Offen.

---

## Vollzogen

### `src/protection/c64/uft_track_align.c` (+ Header + Test)

| Feld | Inhalt |
|---|---|
| **Datei** | `src/protection/c64/uft_track_align.c` (1175 Z.), `include/uft/protection/uft_track_align.h` (622 Z.), `tests/test_track_align.c` (602 Z.) |
| **Verdacht** | **nibtools** (rittwage), Zone **GELB** (GPL-3.0). Beleg: Kopf `:5` „Based on nibtools by Pete Rittwage and Markus Brenner", `:479` nennt `prot.c align_rl_special()` |
| **Datum der Quelle** | LICENSE erst **2025-01-30** (`a549c18`) — davor **lizenzlos**. Aufnahme in unseren Baum **2026-02-08** (`4d622192`), also nach der GPL-3-Lizenzierung |
| **Betroffene Fähigkeit** | **keine.** 34 exportierte Funktionen, **0 Produktions-Aufrufer** (der einzige Kandidat `find_sync` war ein Namensgleichklang: `uft_mfm_sector_parser.c:104` und `uft_g64.c:223` führen je ein eigenes `static`) |
| **Audit-Stand** | **portiert** — belegt. `shift_buffer_left` zeichengleich samt des erfundenen Bezeichners `carryshift`, des Sentinels `tempbuf[length]=0x00` und des Schleifenausdrucks; **sieben** wörtliche Kommentar-Echos („back up a little", „set first byte to shift", „shift buffer left to edge of sync marks", „36 - 42 (non-standard)"); alle 14 `prot.c`-Funktionen haben eine Entsprechung, **acht namensgleich** |
| **Vorgesehener Weg** | **2** — Clean-Room-Neubau, wenn ein Baustein ihn verlangt |
| **Oracle** | `nibscan` (nibtools) — hardwarefrei mit MinGW baubar, vom Scout gebaut und gelaufen. **Noch nicht registriert**, siehe `ORACLES.md` §vorgemerkt |
| **MF** | Quarantäne **MF-635**; Bau-Nachzug **MF-637** (drittes CMakeLists übersehen) |

---

### `src/formats/ipf/uft_caps_ipf.c`

| Feld | Inhalt |
|---|---|
| **Datei** | `src/formats/ipf/uft_caps_ipf.c` (790 Z.) — gelöscht MF-1002 |
| **Verdacht** | „**Based on SPS CAPS Library** (Software Preservation Society)", `:5` — **kein SPDX, keine Lizenzangabe, keine Fundstelle** (nachgemessen MF-815: `grep -c SPDX` = 0). Die CAPS-Bibliothek ist proprietär und quellgeschlossen; IPF ist unveröffentlicht, und die SPS behält sich seine Erzeugung vor. „Based on" ist bei einer solchen Quelle eine Aussage über **Verteilbarkeit** |
| **Betroffene Fähigkeit** | **keine — gemessen, nicht geschätzt.** Bis MF-1002 stand hier „IPF-Erkennung, erreichbar". Erreichbar war sie; eine Fähigkeit war es nicht. Am belegten Pfad übersetzt und ausgeführt:<br>`uft_caps_is_ipf(echtes IPF, "CAPS")` = **FALSE**<br>`uft_caps_is_ipf(kein IPF, 00 00 00 01)` = **TRUE** |
| **Ursache** | `read_block_header()` legt den **rohen ASCII-Vierer** des Satzkopfs als BE-u32 in `header->type`. Verglichen wird gegen eine interne Aufzählung `1..10` (`IPF_BLOCK_CAPS 1`). Für „CAPS" steht dort `0x43415053`, nie `1` — deshalb kann `switch (header.type)` in `uft_caps_load_image()` für **keinen** der zehn Satztypen je greifen. Die Datei war strukturell außerstande, ein IPF zu lesen. Übrig blieb ein **falsch positiver** Zweig in der Sonde: Konfidenz **90** für jede Datei, die mit `00 00 00 01` beginnt — nach MF-729 die Stufe „Merkmal getroffen" |
| **Klasse** | **MF-961** — dort probte `86f` auf „86BX", ein Magic, das in **keiner** echten Datei steht. Beide Male sah die Merkmalstafel gesund aus, während der Leser an der Wirklichkeit vorbeigriff |
| **Vollzogener Weg** | **Löschung.** MF-699 („erst der Ersatz, dann die Löschung") ist erfüllt, ohne dass etwas zu ersetzen war: die richtige Erkennung stand schon eine Zeile darüber in `ipf_plugin_probe()` — der ASCII-Vierer „CAPS", der wirklich am Anfang jeder IPF-Datei steht |
| **Mitentfernt** | der falsch-positive Sondenzweig, die `.pro`-Zeile, zwei Testziele in `tests/CMakeLists.txt` und die Deklaration in `include/uft/protection/uft_amiga_caps.h` — **P3-264-Lehre**: keine Zusage ohne Umsetzung stehen lassen (MF-549 hatte damals die Fehlermeldung berichtigt und die Deklaration vergessen) |
| **Beweis** | **Lösch-Beweispipeline** aus `audit_cleanup_2026_08.md`, sechs Stufen. Stufe 1 („steht der `.pro`-Eintrag in einem bedingten Block?") schlug an und war ein **Fehlalarm meines Zählers** — von Hand nachgesehen, der Eintrag steht in einer schlichten `SOURCES`-Liste. Stufe 4: alle 13 exportierten Symbole ohne Aufrufer. Rotbeweis `tests/test_ipf_sonde_beansprucht_nur_ipf.c` (4 Fälle) |
| **MF** | **MF-1002** |

---

## Vorgemerkt — Audit fertig, Vollzug wartet auf `LIZ-2`

Diese vier liegen **noch im Baum und werden gebaut**. Das Audit ist
abgeschlossen; der Vollzug ist eine Eigentümer-Entscheidung, weil einer
der Fälle eine Fähigkeit kostet.

| Datei | Zeilen | Verdacht (Beleg im **eigenen** Kopf) | Fähigkeit | Weg | Oracle |
|---|---|---|---|---|---|
| `src/formats/ipf/uft_ipf_air.c` | 1042 | „**Full port** of AIR `IPFReader.cs`/`IPFStruct.cs`/`IPFWriter.cs` to C. Original: © 2014 Jean Louis-Guerin (**GPL-3.0**)" | **IPF-Lesen** — das Plugin ruft `ipf_air_alloc` (`:70`), `_free` (`:78`), `_get_geometry` (`:85`), `_get_track_meta` (`:135`); keine statischen Gleichnamen | **3** — entschieden, Register unten (MF-917) | capsimg — Lizenz **gemessen** (SPS DECODER LIBRARY v1.02, GPL-**un**vereinbar): Helfer über Prozessgrenze, nie Oracle im Baum |
| `src/formats/kfx/uft_kfstream_air.c` | 922 | „Full port of AIR `KFReader.cs` to C. © 2013-2015 SPS & Jean Louis-Guerin (**GPL-3.0**)" | **keine** — 0 Aufrufer, steht in `orphan_baseline.txt` | 2 | KryoFlux DTC |
| `src/formats/stx/uft_stx_air.c` | 1049 | „Full port of AIR `PastiRead.cs`/`PastiStruct.cs`/`PastiWrite.cs` to C. © 2014 Jean Louis-Guerin (**GPL-3.0**)" | **STX-Lesen** — berichtigt MF-960. Hier stand „**keine** — 0 Aufrufer, steht in `orphan_baseline.txt`". **MF-854 (2026-09-04) hat den Port zum registrierten Leser gemacht**: `src/formats/stx/uft_stx_plugin.c:65,70,74,86` ruft `uft_stx_air_open/_close/_cylinders`. Die alte Angabe bezifferte, was eine Loeschung kostet — nach ihr war Weg 2 billig. Heute kostet sie **STX-Lesen**. Das Verwaisten-Tor meldet die Grundlinien-Zeile seither als streichbar; das Register ist nicht nachgezogen worden | 2 | Pasti-Spec liegt (T2-Quelle) |
| `src/formats/amiga/uft_amiga_protection.c` | 766 | „C99 **port** of XCopy Pro (1989-2011) 68000 Assembly algorithms", `:47` „Port of `ROL.L #1,D0`" — **keine Lizenz genannt** | **keine** — 0 Produktions-Aufrufer, 1 Test | **entfernen** — MF-744: die Lizenz liegt vor und gestattet **keine Bearbeitung** . **BERICHTIGT MF-746:** ein Bearbeitungsrecht gibt es sehr wohl — `Readme 2011` sagt „You are welcome to enhance it or develop further versions". Es ist aber an „**just keep it free (don't sell it)**" geknüpft, und die Lizenz daneben verbietet kommerzielle und behördliche Nutzung. Ein Verkaufsvorbehalt ist eine zusätzliche Beschränkung im Sinne von GPL §6 — damit **GPL-inkompatibel**, keine Rechtsverletzung. Kein Nachbau: die Fakten sind zu 1 von 4 belegt und 2 widerlegt (MF-740). | entfällt |
| `src/analysis/uft_track_analysis.c` + `.h` | 1050 | „Universal track analysis algorithms **derived from** XCopy Pro (1989-2011)“ — **keine Lizenz genannt**. Nachgetragen MF-741: der Idiom-Test (MF-696) belegt **dieselbe** Ableitung wie die Zeile darueber — **zwoelf** Bezeichner kommen in genau diesen beiden Dateien vor und in **keiner** der uebrigen 715 (`detect_breakpoints`, `has_breakpoints`, `gap_sector_index`, `unique_lengths`, `rol32`, …). „Breakpoint“ ist die Uebersetzung von „Bruchstelle“ aus dem Original-Assembly-Kommentar. Die Textaehnlichkeit betraegt **1,8 %** und haette Entwarnung bedeutet. | **keine** — 20 Exporte, 0 Aufrufer ausserhalb; steht aber im Gegensatz zur Zeile darueber **noch im qmake-Bau** (`.pro:1219`) | 2 | fehlt — und die Fakten selbst sind unbelegt (MF-740) |
| `include/uft/formats/supercopy_formats.h` + `src/formats/cpm/uft_supercopy_detect.c` | 851 | „**SuperCopy v3.40 SELECT.DAT** — CP/M-Format-Datenbank … 313 CP/M-Diskettenformate **aus dem SuperCopy-Kopierprogramm** (1991) … Quelle: SuperCopy v3.40 von Oliver Müller“ — **keine Lizenz genannt, kein SPDX in beiden Dateien**. Es ist keine Portierung von Code, sondern eine **extrahierte Datentabelle** aus einer fremden Anwendung; ob die Sammlung von Geometrieparametern eine eigene Schutzfähigkeit hat, ist ungeprüft. Gefunden MF-914, nachdem das Herkunfts-Tor um `quelle:` erweitert wurde — vorher war die Datei für das Tor **unsichtbar** | **keine** — alle fünf Exporte (`sc_detect_by_geometry`, `sc_detect_refine`, `sc_get_stats`, `sc_detect_print`, `sc_iterate_by_density`) haben **0 Aufrufer** außerhalb der eigenen Datei; die Datei steht aber im qmake-Bau | **1** (Rehabilitierung) prüfen: sind reine Geometrieparameter überhaupt schöpferisch? Sonst **2** | `cpmtools` diskdefs — selbst noch ungemessen (`LIZ-1`) |
### Register nach MF-699 — `uft_ipf_air.c` (IPF-Lesen)

> **Aufgestellt MF-917.** Die Zeile oben stand seit MF-638 mit dem Feld
> „Weg **3** oder 2" und „Lizenz ungeprüft" — also ohne Entscheidung.
> §5 nennt das ausdrücklich keinen Ausgang: „Bloßes Liegenlassen ist
> kein Weg X."

| Pflichtfeld | Inhalt |
|---|---|
| **Nachbau-Route** | **Weg 3** — Helfer über Prozessgrenze. Weg 2 (Clean-Room aus Spec) ist versperrt: das IPF-Format ist **bewusst** undokumentiert, die SPS behält sich seine Erzeugung vor. Weg 1 scheidet aus, weil die Datei sich im eigenen Kopf als „Full port" erklärt |
| **Oracle-Kandidat** | `capsimg` selbst — in **Doppelrolle**: Helfer im Betrieb *und* Prüfreferenz für den Datenfluss-Schnitt. **Nie** Teil der Verteilung, nie im Baum |
| **Kennzahl-Bezug** | **fünfte** Kennzahl (Dateien mit ungeklärter Herkunft). Ein fertiger Helfer macht `uft_ipf_air.c` entbehrlich und schließt die **teuerste** Zeile dieser Liste — die einzige der fünf, die eine Fähigkeit kostet |
| **Aufwand** | **mittel.** UFT-Seite: **erledigt** (MF-917). Helfer-Seite: offen, aber ohne Dekodierlogik — `capsimg` deutet, der Helfer schreibt nur Index und Beilage |

**Die Lizenz ist jetzt gemessen, nicht geschätzt.** `LICENCE.txt` der
SPS DECODER LIBRARY v1.02 (Quelle: `simonowen/capsimage`, dort das
vollständige v5.1-Paket) sagt wörtlich: *„Redistributions may not be
sold, nor may they be used in a commercial product or activity."* Das
ist eine **zusätzliche Beschränkung** im Sinne von GPL §6 — dieselbe
Rechtslage wie bei XCopy Pro (MF-746), also **unvereinbar**, nicht
verletzt. Folge: kein Einlinken, keine Quelle im Baum, keine
Auslieferung; der Benutzer installiert den Helfer selbst und stimmt
dabei der Lizenz der SPS zu.

**Was seit MF-917 im Baum steht** — und was ausdrücklich nicht:

| | |
|---|---|
| Vertrag | `docs/specs/capsimg-helper/PROTOCOL.md` (Fassung 1, mit Prüfvektoren H1–H6 für die andere Seite) |
| UFT-Seite | `src/formats/ipf/uft_ipf_helper.c` + Kopf; **verdrahtet** — `uft_ipf_plugin.c` fragt den Helfer **zuerst** |
| gemessen | `tests/test_ipf_helper.c`, 8 Prüfungen, **7 von 7 Mutationen fallen an der erwarteten Stelle** |
| **nicht** im Baum | der Helfer selbst, `capsimg`, irgendein davon abgeleiteter Code |
| **nicht belegt** | dass ein gegen `capsimg` gebauter Helfer diese Antwort erzeugt — auf dieser Maschine liegt kein `capsimg` (gemessen: `which capsimg` leer, keine `CAPSImg.dll`). Offen als **P3-190** |

**Die Reihenfolge bleibt die von MF-699:** erst der Ersatz, dann die
Löschung. `uft_ipf_air.c` bleibt der Rückfall, solange kein Helfer
eingerichtet ist — aber wer einen einrichtet, bekommt ihn nie mehr zu
sehen. Damit ist die ausgelieferte Fähigkeit dieselbe wie vorher, und
der legale Weg ist **ab sofort** benutzbar statt nur beschrieben.

---

**Audit-Stand aller fünf: portiert** — bei den drei AIR-Dateien ohne
jedes Ähnlichkeitsaudit, weil sie sich **selbst** als „Full port"
erklären und die Lizenz im eigenen Kopf nennen. Bei XCopy Pro nach §4
„unklar ⇒ wie portiert".

Die fünfte Zeile (`uft_track_analysis`) ist **anders zustande gekommen
als die vier darüber** und deshalb der lehrreiche Fall: sie erklärt sich
nicht selbst als Port, ihre Textähnlichkeit zur Nachbarzeile beträgt
**1,8 %**, und ein Ähnlichkeitsvergleich hätte sie freigesprochen.
Belegt hat sie erst der **Idiom-Test** aus §4 — zwölf Bezeichner, die
nur diese beiden Dateien im ganzen Baum teilen. Das ist genau der Grund,
warum dort „beweiskräftig sind Idiome, nicht Fakten" steht (MF-741).

Und sie ist die einzige der fünf, die **noch im qmake-Bau steht**
(`.pro:1219`). `uft_amiga_protection.c` ist seit MF-699 herauskommentiert;
diese Datei ist es nie gewesen, weil bis MF-739 niemand wusste, dass sie
dieselbe Ableitung ist.

**MF:** Audit **MF-638**, Vollzug offen (`LIZ-2`).

> **Warum nicht längst vollzogen.** Zwei der drei AIR-Dateien und die
> XCopy-Datei sind Waisen — dort kostet die Quarantäne nichts, und §1
> Vorrangregel würde sogar die einfachere Löschung erlauben. Nur
> `uft_ipf_air.c` kostet **IPF-Lesen**, und §6 sagt dazu klar: der
> Fähigkeitsverlust ist kein Argument gegen die Quarantäne. Die
> Entscheidung ist trotzdem die des Eigentümers, weil sie eine
> beworbene Fähigkeit zurücknimmt — Schritt 3 des Verfahrens
> („IPF: erkannt, nicht gelesen").

---

## Was **nicht** auf dieser Liste steht, und warum

§1 trennt Quarantäne von normaler Arbeit. Drei Fälle aus derselben
Woche, die trotz Löschung **keine** Quarantänefälle sind:

| Gelöscht | Warum keine Quarantäne |
|---|---|
| `src/flux/fdc_bitstream/` (12 Dateien + 14 Header, 6483 Z.) | **MIT** mit `LICENSE.md` und Autorennennung — Zone GRÜN, keine Herkunftsfrage. Reiner Verwaisten-Fall (MF-626). Der Rückweg ist trotzdem benannt: **externes Oracle**, siehe `ORACLES.md` |
| `src/formats/c64/uft_nib_format.c` (+ Header + Test) | Waise ohne Herkunftsfrage — 27 exportierte Funktionen, **0** Produktions-Aufrufer. Verwaisten-Regel (MF-635) |
| `src/formats/uft_format_registry_v2.c` (587 Z.) | Waise; Inhalt als Dokument gesichert (`FORMAT_CATALOG.md`), MF-624 |

**Die Zuschreibung wurde trotzdem notiert** (§1 Anmerkung): `fdc_bitstream`
= yas-sim, MIT. Damit ist die Frage „gibt es Geschwister derselben
Quelle?" später beantwortbar, ohne die Historie zu durchsuchen.

---

## Rehabilitiert (Weg 1) — zur Vollständigkeit

Zwei Dateien standen unter demselben Verdacht und wurden **freigesprochen**.
Sie waren nie in Quarantäne, gehören aber ins Protokoll, weil das Audit
dieselbe Methode benutzte:

| Datei | Verdacht | Audit | Folge |
|---|---|---|---|
| `src/formats/c64/uft_gcr_ops.c` | „Based on nibtools gcr.c" | **eigenständig** — `strip_runs`, `kill_partial_sync`, `reduce_runs` je anderer Algorithmus, andere Parameter, andere Speicherstrategie; ein Echo, und das ist der Fachbegriff „header checksum" | Kopf berichtigt (MF-635): „Verhalten nach der nibtools-Dokumentation und -Quelle — EIGENSTÄNDIGE Implementierung, kein Port" |
| `src/formats/c64/uft_d64_g64.c` | „Based on nibtools" | **eigenständig** — keine Entsprechung zu `convert_GCR_sector`, keine der auffälligen Konstanten (`0x07`, `0x4b` „Original Format Pattern"), keine Tri-Bit-/Low-Frequency-Notizen | Kopf berichtigt (MF-635) |

Und der Entlastungsbefund, der die Methode bestätigt: `src/core/uft_interleave.c`
und `src/core/uft_write_precomp.c` erklären sich als „Port of a8rawconv
0.95 … **(GPL-2-or-later)**". Nachgeprüft am Original (MF-643): die
Angabe **stimmt**, GPL-Kopf wörtlich in `interleave.cpp:1-16` und
`compensation.cpp:1-16`. Zone GRÜN, kein Handlungsbedarf.

---

## Offene Vorbedingung

`LIZ-1` — **124** Attributionen nennen eine fremde Codebasis ohne Lizenz daneben (MF-651; die frühere Zahl 48 stammte aus einem Zensus, der `Reference:` nicht kannte)
(`scripts/audit_spdx_policy.py`, Attributionsstufe). Solange die nicht
geklärt sind, ist jede Aussage über die Gesamtlage des Baums vorläufig,
und die Lizenzentscheidung in `LIZ-2` steht auf unvollständigem Grund.
