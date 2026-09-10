# Plan für den nächsten Durchgang

**Stand:** `4f934bef` (MF-999) · CI vollständig grün · 385/385 Tests
**Grundlage:** gemessen am 2026-09-10. Jede Zahl hat einen Befehl daneben,
mit dem sie nachzurechnen ist.

> **Nachtrag am selben Tag:** Der Eigentümer hat `neue-ideen/formats.zip`
> beigesteuert — **MAMEs vollständiges `src/lib/formats/`**. Das ändert
> Phase B grundlegend, und die Zahlen unten sind bereits danach gerechnet.

---

## 0. Was „perfekt funktionieren" hier heißen kann

> **Jede Zusage des Werkzeugs ist entweder belegt oder als unbelegt
> gekennzeichnet — und die Grenze dazwischen ist gemessen, nicht
> geschätzt.**

Das ist der Zustand, in dem ein forensisches Werkzeug „fertig" sein kann.
Alles andere wäre die Lage, in der die fünf fabrizierten Parser
(FMT-2/3/10/11/12) jahrelang grün standen.

---

## 1. Die Engstelle

Alle offenen Punkte hängen an **einer** Größe: der Zahl der Formate, deren
Leser gegen eine fremde Hand geprüft ist.

| | Zahl |
|---|---|
| Format-IDs | 138 |
| davon mit Plugin | 88 |
| T1 + T1b (gegen fremde Quelle belegt) | **29** |
| T2 | 24 |
| **T3 (unbelegt)** | **35** |

**Warum das alles blockiert** — belegt am eigenen Verlauf:

* Die zehn Dateischreiber ohne Weg (P3-204) gehören **alle** zu
  T3-Formaten. Ein Rundlauf darauf beweist nur, dass unser Leser unseren
  Schreiber versteht — die Selbstbestätigung, die **MF-992** an der
  D64-BAM aufdeckte.
* **Die Reihenfolge steht fest:** `f3c8099a` (MF-905) hob den OPD-Leser
  von T3 auf T2, erst danach verdrahtete `c601a789` (MF-931) den
  Schreiber.

---

## 2. Der MAME-Fund — was `formats.zip` ändert

**Gemessen an der Beisteuerung:**

| | |
|---|---|
| Dateien gesamt | 457 |
| **Diskettenformate** (`floppy_image_format_t` & Verwandte) | **113** |
| davon BSD-3-Clause | **111** (je 1× LGPL-2.1+, GPL-2.0+) |
| **Kassettenformate** | **50** |
| davon BSD-3-Clause | **48** (2× GPL-2.0+) |
| **Dateien ohne Lizenzzeile** | **0** |

```sh
grep -rhoP '^// license:\K[^\s]+' formats/ | sort | uniq -c
```

