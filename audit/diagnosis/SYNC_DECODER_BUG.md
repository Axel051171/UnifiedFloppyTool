# MFM-Sync-Decoder-Bug — Diagnose

**Subsystem:** `src/flux/uft_flux_decoder.c` — IBM System-34 MFM Sektor-Decoder
**Symptom:** UFT dekodiert **0 Sektoren** aus jedem regulären IBM-MFM-Flux-Stream
**Status:** diagnostiziert; Fix als **MF-218** (`a597f92`) appliziert — dieses
Dokument ist der retrospektive Diagnose-Record (read-only gegen `src/`,
geschrieben wird nur `audit/diagnosis/`).

Aufgedeckt wurde der Bug durch den P3.2-Differential-Harness
(`tests/differential/`) beim ersten Lauf gegen echten Greaseweazle-Flux —
nicht durch Code-Review.

---

## Reproducer

**Minimaler Repro-Flux-Stream (synthetisch via gw):**

```
tests/differential/corpus/flux/ibm_dd.scp
```

Erzeugt deterministisch durch `tests/differential/gen_corpus.py`:

```
gw convert tests/differential/corpus/sources/ibm_dd.img \
           tests/differential/corpus/flux/ibm_dd.scp --format=ibm.720
```

`ibm_dd.img` = 737280 B (80 Zyl × 2 Köpfe × 9 Sektoren × 512 B), gefüllt mit
dem deterministischen `synth_pattern`.

**UFT-Aufruf der den Bug zeigt** (Test-Fixture, das die UFT-Flux-Engine
direkt linkt — UFT selbst ist GUI-only):

```
tests/differential/uft_flux_decode.exe \
    tests/differential/corpus/flux/ibm_dd.scp mfm out.img 2 9 512 1 2000
```

**Erwartete Ausgabe (gw):**

```
$ gw convert ibm_dd.scp out.gw.img --format=ibm.720
Found 1440 sectors of 1440 (100%)
$ wc -c out.gw.img
737280 out.gw.img
```

**Tatsächliche Ausgabe (UFT, vor MF-218):**

```
uft_flux_decode: decoded 0 sectors from 'tests/differential/corpus/flux/ibm_dd.scp'
exit code: 2   (EXIT_SCP_ERROR — kein Output-File geschrieben)
```

`flux_decode_mfm()` liefert `FLUX_ERR_NO_SYNC`, weil `track->sector_count == 0`
für **jede** Spur bleibt.

**Hex-Dump-Vergleich der ersten 256 Bytes des decoded Output:**

```
gw  out.gw.img  [0x0000]: 11 30 4f 6e 8d ac cb ea 09 28 47 66 85 a4 c3 e2
                          ... (synth_pattern, 256 B vorhanden)
uft out.img     [0x0000]: <kein File — 0 Sektoren, fwrite nie erreicht>
```

Die Divergenz ist nicht „andere Bytes" sondern „gar keine Bytes": der Decoder
findet keinen einzigen Sektor.

---

## Lokalisation

**Funktion die den Sync sucht:**
`flux_find_sync()` — `src/flux/uft_flux_decoder.c:249`
Schiebt ein 16-Bit-Fenster bitweise durch den Bitstream und liefert die
Bit-Position des ersten Vorkommens von `MFM_SYNC_PATTERN` (0x4489,
`include/uft/flux/uft_flux_decoder.h:41`).

**Funktion die den Sync *konsumiert* (Bug-Stelle):**
`decode_mfm_sector()` — `src/flux/uft_flux_decoder.c:297`
Die fehlerhafte Zeile war das ursprüngliche `pos += 16` direkt nach dem
gefundenen Sync (heute ersetzt durch `mfm_skip_sync_run()`, `:303` und `:349`).

**Aufruf-Kette von readTrack bis zur Sync-Scan-Funktion:**

