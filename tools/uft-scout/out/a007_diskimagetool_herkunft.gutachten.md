# Gutachten A-007 — `DiskImageTool-extrakt.zip` (Herkunft · Schreibnähte · PC-Schutzmuster)

**Auftrag:** „arbeite alles sehr genau aus / finde alles und alles raussuchen
was ich übersehen habe / - wo können die formate verbessert werden / - ist es
auf andere formate übertragbar / - welche einstellungen fehlen noch / - was
habe wir noch nicht / - brauch es eine HAL-Erweiterungen / erstelle mir code
Beispiele was besser gemacht werden kann / plan verstanden, was kannst du
besser machen ??"

**Stand:** 2026-09-16 · Baum bei `6dbb4ebf` (MF-1185) · Posten `A-007`
**Kein Byte des Pakets ist in den Baum geschrieben.** Gemessen an einer Kopie
unter `$CLAUDE_JOB_DIR/tmp/a007`.

---

## Ergebnis in einem Satz

Von den drei Funden **fehlt einer ganz** (PC-Schutz aus Fehlsektormustern),
**fehlt einer für den Datenträger, während seine Bausteine da sind**
(Bootstrap-Herkunft), und **einer ist der dritte Anlauf derselben Größe** —
der Baum hat schon zwei Schreibnaht-Erkenner und ein Etikett, das niemand
setzt, und **keiner davon hat einen Produktivaufrufer**. Dazu ein Nebenfund,
der über dieses Gutachten hinausreicht: die Lizenzprämisse des Pakets trägt
nicht nur, sie ist **belegt** — und sie stellt das Urteil von `P3-385` in
Frage.

---

## 1. Was das Paket über sich sagt — geprüft

| Zusage | Messung |
|---|---|
| baut mit `-Wall -Wextra -Werror` | **trägt.** Auch mit `-pedantic` (gcc 13.1.0): **0 Warnungen** |
| Tests bestehen | **trägt.** 6 Tests, `BESTANDEN (0 Fehler)` |
| Tests tragen Rot-Proben | **trägt, dreifach** — siehe unten |
| unterscheidet „unbekannt" von „nicht im Bestand" | **trägt.** Die Ausgabe sagt wörtlich: „Bootstrap: 16 Byte, CRC32 DEEBAD83 — nicht in der Datenbank. Das heisst NICHT ‚unbekanntes Werkzeug', sondern ‚dieser Bestand kennt ihn nicht.'" |

**Die drei Rot-Proben sind echt und eine ist lehrreich:**

* Test 3: „ohne den Rueckfallschritt meldete JEDE Diskette aus einem
  Windows-Rechner einen Fehlalarm"
* Test 4: „ein Pruefer, der fest auf **0x4E** vergleicht, meldete hier
  **20 Naehte statt keiner**"
* Test 6: „mit defekten Raendern 0 Treffer statt 1 — ohne die
  Negativbedingung waere jede gealterte Diskette ‚geschuetzt'"

Die zweite trifft den Baum, nicht nur das Paket — siehe §5.

**Eine Entwurfsentscheidung ist richtig und begründet:**
`uft_bs_extract_bootstrap()` schneidet nachlaufende Nullen ab, mit dem Grund
im Code — „Nachlaufende Nullen gehoeren nicht zum Code, sonst haengt die CRC
[vom Fuellwerk ab]". Ohne das wäre der Schlüssel von der Polsterung abhängig
und damit wertlos. Die 16 Byte in der Beispielausgabe sind die Codelänge der
**Prüfdatei**, nicht die des Verfahrens.

---

## 2. Fund 1 — Herkunft aus dem Bootstrap-Code: fehlt für den Datenträger, die Bausteine sind da

Gemessen über `git ls-files` je Bezeichner:

```
bootstrap              .c/.cpp  0    .h  1
crc32_boot             .c/.cpp  0    .h  0
oem_name               .c/.cpp  9    .h 12
uft_provenance         .c/.cpp  6    .h  3
uft_fundus_provenance  .c/.cpp  3    .h  1
```

