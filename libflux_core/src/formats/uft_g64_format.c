/*
 * uft_g64_format.c
 *
 * Complete G64 (GCR-encoded 1541 disk image) format handler implementation.
 *
 * Build:
 *   cc -std=c11 -O2 -Wall -Wextra -pedantic -c uft_g64_format.c
 */

#include "formats/uft_g64_format.h"
#include <string.h>
#include <stdlib.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * INTERNAL HELPERS
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Read little-endian 16-bit value */
static inline uint16_t read_le16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

/* Read little-endian 32-bit value */
static inline uint32_t read_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Write little-endian 16-bit value */
static inline void write_le16(uint8_t *p, uint16_t val)
{
    p[0] = (uint8_t)val;
    p[1] = (uint8_t)(val >> 8);
}

/* Write little-endian 32-bit value */
static inline void write_le32(uint8_t *p, uint32_t val)
{
    p[0] = (uint8_t)val;
    p[1] = (uint8_t)(val >> 8);
    p[2] = (uint8_t)(val >> 16);
    p[3] = (uint8_t)(val >> 24);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * SPEED ZONE TABLES
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * Speed zone for each track (1-42).
 * Zone 3 = fastest (tracks 1-17)
 * Zone 0 = slowest (tracks 31-42)
 */
static const uint8_t track_speed_zones[43] = {
    0,  /* Track 0 (unused) */
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3,  /* Tracks 1-10: Zone 3 */
    3, 3, 3, 3, 3, 3, 3,          /* Tracks 11-17: Zone 3 */
    2, 2, 2, 2, 2, 2, 2,          /* Tracks 18-24: Zone 2 */
    1, 1, 1, 1, 1, 1,             /* Tracks 25-30: Zone 1 */
    0, 0, 0, 0, 0,                /* Tracks 31-35: Zone 0 */
    0, 0, 0, 0, 0, 0, 0           /* Tracks 36-42: Zone 0 */
};

/* Expected track size for each zone */
static const uint16_t zone_track_sizes[4] = {
    6250,   /* Zone 0: 17 sectors */
    6667,   /* Zone 1: 18 sectors */
    7143,   /* Zone 2: 19 sectors */
    7692    /* Zone 3: 21 sectors */
};

/* Bit rate for each zone */
static const uint32_t zone_bitrates[4] = {
    250000,  /* Zone 0: 4.00µs bit cell */
    266667,  /* Zone 1: 3.75µs bit cell */
    285714,  /* Zone 2: 3.50µs bit cell */
    307692   /* Zone 3: 3.25µs bit cell */
};

/* ═══════════════════════════════════════════════════════════════════════════
 * DETECTION
 * ═══════════════════════════════════════════════════════════════════════════ */

bool uft_g64_detect(const uint8_t *data, size_t size,
                    uft_g64_detect_t *result)
{
    if (result) {
        memset(result, 0, sizeof(*result));
    }
    
    /* Minimum size: header + track tables */
    if (!data || size < 12) {
        return false;
    }
    
    /* Check signature */
    if (memcmp(data, UFT_G64_SIGNATURE, UFT_G64_SIGNATURE_LEN) != 0) {
        return false;
    }
    
    uint8_t version = data[UFT_G64_OFF_VERSION];
    uint8_t num_tracks = data[UFT_G64_OFF_NUM_TRACKS];
    uint16_t max_size = read_le16(data + UFT_G64_OFF_MAX_SIZE);
    
    /* Validate */
    if (version > 1) {
        return false;
    }
    
    if (num_tracks == 0 || num_tracks > UFT_G64_MAX_TRACKS) {
        return false;
    }
    
    /* Minimum file size: header + offset table + speed table */
    size_t min_size = 12 + (num_tracks * 4) + (num_tracks * 4);
    if (size < min_size) {
        return false;
    }
    
    if (result) {
        result->is_valid = true;
        result->version = version;
        result->num_tracks = num_tracks;
        result->max_track_size = max_size;
        result->file_size = (uint32_t)size;
        result->has_half_tracks = (num_tracks > 42);
        
        /* Count speed zones */
        size_t speed_table_offset = 12 + (num_tracks * 4);
        for (int i = 0; i < num_tracks; i++) {
            uint32_t speed = read_le32(data + speed_table_offset + i * 4);
            if (speed < 4) {
                result->speed_zone_count[speed]++;
            }
        }
    }
    
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * FILE OPERATIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_g64_parse(const uint8_t *data, size_t size,
                  uft_g64_image_t *image)
{
    uft_g64_detect_t detect;
    
    if (!uft_g64_detect(data, size, &detect)) {
        return UFT_G64_E_INVALID;
    }
    
    memset(image, 0, sizeof(*image));
    
    /* Parse header */
    memcpy(image->header.signature, data, 8);
    image->header.version = data[UFT_G64_OFF_VERSION];
    image->header.num_tracks = data[UFT_G64_OFF_NUM_TRACKS];
    image->header.max_track_size = read_le16(data + UFT_G64_OFF_MAX_SIZE);
    
    /* Parse track offset table */
    size_t offset_table = UFT_G64_OFF_TRACK_TABLE;
    size_t speed_table = offset_table + (image->header.num_tracks * 4);
    
    for (int i = 0; i < image->header.num_tracks; i++) {
        uint32_t track_offset = read_le32(data + offset_table + i * 4);
        uint32_t track_speed = read_le32(data + speed_table + i * 4);
        
        image->tracks[i].half_track = i + 1;
        image->tracks[i].offset = track_offset;
        image->tracks[i].speed_zone = (track_speed < 4) ? track_speed : 
                                      uft_g64_track_speed_zone((i / 2) + 1);
        
        if (track_offset > 0 && track_offset + 2 <= size) {
            image->tracks[i].present = true;
            image->tracks[i].length = read_le16(data + track_offset);
            
            /* Validate track length */
            if (track_offset + 2 + image->tracks[i].length > size) {
                image->tracks[i].length = 0;
                image->tracks[i].present = false;
            }
        }
    }
    
    /* Store raw data reference */
    image->data = (uint8_t *)data;
    image->data_size = size;
    
    return UFT_G64_OK;
}

int uft_g64_open(const char *filename, uft_g64_image_t *image)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        return UFT_G64_E_FILE;
    }
    
    /* Get file size */
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    if (file_size <= 0 || file_size > 10 * 1024 * 1024) {  /* Max 10MB */
        fclose(fp);
        return UFT_G64_E_INVALID;
    }
    
    /* Allocate and read */
    uint8_t *data = (uint8_t *)malloc((size_t)file_size);
    if (!data) {
        fclose(fp);
        return UFT_G64_E_MEMORY;
    }
    
    if (fread(data, 1, (size_t)file_size, fp) != (size_t)file_size) {
        free(data);
        fclose(fp);
        return UFT_G64_E_FILE;
    }
    
    fclose(fp);
    
    /* Parse */
    int result = uft_g64_parse(data, (size_t)file_size, image);
    if (result != UFT_G64_OK) {
        free(data);
        return result;
    }
    
    /* Store filename */
    strncpy(image->filename, filename, sizeof(image->filename) - 1);
    
    return UFT_G64_OK;
}

