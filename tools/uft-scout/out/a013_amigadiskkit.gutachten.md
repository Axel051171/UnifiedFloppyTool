# A-013 · Gutachten `thomas-luebker/AmigaDiskKit` — **das ist `P3-385(b)`**

**Stand:** 2026-09-16 · **Posten:** `A-013` · **Lizenz:** **Apache-2.0**
**Prüfstand:** `origin/main` = `70a940af` · Klon: `tools/uft-scout/work/AmigaDiskKit`

---

## Ergebnis in vier Sätzen

Das Repo ist **groß, aktiv und inhaltlich stark** — 97 Swift-Dateien in
vierzehn Achsen, 26 Prüfdateien aus **echten** Amiga-Aufbauten. Der
Lizenzbefund von `P3-385` gilt unverändert: **Apache-2.0, also kein Port**.
**Drei Zahlen von `P3-385` sind allerdings falsch, und ich habe sie
nachgemessen** — `RDB`, `FFS2` und `LHA` stehen dort als 0 bzw. 1, gemessen
sind es 11, 1 und 8. Der Orakel-Kanal, den `P3-385` als einzigen offenlässt,
ist auf dieser Maschine **verschlossen**: es gibt keine Swift-Werkzeugkette.

**Empfehlung: REFERENCE + Daten-Kandidat, Orakel delegieren.**

---

## 1. Die vierzehn Achsen, je mit Urteil (a)

Gemessen je Verzeichnis unter `Sources/AmigaDiskKit/`, dazu die
Gegenmessung im Baum (`git grep`, **case-sensitive**, `rc` geprüft):

| Achse | Dateien | Zeilen | Im Baum | Urteil |
|---|---|---|---|---|
| **PFS3** | 6 | **3207** | `PFS3` **rc 1, 0 Treffer** | **fehlt im Baum** |
| **LHA** | 7 | **1797** | `LHA` 8 Treffer — **drei ehrliche Stummel** (§2) | **ist da und schwächer** |
| **FFS** | 5 | 1793 | `OFS` 114, `FFS2` **1** (Kommentar) | **ist da; FFS2 fehlt** |
| **Floppy** | 8 | 1679 | ADF 460 Treffer, `uft_amigados` auf FS-T2 | **ist da und gleichwertig** |
| **FAT** | 5 | 1183 | `FAT32` 80 Treffer, eigene Bootsektor-Schicht | **ist da und gleichwertig** |
| **Preview** (Icon, IFF) | 6 | 1133 | `IFF` 13 Treffer | **ist da und schwächer** |
| **Tooling** | 5 | 995 | — | uninteressant (Swift-Werkzeugkasten) |
| **RDB** | 4 | **908** | `RDB` **11 Treffer in 2 Dateien**, echte Umsetzung (§3) | **ist da und schwächer** |
| **ZIP** | 2 | 498 | — | uninteressant für Disketten |
| **ImageIO** | 3 | 460 | eigene Behälterschicht | **ist da und gleichwertig** |
| **Parsing** | 4 | 316 | — | uninteressant (Swift-Hilfsmittel) |
| **ISO** | 1 | 279 | — | ausserhalb des Scope |
| **Diagnostics** | 1 | 118 | eigene Diagnoseschicht | **ist da** |
| **MBR** | 1 | 109 | `uft_fat32_mbr.h` | **ist da und gleichwertig** |

Dazu 3 Dateien in der Wurzel und **1 `AmigaDiskCLI`** — ein
Kommandozeilenwerkzeug, das für UFT (GUI-only) nicht als Vorlage taugt,
wohl aber als **Orakel-Einstiegspunkt** (§4).

**Die eine Achse, die dem Baum wirklich fehlt, ist `PFS3` — 3207 Zeilen,
die größte Einzelachse des Repos, und im Baum rc 1 / 0 Treffer.**

---

## 2. LHA: deklariert, registriert, gestummelt

Der Baum trägt LHA an acht Stellen:

| Stelle | Art |
|---|---|
| `include/uft/core/uft_format_registry.h:144` | `UFT_FMT_LHA = 203` — eine **Format-ID** |
| `include/uft/fs/uft_amigados_extended.h` | sechs Zeilen: Kopfkommentar, Abschnitt, Struktur, **drei Funktionsdeklarationen** |
| `src/fs/uft_amigados_extended.c:683` | Abschnitt **„LHA Archive - Stubs"** |

Und die drei Funktionen dort lauten vollständig:

```c
int uft_amiga_lha_list(...)            { …; return -1;  /* Not implemented */ }
int uft_amiga_lha_extract(...)         { …; return -1;  /* Not implemented */ }
int uft_amiga_lha_extract_to_host(...) { …; return -1;  /* Not implemented */ }
```

**Das ist ein ehrlicher Stummel** — er lügt nicht, er sagt es. Aber es ist
eine Fähigkeit, die als ID registriert ist und nicht existiert; die Klasse
`P3-204`/MF-930. **AmigaDiskKit hat sie vollständig, in 1797 Zeilen — und
Apache-2.0 verbietet den Port.**

