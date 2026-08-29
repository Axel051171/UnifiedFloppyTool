# Einstellbare Fläche — was sichtbar wird, und wann

**Stand:** 2026-08-29 (MF-670) · **Anker:** `SETTINGS_ROADMAP`

## Wozu dieses Dokument

Einstellbarkeit ist die Menge der **wirksamen** Regler, nicht der
sichtbaren. Das ist keine Wortklauberei — es ist der Unterschied, an dem
dieser Baum 38 Bedienelemente lang vorbeigelaufen ist.

Bis MF-668 boten drei „Advanced…"-Dialoge 38 Regler an, deren
vollständiger Weg lautete: Dialog → Mitglied → Dialog. Der Benutzer
stellte etwas ein, es wurde gemerkt, es wurde ihm beim nächsten Öffnen
wieder gezeigt, und es wirkte sich auf nichts aus. Das ist das
Oberflächen-Gegenstück zum Test, der nicht scheitern kann.

Dieses Dokument ist der Ersatz für jene Dialoge — und zwar der bessere,
weil es versionierbar, ankerfähig und ehrlich ist. Tote Regler in einer
Oberfläche sehen aus wie Zusagen. Eine Zeile in einer Roadmap sieht aus
wie das, was sie ist.

## Die Regel

> **Ein Regler erscheint in der Oberfläche erst, wenn sein Träger eine
> Lesestelle hat.** Mechanismus zuerst, Regler zuletzt — nie umgekehrt.

Durchgesetzt von `scripts/audit_setting_wiring.py` (40. Kategorie in
`check_consistency.py`, seit MF-669): ein Feld, das gesetzt und nie
gelesen wird, färbt das Tor rot. Der Rotbeweis dafür ist geführt.

Daraus folgt die Reihenfolge in diesem Dokument: **(a) zuerst**, weil es
nichts kostet; **(b) danach**, weil es einen Mechanismus braucht; **(c)
gar nicht**.

## Die Anwendbarkeits-Matrix

Ein Regler kann drei Zustände haben, und nur der dritte ist verboten:

| | |
|---|---|
| **wirkt** | Quelle und Wert passen zusammen. |
| **gilt hier nicht, und sagt warum** | z. B. Zellendauer-Feineinsteller auf einem HFE: ein fertig getakteter Bitstrom hat keine Zellgrenzen mehr zu verschieben. |
| ~~schweigt~~ | **verboten.** Der Benutzer glaubt dann, seine Zahl habe gewirkt. |

Die Ebene, auf der eine Einstellung angreift, gehört zu ihrer
Deklaration:

| Ebene | was dort noch veränderlich ist |
|---|---|
| **Fluss** (SCP, KryoFlux, A2R) | Zeiten → Zellen. Alles ist offen. |
| **Bitstrom** (HFE, G64, WOZ) | Zellgrenzen stehen fest. Sync-, Roh- und Spurraster-Fragen bleiben. |
| **Sektor-Abbild** (D64, ADF, IMG) | nur noch Dateisystem- und Geometriefragen. |

Der zentrale Anwender ist `uftc_apply_decode_options()` in
`src/formats/uft_format_convert_flux.c` (MF-668). **Neue Quellzweige
rufen ihn auf, statt eine Parameterkopie zu tragen** — genau daran ist
MF-480 gescheitert: der Wert stand im SCP-Zweig und fehlte im HFE-Zweig,
anderthalb Jahre lang, ohne Wirkung und ohne Warnung.

---

## (a) Synonyme — 16 Regler, null neue Leitungen

Diese Namen meinen etwas, das UFT **schon kann**. Der Regler muss nur
auf den vorhandenen Träger zeigen. Das ist die billigste Einstellbarkeit,
die es gibt, und sie kommt zuerst.

Belege: Träger und Lesestelle je Zeile. Vier davon
(`pll_lock_threshold`, `weak_bit_cv`, `noflux_threshold`,
`detect_weak_bits`) habe ich nach dem Gutachten an den genannten Zeilen
selbst nachgeprüft.

