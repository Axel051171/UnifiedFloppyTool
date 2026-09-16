# A-011 · Gutachten `Metadaten.zip` — Herkunft und Datierung über die VSN

**Stand:** 2026-09-16 · **Posten:** `A-011` · **Kanal:** *Spec* · **Lizenz:** GPL-2.0-or-later
**Prüfstand:** `origin/main` = `70a940af`

---

## Ergebnis in drei Sätzen

**Der veröffentlichte Prüfvektor ist hier nachgerechnet und trifft** — beide
Wörter und alle vier Zwischenwerte. Die Zulieferung bringt mit `uft_vsn_*`
vier Symbole, die es im Baum nicht gibt, und beantwortet damit eine Frage,
die der Baum bisher nicht stellt: *wann wurde diese Diskette formatiert?*
**Die offene Bytefolge-Frage bleibt offen — aber sie ist jetzt entscheidbar**,
weil der Baum einen echten Prüfwert aus einem Win98-Abbild führt, den bisher
niemand dafür herangezogen hat.

**Empfehlung: REGISTER, mit einer benannten Vorbedingung.**

---

## 1. Der Prüfvektor, selbst nachgerechnet (b)

Die Belegkette nennt Wilsons durchgerechnetes Beispiel:

> **19.10.2003 22:33:27.01 -> `2514-1DF4`**

Die Rechenvorschrift aus `uft_vsn.c:12-18`:

```
lo = (sekunde * 256 + hundertstel) + (monat * 256 + tag)
hi = (stunde  * 256 + minute)      + jahr
```

**Nachgerechnet — nicht im Kopf, sondern gerechnet**, weil Handarithmetik in
diesem Baum eine belegte Fehlerquelle ist:

| Zwischenwert | gerechnet | Bericht | |
|---|---|---|---|
| `sekunde*256 + hundertstel` | 6913 = `0x1B01` | `0x1B01` | ✓ |
| `monat*256 + tag` | 2579 = `0x0A13` | `0x0A13` | ✓ |
| **lo** | 9492 = **`0x2514`** | `0x2514` | **trifft** |
| `stunde*256 + minute` | 5665 = `0x1621` | `0x1621` | ✓ |
| `jahr` | 2003 = `0x07D3` | `0x07D3` | ✓ |
| **hi** | 7668 = **`0x1DF4`** | `0x1DF4` | **trifft** |

**Beide Wörter treffen, und die vier Zwischenwerte ebenfalls.** Das ist der
einzige Rotbeweis dieser Begutachtung, der von außen kommt, und er hält.

**Eine Beobachtung am Rand, die zu §3 gehört:** die Rechnung liefert den
Zahlenwert `hi:lo` = `0x1DF42514`; angezeigt wird `2514-1DF4`, also **lo
zuerst**. Das ist innerhalb der Zulieferung schlüssig (`uft_vsn_format()`
druckt `vsn & 0xFFFF` vor `vsn >> 16`) — es ist aber eine **Festlegung**,
keine Messung.

---

## 2. Was es im Baum nicht gibt (a, Teil 1)

| Bezeichner | rc | Treffer | Urteil |
|---|---|---|---|
| `uft_vsn_compute` | **1** | 0 | **gibt es nicht** |
| `uft_vsn_from_bytes` | **1** | 0 | **gibt es nicht** |
| `VolumeSerial` | **1** | 0 | **gibt es nicht** |
| `volume_serial` | 0 | 19 | **gibt es** — als Feld, nicht als Rechnung |

**Der Baum speichert die VSN, er deutet sie nicht.** Er liest vier Bytes und
legt sie ab; dass darin ein **Zeitstempel** steckt, steht nirgends.

---

## 3. Die offene Bytefolge-Frage — offen gelassen und messbar gemacht (c)

Die Zulieferung sagt selbst:

> *„Die Bytefolge der beiden Wörter im Sektor ist aus den Quellen nicht
> zweifelsfrei zu entnehmen. Wilsons Beispiel zeigt `25 14 1D F4` … `src/fs/
> uft_fat12.c:191` liest dagegen `le32(d + 0x27)`."*

**Gemessen im Baum — die Zeile stimmt, nur die Nummer nicht:**
`src/fs/uft_fat12.c:221` liest `v->serial = le32(d + 0x27);`

