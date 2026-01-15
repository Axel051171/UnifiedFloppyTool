/**
 * @file uft_udi_format.c
 * @brief UDI (Ultra Disk Image) Format Support
 * 
 * UDI is a ZX Spectrum disk image format developed by Alexander Makeev.
 * It stores byte-level track images including gaps, sync bytes, and markers.
 * 
 * Format specification: http://speccy.info/UDI
 * 
 * Features:
 * - Byte-level track storage (not sector-level)
 * - Sync byte bitmap per track
 * - CRC-32 validation
 * - Multi-revolution support
 * 
 * @license MIT (UFT portions), GPL v3 (derived from FDRC)
 */

#include "uft/ride/uft_flux_decoder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/*============================================================================
 * UDI FORMAT CONSTANTS
 *============================================================================*/

#define UDI_SIGNATURE       0x21494455  /* "UDI!" little-endian */
#define UDI_VERSION         0x00        /* Current version */

/* UDI Header (16 bytes) */
typedef struct {
    uint32_t signature;     /* "UDI!" = 0x21494455 */
    uint32_t file_size;     /* Total file size including CRC */
    uint8_t  version;       /* Format version (0) */
    uint8_t  max_cylinder;  /* Maximum cylinder number */
    uint8_t  max_head;      /* Maximum head (0 or 1) */
    uint8_t  reserved;      /* Reserved, must be 0 */
    uint32_t ext_header;    /* Extended header size (usually 0) */
} __attribute__((packed)) udi_header_t;

/* UDI Track Header (3 bytes) */
typedef struct {
    uint8_t  type;          /* Track type (0 = MFM) */
    uint16_t length;        /* Track data length in bytes */
} __attribute__((packed)) udi_track_header_t;

/*============================================================================
 * CRC-32 IMPLEMENTATION (UDI-specific polynomial)
 *============================================================================*/

static uint32_t udi_crc32_table[256];
static bool udi_crc32_initialized = false;

/**
 * @brief Initialize UDI CRC-32 lookup table
 */
static void udi_crc32_init(void) {
    if (udi_crc32_initialized) return;
    
    for (int i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
        udi_crc32_table[i] = crc;
    }
    udi_crc32_initialized = true;
}

/**
 * @brief Calculate UDI CRC-32 for a single byte
 */
static inline uint32_t udi_crc32_byte(uint32_t crc, uint8_t byte) {
    crc ^= 0xFFFFFFFF ^ byte;
    crc = (crc >> 8) ^ udi_crc32_table[crc & 0xFF];
    crc ^= 0xFFFFFFFF;
    return crc;
}

/**
 * @brief Calculate UDI CRC-32 for a buffer
 */
static uint32_t udi_crc32_buffer(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= 0xFFFFFFFF ^ data[i];
        crc = (crc >> 8) ^ udi_crc32_table[crc & 0xFF];
        crc ^= 0xFFFFFFFF;
    }
    return crc;
}

/*============================================================================
 * UDI LOADER IMPLEMENTATION
 *============================================================================*/

/**
 * @brief Get UDI file information
 * @param path Path to UDI file
 * @param info Output info structure
 * @return 0 on success, -1 on error
 */
