/**
 * @file uft_gui_params.h
 * @brief Unified GUI Parameter Mapping für alle PLL/Decoder Algorithmen
 * @version 3.1.4.004
 *
 * Dieser Header mappt alle internen Decoder-Parameter auf GUI-freundliche
 * Strukturen mit konsistenten Einheiten und Bereichen.
 *
 * **Unterstützte Algorithmen:**
 * - WD1772 DPLL (uft_dpll_wd1772.h)
 * - MAME-style PLL (bbc-fdc)
 * - Simple PLL (uft_pll.h)
 * - Adaptive Decoder (uft_adaptive_decoder.h)
 * - P64 Range Decoder
 *
 * **GUI-Integration:**
 * - Alle Werte haben Prozent- oder µs-Einheiten
 * - Definierte Min/Max/Default-Bereiche
 * - Presets für verschiedene Disk-Typen
 * - Bidirektionales Mapping (GUI ↔ Internal)
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_GUI_PARAMS_H

#define UFT_GUI_PARAMS_H

#include <stddef.h>  /* for size_t */
#include <stdbool.h>

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * GUI PARAMETER BEREICHE
 *============================================================================*/

/** @brief Prozent als float (0.0 - 100.0) */
typedef float uft_percent_t;

/** @brief Zeit in Mikrosekunden als float */
typedef float uft_usec_t;

/** @brief Zeit in Nanosekunden als int */
typedef int32_t uft_nsec_t;

/*============================================================================
 * PLL PARAMETER GUI STRUKTUR
 *============================================================================*/

/**
 * @brief Universelle PLL-Parameter für GUI
 *
 * Alle PLL-Algorithmen (WD1772, MAME, Simple) werden auf diese
 * Struktur gemappt für einheitliche GUI-Bedienung.
 */
typedef struct uft_gui_pll_params {
    /*--------------------------------------------------------------------
     * Phasen-Korrektur (wie schnell folgt PLL einer Transition)
     *--------------------------------------------------------------------*/
    uft_percent_t phase_adjust;     /**< 0-100%, Default: 65% (MAME), 70% (WD1772) */
    
    /*--------------------------------------------------------------------
     * Frequenz-Korrektur (wie schnell passt PLL die Zellbreite an)
     *--------------------------------------------------------------------*/
    uft_percent_t freq_adjust;      /**< 0-100%, Default: 5% (MAME) */
    
    /*--------------------------------------------------------------------
     * Zell-Timing Grenzen
     *--------------------------------------------------------------------*/
    uft_percent_t period_min;       /**< Minimale Periode, Default: 75% */
    uft_percent_t period_max;       /**< Maximale Periode, Default: 125% */
    
    /*--------------------------------------------------------------------
     * Basis-Zellbreite (abhängig von Format/Dichte)
     *--------------------------------------------------------------------*/
    uft_usec_t    bitcell_us;       /**< Nominale Zellbreite in µs */
    
    /*--------------------------------------------------------------------
     * Erweiterte WD1772-Parameter (optional, für Experten-Modus)
     *--------------------------------------------------------------------*/
    int32_t       wd1772_low_stop;  /**< WD1772: Lower bound (default 115) */
    int32_t       wd1772_high_stop; /**< WD1772: Upper bound (default 141) */
    
} uft_gui_pll_params_t;

/*============================================================================
 * ADAPTIVE DECODER PARAMETER GUI STRUKTUR
 *============================================================================*/

/**
 * @brief Adaptive Decoder Parameter für GUI
 */
