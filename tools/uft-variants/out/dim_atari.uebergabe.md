# Übergabe: Format-Varianten `dim_atari` (Atari ST DIM)

Stand 2026-08-29 · Zyklus-Anlass: **FMT-13** (`docs/OPEN_ITEMS.md:3688`, MF-684)
· Evidenz: `work/dim_atari.evidenz.json` · Korpus: `work/korpus.json`
· Kennzahl des Zyklus: **ungeprüfte Formate (T3) runter** — `dim_atari`
steht auf T3 (`docs/VERIFICATION_TIERS.md:65`), ohne Fixture, ohne Test.

---

## 0 · Antwort auf die FMT-13-Kernfrage

**`BB` (0x4242) an Offset 0x00–0x01 ist das Magic des Formats, nicht
einer Variante.** FMT-13 kann geschlossen werden: unser Header-Kommentar
(`src/formats/dim_atari/uft_dim_atari.c:10-11`, „Flags (unused, often
0)" / „Reserved") ist von **keiner** gefundenen Quelle gedeckt; die
Magic-Prüfung gehört in die Probe.

Beleg sind **drei unabhängige Implementierungen**, die auf dasselbe Feld
verzweigen und ohne `BB` ablehnen (AGENT.md Regel 2 — die Verzweigung
ist der stärkste Beleg):

| Leser | Fundstelle | Verhalten ohne `BB` |
|---|---|---|
| Hatari (Klon `51ed999`, 2026-08-26) | `src/floppies/dim.c:75-76` | „This is not a valid DIM image!", Abbruch |
| HxC libhxcfe (Klon `05b53aa`) | `loaders/dim_loader/dim_loader.c:74` (Probe) und `:110` (Load) | `HXCFE_BADFILE` / `HXCFE_FILECORRUPTED` |
| Jacknife (Klon `97ce2a4`) | `dllmain.c:590` | fällt in andere Format-Zweige durch |

Dazu die Ur-Spec (eine **einzige** Prosa-Abstammungslinie, zählt als
eine Quelle): Darren-Birks-Notiz, zitiert vom Steem-Autor „Russ",
kopiert in HxC (`dim_loader/dim-format.txt`), in Hataris
Dateikommentar (`dim.c:24-46`) und ins DrCoolZic-PDF
(`info-coach.fr/atari/documents/_mydoc/FD_Image_File_Format.pdf`,
S. 4, abgerufen 2026-08-29): „0x0000 Word ID Header (0x4242('BB'))".

**Warum eine Magic-Prüfung keine gültigen Dateien abweist:** Die einzige
kursierende kopflose `.dim`-Fassung ist E-Copy (§1, V5) — und die hat
**gar keinen** 32-Byte-Kopf. Unsere heutige Geometrie-Lesart (Bytes
0x06/0x08/0x0C) wäre für sie genauso falsch; heute lehnt der exakte
Größen-Check sie ohnehin praktisch immer ab. Es gibt in keiner Quelle
ein „allgemeines Atari-DIM ohne BB", dessen Ablehnung ein Verlust wäre.

**Tür-Messung, gelaufen (2026-08-29):** Produktions-Probe
(unverändertes `uft_dim_atari.c`, Wegwerf-Harness mit Link-Stubs für
nie aufgerufene Funktionen, MinGW 13.1.0):

```
atarist_dd_HXC_DIM.dim:  probe=ANGENOMMEN confidence=90   (Magic BB)
atarist_dd_NOMAGIC.dim:  probe=ANGENOMMEN confidence=90   (Magic genullt, sonst identisch)
```

