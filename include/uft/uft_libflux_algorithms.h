/**
 * @file uft_libflux_algorithms.h
 * @brief UFT HFE Format Algorithms Integration for UFT
 * 
 * This file contains algorithms and data structures extracted from the
 * 
 * Original UFT HFE Format: http://hxc2001.free.fr
 * 
 * Integrated for UnifiedFloppyTool (UFT) - December 2025
 * 
 * HxC is the most comprehensive floppy emulator library with support for:
 * - 100+ image formats
 * - Multiple encoding schemes (MFM, FM, GCR, M2FM)
 * - Flux stream analysis with configurable PLL
 * - Multiple filesystem support (FAT12, AmigaDOS, CP/M, FLEX)
 * - Hardware emulation interfaces
 */

#ifndef UFT_LIBFLUX_ALGORITHMS_H
#define UFT_LIBFLUX_ALGORITHMS_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================*/
/*                           ERROR CODES                                      */
/*===========================================================================*/

#define UFT_LIBFLUX_VALIDFILE                  1
#define UFT_LIBFLUX_NOERROR                    0
#define UFT_LIBFLUX_ACCESSERROR               -1
#define UFT_LIBFLUX_BADFILE                   -2
#define UFT_LIBFLUX_FILECORRUPTED             -3
#define UFT_LIBFLUX_BADPARAMETER              -4
#define UFT_LIBFLUX_INTERNALERROR             -5
#define UFT_LIBFLUX_UNSUPPORTEDFILE           -6

/*===========================================================================*/
/*                           ENCODING TYPES                                   */
/*===========================================================================*/

/* Track encoding types from HxC */
typedef enum {
    UFT_ISOIBM_MFM_ENCODING = 0,
    UFT_AMIGA_MFM_ENCODING,
    UFT_ISOIBM_FM_ENCODING,
    UFT_EMU_FM_ENCODING,
    UFT_TYCOM_FM_ENCODING,
    UFT_MEMBRAIN_MFM_ENCODING,
    UFT_APPLEII_GCR1_ENCODING,
    UFT_APPLEII_GCR2_ENCODING,
    UFT_APPLEII_HDDD_A2_GCR1_ENCODING,
    UFT_APPLEII_HDDD_A2_GCR2_ENCODING,
    UFT_ARBURGDAT_ENCODING,
    UFT_ARBURGSYS_ENCODING,
    UFT_AED6200P_MFM_ENCODING,
    UFT_NORTHSTAR_HS_MFM_ENCODING,
    UFT_HEATHKIT_HS_FM_ENCODING,
    UFT_DEC_RX02_M2FM_ENCODING,
    UFT_C64_GCR_ENCODING,
    UFT_VICTOR9K_GCR_ENCODING,
    UFT_APPLEMAC_GCR_ENCODING,
    UFT_QD_MO5_ENCODING,
    UFT_MICRALN_HS_FM_ENCODING,
    UFT_CENTURION_MFM_ENCODING,
    UFT_UNKNOWN_ENCODING
} uft_track_encoding_t;

/* Format types from HxC */
typedef enum {
    UFT_IBMFORMAT_SD = 0x00,
    UFT_IBMFORMAT_DD = 0x01,
    UFT_ISOFORMAT_SD = 0x02,
    UFT_ISOFORMAT_DD = 0x03,
    UFT_ISOFORMAT_DD11S = 0x04,
    UFT_AMIGAFORMAT_DD = 0x05,
    UFT_TYCOMFORMAT_SD = 0x06,
    UFT_MEMBRAINFORMAT_DD = 0x07,
    UFT_APPLE2_GCR_5P25 = 0x08,
    UFT_APPLE2_GCR_5P25_HDDD = 0x09,
    UFT_ARBURGDAT_SD = 0x0A,
    UFT_ARBURGSYS_SD = 0x0B,
    UFT_AED6200P_DD = 0x0C,
    UFT_NORTHSTAR_DD = 0x0D,
    UFT_HEATHKIT_SD = 0x0E,
    UFT_DECRX02_SDDD = 0x0F,
    UFT_C64_GCR_DD = 0x10,
    UFT_VICTOR9K_DD = 0x11,
    UFT_APPLEMAC_GCR = 0x12,
    UFT_QD_MO5 = 0x13,
    UFT_MICRALN_SD = 0x14,
    UFT_CENTURION_DD = 0x15
} uft_track_format_t;

/* Interface modes from HxC */
typedef enum {
    UFT_IBMPC_DD_FLOPPYMODE = 0,
    UFT_IBMPC_HD_FLOPPYMODE,
    UFT_ATARIST_DD_FLOPPYMODE,
    UFT_ATARIST_HD_FLOPPYMODE,
    UFT_AMIGA_DD_FLOPPYMODE,
    UFT_AMIGA_HD_FLOPPYMODE,
    UFT_CPC_DD_FLOPPYMODE,
    UFT_GENERIC_SHUGART_DD_FLOPPYMODE,
    UFT_IBMPC_ED_FLOPPYMODE,
    UFT_MSX2_DD_FLOPPYMODE,
    UFT_C64_DD_FLOPPYMODE,
    UFT_EMU_SHUGART_FLOPPYMODE
} uft_floppy_interface_mode_t;

/*===========================================================================*/
/*                           PLL CONFIGURATION                                */
/*===========================================================================*/

/**
 * HxC PLL State Structure
 * This is the core PLL implementation from HxC fluxStreamAnalyzer
 */
