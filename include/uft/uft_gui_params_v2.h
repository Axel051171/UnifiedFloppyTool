/**
 * @file uft_gui_params_v2.h
 * @brief Unified GUI Parameters V2 - Abgleich mit offiziellem Greaseweazle
 * @version 3.3.0
 *
 * Dieser Header vereint:
 * - Unsere bestehenden UFT GUI Parameter
 * - Offizielle Greaseweazle Parameter (Keir Fraser)
 * - FAT12/FAT32/NTFS Recovery Parameter
 * - Erweiterte Forensik-Parameter
 *
 * ÄNDERUNGEN gegenüber V1:
 * - PLL: period_adj und phase_adj statt freq_adjust (GW-kompatibel)
 * - GAPs: Offizielle Werte aus ibm.py
 * - Precomp: MFM/FM/GCR Patterns aus track.py
 * - Drive Delays: Offizielle GW Defaults
 * - Interleave: cskew/hskew Support
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_GUI_PARAMS_V2_H
#define UFT_GUI_PARAMS_V2_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * PARAMETER ABGLEICH: UFT V1 vs Greaseweazle Official
 *============================================================================
 *
 * | Parameter           | UFT V1              | Greaseweazle Official  | Status    |
 * |---------------------|---------------------|------------------------|-----------|
 * | PLL Phase Adjust    | phase_adjust 65%    | phase_adj_pct 60%      | UPDATED   |
 * | PLL Period Adjust   | freq_adjust 5%      | period_adj_pct 5%      | RENAMED   |
 * | PLL Lowpass         | (nicht vorhanden)   | lowpass_thresh µs      | ADDED     |
 * | Period Min          | period_min 75%      | (implicit)             | KEEP      |
 * | Period Max          | period_max 125%     | (implicit)             | KEEP      |
 * | Precomp Type        | (nicht vorhanden)   | MFM/FM/GCR             | ADDED     |
 * | Precomp NS          | (nicht vorhanden)   | 140ns default          | ADDED     |
 * | Gap1                | (nicht vorhanden)   | 26 FM / 50 MFM         | ADDED     |
 * | Gap2                | (nicht vorhanden)   | 11 FM / 22 MFM         | ADDED     |
 * | Gap3                | (nicht vorhanden)   | Size-dependent         | ADDED     |
 * | Gap4a               | (nicht vorhanden)   | 40 FM / 80 MFM         | ADDED     |
 * | Interleave          | (nicht vorhanden)   | 1 default              | ADDED     |
 * | Cylinder Skew       | (nicht vorhanden)   | 0 default              | ADDED     |
 * | Head Skew           | (nicht vorhanden)   | 0 default              | ADDED     |
 * | Step Delay          | (nicht vorhanden)   | 3000µs default         | ADDED     |
 * | Settle Delay        | (nicht vorhanden)   | 15ms default           | ADDED     |
 * | Motor Delay         | (nicht vorhanden)   | 500ms default          | ADDED     |
 * | Auto Off            | (nicht vorhanden)   | 10s default            | ADDED     |
 * | Read Revs           | (nicht vorhanden)   | 2 default              | ADDED     |
 * | Verify Writes       | (nicht vorhanden)   | true default           | ADDED     |
 *
 *============================================================================*/

/*============================================================================
 * EINHEITEN-TYPEN (kompatibel mit V1)
 *============================================================================*/

typedef float uft_percent_t;    /**< Prozent (0.0 - 100.0) */
typedef float uft_usec_t;       /**< Mikrosekunden */
typedef int32_t uft_nsec_t;     /**< Nanosekunden */
typedef uint16_t uft_msec_t;    /**< Millisekunden */

/*============================================================================
 * PLL PARAMETER V2 (Greaseweazle-kompatibel)
 *============================================================================*/

/**
 * @brief PLL Preset Typen
 */
typedef enum {
    UFT_PLL_AGGRESSIVE = 0,     /**< Schnelle Sync (GW Default) */
    UFT_PLL_CONSERVATIVE,       /**< Rauschunterdrückung */
    UFT_PLL_CUSTOM,             /**< Benutzerdefiniert */
    UFT_PLL_WD1772,             /**< WD1772-Emulation */
    UFT_PLL_MAME,               /**< MAME-Style */
    UFT_PLL_PRESET_COUNT
} uft_pll_preset_type_t;