int uft_g64_save(const char *filename, const uft_g64_image_t *image)
{
    uint8_t *buffer;
    size_t buffer_size = 1024 * 1024;  /* 1MB initial */
    size_t written;
    
    buffer = (uint8_t *)malloc(buffer_size);
    if (!buffer) {
        return UFT_G64_E_MEMORY;
    }
    
    int result = uft_g64_write(image, buffer, buffer_size, &written);
    if (result != UFT_G64_OK) {
        free(buffer);
        return result;
    }
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        free(buffer);
        return UFT_G64_E_FILE;
    }
    
    if (fwrite(buffer, 1, written, fp) != written) {
        fclose(fp);
        free(buffer);
        return UFT_G64_E_FILE;
    }
    
    fclose(fp);
    free(buffer);
    return UFT_G64_OK;
}

int uft_g64_write(const uft_g64_image_t *image,
                  uint8_t *buffer, size_t size,
                  size_t *written)
{
    size_t pos = 0;
    int num_tracks = image->header.num_tracks;
    
    /* Calculate required size */
    size_t header_size = 12 + (num_tracks * 4) + (num_tracks * 4);
    size_t data_size = 0;
    
    for (int i = 0; i < num_tracks; i++) {
        if (image->tracks[i].present && image->tracks[i].length > 0) {
            data_size += 2 + image->tracks[i].length;
        }
    }
    
    if (size < header_size + data_size) {
        return UFT_G64_E_BUFFER;
    }
    
    /* Write signature */
    memcpy(buffer + pos, UFT_G64_SIGNATURE, 8);
    pos += 8;
    
    /* Write version */
    buffer[pos++] = image->header.version;
    
    /* Write num tracks */
    buffer[pos++] = (uint8_t)num_tracks;
    
    /* Write max track size */
    write_le16(buffer + pos, image->header.max_track_size);
    pos += 2;
    
    /* Calculate track data start */
    size_t data_start = header_size;
    size_t current_data_pos = data_start;
    
    /* Write track offset table */
    for (int i = 0; i < num_tracks; i++) {
        if (image->tracks[i].present && image->tracks[i].length > 0) {
            write_le32(buffer + pos, (uint32_t)current_data_pos);
            current_data_pos += 2 + image->tracks[i].length;
        } else {
            write_le32(buffer + pos, 0);
        }
        pos += 4;
    }
    
    /* Write speed zone table */
    for (int i = 0; i < num_tracks; i++) {
        write_le32(buffer + pos, image->tracks[i].speed_zone);
        pos += 4;
    }
    
    /* Write track data */
    for (int i = 0; i < num_tracks; i++) {
        if (image->tracks[i].present && image->tracks[i].length > 0) {
            /* Write track length */
            write_le16(buffer + pos, image->tracks[i].length);
            pos += 2;
            
            /* Write track data */
            if (image->tracks[i].data) {
                memcpy(buffer + pos, image->tracks[i].data, 
                       image->tracks[i].length);
            } else if (image->data && image->tracks[i].offset > 0) {
                memcpy(buffer + pos, 
                       image->data + image->tracks[i].offset + 2,
                       image->tracks[i].length);
            } else {
                memset(buffer + pos, 0x55, image->tracks[i].length);
            }
            pos += image->tracks[i].length;
        }
    }
    
    if (written) {
        *written = pos;
    }
    
    return UFT_G64_OK;
}