typedef struct uft_libflux_pll_state {
    /* Current cell size (1 cell size) */
    int32_t pump_charge;
    
    /* Current window phase */
    int32_t phase;
    
    /* PLL limits */
    int32_t pll_max;
    int32_t pivot;      /* Center value */
    int32_t pll_min;
    
    /* Last error for diagnostics */
    int32_t last_error;
    
    /* Last pulse phase */
    int32_t lastpulsephase;
    
    /* Timing parameters */
    int tick_freq;
    int pll_min_max_percent;
    
    /* Correction ratios (numerator/denominator) */
    int fast_correction_ratio_n;
    int fast_correction_ratio_d;
    int slow_correction_ratio_n;
    int slow_correction_ratio_d;
    
    /* Inter-band rejection mode: 0=off, 1=GCR, 2=FM */
    int inter_band_rejection;
    
    /* Maximum PLL error in ticks before marking flakey */
    int max_pll_error_ticks;
    
    /* Band mode for variable speed formats (Victor 9000) */
    int band_mode;
    int bands_separators[16];
    
    /* Current track/side for debugging */
    int track;
    int side;
} uft_libflux_pll_state_t;

/**
 * Default HxC PLL parameters
 */
#define UFT_LIBFLUX_DEFAULT_MAXPULSESKEW     25      /* in per-256 */
#define UFT_LIBFLUX_DEFAULT_BLOCK_TIME       1000    /* in uS */
#define UFT_LIBFLUX_DEFAULT_SEARCHDEPTH      0.025
#define UFT_LIBFLUX_DEFAULT_PLL_PERCENT      10      /* +/- 10% */
#define UFT_LIBFLUX_DEFAULT_FAST_RATIO_N     7
#define UFT_LIBFLUX_DEFAULT_FAST_RATIO_D     8
#define UFT_LIBFLUX_DEFAULT_SLOW_RATIO_N     7
#define UFT_LIBFLUX_DEFAULT_SLOW_RATIO_D     8
#define UFT_LIBFLUX_DEFAULT_MAX_ERROR_TICKS  100

/* Internal stream tick frequency */
#define UFT_LIBFLUX_TICKFREQ 250000000  /* 250MHz tick */

/**
 * Initialize HxC-style PLL
 */
static inline void uft_libflux_pll_init(uft_libflux_pll_state_t *pll, 
                                     int tick_freq, 
                                     int bitcell_ticks)
{
    memset(pll, 0, sizeof(uft_libflux_pll_state_t));
    
    pll->tick_freq = tick_freq;
    pll->pump_charge = (bitcell_ticks * 16) / 2;
    pll->phase = 0;
    pll->pivot = bitcell_ticks * 16;
    pll->pll_min_max_percent = UFT_LIBFLUX_DEFAULT_PLL_PERCENT;
    pll->pll_max = pll->pivot + ((pll->pivot * pll->pll_min_max_percent) / 100);
    pll->pll_min = pll->pivot - ((pll->pivot * pll->pll_min_max_percent) / 100);
    pll->lastpulsephase = 0;
    
    pll->fast_correction_ratio_n = UFT_LIBFLUX_DEFAULT_FAST_RATIO_N;
    pll->fast_correction_ratio_d = UFT_LIBFLUX_DEFAULT_FAST_RATIO_D;
    pll->slow_correction_ratio_n = UFT_LIBFLUX_DEFAULT_SLOW_RATIO_N;
    pll->slow_correction_ratio_d = UFT_LIBFLUX_DEFAULT_SLOW_RATIO_D;
    
    pll->max_pll_error_ticks = UFT_LIBFLUX_DEFAULT_MAX_ERROR_TICKS;
    pll->inter_band_rejection = 0;
    pll->band_mode = 0;
}

/**
 * HxC-style PLL cell timing calculator
 * Returns number of bit cells for the given pulse value
 */
static inline int uft_libflux_get_cell_timing(uft_libflux_pll_state_t *pll,
                                          int current_pulsevalue,
                                          int *badpulse,
                                          int overlapval,
                                          int phasecorrection)
{
    int blankcell;
    int cur_pll_error, left_boundary, right_boundary, center;
    int current_pulse_position;
    
    blankcell = 0;
    
    current_pulsevalue = current_pulsevalue * 16;
    
    /* Overflow fix for very long tracks */
    if (pll->phase > (512 * 1024 * 1014)) {
        pll->phase -= (256 * 1024 * 1014);
        pll->lastpulsephase -= (256 * 1024 * 1014);
    }
    
    left_boundary = pll->phase;
    right_boundary = pll->phase + pll->pump_charge;
    center = pll->phase + (pll->pump_charge / 2);
    current_pulse_position = pll->lastpulsephase + current_pulsevalue;
    
    pll->last_error = 0xFFFF;
    
    /* Pulse before current window? */
    if (current_pulse_position < left_boundary) {
        pll->lastpulsephase = pll->lastpulsephase + current_pulsevalue;
        if (badpulse) *badpulse = 1;
        return blankcell;
    } else {
        blankcell = 1;
    }
    
    /* Pulse after current window? Count blank cells */
    while (current_pulse_position > right_boundary) {
        pll->phase = pll->phase + pll->pump_charge;
        left_boundary = pll->phase;
        right_boundary = pll->phase + pll->pump_charge;
        center = pll->phase + (pll->pump_charge / 2);
        blankcell++;
    }
    
    /* Inter-band rejection (GCR/FM specific) */
    if (pll->inter_band_rejection) {
        switch (pll->inter_band_rejection) {
            case 1: /* GCR */
                if (blankcell == 3) {
                    blankcell = ((right_boundary - current_pulse_position) > pll->pump_charge / 2) ? 2 : 4;
                }
                if (blankcell == 5) {
                    blankcell = ((right_boundary - current_pulse_position) > pll->pump_charge / 2) ? 4 : 6;
                }
                break;
            case 2: /* FM */
                if (blankcell == 1) blankcell = 2;
                if (blankcell == 3) {
                    blankcell = ((right_boundary - current_pulse_position) > pll->pump_charge / 2) ? 2 : 4;
                }
                if (blankcell > 4) blankcell = 4;
                break;
        }
    }
    
    /* Calculate PLL error */
    cur_pll_error = current_pulse_position - center;
    
    if (overlapval) {
        /* Adjust pump charge based on error */
        if (pll->pump_charge < (pll->pivot / 2)) {
            if (cur_pll_error < 0)
                pll->pump_charge = ((pll->pump_charge * pll->slow_correction_ratio_n) + 
                                   (pll->pump_charge + cur_pll_error)) / pll->slow_correction_ratio_d;
            else
                pll->pump_charge = ((pll->pump_charge * pll->fast_correction_ratio_n) + 
                                   (pll->pump_charge + cur_pll_error)) / pll->fast_correction_ratio_d;
        } else {
            if (cur_pll_error < 0)
                pll->pump_charge = ((pll->pump_charge * pll->fast_correction_ratio_n) + 
                                   (pll->pump_charge + cur_pll_error)) / pll->fast_correction_ratio_d;
            else
                pll->pump_charge = ((pll->pump_charge * pll->slow_correction_ratio_n) + 
                                   (pll->pump_charge + cur_pll_error)) / pll->slow_correction_ratio_d;
        }
        
        /* Clamp pump charge */
        if (pll->pump_charge < (pll->pll_min / 2))
            pll->pump_charge = pll->pll_min / 2;
        if (pll->pump_charge > (pll->pll_max / 2))
            pll->pump_charge = pll->pll_max / 2;
        
        /* Phase adjustment */
        pll->phase = pll->phase + (cur_pll_error / phasecorrection);
    }
    
    /* Advance to next window */
    pll->phase = pll->phase + pll->pump_charge;
    pll->lastpulsephase = pll->lastpulsephase + current_pulsevalue;
    
    pll->last_error = cur_pll_error;
    
    return blankcell;
}

