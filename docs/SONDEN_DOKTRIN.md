# Sonden-Doktrin — was darf eine Sonde behaupten, und wer gewinnt bei Gleichstand?

**Eigentümer-Entscheidung vom 2026-09-15 (MF-1153). Verbindlich.**
Bei Konflikt zwischen dieser Seite und einer Sonde gewinnt diese Seite.

---

## Warum es diese Seite gibt

Dieselbe Frage wurde je Fall neu beantwortet. Der Eigentümer hat
gezählt, was in `docs/OPEN_ITEMS.md` offen stand, und **acht der
dreiundzwanzig offenen Punkte waren eine Frage:**

| Punkt | was er eigentlich fragt |
|---|---|
| P3-406 | MYZ80 gewinnt gegen jede frische CP/M-Diskette — verdrängt breiter engeren? |
| P3-402 | Was bei `tied > 1`? |
| P3-401 | Wie weit darf eine Sonde sehen? |
| P3-405 | Ist 85 für ein Format ohne Kennung zulässig? |
| P3-403 | Flach und gleichförmig — mechanisch T1b, sachlich unbelegbar |
| P3-393 | `pdp`/`hardsector` teilen eine Größe |
| P3-394 | `v9t9` bleibt W0 wegen zweideutiger Größe |
| P3-392 | Zwei Micropolis-Leser, uneins über die Sektorgröße |

**Und der Beleg, dass es Kasuistik war, kommt aus einem einzigen Tag:**
MF-1151 hat `dmk` von 100 auf 75 gesenkt, weil das Format keine Kennung
hat — und MF-1152 hat `ssd` im selben Lauf bei 85 gelassen, obwohl
Acorn DFS ebenso keine hat. Zwei Entscheidungen, zwei Begründungen.
Jede Einzelentscheidung erzeugt den nächsten Sonderfall, und **die Zahl
im Code trägt keine Begründung.**

Deshalb wird die Frage hier **einmal** beantwortet, und die Konfidenz
wird **abgeleitet statt vergeben**.

---

## Die Leiter

Eine Konfidenz ist die **Summe belegter Merkmale**, nie ein Handwert:

| Beleg | Wert | was er heißt |
|---|---|---|
| **Kennung** an fester Position, formatspezifisch | **+50** | eine Zeichenfolge oder ein Zahlenwert, den nur dieses Format dort hat |
| **Selbstkonsistenz** | **+25** | der Kopf sagt eine Größe, und die Datei hat sie — die Datei bestätigt sich selbst |
| **Struktur an berechneter Stelle** | **+15** | Verzeichnis, BAM, Katalog, Spurtabelle liegt dort, wo die Rechnung sie erwartet |
| **Geometrie plausibel** | **+10** | Zylinder, Köpfe, Sektoren, Sektorgröße ergeben zusammen ein Laufwerk, das es gab |
| **Größe allein** | **0** | nie hinreichend |

Umgesetzt in `uft_probe_konfidenz()`
(`include/uft/uft_format_plugin.h`); die Flags heißen
`UFT_BELEG_KENNUNG`, `UFT_BELEG_SELBSTKONSISTENZ`,
`UFT_BELEG_STRUKTUR`, `UFT_BELEG_GEOMETRIE`.

### Die Klemme, und warum sie eine Klemme ist

Die Regel lautet: **ohne Kennung ist die Obergrenze 45.** Die Summe der
drei übrigen Belege ist aber 25 + 15 + 10 = **50**. Das ist kein
Widerspruch, sondern eine Klemme *nach* der Summe:

```c
if (!(belege & UFT_BELEG_KENNUNG) && k > 45) k = 45;
```

**Damit ist die Regel scharf statt beinahe:** kein Weg ohne Kennung
erreicht je das Strukturband (50), und die Klemme sagt es an EINER
Stelle statt in jeder Sonde. Zur Wahl stand, statt der Klemme die
Selbstkonsistenz auf +20 zu setzen; die Klemme ist gewählt, weil sie
die Regel *ausspricht* und die Gewichte unberührt lässt.

### Was die Leiter für die Bänder bedeutet

