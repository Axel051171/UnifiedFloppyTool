/**
 * @file uft_flux_statistics.h
 * @brief Erweiterte Flux-Statistik und Hardware-Korrelation
 * 
 * Implementiert M-001, M-005, S-007 aus TODO_MASTER.md:
 * - Varianz-Berechnung pro Bitcell
 * - Confidence-Score pro Sektor
 * - Anomalie-Heatmap
 * - Hardware-Decode-Korrelation
 * - PLL-Qualitäts-Metriken
 * 
 * Leitmotiv: "Bei uns geht kein Bit verloren"
 * 
 * @version 3.3.0
 * @date 2026-01-04
 */

#ifndef UFT_FLUX_STATISTICS_H
#define UFT_FLUX_STATISTICS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Konstanten
 *============================================================================*/

#define UFT_FSTAT_VERSION           0x030300  /* 3.3.0 */
#define UFT_FSTAT_MAX_TRACKS        84
#define UFT_FSTAT_MAX_SECTORS       32
#define UFT_FSTAT_MAX_REVOLUTIONS   64
#define UFT_FSTAT_HISTOGRAM_BINS    512
#define UFT_FSTAT_HEATMAP_RESOLUTION 256

/* Anomalie-Schwellenwerte */
#define UFT_FSTAT_ANOMALY_NONE      0
#define UFT_FSTAT_ANOMALY_LOW       1    /* <10% Abweichung */
#define UFT_FSTAT_ANOMALY_MEDIUM    2    /* 10-25% Abweichung */
#define UFT_FSTAT_ANOMALY_HIGH      3    /* 25-50% Abweichung */
#define UFT_FSTAT_ANOMALY_CRITICAL  4    /* >50% Abweichung */

/* PLL Status */
#define UFT_PLL_STATUS_LOCKED       0x01
#define UFT_PLL_STATUS_TRACKING     0x02
#define UFT_PLL_STATUS_SLIP         0x04
#define UFT_PLL_STATUS_LOST         0x08
#define UFT_PLL_STATUS_REACQUIRE    0x10

/* Fehler-Codes */
#define UFT_FSTAT_OK                0
#define UFT_FSTAT_ERR_NULL          -1
#define UFT_FSTAT_ERR_RANGE         -2
#define UFT_FSTAT_ERR_MEMORY        -3
#define UFT_FSTAT_ERR_NO_DATA       -4
#define UFT_FSTAT_ERR_INVALID       -5

/*============================================================================
 * Bitcell-Statistik
 *============================================================================*/

/**
 * @brief Statistische Daten für eine einzelne Bitcell
 */
typedef struct uft_bitcell_stats {
    /* Timing-Statistik */
    double   mean_timing_ns;      /**< Mittleres Timing in ns */
    double   variance_ns;         /**< Varianz in ns² */
    double   stddev_ns;           /**< Standardabweichung in ns */
    double   min_timing_ns;       /**< Minimum über alle Revolutions */
    double   max_timing_ns;       /**< Maximum über alle Revolutions */
    
    /* Confidence */
    uint8_t  confidence;          /**< 0-100% */
    uint8_t  consistency;         /**< Konsistenz über Revolutions */
    
    /* Wert-Statistik */
    uint16_t one_count;           /**< Wie oft als 1 gelesen */
    uint16_t zero_count;          /**< Wie oft als 0 gelesen */
    uint8_t  best_value;          /**< Wahrscheinlichster Wert */
    
    /* Anomalie */
    uint8_t  anomaly_level;       /**< UFT_FSTAT_ANOMALY_* */
    uint16_t anomaly_flags;       /**< Detaillierte Flags */
    
    /* Position */
    uint32_t bit_position;        /**< Position im Track */
    uint32_t flux_sample_start;   /**< Erste Flux-Sample Referenz */
} uft_bitcell_stats_t;

/*============================================================================
 * Sektor-Statistik
 *============================================================================*/

/**
 * @brief Statistische Daten für einen Sektor
 */
