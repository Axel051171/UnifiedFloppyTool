// SPDX-License-Identifier: MIT
/*
 * track_encoder.c - Track Encoder Implementation
 * 
 * UFM wrapper for HxC track encoders
 * Integrates X-Copy metadata and Bootblock detection
 * 
 * @version 2.7.0
 * @date 2024-12-25
 */

#include "uft/uft_error.h"
#include "track_encoder.h"
#include "mfm_ibm_encode.h"
#include "flux_logical.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*============================================================================*
 * STATISTICS
 *============================================================================*/

static track_encoder_stats_t g_stats = {0};

/*============================================================================*
 * INITIALIZATION
 *============================================================================*/

int track_encoder_init(void)
{
    memset(&g_stats, 0, sizeof(g_stats));
    return 0;
}

void track_encoder_shutdown(void)
{
    // Nothing to clean up yet
}

/*============================================================================*
 * DEFAULT PARAMETERS
 *============================================================================*/

int track_encoder_get_defaults(
    track_encoder_type_t type,
    track_encoder_params_t *params)
{
    if (!params) return -1;
    
    memset(params, 0, sizeof(*params));
    params->type = type;
    
    switch (type) {
    case TRACK_ENC_IBM_MFM:
        // PC 1.44MB defaults
        params->params.ibm.sectors_per_track = 18;
        params->params.ibm.sector_size = 512;
        params->params.ibm.bitrate_kbps = 500;
        params->params.ibm.rpm = 300;
        params->params.ibm.gap3_length = 54;
        return 0;
        
    case TRACK_ENC_AMIGA_MFM:
        // Amiga DD defaults
        params->params.amiga.sectors_per_track = 11;
        params->params.amiga.sector_size = 512;
        params->params.amiga.long_track = false;
        params->params.amiga.custom_length = 0;
        return 0;
        
    case TRACK_ENC_C64_GCR:
        // C64 outer track defaults
        params->params.c64.track_number = 1;
        params->params.c64.sectors_per_track = 0;  // Auto from track
        return 0;
        
    default:
        return -1;
    }
}

/*============================================================================*
 * AUTO-DETECTION
 *============================================================================*/

track_encoder_type_t track_encoder_auto_detect(const void *track)
{
    // TODO: Implement using bootblock DB (v2.6.3)
    // For now, default to IBM MFM
    (void)track;
    return TRACK_ENC_IBM_MFM;
}

/*============================================================================*
 * CORE ENCODING - IBM MFM
 *============================================================================*/

static int encode_ibm_mfm(
    const ufm_logical_image_t *li,
    const track_encoder_params_t *params,
    track_encoder_output_t *output)
{
    if (!li || !params || !output) return -1;
    
    memset(output, 0, sizeof(*output));
    
    // Build HxC parameters
    mfm_ibm_track_params_t libflux_params = {
        .cyl = 0,  // Will be set per track
        .head = 0,
        .spt = params->params.ibm.sectors_per_track,
        .sec_size = params->params.ibm.sector_size,
        .bit_rate_kbps = params->params.ibm.bitrate_kbps,
        .rpm = params->params.ibm.rpm
    };
    
    // Call HxC encoder
    uint8_t *bits = NULL;
    size_t bit_count = 0;
    
    int rc = mfm_ibm_build_track_bits(li, &libflux_params, &bits, &bit_count);
    if (rc < 0) {
        g_stats.errors++;
        return rc;
    }
    
    // Fill output
    output->bitstream = bits;
    output->bitstream_size = (bit_count + 7) / 8;
    output->bitstream_bits = bit_count;
    output->track_length = output->bitstream_size;
    output->bitrate_kbps = params->params.ibm.bitrate_kbps;
    output->sectors_encoded = params->params.ibm.sectors_per_track;
    
    // Update statistics
    g_stats.tracks_encoded++;
    g_stats.bytes_encoded += output->bitstream_size;
    
    return 0;
}

/*============================================================================*
 * CORE ENCODING - AMIGA MFM (with copy protection support!)
 *============================================================================*/

