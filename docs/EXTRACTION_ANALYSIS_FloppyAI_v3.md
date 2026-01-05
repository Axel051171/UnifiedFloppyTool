# UFT Extraction Analysis - FloppyAI + v3

> **Datum:** 2026-01-04  
> **Pakete:** floppyai-main.zip, uft_floppy_extraction_v3.zip  
> **Gesamt analysiert:** ~52,000 LOC  
> **Integriert:** 4 Header + 11 Referenz-Dateien

---

## 📊 FloppyAI - Übersicht

**FloppyAI** ist ein Python-basiertes Flux-Analyse-Tool mit innovativen Algorithmen:

| Datei | LOC | Funktion |
|-------|-----|----------|
| flux_analyzer.py | 601 | KryoFlux C2/OOB Parsing, Instability |
| patterns.py | 173 | PRBS/LFSR Pattern Generation |
| overlay_detection.py | 172 | FFT/ACF Sector Detection |
| custom_decoder.py | 213 | K-means Adaptive Decoding |
| custom_encoder.py | 149 | Manchester/RLL Encoding |
| metrics.py | 424 | Experiment Analysis |
| analyze_disk.py | 686 | Disk Surface Mapping |

**Gesamt:** ~2,400 LOC Python

---

## 🧠 Kern-Algorithmen aus FloppyAI

### 1. K-Means Clustering für Thresholds

```python
# FloppyAI custom_decoder.py
kmeans = KMeans(n_clusters=3, random_state=42, n_init=10)
labels = kmeans.fit_predict(flux_data.reshape(-1, 1))
centers = sorted(kmeans.cluster_centers_.flatten())
# centers[0] = half_short, centers[1] = long_cell
```

**UFT Port:** `uft_adaptive_decoder.h`
- Ersetzt statische Thresholds durch datengetriebene
- Funktioniert bei unbekannten Formaten
- Automatische Encoding-Detection

### 2. Instability Scoring

```python
# FloppyAI flux_analyzer.py
w_var = 0.4; w_incoh = 0.3; w_gap = 0.2; w_out = 0.1
instability_score = float(
    np.clip(
        w_var * phase_var_p95
        + w_incoh * phase_incoherence
        + w_gap * gap_rate
        + w_out * outlier_rate,
        0.0, 1.0
    )
)
```

**UFT Port:** `uft_flux_instability.h`
- Phase Variance: Angular jitter across revolutions
- Cross-Rev Coherence: Korrelation zum Mittelwert-Profil
- Outlier/Gap Detection: Anomale Intervalle

### 3. FFT Sector Detection (MFM)

```python
# FloppyAI overlay_detection.py
H = np.fft.rfft(hist)
P = np.abs(H) ** 2
P[0] = 0.0  # Remove DC
k0 = int(np.argmax(P))  # Dominant frequency = sector count

# Autocorrelation confirmation
acf = np.fft.irfft(P)
acf[0] = 0.0
k1 = int(np.argmax(acf[:bins//2]))
```

**UFT Port:** `uft_sector_overlay.h`
- Findet Sektoren ohne Format-Wissen
- FFT für Periodizität, ACF für Bestätigung
- Local Maximum Refinement

### 4. PRBS/LFSR Pattern Generation

```python
# FloppyAI patterns.py
taps = {
    7: (7, 6),     # x^7 + x^6 + 1
    15: (15, 14),  # x^15 + x^14 + 1
}
state = 1 << (order - 1)
fb = ((state >> (t[0]-1)) ^ (state >> (t[1]-1))) & 1
state = (state >> 1) | (fb << (order - 1))
```

**UFT Port:** `uft_pattern_generator.h`
- Standard PRBS für PLL-Tests
- Verschiedene Muster für Media-Toleranz

---

## 📁 v3 HxC Reference (45,000 LOC)

| Datei | LOC | Beschreibung |
|-------|-----|--------------|
| fluxStreamAnalyzer.c | 4816 | **Master** Flux Analyzer |
| display_track.c | 4188 | Track Visualization |
| track_generator.c | 1719 | Track Generation |
| sector_extractor.c | 1250 | Sector Extraction |

### fluxStreamAnalyzer.c Highlights

- **Victor 9k Bands:** Zone-basierte GCR Timing
- **Peak Detection:** Histogram-basierte Analyse
- **Multi-Pass:** Second-pass analysis for accuracy
- **PLL Tuning:** Block-based timing adjustment

---

## 🆕 Neue UFT Header

### uft_flux_instability.h (350 LOC)
```c
typedef struct {
    double phase_var_p95;       // 95th percentile variance
    double phase_incoherence;   // 1 - mean(correlation)
    double outlier_rate;        // Short/long outliers
    double gap_rate;            // Very long gaps
} uft_instab_features_t;

double uft_instab_compute_score(const uft_instab_features_t *f);
```

### uft_pattern_generator.h (310 LOC)
```c
typedef struct {
    uint32_t state;
    uint8_t order;      // 7, 15, 23, 31
    uint8_t tap1, tap2;
} uft_lfsr_t;

static inline uint8_t uft_lfsr_next_bit(uft_lfsr_t *lfsr);
void uft_pattern_gen_prbs(uft_pattern_data_t *data, uint8_t order, ...);
```

### uft_sector_overlay.h (340 LOC)
```c
int uft_overlay_detect_mfm(const double *histogram, uint16_t bins,
                           const uft_overlay_config_t *config,
                           uft_overlay_result_t *result);

int uft_overlay_detect_gcr(const double *histogram, uint16_t bins,
                           const int *candidates, size_t candidate_count, ...);
```

### uft_adaptive_decoder.h (365 LOC)
```c
uint8_t uft_adec_kmeans(const uint32_t *intervals, size_t count, uint8_t k,
                        uft_adec_cluster_t *clusters, uint16_t max_iterations);

int uft_adec_learn(uft_adec_state_t *state, const uint32_t *intervals, size_t count);
int uft_adec_decode(uft_adec_state_t *state, const uint32_t *intervals, ...);
```

---

## 🔧 Implementation Notes

### Dependencies für C Port

1. **K-Means:** Eigene Implementation oder libcluster
2. **FFT:** FFTW3 oder KissFFT für Overlay Detection
3. **Statistics:** Eigene mean/std/correlation Funktionen

### Priorität

1. ✅ Headers erstellt (API Design)
2. 🔄 K-Means Implementation (keine externe Deps)
3. 🔄 Statistics Functions
4. ⏸️ FFT Integration (benötigt Library-Entscheidung)

---

## ✅ Integration Summary

- **4 neue Header-Module** (~1,365 LOC)
- **8 FloppyAI Python Referenzen** (~2,400 LOC)
- **4 v3 HxC C Referenzen** (~12,000 LOC)
- **2 Pakete** analysiert
- **~52,000 LOC** reviewed
- Projekt gewachsen auf **~200,000 LOC**
