/**
 * @file uft_apd_format.c
 * @brief Acorn Protected Disk (APD) Format Handler - Implementation
 * 
 * @version 1.0.0
 * @date 2026-01-15
 */

#include "uft/uft_apd_format.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

/*===========================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

struct uft_apd_context {
    char *path;
    
    /* Track table from header */
    uft_apd_track_entry_t tracks[UFT_APD_NUM_TRACKS];
    
    /* Raw uncompressed data */
    uint8_t *data;
    size_t data_size;
    
    /* Track data offsets */
    size_t track_offsets[UFT_APD_NUM_TRACKS][UFT_APD_NUM_DENSITIES];
    
    /* Info */
    uft_apd_info_t info;
    bool loaded;
    bool modified;
};

/*===========================================================================
 * CONSTANTS
 *===========================================================================*/

/* FM address marks (single density) */
#define FM_IAM      0xFC    /* Index Address Mark */
#define FM_IDAM     0xFE    /* ID Address Mark */
#define FM_DAM      0xFB    /* Data Address Mark */
#define FM_DDAM     0xF8    /* Deleted Data Address Mark */

/* MFM sync patterns */
#define MFM_SYNC    0x4489  /* Standard MFM sync */
#define MFM_IAM     0x5224  /* Index mark pattern */

/*===========================================================================
 * HELPERS
 *===========================================================================*/

/**
 * @brief Read 32-bit little-endian from buffer
 */
