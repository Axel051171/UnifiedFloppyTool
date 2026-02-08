/**
 * @file woz_writer.c
 * @brief WOZ Image Writer for Apple II
 * 
 * WOZ is the Applesauce flux-level format for Apple II disks.
 * Supports WOZ 1.0 and WOZ 2.0 formats.
 * 
 * @version 5.29.0
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* WOZ Constants */
#define WOZ1_SIGNATURE  0x315A4F57  /* "WOZ1" */
#define WOZ2_SIGNATURE  0x325A4F57  /* "WOZ2" */
#define WOZ_MAGIC       0x0A0D0AFF

#define CHUNK_INFO      0x4F464E49  /* "INFO" */
#define CHUNK_TMAP      0x50414D54  /* "TMAP" */
#define CHUNK_TRKS      0x534B5254  /* "TRKS" */
#define CHUNK_META      0x4154454D  /* "META" */

/* Apple II 6-and-2 GCR encoding */
static const uint8_t WRITE_TABLE[64] = {
    0x96, 0x97, 0x9A, 0x9B, 0x9D, 0x9E, 0x9F, 0xA6,
    0xA7, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB2, 0xB3,
    0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC,
    0xBD, 0xBE, 0xBF, 0xCB, 0xCD, 0xCE, 0xCF, 0xD3,
    0xD6, 0xD7, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE,
    0xDF, 0xE5, 0xE6, 0xE7, 0xE9, 0xEA, 0xEB, 0xEC,
    0xED, 0xEE, 0xEF, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6,
    0xF7, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};

#pragma pack(push, 1)
typedef struct {
    uint32_t signature;
    uint32_t magic;
} woz_file_header_t;

typedef struct {
    uint32_t chunk_id;
    uint32_t chunk_size;
} woz_chunk_header_t;

typedef struct {
    uint8_t  version;
    uint8_t  disk_type;      /* 1=5.25", 2=3.5" */
    uint8_t  write_protected;
    uint8_t  synchronized;
    uint8_t  cleaned;
    char     creator[32];
    uint8_t  disk_sides;
    uint8_t  boot_sector_format;
    uint8_t  optimal_bit_timing;
    uint16_t compatible_hardware;
    uint16_t required_ram;
    uint16_t largest_track;
    /* WOZ2 additional fields */
    uint16_t flux_block;
    uint16_t largest_flux_track;
} woz_info_chunk_t;

typedef struct {
    uint16_t starting_block;
    uint16_t block_count;
    uint32_t bit_count;
} woz2_trk_entry_t;
#pragma pack(pop)

typedef struct {
    uint8_t *data;
    uint32_t bit_count;
    uint16_t byte_count;
} woz_track_t;

typedef struct {
    woz_track_t tracks[40];
    uint8_t tmap[160];
    woz_info_chunk_t info;
    bool is_woz2;
} woz_image_t;

/* ============================================================================
 * 6-and-2 GCR Encoding
 * ============================================================================ */

/**
 * @brief Encode byte to 6-and-2 GCR
 */
static uint8_t gcr_encode_62(uint8_t val)
{
    return WRITE_TABLE[val & 0x3F];
}

/**
 * @brief Encode 256-byte sector to GCR nibbles
 */
static size_t encode_sector_62(int track, int sector, const uint8_t *data,
                               uint8_t *nibbles)
{
    size_t pos = 0;
    int volume = 254;
    
    /* Address prologue: D5 AA 96 */
    nibbles[pos++] = 0xD5;
    nibbles[pos++] = 0xAA;
    nibbles[pos++] = 0x96;
    
    /* Address field: volume, track, sector, checksum */
    uint8_t checksum = volume ^ track ^ sector;
    nibbles[pos++] = gcr_encode_62((volume >> 1) | 0xAA);
    nibbles[pos++] = gcr_encode_62(volume | 0xAA);
    nibbles[pos++] = gcr_encode_62((track >> 1) | 0xAA);
    nibbles[pos++] = gcr_encode_62(track | 0xAA);
    nibbles[pos++] = gcr_encode_62((sector >> 1) | 0xAA);
    nibbles[pos++] = gcr_encode_62(sector | 0xAA);
    nibbles[pos++] = gcr_encode_62((checksum >> 1) | 0xAA);
    nibbles[pos++] = gcr_encode_62(checksum | 0xAA);
    
    /* Address epilogue: DE AA EB */
    nibbles[pos++] = 0xDE;
    nibbles[pos++] = 0xAA;
    nibbles[pos++] = 0xEB;
    
    /* Gap */
    for (int i = 0; i < 6; i++) nibbles[pos++] = 0xFF;
    
    /* Data prologue: D5 AA AD */
    nibbles[pos++] = 0xD5;
    nibbles[pos++] = 0xAA;
    nibbles[pos++] = 0xAD;
    
    /* Pre-nibblize: create auxiliary buffer */
    uint8_t aux[86], primary[256];
    memset(aux, 0, 86);
    
    for (int i = 0; i < 256; i++) {
        primary[i] = data[i] >> 2;
        int aux_idx = i % 86;
        int bit_pair = i / 86;
        aux[aux_idx] |= ((data[i] & 0x01) << (bit_pair * 2 + 1)) |
                        ((data[i] & 0x02) >> 1 << (bit_pair * 2));
    }
    
    /* Encode auxiliary bytes */
    uint8_t prev = 0;
    for (int i = 85; i >= 0; i--) {
        nibbles[pos++] = gcr_encode_62(aux[i] ^ prev);
        prev = aux[i];
    }
    
    /* Encode primary bytes */
    for (int i = 0; i < 256; i++) {
        nibbles[pos++] = gcr_encode_62(primary[i] ^ prev);
        prev = primary[i];
    }
    
    /* Checksum */
    nibbles[pos++] = gcr_encode_62(prev);
    
    /* Data epilogue: DE AA EB */
    nibbles[pos++] = 0xDE;
    nibbles[pos++] = 0xAA;
    nibbles[pos++] = 0xEB;
    
    /* Inter-sector gap */
    for (int i = 0; i < 27; i++) nibbles[pos++] = 0xFF;
    
    return pos;
}

