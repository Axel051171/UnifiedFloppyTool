/**
 * @file uft_dpll_wd1772.c
 * @brief WD1772-kompatible Digital Phase-Locked Loop Implementation
 * @version 3.1.4.001
 * 
 * Basiert auf dem WD1772 FDC DPLL-Algorithmus (US Patent 4,780,844)
 * 
 * Original: Jean Louis-Guerin (AUFIT Project, GPLv3)
 * Port: UFT Project 2025
 * 
 * SPDX-License-Identifier: MIT
 */

#include "uft/uft_dpll_wd1772.h"
#include <string.h>

/*============================================================================
 * INTERNE KONSTANTEN
 *============================================================================*/

/* Frequenz-Korrektur Lookup-Tabellen (aus Patent) */

/* Error = 2: Starke Korrektur */
static const int8_t g_freq_error2[8] = {
    0x4,  /* 000: sehr schnell → +4 */
    0x3,  /* 001: schnell → +3 */
    0x2,  /* 010: leicht schnell → +2 */
    0x1,  /* 011: minimal schnell → +1 */
    0x9,  /* 100: minimal langsam → -1 (0x9 = 1001, MSB=1 → down) */
    0xA,  /* 101: leicht langsam → -2 */
    0xB,  /* 110: langsam → -3 */
    0xC   /* 111: sehr langsam → -4 */
};

/* Error = 1: Moderate Korrektur */
static const int8_t g_freq_error1[8] = {
    0x3,  /* 000: schnell → +3 */
    0x2,  /* 001: leicht schnell → +2 */
    0x1,  /* 010: minimal schnell → +1 */
    0x0,  /* 011: OK → 0 */
    0x8,  /* 100: OK → 0 (MSB ignoriert wenn Amount=0) */
    0x9,  /* 101: minimal langsam → -1 */
    0xA,  /* 110: leicht langsam → -2 */
    0xB   /* 111: langsam → -3 */
};

/* Phasen-Korrektur Lookup-Tabelle */
static const int8_t g_phase_correction[8] = {
    0x4,  /* 000: große Voreilung → high select, 4 Zyklen */
    0x3,  /* 001: Voreilung → high select, 3 Zyklen */
    0x2,  /* 010: mittlere Voreilung → high select, 2 Zyklen */
    0x1,  /* 011: kleine Voreilung → high select, 1 Zyklus */
    0x9,  /* 100: kleine Nacheilung → low select, 1 Zyklus */
    0xA,  /* 101: mittlere Nacheilung → low select, 2 Zyklen */
    0xB,  /* 110: Nacheilung → low select, 3 Zyklen */
    0xC   /* 111: große Nacheilung → low select, 4 Zyklen */
};

/*============================================================================
 * INITIALISIERUNG
 *============================================================================*/

void uft_dpll_wd1772_init(uft_dpll_wd1772_t* dpll, const uft_dpll_config_t* config)
{
    if (!dpll) return;
    
    memset(dpll, 0, sizeof(*dpll));
    
    if (config) {
        dpll->high_density = config->high_density;
    }
    
    uft_dpll_wd1772_reset(dpll);
}

void uft_dpll_wd1772_reset(uft_dpll_wd1772_t* dpll)
{
    if (!dpll) return;
    
    dpll->up = false;
    dpll->down = false;
    dpll->count = 128;           /* Perioden-Zähler initial auf 128 */
    dpll->adder = 0;             /* 11-Bit Addierer zurücksetzen */
    dpll->low = false;
    dpll->high = false;
    dpll->history = 0;           /* History zurücksetzen */
    dpll->freq_amount = 0;
    dpll->phase_amount = 0;
    
    /* current_time wird NICHT zurückgesetzt - behält Track-Position */
}

/*============================================================================
 * KERN-ALGORITHMUS: bitSpacing()
 *============================================================================*/

int32_t uft_dpll_wd1772_bit_spacing(uft_dpll_wd1772_t* dpll, int32_t data_time_ns)
{
    uft_dpll_result_t result = uft_dpll_wd1772_bit_spacing_ex(dpll, data_time_ns);
    return result.num_windows;
}

