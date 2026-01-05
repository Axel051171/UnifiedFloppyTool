# UnifiedFloppyTool v2.1 Release Notes

## Übersicht

Diese Version enthält fundamentale Algorithmus-Verbesserungen und ein neues 
forensisches Recovery-System.

---

## Neue Features

### 1. GOD MODE Algorithmen (ersetzt bestehende Implementierungen)

#### Kalman-PLL (`src/algorithms/uft_kalman_pll.c`)
- **Ersetzt:** Alter PLL mit festem Alpha-Wert
- **Neu:** Kalman-Filter mit adaptivem Gain
- **Vorteile:**
  - Beweisbar optimale Schätzung für Gaußsches Rauschen
  - Automatische Drift-Erkennung
  - Per-Bit Confidence-Scores
  - Weak-Bit-Erkennung (Innovation > 3σ)
- **LOC:** 377

#### GCR-Viterbi-Decoder (`src/algorithms/uft_gcr_viterbi.c`)
- **Ersetzt:** Alter GCR-Decoder mit 10-Sample Cell-Detection
- **Neu:** Viterbi-basierter Soft-Decision-Decoder
- **Vorteile:**
  - Format-Erkennung via Sync-Pattern-Analyse
  - Fehlerkorrektur durch Minimum-Hamming-Distanz
  - Per-Sector Cell-Time-Kalibrierung
  - Unterstützt C64 und Apple GCR
- **LOC:** 456

#### Bayesian Format Classifier (`src/algorithms/uft_bayesian_format.c`)
- **Ersetzt:** Alter Format-Detector mit linearer Score-Addition
- **Neu:** Echte Bayessche Klassifikation
- **Vorteile:**
  - Mathematisch korrekte Wahrscheinlichkeiten
  - Prior-Anpassung nach Region (EU/US/JP)
  - Unsicherheits-Quantifizierung
  - Top-10 Kandidaten mit Posterior-Verteilung
- **LOC:** 572

### 2. Forensic Recovery System

#### Multi-Phasen-Recovery (`src/recovery/`)
- **Phase 1:** Timing-Analyse
- **Phase 2:** Fuzzy Sync-Detection
- **Phase 3:** Multi-Pass-Korrelation
- **Phase 4:** CRC-basierte Fehlerkorrektur
- **Phase 5:** Validierung

#### Kernfähigkeiten
| Feature | Alter Code | Neuer Code |
|---------|------------|------------|
| CRC-Fehler | Verwerfen | Korrigieren (1-2 Bit) |
| Sync-Suche | Exakt | Fuzzy (Hamming ≤ 2) |
| Multi-Pass | Majority Voting | Confidence-gewichtet |
| Weak-Bits | Ignoriert | Erkannt + markiert |
| Audit-Trail | Keiner | Vollständig |

---

## Test-Ergebnisse

| Test-Suite | Bestanden | Total | Quote |
|------------|-----------|-------|-------|
| Kalman PLL | 4 | 5 | 80% |
| GCR Viterbi | 7 | 7 | 100% |
| Bayesian Format | 6 | 7 | 86% |
| **Gesamt** | **17** | **19** | **89%** |

---

## Code-Statistik

| Komponente | LOC |
|------------|-----|
| Implementation (.c) | 55,793 |
| Headers (.h) | 48,372 |
| Tests | 4,850 |
| Dokumentation | 13,000+ |
| **TOTAL** | **122,015+** |

| Dateien | Anzahl |
|---------|--------|
| C-Dateien | 285+ |
| Header-Dateien | 261+ |
| Test-Dateien | 15+ |

---

## Upgrade-Hinweise

### Algorithmen-Austausch

Die alten Algorithmen in `step23_reference/` bleiben als Referenz erhalten.
Neue Algorithmen sind in `src/algorithms/` und `src/recovery/`.

### API-Änderungen

```c
// Alt
uft_pll_cfg_t cfg = uft_pll_cfg_default_mfm_dd();
size_t bits = uft_flux_to_bits_pll(timestamps, count, &cfg, ...);

// Neu
uft_kalman_pll_config_t cfg = uft_kalman_pll_config_mfm_dd();
uft_kalman_pll_state_t state;
uft_kalman_pll_init(&state, &cfg);
size_t bits = uft_kalman_pll_process(&state, timestamps, count, out_bits, out_conf);
```

### Forensic Mode

```c
// Paranoid-Mode für maximale Sicherheit
uft_forensic_config_t cfg = uft_forensic_config_paranoid();
cfg.audit_log = fopen("audit.log", "w");
cfg.min_passes = 5;

uft_forensic_session_t session;
uft_forensic_session_init(&session, &cfg);

// Recovery durchführen
uft_forensic_recover_track(..., &session, &track);

// Report generieren
uft_forensic_session_finish(&session);
```

---

## Bekannte Einschränkungen

1. **CRC-Korrektur:** Nur 1-2 Bit-Fehler korrigierbar
2. **Performance:** Forensic-Mode ist langsamer (Gründlichkeit > Geschwindigkeit)
3. **Speicher:** `preserve_all_passes=true` benötigt mehr RAM

---

## Lizenz

MIT License - Copyright (c) 2025 UnifiedFloppyTool Contributors
