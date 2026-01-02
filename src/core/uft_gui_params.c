/**
 * @file uft_gui_params.c
 * @brief Implementierung der GUI Parameter Mappings und Presets
 * @version 3.1.4.004
 */

#include "uft/uft_gui_params.h"
#include "uft/uft_dpll_wd1772.h"
#include "uft/uft_adaptive_decoder.h"
#include <string.h>
#include <math.h>

/*============================================================================
 * PRESET DEFINITIONEN
 *============================================================================*/

static const struct {
    uft_preset_t id;
    const char* name;
    const char* description;
    uft_gui_pll_params_t pll;
    uft_gui_adaptive_params_t adaptive;
    uft_gui_gcr_params_t gcr;
} g_presets[] = {
    
    /*------------------------------------------------------------------------
     * IBM PC DD (720K/360K)
     *------------------------------------------------------------------------*/
    {
        .id = UFT_PRESET_IBM_DD,
        .name = "IBM PC DD",
        .description = "IBM PC 720K/360K Double Density (MFM, 250 kbit/s)",
        .pll = {
            .phase_adjust = 65.0f,
            .freq_adjust = 5.0f,
            .period_min = 75.0f,
            .period_max = 125.0f,
            .bitcell_us = 2.0f,
            .wd1772_low_stop = 115,
            .wd1772_high_stop = 141,
        },
        .adaptive = {
            .thresh_4us = 2.0f,
            .thresh_6us = 3.0f,
            .thresh_8us = 4.0f,
            .adapt_rate = 25.0f,
            .lowpass_radius = 100,
            .offset_ns = 0,
        },
        .gcr = { 0 },  /* Nicht für GCR */
    },
    
    /*------------------------------------------------------------------------
     * IBM PC HD (1.44M/1.2M)
     *------------------------------------------------------------------------*/
    {
        .id = UFT_PRESET_IBM_HD,
        .name = "IBM PC HD",
        .description = "IBM PC 1.44M/1.2M High Density (MFM, 500 kbit/s)",
        .pll = {
            .phase_adjust = 65.0f,
            .freq_adjust = 5.0f,
            .period_min = 75.0f,
            .period_max = 125.0f,
            .bitcell_us = 1.0f,
            .wd1772_low_stop = 115,
            .wd1772_high_stop = 141,
        },
        .adaptive = {
            .thresh_4us = 1.0f,
            .thresh_6us = 1.5f,
            .thresh_8us = 2.0f,
            .adapt_rate = 25.0f,
            .lowpass_radius = 50,
            .offset_ns = 0,
        },
        .gcr = { 0 },
    },
    
    /*------------------------------------------------------------------------
     * Amiga DD (880K)
     *------------------------------------------------------------------------*/
    {
        .id = UFT_PRESET_AMIGA_DD,
        .name = "Amiga DD",
        .description = "Amiga 880K Double Density (MFM, 250 kbit/s)",
        .pll = {
            .phase_adjust = 70.0f,  /* Amiga ist etwas strenger */
            .freq_adjust = 4.0f,
            .period_min = 80.0f,
            .period_max = 120.0f,
            .bitcell_us = 2.0f,
            .wd1772_low_stop = 118,
            .wd1772_high_stop = 138,
        },
        .adaptive = {
            .thresh_4us = 2.0f,
            .thresh_6us = 3.0f,
            .thresh_8us = 4.0f,
            .adapt_rate = 20.0f,
            .lowpass_radius = 80,
            .offset_ns = 0,
        },
        .gcr = { 0 },
    },
    
    /*------------------------------------------------------------------------
     * Amiga HD (1.76M)
     *------------------------------------------------------------------------*/
    {
        .id = UFT_PRESET_AMIGA_HD,
        .name = "Amiga HD",
        .description = "Amiga 1.76M High Density (MFM, 500 kbit/s)",
        .pll = {
            .phase_adjust = 70.0f,
            .freq_adjust = 4.0f,
            .period_min = 80.0f,
            .period_max = 120.0f,
            .bitcell_us = 1.0f,
            .wd1772_low_stop = 118,
            .wd1772_high_stop = 138,
        },
        .adaptive = {
            .thresh_4us = 1.0f,
            .thresh_6us = 1.5f,
            .thresh_8us = 2.0f,
            .adapt_rate = 20.0f,
            .lowpass_radius = 40,
            .offset_ns = 0,
        },
        .gcr = { 0 },
    },
    
    /*------------------------------------------------------------------------
     * Atari ST
     *------------------------------------------------------------------------*/
    {
        .id = UFT_PRESET_ATARI_ST,
        .name = "Atari ST",
        .description = "Atari ST DD/HD (MFM, WD1772 FDC)",
        .pll = {
            .phase_adjust = 70.0f,  /* WD1772 Original */
            .freq_adjust = 5.0f,
            .period_min = 75.0f,
            .period_max = 125.0f,
            .bitcell_us = 2.0f,
            .wd1772_low_stop = 115,
            .wd1772_high_stop = 141,
        },
        .adaptive = {
            .thresh_4us = 2.0f,
            .thresh_6us = 3.0f,
            .thresh_8us = 4.0f,
            .adapt_rate = 25.0f,
            .lowpass_radius = 100,
            .offset_ns = 0,
        },
        .gcr = { 0 },
    },
    
    /*------------------------------------------------------------------------
     * BBC Micro DFS (FM)
     *------------------------------------------------------------------------*/
    {
        .id = UFT_PRESET_BBC_DFS,
        .name = "BBC DFS",
        .description = "BBC Micro DFS Single Density (FM, 125 kbit/s)",
        .pll = {
            .phase_adjust = 65.0f,
            .freq_adjust = 5.0f,
            .period_min = 75.0f,
            .period_max = 125.0f,
            .bitcell_us = 4.0f,  /* FM: doppelte Zellbreite */
            .wd1772_low_stop = 115,
            .wd1772_high_stop = 141,
        },
        .adaptive = {
            .thresh_4us = 4.0f,  /* FM Buckets */
            .thresh_6us = 6.0f,
            .thresh_8us = 8.0f,
            .adapt_rate = 30.0f,
            .lowpass_radius = 150,
            .offset_ns = 0,
        },
        .gcr = { 0 },
    },
    
    /*------------------------------------------------------------------------
     * TRS-80 (FM)
     *------------------------------------------------------------------------*/
    {
        .id = UFT_PRESET_TRS80,
        .name = "TRS-80",
        .description = "TRS-80 Single Density (FM)",
        .pll = {
            .phase_adjust = 65.0f,
            .freq_adjust = 5.0f,
            .period_min = 75.0f,
            .period_max = 125.0f,
            .bitcell_us = 4.0f,
            .wd1772_low_stop = 115,
            .wd1772_high_stop = 141,
        },
        .adaptive = {
            .thresh_4us = 4.0f,
            .thresh_6us = 6.0f,
            .thresh_8us = 8.0f,
            .adapt_rate = 30.0f,
            .lowpass_radius = 150,
            .offset_ns = 0,
        },
        .gcr = { 0 },
    },
    
    /*------------------------------------------------------------------------
     * Commodore 64 / 1541
     *------------------------------------------------------------------------*/
    {
        .id = UFT_PRESET_C64_1541,
        .name = "C64 1541",
        .description = "Commodore 64 1541 Drive (GCR, 250-307 kbit/s)",
        .pll = {
            .phase_adjust = 65.0f,
            .freq_adjust = 5.0f,
            .period_min = 80.0f,
            .period_max = 120.0f,
            .bitcell_us = 4.0f,  /* Zone 0 */
            .wd1772_low_stop = 115,
            .wd1772_high_stop = 141,
        },
        .adaptive = { 0 },  /* Nicht für MFM */
        .gcr = {
            .bucket_1 = 63,     /* Zone 3: Tracks 1-17 */
            .bucket_01 = 99,
            .bucket_1_pct = 100.0f,
            .bucket_01_pct = 157.0f,
            .auto_zone = true,
            .force_zone = 0,
        },
    },
    
    /*------------------------------------------------------------------------
     * Apple II DOS 3.3
     *------------------------------------------------------------------------*/
    {
        .id = UFT_PRESET_APPLE_DOS33,
        .name = "Apple DOS 3.3",
        .description = "Apple II DOS 3.3 (6-and-2 GCR, 250 kbit/s)",
        .pll = {
            .phase_adjust = 65.0f,
            .freq_adjust = 5.0f,
            .period_min = 75.0f,
            .period_max = 125.0f,
            .bitcell_us = 4.0f,
            .wd1772_low_stop = 115,
            .wd1772_high_stop = 141,
        },
        .adaptive = { 0 },
        .gcr = {
            .bucket_1 = 64,
            .bucket_01 = 100,
            .bucket_1_pct = 100.0f,
            .bucket_01_pct = 156.0f,
            .auto_zone = false,
            .force_zone = 0,
        },
    },
    
    /*------------------------------------------------------------------------
     * Apple II ProDOS
     *------------------------------------------------------------------------*/
    {
        .id = UFT_PRESET_APPLE_PRODOS,
        .name = "Apple ProDOS",
        .description = "Apple II ProDOS (6-and-2 GCR)",
        .pll = {
            .phase_adjust = 65.0f,
            .freq_adjust = 5.0f,
            .period_min = 75.0f,
            .period_max = 125.0f,
            .bitcell_us = 4.0f,
            .wd1772_low_stop = 115,
            .wd1772_high_stop = 141,
        },
        .adaptive = { 0 },
        .gcr = {
            .bucket_1 = 64,
            .bucket_01 = 100,
            .bucket_1_pct = 100.0f,
            .bucket_01_pct = 156.0f,
            .auto_zone = false,
            .force_zone = 0,
        },
    },
    
    /*------------------------------------------------------------------------
     * Macintosh 400K
     *------------------------------------------------------------------------*/
    {
        .id = UFT_PRESET_MAC_400K,
        .name = "Mac 400K",
        .description = "Macintosh 400K GCR (variable speed)",
        .pll = {
            .phase_adjust = 65.0f,
            .freq_adjust = 5.0f,
            .period_min = 75.0f,
            .period_max = 125.0f,
            .bitcell_us = 2.0f,
            .wd1772_low_stop = 115,
            .wd1772_high_stop = 141,
        },
        .adaptive = { 0 },
        .gcr = {
            .bucket_1 = 32,
            .bucket_01 = 50,
            .bucket_1_pct = 100.0f,
            .bucket_01_pct = 156.0f,
            .auto_zone = true,
            .force_zone = 0,
        },
    },
    
    /*------------------------------------------------------------------------
     * Macintosh 800K
     *------------------------------------------------------------------------*/
    {
        .id = UFT_PRESET_MAC_800K,
        .name = "Mac 800K",
        .description = "Macintosh 800K GCR (variable speed, 2 sides)",
        .pll = {
            .phase_adjust = 65.0f,
            .freq_adjust = 5.0f,
            .period_min = 75.0f,
            .period_max = 125.0f,
            .bitcell_us = 2.0f,
            .wd1772_low_stop = 115,
            .wd1772_high_stop = 141,
        },
        .adaptive = { 0 },
        .gcr = {
            .bucket_1 = 32,
            .bucket_01 = 50,
            .bucket_1_pct = 100.0f,
            .bucket_01_pct = 156.0f,
            .auto_zone = true,
            .force_zone = 0,
        },
    },
    
    /*------------------------------------------------------------------------
     * Dirty Dump (beschädigte Disks)
     *------------------------------------------------------------------------*/
    {
        .id = UFT_PRESET_DIRTY_DUMP,
        .name = "Dirty Dump",
        .description = "Beschädigte Disks mit weiten Toleranzen",
        .pll = {
            .phase_adjust = 80.0f,  /* Aggressivere Phasenkorrektur */
            .freq_adjust = 10.0f,   /* Schnellere Frequenzanpassung */
            .period_min = 60.0f,    /* Sehr weiter Bereich */
            .period_max = 150.0f,
            .bitcell_us = 2.0f,
            .wd1772_low_stop = 100,
            .wd1772_high_stop = 156,
        },
        .adaptive = {
            .thresh_4us = 2.0f,
            .thresh_6us = 3.0f,
            .thresh_8us = 4.0f,
            .adapt_rate = 50.0f,    /* Sehr schnelle Adaption */
            .lowpass_radius = 200,  /* Starke Glättung */
            .offset_ns = 0,
        },
        .gcr = {
            .bucket_1 = 80,
            .bucket_01 = 130,
            .bucket_1_pct = 125.0f,
            .bucket_01_pct = 200.0f,
            .auto_zone = true,
            .force_zone = 0,
        },
    },
    
    /*------------------------------------------------------------------------
     * Copy Protection Analysis
     *------------------------------------------------------------------------*/
    {
        .id = UFT_PRESET_COPY_PROTECTION,
        .name = "Copy Protection",
        .description = "Kopierschutz-Analyse (minimale Korrektur)",
        .pll = {
            .phase_adjust = 30.0f,  /* Minimale Korrektur */
            .freq_adjust = 2.0f,    /* Sehr langsam */
            .period_min = 90.0f,    /* Enger Bereich */
            .period_max = 110.0f,
            .bitcell_us = 2.0f,
            .wd1772_low_stop = 120,
            .wd1772_high_stop = 136,
        },
        .adaptive = {
            .thresh_4us = 2.0f,
            .thresh_6us = 3.0f,
            .thresh_8us = 4.0f,
            .adapt_rate = 10.0f,    /* Sehr langsam */
            .lowpass_radius = 20,   /* Minimal */
            .offset_ns = 0,
        },
        .gcr = {
            .bucket_1 = 60,
            .bucket_01 = 95,
            .bucket_1_pct = 95.0f,
            .bucket_01_pct = 150.0f,
            .auto_zone = false,
            .force_zone = 0,
        },
    },
};