uft_dpll_result_t uft_dpll_wd1772_bit_spacing_ex(uft_dpll_wd1772_t* dpll,
                                                  int32_t data_time_ns)
{
    uft_dpll_result_t result = {0, 0, false};
    
    if (!dpll) {
        return result;
    }
    
    bool data_not_found = true;
    bool read_pulse = false;
    int32_t error = 0;
    int32_t num_windows = 0;
    
    /* Für HD: Zeit verdoppeln (Clock ist gleich, aber Zellen halb so lang) */
    if (dpll->high_density) {
        data_time_ns *= 2;
    }
    
    /* Prüfung auf sehr lange Flux-Lücke (z.B. NFA, Kopierschutz) */
    if ((data_time_ns - dpll->current_time) > (UFT_DPLL_MAX_GAP_CLOCKS * UFT_DPLL_CLK_PERIOD)) {
        /* Lücke zu groß - DPLL zurücksetzen und Anzahl Fenster schätzen */
        int32_t nw = (data_time_ns - dpll->current_time) / (16 * UFT_DPLL_CLK_PERIOD);
        dpll->current_time = data_time_ns;
        uft_dpll_wd1772_reset(dpll);
        dpll->resets++;
        
        result.num_windows = nw;
        result.was_reset = true;
        result.bit_width_ns = uft_dpll_wd1772_get_bit_width(dpll);
        return result;
    }
    
    /* Hauptschleife: Suche Datenpuls in Inspektionsfenstern */
    do {
        num_windows++;
        dpll->total_windows++;
        
        /* Innere Schleife: Durchlaufe bis 11-Bit Addierer überläuft */
        do {
            dpll->current_time += UFT_DPLL_CLK_PERIOD;
            
            /* Lesepuls nur für einen Durchlauf aktiv */
            if (read_pulse) {
                read_pulse = false;
            }
            
            /* Prüfe ob Datenpuls im aktuellen Fenster */
            if (dpll->current_time >= data_time_ns) {
                if (data_not_found) {
                    read_pulse = true;
                }
                data_not_found = false;
            }
            
            /* ============================================ */
            /* Frequenzkorrektur anwenden                   */
            /* ============================================ */
            if (dpll->up && (dpll->count < UFT_DPLL_HIGH_STOP)) {
                dpll->count++;
                dpll->freq_corrections++;
            }
            if (dpll->down && (dpll->count > UFT_DPLL_LOW_STOP)) {
                dpll->count--;
                dpll->freq_corrections++;
            }
            
            /* ============================================ */
            /* Addierer aktualisieren basierend auf Phase   */
            /* ============================================ */
            if (dpll->low) {
                dpll->adder += UFT_DPLL_LOW_CORRECTION;
            } else if (dpll->high) {
                dpll->adder += UFT_DPLL_HIGH_CORRECTION;
            } else {
                dpll->adder += dpll->count;
            }
            
            /* ============================================ */
            /* Bei Datenpuls: Korrekturbeträge berechnen    */
            /* ============================================ */
            if (read_pulse) {
                /* --- Frequenzkorrektur berechnen --- */
                
                /* Error-Level basierend auf Addierer-MSB und History */
                int32_t adder_msb = (dpll->adder & 0x400) ? 1 : 0;
                
                if (adder_msb == 0) {
                    /* Addierer MSB = 0 */
                    switch (dpll->history) {
                        case 0: error = 2; break;  /* 000: sehr schnell */
                        case 1: error = 1; break;  /* 001: schnell */
                        case 2: error = 0; break;  /* 010: OK */
                        case 3: error = 0; break;  /* 011: jetzt angepasst */
                    }
                } else {
                    /* Addierer MSB = 1 */
                    switch (dpll->history) {
                        case 0: error = 0; break;  /* 100: jetzt angepasst */
                        case 1: error = 0; break;  /* 101: OK */
                        case 2: error = 1; break;  /* 110: langsam */
                        case 3: error = 2; break;  /* 111: sehr langsam */
                    }
                }
                
                /* History aktualisieren */
                dpll->history = ((dpll->adder & 0x400) >> 9) | ((dpll->history >> 1) & 0x7F);
                
                /* Frequenz-Korrekturbetrag aus Tabelle */
                int32_t adder_msbs = (dpll->adder >> 8) & 0x7;
                switch (error) {
                    case 2:
                        dpll->freq_amount = g_freq_error2[adder_msbs];
                        break;
                    case 1:
                        dpll->freq_amount = g_freq_error1[adder_msbs];
                        break;
                    case 0:
                    default:
                        dpll->freq_amount = 0;
                        break;
                }
                
                /* --- Phasenkorrektur berechnen --- */
                dpll->phase_amount = g_phase_correction[adder_msbs];
                dpll->phase_corrections++;
                
            } else {
                /* Kein Datenpuls - Korrekturbeträge dekrementieren */
                if ((dpll->freq_amount & 0x7) != 0) {
                    dpll->freq_amount--;
                }
                if ((dpll->phase_amount & 0x7) != 0) {
                    dpll->phase_amount--;
                }
            }
            
            /* ============================================ */
            /* Korrektur-Flags setzen                       */
            /* ============================================ */
            
            /* Frequenzkorrektur-Flags */
            if ((dpll->freq_amount & 0x07) != 0) {
                if ((dpll->freq_amount & 0x8) != 0) {
                    /* MSB = 1 → verlangsamen (count erhöhen) */
                    dpll->up = false;
                    dpll->down = true;
                } else {
                    /* MSB = 0 → beschleunigen (count verringern) */
                    dpll->up = true;
                    dpll->down = false;
                }
            } else {
                dpll->up = false;
                dpll->down = false;
            }
            
            /* Phasenkorrektur-Flags */
            if ((dpll->phase_amount & 0x7) != 0) {
                if ((dpll->phase_amount & 0x8) != 0) {
                    /* MSB = 1 → Nacheilung → LOW_CORRECTION */
                    dpll->low = true;
                    dpll->high = false;
                } else {
                    /* MSB = 0 → Voreilung → HIGH_CORRECTION */
                    dpll->high = true;
                    dpll->low = false;
                }
            } else {
                dpll->high = false;
                dpll->low = false;
            }
            
        } while (dpll->adder < 2048);  /* Bis Addierer überläuft */
        
        dpll->adder -= 2048;  /* Überlauf behandeln */
        
    } while (data_not_found);  /* Bis Datenpuls gefunden */
    
    result.num_windows = num_windows;
    result.was_reset = false;
    result.bit_width_ns = uft_dpll_wd1772_get_bit_width(dpll);
    
    return result;
}