**Der Prüfwert ist da, der Schlüssel nicht.** Der OEM-Name wird an **neun**
Stellen gelesen — `fat_analyze_boot_sector()` legt ihn in
`result->oem_name` —, aber **niemand hasht den Bootstrap-Code**: 0 Treffer
für `bootstrap` in `.c`, 0 für `crc32_boot`.

**Und die Provenienz-Infrastruktur existiert, nur für das falsche Objekt.**
`uft_provenance` (6 `.c`) und `uft_fundus_provenance` (3 `.c`) führen die
Herkunft von **Korpusdateien** — Werkzeug, Fassung, Weg, Lizenz. Für den
**Datenträger** führt der Baum keine. Die Zulieferung sagt das selbst, und
es trifft zu.

**Verdikt: fehlt.** Nicht „liegt ungerufen da", nicht „zweite Kopie".

### 2a. Nachtrag MF-1187 — die Quelle ist jetzt gelesen, nicht nur zitiert

§12 hat `Modules/BootstrapDB.vb` als **nicht geprüft** benannt. Sie liegt
geklont im Baum (`tools/uft-scout/work/DiskImageTool/DiskImageTool/Modules/`,
213 Zeilen) und ist jetzt gelesen. **Drei Ergebnisse.**

**(1) Die Belegkette trägt, wörtlich.** Das Original bestätigt beide Hälften:

```vb
Private _OEMNameDictionary As Dictionary(Of UInteger, BootstrapLookup)
…
Dim Checksum = CRC32.ComputeChecksum(BootstrapCode)
If _OEMNameDictionary.ContainsKey(Checksum) Then …
```

**CRC32 über den Bootstrap-Code ist der SCHLÜSSEL** (`UInteger` = 32 Bit),
der OEM-Name der **Prüfwert** (`CheckOEMName` vergleicht gegen
`Response.Data.OEMNames`). Auch der Rückfall, den Test 3 der Zulieferung als
Rot-Probe führt, steht im Original:
`If Response.IsWin9x And KnownOEMName.Win9xId Then …`.

**(2) Eine Normalisierung ist NICHT mitgekommen, und sie ist echt.**
`FindXDFMatch()` greift, wenn der Bootstrap-Code genau `&H1AD` = **429 Byte**
lang ist, **nullt die Bytes `0xE7`–`0xEE`** (231–238, **8 Byte**) und rechnet
die CRC neu:

```vb
If BootstrapCode.Length = &H1AD Then Return FindXDFMatch(BootstrapCode)
…
For Counter = &HE7 To &HEE
    BootstrapCode(Counter) = 0
Next
```

Gemessen in der Zulieferung: **0 Treffer** für `0x1AD`, `429`, `0xE7`,
`0xEE`, `xdf`, `XDF`. Ihr `uft_bs_extract_bootstrap()` schneidet nur
nachlaufende Nullen ab. **Eine echte XDF-Diskette verfehlt damit ihren
Eintrag** — acht Byte, die je Datenträger schwanken, gehen in die CRC ein.

Das trifft im Baum eine wunde Stelle: **MF-1087** hat gemessen, dass
`out.xdf` auf **nichts** auflöst, weil die Endung nur in `dim`s Liste stand,
und `ARCH-18` führt `uft_xdf_api_impl.c` als zweite Formatschicht ohne
Aufrufer. XDF ist hier kein Randfall.

**(3) Ein Vorwurf von mir fällt.** Beim ersten Lesen hatte ich den
`Verified`-Zustand als nicht übernommen notiert. **Er ist übernommen:**
`uft_bs_verified_t`, `out->verified = (out->matched->verified ==
UFT_BS_VERIFIED)`, und die Textausgabe unterscheidet „ (verifiziert)" von
„ (NICHT verifiziert)". Der Header quantifiziert es sogar — „490 OEM-Namen,
davon **223** als `verified="true"` markiert, 65 verschiedene JMP-Präambeln,
10 Sprachvarianten".