typedef struct uft_gui_adaptive_params {
    /*--------------------------------------------------------------------
     * MFM Timing Thresholds (in µs)
     *--------------------------------------------------------------------*/
    uft_usec_t    thresh_4us;       /**< 4µs Zelle (01), Default: 2.0 */
    uft_usec_t    thresh_6us;       /**< 6µs Zelle (001), Default: 3.0 */
    uft_usec_t    thresh_8us;       /**< 8µs Zelle (0001), Default: 4.0 */
    
    /*--------------------------------------------------------------------
     * Adaptions-Geschwindigkeit
     *--------------------------------------------------------------------*/
    uft_percent_t adapt_rate;       /**< 0-100%, Default: 25% (rate_of_change=4) */
    
    /*--------------------------------------------------------------------
     * Lowpass-Filter
     *--------------------------------------------------------------------*/
    int32_t       lowpass_radius;   /**< 0-1024, Default: 100 */
    
    /*--------------------------------------------------------------------
     * Threshold-Offset für Fine-Tuning
     *--------------------------------------------------------------------*/
    uft_nsec_t    offset_ns;        /**< Offset in ns, Default: 0 */
    
} uft_gui_adaptive_params_t;

/*============================================================================
 * GCR DECODER PARAMETER (C64/Apple)
 *============================================================================*/

/**
 * @brief GCR Timing Buckets für GUI
 */
typedef struct uft_gui_gcr_params {
    /*--------------------------------------------------------------------
     * C64 GCR Zone-spezifische Buckets (in Samples @ 12.5MHz)
     *--------------------------------------------------------------------*/
    int32_t       bucket_1;         /**< Single '1' max samples */
    int32_t       bucket_01;        /**< '01' pattern max samples */
    
    /*--------------------------------------------------------------------
     * Als Prozent der Zellbreite (GUI-freundlicher)
     *--------------------------------------------------------------------*/
    uft_percent_t bucket_1_pct;     /**< Default: ~100% (genau 1 Zelle) */
    uft_percent_t bucket_01_pct;    /**< Default: ~157% (1.5 Zellen) */
    
    /*--------------------------------------------------------------------
     * Automatische Zonen-Erkennung
     *--------------------------------------------------------------------*/
    bool          auto_zone;        /**< Automatisch nach Track anpassen */
    int32_t       force_zone;       /**< Manuelle Zone (0-3), nur wenn !auto_zone */
    
} uft_gui_gcr_params_t;

/*============================================================================
 * FORMAT-PRESETS
 *============================================================================*/

/**
 * @brief Vordefinierte Parameter-Sets für verschiedene Formate
 */
typedef enum {
    /* MFM Formate */
    UFT_PRESET_IBM_DD,              /**< IBM PC 720K/360K Double Density */
    UFT_PRESET_IBM_HD,              /**< IBM PC 1.44M/1.2M High Density */
    UFT_PRESET_AMIGA_DD,            /**< Amiga 880K Double Density */
    UFT_PRESET_AMIGA_HD,            /**< Amiga 1.76M High Density */
    UFT_PRESET_ATARI_ST,            /**< Atari ST */
    
    /* FM Formate */
    UFT_PRESET_BBC_DFS,             /**< BBC Micro DFS (FM) */
    UFT_PRESET_TRS80,               /**< TRS-80 (FM) */
    
    /* GCR Formate */
    UFT_PRESET_C64_1541,            /**< C64 1541 Drive */
    UFT_PRESET_APPLE_DOS33,         /**< Apple II DOS 3.3 */
    UFT_PRESET_APPLE_PRODOS,        /**< Apple II ProDOS */
    UFT_PRESET_MAC_400K,            /**< Macintosh 400K GCR */
    UFT_PRESET_MAC_800K,            /**< Macintosh 800K GCR */
    
    /* Spezial */
    UFT_PRESET_DIRTY_DUMP,          /**< Beschädigte Disks (weite Toleranzen) */
    UFT_PRESET_COPY_PROTECTION,     /**< Kopierschutz-Analyse */
    
    UFT_PRESET_COUNT
} uft_preset_t;

/*============================================================================
 * GUI ↔ INTERNAL MAPPING FUNKTIONEN
 *============================================================================*/

/**
 * @brief GUI PLL Parameter zu WD1772 DPLL Config konvertieren
 */
void uft_gui_pll_to_wd1772(const uft_gui_pll_params_t* gui,
                           struct uft_dpll_config* dpll);

/**
 * @brief WD1772 DPLL Config zu GUI PLL Parameter konvertieren
 */
