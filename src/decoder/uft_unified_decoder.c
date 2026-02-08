/**
 * @file uft_unified_decoder.c
 * @brief P0-DC-001: Unified Decoder Interface Implementation
 * 
 * @version 1.0.0
 * @date 2025-01-05
 */

#include "uft/decoder/uft_unified_decoder.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*===========================================================================
 * Constants
 *===========================================================================*/

#define MAX_DECODERS        32
#define DEFAULT_SECTORS     32
#define DEFAULT_BITSTREAM   100000

/*===========================================================================
 * Global Decoder Registry
 *===========================================================================*/

static struct {
    const uft_decoder_interface_t *decoders[MAX_DECODERS];
    size_t count;
    bool initialized;
} g_registry = {0};

/*===========================================================================
 * String Tables
 *===========================================================================*/

static const char *encoding_names[] = {
    [UFT_ENC_FM]            = "FM",
    [UFT_ENC_MFM]           = "MFM",
    [UFT_ENC_MFM_HD]        = "MFM HD",
    [UFT_ENC_MFM_AMIGA]     = "Amiga MFM",
    [UFT_ENC_GCR_APPLE_525] = "Apple II GCR",
    [UFT_ENC_GCR_APPLE_35]  = "Apple 3.5 GCR",
    [UFT_ENC_GCR_C64]       = "C64 GCR",
    [UFT_ENC_GCR_C128]      = "C128 GCR",
    [UFT_ENC_GCR_VICTOR9K]  = "Victor 9000 GCR",
    [UFT_ENC_GCR_MAC]       = "Mac GCR",
    [UFT_ENC_RAW]           = "Raw",
    [UFT_ENC_CUSTOM]        = "Custom",
    [UFT_ENC_PROTECTED]     = "Protected",
};

static const char *error_strings[] = {
    [UFT_DEC_OK]                = "Success",
    [UFT_DEC_ERR_INVALID_ARG]   = "Invalid argument",
    [UFT_DEC_ERR_NO_MEMORY]     = "Out of memory",
    [UFT_DEC_ERR_NOT_DETECTED]  = "Encoding not detected",
    [UFT_DEC_ERR_DECODE_FAILED] = "Decode failed",
    [UFT_DEC_ERR_ENCODE_FAILED] = "Encode failed",
    [UFT_DEC_ERR_CRC_ERROR]     = "CRC error",
    [UFT_DEC_ERR_NO_SYNC]       = "Sync not found",
    [UFT_DEC_ERR_TRUNCATED]     = "Data truncated",
    [UFT_DEC_ERR_NOT_REGISTERED]= "Decoder not registered",
    [UFT_DEC_ERR_UNSUPPORTED]   = "Operation not supported",
};

/*===========================================================================
 * Registry Functions
 *===========================================================================*/

uft_dec_error_t uft_dec_register(const uft_decoder_interface_t *decoder)
{
    if (!decoder || !decoder->name) {
        return UFT_DEC_ERR_INVALID_ARG;
    }
    
    if (g_registry.count >= MAX_DECODERS) {
        return UFT_DEC_ERR_NO_MEMORY;
    }
    
    /* Check for duplicate */
    for (size_t i = 0; i < g_registry.count; i++) {
        if (g_registry.decoders[i]->encoding == decoder->encoding) {
            /* Replace existing */
            g_registry.decoders[i] = decoder;
            return UFT_DEC_OK;
        }
    }
    
    g_registry.decoders[g_registry.count++] = decoder;
    g_registry.initialized = true;
    
    return UFT_DEC_OK;
}

uft_dec_error_t uft_dec_unregister(uft_encoding_t encoding)
{
    for (size_t i = 0; i < g_registry.count; i++) {
        if (g_registry.decoders[i]->encoding == encoding) {
            /* Shift remaining decoders */
            for (size_t j = i; j + 1 < g_registry.count; j++) {
                g_registry.decoders[j] = g_registry.decoders[j + 1];
            }
            g_registry.count--;
            return UFT_DEC_OK;
        }
    }
    
    return UFT_DEC_ERR_NOT_REGISTERED;
}

