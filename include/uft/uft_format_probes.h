/**
 * @file uft_format_probes.h
 * @brief Unified Format Detection API
 * 
 * This header provides access to all format probe functions.
 * Each probe function analyzes raw disk data and returns a confidence score.
 * 
 * Usage:
 *   int confidence;
 *   if (d64_probe(data, size, file_size, &confidence) && confidence > 80) {
 *       // High confidence D64 detection
 *   }
 */

#ifndef UFT_FORMAT_PROBES_H
#define UFT_FORMAT_PROBES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Commodore Formats
 * ═══════════════════════════════════════════════════════════════════════════════ */

/** @brief Commodore 1541 (D64) - 170KB single-sided */
bool d64_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence);

/** @brief Commodore 1571 (D71) - 340KB double-sided */
bool d71_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence);

/** @brief Commodore 8050 (D80) - 500KB single-sided */
bool d80_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence);

/** @brief Commodore 1581 (D81) - 800KB 3.5" */
bool d81_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence);

/** @brief Commodore 8250 (D82) - 1MB double-sided */
bool d82_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence);

/** @brief Commodore GCR (G64) - raw GCR encoding */
bool g64_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence);

/** @brief Commodore 1571 GCR (G71) */
bool g71_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Atari Formats
 * ═══════════════════════════════════════════════════════════════════════════════ */

/** @brief Atari 8-bit (ATR) */
bool atr_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence);

/** @brief Atari ST Extended (STX) */
bool stx_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Flux Formats
 * ═══════════════════════════════════════════════════════════════════════════════ */

/** @brief SuperCard Pro (SCP) */
bool scp_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence);

/** @brief HxC Floppy Emulator (HFE) */
bool hfe_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence);

/** @brief HFE v3 format */
bool hfe_v3_probe(const uint8_t* data, size_t size);

/** @brief CAPS/SPS (IPF) */
bool ipf_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Apple Formats
 * ═══════════════════════════════════════════════════════════════════════════════ */

/** @brief Apple II WOZ */
bool woz_probe(const uint8_t* data, size_t size);

/** @brief Apple II Nibble (NIB) */
bool nib_probe(const uint8_t* data, size_t size);

/* ═══════════════════════════════════════════════════════════════════════════════
 * PC/IBM Formats
 * ═══════════════════════════════════════════════════════════════════════════════ */

/** @brief Raw sector image (IMG/IMA) */
bool img_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence);

/** @brief Teledisk (TD0) */
bool td0_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence);

/** @brief ImageDisk (IMD) */
bool imd_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence);

/** @brief CopyQM (CQM) */
bool cqm_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence);

/** @brief Formatted Disk Image (FDI) */
bool fdi_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Japanese Formats
 * ═══════════════════════════════════════════════════════════════════════════════ */

/** @brief NEC PC-88/98 (D88) */
bool d88_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence);

/* ═══════════════════════════════════════════════════════════════════════════════
 * TRS-80 / Tandy Formats
 * ═══════════════════════════════════════════════════════════════════════════════ */

/** @brief DMK (TRS-80) */
bool dmk_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence);

/* ═══════════════════════════════════════════════════════════════════════════════
 * British Micro Formats
 * ═══════════════════════════════════════════════════════════════════════════════ */

/** @brief BBC Micro (SSD/DSD) */
bool ssd_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence);

/** @brief SAM Coupé (SAD) */
bool sad_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence);

/** @brief Amstrad CPC (DSK) */
bool dsk_cpc_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Eastern European Formats
 * ═══════════════════════════════════════════════════════════════════════════════ */

/** @brief TR-DOS / ZX Spectrum (TRD) */
bool trd_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Multi-Format Detection
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Probe result structure
 */
typedef struct {
    int format_id;          /**< Detected format ID */
    int confidence;         /**< Confidence 0-100 */
    const char* format_name;/**< Human-readable name */
    const char* extension;  /**< File extension */
    bool is_flux;           /**< True if flux format */
    bool has_protection;    /**< Protection detected */
} uft_probe_result_t;

/**
 * @brief Probe all formats and return best match
 */
int uft_probe_all(const uint8_t* data, size_t size, 
                  size_t file_size, uft_probe_result_t* results, int max_results);

/**
 * @brief Probe specific format
 */
int uft_probe_specific(const uint8_t* data, size_t size,
                       size_t file_size, int format_id, int* confidence);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FORMAT_PROBES_H */
