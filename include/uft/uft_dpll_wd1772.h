/**
 * @file uft_dpll_wd1772.h
 * @brief WD1772-kompatible Digital Phase-Locked Loop Implementation
 * @version 3.1.4.001
 * 
 * Basiert auf dem WD1772 FDC DPLL-Algorithmus (US Patent 4,780,844)
 * 
 * Original: Jean Louis-Guerin (AUFIT Project, GPLv3)
 * Port: UFT Project 2025
 * 
 * Features:
 * - Exakte WD1772 DPLL Emulation
 * - Frequenz- und Phasenkorrektur
 * - DD/HD Unterstützung
 * - Noise-tolerant für beschädigte Disks
 * 
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_DPLL_WD1772_H
#define UFT_DPLL_WD1772_H

#include "uft_config.h"
#include <stdint.h>
#include <stdbool.h>

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * KONSTANTEN - WD1772 DPLL PARAMETER
 *============================================================================*/

/** @brief 8 MHz Clock = 125ns, wir nutzen 50ns Perioden (80 = 8MHz/0.1MHz) */
#define UFT_DPLL_CLK_PERIOD         80

/** @brief Phasenkompensations-Betrag (90 von 128 = ~70%) */
#define UFT_DPLL_PHASE_CORRECTION   90

/** @brief Unterer Wert für Phasenkorrektur (128 - 90 = 38) */
#define UFT_DPLL_LOW_CORRECTION     (128 - UFT_DPLL_PHASE_CORRECTION)

/** @brief Oberer Wert für Phasenkorrektur (128 + 90 = 218) */
#define UFT_DPLL_HIGH_CORRECTION    (128 + UFT_DPLL_PHASE_CORRECTION)

/** @brief Untere Grenze für Zähler (128 - 10% = 115) */
#define UFT_DPLL_LOW_STOP           115

/** @brief Obere Grenze für Zähler (128 + 10% = 141) */
#define UFT_DPLL_HIGH_STOP          141

/** @brief Maximale Flux-Lücke vor Reset (32µs = 256 * 125ns) */
#define UFT_DPLL_MAX_GAP_CLOCKS     256

/*============================================================================
 * DPLL ZUSTAND
 *============================================================================*/

/**
 * @brief WD1772 DPLL Zustandsstruktur
 * 
 * Hält den gesamten internen Zustand der DPLL für eine Track-Decodierung.
 * Kann für Multi-Pass-Decodierung gespeichert/wiederhergestellt werden.
 */
typedef struct uft_dpll_wd1772 {
    /* Timing */
    int32_t  current_time;      /**< Aktuelle Zeit in Nanosekunden */
    
    /* Frequenzkorrektur */
    bool     up;                /**< Zähler erhöhen */
    bool     down;              /**< Zähler verringern */
    
    /* Phasenkorrektur */
    bool     low;               /**< LOW_CORRECTION auswählen */
    bool     high;              /**< HIGH_CORRECTION auswählen */
    
    /* 11-Bit Addierer (modulo 2048) */
    int32_t  count;             /**< Perioden-Zähler (initialisiert auf 128) */
    int32_t  adder;             /**< 11-Bit Addierer */
    
    /* History für Frequenzkorrektur */
    int32_t  history;           /**< Lead/Lag MSB History (letzte 2 MSBs) */
    
    /* Korrektur-Beträge */
    int32_t  freq_amount;       /**< Frequenzkorrektur-Betrag */
    int32_t  phase_amount;      /**< Phasenkorrektur-Betrag */
    
    /* Konfiguration */
    bool     high_density;      /**< true = HD (1µs), false = DD (2µs) */
    
    /* Statistiken */
    uint32_t total_windows;     /**< Gesamt-Inspektionsfenster */
    uint32_t resets;            /**< Anzahl Resets (lange Lücken) */
    uint32_t phase_corrections; /**< Anzahl Phasenkorrekturen */
    uint32_t freq_corrections;  /**< Anzahl Frequenzkorrekturen */
    
} uft_dpll_wd1772_t;

/*============================================================================
 * DPLL KONFIGURATION
 *============================================================================*/

/**
 * @brief DPLL Konfigurationsstruktur
 */
typedef struct uft_dpll_config {
    bool     high_density;      /**< true = HD, false = DD */
    uint32_t clk_period_ns;     /**< Clock-Periode in ns (default: 80) */
    
    /* Optionale Overrides für Tuning */
    int32_t  phase_correction;  /**< Override für PHASE_CORRECTION (0 = default) */
    int32_t  low_stop;          /**< Override für LOW_STOP (0 = default) */
    int32_t  high_stop;         /**< Override für HIGH_STOP (0 = default) */
    
} uft_dpll_config_t;

