/**
 * @file uft_g71.c
 * @brief G71 (Double-sided G64 for 1571) Format Implementation
 * @version 4.1.0
 * 
 * G71 is the double-sided GCR track image format for the 1571 drive.
 * It's essentially a double-sided version of G64, containing raw GCR
 * encoded track data for both sides of a 1571 disk.
 * 
 * Structure:
 * - Header: "GCR-1571" signature
 * - Track data: 84 tracks (42 per side x 2 sides)
 * - Each track contains raw GCR data including sync marks
 * 
 * Reference: VICE emulator, nibtools
 */

#include "uft/formats/uft_g71.h"
/* Note: uft_g71.h includes uft/core/uft_unified_types.h which provides:
 *   - uft_track_t, uft_disk_image_t, uft_sector_t
 *   - Error codes: UFT_OK, UFT_ERR_*, UFT_ERC_*
 *   - Encoding constants: UFT_ENC_GCR_C64, etc.
 *   - uft_track_alloc(), uft_track_free()
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * G71 Format Constants
 * ============================================================================ */

#define G71_SIGNATURE       "GCR-1571"
#define G71_SIGNATURE_LEN   8
#define G71_VERSION         0

#define G71_TRACKS_TOTAL    84          /* 42 tracks x 2 sides */
#define G71_TRACKS_PER_SIDE 42
#define G71_HALF_TRACKS     168         /* Including half tracks */
#define G71_MAX_TRACK_SIZE  7928        /* Maximum GCR track size */

/* Track offset table starts after header */
#define G71_HEADER_SIZE     12
#define G71_OFFSET_TABLE    (G71_HEADER_SIZE)
#define G71_SPEED_TABLE     (G71_OFFSET_TABLE + G71_HALF_TRACKS * 4)
#define G71_TRACK_DATA      (G71_SPEED_TABLE + G71_HALF_TRACKS * 4)

/* Speed zones (same as 1541/G64) */
#define SPEED_ZONE_0        3   /* Tracks 31-42: 17 sectors */
#define SPEED_ZONE_1        2   /* Tracks 25-30: 18 sectors */
#define SPEED_ZONE_2        1   /* Tracks 18-24: 19 sectors */
#define SPEED_ZONE_3        0   /* Tracks 1-17:  21 sectors */

/* ============================================================================
 * Structures
 * ============================================================================ */

#pragma pack(push, 1)
typedef struct {
    uint8_t  signature[8];      /* "GCR-1571" */
    uint8_t  version;           /* Version (0) */
    uint8_t  num_tracks;        /* Number of tracks (84) */
    uint16_t max_track_size;    /* Maximum track size */
} g71_header_t;
#pragma pack(pop)

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

static uint16_t read_le16(const uint8_t *p) {
    return p[0] | (p[1] << 8);
}

