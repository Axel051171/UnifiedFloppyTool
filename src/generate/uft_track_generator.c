/**
 * @file uft_track_generator.c
 * @brief Track Generator Implementation
 * 
 * EXT4-011: Complete track generation for various formats
 * 
 * Features:
 * - Standard IBM/PC track generation
 * - Amiga track generation
 * - Variable sector size support
 * - Gap customization
 * - CRC calculation
 */

#include "uft/generate/uft_track_generator.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*===========================================================================
 * Constants
 *===========================================================================*/

/* Standard MFM gap sizes */
#define GAP4A_SIZE      80      /* Pre-index gap */
#define GAP1_SIZE       50      /* Post-index gap */
#define GAP2_SIZE       22      /* ID to data gap */
#define GAP3_SIZE_DD    54      /* Inter-sector gap (DD) */
#define GAP3_SIZE_HD    108     /* Inter-sector gap (HD) */
#define GAP4B_SIZE      200     /* Post-data gap */

/* Sync patterns */
#define SYNC_COUNT      12      /* Number of 0x00 sync bytes */
#define SYNC_A1_COUNT   3       /* Number of A1 sync marks */

/*===========================================================================
 * CRC-CCITT Calculation
 *===========================================================================*/

static uint16_t crc_ccitt_table[256];
static bool crc_table_init = false;

static void init_crc_table(void)
{
    if (crc_table_init) return;
    
    for (int i = 0; i < 256; i++) {
        uint16_t crc = i << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
        crc_ccitt_table[i] = crc;
    }
    crc_table_init = true;
}

static uint16_t crc_ccitt(const uint8_t *data, size_t len, uint16_t init)
{
    init_crc_table();
    
    uint16_t crc = init;
    for (size_t i = 0; i < len; i++) {
        crc = (crc << 8) ^ crc_ccitt_table[((crc >> 8) ^ data[i]) & 0xFF];
    }
    return crc;
}

/*===========================================================================
 * Track Buffer Management
 *===========================================================================*/

int uft_track_buffer_init(uft_track_buffer_t *buf, size_t capacity)
{
    if (!buf) return -1;
    
    memset(buf, 0, sizeof(*buf));
    
    buf->data = malloc(capacity);
    if (!buf->data) return -1;
    
    buf->capacity = capacity;
    buf->length = 0;
    
    return 0;
}

void uft_track_buffer_free(uft_track_buffer_t *buf)
{
    if (!buf) return;
    free(buf->data);
    memset(buf, 0, sizeof(*buf));
}

static int buf_append(uft_track_buffer_t *buf, uint8_t byte)
{
    if (buf->length >= buf->capacity) return -1;
    buf->data[buf->length++] = byte;
    return 0;
}

static int buf_append_n(uft_track_buffer_t *buf, uint8_t byte, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        if (buf_append(buf, byte) != 0) return -1;
    }
    return 0;
}

static int buf_append_data(uft_track_buffer_t *buf, const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        if (buf_append(buf, data[i]) != 0) return -1;
    }
    return 0;
}

/*===========================================================================
 * Gap Generation
 *===========================================================================*/

static int gen_gap(uft_track_buffer_t *buf, int size, uint8_t fill)
{
    return buf_append_n(buf, fill, size);
}

static int gen_sync(uft_track_buffer_t *buf)
{
    return buf_append_n(buf, 0x00, SYNC_COUNT);
}

static int gen_sync_a1(uft_track_buffer_t *buf)
{
    /* A1 sync mark (0xA1 with missing clock bit) */
    return buf_append_n(buf, 0xA1, SYNC_A1_COUNT);
}

/*===========================================================================
 * IBM MFM Track Generation
 *===========================================================================*/

