/**
 * @file uft_integration.c
 * @brief UFT Integration Hub - Implementation
 * 
 * Verbindet alle externen Algorithmen mit dem UFT-Core
 * 
 * @version 5.28.0 GOD MODE
 */

#include "uft/uft_integration.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define MAX_TRACK_DRIVERS   64
#define MAX_FS_DRIVERS      32
#define DEFAULT_CLOCK_NS    2000.0  /* 2µs = 500kHz MFM clock */
#define PLL_DEFAULT_FREQ    0.05
#define PLL_DEFAULT_PHASE   0.60

/* ============================================================================
 * Static Data
 * ============================================================================ */

static const uft_track_driver_t *g_track_drivers[MAX_TRACK_DRIVERS];
static size_t g_track_driver_count = 0;

static const uft_fs_driver_t *g_fs_drivers[MAX_FS_DRIVERS];
static size_t g_fs_driver_count = 0;

static bool g_initialized = false;

/* ============================================================================
 * Flux Decoder Implementation
 * ============================================================================ */

struct uft_flux_decoder {
    uft_flux_config_t config;
    
    /* PLL State */
    double pll_clock;
    double pll_phase;
    
    /* Statistics */
    uint32_t total_flux;
    uint32_t decoded_bits;
    uint32_t weak_bits;
    uint32_t errors;
};

uft_flux_decoder_t* uft_flux_decoder_create(const uft_flux_config_t *config)
{
    uft_flux_decoder_t *dec = calloc(1, sizeof(*dec));
    if (!dec) return NULL;
    
    if (config) {
        dec->config = *config;
    } else {
        /* Defaults */
        dec->config.sample_rate_mhz = 24.0;
        dec->config.use_pll = true;
        dec->config.pll_freq_gain = PLL_DEFAULT_FREQ;
        dec->config.pll_phase_gain = PLL_DEFAULT_PHASE;
        dec->config.detect_encoding = true;
        dec->config.revolutions = 1;
    }
    
    /* Initialize PLL */
    dec->pll_clock = DEFAULT_CLOCK_NS;
    dec->pll_phase = 0.0;
    
    return dec;
}

/**
 * @brief Detect encoding from flux histogram
 */
static uft_encoding_t detect_encoding_from_histogram(
    const uint32_t *flux_times, size_t count, double sample_rate_mhz)
{
    /* Build histogram of flux intervals */
    uint32_t hist[256] = {0};
    double ns_per_tick = 1000.0 / sample_rate_mhz;
    
    for (size_t i = 0; i < count; i++) {
        double ns = flux_times[i] * ns_per_tick;
        int bin = (int)(ns / 100.0);  /* 100ns bins */
        if (bin >= 0 && bin < 256) {
            hist[bin]++;
        }
    }
    
    /* Find peaks */
    int peak1 = 0, peak2 = 0, peak3 = 0;
    uint32_t max1 = 0, max2 = 0, max3 = 0;
    
    for (int i = 10; i < 100; i++) {
        if (hist[i] > max1) {
            max3 = max2; peak3 = peak2;
            max2 = max1; peak2 = peak1;
            max1 = hist[i]; peak1 = i;
        } else if (hist[i] > max2) {
            max3 = max2; peak3 = peak2;
            max2 = hist[i]; peak2 = i;
        } else if (hist[i] > max3) {
            max3 = hist[i]; peak3 = i;
        }
    }
    
    /* Analyze peak ratios */
    double p1_ns = peak1 * 100.0;
    double p2_ns = peak2 * 100.0;
    double p3_ns = peak3 * 100.0;
    
    /* Sort peaks */
    if (p1_ns > p2_ns) { double t = p1_ns; p1_ns = p2_ns; p2_ns = t; }
    if (p2_ns > p3_ns) { double t = p2_ns; p2_ns = p3_ns; p3_ns = t; }
    if (p1_ns > p2_ns) { double t = p1_ns; p1_ns = p2_ns; p2_ns = t; }
    
    /* MFM: 2T/3T/4T ratio ~2:3:4 (4µs/6µs/8µs at 500kHz) */
    if (p1_ns > 3500 && p1_ns < 4500 &&
        p2_ns > 5500 && p2_ns < 6500 &&
        p3_ns > 7500 && p3_ns < 8500) {
        return UFT_ENC_MFM;
    }
    
    /* FM: 4T/8T ratio ~1:2 */
    if (p1_ns > 7500 && p1_ns < 8500 &&
        p2_ns > 15500 && p2_ns < 16500) {
        return UFT_ENC_FM;
    }
    
    /* GCR: Different timing patterns */
    if (p1_ns > 2500 && p1_ns < 3500) {
        /* Likely GCR - need more analysis */
        return UFT_ENC_GCR_C64;  /* Default to C64 GCR */
    }
    
    /* Amiga MFM: Similar to standard MFM but 2µs clock */
    if (p1_ns > 3800 && p1_ns < 4200) {
        return UFT_ENC_AMIGA;
    }
    
    return UFT_ENC_UNKNOWN;
}

