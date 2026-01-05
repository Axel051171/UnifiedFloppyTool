# UFT Performance Targets

**Version:** 1.0  
**Status:** VERBINDLICH

---

## 1. Performance-Ziele

### 1.1 Core Operations

| Operation | Target | Acceptable | Maximum |
|-----------|--------|------------|---------|
| Format Detection | < 10ms | < 100ms | 500ms |
| Open D64 | < 5ms | < 20ms | 100ms |
| Open SCP | < 50ms | < 200ms | 1s |
| Read Track (D64) | < 1ms | < 5ms | 20ms |
| Read Track (SCP/Flux) | < 10ms | < 50ms | 200ms |
| Decode MFM (per track) | < 5ms | < 20ms | 100ms |
| Decode GCR (per track) | < 5ms | < 20ms | 100ms |

### 1.2 Full Disk Operations

| Operation | Target | Acceptable | Maximum |
|-----------|--------|------------|---------|
| Read D64 (full) | < 50ms | < 200ms | 1s |
| Read SCP (full, 80 tracks) | < 500ms | < 2s | 5s |
| Convert SCP→D64 | < 2s | < 5s | 15s |
| Convert D64→G64 | < 500ms | < 2s | 5s |

### 1.3 Throughput

| Operation | Target | Acceptable |
|-----------|--------|------------|
| MFM Decode | > 10 MB/s | > 5 MB/s |
| GCR Decode | > 10 MB/s | > 5 MB/s |
| Flux Read (from file) | > 50 MB/s | > 20 MB/s |
| Flux Accumulation | > 100M samples/s | > 50M samples/s |

### 1.4 GUI Responsiveness

| Requirement | Target |
|-------------|--------|
| Frame time (60 FPS) | < 16ms |
| UI blocking (max) | < 16ms |
| Progress update rate | ≥ 10 Hz |
| Cancel response time | < 100ms |

---

## 2. Benchmark-Suite

### 2.1 Micro-Benchmarks

```c
// tests/benchmark/bench_decode.c

void bench_mfm_decode(void) {
    // Setup: 80 tracks × 6250 bytes MFM data
    BENCH_START("mfm_decode");
    for (int i = 0; i < 80; i++) {
        uft_decode_mfm(mfm_data[i], MFM_TRACK_SIZE, output);
    }
    BENCH_END();  // Should be < 400ms for target
}

void bench_gcr_decode(void) {
    // Setup: 35 tracks × 7928 bytes GCR data
    BENCH_START("gcr_decode");
    for (int i = 0; i < 35; i++) {
        uft_decode_gcr(gcr_data[i], GCR_TRACK_SIZE, output);
    }
    BENCH_END();  // Should be < 175ms for target
}

void bench_flux_accumulate(void) {
    // Setup: 1M flux transitions
    BENCH_START("flux_accumulate");
    uint32_t total = 0;
    for (size_t i = 0; i < 1000000; i++) {
        total += flux[i];
    }
    BENCH_END();  // Should be < 10ms for target
}
```

### 2.2 Macro-Benchmarks

```c
// tests/benchmark/bench_workflow.c

void bench_full_d64_read(void) {
    BENCH_START("d64_full_read");
    uft_disk_t disk;
    uft_open(&disk, "test.d64", true);
    for (int c = 0; c < 35; c++) {
        for (int h = 0; h < 1; h++) {
            uft_track_t track;
            uft_read_track(&disk, c, h, &track);
            uft_track_cleanup(&track);
        }
    }
    uft_close(&disk);
    BENCH_END();  // Should be < 50ms
}

void bench_scp_to_d64(void) {
    BENCH_START("scp_to_d64");
    uft_convert("test.scp", "output.d64", NULL);
    BENCH_END();  // Should be < 2s
}
```

---

## 3. Profiling-Infrastruktur

### 3.1 Profiling-Hooks