int uft_gen_ibm_track(const uft_gen_params_t *params, uft_track_buffer_t *buf)
{
    if (!params || !buf || !buf->data) return -1;
    
    buf->length = 0;
    
    int sector_size = 128 << params->size_code;
    int gap3_size = params->density == UFT_DENSITY_HD ? GAP3_SIZE_HD : GAP3_SIZE_DD;
    
    if (params->gap3_size > 0) {
        gap3_size = params->gap3_size;
    }
    
    /* GAP 4a (pre-index) */
    gen_gap(buf, GAP4A_SIZE, 0x4E);
    
    /* Index Address Mark */
    gen_sync(buf);
    gen_sync_a1(buf);
    buf_append(buf, 0xFC);  /* IAM */
    
    /* GAP 1 (post-index) */
    gen_gap(buf, GAP1_SIZE, 0x4E);
    
    /* Generate sectors */
    for (int s = 0; s < params->sectors; s++) {
        int sector_id = params->interleave ? 
                        params->interleave[s] : (s + params->first_sector);
        
        /* ID Field */
        gen_sync(buf);
        gen_sync_a1(buf);
        buf_append(buf, 0xFE);  /* IDAM */
        
        uint8_t id_field[4] = {
            params->cylinder,
            params->head,
            sector_id,
            params->size_code
        };
        buf_append_data(buf, id_field, 4);
        
        /* ID CRC */
        uint16_t id_crc = crc_ccitt(id_field, 4, 0xB230);  /* Pre-init with A1s */
        buf_append(buf, (id_crc >> 8) & 0xFF);
        buf_append(buf, id_crc & 0xFF);
        
        /* GAP 2 (ID to Data) */
        gen_gap(buf, GAP2_SIZE, 0x4E);
        
        /* Data Field */
        gen_sync(buf);
        gen_sync_a1(buf);
        buf_append(buf, params->deleted ? 0xF8 : 0xFB);  /* DAM */
        
        /* Sector data */
        const uint8_t *sector_data = NULL;
        if (params->data) {
            sector_data = params->data + s * sector_size;
        }
        
        for (int b = 0; b < sector_size; b++) {
            buf_append(buf, sector_data ? sector_data[b] : params->fill_byte);
        }
        
        /* Data CRC */
        /* For proper CRC, need to include DAM */
        uint8_t *data_start = buf->data + buf->length - sector_size;
        uint16_t data_crc = crc_ccitt(data_start - 1, sector_size + 1, 0xB230);
        buf_append(buf, (data_crc >> 8) & 0xFF);
        buf_append(buf, data_crc & 0xFF);
        
        /* GAP 3 (inter-sector) */
        if (s < params->sectors - 1) {
            gen_gap(buf, gap3_size, 0x4E);
        }
    }
    
    /* GAP 4b (fill to track end) */
    while (buf->length < params->track_length) {
        buf_append(buf, 0x4E);
    }
    
    return 0;
}

/*===========================================================================
 * Amiga Track Generation
 *===========================================================================*/

static uint32_t amiga_checksum(const uint8_t *data, size_t len)
{
    uint32_t checksum = 0;
    for (size_t i = 0; i < len; i += 4) {
        uint32_t word = (data[i] << 24) | (data[i+1] << 16) | 
                       (data[i+2] << 8) | data[i+3];
        checksum ^= word;
    }
    return checksum & 0x55555555;
}

static void amiga_encode_odd_even(const uint8_t *src, uint8_t *dst, size_t len)
{
    /* Odd bits */
    for (size_t i = 0; i < len; i++) {
        dst[i] = ((src[i] >> 1) & 0x55) | 0xAA;
    }
    /* Even bits */
    for (size_t i = 0; i < len; i++) {
        dst[len + i] = (src[i] & 0x55) | 0xAA;
    }
}

int uft_gen_amiga_track(const uft_gen_params_t *params, uft_track_buffer_t *buf)
{
    if (!params || !buf || !buf->data) return -1;
    
    buf->length = 0;
    
    int sectors = params->density == UFT_DENSITY_HD ? 22 : 11;
    int sector_size = 512;
    
    for (int s = 0; s < sectors; s++) {
        /* Sync pattern */
        buf_append_n(buf, 0xAA, 2);  /* Gap */
        
        /* Sync word (0x4489 twice, but stored as raw) */
        buf_append(buf, 0x44);
        buf_append(buf, 0x89);
        buf_append(buf, 0x44);
        buf_append(buf, 0x89);
        
        /* Sector header */
        uint8_t header[4] = {
            0xFF,                    /* Format type */
            params->cylinder * 2 + params->head,  /* Track number */
            s,                       /* Sector number */
            sectors - s              /* Sectors to gap */
        };
        
        /* Encode header odd/even */
        uint8_t enc_header[8];
        amiga_encode_odd_even(header, enc_header, 4);
        buf_append_data(buf, enc_header, 8);
        
        /* Sector label (16 bytes, usually 0) */
        uint8_t label[16] = {0};
        uint8_t enc_label[32];
        amiga_encode_odd_even(label, enc_label, 16);
        buf_append_data(buf, enc_label, 32);
        
        /* Header checksum */
        uint32_t hdr_check = amiga_checksum(enc_header, 40);
        uint8_t hdr_check_bytes[4] = {
            (hdr_check >> 24) & 0xFF,
            (hdr_check >> 16) & 0xFF,
            (hdr_check >> 8) & 0xFF,
            hdr_check & 0xFF
        };
        uint8_t enc_hdr_check[8];
        amiga_encode_odd_even(hdr_check_bytes, enc_hdr_check, 4);
        buf_append_data(buf, enc_hdr_check, 8);
        
        /* Data checksum placeholder (calculate after encoding) */
        size_t checksum_pos = buf->length;
        buf_append_n(buf, 0x00, 8);
        
        /* Sector data */
        const uint8_t *sector_data = NULL;
        if (params->data) {
            sector_data = params->data + s * sector_size;
        }
        
        uint8_t data_block[512];
        for (int b = 0; b < 512; b++) {
            data_block[b] = sector_data ? sector_data[b] : params->fill_byte;
        }
        
        /* Encode data odd/even */
        uint8_t enc_data[1024];
        amiga_encode_odd_even(data_block, enc_data, 512);
        buf_append_data(buf, enc_data, 1024);
        
        /* Calculate and insert data checksum */
        uint32_t data_check = amiga_checksum(enc_data, 1024);
        uint8_t data_check_bytes[4] = {
            (data_check >> 24) & 0xFF,
            (data_check >> 16) & 0xFF,
            (data_check >> 8) & 0xFF,
            data_check & 0xFF
        };
        uint8_t enc_data_check[8];
        amiga_encode_odd_even(data_check_bytes, enc_data_check, 4);
        memcpy(buf->data + checksum_pos, enc_data_check, 8);
    }
    
    /* Fill to track length */
    while (buf->length < params->track_length) {
        buf_append(buf, 0xAA);
    }
    
    return 0;
}

