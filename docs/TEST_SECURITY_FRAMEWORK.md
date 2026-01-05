# UFT Test & Security Framework

## Übersicht: Was haben wir?

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    TEST & SECURITY INFRASTRUCTURE                           │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   ✅ GOLDEN TESTS                                                           │
│   ───────────────                                                           │
│   • 60+ Golden Test Images definiert (D64, ADF, IMG, SCP, G64, HFE)        │
│   • Pro Format: Varianten, Edge Cases, Kopierschutz, Corrupt                │
│   • Hash-Vergleich + Sektor-Verifikation                                   │
│                                                                             │
│   ✅ FUZZ HARNESSES                                                         │
│   ─────────────────                                                         │
│   • AFL/libFuzzer-kompatibel                                               │
│   • 7 Fuzz-Targets: FORMAT_PROBE, D64, ADF, SCP, G64, HFE, PLL            │
│   • Persistent Mode für Performance                                        │
│                                                                             │
│   ✅ SECURITY TESTS                                                         │
│   ─────────────────                                                         │
│   • 7 Crash-Klassen abgedeckt                                              │
│   • Konkrete Test-Vektoren                                                 │
│   • Safe Integer Operations                                                │
│                                                                             │
│   ✅ REGRESSION WORKFLOW                                                    │
│   ─────────────────────                                                     │
│   • Bug → Test → Fix Pipeline                                              │
│   • Automatische Test-Generierung                                          │
│   • Crash-Input-Minimierung (afl-tmin)                                     │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 1. Golden Tests: 60+ Test-Images

### Warum Golden Tests?

Golden Tests verhindern Regressionen: Wenn ein Format einmal korrekt gelesen wird, darf es nie wieder fehlschlagen.

### Kategorien pro Format

| Format | Standard | Varianten | Edge Cases | Protection | Corrupt | Total |
|--------|----------|-----------|------------|------------|---------|-------|
| D64    | 2        | 4         | 2          | 1          | 1       | **10** |
| ADF    | 2        | 5         | 1          | 1          | 1       | **10** |
| IMG    | 4        | 6         | 2          | 0          | 0       | **12** |
| SCP    | 2        | 3         | 3          | 1          | 1       | **10** |
| G64    | 2        | 3         | 2          | 1          | 0       | **8** |
| HFE    | 2        | 3         | 1          | 0          | 0       | **6** |
| **Total** | | | | | | **56+** |

### Wichtigste Golden Tests pro Format

#### D64 (Commodore 64)

| Test | Beschreibung | Größe | Prüfung |
|------|--------------|-------|---------|
| `d64_empty_35` | Leer, 35 Tracks | 174,848 | BAM, Directory |
| `d64_full_35` | Voll, max Files | 174,848 | Alle Sektoren belegt |
| `d64_with_errors` | Mit Error Map | 175,531 | Error-Bytes korrekt |
| `d64_40_tracks` | Extended 40 | 196,608 | Extra-Tracks lesbar |
| `d64_42_tracks` | Max Extended | 205,312 | Track 36-42 |
| `d64_copy_protected` | Schutz-Marker | 174,848 | Non-standard sectors |
| `d64_geos` | GEOS Format | 174,848 | GEOS-Strukturen |
| `d64_corrupted_bam` | Kaputte BAM | 174,848 | Error, kein Crash |

#### ADF (Amiga)

| Test | Beschreibung | Größe | Prüfung |
|------|--------------|-------|---------|
| `adf_ofs_dd` | OFS Filesystem | 901,120 | OFS Blocks |
| `adf_ffs_dd` | FFS Filesystem | 901,120 | FFS Blocks |
| `adf_intl_dd` | International | 901,120 | INTL Flag |
| `adf_dirc_dd` | Dir Cache | 901,120 | DIRC Flag |
| `adf_hd` | High Density | 1,802,240 | 22 Sektoren/Track |
| `adf_bootable` | Mit Bootblock | 901,120 | Boot Checksum |
| `adf_not_dos` | Custom Format | 901,120 | Kein "DOS" |

#### SCP (Flux)

| Test | Beschreibung | Prüfung |
|------|--------------|---------|
| `scp_single_rev` | 1 Revolution | Minimal valid |
| `scp_multi_rev` | 5 Revolutions | Multi-rev parsing |
| `scp_cbm_dd` | Von C64 Disk | GCR decode |
| `scp_amiga_dd` | Von Amiga | MFM decode |
| `scp_weak_bits` | Weak Regions | Weak detection |
| `scp_half_tracks` | Half-tracks | 0.5 track positions |

---

## 2. Fuzz Testing: AFL/libFuzzer

