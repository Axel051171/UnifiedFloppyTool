# Gutachten: SuperDuper 3 (Quellen) · AllowBad 0.7 · badformat 0.2

Scout-Zyklus 2026-09-01 · Bearbeiter: uft-scout · Inventar: HEAD `49fb7bf6`
(`tools/uft-scout/work/inv.json`, 88 Plugins aus SSOT, 24 Korpus-Abbilder)

**Abweichung vom Standardpfad, benannt:** Die drei Vorlagen sind lokale
Archive vom Eigentümer, keine klonbaren Repos — `scout.py`/`vermessen.py`
(Repo-getrieben) liefen deshalb nicht; Vermessung von Hand, jede Zahl mit
Fundstelle. Ausgabeort ist auftragsgemäß `docs/scout/`, nicht
`tools/uft-scout/out/`. Arbeitskopien liegen außerhalb des Baums unter
`C:\Users\Axel\Github\xcopy\_scout\{superduper,allowbad,badformat}`.

**TL;DR:** Alle drei Pakete sind echt und einschlägig. Lizenzlage: keines
trägt eine Lizenz aus der Matrix — **alle drei Zone PRÜFEN bis ROT**, kein
Port möglich, Urteil ausdrücklich offen gelassen (MF-679). AllowBad und
badformat enthalten **keinen Quellcode**, nur Binary + Doku. Der eine
Fund, der eine Release-Kennzahl bewegt: SuperDuper als **benannte
Verhaltensreferenz und Emulations-Oracle für den fehlenden
AmigaDOS-MFM-Encoder** (MF-539-Lücke, Kennzahl „angebotene
Wandlungspfade rauf"). Vier weitere Funde sind Fundus.

---

## 1. Herkunfts- und Lizenzbefund je Paket

### 1.1 SuperDuper 3.0/3.13 — Quellarchiv

| | |
|---|---|
| Archiv | `C:\Users\Axel\Github\xcopy\SuperDuper-3.0_src.zip`, 600 603 B |
| SHA-256 | `62ccf7fe443b6d80fbdab03b31eaf7ec5954a411dcf468c77e92946144321d64` |
| Autor | Sebastiano Vigna, 1991–1994 |
| Inhalt | C-Quellen (SD.c 48 549 B, MFM.c 25 501 B, Disk.c 16 576 B, …), 68000-Assembler (TrackDecode.a, DMA.a, Interrupt.a), Objektdateien, Amiga-Binaries `SD` (59 080 B) und `SD.68020`, Release-Archiv `SD3V13.lha` (135 491 B, mit Binary + Doku), Texinfo-Doku |

Schlüsseldateien (SHA-256):

```
cafdbc60…c3e56f2  SD.c            e9ebed8a…c24da7cc  Disk.c
da622db7…4351764f  MFM.c           4897d57b…21478a29  TrackDecode.a
63bfc805…6e628208  SD (Binary)     740269ef…7140350   SD3V13.lha
a4eb0abb…382a4b04  docs/SuperDuper.texinfo
```

**Lizenztext, wörtlich** (`docs/SuperDuper.texinfo:1651ff`, Kapitel
„Disclaimer and Author Info" — die *einzige* Rechte-Aussage im Paket;
keine LICENSE/COPYING-Datei, kein Lizenzkopf in irgendeiner Quelldatei,
geprüft per grep über alle `.c`/`.h`/`.a`):

> "SuperDuper is Copyright © 1991-1994 Sebastiano Vigna and it's freely
> distributable as long as all of its files are included in their
> original form without additions, deletions, or modifications of any
> kind, and only a nominal fee is charged for its distribution. This
> software is provided AS IS without warranty of any kind […]"

Netzrecherche (2026-09-01): Aminet führt das Paket als
`disk/misc/SuperDuper-3.0_src.lha`; das Aminet-Readme enthält **keine**
zusätzliche Lizenzaussage. Der Autor verlinkt die Quellen bis heute auf
`https://vigna.di.unimi.it/software.php` („sources V2", „sources V3"),
ebenfalls **ohne** Lizenzangabe.

**Befund (kein Urteil):** Weitergabe nur unverändert erlaubt; ein
**Bearbeitungsrecht wird nicht eingeräumt**. Das ist dieselbe Klasse wie
die XCopy-Lizenz (LIZ-4 B, `docs/OPEN_ITEMS.md:7148`, MF-744: „verbietet
Bearbeitung"). Die Kennung steht nicht in
`tools/uft-scout/playbook/lizenzmatrix.md` → **Zone PRÜFEN**, praktisch
mit ROT-Konsequenz für Port: Konzept-Nachbau und Oracle-Nutzung des
Binaries bleiben nach Matrix-ROT-Zeile offen. Besonderheit, dem
Eigentümer vorzulegen: der Autor ist erreichbar (GitHub `@vigna`,
Universität Mailand) — eine Nachlizenzierungs-Anfrage ist derselbe Weg,
über den die XCopy-Lizenz beschafft wurde (MF-747).

### 1.2 AllowBad 0.7β

| | |
|---|---|
| Archiv | `C:\Users\Axel\Downloads\allowbad.lha`, 16 921 B |
| SHA-256 | `20f9108b4d08226265b598440b8337838e2b7de6906f5d9dfb495236a168b989` |
| Autor | Mikolaj Calusinski, 1995–1996 |
| Inhalt | **nur Binary + Doku**: `Allowbad` (3 472 B, AmigaOS-Executable, SHA `1b37cb3e…0a76c0`), `AllowBad.doc` (13 148 B, SHA `a085266e…9d5cc2`), `AllowBad.guide`, `AllowBad.doc.pl`. **Kein Quellcode** — die Doku sagt: Quelle nur auf Anfrage per Post („receive the source, etc.） please contact me", `AllowBad.doc`, Abschnitt „Bugs") |

**Lizenztext, wörtlich** (`AllowBad.doc`, Abschnitt „Distribution"):

> "AllowBad may be distributed freely, providing the following criteria
> are met:
> - None of the files in the AllowBad distribution archive may be
>   modified or omitted.
> - No money is charged for it apart from media and small handling fee.
> […]
> - You may not reverse-engineer or modify the AllowBad executable on
>   disk or on memory except for compressing it."

Dazu einleitend: "AllowBad is freeware (see 'Distribution') and
copyright 1995-1996 by Mikolaj Calusinski. All rights reserved."

**Befund (kein Urteil):** Freeware mit ausdrücklichem Modifikations-
**und Reverse-Engineering-Verbot**. Nicht in der Matrix → **Zone
PRÜFEN**; verwertbar ist nach ROT-Zeile nur die **Doku als
Verhaltensquelle** und ggf. das Binary als Oracle. Die Doku ist dafür
ungewöhnlich ergiebig (siehe Fund C/D).

### 1.3 badformat 0.2

| | |
|---|---|
| Archiv | `C:\Users\Axel\Downloads\badformat.lha`, 8 290 B |
| SHA-256 | `ae1262b0bc0d3370418639dcb7a85afcdfb98336aa00660621908ae9879e8e73` |
| Autor | Timm S. Mueller (tmueller@neoscientists.org), 2017 |
| Inhalt | **nur Binary + README**: `badformat` (12 064 B, AmigaOS-Executable, SHA `cbb5070e…242613f`), `README` (1 651 B, SHA `6d605050…ad040a1a`). Kein Quellcode |

**Lizenzaussage: es gibt keine.** Das README enthält ausschließlich
„Use it at your own risk. This software is experimental […]" — das ist
ein Haftungsausschluss, keine Rechteeinräumung über die
Aminet-Verteilung hinaus. Auffälligkeit der Provenienz: der
Versionsstring im Binary lautet `$VER: Badformat 0.1 (22.08.2017)`,
das README sagt Version 0.2 (per Python-Strings-Extraktion gemessen).

**Befund (kein Urteil):** Nach Matrix wörtlich „keine Lizenzdatei = alle
Rechte vorbehalten" → **Zone ROT**. Verhaltens-Spec nur aus README,
Oracle-Nutzung des legal von Aminet bezogenen Binaries nach ROT-Zeile
möglich.

---

## 2. Funde

### Fund A — SuperDuper belegt einen vollständigen AmigaDOS-MFM-Schreibpfad (bewegt eine Kennzahl)

**Was die Vorlage kann:** SuperDuper liest, kodiert, schreibt und
verifiziert komplette Amiga-MFM-Spuren. Der Encoder-Kern liegt in
`TrackDecode.a` (Blitter-Assembler): `EncodeSector`, `EncodeLong`,
`MFMTrack`, `CheckSum` (`AsmProtos.h:10,23,25,26`); der C-Überbau in
`MFM.c` (z. B. `UpdateMFMRootBlock`, MFM.c:860ff, das einen Sektor
dekodiert, den Rootblock ändert, neu kodiert, die Prüfsumme neu setzt
und die Spur neu MFM-kodiert; Sync `0x4489`, MFM.c:26–28).

**Was UFT heute stattdessen tut (gemessen):** `ADF→HFE` wird
**abgelehnt**, weil dem Baum ein AmigaDOS-Encoder fehlt —
`src/formats/uft_format_convert_bitstream.c:753–784` (MF-539), Warntext
wörtlich: „ADF->HFE requires an AmigaDOS MFM encoder; this tree has only
an IBM System-34 encoder". `docs/OPEN_ITEMS.md:2397–2402` (MF-646)
führt die Encoder-Lücke bereits als Folgeziel und nennt libhxcfe und
`keirf/disk-utilities` als Kandidaten.

**Ehrliche Einschränkung:** Der Encoder-Kern ist 68000-Blitter-Assembler
— als *Code* unportierbar (Lizenz **und** Technik), als *Spec-Quelle*
zweitrangig gegenüber der offenen Amiga-Dokumentation. Der Wert liegt
woanders: SuperDuper ist eine **zeitgenössische, millionenfach benutzte
Referenz-Implementierung**, deren Binary unter Emulation als
**unabhängiges Oracle** dienen kann (Abschnitt 3), und deren Quelle die
Randfälle benennt, die eine Spec braucht (Gap-Behandlung, `FindOffset`/
`FindSync`-Toleranzen, Rootblock-Update in der kodierten Spur).

**Kennzahl:** angebotene Wandlungspfade **rauf** (ADF→HFE verlässt die
Ablehnung nur mit Encoder). **Kanal:** Oracle + Spec-Zweitquelle
(Lizenz PRÜFEN, kein Bearbeitungsrecht → kein Port; primäre Spec aus
offener Doku/libhxcfe gemäß MF-646). **Einhängepunkt, per git grep
auffindbar:** `MF-539` in `src/formats/uft_format_convert_bitstream.c`
und der MF-646-Abschnitt in `docs/OPEN_ITEMS.md`. **EINFRIER-REGEL:**
ein Encoder ist Format-/Decoder-Code — Rotbeweis zuerst; die benannte
Referenz und das Oracle liefert genau dieser Fund. Als Ground-Truth
liegt bereits `tests/corpus_free/gw_amigados.hfe`-Messung im Baum
(Sync-0x4489-Zählung in der MF-539-Kommentarbox, uft_format_convert_bitstream.c:757–763).
**Aufwand:** M (Oracle-Aufbau unter Emulation) + die eigentliche
Encoder-Arbeit ist Stufe 4.

### Fund B — SuperDupers Verify vergleicht auf Rohstrom-Ebene (Fundus)

**Verhalten, belegt:** Nach jedem Spur-Schreiben liest SuperDuper die
Spur zurück und vergleicht **den kodierten MFM-Strom** gegen den
Schreibpuffer (`VerifyTrack`, MFM.c:824–857, Vergleich per
`CompareTracks` auf den Rohdaten, nicht auf dekodierten Sektoren) —
inklusive des Kniffs, den rotationsbedingten Versatz über `FindOffset`/
`FindSync` zu normalisieren statt zu dekodieren. Zusätzlich wird vor
jedem Lesen der Pufferanfang absichtlich zerstört, damit ein leerer
DMA-Transfer nicht als Erfolg durchgeht (`TrashBuffer`, Disk.c:187–192:
„otherwise a completely empty read could pass unnoticed").

**Was UFT heute tut (gemessen):** Der Greaseweazle-Schreibpfad setzt
`.verify=false` an der einzigen Konstruktionsstelle
(`src/hal/uft_greaseweazle_full.c:1113`); einen Nachschreib-Vergleich
auf Rohstrom-Ebene gibt es dort nicht.

**Kennzahl:** keine der vier → **Fundus**. Prüfbar nur an echter
Hardware → nach MF-310 an die Gemeinschaft delegierbar. Die
`TrashBuffer`-Idee (leeren Transfer erkennbar machen) wäre auch ohne
Hardware in HAL-Tests nachstellbar, bewegt aber ebenfalls keine Zahl.

### Fund C — Zwei dokumentierte Defektmasken-Mechanismen für AmigaDOS (Fundus, mit Falschaussage-Risiko im Bestand)

**Verhalten, belegt:**

* **AllowBad** maskiert **ganze Spuren** über eine geschützte
  Pseudo-Datei `dummy.bad` („AllowBad masks corrupted areas using dummy
  file which pretends to occupy all the bad tracks", AllowBad.doc,
  „Way of Operation"; im Binary bestätigt: String `BUSYdummy.bad`).
  Vorbedingungen sind präzise dokumentiert: lesbar sein müssen genau die
  beiden Bootblöcke und der Rootblock; je Spur bis zu 4 Zugriffe
  (Format mit Muster → Verify → Nullen schreiben → Verify).
* **badformat** maskiert auf einer Ebene, die „neither
  defragmentation/reorganization nor regenerating a disk's invalid
  bitmap" überlebt (README) — das deutet auf **Bitmap-Belegung ohne
  Datei** hin. *Mechanismus nicht verifiziert* (nur Binary; siehe
  UNGEKLÄRT).

**Was UFT heute tut (gemessen):** Der AmigaDOS-Walker erkennt „BAM says
used but unreachable from root" und meldet es als **Orphan-Blöcke mit
der Semantik gelöschter Dateien** (`src/fs/uft_amigados_extended.c:291–292,
340–353, 607–629`: „orphan / deleted-file data still on disk =
forensically valuable"). Eine badformat-Diskette würde damit heute als
„Orphan-Blöcke" gemeldet — die forensisch richtige Lesart „absichtliche
Defektmaske" kennt der Baum nicht; eine AllowBad-Diskette listet
`dummy.bad` als gewöhnliche Datei. Ein Suchlauf nach `dummy.bad` über
`src/` und `include/` liefert null Treffer.

**Kennzahl:** keine der vier → **Fundus**, trotz des realen Risikos
einer irreführenden Diagnose. Falls der Eigentümer die fünfte Kennzahl
oder eine FS-Kennzahl einführt, ist dies der erste Kandidat. Kanal:
Spec (beide Doku-Texte sind frei lesbar; kein Code nötig). Anknüpfung
im Bestand: KI-7.4 (`docs/OPEN_ITEMS.md:306`, ADF-Schreibseite Plan
v4.2 „AmigaDOS-Bitmap-Belegung") — dort gehörte die
Defektmasken-Semantik dazu.

### Fund D — Drei benannte Wiederhol-Strategien als Referenz für die Recovery-Pipeline (Fundus)

**Verhalten, belegt:**

| Werkzeug | Strategie | Fundstelle |
|---|---|---|
| SuperDuper | Default **4 Retries** (`RetryNumber = 4`, SD.c:106, zur Laufzeit stellbar via `SetRetryNumber`, Protos.h:78); zwischen Fehlversuch und Retry Rück-Seek auf die Fehlspur, danach Restore (`MoveAndBackupTrack`/`RestoreTrack`, Disk.c:176–185); keine Parametervariation zwischen Versuchen | SD.c:363–421 (`VerifyAndRetry`, `RecodeAndRetry`) |
| AllowBad | je Spur **bis 4 Zugriffe**: Format mit Prüfmuster → Rücklesen/Vergleich → Nullen schreiben → Verify | AllowBad.doc „Way of Operation" |
| badformat | HEAL-Modus: Spur **bis 20× mit wechselnden Bitmustern** formatieren bis Verify greift, dann mit anderem Mustersatz ebenso oft bestätigen, sonst maskieren | README („In healing mode, a track is formatted up to twenty times over with varying bit patterns until a verify succeeds…") |

**Was UFT heute tut (gemessen):** `max_retries = 5`
(`src/recovery/uft_sector_recovery.c:209`) und Multiread-Voting mit
`min_passes = 3` (`src/recovery/uft_multiread_pipeline.c:90`) — beide
Zahlen ohne benannte Referenz; eine Variation physischer Parameter
zwischen Versuchen findet nicht statt (und wäre ohnehin
Controller-Sache).

**Kennzahl:** keine der vier → **Fundus**. Wert: die drei Zitate machen
UFTs bislang unreferenzierte Retry-Konstanten erstmals **vergleichbar**
— als Zitatgrundlage für Header/Doku, nicht als Änderungsauftrag. Die
badformat-„Heilung" durch Neuformatierung ist nur an echter Hardware
mit echt defekten Medien prüfbar → Gemeinschafts-Delegation (MF-310).

### Fund E — SDDD/SDHD: SuperDupers IFF-Puffer-Diskformat (Fundus, EINFRIER-REGEL)

SuperDuper speichert Disk-Puffer als IFF `FORM SDDD` / `FORM SDHD` mit
`BODY`-Chunk, optional XPK-komprimiert (`XPKF`) — MFM.c:31–38
(`ID_SDDD`, `ID_SDHD`, `ID_XPKF`). Ein Lese-Plugin dafür wäre ein
**neues Format** und fällt unter das Moratorium — auch als Vorschlag.
Der Baum hat das bereits entschieden: `tests/differential/oracles.py:530`
führt SuperDuper3 unter den XAD-Clients ohne Plugin ausdrücklich als
„Fundus, nicht Auftrag". Hier nur ergänzt: die Quelle des Formats liegt
jetzt lokal vor, und `xadUnDisk` (XAD, bereits als Oracle-Kandidat im
Baum) kann SDDD entpacken — falls das Format je gehoben wird, sind
Referenz und Oracle damit benannt.

**Inventar-Abfragen, zitiert** (`inventar.py query`, 2026-09-01):
`superduper` → `vorhanden: false, abgedeckt: false` (Index kennt keine
Fähigkeiten; Handprüfung siehe oben); `SDDD` → dito, und da
`plugin_liste_vollstaendig: true` und SDDD ein Formatname ist: **als
Plugin wirklich nicht vorhanden**; `amigados` → `vorhanden: true`
(Treffer `amiga`); `adf` → `vorhanden: true, tier: T1b`.

---

## 3. Oracle-Tauglichkeit

**SuperDuper 3.13 (Binary `SD` im Quellarchiv, zusätzlich `SD3V13.lha`
mit Doku): tauglich, mit Aufbauaufwand.** Muster ist das im Baum bereits
beschlossene „Original-DMS unter Emulation — dasselbe Muster wie bei
X-Copy" (`tests/differential/oracles.py`). Wogegen:

1. **AmigaDOS-Encoder (Fund A):** SuperDuper formatiert in der Emulation
   eine Diskette (ADF in WinUAE), UFTs künftiger Encoder kodiert dasselbe
   ADF → Vergleich der MFM-Ströme (Sync-Zählung, Sektor-Layout,
   Prüfsummen) gegen den Emulator-Spurpuffer bzw. gegen ein aus der
   Emulation gezogenes Flux/HFE. Zweite, von libhxcfe unabhängige Hand —
   genau die Zirkularitätsvermeidung, die der fünfte Zyklus bei ADFlib
   angemahnt hat (`docs/OPEN_ITEMS.md`, MF-646 §2).
2. **Verify-Semantik (Fund B):** Referenz dafür, was ein
   zeitgenössisches Werkzeug als „identisch geschrieben" akzeptierte.

Grenze: Emulations-Oracles belegen **Kodierung**, nicht Laufwerksphysik;
Timing-/Jitter-Aussagen sind damit nicht prüfbar (kein Gerät, MF-310).

**AllowBad: als Oracle nur halb tauglich.** Es formatiert in der
Emulation fehlerfreie Disketten normal — aber der interessante Pfad
(Maskenerzeugung) braucht **echt defekte Medien**, die eine Emulation
nicht liefert. Ein `dummy.bad`-Fixture ist so nicht erzeugbar
(UNGEKLÄRT, ob WinUAE Blockfehler injizieren kann).

**badformat: nicht tauglich** — gleiche Defekt-Abhängigkeit, plus
ungeklärter Masken-Mechanismus und Zone ROT.

---

## 4. Was ich NICHT beantworten konnte

1. **badformats Masken-Mechanismus** (Bitmap-Belegung? Pseudo-Datei?):
   nur aus einem README-Nebensatz erschlossen; das Binary dürfte nach
   Zone ROT nicht disassembliert werden, ohne dass der Eigentümer die
   Rechtslage (Interoperabilitäts-Schranke) vorab entscheidet.
   Alternative: Quelle beim Autor erfragen (`neoscientists.org/~bifat/`,
   E-Mail im README).
2. **Ob WinUAE/FS-UAE Medienfehler simulieren kann** — entscheidet, ob
   AllowBad/badformat je ein Fixture liefern können. Nicht gemessen.
3. **Ob die SuperDuper-Quellen je nachlizenziert wurden** — Aminet-Readme
   und Autoren-Webseite tragen keine Lizenz; eine Anfrage an den Autor
   ist die einzige Klärung.
4. **Verhalten der Binaries** — keines der drei wurde ausgeführt (kein
   Amiga, keine eingerichtete Emulation in dieser Sitzung); alle
   Verhaltensaussagen stammen aus Quelle, Doku und String-Extraktion.
5. **AllowBad-Quellcode** — existiert laut Doku nur auf Anfrage beim
   Autor (Postadresse von 1996; Erreichbarkeit ungeprüft).

---

## 5. OPEN_ITEMS-Vorschlag (1 von max. 5 — die übrigen Funde bewegen keine Kennzahl und bleiben Fundus)

| # | Vorschlag | Kennzahl | Kanal | Aufwand |
|---|---|---|---|---|
| 1 | **SuperDuper 3.13 als Emulations-Oracle für den AmigaDOS-MFM-Encoder registrieren** (Erweiterung des MF-646-Folgeziels zur MF-539-Lücke): Eintrag in `tests/differential/oracles.py` nach dem Muster „Original-DMS unter Emulation"; Binary `SD` SHA-gepinnt (`63bfc805…6e628208`), Bezugsquelle Aminet `disk/misc/SuperDuper-3.0_src.lha`; Lizenzzone PRÜFEN mit wörtlich zitierter Klausel (nur unveränderte Weitergabe — für ein extern laufendes Oracle nach Matrix-ROT-Zeile zulässig, Vorlage an Eigentümer); Differenzlauf-Skizze aus §3.1 dieses Gutachtens. Primäre Spec-Quelle bleibt die offene Doku/libhxcfe (MF-646); SuperDuper ist die von ADFlib **und** libhxcfe unabhängige zweite Hand | Wandlungspfade **rauf** (ADF→HFE) | Oracle + Spec-Zweitquelle | M |

**Eigentümer-Vorlagen darin (Eskalation statt Auslegung, Regel 8):**
(a) Nachlizenzierungs-Anfrage an S. Vigna analog XCopy/MF-747 — würde
Fund A vom Oracle- auf den Nachbau- oder gar Port-Kanal heben;
(b) Rechtsfrage Disassemblierung badformat (Interoperabilität) — nur
falls der Masken-Mechanismus je gebraucht wird.