#define NUM_PRESETS (sizeof(g_presets) / sizeof(g_presets[0]))

/*============================================================================
 * PRESET FUNKTIONEN
 *============================================================================*/

void uft_preset_get_pll(uft_preset_t preset, uft_gui_pll_params_t* params)
{
    if (!params) return;
    
    for (size_t i = 0; i < NUM_PRESETS; i++) {
        if (g_presets[i].id == preset) {
            *params = g_presets[i].pll;
            return;
        }
    }
    
    /* Default: IBM DD */
    *params = g_presets[0].pll;
}

void uft_preset_get_adaptive(uft_preset_t preset, uft_gui_adaptive_params_t* params)
{
    if (!params) return;
    
    for (size_t i = 0; i < NUM_PRESETS; i++) {
        if (g_presets[i].id == preset) {
            *params = g_presets[i].adaptive;
            return;
        }
    }
    
    /* Default: IBM DD */
    *params = g_presets[0].adaptive;
}

void uft_preset_get_gcr(uft_preset_t preset, uft_gui_gcr_params_t* params)
{
    if (!params) return;
    
    for (size_t i = 0; i < NUM_PRESETS; i++) {
        if (g_presets[i].id == preset) {
            *params = g_presets[i].gcr;
            return;
        }
    }
    
    /* Default: C64 */
    *params = g_presets[6].gcr;  /* C64_1541 */
}