/* ============================================================================
 * WOZ Writer
 * ============================================================================ */

/**
 * @brief Initialize WOZ image
 */
int woz_create(woz_image_t *img, bool woz2)
{
    if (!img) return -1;
    
    memset(img, 0, sizeof(*img));
    img->is_woz2 = woz2;
    
    /* Initialize info chunk */
    img->info.version = woz2 ? 2 : 1;
    img->info.disk_type = 1;  /* 5.25" */
    img->info.disk_sides = 1;
    img->info.optimal_bit_timing = 32;  /* 4µs */
    strncpy(img->info.creator, "UnifiedFloppyTool", 32);
    
    /* Initialize TMAP (all tracks unmapped) */
    memset(img->tmap, 0xFF, 160);
    
    return 0;
}

/**
 * @brief Encode track from sector data
 */
int woz_encode_track(woz_image_t *img, int track, const uint8_t *sectors,
                     int sector_count)
{
    if (!img || !sectors || track < 0 || track >= 40) return -1;
    
    /* Allocate track buffer */
    size_t max_bytes = 6656;  /* ~53,248 bits */
    img->tracks[track].data = calloc(1, max_bytes);
    if (!img->tracks[track].data) return -1;
    
    /* Sync bytes at start */
    size_t pos = 0;
    for (int i = 0; i < 64; i++) {
        img->tracks[track].data[pos++] = 0xFF;
    }
    
    /* Encode each sector */
    for (int s = 0; s < sector_count; s++) {
        pos += encode_sector_62(track, s, sectors + s * 256,
                                img->tracks[track].data + pos);
    }
    
    /* Fill rest with sync */
    while (pos < max_bytes) {
        img->tracks[track].data[pos++] = 0xFF;
    }
    
    img->tracks[track].byte_count = pos;
    img->tracks[track].bit_count = pos * 8;
    
    /* Update TMAP */
    img->tmap[track * 4] = track;
    img->tmap[track * 4 + 1] = track;
    img->tmap[track * 4 + 2] = (track < 39) ? track : 0xFF;
    img->tmap[track * 4 + 3] = 0xFF;
    
    /* Update largest track */
    if (pos * 512 > img->info.largest_track) {
        img->info.largest_track = pos / 512 + 1;
    }
    
    return 0;
}

/**
 * @brief Calculate CRC32 for WOZ
 */
static uint32_t woz_crc32(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFF;
    static uint32_t table[256] = {0};
    
    if (table[1] == 0) {
        for (int i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int j = 0; j < 8; j++) {
                c = (c >> 1) ^ ((c & 1) ? 0xEDB88320 : 0);
            }
            table[i] = c;
        }
    }
    
    for (size_t i = 0; i < len; i++) {
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    
    return ~crc;
}

/**
 * @brief Save WOZ image to file
 */