```
uft_flux_decode  main()                       tests/differential/uft_flux_decode.c
  └─ flux_decode_mfm()                         src/flux/uft_flux_decoder.c:408
       └─ flux_to_bitstream()                  (Flux-Intervalle → MFM-Zell-Bitstream)
       └─ while-Loop:                          :450
            ├─ flux_find_sync(bits, bit_count,
            │                  MFM_SYNC_PATTERN, pos)   :452
            └─ decode_mfm_sector(bits, bit_count,
                                 sync_pos, sector, &end) :460
                 └─ [BUG] pos += 16  → Adressmarken-Read-Loop  :~307
```

(In der UFT-GUI ist die analoge Kette
`GreaseweazleProviderV2::do_read_raw_flux()` → C-API → derselbe
`flux_decode_*`-Kern; der Test-Fixture-Pfad ist identisch ab `flux_decode_mfm`.)

**Aktive PLL-/Bitstream-Parameter beim Scan** (`flux_decode_mfm`, :419-438):

| Parameter        | Wert beim ibm_dd-Repro                          |
|------------------|--------------------------------------------------|
| `bitcell_ns`     | 2000 (`FLUX_MFM_DD_BITCELL_NS`, DD)             |
| `use_pll`        | `true`                                           |
| `pll.freq_gain`  | `FLUX_PLL_GAIN`                                  |
| `sample_rate`    | 1 GHz (SCP-Helper: 1 Tick = 1 ns)               |
| Bitstream-Layout | rohe MFM-Zellen, Datenbits an geraden Zell-Pos.  |

---

## Hypothese

**H1 — PLL verliert/erreicht keinen Lock → Bitstream-Müll.**
Begründung: ein 23 %-falscher Bitcell-Startwert oder zu hoher `freq_gain`
könnte den PLL aus dem Tritt bringen; dann erscheint `0x4489` nie sauber
16-bit-aligned im Bitstream und `flux_find_sync` liefert durchgehend −1.
*Diagnose-Probe:* in `flux_decode_mfm` vor der Loop instrumentieren —
zählen wie oft `flux_find_sync` ≠ −1 liefert, und die Bit-Position des
ersten Treffers ausgeben.

**H2 — Sync wird gefunden, aber die Adressmarke wird am falschen Offset
gelesen.**
Begründung: IBM System-34 MFM schreibt **drei** `0xA1`-Sync-Bytes (jedes das
Missing-Clock-Wort `0x4489`) vor *jede* Adressmarke. `flux_find_sync` liefert
die Position des **ersten** `0x4489`. `pos += 16` überspringt genau **ein**
16-Bit-Wort → `pos` landet auf dem **zweiten** `0xA1`, nicht auf der IDAM
(`0xFE`). `flux_mfm_decode_byte()` des zweiten `0xA1` ergibt `0xA1`, der Check
`id_field[0] != MFM_IDAM` schlägt fehl → `FLUX_ERR_NO_SYNC`.
*Diagnose-Probe:* direkt nach `pos += 16` das nächste 16-Bit-MFM-Wort und
sein `flux_mfm_decode_byte`-Ergebnis ausgeben.

**Stehengeblieben: H2.** H1 wurde widerlegt (siehe Beweis): `flux_find_sync`
liefert pro Spur Dutzende valide Treffer — der PLL lockt sauber. Der Fehler
sitzt im *Konsum* des Syncs, nicht in seiner *Suche*. H2 erklärt zusätzlich,
warum der Bug für **alle** IBM-MFM-Geometrien gleichzeitig auftritt
(IBM-DD/HD, AtariST): der A1×3-Vorlauf ist System-34-Standard, geometrie­
unabhängig.

---

## Beweis

**Probe für H1 (Sync-Trefferzählung), Output (ibm_dd, eine Spur):**

```
[probe] track 0/0: flux_find_sync hits = 34   first 0x4489 @ bit 1216
[probe] track 0/0: decode_mfm_sector OK = 0   (alle 34 -> FLUX_ERR_NO_SYNC)
```

→ H1 widerlegt: 34 Sync-Treffer, PLL lockt. Trotzdem 0 dekodierte Sektoren.

**Probe für H2 (Byte hinter `pos += 16`), Output:**