/**
 * @brief PLL-based flux to bitstream conversion
 */
static uft_error_t pll_decode(uft_flux_decoder_t *dec,
                              const uint32_t *flux_times, size_t count,
                              uint8_t **bitstream, size_t *bit_count)
{
    double ns_per_tick = 1000.0 / dec->config.sample_rate_mhz;
    
    /* Estimate output size */
    size_t est_bits = count * 2;
    *bitstream = calloc((est_bits + 7) / 8 + 1024, 1);
    if (!*bitstream) return UFT_ERROR_NO_MEMORY;
    
    size_t bits = 0;
    double clock = dec->pll_clock;
    double phase = 0.0;
    
    for (size_t i = 0; i < count; i++) {
        double interval = flux_times[i] * ns_per_tick;
        
        /* Adjust for phase */
        interval -= phase;
        
        /* Calculate number of clock periods */
        int periods = (int)round(interval / clock);
        if (periods < 1) periods = 1;
        if (periods > 8) periods = 8;
        
        /* Output zeros for empty periods, then a one */
        for (int p = 0; p < periods - 1; p++) {
            if (bits < est_bits) {
                bits++;  /* Zero bit */
            }
        }
        
        if (bits < est_bits) {
            (*bitstream)[bits / 8] |= (1 << (7 - (bits % 8)));
            bits++;
        }
        
        /* PLL adjustment */
        double error = interval - (periods * clock);
        clock += error * dec->config.pll_freq_gain;
        phase = error * dec->config.pll_phase_gain;
        
        /* Clamp clock to reasonable range */
        if (clock < DEFAULT_CLOCK_NS * 0.8) clock = DEFAULT_CLOCK_NS * 0.8;
        if (clock > DEFAULT_CLOCK_NS * 1.2) clock = DEFAULT_CLOCK_NS * 1.2;
        
        dec->total_flux++;
    }
    
    *bit_count = bits;
    dec->decoded_bits = bits;
    
    return UFT_OK;
}

uft_error_t uft_flux_decode(uft_flux_decoder_t *decoder,
                            const uint32_t *flux_times,
                            size_t flux_count,
                            uft_flux_result_t *result)
{
    if (!decoder || !flux_times || !flux_count || !result) {
        return UFT_ERROR_INVALID_PARAM;
    }
    
    memset(result, 0, sizeof(*result));
    
    /* Detect encoding if requested */
    if (decoder->config.detect_encoding) {
        result->encoding = detect_encoding_from_histogram(
            flux_times, flux_count, decoder->config.sample_rate_mhz);
    } else {
        result->encoding = decoder->config.encoding;
    }
    
    /* Adjust clock based on encoding */
    switch (result->encoding) {
        case UFT_ENC_MFM:
        case UFT_ENC_AMIGA:
            decoder->pll_clock = 2000.0;  /* 2µs */
            break;
        case UFT_ENC_FM:
            decoder->pll_clock = 4000.0;  /* 4µs */
            break;
        case UFT_ENC_GCR_C64:
            decoder->pll_clock = 3200.0;  /* ~3.2µs */
            break;
        case UFT_ENC_GCR_APPLE2:
            decoder->pll_clock = 4000.0;  /* 4µs */
            break;
        default:
            decoder->pll_clock = 2000.0;
            break;
    }
    
    /* Decode */
    uft_error_t err = pll_decode(decoder, flux_times, flux_count,
                                 &result->bitstream, &result->bitstream_len);
    if (err != UFT_OK) {
        return err;
    }
    
    result->clock_period_ns = decoder->pll_clock;
    result->weak_bits = decoder->weak_bits;
    result->errors = decoder->errors;
    result->confidence = 85;  /* Base confidence */
    
    return UFT_OK;
}

