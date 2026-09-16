# A-015 · Gutachten `programandala-net/mkmgt` — MGT steht auf T1, die Frage ist Beta DOS

**Stand:** 2026-09-16 · **Posten:** `A-015` · **Lizenz:** GPL-3.0 · Sprache: **Forth**
**Prüfstand:** `origin/main` = `70a940af` · Klon: `tools/uft-scout/work/mkmgt`

---

## Ergebnis in vier Sätzen

Der Posten fragte nach **Beta DOS** und bekommt eine Antwort, die weiter
reicht: **UFT benennt keines der drei DOSe im Quelltext** — weder GDOS noch
G+DOS noch Beta DOS, alle drei rc 1 / 0 Treffer in `src/` und `include/`.
**Und beim Nachmessen der Feldlagen ist etwas Schwereres aufgefallen: UFTs
Verzeichnisstruktur ist ab Versatz 11 um ein Byte verschoben**, weil sie die
Sektorzahl als **ein** Byte führt, wo der Schreiber **zwei** in Big-Endian
schreibt. **Das ist an einem echten Abbild des eigenen Korpus gemessen, nicht
vermutet**: alle vier geprüften Verzeichniseinträge melden `sectors_used = 0`
(§3.5). `mgt` steht dabei auf **T1**, der höchsten Stufe, mit sechs Tests.

**Empfehlung: der Fund ist ein OPEN_ITEMS-Befund, kein Einbau.** Aus dem
Repo selbst ist nichts zu übernehmen — 773 Zeilen Forth, und der Behälter
ist fertig.

---

## 1. Was ein Schreiber für `mgt` nicht mehr leisten kann (a)

Die Aufnahme dieses Postens hat ihre eigene Erwartung bereits korrigiert,
und sie hatte recht: **`mgt` steht auf T1** — die höchste Stufe — mit sechs
Tests (`test_durchschreibprobe`, `test_kopflose_sonden_sind_erreichbar`,
`test_mgt_gegen_mame`, `test_mgt_schreibt_in_die_datei`,
`test_mgt_verzeichnis_vollstaendig`, `test_oeffentliche_api_am_korpus`).

**Ein fremder Erzeuger hebt dort nichts.** Und `mkmgt` wäre ohnehin ein
schwacher Kandidat: es ist in **Forth** geschrieben, und `gforth`, `pforth`,
`sf` und `swiftforth` sind auf dieser Maschine alle vier nicht vorhanden —
**die vierte fehlende Werkzeugkette in Folge** nach `mkfs.fat`/`mtools`
(A-005), Swift (A-013) und Rust (A-014).

**Der Wert des Repos liegt woanders: es ist ein SCHREIBER, und er
dokumentiert die Feldlagen des Verzeichniseintrags im Klartext.** Genau
daran ist §3 aufgefallen.

---

## 2. Die drei DOSe, einzeln gegen den Baum (b)

Gemessen **nur in `src/` und `include/`** — Doku und die Aufgabenliste sind
ausgeschlossen, weil dort mein eigener Aufnahmetext die Repo-Beschreibung
zitiert und ein Selbstbezug entstünde:

| Begriff | rc | Treffer im Code |
|---|---|---|
| `GDOS` | **1** | **0** |
| `gdos` | **1** | **0** |
| `G+DOS` | **1** | **0** |
| `BetaDOS` | **1** | **0** |
| `Beta DOS` | **1** | **0** |
| `betados` | **1** | **0** |
| `plusd` | **1** | **0** |
| `DISCiPLE` | 0 | **9** in 5 Dateien |

**Antwort auf (b): UFT nimmt keines an und benennt auch keines.** Es kennt
die **Hardware** (DISCiPLE/+D, in `uft_mgt.h`, `uft_format_plugin.h`,
`uft_xdf_zxdf.h`, `uft_profile_uk.c`, `uft_mgt.c`) und liest den
**Behälter** samt Verzeichnis — aber welches Dateisystem darin liegt, ist im
Quelltext kein Begriff.

**Das ist kein Fehler, sondern eine Lücke mit Grund:** die drei DOSe teilen
sich dasselbe Verzeichnisformat; sie unterscheiden sich in Randdingen.
Solange UFT nur Typ und Namen liest, muss es sie nicht trennen. **Sobald es
den GDOS-Kopf (Versatz 210-219) auswertet, muss es das sehr wohl** — dort
liegen Startadresse und Autostart, und die sind vom DOS abhängig.

