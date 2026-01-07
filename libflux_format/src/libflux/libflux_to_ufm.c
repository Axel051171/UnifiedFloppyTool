// SPDX-License-Identifier: MIT
/*
 * libflux_to_ufm.c - HXC to UFM Converter
 * 
 * Converts HXC formats to Universal Flux Model.
 * This enables integration with the complete UFT ecosystem.
 * 
 * Conversion Strategy:
 *   - HFE MFM track → MFM decode → Flux transitions
 *   - IBM sectors → Sector data preservation
 *   - Metadata → UFM metadata
 * 
 * @version 2.7.5
 * @date 2024-12-25
 */

#include "uft/uft_error.h"
#include "libflux_format.h""
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/*============================================================================*
 * UFM STRUCTURES (Simplified - full UFM in separate module)
 *============================================================================*/

typedef struct {
    uint32_t timing_ns;
    uint8_t strength;
    uint8_t flags;
} ufm_flux_t;

typedef struct {
    ufm_flux_t *transitions;
    uint32_t count;
    uint8_t cylinder;
    uint8_t head;
    uint32_t rpm;
} ufm_track_t;

typedef struct {
    ufm_track_t *tracks;
    uint32_t track_count;
    char *creator;
    uint16_t bitrate_kbps;
    uint8_t encoding;
} ufm_image_t;

/*============================================================================*
 * TIMING CALCULATIONS
 *============================================================================*/

/**
 * @brief Calculate cell time from bitrate
 * 
 * @param bitrate_kbps Bitrate in Kbps
 * @return Cell time in nanoseconds
 */
static uint32_t calculate_cell_time_ns(uint16_t bitrate_kbps)
{
    /* Cell time (ns) = 1,000,000 / bitrate_kbps */
    if (bitrate_kbps == 0) {
        bitrate_kbps = 250;  /* Default: 250 Kbps */
    }
    
    return (uint32_t)(1000000000ULL / (bitrate_kbps * 1000));
}

/*============================================================================*
 * MFM TO FLUX CONVERSION
 *============================================================================*/

/**
 * @brief Convert MFM bitstream to flux transitions
 * 
 * Algorithm:
 *   1. Read MFM bits
 *   2. For each '1' bit → flux transition
 *   3. For '0' bits → accumulate time
 *   4. Apply timing from bitrate
 * 
 * @param mfm_bits MFM bitstream
 * @param mfm_bit_count Number of MFM bits
 * @param bitrate_kbps Bitrate
 * @param flux_out Flux transitions (allocated)
 * @param flux_count_out Number of transitions
 * @return 0 on success
 */
static int mfm_to_flux(
    const uint8_t *mfm_bits,
    size_t mfm_bit_count,
    uint16_t bitrate_kbps,
    ufm_flux_t **flux_out,
    uint32_t *flux_count_out
) {
    if (!mfm_bits || !flux_out || !flux_count_out) {
        return -1;
    }
    
    uint32_t cell_time_ns = calculate_cell_time_ns(bitrate_kbps);
    
    /* Allocate max possible transitions */
    ufm_flux_t *flux = calloc(mfm_bit_count, sizeof(*flux));
    if (!flux) {
        return -1;
    }
    
    uint32_t flux_idx = 0;
    uint32_t accumulated_time = 0;
    
    /* Process each MFM bit */
    for (size_t bit_pos = 0; bit_pos < mfm_bit_count; bit_pos++) {
        size_t byte_idx = bit_pos / 8;
        size_t bit_in_byte = 7 - (bit_pos % 8);
        
        if (byte_idx >= (mfm_bit_count + 7) / 8) {
            break;
        }
        
        uint8_t bit = (mfm_bits[byte_idx] >> bit_in_byte) & 1;
        
        accumulated_time += cell_time_ns;
        
        if (bit == 1) {
            /* Flux transition! */
            flux[flux_idx].timing_ns = accumulated_time;
            flux[flux_idx].strength = 255;
            flux[flux_idx].flags = 0;
            
            flux_idx++;
            accumulated_time = 0;
        }
    }
    
    /* Shrink to actual size */
    if (flux_idx > 0) {
        ufm_flux_t *final = realloc(flux, flux_idx * sizeof(*flux));
        if (final) {
            flux = final;
        }
    } else {
        free(flux);
        flux = NULL;
    }
    
    *flux_out = flux;
    *flux_count_out = flux_idx;
    
    return 0;
}

/*============================================================================*
 * HFE TO UFM CONVERSION
 *============================================================================*/

