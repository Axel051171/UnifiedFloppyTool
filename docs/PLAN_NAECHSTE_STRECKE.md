# Die nächste Strecke — ein Plan mit gemessener Wirbelsäule

**Stand 2026-09-02 (MF-786).** Jede Zahl hier ist gemessen, nicht
geschätzt; wo eine fehlt, steht das da.

---

## Der eine Satz

> **Die Einfrier-Regel ist ein Format davon entfernt, sich selbst
> aufzuheben.**

Das ist keine Rhetorik, sondern der Wortlaut von `MF-363/498`. Das
Moratorium endet, wenn **ATR, D64, ADF, FDI und NFD-r0** auf T1/T1b
stehen. Gemessen am 2026-09-02:

| Format | Stufe | |
|---|---|---|
| `fdi` | **T1** | ✔ |
| `adf` | **T1b** | ✔ |
| `atr` | **T1b** | ✔ |
| `d64` | **T1b** | ✔ |
| **`nfd`** | **T2** | ✘ — der letzte |

Danach gilt nicht „alles erlaubt", sondern **1:2**: ein neues Format
kostet zwei Hebungen. Das ist ein Regimewechsel, keine Kennzahl — und er
hängt an **einer** Datei.

---

## Wo wir stehen, in vier Zahlen

| Kennzahl | Stand | Bewegung diese Sitzung |
|---|---|---|
| **ungeprüfte Formate (T3)** | **37** von 88 | **50 → 37** (dreizehn Hebungen; `edsk` in MF-796) |
| angebotene Wandlungspfade | 16 Matrix-Einträge, 14 angeboten | unverändert |
| leckende Tests | 0 | gehalten |
| Bench-Alter je Controller | kein Gerät (MF-310) | unverändert |

Dazu, nicht als Kennzahl geführt, aber tragend: **Oracles 3 von 14
verfügbar** (war 0 von 10). `gw` 1.23, `samdisk` 4.0, `hxcfe` 2.16.13.

**Ein Nebenbefund, der zur Sache gehört:** `P3-1` — ausgerechnet der
Punkt, der das Moratorium führt — sagt bis heute *„57 Formate auf T3"*.
Gemessen sind es 39. Die Zahl, die eine Regel trägt, war um 18 daneben.
Das ist dieselbe Drift-Klasse, die dieser Baum bei Wandlungspfaden
dreimal gefunden hat, diesmal an der Stelle, die am meisten kostet.

---

## Was blockiert, gemessen statt vermutet

### B1 — `nfd` ist T2, weil ihm ein fremd erzeugtes Abbild fehlt

Es hat einen Test (`test_nfd_r0`) **und** eine benannte Spec-Quelle
(pc98.org, d88split). Was fehlt, ist allein ein **cross-tool-Abbild** —
genau der Eintrag, den `origin: cross-tool` im Manifest verlangt.

NFD ist ein **Container mit Kopf**, fällt also unter B2.

### B2 — Container lassen sich nicht sauber erzeugen

