# Die Tripel-Tabelle — Spurverdikt als drei Felder statt einer Ziffer

**Werkstatt-Dokument, Zug 2. Stand 2026-09-01 (MF-757).**

X-Copy presst drei Aussagen in eine rote Ziffer. Dieses Dokument
zerlegt sie und stellt daneben, was UFT heute für denselben Fall setzt.

| Feld | Werte |
|---|---|
| **Diagnose** | keine · Fremdformat · Schutz · Schaden · leer |
| **Folge** | auf Normallaufwerk reproduzierbar · **nicht** reproduzierbar |
| **Reparierbarkeit** | korrigierbar · nicht korrigierbar · nach Korrektur **gerettet** |

Die Abbildung *Code → Tripel* ist der **Konformitätsvertrag**. Jede
Zeile links ist MEASURED aus den Handbuchständen, jede Zeile rechts
MEASURED am Code — mit Datei und Zeile.

> **P6 ist hier ausdrücklich ausgeklammert.** Der Schreibstartpunkt ist
> kein Verdikt über eine gelesene Spur, sondern eine Fähigkeit des
> Schreibpfads. Er hat mit diesem Modell nichts zu tun und wartet als
> eigener Auftrag auf Hardware (MF-756, rot).

## Quellen der Soll-Seite

| | |
|---|---|
| **3.4-Autoren-Master**, 17.02.1991 | `Anleitung XCOPY`, Abschnitt **7.2) ERRORS** — alle acht Codes **mit Bedeutung** |
| **5.21-Datenträger**, 24.10.1991 | `MANUAL/MANUAL.XCOPY` — *Checkdisk*, *Speedcheck*, *Sync*, die Betriebsarten |
| **2.x**, überliefert | die bloße Codeliste; **Nummerierung identisch mit 3.4** |

Alle drei stehen unter ASI-Copyright und sind **nicht Teil der Freigabe
von 2011**. Hier stehen abgeleitete Aussagen und Fundstellen, **keine
Abschriften**.

---

## Die Tabelle

### 0 — grüne Null

| | |
|---|---|
| **Diagnose** | keine |
| **Folge** | reproduzierbar |
| **Reparierbarkeit** | — |
| **Ist in UFT** | `UFT_TRACK_OK = 0`, `UFT_SECTOR_OK = 0` — **vorhanden** |

### 1 — Less or **more** than 11 sectors

| | |
|---|---|
| **Diagnose** | **Fremdformat** — das Handbuch liest eine abweichende Sektorzahl ausdrücklich als möglicherweise anderes Format, nicht als Schaden |
| **Folge** | reproduzierbar (Rohkopie) |
| **Reparierbarkeit** | nicht korrigierbar — es ist kein Defekt |
| **Ist in UFT** | **nicht unterschieden.** `OTDR_EVT_EXTRA_SECTOR` und `OTDR_EVT_MISSING_SECTOR` sind **deklariert, aber nirgends erzeugt** (gemessen: 0 `add_event`-Stellen). `floppy_otdr.c:116-132` berechnet `expected_sectors` je Format (9/11/18) — und **liest die Zahl nie wieder**. Dreifach vorbereitet, nirgends verdrahtet. |

> **Die Richtung „mehr als" fehlt ganz.** Eine Spur mit *zu vielen*
> Sektoren ist ein klassisches Schutzmerkmal; eine einseitige Schranke
> sieht es nicht.

### 2 — No sync found

| | |
|---|---|
| **Diagnose** | **Schutz oder Fremdformat** — nicht „leer", nicht „unlesbar" |
| **Folge** | reproduzierbar (nur im Nibbler-Modus) |
| **Reparierbarkeit** | nicht korrigierbar |
| **Ist in UFT** | **drei Fälle fallen auf einen.** `uft_flux_decoder.c:738,810` gibt `return (track->sector_count > 0) ? FLUX_OK : FLUX_ERR_NO_SYNC;` — null Sektoren ergibt `NO_SYNC`, ob die Spur leer, fremd, geschützt oder verrauscht ist. `UFT_TRACK_UNFORMATTED` setzen nur Container-Plugins (g64, g71, hfe, scp) für „im Abbild leer". **`UFT_TRACK_READ_ERROR` setzt niemand** (0 Zuweisungen im ganzen Baum). |