static uint32_t read_le32(const uint8_t *p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

static void write_le16(uint8_t *p, uint16_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
}

static void write_le32(uint8_t *p, uint32_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF;
    p[3] = (v >> 24) & 0xFF;
}

static int get_speed_zone(int track) {
    if (track < 17) return SPEED_ZONE_3;      /* 21 sectors */
    if (track < 24) return SPEED_ZONE_2;      /* 19 sectors */
    if (track < 30) return SPEED_ZONE_1;      /* 18 sectors */
    return SPEED_ZONE_0;                       /* 17 sectors */
}

static int get_sectors_for_track(int track) {
    if (track < 17) return 21;
    if (track < 24) return 19;
    if (track < 30) return 18;
    return 17;
}

/* ============================================================================
 * Probe Function
 * ============================================================================ */

bool uft_g71_probe(const uint8_t *data, size_t size, int *confidence) {
    if (!data || size < G71_HEADER_SIZE) return false;
    
    if (memcmp(data, G71_SIGNATURE, G71_SIGNATURE_LEN) == 0) {
        if (confidence) *confidence = 95;
        return true;
    }
    
    /* Also check for "GCR-1541" followed by 84 tracks indication */
    if (memcmp(data, "GCR-1541", 8) == 0 && data[9] >= 84) {
        if (confidence) *confidence = 70;
        return true;
    }
    
    return false;
}

/* ============================================================================
 * Read Functions
 * ============================================================================ */

int uft_g71_read(const char *path, uft_disk_image_t **out) {
    if (!path || !out) return UFT_ERR_INVALID_ARG;
    
    FILE *f = fopen(path, "rb");
    if (!f) return UFT_ERR_IO;
    
    /* Read header */
    g71_header_t header;
    if (fread(&header, sizeof(header), 1, f) != 1) {
        fclose(f);
        return UFT_ERR_IO;
    }
    
    /* Verify signature */
    if (memcmp(header.signature, G71_SIGNATURE, G71_SIGNATURE_LEN) != 0 &&
        memcmp(header.signature, "GCR-1541", 8) != 0) {
        fclose(f);
        return UFT_ERC_FORMAT;
    }
    
    int num_tracks = header.num_tracks;
    if (num_tracks < 42) num_tracks = 84;  /* Default to double-sided */
    
    /* Allocate disk image */
    uft_disk_image_t *disk = calloc(1, sizeof(uft_disk_image_t));
    if (!disk) {
        fclose(f);
        return UFT_ERR_MEMORY;
    }
    
    disk->tracks = G71_TRACKS_PER_SIDE;
    disk->heads = 2;
    disk->track_count = num_tracks;
    /* Note: GCR encoding is per-track, not per-disk */
    disk->bytes_per_sector = 256;
    
    /* Allocate track array */
    disk->track_data = calloc(num_tracks * 2, sizeof(void*));  /* Include half tracks */
    if (!disk->track_data) {
        free(disk);
        fclose(f);
        return UFT_ERR_MEMORY;
    }
    
    /* Read track offset table */
    uint32_t *offsets = malloc(G71_HALF_TRACKS * sizeof(uint32_t));
    uint32_t *speeds = malloc(G71_HALF_TRACKS * sizeof(uint32_t));
    if (!offsets || !speeds) {
        free(offsets);
        free(speeds);
        free(disk->track_data);
        free(disk);
        fclose(f);
        return UFT_ERR_MEMORY;
    }
    
    fseek(f, G71_OFFSET_TABLE, SEEK_SET);
    for (int i = 0; i < G71_HALF_TRACKS; i++) {
        uint8_t buf[4];
        fread(buf, 4, 1, f);
        offsets[i] = read_le32(buf);
    }
    
    for (int i = 0; i < G71_HALF_TRACKS; i++) {
        uint8_t buf[4];
        fread(buf, 4, 1, f);
        speeds[i] = read_le32(buf);
    }
    
    /* Read track data */
    for (int side = 0; side < 2; side++) {
        for (int t = 0; t < G71_TRACKS_PER_SIDE; t++) {
            int half_track = (side * G71_TRACKS_PER_SIDE + t) * 2;
            if (half_track >= G71_HALF_TRACKS) continue;
            
            uint32_t offset = offsets[half_track];
            if (offset == 0) continue;
            
            /* Seek to track data */
            fseek(f, offset, SEEK_SET);
            
            /* Read track size */
            uint8_t size_buf[2];
            fread(size_buf, 2, 1, f);
            uint16_t track_size = read_le16(size_buf);
            
            if (track_size == 0 || track_size > G71_MAX_TRACK_SIZE) continue;
            
            /* Allocate track using unified API */
            int num_sectors = get_sectors_for_track(t);
            uft_track_t *track = uft_track_alloc(num_sectors, (size_t)track_size * 8);
            if (!track) continue;
            
            /* Set track metadata using unified_types member names */
            track->track_num = (uint16_t)t;
            track->head = (uint8_t)side;
            track->encoding = UFT_ENC_GCR_C64;
            track->raw_bits = (size_t)track_size * 8;  /* Convert bytes to bits */
            
            /* Allocate and read raw GCR data */
            if (!track->raw_data) {
                track->raw_data = malloc(track_size);
                track->raw_capacity = track_size;
                track->owns_data = true;
            }
            if (track->raw_data) {
                fread(track->raw_data, 1, track_size, f);
            }
            
            int idx = side * G71_TRACKS_PER_SIDE + t;
            disk->track_data[idx] = track;
        }
    }
    
    free(offsets);
    free(speeds);
    fclose(f);
    *out = disk;
    return UFT_OK;
}

/* ============================================================================
 * Write Functions
 * ============================================================================ */

int uft_g71_write(const char *path, const uft_disk_image_t *disk) {
    if (!path || !disk) return UFT_ERR_INVALID_ARG;
    
    FILE *f = fopen(path, "wb");
    if (!f) return UFT_ERR_IO;
    
    /* Write header */
    g71_header_t header = {0};
    memcpy(header.signature, G71_SIGNATURE, G71_SIGNATURE_LEN);
    header.version = G71_VERSION;
    header.num_tracks = G71_TRACKS_TOTAL;
    header.max_track_size = G71_MAX_TRACK_SIZE;
    fwrite(&header, sizeof(header), 1, f);
    
    /* Calculate track offsets */
    uint32_t offsets[G71_HALF_TRACKS] = {0};
    uint32_t speeds[G71_HALF_TRACKS] = {0};
    
    uint32_t current_offset = G71_TRACK_DATA;
    
    for (int side = 0; side < 2; side++) {
        for (int t = 0; t < G71_TRACKS_PER_SIDE; t++) {
            int half_track = (side * G71_TRACKS_PER_SIDE + t) * 2;
            int idx = side * G71_TRACKS_PER_SIDE + t;
            
            if (idx < (int)disk->track_count && disk->track_data[idx]) {
                uft_track_t *track = (uft_track_t*)disk->track_data[idx];
                size_t raw_bytes = (track->raw_bits + 7) / 8;  /* Convert bits to bytes */
                if (track->raw_data && raw_bytes > 0) {
                    offsets[half_track] = current_offset;
                    speeds[half_track] = get_speed_zone(t);
                    current_offset += 2 + (uint32_t)raw_bytes;  /* Size word + data */
                }
            }
        }
    }
    
    /* Write offset table */
    for (int i = 0; i < G71_HALF_TRACKS; i++) {
        uint8_t buf[4];
        write_le32(buf, offsets[i]);
        fwrite(buf, 4, 1, f);
    }
    
    /* Write speed table */
    for (int i = 0; i < G71_HALF_TRACKS; i++) {
        uint8_t buf[4];
        write_le32(buf, speeds[i]);
        fwrite(buf, 4, 1, f);
    }
    
    /* Write track data */
    for (int side = 0; side < 2; side++) {
        for (int t = 0; t < G71_TRACKS_PER_SIDE; t++) {
            int idx = side * G71_TRACKS_PER_SIDE + t;
            
            if (idx < (int)disk->track_count && disk->track_data[idx]) {
                uft_track_t *track = (uft_track_t*)disk->track_data[idx];
                size_t raw_bytes = (track->raw_bits + 7) / 8;  /* Convert bits to bytes */
                if (track->raw_data && raw_bytes > 0) {
                    uint8_t size_buf[2];
                    write_le16(size_buf, (uint16_t)raw_bytes);
                    fwrite(size_buf, 2, 1, f);
                    fwrite(track->raw_data, 1, raw_bytes, f);
                }
            }
        }
    }
    
    fclose(f);
    return UFT_OK;
}

/* ============================================================================
 * Info/Conversion Functions
 * ============================================================================ */

int uft_g71_get_info(const char *path, char *buf, size_t buf_size) {
    if (!path || !buf) return UFT_ERR_INVALID_ARG;
    
    FILE *f = fopen(path, "rb");
    if (!f) return UFT_ERR_IO;
    
    g71_header_t header;
    fread(&header, sizeof(header), 1, f);
    
    fseek(f, 0, SEEK_END);
    size_t file_size = ftell(f);
    fclose(f);
    
    int track_count = 0;
    /* Count tracks would require reading offset table... simplified for now */
    track_count = header.num_tracks;
    
    snprintf(buf, buf_size,
        "Format: G71 (1571 GCR Track Image)\n"
        "Signature: %.8s\n"
        "Version: %d\n"
        "Tracks: %d (%d per side x 2 sides)\n"
        "Max Track Size: %d bytes\n"
        "File Size: %zu bytes\n"
        "Encoding: GCR (Group Code Recording)\n",
        header.signature, header.version,
        track_count, track_count / 2,
        read_le16((uint8_t*)&header.max_track_size),
        file_size);
    
    return UFT_OK;
}

int uft_g71_to_d71(const uft_disk_image_t *g71, uft_disk_image_t **d71_out) {
    if (!g71 || !d71_out) return UFT_ERR_INVALID_ARG;
    
    /* Create D71 image */
    uft_disk_image_t *d71 = calloc(1, sizeof(uft_disk_image_t));
    if (!d71) return UFT_ERR_MEMORY;
    
    d71->tracks = 35;
    d71->heads = 2;
    d71->bytes_per_sector = 256;
    /* Note: GCR encoding is per-track */
    d71->track_count = 70;
    
    d71->track_data = calloc(70, sizeof(void*));
    if (!d71->track_data) {
        free(d71);
        return UFT_ERR_MEMORY;
    }
    
    /* Convert GCR tracks to sectors - simplified, real implementation needs GCR decoder */
    /* This is a placeholder - full GCR decode would be needed */
    
    *d71_out = d71;
    return UFT_OK;
}

/* ============================================================================
 * Compatibility wrapper for uft_smart_open.c
 * ============================================================================ */

/**
 * @brief Wrapper for legacy g71_probe signature expected by uft_smart_open
 */
bool g71_probe(const uint8_t *data, size_t size, size_t file_size, int *confidence) {
    (void)file_size;  /* Not used in our implementation */
    return uft_g71_probe(data, size, confidence);
}
