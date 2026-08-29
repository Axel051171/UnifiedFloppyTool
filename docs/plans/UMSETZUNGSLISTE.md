# Umsetzungsliste — was jetzt perfekt umsetzbar ist

Stand 2026-08-28, nach MF-646. Geordnet nach **Umsetzungsreife**, nicht
nach Wichtigkeit: oben steht, was heute ohne Beschaffung, ohne
Eigentümer-Entscheidung und ohne offene Frage gebaut werden kann.

Jede Zeile nennt ihre **Kennzahl** (Regel 9, CLAUDE.md). Was keine
bewegt, steht unter „Fundus" und wird nicht eingeplant.

**Legende der Reife:**
🟢 sofort · 🟡 braucht einen Bau oder eine Messung · 🔴 wartet auf
Eigentümer oder Beschaffung

---

## Stufe A — heute baubar, nichts fehlt

### A1 🟢 FM-Fluss-Dekodierung real machen
**Kennzahl: Wandlungspfade rauf** (das erste Atari-Paar überhaupt)

`flux_decode_fm()` findet Syncs, setzt `detected_encoding` und
`avg_bitrate` — und erhöht `sector_count` **kein einziges Mal**.
Die Rückgabe kann nur `FLUX_ERR_NO_SYNC` sein. Im Rumpf steht wörtlich
„FM sector decoding would go here". Flux→ATR ist damit unmöglich, und
die Rundlauf-Matrix führt **kein** Atari-Paar.

* **Referenz:** `src/a8rawconv/sectorparser.cpp` (615 Z., WD177x-genau),
  **liegt im Baum**, Zone GRÜN (GPL-2.0-or-later, MF-643 nachgeprüft)
* **Fixture:** a8rawconv erzeugt es selbst — `ATR→SCP(FM)` lief heute
  byteidentisch im Kreis. **Keine Beschaffung.**
* **Rotbeweis:** generiertes FM-SCP gegen `flux_decode_fm` → heute
  0 Sektoren
* **Einfrier-Regel:** greift nicht — Fehler in Bestehendem mit benannter
  In-Tree-Referenz ist ausdrücklich erlaubt
* CHANGELOG-Zeile ist bereits berichtigt (MF-644)

### A5 ✅ 40-Spur-D64: die Belegungskarte liest den Disknamen — **erledigt (MF-649)**
**Kennzahl: ungeprüfte Formate runter** (`d64`; Hebung ist Moratoriums-Bedingung)

`uft_d64_parser_v3.c:1029ff` rechnet `entry_off = 4 + (track-1)*4`. Für
Spur 36 ist das **0x90** — und vierzehn Zeilen tiefer liest dieselbe
Funktion den Disknamen von `bam + 0x90`. Jedes 40-Spur-Abbild bekommt
für die Spuren 36–40 Namens- und ID-Bytes als Belegung; `free_blocks`
ist falsch. Die Größe wird ausdrücklich angenommen (`:524ff`).

* **Referenz, benannt und Zone GRÜN:** lib1541img (BSD-2) sondiert
  DolphinDOS/SpeedDOS/PrologicDOS statt zu extrapolieren
  (`cbmdosvfsreader.c:88-115, 395-435`); Eigenständigkeit belegt, also
  **nicht zirkulär**
* **Rotbeweis:** 40-Spur-D64 → `free_blocks` und BAM je Spur → heute
  liefert Spur 36 den Disknamen
* **Nebenbefund gleich mit:** `uft_d64_plugin.c:68` nimmt ab 205312 B
  **42** Spuren an, der v3-Parser lehnt dieselbe Datei ab
* **Warum vor A2:** A2 leitet genau diesen Leser aus. Ein Differenzlauf
  gegen `flophashes` auf falscher Belegung ist wertlos — grün wie rot