> **Das ist P4, und es ist die größte Einzellücke.** Die Vorlage
> unterscheidet hier vier Zustände, UFT einen.

### 3 — No sync after gap found

| | |
|---|---|
| **Diagnose** | **Schaden, teilweise** — AmigaDOS-Struktur vorhanden, aber teilweise zerstört |
| **Folge** | reproduzierbar |
| **Reparierbarkeit** | im Handbuch **nicht** als korrigierbar genannt |
| **Ist in UFT** | **nicht unterschieden.** Kein Zustand zwischen „lesbar" und `NO_SYNC`. B9 nennt die Unterscheidung diagnostisch wertvoll, weil sie *Medium* von *Aufnahmeparametern* trennt — eine zu kurz geratene Aufnahme sieht aus wie ein defektes Medium. |

### 4 — Header checksum error

| | |
|---|---|
| **Diagnose** | Schaden |
| **Folge** | reproduzierbar |
| **Reparierbarkeit** | **korrigierbar** — das Handbuch nennt `DOSCOPY+` je Code ausdrücklich |
| **Ist in UFT** | `UFT_SECTOR_ID_CRC_ERROR` (`1<<1`) — **Diagnose vorhanden.** Reparierbarkeit **nicht als Feld**: `uft_crc_correction_v2.c:333` führt `sectors_uncorrectable` als **Zähler**, `uft_forensic_recovery.c:745` als **Protokollzeile**. Kein Zustand am Sektor. |

### 5 — Error in header / format long

| | |
|---|---|
| **Diagnose** | Schaden — **Kopfinhalt** zerstört, nicht nur dessen Prüfsumme |
| **Folge** | reproduzierbar |
| **Reparierbarkeit** | **korrigierbar** |
| **Ist in UFT** | **nicht von Code 4 unterschieden.** `UFT_SECTOR_ID_CRC_ERROR` deckt die Prüfsumme ab; für „Inhalt zerstört" gibt es keinen eigenen Zustand. |

### 6 — Data block checksum error

| | |
|---|---|
| **Diagnose** | Schaden |
| **Folge** | reproduzierbar |
| **Reparierbarkeit** | **korrigierbar** |
| **Ist in UFT** | `UFT_SECTOR_CRC_ERROR` (`1<<0`) — **vorhanden und von Code 4 getrennt.** Reparierbarkeit wie oben: kein Feld. |

### 7 — Long track

| | |
|---|---|
| **Diagnose** | **Schutz** — mit Spezialhardware geschrieben |
| **Folge** | **NICHT reproduzierbar** auf gewöhnlichen Laufwerken |
| **Reparierbarkeit** | nicht korrigierbar |
| **Ist in UFT** | `OTDR_EVT_PROT_LONG_TRACK` (`floppy_otdr.c:626`), Text „Long Track (CP)", abgebildet in `uft_otdr_bridge.c:300`, **erreicht `src/gui/uft_otdr_panel.cpp`**. Diagnose **vorhanden und mit Tür.** — **Die Folge fehlt:** dass die Spur mit gewöhnlicher Hardware nicht schreibbar ist, sagt der Bericht nicht. |

> Der einzige Code, dessen Diagnose in UFT vollständig ankommt. Und
> zugleich der einzige, bei dem die **Folge** eine eigene Aussage ist —
> sie ist die Antwort auf „kann ich diese Diskette überhaupt sichern?".

### 8 — Verify error