static uint32_t read_le32(const uint8_t *p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

/**
 * @brief Write 32-bit little-endian to buffer
 */
static void write_le32(uint8_t *p, uint32_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF;
    p[3] = (v >> 24) & 0xFF;
}

/**
 * @brief Get bit from big-endian bitstream
 */
static int get_bit(const uint8_t *data, int bit) {
    return (data[bit / 8] >> (7 - (bit % 8))) & 1;
}

/**
 * @brief Bits to bytes (rounded up)
 */
static size_t bits_to_bytes(uint32_t bits) {
    return (bits + 7) / 8;
}

/*===========================================================================
 * LIFECYCLE
 *===========================================================================*/

uft_apd_t* uft_apd_create(void) {
    uft_apd_t *apd = (uft_apd_t *)calloc(1, sizeof(*apd));
    return apd;
}

void uft_apd_destroy(uft_apd_t *apd) {
    if (!apd) return;
    uft_apd_close(apd);
    free(apd);
}

void uft_apd_close(uft_apd_t *apd) {
    if (!apd) return;
    free(apd->path);
    apd->path = NULL;
    free(apd->data);
    apd->data = NULL;
    apd->data_size = 0;
    apd->loaded = false;
    memset(&apd->info, 0, sizeof(apd->info));
}

/*===========================================================================
 * FILE OPERATIONS
 *===========================================================================*/

int uft_apd_open(uft_apd_t *apd, const char *path) {
    if (!apd || !path) return -1;
    
    uft_apd_close(apd);
    
    /* Open file */
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    
    /* Get file size */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (file_size <= 0) {
        fclose(f);
        return -1;
    }
    
    /* Read compressed data */
    uint8_t *compressed = (uint8_t *)malloc(file_size);
    if (!compressed) {
        fclose(f);
        return -1;
    }
    
    fread(compressed, 1, file_size, f);
    fclose(f);
    
    /* Check if gzip compressed */
    bool is_gzip = (compressed[0] == 0x1F && compressed[1] == 0x8B);
    
    if (is_gzip) {
        /* Decompress with zlib */
        /* Estimate uncompressed size (max ~4MB for floppy) */
        size_t max_size = 4 * 1024 * 1024;
        apd->data = (uint8_t *)malloc(max_size);
        if (!apd->data) {
            free(compressed);
            return -1;
        }
        
        z_stream strm;
        memset(&strm, 0, sizeof(strm));
        strm.next_in = compressed;
        strm.avail_in = file_size;
        strm.next_out = apd->data;
        strm.avail_out = max_size;
        
        /* Use inflateInit2 with 16+MAX_WBITS for gzip */
        if (inflateInit2(&strm, 16 + MAX_WBITS) != Z_OK) {
            free(compressed);
            free(apd->data);
            apd->data = NULL;
            return -1;
        }
        
        int ret = inflate(&strm, Z_FINISH);
        inflateEnd(&strm);
        
        if (ret != Z_STREAM_END) {
            free(compressed);
            free(apd->data);
            apd->data = NULL;
            return -1;
        }
        
        apd->data_size = strm.total_out;
        free(compressed);
    } else {
        /* Already uncompressed */
        apd->data = compressed;
        apd->data_size = file_size;
    }
    
    /* Verify magic */
    if (apd->data_size < UFT_APD_HEADER_SIZE ||
        memcmp(apd->data, UFT_APD_MAGIC, UFT_APD_MAGIC_LEN) != 0) {
        free(apd->data);
        apd->data = NULL;
        apd->data_size = 0;
        return -1;
    }
    
    /* Parse track table */
    const uint8_t *p = apd->data + UFT_APD_MAGIC_LEN;
    size_t data_offset = UFT_APD_HEADER_SIZE;
    
    for (int t = 0; t < UFT_APD_NUM_TRACKS; t++) {
        apd->tracks[t].sd_bits = read_le32(p);
        apd->tracks[t].dd_bits = read_le32(p + 4);
        apd->tracks[t].qd_bits = read_le32(p + 8);
        p += 12;
        
        /* Calculate data offsets */
        apd->track_offsets[t][UFT_APD_DENSITY_SD] = data_offset;
        data_offset += bits_to_bytes(apd->tracks[t].sd_bits);
        
        apd->track_offsets[t][UFT_APD_DENSITY_DD] = data_offset;
        data_offset += bits_to_bytes(apd->tracks[t].dd_bits);
        
        apd->track_offsets[t][UFT_APD_DENSITY_QD] = data_offset;
        data_offset += bits_to_bytes(apd->tracks[t].qd_bits);
        
        /* Update info */
        if (apd->tracks[t].sd_bits > 0) apd->info.has_sd = true;
        if (apd->tracks[t].dd_bits > 0) apd->info.has_dd = true;
        if (apd->tracks[t].qd_bits > 0) apd->info.has_qd = true;
        if (apd->tracks[t].sd_bits > 0 || apd->tracks[t].dd_bits > 0 || 
            apd->tracks[t].qd_bits > 0) {
            apd->info.num_tracks = t + 1;
        }
    }
    
    apd->info.total_size = apd->data_size;
    apd->path = strdup(path);
    apd->loaded = true;
    
    /* Detect format */
    apd->info.format = uft_apd_detect_format(apd);
    
    return 0;
}

int uft_apd_save(uft_apd_t *apd, const char *path) {
    if (!apd || !apd->loaded || !path) return -1;
    
    /* Compress with gzip */
    size_t max_compressed = compressBound(apd->data_size);
    uint8_t *compressed = (uint8_t *)malloc(max_compressed + 20);
    if (!compressed) return -1;
    
    z_stream strm;
    memset(&strm, 0, sizeof(strm));
    strm.next_in = apd->data;
    strm.avail_in = apd->data_size;
    strm.next_out = compressed;
    strm.avail_out = max_compressed;
    
    /* Use deflateInit2 with 16+MAX_WBITS for gzip */
    if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                     16 + MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        free(compressed);
        return -1;
    }
    
    int ret = deflate(&strm, Z_FINISH);
    deflateEnd(&strm);
    
    if (ret != Z_STREAM_END) {
        free(compressed);
        return -1;
    }
    
    /* Write to file */
    FILE *f = fopen(path, "wb");
    if (!f) {
        free(compressed);
        return -1;
    }
    
    fwrite(compressed, 1, strm.total_out, f);
    fclose(f);
    free(compressed);
    
    return 0;
}