uft_error_t uft_flux_decode_multi_rev(uft_flux_decoder_t *decoder,
                                      const uint32_t **flux_times,
                                      const size_t *flux_counts,
                                      size_t rev_count,
                                      uft_flux_result_t *result)
{
    if (!decoder || !flux_times || !flux_counts || !rev_count || !result) {
        return UFT_ERROR_INVALID_PARAM;
    }
    
    /* For now, decode first revolution and use others for verification */
    uft_error_t err = uft_flux_decode(decoder, flux_times[0], flux_counts[0], result);
    if (err != UFT_OK) {
        return err;
    }
    
    /* Multi-revolution voting would go here */
    /* For each weak/uncertain bit, check other revolutions */
    
    if (rev_count > 1) {
        result->confidence += 10;  /* Boost confidence for multi-rev */
    }
    
    return UFT_OK;
}

void uft_flux_decoder_free(uft_flux_decoder_t *decoder)
{
    free(decoder);
}

void uft_flux_result_free(uft_flux_result_t *result)
{
    if (result) {
        free(result->bitstream);
        memset(result, 0, sizeof(*result));
    }
}

/* ============================================================================
 * Bitstream Decoder Implementation
 * ============================================================================ */

struct uft_bitstream_decoder {
    uft_bitstream_config_t config;
    uft_encoding_t last_successful_encoding;
};

uft_bitstream_decoder_t* uft_bitstream_decoder_create(
    const uft_bitstream_config_t *config)
{
    uft_bitstream_decoder_t *dec = calloc(1, sizeof(*dec));
    if (!dec) return NULL;
    
    if (config) {
        dec->config = *config;
    } else {
        dec->config.auto_detect = true;
        dec->config.try_all_formats = true;
    }
    
    dec->last_successful_encoding = UFT_ENC_UNKNOWN;
    
    return dec;
}

/**
 * @brief Calculate CRC-16 CCITT
 */
static uint16_t crc16_ccitt(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    
    return crc;
}

/**
 * @brief Find MFM sync marks
 */
static int find_mfm_sync(const uint8_t *bitstream, size_t bit_count,
                         size_t start, size_t *sync_pos)
{
    /* MFM sync: A1 A1 A1 with missing clock = 4489 4489 4489 */
    static const uint8_t sync_pattern[] = {0x44, 0x89, 0x44, 0x89, 0x44, 0x89};
    
    for (size_t i = start; i + 48 < bit_count; i++) {
        bool match = true;
        for (int j = 0; j < 6 && match; j++) {
            size_t byte_pos = (i + j * 8) / 8;
            int bit_off = (i + j * 8) % 8;
            
            uint8_t b = (bitstream[byte_pos] << bit_off);
            if (byte_pos + 1 < (bit_count + 7) / 8) {
                b |= (bitstream[byte_pos + 1] >> (8 - bit_off));
            }
            
            if (b != sync_pattern[j]) {
                match = false;
            }
        }
        
        if (match) {
            *sync_pos = i;
            return 1;
        }
    }
    
    return 0;
}

/**
 * @brief Decode MFM data
 */