---

## 3. Der eigentliche Fund: die Verzeichnisstruktur ist um ein Byte verschoben

**Das ist nicht das, wonach dieser Posten gesucht hat, und es wiegt
schwerer.**

### 3.1 Was der Schreiber schreibt

`mkmgt.fs` setzt die Felder einzeln und kommentiert jede Position:

| Position | was dort steht | Forth-Wort |
|---|---|---|
| **0** | Dateityp | `file-type 0 entry-pos+ mgtc!` (Byte) |
| **1-10** | Dateiname | `dos-filename 1 entry-pos+ filename!` |
| **11-12** | **Zahl der belegten Sektoren** | `11 entry-pos+ mgt!be` — **16 Bit, Big-Endian** |
| **13** | Startspur (Seitenbit 128) | `13 entry-pos+ mgtc!` (Byte) |
| **14** | Startsektor (1-10) | `starting-sector @ 14 entry-pos+ mgtc!` |
| **15-209** | Sektorbelegungskarte | „Create the sector address map (positions 15-209)" |
| **210-219** | GDOS-Dateikopf | „Set the GDOS header (positions 210-219)" |

### 3.2 Was UFT annimmt

`include/uft/formats/uft_mgt.h`, `#pragma pack(push, 1)`:

```c
typedef struct {
    uint8_t  type;              /* 0        */
    char     filename[10];      /* 1-10     */
    uint8_t  sectors_used;      /* 11       <- real: 11-12, 16 Bit BE */
    uint8_t  track;             /* 12       <- real: 13 */
    uint8_t  sector;            /* 13       <- real: 14 */
    uint8_t  sector_map[195];   /* 14-208   <- real: 15-209 */
    uint8_t  reserved[46];      /* 209-254  <- GDOS-Kopf liegt bei 210-219 */
} mgt_dir_entry_t;
```

**Ab Versatz 11 stimmt keine Angabe mehr.** Die Folgen, je Feld:

* `sectors_used` liest das **hohe** Byte einer Big-Endian-16-Bit-Zahl. Für
  jede Datei unter 256 Sektoren ist das **0** — eine zehn Sektoren lange
  Datei meldet null.
* `track` liest das **niedrige Byte der Sektorzahl**.
* `sector` liest die **Spur**.
* `sector_map` beginnt ein Byte zu früh und endet ein Byte zu früh.

### 3.3 Und die Struktur widerspricht sich selbst

`1 + 10 + 1 + 1 + 1 + 195 + 46 = 255`. Ein Verzeichniseintrag ist
`MGT_DIR_ENTRY_SIZE = 256`. **Die gepackte Struktur ist ein Byte zu kurz**,
und `uft_mgt_read_directory()` kopiert mit
`memcpy(&entries[count], src, sizeof(mgt_dir_entry_t))` genau 255 Byte —
das letzte Byte jedes Eintrags fällt still weg.

Das ist ein **inneres** Anzeichen, das ohne fremde Quelle sichtbar gewesen
wäre: eine Struktur, die einen 256-Byte-Satz beschreibt und 255 Byte lang
ist, hat ein Feld zu wenig oder eines zu kurz.

### 3.4 Warum keiner der sechs Tests anschlägt — gemessen

`tests/test_mgt_verzeichnis_vollstaendig.c` baut seine Prüfdaten **selbst**
(`p[0] = 1; /* type: BASIC */`) und prüft:

```
ASSERT(n == 12);                                    /* Anzahl */
ASSERT(n == MGT_DIR_ENTRIES);                       /* Anzahl */
ASSERT(eintraege[MGT_DIR_ENTRIES - 1].type == 1);   /* Versatz 0  */
ASSERT(memcmp(..., .filename, "DATEI079", 8) == 0); /* Versatz 1-10 */
ASSERT(eintraege[i].type == 0);                     /* Versatz 0  */
```

**Es fasst `sectors_used`, `track`, `sector` und `sector_map` nie an** —
also genau die vier verschobenen Felder nicht. Geprüft werden die beiden
Felder, die stimmen.