int uft_apd_get_info(uft_apd_t *apd, uft_apd_info_t *info) {
    if (!apd || !info) return -1;
    *info = apd->info;
    return 0;
}

/*===========================================================================
 * DETECTION
 *===========================================================================*/

int uft_apd_detect(const uint8_t *data, size_t size) {
    if (!data || size < UFT_APD_HEADER_SIZE) return 0;
    
    int score = 0;
    
    /* Check magic (uncompressed) */
    if (memcmp(data, UFT_APD_MAGIC, UFT_APD_MAGIC_LEN) == 0) {
        score = 100;
    }
    /* Check gzip magic */
    else if (data[0] == 0x1F && data[1] == 0x8B) {
        /* Could be gzipped APD - need to decompress to verify */
        score = 30;  /* Tentative */
    }
    
    return score;
}

bool uft_apd_detect_file(const char *path) {
    if (!path) return false;
    
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    
    uint8_t header[16];
    size_t read = fread(header, 1, sizeof(header), f);
    fclose(f);
    
    if (read < sizeof(header)) return false;
    
    return uft_apd_detect(header, read) >= 50;
}

int uft_apd_detect_format(uft_apd_t *apd) {
    if (!apd || !apd->loaded) return -1;
    
    /* Check track 0 for format detection */
    if (apd->info.has_qd) {
        return UFT_ADFS_FORMAT_F;  /* 1600K F format */
    }
    
    if (apd->info.has_dd) {
        /* Could be D or E format - check boot sector */
        return UFT_ADFS_FORMAT_E;  /* Assume E format (most common) */
    }
    
    if (apd->info.has_sd) {
        /* S, M, or L format */
        if (apd->info.num_tracks <= 40) {
            return UFT_ADFS_FORMAT_S;
        } else if (apd->info.num_tracks <= 80) {
            return UFT_ADFS_FORMAT_M;
        } else {
            return UFT_ADFS_FORMAT_L;
        }
    }
    
    return -1;
}

/*===========================================================================
 * TRACK OPERATIONS
 *===========================================================================*/

int uft_apd_read_track(uft_apd_t *apd, int track_num, uft_apd_track_t *track) {
    if (!apd || !apd->loaded || !track) return -1;
    if (track_num < 0 || track_num >= UFT_APD_NUM_TRACKS) return -1;
    
    memset(track, 0, sizeof(*track));
    track->track_num = track_num;
    track->cylinder = uft_apd_cylinder(track_num);
    track->head = uft_apd_head(track_num);
    
    /* Copy SD data */
    track->sd_bits = apd->tracks[track_num].sd_bits;
    track->sd_size = bits_to_bytes(track->sd_bits);
    if (track->sd_size > 0) {
        track->sd_data = (uint8_t *)malloc(track->sd_size);
        if (track->sd_data) {
            memcpy(track->sd_data, 
                   apd->data + apd->track_offsets[track_num][UFT_APD_DENSITY_SD],
                   track->sd_size);
        }
    }
    
    /* Copy DD data */
    track->dd_bits = apd->tracks[track_num].dd_bits;
    track->dd_size = bits_to_bytes(track->dd_bits);
    if (track->dd_size > 0) {
        track->dd_data = (uint8_t *)malloc(track->dd_size);
        if (track->dd_data) {
            memcpy(track->dd_data,
                   apd->data + apd->track_offsets[track_num][UFT_APD_DENSITY_DD],
                   track->dd_size);
        }
    }
    
    /* Copy QD data */
    track->qd_bits = apd->tracks[track_num].qd_bits;
    track->qd_size = bits_to_bytes(track->qd_bits);
    if (track->qd_size > 0) {
        track->qd_data = (uint8_t *)malloc(track->qd_size);
        if (track->qd_data) {
            memcpy(track->qd_data,
                   apd->data + apd->track_offsets[track_num][UFT_APD_DENSITY_QD],
                   track->qd_size);
        }
    }
    
    return 0;
}