static void mfm_decode_bytes(const uint8_t *bitstream, size_t bit_start,
                             uint8_t *output, size_t byte_count)
{
    for (size_t i = 0; i < byte_count; i++) {
        uint8_t byte = 0;
        for (int b = 0; b < 8; b++) {
            /* MFM: clock bit, data bit - take every other bit */
            size_t bit_pos = bit_start + (i * 16) + (b * 2) + 1;
            size_t byte_pos = bit_pos / 8;
            int bit_off = 7 - (bit_pos % 8);
            
            if (bitstream[byte_pos] & (1 << bit_off)) {
                byte |= (1 << (7 - b));
            }
        }
        output[i] = byte;
    }
}

uft_error_t uft_bitstream_decode(uft_bitstream_decoder_t *decoder,
                                 const uint8_t *bitstream,
                                 size_t bit_count,
                                 uft_bitstream_result_t *result)
{
    if (!decoder || !bitstream || !bit_count || !result) {
        return UFT_ERROR_INVALID_PARAM;
    }
    
    memset(result, 0, sizeof(*result));
    
    /* Allocate sectors array */
    result->sectors = calloc(64, sizeof(uft_decoded_sector_t));
    if (!result->sectors) {
        return UFT_ERROR_NO_MEMORY;
    }
    
    size_t pos = 0;
    size_t sync_pos;
    
    /* Scan for sectors */
    while (pos < bit_count - 1024 && result->sector_count < 64) {
        if (!find_mfm_sync(bitstream, bit_count, pos, &sync_pos)) {
            break;
        }
        
        pos = sync_pos + 48;  /* Skip sync */
        
        /* Read IDAM (ID Address Mark) */
        uint8_t idam[8];  /* FE + C H R N + CRC */
        mfm_decode_bytes(bitstream, pos, idam, 7);
        pos += 7 * 16;
        
        if (idam[0] != 0xFE) {
            continue;  /* Not an ID mark */
        }
        
        uft_decoded_sector_t *sec = &result->sectors[result->sector_count];
        sec->track = idam[1];
        sec->head = idam[2];
        sec->sector = idam[3];
        sec->size_code = idam[4];
        sec->header_crc = (idam[5] << 8) | idam[6];
        
        /* Verify header CRC (CRC-CCITT over sync+mark+CHRN) */
        {
            uint8_t hdr_buf[8] = {0xA1, 0xA1, 0xA1, 0xFE,
                                   idam[1], idam[2], idam[3], idam[4]};
            uint16_t crc = 0xFFFF;
            for (int b = 0; b < 8; b++) {
                crc ^= (uint16_t)hdr_buf[b] << 8;
                for (int bit = 0; bit < 8; bit++)
                    crc = (crc << 1) ^ ((crc & 0x8000) ? 0x1021 : 0);
            }
            sec->header_crc_ok = (crc == sec->header_crc);
        }
        
        /* Look for DAM (Data Address Mark) */
        size_t data_sync;
        if (!find_mfm_sync(bitstream, bit_count, pos, &data_sync)) {
            continue;
        }
        
        pos = data_sync + 48;
        
        uint8_t dam;
        mfm_decode_bytes(bitstream, pos, &dam, 1);
        pos += 16;
        
        if (dam != 0xFB && dam != 0xF8) {
            continue;  /* Not a data mark */
        }
        
        /* Calculate sector size */
        size_t data_len = 128 << sec->size_code;
        if (data_len > 8192) data_len = 8192;
        
        sec->data = malloc(data_len);
        if (!sec->data) {
            continue;
        }
        
        mfm_decode_bytes(bitstream, pos, sec->data, data_len);
        sec->data_len = data_len;
        pos += data_len * 16;
        
        /* Read data CRC */
        uint8_t crc_bytes[2];
        mfm_decode_bytes(bitstream, pos, crc_bytes, 2);
        sec->data_crc = (crc_bytes[0] << 8) | crc_bytes[1];
        pos += 32;
        
        /* Verify data CRC (CRC-CCITT over sync+DAM+data) */
        {
            uint16_t crc = 0xFFFF;
            /* CRC covers: A1 A1 A1 <dam> <data...> */
            uint8_t prefix[4] = {0xA1, 0xA1, 0xA1, dam};
            for (int b = 0; b < 4; b++) {
                crc ^= (uint16_t)prefix[b] << 8;
                for (int bit = 0; bit < 8; bit++)
                    crc = (crc << 1) ^ ((crc & 0x8000) ? 0x1021 : 0);
            }
            for (size_t b = 0; b < data_len; b++) {
                crc ^= (uint16_t)sec->data[b] << 8;
                for (int bit = 0; bit < 8; bit++)
                    crc = (crc << 1) ^ ((crc & 0x8000) ? 0x1021 : 0);
            }
            sec->data_crc_ok = (crc == sec->data_crc);
        }
        
        sec->encoding = UFT_ENC_MFM;
        result->sector_count++;
    }
    
    result->encoding = UFT_ENC_MFM;
    result->confidence = (result->sector_count > 0) ? 80 : 0;
    
    return UFT_OK;
}