| | |
|---|---|
| **Diagnose** | **Schaden am ZIEL** — physischer Defekt des Zielmediums |
| **Folge** | — |
| **Reparierbarkeit** | Wiederholung (`R`) oder Abbruch (`C`) |
| **Ist in UFT** | `WriteVerifyFailed` als eigenes Verdikt (`src/fluxwritejob.cpp:224`), Rückvergleich auf **Rohfluss-Ebene** — **besser als die Vorlage**, die rohkopierte Spuren überspringt (B12). **Aber `m_verify(false)`** — standardmäßig aus. |

### Zusatzzustand ohne X-Copy-Code: **gerettet**

| | |
|---|---|
| **Herkunft** | B11 — das Änderungsprotokoll zu 5.21 dokumentiert die Korrektur: eine nach Lesefehler im Rohverfahren gerettete Spur darf **nicht** als fehlerfrei erscheinen |
| **Reparierbarkeit** | **nach Korrektur gerettet** — der dritte Wert des Feldes |
| **Ist in UFT** | `UFT_SECTOR_RECOVERED` existiert, wird an 10 Stellen korrekt gesetzt, und `tests/test_sector_recovery_honesty.c` prüft die Regel. **Aber: 5 exportierte Funktionen, 1 Aufrufer von außen, und das ist der Test.** Null Produktionsaufrufer — in der Produktion wird nie ein Sektor als RECOVERED markiert. |

---

## Das Lückenbild in einem Blick

| Code | Diagnose | Folge | Reparierbarkeit |
|---|---|---|---|
| 0 | ✔ | — | — |
| **1** | **✘ nicht unterschieden** | ✔ | ✘ |
| **2** | **✘ vier Fälle auf einen** | ✔ | ✘ |
| **3** | **✘ nicht unterschieden** | ✔ | ✘ |
| 4 | ✔ | ✔ | **✘ kein Feld** |
| **5** | **✘ = Code 4** | ✔ | **✘ kein Feld** |
| 6 | ✔ | ✔ | **✘ kein Feld** |
| 7 | ✔ **mit Tür** | **✘ Folge fehlt** | ✘ |
| 8 | ✔ | — | ✔ (aber aus) |
| gerettet | ✔ **ohne Tür** | — | ✘ kein Feld |

**Diagnose:** 4 von 9 Fällen unterschieden.
**Folge:** existiert als Begriff nicht — der einzige Fall, der sie
braucht (Code 7), nennt sie nicht.
**Reparierbarkeit:** existiert nirgends als Zustand; nur als Zähler und
Protokollzeile.

## Was daraus folgt

**Ein Modell statt vier Einzelfixes.** P3, P4, P5 und B11 hängen alle
an denselben drei Feldern:

* **P4** ist die Diagnose-Spalte, Zeilen 1, 2 und 3
* **P5** ist die Folge-Spalte, Zeile 7
* **P3** und **B11** sind der dritte Wert der Reparierbarkeit
* **P2** ist keine Verdikt-Frage, sondern die Voraussetzung: ohne
  bitweise Sync-Suche fällt Zeile 2 gar nicht erst an

**Die Reihenfolge steht damit fest:** erst die drei Felder im
Spurverdikt, dann die Türen. Wer vorher verdrahtet, verdrahtet ins alte
Ein-Zustand-Modell.

## Was hier NICHT steht

* **Kein Übernahmevorschlag.** Die Tabelle ist ein Vertrag über UFTs
  eigenes Verhalten. Sie enthält keine Struktur, keine Zerlegung, keine
  Namensgebung der Vorlage.
* **Keine Zahlenwerte der Vorlage.** Die Sollzahl 11 steht als
  *AmigaDOS-Regelzahl* da, nicht als zu übernehmende Konstante;
  `floppy_otdr.c` kennt sie ohnehin selbst.
* **Kein Urteil über P1, P8, P9, E1.** Die brauchen Hardware, Korpus
  oder Emulation.


---

## Vorab-Punkt zu P2: die zwei Fassungen sind **verschieden**, nicht doppelt