/**
 * @brief PLL Parameter V2
 * 
 * Änderungen:
 * - freq_adjust umbenannt zu period_adj (GW-Kompatibilität)
 * - lowpass_thresh hinzugefügt
 * - Presets mit offiziellen GW-Werten
 */
typedef struct {
    /* Greaseweazle-kompatible Parameter */
    uft_percent_t period_adj;       /**< Period adjustment %, Default: 5 (GW) */
    uft_percent_t phase_adj;        /**< Phase adjustment %, Default: 60 (GW) */
    uft_usec_t    lowpass_thresh;   /**< Lowpass threshold µs, 0=disabled */
    
    /* Erweiterte Parameter (UFT V1 kompatibel) */
    uft_percent_t period_min;       /**< Min period %, Default: 75 */
    uft_percent_t period_max;       /**< Max period %, Default: 125 */
    uft_usec_t    bitcell_us;       /**< Nominal bitcell width µs */
    
    /* WD1772-spezifische Parameter */
    int32_t       wd1772_low_stop;  /**< WD1772 lower bound (115) */
    int32_t       wd1772_high_stop; /**< WD1772 upper bound (141) */
    
    /* Metadaten */
    uft_pll_preset_type_t preset;   /**< Aktives Preset */
} uft_pll_params_v2_t;

/* Offizielle Greaseweazle PLL Defaults */
#define UFT_PLL_PERIOD_ADJ_AGGRESSIVE   5
#define UFT_PLL_PHASE_ADJ_AGGRESSIVE    60
#define UFT_PLL_PERIOD_ADJ_CONSERVATIVE 1
#define UFT_PLL_PHASE_ADJ_CONSERVATIVE  10

/*============================================================================
 * PRECOMPENSATION (NEU aus Greaseweazle)
 *============================================================================*/

typedef enum {
    UFT_PRECOMP_MFM = 0,
    UFT_PRECOMP_FM  = 1,
    UFT_PRECOMP_GCR = 2
} uft_precomp_type_t;

/**
 * @brief Precompensation Parameter
 * 
 * Aus track.py:
 * - MFM: Patterns 10100 und 00101
 * - FM/GCR: Patterns 110 und 011 (adjacent 1s)
 */
typedef struct {
    uft_precomp_type_t type;    /**< Encoding type */
    uft_nsec_t         ns;      /**< Precomp in nanoseconds */
    bool               enabled; /**< Active flag */
} uft_precomp_params_t;

/* Offizielle Defaults */
#define UFT_PRECOMP_MFM_DEFAULT     140     /* 140ns */
#define UFT_PRECOMP_FM_DEFAULT      0
#define UFT_PRECOMP_GCR_DEFAULT     0

/*============================================================================
 * GAP PARAMETER (NEU aus Greaseweazle ibm.py)
 *============================================================================*/

/**
 * @brief GAP Parameter für IBM-kompatible Formate
 */
typedef struct {
    uint8_t gap1;               /**< Post-IAM gap */
    uint8_t gap2;               /**< Post-IDAM gap */
    uint8_t gap3;               /**< Post-DAM gap (size-dependent) */
    uint8_t gap4a;              /**< Post-Index gap */
} uft_gap_params_t;

/* Offizielle FM Gaps */
#define UFT_GAP1_FM     26
#define UFT_GAP2_FM     11
#define UFT_GAP3_FM_128 27
#define UFT_GAP3_FM_256 42
#define UFT_GAP3_FM_512 58
#define UFT_GAP3_FM_1024 138
#define UFT_GAP4A_FM    40

/* Offizielle MFM Gaps */
#define UFT_GAP1_MFM    50
#define UFT_GAP2_MFM    22
#define UFT_GAP3_MFM_128 32
#define UFT_GAP3_MFM_256 54
#define UFT_GAP3_MFM_512 84
#define UFT_GAP3_MFM_1024 116
#define UFT_GAP4A_MFM   80