typedef struct uft_sector_stats {
    /* Identifikation */
    uint8_t  cylinder;
    uint8_t  head;
    uint8_t  sector;
    uint16_t size;
    
    /* CRC-Status */
    bool     header_crc_ok;
    bool     data_crc_ok;
    uint16_t header_crc;
    uint16_t data_crc;
    
    /* Confidence */
    uint8_t  min_confidence;      /**< Minimale Bitcell-Confidence */
    uint8_t  avg_confidence;      /**< Durchschnittliche Confidence */
    uint8_t  max_confidence;      /**< Maximale Confidence */
    
    /* Bitcell-Analyse */
    uint16_t total_bits;          /**< Gesamte Bits im Sektor */
    uint16_t weak_bits;           /**< Bits mit Confidence <60% */
    uint16_t ambiguous_bits;      /**< Bits mit Confidence <50% */
    uint16_t corrected_bits;      /**< CRC-korrigierte Bits */
    
    /* Timing */
    double   avg_timing_ns;       /**< Durchschnittliches Bit-Timing */
    double   timing_jitter_ns;    /**< Timing-Jitter (Stddev) */
    
    /* Anomalien */
    uint8_t  max_anomaly_level;   /**< Höchste Anomalie im Sektor */
    uint16_t anomaly_count;       /**< Anzahl Anomalien */
    
    /* Revolution-Statistik */
    uint8_t  revolutions_used;    /**< Für Fusion genutzte Revs */
    uint8_t  best_revolution;     /**< Beste einzelne Revolution */
    
    /* Position */
    uint32_t start_bit;           /**< Start-Position im Track */
    uint32_t end_bit;             /**< End-Position im Track */
} uft_sector_stats_t;

/*============================================================================
 * Track-Statistik
 *============================================================================*/

/**
 * @brief Statistische Daten für einen Track
 */
typedef struct uft_track_stats {
    /* Identifikation */
    uint8_t  cylinder;
    uint8_t  head;
    
    /* Sector-Stats */
    uint8_t  sector_count;
    uft_sector_stats_t sectors[UFT_FSTAT_MAX_SECTORS];
    
    /* Gesamt-Statistik */
    uint8_t  overall_confidence;
    uint8_t  sectors_ok;          /**< Sektoren mit CRC OK */
    uint8_t  sectors_recovered;   /**< Durch Fusion wiederhergestellt */
    uint8_t  sectors_failed;      /**< Nicht lesbar */
    
    /* Timing */
    double   rotation_time_ms;    /**< Index-zu-Index Zeit */
    double   rpm;                 /**< Berechnete RPM */
    double   rpm_variance;        /**< RPM-Varianz über Revolutions */
    
    /* Anomalie-Heatmap */
    uint8_t  heatmap[UFT_FSTAT_HEATMAP_RESOLUTION]; /**< Anomalie-Level pro Position */
    
    /* Flux-Statistik */
    uint32_t total_flux_transitions;
    uint32_t flux_min;            /**< Minimum Flux-Count */
    uint32_t flux_max;            /**< Maximum Flux-Count */
    double   flux_mean;
    double   flux_variance;
} uft_track_stats_t;

/*============================================================================
 * PLL-Qualitäts-Metriken (S-007)
 *============================================================================*/

/**
 * @brief PLL-Qualitäts-Metriken
 */
typedef struct uft_pll_metrics {
    /* Lock-Status */
    uint8_t  status;              /**< UFT_PLL_STATUS_* Flags */
    uint32_t lock_time_samples;   /**< Samples bis Lock erreicht */
    uint32_t total_samples;       /**< Gesamte verarbeitete Samples */
    
    /* Phase-Fehler */
    double   phase_error_mean;    /**< Mittlerer Phase-Fehler */
    double   phase_error_variance;/**< Phase-Fehler Varianz */
    double   phase_error_max;     /**< Maximaler Phase-Fehler */
    
    /* Frequenz */
    double   frequency_estimate;  /**< Geschätzte Bit-Frequenz */
    double   frequency_drift;     /**< Frequenz-Drift über Track */
    
    /* Ereignisse */
    uint32_t sync_loss_count;     /**< Anzahl Sync-Verluste */
    uint32_t slip_count;          /**< Anzahl Bit-Slips */
    uint32_t reacquire_count;     /**< Anzahl Re-Akquisitionen */
    
    /* Qualitäts-Score */
    uint8_t  quality_score;       /**< 0-100 Gesamt-Qualität */
    
    /* Histogram der Phase-Fehler */
    uint16_t phase_histogram[UFT_FSTAT_HISTOGRAM_BINS];
} uft_pll_metrics_t;

/*============================================================================
 * Hardware-Decode-Korrelation (M-005)
 *============================================================================*/

/**
 * @brief Korrelation zwischen Hardware-Messung und Decode-Fehler
 */