int uft_udi_get_info(const char *path, uft_udi_info_t *info) {
    if (!path || !info) return -1;
    
    udi_crc32_init();
    memset(info, 0, sizeof(*info));
    
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    
    /* Get file size */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (file_size < (long)(sizeof(udi_header_t) + 4)) {
        fclose(f);
        return -1;
    }
    
    /* Read header */
    udi_header_t header;
    if (fread(&header, 1, sizeof(header), f) != sizeof(header)) {
        fclose(f);
        return -1;
    }
    
    /* Validate signature */
    if (header.signature != UDI_SIGNATURE) {
        fclose(f);
        return -1;
    }
    
    info->version = header.version;
    info->cylinders = header.max_cylinder + 1;
    info->heads = header.max_head + 1;
    info->file_size = header.file_size;
    
    /* Read and validate CRC */
    fseek(f, file_size - 4, SEEK_SET);
    uint32_t stored_crc;
    if (fread(&stored_crc, 1, 4, f) != 4) {
        fclose(f);
        return -1;
    }
    info->stored_crc = stored_crc;
    
    /* Calculate CRC over file content (excluding CRC itself) */
    fseek(f, 0, SEEK_SET);
    uint8_t *buffer = malloc(file_size - 4);
    if (!buffer) {
        fclose(f);
        return -1;
    }
    
    if (fread(buffer, 1, file_size - 4, f) != (size_t)(file_size - 4)) {
        free(buffer);
        fclose(f);
        return -1;
    }
    
    info->calculated_crc = udi_crc32_buffer(buffer, file_size - 4);
    info->crc_valid = (info->stored_crc == info->calculated_crc);
    
    free(buffer);
    fclose(f);
    return 0;
}

/**
 * @brief Load UDI track data
 * @param path Path to UDI file
 * @param cylinder Cylinder to load
 * @param head Head to load
 * @param track_data Output track data (caller must free)
 * @param track_len Output track length
 * @param sync_map Output sync byte bitmap (caller must free, may be NULL)
 * @param sync_len Output sync map length
 * @return 0 on success, -1 on error
 */
int uft_udi_load_track(const char *path, int cylinder, int head,
                        uint8_t **track_data, size_t *track_len,
                        uint8_t **sync_map, size_t *sync_len) {
    if (!path || !track_data || !track_len) return -1;
    
    *track_data = NULL;
    *track_len = 0;
    if (sync_map) *sync_map = NULL;
    if (sync_len) *sync_len = 0;
    
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    
    /* Read and validate header */
    udi_header_t header;
    if (fread(&header, 1, sizeof(header), f) != sizeof(header)) {
        fclose(f);
        return -1;
    }
    
    if (header.signature != UDI_SIGNATURE) {
        fclose(f);
        return -1;
    }
    
    /* Check bounds */
    if (cylinder > header.max_cylinder || head > header.max_head) {
        fclose(f);
        return -1;
    }
    
    /* Skip extended header if present */
    if (header.ext_header > 0) {
        fseek(f, header.ext_header, SEEK_CUR);
    }
    
    /* Navigate to requested track */
    int num_heads = header.max_head + 1;
    int target_track = cylinder * num_heads + head;
    
    for (int t = 0; t < target_track; t++) {
        udi_track_header_t trk_hdr;
        if (fread(&trk_hdr, 1, sizeof(trk_hdr), f) != sizeof(trk_hdr)) {
            fclose(f);
            return -1;
        }
        
        /* Skip track data + sync bitmap */
        size_t sync_size = (trk_hdr.length + 7) / 8;
        fseek(f, trk_hdr.length + sync_size, SEEK_CUR);
    }
    
    /* Read target track header */
    udi_track_header_t trk_hdr;
    if (fread(&trk_hdr, 1, sizeof(trk_hdr), f) != sizeof(trk_hdr)) {
        fclose(f);
        return -1;
    }
    
    /* Allocate and read track data */
    *track_data = malloc(trk_hdr.length);
    if (!*track_data) {
        fclose(f);
        return -1;
    }
    
    if (fread(*track_data, 1, trk_hdr.length, f) != trk_hdr.length) {
        free(*track_data);
        *track_data = NULL;
        fclose(f);
        return -1;
    }
    *track_len = trk_hdr.length;
    
    /* Read sync bitmap if requested */
    size_t sync_size = (trk_hdr.length + 7) / 8;
    if (sync_map && sync_len) {
        *sync_map = malloc(sync_size);
        if (*sync_map) {
            if (fread(*sync_map, 1, sync_size, f) == sync_size) {
                *sync_len = sync_size;
            } else {
                free(*sync_map);
                *sync_map = NULL;
            }
        }
    }
    
    fclose(f);
    return 0;
}