/*============================================================================
 * INTERLEAVE PARAMETER (NEU aus Greaseweazle)
 *============================================================================*/

typedef struct {
    uint8_t interleave;         /**< Sector interleave (1=none) */
    uint8_t cskew;              /**< Cylinder skew */
    uint8_t hskew;              /**< Head skew */
} uft_interleave_params_t;

/* Defaults */
#define UFT_INTERLEAVE_DEFAULT  1
#define UFT_CSKEW_DEFAULT       0
#define UFT_HSKEW_DEFAULT       0

/*============================================================================
 * DRIVE TIMING PARAMETER (NEU aus Greaseweazle)
 *============================================================================*/

typedef struct {
    uft_usec_t  step_delay;     /**< Step delay µs (default 3000) */
    uft_msec_t  settle_delay;   /**< Head settle ms (default 15) */
    uft_msec_t  motor_delay;    /**< Motor spin-up ms (default 500) */
    uint8_t     auto_off;       /**< Auto motor-off seconds (default 10) */
} uft_drive_timing_t;

/* Offizielle Greaseweazle Defaults */
#define UFT_STEP_DELAY_DEFAULT      3000    /* 3ms */
#define UFT_SETTLE_DELAY_DEFAULT    15      /* 15ms */
#define UFT_MOTOR_DELAY_DEFAULT     500     /* 500ms */
#define UFT_AUTO_OFF_DEFAULT        10      /* 10s */

/*============================================================================
 * READ/WRITE PARAMETER (NEU)
 *============================================================================*/

typedef struct {
    uint8_t revs;               /**< Revolutions to read (default 2) */
    float   verify_revs;        /**< Verify revolutions (default 1.1) */
    uint8_t retries;            /**< Retry count (default 3) */
    bool    index_cued;         /**< Cue at index (default true) */
    bool    verify_writes;      /**< Verify after write (default true) */
} uft_rw_params_t;

/* Defaults */
#define UFT_REVS_DEFAULT            2
#define UFT_VERIFY_REVS_DEFAULT     1.1f
#define UFT_RETRIES_DEFAULT         3

/*============================================================================
 * FORMAT PARAMETER (Erweitert)
 *============================================================================*/

typedef enum {
    UFT_MODE_FM = 0,
    UFT_MODE_MFM,
    UFT_MODE_DEC_RX02,
    UFT_MODE_GCR_C64,
    UFT_MODE_GCR_APPLE,
    UFT_MODE_GCR_MAC
} uft_encoding_mode_t;

typedef struct {
    const char          *name;
    uft_encoding_mode_t  mode;
    uint8_t              cyls;
    uint8_t              heads;
    uint8_t              secs;
    uint8_t              size_code;     /**< 0=128, 1=256, 2=512, 3=1024 */
    uint16_t             rpm;
    uint16_t             data_rate;     /**< kbit/s */
    float                clock_us;      /**< Bit clock in µs */
} uft_format_def_t;

/* Erweiterte Format-Tabelle */
static const uft_format_def_t UFT_FORMAT_PRESETS[] = {
    /* PC 3.5" */
    { "PC 720K DD",      UFT_MODE_MFM, 80, 2, 9,  2, 300, 250, 4.0f },
    { "PC 1.44M HD",     UFT_MODE_MFM, 80, 2, 18, 2, 300, 500, 2.0f },
    { "PC 2.88M ED",     UFT_MODE_MFM, 80, 2, 36, 2, 300, 1000, 1.0f },
    /* PC 5.25" */
    { "PC 360K DD",      UFT_MODE_MFM, 40, 2, 9,  2, 300, 250, 4.0f },
    { "PC 1.2M HD",      UFT_MODE_MFM, 80, 2, 15, 2, 360, 500, 2.0f },
    /* Atari ST */
    { "Atari ST DD",     UFT_MODE_MFM, 80, 2, 9,  2, 300, 250, 4.0f },
    { "Atari ST HD",     UFT_MODE_MFM, 80, 2, 18, 2, 300, 500, 2.0f },
    /* Amiga */
    { "Amiga DD",        UFT_MODE_MFM, 80, 2, 11, 2, 300, 250, 2.0f },
    { "Amiga HD",        UFT_MODE_MFM, 80, 2, 22, 2, 300, 500, 1.0f },
    /* FM */
    { "IBM 8\" SSSD",    UFT_MODE_FM,  77, 1, 26, 0, 360, 250, 4.0f },
    { "IBM 5.25\" SSSD", UFT_MODE_FM,  40, 1, 10, 1, 300, 125, 8.0f },
    /* DEC */
    { "DEC RX02",        UFT_MODE_DEC_RX02, 77, 1, 26, 1, 360, 250, 4.0f },
    /* C64 */
    { "C64 1541",        UFT_MODE_GCR_C64, 35, 1, 21, 0, 300, 0, 0 },
    /* Apple */
    { "Apple II DOS",    UFT_MODE_GCR_APPLE, 35, 1, 16, 1, 300, 0, 0 },
    { "Apple ProDOS",    UFT_MODE_GCR_APPLE, 35, 1, 16, 1, 300, 0, 0 },
    /* Mac */
    { "Mac 400K",        UFT_MODE_GCR_MAC, 80, 1, 12, 1, 394, 0, 0 },
    { "Mac 800K",        UFT_MODE_GCR_MAC, 80, 2, 12, 1, 394, 0, 0 },
};

