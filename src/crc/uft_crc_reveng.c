/**
 * @file uft_crc_reveng.c
 * @brief CRC Reverse Engineering Integration
 * 
 * EXT2-015: CRC polynomial detection and reverse engineering
 * 
 * Features:
 * - CRC polynomial detection
 * - Parameter brute-force
 * - Known-good verification
 * - Multi-algorithm testing
 */

#include "uft/crc/uft_crc_reveng.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*===========================================================================
 * Constants
 *===========================================================================*/

#define MAX_CRC_WIDTH       32
#define MAX_SEARCH_POLY     0xFFFFFFFF
#define MAX_TEST_SAMPLES    16

/*===========================================================================
 * Known CRC Parameters
 *===========================================================================*/

/* Common polynomials for brute-force */
static const uint32_t common_polys_8[] = {
    0x07, 0x1D, 0x31, 0x39, 0x9B, 0xD5, 0
};

static const uint32_t common_polys_16[] = {
    0x1021, 0x8005, 0x0589, 0x3D65, 0xC867, 0x8BB7, 0
};

static const uint32_t common_polys_32[] = {
    0x04C11DB7, 0x1EDC6F41, 0xA833982B, 0x814141AB, 0x000000AF, 0
};

/*===========================================================================
 * CRC Calculation (for reverse engineering)
 *===========================================================================*/

static uint32_t calc_crc(const uint8_t *data, size_t len,
                         int width, uint32_t poly, uint32_t init,
                         uint32_t xor_out, bool ref_in, bool ref_out)
{
    uint32_t crc = init;
    uint32_t mask = (width < 32) ? ((1U << width) - 1) : 0xFFFFFFFF;
    uint32_t topbit = 1U << (width - 1);
    
    for (size_t i = 0; i < len; i++) {
        uint8_t byte = data[i];
        
        if (ref_in) {
            /* Reflect input byte */
            byte = ((byte & 0x80) >> 7) | ((byte & 0x40) >> 5) |
                   ((byte & 0x20) >> 3) | ((byte & 0x10) >> 1) |
                   ((byte & 0x08) << 1) | ((byte & 0x04) << 3) |
                   ((byte & 0x02) << 5) | ((byte & 0x01) << 7);
        }
        
        crc ^= ((uint32_t)byte << (width - 8));
        
        for (int bit = 0; bit < 8; bit++) {
            if (crc & topbit) {
                crc = (crc << 1) ^ poly;
            } else {
                crc <<= 1;
            }
        }
    }
    
    if (ref_out) {
        /* Reflect output */
        uint32_t reflected = 0;
        for (int i = 0; i < width; i++) {
            if (crc & (1U << i)) {
                reflected |= (1U << (width - 1 - i));
            }
        }
        crc = reflected;
    }
    
    return (crc ^ xor_out) & mask;
}

/*===========================================================================
 * Polynomial Detection
 *===========================================================================*/

int uft_crc_detect_poly(const uft_crc_sample_t *samples, size_t count,
                        int width, uft_crc_result_t *result)
{
    if (!samples || !result || count < 2) return -1;
    if (width != 8 && width != 16 && width != 32) return -1;
    
    memset(result, 0, sizeof(*result));
    result->width = width;
    
    /* Select polynomial list based on width */
    const uint32_t *polys;
    switch (width) {
        case 8:  polys = common_polys_8; break;
        case 16: polys = common_polys_16; break;
        case 32: polys = common_polys_32; break;
        default: return -1;
    }
    
    /* Try each polynomial */
    for (int p = 0; polys[p] != 0; p++) {
        uint32_t poly = polys[p];
        
        /* Try different init/xor combinations */
        uint32_t inits[] = {0, 0xFFFFFFFF};
        uint32_t xors[] = {0, 0xFFFFFFFF};
        bool refs[] = {false, true};
        
        for (int i = 0; i < 2; i++) {
            for (int x = 0; x < 2; x++) {
                for (int ri = 0; ri < 2; ri++) {
                    for (int ro = 0; ro < 2; ro++) {
                        /* Test all samples */
                        bool all_match = true;
                        
                        for (size_t s = 0; s < count && all_match; s++) {
                            uint32_t computed = calc_crc(
                                samples[s].data, samples[s].length,
                                width, poly, inits[i], xors[x],
                                refs[ri], refs[ro]
                            );
                            
                            if (computed != samples[s].crc) {
                                all_match = false;
                            }
                        }
                        
                        if (all_match) {
                            result->found = true;
                            result->poly = poly;
                            result->init = inits[i];
                            result->xor_out = xors[x];
                            result->ref_in = refs[ri];
                            result->ref_out = refs[ro];
                            result->confidence = 100.0;
                            return 0;
                        }
                    }
                }
            }
        }
    }
    
    return -1;  /* Not found */
}

/*===========================================================================
 * Brute-Force Search
 *===========================================================================*/