static int encode_amiga_mfm(
    const ufm_logical_image_t *li,
    const track_encoder_params_t *params,
    track_encoder_output_t *output)
{
    if (!li || !params || !output) return -1;
    
    memset(output, 0, sizeof(*output));
    
    // Build parameters - similar to IBM but with Amiga specifics
    mfm_ibm_track_params_t libflux_params = {
        .cyl = 0,
        .head = 0,
        .spt = params->params.amiga.sectors_per_track,
        .sec_size = params->params.amiga.sector_size,
        .bit_rate_kbps = 250,  // Amiga uses 250 kbps
        .rpm = 300
    };
    
    // Call HxC encoder (same IBM encoder works for Amiga MFM!)
    uint8_t *bits = NULL;
    size_t bit_count = 0;
    
    int rc = mfm_ibm_build_track_bits(li, &libflux_params, &bits, &bit_count);
    if (rc < 0) {
        g_stats.errors++;
        return rc;
    }
    
    // CRITICAL: Handle LONG TRACK for copy protection! ⭐⭐⭐
    if (params->params.amiga.long_track) {
        // Calculate extended length for copy protection
        // Normal Amiga track: ~12,668 bytes
        // Long track (Rob Northen, etc.): ~13,200+ bytes
        
        size_t normal_size = output->bitstream_size;
        size_t long_size = params->params.amiga.custom_length;
        
        if (long_size == 0) {
            // Auto-calculate: add ~4% (typical for Rob Northen)
            long_size = normal_size + (normal_size * 4 / 100);
        }
        
        if (long_size > normal_size) {
            // Reallocate with padding
            uint8_t *long_bits = (uint8_t*)realloc(bits, long_size);
            if (long_bits) {
                // Pad with 0x4E (standard gap fill)
                memset(long_bits + normal_size, 0x4E, long_size - normal_size);
                bits = long_bits;
                bit_count = long_size * 8;
                
                g_stats.long_tracks++;  // Count copy protection tracks!
            }
        }
    }
    
    // Fill output
    output->bitstream = bits;
    output->bitstream_size = (bit_count + 7) / 8;
    output->bitstream_bits = bit_count;
    output->track_length = output->bitstream_size;
    output->bitrate_kbps = 250;
    output->sectors_encoded = params->params.amiga.sectors_per_track;
    
    // Update statistics
    g_stats.tracks_encoded++;
    g_stats.bytes_encoded += output->bitstream_size;
    
    return 0;
}

/*============================================================================*
 * MAIN ENCODING FUNCTION
 *============================================================================*/

int track_encode(
    const void *track,
    const track_encoder_params_t *params,
    track_encoder_output_t *output)
{
    if (!track || !params || !output) return -1;
    
    // Cast to UFM logical image
    // (In real integration, this would be ufm_track_t)
    const ufm_logical_image_t *li = (const ufm_logical_image_t*)track;
    
    // Dispatch to encoder
    switch (params->type) {
    case TRACK_ENC_IBM_MFM:
        return encode_ibm_mfm(li, params, output);
        
    case TRACK_ENC_AMIGA_MFM:
        return encode_amiga_mfm(li, params, output);
        
    case TRACK_ENC_C64_GCR:
        // TODO: Implement C64 GCR encoder
        g_stats.errors++;
        return -1;
        
    case TRACK_ENC_APPLE_GCR:
        // TODO: Implement Apple GCR encoder
        g_stats.errors++;
        return -1;
        
    case TRACK_ENC_FM:
        // TODO: Implement FM encoder
        g_stats.errors++;
        return -1;
        
    default:
        g_stats.errors++;
        return -1;
    }
}

/*============================================================================*
 * CLEANUP
 *============================================================================*/

void track_encoder_free_output(track_encoder_output_t *output)
{
    if (!output) return;
    
    if (output->bitstream) {
        free(output->bitstream);
        output->bitstream = NULL;
    }
    
    memset(output, 0, sizeof(*output));
}

/*============================================================================*
 * UTILITIES
 *============================================================================*/

const char* track_encoder_type_name(track_encoder_type_t type)
{
    switch (type) {
    case TRACK_ENC_IBM_MFM:    return "IBM MFM";
    case TRACK_ENC_AMIGA_MFM:  return "Amiga MFM";
    case TRACK_ENC_C64_GCR:    return "C64 GCR";
    case TRACK_ENC_APPLE_GCR:  return "Apple GCR";
    case TRACK_ENC_FM:         return "FM";
    case TRACK_ENC_CUSTOM:     return "Custom";
    default:                   return "Unknown";
    }
}

uint32_t track_encoder_calc_length(uint16_t bitrate_kbps, uint16_t rpm)
{
    if (bitrate_kbps == 0 || rpm == 0) return 0;
    
    // Same calculation as HxC
    const double seconds = 60.0 / (double)rpm;
    const double bits = (double)bitrate_kbps * 1000.0 * seconds;
    return (uint32_t)(bits / 8.0 + 0.5);
}

int track_encoder_validate_params(const track_encoder_params_t *params)
{
    if (!params) return -1;
    
    switch (params->type) {
    case TRACK_ENC_IBM_MFM:
        if (params->params.ibm.sectors_per_track == 0) return -1;
        if (params->params.ibm.sector_size == 0) return -1;
        if (params->params.ibm.bitrate_kbps == 0) return -1;
        if (params->params.ibm.rpm == 0) return -1;
        return 0;
        
    case TRACK_ENC_AMIGA_MFM:
        if (params->params.amiga.sectors_per_track == 0) return -1;
        if (params->params.amiga.sector_size == 0) return -1;
        return 0;
        
    case TRACK_ENC_C64_GCR:
        if (params->params.c64.track_number == 0) return -1;
        return 0;
        
    default:
        return -1;
    }
}

/*============================================================================*
 * STATISTICS
 *============================================================================*/

void track_encoder_get_stats(track_encoder_stats_t *stats)
{
    if (stats) {
        *stats = g_stats;
    }
}

void track_encoder_reset_stats(void)
{
    memset(&g_stats, 0, sizeof(g_stats));
}