**Und bei den JMP-Präambeln gehen die zwei Wege auseinander, ohne dass einer
falsch ist.** Das Original iteriert über eine **Menge** von JMP-Bytefolgen
(`For Each JmpString In _JmpInst`) und probiert je Form eine CRC. Die
Zulieferung leitet den Versatz aus dem **Opcode** ab (`0xEB` gegen `0xE9`,
`uft_bootstrap_db.c:56/58`) — strukturell statt aufzählend, und damit näher
an MF-930. **Ob beide Wege dieselbe Menge treffen, ist nicht gemessen** und
braucht die Datenbank.

---

## 3. Fund 2 — Schreibnähte: die Fähigkeit fehlt, das Muster hat drei Vorgänger, und keiner wird gerufen

Die Zulieferung nennt zwei vorhandene Stellen. Gemessen sind es **drei**, und
eine ihrer zwei ist gar keine:

| Stelle | was es ist | Produktivaufrufer |
|---|---|---|
| `src/formats/uff/uft_uff.c:502` `uff_detect_splices()`, Schwelle `UFF_SPLICE_THRESHOLD 500` Ticks | echter Erkenner über eine **Zeitlücke im Fluss** | **0** außerhalb der eigenen Datei |
| `src/formats/g64/uft_g64_parser_v3.c:225` `G64_DIAG_SPLICE_DETECTED` | **ein ETIKETT.** Nur der Aufzählungswert und zwei Texttafeln („Write splice detected", „Normal for written disks") — **nichts setzt es** | entfällt, es gibt keinen Erkenner |
| `src/analysis/deepread/uft_deepread_splice.c`, `uft_deepread_detect_splice()` + `uft_deepread_disk_splice_map()` | echter Erkenner über OTDR-Spuren | **0** — nur die eigene Datei und `tests/test_flux_splice_pos.c` |

**Die Zulieferung übersieht den dritten**, und der ist der interessanteste: es
ist die *Write-Splice Detection* aus `CLAUDE.md` §DeepRead, eines der **7 von
8** Module ohne Aufrufer (MF-627/MF-767).

**Was die Zulieferung richtig sieht:** „Für IBM-MFM-Bitströme gibt es
nichts." Beide echten Erkenner arbeiten auf Fluss bzw. OTDR, nicht auf einem
IBM-MFM-Bitstrom. Der Weg über die **Lückenwerte** ist neu, und er ist
billiger als beide: „Kein Fluss noetig, keine Umdrehungsstruktur", messbar aus
HFE, 86F, MFM, DMK, PRI.

**Verdikt: die Fähigkeit fehlt, das Muster ist der dritte Anlauf.** Ein
vierter Erkenner ohne Aufrufer wäre P3-204 zum vierten Mal. **D2 gilt hier
wörtlich:** kein neuer Algorithmus ohne Aufrufer und ohne einen Test, der rot
wird, wenn der Aufruf entfernt wird.

---

## 4. Fund 3 — PC-Schutz aus Fehlsektormustern: fehlt

```
bad_sector        im ganzen Baum      25 .c-Dateien
bad_sector        in src/protection/   1 Datei (uft_atari8_protection.c)
protect_sig                            0
Musterschluss (bad.sector + muster|pattern|signature|scheme)   0 Treffer
```

**Die Daten sind überall, die Deutung nirgends.** Fehlerhafte Sektoren werden
in 25 `.c`-Dateien geführt; in `src/protection/` nennt sie **eine** Datei, und
**keine** leitet aus dem *Muster* ein benanntes Verfahren ab.