Das ist damit ein **Nachbau**-Kandidat nach `docs/QUARANTINE_PROCESS.md` §5,
kein Port. LHA ist ein dokumentiertes Archivformat; die Spezifikation ist
frei verfügbar, die Umsetzung hier ist nur eine von vielen.

---

## 3. Drei Zahlen von `P3-385` sind falsch (e)

`P3-385` notiert: *„`rdb` 1 Datei / `RDB` 0 / `rigid_disk` 0,
`pfs3`/`PFS3` **0**, `ffs2`/`FFS2` **0**, `lha` 0 / `LHA` 1."*

**Nachgemessen, case-sensitive, `rc` je Lauf:**

| Suche | P3-385 | gemessen | |
|---|---|---|---|
| `RDB` | **0** | **11 Treffer in 2 Dateien** | falsch |
| `rigid_disk` | 0 | rc 1, **0** | stimmt |
| `PFS3` | 0 | rc 1, **0** | stimmt |
| `FFS2` | **0** | **1** | falsch |
| `LHA` | **1** | **8** | falsch |

**Und `RDB` ist nicht bloß erwähnt, sondern umgesetzt.**
`src/formats/uft_hdf_parser.c` enthält `uft_hdf_parse_rdb()`,
`calc_rdb_checksum()`, liest `rdb_blocks_lo`/`_hi` als Big-Endian bei
Versatz 128/132 und hat eine ausdrückliche Schranke gegen ein
„malformed RDB (valid magic + size 0/1/2)". 14 Funktionen in der Datei.

**`FFS2` dagegen ist genau eine Kommentarzeile** —
`include/uft/fs/uft_amigados.h:15`: *„OFS/FFS + Long Filenames (FFS2)"*.
Also **erwähnt, nicht umgesetzt**: die Zahl 1 ist richtig, das Urteil
„fehlt" bleibt es auch.

`P3-385` gehört um diese drei Messungen fortgeschrieben — **nicht** durch
einen neuen Punkt, wie die Abschlussbedingung (e) verlangt.

---

## 4. Die Orakel-Frage, ausdrücklich entschieden (b)

`P3-385` lässt als einzigen Kanal **Orakel** offen. `docs/ORACLES.md`
verlangt dafür: *„Kein Oracle auf Zusicherung — ein Werkzeug, das nicht
gebaut und ausgeführt wurde, ist kein Eintrag."*

**Auf dieser Maschine ist der Kanal verschlossen.** `swift`, `swiftc` und
`xcodebuild` sind nicht vorhanden (bei der Aufnahme gemessen). Damit ist
ein Eintrag nach `docs/ORACLES.md` **nicht möglich** — nicht grundsätzlich,
sondern hier. Dieselbe Lage wie die Tier-3-Hardwarebank (MF-310):
**delegierbar, nicht hausintern.**

**Was genau delegiert werden müsste, damit es ein Eintrag wird:**

1. **Bau** auf macOS oder Linux mit Swift >= 5.9 (`Package.swift` liegt
   vor), Ziel `AmigaDiskCLI`.
2. **Versionsabfrage**, die in `docs/ORACLES.md` zitierbar ist — der Baum
   verlangt sie für jedes Orakel (vgl. den libdsk-Fall, wo
   `dsktrans -version` nachträglich gefunden wurde).
3. **Der Beleg wäre ein Differenzlauf**, nicht ein Lauf: dieselbe ADF
   einmal durch `AmigaDiskCLI` und einmal durch UFT, Sektor für Sektor
   verglichen. Erst eine **Abweichung oder ihr Ausbleiben** ist die
   Aussage.
4. **Und die Gegenprobe gehört dazu:** `P3-385` sucht eine
   **ADFlib-unabhängige** Zweitmeinung. Dass AmigaDiskKit unabhängig ist,
   muss selbst gemessen werden — im Repo nach ADFlib-Spuren suchen, wie es
   beim Nachbarn `amigadx` (der ADFlib 0.7.10 vendort) aufgefallen ist.

**Ohne Punkt 4 ist auch ein erfolgreicher Bau wertlos**: zwei Werkzeuge mit
derselben Herkunft sind eine Hand, nicht zwei (MF-1033).

---

## 5. Die 26 Prüfdateien als Daten-Kanal (c)

**Gemessen: 26 `.bin`, zusammen 5 097 472 Byte**, unter
`Tests/AmigaDiskKitTests/Fixtures/binary/`.

Die Namen sagen, was sie sind — **gezielte Ausschnitte echter Aufbauten**,
keine ganzen Abbilder:

| Gruppe | Beispiele |
|---|---|
| `classic-8g-` | `dh0-bootblock`, `dh1-bootblock`, `rdb-area` |
| `mister-8g-` | `dh0-bootblock`, `rdb-area` (MiSTer-FPGA) |
| `pistorm-29g-` | `fat32-boot-sector` (PiStorm) |
| `hst-` | `fat32-system-area`, `fsadd-dos7-rdb-area`, `rdbinit-rdb-area` |
| `pfs3-2g/8g-` | `rdb-area`, `reserved-area` |