/*============================================================================
 * HILFSFUNKTIONEN
 *============================================================================*/

int32_t uft_dpll_wd1772_get_bit_width(const uft_dpll_wd1772_t* dpll)
{
    if (!dpll || dpll->count == 0) {
        return 2000;  /* Default DD */
    }
    
    /* clock = (2048 * 125ns) / period_count */
    int32_t clock_ns = 256000 / dpll->count;
    
    if (dpll->high_density) {
        return clock_ns / 2;
    }
    
    return clock_ns;
}

void uft_dpll_wd1772_get_stats(const uft_dpll_wd1772_t* dpll,
                                uint32_t* total_windows,
                                uint32_t* resets,
                                uint32_t* phase_corrections,
                                uint32_t* freq_corrections)
{
    if (!dpll) return;
    
    if (total_windows)      *total_windows = dpll->total_windows;
    if (resets)             *resets = dpll->resets;
    if (phase_corrections)  *phase_corrections = dpll->phase_corrections;
    if (freq_corrections)   *freq_corrections = dpll->freq_corrections;
}

/*============================================================================
 * HIGH-LEVEL API: FLUX-TO-MFM
 *============================================================================*/

size_t uft_dpll_flux_to_mfm(const int64_t* flux_times_ns,
                            size_t flux_count,
                            uint8_t* mfm_out,
                            size_t mfm_size,
                            const uft_dpll_config_t* config)
{
    if (!flux_times_ns || flux_count < 2 || !mfm_out || mfm_size == 0) {
        return 0;
    }
    
    uft_dpll_wd1772_t dpll;
    uft_dpll_wd1772_init(&dpll, config);
    
    /* Startzeit setzen */
    dpll.current_time = (int32_t)flux_times_ns[0];
    
    size_t mfm_index = 0;
    size_t bit_index = 0;
    uint8_t current_byte = 0;
    
    /* Jeder Flux-Übergang */
    for (size_t i = 1; i < flux_count && mfm_index < mfm_size; i++) {
        int32_t delta_ns = (int32_t)(flux_times_ns[i] - flux_times_ns[i-1]);
        int32_t num_windows = uft_dpll_wd1772_bit_spacing(&dpll, (int32_t)flux_times_ns[i]);
        
        /* MFM-Bits generieren: 1 gefolgt von (num_windows-1) Nullen */
        /* Beispiel: num_windows=3 → "100" */
        for (int32_t w = 0; w < num_windows && mfm_index < mfm_size; w++) {
            uint8_t bit = (w == 0) ? 1 : 0;
            
            current_byte = (current_byte << 1) | bit;
            bit_index++;
            
            if (bit_index == 8) {
                mfm_out[mfm_index++] = current_byte;
                current_byte = 0;
                bit_index = 0;
            }
        }
    }
    
    /* Letzte Bits schreiben falls nicht byte-aligned */
    if (bit_index > 0 && mfm_index < mfm_size) {
        current_byte <<= (8 - bit_index);
        mfm_out[mfm_index++] = current_byte;
    }
    
    return mfm_index;
}