### Gefährlichste Fuzz-Eingänge

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    ATTACK SURFACE PRIORITY                                  │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   🔴 KRITISCH (sofort absichern)                                           │
│   ────────────────────────────────                                          │
│   • Size fields in headers (track_size, sector_count, data_size)          │
│   • Offset tables (track_offsets[], sector_offsets[])                     │
│   • Count × Size multiplications                                           │
│                                                                             │
│   🟠 HOCH (wichtig)                                                         │
│   ─────────────────                                                         │
│   • Magic bytes (partial, wrong, truncated)                               │
│   • Version fields (unknown versions)                                      │
│   • Nested structures (recursive references)                              │
│                                                                             │
│   🟡 MITTEL                                                                 │
│   ────────────                                                              │
│   • String fields (unterminated, oversized)                               │
│   • Checksum fields (wrong values)                                        │
│   • Reserved/padding bytes                                                 │
│                                                                             │
│   🟢 NIEDRIG                                                                │
│   ───────────                                                               │
│   • Valid but unusual values                                               │
│   • Encoding edge cases                                                    │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Fuzz Targets

```bash
# Kompilieren mit libFuzzer
clang -g -O1 -fno-omit-frame-pointer \
      -fsanitize=fuzzer,address,undefined \
      -DFUZZ_TARGET_D64 \
      tests/fuzz/fuzz_harness.c \
      src/formats/*.c \
      -o fuzz_d64

# Kompilieren mit AFL
afl-clang-fast -g -O2 \
      -DFUZZ_TARGET_D64 \
      tests/fuzz/fuzz_harness.c \
      src/formats/*.c \
      -o fuzz_d64_afl

# Fuzzing starten
mkdir -p corpus/d64 crashes/d64
./fuzz_d64 corpus/d64 -max_len=200000

# Mit AFL
afl-fuzz -i corpus/d64 -o crashes/d64 -- ./fuzz_d64_afl @@
```

### Seed Corpus

```
corpus/
├── d64/
│   ├── empty_35.d64          # Minimal valid
│   ├── minimal_header.bin    # Just header
│   └── full_35.d64           # Full file
├── scp/
│   ├── minimal.scp           # Smallest valid
│   ├── one_track.scp         # Single track
│   └── multi_rev.scp         # Multiple revolutions
└── ...
```

---

## 3. Crash-Klassen & Mitigations

### Übersicht aller Crash-Klassen

| Klasse | Severity | ASan/UBSan Symptom | Ursache | Mitigation |
|--------|----------|-------------------|---------|------------|
| **OOB-R** | HIGH | heap-buffer-overflow (read) | `offset > file_size` | Bounds check |
| **OOB-W** | CRITICAL | heap-buffer-overflow (write) | `write > allocated` | Size validation |
| **INT-OVF** | CRITICAL | signed-integer-overflow | `a * b > MAX` | `safe_mul()` |
| **NULL** | MEDIUM | SIGSEGV at 0x0 | Unchecked malloc | Always check |
| **UAF** | CRITICAL | heap-use-after-free | Dangling pointer | NULL after free |
| **DBL-FREE** | HIGH | double-free | Ownership unclear | Clear ownership |
| **DIV-ZERO** | LOW | division by zero | Divisor = 0 | Validate divisor |

### Safe Integer Operations

```c
// IMMER diese Funktionen nutzen bei Size-Berechnungen!

static inline bool safe_mul_size(size_t a, size_t b, size_t* result) {
    if (a == 0 || b == 0) { *result = 0; return true; }
    if (a > SIZE_MAX / b) return false;  // Overflow!
    *result = a * b;
    return true;
}

static inline bool safe_add_size(size_t a, size_t b, size_t* result) {
    if (a > SIZE_MAX - b) return false;  // Overflow!
    *result = a + b;
    return true;
}

// Beispiel: Track-Buffer allozieren
size_t total_size;
if (!safe_mul_size(track_count, max_track_size, &total_size)) {
    return UFT_ERROR_OVERFLOW;  // Nicht crashen!
}
uint8_t* buffer = malloc(total_size);
if (!buffer) return UFT_ERROR_NO_MEMORY;
```

### Validation Patterns

```c
// IMMER validieren vor Zugriff!

// 1. Offset-Validierung
if (offset + size > file_size) {
    return UFT_ERROR_CORRUPT_DATA;
}

// 2. Index-Validierung  
if (track_num >= track_count || sector_num >= sectors_per_track) {
    return UFT_ERROR_INVALID_ARG;
}

// 3. Divisor-Validierung
if (sectors_per_track == 0) {
    return UFT_ERROR_INVALID_GEOMETRY;
}
int track = lba / sectors_per_track;

// 4. Pointer-Validierung
if (!image || !image->data) {
    return UFT_ERROR_NULL_POINTER;
}
```

---

## 4. Bug → Test → Fix Workflow