Die vier Bänder aus MF-729 bleiben (`0–29 kein Anspruch · 30–49 nur die
Größe · 50–79 Struktur gelesen · 80–100 Merkmal getroffen`), aber sie
sind jetzt **erreichbar statt behauptet**:

| Belege | Konfidenz | Band |
|---|---|---|
| nichts, nur die Größe | 0 | kein Anspruch |
| Geometrie | 10 | kein Anspruch |
| Struktur + Geometrie | 25 | kein Anspruch |
| Selbstkonsistenz + Struktur + Geometrie | **45** (geklemmt von 50) | nur die Größe |
| Kennung | 50 | Struktur gelesen |
| Kennung + Selbstkonsistenz | 75 | Struktur gelesen |
| Kennung + Struktur + Geometrie | 75 | Struktur gelesen |
| Kennung + Selbstkonsistenz + Geometrie | 85 | Merkmal getroffen |
| Kennung + Selbstkonsistenz + Struktur | 90 | Merkmal getroffen |
| alle vier | 100 | Merkmal getroffen |

**Das Merkmalsband verlangt damit eine Kennung UND zwei weitere
Belege.** Ein Format mit Kennung und nichts sonst landet bei 50 — das
ist Absicht: eine Kennung sagt „diese Bytes öffnen ein Tor", nicht
„diese Datei ist es".

---

## Die fünf Regeln

### 1. Ohne Kennung ist die Obergrenze 45

Damit ist **P3-405** beantwortet. `ssd` kann 85 nicht erreichen, weil
Acorn DFS keine Kennung hat — und `dmk` ebenso nicht, was MF-1151
schon einzeln entschieden hatte. Die Regel macht aus zwei Einzelfällen
einen Satz.

### 2. Bei Gleichstand gewinnt der ENGERE Anspruch, nie der breitere

Damit ist **P3-406** beantwortet, und die Richtung ist die richtige:
**wer mehr behauptet, muss mehr belegen.** MYZ80 erklärt 256 von
737 280 Byte, `cpm` erklärt alle — also gewinnt `cpm`, obwohl MYZ80s
Zahl höher war.

„Enger" heißt: **wie viel der Datei erklärt der Anspruch?** Eine Sonde,
die eine Gesamtgröße verlangt, erklärt die ganze Datei; eine, die 256
Byte am Anfang prüft und den Rest offen lässt, erklärt 256 Byte.

### 2b. Wo der Pfad einen Namen hat, verengt die Endung — und sonst nichts

Eigentümer-Entscheidung vom 2026-09-19, wörtlich: **„die Endung als
engeren Anspruch nehmen, dann weiter mit 2 und 3"**. Umgesetzt in
MF-1252 als `engerer_anspruch_durch_endung()`
(`src/core/uft_format_plugin.c`), gehalten von
`tests/test_sonde_sagt_ab_bei_gleichstand.c`.

**Diese Regel steht hier, weil sie bis MF-1258 NUR im Code stand.** Das
ist die Bauform aus `CLAUDE.md` §MF-1177 — eine Regel, zwei Stellen —
und sie war umso teurer, weil die verbindliche Fassung „enger" bereits
belegt hatte: als **Erklärungsumfang** (Regel 2). Die Endung ist kein
Erklärungsumfang. Sie ist ein zweiter, andersartiger Schritt, und ihn
unter denselben Namen zu stellen wäre genau die stille Doppelbedeutung,
gegen die dieses Dokument existiert.

**Was der Schritt tut:** unter den Plugins, die **bereits auf der
Spitzenkonfidenz gleichauf** liegen, wählt er das eine, dessen
`extensions` die Endung des Pfades beansprucht. Bleiben null oder
mehrere übrig, gilt weiter Regel 3.

**Was er nie tut, und das ist am Code festgenagelt:**

* **Er hebt nichts an.** `if (conf != spitzenkonfidenz) continue;` —
  ein schwächer belegtes Plugin kann über die Endung niemals ein
  stärker belegtes überholen.
* **Er wird nie zum Beleg.** Die Endung erhöht keine Konfidenz und
  erzeugt keinen Anspruch. Eine Endung allein öffnet nichts.
* **Er versteckt sich nicht.** `result` trägt die Messung unverändert,
  also weiterhin `tied > 1`, auch wenn die Endung entschieden hat. Wer
  ein Plugin **und** `tied > 1` sieht, weiß: hier hat der Name verengt,
  nicht die Evidenz.