void uft_apd_track_free(uft_apd_track_t *track) {
    if (!track) return;
    free(track->sd_data);
    free(track->dd_data);
    free(track->qd_data);
    memset(track, 0, sizeof(*track));
}

int uft_apd_get_track(uft_apd_t *apd, int cylinder, int head,
                       uft_apd_track_t *track) {
    return uft_apd_read_track(apd, uft_apd_track_num(cylinder, head), track);
}

/*===========================================================================
 * FM/MFM DECODING
 *===========================================================================*/

int uft_apd_find_fm_sync(const uint8_t *data, size_t bits, int start) {
    if (!data || bits < 32) return -1;
    
    /* FM uses clock bits, looking for address mark pattern */
    /* FM sync is typically 6 bytes of 0x00 followed by address mark */
    for (size_t bit = start; bit + 16 <= bits; bit++) {
        /* Look for FM IDAM pattern (0xFE with clock) */
        int byte = uft_apd_decode_fm_byte(data, bit);
        if (byte == FM_IDAM) {
            return (int)bit;
        }
    }
    
    return -1;
}

int uft_apd_find_mfm_sync(const uint8_t *data, size_t bits, int start) {
    if (!data || bits < 32) return -1;
    
    /* MFM sync is 0x4489 (three times) */
    for (size_t bit = start; bit + 48 <= bits; bit++) {
        uint16_t w1 = 0, w2 = 0, w3 = 0;
        
        for (int i = 0; i < 16; i++) {
            w1 = (w1 << 1) | get_bit(data, bit + i);
            w2 = (w2 << 1) | get_bit(data, bit + 16 + i);
            w3 = (w3 << 1) | get_bit(data, bit + 32 + i);
        }
        
        if (w1 == MFM_SYNC && w2 == MFM_SYNC && w3 == MFM_SYNC) {
            return (int)bit;
        }
    }
    
    return -1;
}

int uft_apd_decode_fm_byte(const uint8_t *data, int bit_offset) {
    if (!data) return -1;
    
    /* FM: each data bit preceded by clock bit */
    /* So 16 bits encode 8 data bits */
    int byte = 0;
    for (int i = 0; i < 8; i++) {
        /* Skip clock bit, read data bit */
        int data_bit = get_bit(data, bit_offset + i * 2 + 1);
        byte = (byte << 1) | data_bit;
    }
    
    return byte;
}

int uft_apd_decode_mfm_byte(const uint8_t *data, int bit_offset) {
    if (!data) return -1;
    
    /* MFM: clock bits only between consecutive 0s */
    /* 16 bits encode 8 data bits */
    int byte = 0;
    for (int i = 0; i < 8; i++) {
        /* In MFM, data bits are at odd positions */
        int data_bit = get_bit(data, bit_offset + i * 2 + 1);
        byte = (byte << 1) | data_bit;
    }
    
    return byte;
}

/*===========================================================================
 * SECTOR OPERATIONS
 *===========================================================================*/