typedef struct uft_decode_correlation {
    /* Position */
    uint32_t bit_position;
    uint32_t flux_sample_index;
    
    /* Decode-Ergebnis */
    bool     decode_error;        /**< Fehler beim Decodieren */
    uint8_t  error_type;          /**< Art des Fehlers */
    
    /* Hardware-Messwerte zum Fehlerzeitpunkt */
    double   timing_at_error_ns;  /**< Timing an Fehler-Position */
    double   timing_expected_ns;  /**< Erwartetes Timing */
    double   timing_deviation;    /**< Abweichung in % */
    
    /* PLL-Status zum Fehlerzeitpunkt */
    uint8_t  pll_status;          /**< PLL-Status bei Fehler */
    double   pll_phase_error;     /**< Phase-Fehler bei Fehler */
    
    /* Flux-Kontext */
    uint8_t  flux_pattern[8];     /**< Flux-Muster um Fehler */
    uint8_t  expected_pattern[8]; /**< Erwartetes Muster */
    
    /* Korrelations-Score */
    uint8_t  correlation_score;   /**< Wie stark korreliert */
} uft_decode_correlation_t;

/**
 * @brief Aggregierte Korrelations-Statistik
 */
typedef struct uft_correlation_stats {
    /* Fehler-Zähler */
    uint32_t total_errors;
    uint32_t timing_correlated;   /**< Fehler durch Timing */
    uint32_t pll_correlated;      /**< Fehler durch PLL */
    uint32_t pattern_correlated;  /**< Fehler durch Muster */
    uint32_t uncorrelated;        /**< Keine klare Ursache */
    
    /* Timing-Korrelation */
    double   timing_threshold_ns; /**< Ab wann Timing problematisch */
    double   avg_error_deviation; /**< Mittlere Abweichung bei Fehlern */
    
    /* PLL-Korrelation */
    uint32_t errors_at_lock_loss;
    uint32_t errors_at_slip;
    double   avg_phase_at_error;
    
    /* Muster-Analyse */
    uint8_t  problem_patterns[16][8]; /**< Häufige Problem-Muster */
    uint32_t pattern_counts[16];
} uft_correlation_stats_t;

/*============================================================================
 * Analyse-Report
 *============================================================================*/

/**
 * @brief Vollständiger Analyse-Report
 */
typedef struct uft_flux_analysis_report {
    /* Header */
    uint32_t version;
    uint32_t timestamp;
    char     source_file[256];
    
    /* Track-Statistik */
    uint8_t  total_tracks;
    uft_track_stats_t tracks[UFT_FSTAT_MAX_TRACKS * 2]; /* Beide Seiten */
    
    /* Gesamt-Statistik */
    uint32_t total_sectors;
    uint32_t sectors_ok;
    uint32_t sectors_recovered;
    uint32_t sectors_failed;
    uint8_t  overall_confidence;
    
    /* PLL-Metriken (aggregiert) */
    uft_pll_metrics_t pll_metrics;
    
    /* Korrelations-Analyse */
    uft_correlation_stats_t correlation;
    
    /* Anomalie-Zusammenfassung */
    uint32_t anomaly_total;
    uint32_t anomaly_by_level[5]; /* NONE, LOW, MEDIUM, HIGH, CRITICAL */
    
    /* Empfehlungen */
    char     recommendations[1024];
} uft_flux_analysis_report_t;

/*============================================================================
 * Funktionen - Initialisierung
 *============================================================================*/

/**
 * @brief Initialisiert das Statistik-System
 */
int uft_fstat_init(void);

/**
 * @brief Gibt Ressourcen frei
 */
void uft_fstat_cleanup(void);

/*============================================================================
 * Funktionen - Bitcell-Analyse
 *============================================================================*/

/**
 * @brief Analysiert Bitcell-Statistik über mehrere Revolutions
 * @param flux_data Array von Flux-Daten (pro Revolution)
 * @param rev_count Anzahl Revolutions
 * @param bit_position Position im Track
 * @param[out] stats Ergebnis
 */
int uft_fstat_analyze_bitcell(
    const uint32_t *flux_data[],
    size_t rev_count,
    uint32_t bit_position,
    uft_bitcell_stats_t *stats
);

/**
 * @brief Berechnet Varianz für Bitcell-Array
 */
int uft_fstat_calculate_variance(
    const double *values,
    size_t count,
    double *mean,
    double *variance,
    double *stddev
);

/*============================================================================
 * Funktionen - Sektor-Analyse
 *============================================================================*/

/**
 * @brief Analysiert einen Sektor
 * @param track_data Decodierte Track-Daten
 * @param track_len Länge in Bytes
 * @param sector_num Sektor-Nummer
 * @param[out] stats Ergebnis
 */
int uft_fstat_analyze_sector(
    const uint8_t *track_data,
    size_t track_len,
    uint8_t sector_num,
    uft_sector_stats_t *stats
);

/**
 * @brief Berechnet Confidence-Score für Sektor
 */
uint8_t uft_fstat_sector_confidence(const uft_sector_stats_t *stats);

/*============================================================================
 * Funktionen - Track-Analyse
 *============================================================================*/