**Der Vorbehalt gegen MF-444 bleibt gültig.** Dort steht „the name is
not evidence about the bytes" (`src/core/uft_probe_format_impl.c`), und
das gilt unverändert: die Endung ist hier kein Beleg **über die Bytes**,
sondern eine Auswahl unter Ansprüchen, die die Bytes schon gleichrangig
gemacht haben. Wer nur den Puffer hat und keinen Pfad, bekommt diesen
Schritt nicht — `uft_probe_buffer_format()` sagt bei Gleichstand ab.

**Und er ersetzt Regel 2 NICHT — gemessen, nicht angenommen.** Der
Prüffall aus `P3-439` ist `tests/corpus_free/cpmtools_cf2dd_720k.cpm`:
MYZ80 und `cpm` stehen dort seit MF-1182 gleichauf bei **25**, und die
Endung `.cpm` beansprucht nur `cpm` (`extensions = "cpm,dsk"` gegen
`"myz80,myz"`). Trotzdem rührt 2b den Fall nicht an, denn **Sieger ist
`MSX` mit 45 bei `tied` = 1** — der Gleichstand liegt gar nicht an der
Spitze. Das Maß für „wie viel der Datei erklärt der Anspruch" fehlt dem
Sondenvertrag weiterhin; **`P3-439` bleibt offen.**

### 3. Bleibt es gleich, gewinnt KEINER

Rückgabe **„mehrdeutig"** mit beiden Namen, nicht eine stille Wahl.
Damit sind **P3-402**, **P3-393** und **P3-394** beantwortet — und es
ist der Weg, den dieser Baum bei `logical` (MF-1032) und `cpm`
(MF-1039) schon geht, jetzt als Regel statt als Einzelfall.

### 4. Eine Sonde sieht nur ihren Puffer

Was dahinter liegt, prüft `open`. Damit ist **P3-401** beantwortet:
`d64`s BAM bei 0x16600 und `d81`s Kopf bei 0x61803 liegen hinter dem
65 536-Byte-Puffer, und dort prüft sie niemand — die Zweige bleiben
beschriftet stehen (MF-1147), aber sie sind kein Beleg und dürfen
keinen tragen.

### 5. Ein Format ohne belegbares Merkmal bekommt keine Sonde

Sondern eine **ausdrückliche Absage**. Damit ist **P3-403**
beantwortet, und es ist dasselbe Muster wie bei `rcpmfs` (MF-1035): wo
nichts zu erkennen ist, ist „ich erkenne es nicht" die einzige
ehrliche Antwort. `akai_s900` und `korg_dss1` sind flach und
gleichförmig — in ihrer Datei steht nichts, was eine Geometrie verrät
—, und P3-403 hat das gemessen: drei hxcfe-Zellströme unterscheiden
sich in bis zu 1 790 325 von 2 008 064 Byte und dekodieren **alle** zur
identischen flachen Datei zurück.

---

## Was das kostet, gemessen

Über alle **72** Korpusabbilder am echten `uft_disk_open()`-Pfad
gemessen (2026-09-15, `tests/test_oeffentliche_api_am_korpus.c`):

| heute | Dateien |
|---|---|
| Merkmal 80+ | 34 |
| Struktur 50–79 | 14 |
| Größe 30–49 | 24 |
| kein Anspruch <30 | 0 |

* **16** Abbilder haben schon heute einen exakten Gleichstand an der
  Spitze — den `uft_probe_buffer_format()` berechnet und wegwirft
  (P3-402).
* **24** haben einen Sieger unter 50.
* Die Vereinigung ist **26 von 72 (36 %)**: so viele würden nach
  Regel 3 „mehrdeutig" antworten statt einer stillen Wahl.

**Das ist kein Rückschritt, sondern ein Tausch.** Heute werden
**11 von 72** Abbildern mit einer ANDEREN Teilung geöffnet, als ihr
Format sagt (MF-1150), und 16 Gleichstände bleiben verschwiegen.
Danach sind es 26 Rückfragen und keine stille Falschaussage.