const uft_decoder_interface_t *uft_dec_get(uft_encoding_t encoding)
{
    for (size_t i = 0; i < g_registry.count; i++) {
        if (g_registry.decoders[i]->encoding == encoding) {
            return g_registry.decoders[i];
        }
    }
    return NULL;
}

const uft_decoder_interface_t *uft_dec_get_by_name(const char *name)
{
    if (!name) return NULL;
    
    for (size_t i = 0; i < g_registry.count; i++) {
        if (strcmp(g_registry.decoders[i]->name, name) == 0) {
            return g_registry.decoders[i];
        }
    }
    return NULL;
}

size_t uft_dec_list(uft_encoding_t *encodings, size_t max_count)
{
    size_t count = 0;
    for (size_t i = 0; i < g_registry.count && count < max_count; i++) {
        if (encodings) {
            encodings[count] = g_registry.decoders[i]->encoding;
        }
        count++;
    }
    return count;
}

/*===========================================================================
 * Auto-Detection
 *===========================================================================*/

/**
 * @brief Compare function for sorting detection results
 */
static int compare_detection(const void *a, const void *b)
{
    const uft_detection_result_t *ra = a;
    const uft_detection_result_t *rb = b;
    
    /* Sort by confidence descending */
    if (ra->confidence > rb->confidence) return -1;
    if (ra->confidence < rb->confidence) return 1;
    return 0;
}

size_t uft_dec_auto_detect(const uft_bitstream_t *bs,
                           uft_detection_result_t *results,
                           size_t max_results)
{
    if (!bs || !results || max_results == 0) {
        return 0;
    }
    
    size_t count = 0;
    
    /* Try each registered decoder */
    for (size_t i = 0; i < g_registry.count && count < max_results; i++) {
        const uft_decoder_interface_t *dec = g_registry.decoders[i];
        
        if (dec->detect) {
            float confidence = 0.0f;
            uft_dec_error_t err = dec->detect(bs, &confidence);
            
            if (err == UFT_DEC_OK && confidence > 0.1f) {
                results[count].encoding = dec->encoding;
                results[count].confidence = confidence;
                results[count].decoder = dec;
                count++;
            }
        }
    }
    
    /* Sort by confidence */
    if (count > 1) {
        qsort(results, count, sizeof(uft_detection_result_t), compare_detection);
    }
    
    return count;
}

uft_encoding_t uft_dec_detect_best(const uft_bitstream_t *bs, float *confidence)
{
    uft_detection_result_t results[MAX_DECODERS];
    size_t count = uft_dec_auto_detect(bs, results, MAX_DECODERS);
    
    if (count > 0) {
        if (confidence) *confidence = results[0].confidence;
        return results[0].encoding;
    }
    
    if (confidence) *confidence = 0.0f;
    return UFT_ENC_RAW;
}

/*===========================================================================
 * High-Level Decode Functions
 *===========================================================================*/

uft_dec_error_t uft_dec_decode_track(const uft_bitstream_t *bs,
                                      uft_track_t *track)
{
    if (!bs || !track) {
        return UFT_DEC_ERR_INVALID_ARG;
    }
    
    /* Auto-detect encoding */
    float confidence;
    uft_encoding_t encoding = uft_dec_detect_best(bs, &confidence);
    
    track->encoding = encoding;
    track->detection_confidence = confidence;
    
    return uft_dec_decode_track_as(bs, encoding, track);
}

uft_dec_error_t uft_dec_decode_track_as(const uft_bitstream_t *bs,
                                         uft_encoding_t encoding,
                                         uft_track_t *track)
{
    if (!bs || !track) {
        return UFT_DEC_ERR_INVALID_ARG;
    }
    
    const uft_decoder_interface_t *dec = uft_dec_get(encoding);
    if (!dec) {
        return UFT_DEC_ERR_NOT_REGISTERED;
    }
    
    if (!dec->decode) {
        return UFT_DEC_ERR_UNSUPPORTED;
    }
    
    /* Ensure track has sector capacity */
    if (track->sector_capacity == 0) {
        track->sectors = calloc(DEFAULT_SECTORS, sizeof(uft_sector_t));
        if (!track->sectors) {
            return UFT_DEC_ERR_NO_MEMORY;
        }
        track->sector_capacity = DEFAULT_SECTORS;
    }
    
    /* Set track info */
    track->track_num = bs->track;
    track->side = bs->side;
    track->encoding = encoding;
    
    /* Decode */
    size_t count = track->sector_capacity;
    uft_dec_error_t err = dec->decode(bs, track->sectors, &count);
    
    track->sector_count = count;
    
    /* Calculate statistics */
    track->good_sectors = 0;
    track->bad_sectors = 0;
    
    for (size_t i = 0; i < count; i++) {
        if (track->sectors[i].crc_valid) {
            track->good_sectors++;
        } else {
            track->bad_sectors++;
        }
    }
    
    return err;
}