Null Dateien ohne Lizenzzeile — das ist die sauberste Herkunftslage, die
diesem Baum je begegnet ist, und der Gegenpol zu `uft_caps_ipf.c`
(„Based on SPS CAPS Library", kein SPDX, keine Lizenz).

### 2.1 Der Kanal ist bereits erprobt — MF-614

Der Baum hat das Muster schon, und zwar für MAME. `src/formats/mfi/uft_mfi.c`:

```
 * Referenz (benannt, wie die Einfrier-Regel es verlangt):
 *   [1] MAME, src/lib/formats/mfi_dsk.h — struct header / struct entry
 *   [2] MAME, src/lib/formats/mfi_dsk.cpp:81-82 — sign/sign_old
 * Beide Dateien BSD-3-Clause (SPDX je Datei). Kein MAME-Code kopiert.
```

**MAME-Quelle als benannte Referenz lesen, eigenständig umsetzen, Datei
und Zeile zitieren, ausdrücklich festhalten, dass nichts kopiert wurde.**
Keine Lizenzentscheidung nötig, weil nichts übernommen wird — und BSD-3
wäre in einem GPL-2-Baum ohnehin verträglich.

### 2.2 Was das für die T3-Zahl bedeutet

| Quelle des Orakels | T3-Formate |
|---|---|
| `src/samdisk/` (schon im Baum) | 6 — `cfi cpm ipf mgt scl udi` |
| **MAME (BSD-3)** | **11** — `apridisk cpm dim ipf jv1 jv3 mgt nib scl vdk victor9k` |
| **Vereinigung** | **13** |
| abzüglich `ipf` (Lizenz-Quarantäne) | **12 nutzbar** |

**T3 könnte damit von 35 auf 23 fallen** — die größte Bewegung dieser
Kennzahl, die je möglich war.

Und **drei der zehn Schreiber ohne Weg** (P3-204) bekommen damit ihr
Orakel: `apridisk`, `cfi`, `mgt`.

### 2.3 Zwei Befunde, die der Fund sofort korrigiert

* **P3-312 muss abgeschwächt werden.** MAMEs `coupedsk.cpp` rechnet den
  MGT-Spurversatz als `(track*2+head)*track_size` — **dieselbe Anordnung
  wie UFTs `idx = c*heads + h`. UFT liest MGT richtig.** MAME kennt die
  IMG-Variante ebenfalls nicht; sie ist eine SAMdisk-Besonderheit. Der
  Eintrag bleibt als Hinweis, verliert aber seine Schärfe.
* **Der `mgt`-Blocker ist weg.** P3-312 nannte das fehlende
  `src/samdisk/SAMCoupe.h` als Grund, warum die Erkennung nicht
  begründbar sei. `coupedsk.cpp` ist ein vollständiges, unabhängiges
  Orakel — **Lese- UND Schreibseite**.

### 2.4 Was MAME hat und UFT nicht

Nach deklarierter Endung: **32 kennen beide, 42 nur MAME.**

```
2d 2hd 360 40t ads afd bbc bin ccvf d1m d2m d40 d4m dds dfi ds9 dtk edd
fd fd5 fdd fl1 hdm hmd iqd juk kay kdi ldf mfloppy mfm moof odi opu os9
out sdd sf7 swd ufi w30 wta
```

**Das ist eine Liste für den Fundus, nicht für die Arbeitsplanung.** Jedes
davon wäre ein **neues Format-Plugin** und fällt unter das Moratorium der
EINFRIER-REGEL (1:2 — ein neues Format kostet zwei Hebungen). Sie stehen
hier, weil MF-695 verlangt, dass ein Fund benannt wartet statt zu
verfallen — und weil die Lizenzfrage für alle 42 bereits beantwortet ist.

Bemerkenswert darunter: **`moof`** (Apple, in CLAUDE.md als unterstützt
geführt) und **`edd`** (`uft_edd.c` existiert) — beide verdienen eine
Einzelprüfung, ob UFT sie wirklich hat oder nur nennt.

---

## 3. IPF — die teuerste Quarantänezeile, aus neuer Richtung

Die Quarantäne begründet IPFs Sonderweg (`docs/QUARANTINE.md:76`):

> **Weg 2 (Clean-Room aus Spec) ist versperrt: das IPF-Format ist bewusst
> undokumentiert, die SPS behält sich seine Erzeugung vor.**

Diese Aussage ist über **Dokumentation**. `formats/ipf_dsk.cpp` (25 KB,
BSD-3-Clause, Olivier Galibert) ist eine unabhängig geschriebene,
freizügig lizenzierte **Umsetzung** — und der Baum behandelt so etwas seit
MF-614 als legitime benannte Referenz.

Betroffen sind drei Dateien:

| Datei | Lage |
|---|---|
| `uft_ipf_air.c` (1042 Z.) | „Full port" GPL-3, Weg 3 beschlossen |
| `uft_caps_ipf.c` (790 Z.) | „Based on SPS CAPS Library" — **kein SPDX, keine Lizenz**, und **erreichbar** (`uft_ipf_plugin.c:46`) |
| `uft_ipf_helper.c` | verdrahtet, Gegenseite existiert nicht (P3-190) |

**Die Frage, die vor jedem Handgriff stand** — eine Eigentümerfrage,
keine Messung:

> Ist MAMEs IPF-Leser wirklich unabhängig entstanden, oder leitet er sich
> seinerseits von SPS-Material ab? Der BSD-3-Kopf ist Galiberts Zusage,
> und nach der Regel dieses Baums ist eine Attribution eine **rechtliche
> Aussage** (MF-636), keine Höflichkeit.

> ### ✅ Beantwortet am 2026-09-10 — **ja** (MF-1003)
>
> Der Eigentümer hat bestätigt: MAMEs `ipf_dsk.cpp` gilt als unabhängig
> entstanden. Damit ist er nach dem **MF-614-Muster** eine benannte
> Referenz — lesen, eigenständig umsetzen, Datei und Zeile zitieren,
> ausdrücklich festhalten, dass nichts kopiert wurde. BSD-3 ist mit
> UFTs GPL-2-or-later verträglich. Das Register in
> `docs/QUARANTINE.md` führt für `uft_ipf_air.c` jetzt **Weg 1/2**
> statt Weg 3; Weg 3 bleibt Rückfall.

### 3.0 Was das ja gebracht hat — und was es NICHT gebracht hat

**Die eine der beiden Problemdateien ist weg, ohne dass das ja dafür
nötig war.** Eine Messung vor dem ersten Handgriff hat `uft_caps_ipf.c`
(790 Z., keine Lizenz) anders entschieden als erwartet — sie kostete
**keine Fähigkeit**:

```
uft_caps_is_ipf(echtes IPF, "CAPS")      = FALSE
uft_caps_is_ipf(kein IPF, 00 00 00 01)   = TRUE
```

`read_block_header()` legt den rohen ASCII-Vierer als BE-u32 ab und
vergleicht gegen eine interne Aufzählung `1..10`. Für `"CAPS"` steht dort
`0x43415053`, nie `1` — kein `case` konnte je greifen. Die Datei war
strukturell außerstande, ein IPF zu lesen, und der einzige erreichbare
Zweig konnte nur **falsch positiv** werden. Gelöscht in **MF-1002**;
Quarantäne-Befundstufe **7 → 6**.

**Die zweite Datei bleibt — und der Blocker hat gewechselt.**
`uft_ipf_air.c` (1042 Z., **GPL-3-only**) ist der wirkliche IPF-Leser und
bindet den ganzen Baum an GPL-3. Ihn zu ersetzen ist jetzt
**lizenzseitig frei**. Was stattdessen blockiert, ist **Daten**:

| | |
|---|---|
| IPF im Korpus | **null** (`find tests -iname '*.ipf'` leer, kein Manifest-Eintrag) |
| heutiger Test | `test_ipf_air_accessors.c` baut einen **synthetischen** `"CAPS"`-Puffer |
| Stufe | `ipf` = **T3** |

Ein Ersatz, der heute geschrieben würde, ließe sich nur gegen die eigene
Vorstellung vom Format prüfen — die Selbstbestätigung aus **MF-992**, und
die EINFRIER-REGEL (b) verlangt gemessene Zahlen.

**Kanal nach MF-695: Daten/Fixture.** Es braucht ein IPF, dessen
Weitergabe geklärt ist. Das ist die **nächste Eigentümerfrage** — und
eine kleinere als die beantwortete.

**Reihenfolge, belegt statt geraten:** erst Daten, dann Code. `f3c8099a`
(MF-905) hob den OPD-Leser gegen ein Orakel, erst danach verdrahtete
`c601a789` (MF-931) den Schreiber; MF-864 baute den FM-Dekoder erst,
nachdem `fluxtoimd` als fremde Hand die Prüfspur abnehmen konnte.

### 3.1 `caps.zip` beantwortet die Frage nicht — es wiederholt sie

Der Eigentümer hat am selben Tag `neue-ideen/caps.zip` beigesteuert.
**Gemessen, bevor irgendetwas damit geschah:**

| | |
|---|---|
| Inhalt | 3 Dateien: `CapsAPI.h`, `Comtype.h`, `caps_amiberry.cpp` |
| tatsächliches Format | **GitHub-HTML-Seiten**, kein Quelltext — der Download hat die gerenderte Seite gespeichert |
| Herkunft (aus den Seiten) | `BlitterStudio/amiberry`, Pfad `src/caps/` |
| Quelltext wiederherstellbar | ja, die Nutzlast steckt eingebettet in der Seite |
| **Lizenzzeile in den Köpfen** | **keine.** Kein SPDX, kein Copyright, kein GPL-Hinweis — in keiner der drei |

Damit ist es **wörtlich dieselbe Lage wie `uft_caps_ipf.c`**: „Based on
SPS CAPS Library", ohne Lizenz. Der Fund löst das Problem nicht, er
verdoppelt es.

**Und der Kanal ist ein anderer, als der Name vermuten lässt.**
`CapsAPI.h` ist die **Schnittstelle zur geschlossenen `capsimg`-
Bibliothek** — eine Deklaration für ein Binärobjekt, keine Umsetzung des
Formats. Nach MF-695 öffnet das **Helfer-Prozess** oder **Oracle**
(capsimg *rufen*), niemals **Port** und niemals **Spec**. Man kann daraus
nichts nachbauen, weil nichts darin beschreibt, wie IPF aufgebaut ist.

MAMEs `ipf_dsk.cpp` bleibt der einzige Kandidat für Weg 1 — und die Frage
aus §3 bleibt unbeantwortet.

> **Aus den Seiten wurde nur der Kopf gelesen** (35 Zeilen je Datei), weil
> eine Lizenzbestimmung den Kopf braucht und nicht die Umsetzung. Die
> Zwei-Hände-Brandmauer aus `docs/QUARANTINE_PROCESS.md` bleibt damit
> unversehrt: falls je ein Clean-Room-Nachbau ansteht, hat niemand die
> Vorlage gesehen.

---

## 4. Phase A — die Hebungen, neu gerechnet

**Zwölf Formate haben jetzt ein lizenzsauberes, lesbares Orakel.**
Reihenfolge nach Doppelnutzen:

| # | Format | Orakel | zusätzlich |
|---|---|---|---|
| ~~1~~ | ✅ **`cfi` — erledigt MF-1004** | `src/samdisk/cfi.cpp` (74 Z.) | **Schreiber verdrahtet**, zweiter der elf nach `opus` |
| 2 | `mgt` | MAME `coupedsk.cpp` | Schreiber (P3-204), Blocker weg |
| 3 | `apridisk` | MAME `apridisk.cpp` | Schreiber (P3-204) |
| 4–12 | `cpm` `dim` `jv1` `jv3` `nib` `scl` `udi` `vdk` `victor9k` | samdisk bzw. MAME | — |

**Je Durchgang** (das MF-905-Muster): fremden Leser Feld für Feld gegen
unseren halten · Rotbeweis für jede Abweichung **vor** dem Fix ·
Tier-Zeile mit benannter Referenz · bei den ersten drei unmittelbar
danach den Schreiber nach dem MF-931-Rezept verdrahten.

**Grenze, ausdrücklich:** Diese zwölf erreichen **T2**, nicht T1b — dafür
braucht es ein von fremder Hand *erzeugtes Abbild*, und für keines liegt
eines im Korpus. Ein gelesener Quelltext belegt die Struktur, nicht die
Wirklichkeit.

### 4.1 Was der erste Durchgang gelehrt hat (MF-1004, `cfi`)

**T3 → T2 erreicht, Schreiber verdrahtet, T3 = 34.** Sieben Abweichungen
gegen `ReadCFI()`, sechs davon dieselbe Klasse: das Orakel bricht ab, UFT
kürzte still (leerer Spurblock verwarf den **Rest der Datei**;
Pufferüberlauf wurde geklemmt und die Geometrie danach aus der
*geklemmten* Größe erfunden).

**Die siebte ist die lehrreiche, und sie kam nicht aus dem
Feldabgleich.** `uft_cfi_read_mem()` wies jede Datei unter 512 Byte ab.
CFI *komprimiert*: der eigene Schreiber erzeugte aus 9216 Byte Nutzlast
eine **39 Byte** große, strukturell einwandfreie Datei — und der eigene
Leser lehnte sie ab. `ReadCFI()` hat keine solche Konstante.

> **Ein Feldabgleich vergleicht Verhalten. Eine Konstante, die das
> Orakel gar nicht hat, fällt dabei nicht auf.** Gefunden hat es der
> Rundlauf der Schreibseite.

**Für die restlichen elf heißt das: Leseabgleich UND Rundlauf, nicht
eines von beiden.** Die Reihenfolge bleibt (erst Leser, dann Schreiber —
MF-905 → MF-931), aber der Rundlauf ist Teil der Hebung, nicht ein
Nachtrag.

---

## 5. Phase B — was jetzt noch beschafft werden muss

Von 29 auf **23** geschrumpft:

```
2img cas dcm dms edk fdi_pc98 fds hardsector kfx logical myz80 nanowasp
posix pri pro qrst rcpmfs sap_thomson syn tan v9t9 xdm86
```

Je Format eine Zeile: Kanal (MF-695) · was genau fehlt · wer es liefern
kann · Aufwand. **Auftrag für `uft-scout` / `uft-github-scout`** — sie
liefern Dokumente, keinen Code.

Zwei stehen fest: **`hfe`** braucht eine von HxC oder greaseweazle *leer
erzeugte* HFE (P3-309). **`ipf`** braucht seit MF-1003 kein Lizenzurteil
mehr, sondern **eine Datei** — siehe §3.0; es ist damit von der
teuersten Zeile der Liste zu einem gewöhnlichen Beschaffungsposten
geworden.

---

## 6. Phase C — die Schreibseite hat keine Prüfstufe

**Der strukturelle Befund aus MF-992:** die Tier-Stufen fragen, ob ein
Format richtig **gelesen** wird. Fürs **Schreiben** gibt es kein
Gegenstück. `bam_create_d64` war zehnfach getestet und hätte in keiner
Tier-Zahl auffallen können — die BAM stand trotzdem 4 bis 32 Byte daneben.

Vorschlag, ausdrücklich als **Eigentümer-Entscheidung** (Regel 9 verlangt
eine Begründung für jede neue Zahl):

> **Fünfte Kennzahl: „Formate, deren SCHREIBseite gegen eine fremde Hand
> gemessen ist."** Heute wäre sie **1** (`d64`, MF-994 gegen VICE).

MAME hilft hier besonders: seine Formate haben **`save()`-Methoden**, also
eine belegte Schreibseite — bei `coupedsk.cpp` steht sie zwölf Zeilen
unter der Leseseite.

---

## 7. Phase D — `neue-ideen/`, nach Inhalt

**96 Einträge, UFT-29 bis UFT-93.**

### D1 — bearbeitbar, weil es bestehenden Code betrifft

| Bericht | Was | Warum erlaubt |
|---|---|---|
| **UFT-83** | Doug Bell (Schöpfer) beschreibt den Fuzzy-Bit-Mechanismus selbst; `uft_fuzzy_bits.c` nennt nur „Based on Dungeon Master", `UFT-57` erschloss ihn übers US-Patent 4.849.836 | **Referenz-Aufwertung** — EINFRIER-REGEL (c) |

### D1b — beim Prüfen des Kassetten-Fundes aufgefallen (MF-1000)

Die Kassettenfrage aus §9 führte auf einen `main()` in `src/` und von dort
auf einen eigenen Befund, **P3-313**:

> **36 Selbsttests mit 604 Zusagen, die kein Compiler je gesehen hat.**
> `#ifdef FOO_TEST` … `assert(...)` … `#endif`, wobei `FOO_TEST` von
> keinem Bausystem definiert wird. 44 verschiedene Wächternamen.

Das Bemerkenswerte ist nicht die Zahl, sondern dass **P3-89 / MF-845
genau diese Klasse schon einmal behandelt hat** — und dabei *einen*
Makronamen maß (`UFT_UNIT_TESTS`, 3 Dateien) statt der Klasse. Dieselbe
Form wie MF-930, wo der Kopf eines **Tores** acht Verdächtige aufzählte
und elf gemessen wurden.

Seit MF-1000 hält es **Tor 64**
(`scripts/audit_selbsttest_ohne_uebersetzung.py`, Grundlinie 36,
Selbsttest 6/6). Der Wächtername ist dort **abgeleitet**, nicht
aufgezählt — wer morgen `#ifdef WOZ4_TEST` schreibt, ist am selben Tag
erfasst.

**Der Eintrag ist kein Auftrag, 36 Blöcke zu heben.** Er ist die
Zusicherung, dass der 37. nicht mehr unbemerkt entsteht. Das Muster fürs
Heben steht in MF-851, samt seiner Grenze: `static`-Funktionen sind von
außen nicht erreichbar.

### D2 — Bestätigungen, die eingetragen gehören

| Bericht | Bestätigt |
|---|---|
| **UFT-84** | `uft_edd.c` unterscheidet die EDD-Fassungen bereits richtig |
| **UFT-85 / 86** | zwei Praxiskonventionen (Zero-Page-Magic, Timing-Bits nach dem Datenprolog) |
| **UFT-91** | `uft_geos_protection.c` führt **sechs** Schutztypen und hat **drei** Prüffunktionen, von denen keine einen davon erkennt; steht in `orphan_baseline.txt`. **Kein neuer Befund — ein Beleg für P0-2** |

### D3 — Moratorium, als Fundus mit benanntem Kanal

`UFT-87` (Viertelspuren) · `UFT-88` (AMSDOS-Kopf) · `UFT-89` (RWTS18 /
Prince of Persia) · `UFT-90` (SSI RDOS) · `UFT-92` (GEOS-Seriennummern) ·
`UFT-93` (GeoCopy-Schreibseite) · dazu `UFT-71/72/75/76/77/79/81/82` ·
**und die 42 MAME-Formate aus §2.4.**

#### `neue-ideen/spf-main.zip` — Port legal offen, heute ohne Nutzen

Gemessen: **SPF „Stress ProDOS Filesystem"** (ADTPro, David Schmidt),
67 Einträge, 34 Assemblerdateien für den Apple II.

| | |
|---|---|
| Lizenz | **GPL-2.0-or-later**, in *jeder* Quelldatei im Kopf (keine `LICENSE`-Datei) |
| Kanal nach MF-695 | **Port** — der stärkste; GPL-2+ ist mit UFTs GPL-2 verträglich |
| Inhalt | ProDOS-Dateisystem-Belastungstest; `format.asm` (1035 Z.), `diskii.asm` (1539 Z.) |

**Und trotzdem: heute kein Auftrag.** `diskii.asm:1326` trägt
`NIB_2_6BB`, die 6&2-Entschlüsselungstafel ab `$96` — genau die
Apple-GCR-Tafel. UFT **braucht sie nicht**: `uft_apple_gcr.c` ist gegen
*Beneath Apple DOS* und das Oracle `to_woz2` abgenommen (560 von 560
Sektoren). Was `nib` auf T3 hält, ist nach **P3-234** nicht eine
fehlende Tafel, sondern dass das geprüfte Modul **nur von Tests**
gerufen wird und `nib` eine eigene Kopie führt.

**Der Fix für `nib` ist Verdrahtung, kein Import** — und Verdrahten
vorhandenen, geprüften Codes ist unter der EINFRIER-REGEL ausdrücklich
erlaubt. Diese Messung hat damit einen unnötigen Import verhindert; das
ist ihr Ertrag.

Der Rest von SPF zielt auf **echte Apple-II-Hardware**, die dieses
Projekt nicht hat (MF-310).

**`UFT-89` verdient eine eigene Notiz:** der Originalautor sagt, das
unveränderte Prince of Persia lasse sich bis heute nicht emulieren.

---

## 8. Reihenfolge

1. ~~**§3 zur Entscheidung vorlegen** — die IPF-Herkunftsfrage.~~
   **Erledigt 2026-09-10 (MF-1003): ja.** Eine der beiden Dateien ist
   dabei ganz weggefallen (MF-1002), die andere wartet jetzt auf eine
   Datei statt auf ein Urteil. Die verbleibende Eigentümerfrage ist
   klein: **darf ein IPF in den Korpus** (§3.0).
2. **Phase A**, Format für Format, `cfi` → `mgt` → `apridisk` zuerst.
3. **UFT-83** dazwischen — klein, unabhängig, verbessert eine Quelle.
4. **Phase B als Auftrag an die Aufklärungs-Agenten**, parallel.
5. **D2/D3 eintragen**, sobald der erste A-Durchgang steht.
6. **Phase C vorlegen.**

**Was NICHT zuerst kommt:** die zehn Schreiber. Sie sehen nach der größten
Lücke aus, sind aber die Folge der T3-Zahl, nicht ihre Ursache (MF-997).

---

## 9. Was dieser Plan offen lässt

* **Hardware.** Acht der neun Controller sind nie an einem Gerät gelaufen.
  Das Projekt hat keine (MF-310); ein Plan, der so tut, wäre eine
  erfundene Zusage.
* **Kassetten.** MAME hat 50 Kassettenformate (48 BSD-3) mit 65 Endungen;
  UFT teilt davon genau **eine** (`cas`). Der Kassettencode existiert —
  fünf Dateien, gemessen:

  | Datei | im qmake-Build | Aufrufer |
  |---|---|---|
  | `src/formats/tzx/uft_tzx_wav.c` (1282 Z.) | ja | keiner (`orphan_baseline`) |
  | `src/formats/tzx/uft_zxtap.c` (682 Z.) | ja | keiner (`orphan_baseline`) |
  | `src/formats/bbc/uft_bbc_tape.c` (508 Z.) | ja | keiner (`orphan_baseline`) |
  | `src/formats/tap/uft_tap_parser_v2.c` (295 Z.) | ja | keiner (`orphan_baseline`) |
  | `src/formats/c64/uft_tap.c` (861 Z.) | ja | **nicht** als verwaist geführt |

  Er ist **kein** Disketten-Plugin und steht zu Recht nicht in der
  Registry — Kassetten sind ein anderes Medium. Aber vier der fünf haben
  im ganzen Baum keinen Aufrufer, und ihre Selbsttests stehen hinter
  Wächtern, die niemand definiert (P3-313). **Das ist Bestand, nicht
  Fähigkeit** — dieselbe Formel wie beim Kopierschutz-Katalog (P0-2).

  Kassetten gehören nicht in diesen Plan, solange die Diskettenseite
  offen ist. Aber die Zeile „TAP, TZX" in `CLAUDE.md` §2 verspricht
  mehr, als vier verwaiste Dateien halten, und das gehört bei der
  nächsten Durchsicht der Titelseite geprüft.
* **P3-307** (161 Unbenutzt-Warnungen, 13 stille Kürzungen). Real, aber
  ohne Tor — und ein Tor bräuchte eine Auswertung des CI-Protokolls.

---

*Jede Zahl in diesem Dokument ist gemessen; wo eine Vermutung steht, ist
sie als solche gekennzeichnet.*