Beide Lesarten sind unvereinbar, und das lässt sich beziffern: aus den
Sektorbytes `25 14 1D F4` wird
* als `le32`: **`0xF41D1425`**
* als zwei Big-Endian-Wörter, lo zuerst: **`0x1DF42514`**

**Diese Frage wird hier NICHT entschieden.** Aber sie ist entscheidbar, und
der Prüfstein liegt bereits im Baum:

> `tests/test_win98_fdb.c:279`
> `ASSERT(r.fat.volume_serial == 0x27156C21u);`

Das ist ein **echter** Wert aus einem echten Win98-Abbild, gelesen über
`src/formats/fat/uft_fat_bootsector.c:342`. Wer die vier Rohbytes an
Versatz 0x27 dieses Abbilds neben diesen geparsten Wert legt, hat die
Bytefolge in einer Messung — ohne eine einzige Quelle zitieren zu müssen.

**Und eine zweite, schärfere Probe liegt gleich daneben:** wenn die VSN ein
Zeitstempel ist, muss sich `0x27156C21` in ein **plausibles Datum**
zurückrechnen lassen. Tut es das nicht, ist entweder die Bytefolge falsch
oder die Diskette wurde nicht von DOS formatiert — und beides ist ein
Ergebnis. Das ist derselbe Beweistyp wie MF-869 (die CRCs standen auf der
Diskette) und MF-1013.

**Vorbedingung für einen Einbau:** erst diese Messung, dann der Code. Ohne
sie trüge ein Datierungsbefund eine Bytefolge, die niemand geprüft hat —
und ein falsches Datum in einem Erhaltungsprotokoll ist genau die stille
Falschaussage, gegen die dieser Baum gebaut ist.

---

## 4. Die Zahl der Metadatenmodelle (d)

Gezählt, nicht geschätzt: **`volume_serial` ist als Strukturfeld in zwölf
Headern deklariert.**

| # | Datei:Zeile | Versatz laut Kommentar |
|---|---|---|
| 1 | `include/uft/detect/mfm_detect.h:295` | — |
| 2 | `include/uft/formats/fat/uft_fat_bootsector.h:162` | 0x27 |
| 3 | `include/uft/formats/fat/uft_fat_bootsector.h:182` | 0x43 |
| 4 | `include/uft/formats/fat/uft_fat_bootsector.h:238` | — |
| 5 | `include/uft/formats/uft_fat12.h:172` | — |
| 6 | `include/uft/formats/uft_fdi.h:128` | — |
| 7 | `include/uft/fs/uft_fat12.h:185` | 0x27 |
| 8 | `include/uft/fs/uft_fat32.h:113` | 0x43 |
| 9 | `include/uft/fs/uft_fat32.h:171` | — |
| 10 | `include/uft/uft_fat12.h:161` | — |
| 11 | `include/uft/xdf/uft_xdf_pxdf.h:149` | — |
| 12 | `include/uft/xdf/uft_xdf_pxdf.h:173` | — |

**Dazu ein dreizehnter Ort mit einem ANDEREN Namen:**
`src/fs/uft_fat12.c:221` legt denselben Wert in `v->serial` ab.

**Das ist `MF-1177` in der Metadatenschicht: eine Größe, zwölf Stellen** —
und die Lehre dort lautet, dass die Stellen driften und die Abweichung
danach wie ein Fehler in den DATEN aussieht. Bei `volume_serial` ist noch
nichts gedriftet (alle sind `uint32_t`), aber die Voraussetzung dafür liegt
vor: zwei Namen, zwei dokumentierte Versätze (0x27 für FAT12/16, 0x43 für
FAT32), und keine gemeinsame Rechnung.

**Ein Einbau von `uft_vsn_*` sollte deshalb NICHT als dreizehnte Kopie
kommen**, sondern als die eine Stelle, die aus vier Bytes eine Aussage
macht — und die zwölf Felder sollten sie rufen.

---

## 5. Die fünf Fragen (a, Teil 2)

### 5.1 „wo können die formate verbessert werden"

Nicht die Formate — die **Aussage über sie**. Der Baum liest die VSN und
zeigt sie als acht Hexziffern (`src/detect/mfm/mfm_detect.c:1661`,
`fat_format_serial()`). Dass darin Monat, Tag, Stunde, Minute, Sekunde und
Jahr stecken, sagt er nirgends.

### 5.2 „ist es auf andere formate übertragbar"