void uft_wd1772_to_gui_pll(const struct uft_dpll_config* dpll,
                           uft_gui_pll_params_t* gui);

/**
 * @brief GUI PLL Parameter zu MAME-style PLL konvertieren
 *
 * MAME PLL verwendet:
 * - period_adjust = 5%
 * - phase_adjust = 65%
 * - min_period = 75%
 * - max_period = 125%
 */
void uft_gui_pll_to_mame(const uft_gui_pll_params_t* gui,
                         float* period_adjust,
                         float* phase_adjust,
                         float* min_period,
                         float* max_period);

/**
 * @brief GUI Adaptive Parameter zu uft_adaptive_config_t konvertieren
 */
void uft_gui_adaptive_to_internal(const uft_gui_adaptive_params_t* gui,
                                   struct uft_adaptive_config* config);

/**
 * @brief Internal Adaptive Config zu GUI Parameter konvertieren
 */
void uft_internal_to_gui_adaptive(const struct uft_adaptive_config* config,
                                   uft_gui_adaptive_params_t* gui);

/*============================================================================
 * PRESET FUNKTIONEN
 *============================================================================*/

/**
 * @brief PLL Preset laden
 * @param preset Preset-ID
 * @param params Output: GUI Parameter
 */
void uft_preset_get_pll(uft_preset_t preset, uft_gui_pll_params_t* params);

/**
 * @brief Adaptive Decoder Preset laden
 */
void uft_preset_get_adaptive(uft_preset_t preset, uft_gui_adaptive_params_t* params);

/**
 * @brief GCR Preset laden
 */
void uft_preset_get_gcr(uft_preset_t preset, uft_gui_gcr_params_t* params);

/**
 * @brief Preset-Name abrufen
 */
const char* uft_preset_name(uft_preset_t preset);

/**
 * @brief Preset-Beschreibung abrufen
 */
const char* uft_preset_description(uft_preset_t preset);

/*============================================================================
 * VORSCHAU / LIVE-UPDATE
 *============================================================================*/

/**
 * @brief Parameter-Änderung für Live-Vorschau validieren
 *
 * @param params Aktuelle GUI-Parameter
 * @param changed_field Name des geänderten Felds
 * @param new_value Neuer Wert
 * @param error_msg Output: Fehlermeldung falls ungültig
 * @return true wenn gültig
 */
bool uft_gui_validate_param(const uft_gui_pll_params_t* params,
                            const char* changed_field,
                            float new_value,
                            char* error_msg);

/**
 * @brief Empfohlene Parameter-Werte basierend auf Track-Analyse
 *
 * Analysiert einen Track und schlägt optimale Parameter vor.
 */
typedef struct uft_param_suggestion {
    uft_gui_pll_params_t pll;
    uft_gui_adaptive_params_t adaptive;
    float confidence;               /**< 0.0 - 1.0 */
    char reason[256];               /**< Begründung */
} uft_param_suggestion_t;

bool uft_analyze_and_suggest(const uint8_t* flux_data,
                             size_t flux_len,
                             uft_param_suggestion_t* suggestion);

/*============================================================================
 * GUI WIDGET HELPER
 *============================================================================*/

/**
 * @brief Slider-Konfiguration für einen Parameter
 */
typedef struct uft_slider_config {
    float min_value;
    float max_value;
    float default_value;
    float step;
    const char* label;
    const char* unit;
    const char* tooltip;
} uft_slider_config_t;

/**
 * @brief Slider-Konfiguration für PLL Phase Adjust abrufen
 */
uft_slider_config_t uft_gui_slider_pll_phase(void);

/**
 * @brief Slider-Konfiguration für PLL Freq Adjust abrufen
 */
uft_slider_config_t uft_gui_slider_pll_freq(void);

/**
 * @brief Slider-Konfiguration für Adaptive Rate abrufen
 */
uft_slider_config_t uft_gui_slider_adapt_rate(void);

/**
 * @brief Slider-Konfiguration für Lowpass Radius abrufen
 */