Größenverteilung: 6 x 512 B (Bootsektoren), 1 x 1024, 8 x 4096,
5 x 32768, 2 x 40960, 1 x 81920, 1 x 524288, 1 x 2097152.

**Das ist ausgezeichnetes Fixture-Material** — klein, gezielt, aus echter
Hardware, und genau die Bereiche (RDB, Bootblock, FAT32-Systembereich), an
denen sich ein Leser blamiert.

**ABER: die Lizenzfrage ist eine andere als die des Repos.** Das ist die
Lage aus `SCOUT-5`: **Repolizenz ist nicht Disketteninhalt.** Apache-2.0
deckt den Quelltext; ob der Urheber die Ausschnitte fremder Systemsoftware
(Amiga-Bootblöcke enthalten Code von Commodore bzw. dem jeweiligen
Dateisystem) weitergeben durfte, sagt die Repolizenz **nicht**.

**Urteil: Daten-Kandidat mit offener Herkunftsfrage.** Eine Herkunftsangabe
je Datei fehlt im Repo (gesucht, nicht gefunden). Das gehört nachgefragt,
bevor eine dieser Dateien in den Korpus wandert.

---

## 6. Die fünf Fragen (d)

### 6.1 „wo können die formate verbessert werden"

Zwei Stellen, beide gemessen: **LHA** ist registriert und gestummelt (§2),
**FFS2** ist eine Kommentarzeile ohne Umsetzung (§3). Beides sind Zusagen
ohne Tat in der Amiga-Schicht.

### 6.2 „ist es auf andere formate übertragbar"

**PFS3 nicht** — es ist ein Amiga-Dateisystem. **LHA schon**: das Archiv
kommt auch auf MSX, Sharp X68000 und PC-98 vor. **Und die RDB-Achse
ebenfalls**, weil Partitionstabellen auf Festplatten aller dieser Systeme
liegen — nur ist das jenseits des Disketten-Scope.

### 6.3 „welche einstellungen fehlen noch"

Keine, die dieses Repo beantwortet — es ist eine Bibliothek ohne
Einstellungsebene.

### 6.4 „was habe wir noch nicht"

**PFS3** (3207 Zeilen, im Baum rc 1 / 0 Treffer) und eine **funktionierende
LHA-Entpackung** (im Baum drei Stummel).

### 6.5 „brauch es eine HAL-Erweiterungen"

**Nein.** Alles hier arbeitet auf Abbilddateien.

---

## 7. Disposition

| Teil | Urteil | Kanal |
|---|---|---|
| Quelltext gesamt (97 Swift) | **BLOCKED für Port** | Apache-2.0, unverträglich mit GPL-2 (`P3-385`) |
| `PFS3` (6 Dateien, 3207 Z.) | **REFERENCE** | *Spec* — lesen erlaubt; ein **Nachbau** wäre ein eigener Posten |
| `LHA` (7 Dateien, 1797 Z.) | **REFERENCE** | *Spec*; LHA ist dokumentiert, ein Nachbau braucht dieses Repo nicht |
| `AmigaDiskCLI` | **Orakel-Kandidat, hier verschlossen** | delegieren (§4) |
| die 26 `.bin` | **Daten-Kandidat mit offener Herkunft** | §5 |
| `ZIP`, `ISO`, `Parsing`, `Tooling` | uninteressant | ausserhalb des Scope |

**Kein Byte dieses Repos ist nach `src/`, `include/` oder `tests/`
geschrieben worden (f).** Der Klon liegt unter `tools/uft-scout/work/`.

---

## 8. Fortschreibung von `P3-385` (e)

Kein neuer Punkt. `P3-385` bekommt:

1. **Drei berichtigte Zahlen** (§3): `RDB` 11 statt 0 — und **umgesetzt**,
   nicht erwähnt; `FFS2` 1 statt 0, aber nur als Kommentar; `LHA` 8 statt 1,
   davon drei **Stummel**.
2. **Die Orakel-Absage mit Begründung** (§4): auf dieser Maschine
   verschlossen, delegierbar, und was genau delegiert werden müsste — samt
   der Gegenprobe auf ADFlib-Unabhängigkeit, ohne die auch ein
   erfolgreicher Bau nichts belegt.
3. **Den Daten-Kanal mit seiner offenen Frage** (§5): 26 Dateien,
   5 097 472 Byte, echte Aufbauten, **Herkunft je Datei nicht angegeben**.

---

## 9. Quellen

* `https://github.com/thomas-luebker/AmigaDiskKit`, flach geklont
  2026-09-16 nach `tools/uft-scout/work/AmigaDiskKit`
* **Apache License 2.0** (`LICENSE`)
* Umfang gemessen: **97** `.swift`, **26** `.bin`, **11** `.txt` — genau
  die Zahlen aus `P3-385`
* Baum: `origin/main` = `70a940af`
* Gegengehalten: `P3-385`, `P3-316`/`P3-353` (Apache-Präzedenz), `P3-204`,
  `MF-310` (delegierbar statt hausintern), `MF-1033` (zwei Werkzeuge
  derselben Hand sind eine Hand), `SCOUT-5` (Repolizenz ist nicht Inhalt)
