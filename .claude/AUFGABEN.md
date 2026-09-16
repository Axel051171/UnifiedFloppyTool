# Arbeitsliste

**Ein Posten ist in Arbeit. Alle anderen warten.**

Gepflegt vom Skill `.claude/skills/aufgabe/SKILL.md` (`/aufgabe`).
Aufnahme startet keine Arbeit. Abschluss verlangt einen Beleg.

**Diese Liste hält Aufträge, keine Befunde.** Ein Befund gehört nach
[`docs/OPEN_ITEMS.md`](../docs/OPEN_ITEMS.md) — seit MF-588 die einzige
Liste dafür, „neue Befunde kommen dorthin, nirgendwo sonst". Hier steht
nur, **was wann bearbeitet wird**, und der Posten verweist auf seine
`P?-NNN`, statt sie zu wiederholen.

Nichts wird gelöscht: Erledigtes wandert nach unten, Verworfenes bekommt
eine Zeile `Zurückgenommen:` mit Grund. Nummern (`A-001`, `A-002`, …)
laufen fort und werden nie wiederverwendet.

> **Warum die Zählung bei `A-004` beginnt.** `A-001` bis `A-003` wurden am
> 2026-09-16 bei der Abnahme des Skills selbst verbraucht (Paarlauf mit
> zwei Unteragenten, Sitzungsprotokoll). Sie stehen in keiner Fassung
> dieser Datei, und wiederverwendet werden sie nach der Regel oben nicht.

---

## In Arbeit

*(höchstens einer — leer heißt: nichts läuft)*

**Leer.** `A-007` ist **erledigt** (Gutachten, MF-1186); sein Eintrag steht
unverändert an seinem Platz in der Warteschlange unten und trägt dort Status,
Stand und Beleg. Er wird hier **nicht** wiederholt — zwei Fassungen desselben
Postens wären zwei Wahrheiten (D3).

Erledigt: `A-005` (MF-1184 = `2aa7bda6`), `A-006` (MF-1185 = `6dbb4ebf`),
`A-007` (MF-1186). `/aufgabe weiter` zieht **`A-008`** hoch (Zulieferung
`UFT_C64PP_Protection_Catalog.zip`).

> **Ein Befund aus `A-007` betrifft zwei wartende Posten.** MF-698 bindet
> die verteilbare Fassung an GPL-3, und Apache-2.0 ist GPL-3-verträglich —
> die Kanal-Zeilen von `A-013` und `A-014` messen dagegen gegen GPL-2 und
> führen den Port als geschlossen. Sie bleiben unverändert stehen, bis der
> Eigentümer entscheidet; das Gutachten `a007_…` §8 nennt die Messung.

---

## Warteschlange

*(Reihenfolge = Bearbeitungsreihenfolge; oben ist als Nächstes dran)*

### A-005 · Zulieferung `UFT_Floppy_Reference_AARD_Paket.zip` begutachten
- **Status:** **erledigt** 2026-09-16 (Begutachtung; der EINBAU ist offen und
  wäre ein eigener Posten) · **Aufgenommen:** 2026-09-16 · hochgezogen mit
  „weiter zieht A-005 hoch". Der Posten bleibt an dieser Stelle stehen, bis
  der nächste `weiter`-Griff ihn nach `Erledigt` verschiebt — verschoben wird
  er dann als Ganzes, nicht kopiert.
- **Wortlaut:** „arbeite alles sehr genau aus / finde alles und alles
  raussuchen was ich übersehen habe / - wo können die formate verbessert
  werden / - ist es auf andere formate übertragbar / - welche einstellungen
  fehlen noch / - was habe wir noch nicht / - brauch es eine
  HAL-Erweiterungen / erstelle mir code Beispiele was besser gemacht werden
  kann / plan verstanden, was kannst du besser machen ??"
- **Kennzahl:** **keine der vier → und das ist hier keine Absage.** Der
  Auftrag ist eine **Beurteilung**, kein Einbau; MF-640 regelt, was in die
  Arbeitsliste geplant wird, nicht was der Eigentümer bestellt. Welche der
  vier ein Einbau bewegen würde, ist Teil der Antwort und wird gemessen,
  nicht behauptet. Vorwegnahme ausdrücklich nicht: die Wikipedia-Tafel ist
  eine **Sekundärquelle**, und eine Sekundärquelle hebt nach dem Maßstab
  von `docs/ORACLES.md` keine Tier-Stufe.
- **Kanal:** zweiteilig, und das muss es bleiben. **Daten** —
  `floppy_reference_wikipedia.tsv` (113 logische + 24 physische Sätze),
  geerntet aus „List of floppy disk formats", **CC BY-SA 4.0**, also
  *copyleft* und mit derselben Frage wie die OmniFlop-239-Ernte
  (EU-Datenbankherstellerrecht, §§ 87a ff. UrhG). **Code** — `LICENSE` des
  Pakets ist GPL-2.0-or-later, © 2024–2026 Axel Kramer, also **eigener**
  Code und keine Fremdübernahme. Für die Begutachtung selbst: *gelesen,
  nicht eingebaut*.
- **Einfrier-Regel:** für die **Begutachtung** nein (kein Code). Für einen
  **Einbau** ja, mittelbar: `src/formats/reference/` liegt im Format-Layer
  und der Abgleicher rangiert Format-Kandidaten — also Rotbeweis zuerst,
  benannte Referenz im Header, jede Zahl gemessen.
- **OPEN_ITEMS:** keiner. Gemessen 2026-09-16:
  `grep -n 'OmniFlop\|Wikipedia\|CC BY-SA' docs/OPEN_ITEMS.md` findet keine
  Zeile zu dieser Ernte. Fachlich berührt, aber **nicht** erledigt:
  `LIZ-1` (Attributions-Zensus) und `P3-435` (Spurbilanz ohne Quelle →
  kein FDC-Profil, S1).
- **Bei der Aufnahme gemessen — eine Zusage des Pakets trägt nicht:**
  `INTEGRATION.md` sagt „The current UFT workspace is already wired" und
  „The repository copy already contains this wiring in
  `tests/CMakeLists.txt`". `git ls-files | grep -iE 'aard|floppy_reference'`
  ist **leer** — kein Byte des Pakets liegt im Baum, weder Quelle noch Test
  noch Daten. Das ist Teil des Prüfauftrags, nicht seine Antwort.
- **Fertig heißt:** ein Gutachten als Dokument (`tools/uft-scout/out/` oder
  `docs/`), das **(a)** jede der fünf Fragen mit einer Messung gegen den
  Baum beantwortet (`git grep` je Bezeichner, keine Aufzählung — MF-930),
  **(b)** jeden Code-Vorschlag an `Datei:Zeile` der Stelle festmacht, die er
  ersetzt, mit der Kennzahl nach MF-640, **(c)** die Lizenzfrage der Ernte
  als benannte **Eigentümer-Entscheidung** stehen lässt statt sie zu
  empfehlen, und **(d)** kein Byte des Pakets nach `src/`, `include/`,
  `tests/` oder `data/` schreibt. Der Einbau ist ein **eigener** Posten mit
  eigenem Rotbeweis.
- **Aufwand:** Begutachtung eine Sitzung. Einbau **nicht schätzbar**, weil
  er an der Lizenzentscheidung hängt.
- **Stand:** **Begutachtung fertig.** Gutachten liegt als
  `tools/uft-scout/out/a005_floppy_reference_aard.gutachten.md` (571 Zeilen).
  Alle fünf Bedingungen der Zeile `Fertig heißt` sind erfüllt: fünf Fragen je
  mit Messung, vier Code-Vorschläge je an `Datei:Zeile` mit Kennzahl, die
  Lizenzfrage steht als Frage, und `git status --short` zeigt **kein** Byte
  des Pakets unter `src/`, `include/`, `tests/`, `data/`.
  **Kernbefunde:** der Abgleicher sagt bei Gleichstand nicht ab — für eine
  PC-1,44-M-Diskette drei Sätze mit score 100, 7 verglichen, 0 abweichend,
  und keiner ist der PC-Satz; „unbekannt" und „widerspricht" sind dasselbe
  (3 von 3 gemessen); von acht angeblich fehlenden Einstellungen hat der Baum
  **fünf**; die physische Achse (`coercivity`/`tracks_per_inch`/
  `bits_per_inch`) ist mit **je 0 Treffern** die einzige echte Lücke; die
  AARD-Flagge feuert auf das Wort **AARDVARK** (confidence 45), während ihre
  Zufallsrate gemessen in Ordnung ist (6 gegen 5,60 erwartet, 0 von 64
  Flaggen). **Der Einbau ist NICHT gemacht** und braucht die
  Lizenzentscheidung — das ist ein eigener Posten, wenn der Eigentümer ihn
  will.
- **Beleg:** Gutachten `tools/uft-scout/out/a005_floppy_reference_aard.gutachten.md`,
  **MF-1184 = `2aa7bda6`** (2 Dateien, 1012 Zeilen, alle Vorhaken grün).
  Nachgetragen wie angekündigt: die Zeile lag im MF-1184-Commit selbst und
  konnte ihren eigenen Hash nicht kennen.

### A-006 · Zulieferung `hacking floppy disk.zip` (Kopierschutz/Umdrehungen) begutachten
- **Status:** **erledigt** 2026-09-16 (Begutachtung; der EINBAU ist offen) ·
  **Aufgenommen:** 2026-09-16 · hochgezogen mit „weiter zieht A-006 hoch"
- **Wortlaut:** „finde alles und alles raussuchen was ich übersehen habe /
  - wo können die formate verbessert werden / - ist es auf andere formate
  übertragbar / - welche einstellungen fehlen noch / - was habe wir noch
  nicht / - brauch es eine HAL-Erweiterungen / erstelle mir code Beispiele
  was besser gemacht werden kann / plan verstanden, was kannst du besser
  machen ??"