/*===========================================================================*/
/*                           MFM ENCODING/DECODING                            */
/*===========================================================================*/

/**
 * HxC MFM Byte-to-MFM Lookup Table
 * Each byte expands to 16 bits with clock bits inserted
 */
static const uint16_t UFT_LIBFLUX_LUT_Byte2MFM[256] = {
    0xAAAA, 0xAAA9, 0xAAA4, 0xAAA5, 0xAA92, 0xAA91, 0xAA94, 0xAA95,
    0xAA4A, 0xAA49, 0xAA44, 0xAA45, 0xAA52, 0xAA51, 0xAA54, 0xAA55,
    0xA92A, 0xA929, 0xA924, 0xA925, 0xA912, 0xA911, 0xA914, 0xA915,
    0xA94A, 0xA949, 0xA944, 0xA945, 0xA952, 0xA951, 0xA954, 0xA955,
    0xA4AA, 0xA4A9, 0xA4A4, 0xA4A5, 0xA492, 0xA491, 0xA494, 0xA495,
    0xA44A, 0xA449, 0xA444, 0xA445, 0xA452, 0xA451, 0xA454, 0xA455,
    0xA52A, 0xA529, 0xA524, 0xA525, 0xA512, 0xA511, 0xA514, 0xA515,
    0xA54A, 0xA549, 0xA544, 0xA545, 0xA552, 0xA551, 0xA554, 0xA555,
    0x92AA, 0x92A9, 0x92A4, 0x92A5, 0x9292, 0x9291, 0x9294, 0x9295,
    0x924A, 0x9249, 0x9244, 0x9245, 0x9252, 0x9251, 0x9254, 0x9255,
    0x912A, 0x9129, 0x9124, 0x9125, 0x9112, 0x9111, 0x9114, 0x9115,
    0x914A, 0x9149, 0x9144, 0x9145, 0x9152, 0x9151, 0x9154, 0x9155,
    0x94AA, 0x94A9, 0x94A4, 0x94A5, 0x9492, 0x9491, 0x9494, 0x9495,
    0x944A, 0x9449, 0x9444, 0x9445, 0x9452, 0x9451, 0x9454, 0x9455,
    0x952A, 0x9529, 0x9524, 0x9525, 0x9512, 0x9511, 0x9514, 0x9515,
    0x954A, 0x9549, 0x9544, 0x9545, 0x9552, 0x9551, 0x9554, 0x9555,
    0x4AAA, 0x4AA9, 0x4AA4, 0x4AA5, 0x4A92, 0x4A91, 0x4A94, 0x4A95,
    0x4A4A, 0x4A49, 0x4A44, 0x4A45, 0x4A52, 0x4A51, 0x4A54, 0x4A55,
    0x492A, 0x4929, 0x4924, 0x4925, 0x4912, 0x4911, 0x4914, 0x4915,
    0x494A, 0x4949, 0x4944, 0x4945, 0x4952, 0x4951, 0x4954, 0x4955,
    0x44AA, 0x44A9, 0x44A4, 0x44A5, 0x4492, 0x4491, 0x4494, 0x4495,
    0x444A, 0x4449, 0x4444, 0x4445, 0x4452, 0x4451, 0x4454, 0x4455,
    0x452A, 0x4529, 0x4524, 0x4525, 0x4512, 0x4511, 0x4514, 0x4515,
    0x454A, 0x4549, 0x4544, 0x4545, 0x4552, 0x4551, 0x4554, 0x4555,
    0x52AA, 0x52A9, 0x52A4, 0x52A5, 0x5292, 0x5291, 0x5294, 0x5295,
    0x524A, 0x5249, 0x5244, 0x5245, 0x5252, 0x5251, 0x5254, 0x5255,
    0x512A, 0x5129, 0x5124, 0x5125, 0x5112, 0x5111, 0x5114, 0x5115,
    0x514A, 0x5149, 0x5144, 0x5145, 0x5152, 0x5151, 0x5154, 0x5155,
    0x54AA, 0x54A9, 0x54A4, 0x54A5, 0x5492, 0x5491, 0x5494, 0x5495,
    0x544A, 0x5449, 0x5444, 0x5445, 0x5452, 0x5451, 0x5454, 0x5455,
    0x552A, 0x5529, 0x5524, 0x5525, 0x5512, 0x5511, 0x5514, 0x5515,
    0x554A, 0x5549, 0x5544, 0x5545, 0x5552, 0x5551, 0x5554, 0x5555
};