/**
 * @brief Convert HFE to UFM
 * 
 * Professional HFE → UFM converter with complete metadata preservation.
 * 
 * @param hfe HFE image
 * @param ufm_out UFM image (output)
 * @return 0 on success
 */
int libflux_hfe_to_ufm(
    const libflux_hfe_image_t *hfe,
    ufm_image_t *ufm_out
) {
    if (!hfe || !ufm_out) {
        return -1;
    }
    
    memset(ufm_out, 0, sizeof(*ufm_out));
    
    /* Allocate tracks */
    ufm_out->tracks = calloc(hfe->track_count, sizeof(*ufm_out->tracks));
    if (!ufm_out->tracks) {
        return -1;
    }
    
    ufm_out->track_count = hfe->track_count;
    
    /* Convert each track */
    for (uint32_t i = 0; i < hfe->track_count; i++) {
        const libflux_track_t *hfe_track = &hfe->tracks[i];
        ufm_track_t *ufm_track = &ufm_out->tracks[i];
        
        if (!hfe_track->data || hfe_track->size == 0) {
            continue;
        }
        
        /* Convert MFM to flux */
        size_t bit_count = hfe_track->size * 8;
        
        int rc = mfm_to_flux(
            hfe_track->data,
            bit_count,
            hfe_track->bitrate ? hfe_track->bitrate : hfe->bitrate_kbps,
            &ufm_track->transitions,
            &ufm_track->count
        );
        
        if (rc < 0) {
            /* Cleanup on error */
            for (uint32_t j = 0; j < i; j++) {
                free(ufm_out->tracks[j].transitions);
            }
            free(ufm_out->tracks);
            return -1;
        }
        
        /* Set track metadata */
        ufm_track->cylinder = i / hfe->number_of_sides;
        ufm_track->head = i % hfe->number_of_sides;
        ufm_track->rpm = hfe->rpm;
    }
    
    /* Copy metadata */
    ufm_out->bitrate_kbps = hfe->bitrate_kbps;
    ufm_out->encoding = hfe->track_encoding;
    
    /* Create creator string */
    char creator_buf[64];
    snprintf(creator_buf, sizeof(creator_buf),
             "HFE v%u (%s, %u Kbps, %u RPM)",
             hfe->format_revision,
             libflux_get_encoding_name(hfe->track_encoding),
             hfe->bitrate_kbps,
             hfe->rpm);
    
    ufm_out->creator = strdup(creator_buf);
    
    return 0;
}

/**
 * @brief Free UFM image
 */
void libflux_free_ufm(ufm_image_t *ufm)
{
    if (!ufm) {
        return;
    }
    
    if (ufm->tracks) {
        for (uint32_t i = 0; i < ufm->track_count; i++) {
            free(ufm->tracks[i].transitions);
        }
        free(ufm->tracks);
    }
    
    free(ufm->creator);
    
    memset(ufm, 0, sizeof(*ufm));
}

/**
 * @brief Print UFM statistics
 */
void libflux_ufm_print_stats(const ufm_image_t *ufm)
{
    if (!ufm) {
        return;
    }
    
    printf("UFM Conversion Statistics:\n");
    printf("  Bitrate:    %u Kbps\n", ufm->bitrate_kbps);
    printf("  Encoding:   %s\n", libflux_get_encoding_name(ufm->encoding));
    printf("  Tracks:     %u\n", ufm->track_count);
    
    if (ufm->creator) {
        printf("  Creator:    %s\n", ufm->creator);
    }
    
    printf("\n");
    
    /* Calculate statistics */
    uint64_t total_transitions = 0;
    uint64_t total_time_ns = 0;
    uint32_t non_empty = 0;
    
    for (uint32_t i = 0; i < ufm->track_count; i++) {
        const ufm_track_t *t = &ufm->tracks[i];
        
        if (t->count > 0) {
            non_empty++;
            total_transitions += t->count;
            
            for (uint32_t j = 0; j < t->count; j++) {
                total_time_ns += t->transitions[j].timing_ns;
            }
        }
    }
    
    printf("Track Statistics:\n");
    printf("  Non-empty:       %u\n", non_empty);
    printf("  Total transitions: %llu\n", (unsigned long long)total_transitions);
    printf("  Total time:      %.2f ms\n", total_time_ns / 1000000.0);
    
    if (total_transitions > 0) {
        uint32_t avg = (uint32_t)(total_time_ns / total_transitions);
        printf("  Avg timing:      %u ns\n", avg);
    }
    
    printf("\n");
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "Linux"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