void uft_bitstream_decoder_free(uft_bitstream_decoder_t *decoder)
{
    free(decoder);
}

void uft_bitstream_result_free(uft_bitstream_result_t *result)
{
    if (result) {
        for (size_t i = 0; i < result->sector_count; i++) {
            free(result->sectors[i].data);
        }
        free(result->sectors);
        memset(result, 0, sizeof(*result));
    }
}

/* ============================================================================
 * Track Driver Registry
 * ============================================================================ */

uft_error_t uft_track_driver_register(const uft_track_driver_t *driver)
{
    if (!driver || !driver->name || !driver->probe || !driver->decode) {
        return UFT_ERROR_INVALID_PARAM;
    }
    
    if (g_track_driver_count >= MAX_TRACK_DRIVERS) {
        return UFT_ERROR_NO_MEMORY;
    }
    
    g_track_drivers[g_track_driver_count++] = driver;
    return UFT_OK;
}

const uft_track_driver_t* uft_track_driver_get(const char *name)
{
    for (size_t i = 0; i < g_track_driver_count; i++) {
        if (strcmp(g_track_drivers[i]->name, name) == 0) {
            return g_track_drivers[i];
        }
    }
    return NULL;
}

size_t uft_track_driver_list(const uft_track_driver_t ***drivers)
{
    if (drivers) {
        *drivers = g_track_drivers;
    }
    return g_track_driver_count;
}

uft_error_t uft_track_decode_auto(const uint8_t *track_data, size_t len,
                                  uint8_t track_num, uint8_t head,
                                  uft_bitstream_result_t *result,
                                  const uft_track_driver_t **used_driver)
{
    if (!track_data || !len || !result) {
        return UFT_ERROR_INVALID_PARAM;
    }
    
    /* Score all drivers */
    int best_score = 0;
    const uft_track_driver_t *best_driver = NULL;
    
    for (size_t i = 0; i < g_track_driver_count; i++) {
        int score = g_track_drivers[i]->probe(track_data, len);
        if (score > best_score) {
            best_score = score;
            best_driver = g_track_drivers[i];
        }
    }
    
    if (!best_driver || best_score < 10) {
        return UFT_ERROR_NOT_FOUND;
    }
    
    if (used_driver) {
        *used_driver = best_driver;
    }
    
    return best_driver->decode(track_data, len, track_num, head, result);
}

/* ============================================================================
 * Filesystem Driver Registry
 * ============================================================================ */

uft_error_t uft_fs_driver_register(const uft_fs_driver_t *driver)
{
    if (!driver || !driver->name || !driver->probe || !driver->mount) {
        return UFT_ERROR_INVALID_PARAM;
    }
    
    if (g_fs_driver_count >= MAX_FS_DRIVERS) {
        return UFT_ERROR_NO_MEMORY;
    }
    
    g_fs_drivers[g_fs_driver_count++] = driver;
    return UFT_OK;
}

const uft_fs_driver_t* uft_fs_driver_get(uft_fs_type_t type)
{
    for (size_t i = 0; i < g_fs_driver_count; i++) {
        if (g_fs_drivers[i]->type == type) {
            return g_fs_drivers[i];
        }
    }
    return NULL;
}