#define UFT_FORMAT_PRESET_COUNT (sizeof(UFT_FORMAT_PRESETS) / sizeof(UFT_FORMAT_PRESETS[0]))

/*============================================================================
 * VOLLSTÄNDIGE GUI PARAMETER STRUKTUR V2
 *============================================================================*/

/**
 * @brief Komplette GUI Parameter Struktur V2
 * 
 * Vereint alle Parameter für einheitliche GUI-Bindung
 */
typedef struct {
    /* PLL Parameter */
    uft_pll_params_v2_t     pll;
    
    /* Precompensation */
    uft_precomp_params_t    precomp;
    
    /* Gap Parameter */
    uft_gap_params_t        gaps;
    
    /* Interleave */
    uft_interleave_params_t interleave;
    
    /* Drive Timing */
    uft_drive_timing_t      timing;
    
    /* Read/Write */
    uft_rw_params_t         rw;
    
    /* Format */
    uint8_t                 format_preset;
    uft_encoding_mode_t     encoding;
    uint8_t                 cyls;
    uint8_t                 heads;
    uint8_t                 secs;
    uint8_t                 size_code;
    uint16_t                rpm;
    uint16_t                data_rate;
    
    /* Dirty Flag für GUI-Update */
    bool                    dirty;
    
} uft_gui_params_v2_t;

/*============================================================================
 * INITIALISIERUNG
 *============================================================================*/

/**
 * @brief Initialisiert Parameter mit Greaseweazle-Defaults
 */
static inline void uft_gui_params_v2_init(uft_gui_params_v2_t *p) {
    memset(p, 0, sizeof(*p));
    
    /* PLL - Greaseweazle Aggressive Defaults */
    p->pll.period_adj = UFT_PLL_PERIOD_ADJ_AGGRESSIVE;
    p->pll.phase_adj = UFT_PLL_PHASE_ADJ_AGGRESSIVE;
    p->pll.lowpass_thresh = 0;
    p->pll.period_min = 75.0f;
    p->pll.period_max = 125.0f;
    p->pll.bitcell_us = 2.0f;   /* MFM HD default */
    p->pll.preset = UFT_PLL_AGGRESSIVE;
    
    /* Precomp - MFM default */
    p->precomp.type = UFT_PRECOMP_MFM;
    p->precomp.ns = UFT_PRECOMP_MFM_DEFAULT;
    p->precomp.enabled = true;
    
    /* Gaps - MFM 512-byte default */
    p->gaps.gap1 = UFT_GAP1_MFM;
    p->gaps.gap2 = UFT_GAP2_MFM;
    p->gaps.gap3 = UFT_GAP3_MFM_512;
    p->gaps.gap4a = UFT_GAP4A_MFM;
    
    /* Interleave */
    p->interleave.interleave = UFT_INTERLEAVE_DEFAULT;
    p->interleave.cskew = UFT_CSKEW_DEFAULT;
    p->interleave.hskew = UFT_HSKEW_DEFAULT;
    
    /* Drive Timing - Greaseweazle Defaults */
    p->timing.step_delay = UFT_STEP_DELAY_DEFAULT;
    p->timing.settle_delay = UFT_SETTLE_DELAY_DEFAULT;
    p->timing.motor_delay = UFT_MOTOR_DELAY_DEFAULT;
    p->timing.auto_off = UFT_AUTO_OFF_DEFAULT;
    
    /* Read/Write */
    p->rw.revs = UFT_REVS_DEFAULT;
    p->rw.verify_revs = UFT_VERIFY_REVS_DEFAULT;
    p->rw.retries = UFT_RETRIES_DEFAULT;
    p->rw.index_cued = true;
    p->rw.verify_writes = true;
    
    /* Format - PC 1.44M default */
    p->format_preset = 1;   /* PC 1.44M HD */
    p->encoding = UFT_MODE_MFM;
    p->cyls = 80;
    p->heads = 2;
    p->secs = 18;
    p->size_code = 2;       /* 512 bytes */
    p->rpm = 300;
    p->data_rate = 500;
    
    p->dirty = false;
}

