/**
 * @file uft_supercopy_detect.h
 * @brief SuperCopy v3.40 CP/M Format Detection API
 *
 * Public interface for CP/M disk format detection using the SuperCopy
 * format database (301 formats, Oliver Müller, 1991).
 *
 * Usage:
 *   sc_detect_result_t result;
 *   uint16_t n = sc_detect_by_geometry(512, 9, 2, 80, &result);
 *   if (n == 1) { // unique match }
 *   else { sc_detect_refine(&result, boot, blen, dir, dlen); }
 *
 * @version 4.0.0
 */

#ifndef UFT_SUPERCOPY_DETECT_H
#define UFT_SUPERCOPY_DETECT_H

#include "uft/formats/supercopy_formats.h"
#include "uft/formats/uft_cpm_defs.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SC_MAX_CANDIDATES 32

/* ---- Confidence levels ---- */
typedef enum {
    SC_CONFIDENCE_NONE      = 0,
    SC_CONFIDENCE_GEOMETRY  = 30,
    SC_CONFIDENCE_DENSITY   = 40,
    SC_CONFIDENCE_CAPACITY  = 50,
    SC_CONFIDENCE_DPB_GUESS = 60,
    SC_CONFIDENCE_DIRECTORY = 75,
    SC_CONFIDENCE_BOOT      = 80,
    SC_CONFIDENCE_FULL      = 90,
    SC_CONFIDENCE_UNIQUE    = 99,
} sc_confidence_t;

/* ---- Candidate ---- */
typedef struct {
    const supercopy_format_t *sc_format;
    const cpm_format_def_t   *cpm_def;
    sc_confidence_t           confidence;
    uint16_t                  block_size;
    uint8_t                   dir_entries;
    uint8_t                   off_tracks;
} sc_candidate_t;

/* ---- Result ---- */
typedef struct {
    sc_candidate_t candidates[SC_MAX_CANDIDATES];
    uint16_t       count;
    uint16_t       best_index;
} sc_detect_result_t;

/* ---- Detection API ---- */
uint16_t sc_detect_by_geometry(uint16_t sector_size, uint8_t spt,
                               uint8_t heads, uint16_t cylinders,
                               sc_detect_result_t *result);

void sc_detect_refine(sc_detect_result_t *result,
                      const uint8_t *boot_sector, size_t boot_len,
                      const uint8_t *dir_sector,  size_t dir_len);

void sc_detect_print(const sc_detect_result_t *result, FILE *out);

/* ---- Statistics ---- */
void sc_get_stats(uint16_t *total_formats, uint16_t *unique_geometries,
                  uint16_t *sd_formats, uint16_t *dd_formats,
                  uint16_t *hd_formats);

/* ---- Iteration ---- */
uint16_t sc_iterate_by_density(uint8_t density,
    void (*callback)(const supercopy_format_t *fmt, void *user),
    void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* UFT_SUPERCOPY_DETECT_H */