Gemessen (MF-785): SAMdisk und hxcfe **raten die Eingangsgeometrie**
(*„input format guessed from file size — please check"*) und legen den
Inhalt neu aus. Fünf Kandidaten fielen durch; die Prüfmarke „Sektor *n*
trägt seine Nummer" überlebt es nicht.

**Das ist eine Verfahrensfrage, keine Werkzeugfrage.** Beide Werkzeuge
nehmen Geometrieangaben entgegen — sie wurden nur nicht genutzt.

> **BERICHTIGUNG (MF-794) — die Prämisse trägt nicht.** Der Satz oben
> war plausibel und falsch. Gemessen: mit expliziter Geometrie
> (`SAMdisk copy -c80 -s10 -z2 -b1`) kommt für `sad` eine Datei heraus,
> die mit der geratenen **byteidentisch** ist. Das Raten war folgenlos.
>
> Die Ursache lag bei der **Erwartung**. Der Korpus-Test verglich gegen
> eine *lineare* Anordnung; SAD legt aber **kopf-dur** ab — alle
> Zylinder von Seite 0, dann alle von Seite 1. Zwei unabhängige Hände
> sagen dasselbe: SAMdisk schreibt so, und hxcfe 2.16.13 liest dieselbe
> Datei zu einem Rohabbild zurück, das mit der Quelle byteidentisch
> ist. Für `mgt` und `edsk` schließt sich derselbe Rundlauf ebenfalls
> byteidentisch.
>
> **Die Werkzeuge legen den Inhalt nach dem Zielformat ab. Sie
> zerstören nichts.**
>
> Was der Differenzlauf stattdessen fand, ist ein echter Lesefehler in
> UFT: `uft_sad.c` rechnete zylinder-dur und las **158 von 160 Spuren
> an der falschen Stelle** — bei einem Format, das auf **T1b** stand.
> Richtig lagen nur (0,0) und (79,1), wo beide Formeln denselben Index
> ergeben, und genau (0,0) hatte der Test geprüft.
>
> Die Lehre ist nicht „Geometrie angeben". Sie ist: **ein Werkzeug, das
> nicht das liefert, was man erwartet, ist erst der zweite Verdächtige.**

### B3 — FAT12 ist gegen sich selbst geprüft

`src/fs/uft_fat12.c` steht auf **FS-T1** mit dem Vermerk *„alle Tests
bauen ihre Eingabe selbst — geprüft gegen den eigenen Erzeuger."* Das ist
die Selbstkonsistenz-Falle **eine Schicht unter** den Format-Plugins, und
sie betrifft **jedes** FAT12-tragende Format zugleich.

Der geplante Weg war blueMSX + Nextor (Beschaffung + Klick-Sitzung). In
der Sammlung des Eigentümers liegt aber **mtools** — ein
Befehlszeilen-Formatierer.

### B4 — was nur der Eigentümer kann

| | wartet auf |
|---|---|
| **P3-12** | ein Hardwaretag (deckt P6/P3-7, P1 und P7 zugleich) |
| **P3-8/P3-11** | Lizenz-Entscheidung zu den Aminet-Paketen |
| X-Copy-Sitzung | Amiga Forever + WinUAE |

Alle drei sind **eingetragen und begründet**; keiner blockiert die
Strecke unten.

---

## Die Strecke, nach Hebelwirkung geordnet

### Schritt 1 — `P3-1` berichtigen *(Minuten)*

Die Zahl, die das Moratorium trägt, ist um 18 falsch. Solange sie
falsch ist, kann niemand sehen, wie nah das Ende ist.

**Kennzahl:** keine. **Wirkung:** die Regel wird lesbar.

### Schritt 2 — das Container-Verfahren reparieren *(B2)*

Nicht mehr Formate, sondern die **Geometrie explizit angeben**. Der
Rotbeweis liegt schon vor: dieselben fünf Kandidaten müssen dann die
Reihenfolgeprüfung bestehen, die sie eben nicht bestanden haben.

**Kennzahl:** ungeprüfte Formate ↓. **Warum zuerst:** es entsperrt B1
*und* die restlichen Container in einem Zug.

### Schritt 3 — `nfd` auf T1b *(B1)*

Das letzte der fünf. Braucht Schritt 2.

**Kennzahl:** ungeprüfte Formate ↓ — **und** das Ende des Moratoriums.

### Schritt 4 — FAT12 mit einer fremden Hand *(B3)*

`mtools` formatiert von der Befehlszeile, reproduzierbar. Zwei Fragen
entscheiden: baut es hier, und **schreibt es Bootcode oder nur BPB?**
Die zweite ist die Frage aus MF-779/781 — ein Abbild mit fremdem
Bootcode gehört nach `tests/corpus/` (gitignored), nicht nach
`corpus_free/`.

**Kennzahl:** ungeprüfte Formate ↓ (über die FS-Stufe), und zwar für
**alle** FAT12-tragenden Formate zugleich.

### Schritt 5 — `P3-10`, die dritte Fundstelle *(Innendienst)*

43 CMake-Blöcke **plus** `tests/differential/conftest.py` pflegen
denselben Quellensatz von Hand. Dreimal in dieser Sitzung hat das den
Bau gebrochen; der Hook fängt es seit MF-773, aber er behandelt das
Symptom.

**Kennzahl:** keine direkt — er schützt alle vier davor, auf einem Baum
gemessen zu werden, der nicht baut.

---

## Was wir **nicht** tun

* **Keine neuen Format-Plugins**, bis Schritt 3 steht. Danach 1:2.
* **Kein Erzwingen halber Belege.** In dieser Sitzung wurden fünf
  Container-Einträge **zurückgenommen**, obwohl sie das Tor formal
  bestanden hätten. Ein halb belegtes T1b ist schlimmer als ein
  ehrliches T3: es sieht aus wie Fortschritt.
* **Keine Hardware-Vorschläge** (MF-310).
* **Keine Sichtung ohne Kennzahl.** Ein Fund, der keine Zahl bewegt,
  ist Fundus — eingetragen, nicht eingeplant.

---

## Die drei Lehren dieser Sitzung, damit sie nicht verlorengehen

**Größengleichheit ist keine Geometriegleichheit.** `v9t9` und `cpm`
liefen byteidentisch durch den Rundlauf und waren trotzdem falsch
gepaart — gleiche Dateigröße, andere Sektoraufteilung. Beide hätten das
Tor formal bestanden.

**Ein Skip, der einen Bruch verdeckt, ist schlimmer als ein roter
Test.** Die gw-Paritätstests meldeten jahrelang „6 skipped"; dahinter lag
ein Ziel, das sich nicht einmal übersetzen ließ.

**Eine Herkunftsangabe ist eine Behauptung wie jede Zahl.** „Das ist
X-Copys Code 3" war abgeleitet, nicht gemessen — und widerlegt. Ebenso
hätte `openMSX` als Oracle für `msx_disk` nur bestätigt, wovon UFT
abgeleitet ist.

---

## Der nächste Griff

**Schritt 1**, dann **Schritt 2**. Schritt 4 wartet auf das
Scout-Gutachten zu `mtools`; Schritt 3 wartet auf Schritt 2.