const char* uft_preset_name(uft_preset_t preset)
{
    for (size_t i = 0; i < NUM_PRESETS; i++) {
        if (g_presets[i].id == preset) {
            return g_presets[i].name;
        }
    }
    return "Unknown";
}

const char* uft_preset_description(uft_preset_t preset)
{
    for (size_t i = 0; i < NUM_PRESETS; i++) {
        if (g_presets[i].id == preset) {
            return g_presets[i].description;
        }
    }
    return "Unknown preset";
}

/*============================================================================
 * KONVERTIERUNGS-FUNKTIONEN
 *============================================================================*/

void uft_gui_pll_to_mame(const uft_gui_pll_params_t* gui,
                         float* period_adjust,
                         float* phase_adjust,
                         float* min_period,
                         float* max_period)
{
    if (!gui) return;
    
    if (period_adjust) *period_adjust = gui->freq_adjust / 100.0f;
    if (phase_adjust)  *phase_adjust = gui->phase_adjust / 100.0f;
    if (min_period)    *min_period = gui->period_min / 100.0f;
    if (max_period)    *max_period = gui->period_max / 100.0f;
}

/*============================================================================
 * SLIDER KONFIGURATIONEN
 *============================================================================*/

uft_slider_config_t uft_gui_slider_pll_phase(void)
{
    return (uft_slider_config_t){
        .min_value = UFT_GUI_PLL_PHASE_MIN,
        .max_value = UFT_GUI_PLL_PHASE_MAX,
        .default_value = UFT_GUI_PLL_PHASE_DEFAULT,
        .step = 1.0f,
        .label = "Phase Adjust",
        .unit = "%",
        .tooltip = "How quickly PLL phase locks to transitions (65% MAME, 70% WD1772)"
    };
}