| Regler | Träger | Lesestelle | Ebene |
|---|---|---|---|
| `bitcellPeriod`, `bitCell` | `flux_decoder_options_t.bitcell_ns` | `uft_flux_decoder.c` (2 Stellen) | Fluss |
| `pllBandwidth` | `.pll_gain` | `uft_flux_decoder.c:376` u. a. (6) | Fluss |
| ~~`pllLockThreshold`, `lockThreshold`~~ **angeschlossen MF-671** | `otdr_config.pll_lock_threshold` | `floppy_otdr.c:578` | Fluss |
| ~~`weakBitThreshold`~~ **angeschlossen MF-671** | `otdr_config.weak_bit_cv` | `floppy_otdr.c:705` | Fluss |
| ~~`weakBitDetection`~~ **war schon angeschlossen** | `otdr_config.detect_weak_bits` | `floppy_otdr.c:867`; GUI `uft_otdr_panel.cpp` | Fluss |
| ~~`noFluxThreshold`~~ **angeschlossen MF-671** | `otdr_config.noflux_threshold` | `floppy_otdr.c:514/533/594` | Fluss |
| `useIndex` | `flux_raw_data_t.index_times` | `uft_flux_decoder.c:263-287` | Fluss |
| `softIndex` | `synthetic_revolutions` + `revolution_ns` | `uft_flux_decoder.c:220-228/263ff` | Fluss |
| `gcrVariant` | `.encoding` (`FLUX_ENC_GCR_C64` / `_APPLE`) | Dispatch `uft_flux_decoder.c:1766` | Fluss |
| `rawNibble` | `.keep_raw_bits` | `uft_flux_decoder.c:734/999/1192/1546` | Fluss, Bitstrom |
| `preserveGaps` | `.keep_raw_bits` (Gaps stehen im Rohbitstrom) | `uft_flux_decoder.c:734` | Fluss, Bitstrom |
| `preserveSync` | `.keep_raw_bits` (Sync-Läufe ebenso) | `uft_flux_decoder.c:734` | Fluss, Bitstrom |
| `syncPattern` | `.sync_patterns` / `.sync_count` (MF-453) | `uft_flux_decoder.c` (2) | Fluss, Bitstrom |
| `ignoreBadGCR` | `gcr_decode(..., size_t *error_count)` | `uft_gcr_ops.c:178` | Bitstrom |
| `includeHalfTracks` | `has_half_tracks`, `G64_MAX_TRACKS 84` | `uft_g64.c:156-157` | Bitstrom |
| `includeQuarterTracks` | `quarter_tracks` (WOZ-TMAP) | `uft_woz.c:376-378`, `:609` | Bitstrom |
| `trackStep` | `geometry.double_step` | `uft_kryoflux_dtc.c:540` | **Erfassung**, nicht Dekodierung |
| `mergeRevs` | `use_multiple_revs` | `uft_format_convert_flux.c:964` | Fluss |
| **neu gefunden:** Umdrehungen im ERZEUGTEN Abbild | `synthetic_revolutions` | `convert_bitstream.c:64/280`, `dispatch.c:349`, `convert_flux.c:2335` — alle vier in `scp_writer_create()` | **Ziel**, nicht Quelle |
| **neu gefunden:** Strenge der Umdrehungs-Abstimmung | `multiread_config_t.{min_passes, majority_pct, min_confidence}` | `uft_multiread_pipeline.c` (10 Lesestellen) | Fluss |

### Die Einheiten-Falle

**Drei dieser Regler behaupten eine Einheit, die ihr Träger nicht
rechnet.** Wörtliche Übernahme wäre die nächste stille Falschaussage —
dieselbe Klasse wie FMT-2/3/10/11/12, nur in der Oberfläche:

| Regler behauptet | Träger rechnet in |
|---|---|
| `weakBitThreshold` in **%** | Variationskoeffizient |
| `noFluxThreshold` in **µs** | Vielfachem der Nennperiode |
| `bitcellPeriod` in **µs** | Nanosekunden (`bitcell_ns`) |

Wer einen dieser Regler anschließt, rechnet **an der einen Stelle um,
die den Träger setzt** — nicht im Dialog, damit die Einheit nicht zweimal
im Baum steht (so wie MF-668 es mit Prozent → Bruchteil gehalten hat).

### Zwei Einschränkungen, die zur Zeile gehören

* **`gcrVariant`:** „Standard-GCR" gibt es nicht, und „Victor" fehlt im
  Fluss-Dekoder — es existiert nur als Sektor-Plugin
  (`uft_victor9k.c:189`). Der Regler bietet also **zwei** Werte an, nicht
  vier. Ein Victor-GCR-Dekoder wäre neuer Decoder-Code und fällt unter
  die EINFRIER-REGEL.
* **`rawNibble`:** der Träger `keep_raw_bits` lebt, aber das benachbarte
  Enum `FLUX_ENC_RAW` ist **tot** — der Dispatch beantwortet es mit
  `FLUX_ERR_INVALID`. Wer den Regler anschließt, schließt ihn an
  `keep_raw_bits` an, nicht an das Enum.

---

## (b) Baubar — 0 Namen

Keiner der 34 Namen hat die Aufnahmebedingung erfüllt: **zwei
unabhängige Quellen** (der eigene Baum zählt nie mit), ein
Oracle-Kandidat, und eine der vier Release-Kennzahlen, die er bewegt.

Das ist ein Ergebnis, kein Versäumnis. Drei Namen kamen nahe heran und
stehen darum als **Fundus**, nicht als Auftrag:

| Name | woran es liegt |
|---|---|
| `filterType` (Simple/PID/Adaptive) | Reales Konzept, aber **eine** Quelle: das mit MF-626 entfernte `fdc_bitstream`-Vendoring (MIT, `git show 70d2e0e7^`). Im Baum lebt genau eine PLL. |
| `adaptiveGain` | Gain hoch im Sync-Feld, niedrig bei Daten — belegt, aber ebenfalls nur aus jener einen Quelle. |
| `historyDepth` | Nur in `vfo_pid3` desselben gelöschten Baums. Keine zweite Quelle. |

Nach Regel 9 (MF-640) bewegt keiner davon eine der vier Kennzahlen. Sie
werden notiert, nicht eingeplant. Wer sie aufgreift, braucht zuerst die
zweite Quelle.

**Lizenz-Hinweis:** nibtools ist GPL-3.0 (Lizenzdatei gelesen). In einem
GPL-2.0-Baum nicht portierbar — nutzbar allein als **Oracle**, also zum
Vergleichen von Ausgaben, nie als Codequelle. Lizenz vor Fähigkeit.

---

## (c) Gestrichen — 18 Namen

Mit Grund, damit niemand sie in zwei Jahren als vergessene Wunschliste
wiederentdeckt.

| Name | Grund |
|---|---|
| `pllFrequency` | Kehrwert-Dublette von `bitcell_ns`. Zwei Regler für eine Größe sind gebaute Zahlendrift. |
| `pllPhase` | Ein Anfangsphasen-Offset ist wirkungslos — die PLL rastet in wenigen Zellen ein (`uft_flux_decoder.c:363-373`). |
| `clockTolerance` | Beide Träger tot (`opts->tolerance`, `clock_tolerance_pct`: je 0 Lesestellen). Der lebende Weg **misst** die Zelle, statt sie zu tolerieren (`measured_cell_ns`, MF-492). |
| `indexOffset` | Index-Lagen sind Daten der Aufnahme. Ein Verschiebe-Regler erfindet Geometrie — Konflikt mit „Keine erfundenen Daten". |
| `clockRate` | Abtastrate steht in der Flussdatei, Datenrate ist 1/`bitcell_ns`. Ein Override widerspräche der Datei. |
| `filterType`, `historyDepth`, `adaptiveGain` | siehe (b) — Fundus, kein Regler. |
| `unlockThreshold` | Hysterese-Zweitschwelle ohne Konsumenten; die OTDR-Lock-Klassifikation ist zustandslos je Segment. |
| `weakBitWindow` | Die lebenden Weak-Bit-Wege arbeiten je Bitzelle über ≥2 Umdrehungen und brauchen kein Fenster. |
| `decodeToSectors` | Komplement von `rawNibble` und der Normalzweck. Ein Häkchen dafür sagt nichts. |
| `syncLength` | Sync-Lauflängen behandelt der Dekoder selbst und tolerant (`mfm_skip_sync_run`). |
| `autoDetectSync` | Existiert bereits als `FLUX_ENC_AUTO`. Freies Sync-Scannen wäre neuer Decoder-Mechanismus (EINFRIER). |
| `fillBadSectors`, `fillByte` | Als **globaler** Schalter Konflikt mit „Keine erfundenen Daten". Wo Füllen Format-Semantik ist, existiert es bereits lokal (`uft_hardsector.c:290/329`, `uft_imd_parser_v2.c:373`). |
| `revsToRead` | Doppelgänger eines Reglers, den es **schon gibt**: `ui->spinRevolutions` → `setRevolutions()`, `src/workflowtab.cpp:587` (MF-472). Und er sitzt auf der richtigen Seite — wie viele Umdrehungen *aufgenommen* werden, entscheidet die Erfassung, nicht die Dekodierung. |
| `revsToUse` | Kein Mechanismus. Das Zielfeld war `flux_decoder_options_t.revolution` (0 Lesestellen, mit MF-669 gelöscht). Die Verschmelzung nimmt **immer alle** Umdrehungen, die in der Datei stehen (`convert_flux.c:993`, `for r < td.revolution_count`) — es gibt nichts auszuwählen. |
| `mergeMode` (First/Best/All) | Zwei seiner drei Werte sind eine Umkodierung von `mergeRevs` — „All" ist `use_multiple_revs = true`, „First" ist `false`. „Best" hat keinen Mechanismus: eine Verschmelzungs-**Strategie** gibt es im Baum nicht (`git grep merge_mode\|fusion_mode\|vote_mode` → 0 Treffer). Ein Auswahlfeld, dessen dritter Wert nichts bedeutet, ist schlimmer als ein Häkchen. |

---

## Erledigt

### MF-671 — die vier OTDR-Schwellen, erster Stapel