- **Kennzahl:** **keine der vier für die Begutachtung.** Ein Einbau würde
  keine der vier bewegen — Schutzerkennung ist keine Tier-Stufe, kein
  Wandlungspfad, kein Leck, kein Bench-Alter. **Das ist genau der Fall, für
  den MF-640 die fünfte Zahl offengelassen hat**, und der Punkt gehört bei
  der Begutachtung benannt, nicht umgangen. Die Zulieferung nennt sich
  selbst „T2-Niveau, nicht T1b" — sie weiß das.
- **Kanal:** **Spec.** Die Belegkette nennt DrCoolZic Rev 1.2 („Copyleft",
  laut Zulieferung zitieren und Werte nehmen, nicht Zeilen), Chris Evans
  2020 und Atari-Forum-Faden 21952. Der **Code** des Pakets (`uft_revolution
  .c/.h`, `uft_protection_scan.c/.h`, `test_protect.c`) trägt in der
  Zulieferung **keine Lizenzzeile** — gemessen: `grep` nach `SPDX`, `GPL`,
  `MIT`, `Lizenz`, `License` findet im Analysedokument nur die
  DrCoolZic-Zeile, und im Paket liegt **keine `LICENSE`-Datei** (anders als
  bei A-005). Herkunft und Lizenz des Codes sind damit bei der Aufnahme
  **ungeklärt** und vor jedem Einbau zu klären (`LIZ-1`-Klasse).
- **Einfrier-Regel:** **ja.** `src/protection/` und der Flusspfad sind
  Decoder-Layer. Rotbeweis zuerst, benannte Referenz im Header, jede Zahl
  gemessen — und die Zulieferung sagt selbst, dass **kein geschütztes
  Abbild im Korpus** liegt, also ist alles darin synthetisch geprüft.
- **OPEN_ITEMS:** **Treffer, und sie sind der Kern der Prüfung.** `P3-236`
  (Umdrehungsvergleich zur Weak-Bit-Erkennung, ✅ behoben), `P3-237`
  (Weak-Bit-Erkennung für mehrfach umdrehende Formate, ✅ geklärt),
  `P3-238` (HAL reicht Umdrehungsgrenzen durch, ✅ so weit es ohne Gerät
  geht), `P0-2` (Schutzkatalog ohne Aufrufer, ✅ MF-983). Dazu offen:
  `P3-41` (`uft_fuzzy_bits.c`, vier Mängel), `P3-46`/`P3-47` (Index- und
  Ovl16-Randfälle), `P3-222` (Phantom-Modul `uft_flux_analysis.h`).
- **Bei der Aufnahme gemessen — zwei Zahlen der Zulieferung, zwei
  Ergebnisse:** ihre Kopftafel sagt `uft_flux_revolution_t` habe 2 Header
  und **0** `.c`-Dateien; das **trifft zu** (gemessen über `git ls-files`).
  Ihre Aussage, die HAL reiche Umdrehungen nicht durch, steht dagegen neben
  drei ✅-Punkten: `uft_hal_read_flux_ex` hat **2** `.c`-Aufrufer, und
  `uft_multi_rev_fusion.c/.h` liegt im Baum. Die Zulieferung will
  `uft_revolution.c/.h` **neu** anlegen — Dateien dieses Namens gibt es
  nicht, ein Mehrfach-Umdrehungs-Modul aber schon. Ob das eine Lücke oder
  eine **zweite Kopie** derselben Rechnung ist (Klasse MF-1015/MF-1026/
  MF-1177), ist die erste zu klärende Frage, nicht ihre Antwort.
- **Fertig heißt:** ein Gutachten als Dokument, das **(a)** jede der fünf
  Fragen mit einer Messung gegen den Baum beantwortet, **(b)** je Aussage
  der Zulieferung „trifft zu / überzeichnet / veraltet seit MF-NNN"
  vergibt — mit `git ls-files`-Messung je Bezeichner, nicht per
  Aufzählung (MF-930), **(c)** die Lizenz- und Herkunftsfrage des
  mitgelieferten Codes als offene Eigentümer-Entscheidung stehen lässt,
  **(d)** für jeden Code-Vorschlag die Stelle nennt, die er ersetzt, samt
  Rotbeweis-Skizze, und **(e)** kein Byte des Pakets nach `src/`,
  `include/` oder `tests/` schreibt. Einbau ist ein eigener Posten.
- **Aufwand:** Begutachtung eine Sitzung (521 Zeilen Analysedokument +
  4 Quelldateien + 1 Test). Einbau **nicht schätzbar** — er hängt an der
  Lizenzfrage und daran, ob ein geschütztes Abbild beschafft werden kann.