/**
 * HxC MFM Clock Mask Table
 * Used for special address marks with missing clock bits
 */
static const uint16_t UFT_LIBFLUX_LUT_Byte2MFMClkMask[256] = {
    0x5555, 0x5557, 0x555D, 0x555F, 0x5575, 0x5577, 0x557D, 0x557F,
    0x55D5, 0x55D7, 0x55DD, 0x55DF, 0x55F5, 0x55F7, 0x55FD, 0x55FF,
    0x5755, 0x5757, 0x575D, 0x575F, 0x5775, 0x5777, 0x577D, 0x577F,
    0x57D5, 0x57D7, 0x57DD, 0x57DF, 0x57F5, 0x57F7, 0x57FD, 0x57FF,
    0x5D55, 0x5D57, 0x5D5D, 0x5D5F, 0x5D75, 0x5D77, 0x5D7D, 0x5D7F,
    0x5DD5, 0x5DD7, 0x5DDD, 0x5DDF, 0x5DF5, 0x5DF7, 0x5DFD, 0x5DFF,
    0x5F55, 0x5F57, 0x5F5D, 0x5F5F, 0x5F75, 0x5F77, 0x5F7D, 0x5F7F,
    0x5FD5, 0x5FD7, 0x5FDD, 0x5FDF, 0x5FF5, 0x5FF7, 0x5FFD, 0x5FFF,
    0x7555, 0x7557, 0x755D, 0x755F, 0x7575, 0x7577, 0x757D, 0x757F,
    0x75D5, 0x75D7, 0x75DD, 0x75DF, 0x75F5, 0x75F7, 0x75FD, 0x75FF,
    0x7755, 0x7757, 0x775D, 0x775F, 0x7775, 0x7777, 0x777D, 0x777F,
    0x77D5, 0x77D7, 0x77DD, 0x77DF, 0x77F5, 0x77F7, 0x77FD, 0x77FF,
    0x7D55, 0x7D57, 0x7D5D, 0x7D5F, 0x7D75, 0x7D77, 0x7D7D, 0x7D7F,
    0x7DD5, 0x7DD7, 0x7DDD, 0x7DDF, 0x7DF5, 0x7DF7, 0x7DFD, 0x7DFF,
    0x7F55, 0x7F57, 0x7F5D, 0x7F5F, 0x7F75, 0x7F77, 0x7F7D, 0x7F7F,
    0x7FD5, 0x7FD7, 0x7FDD, 0x7FDF, 0x7FF5, 0x7FF7, 0x7FFD, 0x7FFF,
    0xD555, 0xD557, 0xD55D, 0xD55F, 0xD575, 0xD577, 0xD57D, 0xD57F,
    0xD5D5, 0xD5D7, 0xD5DD, 0xD5DF, 0xD5F5, 0xD5F7, 0xD5FD, 0xD5FF,
    0xD755, 0xD757, 0xD75D, 0xD75F, 0xD775, 0xD777, 0xD77D, 0xD77F,
    0xD7D5, 0xD7D7, 0xD7DD, 0xD7DF, 0xD7F5, 0xD7F7, 0xD7FD, 0xD7FF,
    0xDD55, 0xDD57, 0xDD5D, 0xDD5F, 0xDD75, 0xDD77, 0xDD7D, 0xDD7F,
    0xDDD5, 0xDDD7, 0xDDDD, 0xDDDF, 0xDDF5, 0xDDF7, 0xDDFD, 0xDDFF,
    0xDF55, 0xDF57, 0xDF5D, 0xDF5F, 0xDF75, 0xDF77, 0xDF7D, 0xDF7F,
    0xDFD5, 0xDFD7, 0xDFDD, 0xDFDF, 0xDFF5, 0xDFF7, 0xDFFD, 0xDFFF,
    0xF555, 0xF557, 0xF55D, 0xF55F, 0xF575, 0xF577, 0xF57D, 0xF57F,
    0xF5D5, 0xF5D7, 0xF5DD, 0xF5DF, 0xF5F5, 0xF5F7, 0xF5FD, 0xF5FF,
    0xF755, 0xF757, 0xF75D, 0xF75F, 0xF775, 0xF777, 0xF77D, 0xF77F,
    0xF7D5, 0xF7D7, 0xF7DD, 0xF7DF, 0xF7F5, 0xF7F7, 0xF7FD, 0xF7FF,
    0xFD55, 0xFD57, 0xFD5D, 0xFD5F, 0xFD75, 0xFD77, 0xFD7D, 0xFD7F,
    0xFDD5, 0xFDD7, 0xFDDD, 0xFDDF, 0xFDF5, 0xFDF7, 0xFDFD, 0xFDFF,
    0xFF55, 0xFF57, 0xFF5D, 0xFF5F, 0xFF75, 0xFF77, 0xFF7D, 0xFF7F,
    0xFFD5, 0xFFD7, 0xFFDD, 0xFFDF, 0xFFF5, 0xFFF7, 0xFFFD, 0xFFFF
};

/**
 * Extract even bits from byte (for Amiga MFM)
 */