int uft_apd_decode_sectors(const uft_apd_track_t *track, int density,
                            uft_acorn_sector_t *sectors, int max_sectors) {
    if (!track || !sectors || max_sectors <= 0) return 0;
    
    const uint8_t *data;
    uint32_t bits;
    
    switch (density) {
        case UFT_APD_DENSITY_SD:
            data = track->sd_data;
            bits = track->sd_bits;
            break;
        case UFT_APD_DENSITY_DD:
            data = track->dd_data;
            bits = track->dd_bits;
            break;
        case UFT_APD_DENSITY_QD:
            data = track->qd_data;
            bits = track->qd_bits;
            break;
        default:
            return 0;
    }
    
    if (!data || bits == 0) return 0;
    
    int sector_count = 0;
    int search_pos = 0;
    
    while (sector_count < max_sectors) {
        int sync_pos;
        
        if (density == UFT_APD_DENSITY_SD) {
            sync_pos = uft_apd_find_fm_sync(data, bits, search_pos);
        } else {
            sync_pos = uft_apd_find_mfm_sync(data, bits, search_pos);
        }
        
        if (sync_pos < 0) break;
        
        /* Decode sector ID */
        int id_pos = sync_pos + (density == UFT_APD_DENSITY_SD ? 16 : 48);
        
        if (id_pos + 64 > (int)bits) break;
        
        uft_acorn_sector_t *sec = &sectors[sector_count];
        
        if (density == UFT_APD_DENSITY_SD) {
            sec->cylinder = uft_apd_decode_fm_byte(data, id_pos);
            sec->head = uft_apd_decode_fm_byte(data, id_pos + 16);
            sec->sector = uft_apd_decode_fm_byte(data, id_pos + 32);
            sec->size = 128 << uft_apd_decode_fm_byte(data, id_pos + 48);
        } else {
            sec->cylinder = uft_apd_decode_mfm_byte(data, id_pos);
            sec->head = uft_apd_decode_mfm_byte(data, id_pos + 16);
            sec->sector = uft_apd_decode_mfm_byte(data, id_pos + 32);
            sec->size = 128 << uft_apd_decode_mfm_byte(data, id_pos + 48);
        }
        
        /* TODO: Find data mark and record offset */
        sec->data_offset = 0;
        sec->crc_valid = true;  /* TODO: verify CRC */
        
        sector_count++;
        search_pos = sync_pos + 16;
    }
    
    return sector_count;
}

/*===========================================================================
 * CONVERSION
 *===========================================================================*/

int uft_apd_to_adf(uft_apd_t *apd, const char *adf_path) {
    if (!apd || !apd->loaded || !adf_path) return -1;
    
    /* Determine output size based on format */
    size_t adf_size;
    int sectors_per_track;
    int sector_size;
    
    switch (apd->info.format) {
        case UFT_ADFS_FORMAT_E:
        case UFT_ADFS_FORMAT_D:
            adf_size = 800 * 1024;
            sectors_per_track = 5;
            sector_size = 1024;
            break;
        case UFT_ADFS_FORMAT_F:
            adf_size = 1600 * 1024;
            sectors_per_track = 10;
            sector_size = 1024;
            break;
        case UFT_ADFS_FORMAT_L:
            adf_size = 640 * 1024;
            sectors_per_track = 16;
            sector_size = 256;
            break;
        default:
            return -1;
    }
    
    /* Allocate output buffer */
    uint8_t *adf_data = (uint8_t *)calloc(1, adf_size);
    if (!adf_data) return -1;
    
    /* TODO: Decode sectors and build ADF */
    /* For now, just create empty file structure */
    
    /* Write output */
    FILE *f = fopen(adf_path, "wb");
    if (!f) {
        free(adf_data);
        return -1;
    }
    
    fwrite(adf_data, 1, adf_size, f);
    fclose(f);
    free(adf_data);
    
    return 0;
}

int uft_kryoflux_to_apd(const char *kf_dir, const char *apd_path, bool scan_fm) {
    if (!kf_dir || !apd_path) return -1;
    
    /* TODO: Implement KryoFlux to APD conversion */
    /* This would use the kryolib-style parsing */
    
    (void)scan_fm;
    return -1;
}

/*===========================================================================
 * PROTECTION DETECTION
 *===========================================================================*/

