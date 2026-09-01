# XCopy-Prüfaufträge — die saubere Übergabe (MF-745)

**Dies ist die einzige Datei dieses Vorgangs, die Hand B lesen darf.**

Sie enthält Anforderungen an **UFTs eigenes Verhalten** und
Rotbeweis-Skizzen. Sie enthält keine Aussage darüber, wie die Vorlage
etwas umsetzt — keine Struktur, keine Zerlegung, keine Namensgebung,
kein Datenlayout.

Die dazugehörigen Verhaltensbefunde liegen in
[`xcopy_verhalten_HAND-A.md`](xcopy_verhalten_HAND-A.md). **Wer die
liest, ist für die Umsetzungsseite verbrannt** und darf diese
Prüfaufträge nur noch auditieren, nicht implementieren.

Herkunft: Gutachten des Eigentümers vom 2026-09-01, §4.

**Die Vorlage, eingefroren über `tools/uft-nachbau/scripts/sichtprotokoll.py`:**

| Archiv | SHA-256 (Anfang) | gelesen |
|---|---|---|
| `xcopypro_source_2011.zip` | `0047220e40ae5441…` | **nur** `xcopy_licence.txt` |
| `xcopypro_source_2011_v2.zip` | `09d051e6eba9bc8b…` | nichts |
| `xcopypro_source_2011_v2(1).zip` | `0df1adc3ee4002ca…` | **nur** `Readme 2011` |

**Ungeöffnet:** beide ADFs, `Source/xcopypro_src_original_1992.lha`,
`Source/xcopypro_src_fixed_2011.lha`.

> **MF-749 — warum diese Datei hier steht und nicht in
> `tools/uft-nachbau/out/`.** Dort lag sie zuerst, und
> `tools/uft-nachbau/.gitignore` ignoriert `out/`. Die Commit-Texte von
> MF-745 und MF-747 sagten „Das Gutachten abgelegt" und nannten die
> Pfade — **die Dateien sind nie in den Baum gekommen.** Wer klont,
> bekommt die Beschreibung ohne den Gegenstand. `out/` ist für
> Wegwerf-Material des Agenten; eine Entscheidungsgrundlage gehört
> nach `docs/`.

---

## P1 — Kalibriert UFT die Schreiblänge je Laufwerk?

**Anforderung.** Beim Schreiben einer Rohspur muss die verwendete Länge
aus einer Messung am konkreten Ziellaufwerk stammen, nicht aus einem
Nennwert. Zu klären: tut UFT das, wird die Messung je Laufwerk
zwischengespeichert, gibt es eine Reserve gegen Drehzahlschwankung?

**Rotbeweis.** Zwei Laufwerke mit messbar verschiedener Drehzahl,
dieselbe Quellspur. Wird auf beiden dieselbe Bytezahl geschrieben, ist
der Nennwert im Spiel. Erwartetes Fehlerbild bei Nennwertbetrieb: auf
dem schnelleren Laufwerk überschreibt das Spurende den Spuranfang.