static const uint8_t UFT_LIBFLUX_LUT_Byte2EvenBits[256] = {
    0x00, 0x01, 0x00, 0x01, 0x02, 0x03, 0x02, 0x03,
    0x00, 0x01, 0x00, 0x01, 0x02, 0x03, 0x02, 0x03,
    0x04, 0x05, 0x04, 0x05, 0x06, 0x07, 0x06, 0x07,
    0x04, 0x05, 0x04, 0x05, 0x06, 0x07, 0x06, 0x07,
    0x00, 0x01, 0x00, 0x01, 0x02, 0x03, 0x02, 0x03,
    0x00, 0x01, 0x00, 0x01, 0x02, 0x03, 0x02, 0x03,
    0x04, 0x05, 0x04, 0x05, 0x06, 0x07, 0x06, 0x07,
    0x04, 0x05, 0x04, 0x05, 0x06, 0x07, 0x06, 0x07,
    0x08, 0x09, 0x08, 0x09, 0x0A, 0x0B, 0x0A, 0x0B,
    0x08, 0x09, 0x08, 0x09, 0x0A, 0x0B, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0C, 0x0D, 0x0E, 0x0F, 0x0E, 0x0F,
    0x0C, 0x0D, 0x0C, 0x0D, 0x0E, 0x0F, 0x0E, 0x0F,
    0x08, 0x09, 0x08, 0x09, 0x0A, 0x0B, 0x0A, 0x0B,
    0x08, 0x09, 0x08, 0x09, 0x0A, 0x0B, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0C, 0x0D, 0x0E, 0x0F, 0x0E, 0x0F,
    0x0C, 0x0D, 0x0C, 0x0D, 0x0E, 0x0F, 0x0E, 0x0F,
    /* ... remaining 128 entries follow same pattern ... */
    0x00, 0x01, 0x00, 0x01, 0x02, 0x03, 0x02, 0x03,
    0x00, 0x01, 0x00, 0x01, 0x02, 0x03, 0x02, 0x03,
    0x04, 0x05, 0x04, 0x05, 0x06, 0x07, 0x06, 0x07,
    0x04, 0x05, 0x04, 0x05, 0x06, 0x07, 0x06, 0x07,
    0x00, 0x01, 0x00, 0x01, 0x02, 0x03, 0x02, 0x03,
    0x00, 0x01, 0x00, 0x01, 0x02, 0x03, 0x02, 0x03,
    0x04, 0x05, 0x04, 0x05, 0x06, 0x07, 0x06, 0x07,
    0x04, 0x05, 0x04, 0x05, 0x06, 0x07, 0x06, 0x07,
    0x08, 0x09, 0x08, 0x09, 0x0A, 0x0B, 0x0A, 0x0B,
    0x08, 0x09, 0x08, 0x09, 0x0A, 0x0B, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0C, 0x0D, 0x0E, 0x0F, 0x0E, 0x0F,
    0x0C, 0x0D, 0x0C, 0x0D, 0x0E, 0x0F, 0x0E, 0x0F,
    0x08, 0x09, 0x08, 0x09, 0x0A, 0x0B, 0x0A, 0x0B,
    0x08, 0x09, 0x08, 0x09, 0x0A, 0x0B, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0C, 0x0D, 0x0E, 0x0F, 0x0E, 0x0F,
    0x0C, 0x0D, 0x0C, 0x0D, 0x0E, 0x0F, 0x0E, 0x0F
};

/**
 * Extract odd bits from byte (for Amiga MFM)
 */
static const uint8_t UFT_LIBFLUX_LUT_Byte2OddBits[256] = {
    0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x01,
    0x02, 0x02, 0x03, 0x03, 0x02, 0x02, 0x03, 0x03,
    0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x01,
    0x02, 0x02, 0x03, 0x03, 0x02, 0x02, 0x03, 0x03,
    0x04, 0x04, 0x05, 0x05, 0x04, 0x04, 0x05, 0x05,
    0x06, 0x06, 0x07, 0x07, 0x06, 0x06, 0x07, 0x07,
    0x04, 0x04, 0x05, 0x05, 0x04, 0x04, 0x05, 0x05,
    0x06, 0x06, 0x07, 0x07, 0x06, 0x06, 0x07, 0x07,
    0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x01,
    0x02, 0x02, 0x03, 0x03, 0x02, 0x02, 0x03, 0x03,
    0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x01,
    0x02, 0x02, 0x03, 0x03, 0x02, 0x02, 0x03, 0x03,
    0x04, 0x04, 0x05, 0x05, 0x04, 0x04, 0x05, 0x05,
    0x06, 0x06, 0x07, 0x07, 0x06, 0x06, 0x07, 0x07,
    0x04, 0x04, 0x05, 0x05, 0x04, 0x04, 0x05, 0x05,
    0x06, 0x06, 0x07, 0x07, 0x06, 0x06, 0x07, 0x07,
    0x08, 0x08, 0x09, 0x09, 0x08, 0x08, 0x09, 0x09,
    0x0A, 0x0A, 0x0B, 0x0B, 0x0A, 0x0A, 0x0B, 0x0B,
    0x08, 0x08, 0x09, 0x09, 0x08, 0x08, 0x09, 0x09,
    0x0A, 0x0A, 0x0B, 0x0B, 0x0A, 0x0A, 0x0B, 0x0B,
    0x0C, 0x0C, 0x0D, 0x0D, 0x0C, 0x0C, 0x0D, 0x0D,
    0x0E, 0x0E, 0x0F, 0x0F, 0x0E, 0x0E, 0x0F, 0x0F,
    0x0C, 0x0C, 0x0D, 0x0D, 0x0C, 0x0C, 0x0D, 0x0D,
    0x0E, 0x0E, 0x0F, 0x0F, 0x0E, 0x0E, 0x0F, 0x0F,
    0x08, 0x08, 0x09, 0x09, 0x08, 0x08, 0x09, 0x09,
    0x0A, 0x0A, 0x0B, 0x0B, 0x0A, 0x0A, 0x0B, 0x0B,
    0x08, 0x08, 0x09, 0x09, 0x08, 0x08, 0x09, 0x09,
    0x0A, 0x0A, 0x0B, 0x0B, 0x0A, 0x0A, 0x0B, 0x0B,
    0x0C, 0x0C, 0x0D, 0x0D, 0x0C, 0x0C, 0x0D, 0x0D,
    0x0E, 0x0E, 0x0F, 0x0F, 0x0E, 0x0E, 0x0F, 0x0F,
    0x0C, 0x0C, 0x0D, 0x0D, 0x0C, 0x0C, 0x0D, 0x0D,
    0x0E, 0x0E, 0x0F, 0x0F, 0x0E, 0x0E, 0x0F, 0x0F
};