Die Frage vor dem Verdrahten lautete: prüfen beide bitweisen Fassungen
denselben Sync-Satz und dieselbe Rotation? Gemessen — **nein**, und
damit entscheidet die Auswahl das Verdikt.

| | Mustersatz | Mechanismus |
|---|---|---|
| `uft_fuzzy_sync_v2.c:22-34` | **kodierungsübergreifend**: MFM `0x4489`, `0x5224` (Index-AM), FM `0xF57E`/`0xF56F`, GCR-CBM, GCR-Apple `0xD5AA96` — je ein Muster pro Familie, mit **Hamming-Toleranz** | Schiebefenster über den Bitstrom, `get_bits16(data, bit_pos)` |
| `uft_track_analysis.c:30-44` | **Amiga-Schutz**: `0x4489`, `0x9521`, `0xA245`, `0xA89A`, `0x448A`, dazu Plattformtabellen (`0x4489, 0xA1A1, 0x4E4E`) | Rotation über `rol32` |

**Keine ist tot.** Sie beantworten verschiedene Fragen: *„wo ist
irgendein Sync irgendeiner Kodierung, Bitfehler erlaubt"* gegen *„wo
sind die Amiga-Schutz-Syncs"*.

### Und das hat eine Folge für die Entfernung

`uft_track_analysis.c` wird wegen der Lizenz entfernt (MF-744). Danach:

| | bleibt |
|---|---|
| der Amiga-Sync-**Satz** | ja — `src/formats/amiga/uft_amiga_syncs.c` führt die vier Werte als eigene Tabelle (drei davon als `XCOPY_SRC` markiert) |
| die **bitweise Suche** | ja — `uft_fuzzy_sync_v2.c`, aber **ohne Aufrufer** |
| die **Verbindung** beider | **nein** |

Nach der Entfernung stehen Tabelle und Suchverfahren im Baum, und
nichts führt sie zusammen.

**Kein Verlust an geprüfter Fähigkeit:** von den vier Werten ist einer
belegt, zwei sind von unabhängiger Preservation-Dokumentation
widersprochen, und der Baum führte `0xA245` selbst unter zwei Namen
(MF-740). Was verschwindet, sind vier ungeprüfte Konstanten in einem
Suchlauf ohne Tür.

**Für die Verdrahtung heißt das:** Zeile 2 der Tabelle (Diagnose
*Schutz oder Fremdformat*) braucht beide Teile — den erweiterbaren
Sync-Satz **und** eine erreichbare bitweise Suche. Heute fehlt die
Verbindung, morgen zusätzlich der eine Aufrufer. Wer P2 verdrahtet,
verdrahtet `uft_fuzzy_sync_v2` gegen `uft_amiga_syncs` — nicht gegen
die entfernte Datei.

> B5 verschiebt die Anforderung ohnehin: nicht „diese fünf Werte
> kennen", sondern **einen erweiterbaren Sync-Satz führen**. Das
> Handbuch führt die Syncs seit 5.21 als vom Anwender änderbare
> Einstellung — ein fest verdrahteter Satz war schon 1991 als
> unzureichend erkannt.


---

## Fünfte Frage beantwortet — und sie fällt negativ aus (MF-758)

Der Eigentümer hat den Weg genannt: *„Die Abstammung steht im Kopf von
`DMS.c`, ein Blick statt ein Lauf."* Nachgelesen — er steht dort:

> *„This client is lossely based (mainly decrunch stuff) on **xDMS**
> source made by Andre R. de la Rocha."*
> — `libxad/portable/clients/DMS.c`, © 1998 ff. Dirk Stöcker, LGPL-2.1

Unsere Attribution lautet *„Based on **xDMS** source, dms2adf, AROS
source"*. **Dieselbe Hand.** Ein von xDMS geerbter Fehler steckt in
beiden, und ein Differenzlauf entscheidet darüber nichts.