/**
 * @brief Analysiert einen kompletten Track
 * @param flux_revs Array von Flux-Daten (pro Revolution)
 * @param rev_count Anzahl Revolutions
 * @param cylinder Zylinder-Nummer
 * @param head Head-Nummer
 * @param[out] stats Ergebnis
 */
int uft_fstat_analyze_track(
    const uint32_t *flux_revs[],
    const size_t *rev_lengths,
    size_t rev_count,
    uint8_t cylinder,
    uint8_t head,
    uft_track_stats_t *stats
);

/**
 * @brief Generiert Anomalie-Heatmap für Track
 */
int uft_fstat_generate_heatmap(
    const uft_track_stats_t *stats,
    uint8_t *heatmap,
    size_t resolution
);

/*============================================================================
 * Funktionen - PLL-Metriken (S-007)
 *============================================================================*/

/**
 * @brief Sammelt PLL-Metriken während der Decodierung
 * @param sample_time Aktueller Sample-Zeitstempel
 * @param expected_time Erwarteter Zeitstempel
 * @param locked Ist PLL gelockt?
 * @param[out] metrics Metriken (wird aktualisiert)
 */
int uft_fstat_pll_update(
    double sample_time,
    double expected_time,
    bool locked,
    uft_pll_metrics_t *metrics
);

/**
 * @brief Berechnet PLL-Qualitäts-Score
 */
uint8_t uft_fstat_pll_quality_score(const uft_pll_metrics_t *metrics);

/**
 * @brief Erkennt PLL-Events (Lock-Loss, Slip, etc.)
 */
int uft_fstat_pll_detect_events(
    const double *phase_errors,
    size_t count,
    uft_pll_metrics_t *metrics
);

/*============================================================================
 * Funktionen - Hardware-Decode-Korrelation (M-005)
 *============================================================================*/

/**
 * @brief Korreliert Decode-Fehler mit Hardware-Messwerten
 * @param error_position Bit-Position des Fehlers
 * @param flux_data Flux-Daten am Fehler
 * @param pll PLL-Metriken
 * @param[out] correlation Ergebnis
 */
int uft_fstat_correlate_error(
    uint32_t error_position,
    const uint32_t *flux_data,
    const uft_pll_metrics_t *pll,
    uft_decode_correlation_t *correlation
);

/**
 * @brief Aggregiert Korrelations-Statistik
 */
int uft_fstat_aggregate_correlations(
    const uft_decode_correlation_t *correlations,
    size_t count,
    uft_correlation_stats_t *stats
);

/*============================================================================
 * Funktionen - Report-Generierung
 *============================================================================*/

/**
 * @brief Erstellt vollständigen Analyse-Report
 * @param tracks Array von Track-Statistiken
 * @param track_count Anzahl Tracks
 * @param pll PLL-Metriken
 * @param correlation Korrelations-Statistik
 * @param[out] report Ergebnis
 */
int uft_fstat_create_report(
    const uft_track_stats_t *tracks,
    size_t track_count,
    const uft_pll_metrics_t *pll,
    const uft_correlation_stats_t *correlation,
    uft_flux_analysis_report_t *report
);

/**
 * @brief Exportiert Report als JSON
 */
int uft_fstat_export_json(
    const uft_flux_analysis_report_t *report,
    char *buffer,
    size_t buffer_size
);

/**
 * @brief Exportiert Report als Markdown
 */
int uft_fstat_export_markdown(
    const uft_flux_analysis_report_t *report,
    char *buffer,
    size_t buffer_size
);

/*============================================================================
 * Funktionen - Anomalie-Detektion
 *============================================================================*/

/**
 * @brief Bewertet Anomalie-Level basierend auf Statistik
 */
uint8_t uft_fstat_evaluate_anomaly(
    double value,
    double expected,
    double tolerance_percent
);

/**
 * @brief Erkennt Anomalie-Muster im Track
 */
int uft_fstat_detect_anomalies(
    const uft_track_stats_t *stats,
    uint32_t *anomaly_positions,
    uint8_t *anomaly_levels,
    size_t max_anomalies,
    size_t *found_count
);

/*============================================================================
 * Utility-Funktionen
 *============================================================================*/

/**
 * @brief Konvertiert Anomalie-Level zu String
 */
const char *uft_fstat_anomaly_name(uint8_t level);

/**
 * @brief Konvertiert PLL-Status zu String
 */
const char *uft_fstat_pll_status_name(uint8_t status);

/**
 * @brief Berechnet Confidence aus Varianz
 */
uint8_t uft_fstat_variance_to_confidence(double variance, double max_variance);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FLUX_STATISTICS_H */