void uft_g64_close(uft_g64_image_t *image)
{
    if (!image) return;
    
    /* Free allocated track data */
    for (int i = 0; i < UFT_G64_MAX_TRACKS; i++) {
        if (image->tracks[i].data) {
            free(image->tracks[i].data);
            image->tracks[i].data = NULL;
        }
    }
    
    /* Don't free image->data if it was passed from outside */
    /* Only free if we allocated it in uft_g64_open */
    if (image->filename[0] != '\0' && image->data) {
        free(image->data);
        image->data = NULL;
    }
    
    memset(image, 0, sizeof(*image));
}

int uft_g64_create(uft_g64_image_t *image, int num_tracks)
{
    if (num_tracks != 42 && num_tracks != 84 && num_tracks != 168) {
        return UFT_G64_E_TRACK;
    }
    
    memset(image, 0, sizeof(*image));
    
    memcpy(image->header.signature, UFT_G64_SIGNATURE, 8);
    image->header.version = UFT_G64_VERSION_ORIG;
    image->header.num_tracks = (num_tracks == 42) ? 84 : num_tracks;
    image->header.max_track_size = UFT_G64_TRACK_SIZE_MAX;
    
    /* Initialize track entries */
    for (int i = 0; i < image->header.num_tracks; i++) {
        image->tracks[i].half_track = i + 1;
        image->tracks[i].speed_zone = uft_g64_track_speed_zone((i / 2) + 1);
        image->tracks[i].present = false;
        image->tracks[i].length = 0;
    }
    
    return UFT_G64_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TRACK OPERATIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_g64_read_track(const uft_g64_image_t *image,
                       int half_track,
                       uint8_t *buffer, size_t size,
                       size_t *length_out)
{
    if (half_track < 1 || half_track > image->header.num_tracks) {
        return UFT_G64_E_TRACK;
    }
    
    const uft_g64_track_t *track = &image->tracks[half_track - 1];
    
    if (!track->present || track->length == 0) {
        return UFT_G64_E_NO_DATA;
    }
    
    if (size < track->length) {
        return UFT_G64_E_BUFFER;
    }
    
    if (track->data) {
        memcpy(buffer, track->data, track->length);
    } else if (image->data && track->offset > 0) {
        memcpy(buffer, image->data + track->offset + 2, track->length);
    } else {
        return UFT_G64_E_NO_DATA;
    }
    
    if (length_out) {
        *length_out = track->length;
    }
    
    return UFT_G64_OK;
}

int uft_g64_write_track(uft_g64_image_t *image,
                        int half_track,
                        const uint8_t *data, size_t length,
                        uint8_t speed_zone)
{
    if (half_track < 1 || half_track > image->header.num_tracks) {
        return UFT_G64_E_TRACK;
    }
    
    if (length > image->header.max_track_size) {
        return UFT_G64_E_BUFFER;
    }
    
    uft_g64_track_t *track = &image->tracks[half_track - 1];
    
    /* Free existing data */
    if (track->data) {
        free(track->data);
    }
    
    /* Allocate new data */
    track->data = (uint8_t *)malloc(length);
    if (!track->data) {
        return UFT_G64_E_MEMORY;
    }
    
    memcpy(track->data, data, length);
    track->length = (uint16_t)length;
    track->speed_zone = speed_zone;
    track->present = true;
    
    image->modified = true;
    
    return UFT_G64_OK;
}

int uft_g64_get_track_info(const uft_g64_image_t *image,
                           int half_track,
                           uft_g64_track_t *track_out)
{
    if (half_track < 1 || half_track > image->header.num_tracks) {
        return UFT_G64_E_TRACK;
    }
    
    *track_out = image->tracks[half_track - 1];
    track_out->data = NULL;  /* Don't copy data pointer */
    
    return UFT_G64_OK;
}

uint8_t uft_g64_track_speed_zone(int track)
{
    if (track < 1 || track > 42) {
        return 0;
    }
    return track_speed_zones[track];
}

uint16_t uft_g64_zone_track_size(uint8_t speed_zone)
{
    if (speed_zone > 3) {
        return zone_track_sizes[0];
    }
    return zone_track_sizes[speed_zone];
}

uint32_t uft_g64_zone_bitrate(uint8_t speed_zone)
{
    if (speed_zone > 3) {
        return zone_bitrates[0];
    }
    return zone_bitrates[speed_zone];
}

/* ═══════════════════════════════════════════════════════════════════════════
 * CONVERSION FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_g64_halftrack_to_track(int half_track)
{
    if (half_track < 1 || half_track > 168) {
        return 0;
    }
    
    /* Half-tracks 2, 4, 6, ... are real tracks */
    if ((half_track % 2) == 0) {
        return half_track / 2;
    }
    
    /* Odd half-tracks are between tracks */
    return 0;
}