uft_slider_config_t uft_gui_slider_lowpass(void);

/*============================================================================
 * DEFAULT WERTE (als Konstanten für GUI)
 *============================================================================*/

/** @name PLL Defaults */
/**@{*/
#define UFT_GUI_PLL_PHASE_DEFAULT       65.0f   /**< 65% (MAME) */
#define UFT_GUI_PLL_PHASE_MIN           10.0f
#define UFT_GUI_PLL_PHASE_MAX           95.0f

#define UFT_GUI_PLL_FREQ_DEFAULT        5.0f    /**< 5% (MAME) */
#define UFT_GUI_PLL_FREQ_MIN            0.5f
#define UFT_GUI_PLL_FREQ_MAX            20.0f

#define UFT_GUI_PLL_PERIOD_MIN_DEFAULT  75.0f   /**< 75% */
#define UFT_GUI_PLL_PERIOD_MAX_DEFAULT  125.0f  /**< 125% */
/**@}*/

/** @name WD1772 Specific */
/**@{*/
#define UFT_GUI_WD1772_PHASE_DEFAULT    70.0f   /**< 70% (~90/128) */
#define UFT_GUI_WD1772_LOW_STOP_DEFAULT 115
#define UFT_GUI_WD1772_HIGH_STOP_DEFAULT 141
/**@}*/

/** @name Adaptive Decoder Defaults */
/**@{*/
#define UFT_GUI_ADAPT_RATE_DEFAULT      25.0f   /**< 25% (rate_of_change=4) */
#define UFT_GUI_ADAPT_RATE_MIN          5.0f
#define UFT_GUI_ADAPT_RATE_MAX          100.0f

#define UFT_GUI_LOWPASS_DEFAULT         100
#define UFT_GUI_LOWPASS_MIN             0
#define UFT_GUI_LOWPASS_MAX             1024
/**@}*/

/** @name Bitcell Timing Defaults (µs) */
/**@{*/
#define UFT_GUI_BITCELL_DD              2.0f    /**< DD: 2µs */
#define UFT_GUI_BITCELL_HD              1.0f    /**< HD: 1µs */
#define UFT_GUI_BITCELL_FM              4.0f    /**< FM: 4µs */
#define UFT_GUI_BITCELL_C64_ZONE3       3.25f   /**< C64 Zone 3: ~3.25µs */
#define UFT_GUI_BITCELL_C64_ZONE0       4.0f    /**< C64 Zone 0: 4µs */
#define UFT_GUI_BITCELL_APPLE           4.0f    /**< Apple II: 4µs */
/**@}*/

/*============================================================================
 * KONVERTIERUNGS-HELPER
 *============================================================================*/

/**
 * @brief Prozent (0-100) zu Float-Faktor (0.0-1.0)
 */
static inline float uft_percent_to_factor(uft_percent_t pct) {
    return pct / 100.0f;
}

/**
 * @brief Float-Faktor (0.0-1.0) zu Prozent (0-100)
 */
static inline uft_percent_t uft_factor_to_percent(float factor) {
    return factor * 100.0f;
}

/**
 * @brief µs zu ns
 */
static inline uft_nsec_t uft_us_to_ns(uft_usec_t us) {
    return (uft_nsec_t)(us * 1000.0f);
}

/**
 * @brief ns zu µs
 */
static inline uft_usec_t uft_ns_to_us(uft_nsec_t ns) {
    return (uft_usec_t)ns / 1000.0f;
}

/**
 * @brief Rate-of-Change (4-16) zu Prozent (100-25)
 * Inverse Beziehung: größer = langsamer
 */
static inline uft_percent_t uft_roc_to_percent(float roc) {
    if (roc <= 0) return 100.0f;
    return 100.0f / roc;
}

/**
 * @brief Prozent (100-25) zu Rate-of-Change (1-4)
 */
static inline float uft_percent_to_roc(uft_percent_t pct) {
    if (pct <= 0) return 16.0f;
    return 100.0f / pct;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_GUI_PARAMS_H */