/**
 * Bit reversal table
 */
static const uint8_t UFT_LIBFLUX_LUT_ByteBitsInverter[256] = {
    0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0,
    0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0,
    0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8,
    0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8,
    0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4,
    0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
    0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC,
    0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC,
    0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2,
    0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,
    0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA,
    0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
    0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6,
    0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6,
    0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE,
    0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,
    0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1,
    0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
    0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9,
    0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9,
    0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5,
    0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,
    0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED,
    0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
    0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3,
    0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3,
    0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB,
    0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,
    0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7,
    0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
    0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF,
    0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF
};

/*===========================================================================*/
/*                           C64 GCR ENCODING                                 */
/*===========================================================================*/

/**
 * C64 GCR Encoding Table (4 bits -> 5 bits)
 */
static const uint8_t UFT_LIBFLUX_GCR_ENCODE[16] = {
    0x0A, 0x0B, 0x12, 0x13,
    0x0E, 0x0F, 0x16, 0x17,
    0x09, 0x19, 0x1A, 0x1B,
    0x0D, 0x1D, 0x1E, 0x15
};

/**
 * C64 GCR Decoding Table (5 bits -> 4 bits, 0xFF = invalid)
 */
static const uint8_t UFT_LIBFLUX_GCR_DECODE[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 00-07 */
    0xFF, 0x08, 0x00, 0x01, 0xFF, 0x0C, 0x04, 0x05,  /* 08-0F */
    0xFF, 0xFF, 0x02, 0x03, 0xFF, 0x0F, 0x06, 0x07,  /* 10-17 */
    0xFF, 0x09, 0x0A, 0x0B, 0xFF, 0x0D, 0x0E, 0xFF   /* 18-1F */
};

/*===========================================================================*/
/*                           APPLE II GCR ENCODING                            */
/*===========================================================================*/

/**
 * Apple II 6-and-2 GCR Encoding Table
 */
static const uint8_t UFT_LIBFLUX_APPLE2_GCR6_ENCODE[64] = {
    0x96, 0x97, 0x9A, 0x9B, 0x9D, 0x9E, 0x9F, 0xA6,
    0xA7, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB2, 0xB3,
    0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC,
    0xBD, 0xBE, 0xBF, 0xCB, 0xCD, 0xCE, 0xCF, 0xD3,
    0xD6, 0xD7, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE,
    0xDF, 0xE5, 0xE6, 0xE7, 0xE9, 0xEA, 0xEB, 0xEC,
    0xED, 0xEE, 0xEF, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6,
    0xF7, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};

/**
 * Apple II 6-and-2 GCR Decoding Table
 */
static const uint8_t UFT_LIBFLUX_APPLE2_GCR6_DECODE[256] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x01, 0xFF, 0xFF, 0x02, 0x03, 0xFF, 0x04, 0x05, 0x06,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x07, 0x08, 0xFF, 0xFF, 0xFF, 0x09, 0x0A, 0x0B, 0x0C, 0x0D,
    0xFF, 0xFF, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0xFF, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x1B, 0xFF, 0x1C, 0x1D, 0x1E,
    0xFF, 0xFF, 0xFF, 0x1F, 0xFF, 0xFF, 0x20, 0x21, 0xFF, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x29, 0x2A, 0x2B, 0xFF, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32,
    0xFF, 0xFF, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0xFF, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F
};

/*===========================================================================*/
/*                           CRC FUNCTIONS                                    */
/*===========================================================================*/

/**
 * CRC-16-CCITT implementation from HxC
 * Uses 4-bit table for smaller footprint
 */
typedef struct {
    uint8_t high;
    uint8_t low;
    uint8_t table[32];  /* 16 entries for high byte + 16 for low byte */
} uft_libflux_crc16_t;

static inline uint16_t uft_libflux_crc16_gen_entry(uint16_t index, int num_bits, uint16_t poly)
{
    uint16_t ret = index << (16 - num_bits);
    for (int i = 0; i < num_bits; i++) {
        if (ret & 0x8000)
            ret = (ret << 1) ^ poly;
        else
            ret = ret << 1;
    }
    return ret;
}

static inline void uft_libflux_crc16_init(uft_libflux_crc16_t *crc, uint16_t poly, uint16_t init)
{
    for (int i = 0; i < 16; i++) {
        uint16_t te = uft_libflux_crc16_gen_entry(i, 4, poly);
        crc->table[i + 16] = (uint8_t)(te >> 8);
        crc->table[i] = (uint8_t)(te & 0xFF);
    }
    crc->high = (uint8_t)(init >> 8);
    crc->low = (uint8_t)(init & 0xFF);
}

static inline void uft_libflux_crc16_update4(uft_libflux_crc16_t *crc, uint8_t val)
{
    uint8_t t = (crc->high >> 4) ^ val;
    crc->high = (crc->high << 4) | (crc->low >> 4);
    crc->low = crc->low << 4;
    crc->high ^= crc->table[t + 16];
    crc->low ^= crc->table[t];
}

static inline void uft_libflux_crc16_update(uft_libflux_crc16_t *crc, uint8_t val)
{
    uft_libflux_crc16_update4(crc, val >> 4);
    uft_libflux_crc16_update4(crc, val & 0x0F);
}

static inline uint16_t uft_libflux_crc16_get(uft_libflux_crc16_t *crc)
{
    return ((uint16_t)crc->high << 8) | crc->low;
}

/*===========================================================================*/
/*                           HFE FORMAT STRUCTURES                            */
/*===========================================================================*/