uft_slider_config_t uft_gui_slider_pll_freq(void)
{
    return (uft_slider_config_t){
        .min_value = UFT_GUI_PLL_FREQ_MIN,
        .max_value = UFT_GUI_PLL_FREQ_MAX,
        .default_value = UFT_GUI_PLL_FREQ_DEFAULT,
        .step = 0.5f,
        .label = "Freq Adjust",
        .unit = "%",
        .tooltip = "How quickly PLL adjusts cell width (5% typical)"
    };
}

uft_slider_config_t uft_gui_slider_adapt_rate(void)
{
    return (uft_slider_config_t){
        .min_value = UFT_GUI_ADAPT_RATE_MIN,
        .max_value = UFT_GUI_ADAPT_RATE_MAX,
        .default_value = UFT_GUI_ADAPT_RATE_DEFAULT,
        .step = 5.0f,
        .label = "Adapt Rate",
        .unit = "%",
        .tooltip = "Threshold adaptation speed (higher = faster)"
    };
}

uft_slider_config_t uft_gui_slider_lowpass(void)
{
    return (uft_slider_config_t){
        .min_value = (float)UFT_GUI_LOWPASS_MIN,
        .max_value = (float)UFT_GUI_LOWPASS_MAX,
        .default_value = (float)UFT_GUI_LOWPASS_DEFAULT,
        .step = 10.0f,
        .label = "Lowpass Radius",
        .unit = "",
        .tooltip = "Moving average filter radius (0 = off, 100 typical)"
    };
}

