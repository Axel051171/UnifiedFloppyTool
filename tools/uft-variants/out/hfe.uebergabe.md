# Übergabe: Format-Varianten `HFE` — tiefengeprüft
Stand 2026-08-29 (zweiter Lauf; der erste verlor alles an ein
Sitzungslimit — dieses Dokument wurde diesmal früh geschrieben und
fortgeschrieben). Evidenz: `work/hfe.evidenz.json` ·
Korpus: `work/korpus.json` · Regeln: `AGENT.md`.

Quellenlage: **hxcfe** (Format-Urheber, Klon
`tools/uft-scout/work/HxCFloppyEmulator`, HEAD `05b53aa9`, 2026-08-22)
und **samdisk** (vendort, `src/samdisk/hfe.cpp`) sind unabhängige
Implementierungen → Zwei-Quellen-Regel erfüllbar. `uft_selbst` nur für
die Widerspruchsfrage (Regel 2). **Fehlende Quellen:** die Klone
`fluxengine` und `mame` aus `config.json` liegen nicht unter
`tools/uft-scout/work/` — beide implementieren HFE und hätten als
dritte/vierte Stimme gedient; ihr Fehlen ist gemeldet, nicht ersetzt.
Web-Belege (FlashFloppy, Greaseweazle) sind unten mit Abrufdatum
geführt; sie sind Abnehmer-Belege, keine Ersatz-Zweitquellen für
Header-Semantik.

---

## 0 · Die Offset-8-Frage, entschieden

**HFE unterscheidet seine Generationen an der SIGNATUR, nicht am
Byte auf Offset 8.** Gemessen am Urheber:

* Der v1-Writer schreibt Signatur `HXCPICFE` und `formatrevision=0`
  (`hfe_writer.c:116,139`); der v3-Writer schreibt Signatur `HXCHFEV3`
  und **ebenfalls** `formatrevision=0` (`hfev3_writer.c:125,148`).
  Ein HFEv3 hat also rev=0 — Offset 8 trägt die Generation nicht.
* Offset 8 ist trotzdem nicht immer 0: der **ExtHFE**-Writer schreibt
  `HXCPICFE` mit `formatrevision=1` (`exthfe_writer.c:109,126`).
  Am erzeugten Fixture gemessen: Byte 8 = `0x01`
  (`amiga_dd_HXC_EXTHFE.hfe`, xxd: `4858 4350 4943 4645 01…`).