uft_dec_error_t uft_dec_encode_track(const uft_track_t *track,
                                      uft_encoding_t encoding,
                                      uft_bitstream_t *bs)
{
    if (!track || !bs) {
        return UFT_DEC_ERR_INVALID_ARG;
    }
    
    const uft_decoder_interface_t *dec = uft_dec_get(encoding);
    if (!dec) {
        return UFT_DEC_ERR_NOT_REGISTERED;
    }
    
    if (!dec->encode) {
        return UFT_DEC_ERR_UNSUPPORTED;
    }
    
    return dec->encode(track->sectors, track->sector_count, bs);
}

/*===========================================================================
 * Memory Management
 *===========================================================================*/

uft_bitstream_t *uft_bitstream_alloc(size_t bit_capacity, bool with_timing)
{
    uft_bitstream_t *bs = calloc(1, sizeof(uft_bitstream_t));
    if (!bs) return NULL;
    
    size_t byte_capacity = (bit_capacity + 7) / 8;
    bs->bits = calloc(byte_capacity, 1);
    if (!bs->bits) {
        free(bs);
        return NULL;
    }
    
    bs->capacity = bit_capacity;
    
    if (with_timing) {
        bs->timing = calloc(bit_capacity, sizeof(uint32_t));
        bs->has_timing = (bs->timing != NULL);
    }
    
    return bs;
}

void uft_bitstream_free(uft_bitstream_t *bs)
{
    if (!bs) return;
    free(bs->bits);
    free(bs->timing);
    free(bs);
}

uft_sector_t *uft_sector_alloc(size_t data_size)
{
    uft_sector_t *sector = calloc(1, sizeof(uft_sector_t));
    if (!sector) return NULL;
    
    if (data_size > 0) {
        sector->data = calloc(data_size, 1);
        if (!sector->data) {
            free(sector);
            return NULL;
        }
        sector->data_size = data_size;
    }
    
    return sector;
}

void uft_sector_free(uft_sector_t *sector)
{
    if (!sector) return;
    free(sector->data);
    free(sector);
}

uft_track_t *uft_track_alloc(size_t sector_capacity)
{
    uft_track_t *track = calloc(1, sizeof(uft_track_t));
    if (!track) return NULL;
    
    if (sector_capacity > 0) {
        track->sectors = calloc(sector_capacity, sizeof(uft_sector_t));
        if (!track->sectors) {
            free(track);
            return NULL;
        }
        track->sector_capacity = sector_capacity;
    }
    
    return track;
}

void uft_track_free(uft_track_t *track)
{
    if (!track) return;
    
    /* Free sector data */
    for (size_t i = 0; i < track->sector_count; i++) {
        free(track->sectors[i].data);
    }
    
    free(track->sectors);
    
    /* Free preserved bitstream */
    if (track->bitstream) {
        uft_bitstream_free(track->bitstream);
    }
    
    free(track);
}

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

static const char *uft_decoder_encoding_name(uft_encoding_t encoding)
{
    if (encoding >= UFT_ENC_COUNT) {
        return "Unknown";
    }
    return encoding_names[encoding] ? encoding_names[encoding] : "Unknown";
}

const char *uft_dec_error_str(uft_dec_error_t error)
{
    if (error < 0 || error > UFT_DEC_ERR_UNSUPPORTED) {
        return "Unknown error";
    }
    return error_strings[error];
}

/*===========================================================================
 * Built-in MFM Decoder
 *===========================================================================*/

