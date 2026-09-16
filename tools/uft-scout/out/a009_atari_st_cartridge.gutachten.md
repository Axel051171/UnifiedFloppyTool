# A-009 · Gutachten `UFT_Atari_ST_Cartridge_Detection.zip`

**Stand:** 2026-09-16 · **Posten:** `A-009` · **Kanal:** Nachbau (selbsterklärt, Quelle **unbenannt**)
**Prüfstand:** `origin/main` = `70a940af`

---

## Ergebnis in drei Sätzen

Die Zulieferung hat **architektonisch recht**: eine Nicht-Disketten-Absage
gehört in den **Prüfpfad**, und genau dort setzt sie an — `DiskImageValidator`
ist mit **8 produktiven GUI-Dateien** der erreichbarste Baustein dieser ganzen
Begutachtungsreihe, und die Änderung dort ist **rein additiv** (+58/−0).
**Ihre mitgelieferte `src/mainwindow.cpp` würde dagegen 50 Zeilen lebenden
Code löschen und damit MF-1194 rückgängig machen** — vier GUI-Reiter, die
erst vor kurzem nach einer Handprobe verdrahtet wurden. Und der Kanal trägt
nicht: die Quelle ist selbst als „öffentlich beschrieben" bezeichnet, aber
**nicht benannt**, und keine der drei neuen Dateien hat eine Lizenzzeile.

**Empfehlung: PARTIAL.** Die Idee registrieren, die drei neuen Dateien
zurückstellen bis die Quelle benannt ist, die sechs überlappenden Dateien
**nicht** übernehmen.

---

## 1. Die Grundfrage, entschieden (b)

> *Gehört eine Nicht-Disketten-Absage in `src/formats/` oder in den Prüfpfad?*

**In den Prüfpfad — und das ist gemessen, nicht gemeint.**

| Ort | Erreichbarkeit (gemessen, `git grep`, rc 0) |
|---|---|
| `DiskImageValidator` | **8 produktive GUI-Dateien**: `decodejob.cpp` (5), `explorertab.cpp` (3), `forensictab.cpp` (2), `mainwindow.cpp` (3), `nibbletab.cpp` (3), `toolstab.cpp` (8), `xcopytab.cpp` (3), dazu `.pro` und ein Test |
| `src/formats/atari/uft_atari.c` | **kein Plugin-Anschluss** — 0 Treffer für `DSK_PLUGIN`, `uft_format_plugin_t`, `uft_register…`, `.probe` |

**Und der Baum liefert den Präzedenzfall gleich mit, in beide Richtungen:**

* Atari-**7800**-Cartridges liegen bereits in `src/formats/atari/` —
  `a78_cart_type_t`, `A78_*`-Konstanten, Kopffelder, verteilt auf
  `include/uft/formats/atari/uft_atari.h` (24 Nennungen),
  `src/formats/atari/uft_atari.c` (35) und `tests/test_atari.c` (7).
* Sie sind **unerreichbar**: die Datei registriert kein Plugin. Struktur und
  Test ohne Tür — die Klasse `P3-204`/MF-930.

Wer die ST-Cartridge-Erkennung nur nach `src/formats/atari/` legte, bekäme
denselben Zustand ein zweites Mal. **Die Zulieferung tut das nicht** — ihr
`src/disk_image_validator.cpp` ruft den Erkenner wirklich:

```
:7   #include "uft/formats/atari/uft_atari_st_cartridge.h"
:55  // A cartridge is deliberately not accepted as a disk image.
:67  info.isNonDiskImage = true;
:68  info.formatName = QStringLiteral("Atari ST Cartridge");
:71  "Atari-ST-Cartridge erkannt – kein Diskettenimage"
```

**Damit ist D2 auf ihrer Seite erfüllt**, und die Bauform stimmt mit
`UFT_CAPS_OS_VOLUME` (MF-1176) überein: *der Eintrag ist eine Absage, keine
Zusage.*

**Nebenbefund, der dem Baum gehört:** `docs/SCOPE_DECISION_NON_FLOPPY.md`
führt die Eigentümer-Entscheidung von 2026-05-25 (Option C, ausgeführt
2026-08-19 als MF-271), mit der `src/cart7/` — Atari-7800-Cartridges —
gelöscht wurde. Die **A78-Strukturen in `src/formats/atari/` haben das
überlebt**. Das ist kein Widerspruch zur Entscheidung (gelöscht wurde ein
anderer Teilbaum), aber es ist ein Rest, den niemand geführt hat, und er ist
unerreichbar. Vorschlag 2 unten.

---

## 2. Der schwerste Befund: die mitgelieferte `mainwindow.cpp` (d)

| Datei | Zulieferung gegen Baum |
|---|---|
| `src/disk_image_validator.cpp` | **+58 / −0** — rein additiv |
| `src/disk_image_validator.h` | **+2 / −0** — additiv (`isNonDiskImage`) |
| `src/explorertab.cpp` | +11 / **−2** |
| **`src/mainwindow.cpp`** | +17 / **−50** |
| `tests/CMakeLists.txt` | **341 513** B gegen **361 386** (−19 873) |
| `UnifiedFloppyTool.pro` | **81 746** B gegen **83 293** (−1 547) |