#pragma pack(push, 1)

/**
 * HFE File Header (native HxC format)
 */
typedef struct {
    uint8_t  signature[8];          /* "HXCPICFE" or "HXCHFEV3" */
    uint8_t  format_revision;
    uint8_t  number_of_track;
    uint8_t  number_of_side;
    uint8_t  track_encoding;
    uint16_t bitrate;               /* In Kbit/s, 250 for DD, 500 for HD */
    uint16_t uft_floppy_rpm;            /* 300 or 360 */
    uint8_t  uft_floppy_interface_mode;
    uint8_t  write_protected;
    uint16_t track_list_offset;     /* In 512-byte blocks */
    uint8_t  write_allowed;
    uint8_t  single_step;
    uint8_t  track0s0_altencoding;
    uint8_t  track0s0_encoding;
    uint8_t  track0s1_altencoding;
    uint8_t  track0s1_encoding;
} uft_hfe_header_t;

/**
 * HFE Track Entry
 */
typedef struct {
    uint16_t offset;                /* In 512-byte blocks */
    uint16_t track_len;             /* In bytes */
} uft_hfe_track_t;

/**
 * SCP File Header (SuperCard Pro format)
 */
typedef struct {
    uint8_t  sign[3];               /* "SCP" */
    uint8_t  version;               /* (Version<<4)|Revision */
    uint8_t  disk_type;
    uint8_t  number_of_revolution;
    uint8_t  start_track;
    uint8_t  end_track;
    uint8_t  flags;
    uint8_t  bit_cell_width;        /* 0 = 16 bits */
    uint8_t  number_of_heads;       /* 0=both, 1=side0, 2=side1 */
    uint8_t  resolution;            /* 0=25ns, 1=50ns, etc. */
    uint32_t file_data_checksum;
} uft_scp_header_t;

/**
 * SCP Track Revolution Entry
 */
typedef struct {
    uint32_t index_time;            /* Duration in 25ns ticks */
    uint32_t track_length;          /* Number of bitcells */
    uint32_t track_offset;          /* Offset from TDH start */
} uft_scp_index_pos_t;

/**
 * SCP Track Data Header
 */
typedef struct {
    uint8_t  trk_sign[3];           /* "TRK" */
    uint8_t  track_number;
    /* Followed by scp_index_pos entries */
} uft_scp_track_header_t;

/**
 */
typedef struct {
    uint8_t  sign;                  /* 0x0D */
    uint8_t  type;
    uint16_t size;
} uft_kf_oob_header_t;

#pragma pack(pop)

/* SCP Constants */
#define UFT_SCP_MAX_TRACKS          168
#define UFT_SCP_DEFAULT_PERIOD_NS   25000

/* SCP Flag definitions */
#define UFT_SCP_FLAG_INDEX          0x01
#define UFT_SCP_FLAG_96TPI          0x02
#define UFT_SCP_FLAG_360RPM         0x04
#define UFT_SCP_FLAG_NORMALIZED     0x08
#define UFT_SCP_FLAG_READWRITE      0x10
#define UFT_SCP_FLAG_FOOTER         0x20

#define UFT_UFT_KF_OOB_SIGN             0x0D
#define UFT_UFT_KF_OOBTYPE_STREAM_READ  0x01
#define UFT_UFT_KF_OOBTYPE_INDEX        0x02
#define UFT_UFT_KF_OOBTYPE_STREAM_END   0x03
#define UFT_UFT_KF_OOBTYPE_STRING       0x04
#define UFT_UFT_KF_OOBTYPE_END          0x0D

#define UFT_UFT_KF_DEFAULT_MCLOCK       48054857.14285714  /* (((18432000*73)/14)/2) */
#define UFT_UFT_KF_DEFAULT_SCLOCK       (UFT_UFT_KF_DEFAULT_MCLOCK / 2)  /* ~41.619ns */

/*===========================================================================*/
/*                           VICTOR 9000 SPEED ZONES                          */
/*===========================================================================*/

/**
 * Victor 9000 variable speed zone definitions
 * Format: start_cyl, bit1_time, bit2_code, bit2_time, bit3_code, bit3_time, end_marker
 */
static const int UFT_LIBFLUX_VICTOR9K_BANDS[][8] = {
    {  0, 1, 2142, 3, 3600, 5, 5200, 0 },
    {  4, 1, 2492, 3, 3800, 5, 5312, 0 },
    { 16, 1, 2550, 3, 3966, 5, 5552, 0 },
    { 27, 1, 2723, 3, 4225, 5, 5852, 0 },
    { 38, 1, 2950, 3, 4500, 5, 6450, 0 },
    { 48, 1, 3150, 3, 4836, 5, 6800, 0 },
    { 60, 1, 3400, 3, 5250, 5, 7500, 0 },
    { 71, 1, 3800, 3, 5600, 5, 8000, 0 },
    { -1, 0,    0, 0,    0, 0,    0, 0 }  /* End marker */
};

/*===========================================================================*/
/*                           IBM PC FORMAT CONFIGURATIONS                     */
/*===========================================================================*/

/**
 * IBM/ISO Track Format Configuration
 */