/*============================================================================
 * VALIDIERUNG
 *============================================================================*/

bool uft_gui_validate_param(const uft_gui_pll_params_t* params,
                            const char* changed_field,
                            float new_value,
                            char* error_msg)
{
    if (!params || !changed_field) return false;
    
    if (strcmp(changed_field, "phase_adjust") == 0) {
        if (new_value < UFT_GUI_PLL_PHASE_MIN || new_value > UFT_GUI_PLL_PHASE_MAX) {
            if (error_msg) {
                snprintf(error_msg, 256, "Phase adjust must be %.0f-%.0f%%",
                         UFT_GUI_PLL_PHASE_MIN, UFT_GUI_PLL_PHASE_MAX);
            }
            return false;
        }
    }
    else if (strcmp(changed_field, "freq_adjust") == 0) {
        if (new_value < UFT_GUI_PLL_FREQ_MIN || new_value > UFT_GUI_PLL_FREQ_MAX) {
            if (error_msg) {
                snprintf(error_msg, 256, "Freq adjust must be %.1f-%.1f%%",
                         UFT_GUI_PLL_FREQ_MIN, UFT_GUI_PLL_FREQ_MAX);
            }
            return false;
        }
    }
    else if (strcmp(changed_field, "period_min") == 0) {
        if (new_value < 50.0f || new_value > params->period_max - 5.0f) {
            if (error_msg) {
                snprintf(error_msg, 256, "Min period must be 50-%.0f%%",
                         params->period_max - 5.0f);
            }
            return false;
        }
    }
    else if (strcmp(changed_field, "period_max") == 0) {
        if (new_value < params->period_min + 5.0f || new_value > 200.0f) {
            if (error_msg) {
                snprintf(error_msg, 256, "Max period must be %.0f-200%%",
                         params->period_min + 5.0f);
            }
            return false;
        }
    }
    
    return true;
}