int uft_g64_track_to_halftrack(int track)
{
    if (track < 1 || track > 84) {
        return 0;
    }
    return track * 2;
}

bool uft_g64_is_real_track(int half_track)
{
    return (half_track >= 2 && half_track <= 168 && (half_track % 2) == 0);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * ANALYSIS
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_g64_analyze(const uft_g64_image_t *image,
                    uft_g64_analysis_t *analysis)
{
    memset(analysis, 0, sizeof(*analysis));
    
    analysis->total_tracks = image->header.num_tracks;
    
    for (int i = 0; i < image->header.num_tracks; i++) {
        const uft_g64_track_t *track = &image->tracks[i];
        
        if (track->present && track->length > 0) {
            analysis->valid_tracks++;
            analysis->total_gcr_bytes += track->length;
            analysis->speed_zones_used |= (1 << track->speed_zone);
            
            if ((i % 2) == 0) {
                /* Odd index = half-track (between real tracks) */
                analysis->half_tracks_used++;
            }
        } else {
            analysis->empty_tracks++;
        }
    }
    
    return UFT_G64_OK;
}

void uft_g64_dump_track_table(const uft_g64_image_t *image, FILE *out)
{
    if (!out) out = stdout;
    
    fprintf(out, "G64 Track Table:\n");
    fprintf(out, "Version: %d, Tracks: %d, Max Size: %d\n\n",
            image->header.version, image->header.num_tracks,
            image->header.max_track_size);
    
    fprintf(out, "HT# TRK  Offset    Length  Zone\n");
    fprintf(out, "--- ---  --------  ------  ----\n");
    
    for (int i = 0; i < image->header.num_tracks; i++) {
        const uft_g64_track_t *track = &image->tracks[i];
        
        if (track->present) {
            int real_track = uft_g64_halftrack_to_track(i + 1);
            if (real_track > 0) {
                fprintf(out, "%3d %3d  %08X  %6d  %d\n",
                        i + 1, real_track, track->offset, 
                        track->length, track->speed_zone);
            } else {
                fprintf(out, "%3d  .%d  %08X  %6d  %d  (half-track)\n",
                        i + 1, (i + 1) / 2, track->offset,
                        track->length, track->speed_zone);
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * ERROR STRINGS
 * ═══════════════════════════════════════════════════════════════════════════ */

static const char *error_strings[] = {
    "Success",
    "Invalid G64 format",
    "Unsupported G64 version",
    "File truncated",
    "Invalid track number",
    "Track has no data",
    "Buffer too small",
    "File I/O error",
    "Memory allocation error",
    "GCR decode error"
};

const char *uft_g64_error_string(int error_code)
{
    if (error_code < 0 || error_code >= (int)(sizeof(error_strings)/sizeof(error_strings[0]))) {
        return "Unknown error";
    }
    return error_strings[error_code];
}