Und `test_mgt_gegen_mame` (MF-1006) verglich laut seiner eigenen Notiz
*„Geometrie, Sektor-IDs und Spurversatz"* — den **Behälter**, nicht das
Verzeichnis. **Die T1-Stufe ist damit korrekt vergeben und sagt über diese
Struktur nichts.** Das ist wörtlich die Lehre aus
`schreibseite_ohne_fremde_hand`: die Tier-Stufen messen eine bestimmte
Achse, nicht alles.

### 3.5 Der Befund ist BEWIESEN — an einem echten Abbild aus dem Korpus

Der Baum führt drei echte MGT-Abbilder: `tests/corpus/zxfd_plusd_clean.mgt`,
`zxfd_plusd_gdos_clean.mgt` und `zxfd_plusd_gdos_tools.mgt` (je 819 200 B).
Die ersten vier belegten Verzeichniseinträge des dritten, roh gelesen:

| Eintrag | Typ | Name | Bytes 11-14 | **mkmgt-Lesart** | **UFT-Lesart** |
|---|---|---|---|---|---|
| 0 | 4 | `+SYS 2a` | `00 0E 04 01` | 14 Sektoren, Spur 4, Sektor 1 | **0 Sektoren**, Spur 14, Sektor 4 |
| 1 | 1 | `CONFIG` | `00 20 05 05` | 32 Sektoren, Spur 5, Sektor 5 | **0 Sektoren**, Spur 32, Sektor 5 |
| 2 | 4 | `CONFIG1_C` | `00 04 08 07` | 4 Sektoren, Spur 8, Sektor 7 | **0 Sektoren**, Spur 4, Sektor 8 |
| 3 | 4 | `CONFIG2_C` | `00 0E 09 01` | 14 Sektoren, Spur 9, Sektor 1 | **0 Sektoren**, Spur 14, Sektor 9 |

**Drei Dinge entscheiden es, und keines davon braucht eine fremde Quelle:**

1. **`sectors_used` ist bei JEDEM Eintrag 0.** Eine vorhandene Datei mit
   null belegten Sektoren gibt es nicht. Byte 11 ist immer das **hohe**
   Byte einer kleinen Zahl.
2. **Die Sektornummern einer MGT-Diskette laufen von 1 bis 10.** Nach
   mkmgts Lesart ergeben sich 1, 5, 7, 1 — alle im Bereich. Nach UFTs
   Lesart ergeben sich 4, 5, 8, 9 — zufällig auch im Bereich, aber es sind
   in Wirklichkeit **Spurnummern**.
3. **Die Spurwerte nach UFTs Lesart sind 14, 32, 4, 14** — und bei
   `CONFIG` läge die Datei damit auf Spur 32, während die Sektorzahl
   verschwindet. Nach mkmgts Lesart steigen die Spuren monoton (4, 5, 8, 9),
   wie es bei fortlaufend belegten Dateien sein muss.

**Damit ist der Defekt gemessen, nicht vermutet.** Die drei Stützen aus der
Quellenlage bleiben bestehen und stimmen mit der Messung überein:
`mkmgt.fs` als **Schreiber** (MF-1179), die interne Unstimmigkeit 255 gegen
256 (§3.3), und `src/samdisk/mgt.cpp:34` mit `dir_offset = 0xd3` = **211**
— genau mkmgts „211: Tape header ID".

---

## 4. Die fünf Fragen

### 4.1 „wo können die formate verbessert werden"

§3. Und es ist eine Verbesserung, die keine neue Fähigkeit braucht — nur
eine berichtigte Struktur und einen Test, der die vier übrigen Felder
anfasst.

### 4.2 „ist es auf andere formate übertragbar"

**Die Fehlerklasse ja, und sie ist in diesem Baum belegt.** Eine
Strukturbeschreibung, deren Feldsumme nicht auf die Satzgröße aufgeht, ist
dasselbe Muster wie MF-1015 (`udi`, Kopf ohne Größenfeld), MF-1009
(`apridisk`, 8-Byte-Deskriptor, den es nicht gibt) und MF-1017 (`jv3`,
`0x2300` statt `0x2200`). **Ein billiges Tor wäre: für jede gepackte
Struktur, die einen Satz fester Größe beschreibt, die Feldsumme gegen die
Konstante halten.**