```
[probe] sync_pos=1216  word@(sync+16)=0x4489  decoded byte=0xA1  (erwartet IDAM 0xFE)
[probe] sync_pos=1216  word@(sync+32)=0x4489  decoded byte=0xA1
[probe] sync_pos=1216  word@(sync+48)=0x5554  decoded byte=0xFE  <-- DAS ist die IDAM
```

→ H2 bestätigt.

**Konkrete Stelle wo die Sync-Detektion failt:**
`decode_mfm_sector()`, Adressmarken-Read-Loop. Nach `pos += 16` zeigt `pos`
**ins A1×3-Run hinein** (auf das 2. von 3 `0xA1`). Der erste gelesene
„Adressmarken"-Byte ist `0xA1` statt `0xFE`; `id_field[0] != MFM_IDAM` →
sofortiges `return FLUX_ERR_NO_SYNC`. Die IDAM säße erst bei `sync_pos + 48`
(nach allen drei `0xA1`). Ergebnis: `sector_count` bleibt 0, `flux_decode_mfm`
liefert `FLUX_ERR_NO_SYNC`, der Helper meldet „decoded 0 sectors".

---

## Empfehlung

**Welche Funktion / welche Zeilen:**

1. Neue Helper-Funktion `mfm_skip_sync_run(bits, bit_count, first_sync_pos)` —
   überspringt **alle** aufeinanderfolgenden `0x4489`-Worte und liefert die
   Position des ersten Nicht-Sync-Worts (die Adressmarke). Toleriert 1, 2
   oder 3+ Sync-Worte → korrekt für IBM (A1×3) **und** Amiga (A1×2).
2. In `decode_mfm_sector()` das `pos += 16` an **beiden** Sync-Punkten
   ersetzen — ID-Feld-Sync **und** Daten-Feld-Sync — durch
   `pos = mfm_skip_sync_run(...)`.
3. In `flux_decode_mfm()` die Loop-Resume-Position so setzen, dass nach einem
   erfolgreich dekodierten Sektor **hinter** dessen Datenfeld weitergesucht
   wird (`sector_end`), sonst wird derselbe Sektor mehrfach dekodiert.

**Tests die VOR dem Fix existieren müssen (regression-first):**
`tests/test_flux_mfm_sync.c` — synthetisiert eine IBM-MFM-Spur
(`Gap + A1×3 + IDAM + C/H/S/N + CRC + ... + A1×3 + DAM + data + CRC`) und
assertet, dass **genau** die erwartete Sektorzahl byte-exakt dekodiert wird.
Dieser Test schlägt mit dem alten `pos += 16` fehl (0 Sektoren) und ist nach
dem Fix grün — er ist der Regressions-Wächter.

**Aufwand & Risiko:**

| | |
|---|---|
| **Aufwand** | **S** — ~20 LOC (eine Helper-Funktion + 3 Aufrufstellen) |
| **Risiko**  | betrifft **alle** IBM-System-34-MFM-Formate gleichzeitig: IBM-DD, IBM-HD, Atari-ST-DD. Alle drei sind nach dem Fix byte-exakt gegen gw verifiziert (`tests/differential/` → `ibm_dd`, `ibm_hd`, `atarist_dd` ✅). `mfm_skip_sync_run` wird zusätzlich vom Amiga-Decoder (`flux_decode_amiga`, A1×2-Vorlauf) genutzt — ebenfalls per Differential byte-exakt verifiziert. Kein anderes Encoding (FM, GCR-C64, GCR-Apple) berührt diesen Pfad. |

**Umsetzungs-Status:** appliziert als **MF-218** (`a597f92`,
`fix(flux): IBM-MFM decoder skipped only 1 of 3 A1 sync words`). Die
Helper-Funktion `mfm_skip_sync_run()` steht heute in
`src/flux/uft_flux_decoder.c:282`, die Aufrufstellen bei `:303` / `:349`,
die Resume-Logik bei `:467-471`. Regressions-Test `tests/test_flux_mfm_sync.c`
ist grün; der gesamte P3.2-Differential (6/6 Formate) ist byte-exakt.