**Erledigt MF-649.** Rotbeweis `tests/test_d64_bam_40track.c` lief zuerst
rot und nannte die Zahlen: Spur 36 führte `0x55` — ein Byte aus dem
Disknamen —, und `free_blocks` stand bei **1003** statt **578**, also
**425 Phantomblöcke**. Danach grün, 269/269 im Prüfstand.
Der Fix sondiert DolphinDOS (`bam+0x1c+4*track`) und SpeedDOS
(`bam+0x30+4*track`) und nimmt eine Lage nur an, wenn sie sich für
**alle** Spuren 36–40 selbst bestätigt: das Zählbyte muss der Anzahl
gesetzter Bits entsprechen. Findet sich keine, bleiben die Einträge leer
und der Leser sagt es — statt zu raten. Prüfkriterium und Versätze aus
lib1541img `cbmdosvfsreader.c:16-34, 395-436` (BSD-2, Commit `face2dd`).
Der Plugin-Pfad war nicht betroffen: er liest die BAM nicht.

### A2 🟢 D64-Verzeichnis ausleiten (Phase 1, erster Commit)
**Kennzahl: ungeprüfte Formate runter** (`d64` T1b → inhaltlich belegt)

Der Leser **existiert und liest vollständig**
(`uft_d64_parser_v3.c:1085ff` füllt `directory[]`) — kein erreichbarer
Weg gibt ihn heraus. Von vier `uft_d64_v3_*`-Funktionen hat nur
`detect_protection` einen Aufrufer; `get_diagnosis` liefert bloß
Spurzahl und Größe.

* **Kein Neubau**, eine Ausleitung plus Aufrufer
* **Oracle liegt registriert:** `floptool flophashes d64 cbmdos` gibt
  CRC32 **und SHA-1 je Datei**; am Korpus gemessen: `UFT MARKER`,
  254 B, sha1 `56fea729…`
* **Grund ist sauber** (MF-644): `uft_d64_parser_v3.c` trägt keine
  Zuschreibung und nutzt **null** Funktionen aus dem auditierten
  `uft_gcr_ops.c` — eigene GCR-Tabellen
* **Rotbeweis:** Test stellt UFT-Liste gegen `flophashes` → heute rot,
  weil UFT nichts liefert

### A3 🟢 ATR↔XFD als Wandlungspfad
**Kennzahl: Wandlungspfade rauf** (12 → 13)

Das Paar fehlt komplett — der einzige ATR-Bezug in
`src/core/uft_roundtrip.c` ist ein Kommentar, der sagt, ein Wandler
fehle noch. Beide Formate stehen auf T1b, das byteverwandte Korpus-Paar
**liegt** (`atrcopy_dos2sd.atr` / `.xfd`).

* Der Unterschied ist ein 16-Byte-Kopf (MF-426)
* **Muster liegt vor:** Bit-Identitäts-Messung wie MF-532/533/539
* **Gegenprobe:** a8rawconv, heute gebaut — `ATR→XFD` byteidentisch

### A4 ✅ `uft_adl.c`-Identitätsfehler beheben — **erledigt (MF-654)**
**Kennzahl: ungeprüfte Formate runter** (`adl` T3 → T2)

Der Kopf behauptet „Acorn **DFS** Large … 80 × 1 × 16 × 256 = 327 680"
mit `heads = 1`. **Zweifach falsch:** `.adl` ist ADFS **L** =
**655 360** Byte, **zwei** Seiten — und 327 680 ist ADFS **M**.
Fabrikations-Klasse (FMT-2/3/10/11/12).

* **Zwei unabhängige Belege** (MF-642): `DiscImage_ADFS.pas:73-75`
  führt die drei Größen; das beigelegte `ADFS_L.adl` misst 655 360 Byte
* **Rotbeweis:** ein 655 360-Byte-Abbild gegen `uft_disk_open()` → heute
  NULL

**Erledigt MF-654 — und der Fehler war größer als beschrieben.**
`tests/test_acorn_adfs_identity.c` lief mit vier Fehlschlägen an. Der
schwerste stand nicht in der Aufgabe: `adl_open()` **öffnete** eine
655 360-Byte-Datei und beschrieb sie mit einer Geometrie über 327 680
Byte — die halbe Diskette, still. Dazu wiesen `read_track` und
`write_track` Seite 1 unbedingt ab.