### 4.3 „welche einstellungen fehlen noch"

Keine aus diesem Repo.

### 4.4 „was habe wir noch nicht"

Die **Auswertung des GDOS-Kopfes** (Versatz 210-219): Bandkopf-Kennung,
Dateilänge, Startadresse, Autostart. UFT führt diese zehn Byte als
namenloses `reserved[46]`.

### 4.5 „brauch es eine HAL-Erweiterungen"

**Nein.**

---

## 5. Disposition

| Teil | Urteil | Grund |
|---|---|---|
| `mkmgt.fs` (773 Z., Forth) | **REFERENCE** | *Spec* — die Feldlagen sind der Wert, nicht der Code |
| als Orakel | **verschlossen** | keine Forth-Werkzeugkette; vierte in Folge |
| als Port | **entfällt** | Forth nach C ist eine Neuschreibung, kein Port |
| `README.adoc` | **REFERENCE** | nennt die drei DOSe ausdrücklich |

**Kein Byte dieses Repos ist nach `src/`, `include/` oder `tests/`
geschrieben worden.** Der Klon liegt unter `tools/uft-scout/work/`.

---

## 6. Vorschläge für `docs/OPEN_ITEMS.md`

1. **`mgt_dir_entry_t` ist ab Versatz 11 um ein Byte verschoben — gemessen
   an einem echten Korpus-Abbild** (§3.5). `sectors_used` liest das hohe
   Byte einer 16-Bit-BE-Zahl und ist bei **allen vier** geprüften Einträgen
   von `zxfd_plusd_gdos_tools.mgt` **0**; `track` liest das niedrige Byte
   der Sektorzahl (14, 32, 4, 14); `sector` liest die Spur. Dazu geht die
   Feldsumme nicht auf: 255 gegen `MGT_DIR_ENTRY_SIZE` 256, und
   `uft_mgt_read_directory()` kopiert entsprechend ein Byte zu wenig.
   **`mgt` steht auf T1** — die Stufe ist korrekt vergeben und sagt über
   das Verzeichnis nichts; `test_mgt_gegen_mame` verglich den Behälter, und
   `test_mgt_verzeichnis_vollstaendig` prüft nur die Versätze 0 und 1-10,
   also genau die beiden Felder, die stimmen. *Kennzahl:* keine der vier;
   es ist die Ehrlichkeit einer T1-Aussage.
2. **Kein einziges der drei ZX-Dateisysteme wird im Quelltext benannt** —
   GDOS, G+DOS, Beta DOS je rc 1 / 0 Treffer in `src/` und `include/`
   (§2). Solange nur Typ und Name gelesen werden, ist das folgenlos; mit
   dem GDOS-Kopf wird es zur Unterscheidungspflicht. *Kennzahl:* keine der
   vier.
3. **Ein Tor wäre billig: Feldsumme gegen Satzgröße** (§4.2) — für jede
   `#pragma pack`-Struktur, die einen Satz fester Größe beschreibt.
   Belegte Vorfälle derselben Klasse: MF-1015, MF-1009, MF-1017.
   *Kennzahl:* keine der vier; Verlässlichkeit des Prüfstands.

---

## 7. Quellen

* `https://github.com/programandala-net/mkmgt`, flach geklont 2026-09-16
  nach `tools/uft-scout/work/mkmgt`
* **GPL-3.0**; ein Forth-Quelltext, **773 Zeilen**, letzter Push 2020-05-14
* Feldlagen: `mkmgt.fs:360-400` (Verzeichniseintrag) und `:415-480`
  (GDOS-Kopf), je mit Positionskommentar
* Baum: `origin/main` = `70a940af`; `include/uft/formats/uft_mgt.h`,
  `src/formats/mgt/uft_mgt.c` (573 Z.), `src/samdisk/mgt.cpp`,
  `tests/test_mgt_verzeichnis_vollstaendig.c`
* Gegengehalten: `MF-1006` (mgt gegen MAME — Behälter, nicht Verzeichnis),
  `MF-1179` (Schreiber ist die stärkere Gattung), `MF-1015`/`MF-1009`/
  `MF-1017` (Strukturen, deren Feldsumme nicht aufging)