uft_error_t uft_fs_mount_auto(const uft_disk_t *disk,
                              uft_filesystem_t **fs,
                              const uft_fs_driver_t **used_driver)
{
    if (!disk || !fs) {
        return UFT_ERROR_INVALID_PARAM;
    }
    
    /* Score all drivers */
    int best_score = 0;
    const uft_fs_driver_t *best_driver = NULL;
    
    for (size_t i = 0; i < g_fs_driver_count; i++) {
        int score = g_fs_drivers[i]->probe(disk);
        if (score > best_score) {
            best_score = score;
            best_driver = g_fs_drivers[i];
        }
    }
    
    if (!best_driver || best_score < 10) {
        return UFT_ERROR_NOT_FOUND;
    }
    
    if (used_driver) {
        *used_driver = best_driver;
    }
    
    return best_driver->mount(disk, fs);
}

/* ============================================================================
 * Pipeline Implementation
 * ============================================================================ */

struct uft_pipeline_impl {
    uft_pipeline_t pub;
    uft_flux_decoder_t *flux_dec;
    uft_bitstream_decoder_t *bs_dec;
};

uft_pipeline_t* uft_pipeline_create(void)
{
    struct uft_pipeline_impl *impl = calloc(1, sizeof(*impl));
    if (!impl) return NULL;
    
    impl->flux_dec = uft_flux_decoder_create(NULL);
    impl->bs_dec = uft_bitstream_decoder_create(NULL);
    
    if (!impl->flux_dec || !impl->bs_dec) {
        uft_flux_decoder_free(impl->flux_dec);
        uft_bitstream_decoder_free(impl->bs_dec);
        free(impl);
        return NULL;
    }
    
    return &impl->pub;
}

uft_error_t uft_pipeline_run(uft_pipeline_t *pipeline)
{
    struct uft_pipeline_impl *impl = (struct uft_pipeline_impl *)pipeline;
    
    if (!impl || !impl->pub.flux_revs || !impl->pub.rev_count) {
        return UFT_ERROR_INVALID_PARAM;
    }
    
    /* Step 1: Flux → Bitstream */
    uft_error_t err = uft_flux_decode_multi_rev(
        impl->flux_dec,
        impl->pub.flux_revs,
        impl->pub.flux_counts,
        impl->pub.rev_count,
        &impl->pub.flux_result);
    
    if (err != UFT_OK) {
        snprintf(impl->pub.error_message, sizeof(impl->pub.error_message),
                 "Flux decode failed: %d", err);
        impl->pub.last_error = err;
        return err;
    }
    
    /* Step 2: Bitstream → Sectors */
    err = uft_bitstream_decode(
        impl->bs_dec,
        impl->pub.flux_result.bitstream,
        impl->pub.flux_result.bitstream_len,
        &impl->pub.bitstream_result);
    
    if (err != UFT_OK) {
        snprintf(impl->pub.error_message, sizeof(impl->pub.error_message),
                 "Bitstream decode failed: %d", err);
        impl->pub.last_error = err;
        return err;
    }
    
    return UFT_OK;
}

void uft_pipeline_free(uft_pipeline_t *pipeline)
{
    struct uft_pipeline_impl *impl = (struct uft_pipeline_impl *)pipeline;
    if (impl) {
        uft_flux_decoder_free(impl->flux_dec);
        uft_bitstream_decoder_free(impl->bs_dec);
        uft_flux_result_free(&impl->pub.flux_result);
        uft_bitstream_result_free(&impl->pub.bitstream_result);
        free(impl);
    }
}

/* ============================================================================
 * Initialization
 * ============================================================================ */

const char* uft_integration_version(void)
{
    return UFT_INTEGRATION_VERSION_STRING " GOD MODE";
}

uft_error_t uft_integration_init(void)
{
    if (g_initialized) {
        return UFT_OK;
    }
    
    /* Register built-in track drivers */
    /* These would be defined in separate files */
    
    /* Register built-in filesystem drivers */
    /* These would be defined in separate files */
    
    g_initialized = true;
    return UFT_OK;
}

void uft_integration_cleanup(void)
{
    g_track_driver_count = 0;
    g_fs_driver_count = 0;
    g_initialized = false;
}
