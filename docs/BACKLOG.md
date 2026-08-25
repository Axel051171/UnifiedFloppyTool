# Ein Rückstand, eine Ordnung

**Stand 2026-08-24.** Zusammengeführt aus `OPEN_ITEMS.md`, `KNOWN_ISSUES.md`,
`MASTER_PLAN.md`, `STUB_ELIMINATION_PLAN.md`, `VERIFICATION_PLAN.md` und den
Funden der Prüf-Sitzung MF-534…548.

**Fortschritt:** von 24 Einträgen sind 4 abgetragen (A4, A6, A7, A8),
1 überwacht statt behoben (A5). Was bleibt, steht unten mit dem Grund,
warum es bleibt.

---

## Wonach sortiert wird

Nicht nach Subsystem, nicht nach „Schweregrad", nicht nach Aufwand.

**Nach der Frage: was passiert, wenn es falsch ist und niemand merkt es?**

Das ist für dieses Werkzeug die einzige Rangfolge, die trägt. Ein Absturz ist
ärgerlich — er wird bemerkt, jemand meldet ihn, er wird repariert. Eine
Diskette, die als „vollständig gesichert" gemeldet wird und es nicht ist, wird
**nie** bemerkt: das Original wandert danach in den Container, und die
Fälschung ist ab da die einzige Wahrheit.

| Stufe | Kriterium |
|---|---|
| **A** | Das Werkzeug sagt etwas Falsches über gesicherte Daten, und der Benutzer kann es nicht sehen |
| **B** | Das Werkzeug beschädigt oder verliert Daten, sichtbar |
| **C** | Das Werkzeug behauptet eine Fähigkeit, die es nicht hat |
| **D** | Das Werkzeug ist unvollständig, sagt es aber |

Ein Absturz ist Stufe B, nicht A. Das ist gegen die übliche Rangfolge und
Absicht: **ein Absturz ist ein ehrlicher Fehler.**

Herkunft jedes Eintrags ist vermerkt: *gemessen* (in dieser Sitzung selbst
nachgeprüft) oder *gemeldet* (aus einem Bericht übernommen, noch nicht
nachgemessen). Die Unterscheidung ist nicht Formsache — von vier
Agentenberichten dieser Sitzung war **jeder einzelne** in mindestens einer
Zahl korrekturbedürftig.

---

## A — falsche Aussage über gesicherte Daten

| # | Sache | Herkunft | Stand |
|---|---|---|---|
| **A1** | **57 von 88 tier-geführten Formaten stehen auf T3 — ungeprüft.** Kein Test, oder ein synthetischer Test ohne Abgleich gegen eine autoritative Quelle. Genau in dieser Lage waren die fünf fabrizierten Parser grün. Belegt: T1=2, T1b=12, T2=17 | gemessen | **offen**, Moratorium MF-363/498 |
| **A2** | **29 von 44 Wandlungspfaden ungeprüft.** Das Preflight-Tor weist sie ab — richtig. Aber die Doku nennt weiter „44 Pfade" als Fähigkeit | gemessen | **offen**; Zahlen seit MF-541 abgeleitet |
| **A3** | Header-Prototypen ohne Definition | gemeldet, dann gemessen | ⚠ **294 → 237 (MF-549)**: die 2 gefaehrlichen Faelle (von `static inline` im selben Header gerufen) sind weg, dazu zwei Header mit 809 Zeilen und 0 Implementierung. **237 bleiben** — davon 40 in zwei Tests, die in EXCLUDED_TESTS stehen |
| ~~**A4**~~ | POSIX-Plugin kann nie gewinnen: `posix_probe_plugin()` gibt unbedingt `false` zurueck, und `uft_disk_open()` waehlt nur ueber den Inhalt | gemessen | ✅ **MF-546** — ehrlich beschriftet, 27. Tor verhindert den zweiten Fall (gemessen: es ist der einzige) |
| ~~**A5**~~ | Fuenf `uft_format_id_t` unter einem Waechter | gemessen | ⚠ **MF-540/559 ueberwacht.** Teil des groesseren Befundes C4 — `UFT_FMT_D64` selbst ist NICHT scharf (alle Einheiten sehen 4); drei andere Konstanten sind es |
| ~~**A6**~~ | 14 Stellen `success = true`, weil sich die Datei ANLEGEN liess — nicht weil etwas drinstand | gemeldet, dann gemessen | ✅ **MF-545** — 7 Wandler mit Spurschleife auf `uftc_finish_or_refuse()`; die 7 uebrigen sind Rohkopien ohne Schleife. Dazu ein Mantel um die 13 Ausgaenge von `uft_convert_file()`, damit kein Fehlschlag eine ALTE Datei zuruecklaesst |
| ~~**A7**~~ | GUI-Recovery zaehlte einen Fortschrittsbalken hoch und meldete „Recovery complete“, ohne die Diskette anzufassen | gemeldet, dann gemessen | ✅ **MF-548** — kein Balken, keine Spurmeldung, keine Erfolgsmeldung; auch der Wizard-Text im C-Teil. **Kein Bedien-Rauchtest** |
| ~~**A8**~~ | `uft_cross_track_recover()` ueberschrieb einen defekten Sektor mit den Bytes eines anderen, setzte `valid = true` und uebernahm **den Hash des Spenders** | gemeldet, dann gemessen | ✅ **MF-547** — zaehlt jetzt Kandidaten, aendert nichts. **Kein Tor bewacht das** (Typen dateilokal, kein Aufrufer) |