/*============================================================================
 * UDI WRITER IMPLEMENTATION
 *============================================================================*/

/**
 * @brief Write UDI file from track data
 * @param path Output file path
 * @param tracks Array of track data [cylinder][head]
 * @param num_cylinders Number of cylinders
 * @param num_heads Number of heads (1 or 2)
 * @return 0 on success, -1 on error
 */
int uft_udi_write(const char *path, 
                   uft_udi_track_data_t tracks[][2],
                   int num_cylinders, int num_heads) {
    if (!path || !tracks || num_cylinders < 1 || num_heads < 1 || num_heads > 2) {
        return -1;
    }
    
    udi_crc32_init();
    
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    
    /* Build file in memory for CRC calculation */
    size_t total_size = sizeof(udi_header_t);
    
    /* Calculate total size */
    for (int c = 0; c < num_cylinders; c++) {
        for (int h = 0; h < num_heads; h++) {
            if (tracks[c][h].data) {
                total_size += sizeof(udi_track_header_t);
                total_size += tracks[c][h].length;
                total_size += (tracks[c][h].length + 7) / 8;  /* Sync map */
            }
        }
    }
    
    uint8_t *buffer = malloc(total_size);
    if (!buffer) {
        fclose(f);
        return -1;
    }
    
    size_t pos = 0;
    
    /* Write header */
    udi_header_t header = {
        .signature = UDI_SIGNATURE,
        .file_size = (uint32_t)total_size,
        .version = UDI_VERSION,
        .max_cylinder = num_cylinders - 1,
        .max_head = num_heads - 1,
        .reserved = 0,
        .ext_header = 0
    };
    memcpy(buffer + pos, &header, sizeof(header));
    pos += sizeof(header);
    
    /* Write tracks */
    for (int c = 0; c < num_cylinders; c++) {
        for (int h = 0; h < num_heads; h++) {
            if (tracks[c][h].data) {
                /* Track header */
                udi_track_header_t trk_hdr = {
                    .type = 0,  /* MFM */
                    .length = (uint16_t)tracks[c][h].length
                };
                memcpy(buffer + pos, &trk_hdr, sizeof(trk_hdr));
                pos += sizeof(trk_hdr);
                
                /* Track data */
                memcpy(buffer + pos, tracks[c][h].data, tracks[c][h].length);
                pos += tracks[c][h].length;
                
                /* Sync map */
                size_t sync_size = (tracks[c][h].length + 7) / 8;
                if (tracks[c][h].sync_map) {
                    memcpy(buffer + pos, tracks[c][h].sync_map, sync_size);
                } else {
                    memset(buffer + pos, 0, sync_size);
                }
                pos += sync_size;
            }
        }
    }
    
    /* Calculate CRC */
    uint32_t crc = udi_crc32_buffer(buffer, total_size);
    
    /* Write file */
    fwrite(buffer, 1, total_size, f);
    fwrite(&crc, 1, 4, f);
    
    free(buffer);
    fclose(f);
    return 0;
}

/*============================================================================
 * UDI CONVERSION UTILITIES
 *============================================================================*/

/**
 * @brief Convert MFM bitstream to UDI track format
 * @param mfm_data MFM bitstream data
 * @param mfm_bits Number of bits
 * @param track_data Output track data (caller must free)
 * @param track_len Output track length
 * @param sync_map Output sync bitmap (caller must free)
 * @param sync_len Output sync map length
 * @return 0 on success, -1 on error
 */