```c
// include/uft/core/uft_profile.h

#ifdef UFT_PROFILING

typedef struct {
    const char* name;
    uint64_t total_ns;
    uint64_t call_count;
    uint64_t max_ns;
    uint64_t min_ns;
} uft_profile_entry_t;

#define UFT_PROFILE_BEGIN(name) \
    uint64_t _prof_start_##name = uft_profile_now()

#define UFT_PROFILE_END(name) \
    uft_profile_record(#name, uft_profile_now() - _prof_start_##name)

void uft_profile_init(void);
void uft_profile_report(FILE* out);
void uft_profile_reset(void);

#else

#define UFT_PROFILE_BEGIN(name) ((void)0)
#define UFT_PROFILE_END(name) ((void)0)

#endif
```

### 3.2 Beispiel-Report

```
═══════════════════════════════════════════════════════════════════════════════
UFT PROFILING REPORT
═══════════════════════════════════════════════════════════════════════════════

Function                  Calls       Total (ms)  Avg (µs)    Max (µs)
──────────────────────────────────────────────────────────────────────────────
mfm_decode                   80         312.45      3905        4521
gcr_decode                   35         142.31      4066        4892
flux_accumulate             160         156.78       980        1245
scp_read_revolution         160         823.45      5146        8234
d64_read_sector            683          42.12        62         128
pll_sync_find               80          89.34      1117        1567
──────────────────────────────────────────────────────────────────────────────
TOTAL                                  1566.45

Top 3 Hotspots:
  1. scp_read_revolution  52.6% of total
  2. mfm_decode           19.9% of total
  3. flux_accumulate      10.0% of total
═══════════════════════════════════════════════════════════════════════════════
```

---

## 4. Identifizierte Hotspots

### 4.1 Aktuelle Hotspots

| Function | % of Time | Optimierung |
|----------|-----------|-------------|
| `flux_accumulate` | 15-30% | SIMD (SSE2/AVX2) ✅ |
| `mfm_decode` | 20-40% | SIMD, LUT ✅ |
| `gcr_decode` | 15-30% | SIMD, LUT ✅ |
| `pll_sync_find` | 10-20% | Kalman-PLL ✅ |
| `file_read` | 10-20% | Memory-mapped I/O |

### 4.2 SIMD-Optimierungen

```c
// Verfügbare SIMD-Implementierungen:
// src/simd/uft_simd_mfm_sse2.c
// src/simd/uft_simd_mfm_avx2.c
// src/simd/uft_simd_gcr_sse2.c
// src/simd/uft_simd_gcr_avx2.c

// Runtime-Detection:
if (uft_cpu_has_avx2()) {
    use_avx2_decoder();
} else if (uft_cpu_has_sse2()) {
    use_sse2_decoder();
} else {
    use_scalar_decoder();
}
```

---

## 5. Regression Prevention

### 5.1 CI Performance Gate

```yaml
# .github/workflows/benchmark.yml
benchmark:
  runs-on: ubuntu-latest
  steps:
    - name: Run Benchmarks
      run: ./tests/benchmark/run_all.sh > results.txt
      
    - name: Check Regression
      run: |
        ./scripts/check_perf_regression.sh results.txt baseline.txt 10
        # Fails if any benchmark > 10% slower than baseline
```

### 5.2 Baseline-Management

```bash
# Bei Release: Baseline aktualisieren
./tests/benchmark/run_all.sh > baseline.txt
git add baseline.txt
git commit -m "Update performance baseline for v2.9"
```

---

## 6. Optimierungs-Prioritäten

### 6.1 High Priority

1. **File I/O:** Memory-mapped für große Dateien
2. **Flux Processing:** SIMD durchgängig
3. **Memory Allocation:** Pool für häufige Allokationen

### 6.2 Medium Priority

1. **PLL:** Tabellenbasierte Approximation
2. **CRC:** Hardware-CRC wenn verfügbar (ARM)
3. **String Operations:** Keine strlen() in Loops

### 6.3 Low Priority

1. **GUI Rendering:** Lazy updates
2. **Format Detection:** Caching
3. **Config Loading:** Binary format statt JSON

---

**DOKUMENT ENDE**