int woz_save(const woz_image_t *img, const char *filename)
{
    if (!img || !filename) return -1;
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) return -1;
    
    /* File header */
    woz_file_header_t header = {
        .signature = img->is_woz2 ? WOZ2_SIGNATURE : WOZ1_SIGNATURE,
        .magic = WOZ_MAGIC
    };
    if (fwrite(&header, sizeof(header), 1, fp) != 1) { /* I/O error */ }
    /* Placeholder for CRC */
    uint32_t crc_placeholder = 0;
    long crc_pos = ftell(fp);
    if (fwrite(&crc_placeholder, 4, 1, fp) != 1) { /* I/O error */ }
    /* INFO chunk */
    woz_chunk_header_t chunk = { CHUNK_INFO, 60 };
    if (fwrite(&chunk, sizeof(chunk), 1, fp) != 1) { /* I/O error */ }
    if (fwrite(&img->info, 60, 1, fp) != 1) { /* I/O error */ }
    /* TMAP chunk */
    chunk.chunk_id = CHUNK_TMAP;
    chunk.chunk_size = 160;
    if (fwrite(&chunk, sizeof(chunk), 1, fp) != 1) { /* I/O error */ }
    if (fwrite(img->tmap, 160, 1, fp) != 1) { /* I/O error */ }
    /* TRKS chunk */
    chunk.chunk_id = CHUNK_TRKS;
    
    if (img->is_woz2) {
        /* WOZ2: TRK entries + bit data */
        size_t total_bits = 0;
        for (int t = 0; t < 40; t++) {
            if (img->tracks[t].data) {
                total_bits += ((img->tracks[t].bit_count + 511) / 512) * 512;
            }
        }
        chunk.chunk_size = 160 * 8 + total_bits / 8;
        if (fwrite(&chunk, sizeof(chunk), 1, fp) != 1) { /* I/O error */ }
        /* TRK entries */
        uint16_t block = 3;  /* First data block */
        for (int t = 0; t < 160; t++) {
            woz2_trk_entry_t entry = {0};
            int real_track = t / 4;
            if (img->tracks[real_track].data && (t % 4 == 0)) {
                entry.starting_block = block;
                entry.block_count = (img->tracks[real_track].byte_count + 511) / 512;
                entry.bit_count = img->tracks[real_track].bit_count;
                block += entry.block_count;
            }
            if (fwrite(&entry, sizeof(entry), 1, fp) != 1) { /* I/O error */ }
        }
        
        /* Track data (512-byte aligned) */
        for (int t = 0; t < 40; t++) {
            if (img->tracks[t].data) {
                size_t aligned = ((img->tracks[t].byte_count + 511) / 512) * 512;
                if (fwrite(img->tracks[t].data, 1, img->tracks[t].byte_count, fp) != img->tracks[t].byte_count) { /* I/O error */ }
                /* Padding */
                uint8_t pad[512] = {0};
                size_t padding = aligned - img->tracks[t].byte_count;
                if (padding > 0) fwrite(pad, 1, padding, fp);
            }
        }
    } else {
        /* WOZ1: simpler format */
        chunk.chunk_size = 40 * (2 + 2 + 6646);
        if (fwrite(&chunk, sizeof(chunk), 1, fp) != 1) { /* I/O error */ }
        for (int t = 0; t < 40; t++) {
            uint16_t bytes_used = img->tracks[t].byte_count;
            uint16_t bit_count_lo = img->tracks[t].bit_count & 0xFFFF;
            if (fwrite(&bytes_used, 2, 1, fp) != 1) { /* I/O error */ }
            if (fwrite(&bit_count_lo, 2, 1, fp) != 1) { /* I/O error */ }
            uint8_t track_data[6646] = {0};
            if (img->tracks[t].data) {
                memcpy(track_data, img->tracks[t].data, 
                       bytes_used < 6646 ? bytes_used : 6646);
            }
            if (fwrite(track_data, 1, 6646, fp) != 6646) { /* I/O error */ }
        }
    }
    
    /* Calculate and write CRC */
    if (fseek(fp, 0, SEEK_END) != 0) { /* seek error */ }
    size_t file_size = ftell(fp);
    if (fseek(fp, 12, SEEK_SET) != 0) { /* seek error */ }
    uint8_t *buffer = malloc(file_size - 12);
    if (fread(buffer, 1, file_size - 12, fp) != file_size - 12) { /* I/O error */ }
    uint32_t crc = woz_crc32(buffer, file_size - 12);
    free(buffer);
    
    if (fseek(fp, crc_pos, SEEK_SET) != 0) { /* seek error */ }
    if (fwrite(&crc, 4, 1, fp) != 1) { /* I/O error */ }
    if (ferror(fp)) {
        fclose(fp);
        return UFT_ERR_IO;
    }
        fclose(fp);
        return 0;
}

/**
 * @brief Convert DO/DSK to WOZ
 */
int woz_from_do(const char *do_file, const char *woz_file, bool woz2)
{
    FILE *fp = fopen(do_file, "rb");
    if (!fp) return -1;
    
    uint8_t data[143360];
    if (fread(data, 1, 143360, fp) != 143360) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    
    woz_image_t img;
    woz_create(&img, woz2);
    
    /* Encode all 35 tracks */
    for (int t = 0; t < 35; t++) {
        woz_encode_track(&img, t, data + t * 16 * 256, 16);
    }
    
    int result = woz_save(&img, woz_file);
    
    /* Cleanup */
    for (int t = 0; t < 40; t++) {
        free(img.tracks[t].data);
    }
    
    return result;
}

/**
 * @brief Free WOZ image
 */
void woz_free(woz_image_t *img)
{
    if (img) {
        for (int t = 0; t < 40; t++) {
            free(img->tracks[t].data);
        }
        memset(img, 0, sizeof(*img));
    }
}