int uft_mfm_to_udi_track(const uint8_t *mfm_data, size_t mfm_bits,
                          uint8_t **track_data, size_t *track_len,
                          uint8_t **sync_map, size_t *sync_len) {
    if (!mfm_data || mfm_bits < 16 || !track_data || !track_len) {
        return -1;
    }
    
    /* UDI stores decoded bytes, not raw MFM bits */
    /* We need to decode MFM and track sync positions */
    
    size_t max_bytes = mfm_bits / 16 + 1;
    *track_data = malloc(max_bytes);
    if (!*track_data) return -1;
    
    size_t sync_size = (max_bytes + 7) / 8;
    if (sync_map && sync_len) {
        *sync_map = calloc(1, sync_size);
        if (!*sync_map) {
            free(*track_data);
            *track_data = NULL;
            return -1;
        }
        *sync_len = sync_size;
    }
    
    /* Decode MFM to bytes */
    size_t byte_count = 0;
    uint16_t shift_reg = 0;
    int bit_count = 0;
    
    for (size_t i = 0; i < mfm_bits; i++) {
        int byte_idx = i / 8;
        int bit_idx = 7 - (i % 8);
        int bit = (mfm_data[byte_idx] >> bit_idx) & 1;
        
        shift_reg = (shift_reg << 1) | bit;
        bit_count++;
        
        if (bit_count >= 16) {
            /* Check for sync patterns */
            bool is_sync = false;
            uint8_t decoded_byte;
            
            if ((shift_reg & 0xFFFF) == 0x4489) {
                /* A1 sync (missing clock) */
                decoded_byte = 0xA1;
                is_sync = true;
            } else if ((shift_reg & 0xFFFF) == 0x5224) {
                /* C2 sync (missing clock) */
                decoded_byte = 0xC2;
                is_sync = true;
            } else {
                /* Normal MFM decode: extract data bits (odd positions) */
                decoded_byte = 0;
                for (int b = 0; b < 8; b++) {
                    decoded_byte |= ((shift_reg >> (14 - b * 2)) & 1) << (7 - b);
                }
            }
            
            (*track_data)[byte_count] = decoded_byte;
            
            if (is_sync && sync_map && *sync_map) {
                (*sync_map)[byte_count / 8] |= (1 << (byte_count % 8));
            }
            
            byte_count++;
            bit_count = 0;
            
            if (byte_count >= max_bytes) break;
        }
    }
    
    *track_len = byte_count;
    
    /* Shrink allocations to actual size */
    uint8_t *new_data = realloc(*track_data, byte_count);
    if (new_data) *track_data = new_data;
    
    if (sync_map && *sync_map && sync_len) {
        size_t actual_sync_size = (byte_count + 7) / 8;
        uint8_t *new_sync = realloc(*sync_map, actual_sync_size);
        if (new_sync) {
            *sync_map = new_sync;
            *sync_len = actual_sync_size;
        }
    }
    
    return 0;
}

/**
 * @brief Verify UDI file integrity
 * @param path Path to UDI file
 * @param result Output verification result
 * @return 0 on success, -1 on error
 */