uft_acorn_protection_t uft_apd_detect_protection(uft_apd_t *apd) {
    if (!apd || !apd->loaded) return UFT_ACORN_PROT_NONE;
    
    /* Check for tracks beyond 160 */
    for (int t = UFT_APD_USED_TRACKS; t < UFT_APD_NUM_TRACKS; t++) {
        if (apd->tracks[t].sd_bits > 0 || apd->tracks[t].dd_bits > 0 ||
            apd->tracks[t].qd_bits > 0) {
            return UFT_ACORN_PROT_LONG_TRACK;
        }
    }
    
    /* Check for mixed density on same track */
    for (int t = 0; t < UFT_APD_USED_TRACKS; t++) {
        int densities = 0;
        if (apd->tracks[t].sd_bits > 0) densities++;
        if (apd->tracks[t].dd_bits > 0) densities++;
        if (apd->tracks[t].qd_bits > 0) densities++;
        
        if (densities > 1) {
            return UFT_ACORN_PROT_MIXED_DENSITY;
        }
    }
    
    /* Check for QD track (unusual) */
    if (apd->info.has_qd && !apd->info.has_dd) {
        return UFT_ACORN_PROT_QD_TRACK;
    }
    
    return UFT_ACORN_PROT_NONE;
}

const char* uft_acorn_protection_name(uft_acorn_protection_t prot) {
    switch (prot) {
        case UFT_ACORN_PROT_NONE:           return "None";
        case UFT_ACORN_PROT_WEAK_BITS:      return "Weak Bits";
        case UFT_ACORN_PROT_LONG_TRACK:     return "Long Track";
        case UFT_ACORN_PROT_MIXED_DENSITY:  return "Mixed Density";
        case UFT_ACORN_PROT_INVALID_ID:     return "Invalid Sector ID";
        case UFT_ACORN_PROT_CRC_ERROR:      return "Intentional CRC Error";
        case UFT_ACORN_PROT_DUPLICATE:      return "Duplicate Sectors";
        case UFT_ACORN_PROT_SECTOR_IN_SECTOR: return "Sector in Sector";
        case UFT_ACORN_PROT_UNFORMATTED:    return "Unformatted Track";
        case UFT_ACORN_PROT_QD_TRACK:       return "Quad Density Track";
        default:                             return "Unknown";
    }
}

/*===========================================================================
 * UTILITIES
 *===========================================================================*/

const char* uft_adfs_format_name(int format) {
    switch (format) {
        case UFT_ADFS_FORMAT_S: return "ADFS S (100K)";
        case UFT_ADFS_FORMAT_M: return "ADFS M (200K)";
        case UFT_ADFS_FORMAT_L: return "ADFS L (640K)";
        case UFT_ADFS_FORMAT_D: return "ADFS D (800K)";
        case UFT_ADFS_FORMAT_E: return "ADFS E (800K)";
        case UFT_ADFS_FORMAT_F: return "ADFS F (1600K)";
        case UFT_ADFS_FORMAT_G: return "ADFS G (1.6M HD)";
        default:                return "Unknown";
    }
}

void uft_apd_print_info(uft_apd_t *apd) {
    if (!apd) return;
    
    printf("APD Disk Info:\n");
    printf("  Path: %s\n", apd->path ? apd->path : "N/A");
    printf("  Tracks: %d\n", apd->info.num_tracks);
    printf("  Format: %s\n", uft_adfs_format_name(apd->info.format));
    printf("  Has SD: %s\n", apd->info.has_sd ? "Yes" : "No");
    printf("  Has DD: %s\n", apd->info.has_dd ? "Yes" : "No");
    printf("  Has QD: %s\n", apd->info.has_qd ? "Yes" : "No");
    printf("  Size: %zu bytes\n", apd->info.total_size);
    
    uft_acorn_protection_t prot = uft_apd_detect_protection(apd);
    printf("  Protection: %s\n", uft_acorn_protection_name(prot));
}

void uft_apd_print_track(const uft_apd_track_t *track) {
    if (!track) return;
    
    printf("Track %d (Cyl %d, Head %d):\n", 
           track->track_num, track->cylinder, track->head);
    printf("  SD: %u bits (%zu bytes)\n", track->sd_bits, track->sd_size);
    printf("  DD: %u bits (%zu bytes)\n", track->dd_bits, track->dd_size);
    printf("  QD: %u bits (%zu bytes)\n", track->qd_bits, track->qd_size);
}