typedef struct {
    uint8_t  format_type;
    
    /* Post-index GAP4a */
    uint8_t  gap4a_byte;
    uint16_t gap4a_len;
    
    /* Index sync */
    uint8_t  index_sync_byte;
    uint16_t index_sync_len;
    
    /* Index mark */
    uint8_t  index_mark_byte;
    uint8_t  index_mark_clock;
    uint8_t  index_mark_len;
    uint8_t  index_mark2_byte;
    uint8_t  index_mark2_clock;
    uint8_t  index_mark2_len;
    
    /* GAP1 */
    uint8_t  gap1_byte;
    uint16_t gap1_len;
    
    /* Header sync */
    uint8_t  header_sync_byte;
    uint16_t header_sync_len;
    
    /* Data sync */
    uint8_t  data_sync_byte;
    uint16_t data_sync_len;
    
    /* Address mark */
    uint8_t  addr_mark_byte;
    uint8_t  addr_mark_clock;
    uint8_t  addr_mark_len;
    uint8_t  addr_mark2_byte;
    uint8_t  addr_mark2_clock;
    uint8_t  addr_mark2_len;
    
    /* GAP2 */
    uint8_t  gap2_byte;
    uint16_t gap2_len;
    
    /* Data mark */
    uint8_t  data_mark_byte;
    uint8_t  data_mark_clock;
    uint8_t  data_mark_len;
    uint8_t  data_mark2_byte;
    uint8_t  data_mark2_clock;
    uint8_t  data_mark2_len;
    
    /* GAP3 */
    uint8_t  gap3_byte;
    uint16_t gap3_len;
    
    /* GAP4 (fill) */
    uint8_t  gap4_byte;
    uint16_t gap4_len;
    
    /* CHRN defaults */
    uint8_t  default_cyl;
    uint8_t  default_head;
    uint8_t  default_sector;
    uint8_t  default_size;
    
    /* CRC polynomial and init */
    uint16_t crc_poly;
    uint16_t crc_init;
    
    /* Post-CRC glitch bytes */
    uint8_t  post_header_crc_byte;
    uint8_t  post_header_crc_clock;
    uint8_t  post_header_crc_len;
    uint8_t  post_data_crc_byte;
    uint8_t  post_data_crc_clock;
    uint8_t  post_data_crc_len;
} uft_ibm_format_config_t;

/*===========================================================================*/
/*                           GAP3 LOOKUP TABLE                                */
/*===========================================================================*/

/**
 * Standard GAP3 values based on track mode, sector size, and sector count
 */
typedef struct {
    uint8_t  trackmode;
    uint16_t sectorsize;
    uint8_t  numberofsector;
    uint8_t  gap3;
} uft_gap3_config_t;

static const uft_gap3_config_t UFT_LIBFLUX_STD_GAP3_TABLE[] = {
    /* DD 5.25" */
    { UFT_IBMFORMAT_DD, 256,  0x12, 0x0C },
    { UFT_IBMFORMAT_DD, 256,  0x10, 0x32 },
    { UFT_IBMFORMAT_DD, 512,  0x08, 0x50 },
    { UFT_IBMFORMAT_DD, 512,  0x09, 0x50 },
    { UFT_IBMFORMAT_DD, 1024, 0x04, 0xF0 },
    { UFT_IBMFORMAT_DD, 2048, 0x02, 0xF0 },
    { UFT_IBMFORMAT_DD, 4096, 0x01, 0xF0 },
    /* HD 5.25" */
    { UFT_IBMFORMAT_DD, 256,  0x1A, 0x36 },
    { UFT_IBMFORMAT_DD, 512,  0x0F, 0x54 },
    { UFT_IBMFORMAT_DD, 512,  0x12, 0x6C },
    { UFT_IBMFORMAT_DD, 1024, 0x08, 0x74 },
    { UFT_IBMFORMAT_DD, 2048, 0x04, 0xF0 },
    { UFT_IBMFORMAT_DD, 4096, 0x02, 0xF0 },
    { UFT_IBMFORMAT_DD, 8192, 0x01, 0xF0 },
    /* DMF */
    { UFT_IBMFORMAT_DD, 512,  0x24, 0x53 },
    /* FM 8" */
    { UFT_IBMFORMAT_SD, 128,  0x1A, 0x1B },
    { UFT_IBMFORMAT_SD, 256,  0x0F, 0x2A },
    { UFT_IBMFORMAT_SD, 512,  0x08, 0x3A },
    { UFT_IBMFORMAT_SD, 1024, 0x04, 0x8A },
    { UFT_IBMFORMAT_SD, 2048, 0x02, 0xF8 },
    { UFT_IBMFORMAT_SD, 4096, 0x01, 0xF8 },
    /* End marker */
    { 0xFF, 0xFFFF, 0xFF, 0xFF }
};

/*===========================================================================*/
/*                           FLOPPY SIDE/TRACK STRUCTURES                     */
/*===========================================================================*/

/**
 * Floppy Side Structure (from HxC)
 */
typedef struct {
    int32_t   number_of_sector;     /* -1 if unknown */
    uint8_t  *databuffer;           /* Bit data buffer */
    int32_t   bitrate;              /* Use -1 for variable */
    uint32_t *timingbuffer;         /* Per-byte bitrate (if variable) */
    uint8_t  *flakybitsbuffer;      /* Weak/flakey bits mask */
    uint8_t  *indexbuffer;          /* Index signal per bit */
    uint8_t  *track_encoding_buffer;/* Encoding per region */
    int32_t   track_encoding;       /* Default encoding */
    int32_t   tracklen;             /* Length in bits */
    void     *stream_dump;          /* Original flux stream */
    uint32_t *cell_to_tick;         /* Bit position to tick mapping */
    int       tick_freq;            /* Tick frequency for timing */
} uft_libflux_side_t;

/**
 * Floppy Cylinder Structure
 */
typedef struct {
    int32_t       floppyRPM;
    int32_t       number_of_side;
    uft_libflux_side_t **sides;
} uft_libflux_cylinder_t;

/**
 * Complete Floppy Structure
 */
typedef struct {
    int32_t   floppyBitRate;
    int32_t   floppyNumberOfSide;
    int32_t   floppyNumberOfTrack;
    int32_t   floppySectorPerTrack;
    int32_t   floppyiftype;
    int32_t   double_step;
    uft_libflux_cylinder_t **tracks;
    uint32_t  flags;                /* Bit 0 = write protected */
} uft_libflux_floppy_t;

#ifdef __cplusplus
}
#endif

#endif /* UFT_LIBFLUX_ALGORITHMS_H */