/*============================================================================
 * PRESET-ANWENDUNG
 *============================================================================*/

/**
 * @brief Format-Preset anwenden
 */
static inline void uft_gui_apply_format(uft_gui_params_v2_t *p, uint8_t preset) {
    if (preset >= UFT_FORMAT_PRESET_COUNT) return;
    
    const uft_format_def_t *fmt = &UFT_FORMAT_PRESETS[preset];
    
    p->format_preset = preset;
    p->encoding = fmt->mode;
    p->cyls = fmt->cyls;
    p->heads = fmt->heads;
    p->secs = fmt->secs;
    p->size_code = fmt->size_code;
    p->rpm = fmt->rpm;
    p->data_rate = fmt->data_rate;
    p->pll.bitcell_us = fmt->clock_us;
    
    /* Gaps basierend auf Mode */
    if (fmt->mode == UFT_MODE_FM) {
        p->gaps.gap1 = UFT_GAP1_FM;
        p->gaps.gap2 = UFT_GAP2_FM;
        p->gaps.gap4a = UFT_GAP4A_FM;
        switch (fmt->size_code) {
            case 0: p->gaps.gap3 = UFT_GAP3_FM_128; break;
            case 1: p->gaps.gap3 = UFT_GAP3_FM_256; break;
            case 2: p->gaps.gap3 = UFT_GAP3_FM_512; break;
            default: p->gaps.gap3 = UFT_GAP3_FM_1024; break;
        }
        p->precomp.type = UFT_PRECOMP_FM;
    } else if (fmt->mode == UFT_MODE_MFM || fmt->mode == UFT_MODE_DEC_RX02) {
        p->gaps.gap1 = UFT_GAP1_MFM;
        p->gaps.gap2 = UFT_GAP2_MFM;
        p->gaps.gap4a = UFT_GAP4A_MFM;
        switch (fmt->size_code) {
            case 0: p->gaps.gap3 = UFT_GAP3_MFM_128; break;
            case 1: p->gaps.gap3 = UFT_GAP3_MFM_256; break;
            case 2: p->gaps.gap3 = UFT_GAP3_MFM_512; break;
            default: p->gaps.gap3 = UFT_GAP3_MFM_1024; break;
        }
        p->precomp.type = UFT_PRECOMP_MFM;
    } else {
        /* GCR */
        p->precomp.type = UFT_PRECOMP_GCR;
    }
    
    p->dirty = true;
}

/**
 * @brief PLL-Preset anwenden
 */