* Beide unabhängigen Leser lehnen rev≠0 **laut** ab:
  HxC `hfe_loader.c:161-163` („Format version %d currently not
  supported", rechnet dabei `formatrevision+1` — daher die Ausgabe
  „version 2" für rev=1); SamDisk `hfe.cpp:107-108`
  (`throw … "unsupported HFE format revision"`).
* Die früher in `config.json` stehende Zuordnung „1 = v2" war also ein
  Halbwissen: rev=1 ist **ExtHFE**, und nur HxCs eigene Fehlermeldung
  nennt das rechnerisch „version 2". Kein Leser im Feld akzeptiert es
  als eigene Generation. Die Erkennungstabelle bleibt wie von MF-656
  bereinigt: `map {"0": "rev0"}`, rev=1 hier als Variante dokumentiert.

---

## 1 · Variantentabelle (Version · Erkennung · Unterschied · Beleg ×2)

| # | Variante | Erkennung (Byte, Offset) | Verhaltensunterschied | Beleg 1 (hxcfe) | Beleg 2 (unabhängig) | Status |
|---|---|---|---|---|---|---|
| 1 | **HFE v1** (rev0) | `"HXCPICFE"`@0, Byte 8 = 0x00 | Basisfassung: 512-B-Header, LUT in 512er-Blöcken, Spuren seiten-verschränkt in 256-B-Blöcken, Bits LSB-first | `hfe_loader.c:100,151,161` | `src/samdisk/hfe.cpp:8,20-27,107` | **[MESSBAR]** |
| 2 | **ExtHFE** (rev1) | `"HXCPICFE"`@0, Byte 8 = 0x01 | Layout identisch zu v1 (per `diff hfe_writer.c exthfe_writer.c` gemessen); einziger Datenunterschied: Spurende-Auffüllung mit festem Muster `00 06 AA AA` (bit-invertiert, `extaddpad`, `exthfe_writer.c:44-72`) statt Wiederholung des letzten Bytes. Zielgerät „standalone emulator" (`exthfe_writer.c:101`). **Write-only selbst beim Urheber**: Loader-Slots = 0 (`hfe_loader.c:395-397`), `-modulelist` meldet `HXC_EXTHFE; W`. Beide unabhängigen Leser lehnen rev=1 laut ab (s. §0) | `exthfe_writer.c:109,126` | SamDisk lehnt ab: `hfe.cpp:107-108`; Existenz zusätzlich am eigenen Messlauf (Fixture-Kopfbyte 0x01) | **[MESSBAR]** (Existenz + Ablehnung); ob irgendein Gerät rev1 verlangt: [ZU VERIFIZIEREN] |
| 3 | **HFE v3** | `"HXCHFEV3"`@0 (Byte 8 = 0x00) | Spurdaten sind ein **Opcode-Strom** im bit-invertierten Raum: `0xF0 NOP`, `0xF1 SETINDEX`, `0xF2 SETBITRATE` (2 B), `0xF3 SKIPBITS` (2 B, `&0x7`, wirkt auf das NÄCHSTE Datenbyte), `0xF4 RAND` = weak bits, emittiert `8-skip` Bits und setzt skip zurück (`hfev3_loader.c:363-427`, Opcodes `hfev3_trackgen.h:48-54`). HxC füllt RAND mit `rand()&0x54` je Lesung (`hfev3_loader.c:416-421`) | `hfev3_loader.c:103,204` + `hfev3_trackgen.h:48-54` | FlashFloppy liest/schreibt HFEv3 (Wiki „Image Formats", Release 3.42 „HFEv3 read/write improvements"; abgerufen 2026-08-29); Greaseweazle erzeugt v3 auf Anfrage (Discussion #468, abgerufen 2026-08-29) | **[MESSBAR]** (Opcode-Semantik durch hxcfe-Quelle; Verbreitung durch zwei unabhängige Abnehmer) |
| 4 | **HDDD-A2-HFE** | **nicht am Header unterscheidbar**: `"HXCPICFE"`@0, Byte 8 = 0x00 — wie v1. Indizien: bitRate ≈ doppelt (gemessen 0x01FA = 506 statt 253), Datei ≈ 2× | Für Apple-II-HDDD-Mod; Bitstrom mit `LUT_Byte2ShortEvenBitsExpander` auf doppelte Rate gespreizt (factor=2-Pfad in `hfe_hddd_a2_writer.c`), Encodings `APPLEII_HDDD_A2_GCR1/2` = 0x08/0x09 vorgesehen (`libhxcfe.h:434-435`). Write-only beim Urheber (`-modulelist`: `HXC_HDDD_A2_HFE; W`) | `hfe_hddd_a2_writer.c:83,106` + Messlauf (Kopf: rev 0, bitRate 506, 4 258 816 B) | keine zweite Implementierung bekannt | **[ZU VERIFIZIEREN]** (nur eine Quelle; kursiert vermutlich kaum) |
| 5 | **Stream-HFE** („HxC v4") | `"HxC_Stream_Image"` (16 Bytes)@0 | Völlig andere Struktur: 32-bit-Felder, Flux-Pulse (`bits_period` in ps), gepackte Tracks (`streamhfe_format.h:41-76`); trägt trotzdem die Endung `.hfe` (`streamhfe_loader.c:528`) | `streamhfe_loader.c:98` + `streamhfe_format.h:43` | keine zweite Implementierung im Klonbestand | **[ZU VERIFIZIEREN]** (Existenz sicher — Fixture erzeugt —, Verbreitung außerhalb HxC offen) |

**Track-Encodings (Byte 11) und Interface-Modes (Byte 16), zweifach
belegt** — HxC `libhxcfe.h:407-448` und SamDisk `hfe.cpp:35-61` stimmen
überein:

* Encodings: `0x00 ISOIBM_MFM · 0x01 AMIGA_MFM · 0x02 ISOIBM_FM ·
  0x03 EMU_FM · 0xFF UNKNOWN`. SamDisk kennt nur diese; libhxcfe führt
  intern weitere (0x04-0x15: Apple GCR, C64 GCR, Northstar …,
  `libhxcfe.h:426-448`) — ob die je in v1-Dateien im Feld stehen:
  [ZU VERIFIZIEREN] (nur eine Quelle).
* Interface-Modes: `0x00 IBMPC_DD · 0x01 IBMPC_HD · 0x02 ATARIST_DD ·
  0x03 ATARIST_HD · 0x04 AMIGA_DD · 0x05 AMIGA_HD · 0x06 CPC_DD ·
  0x07 GENERIC_SHUGART_DD · 0x08 IBMPC_ED · 0x09 MSX2_DD ·
  0x0A C64_DD · 0x0B EMU_SHUGART · 0x0C S950_DD · 0x0D S950_HD ·
  0xFE DISABLE` (beide Quellen wortgleich). 0xFF ist im Feld üblich
  für „nicht angegeben" (Greaseweazle-Fixture `gw_amigados.hfe`:
  Byte 11 = 0xFF **und** Byte 16 = 0xFF, gemessen).

---

## 2 · Befunde am eigenen Baum (Fallen, nach Stufe)

### F1 · Interface-Mode-Tabelle im registrierten Plugin ab 0x08 um eins verschoben — Stufe 2

`src/formats/hfe/uft_hfe.c:63-67` führt `0x08 MSX2 · 0x09 C64 ·
0x0A EMU_SHUGART · 0x0B S950_DD · 0x0C S950_HD` — gegen beide
unabhängigen Quellen ist das ab 0x08 **falsch**: `0x08 IBMPC_ED` fehlt,
alles danach rückt auf. Wirkung: `read_metadata("interface_mode")`
(`uft_hfe.c:939-947`) nennt eine echte MSX2-Datei (0x09) „C64" und eine
echte C64-Datei (0x0A) nicht. Keine Datenverfälschung (der Decode hängt
nicht am Interface-Byte), aber eine stille Falschauskunft in der
Metadaten-Tür, die der Varianten-Plan (`VARIANTEN_UND_FAEHIGKEITEN.md`)
gerade verdrahten will.

**Der eigene Baum widerspricht sich dabei selbst** (Regel-2-Befund):
`include/uft/uft_hfe_format.h:53-67` und
`src/formats/hfe/uft_hfe_parser_v2.c:47-60` haben die **korrekte**
Tabelle (inkl. `0x08 IBMPC_ED`). Der Konverter-Schreibpfad
(`uft_format_convert_bitstream.c` via `uft_format_convert_internal.h`)
benutzt die korrekte; nur das registrierte Lese-Plugin die falsche.
`uft_hfe_parser_v2.c` steht in der `.pro` (Zeile 1482), hat aber keinen
gefundenen Aufrufer — Bestand, nicht Fähigkeit.

Kennzahl: **T3 runter** (HFE-Metadatenpfad wird prüfbar).

### F2 · `format_revision` wird beim Lesen nirgends angefasst — Stufe 3 (grenzwertig 1)

`grep format_revision uft_hfe.c`: nur Struct-Feld (:79) und Schreiben
von 0 (:543). Ein ExtHFE (rev=1) wird **still als rev0 gelesen**, wo
Urheber und SamDisk laut ablehnen. Mildernd: das Layout ist identisch
(gemessen per Diff, §1 Zeile 2), nur die Spurende-Füllung weicht ab —
der gelieferte Bitstrom ist also im Wesentlichen korrekt, darum
Stufe 3, nicht 1. Aber UFT sagt dem Nutzer nicht einmal, dass die Datei
rev1 trägt (`read_metadata` kennt keinen `revision`-Schlüssel).
Prüffrage, kein Vorab-Urteil: annehmen-und-kennzeichnen ist vertretbar
(liberaler als der Urheber), stilles Verschlucken nicht.

Kennzahl: **T3 runter**.

### F3 · Feature-Matrix behauptet „HFE v2 (HxC2001) SUPPORTED" — Prinzip-7-Falschaussage

`uft_hfe.c:970`. Es gibt im Plugin keinen v2-Begriff, keinen rev-Check,
keine Definition, was „v2" sein soll. Nach der Quellenlage (§0) gibt es
„v2" als kursierendes Format gar nicht — es gibt rev1 = ExtHFE, das
niemand liest. Der Eintrag ist Bestand ohne Deckung, dieselbe Klasse
wie die MF-508/627-Befunde.

Kennzahl: **T3 runter** (Ehrlichkeit der Feature-Matrix ist Teil der
Tier-Hebung).

### F4 · Feature-Matrix behauptet „Per-track bitrate SUPPORTED" — nicht belegt

`uft_hfe.c:972`. Der v1-Header hat EINEN globalen `bitRate`; die
v3-`SETBITRATE`-Opcodes werden beim Decode **verworfen**
(`uft_hfe.c:239`: `case HFEV3_SETBITRATE: l += 2; break;` — nichts wird
gespeichert; der Ablehnungs-Kommentar am Write-Pfad `:791-796` sagt es
selbst: „that decode drops NOP/SETINDEX/SETBITRATE"). Es gibt keinen
Pfad, der eine Bitrate pro Spur liefert.

Kennzahl: **T3 runter**.

### F5 · Der Amiga-Encoding-Zweig lief noch nie mit einer echten Datei

Bestätigt (MF-650): das einzige Korpus-Fixture `gw_amigados.hfe` trägt
`track_encoding = 0xFF` und `interface_mode = 0xFF` (gemessen, §1) —
der Zweig `case HFE_ENC_AMIGA_MFM:` (`uft_hfe.c:137`) und die gesamte
Encoding-/Interface-Beschriftung wurden nie von einer Datei mit echten
Werten berührt. Behoben durch die neuen Fixtures (§4): das native
HxC-v1 trägt 0x01/0x04.

Kennzahl: **T3 runter** (genau die fehlende Fixture-Achse).

### F6 · v3-Decode-Semantik: verifiziert korrekt, eine dokumentierte Absichts-Abweichung

Der Auftraggeber-Befund hält der Prüfung stand: `hfe_v3_decode`
(`uft_hfe.c:217-268`) folgt der HxC-Schleife exakt — SKIPBITS `&0x7`
aufs nächste Datenbyte, RAND emittiert `8-skip` Bits und setzt skip
zurück, SETBITRATE konsumiert 2 Bytes, Datenbyte emittiert Bits
`[skip..7]` und setzt skip zurück (Abgleich gegen
`hfev3_loader.c:363-449`). Absichtliche Abweichung, im Code erklärt
(`uft_hfe.c:203-207`): HxC füllt RAND-Bits mit `rand()&0x54` je Lesung;
UFT setzt deterministisch 0 + `weak_mask` — forensisch die ehrlichere
Wahl, kein Befund. v3-Write wird laut abgelehnt
(`uft_hfe.c:797`, `UFT_ERROR_NOT_SUPPORTED`) — korrekt, da der Decode
verlustbehaftet ist.

### F7 · Stream-HFE und HDDD-A2 an der Tür

* Stream-HFE: Signatur passt nicht auf `HXCPICFE`/`HXCHFEV3` →
  `hfe_probe` (`uft_hfe.c:354-368`) gibt false, `hfe_open` gibt
  `UFT_ERROR_FORMAT_INVALID` (`:419`) — **laute Ablehnung, Stufe 3**,
  gewollt. Tür-Messung als Rotbeweis absichern (R5).
* HDDD-A2: nicht unterscheidbar von v1 (§1 Zeile 4) → UFT liest es als
  v1. Die Bits sind bei doppelter Rate „gültig", aber jeder nachgelagerte
  Decoder, der von 250 kbit/s ausgeht, holt Müll — mildernd steht die
  echte Rate ehrlich im bitRate-Feld (gemessen 506). **Stufe 2**,
  praktisch selten (Apple-II-HDDD-Nische, Write-only-Format).

---

## 3 · Korpus-Abdeckung je Variante

| Variante | vorhanden (`tests/corpus_free`) | neu erzeugt (`tools/uft-variants/work/fixtures/`, gitignored — Übernahme = menschliches Tor) |
|---|---|---|
| v1 rev0, Felder 0xFF | `gw_amigados.hfe` (Greaseweazle-typisch) | — |
| v1 rev0, echte Felder (0x01/0x04) | **fehlte** | `amiga_dd_HXC_HFE.hfe` · sha256 `557d2ca4…` |
| v3 | **fehlte — der ganze v3-Pfad hing an synthetischen Opcode-Strömen** (`tests/test_hfe_v3_weak.c`) | `amiga_dd_HXC_HFEV3.hfe` · sha256 `eb65fdef…` |
| ExtHFE rev1 | fehlte | `amiga_dd_HXC_EXTHFE.hfe` · sha256 `f51738d9…` |
| HDDD-A2 | fehlte | `amiga_dd_HXC_HDDD_A2_HFE.hfe` · sha256 `5a26c58e…` |
| Stream-HFE | fehlte | `amiga_dd_HXC_STREAMHFE.hfe` · sha256 `c894de65…` |

**Herkunft/Lizenz (Fixture-Lizenz wie Code-Lizenz):** alle fünf neuen
Fixtures wurden aus dem bereits freien, selbst erzeugten Korpus-Fixture
`tests/corpus_free/xdftool_dd_ofs.adf` mit dem in MF-650 gebauten
`hxcfe.exe` konvertiert (Werkzeug verifiziert vorhanden, sha256 beginnt
`a3ed7a41`, Klon-HEAD `05b53aa9`). Werkzeugkette vollständig benennbar
→ GRÜN. Die Kommandozeile (reproduzierbar, aus der Baumwurzel):

    tools/uft-scout/work/HxCFloppyEmulator/build/hxcfe.exe \
      -finput:tests/corpus_free/xdftool_dd_ofs.adf \
      -conv:HXC_HFE    -foutput:<ziel>.hfe        # natives v1 (enc 0x01, if 0x04)
      -conv:HXC_HFEV3  -foutput:<ziel>.hfe        # echtes v3
      # analog: HXC_EXTHFE · HXC_HDDD_A2_HFE · HXC_STREAMHFE

Gemessene Kopfbytes (xxd, erste 20):

    HXC_HFE   : 4858 4350 4943 4645 00 54 02 01 fd00 0000 04 01 0100  (rev0, 84 Spuren, 2 Seiten, enc AMIGA_MFM, 253 kbit/s, if AMIGA_DD)
    HXC_HFEV3 : 4858 4348 4645 5633 00 54 02 01 fd00 0000 04 01 0100  (Signatur HXCHFEV3, rev0!)
    HXC_EXTHFE: 4858 4350 4943 4645 01 54 02 01 fd00 0000 04 01 0100  (rev=1, sonst wie v1)
    HDDD_A2   : 4858 4350 4943 4645 00 54 02 01 fa01 0000 04 01 0100  (rev0, bitRate 506!)
    STREAMHFE : 4878 435f 5374 7265 616d 5f49 6d61 6765               ("HxC_Stream_Image")

---

## 4 · UFT-Prüffragen (Tür-Messungen, MF-629-Muster — an den MF-Workflow)

1. Öffnet `uft_disk_open()` das native v1-Fixture, und liefert
   `read_metadata`: `version=HFEv1`, `interface_mode=Amiga DD`,
   `track->encoding == UFT_ENC_AMIGA_MFM`? (Erster echter Lauf von
   `uft_hfe.c:137`.)
2. Öffnet es das v3-Fixture, und ist der dekodierte Bitstrom gegen die
   ADF-Quelle (`xdftool_dd_ofs.adf`) inhaltlich richtig? (Erster Lauf
   des v3-Pfads mit einer HxC-erzeugten Datei statt Synthetik.)
3. Was tut der Pfad mit ExtHFE rev=1 heute — und was soll er tun
   (kennzeichnen vs. ablehnen)? Entscheid dokumentieren.
4. Lehnt die Tür Stream-HFE laut ab (kein stilles Anfassen als v1)?
5. F1: nach Korrektur der Tabelle muss ein synthetisches v1 mit
   Interface 0x09 als „MSX2 DD" gemeldet werden (heute erwartet rot).
6. F3/F4: Feature-Matrix-Einträge „HFE v2" und „Per-track bitrate"
   belegen oder auf ehrliche Aussagen zurückschneiden.

## 5 · Rotbeweis-Skizzen (Muster `tests/test_convert_atr_xfd.c`, MF-655)

* **R1 (v1 nativ):** `amiga_dd_HXC_HFE.hfe` rein → open OK; ASSERT
  `version=="HFEv1"`, `interface_mode=="Amiga DD"`, Spur-Encoding
  AMIGA_MFM. Heute vermutlich grün bis auf nichts — Zweck ist der
  Regressionsschutz des nie gelaufenen Zweigs.
* **R2 (v3 nativ):** `amiga_dd_HXC_HFEV3.hfe` rein → open OK;
  `version=="HFEv3"`; `read_track` liefert nbits>0; Bitstrom enthält
  die AmigaDOS-Strukturen der ADF-Quelle. NIE stiller Müll: schlägt der
  Decode fehl, muss ein Fehlercode kommen.
* **R3 (ExtHFE):** `amiga_dd_HXC_EXTHFE.hfe` rein → erwartetes
  Verhalten nach Entscheid aus Prüffrage 3: entweder open OK **und**
  ein sichtbares `revision`-Metadatum, oder laute Ablehnung. Rot ist:
  open OK ohne jede Kennzeichnung (Ist-Zustand).
* **R4 (Interface-Tabelle):** synthetischer 512-B-Header, Byte 16 =
  0x09 → `interface_mode` MUSS „MSX2 DD" sein. Vor dem Fix rot
  („C64"), nach dem Fix grün — echter Rotbeweis-zuerst-Kandidat.
* **R5 (Stream-HFE):** `amiga_dd_HXC_STREAMHFE.hfe` rein →
  `uft_disk_open` erkennt es NICHT als HFE; kein Absturz, kein
  Teil-Read. (Auch ein guter Fuzz-Seed — 16-Byte-Signatur, deren erste
  3 Bytes `HxC` mit nichts kollidieren.)
* **R6 (HDDD-A2):** rein → open OK als v1 ist akzeptabel, ASSERT
  `bitrate=="506 kbit/s"` (die ehrliche Tür); dokumentiert damit die
  Nicht-Unterscheidbarkeit statt sie zu verstecken.

## 6 · WRITE-Zielversion mit Abnehmer-Begründung

**Standard beim Speichern: HFE v1, rev0, mit aus dem Inhalt
abgeleiteten `track_encoding`/`interface_mode`** (wie SamDisk es tut,
`hfe.cpp:265-271`) — nicht hart `0x00/0x00` wie heute in
`hfe_create` (`uft_hfe.c:546,549`; für den einzig angebotenen Pfad
IMG→HFE zufällig richtig, weil IBM-PC-Inhalt).

Begründung über die Abnehmer: v1 rev0 lesen **alle** bekannten
Konsumenten — HxC-Firmware/Software (Urheber), FlashFloppy/Gotek,
Greaseweazle, SamDisk, FluxEngine, MAME. v3 lesen nur HxC, FlashFloppy
(seit 3.42 stabiler) und Greaseweazle; SamDisk erkennt die
v3-Signatur gar nicht (`hfe.cpp:8` prüft nur `HXCPICFE`). rev=1 lehnen
Urheber und SamDisk laut ab. Wer maximale Kompatibilität schreiben
will, schreibt v1 rev0. (Quellen: FlashFloppy-Wiki „Image Formats",
Greaseweazle Discussion #468 / Wiki „Supported Image Types", beide
abgerufen 2026-08-29.)

**Nie anbieten:**

* **v3 schreiben** — UFT hat keinen v3-Encoder; die Ablehnung
  (`uft_hfe.c:797`) ist korrekt und bleibt.
* **ExtHFE / HDDD-A2 / Stream-HFE schreiben** — Nischen-Zielgeräte,
  keine Leser im Feld außer HxC selbst; ein Schreibfehler hier wandert
  in fremde Sammlungen, ohne dass ihn je ein zweites Werkzeug prüft.

## 7 · Zuarbeit für `docs/plans/VARIANTEN_UND_FAEHIGKEITEN.md`

**Unterscheidbarkeit:** v1↔v3↔Stream sind an der Signatur **eindeutig**
(8 bzw. 16 Bytes, disjunkt). v1↔ExtHFE eindeutig an Byte 8
(0x00/0x01). v1↔HDDD-A2 **nicht unterscheidbar** (nur Indiz
bitRate≈500 + Apple-Kontext) — hier wäre jede sichere Behauptung
erfunden.

Vorschlag `uft_format_variant_t`-Instanzen (Tabelle, kein Code; Felder
ohne Beleg bleiben offen):

| Feld | „HFE-v1" | „HFE-v1-ext" | „HFE-v3" |
|---|---|---|---|
| name | `"HFE-v1"` | `"HFE-v1-ext"` | `"HFE-v3"` |
| description | `"HXCPICFE rev0"` | `"HXCPICFE rev1 (ExtHFE, write-only beim Urheber)"` | `"HXCHFEV3 opcode stream"` |
| base_format | `UFT_FORMAT_HFE` | `UFT_FORMAT_HFE` | `UFT_FORMAT_HFE` |
| min_size | 1024 (Header+LUT-Block) | 1024 | 1024 |
| max_size / exact_sizes | offen (variabel, Spurdaten-abhängig) | offen | offen |
| cylinders/heads | offen — stehen im Header (Byte 9/10), nicht in der Variante | offen | offen |
| sectors_min/max, sector_size | nicht anwendbar (Bitstream-Format) — 0 lassen | dito | dito |
| validate | memcmp `"HXCPICFE"` && Byte 8==0 | memcmp `"HXCPICFE"` && Byte 8==1 | memcmp `"HXCHFEV3"` |

Stream-HFE bekommt **keine** Variante: es ist strukturell ein anderes
Format, und eine neue Format-ID wäre Moratorium → **Fundus** („wartet
auf die 1:2-Bedingung"). HDDD-A2 bekommt keine Variante, weil es kein
unterscheidendes Byte gibt — eine Variante ohne Erkennung wäre eine
erfundene Behauptung.

## 8 · Kennzahl je Vorschlag (Regel 9)

| Vorschlag | Kennzahl |
|---|---|
| F1-Fix + R4 (Interface-Tabelle) | T3 runter |
| R1/R2 (native v1/v3-Fixtures in den Korpus) | T3 runter — hebt HFE Richtung T1b („Fixture je kursierender Version") |
| F2/R3 (rev1-Entscheid) | T3 runter |
| F3/F4 (Feature-Matrix ehrlich) | T3 runter |
| R5/R6 (Stream/HDDD-Türmessung) | T3 runter |
| WRITE-Ziel §6 (Encoding/Interface aus Inhalt ableiten) | angebotene Wandlungspfade rauf — Voraussetzung, um weitere *→HFE-Pfade je mit Messung anzubieten |
| Stream-HFE als eigenes Format | **Fundus** (Moratorium, 1:2) |
| HDDD-A2-Spezialbehandlung | **Fundus** (keine Erkennung möglich, Nische) |

## 9 · UNGEKLÄRT

* Nennt irgendeine unabhängige Quelle rev=1 „HFE v2"? (Nur HxCs
  `formatrevision+1`-Fehlermeldung legt es nahe.) — für F3-Formulierung
  relevant, nicht blockierend.
* Kommen libhxcfe-Encodings > 0x03 (Apple GCR 0x06/0x07, C64 GCR 0x12
  …) in kursierenden v1-Dateien vor, oder nur intern? Eine Quelle.
* Verbreitung von Stream-HFE außerhalb der HxC-Werkzeuge.
* `uft_hfe_parser_v2.c`: kompiliert (`UnifiedFloppyTool.pro:1482`),
  kein Aufrufer gefunden — Kandidat für den Bestand/Fähigkeit-Zensus,
  nicht Teil dieses Auftrags.
* Die offizielle Spec-PDF
  (`hxc2001.com/download/floppy_drive_emulator/SDCard_HxC_Floppy_Emulator_HFE_file_format.pdf`,
  referenziert in `src/samdisk/hfe.cpp:2`) ist NICHT nach
  `docs/specs/hfe/` gesichert (Regel 7) — nachholen bei Übernahme.

## 10 · Fehlende Quellen (Regel 1/11)

`fluxengine` und `mame` (beide in `config.json` geführt) liegen nicht
als Klone vor; `korpus_zensus`-Lauf vom Vorlauf unverändert übernommen
(`work/korpus.json`, 36 Dateien). Die Websuche ersetzt keine
Quell-Lektüre: FlashFloppy- und Greaseweazle-Aussagen sind
Abnehmer-Belege mit Abrufdatum, ihre Quelltexte wurden nicht gelesen.