### Eine Präzisierung, die derselbe Kopf liefert

Geteilt ist ausdrücklich nur die **Entpackung** („mainly decrunch
stuff"). Zwei Sätze weiter steht, der Client benutze *„not DMS header
information (except password flag), but always the track data"*.

| Ebene | libxad gegenüber xDMS |
|---|---|
| Entpackungsalgorithmus | **abgeleitet** — kein unabhängiger Zeuge |
| Containerbehandlung / Spurdaten | **möglicherweise unabhängig** |

Wer `dms` trotzdem hierüber prüfen will, muss beides trennen — sonst
ist der Lauf zirkulär.

### Folge, wie vom Eigentümer vorgezeichnet

* **Der erste Differenzlauf ist `trd`, nicht `dms`.** TR-DOS hat die
  Abstammungsfrage nicht; dort fällt die Kennzahl zuerst.
* **`dms` bekommt das X-Copy-Muster:** das **Original-DMS unter
  Emulation** als Oracle, nicht eine Reimplementierung derselben Linie.
* Der Registereintrag `xadundisk` ist entsprechend berichtigt — er nennt
  die Abstammung jetzt als gemessen, nicht als offen.

> **Die Vorhersage traf zu, und das Verfahren hat sie eingeholt.** Es
> hätte eine Registrierung, einen Bau und einen Lauf gekostet,
> herauszufinden, was in vierzig Zeilen Kopfkommentar stand.


---

## Zug 2b — erste Messungen, und der Plan ändert sich an zwei Stellen (MF-759)

*Messung vor Plan.* Beim Angehen der freigegebenen Reihenfolge sind zwei
Annahmen gefallen.

### P7 — vollzogen

`src/fluxwritejob.cpp:24`, `m_verify(false)` → **`m_verify(true)`**.
Begründung im Quelltext: bei „kein stiller Datenverlust" ist Verify-an
die konsistente Wahl; der Preis ist Zeit je Spur, nicht Richtigkeit. Es
ist zugleich die **einzige Stelle, an der UFT die Vorlage übertrifft**
(B12 überspringt rohkopierte Spuren, UFT prüft sie auf Rohfluss-Ebene)
— und eine Fähigkeit, die per Vorgabe aus ist, wirkt wie keine.

### Berichtigung 1: `uft_crc_correction_v2.c:333` ist die falsche Stelle

Dort steht `correct_sector()`, und die Funktion hat **keinen Aufrufer**.
Ein Bit dort wäre das vierte Stück Bestand ohne Tür.

Die lebende Verwandte heißt **`uft_forensic_correct_sector()`**
(`src/recovery/uft_forensic_recovery.c:654`, öffentlicher Header, mit
`tests/test_forensic_crc_honesty.c`). Sie führt das Gewünschte sogar
**schon mit**: `sector->quality.corrections_applied++` und einen
Fehlersatz mit `recovery_attempted / recovery_successful`.

**Aber auch sie hat keinen Produktionsaufrufer** — Zeile 921 ist ein
Kommentar, der ausdrücklich sagt, dass sie dort *nicht* gerufen wird,
und der Ehrlichkeitstest sagt dasselbe.

> **Die „eine Zeile" ist also eine Zeile plus ein Aufrufer, den es nicht
> gibt.** Einen Aufruf in den Forensik-Pfad zu setzen, ist eine
> Verhaltensänderung an einer Wiederherstellungskette — kein Bit.
> **Eigentümer-Entscheidung**, kein Agentenschritt.
>
> Was fehlt, ist außerdem kein „gerettet"-Bit, sondern ein
> **„hergeleitet"-Merkmal**: gemessen gibt es im ganzen Baum **kein**
> Feld, das sagt „diese Bytes wurden nicht gelesen, sondern erzeugt".
> Das ist die B8-Auflage, und sie ist breiter als P3.

### Berichtigung 2: es gibt eine **dritte** Sync-Suche, und sie ist die beste

Meine P2-Antwort nannte zwei Fassungen. Es sind drei, und die dritte
ist als einzige **erreichbar**:

| | Ebene | erreichbar |
|---|---|---|
| `uft_fuzzy_sync_v2.c` | Bitstrom, Hamming-Toleranz | nein |
| `uft_track_analysis.c` | Bitstrom, `rol32` | wird entfernt |
| **`uft_flux_sync_search.c`** | **Flussstrom, VOR der PLL** | **ja** — `uft_flux_decoder.c:1507` |

Die dritte ist nicht nur erreichbar, sondern der Sache nach überlegen:
sie sucht **maßstabsunabhängig** nach Abstandsverhältnissen, findet
Marken also auch dann, wenn die PLL den Takt verliert. Die Begründung
im Kopf ist gemessen (FLUX-11, MF-487): bei 0,85 der wahren Zellendauer
und 4 % Zittern kommen **0 von 11** Sektoren zurück, *obwohl alle elf
Marken unverändert im Strom stehen*.

**Und sie nimmt bereits einen konfigurierbaren Sync-Satz entgegen** —
`opts->sync_patterns` / `opts->sync_count`, mit Rückfall auf `0x4489`
allein (`uft_flux_decoder.c:1491-1494`). Das ist B5s Anforderung, schon
gebaut.

### Die Lücke ist damit eine Verbindung, kein Neubau

Gemessen: die **Plattform-Profile führen bereits Sync-Sätze** —
`PC98_SYNCS`, `X68000_SYNCS`, `ELECTRON_SYNCS`, `ENTERPRISE_SYNCS`,
`EINSTEIN_SYNCS`, `MEMOTECH_SYNCS`, und für Amiga `{0x4489}`
(`src/analysis/profiles/*.c`).

```
Profil trägt Sync-Satz          ✔  uft_platform_profile_t.sync_patterns
Dekoder nimmt Sync-Satz         ✔  flux_decoder_options_t.sync_patterns
etwas verbindet beide           ✘
```

**Das ist P2 in einem Satz.** Und es erklärt, warum Löschung und P2
derselbe Commit sind: `uft_platform_profile_t` wird von der zu
entfernenden Datei deklariert — die Profile hängen genau daran.

> **Die Neuverbindung geht also nicht gegen `fuzzy_sync_v2`**, wie ich
> zuvor geschrieben habe, sondern gegen `uft_flux_sync_search` — die
> Fassung, die schon eine Tür hat. Der Typ `uft_platform_profile_t`
> muss dabei aus der entfernten Datei in einen eigenen Kopf wandern;
> gemessen brauchen die sieben Profil-Dateien daraus **nur zwei
> Deklarationen** (MF-744).

### Hamming-Toleranz: die Regel ist gemessen und bestätigt

Nachgerechnet:

```
0x4489 ⊕ 0x448A = 0x0003   → Abstand 2
0x448B ⊕ 0x4489 = 0x0002   → Abstand 1
0x448B ⊕ 0x448A = 0x0001   → Abstand 1
```

Mit Toleranz 1 fände UFT `0x448B` als Treffer für **beide** Muster —
einen Sync, den X-Copy nie findet. **Toleranz 0 für den Schutzsatz** ist
damit keine Vorsichtsmaßnahme, sondern die Bedingung dafür, dass ein
Oracle-Vergleich überhaupt aussagekräftig ist.

`uft_flux_sync_search` benutzt `UFT_SYNC_DEFAULT_TOL` — was diese
Toleranz für den Schutzsatz bedeutet, ist **vor** der Verdrahtung zu
messen.

### Stand der freigegebenen Reihenfolge

| | Stand |
|---|---|
| **P7** | **vollzogen** |
| **Code 1** (Emit-Stelle) | offen, unverändert machbar |
| **gerettet / P3 / B8** | **umgeleitet** — braucht einen Aufrufer, Eigentümer-Entscheidung |
| **P2** | **Ziel korrigiert** — gegen `uft_flux_sync_search`, nicht `fuzzy_sync_v2`; Toleranzfrage vorher messen |
| Code 5, P4, Code 3 | offen, unverändert |


---

## Code 1 — die drei Teile sind vier, und der vierte fehlt (MF-761)

Der Auftrag lautete: `expected_sectors` dort lesen, wo es berechnet
wird, und die zwei deklarierten Events erzeugen. **Eine Emit-Stelle.**

Die Stelle stimmt. `otdr_track_detect_events(otdr_track_t *track, const
otdr_config_t *cfg)` hat beides zur Hand, und der Abschnitt
`/* --- Track-level events --- */` zeigt am LONG_TRACK-Fall genau das
Muster. Es wäre wirklich ein Block gewesen.

### Die Falle

```
otdr_track_t.sector_count   deklariert  floppy_otdr.h:258
                            gelesen     floppy_otdr.c:999, 1000, 1338
                            GESETZT     nirgends
```

Ein Vergleich `sector_count != expected_sectors` feuerte damit auf
**jeder** Spur, weil der Wert immer 0 ist. Nicht Verdrahtung, sondern
ein Ereignis, das nichts bedeutet.

### Und der Fund dahinter ist größer als Code 1

Die drei Lesestellen sind keine Nebensache:

```c
total_sectors += trk->sector_count;                    /* :999  immer 0 */
for (uint8_t s = 0; s < trk->sector_count; s++) {      /* :1000 laeuft nie */
    if (trk->sectors[s].data_ok) good_sectors++;
    else                         bad_sectors++;        /* beide bleiben 0 */
}
...
fprintf(f, "... %5u   %4u     %s
", ..., trk->sector_count, notes);  /* :1338 */
```

**Der Bericht erreicht den Benutzer.** `otdr_disk_export_report()` wird
aus `src/gui/uft_otdr_panel.cpp:651` gerufen — die Oberfläche
exportiert ihn.

Er druckt für **jede** Spur eine Sektorzahl, die strukturell null ist,
und weist „gute" und „schlechte" Sektoren aus, die beide null bleiben,
weil die Schleife nie läuft.

> **Das ist die Klasse aus MF-569/735** — eine Anzeige, deren Werte
> nicht aus dem Abbild stammen. Mit einem Unterschied: dort gab der
> Quelltext es zu („Placeholder"), und deshalb konnte Tor 34 es fangen.
> **Hier gesteht er nichts ein**, und das Tor ist blind dafür.
>
> Für einen Forensiker ist eine Spurtabelle mit lauter Nullen in der
> Sektorspalte von einer echten nicht zu unterscheiden — sie sieht aus
> wie eine leere Diskette.

### Die Reihenfolge ist damit vier Schritte, nicht einer

1. **`track->sector_count` füllen** — die Zahl kommt aus dem Dekoder,
   nicht aus dem OTDR. Das ist ein Datenfluss zwischen zwei Schichten,
   kein Emit.
2. Erst dann die Ereignisse `OTDR_EVT_EXTRA_SECTOR` /
   `OTDR_EVT_MISSING_SECTOR` erzeugen — beide Richtungen, wie die
   2.x/3.4-Liste es verlangt („less or **more** than 11").
3. Der Bericht wird dabei nebenbei ehrlich.
4. Solange 1 nicht steht, gehört in den Bericht ein **Vorbehalt** statt
   einer Null — dieselbe Behandlung, die MF-569 der Belegungskarte
   gegeben hat („?" statt „F").

**Schritt 4 ist sofort machbar und unabhängig vom Rest.** Er ist auch
der einzige, der ohne Eigentümer-Entscheidung auskommt: eine Anzeige,
die nichts weiß, soll das sagen.

Ich habe **nichts verdrahtet** — ein Ereignis auf jeder Spur wäre
schlimmer als keines.