- **Stand:** **Begutachtung fertig.** Gutachten liegt als
  `tools/uft-scout/out/a006_kopierschutz_umdrehungen.gutachten.md`
  (399 Zeilen). Alle sechs Bedingungen der Zeile `Fertig heißt` erfüllt.
  **Kernbefunde:** der Code ist gut — Bau mit `-Werror -pedantic` 0
  Warnungen, 7 Tests grün **mit eigenen Rot-Proben**, und der Klassierer
  sagt `UNDECIDED` statt zu raten (an 20 000 Bits gemessen: weak 0, fuzzy 0
  bei 50,17 % Flackern). Die **Lagebeschreibung** ist zur Hälfte veraltet:
  `P3-238` ✅ MF-951/954 (`uft_hal_read_flux_ex()` trägt die
  Umdrehungsgrenzen, **SCP verdrahtet**), `P3-236` ✅ MF-949, `P3-237` ✅
  MF-950. Drei Behauptungen berichtigt: Zeile **386** statt 355;
  **Greaseweazle behält** die Indexzeiten (`uft_gw_decode_flux_index_times`);
  vier Zählungen um 1-2 daneben. Echte Lücken: `uft_flux_revolution_t` 0
  `.c`-Verwender **plus Phantom-API** (`uft_flux_revolution_alloc`, 1
  Deklaration / 0 Definitionen), und Sektor-im-Sektor sowie verschobene
  Spuren (je 0 Treffer). **`uft_rev_align()` ist im Vertrag genannt und
  nicht geliefert** — MF-950 hat gemessen, dass ihr Fehlen 99,71 %
  falsches Flackern kostet. **Und drei meiner eigenen vier Erwartungen
  fielen**, weil die Feldbeschreibung von `flux_count` („gemittelt") ihrer
  Umsetzung (`(hi-lo)*16`) widerspricht — das ist der Befund, nicht mein
  Testergebnis. **Der Einbau ist NICHT gemacht** (S1 für zwei unbelegte
  Schwellen, Lizenzfrage zu DrCoolZics „Copyleft" offen).
- **Beleg:** Gutachten
  `tools/uft-scout/out/a006_kopierschutz_umdrehungen.gutachten.md`,
  **MF-1185 = `6dbb4ebf`** — nachgetragen wie angekündigt.

### A-007 · Zulieferung `DiskImageTool-extrakt.zip` (Herkunft · Schreibnähte · PC-Schutzmuster) begutachten
- **Status:** **erledigt** 2026-09-16 (Begutachtung; der EINBAU ist offen) ·
  **Aufgenommen:** 2026-09-16 · hochgezogen mit „weiter mit A-007"
- **Wortlaut:** „arbeite alles sehr genau aus / finde alles und alles
  raussuchen was ich übersehen habe / - wo können die formate verbessert
  werden / - ist es auf andere formate übertragbar / - welche einstellungen
  fehlen noch / - was habe wir noch nicht / - brauch es eine
  HAL-Erweiterungen / erstelle mir code Beispiele was besser gemacht werden
  kann / plan verstanden, was kannst du besser machen ??"
- **Kennzahl:** **keine der vier für die Begutachtung.** Herkunftsnachweis,
  Schreibnahterkennung und Schutzmuster sind keine Tier-Stufe, kein
  Wandlungspfad, kein Leck, kein Bench-Alter. Wie bei A-006 ist das der
  Fall, für den MF-640 die fünfte Zahl offengelassen hat („Dateien mit
  ungeklärter Herkunft" — und **hier geht es um die Herkunft eines
  Datenträgers**, nicht einer Quelldatei; ob das dieselbe Zahl ist, gehört
  zur Antwort).
- **Kanal:** **Port zulässig, und das ist gemessen, nicht angenommen.**
  Gegenstand ist `github.com/Digitoxin1/DiskImageTool` (VB.NET,
  **GPL-3.0**). Die Zulieferung begründet die Zulässigkeit damit, UFTs
  verteilbare Kombination sei über `uft_kfstream_air.c`, `uft_ipf_air.c`
  und `uft_stx_air.c` ohnehin GPL-3 — gemessen über `git ls-files` liegen
  **alle drei** im Baum (1 / 2 / 2 Treffer). Die Prämisse hat also einen
  Boden; ob sie trägt, ist eine Lizenz- und keine Messfrage und bleibt
  Eigentümer-Entscheidung (Konfliktordnung 2: Lizenz vor Fähigkeit).
- **Einfrier-Regel:** **ja.** `src/protection/` und der Flusspfad sind
  Decoder-Layer; `src/analysis/` grenzt daran. Rotbeweis zuerst, benannte
  Referenz im Header (`Modules/BootstrapDB.vb` ist benannt), jede Zahl
  gemessen.
- **OPEN_ITEMS:** berührt `P3-271` (die Spleissstelle ist bei UFT **ein**
  Zeitpunkt je Spur, in IPF eine Gap-Größe auf **beiden** Seiten je
  Sektor) und `P3-7` (der Schreibstartpunkt hat keinen Verbraucher, P6,
  MF-769) — beide direkt auf der Schreibnaht-Hälfte. Status dort ist bei
  der Begutachtung zu lesen, nicht hier zu behaupten.
- **Bei der Aufnahme gemessen — der zweite Fund liegt schon im Baum, und
  zwar ungerufen:** `include/uft/analysis/uft_deepread_splice.h` +
  `src/analysis/deepread/uft_deepread_splice.c` + `tests/
  test_flux_splice_pos.c` existieren. Gemessen über `git ls-files` nennen
  **genau zwei** Dateien `deepread_splice`/`uft_splice_`: das Modul selbst
  und der Test — **kein Produktivaufrufer.** Das ist die Write-Splice
  Detection aus `CLAUDE.md` §DeepRead, eines der **7 von 8** Module ohne
  Aufrufer (MF-627/MF-767). Ein neues `uft_splice.c` daneben wäre die
  zweite Kopie derselben Rechnung (MF-1015/MF-1026/MF-1177) statt der
  fehlenden Tür (P3-204). Welche der beiden Lagen es ist, ist die erste zu
  klärende Frage. **Gleiches gilt für den ersten Fund:**
  `src/forensic/uft_provenance.c` hat **6** `.c`-Nenner und
  `uft_fundus_provenance.c` liegt daneben — die Behauptung „für den
  Datenträger selbst führt UFT keine Provenienz" ist damit eine Aussage
  über eine **Unterscheidung**, die gemessen werden muss.
- **Fertig heißt:** ein Gutachten als Dokument, das **(a)** jede der fünf
  Fragen mit einer Messung gegen den Baum beantwortet, **(b)** für jeden
  der drei Funde entscheidet „fehlt / liegt ungerufen da (P3-204) / ist
  eine zweite Kopie" — je mit `git ls-files`-Messung, nicht per
  Aufzählung (MF-930), **(c)** die GPL-3-Prämisse als benannte
  Eigentümer-Entscheidung stehen lässt und dabei die IPF-Quarantäne
  ausdrücklich prüft, **(d)** die fehlende Bootstrap-Datenbank benennt —
  die Zulieferung sagt selbst „die Datenbank ist nicht im Baum ... wer sie
  nicht hat, hat 379 leere Einträge", also dieselbe Datenbank-/Ernte-Frage
  wie A-005 —, und **(e)** kein Byte des Pakets nach `src/`, `include/`
  oder `tests/` schreibt. Einbau ist ein eigener Posten.
- **Aufwand:** Begutachtung eine Sitzung (542 Zeilen Analysedokument +
  6 Quelldateien + 1 Test + 1 Wandlerskript). Einbau **nicht schätzbar** —
  er hängt an der Lizenzentscheidung und an der Beschaffung der Datenbank.
- **Stand:** **Begutachtung fertig.** Gutachten liegt als
  `tools/uft-scout/out/a007_diskimagetool_herkunft.gutachten.md`.
  Alle fünf Bedingungen der Zeile `Fertig heißt` erfüllt.
  **Die drei Funde, je mit Urteil:** Bootstrap-Herkunft **fehlt für den
  Datenträger** (`oem_name` 9 `.c`, `bootstrap` 0 `.c`; `uft_provenance`
  6 `.c` führt Dateien, nicht Träger) · Schreibnaht **fehlt für
  IBM-MFM, aber das Muster hat drei Vorgänger** — `uff_detect_splices()`
  0 Aufrufer, `uft_deepread_detect_splice()` 0 Aufrufer, und
  `G64_DIAG_SPLICE_DETECTED` ist **ein Etikett, das niemand setzt**; die
  Zulieferung nennt zwei Stellen und übersieht die dritte · PC-Schutz aus
  Fehlsektormustern **fehlt** (`bad_sector` 25 `.c` baumweit, 1 in
  `src/protection/`, 0 Musterschluss). **Selbstzusagen tragen alle vier**,
  Bau `-Werror -pedantic` 0 Warnungen, 6 Tests grün mit drei Rot-Proben.
  **Der Nebenfund reicht über den Posten hinaus:** die Lizenzprämisse ist
  belegt — alle drei AIR-Dateien tragen `GPL-3.0-only`, und MF-698 bindet
  „die VERTEILBARE Fassung des Gesamtwerks an GPL-3". Damit messen
  `P3-385` und mein eigener `A-014`-Eintrag gegen **GPL-2**, während
  Apache-2.0 mit **GPL-3** verträglich ist — der Port-Kanal für `A-013`
  und `A-014` ist möglicherweise offen. **Eigentümer-Entscheidung, nicht
  meine.** Fünf Vorschläge stehen in §13 des Gutachtens.
- **Nachtrag MF-1187 („mach weiter mit A-007"):** zwei der vier
  Nicht-geprüft-Punkte aus §12 sind **geprüft**, und beide Ergebnisse
  betreffen mich selbst. **(1) `Modules/BootstrapDB.vb` gelesen** — sie liegt
  geklont im Baum (213 Zeilen). Die Belegkette **trägt wörtlich**: CRC32 über
  den Bootstrap-Code ist der Schlüssel (`Dictionary(Of UInteger, …)`), der
  OEM-Name der Prüfwert, und der Win9x-Rückfall steht im Original. **Neu
  gefunden:** `FindXDFMatch()` nullt bei Bootstrap-Länge `&H1AD` = 429 Byte
  die Bytes `0xE7`–`0xEE` (8 Byte) und rechnet die CRC neu — die Zulieferung
  hat das **nicht** (0 Treffer), eine echte XDF-Diskette verfehlt damit ihren
  Eintrag. Trifft MF-1087/ARCH-18. **Und ein Vorwurf von mir fällt:** der
  `Verified`-Zustand **ist** übernommen (490 OEM-Namen, 223 verifiziert).
  **(2) Die `0x4E`-Kandidatenzahl ist geprüft und ZURÜCKGENOMMEN.** Die
  Fundstellen sind überwiegend Kennungen („RAIN", „BOUN"), 68000-Opcodes
  (`0x4EFx`, `0x4E75`), ein Versatz `h[0x4E]`, eine Tafelzeile und
  Greaseweazle-Befehlsbytes — kein Füllbyte. **Und meine Nachzählung war
  selbst falsch:** 43 von 36, weil die zweite Messung still auf `grep -li`
  schaltete (36 + 7 = 43). Es steht **keine dritte Zahl** an ihrer Stelle;
  Vorschlag V3 ist zurückgenommen, V6 (XDF) ist neu.
- **Beleg:** Gutachten
  `tools/uft-scout/out/a007_diskimagetool_herkunft.gutachten.md`,
  **MF-1186 = `87abb0b3`**; Korrektur und Nachtrag **MF-1187** (Hash folgt).

### A-008 · Zulieferung `UFT_C64PP_Protection_Catalog.zip` begutachten
- **Status:** aufgenommen · **Aufgenommen:** 2026-09-16
- **Wortlaut:** „arbeite alles sehr genau aus / finde alles und alles
  raussuchen was ich übersehen habe / - wo können die formate verbessert
  werden / - ist es auf andere formate übertragbar / - welche einstellungen
  fehlen noch / - was habe wir noch nicht / - brauch es eine
  HAL-Erweiterungen / erstelle mir code Beispiele was besser gemacht werden
  kann / plan verstanden, was kannst du besser machen ??"
- **Kennzahl:** **keine der vier für die Begutachtung.** Schutzerkennung ist
  keine Tier-Stufe (wie A-006). **Ein Teil bewegt aber etwas Messbares, und
  das ist ungewöhnlich:** die Zulieferung stuft die RapidLok-Heuristik zu
  einer neutralen Strukturmeldung herab. Damit trägt sie auf der Seite der
  **Ehrlichkeit** ab, was P3-38/P3-39 als Befund führen — das ist Rücknahme
  einer Falschaussage, nicht Zuwachs, und genau die Richtung von MF-1077.
- **Kanal:** **Spec** für den Inhalt (die Seiten des C64 Preservation
  Project, Abschnitt 9 „Quellen" des Auditberichts), **MIT** für den
  mitgelieferten Kern: `LICENSE_C64PP_MODULE.txt` ist MIT © 2026 „UFT
  Project contributors", `LICENSE_UFT_PROJECT.txt` liegt daneben — MIT in
  ein GPL-2-or-later-Projekt ist unproblematisch (permissiv in Copyleft).
  **Vorsicht ist trotzdem geboten:** C64-Schutzarbeit hat in diesem Baum
  die nibtools-Vorgeschichte („vier Dateien und Tage gekostet"), also
  gehört die Herkunft je Katalogeintrag geprüft, nicht je Paket.
- **Einfrier-Regel:** **ja.** `src/protection/` ist Decoder-Layer; dazu
  kommt eine GUI-Änderung. Rotbeweis zuerst — und die Zulieferung sagt
  selbst, ihre Tests seien **synthetisch** („synthetische Positiv-/Negativ-
  verträge").
- **OPEN_ITEMS:** **viele Treffer, und sie sind der Maßstab.** `P3-38` (die
  C64-Schutzerkennung rät Markennamen, das Unterscheidungsmerkmal fehlt
  ganz), `P3-39` (alle drei Markennamen an der Quelle widerlegt), `P3-144`
  (der einzige erreichbare Schutzerkenner bestand aus drei Heuristiken),
  `P3-79` (Artefaktsignaturen identifizieren Codefamilien, nicht Produkte —
  Korpus von 17 C64-Kopierprogrammen), `P3-187` (C64-Signaturkonstanten
  ohne benannte Quelle). Dazu auf der Behälterseite `P3-36`, `P3-147`,
  `P3-194`, `P3-195`.
- **Bei der Aufnahme gemessen — die schwerste Feststellung dieser vier
  Zulieferungen:** das Paket liefert `UnifiedFloppyTool.pro` und
  `tests/CMakeLists.txt` mit und nennt sie in `README_EINBAU.md`
  „**aktuelle Verdrahtung**". Gemessen sind sie **kleiner als die des
  Baums**: `tests/CMakeLists.txt` **341 230** gegen **361 386** Byte
  (−20 156), `UnifiedFloppyTool.pro` **81 628** gegen **83 293** (−1 665).
  Sie stammen also aus einem **älteren** Stand; sie zu kopieren wäre eine
  **stille Rücknahme** fremder Verdrahtung — die teuerste Fehlerklasse
  dieses Baums. Der Einbau darf diese zwei Dateien **nicht** übernehmen,
  sondern nur die eigenen Zeilen daraus. Zweitens:
  `src/gui/ProtectionAnalysisWidget.cpp/.h` **liegen schon im Baum** — die
  mitgelieferten 35 KB sind eine geänderte Kopie, kein neues Modul.
  Drittens sagt das Paket selbst, der **Qt-Gesamtbau sei nicht gelaufen**.
- **Fertig heißt:** ein Gutachten als Dokument, das **(a)** jede der fünf
  Fragen mit einer Messung gegen den Baum beantwortet, **(b)** je
  Katalogeintrag „belegt / Kandidat / widerlegt" vergibt und dabei gegen
  P3-38/P3-39/P3-79 hält statt gegen die Zulieferung, **(c)** für die
  GUI-Hälfte einen **Zeilendiff** gegen die vorhandene
  `ProtectionAnalysisWidget.cpp` nennt statt eine Dateiübernahme,
  **(d)** ausdrücklich festhält, dass `.pro` und `tests/CMakeLists.txt` des
  Pakets **verworfen** werden, mit den gemessenen Bytezahlen als Grund,
  und **(e)** kein Byte des Pakets nach `src/`, `include/` oder `tests/`
  schreibt. Einbau ist ein eigener Posten.
- **Aufwand:** Begutachtung eine Sitzung (224 Zeilen Auditbericht + 20
  Katalogeinträge + 1 Test + 1 Werkzeug + GUI-Diff). Einbau **nicht
  schätzbar** — die GUI-Hälfte ist ein Diff gegen lebenden Code und der
  Qt-Bau ist unerprobt.
- **Stand:** —
- **Beleg:** —

### A-009 · Zulieferung `UFT_Atari_ST_Cartridge_Detection.zip` begutachten
- **Status:** aufgenommen · **Aufgenommen:** 2026-09-16
- **Wortlaut:** „arbeite alles sehr genau aus / finde alles und alles
  raussuchen was ich übersehen habe / - wo können die formate verbessert
  werden / - ist es auf andere formate übertragbar / - welche einstellungen
  fehlen noch / - was habe wir noch nicht / - brauch es eine
  HAL-Erweiterungen / erstelle mir code Beispiele was besser gemacht werden
  kann / plan verstanden, was kannst du besser machen ??"
- **Kennzahl:** **keine der vier.** Ein Cartridge-ROM ist keine Diskette,
  also keine Tier-Stufe, kein Wandlungspfad. **Und das ist hier die Pointe,
  nicht der Einwand:** die Zulieferung baut eine **Absage** — ein erkanntes
  Cartridge bleibt absichtlich `isValid == false`, gekennzeichnet über
  `DiskImageInfo::isNonDiskImage`, damit es „nicht als Diskettenabbild
  fehlklassifiziert oder an einen Disketten-Decoder weitergereicht wird".
  Das ist genau die Bauform von `UFT_CAPS_OS_VOLUME` aus MF-1176 („der
  Eintrag ist eine Absage, keine Zusage"), und Absagen bewegen in diesem
  Baum keine Kennzahl — sie verhindern Falschaussagen.
- **Kanal:** **Nachbau, selbsterklärt — und die Quelle ist nicht
  benannt.** `docs/ATARI_ST_CARTRIDGE_DETECTION.md` sagt: „Die
  Datenstruktur wurde anhand der oeffentlich beschriebenen Atari-ST-
  Cartridge-Header neu implementiert. Es wurde kein Assembler-, Loader-
  oder Entpacker-Code der Referenzseite uebernommen." Welche Seite, welche
  Fassung, welche Stelle — steht nicht da. Nach MF-636 ist eine
  Attribution eine **rechtliche Aussage**, und „nach fremder Doku
  eigenständig implementiert" muss sagen, nach welcher. Dazu gemessen:
  **keine `SPDX`-Zeile und keine Lizenzangabe** in `.c` oder `.h`
  (0 Treffer für `SPDX`, `Licen`) — dieselbe Lücke wie bei A-006.
- **Einfrier-Regel:** **ja, aber nicht das Moratorium.** Gemessen: die
  Datei registriert **kein** Format-Plugin — 0 Treffer für
  `uft_format_plugin_t`, `uft_register_format_plugin`, `DSK_PLUGIN`,
  `.probe`, `uft_probe_konfidenz`. Das Moratorium für neue Plugins greift
  also nicht; die Regel „kein neuer **ungeprüfter** Code im
  Format-/Decoder-Layer" greift sehr wohl, weil die Datei in
  `src/formats/atari/` liegt. Rotbeweis zuerst, Referenz im Header.
- **OPEN_ITEMS:** kein Treffer zu „Cartridge" in der Befundliste. Fachlich
  benachbart und **gegenläufig**: der SCOUT-17b-Block (Zeile 1456 ff.)
  führt `CRT`, `EDD` und `FDX` als „Code liegt im Baum, gebaut, aber nie
  registriert und über `uft_disk_open()` unerreichbar" — die Lage
  MF-446/447 und P3-204. Diese Zulieferung wählt ausdrücklich den anderen
  Weg (kein Plugin, sondern eine Absage im Prüfpfad), und ob das der
  richtige ist, gehört in das Gutachten. Dazu `P3-349` (Formate, die ein
  Werkzeug im Baum schreibt und UFT nicht liest).
- **Bei der Aufnahme gemessen — zweites Mal dasselbe Muster, damit ist es
  eines:** wie A-008 liefert dieses Paket lebende Baudateien mit, und sie
  sind **kleiner als die des Baums**: `tests/CMakeLists.txt` 341 513 gegen
  361 386 (**−19 873**), `UnifiedFloppyTool.pro` 81 746 gegen 83 293
  (−1 547), und — schwerer — `src/mainwindow.cpp` 25 318 gegen 25 457
  (**−139**, lebender GUI-Code). Drei weitere Dateien sind geänderte
  Kopien (`explorertab.cpp` +364, `disk_image_validator.cpp` +2 963,
  `.h` +59). **Wirklich neu sind nur drei Dateien:**
  `src/formats/atari/uft_atari_st_cartridge.c` (341 Zeilen),
  `include/uft/formats/atari/uft_atari_st_cartridge.h`,
  `tests/test_atari_st_cartridge.c`. Ein Kopieren wäre eine stille
  Rücknahme. **Und eine Gegenprobe zu meinen eigenen Gunsten:** die
  Kennung `0xABCDEF42` kommt in der `.c` **nicht** vor — sie steht als
  `UFT_STCART_MAGIC` im Header, neben `0x00FA0000`. Das ist richtig so
  (D3, Wissen nicht doppelt halten); ich notiere es, weil der erste
  Blick auf die `.c` wie eine fehlende Kennung aussah.
- **Fertig heißt:** ein Gutachten als Dokument, das **(a)** jede der fünf
  Fragen mit einer Messung gegen den Baum beantwortet, **(b)** die
  Grundfrage ausdrücklich entscheidet — gehört eine Nicht-Disketten-Absage
  in `src/formats/` oder in den Prüfpfad —, **(c)** die unbenannte Quelle
  als **Nachforderung** stellt (welches Dokument, welche Fassung, welche
  Stelle) statt sie zu ergänzen, **(d)** für die sechs überlappenden
  Dateien einen **Zeilendiff** nennt statt eine Dateiübernahme, mit den
  gemessenen Bytezahlen als Grund, und **(e)** kein Byte des Pakets nach
  `src/`, `include/` oder `tests/` schreibt. Einbau ist ein eigener Posten.
- **Aufwand:** Begutachtung eine Sitzung (341 Zeilen Modul + Header + Test
  + 1413 Byte Doku + sechs Dateidiffs). Einbau **nicht schätzbar** — er
  hängt an der Quellenangabe und an sechs Diffs gegen lebenden Code.
- **Stand:** —
- **Beleg:** —

### A-010 · Zulieferung `FloImg-extrakt.zip` (Ganzdurchläufe · Mediendiagnosen) auseinandernehmen
- **Status:** aufgenommen · **Aufgenommen:** 2026-09-16
- **Wortlaut:** „nimm den code komplett auseinander , sehr genau / finde
  alles und alles raussuchen was ich übersehen habe , stimme es mit mein
  aktuellen tool ab / - wo können die formate verbessert werden / - ist es
  auf andere formate übertragbar / - welche einstellungen fehlen noch / -
  was habe wir noch nicht / - brauch es eine HAL-Erweiterungen plan
  verstanden, was kannst du besser machen ??"
- **Kennzahl:** **keine der vier für die Begutachtung.** Ganzdurchläufe und
  Mediendiagnosen sind keine Tier-Stufe und kein Wandlungspfad. **Ein
  Einbau könnte allerdings P3-284 abtragen** („`adaptive_passes` war ein
  Schalter ohne Schaltung — und der forensische Bericht meldete ihn als
  ‚yes'"), und das ist Rücknahme einer Falschaussage, also die Richtung von
  MF-1077.
- **Kanal:** **Spec.** Die Belegkette der Zulieferung nennt zwei Quellen:
  **FloImg 1.02** (Petari, 8bitchip.info, Hilfe zu v1.02 vom 2011-08-08) für
  „Ganzdurchläufe bringen mehr als sofortige Wiederholungen", und
  `NFORMAT.DOC` (1992) plus cw2dmk `jv3.h` (GPL-2, nur gelesen) für die
  Spurkapazität — **letzteres ist dieselbe Quelle, an der MF-1166 und
  MF-1183 hängen**. Gemessen: **keine `LICENSE`-Datei** im Paket, aber
  `SPDX` steht in `uft_os_volume.c`. Die Paketlizenz ist damit ungeklärt,
  die Dateilizenz nicht.
- **Einfrier-Regel:** **ja.** `src/hal/` und `src/analysis/` grenzen an den
  Decoder-Layer, und `uft_os_volume.c` ist lebender Code aus MF-1176.
  Rotbeweis zuerst.
- **OPEN_ITEMS:** **viele Treffer, und sie sind der Maßstab.** `P3-284`
  (`adaptive_passes` ohne Schaltung), `P3-113` (`distinct_contents` zählte
  nicht, was sein Name sagt — behoben MF-860, `classify_passes()`),
  `P3-88` (Multi-Read-Voting konnte einen Sektor erfinden — behoben
  MF-845), `P3-291` (leere Sektoren werden gezählt, nicht gewählt),
  `P3-429` (HAL-Merkmalstafel ohne Leser).
- **Bei der Aufnahme gemessen — vier Dinge, zwei davon entscheiden schon:**
  **(1)** Das Paket **erweitert**, statt zu ersetzen:
  `include/uft/hal/uft_os_volume.h` Baum 8 145 / Paket 9 625 (**+1 480**),
  `src/hal/uft_os_volume.c` Baum 10 557 / Paket 13 984 (**+3 427**). Das
  ist das **Gegenteil** des A-008/A-009-Musters und gehört gelobt.
  **(2)** Die Wiederholungsinfrastruktur ist **da**: `retries` in **46**
  `.c` und **23** `.h`, `multiread_execute` 7 `.c`, `distinct_contents`
  3 `.c`, `classify_passes` 2 `.c`, `adaptive_passes` 2 `.c`. Ein
  „`--passes` neben `--retries`" trifft also auf einen bestellten Tisch.
  **(3) Und hier liegt ein harter Konflikt: `--passes` ist ein
  CLI-Schalter, und dieses Projekt hat kein CLI.** Gemessen: 0 Dateien
  passen auf `cli\.c|main_cli|uft_cli`, und `CLAUDE.md` sagt „Dies ist ein
  **GUI-only-Projekt**. Es gibt keinen CLI-Modus mehr." Der Vorschlag
  gehört also an die Oberfläche oder an einen Optionssatz, nicht an eine
  Kommandozeile — das ist bei der Begutachtung zu entscheiden, nicht zu
  übernehmen. **(4)** Die Zulieferung stellt selbst eine Frage, die ich
  beantworten kann: „hat das `FloppyDevice`-Subsystem überhaupt einen
  Verteiler?" Sie zählt 79 Dateien, ich zähle **84** über
  `git ls-files | xargs grep -l 'uft_floppy_device.h'` — und der
  Gedächtnisstand `arch_formate_8bit` sagt, **alle** externen Aufrufer
  seien Tests. Die Antwort lautet damit wahrscheinlich „nein", und sie
  gehört gemessen statt zitiert.
- **Fertig heißt:** ein Gutachten als Dokument, das **(a)** jede
  öffentliche Funktion der vier Paketdateien einzeln gegen den Baum hält —
  „gibt es / gibt es anders / gibt es nicht", je mit `git ls-files`-Messung,
  weil der Auftrag ausdrücklich „komplett auseinander, sehr genau" und
  „stimme es mit mein aktuellen tool ab" sagt; **(b)** für `uft_os_volume`
  einen **Zeilendiff** gegen den lebenden Stand aus MF-1176 nennt, nicht
  eine Dateiübernahme; **(c)** die fünf Fragen je mit Messung beantwortet;
  **(d)** den CLI-Konflikt ausdrücklich entscheidet; **(e)** die
  Paketlizenz als offene Frage stehen lässt; und **(f)** kein Byte des
  Pakets nach `src/`, `include/` oder `tests/` schreibt.
- **Aufwand:** Begutachtung eine Sitzung (279 Zeilen Bericht + 4
  Quelldateien + 1 Test + 2 Dateidiffs gegen lebenden Code). Einbau
  **nicht schätzbar** — er hängt am CLI-Konflikt und an P3-284.
- **Stand:** —
- **Beleg:** —

### A-011 · Zulieferung `Metadaten.zip` (Herkunft · Datierung über die VSN) begutachten
- **Status:** aufgenommen · **Aufgenommen:** 2026-09-16
- **Wortlaut:** „arbeite alles sehr genau aus / finde alles und alles
  raussuchen was ich übersehen habe / - wo können die formate verbessert
  werden / - ist es auf andere formate übertragbar / - welche einstellungen
  fehlen noch / - was habe wir noch nicht / - brauch es eine
  HAL-Erweiterungen"
- **Kennzahl:** **keine der vier.** Metadaten und Datierung sind keine
  Tier-Stufe und kein Wandlungspfad. **Aber es berührt die fünfte Zahl, die
  MF-640 offengelassen hat** („Dateien mit ungeklärter Herkunft") — und
  zwar von der anderen Seite: hier geht es um die Herkunft des
  **Datenträgers**, nicht der Quelldatei. Dieselbe Verschränkung wie
  `A-007`; welche Zahl das ist, gehört zur Antwort.
- **Kanal:** **Spec, und die Belegkette ist die beste dieser Reihe.** Für
  die VSN-Formel `(Sek<<8|Hundertstel)+(Monat<<8|Tag)`,
  `(Std<<8|Min)+Jahr`: Craig Wilson, *Volume Serial Numbers and Format
  Date/Time Verification*, digital-detective.net, plus Ralf Brown. Dazu ein
  **veröffentlichter Prüfvektor** (19.10.2003 22:33:27.01 → `2514-1DF4`),
  den die Zulieferung nachgerechnet hat — „beide Wörter treffen". Weitere
  Aussagen gegen MAME `imd_dsk.cpp`, a8rawconv `rawdiskscp.cpp:380-424`
  und den **eigenen Baum** (`uft_2img.c:6-21`, `uft_scp_writer.c:292`).
  Gemessen: `SPDX` steht in beiden Quelldateien, **keine `LICENSE`-Datei**
  im Paket — wie bei `A-006`.
- **Einfrier-Regel:** **ja, mittelbar.** `src/analysis/` grenzt an den
  Decoder-Layer, und die VSN-Deutung ist eine Aussage über ein
  Dateisystemfeld. Rotbeweis zuerst — und der Prüfvektor **ist** einer,
  weil er von außen kommt.
- **OPEN_ITEMS:** **`P3-387`** ist der nächste Nachbar: „Zwei forensische
  Fähigkeiten, die der Baum nicht hat, und die seine eigene Mission
  verlangt" — darunter „`uft_format_mark_last_missing()` kennzeichnet im
  SPEICHER, und keine Datei trägt es hinaus — der Befund existiert im Lauf
  und überlebt ihn nicht". Das ist dieselbe Klasse wie „gelesen und
  weggeworfen". Dazu `P3-354` (WOZ-CRC stellt eine Beschädigung fest und
  hat keinen Weg, sie zu melden).
- **Bei der Aufnahme gemessen — vier Dinge:** **(1) Die saubersten
  Zulieferungsform bisher:** alle fünf Dateien sind **neu**, keine
  Überlappung mit dem Baum, **keine** mitgelieferten Baudateien. Kein
  Diff gegen lebenden Code nötig. **(2) Die tragende Behauptung trifft
  präzise zu:** „`metadata_count` in keiner `.c` außer MOOFs eigener
  Struktur" — gemessen ist `metadata_count` in **genau einer** `.c`-Datei
  genannt, `src/formats/apple/uft_moof_parser.c`. **(3) VSN als Begriff
  fehlt im Baum vollständig:** der einzige Treffer auf `VSN` ist
  `resload_VSNPrintF` in `src/whdload/`, also unverwandt. `volume_serial`
  wird dagegen gelesen (3 `.c` / 8 `.h`) und über `fat_format_serial()`
  nur **angezeigt** (`%04X-%04X`) — gelesen, nicht gedeutet. **(4) Die
  Zahl „drei unvereinbare Metadatenmodelle" ist womöglich zu niedrig:**
  gemessen nennen mindestens **acht** Header „metadata", darunter
  `uft_disk.h`, `uft_format_registry.h`, `uft_snapshot.h` und
  `uft_flux_meta.h`. Das gehört nachgezählt, nicht übernommen.
- **Fertig heißt:** ein Gutachten als Dokument, das **(a)** jede der fünf
  Fragen mit einer Messung gegen den Baum beantwortet; **(b)** den
  veröffentlichten Prüfvektor **selbst nachrechnet** statt die Nachrechnung
  zu zitieren — das ist der einzige Rotbeweis, der hier von außen kommt;
  **(c)** die offene Bytefolge-Frage („die VSN-Bytefolge im Sektor ist aus
  den Quellen nicht zweifelsfrei zu entnehmen") als offen stehen lässt und
  messbar macht, statt sie zu entscheiden; **(d)** die Zahl der
  Metadatenmodelle nachzählt; **(e)** die Paketlizenz als offene Frage
  nennt; und **(f)** kein Byte des Pakets nach `src/`, `include/` oder
  `tests/` schreibt.
- **Aufwand:** Begutachtung eine Sitzung (487 Zeilen Bericht + 4
  Quelldateien + 1 Test, alle neu). Einbau **nicht schätzbar** — er hängt
  an der Bytefolge-Frage und daran, ob ein Träger mit bekanntem
  Formatierungsdatum beschafft werden kann (die Zulieferung nennt das
  ausdrücklich als „nicht verifiziert").
- **Stand:** —
- **Beleg:** —

### A-012 · `yas-sim/xm7-related-tools` auseinandernehmen (FM-7: D77 · T77 · FM-Dateisystem · Boot-ROM)
- **Status:** aufgenommen · **Aufgenommen:** 2026-09-16
- **Wortlaut:** „https://github.com/yas-sim/xm7-related-tools.git nimm den
  code komplett auseinander , sehr genau / finde alles und alles raussuchen
  was ich übersehen habe , stimme es mit mein aktuellen tool ab / - wo
  können die formate verbessert werden / - ist es auf andere formate
  übertragbar / - welche einstellungen fehlen noch / - was habe wir noch
  nicht / - brauch es eine HAL-Erweiterungen"
- **Kennzahl:** **keine der vier für die Begutachtung** — aber dieser Fund
  zielt auf die Achse, die `P3-389` als **die dünnste des Baums** benennt:
  „Die DEKODIERKETTE ist die duennste Achse des Baums … und der FM-Weg hat
  bis heute keinen ERZEUGER." Ein FM-Erzeuger wäre kein Kennzahlwert,
  sondern das **Werkzeug**, mit dem FM-Formate überhaupt eine Prüfspur
  bekommen (MF-864 musste dafür `fluxtoimd` von fremder Hand nehmen).
- **Kanal:** **Port zulässig — und das ist die Ausnahme in dieser Reihe.**
  Gemessen über `gh api`: `license.spdx_id` = **MIT**, 925 KB, C++,
  7 Sterne, letzter Push **2022-09-21**, Standardzweig `main`. Damit ist
  der stärkste Kanal aus MF-695 offen. **Zwei Vorbehalte gehören dazu:**
  die 16 `.zip`-Dateien enthalten vorgebaute Windows-Binärdateien (Kanal
  *Oracle*, nicht *Port*), und für **Prüfdaten** gilt weiter, was
  `SCOUT-5` festgehalten hat — „das Repo ist MIT, der Disketten-INHALT
  hat eigene Urheber".
- **Einfrier-Regel:** **ja.** Jeder Einbau berührt den Format- oder
  Decoder-Layer. Rotbeweis zuerst, benannte Referenz im Header.
- **OPEN_ITEMS:** **drei Treffer, und der erste ist derselbe Autor.**
  **`P3-389`** entstand auf genau diese Eigentümer-Frage („finde was ich
  uebersehen habe") zu `yas-sim/fdc_bitstream` — Behälter und Dateisystem
  wurden bewertet, die **Dekodierkette fehlte in der Bewertung**, und die
  Messung MF-1121 ergab: kein FM-Erzeuger. **`SCOUT-5`** nennt ein
  D77-Wahrheitspaar beim Upstream (`2019FM77AVDemo-4MHz.raw`, `-8MHz.raw`,
  `2019FM77AVDemo.d77`), **blockiert bis Lizenzklärung**. Dazu `P3-218`
  („kein FM-Encoder, deshalb die Fremdabnahme durch `fluxtoimd`").
- **Bei der Aufnahme gemessen — fünf Dinge:** **(1) Es ist ein echtes
  Quellrepo:** 183 Blobs, davon **70 Quelldateien** (42 `.cpp`, 25 `.h`,
  3 `.c`), dazu `CMakeLists.txt`, 9 ROM-Listings (`.lst`), 7
  Assemblerquellen (`.s`/`.src`) — und 16 `.zip` mit Binärdateien.
  **22 Werkzeuge** auf oberster Ebene, darunter `d77enc_dec`, `d77uty`,
  `fmtools` (mit `fmfslib/cfilesys.cpp`), `t77dec`, `t772wav`, `wav2t77`,
  `fdump`, `BootROM`. **(2) `T77` fehlt im Baum vollständig:** 0 Dateien
  für `t77`/`T77`. Gemessen über zitierte Endungen in `.c`-Dateien führt
  der Baum an Bandformaten nur `"cas"` (2×) und `"tzx"` (1×) — die Methode
  ist eng, aber der T77-Nullwert ist eindeutig. **(3) Kein
  FM-7-Dateisystemeintrag** in `docs/VERIFICATION_TIERS_FS.md`.
  **(4) `d77` steht heute auf T1b** mit `test_d77_gegen_hxcfe`,
  `test_d88_header_variants` und `test_oeffentliche_api_am_korpus`, Quelle
  pc98.org — **`SCOUT-5`s Messung „T3 ohne alles" ist damit veraltet**, die
  Lizenzsperre bleibt. **(5) Und der eigentliche Grund, hinzusehen:** das
  Repo enthält **Erzeuger** — `wav2t77` schreibt Bandaudio, `d77enc`
  schreibt D77 —, und P3-389s Befund lautet, dass dem FM-Weg genau das
  fehlt. Ob einer davon **der** fehlende FM-Erzeuger ist, ist die erste zu
  klärende Frage, nicht ihre Antwort.
- **Fertig heißt:** ein Gutachten als Dokument, das **(a)** die 70
  Quelldateien nach Werkzeug gruppiert durchgeht und je Werkzeug „für UFT
  brauchbar / Oracle-Kandidat / uninteressant" mit Grund vergibt — der
  Auftrag sagt „komplett auseinander, sehr genau"; **(b)** die Frage von
  P3-389 ausdrücklich beantwortet: liefert dieses Repo einen FM-**Erzeuger**,
  und wenn ja, für welche Kodierung; **(c)** je Fund den Kanal nennt (Port
  bei MIT-Quelle, Oracle bei Binärdatei, Spec bei ROM-Listing) und die
  Lizenzfrage der **Prüfdaten** getrennt davon offen lässt (SCOUT-5);
  **(d)** die fünf Fragen je mit Messung gegen den Baum beantwortet;
  **(e)** `T77` und das FM-Dateisystem als gemessene Lücken belegt statt
  behauptet; und **(f)** kein Byte nach `src/`, `include/` oder `tests/`
  schreibt — geklont wird nach `tools/uft-scout/work/`, dem dafür
  vorgesehenen Ort.
- **Aufwand:** Begutachtung **mehr als eine Sitzung** — 70 Quelldateien in
  22 Werkzeugen sind mehr als jede bisherige Zulieferung dieser Reihe
  (A-010 hatte 4). Genauer nicht schätzbar. Einbau **nicht schätzbar** und
  in jedem Fall ein eigener Posten.
- **Stand:** —
- **Beleg:** —

### A-013 · `thomas-luebker/AmigaDiskKit` auseinandernehmen — **das ist `P3-385(b)`**
- **Status:** aufgenommen · **Aufgenommen:** 2026-09-16
- **Wortlaut:** „https://github.com/thomas-luebker/AmigaDiskKit.git nimm den
  code komplett auseinander , sehr genau / finde alles und alles raussuchen
  was ich übersehen habe , stimme es mit mein aktuellen tool ab / - wo
  können die formate verbessert werden / - ist es auf andere formate
  übertragbar / - welche einstellungen fehlen noch / - was habe wir noch
  nicht / - brauch es eine HAL-Erweiterungen"
- **Kennzahl:** **keine der vier für die Begutachtung.** Der Punkt, den es
  abtragen könnte, ist `P3-385` — die „ADFlib-unabhängige Zweitmeinung, die
  `docs/ORACLES.md:650` selbst als ‚Offene Lücke' benennt". Ein
  unabhängiges Orakel bewegt keine der vier direkt, ist aber die
  Voraussetzung für Stufenhebungen im Amiga-Zweig.
- **Kanal:** **`P3-385` hat das Urteil bereits gefällt, und ich habe es
  nachgemessen — es gilt, aber es ist nicht ausführbar.** Der Punkt sagt
  wörtlich: „**Lizenz berichtigt:** Apache-2.0, NICHT MIT — mit GPL-2
  unverträglich, also **kein Port**, nur Orakel." Gemessen bestätigt:
  `license.spdx_id` = **Apache-2.0**. **Der offene Widerspruch:** `swift`,
  `swiftc` und `xcodebuild` sind auf diesem Rechner **alle drei nicht
  vorhanden**, und `docs/ORACLES.md` verlangt „Kein Oracle auf Zusicherung
  — ein Werkzeug, das nicht gebaut und ausgeführt wurde, ist kein
  Eintrag". Der einzige Kanal, den P3-385 offenlässt, ist damit hier
  **verschlossen** — nicht grundsätzlich, aber auf dieser Maschine. Das ist
  dieselbe Lage wie die Tier-3-Hardwarebank (MF-310): delegierbar, nicht
  hausintern. Was bleibt: **Spec** (lesen) und **Daten** (die 26 `.bin`).
  Für Apache-2.0 gibt es dabei **zwei entschiedene Präzedenzfälle** —
  `P3-316` ✅ MF-1008 und `P3-353` ✅, beide vom Eigentümer festgestellt.
- **Einfrier-Regel:** **ja** für jeden Einbau; für das Lesen nicht.
- **OPEN_ITEMS:** **`P3-385`** — dieser Posten IST der Auftrag, ihn
  abzutragen; der Befund bleibt dort und wird hier nicht abgeschrieben.
  Berührt `P3-316`/`P3-353` (Apache-Präzedenz) und die Amiga-FS-Stufen.
- **Bei der Aufnahme gemessen — fünf Dinge:** **(1) Die Zahl von P3-385
  stimmt:** **139 Blobs**, genau wie dort notiert; davon **97 `.swift`**,
  26 `.bin`, 11 `.txt`. Oberste Ebene `Sources`, `Tests`, `Package.swift`
  — eine Swift-Paketstruktur, kein Werkzeugkasten. **(2) Der Umfang ist
  breiter als „Diskette":** die Beschreibung nennt „RDB/MBR layouts,
  FFS/OFS/FFS2, PFS3, FAT32, ADF & flux floppies, LHA" und ausdrücklich
  „No external tools or Python". **(3) Vier dieser Achsen fehlen dem Baum
  gemessen:** `rdb` 1 Datei / `RDB` 0 / `rigid_disk` 0, `pfs3`/`PFS3`
  **0**, `ffs2`/`FFS2` **0**, `lha` 0 / `LHA` 1. **(4) Die Amiga-Seite des
  Baums ist ungleich belegt:** `uft_amigados` steht auf **FS-T2**,
  `uft_amigados_extended`, `uft_bootblock_scanner` und
  `uft_amiga_virus_db` auf FS-T1 — und `uft_fs_amigados_driver` auf
  **FS-T0** mit der Bemerkung „kein Test nennt ein Symbol dieses Lesers".
  **(5) Es ist aktiv:** letzter Push **2026-07-29**, nicht archiviert —
  anders als der Nachbar `amigadx` (letzter Commit 2014-07-18, GPL,
  vendort ADFlib 0.7.10), der laut P3-385 gerade **nicht** unabhängig ist.
- **Fertig heißt:** ein Gutachten als Dokument, das **(a)** die 97
  Swift-Dateien nach Achse gruppiert durchgeht (RDB/MBR · FFS/OFS/FFS2 ·
  PFS3 · FAT32 · ADF · Fluss · LHA) und je Achse „fehlt im Baum / ist da
  und schwächer / ist da und gleichwertig" mit `git ls-files`-Messung
  vergibt — der Auftrag sagt „komplett auseinander, sehr genau" und
  „stimme es mit mein aktuellen tool ab"; **(b)** die Orakel-Frage
  ausdrücklich entscheidet: ohne Swift-Werkzeugkette ist kein Eintrag nach
  `docs/ORACLES.md` möglich, also ist zu sagen, **was genau delegiert
  werden müsste** (Bau auf welcher Plattform, welche Ausgabe wäre der
  Beleg); **(c)** die 26 `.bin` als möglichen **Daten**-Kanal prüft und
  ihre Herkunft/Lizenz getrennt von der Apache-Frage nennt (die Lage aus
  `SCOUT-5`: Repolizenz ≠ Disketteninhalt); **(d)** die fünf Fragen je mit
  Messung beantwortet; **(e)** `P3-385` mit dem Ergebnis fortschreibt statt
  einen neuen Punkt anzulegen; und **(f)** kein Byte nach `src/`,
  `include/` oder `tests/` schreibt — geklont wird nach
  `tools/uft-scout/work/`.
- **Aufwand:** Begutachtung **mehr als eine Sitzung** — 97 Swift-Dateien
  in einer Sprache, für die hier keine Werkzeugkette existiert, also
  reines Lesen ohne Ausführen. Genauer nicht schätzbar. Einbau
  **entfällt** unter dem heutigen Lizenzurteil; was bliebe, wäre ein
  **Nachbau** nach `docs/QUARANTINE_PROCESS.md` §5 — und das ist ein
  eigener Posten mit eigener Eigentümer-Entscheidung.
- **Stand:** —
- **Beleg:** —

### A-014 · `SecurityRonin/disk-forensic` auseinandernehmen — und die Vorfrage lautet: ist das Diskettenarbeit?
- **Status:** aufgenommen · **Aufgenommen:** 2026-09-16
- **Wortlaut:** „https://github.com/SecurityRonin/disk-forensic.git nimm den
  code komplett auseinander , sehr genau / finde alles und alles raussuchen
  was ich übersehen habe , stimme es mit mein aktuellen tool ab / - wo
  können die formate verbessert werden / - ist es auf andere formate
  übertragbar / - welche einstellungen fehlen noch / - was habe wir noch
  nicht / - brauch es eine HAL-Erweiterungen"
- **Kennzahl:** **keine der vier.** Und hier ist die Zeile ausnahmsweise
  ein echter Einwand, nicht nur Buchhaltung: **der Gegenstand ist keine
  Diskette.** E01, VMDK, VHDX, VHD, QCOW2 und DMG sind Festplatten- und
  VM-Behälter, MBR/GPT/APM sind Partitionsschemata für Medien, die größer
  sind als eine Diskette, und ISO 9660 ist optisch. Eine 3,5-Zoll-Diskette
  hat keine Partitionstabelle. **Genau ein Teil trägt** — siehe die
  Aufnahme-Messung.
- **Kanal:** **Spec, und mehr ist hier nicht zu holen.** Gemessen:
  `license.spdx_id` = **Apache-2.0**, Sprache **Rust**, 574 KB, angelegt
  2026-06-05, letzter Push 2026-08-26, 4 Sterne, kein Fork. Apache-2.0
  gegen GPL-2 ist unverträglich — dieselbe Lage wie `A-013`/`P3-385`, und
  auch hier gibt es die zwei Präzedenzfälle `P3-316` ✅ und `P3-353` ✅.
  **Dazu verschlossen: `cargo` und `rustc` sind auf diesem Rechner nicht
  vorhanden**, also ist auch *Orakel* nach `docs/ORACLES.md` hausintern
  unmöglich („Kein Oracle auf Zusicherung"). Es bleibt **Lesen**.
- **Einfrier-Regel:** **nein für das Lesen.** Ein Einbau wäre eine neue
  Behälterschicht und fiele voll unter das Moratorium — als *neues Format*,
  nicht als Bugfix.
- **OPEN_ITEMS:** **`P3-387`** ist der tragende Anker — „Zwei forensische
  Fähigkeiten, die der Baum nicht hat, und die seine eigene Mission
  verlangt: ein stückweiser Hash und ein Fehlerprotokoll, das den Lauf
  überlebt." Dazu `P3-380` (ATX kann exportieren, niemand bietet es an),
  `P3-381` (darf ein Schreiber die Identitätsdatei überschreiben) und
  `KI-6.1` (keine CI-Prüfung durch echte Emulatoren).
- **Bei der Aufnahme gemessen — und ein Fund ist nicht offensichtlich:**
  **(1) Nichts davon ist im Baum.** Fünfzehn Begriffe gesucht, alle **0
  Dateien**: `E01`, `ewf`/`EWF`, `vmdk`, `vhdx`, `qcow`, `dmg`,
  `iso9660`, `GPT`, `APM`. **Mit einer Ausnahme:** `mbr` hat **2 Dateien**
  — `include/uft/formats/uft_fat32_mbr.h` und
  `src/formats/fat32/uft_fat32_mbr.c` —, und `partition` hat 0. MBR ist
  also punktuell da, als FAT32-Anhang, nicht als Achse.
  **(2) Der eine Teil, der trägt, ist `E01` — und zwar als SCHREIBZIEL,
  nicht als Leser.** Das EnCase Expert Witness Format führt eine **CRC je
  32-KiB-Block** und einen Bereichsvermerk für fehlerhafte Sektoren. Das
  ist wörtlich, was `P3-387` als fehlend benennt: „ein SHA-256 je
  (Zylinder, Kopf) statt einer Summe über die ganze Datei" und „`uft_format
  _mark_last_missing()` kennzeichnet im SPEICHER, und keine Datei trägt es
  hinaus". **E01 ist der bestehende Industriestandard für genau diese zwei
  Fähigkeiten** — und der Wert dieses Repos liegt darin, seinen Aufbau zu
  zeigen, nicht darin, VM-Behälter zu lesen. **(3) Die übrigen sieben
  Behälter sind für dieses Werkzeug Fundus**, nicht Auftrag: sie bewegen
  keine Kennzahl und gehören nicht zur Mission. Das ist eine Einordnung,
  keine Abwertung des Repos.
- **Fertig heißt:** ein Gutachten als Dokument, das **(a)** die Vorfrage
  ausdrücklich beantwortet — welche der acht Behälter und drei
  Partitionsschemata für ein **Disketten**werkzeug überhaupt in Betracht
  kommen, je mit Begründung, und welche als Fundus notiert werden;
  **(b)** für `E01` den Aufbau aus dem Rust-Code **liest** und daraus
  benennt, was ein UFT-Schreiber bräuchte (Blockgröße, CRC-Stelle,
  Fehlerbereichs-Satz, Fallmetadaten) — als **Spec**, ohne eine Zeile zu
  übernehmen, und mit dem Hinweis, dass die kanonische Quelle
  libewf/ASR-Dokumentation ist und nicht dieses Repo; **(c)** die Brücke
  zu `P3-387` herstellt oder begründet verwirft; **(d)** die fünf Fragen je
  mit Messung beantwortet; **(e)** festhält, dass `cargo`/`rustc` fehlen
  und ein Orakel damit delegiert werden müsste — samt der Angabe, was
  genau zu bauen und welche Ausgabe der Beleg wäre; und **(f)** kein Byte
  nach `src/`, `include/` oder `tests/` schreibt.
- **Aufwand:** Begutachtung **eine Sitzung, wenn sie sich auf `E01`
  beschränkt** — genau das ist zu entscheiden, nicht zu unterstellen. Eine
  Durcharbeitung aller acht Behälter ist **nicht schätzbar** und wäre nach
  MF-640 Fundus. Einbau **entfällt** unter dem heutigen Lizenzurteil; ein
  E01-Schreiber wäre ein **Nachbau** nach benannter Spezifikation und ein
  eigener Posten mit eigener Eigentümer-Entscheidung.
- **Stand:** —
- **Beleg:** —

### A-015 · `programandala-net/mkmgt` auseinandernehmen — MGT steht auf T1, die Frage ist Beta DOS
- **Status:** aufgenommen · **Aufgenommen:** 2026-09-16
- **Wortlaut:** „https://github.com/programandala-net/mkmgt.git nimm den
  code komplett auseinander , sehr genau / finde alles und alles raussuchen
  was ich übersehen habe , stimme es mit mein aktuellen tool ab / - wo
  können die formate verbessert werden / - ist es auf andere formate
  übertragbar / - welche einstellungen fehlen noch / - was habe wir noch
  nicht / - brauch es eine HAL-Erweiterungen"
- **Kennzahl:** **keine der vier, und hier ist der Grund ungewöhnlich
  günstig: der Behälter ist schon fertig.** Gemessen steht `mgt` auf
  **T1** — die höchste Stufe — mit **sechs** Tests
  (`test_durchschreibprobe`, `test_kopflose_sonden_sind_erreichbar`,
  `test_mgt_gegen_mame`, `test_mgt_schreibt_in_die_datei`,
  `test_mgt_verzeichnis_vollstaendig`, `test_oeffentliche_api_am_korpus`).
  **Damit korrigiere ich meine eigene Erwartung bei der Aufnahme:** ich
  hatte MF-1006 im Kopf („`mgt` von T3 auf T2, Feldabgleich gegen MAMEs
  `coupedsk.cpp`, und er fand KEINEN Fehler") und daraus geschlossen, ein
  fremder **Schreiber** würde die Stufe heben. Er würde sie nicht heben —
  sie ist schon oben.
- **Kanal:** **Spec.** Gemessen: `license.spdx_id` = **GPL-3.0**, Sprache
  **Forth**, 40 KB, letzter Push **2020-05-14**, 2 Sterne. GPL-3 ist mit
  diesem Baum verträglich (er trägt GPL-3-Bestandteile), ein *Port* wäre
  also rechtlich offen — aber **Forth nach C ist eine Neuschreibung, kein
  Port**. Und *Orakel* ist hausintern verschlossen: `gforth`, `pforth`,
  `sf` und `swiftforth` sind **alle vier nicht vorhanden**. Es bleibt
  Lesen. **Das ist die vierte fehlende Werkzeugkette in Folge** —
  `mkfs.fat`/`mtools` (A-005), Swift (A-013), Rust (A-014), Forth (hier).
- **Einfrier-Regel:** **nein für das Lesen.** Ein Beta-DOS-Leser wäre eine
  neue Dateisystemvariante und fiele unter das Moratorium.
- **OPEN_ITEMS:** kein Treffer zu `mkmgt`, `GDOS` oder `Beta DOS`. Der
  Nachbar ist die Namensrolle/Variantenfrage: `mgt` ist ein **Behälter**,
  und was darin liegt, ist GDOS, G+DOS oder Beta DOS — die Achse, für die
  `uft-variants` zuständig ist („wo sagen wir etwas Falsches, ohne dass es
  auffällt?").
- **Bei der Aufnahme gemessen — und ein Wert bleibt:** **(1) Der Behälter
  ist belegt, das Dateisystem nicht vollständig.** Das Repo nennt
  ausdrücklich **drei** DOSe: „ZX Spectrum GDOS, G+DOS and Beta DOS".
  Gemessen im Baum: `DISCiPLE` steht in **5** Dateien, darunter
  `include/uft/formats/uft_mgt.h`, `src/formats/mgt/uft_mgt.c`,
  `include/uft/uft_format_plugin.h`, `include/uft/xdf/uft_xdf_zxdf.h` und
  `src/analysis/profiles/uft_profile_uk.c` — der Baum kennt also die
  DISCiPLE/+D-Herkunft. **`gdos` und `plusd` stehen dagegen nur in zwei
  TESTS** (`test_kopflose_sonden_sind_erreichbar.c`,
  `test_zx_und_pc98_echte_abbilder.c`), sehr wahrscheinlich als
  Korpus-Dateinamen und nicht als unterschiedene Dateisysteme.
  **`Beta DOS`/`BetaDOS`/`betados`: 0 Treffer, überall.** Das ist die
  Lücke, und sie ist klein und benennbar. **(2) Zwei MGT-Leser liegen
  schon im Baum:** `src/formats/mgt/uft_mgt.c` und `src/samdisk/mgt.cpp`.
  Ein dritter wäre die Lage aus P3-147 (G64 hat drei Leser), nicht ein
  Zugewinn. **(3) Eine Messung ist unbrauchbar und wird nicht verwendet:**
  `grep -F "+D"` meldete 12 Dateien, trifft aber jedes `x+D` in
  C-Ausdrücken. Für „+D" gibt es damit **keine** belastbare Zahl in dieser
  Aufnahme.
- **Fertig heißt:** ein Gutachten als Dokument, das **(a)** ausdrücklich
  feststellt, was ein Schreiber für `mgt` **nicht** mehr leisten kann,
  weil das Format auf T1 steht — und damit die Erwartung korrigiert, statt
  sie zu bedienen; **(b)** die drei DOSe **einzeln** gegen den Baum hält:
  liest UFT GDOS, G+DOS und Beta DOS, oder nimmt es eines an und schweigt?
  Je mit `git ls-files`-Messung und, wo möglich, an einem Korpus-Abbild;
  **(c)** die Forth-Quelle als **Spec** liest — Verzeichnisaufbau,
  Sektorbelegung, Namensfelder — und daraus einen Prüfauftrag formt statt
  eines Ports; **(d)** die fünf Fragen je mit Messung beantwortet;
  **(e)** die vierte fehlende Werkzeugkette in Folge als eigenen
  Vorschlag für `docs/OPEN_ITEMS.md` benennt, weil ein Orakel-Kanal, der
  regelmäßig an der Werkzeugkette scheitert, eine Entscheidung braucht
  (delegieren wie die Tier-3-Bank, oder die Kanalregel präzisieren); und
  **(f)** kein Byte nach `src/`, `include/` oder `tests/` schreibt.
- **Aufwand:** Begutachtung **eine Sitzung** — 40 KB Forth sind der
  kleinste Gegenstand dieser Reihe, und die Frage ist eng (drei DOSe, ein
  Behälter, der schon belegt ist). Einbau: für Beta DOS **nicht
  schätzbar** und ein eigener Posten unter dem Moratorium.
- **Stand:** —
- **Beleg:** —

### A-004 · AUFTRAG „UFT offene Punkte, autonome Abarbeitung" — Punkt 7
- **Status:** **angehalten am vereinbarten Schnitt** · **Aufgenommen:**
  2026-09-16 (nachgetragen) · verschoben ans Ende 2026-09-16
- **Wortlaut:** „danach die verbleibenden Punkte aus OPEN_ITEMS.md, nach
  demselben Muster." · zuletzt bestätigt mit „weiter mit punkt 7"; der
  Schnitt ist genehmigt mit „ja, schnitt bei P3-423 ist ok"
- **Kennzahl:** je Teilposten verschieden; die Punkte haben T3 und die
  Wandlungspfade nicht bewegt, sondern Falschaussagen behoben — die
  Kennzahl steht im jeweiligen Commit, nicht hier
- **Kanal:** entfällt (eigener Baum)
- **Einfrier-Regel:** ja für jeden Format-Teilposten → **Rotbeweis zuerst**
- **OPEN_ITEMS:** die Restliste; nach dem Schnitt offen: **P3-425**
  (dieselbe Diskette, zwei Geometrien — `xfd` liest die Quad Density als
  40×2, `atr` weiter als 80×1, und `atr`s Sonde gewinnt mit 95 gegen 40),
  **P3-426** (`tests/CMakeLists.txt` sammelt die Format-Schicht an zwölf
  Stellen mit je einem eigenen GLOB ein, elf davon halten eine
  Aufzählung), **P3-421 Teil 2** (die Rückfallebene von `repo_scope`
  betrifft 16 Skripte), die MF-1162-Punkte und die Eigentümerränge 4–10.
  Neu dazugekommen durch die Arbeit selbst: **P3-439**, **P3-440**,
  **P3-441**.
- **Fertig heißt:** kein Punkt aus der Restliste mehr ohne Commit **oder**
  ohne benannte Stoppbedingung (S1–S5) in `docs/OPEN_ITEMS.md`
- **Aufwand:** nicht schätzbar (Restliste offen)
- **Stand:** **Schnitt erreicht, zwei Commits lokal, nichts gepusht.**
  MF-1182 (P3-406, Sonden-Doktrin Regel 2) und MF-1183 (P3-423, Geometrie
  aus dem BPB) liegen; `git rev-list --count origin/main..HEAD` = **2**.
  Beide Male Bau 0 Warnungen, Suite **493/493 grün** mit einem benannten
  Skip (`test_freezer`), `check_consistency` 0, alle Vorhaken grün.
  **Eine Lehre aus dem Lauf, die beim Wiederaufnehmen gilt:** die
  Generator-Reihenfolge ist vollständig zu fahren —
  `gen_verification_tiers.py --write` → `gen_fs_tiers.py` →
  `gen_erzeuger_zensus.py` → `update_inventory.py` → `gen_stand.py`
  **zuletzt**. `kette1182.sh` ließ die ersten zwei aus, und
  `check_consistency` brach mit `[verification tiers stale]` und
  `[FS tiers stale]` ab — der Fehler lag im Kettenskript, nicht im Baum.
  Die fertige Kette liegt als `kette1183.sh` im Job-Kratzverzeichnis.
  **Nächster Griff beim Wiederaufnehmen:** P3-425, weil dort zwei Leser
  dieselbe Nutzlast verschieden zerlegen und das Erkennungsrennen die
  falsche Geometrie gewinnt — ein messbarer Befund mit Rotbeweis-Ort.
- **Beleg:** MF-1175…MF-1181 = `90c4f88b..76c56535`; **MF-1182 =
  `1d43b345`**; **MF-1183 = `e5daccf1`**. Der Posten bleibt offen, weil die
  Restliste offen ist — nicht weil ein Beleg fehlt.

---

## Fundus

*(aufgenommen, bewegt aber keine der vier Kennzahlen aus `CLAUDE.md`
§„jeder Baustein benennt seine Kennzahl" — nach MF-640 ist das Fundus,
nicht Auftrag. Steht hier, bis ein Anlass es hochholt.)*

---

## Erledigt

*(mit Beleg: Commit-Hash und MF-Nummer)*

---

## Zurückgenommen

*(mit Grund. Eine Liste, die durch Schrumpfen produktiv aussieht, ist
der Spiegelfehler aus MF-1077.)*