/*============================================================================
 * DPLL ERGEBNIS
 *============================================================================*/

/**
 * @brief Ergebnis eines bitSpacing()-Aufrufs
 */
typedef struct uft_dpll_result {
    int32_t  num_windows;       /**< Anzahl 2µs Inspektionsfenster (2,3,4 normal) */
    int32_t  bit_width_ns;      /**< Aktuelle Bitbreite in Nanosekunden */
    bool     was_reset;         /**< true wenn DPLL zurückgesetzt wurde */
    
} uft_dpll_result_t;

/*============================================================================
 * API FUNKTIONEN
 *============================================================================*/

/**
 * @brief DPLL initialisieren
 * 
 * @param dpll Zeiger auf DPLL-Struktur
 * @param config Konfiguration (NULL für Defaults)
 */
UFT_API void uft_dpll_wd1772_init(uft_dpll_wd1772_t* dpll, 
                                   const uft_dpll_config_t* config);

/**
 * @brief DPLL zurücksetzen (für neuen Track)
 */
UFT_API void uft_dpll_wd1772_reset(uft_dpll_wd1772_t* dpll);

/**
 * @brief Bit-Spacing für einen Flux-Übergang berechnen
 * 
 * Dies ist die Kernfunktion der DPLL. Sie berechnet, in welchem
 * Inspektionsfenster der Datenpuls liegt und passt Frequenz/Phase an.
 * 
 * @param dpll DPLL-Zustand
 * @param data_time_ns Absolute Zeit des Flux-Übergangs in Nanosekunden
 * @return Anzahl 2µs Fenster (2=4µs, 3=6µs, 4=8µs für DD)
 * 
 * @note Für HD werden die Zeiten intern verdoppelt, Rückgabe ist gleich.
 */
UFT_API int32_t uft_dpll_wd1772_bit_spacing(uft_dpll_wd1772_t* dpll,
                                             int32_t data_time_ns);

/**
 * @brief Erweiterte Version mit Detailergebnis
 */
UFT_API uft_dpll_result_t uft_dpll_wd1772_bit_spacing_ex(uft_dpll_wd1772_t* dpll,
                                                          int32_t data_time_ns);

/**
 * @brief Aktuelle Bitbreite abfragen
 * 
 * @return Bitbreite in Nanosekunden (ca. 2000 für DD, 1000 für HD)
 */
UFT_API int32_t uft_dpll_wd1772_get_bit_width(const uft_dpll_wd1772_t* dpll);

/**
 * @brief Statistiken abrufen
 */
UFT_API void uft_dpll_wd1772_get_stats(const uft_dpll_wd1772_t* dpll,
                                        uint32_t* total_windows,
                                        uint32_t* resets,
                                        uint32_t* phase_corrections,
                                        uint32_t* freq_corrections);

/*============================================================================
 * FLUX-TO-MFM DECODER MIT DPLL
 *============================================================================*/

/**
 * @brief Flux-Daten zu MFM-Bitstream mit WD1772 DPLL decodieren
 * 
 * @param flux_times_ns Array mit Flux-Übergangszeiten in Nanosekunden
 * @param flux_count Anzahl Flux-Übergänge
 * @param mfm_out Output-Buffer für MFM-Bits (muss groß genug sein)
 * @param mfm_size Größe des Output-Buffers
 * @param config DPLL-Konfiguration (NULL für DD-Default)
 * @return Anzahl geschriebener MFM-Bits, oder 0 bei Fehler
 */
UFT_API size_t uft_dpll_flux_to_mfm(const int64_t* flux_times_ns,
                                     size_t flux_count,
                                     uint8_t* mfm_out,
                                     size_t mfm_size,
                                     const uft_dpll_config_t* config);

/*============================================================================
 * CONVENIENCE MAKROS
 *============================================================================*/

/** @brief Default DD DPLL erstellen */
#define UFT_DPLL_DD_DEFAULT() \
    (uft_dpll_config_t){ .high_density = false, .clk_period_ns = 80 }

/** @brief Default HD DPLL erstellen */
#define UFT_DPLL_HD_DEFAULT() \
    (uft_dpll_config_t){ .high_density = true, .clk_period_ns = 80 }

#ifdef __cplusplus
}
#endif

#endif /* UFT_DPLL_WD1772_H */