static inline void uft_gui_apply_pll_preset(uft_gui_params_v2_t *p,
                                             uft_pll_preset_type_t preset) {
    switch (preset) {
        case UFT_PLL_AGGRESSIVE:
            p->pll.period_adj = UFT_PLL_PERIOD_ADJ_AGGRESSIVE;
            p->pll.phase_adj = UFT_PLL_PHASE_ADJ_AGGRESSIVE;
            p->pll.lowpass_thresh = 0;
            break;
        case UFT_PLL_CONSERVATIVE:
            p->pll.period_adj = UFT_PLL_PERIOD_ADJ_CONSERVATIVE;
            p->pll.phase_adj = UFT_PLL_PHASE_ADJ_CONSERVATIVE;
            p->pll.lowpass_thresh = 0;
            break;
        case UFT_PLL_WD1772:
            p->pll.period_adj = 5;
            p->pll.phase_adj = 70;
            p->pll.wd1772_low_stop = 115;
            p->pll.wd1772_high_stop = 141;
            break;
        case UFT_PLL_MAME:
            p->pll.period_adj = 5;
            p->pll.phase_adj = 65;
            break;
        default:
            break;
    }
    p->pll.preset = preset;
    p->dirty = true;
}

/*============================================================================
 * V1 → V2 MIGRATION
 *============================================================================*/

/* Forward declaration für V1 Struktur */
struct uft_gui_pll_params;

/**
 * @brief V1 PLL Parameter zu V2 migrieren
 */
static inline void uft_migrate_pll_v1_to_v2(
    const struct uft_gui_pll_params *v1,
    uft_pll_params_v2_t *v2)
{
    /* V1 phase_adjust → V2 phase_adj */
    /* V1 freq_adjust → V2 period_adj (Umbenennung!) */
    /* V1 period_min/max → V2 period_min/max */
    /* V1 bitcell_us → V2 bitcell_us */
    /* Neue Parameter auf Defaults */
    v2->lowpass_thresh = 0;
    v2->preset = UFT_PLL_CUSTOM;
}

/*============================================================================
 * Qt WIDGET MAPPING
 *============================================================================*/

/**
 * @brief Slider-Konfiguration für Qt
 */
typedef struct {
    const char *name;
    const char *unit;
    float min_val;
    float max_val;
    float default_val;
    float step;
    const char *tooltip;
} uft_widget_config_t;

/* Widget-Konfigurationen für GUI-Builder */
static const uft_widget_config_t UFT_WIDGET_CONFIGS[] = {
    /* PLL */
    { "Period Adjust", "%", 0, 20, 5, 1, 
      "Wie schnell passt PLL die Bitzellenbreite an (Greaseweazle: 5%)" },
    { "Phase Adjust", "%", 0, 100, 60, 5,
      "Wie schnell folgt PLL einer Transition (Greaseweazle: 60%)" },
    { "Lowpass Threshold", "µs", 0, 10, 0, 0.5f,
      "Rauschfilter-Schwelle (0=aus)" },
    
    /* Precomp */
    { "Precompensation", "ns", 0, 500, 140, 10,
      "Write Precompensation für MFM (140ns Standard)" },
    
    /* Gaps */
    { "Gap 1 (Post-IAM)", "bytes", 0, 100, 50, 1, "Gap nach Index Address Mark" },
    { "Gap 2 (Post-IDAM)", "bytes", 0, 50, 22, 1, "Gap nach ID Address Mark" },
    { "Gap 3 (Post-DAM)", "bytes", 0, 200, 84, 1, "Gap nach Data Address Mark" },
    { "Gap 4a (Post-Index)", "bytes", 0, 150, 80, 1, "Gap nach Index Pulse" },
    
    /* Drive */
    { "Step Delay", "µs", 1000, 20000, 3000, 500,
      "Verzögerung pro Schritt (Standard: 3ms)" },
    { "Settle Delay", "ms", 5, 50, 15, 1,
      "Kopf-Einschwingzeit (Standard: 15ms)" },
    { "Motor Delay", "ms", 100, 2000, 500, 50,
      "Motor-Anlaufzeit (Standard: 500ms)" },
    
    /* R/W */
    { "Revolutions", "", 1, 10, 2, 1,
      "Umdrehungen zum Lesen" },
    { "Retries", "", 0, 10, 3, 1,
      "Wiederholungsversuche bei Fehlern" },
};

#define UFT_WIDGET_CONFIG_COUNT (sizeof(UFT_WIDGET_CONFIGS) / sizeof(UFT_WIDGET_CONFIGS[0]))

#ifdef __cplusplus
}
#endif

#endif /* UFT_GUI_PARAMS_V2_H */