int uft_verify_udi(const char *path, uft_verify_result_t *result) {
    if (!path || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    result->format_name = "UDI";
    
    uft_udi_info_t info;
    if (uft_udi_get_info(path, &info) != 0) {
        result->valid = false;
        result->error_code = -1;
        snprintf(result->details, sizeof(result->details),
                 "Failed to read UDI file");
        return -1;
    }
    
    result->valid = info.crc_valid;
    result->error_code = info.crc_valid ? 0 : 1;
    
    snprintf(result->details, sizeof(result->details),
             "UDI v%d, %d cylinders, %d heads, CRC %s (0x%08X)",
             info.version,
             info.cylinders,
             info.heads,
             info.crc_valid ? "OK" : "FAIL",
             info.stored_crc);
    
    return 0;
}

/*============================================================================
 * UDI SECTOR EXTRACTION
 *============================================================================*/

/**
 * @brief Extract sectors from UDI track data
 * @param track_data UDI track data
 * @param track_len Track data length
 * @param sync_map Sync byte bitmap (may be NULL)
 * @param sectors Output sector array (caller allocated)
 * @param max_sectors Maximum sectors to extract
 * @return Number of sectors found, or -1 on error
 */
int uft_udi_extract_sectors(const uint8_t *track_data, size_t track_len,
                             const uint8_t *sync_map,
                             uft_sector_t *sectors, int max_sectors) {
    if (!track_data || track_len < 10 || !sectors || max_sectors < 1) {
        return -1;
    }
    
    int sector_count = 0;
    size_t pos = 0;
    
    /* State machine for IDAM processing */
    enum { WAIT_A1, WAIT_FE, GET_HDR, WAIT_DATA_A1, WAIT_DAM, GET_DATA } state = WAIT_A1;
    
    uint8_t hdr_track = 0, hdr_side = 0, hdr_sector = 0, hdr_length = 0;
    uint16_t hdr_crc = 0;
    size_t data_start = 0;
    int data_remaining = 0;
    
    while (pos < track_len && sector_count < max_sectors) {
        uint8_t byte = track_data[pos];
        bool is_sync = sync_map && (sync_map[pos / 8] & (1 << (pos % 8)));
        
        switch (state) {
            case WAIT_A1:
                if (byte == 0xA1 && is_sync) {
                    state = WAIT_FE;
                }
                break;
                
            case WAIT_FE:
                if (byte == 0xA1 && is_sync) {
                    /* Stay in WAIT_FE */
                } else if (byte == 0xFE) {
                    state = GET_HDR;
                    data_remaining = 6;  /* Track, Side, Sector, Length, CRC (2) */
                } else {
                    state = WAIT_A1;
                }
                break;
                
            case GET_HDR:
                data_remaining--;
                switch (data_remaining) {
                    case 5: hdr_track = byte; break;
                    case 4: hdr_side = byte; break;
                    case 3: hdr_sector = byte; break;
                    case 2: hdr_length = byte; break;
                    case 1: hdr_crc = byte << 8; break;
                    case 0:
                        hdr_crc |= byte;
                        state = WAIT_DATA_A1;
                        break;
                }
                break;
                
            case WAIT_DATA_A1:
                if (byte == 0xA1 && is_sync) {
                    state = WAIT_DAM;
                } else if (byte == 0xFE) {
                    /* Another IDAM, restart */
                    state = GET_HDR;
                    data_remaining = 6;
                }
                break;
                
            case WAIT_DAM:
                if (byte == 0xA1 && is_sync) {
                    /* Stay in WAIT_DAM */
                } else if (byte == 0xFB || byte == 0xF8) {
                    /* Data Address Mark found */
                    state = GET_DATA;
                    data_start = pos + 1;
                    int sector_size = 128 << (hdr_length & 3);
                    data_remaining = sector_size + 2;  /* Data + CRC */
                } else if (byte == 0xFE) {
                    state = GET_HDR;
                    data_remaining = 6;
                } else {
                    state = WAIT_A1;
                }
                break;
                
            case GET_DATA:
                data_remaining--;
                if (data_remaining == 0) {
                    /* Complete sector found */
                    int sector_size = 128 << (hdr_length & 3);
                    
                    memset(&sectors[sector_count], 0, sizeof(uft_sector_t));
                    sectors[sector_count].header.id.cylinder = hdr_track;
                    sectors[sector_count].header.id.head = hdr_side;
                    sectors[sector_count].header.id.sector = hdr_sector;
                    sectors[sector_count].header.id.size_code = hdr_length;
                    sectors[sector_count].data_size = sector_size;
                    
                    /* Copy sector data */
                    if (data_start + sector_size <= track_len) {
                        sectors[sector_count].data = malloc(sector_size);
                        if (sectors[sector_count].data) {
                            memcpy(sectors[sector_count].data, 
                                   track_data + data_start, sector_size);
                        }
                    }
                    
                    sector_count++;
                    state = WAIT_A1;
                }
                break;
        }
        
        pos++;
    }
    
    return sector_count;
}