/* MFM sync patterns */
#define MFM_SYNC_WORD       0x4489      /* A1 with clock = 4489 */
#define MFM_SYNC_COUNT      3           /* Need 3 sync words */
#define MFM_IDAM            0xFE        /* ID Address Mark */
#define MFM_DAM             0xFB        /* Data Address Mark */
#define MFM_DDAM            0xF8        /* Deleted Data Address Mark */

/**
 * @brief Decode MFM byte from bitstream
 */
static uint8_t mfm_decode_byte(const uint8_t *bits, size_t bit_offset)
{
    uint8_t byte = 0;
    for (int i = 0; i < 8; i++) {
        size_t pos = bit_offset + i * 2 + 1;  /* Skip clock bits */
        size_t byte_pos = pos / 8;
        size_t bit_pos = 7 - (pos % 8);
        if (bits[byte_pos] & (1 << bit_pos)) {
            byte |= (0x80 >> i);
        }
    }
    return byte;
}

/**
 * @brief Find MFM sync pattern
 */
static int64_t mfm_find_sync(const uft_bitstream_t *bs, size_t start)
{
    if (!bs || !bs->bits) return -1;
    
    /* Look for 3x 4489 (A1 with missing clock) */
    uint16_t pattern = 0;
    int sync_count = 0;
    
    for (size_t i = start; i < bs->bit_count; i++) {
        size_t byte_pos = i / 8;
        size_t bit_pos = 7 - (i % 8);
        int bit = (bs->bits[byte_pos] >> bit_pos) & 1;
        
        pattern = (pattern << 1) | bit;
        
        if ((pattern & 0xFFFF) == MFM_SYNC_WORD) {
            sync_count++;
            if (sync_count >= MFM_SYNC_COUNT) {
                return (int64_t)(i + 1);  /* Position after sync */
            }
        } else if (sync_count > 0 && (pattern & 0xFFFF) != MFM_SYNC_WORD) {
            /* Reset if pattern breaks */
            sync_count = 0;
        }
    }
    
    return -1;
}

/**
 * @brief MFM CRC-CCITT calculation
 */