Zweiter Befund am selben Ort: `uft_adf_arc.c` beanspruchte 655 360
ebenfalls, nannte es „ADFS-D“ (das misst 819 200) und hätte es
**linear** gelesen. ADFS L ist als einziges ADFS-Format
spurverschränkt — `DiscImage_Private.pas:547-570`, `FInterleave = 2`,
also `offset = (cyl*2 + head) * 4096`. Die Größe ist dort entfernt;
keine Fähigkeit geht verloren, weil `uft_adl.c` dieselben Endungen
führt und sie jetzt richtig liest.

Damit ist zugleich die SCOUT-41-Frage (lineare gegen verschränkte
Ablage) für ADFS L beantwortet — ausdrücklich **nicht** für D/E/F.
Kennzahl: **T3 55 → 53**, T2 19 → 21. 270/270 grün.

---

## Stufe B — ein Bau oder eine Messung davor

### B1 🟡 Atari-DOS: Tür öffnen **oder** löschen
**Kennzahl: ungeprüfte Formate runter** (`atr` T1b → inhaltlich)

**Zwei parallele** Implementierungen, zusammen ~3 400 Zeilen, **0
Aufrufer, 0 Tests, kein Anker**; der ExplorerTab kennt nicht einmal
`*.atr`. Nach der Verwaisten-Regel ein Löschfall — oder die Phase-1-Tür.

**Die Entscheidung ist ein Differenzlauf, keine Regelanwendung:** beide
Fassungen gegen den Korpus, gegen `lsatr` diffen. Stimmt eine überein,
bekommt sie den Anker und die andere fällt. Stimmt keine, fallen beide
und Atari-DOS wird sauberer Neubau.

* **Davor:** ~~`lsatr` bauen~~ — **erledigt (MF-650)**, gebaut, gelaufen,
  als siebtes Oracle registriert
* 🔴 **Neue Vorbedingung, gemessen (MF-650):** unser einziges ATR-Abbild
  ist **leer** — `lsatr` sagt „707 sectors free of 707 total", kein
  Verzeichniseintrag. Ein Differenzlauf darauf entscheidet auf
  Datei-Ebene **nichts**. B1 braucht zuerst ein nicht-leeres DOS-2-Trio
  (SD/ED/DD), siehe SCOUT-65. Das Urteil bleibt beim unabhängigen
  `lsatr`, deshalb dürfen die Fixtures von derselben Hand wie der Korpus
  stammen

### B2 🟡 `lsatr` + a8rawconv als ATR-Zweithände registrieren
**Kennzahl: begründete fünfte** — „Dateisysteme mit Inhalts-Differenzlauf"

atrcopy hat unseren ATR-Korpus **erzeugt**; als alleiniges Oracle wäre
der T1b-Eintrag **zirkulär** (`ORACLES.md`, fünfte Registrierungsfrage).
Zwei unabhängige Hände sind gefunden.

* `atrcopy` selbst braucht `numpy<1.23` — drei gemessene Abstürze unter
  NumPy 2.5.1; auf dieser Maschine (nur Python 3.13) nicht herstellbar

### B3 🟡 fdc_bitstream als **externes** Oracle
**Kennzahl: stützt Tier-Hebung** (MFM-Schiedsrichter)

Upstream im Prüfstand bauen, in `ORACLES.md` eintragen. **Kein
Zurückholen** — die vendorte Kopie ist mit MF-626 gelöscht, und ein
Revert brächte 2795 Zeilen zurück, die die Verwaisten-Regel bei jedem
Durchgang erneut anfasst. Eigentümer-Entscheidung bereits gefallen
(MF-644).

### B4 🟡 HFE-Fixture mit echter Encoding-Kennung
**Kennzahl: ungeprüfte Formate runter** (`hfe`)