Das passt zu `MF-508`: automatisch läuft die Erkennung von Schutz-**Signalen**,
und der Katalog der benannten Verfahren „ist Bestand, nicht Fähigkeit". Ein
Musterschluss wäre eine neue Brücke zwischen beiden — und die
Negativbedingung aus Test 6 ist genau das, was ihn von `P3-38`/`P3-39`
unterscheidet („die C64-Schutzerkennung rät Markennamen").

**Verdikt: fehlt**, und es ist der Fund mit dem klarsten Weg.

---

## 5. Frage „welche Einstellungen fehlen" — **ZURÜCKGENOMMEN (MF-1187)**

> ### Rücknahme, nachgemessen und nicht bestätigt
>
> Hier stand: „das Literal `0x4E` steht in 37 `.c`-Dateien … ein Füllbyte,
> das 37 Mal als Literal dasteht, ist keine Einstellung." Die Zahl war
> ausdrücklich als **Kandidatenzahl** gekennzeichnet und in §12 als nicht
> geprüft benannt. **Sie ist jetzt geprüft, und der Befund fällt.**
>
> **Was die Stichprobe zeigt.** Von den Fundstellen sind
> `{0x52,0x41,0x49,0x4E}` = „RAIN" (Rainbow Arts) und
> `{0x42,0x4F,0x55,0x4E}` = „BOUN" **Kennungen**; `0x4EFx` und `0x4E75`
> sind **68000-Opcodes** (`JMP`, `RTS`), ebenso
> `hi == 0x4E && (lo == 0x75 …)`; `h[0x4E]` ist ein **Versatz**, kein
> Wert; `0x4E554654u` ist der Same „NUFT"; `0x0E,0x8E,0x4E,0xCE,…` ist
> eine **Tafelzeile**; `{ SETBITRATE, RAND, 0x4E }` und
> `{ SKIPBITS, RAND, 0x4E }` sind **Greaseweazle-Befehlsbytes**; und
> „haeufigstes Byte 0x55 (12498) 0x4E (6712)" ist ein **Messkommentar**.
> **Kein Füllbyte.**
>
> **Und mein Versuch, die echte Teilmenge zu zählen, war selbst falsch.**
> Er lieferte **43 von 36** — arithmetisch unmöglich für eine Teilmenge.
> Ursache gefunden: die erste Messung lief mit `grep -l "0x4E"`
> (Groß/Klein beachtet, **36** Dateien), die zweite still mit `grep -li`
> (**43** = 36 + 7 Dateien mit `0x4e`). Zwei Methoden, eine Zahl.
>
> **Es gibt damit keine belastbare Zahl, und hier steht keine dritte.**
> Der Befund wird zurückgenommen, nicht auf einen anderen Wert korrigiert
> — das wäre die dritte unzuverlässige Messung in Folge. Klasse
> `measurement_hit_wrong_class` („‚0 gefunden' ist keine Entwarnung,
> sondern die Frage: was kann dieses Skript nicht sehen?") und
> `zaehlung_ohne_normalisierung`.

**Was von der Frage übrig bleibt, und es ist nicht nichts:**

Die Rot-Probe von Test 4 der Zulieferung steht unverändert — „ein Pruefer,
der fest auf 0x4E vergleicht, meldete hier **20 Naehte statt keiner**". Das
ist eine Aussage über **ihren** Code, gemessen an **ihrem** Testfall, und sie
trägt.

Gemessen im Baum, mit einer Methode und ohne Ableitung:

```
gap_fill    .c/.cpp  6    .h  2
fill_byte   .c/.cpp  2    .h  5
```

**Ein Begriff für das Füllbyte existiert** — was fehlt, ist eine Messung, wie
viele Stellen ihn *umgehen*. Die habe ich nicht, und ein `grep` auf `0x4E`
liefert sie nicht: im Baum bedeutet das Byte überwiegend etwas anderes. **Eine
grep-getriebene Aufräumung wäre hier gefährlich** — das ist die verwertbare
Erkenntnis aus der gescheiterten Messung.

**Die Frage bleibt damit offen**, nicht beantwortet: gehört das
Lücken-Füllbyte ins FDC-Profil neben `gap_beleg`/`gap_quelle`? Wahrscheinlich
ja, nach MF-1177 — aber der Anlass dafür ist **nicht gemessen**, und nach
MF-1077 darf eine Zahl nicht das Motiv sein. Der Weg zu einer belastbaren
Messung: nicht das Literal zählen, sondern die Aufrufer von
`uft_fdc_gap_space()` gegen die Stellen halten, die eine Lücke **schreiben**.

---

## 6. Frage „übertragbar" — dasselbe Muster zum dritten Mal

„**Dieser Bestand kennt ihn nicht**" statt „unbekanntes Werkzeug" ist
dieselbe Bauform wie `UFT_FLICKER_UNDECIDED` aus `A-006` und dieselbe Regel
wie **MF-980** und **D6**. In drei aufeinanderfolgenden Zulieferungen
(`A-005` hat sie *nicht*, `A-006` und `A-007` haben sie) ist das die
wiederkehrende Stärke — und im Baum ist es die wiederkehrende Lücke:
`A-005`s Abgleicher zählt „unbekannt" als „widerspricht", und hier ist es
richtig gemacht.

**Übertragbar ist also nicht der Code, sondern der dritte Rückgabewert.**

---

## 7. Frage „HAL-Erweiterung" — nein, und das ist eindeutig

Alle drei Funde arbeiten auf einem **Abbild**: Bootsektor, Bitstrom,
Sektorliste. Kein Gerätezugriff, keine Umdrehung, keine Flusszeit. Es gibt
nichts, was die HAL liefern müsste und nicht liefert.

---

## 8. Die Lizenzprämisse — sie trägt, und sie reicht weiter als dieses Gutachten

Die Zulieferung begründet die Zulässigkeit so: UFTs verteilbare Kombination
sei über `uft_kfstream_air.c`, `uft_ipf_air.c` und `uft_stx_air.c` ohnehin
GPL-3. **Nachgemessen, und die Prämisse ist nicht nur richtig, sie ist
dokumentiert:**

```
include/uft/formats/stx/uft_stx_air.h : SPDX-License-Identifier: GPL-3.0-only
src/formats/ipf/uft_ipf_air.c        : SPDX-License-Identifier: GPL-3.0-only
src/formats/kfx/uft_kfstream_air.c   : SPDX-License-Identifier: GPL-3.0-only
src/formats/stx/uft_stx_air.c        : SPDX-License-Identifier: GPL-3.0-only
```

Und der Kopf von `uft_ipf_air.c` sagt es wörtlich:

> „MF-698 — Eigentuemer-Entscheidung: die GPL-3-Bindung wird ANGENOMMEN. …
> UFT selbst steht unter GPL-2.0-or-later; das ‚or later' macht die
> Kombination zulaessig und **bindet die VERTEILBARE Fassung des Gesamtwerks
> an GPL-3**."

Dazu die Begründung für `-only` statt `-or-later`: „der Quellkopf nennt
‚GPL-3.0' ohne Zusatz. `-or-later` waere eine Behauptung ueber Rechte, die
niemand gemessen hat." Das ist die Sorgfalt, die dieser Baum verlangt.

**DiskImageTool ist GPL-3.0 → verträglich mit der verteilbaren Fassung. Der
Kanal *Port* ist für dieses Paket offen.** Die Entscheidung selbst bleibt
beim Eigentümer; gemessen ist, dass die Prämisse einen Boden hat.

**Und die IPF-Quarantäne steht dem nicht entgegen** — sie ist *vollzogen*:
`docs/QUARANTINE.md` führt `src/formats/ipf/uft_caps_ipf.c` (790 Z.) als
**gelöscht MF-1002**, mit dem Verdacht „Based on SPS CAPS Library … kein
SPDX, keine Lizenz". Das war eine **andere** Datei als `uft_ipf_air.c`, und
sie ist weg. Die drei AIR-Dateien tragen SPDX und eine benannte
Eigentümer-Entscheidung.

### Der Nebenfund, der über A-007 hinausgeht

`P3-385` urteilt über `thomas-luebker/AmigaDiskKit` (Posten `A-013`):
„**Lizenz berichtigt:** Apache-2.0, NICHT MIT — **mit GPL-2 unverträglich**,
also **kein Port**, nur Orakel." Dasselbe habe ich heute bei `A-014`
(`SecurityRonin/disk-forensic`, Apache-2.0) in den Posten geschrieben.

**Beide Urteile messen gegen GPL-2 — und die verteilbare Fassung ist nach
MF-698 an GPL-3 gebunden.** Apache-2.0 ist mit **GPLv3** verträglich (nicht
mit GPLv2); die Richtung ist einseitig (Apache-Code darf in ein GPL-3-Werk,
nicht umgekehrt), und die NOTICE-/Attributionspflichten bleiben.

**Damit ist der Port-Kanal für zwei Posten möglicherweise offen, die ihn
heute als geschlossen führen.** Ich entscheide das nicht — es ist eine
**Eigentümer-Entscheidung** und eine Rechtsfrage, nicht eine Messung. Gemessen
ist: (a) MF-698 existiert und bindet die verteilbare Fassung an GPL-3,
(b) `P3-385`s Begründung nennt GPL-2. Das gehört als Vorschlag nach
`docs/OPEN_ITEMS.md`, und es ist der Grund, warum dieses Gutachten seinen
Lizenzabschnitt nicht in zwei Zeilen abhandelt.

---

## 9. Die Datenbank fehlt — dieselbe Frage wie bei `A-005`

Die Zulieferung sagt es selbst: „Die Datenbank selbst ist nicht im Baum. Der
Wandler verweist auf den Originalbestand; wer ihn nicht hat, hat **379 leere
Einträge**." Der Wandler ist `tools/bootstrap_xml_to_c.py`, und er ist
sorgfältig — sein Kommentar warnt ausdrücklich: „namehex UND name werden zu
acht Rohbytes. Viele Eintraege sind KEIN Text — wer sie als C-Zeichenkette
ablegt, endet am ersten Nullbyte."

**Das ist dieselbe Lage wie die Wikipedia-Ernte in `A-005` und die
OmniFlop-239-Ernte:** eine fremde **Datenbank**, deren Weitergabe eine eigene
Frage ist, unabhängig von der Codelizenz. Sie steht hier als Frage.

---

## 10. Code-Beispiele

### 10.1 Der vierte Schreibnaht-Erkenner braucht einen Aufrufer, nicht mehr Code

**Ersetzt:** nichts — es ergänzt, was die Zulieferung nicht mitliefert.
**Kennzahl:** keine der vier. **Regel:** D2, und P3-204 zum vierten Mal
vermeiden.

```c
/* Die Zulieferung liefert `uft_splice_scan_mfm()`. Was fehlt, ist die
 * Tuer — und der Baum hat dreimal gezeigt, wie es ohne sie endet:
 *   uff_detect_splices()            0 Aufrufer
 *   uft_deepread_detect_splice()    0 Aufrufer
 *   G64_DIAG_SPLICE_DETECTED        ein Etikett, das niemand setzt
 *
 * Also zuerst der Aufrufer, dann der Erkenner. Die Stelle ist der
 * Bitstrom-Pfad, den es schon gibt: */
uft_splice_report_t naht;
if (uft_splice_scan_mfm(track_bits, track_len, &naht) == UFT_OK
    && naht.count > 0u) {
    uftc_add_warning(result,
        "track %d/%d: %zu write splices at gap offsets %zu.. — this track "
        "was written sector-wise after formatting, not duplicated in one "
        "pass (provenance, not an error).",
        cyl, head, naht.count, naht.first_offset);
}
```

**Rotbeweis-Skizze:** eine Spur, die in einem Durchgang geschrieben wurde,
muss **0** Nähte melden; dieselbe Spur mit einem neu geschriebenen Sektor
muss **2** melden (Ein- und Ausschaltpunkt). Ohne die erste Hälfte wäre
„findet immer Nähte" genauso grün. Und ein dritter Fall: entfernt man den
Aufruf, muss der Test **rot** werden — sonst ist es der vierte ungerufene
Erkenner.

### 10.2 Das Füllbyte gehört ins Profil

**Ersetzt:** die 37 `0x4E`-Literale (Kandidatenzahl, §5).
**Kennzahl:** keine der vier. **Regel:** MF-1177.

```c
/* uft_fdc_profile_t traegt seit MF-1177 gap_beleg und gap_quelle.
 * Das Fuellbyte gehoert daneben — mit derselben Sorgfalt: */
typedef struct {
    /* … vorhandene Felder … */
    uint8_t gap_fill;        /* 0x4E bei IBM System 34; NICHT allgemein */
    const char *fill_beleg;  /* „gemessen an <Abbild>" oder NULL        */
    const char *fill_quelle; /* „ECMA-54 §…" — die Norm, nicht ein Wert */
} uft_fdc_profile_t;
```

**Rotbeweis-Skizze:** ein Profil ohne `fill_beleg` **und** ohne
`fill_quelle` darf sein `gap_fill` nicht an einen Prüfer geben — der Aufruf
muss absagen. Gemessen wäre das an genau dem Fall aus Test 4 der
Zulieferung: 20 gemeldete Nähte, wo keine sind.

### 10.3 Der Bootstrap-Schlüssel gehört zur Provenienz, nicht daneben

**Ersetzt:** nichts; es verbindet `uft_bs_identify()` mit
`src/forensic/uft_provenance.c` (6 `.c`-Nenner).
**Kennzahl:** keine der vier; berührt die fünfte (MF-640).

```c
/* Der Baum fuehrt Provenienz fuer DATEIEN. Der Bootstrap-Schluessel ist
 * Provenienz fuer den DATENTRAEGER — dasselbe Feldwerk, anderes Objekt.
 * Also kein zweites Modell (A-011 hat gemessen, dass es schon mindestens
 * acht Header mit „metadata" gibt), sondern ein Feld: */
typedef struct {
    /* … vorhandene Provenienzfelder der Datei … */
    uint32_t    traeger_bootstrap_crc32;  /* 0 = nicht gemessen        */
    const char *traeger_oem_name;         /* 8 Byte, NICHT 0-terminiert */
    const char *traeger_werkzeug;         /* NULL = Bestand kennt es nicht,
                                           * NICHT „unbekannt"         */
} uft_provenance_t;
```

---

## 11. Was ich besser machen kann als das Paket

1. **Den dritten Erkenner finden.** Die Zulieferung nennt zwei
   Schreibnaht-Stellen; es sind drei, und eine ihrer zwei ist nur ein
   Etikett. Der übersehene ist der aussagekräftigste, weil er zur
   DeepRead-Klasse gehört (7 von 8 ohne Aufrufer).
2. **Die eigene Rot-Probe gegen den Baum halten.** Test 4 warnt vor einem
   festen `0x4E`. Im Baum steht das Literal in 37 `.c`-Dateien. Die
   Zulieferung prüft ihren eigenen Code; ich prüfe, ob der Befund auf den
   Baum zutrifft.
3. **Die Lizenzprämisse nicht nur bestätigen, sondern ihre Folgen ziehen.**
   Sie trägt — und sie stellt `P3-385`s Urteil in Frage, das gegen GPL-2
   messt, während MF-698 die verteilbare Fassung an GPL-3 gebunden hat. Das
   betrifft zwei andere Posten dieser Warteschlange.

**Was ich NICHT besser kann:** den Musterschluss auf Fehlsektoren. Die
Negativbedingung aus Test 6 — „mit defekten Rändern 0 Treffer statt 1" — ist
genau der Unterschied zwischen einem Befund und dem Ratefehler aus
`P3-38`/`P3-39`, und der Baum hat nichts Vergleichbares.

---

## 12. Was nicht geprüft ist

* **Die 379 Datenbankeinträge.** Sie sind nicht im Paket; geprüft ist nur der
  Wandler und sein Warnkommentar.
* **`uft_pc_protect_sig.c` gegen echte geschützte PC-Disketten.** Im Korpus
  liegt keine; alle sechs Tests sind synthetisch.
* ~~**Ob `0x4E` in den 37 Dateien wirklich ein Füllbyte ist.**~~
  **GEPRÜFT MF-1187 — und der Befund ist ZURÜCKGENOMMEN**, siehe §5. Die
  Stichprobe zeigt Kennungen, 68000-Opcodes, einen Versatz, eine
  Tafelzeile und Befehlsbytes; meine Nachzählung lieferte 43 von 36 und
  war selbst falsch (`grep -l` gegen `grep -li`).
* ~~**Der Originalbestand `Modules/BootstrapDB.vb`**~~ **GELESEN MF-1187**,
  siehe §2a: die Belegkette trägt, die XDF-Normalisierung fehlt der
  Zulieferung, und der `Verified`-Zustand ist — anders als ich zuerst
  notierte — übernommen.
* **Die 379 Datenbankeinträge** bleiben ungeprüft — sie sind nicht im
  Paket, und der Originalbestand liegt als XML beim Upstream.
* **`uft_pc_protect_sig.c` gegen echte geschützte PC-Disketten** bleibt
  ungeprüft: im Korpus liegt keine, alle sechs Tests sind synthetisch.
* **Ob die 65 JMP-Präambeln des Originals und die zwei Opcode-Zweige der
  Zulieferung dieselbe Menge treffen** — braucht die Datenbank (§2a).

---

## 13. Vorschläge für `docs/OPEN_ITEMS.md`

Fünf, keiner eingetragen — Eigentümer-Entscheidung.

| Vorschlag | Kern | Kennzahl |
|---|---|---|
| **V1** | **Der wichtigste:** `P3-385` urteilt „Apache-2.0, mit GPL-2 unverträglich, kein Port" — MF-698 hat die verteilbare Fassung aber an **GPL-3** gebunden, und Apache-2.0 ist GPLv3-verträglich. Betrifft `A-013` und `A-014` | keine der vier |
| **V2** | Drei Schreibnaht-Stellen, zwei Erkenner mit **0** Produktivaufrufern, ein Etikett das niemand setzt (`G64_DIAG_SPLICE_DETECTED`) | keine der vier |
| ~~**V3**~~ | **ZURÜCKGENOMMEN MF-1187** (§5). Die 37 war eine Kandidatenzahl und hat die Prüfung nicht überstanden; meine Nachzählung war selbst falsch. Ein Vorschlag bleibt möglich, aber er braucht eine andere Messung: die Aufrufer von `uft_fdc_gap_space()` gegen die Stellen halten, die eine Lücke **schreiben** | keine der vier |
| **V6** | **NEU MF-1187** (§2a). `Modules/BootstrapDB.vb` normalisiert XDF-Bootstraps — bei Länge `&H1AD` = 429 Byte werden `0xE7`–`0xEE` (8 Byte) genullt, dann CRC. Die Zulieferung hat das nicht (0 Treffer). Trifft die XDF-Lage aus MF-1087 und ARCH-18 | keine der vier |
| **V4** | Provenienz gibt es für **Dateien** (`uft_provenance` 6 `.c`), nicht für den **Datenträger**; der OEM-Name wird 9× gelesen, der Bootstrap-Code 0× gehasht | keine der vier |
| **V5** | Fehlerhafte Sektoren werden in 25 `.c` geführt, und **keine** Datei leitet aus dem Muster ein benanntes Verfahren ab — die Brücke zwischen MF-508 (Signale) und dem Katalog (Namen) | keine der vier |

---

## Abnahme

* Kein Byte des Pakets unter `src/`, `include/`, `tests/` — geprüft über
  `git status --short`.
* Jede Zahl gemessen; die Messungen sind `git ls-files`-Zählungen je
  Bezeichner und im Text mit ihrer Methode genannt.
* Eine Zahl ist ausdrücklich als **Kandidatenzahl** gekennzeichnet (37 ×
  `0x4E`), weil die Methode Fehltreffer zulässt.
* Die Lizenzfragen stehen als Fragen; der Nebenfund zu `P3-385` ist als
  Eigentümer-Entscheidung markiert, nicht als Urteil.
* Vier Dinge sind ausdrücklich als nicht geprüft benannt (§12).