> **Hinweis für diesen Baum:** braucht Hardware. Dieses Projekt hat
> keine (MF-310) — der Rotbeweis ist Tier-3 und wird an die Gemeinde
> delegiert. Der Code-Teil der Frage („gibt es überhaupt eine
> Kalibrierung, oder steht dort eine Konstante?") ist dagegen sofort
> messbar.

## P2 — Findet die Sync-Suche bitversetzte Muster?

**Anforderung.** Die Nachsuche muss alle Bitlagen abdecken. Zu klären,
ob UFTs Sync-Erkennung wortweise oder bitweise arbeitet.

**Rotbeweis.** Fixture mit einem Sync-Muster, das bewusst um eine
ungerade Bitzahl versetzt liegt. Eine wortweise Suche meldet die Spur
als syncfrei, eine bitweise findet ihn. Synthetisch erzeugbar, braucht
keine Originaldiskette.

> **Beantwortet MF-745, und der Befund ist rot.** Siehe unten
> „Erledigte Aufträge".

## P3 — Trennt UFT „gerettet" von „sauber gelesen"?

**Anforderung.** Eine Spur, die erst nach Lesefehler im Rohverfahren
übernommen wurde, darf im Bericht nicht als fehlerfrei erscheinen. Zu
klären, ob UFTs Verdikte diesen Zustand als eigene Klasse führen.

**Rotbeweis.** Fixture mit einem gezielt beschädigten Sektor. Erscheint
die Spur nach der Übernahme ohne Merkmal, ist der Befund rot.

## P4 — Behandelt UFT syncfreie Spuren als eigene Klasse?

**Anforderung.** Eine gleichförmig beschriebene Spur ohne Sync ist
nicht dasselbe wie eine leere und nicht dasselbe wie eine unlesbare. Zu
klären, ob UFT diese drei Zustände unterscheidet.

**Rotbeweis.** Drei Fixtures: leere Spur, gleichförmige Spur mit wenigen
Diskontinuitäten, verrauschte Spur. Fallen zwei davon auf dasselbe
Verdikt, ist der Befund rot.

**Nebenauflage.** Falls UFT dabei Daten rekonstruiert — auffüllt,
glättet, ergänzt —, muss das als **Ableitung gekennzeichnet** und vom
Messwert getrennt sein.

## P5 — Wird die überlange Spur erkannt und benannt?

**Anforderung.** Eine Spur oberhalb der Regellänge ist ein eigener
Zustand, keine Fehlmessung. Zu klären, ob UFT eine Längenschranke führt
und ob sie im Bericht auftaucht.

**Rotbeweis.** Fixture mit einer Spur deutlich über Nennlänge. Wird sie
stillschweigend gekürzt, ist der Befund rot.

## P6 — Woraus leitet UFT den Schreibstartpunkt ab?

**Anforderung.** Der Startpunkt muss aus der Analyse der aufgenommenen
Spur stammen, nicht aus einer festen Annahme über den Indexbezug. Zu
klären, welche Quelle UFT verwendet und ob es einen Ausweichfall für
Startpunkte am Pufferrand gibt.

**Rotbeweis.** Quellspur, deren erster Sync unmittelbar hinter dem Index
liegt. Fehlt der Ausweichfall, landet der Schreibstart im Aufsetzbereich
des Kopfs; erwartetes Fehlerbild ist ein beschädigter erster Sektor.

## P7 — Deckt der Verify auch das ab, was schiefging?

**Anforderung.** Der Rückvergleich darf nicht ausgerechnet die Spuren
auslassen, die im Rohverfahren übernommen wurden. Zu klären, ob UFT
einen Rückvergleich auf **Rohstromebene** führt, unabhängig von der
Dekodierbarkeit.

**Rotbeweis.** Rohkopierte Spur mit absichtlich fehlerhaftem
Schreibvorgang. Meldet der Durchlauf Erfolg, ist der Befund rot.

## P8 — Zensus zum Sync-Korpus

**Anforderung.** Kein Prüfauftrag im engeren Sinn, sondern Korpusarbeit
für `uft-variants`: die Sync-Werte sind an den benannten Titeln
**unabhängig nachzumessen** und in die Fixture-Abdeckung aufzunehmen.
Der Satz der Vorlage ist unvollständig und darf **nicht als Referenz
gelten**, sondern nur als Einstiegsliste für eigene Messung.

> **Deckt sich mit MF-740, unabhängig erhoben.** Dort war gemessen: von
> vier Werten ist einer belegt (`0x4489`), drei kommen außerhalb der
> Vorlage nirgends vor, und zwei werden von unabhängiger
> Preservation-Dokumentation widersprochen (Arkanoid → Copylock,
> Beyond the Ice Palace → Cyberblast-Spuren). Der Baum selbst führte
> `0xA245` unter zwei verschiedenen Namen.



## P9 — Eichung gegen das Originalprogramm unter Emulation

**Anforderung.** Zum Quellmaterial gehören zeitgenössische lauffähige
Binaries. Das Originalprogramm kann unter Emulation gegen **dieselben
Fixtures** laufen, die P2–P7 verwenden, und als Oracle für die
erwarteten Verdikte dienen.

Das ersetzt keine Messung an Hardware, aber es **verschiebt die
Beweislast**: wo UFT und Vorlage zum selben Fixture verschiedene
Verdikte liefern, ist mindestens eines von beiden erklärungsbedürftig.

**Nicht anwendbar auf P1 (B3) und auf B13** — Kalibrierung und
Beruhigungszeiten hängen an echter Laufwerksmechanik und lassen sich
unter Emulation nicht sinnvoll prüfen.

> **Einordnung in diesem Baum.** Ein Oracle muss nach `ORAK-1`
> registriert sein (`tests/differential/oracles.py`), sonst trägt sein
> Urteil kein T1b. Zu klären wäre außerdem, ob die Lizenz die
> *Ausführung* deckt — sie deckt sie: „free for private,
> non-commercial use". Ein Oracle wird **ausgeführt, nicht
> weitergegeben**; das ist nach MF-695 genau der Kanal „Oracle:
> Ausführung frei, Weitergabe nicht", derselbe wie bei `dtc`.
>
> **Das ist der stärkste Kanal, der für XCopy offensteht** — und er
> war bis zu dieser Fassung des Gutachtens nicht sichtbar.

---

## Erledigte Aufträge

### P2 — beantwortet MF-745: **die Fähigkeit ist da, die Tür nicht**

| Umsetzung | bitweise? | erreichbar? |
|---|---|---|
| `src/algorithms/advanced/uft_fuzzy_sync_v2.c` | **ja** — `for (size_t bit = …)` über den Bitstrom, `get_bits16()` mit Teilbyte-Fenster | **nein** |
| `src/analysis/uft_track_analysis.c` | **ja** | wird entfernt (MF-744, Lizenz) |

`uft_fuzzy_sync_v2.c` steht im Bauplan (`.pro:967`), sein einziger
Konsument ist `uft_god_mode_api.c` — und dessen Produktionszweig hat
**MF-444 entfernt**, weil er vier erfundene Konstanten über die
gemessenen Werte schrieb. Gemessen: 11 exportierte Funktionen, **eine**
von außen gerufen, und die aus einem Test.

**Folge:** nach dem Vollzug von MF-744 hat UFT **keine erreichbare
bitweise Sync-Suche**. Der Rotbeweis zu P2 wäre heute rot — nicht weil
die Fähigkeit fehlt, sondern weil beide Umsetzungen ohne Tür sind.

Das ist dieselbe Klasse wie der Kopierschutz-Katalog (P0-2) und die
DeepRead-Forensikmodule (MF-627): **Bestand, nicht Fähigkeit.**


### P3 — beantwortet MF-748: **die Klasse gibt es, die Tür nicht**

UFT führt `UFT_SECTOR_RECOVERED` — und `tests/test_sector_recovery_honesty.c`
prüft genau die Regel aus B11. Der Test dokumentiert sogar einen echten,
behobenen Fehler:

```c
if (new_crc == sector->data_crc || sector->status == UFT_SECTOR_CRC_ERROR)
```

Die zweite Hälfte hebt die erste auf — gerufen wird die Funktion nur für
Sektoren mit CRC-Fehler, die Bedingung feuerte also **immer**. Ein
weiterhin CRC-falscher Sektor galt als RECOVERED, und die Rohlesung
wurde durch den unverifizierten Mittelwert ersetzt. **Dieselbe
Fehlerklasse, die XCopys Autoren 1992 in v5.21 korrigiert haben.**

Aber: `src/recovery/uft_sector_recovery.c` exportiert **5** Funktionen,
**eine** wird von außen gerufen — aus dem Test. **Null
Produktionsaufrufer.** In der Produktion wird nie ein Sektor als
RECOVERED markiert, weil der markierende Code nie erreicht wird.

**Nebenbefund — zwei kollidierende `uft_sector_status_t`:**

| Symbol | `include/uft/uft_types.h` | `src/recovery/uft_sector_recovery.h` |
|---|---|---|
| `UFT_SECTOR_MISSING` | `1<<2` = **4** | **3** |
| `UFT_SECTOR_WEAK` | `1<<4` = **16** | **4** |
| `UFT_SECTOR_RECOVERED` | existiert nicht | **5** |

Gleicher Typname, Bitmaske gegen fortlaufend. Und **Tor 39
(`audit_format_id_drift`) ist dafür blind**: es sondiert
`UFT_SECTOR_DELETED`, und die Recovery-Fassung definiert dieses Symbol
gar nicht. Eine Sonde, die ein Symbol wählt, das nur *eine* der beiden
Definitionen trägt, kann die Kollision nicht sehen.

### P4 — beantwortet MF-748: **ROT**

`UFT_TRACK_READ_ERROR` ist definiert und wird **von niemandem im ganzen
Baum gesetzt** (gemessen: 0 Zuweisungen in `src/`).

| P4-Zustand | UFT |
|---|---|
| leere Spur | `UFT_TRACK_UNFORMATTED` — gesetzt von g64, g71, hfe, scp |
| unlesbare Spur | **kein Zustand in der Praxis** |
| gleichförmig mit Diskontinuitäten | **gar kein Zustand** |

Zwei der drei fallen zusammen. Der Flussdekoder entscheidet an zwei
Stellen `return (track->sector_count > 0) ? FLUX_OK : FLUX_ERR_NO_SYNC;`
— null Sektoren ergibt `NO_SYNC`, ob die Spur leer oder verrauscht ist.

### P5 — beantwortet MF-748: **GRÜN, und der erste mit einer Tür**

`OTDR_EVT_PROT_LONG_TRACK` in `src/analysis/otdr/floppy_otdr.c:626`,
Berichtstext „Long Track (CP)", abgebildet in
`src/analysis/otdr/uft_otdr_bridge.c:300`. Die Brücke hat **fünf**
Aufrufer, darunter `src/gui/uft_otdr_panel.cpp` und
`src/formats/uft_format_convert_flux.c`.

Die Nennlänge steht in den Plattform-Profilen
(`track_length_nominal = 12500`). Der erste der fünf Prüfaufträge, bei
dem die Fähigkeit **und** der Weg zum Benutzer existieren.

### P7 — beantwortet MF-748: **GRÜN mit Einschränkung**

`src/fluxwritejob.cpp` führt einen Rückvergleich, und zwar auf
**Rohfluss-Ebene**: `do_write_raw_flux` macht einen Lesedurchgang, das
Ergebnis ist ein eigenes Verdikt `WriteVerifyFailed`. Das ist genau die
Lücke, an der die Vorlage durchfällt (B12 überspringt rohkopierte
Spuren) — UFT prüft dort, wo die Vorlage aufhört.

**Einschränkung:** `m_verify(false)` — standardmäßig **aus**, dem
V1-Verhalten nachgebildet. Die Fähigkeit ist erreichbar, aber sie läuft
nur, wenn jemand sie einschaltet. Ob die Oberfläche das anbietet, ist
Teil der Klick-Sitzung.

### Zwischenstand

| | Fähigkeit | erreichbar | Verdikt |
|---|---|---|---|
| **P2** bitweise Sync-Suche | ja, zweimal | **nein** | rot |
| **P3** „gerettet" als Klasse | ja, mit Test | **nein** | rot |
| **P4** syncfrei ≠ leer ≠ unlesbar | teilweise | — | **rot** |
| **P5** überlange Spur | ja | **ja, bis zur GUI** | grün |
| **P7** Verify auf Rohstromebene | ja | ja, aber aus per Vorgabe | grün* |

**Drei von fünf sind „Bestand, nicht Fähigkeit".** Das ist dieselbe
Klasse wie der Kopierschutz-Katalog (P0-2), die DeepRead-Forensikmodule
(MF-627) und die XCopy-Dateien selbst — und sie tritt hier in einem
Bereich auf, den ein Prüfauftrag von außen adressiert hat, nicht ein
Zensus von innen.

Offen bleiben **P1** (braucht Hardware, MF-310), **P6**
(Schreibstartpunkt), **P8** (Korpus) und **P9** (Oracle).


---

## Nachtrag: die Fehlercodes von X-Copy 2.x (MF-751)

X-Copy schrieb je Spur eine **grüne Null** bei Erfolg und eine **rote
Ziffer** bei einem Fehler. Der Eigentümer hat die Bedeutung der Ziffern
beigebracht (Fassung **2.x**, öffentlich überliefert):

| # | Bedeutung |
|---|---|
| 1 | Less or more than 11 sectors |
| 2 | No sync found |
| 3 | No sync after gap found |
| 4 | Header checksum error |
| 5 | Error in header / format long |
| 6 | Data block checksum error |
| 7 | **Long track** |
| 8 | **Verify error** |

**Das ist keine Ableitung aus dem Quelltext**, sondern überliefertes
Wissen über das Verhalten des Werkzeugs — zitierfähig ohne
Kontaminationsfrage, und aus einer **anderen Fassung** (2.x) als das
Gutachten (5.3).

### Zwei Abweichungen gegenüber B9, beide bedeutsam

**1 · „Less or MORE than 11 sectors".** B9 nennt „zu wenige Sektoren".
Die überlieferte Liste sagt: **zu viele ist ebenfalls ein Fehler.** Das
ist kein Detail — eine Spur mit mehr als der Regelzahl ist ein
klassisches Schutzmerkmal, und ein Werkzeug, das nur nach unten prüft,
sieht es nicht. Für **P4** heißt das: die Sektorzahl ist eine
zweiseitige Schranke, keine Mindestanforderung.

**2 · „Long track" ist ein eigener nummerierter Code.** B7 führt die
überlange Spur als *Sonderfall*, der die vier Verdikte überlagert. Die
2.x-Liste zeigt sie als **gleichrangigen Fehlercode**. Das stützt
**P5** von einer zweiten, unabhängigen Seite: die überlange Spur war
schon 1989 ein benannter Zustand und keine Fehlmessung.

> **P5 ist in UFT grün** (`OTDR_EVT_PROT_LONG_TRACK`, bis zur GUI). Die
> überlieferte Liste bestätigt, dass die Anforderung nicht konstruiert
> ist.

### Was das für P9 bedeutet

Die acht Codes sind ein **fertiger Vergleichsmaßstab** für die
Emulations-Eichung: Vorlage und UFT laufen gegen dasselbe Fixture, und
die Vorlage nennt ihr Urteil als Ziffer. Wo UFT keinen entsprechenden
Zustand führt — nach heutigem Stand mindestens Code 1 (zu *viele*
Sektoren), 3 („kein Sync nach Gap") und der ganze P4-Komplex —, ist die
Differenz benannt statt vermutet.

**Zu prüfen bleibt**, ob die Codes zwischen 2.x und 5.3 gleich
nummeriert sind. Die Liste stammt aus 2.x, das Gutachten aus 5.3, und
B9 zählt sechs plus eine getrennte Verify-Kennung — die 2.x-Liste zählt
acht. Ob dazwischen umnummeriert wurde, sagt keine der beiden Quellen.