**Die 50 Zeilen in `mainwindow.cpp` sind MF-1194.** Entfernt würden unter
anderem:

```c
/* MF-1194: diese vier standen mit SOURCES und FORMS im .pro, wurden uebersetzt
 * und ins Programm gelinkt — und niemand rief je `new` darauf. 2381 Zeilen
 * fertige Oberflaeche ohne Tuer, Klasse P3-204/MF-930. */
#include "protectiontab.h"  #include "forensictab.h"
#include "nibbletab.h"      #include "xcopytab.h"
…
ProtectionTab* protectionTab = new ProtectionTab();
ForensicTab*   forensicTab   = new ForensicTab();
```

samt dem Messprotokoll, das MF-1194 **vor** dem Verdrahten angelegt hat
(11 Urteilstexte in `forensictab`, alle echte Ternäre gegen verglichene Bytes;
0 in `protectiontab` und `nibbletab`; `xcopytab` ehrlicher Stummel nach
MF-012; Tor 34 und 35 baumweit 0, ausdrücklich „keine Entwarnung").

**Eine Dateiübernahme würde vier GUI-Reiter still wieder abhängen** und die
Begründung dafür gleich mitlöschen. Das ist derselbe Mechanismus wie bei
A-008 (dort ein gemessener Kommentar, hier lebende Funktionalität) — nur
teurer.

**Grund ist banal und entlastet die Zulieferung:** ihre Dateien sind schlicht
**älter** als der Baum. Sie wurde gegen einen Stand vor MF-1194 gebaut. Das
ist kein Vorwurf, sondern genau der Grund, warum Baudateien und lebender
GUI-Code **nie** als Datei übernommen werden dürfen — **drittes Paket in
Folge mit demselben Muster** (A-007 hatte es angekündigt, A-008 und A-009
haben es gemessen).

---

## 3. Der Kanal trägt nicht (c)

`docs/ATARI_ST_CARTRIDGE_DETECTION.md` (1413 B) sagt:

> *„Die Datenstruktur wurde anhand der oeffentlich beschriebenen
> Atari-ST-Cartridge-Header neu implementiert. Es wurde kein Assembler-,
> Loader- oder Entpacker-Code der Referenzseite uebernommen."*

**Welche Seite, welche Fassung, welche Stelle — steht nicht da.** Nach
`MF-636` ist eine Attribution eine **rechtliche Aussage**: wer eigenständig
implementiert und fremde Doku gelesen hat, schreibt, *nach welcher*.

Dazu gemessen: **keine der drei neuen Dateien trägt eine SPDX-, Lizenz- oder
Copyright-Zeile** (0 Treffer für `SPDX`, `Licen`, `Copyright` in
`uft_atari_st_cartridge.c`, `.h` und `test_atari_st_cartridge.c`). Bei A-008
lag für jede Datei eine MIT-Zeile vor; hier liegt gar keine.

**Das ist eine Nachforderung, keine Ergänzung.** Zwei Angaben genügen:
1. die Quelle mit Fassung und Stelle (Atari-ST-Cartridge-Kopf: `0x00FA0000`
   als Startkennung, die Struktur dahinter),
2. eine SPDX-Zeile je Datei.

Bis dahin ist der Kanal **Fundus**, nicht Nachbau — nach `MF-695` heißt das
*benannt wartend*, nicht verfallen.

**Zugunsten der Zulieferung gemessen:** die Kennung `0xABCDEF42` steht
korrekt **einmal** als `UFT_STCART_MAGIC` im Header, nicht doppelt in der
`.c` — das ist D3 richtig angewandt, und der erste Blick auf die `.c` täuscht
dort.

---

## 4. Die fünf Fragen (a)

### 4.1 „wo können die formate verbessert werden"

Nicht die Formate — die **Erkennung davor**. Gemessen trägt `DiskImageInfo`
heute `isValid` und `isFluxFormat` (`src/disk_image_validator.h:38-39`); ein
Objekt, das **erkannt und trotzdem abgelehnt** wird, hat kein Feld. Es fällt
damit in denselben Topf wie „unbekannt" — die Klasse `MF-980`/D6:
*„unbekannt" ist nicht „widerspricht".*

`isNonDiskImage` ist genau dieses fehlende dritte Wort, und es ist billig
(+2 Zeilen im Header, additiv).

### 4.2 „ist es auf andere formate übertragbar"

**Ja, und breiter als die Zulieferung es nutzt.** Dieselbe Absage-Bauform
passt auf jedes Nicht-Disketten-Objekt, das heute in einem Dateidialog landen
kann: C64-Cartridges (`CRT`), Bandabbilder (`TAP`, `T64`, `TZX`),
Programmdateien (`PRG`, `P00`) — CLAUDE.md führt sie als „unterstützt", und
`P3-144` hat gemessen, was passiert, wenn ein Schutzerkenner auf eine `.prg`
losgelassen wird: er lief „auf ALLES, was der Benutzer geladen hatte".

**Der Gewinn liegt also nicht bei Atari ST, sondern im Feld selbst.**

### 4.3 „welche einstellungen fehlen noch"

Eine, und sie folgt aus 4.1: die Oberfläche braucht einen Weg, ein
`isNonDiskImage`-Objekt **anzuzeigen statt zu verschlucken**. Die Zulieferung
löst das in `explorertab.cpp` (+11/−2) — der Teil ist ansehenswert, aber als
**Zeilendiff**, nicht als Datei.

### 4.4 „was habe wir noch nicht"

Das Feld `isNonDiskImage` und den Begriff dahinter. Gemessen: `isNonDiskImage`,
`nonDisk`, `STCART` und `0x00FA0000` kommen im ganzen Baum **nur in
`.claude/AUFGABEN.md`** vor — also ausschließlich in meiner eigenen
Aufnahmezeile zu diesem Posten.

### 4.5 „brauch es eine HAL-Erweiterungen"

**Nein.** Ein Cartridge-ROM kommt als Datei, nicht von einem Controller. Die
HAL ist nicht berührt.

---

## 5. „was kannst du besser machen?"

1. **Die Absage von der Erkennung trennen.** Das Feld `isNonDiskImage` und
   die Anzeige dafür sind **unabhängig** von Atari ST wertvoll (4.2) und
   hängen an keiner unbenannten Quelle. Sie könnten zuerst kommen — mit einem
   Erkenner, der nur sagt „das ist kein Diskettenabbild", ohne zu behaupten,
   *welches* Cartridge es ist.
2. **Die Quelle nachfordern, nicht ergänzen.** Eine erfundene Quellenangabe
   wäre schlimmer als keine.
3. **Nie die Datei, immer die Zeilen.** §2 zeigt, was hier auf dem Spiel
   steht: vier verdrahtete GUI-Reiter.

---

## 6. Disposition

| Teil | Urteil | Begründung |
|---|---|---|
| `src/disk_image_validator.{cpp,h}` (+58/−0, +2/−0) | **REGISTER** als Vorlage | rein additiv, erreichbarster Pfad, D2 erfüllt |
| `src/formats/atari/uft_atari_st_cartridge.{c,h}` | **BLOCKED** | keine Lizenzzeile, Quelle unbenannt (§3) |
| `tests/test_atari_st_cartridge.c` | **BLOCKED** | synthetisch; prüft die eigene Tafel |
| `src/explorertab.cpp` (+11/−2) | **PARTIAL** | als Zeilendiff ansehenswert, nicht als Datei |
| **`src/mainwindow.cpp` (+17/−50)** | **BLOCKED** | macht MF-1194 rückgängig (§2) |
| `UnifiedFloppyTool.pro`, `tests/CMakeLists.txt` | **nicht übernehmen** | älter als der Baum |
| `docs/ATARI_ST_CARTRIDGE_DETECTION.md` | **REFERENCE** mit Nachforderung | Quelle fehlt |

**Kein Byte dieser Zulieferung ist nach `src/`, `include/` oder `tests/`
geschrieben worden (e).**

---

## 7. Vorschläge für `docs/OPEN_ITEMS.md`

1. **Ein erkanntes Nicht-Diskettenabbild ist nicht von „unbekannt" zu
   unterscheiden** — `DiskImageInfo` hat kein drittes Wort (§4.1). Klasse
   `MF-980`/D6, auf dem erreichbarsten GUI-Pfad (8 Dateien). *Kennzahl:*
   keine der vier; Ehrlichkeit.
2. **Die A78-Cartridge-Strukturen in `src/formats/atari/` sind unerreichbar**
   — 24+35+7 Nennungen, Test vorhanden, **kein Plugin-Anschluss** (§1).
   Klasse `P3-204`. *Kennzahl:* keine der vier.
3. **Drittes Paket in Folge liefert ältere Baudateien als „aktuelle
   Verdrahtung" mit** (A-007 angekündigt, A-008 und A-009 gemessen). Bei
   A-009 kostet eine Übernahme vier verdrahtete GUI-Reiter. *Kennzahl:*
   keine der vier; es ist eine **Verfahrensregel** für alle künftigen
   Zulieferungen.

---

## 8. Quellen

* Zulieferung: `neue-ideen/UFT_Atari_ST_Cartridge_Detection.zip`, 10 Dateien,
  drei davon neu (24 559 B Quelltext gesamt)
* Baum: `origin/main` = `70a940af`
* Gegengehalten: `MF-636` (Attribution ist eine rechtliche Aussage),
  `MF-695` (stärkster legaler Kanal), `MF-1176` (Absage statt Zusage),
  `MF-1194` (die vier verdrahteten Reiter), `MF-271` /
  `docs/SCOPE_DECISION_NON_FLOPPY.md`, `P3-144`, `P3-204`