**Teilweise, und die Grenze ist scharf.** Die Rechenvorschrift gehört DOS;
sie gilt für FAT-formatierte Medien, unabhängig von der Größe — also auch
für FAT32 (Versatz 0x43) und jedes FAT-Diskettenformat. Sie gilt **nicht**
für Amiga, CBM, Apple oder CP/M. Der übertragbare Teil ist die **Idee**: ein
Formatierer hinterlässt Spuren, aus denen sich der Zeitpunkt rekonstruieren
lässt.

### 5.3 „welche einstellungen fehlen noch"

Eine: die **Bytefolge** als benannte Wahl statt als stille Annahme. Die
Zulieferung hat sie bereits richtig gebaut — `uft_vsn_from_bytes(b, order)`
nimmt die Anordnung als Argument, statt eine zu unterstellen. Das ist die
Bauform, die `logical` (MF-1032) und `posix` (MF-1034) nachträglich
gekostet haben.

### 5.4 „was habe wir noch nicht"

**Die Datierung selbst.** Vier Symbole, im Baum 0 Treffer. Und mit ihr eine
forensische Aussage, die der Baum bisher nicht treffen kann: *diese
Diskette wurde am … formatiert* — belegt aus der Diskette, nicht aus einem
Dateisystem-Zeitstempel des Wirtsrechners.

### 5.5 „brauch es eine HAL-Erweiterungen"

**Nein.** Die VSN steht im Bootsektor; wer den lesen kann, kann sie lesen.

---

## 6. Disposition

| Teil | Urteil | Begründung |
|---|---|---|
| `uft_vsn.{c,h}` | **REGISTER** mit Vorbedingung | 4 Symbole neu; Bytefolge zuerst messen (§3) |
| `uft_image_meta.h` | **PARTIAL** | überschneidet sich mit zwölf vorhandenen Feldern (§4) |
| `test_meta.c` | **PARTIAL** | prüft die eigene Rechnung; der Prüfvektor ist der wertvolle Teil |
| `UFT-NN_Metadaten_Datierung.md` | **REFERENCE** | Belegkette; benennt die offene Frage selbst |

**Lizenz (e):** alle vier Quelldateien tragen
`SPDX-License-Identifier: GPL-2.0-or-later` — die Projektlizenz. **Die
Aufnahme dieses Postens führte sie als offene Frage; gemessen ist sie
geklärt.** Offen bleibt allein, ob die zitierten Quellen über die
Tatsachenbehauptung hinaus etwas Schutzfähiges beitragen — nach `MF-695`
ist das der Kanal *Spec*, und er steht offen.

**Kein Byte dieser Zulieferung ist nach `src/`, `include/` oder `tests/`
geschrieben worden (f).**

---

## 7. Vorschläge für `docs/OPEN_ITEMS.md`

1. **Die VSN-Bytefolge ist im Baum nicht geprüft, und der Prüfstein liegt
   daneben** — `tests/test_win98_fdb.c:279` hält `0x27156C21` aus einem
   echten Abbild fest; niemand hat die vier Rohbytes danebengelegt (§3).
   *Kennzahl:* keine der vier.
2. **`volume_serial` ist in zwölf Headern deklariert, an einer dreizehnten
   Stelle heißt dieselbe Größe `serial`** (§4). `MF-1177` in der
   Metadatenschicht: eine Größe, zwölf Stellen, keine gemeinsame Rechnung.
   *Kennzahl:* keine der vier.
3. **Die VSN wird gespeichert und nie gedeutet** — der Baum zeigt acht
   Hexziffern und sagt nicht, dass ein Zeitstempel darin steckt (§5.1).
   *Kennzahl:* keine der vier.

---

## 8. Quellen

* Zulieferung: `neue-ideen/Metadaten.zip` — `uft_vsn.c`, `uft_vsn.h`,
  `uft_image_meta.h`, `test_meta.c`, Bericht
  `UFT-NN_Metadaten_Datierung.md` (487 Zeilen); alle Quelldateien
  `SPDX-License-Identifier: GPL-2.0-or-later`
* Prüfvektor: Wilsons durchgerechnetes Beispiel, **hier unabhängig
  nachgerechnet** (§1)
* Baum: `origin/main` = `70a940af`
* Gegengehalten: `MF-1177` (eine Größe, eine Rechnung), `MF-1032`/`MF-1034`
  (Anordnung als Argument statt als Annahme), `MF-695`, `MF-869`