---

## B — sichtbarer Datenverlust oder Absturz

| # | Sache | Herkunft | Stand |
|---|---|---|---|
| ~~**B1**~~ | 9 von 137 Plugins vom Fuzzer nie erreicht | gemessen | ✅ **MF-556** — sechs Erzeuger, die Kopf und Dateilaenge gemeinsam setzen. **135 von 137**; die zwei uebrigen (POSIX, CFI) sind dokumentiert unerreichbar |
| ~~**B2**~~ | Fuzzer-Temppfad relativ; zwei Instanzen ueberschreiben einander die EINGABE | gemessen | ✅ **MF-553** — Pfad traegt die Prozesskennung. War bei der QRST-Suche real und kostete zwanzig Minuten |
| ~~**B3**~~ | Flux-Puffer-Deckel 131072: ueberzaehlige Flusswechsel still verworfen | gemeldet, dann gemessen | ✅ **MF-550** — alle drei Stellen melden und beziffern jetzt; gemessen 129024 von 260096 verworfen, Spur zaehlt als gescheitert |
| ~~**B4**~~ | FS-Extraktion: Kettenbruch → Torso-Datei mit `OK`-Status | gemeldet, dann gemessen | ✅ **MF-551** — FAT12 und AmigaDOS vergleichen jetzt gegen die gemeldete Groesse und schreiben bei einer Luecke gar nichts. **Kein Waechter**: der Test-Baukasten bedient `find_path` nicht, Grund im Testbaum |

---

## C — behauptete, nicht vorhandene Fähigkeit