Und die 34 im Merkmalsband schrumpfen, sobald migriert wird: von den
44 Plugins, die heute ≥50 beanspruchen, findet
`scripts/audit_probe_magic.py` bei **8** eine Kennung als Zeichenkette.
Die Zahl ist eine **Untergrenze** — WOZ, SCP, IPF, ATR, MSA, 2IMG,
STX, PRI, DMS, TD0 und CQM haben nachweislich eine, das Skript sieht
sie nur nicht (es kennt `memcmp` gegen Literale am Dateianfang, sonst
nichts). **Die belastbare Zahl entsteht erst durch die Migration
selbst:** wer eine Konfidenz ableitet, muss seine Belege benennen, und
die Benennung IST die Messung.

---

## Wie migriert wird

Eine Sonde gilt als migriert, wenn sie ihre Konfidenz über
`uft_probe_konfidenz()` bildet und keine Zahl mehr selbst zuweist.

```c
/* vorher */
*confidence = 85;

/* nachher */
unsigned belege = UFT_BELEG_GEOMETRIE;
if (kennung_getroffen) belege |= UFT_BELEG_KENNUNG;
if (groesse_stimmt)    belege |= UFT_BELEG_SELBSTKONSISTENZ;
if (katalog_an_stelle) belege |= UFT_BELEG_STRUKTUR;
*confidence = uft_probe_konfidenz(belege);
```

Gehalten wird das von **`scripts/audit_sondendoktrin.py`** mit einer
**fallenden Grundlinie** — dieselbe Bauform wie Tor 57 (MF-883/930):
das Tor zählt die Sonden, die noch eine Zahl zuweisen, und die Zahl
darf nur sinken. Ein NEUES Format kann damit gar nicht mehr anders
anfangen, und das ist der eigentliche Zweck: **die Doktrin schließt
eine Quelle, statt eine Liste abzuarbeiten.**

---

## Was diese Seite NICHT entscheidet

* **Die Reihenfolge der Migration.** 137 Sonden, und jede braucht ein
  Urteil darüber, was sie wirklich liest. Das ist Arbeit, kein Streit.
* **Wann Regel 3 scharf gestellt wird.** „Mehrdeutig" auf dem
  Öffnungspfad ist eine Verhaltensänderung für 26 von 72 Abbildern und
  bekommt ihre eigene Messung und ihren eigenen MF-Eintrag. Bis dahin
  gilt die Leiter für die Konfidenz, nicht für die Auswahl.
* **Was „enger" im Zweifel heißt.** Regel 2 nennt das Maß (wie viel der
  Datei erklärt der Anspruch), aber kein Plugin liefert es heute. Das
  Feld dafür ist die nächste Vertragsfrage — und die erste, die ohne
  diese Seite gar nicht stellbar wäre.

**Gemessen MF-1182, und die Reihenfolge war umgekehrt zur Erwartung.**
Der Fall, an dem Regel 2 begründet wurde, war zu dem Zeitpunkt **kein
Gleichstand**: MYZ80 meldete 70 von Hand, `cpm` 40 von Hand — die Regel
griff also nie, und was fehlte, war nicht das Maß, sondern die
Migration. Seit beide über `uft_probe_konfidenz()` gehen, stehen beide
bei **25**, und *jetzt* steht der Gleichstand wirklich da. Das Feld ist
damit von einer Vorausschau zu einem gemessenen Bedarf geworden:
**P3-439** führt die Zahlen (MYZ80 prüft 256 von 737 280 Byte =
0,0347 %, `cpm` erklärt 100 %) und drei Zuschnitte mit Kosten.

**Und eine Abkürzung ist ausdrücklich verbaut.** Ein erster Entwurf von
MF-1182 hat die Selbstkonsistenz als Ersatzmaß genommen —
`cpm_waehle()` prüft die Gesamtgröße exakt, also „bestätigt sich die
Datei selbst". Das ist falsch, weil diese Seite den Beleg wörtlich an
eine Größenangabe **in** der Datei bindet und eine kopflose Datei sie
nicht hat; „Größe allein" ist nach derselben Tafel **0**. Gefangen hat
den Fehlgriff eine zweite Korpuszeile, nicht das Nachdenken: mit dem
Zugeständnis gewann `cpm` plötzlich das Rennen um eine NanoWasp-Datei.
