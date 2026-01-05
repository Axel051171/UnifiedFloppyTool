/**
 * @file flux_stat.h
 * @brief Statistical Flux Recovery (FluxStat) - Inspiriert von FluxRipper
 * 
 * Multi-Pass Flux Capture und statistische Analyse für die Wiederherstellung
 * marginaler/schwacher Sektoren. Übertrifft SpinRite's DynaStat durch Zugang
 * zu rohen Flux-Daten.
 * 
 * @version 1.0.0
 * @date 2024-12-30
 */

#ifndef UFT_FLUX_STAT_H
#define UFT_FLUX_STAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Konstanten
 *============================================================================*/

#define FLUXSTAT_MAX_PASSES      64
#define FLUXSTAT_DEFAULT_PASSES  8
#define FLUXSTAT_MIN_PASSES      2

/* Confidence Levels */
#define CONF_STRONG    90   /* 90%+ = starkes Signal */
#define CONF_WEAK      60   /* 60-89% = schwaches Signal */
#define CONF_AMBIGUOUS 50   /* <60% = mehrdeutig */

/* Fehler-Codes */
#define FLUXSTAT_OK           0
#define FLUXSTAT_ERR_INVALID  -1
#define FLUXSTAT_ERR_BUSY     -2
#define FLUXSTAT_ERR_TIMEOUT  -3
#define FLUXSTAT_ERR_NO_DATA  -4
#define FLUXSTAT_ERR_ABORT    -5
#define FLUXSTAT_ERR_MEMORY   -6

/*============================================================================
 * Bit-Zell Klassifikation
 *============================================================================*/

typedef enum {
    BITCELL_STRONG_1  = 0,  /* Starke 1 (>90% Confidence) */
    BITCELL_WEAK_1    = 1,  /* Schwache 1 (60-89% Confidence) */
    BITCELL_STRONG_0  = 2,  /* Starke 0 (>90% Confidence) */
    BITCELL_WEAK_0    = 3,  /* Schwache 0 (60-89% Confidence) */
    BITCELL_AMBIGUOUS = 4   /* Mehrdeutig (<60% Confidence) */
} bitcell_class_t;

/*============================================================================
 * Datenstrukturen
 *============================================================================*/

/**
 * @brief Flux-Korrelation über mehrere Durchläufe
 */
typedef struct {
    uint64_t time_sum;       /* Summe der Timestamps */
    uint64_t time_sum_sq;    /* Summe der Quadrate (für Varianz) */
    uint32_t hit_count;      /* Treffer über alle Durchläufe */
    uint32_t total_passes;   /* Gesamte Durchläufe */
} flux_correlation_t;

/**
 * @brief Einzelnes Bit mit Analyse
 */
typedef struct {
    uint8_t  value;           /* 0 oder 1 */
    uint8_t  confidence;      /* 0-100% */
    uint8_t  transition_count;/* Anzahl erkannter Übergänge */
    uint16_t timing_stddev;   /* Timing-Standardabweichung */
    uint8_t  classification;  /* bitcell_class_t */
    uint8_t  corrected;       /* War CRC-korrigiert? */
} fluxstat_bit_t;

/**
 * @brief Daten eines einzelnen Durchlaufs
 */
typedef struct {
    uint32_t flux_count;     /* Anzahl Flux-Übergänge */
    uint32_t index_time;     /* Index-zu-Index Zeit (Clocks) */
    uint32_t base_addr;      /* Speicheradresse der Daten */
    uint32_t data_size;      /* Datengröße in Bytes */
} fluxstat_pass_t;

/**
 * @brief Multi-Pass Capture Ergebnis
 */
typedef struct {
    uint8_t  pass_count;                     /* Tatsächliche Durchläufe */
    uint32_t total_flux;                     /* Summe aller Flux-Übergänge */
    uint32_t min_flux;                       /* Minimum pro Durchlauf */
    uint32_t max_flux;                       /* Maximum pro Durchlauf */
    uint32_t total_time;                     /* Gesamte Capture-Zeit */
    uint32_t base_addr;                      /* Basis-Speicheradresse */
    fluxstat_pass_t passes[FLUXSTAT_MAX_PASSES];
} fluxstat_capture_t;

/**
 * @brief Sektor-Analyse Ergebnis
 */
typedef struct {
    uint8_t  sector_num;      /* Sektor-Nummer */
    uint16_t size;            /* Sektor-Größe */
    uint8_t  crc_ok;          /* CRC war OK */
    uint8_t  confidence_min;  /* Minimale Bit-Confidence */
    uint8_t  confidence_avg;  /* Durchschnittliche Confidence */
    uint16_t weak_bit_count;  /* Anzahl schwacher Bits */
    uint16_t corrected_count; /* Anzahl korrigierter Bits */
    uint8_t  *data;           /* Wiederhergestellte Daten */
    fluxstat_bit_t *bit_map;  /* Bit-für-Bit Analyse (optional) */
} fluxstat_sector_t;

/**
 * @brief Track-Analyse Ergebnis
 */
typedef struct {
    uint8_t  track;
    uint8_t  head;
    uint8_t  sector_count;
    uint8_t  sectors_recovered;
    uint8_t  overall_confidence;
    fluxstat_sector_t sectors[32];  /* Max 32 Sektoren pro Track */
} fluxstat_track_t;

/**
 * @brief Flux-Histogram für Qualitätsanalyse
 */