| # | Sache | Herkunft | Stand |
|---|---|---|---|
| **C1** | **„55+ Kopierschutz-Schemes“ steht als Kernfunktion in `CLAUDE.md` und `README`.** Gemessen: **38 Dateien, 350 Funktionen, 4 von aussen gerufen, 16 von einem Test beruehrt** — und eine der vier ist ein CRC-Helfer. **334 Funktionen sind weder verdrahtet noch geprueft** | gemessen (MF-557) | ⚠ **ueberwacht, nicht verdrahtet.** Den Katalog anzuschliessen hiesse, 334 ungeprueste Funktionen an ein forensisches Urteil zu haengen — genau die Lage, aus der die fuenf fabrizierten Parser kamen. Die Oberflaeche sagt es bereits von sich aus (`ProtectionAnalysisWidget.cpp`); seit MF-557 haelt `scripts/audit_protection_claims.py` die Zahl fest |
| **A9** | **`uft_convert_memory()` ging vollständig am Preflight-Tor vorbei.** Es übergibt keine Dateipfade; die Prüfung kehrte ohne Pfade sofort mit `ABORT_INVALID_ARG` zurück — und der Aufrufer zählte nur drei Abbruchwerte einzeln auf, dieser war nicht dabei | gemessen (MF-567) | ✅ **MF-567** — gemessen: `IMG→SCP`, in der Matrix UNMÖGLICH mit *„synthesising flux would be fabrication"*, lieferte ohne `accept_data_loss` **3 712 758 Byte aus 4096 Byte Zufall**, Rückgabe UFT_OK. Direkt darüber stand seit UFT-A01 der Satz, dieser Umweg sei geschlossen. Zwei Änderungen: das Urteil hängt jetzt an den **Formaten**, nicht an Dateinamen (Pfade braucht nur die Nebendatei), und der Aufrufer zählt auf, was **durchlässt** (`!= UFT_PREFLIGHT_OK`) statt was abbricht — ein unbekannter Entscheidungswert blockiert damit. Abgewiesene Paare im Speicher-Modus: **0 → 32** |
| ~~**C11**~~ | **11 Paare bot die Wandlungstabelle an, ohne dass der Verteiler sie kennt** | gemessen (MF-567) | ✅ **MF-567** — der Benutzer lief bis in den Rückfall („dispatch not yet implemented"), nachdem Tabelle **und** Tor ihm zugesagt hatten, der Weg sei gangbar. Nach dem Tor-Fix: **11 → 1**; das letzte (`IPF→ADF`) hing an einem Matrix-Urteil ohne Wandler. Drei solche Urteile entfernt (`SCP→IMD`, `IPF→ADF`, `STX→ST`); zwei davon standen seit MF-526 als „Verdikte ohne Konsumenten" in CLAUDE.md — **festgestellt und stehen gelassen ist nicht behoben.** Jetzt 0, gehalten von `tests/test_convert_table_has_dispatch.c` |
| **C12** | **10 von 22 Wandler-Funktionen haben null Tests** (`imd→img`, `img→imd`, `kryoflux→{adf,d64,hfe,scp}`, `nbz→{d64,g64}`, `td0→{imd,img}`) | gemessen (MF-567) | ⚠ **nicht angeboten** — keins der zehn hat einen Matrix-Eintrag, das Preflight-Tor weist alle ab. Das ist die Konstruktion aus MF-263, und sie trägt. Ein Test wird erst nötig, wenn eines davon angeboten werden soll |
| ~~**A10**~~ | **Die Konvertieren-Schaltfläche der Oberfläche rief `QFile::copy()` und meldete „Conversion complete!"** | gemessen (MF-568) | ✅ **MF-568** — wer SCP→D64 wählte, bekam eine byteweise Kopie der SCP-Datei unter dem Namen `.d64`. Preflight-Tor, Rundlauf-Matrix, Verlustmeldung und sämtliche Wandler wurden nicht einmal berührt. Zweiter Fall im selben Muster: `DecodeJob::convertImage()` — dort hatte MF-122 sogar den Rückgabewert von `write()` **innerhalb** der Kopie gehärtet, ohne zu bemerken, dass die Funktion nicht wandelt. Beide gehen jetzt durch `uft_convert_file()`. **Bedien-Rauchtest offen (D4)** |
| ~~**A11**~~ | **`uftc_convert_hfe_to_sectors()` meldete Erfolg bei null gewandelten Spuren** | gemessen (MF-568) | ✅ **MF-568** — aus einer hinter dem Kopf abgeschnittenen HFE (1024 Byte) entstand ein **737 280-Byte-IMG**, `UFT_OK`, `success = true`, **0 Spuren gewandelt / 80 gescheitert**. Ein vollständig erfundenes Abbild als Erfolg. Achter Wandler des MF-545-Musters — die anderen sieben waren erfasst, dieser nicht. Zehnter Geschwister-Fall der Reihe |
| ~~**C13**~~ | **Die Oberfläche führte eine EIGENE Wandlungsliste** (`m_conversionMap`, 40 Zeilen) — die vierte nach Tabelle, Matrix und Verteiler, und die einzige, die der Benutzer je sah | gemessen (MF-568) | ✅ **MF-568** — sie bot Paare an, die es nicht gibt: `SCP→ATR`, `SCP→WOZ`, `TRD→SCL`, `D64→TAP`. Die Liste kommt jetzt aus `uft_convert_list_targets()`, also aus der Tabelle selbst. Neu dafür: `uft_format_from_name()` (Rückrichtung zu `uft_format_get_name()`), belegt in `tests/test_format_from_name.c` — 17 von 17 Formaten laufen rund, 44 von 44 Zielen sind über ihren angezeigten Namen wiederauffindbar |
| **A12** | **Der Datei-Browser erfindet Verzeichnislisten.** `ExplorerTab::listDirectory()` liefert für ADF, D64 und ST/MSA fest verdrahtete Einträge mit plausiblen Namen, Größen und Attributen — nicht aus dem Abbild gelesen | gemessen (MF-568) | ⚠ **offen, nächster Schritt.** 13 erfundene Einträge über drei Formate: ADF 6 (`s`, `c`, `devs`, `libs`, `Disk.info`, `Startup-Sequence`), D64 4 (`GAME`, `DEMO`, `MUSIC`, `DATA`, eine davon mit Splat-Flag), ST/MSA 3. Der *generische* Zweig ist ehrlich („Directory listing not available for this format") — nur die drei Formate, die ein Benutzer am ehesten öffnet, fabrizieren. Der Baum **hat** eine Dateisystem-Schicht (AmigaDOS, FAT12, CBM DOS); die Daten könnten echt sein. Für einen Forensiker ist eine erfundene Dateiliste nicht von einer echten zu unterscheiden |
| **C2** | **Schreib-Sicherheitstor ohne Aufrufer.** Ein Tor, das nie läuft, ist eine Zusage, die niemand einlöst | gemessen (P0-4) | **blockiert** — Hardware-Sitzung |
| ~~**C3**~~ | „22 Banner-Header sind unfertig“ | gemessen | ✅ **MF-558** — der Audit zaehlte 78; **67 davon waren leere Aufhaenger**, die sich nur als unfertig beschrifteten. Jetzt **11**, davon 8 mit wirklich offenen Prototypen. Die Zahl, um die es ging, war nie 22 und nie 78 |
| ~~**C4**~~ | 7 Header-Duplikate brauchen Zusammenfuehrung | gemessen | ⚠ **MF-559 gemessen, nicht aufgeloest**: 37 Kollisionen, davon **5 mit echtem Wert-Konflikt** und **3 in gebautem Code scharf** (`UFT_PROT_COPYLOCK` hat fuenf Werte). Das Tor misst jetzt fuenf Sonden statt einer und friert die Verteilung ein. Zusammenlegen = ABI-Arbeit |
| ~~**C5**~~ | `uft_track_writer.h` (473 Z.) und `uft_xdf_mxdf.h` (336 Z.) zu 100 % Phantom | gemeldet, dann gemessen | ✅ **MF-549** — beide entfernt, beide Baeume bauen |
| ~~**C8**~~ | **`SCP→D64` stand als angebotener Pfad in der Matrix und lieferte eine leere Diskette bei `success = true`** | gemessen (MF-565) | ✅ **MF-565** — drei Ursachen: Zonen-Zellzeiten vertauscht (4,0 µs auf der schnellsten Zone), Sektor-Parser ein Stub, nur EINE Umdrehung und die nach `max_flux` gewählt. **0 von 683 → 683 von 683.** Kein Test im Baum hatte diesen Pfad je angefasst |
| ~~**C9**~~ | **Die CBM-Datenprüfsumme ist ein einziges XOR-Byte — ein zerstörter Sektor besteht sie mit 1/256.** Der Wandler behandelt „Prüfsumme heil" als „geprüft" und lässt spätere Umdrehungen dann nicht mehr ran | gemessen (MF-565) | ✅ **MF-566** — Rotbeweis: *gemeldet darf nie mehr sein als byteweise richtig*; er feuerte mit 683 gemeldet gegen 680 richtig. Der D64-Pfad stimmt jetzt über **denselben** Abstimmer ab, den der ADF-Pfad seit MF-473 fährt (`multiread_*`, byteweise, geprüfte Lesung wiegt 100 gegen 50) — kein zweiter Abstimmer. Nachher: **683 von 683 richtig, 680 gemeldet + 3 als unbestätigt benannt.** Der Bericht untertreibt jetzt, statt zu übertreiben. Preis gemessen: die Suite läuft von 33,5 s auf 51,4 s, weil alle Umdrehungen gelesen werden |
| **C10** | **Zwei Zählweisen für dieselbe SCP-Datei.** `scp_writer_add_track()` nimmt den Zylinder und rechnet `*2 + side`; `uft_scp_get_track_flux()` nimmt den fertigen SCP-Index | gemessen (MF-565) | ⚠ **offen.** Kostete beim Bau des Rotbeweises einen halben Durchgang: mit dem Halbspur-Index landeten die Spuren doppelt so weit auseinander, und nur Spur 1 traf — **21 von 683**. Dazu prüft der Schreiber `track_num < 84`, rechnet dann aber bis `167`. Keine Datenverfälschung gemessen, aber eine Falle für jeden nächsten Aufrufer |

---

## D — unvollständig, und sagt es

| # | Sache | Blocker |
|---|---|---|
| **D1** | Tier-3-Hardware-Bench | **kein Gerät** (MF-310) — an die Gemeinschaft delegiert |
| **D2** | LIC-1 Lizenzherkunft `uft_multiread_pipeline.c` | **Entscheidung des Eigentümers** |
| **D3** | Korpus-Beschaffung (`cpmtools`, SAMdisk `tc.cpp`, `sector-cpc`) | **Eigentümer** |
| **D4** | GUI-Rauchtest MF-496/501 | **Eigentümer** — kein Bediener |
| **D5** | 12 Wandlungspfade brauchen Korpusdateien, 10 einen Wandler, der nicht existiert | teils Beschaffung, teils Neubau |
| **D6** | ATR Enhanced Density: gemeldete Geometrie falsch, **kein Datenverlust** | braucht benannte Referenz (MF-498a) |
| **D7** | Teilaufnahme-Karte nach ddrescue-Vorbild | offen |
| ~~**D8**~~ | **`audit_unbounded_alloc.py` schlüsselt seine Ausnahmen nach Zeilennummer.** Jede Änderung oberhalb einer der 9 begründeten Stellen lässt das Tor rot werden, und die billigste Antwort ist, die Zahl hochzuzählen — ohne die Begründung noch einmal zu lesen. Genau die stille Drift, gegen die das Tor gebaut wurde. Gemessen: ohne Zeilennummer kollidiert genau **1 von 9** Schlüsseln, ein Zähl-basierter Abgleich pro `(Datei, Variable)` würde tragen | ✅ **MF-566** — abgeglichen wird jetzt nach **Anzahl je (Datei, Variable)**; die Zeile im Schlüssel ist Fundhilfe, nicht Schlüssel. Drei Rotbeweise: eine reine Verschiebung feuert nicht, eine **zweite** Stelle mit demselben Namen in derselben Funktion feuert, und eine Ausnahme ohne Fundstelle feuert. Der erste Anlauf des zweiten Beweises zielte daneben (Einschleusung landete in einer anderen Funktion) und meldete grün — ein Beweis, der nicht feuert, ist wertlos |

---

## Was in dieser Sitzung abgetragen wurde

38 Fehler, MF-534…548. Die vier, die etwas über den Baum sagen:

**MF-538 — ich habe der Statistik geglaubt und den Zählern nicht.** Eine
Wandlung meldete „0,08 % Abweichung". Die Quelldatei war eine leere Diskette
mit genau 733 Bytes ungleich null, und die Abweichung war genau 733: das
Ergebnis war **leer**. Die Zähler hatten die ganze Zeit recht. Seither rechnet
der Test die **Nulllinie** mit — wie weit wäre eine Datei aus lauter Nullen
entfernt? Ohne sie ist jede Prozentzahl über einem dünnen Abbild wertlos.

**MF-539 — der richtige Encoder lag die ganze Zeit daneben.** `ADF→HFE`
schrieb rohe Bytes statt MFM-Zellen, ließ beide CRC-Felder auf `0x00 0x00`
und spiegelte die Bits nicht. Daneben lag ein vollständiger, korrekter
Encoder ohne einen einzigen Test. Nach der Verdrahtung: `IMG→HFE→IMG`
bitgleich.

**MF-542 — eine Prüfung, die immer wahr war.** `crc_stored = crc_computed`,
dann `crc_valid = (computed == stored)`. Jede noch so zerstörte Diskette wäre
als 100 % CRC-gültig aus der forensischen Recovery gekommen.

**MF-543 — 22 Byte genügen.** `heads` als uint16 aus dem Kopf, `uft_disk_alloc()`
nimmt uint8. Feld mit 88 Plätzen, beschrieben bis Index 599. Die Anzeige log
nicht — sie zeigte den gekürzten Wert.

---

## Die Regel, die aus allen vieren folgt

In jedem der vier Fälle gab es **eine Zahl, die stimmte, und eine Anzeige, die
beruhigte**, und niemand hat sie verglichen.

* MF-538: die Zähler sagten „160 gescheitert", die Prozentzahl sagte „0,08 %".
* MF-539: `tracks_converted` sagte 80, die Datei enthielt kein einziges Sync.
* MF-542: `crc_valid` sagte wahr, verglichen wurde ein Wert mit sich selbst.
* MF-543: die Geometrie meldete 44, die Schleife lief bis 300.

**Eine Zahl ohne ihre Bezugsgröße ist keine Messung.** Das ist die
Arbeitsregel, die aus dieser Sitzung bleibt — und sie gilt für die eigenen
Prüfwerkzeuge zuerst: sieben Messwerkzeuge dieser Sitzung haben beim ersten
Anlauf falsch gemessen, und der einzige Grund, dass keines davon stehen blieb,
ist die Reihenfolge **Rotbeweis zuerst**.