Drei neue Regler im OTDR-Panel (`Lock:` in %, `Weak:` als
Variationskoeffizient, `No-Flux:` als Vielfaches), der vierte war
bereits da. Alle vier gehen jetzt durch **einen** Weg in den Kern.

Beim Anschließen fiel der **neunte** Fall der Aufzählungs-Falle auf:
`otdr_track_analyze()` wurde an vier Stellen aufgerufen, jede mit einer
handkopierten Konfigurations-Zuweisung davor — und **eine der vier war
schon unvollständig.** Sie setzte das Glättungsfenster, aber nicht die
Kodierung. Wer eine Kodierung wählte und *dann* eine Diskette lud, bekam
die erste Spur mit „Auto" ausgewertet, während der Kasten etwas anderes
zeigte.

Behoben nicht durch Nachzählen, sondern durch einen **Trichter**:
`analyzeWithCurrentConfig()` ist der einzige Weg von diesem Panel in die
Analyse. Nachzuzählen, ob jemand den Anwender vergessen hat, fällt erst
auf, *nachdem* es passiert ist; ein einziger Weg kann nicht vergessen
werden. `scripts/audit_setting_wiring.py` hält fest, dass es einer
bleibt (Rotbeweis geführt: zweiter Aufruf → rot).

Die Einheiten-Falle ist dabei ernst genommen worden — die Regler zeigen
die Einheit ihres **Trägers**, nicht die, die die alten Dialoge
behaupteten. `Weak:` ist ausdrücklich kein Prozentregler: 15 % und 0.15
sehen im Dialog gleich plausibel aus und bedeuten dasselbe nur zufällig.

### MF-672 — die vier Umdrehungs-Regler waren einer

Vor dem Verdrahten gemessen, und die Messung hat drei von vier
gestrichen. Die Roadmap-Zeile davor war **falsch**: sie führte
`synthetic_revolutions` als Träger für „wie viele Umdrehungen
verwenden". Alle vier Lesestellen dieses Feldes münden in
`scp_writer_create()` — es entscheidet, wie viele Umdrehungen in ein
**erzeugtes** Abbild geschrieben werden. Quelle und Ziel verwechselt.

Übrig bleibt **ein** echter Regler (`mergeRevs`), und die Messung fand
dabei **zwei Träger, die kein Dialog je benannt hat**:

* **`synthetic_revolutions`** — die Umdrehungszahl im erzeugten Abbild.
  Vier Lesestellen, kein Weg von außen. Wer eine SCP erzeugt, bekommt,
  was drei verschiedene Rückfallwerte im Quelltext für richtig halten.
* **`multiread_config_t`** — die Strenge der Umdrehungs-Abstimmung
  (`min_passes`, `majority_pct`, `min_confidence`). Lebendig und
  erreichbar, aber `uft_format_convert_flux.c:127` ruft
  `multiread_config_default()` **fest verdrahtet**. Das ist die Größe,
  die `mergeMode` eigentlich hätte meinen können — nicht
  First/Best/All, sondern wie stark eine Mehrheit sein muss.

**Nebenbefund, nicht behoben:** dieselbe Option hat in derselben Datei
zwei Rückfallwerte — `convert_bitstream.c:65` nimmt 1 Umdrehung
(HFE→SCP), `:281` nimmt 3 (G64→SCP). Ohne genannten Grund. Welcher der
beiden richtig ist, sagt keine Referenz im Baum; darum benannt und
stehen gelassen, statt eine Zahl zu raten.

## Noch ungeklärt

* **GUI-7:** der PLL-Toleranz-Regler im OTDR-Panel ist der achte Fall
  derselben Klasse — Regler → `QSettings("pllTolerance")` → Regler,
  sonst kein Leser. Sein Zielfeld ist seit MF-669 gelöscht. Entscheidung
  steht aus: entfernen, oder Oracle-first einen Mechanismus bauen.
* Welcher der beiden Rückfallwerte für `synthetic_revolutions` richtig
  ist (1 bei HFE→SCP, 3 bei G64→SCP). Braucht eine externe Referenz.

## Herkunft

Die Triage der 34 Namen stammt aus einem Aufklärungs-Zyklus des
`uft-scout` (`tools/uft-scout/out/settings_triage.gutachten.md`,
2026-08-29). Sie ist **nicht** ungeprüft übernommen: die vier
OTDR-Träger sind an den genannten Zeilen nachgemessen, ebenso die
Erreichbarkeit von `keep_raw_bits`. Das Gutachten korrigiert dabei eine
eigene Vorannahme — Kalman-PLL und `uft_multi_rev_fusion` sehen wie
Träger aus, hängen aber an einer zirkulären Kette über
`uft_smart_open`, das keinen Aufrufer hat.