### Schritt-für-Schritt

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ STEP 1: Bug gefunden (Fuzzer/User/CI)                                       │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   $ ./fuzz_d64 corpus/d64                                                  │
│   ==12345==ERROR: AddressSanitizer: heap-buffer-overflow                   │
│   READ of size 4 at 0x...                                                  │
│       #0 d64_read_track src/formats/d64.c:142                             │
│                                                                             │
│   Crash input saved to: crash-abc123                                       │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ STEP 2: Crash minimieren                                                    │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   $ afl-tmin -i crash-abc123 -o tests/regression/issue_042.bin \           │
│              -- ./fuzz_d64 @@                                              │
│                                                                             │
│   Minimized: 174848 → 256 bytes                                            │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ STEP 3: Regression-Test erstellen                                           │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   // tests/regression/test_issue_042.c                                     │
│   int main(void) {                                                         │
│       uint8_t* data = load_file("tests/regression/issue_042.bin");        │
│       uft_image_t* img = NULL;                                             │
│       uft_error_t err = uft_d64_load(data, size, &img);                   │
│                                                                             │
│       // Sollte NICHT crashen, sondern Error zurückgeben                   │
│       assert(err == UFT_ERROR_CORRUPT_DATA);                              │
│       return 0;                                                            │
│   }                                                                        │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ STEP 4: Bug fixen                                                           │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   // src/formats/d64.c:142                                                 │
│   - memcpy(buf, &data[offset], 256);                                       │
│   + if (offset + 256 > file_size) return UFT_ERROR_CORRUPT_DATA;          │
│   + memcpy(buf, &data[offset], 256);                                       │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ STEP 5: Test läuft, Commit                                                  │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   $ make test                                                              │
│   test_issue_042: PASS                                                     │
│                                                                             │
│   $ git add tests/regression/issue_042.bin                                 │
│   $ git add tests/regression/test_issue_042.c                              │
│   $ git commit -m "Fix heap overflow in D64 parser (issue #42)"           │
│                                                                             │
│   → Test bleibt FOREVER in CI                                              │
│   → Bug kann nie wieder auftreten                                          │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 5. Parser-Sicherheit messen

### Metriken

| Metrik | Ziel | Tool | Aktuell |
|--------|------|------|---------|
| **Line Coverage** | >80% | gcov/llvm-cov | TBD |
| **Branch Coverage** | >70% | gcov/llvm-cov | TBD |
| **ASan Clean** | 0 errors | ASan | ✅ |
| **UBSan Clean** | 0 errors | UBSan | ✅ |
| **Fuzz Hours** | >100h | AFL/libFuzzer | TBD |
| **Regression Tests** | 100% pass | CI | ✅ |

### CI/CD Integration

```yaml
# .github/workflows/security.yml
security-tests:
  runs-on: ubuntu-latest
  steps:
    - uses: actions/checkout@v4
    
    # Build with Sanitizers
    - name: Build with ASan+UBSan
      run: |
        mkdir build-asan && cd build-asan
        cmake .. -DCMAKE_BUILD_TYPE=Debug \
                 -DUFT_ENABLE_ASAN=ON \
                 -DUFT_ENABLE_UBSAN=ON
        make -j$(nproc)
    
    # Run tests under sanitizers
    - name: Run tests with ASan
      run: cd build-asan && ctest --output-on-failure
    
    # Run regression tests
    - name: Regression tests
      run: ./build-asan/test_regression
    
    # Coverage
    - name: Generate coverage
      run: |
        mkdir build-cov && cd build-cov
        cmake .. -DCMAKE_BUILD_TYPE=Debug \
                 -DUFT_ENABLE_COVERAGE=ON
        make -j$(nproc)
        ctest
        llvm-cov report ./uft_test
```

### Fuzz-Kampagnen

```bash
# Langzeit-Fuzzing Setup
screen -S fuzz_d64
./fuzz_d64 -jobs=4 -workers=4 corpus/d64 \
           -max_len=200000 \
           -timeout=30 \
           -rss_limit_mb=2048

# Crash-Triage
ls crashes/d64/
for crash in crashes/d64/*; do
    echo "=== $crash ==="
    ./fuzz_d64 $crash 2>&1 | head -20
done
```

---

## Zusammenfassung: Was fehlt noch?

| Komponente | Status | Priorität |
|------------|--------|-----------|
| Golden Test Catalog | ✅ Definiert (60+) | - |
| Golden Test Files | ⚠️ Müssen erstellt werden | HIGH |
| Fuzz Harnesses | ✅ Implementiert | - |
| Seed Corpus | ⚠️ Muss erstellt werden | HIGH |
| Security Tests | ✅ Test-Vektoren definiert | - |
| Regression Workflow | ✅ Implementiert | - |
| CI Integration | ⚠️ Sanitizer-Jobs erweitern | MEDIUM |
| Coverage Tracking | ⚠️ Einrichten | MEDIUM |
| Fuzz Kampagnen | ⚠️ Starten | HIGH |