static uint16_t mfm_crc(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    
    for (size_t i = 0; i < len; i++) {
        crc ^= ((uint16_t)data[i] << 8);
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

static uft_dec_error_t mfm_detect(const uft_bitstream_t *bs, float *confidence)
{
    if (!bs || !confidence) return UFT_DEC_ERR_INVALID_ARG;
    
    *confidence = 0.0f;
    
    /* Look for MFM sync patterns */
    int syncs_found = 0;
    size_t pos = 0;
    
    for (size_t i = 0; i < 20 && pos < bs->bit_count; i++) {
        int64_t sync_pos = mfm_find_sync(bs, pos);
        if (sync_pos >= 0) {
            syncs_found++;
            pos = (size_t)sync_pos + 100;  /* Skip ahead */
        } else {
            break;
        }
    }
    
    if (syncs_found >= 5) {
        *confidence = 0.9f;
    } else if (syncs_found >= 3) {
        *confidence = 0.7f;
    } else if (syncs_found >= 1) {
        *confidence = 0.4f;
    }
    
    return (*confidence > 0.1f) ? UFT_DEC_OK : UFT_DEC_ERR_NOT_DETECTED;
}

static uft_dec_error_t mfm_decode(const uft_bitstream_t *bs,
                                   uft_sector_t *sectors, size_t *count)
{
    if (!bs || !sectors || !count || *count == 0) {
        return UFT_DEC_ERR_INVALID_ARG;
    }
    
    size_t max_sectors = *count;
    size_t found = 0;
    size_t pos = 0;
    
    while (found < max_sectors && pos < bs->bit_count) {
        /* Find sync */
        int64_t sync_pos = mfm_find_sync(bs, pos);
        if (sync_pos < 0) break;
        
        pos = (size_t)sync_pos;
        
        /* Read address mark */
        if (pos + 16 > bs->bit_count) break;
        
        uint8_t mark = mfm_decode_byte(bs->bits, pos);
        pos += 16;  /* 8 data bits * 2 (MFM encoded) */
        
        if (mark != MFM_IDAM) {
            /* Not an ID field, might be data mark */
            continue;
        }
        
        /* Read IDAM: C H S N + CRC */
        if (pos + 6 * 16 > bs->bit_count) break;
        
        uint8_t header[6];
        for (int i = 0; i < 6; i++) {
            header[i] = mfm_decode_byte(bs->bits, pos);
            pos += 16;
        }
        
        /* Calculate header CRC */
        uint8_t crc_data[5] = {MFM_IDAM, header[0], header[1], header[2], header[3]};
        uint16_t calc_crc = mfm_crc(crc_data, 5);
        uint16_t stored_crc = ((uint16_t)header[4] << 8) | header[5];
        
        /* Fill sector header */
        uft_sector_t *sec = &sectors[found];
        memset(sec, 0, sizeof(*sec));
        
        sec->track = header[0];
        sec->side = header[1];
        sec->sector = header[2];
        sec->size_code = header[3];
        sec->calculated_crc = calc_crc;
        sec->stored_crc = stored_crc;
        sec->crc_valid = (calc_crc == stored_crc);
        sec->start_bit = (uint32_t)(sync_pos - MFM_SYNC_COUNT * 16);
        
        /* Find data field (another sync + DAM/DDAM) */
        int64_t data_sync = mfm_find_sync(bs, pos);
        if (data_sync < 0 || (size_t)data_sync > pos + 1000) {
            /* Data field not found or too far */
            found++;
            continue;
        }
        
        pos = (size_t)data_sync;
        
        if (pos + 16 > bs->bit_count) break;
        
        uint8_t data_mark = mfm_decode_byte(bs->bits, pos);
        pos += 16;
        
        if (data_mark == MFM_DDAM) {
            sec->deleted = true;
        } else if (data_mark != MFM_DAM) {
            /* Not a valid data mark */
            found++;
            continue;
        }
        
        /* Calculate data size */
        uint16_t data_size = uft_sector_size_from_code(sec->size_code);
        
        /* Allocate and read data */
        sec->data = malloc(data_size);
        if (!sec->data) {
            *count = found;
            return UFT_DEC_ERR_NO_MEMORY;
        }
        sec->data_size = data_size;
        
        /* Check if enough bits remain */
        if (pos + (data_size + 2) * 16 > bs->bit_count) {
            sec->data_size = 0;  /* Mark as incomplete */
            found++;
            break;
        }
        
        /* Read data + CRC */
        uint8_t *crc_buf = malloc(data_size + 1);  /* +1 for data mark */
        if (!crc_buf) {
            *count = found;
            return UFT_DEC_ERR_NO_MEMORY;
        }
        
        crc_buf[0] = data_mark;
        for (size_t i = 0; i < data_size; i++) {
            sec->data[i] = mfm_decode_byte(bs->bits, pos);
            crc_buf[i + 1] = sec->data[i];
            pos += 16;
        }
        
        /* Read data CRC */
        uint8_t crc_bytes[2];
        crc_bytes[0] = mfm_decode_byte(bs->bits, pos); pos += 16;
        crc_bytes[1] = mfm_decode_byte(bs->bits, pos); pos += 16;
        
        calc_crc = mfm_crc(crc_buf, data_size + 1);
        stored_crc = ((uint16_t)crc_bytes[0] << 8) | crc_bytes[1];
        
        free(crc_buf);
        
        /* Update CRC status - header was checked earlier, now check data */
        sec->crc_valid = sec->crc_valid && (calc_crc == stored_crc);
        sec->end_bit = (uint32_t)pos;
        sec->confidence = sec->crc_valid ? 1.0f : 0.5f;
        
        found++;
    }
    
    *count = found;
    return (found > 0) ? UFT_DEC_OK : UFT_DEC_ERR_DECODE_FAILED;
}

static uft_dec_error_t mfm_validate(const uft_sector_t *sector)
{
    if (!sector || !sector->data) {
        return UFT_DEC_ERR_INVALID_ARG;
    }
    
    return sector->crc_valid ? UFT_DEC_OK : UFT_DEC_ERR_CRC_ERROR;
}

static uint8_t mfm_expected_sectors(uint16_t track, uint8_t side)
{
    (void)track;
    (void)side;
    return 9;  /* Standard DD */
}

static uint16_t mfm_sector_size(void)
{
    return 512;
}

static uint32_t mfm_capabilities(void)
{
    return UFT_DEC_CAP_ENCODE | UFT_DEC_CAP_TIMING | UFT_DEC_CAP_WEAK_BITS;
}

/* MFM Decoder Interface */
static const uft_decoder_interface_t mfm_decoder = {
    .name = "IBM MFM",
    .description = "IBM PC compatible MFM encoding",
    .encoding = UFT_ENC_MFM,
    .version = 0x0100,
    .detect = mfm_detect,
    .decode = mfm_decode,
    .encode = NULL,  /* MFM encoding via uft_mfm_encoder module */
    .validate = mfm_validate,
    .expected_sectors = mfm_expected_sectors,
    .sector_size = mfm_sector_size,
    .decode_multi = NULL,
    .capabilities = mfm_capabilities,
};

/*===========================================================================
 * Built-in Decoder Registration
 *===========================================================================*/

int uft_dec_init_builtins(void)
{
    int count = 0;
    
    /* Clear registry */
    memset(&g_registry, 0, sizeof(g_registry));
    
    /* Register built-in decoders */
    if (uft_dec_register(&mfm_decoder) == UFT_DEC_OK) {
        count++;
    }
    
    /* Register additional decoder modules.
     * These decoders are implemented in their respective source files:
     * - FM: uft_fm_decoder (single-density)
     * - Amiga MFM: uft_amiga_mfm_decoder (odd/even encoding)
     * - C64 GCR: uft_gcr_decoder (4-bit GCR groups)
     * - Apple GCR: uft_gcr (6-and-2, 5-and-3)
     * Registration happens via uft_dec_register() from each module's init. */
    
    g_registry.initialized = true;
    
    return count;
}

/*===========================================================================
 * Self-Test
 *===========================================================================*/

#ifdef UFT_DECODER_TEST

#include <stdio.h>

int main(void)
{
    printf("UFT Unified Decoder Self-Test\n");
    printf("=============================\n\n");
    
    /* Initialize registry */
    int count = uft_dec_init_builtins();
    printf("Registered %d built-in decoders\n", count);
    
    /* List decoders */
    printf("\nRegistered encodings:\n");
    uft_encoding_t encs[MAX_DECODERS];
    size_t num = uft_dec_list(encs, MAX_DECODERS);
    for (size_t i = 0; i < num; i++) {
        const uft_decoder_interface_t *dec = uft_dec_get(encs[i]);
        printf("  [%d] %s - %s\n", encs[i], 
               dec ? dec->name : "?",
               dec ? dec->description : "?");
    }
    
    /* Test memory allocation */
    printf("\nMemory allocation tests:\n");
    
    uft_bitstream_t *bs = uft_bitstream_alloc(100000, true);
    printf("  Bitstream: %s\n", bs ? "PASS" : "FAIL");
    
    uft_sector_t *sec = uft_sector_alloc(512);
    printf("  Sector: %s\n", sec ? "PASS" : "FAIL");
    
    uft_track_t *track = uft_track_alloc(18);
    printf("  Track: %s\n", track ? "PASS" : "FAIL");
    
    /* Cleanup */
    uft_bitstream_free(bs);
    uft_sector_free(sec);
    uft_track_free(track);
    
    /* Test utility functions */
    printf("\nUtility tests:\n");
    printf("  MFM name: %s - %s\n", 
           uft_encoding_name(UFT_ENC_MFM),
           strcmp(uft_encoding_name(UFT_ENC_MFM), "MFM") == 0 ? "PASS" : "FAIL");
    printf("  Error string: %s - %s\n",
           uft_dec_error_str(UFT_DEC_ERR_CRC_ERROR),
           strcmp(uft_dec_error_str(UFT_DEC_ERR_CRC_ERROR), "CRC error") == 0 ? "PASS" : "FAIL");
    printf("  Sector size(2): %d - %s\n",
           uft_sector_size_from_code(2),
           uft_sector_size_from_code(2) == 512 ? "PASS" : "FAIL");
    
    printf("\nSelf-test complete.\n");
    return 0;
}

#endif /* UFT_DECODER_TEST */