int uft_crc_brute_force(const uft_crc_sample_t *samples, size_t count,
                        int width, uint32_t poly_start, uint32_t poly_end,
                        uft_crc_result_t *result,
                        void (*progress)(uint32_t current, uint32_t total, void *ctx),
                        void *progress_ctx)
{
    if (!samples || !result || count < 2) return -1;
    if (width < 8 || width > 32) return -1;
    
    memset(result, 0, sizeof(*result));
    result->width = width;
    
    uint32_t mask = (width < 32) ? ((1U << width) - 1) : 0xFFFFFFFF;
    uint32_t total = (poly_end - poly_start);
    uint32_t checked = 0;
    
    for (uint32_t poly = poly_start; poly <= poly_end; poly++) {
        /* Progress callback */
        if (progress && (checked % 10000 == 0)) {
            progress(checked, total, progress_ctx);
        }
        checked++;
        
        /* Test polynomial with common configurations */
        uint32_t configs[][4] = {
            {0, 0, 0, 0},                   /* No reflection, no XOR */
            {0xFFFFFFFF, 0xFFFFFFFF, 0, 0}, /* Init/XOR, no reflection */
            {0xFFFFFFFF, 0xFFFFFFFF, 1, 1}, /* Full reflection */
            {0, 0, 1, 1},                   /* Reflection only */
        };
        
        for (int c = 0; c < 4; c++) {
            bool all_match = true;
            
            for (size_t s = 0; s < count && all_match; s++) {
                uint32_t computed = calc_crc(
                    samples[s].data, samples[s].length,
                    width, poly, 
                    configs[c][0] & mask, 
                    configs[c][1] & mask,
                    configs[c][2], configs[c][3]
                );
                
                if (computed != (samples[s].crc & mask)) {
                    all_match = false;
                }
            }
            
            if (all_match) {
                result->found = true;
                result->poly = poly;
                result->init = configs[c][0] & mask;
                result->xor_out = configs[c][1] & mask;
                result->ref_in = configs[c][2];
                result->ref_out = configs[c][3];
                result->confidence = 100.0;
                return 0;
            }
        }
    }
    
    return -1;
}

/*===========================================================================
 * CRC Verification
 *===========================================================================*/

int uft_crc_verify_params(const uft_crc_result_t *params,
                          const uint8_t *data, size_t length,
                          uint32_t expected_crc)
{
    if (!params || !data) return -1;
    
    uint32_t computed = calc_crc(data, length, params->width,
                                 params->poly, params->init,
                                 params->xor_out, params->ref_in,
                                 params->ref_out);
    
    return (computed == expected_crc) ? 0 : -1;
}

/*===========================================================================
 * Algorithm Identification
 *===========================================================================*/

const char *uft_crc_identify_algorithm(const uft_crc_result_t *params)
{
    if (!params || !params->found) return "Unknown";
    
    /* Check against known algorithms */
    struct {
        const char *name;
        int width;
        uint32_t poly;
        uint32_t init;
        uint32_t xor_out;
        bool ref;
    } known[] = {
        {"CRC-8", 8, 0x07, 0, 0, false},
        {"CRC-8/MAXIM", 8, 0x31, 0, 0, true},
        {"CRC-16/CCITT", 16, 0x1021, 0xFFFF, 0, false},
        {"CRC-16/XMODEM", 16, 0x1021, 0, 0, false},
        {"CRC-16/MODBUS", 16, 0x8005, 0xFFFF, 0, true},
        {"CRC-32", 32, 0x04C11DB7, 0xFFFFFFFF, 0xFFFFFFFF, true},
        {"CRC-32C", 32, 0x1EDC6F41, 0xFFFFFFFF, 0xFFFFFFFF, true},
        {NULL, 0, 0, 0, 0, false}
    };
    
    for (int i = 0; known[i].name != NULL; i++) {
        if (params->width == known[i].width &&
            params->poly == known[i].poly &&
            params->init == known[i].init &&
            params->xor_out == known[i].xor_out &&
            params->ref_in == known[i].ref) {
            return known[i].name;
        }
    }
    
    return "Custom";
}

/*===========================================================================
 * Report
 *===========================================================================*/

int uft_crc_result_to_string(const uft_crc_result_t *result,
                             char *buffer, size_t size)
{
    if (!result || !buffer) return -1;
    
    if (!result->found) {
        return snprintf(buffer, size, "CRC parameters not found");
    }
    
    return snprintf(buffer, size,
        "CRC-%d: poly=0x%0*X init=0x%0*X xor=0x%0*X ref_in=%s ref_out=%s (%s)",
        result->width,
        result->width / 4, result->poly,
        result->width / 4, result->init,
        result->width / 4, result->xor_out,
        result->ref_in ? "true" : "false",
        result->ref_out ? "true" : "false",
        uft_crc_identify_algorithm(result)
    );
}