typedef struct {
    uint16_t bins[256];           /* 256 Histogram-Bins */
    uint32_t total_count;         /* Gesamte Flux-Übergänge */
    uint16_t interval_min;        /* Minimum-Intervall */
    uint16_t interval_max;        /* Maximum-Intervall */
    uint8_t  peak_bin;            /* Bin mit höchster Anzahl */
    uint16_t peak_count;          /* Anzahl im Peak-Bin */
    uint16_t mean_interval;       /* EMA des Intervalls */
    uint32_t overflow_count;      /* Überläufe (>255 Bins) */
} fluxstat_histogram_t;

/**
 * @brief Konfiguration
 */
typedef struct {
    uint8_t  pass_count;          /* Anzahl Durchläufe (2-64) */
    uint8_t  confidence_threshold;/* Minimum-Confidence für "OK" */
    uint8_t  max_correction_bits; /* Max Bits für CRC-Korrektur */
    uint8_t  encoding;            /* Encoding-Typ */
    uint32_t data_rate;           /* Datenrate in bps */
    bool     use_crc_correction;  /* CRC-Korrektur aktivieren */
    bool     preserve_weak_bits;  /* Schwache Bits markieren */
} fluxstat_config_t;

/*============================================================================
 * Funktionen - Initialisierung
 *============================================================================*/

/**
 * @brief Initialisiert FluxStat-System
 */
int fluxstat_init(void);

/**
 * @brief Setzt Konfiguration
 */
int fluxstat_configure(const fluxstat_config_t *config);

/**
 * @brief Holt aktuelle Konfiguration
 */
int fluxstat_get_config(fluxstat_config_t *config);

/*============================================================================
 * Funktionen - Multi-Pass Capture
 *============================================================================*/

/**
 * @brief Startet Multi-Pass Capture für einen Track
 * @param drive Drive-Nummer
 * @param track Track-Nummer
 * @param head Head-Nummer
 */
int fluxstat_capture_start(uint8_t drive, uint8_t track, uint8_t head);

/**
 * @brief Bricht laufende Capture ab
 */
int fluxstat_capture_abort(void);

/**
 * @brief Prüft ob Capture läuft
 */
bool fluxstat_capture_busy(void);

/**
 * @brief Wartet auf Capture-Ende
 * @param timeout_ms Timeout in Millisekunden
 */
int fluxstat_capture_wait(uint32_t timeout_ms);

/**
 * @brief Holt Capture-Ergebnis
 */
int fluxstat_capture_result(fluxstat_capture_t *result);

/**
 * @brief Holt Capture-Fortschritt
 */
int fluxstat_capture_progress(uint8_t *current_pass, uint8_t *total_passes);

/*============================================================================
 * Funktionen - Histogram
 *============================================================================*/

/**
 * @brief Löscht Histogram
 */
int fluxstat_histogram_clear(void);

/**
 * @brief Holt Histogram-Statistiken
 */
int fluxstat_histogram_stats(fluxstat_histogram_t *hist);

/**
 * @brief Liest einzelnen Histogram-Bin
 */
int fluxstat_histogram_read_bin(uint8_t bin, uint16_t *count);

/**
 * @brief Erstellt Snapshot des aktuellen Histograms
 */
int fluxstat_histogram_snapshot(void);

/**
 * @brief Aktualisiert Histogram mit neuem Intervall
 */
void fluxstat_histogram_update(fluxstat_histogram_t *h, uint16_t interval);

/*============================================================================
 * Funktionen - Analyse
 *============================================================================*/

/**
 * @brief Analysiert gesamten Track
 */
int fluxstat_analyze_track(fluxstat_track_t *result);

/**
 * @brief Versucht einzelnen Sektor wiederherzustellen
 */
int fluxstat_recover_sector(uint8_t sector_num, fluxstat_sector_t *result);

/**
 * @brief Holt Bit-für-Bit Analyse
 * @param bit_offset Start-Bit-Position
 * @param count Anzahl Bits
 * @param bits Output-Array
 */
int fluxstat_get_bit_analysis(uint32_t bit_offset, uint32_t count,
                              fluxstat_bit_t *bits);

/**
 * @brief Berechnet Confidence für Daten-Buffer
 */
int fluxstat_calculate_confidence(const uint8_t *data, uint32_t length,
                                  uint8_t *min_conf, uint8_t *avg_conf);

/*============================================================================
 * Funktionen - Flux-Korrelation
 *============================================================================*/

/**
 * @brief Berechnet Confidence aus Korrelation
 */
uint8_t fluxstat_correlation_confidence(const flux_correlation_t *corr);

/**
 * @brief Korreliert Flux-Übergänge über alle Durchläufe
 */
int fluxstat_correlate_flux(uint32_t bit_position, flux_correlation_t *corr);

/*============================================================================
 * Funktionen - Utilities
 *============================================================================*/

/**
 * @brief Schätzt Datenrate aus Histogram
 */
int fluxstat_estimate_rate(uint32_t *rate_bps);

/**
 * @brief Holt Rohdaten eines Durchlaufs
 */
int fluxstat_get_pass_data(uint8_t pass, uint32_t *addr, uint32_t *size);

/**
 * @brief Gibt Klassifikations-Namen zurück
 */
const char *fluxstat_classification_name(uint8_t classification);

/**
 * @brief Berechnet RPM aus Index-Zeit
 */
uint32_t fluxstat_calculate_rpm(uint32_t index_clocks, uint32_t clk_mhz);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FLUX_STAT_H */
