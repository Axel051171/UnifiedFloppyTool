/**
 * @file uft_atarist_copylock.c
 * @brief Atari ST CopyLock Protection Implementation
 * 
 * Implementation of Rob Northen CopyLock detection for Atari ST.
 * Series 1 (1988) and Series 2 (1989) support.
 * 
 * @version 1.0.0
 * @date 2026-01-04
 * @author UFT Team
 * 
 * SPDX-License-Identifier: MIT
 */

#include "uft/protection/uft_atarist_copylock.h"
#include <stdio.h>
#include <stdlib.h>

/*===========================================================================
 * Series 1 (1988) Functions
 *===========================================================================*/

/**
 * @brief Decode Series 1 encrypted code block
 */
int uft_copylock88_decode_block(const uint8_t *src, uint8_t *dst, 
                                 size_t len, size_t start_offset)
{
    if (!src || !dst || len < 8) {
        return -1;
    }
    
    /* Copy first 4 bytes as-is (seed) */
    if (start_offset == 0) {
        memcpy(dst, src, 4);
        start_offset = 4;
    }
    
    /* Decode remaining instructions */
    for (size_t i = start_offset; i + 4 <= len; i += 4) {
        uint32_t decoded = uft_copylock88_decode_instr(src + i);
        dst[i]   = (uint8_t)(decoded >> 24);
        dst[i+1] = (uint8_t)(decoded >> 16);
        dst[i+2] = (uint8_t)(decoded >> 8);
        dst[i+3] = (uint8_t)(decoded);
    }
    
    return 0;
}

/*===========================================================================
 * Series 2 (1989) Functions
 *===========================================================================*/

/**
 * @brief Decode Series 2 encrypted code block
 */
int uft_copylock89_decode_block(const uint8_t *src, uint8_t *dst,
                                 size_t len, uint32_t magic32,
                                 size_t start_offset)
{
    if (!src || !dst || len < 8) {
        return -1;
    }
    
    /* Copy first 4 bytes as-is (seed for key derivation) */
    if (start_offset == 0) {
        memcpy(dst, src, 4);
        start_offset = 4;
    }
    
    /* Decode remaining instructions */
    for (size_t i = start_offset; i + 4 <= len; i += 4) {
        uint32_t decoded = uft_copylock89_decode_instr(src + i, magic32);
        dst[i]   = (uint8_t)(decoded >> 24);
        dst[i+1] = (uint8_t)(decoded >> 16);
        dst[i+2] = (uint8_t)(decoded >> 8);
        dst[i+3] = (uint8_t)(decoded);
    }
    
    return 0;
}

/**
 * @brief Encode instruction for Series 2 (for patching)
 */
void uft_copylock89_encode_instr(uint8_t *buf, uint32_t magic32, 
                                  uint32_t instr)
{
    if (!buf) return;
    
    /* Get key from previous instruction */
    uint32_t key32 = ((uint32_t)buf[-4] << 24) | ((uint32_t)buf[-3] << 16) |
                     ((uint32_t)buf[-2] << 8)  | ((uint32_t)buf[-1]);
    key32 += magic32;
    
    /* Encrypt and write */
    uint32_t encrypted = instr ^ key32;
    buf[0] = (uint8_t)(encrypted >> 24);
    buf[1] = (uint8_t)(encrypted >> 16);
    buf[2] = (uint8_t)(encrypted >> 8);
    buf[3] = (uint8_t)(encrypted);
}

/*===========================================================================
 * Detection Functions
 *===========================================================================*/

/**
 * @brief Full CopyLock detection with detailed analysis
 */
int uft_copylock_st_analyze(const uint8_t *data, size_t data_len,
                             uft_copylock_st_result_t *result)
{
    if (!data || !result || data_len < 100) {
        return -1;
    }
    
    /* Run detection */
    if (!uft_copylock_st_detect(data, data_len, result)) {
        return 1;  /* Not CopyLock */
    }
    
    /* Additional analysis based on series */
    if (result->series == UFT_COPYLOCK_SERIES_2_1989) {
        /* For Series 2, we have magic32 - can decode more */
        if (result->start_off >= 0 && result->magic32 != 0) {
            snprintf(result->info, sizeof(result->info),
                     "Series 2 detected, magic32=0x%08X, variant=%c",
                     result->magic32, 'a' + result->variant - 1);
        }
    } else if (result->series == UFT_COPYLOCK_SERIES_1_1988) {
        /* Series 1 analysis */
        if (result->keydisk_off >= 0) {
            result->type = UFT_COPYLOCK_TYPE_INTERNAL;
            snprintf(result->info, sizeof(result->info),
                     "Series 1 internal type, keydisk at offset 0x%X",
                     result->keydisk_off);
        }
    }
    
    return 0;
}

/**
 * @brief Get series name string
 */
const char* uft_copylock_series_name(uft_copylock_series_t series)
{
    switch (series) {
        case UFT_COPYLOCK_SERIES_1_1988:
            return "Rob Northen CopyLock Series 1 (1988)";
        case UFT_COPYLOCK_SERIES_2_1989:
            return "Rob Northen CopyLock Series 2 (1989)";
        default:
            return "Unknown";
    }
}

/**
 * @brief Print detection result to FILE
 */
void uft_copylock_st_print_result(FILE *out, 
                                   const uft_copylock_st_result_t *result)
{
    if (!out || !result) return;
    
    fprintf(out, "=== CopyLock Detection Result ===\n");
    fprintf(out, "Detected:       %s\n", result->detected ? "YES" : "NO");
    
    if (!result->detected) return;
    
    fprintf(out, "Protection:     %s\n", result->name);
    fprintf(out, "Series:         %s\n", 
            uft_copylock_series_name(result->series));
    fprintf(out, "Variant:        %c\n", 'a' + result->variant - 1);
    fprintf(out, "Type:           %s\n",
            result->type == UFT_COPYLOCK_TYPE_WRAPPER ? "Wrapper" : "Internal");
    
    if (result->series == UFT_COPYLOCK_SERIES_2_1989) {
        fprintf(out, "Magic32:        0x%08X\n", result->magic32);
    }
    
    if (result->start_off >= 0) {
        fprintf(out, "Start offset:   0x%X\n", result->start_off);
    }
    if (result->keydisk_off >= 0) {
        fprintf(out, "Keydisk offset: 0x%X\n", result->keydisk_off);
    }
    if (result->serial_off >= 0) {
        fprintf(out, "Serial offset:  0x%X\n", result->serial_off);
        fprintf(out, "Serial usage:   %s\n", 
                uft_copylock_serial_usage_str(result->serial_usage));
    }
    if (result->prog_off >= 0) {
        fprintf(out, "Program offset: 0x%X\n", result->prog_off);
    }
    
    if (result->info[0]) {
        fprintf(out, "Info:           %s\n", result->info);
    }
}
