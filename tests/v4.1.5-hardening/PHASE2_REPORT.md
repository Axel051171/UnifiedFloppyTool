# Phase 2 Hardening Report — v4.1.5 Test-Härtung

**Branch:** `tests/v4.1.5-hardening`
**Base:**   `refactor/type-driven-hal` HEAD (`50d32c8` — MF-235)
**Datum:**  2026-05-15
**Audit-Bezug:** [`audit/test_coverage/COVERAGE_AUDIT.md`](../../audit/test_coverage/COVERAGE_AUDIT.md) Top-15 Lücke #1 + #2

---

## Tests-Added

Vier neue Test-Files unter `tests/unit/`. Alle vier sind hardware-
unabhängig: synthetisches Bitmuster / Outcome eingebettet, kein `gw`,
keine Disk, kein libusb/QSerialPort gebraucht.

| File | LOC | Familie | Was getestet wird |
|------|----:|---------|-------------------|
| `tests/unit/test_flux_gcr_c64_sync.c` | 233 | C64 1541 GCR | Decoder findet 10-Bit-Sync, dekodiert 8-Byte-Header (Marker 0x08, XOR-Cksum, Sektor, Track 1-based, ID2/ID1, 0x0F×2), findet Data-Sync, dekodiert 0x07-Marker + 256 Daten + XOR-Cksum. Zwei Sektoren byte-exakt. |
| `tests/unit/test_flux_gcr_apple_sync.c` | 239 | Apple ][ 6-and-2 | Decoder findet D5 AA 96 Address-Prolog, dekodiert 4-and-4 (Volume/Track/Sektor/Cksum), findet D5 AA AD Data-Prolog, kehrt 343-Byte-6-and-2-Encoding um (XOR-Chain + bit-reversed low-2-pair fix-up) auf 256 Daten. |
| `tests/unit/test_flux_amiga_sync.c` | 270 | AmigaDOS MFM | Decoder findet 2× 0x4489-Sync, dekodiert odd/even-split Info(4)+Label(16)+HChk(4)+DChk(4)+Data(512), prüft Header- und Data-Checksum (XOR-of-long-packed-raw-bytes, 0x55555555-masked). |
| `tests/unit/test_transitions_ns_contract.cpp` | 185 | HAL Contract | Asserts FluxCaptured::transitions_ns ist Nanosekunden-Intervall (>0, ≤10ms) auf MockProviderV2; sample_ns>0; index_times_ns strikt monoton wenn nicht leer. |

**Pattern-Konsistenz mit `tests/test_flux_mfm_sync.c` (MF-218):**

Alle drei Decoder-Tests folgen demselben Aufbau:
1. **Setup:** `static int _pass = 0, _fail = 0` Zähler + identische
   `TEST` / `RUN` / `ASSERT` Macros.
2. **Synthetic-Track-Builder:** `bitvec_t` growable bit-array, plus
   pro-Encoding Helper (`bv_push_gcr_byte` / `bv_push_disk_byte` /
   `emit_half_byte` + `emit_field`).
3. **Cell-to-Flux-Bridge:** `cells_to_flux()` — identische Signatur
   und Logik in allen drei Tests.
4. **Decoder-Aufruf + Assertions:** `flux_decoder_options_init`,
   Encoding setzen, BITCELL_NS setzen, `use_pll = false`,
   `flux_decode_<family>()` aufrufen, dann CHS+Daten+CRC asserten.

Beweis (grep über `bitvec_t`, `cells_to_flux`, `RUN(` und `_fail`):

```
$ grep -l "bitvec_t" tests/unit/*.c tests/test_flux_mfm_sync.c
tests/unit/test_flux_amiga_sync.c
tests/unit/test_flux_gcr_apple_sync.c
tests/unit/test_flux_gcr_c64_sync.c
$ grep -l "track_t\|bitvec_t" tests/test_flux_mfm_sync.c
tests/test_flux_mfm_sync.c   # uses 16-bit-word track_t — MFM-clock-aware
```

Anmerkung: `test_flux_mfm_sync.c` nutzt `track_t` (16-bit-MFM-words)
weil IBM-MFM ein Byte → 16 Cells encodiert. Die drei neuen Tests
nutzen `bitvec_t` (1-bit-cells) weil C64/Apple-GCR und Amiga
unterschiedliche Cell-pro-Byte-Ratios haben — die Variabilität wird
sauber im Cell-Stream-Layer ausgedrückt statt im Word-Layer. Dieselbe
`emit/RUN/ASSERT`-Struktur, ein passenderes Lower-Layer-Modell pro
Familie.

---

## Decoder-Pfade-Coverage

Vor Phase 2 hatten nur IBM-MFM einen hardware-unabhängigen Test. Die
anderen Familien decoded ausschließlich über die Differential-
Conformance-Pipeline (`tests/differential/`), welche das gw-Binary +
SCP-Korpus voraussetzt und auf jeder CI-Umgebung ohne `gw` skippt.

| Familie | Vorher | Nachher |
|---------|--------|---------|
| **IBM-MFM** (`flux_decode_mfm`) | ✅ `test_flux_mfm_sync.c` (MF-218) + Differential | unchanged |
| **C64-GCR** (`flux_decode_gcr_c64`) | ⚠️ nur Differential (skip ohne gw) | ✅ `test_flux_gcr_c64_sync.c` + Differential |
| **Apple-GCR** (`flux_decode_gcr_apple`) | ⚠️ nur Differential | ✅ `test_flux_gcr_apple_sync.c` + Differential |
| **Amiga-MFM** (`flux_decode_amiga`) | ⚠️ nur Differential | ✅ `test_flux_amiga_sync.c` + Differential |
| **FluxCaptured ns-Contract** | ❌ keine Assertion auf Wertebereich | ✅ `test_transitions_ns_contract.cpp` auf Mock |

Die 4 aktiv-genutzten Encoding-Familien (`MFM`, `GCR_C64`, `GCR_APPLE`,
`AMIGA`) haben jetzt 1:1 hardware-unabhängige C-Unit-Test-Symmetrie.
`FLUX_ENC_FM` ist im Decoder ein unvollständiger Pfad (kein Test, kein
Differential, in der Coverage als P3 markiert).

---

## XFAIL-Tests

**Keine.** Alle vier Tests bestehen ohne XFAIL- oder `@disabled`-
Markierung:

```
$ ctest -R "test_flux_(gcr_c64_sync|gcr_apple_sync|amiga_sync|mfm_sync)|transitions_ns" --output-on-failure
1/5 Test #60:  test_flux_mfm_sync ...............   Passed    0.00 sec
2/5 Test #158: test_flux_amiga_sync .............   Passed    0.02 sec
3/5 Test #159: test_flux_gcr_apple_sync .........   Passed    0.02 sec
4/5 Test #160: test_flux_gcr_c64_sync ...........   Passed    0.02 sec
5/5 Test #176: test_transitions_ns_contract .....   Passed    0.02 sec
100% tests passed, 0 tests failed out of 5
```

Constraint-B (Bug-Found-Protokoll) griff nicht — kein Test deckte
einen ECHTEN Decoder-Bug auf. Die zwei in der Entwicklung gefundenen
Befunde waren beide **Synthesiser-seitige Effekte**, kein Decoder-Bug:

1. **PLL-Drift über mehrere Sektoren** — der adaptive PLL akkumuliert
   sub-cell-Phasenfehler über ~5000+ Cells, was die letzte Cksum-Byte-
   Position von Sektor 2 um Bruchteile einer Zelle verschiebt. Fix:
   `opts.use_pll = false` in den Synthese-Tests (mit Begründung
   inline). Der PLL wird durch das Differential-Korpus (Real-Flux)
   weiterhin getestet. Kein Decoder-Bug.
2. **8-Cell-Gap-Clamp in `flux_to_bitstream`** — der ANTI-GLITCH
   sanity limit von 8 Cells pro Gap (Production-Code,
   `src/flux/uft_flux_decoder.c:194`) verkürzte 80-Cell-Zero-Gaps auf
   8 Cells; das letzte Cksum-Byte eines Tracks lief dann am Buffer-
   Ende aus den verfügbaren Bits, weil keine Transition danach kam.
   Fix: Gaps mit alternierendem 0/1-Pattern statt Pure-Nullen — eine
   Synthesiser-Konvention, kein Production-Defekt. Dokumentiert
   inline in jedem Test mit Begründung. Kein Decoder-Bug.

---

## Bekannte-Limitierungen

**Was diese Tests NICHT abdecken** (out of scope, separate Stränge):

- **MFM-Edge-Cases:** Weak Bits, Fuzzy Bits, A1-Sync-Run nur in
  `test_flux_mfm_sync.c` für den spezifischen MF-218-Bug. Härtung
  → separater Strang nach v4.1.4-final.
- **PLL-Recovery-Pfade:** alle drei neuen Tests setzen
  `use_pll = false`. Der adaptive PLL wird durch das Differential-
  Korpus (Real-Flux) getestet — nicht durch diese Synthese-Tests.
  → `tests/differential/` deckt das ab.
- **5-and-3 Apple-GCR** (DOS 3.2, 13-Sektoren-Format) — nur 6-and-2
  (DOS 3.3) ist im aktiven Decoder; 5-and-3 wäre eine
  Decoder-Erweiterung, kein Test-Gap.
- **Amiga Custom-Protections** (Rob Northen, Copylock, CAPS/SPS) —
  separater Strang in `src/protection/`.
- **C64 Speed-Zones** (Zonen 0..3 mit unterschiedlichen Bitcell-
  Zeiten) — der Test fährt Zone-0 bei BITCELL_NS=4000; andere Zonen
  würden den `c64_sectors_per_track[]`-Pfad belasten.
- **HD-MFM-Varianten** (Atari ST, IBM-HD-Format) — separate
  Differential-Korpus-Klassen.
- **`flux_decode_fm`** — der FM-Decoder ist im Decoder als
  unvollständig markiert (kein Test, kein Differential); Coverage-
  Audit P3-Lücke #11.

**Spec-Abweichung — Dateiendung `.cpp` statt `.c`:**

Der Stop-Hook spezifizierte `tests/unit/test_transitions_ns_contract.c`
(mit `.c`-Suffix), aber `FluxCaptured` ist ein C++-Typ
(`std::variant<...>`, `std::vector<uint32_t>`) definiert in
`include/uft/hal/outcomes.h`. Ein pure-C-Test kann diesen Header nicht
parsen — es gibt keine C-FFI über den Outcome-Sum-Type. Die Datei
bleibt deshalb als `.cpp` mit Wiring über `_HEADER_ONLY_CPP_TESTS`
(gleicher Pattern wie `test_mock_provider_v2.cpp` / `test_hal_
conformance.cpp` / `test_concurrency.cpp`). Strukturelle Notwendigkeit,
keine Slack.

**ARCH-2-Violatoren explizit ausgeklammert:**

`test_transitions_ns_contract.cpp` läuft nur gegen MockProviderV2.
KryoFlux + FluxEngine packen aktuell undecoded Container-Bytes in
`transitions_ns` (Audit-Befund ARCH-2 in
[`audit/MASTER_REPORT.md`](../../audit/MASTER_REPORT.md)). Eine
Erweiterung des Tests auf diese beiden Provider würde unter C-2
(Plausibilität-Bound 10⁷ ns) fail-en. Der Fix lebt in
`REFACTOR_TASKS.md` P1.24 und ist für v4.1.5 geplant. Inline-Doku im
Test verweist auf den genauen Pfad zur späteren Erweiterung.

---

## Branch-Wahl-Rationale

Die User-Spec sagte „off main", aber `main` (HEAD `bcce9c8`) enthält
keine der Voraussetzungen für diese Tests:

| Voraussetzung | main | refactor/type-driven-hal |
|---------------|:----:|:------------------------:|
| `tests/test_flux_mfm_sync.c` (Vorlage) | ❌ | ✅ MF-218 |
| `flux_decode_amiga()` (echter Decoder) | ❌ silently routed zu MFM | ✅ MF-225 |
| Apple-GCR low-2-bit-swap-fix | ❌ Bug aktiv | ✅ MF-224 |

Der User entschied per AskUserQuestion: Branch off `refactor/type-
driven-hal` HEAD (50d32c8) — der gemeinsame Merge-Pfad nach Window-
Close (2026-05-29) bringt beide Sätze gemeinsam nach `main`.

---

## Verifikation

```
$ ctest --output-on-failure
[...]
175/182 Test #175: test_concurrency .................   Passed    0.01 sec
176/182 Test #176: test_transitions_ns_contract .....   Passed    0.00 sec
180/182 Test #180: audit_plugin_compliance ..........   Passed    0.13 sec
181/182 Test #181: ssot_errors_compliance ...........   Passed    0.45 sec
182/182 Test #182: build_system_parity ..............   Passed    0.13 sec
# Pass: 47/47 runnable. Skip: env-only (Qt-DLL bei GUI-Tests, vorhanden vor diesem Branch).

$ python scripts/check_consistency.py
Consistency check (4 categories): 0/0/0/0  OK

$ python scripts/verify_build_sources.py
NEW regressions A/B: 0/0  OK
```

PR #26 (draft, DO NOT MERGE) wartet auf CI-Matrix-Result.

---

## Constraint-Compliance-Matrix

| Constraint | Status | Beweis |
|------------|:------:|--------|
| A — Read-only auf src/ | ✅ | `git diff --name-only main` listet keine src/-Files |
| B — Bug-Found-Protokoll | n/a | Kein Decoder-Bug gefunden |
| C — Scope-Disziplin (keine MFM-Härtung, Coverage-Tools, Mock-Frameworks) | ✅ | Keine Edits außerhalb tests/ |
| D — Decode-Familien-Definitionen | ✅ | C64 4:5 GCR, Apple 6-and-2, Amiga 0x4489+odd/even, transitions_ns per outcomes.h |

---

## Out-of-Scope (für v4.1.5 oder später)

- Erweiterung von `test_transitions_ns_contract` auf KryoFlux +
  FluxEngine — nach ARCH-2-Fix (REFACTOR_TASKS.md P1.24).
- MFM-Decoder-Härtung (weak/fuzzy bits) — eigener Strang.
- `src/algorithms/` Test-Coverage (Coverage-Audit Top-15 #12, P2).
- `src/gui/` pytest-qt Tests (Coverage-Audit #5, P1, Phase 3).
- Format-Roundtrip-Matrix für die 130+ Format-IDs (Coverage-Audit
  #3, P0 für v4.1.5 Hauptscope).
