# Ein Rückstand, eine Ordnung

**Stand 2026-08-24.** Zusammengeführt aus `OPEN_ITEMS.md`, `KNOWN_ISSUES.md`,
`MASTER_PLAN.md`, `STUB_ELIMINATION_PLAN.md`, `VERIFICATION_PLAN.md` und den
Funden der Prüf-Sitzung MF-534…543.

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
| **A3** | **294 Header-Prototypen ohne Definition.** Davon 2 im gefährlichen Fall: `static inline`-Wrapper im selben Header rufen sie. Der erste echte Aufrufer bricht den Link | gemeldet, Stichproben gemessen | **offen** |
| **A4** | **POSIX-Plugin kann nie gewinnen.** `posix_probe_plugin()` gibt unbedingt `false` zurück; die echte Erkennung ist pfadbasiert und wird über `.probe` nie gerufen. Steht in der Registry als Fähigkeit | gemessen | **offen** |
| **A5** | **Fünf `uft_format_id_t` unter einem Wächter.** `UFT_FMT_D64` ist 4, 6 oder 20 je nach Include-Reihenfolge. Heute sehen alle gebauten Einheiten dieselbe Zahl — überwacht, nicht behoben | gemessen | **überwacht** (26. Tor), Zusammenlegung offen |
| **A6** | **14 Stellen „Erfolg ohne Tat"** in der Wandler-Schicht: Datei voller Größe geschrieben, `success = true`, obwohl null Spuren gewandelt wurden. In 5 davon lügt zusätzlich der Zähler | gemeldet | **offen** — Muster-Vorlage existiert (`uftc_convert_scp_to_mfm_sectors`) |
| **A7** | **GUI-Recovery meldet „Recovery complete" für eine Simulation.** Fortschrittsbalken läuft Spur für Spur, dann „Verifying data integrity — re-checking all sector CRCs". Nichts davon passiert | gemeldet | **offen** |
| **A8** | **`uft_cross_track_recover()` kopiert bei ≥90 % Ähnlichkeit den Sektor einer *anderen* Spur** in den defekten, setzt `valid = true` und übernimmt sogar den Hash des Spenders. Keine Provenienz-Markierung | gemeldet | **offen** |

---

## B — sichtbarer Datenverlust oder Absturz

| # | Sache | Herkunft | Stand |
|---|---|---|---|
| **B1** | 9 von 137 Plugins vom Fuzzer nie erreicht (MSA, DIM_ATARI, DC42, D77, D88, FDI_PC98, CFI, Logical, POSIX). Sie verlangen einen Kopf, dessen Inhalt und Dateilänge zueinander passen | gemessen | **offen** — braucht Erzeuger, der beides gemeinsam setzt |
| **B2** | Fuzzer-Temppfad ist relativ; zwei Instanzen im selben Verzeichnis überschreiben einander die Eingabe. Falle für parallele CI | gemeldet | **offen** |
| **B3** | Flux-Puffer-Deckel 131072 an mehreren Stellen: überzählige Flusswechsel werden **still** verworfen. Eine Stelle meldet es seit MF-528, die übrigen nicht | gemeldet | **offen** |
| **B4** | FS-Extraktion (FAT12, AmigaDOS): Kettenbruch → `break`, Torso-Datei mit `OK`-Status. Kein Abgleich `geschrieben == Soll` | gemeldet | **offen** |

---

## C — behauptete, nicht vorhandene Fähigkeit

| # | Sache | Herkunft | Stand |
|---|---|---|---|
| **C1** | **„55+ Kopierschutz-Schemes"** — `src/protection/` hat 20 Dateien, ~200 Funktionen und **keinen Aufrufer**. Die Oberfläche zeigt eine Heuristik | gemessen (P0-2) | **offen** |
| **C2** | **Schreib-Sicherheitstor ohne Aufrufer.** Ein Tor, das nie läuft, ist eine Zusage, die niemand einlöst | gemessen (P0-4) | **blockiert** — Hardware-Sitzung |
| **C3** | 22 Banner-Header sind unfertig; der Skelett-Audit sieht sie nicht | gemessen (P1-5) | **offen** |
| **C4** | 7 Header-Duplikate brauchen echte Zusammenführung | gemessen (P1-7) | **offen** |
| **C5** | `uft_track_writer.h` (473 Zeilen) und `uft_xdf_mxdf.h` (336 Zeilen) sind zu 100 % Phantom — dieselbe Klasse, die MF-366 einmal entfernt hat | gemeldet | **offen** |

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

---

## Was in dieser Sitzung abgetragen wurde

34 Fehler, MF-534…543. Die vier, die etwas über den Baum sagen:

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