Unser einziges HFE-Abbild trägt `track_encoding = 0xFF` und
`interface_mode = 0xFF` (selbst nachgemessen). Der Zweig
`case HFE_ENC_AMIGA_MFM:` (`uft_hfe.c:137`) wird **nie durchlaufen**.

* Ein HxC-natives Abbild trägt `0x01`/`0x04`
* ~~**Lizenz des Fixtures ist Zone PRÜFEN** → 🔴 bis geklärt~~ —
  **hinfällig (MF-650).** Der Aufklärer hat `hxcfe` gebaut und das
  Fixture **selbst erzeugt**: `track_encoding = 0x01`,
  `interface_mode = 0x04` gemessen. Damit läuft der tote Zweig
  `uft_hfe.c:137` erstmals, und die Lizenzfrage an einem Fremd-Fixture
  stellt sich gar nicht. Reife: 🟢, gehört nach Stufe A

---

## Stufe C — wartet auf Eigentümer oder Beschaffung

| # | Sache | Wartet auf |
|---|---|---|
| C1 🟡 | `LIZ-1` — **124** Attributionen ohne Lizenz daneben, gemessen mit `scripts/audit_attribution_licence.py`. **Die Zahl 14 aus MF-650 war falsch** (MF-651): sie rechnete auf einer Handklassifikation, deren Zensus `Reference:` gar nicht sah. Heute abgeleitet statt gepflegt; 32 sind bereits geheilt (156 → 124) | Fleißarbeit, keine Sprechstunde — aber mehr davon als gedacht |
| C2 🔴 | `LIZ-2` — drei AIR-Dateien (GPL-3.0, eine erreichbar), XCopy-Port | Sprechstunde. `uft_ipf_air.c` kostet IPF-Lesen |
| C3 🔴 | Klick-Session | MF-496, MF-501, Fluss-Widget, D64-Browser — eine Sitzung |
| C4 🔴 | Acorn-Korpus | **selbst erzeugbar** mit DIMConsole (`new`+`add`+`savecsv`) — braucht Lazarus/FPC |
| C5 🔴 | Hardware-Warenkorb | USB-Floppy, ZoomFloppy, Teensy |
| C6 🔴 | `dms` T3 → T1b | xDMS-Fixture aus dem Aminet mit Verteil-Erlaubnis |

---

## Fundus — bewusst nicht eingeplant

| Sache | Grund |
|---|---|
| DiskSpare, „2M", 5-and-3-Apple-GCR | Einfrier-Regel: neue Formate erst nach der 1:2-Bedingung |
| AddNoise-Dither-Mining | **erzeugt Daten** — Forensik-Vorbehalt, nicht verhandelbar |
| StepStick, siebter Controller | Hardware-Folge, MF-310 |
| Analog-Oszilloskop-Ingest | Fixture abgelehnt (Urheberrecht); Referenz später selbst aufnehmen |
| `flux-analyze` (Ordinal-Suche, Dewarp) | **GPL-3.0** — algorithmisch wertvoll, aber Zone GELB. Erst nach `LIZ-1`/`LIZ-2` |
| Acorn-Erweiterungen über die Hebung hinaus | 1:2-Regel: erst Phase 1 beweisen |

---

## Was die Reihenfolge trägt

A1, A3, A4 und A5 sind **unabhängig voneinander** und brauchen
niemanden. Sie bewegen zusammen drei der vier Kennzahlen und schließen
zwei Fabrikations-Fälle.

**Eine einzige Reihenfolgebindung:** A5 vor A2. Sie ist nicht
organisatorisch, sondern inhaltlich — A2 leitet den Leser aus, den A5
repariert, und ein Differenzlauf auf falscher Belegung beweist nichts
(MF-648).

B1 hängt an einem `make`, B2 an einer Python-Umgebung, B3 an einem
Upstream-Bau — alles Agentenarbeit.

C ist die Sprechstunde. **Sechs Posten, ein Termin**, und `LIZ-1` geht
allen anderen Lizenzfragen vor: solange 124 Attributionen ohne Lizenz
dastehen, ist jede Aussage über die Gesamtlage vorläufig.