Gegenmessung am fremden Werkzeug: `hxcfe.exe` lädt die erste und lehnt
die zweite ab („No loader support the file"). **Der Rotbeweis feuert
heute** — unsere Probe kann die beiden Dateien nicht unterscheiden.

---

## 1 · Variantentabelle

Erkennungsbasis alle: Endung `.dim` (nur Hinweis). Byte-Offsets im
32-Byte-Kopf. Geometriefelder sind **Byte + Null-Füllbyte** (alle
Quellen einig); die BPB-Kopie ab 0x0E ist **Big-Endian** (Jacknife
`BYTE_SWAP_WORD`, `dllmain.c:455-457`; HxC liest `sectorsizeh*256 +
sectorsizel`, `dim_loader.c:121`).

| # | Variante | Erkennung | Unterschied | Beleg ×2 | Status |
|---|---|---|---|---|---|
| V1 | **FastCopy Pro, alle Sektoren** (Regelfall) | `BB` @0x00, Byte 0x03 = 0 | 32-Byte-Kopf + rohe Sektoren wie `.ST` | Hatari `dim.c:75-88` liest; HxC `dim_loader.c:110-130` liest; Jacknife `dllmain.c:617-622` liest | **[MESSBAR]**, Fixture liegt |
| V2 | **FastCopy Pro, „used sectors only"** | `BB` @0x00, Byte 0x03 = 1 | nur FAT-belegte Sektoren gespeichert; FAT-Parsen zum Entpacken nötig; Dateigröße variabel | Jacknife `dllmain.c:604-616` + `expand_dim()` (`dllmain.c:439-462`) entpackt; Hatari `dim.c:76` verzweigt und lehnt **gezielt** ab (Kommentar `dim.c:27-30` beschreibt die Fassung) | **[MESSBAR]** (zwei Leser verzweigen auf 0x03), Fixture fehlt |
| V3 | **FastCopy, Teilbereich** (Start-Track > 0) | `BB` @0x00, Byte 0x0A > 0 | nur Spurbereich enthalten | Hatari `dim.c:76` lehnt gezielt ab; Jacknife `dllmain.c:594-597` lehnt gezielt ab („we don't support partial images") | **[MESSBAR]** als Feld; ob Feld-Dateien kursieren: unbelegt |
| V4 | **Konfigurations-Flag Byte 0x02** | `BB` @0x00, Byte 0x02 ∈ {0,1} | Semantik strittig: Hatari/DrCoolZic-Prosa „1 = automatisch erkannt, 0 = vom Benutzer angegeben"; Jacknife `jacknife.h:94` „0 = keine Konfiguration vorhanden" → ignoriert dann die Geometriefelder und liest den Bootsektor (`dllmain.c:624-628`) | nur **eine** Implementierung verzweigt (Jacknife); Prosa ist eine Linie | **[ZU VERIFIZIEREN]** |
| V5 | **E-Copy `.dim`** (kopflos) | kein Kopf; Bootsektor ab Byte 0; Jacknife rät über Boot-Jump-Bytes (`dllmain.c:630-632`) | „used sectors"-Packung **ohne** Header, Größe variabel, FAT-Entpacken nötig | Jacknife `dllmain.c:630-641` + `expand_dim(FALSE)`; sonst nur Zeugnisse: atari-forum t=6090 („Ecopy .Dim … totally different sizes"), MSA-Converter-Doku (liest DIM nur „non-compressed") | **[ZU VERIFIZIEREN]** — 1 Implementierung, keine zweite. Unbelegt ≠ existiert nicht |
| V6 | ~~GCopy `.dim`~~ | — | im Forum t=6090 zuerst behauptet, vom Autor selbst widerrufen („It's not Gcopy files.. it was Ecopy") | — | **verworfen** (keine Quelle) |
| X | **X68000-DIM** (Namensvetter, eigenes Format `dim`) | 256-Byte-Kopf; `"DIFC HEADER  "` @0xAB; Media-Byte @0x00 | völlig anderes Format | MAME `dim_dsk.cpp` (Signatur @0xab, `FIFID_SIGN`; Web-Abruf 2026-08-29, Klon fehlt); HxC `dim_x68k_loader.c:72` | **[MESSBAR]** — siehe §3 Abgrenzung |

Weitere Korpus-Realität aus den Quellen: **abgeschnittene** FastCopy-
DIMs kursieren (Jacknife `dllmain.c:433`: „Yes, there are .dim files
that are truncated at the end"). Unser exakter Größen-Check lehnt sie
laut ab (Stufe 3) — festhalten, nicht „reparieren", solange kein
Fixture vorliegt.

## 2 · Fallen nach PROBE / READ / WRITE (Stufe = Stille-Falschaussage-Maßstab)

**PROBE**
- **Kein Magic-Check** (`uft_dim_atari.c:98-148`): jede Datei richtiger
  Größe mit 5 plausiblen Bytes wird mit Konfidenz 80–90 angenommen —
  **gemessen** (§0). **Stufe 1**: der Leser liefert plausiblen Müll aus
  Nicht-DIM-Dateien. Genau die FMT-2/3/10/11/12-Lage.
- Header-Doku falsch: 0x00/0x01 sind das Magic, nicht „Flags/Reserved";
  0x0E–0x1F ist keine „Reserved (zero padded)", sondern die
  **BPB-Kopie** (Hatari `dim.c:46`; Jacknife `jacknife.h:101-108`).
  Latente Falle: wer „reserved must be zero" je erzwingt, weist echte
  FastCopy-Dateien mit gefüllter BPB ab. (HxC-**erzeugte** Dateien haben
  dort Nullen — `dim_writer.c:94-109` schreibt nur Geometrie; unser
  Fixture belegt das: Header `42 42 00 … 00`.)
- V2/V3 werden nur durch den Größen-Check abgewiesen — Zufallstreffer
  einer passenden Größe nicht ausgeschlossen. Nach Magic-Fix: 0x03≠0
  und 0x0A≠0 **ausdrücklich** ablehnen (Hatari-Muster) statt implizit
  über die Größe. Stufe 3 → sauber Stufe 3.

**READ**
- `read_track` füllt Kurz-Lesungen mit 0xE5 auf (`uft_dim_atari.c:215-218`) — bei einer als V1 fehlgedeuteten V2-Datei würde das
  stillen Müll erzeugen; heute unerreichbar wegen Größen-Check, nach
  jedem künftigen Lockern der Größenprüfung sofort Stufe 1.

**WRITE**
- `write_track` öffnet `r+b` **ohne Magic-Prüfung** und schreibt an
  berechnete Offsets (`uft_dim_atari.c:150` `r+b` + `:225-250`). Zusammen mit der
  Probe-Lücke: UFT kann heute in eine Nicht-DIM-Datei passender Größe
  hineinschreiben. **Stufe 1 auf dem Schreibpfad** — Schreibfehler
  wandern in fremde Sammlungen (AGENT.md Maßstab, Schreiber vor Leser).
- Create existiert nicht (`Create UNSUPPORTED`) — es gibt keinen Pfad,
  der je einen Kopf erzeugt. Solange das so ist, ist die
  WRITE-Zielversion „bestehenden Kopf erhalten" (siehe §5).

## 3 · Abgrenzung X68000 (Auftragspunkt 3)

Unsere Sonderbehandlung (`uft_dim_atari.c:41-48`, `:104-131`) ist
**doppelt schief, aber im Ergebnis harmlos — und nach dem Magic-Fix
überflüssig**:

1. **Rechnerisch tot:** X68000-DIM-Dateien sind 256 + N·Sektorgröße
   (Sektorgrößen ≥256, Vielfache von 256) ⇒ Dateigröße ≡ 0 oder 256
   (mod 512). Atari-DIM verlangt 32 + N·512 ⇒ ≡ 32 (mod 512). Die
   Mengen schneiden sich nie; der Größen-Check allein trennt schon.
2. **Die Media-Tabelle darin ist von zwei Quellen widerlegt:** unsere
   Fälle 0x01/0x02/0x03 erwarten alle 77·2·8·1024. MAME sagt Media 2 =
   15 spt/512, Media 1 = 9 spt/1024, Media 17 (0x11) = 26 spt/256
   (Web-Abruf `mamedev/mame src/lib/formats/dim_dsk.cpp`, 2026-08-29);
   HxCs Kommentartabelle (`dim_x68k_format.h`) stimmt mit MAME gegen
   uns. Beide fremden X68000-Leser erkennen am **Signatur-String**
   `"DIFC HEADER  "` @0xAB, nicht an Media-Byte+Größe.
3. **Befund über den eigenen Baum (Regel 2, `uft_selbst`):** dieselbe
   falsche Media-Tabelle steht in `src/formats/dim/uft_dim.c:10-24`
   (X68000-Plugin, ebenfalls T3), dessen Probe weder DIFC prüft noch
   exakte Größe verlangt (`file_size < expected` ⇒ nur Untergrenze,
   `uft_dim.c:93-114`) — ein ≥1,2-MB-Müllblob mit Byte 0 ∈
   {0,1,2,3,9,0x11,0x19} wird als X68000 angenommen. Zusätzlich liegt
   ein **dritter** DIM-Leser im Baum: `src/formats/pc98/dim.c`
   (im `.pro` Zeile 1854, als „NEC PC-98" beschriftet, tatsächlich
   X68000, prüft DIFC @0xAB). **Das ist der Zyklus des Formats `dim`,
   nicht dieses — hier nur festgehalten, nicht bearbeitet.**

Empfehlung für `dim_atari`: nach Einbau der Magic-Prüfung die
X68000-Ausschluss-Sonderlocke ersatzlos streichen (BB schließt X68000
aus; deren Byte 0 wäre 0x42 = kein gültiger Media-Type in irgendeiner
Quelle) — als Teil desselben Rotbeweis-Commits, nicht separat.

## 4 · Korpus-Abdeckung und Beschaffung

Zensus 2026-08-29 über `tests/corpus_free`, `tests/corpus`,
`tests/differential/corpus`: **0 DIM-Dateien** (`work/korpus.json`).

| Variante | Abdeckung | Beschaffung | Lizenz |
|---|---|---|---|
| V1 | **Fixture erzeugt**: `work/fixtures/atarist_dd_HXC_DIM.dim` (737 312 B, sha256 `73f1de73…79eb8`) | selbst erzeugt: `hxcfe.exe -finput:tests/differential/corpus/sources/atarist_dd.st -conv:ATARIST_DIM` (Klon `05b53aa`); Quelle `atarist_dd.st` ist eigener Differential-Korpus (sha256-identisch mit `ibm_dd.img` dort ⇒ selbst gebaut) | eigene Daten, Werkzeugkette benannt — **GRÜN**, Übernahme in `tests/corpus_free` = menschliches Tor |
| V1-Gegenstück | `work/fixtures/atarist_dd_NOMAGIC.dim` (identisch, Magic genullt, sha256 `c47c3f8e…97a5`) | selbst erzeugt (Python, dokumentiert oben) | **GRÜN** |
| V2 | fehlt | selbst erzeugbar: FastCopy Pro unter Hatari/EmuTOS, „get sectors: used" auf selbst formatierter Diskette; Aufwand hoch. Downloads realer Spiele-DIMs: **ROT** (fremdes Urheberrecht, MF-650-Linie) | offen |
| V3 | fehlt | wie V2 (FastCopy-Spurbereich-Option); synthetisch konstruierbar erst, wenn V2 die Packungsfrage klärt | offen |
| V5 (E-Copy) | fehlt | E-Copy unter Emulator auf eigener Diskette; vorher zweite Quelle nötig (sonst bliebe auch das Fixture einquellig) | offen |
| Hinweis | ein `BB`-Fixture **mit gefüllter BPB** (0x0E–0x1F ≠ 0) fehlt ebenfalls — HxC erzeugt nur genullte; ein echtes FastCopy-Pro-Erzeugnis wäre der Beleg für die BPB-Doku | wie V2 | offen |

## 5 · Prüfauftrag an den MF-Workflow (Rotbeweis-Skizzen)

**R1 — Magic (entscheidet FMT-13; Kennzahl: T3 runter):**
Fixture-Paar aus §4 in den getrackten Korpus übernehmen (menschliches
Tor), dann Test: `uft_disk_open()` (der echte Registry-Pfad, nicht die
Probe allein) auf beide Dateien.
*Erwartung vor dem Fix (Rotbeweis, hier bereits auf Probe-Ebene
gemessen):* beide werden angenommen → Test ist ROT.
*Erwartung nach dem Fix:* `…HXC_DIM.dim` öffnet, Track 0/Head 0/Sektor 0
byteidentisch zu `atarist_dd.st` Offset 0; `…NOMAGIC.dim` wird von
`dim_atari` abgelehnt (und von keinem anderen Plugin still gefressen —
Registry-Gesamtergebnis prüfen!). Referenz in den Header: Hatari
`dim.c:75`, HxC `dim_loader.c:110`, Jacknife `dllmain.c:590`.
Gleicher Commit: Header-Kommentar 0x00/0x01/0x02/0x03/0x0E berichtigen
(Quellenlage §1), X68000-Sonderlocke raus (§3).

**R2 — V2/V3 ausdrücklich ablehnen (Kennzahl: T3 runter, Ehrlichkeit):**
Synthetisches Minimal-Fixture: V1-Fixture kopieren, Byte 0x03 := 1
(bzw. 0x0A := 5) — als Synthetik markiert; es prüft NUR den Ablehnpfad,
nicht das Entpacken. *Erwartung:* klare Fehlermeldung („used-sectors
DIM wird nicht unterstützt"), nie stiller Müll. Ein Lese-Support für
V2 wäre neuer Format-Code und braucht erst ein echtes Fixture (§4) —
nicht Teil dieses Auftrags.

**R3 — Schreibpfad-Tor (Kennzahl: T3 runter; Schreiber vor Leser):**
Nach R1 sicherstellen, dass `open(…, rw)` + `write_track` auf
`…NOMAGIC.dim` unmöglich ist (derselbe Magic-Check sitzt in `open`).
Messung: Öffnen-Rückgabewert + Datei vor/nach byteidentisch.

**WRITE-Zielversion (Abnehmer-Begründung):** V1, alle Sektoren,
`BB`-Kopf des Originals **unverändert erhalten** (kein Create-Pfad
vorhanden; Abnehmer Hatari/Steem/HxC/Jacknife erwarten `BB` und lesen
V1 universell; „used sectors" schreibt außer FastCopy selbst niemand).
Sollte je ein Create kommen: Hatari-Muster `dim.c:154-160` (BB, 0x03=0,
0x0A=0), Felder 0x02/BPB dann mit geklärter Semantik.

## 6 · UNGEKLÄRT

1. **V4-Semantik (Byte 0x02):** Jacknife gegen Hatari/DrCoolZic-Prosa;
   entscheidbar nur mit echtem FastCopy-Pro-Erzeugnis (beide Semantiken
   am selben Byte beobachten). Bis dahin: Leser darf 0x02 nicht
   auswerten müssen — unser Fixture (0x02=0, Geometrie gültig) wird von
   allen drei Fremdlesern gelesen.
2. **V5 (E-Copy) einquellig.** Zweite Implementierung nicht gefunden
   (Steem-SSE-Quelle nicht beschafft — SourceForge-SVN, im Zeitrahmen
   nicht geklont). Urteil „unbelegt", nicht „existiert nicht".
3. **Spec-Sicherung vs. Verzeichniskonvention:** AGENT.md Regel 7 will
   Kopien fragiler Specs nach `docs/specs/<format>/`; das dortige
   README reserviert das Verzeichnis aber für Clean-Room-Specs des
   Quarantäne-Verfahrens (Weg 2) und ist „absichtlich leer". Nicht
   ausgelegt, sondern eskaliert: Eigentümer-Entscheid nötig. Die
   Birks-Notiz ist einstweilen oben in §0 vollständig referenziert und
   liegt als `dim-format.txt` im (gitignorten) HxC-Klon.
4. **Jacknife hat keine Lizenzdatei** (nur DOSFS „appears to be Public
   Domain"). Für diesen Zyklus folgenlos — gelesen, nichts übernommen —
   aber als Scout-Notiz wert, falls je Code-Nähe entsteht.
5. **Fundus (bewegt keine Kennzahl dieses Zyklus):** X68000-Befunde aus
   §3 (falsche Media-Tabelle in `dim/uft_dim.c`, schwache Probe, dritter
   Leser `pc98/dim.c`) gehören in einen eigenen `dim`-Zyklus; sie heben
   kein Atari-Format und fügen nichts zur Formatliste hinzu.

## 7 · Genannte fehlende und beschnittene Quellen (Regel 11)

- Klone **fehlten**: `fluxengine`, `mame`. MAME per Web-Abruf ersetzt
  (raw.githubusercontent.com `dim_dsk.cpp`, 2026-08-29) — nur für die
  X68000-Abgrenzung genutzt. FluxEngine: kein Ersatz beschafft; nach
  Doku-Lage kein Atari-DIM-Support, **ungeprüft**.
- **Abgeschnitten** vom Sucher: `samdisk_in_uft` (17→12 Dateien),
  `hxcfe` (26→12). Von Hand nachgezogen: gezielter grep über ganz
  `src/samdisk/` und den libdsk-Klon fand **kein** Atari-DIM/FastCopy —
  die Abschneidung hat nichts verdeckt.
- Mechanische Treffer aus `dfsimage`/`discimagemanager` in der Evidenz
  sind Teilwort-Rauschen („dim" ⊂ „dfsimage") und wurden verworfen; die
  Evidenz-Datei führt sie unverändert.
- **Hatari-Klon neu angelegt** (`tools/uft-scout/work/hatari`, shallow,
  HEAD `51ed999`) und in `config.json` nachgetragen.