/*===========================================================================
 * Format Track
 *===========================================================================*/

int uft_gen_format_track(uft_gen_format_t format, 
                         const uft_gen_params_t *params,
                         uft_track_buffer_t *buf)
{
    switch (format) {
        case UFT_GEN_FORMAT_IBM:
            return uft_gen_ibm_track(params, buf);
            
        case UFT_GEN_FORMAT_AMIGA:
            return uft_gen_amiga_track(params, buf);
            
        case UFT_GEN_FORMAT_FM:
            /* FM format (single density) */
            {
                uft_gen_params_t fm_params = *params;
                fm_params.density = UFT_DENSITY_SD;
                /* FM uses different sync/gap structure - simplified */
                return uft_gen_ibm_track(&fm_params, buf);
            }
            
        default:
            return -1;
    }
}

/*===========================================================================
 * Preset Configurations
 *===========================================================================*/

void uft_gen_preset_ibm_720k(uft_gen_params_t *params)
{
    if (!params) return;
    memset(params, 0, sizeof(*params));
    
    params->sectors = 9;
    params->size_code = 2;  /* 512 bytes */
    params->first_sector = 1;
    params->density = UFT_DENSITY_DD;
    params->fill_byte = 0xF6;
    params->track_length = 12500;  /* ~200ms at 250kbps */
    params->gap3_size = GAP3_SIZE_DD;
}

void uft_gen_preset_ibm_1440k(uft_gen_params_t *params)
{
    if (!params) return;
    memset(params, 0, sizeof(*params));
    
    params->sectors = 18;
    params->size_code = 2;
    params->first_sector = 1;
    params->density = UFT_DENSITY_HD;
    params->fill_byte = 0xF6;
    params->track_length = 25000;  /* ~200ms at 500kbps */
    params->gap3_size = GAP3_SIZE_HD;
}

void uft_gen_preset_amiga_dd(uft_gen_params_t *params)
{
    if (!params) return;
    memset(params, 0, sizeof(*params));
    
    params->sectors = 11;
    params->size_code = 2;
    params->first_sector = 0;
    params->density = UFT_DENSITY_DD;
    params->fill_byte = 0x00;
    params->track_length = 12668;  /* Standard Amiga DD track */
}

void uft_gen_preset_amiga_hd(uft_gen_params_t *params)
{
    if (!params) return;
    memset(params, 0, sizeof(*params));
    
    params->sectors = 22;
    params->size_code = 2;
    params->first_sector = 0;
    params->density = UFT_DENSITY_HD;
    params->fill_byte = 0x00;
    params->track_length = 25336;
}

/*===========================================================================
 * Interleave Generation
 *===========================================================================*/

int uft_gen_interleave(int sectors, int interleave, int first,
                       int *table, size_t table_size)
{
    if (!table || table_size < (size_t)sectors || interleave < 1) return -1;
    
    /* Initialize table with -1 (unassigned) */
    for (int i = 0; i < sectors; i++) {
        table[i] = -1;
    }
    
    int pos = 0;
    int sector = first;
    
    for (int i = 0; i < sectors; i++) {
        /* Find next free position */
        while (table[pos] != -1) {
            pos = (pos + 1) % sectors;
        }
        
        table[pos] = sector;
        sector++;
        
        /* Skip interleave positions */
        for (int j = 0; j < interleave && i < sectors - 1; j++) {
            pos = (pos + 1) % sectors;
            while (table[pos] != -1) {
                pos = (pos + 1) % sectors;
            }
        }
    }
    
    return 0;
}
