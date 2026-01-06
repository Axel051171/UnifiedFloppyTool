/**
 * @file uft_transcopy.c
 * @brief Transcopy (.tc) Disk Image Format - Implementation
 * @version 1.0.0
 * 
 * SPDX-License-Identifier: MIT
 */

#include "uft/formats/uft_transcopy.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal Helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

static inline uint16_t rd_le16(const uint8_t* p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static inline void wr_le16(uint8_t* p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
}

static uint16_t get_track_index(const uft_tc_image_t* img, uint8_t track, uint8_t side)
{
    if (!img || track > img->track_end || side >= img->sides) {
        return 0xFFFF;
    }
    return (uint16_t)(track * img->sides + side);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Disk Type Information
 * ═══════════════════════════════════════════════════════════════════════════ */

static const struct {
    uft_tc_disk_type_t type;
    const char*        name;
    int                encoding;    /* 1=MFM, 2=FM, 3=GCR */
    size_t             track_len;   /* Typical track length */
    bool               variable;    /* Variable density */
} tc_disk_info[] = {
    { UFT_TC_TYPE_MFM_HD,      "MFM High Density",          1, 12500, false },
    { UFT_TC_TYPE_MFM_DD_360,  "MFM Double Density 360RPM", 1,  6250, false },
    { UFT_TC_TYPE_APPLE_GCR,   "Apple II GCR",              3,  6392, true  },
    { UFT_TC_TYPE_FM_SD,       "FM Single Density",         2,  3125, false },
    { UFT_TC_TYPE_C64_GCR,     "Commodore GCR",             3,  7928, true  },
    { UFT_TC_TYPE_MFM_DD,      "MFM Double Density",        1,  6250, false },
    { UFT_TC_TYPE_AMIGA_MFM,   "Commodore Amiga MFM",       1, 12668, false },
    { UFT_TC_TYPE_ATARI_FM,    "Atari FM",                  2,  3125, false },
    { UFT_TC_TYPE_UNKNOWN,     "Unknown",                   0,  6250, false }
};

#define TC_DISK_INFO_COUNT (sizeof(tc_disk_info) / sizeof(tc_disk_info[0]))

static int find_disk_info_index(uft_tc_disk_type_t type)
{
    for (size_t i = 0; i < TC_DISK_INFO_COUNT - 1; i++) {
        if (tc_disk_info[i].type == type) {
            return (int)i;
        }
    }
    return (int)(TC_DISK_INFO_COUNT - 1); /* Return "Unknown" */
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Detection Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

bool uft_tc_detect(const uint8_t* data, size_t size)
{
    if (!data || size < UFT_TC_HEADER_SIZE) {
        return false;
    }
    
    /* Check signature "TC" */
    if (data[0] != 'T' || data[1] != 'C') {
        return false;
    }
    
    return true;
}

int uft_tc_detect_confidence(const uint8_t* data, size_t size)
{
    if (!data || size < UFT_TC_HEADER_SIZE) {
        return 0;
    }
    
    int confidence = 0;
    
    /* Check signature */
    if (data[0] == 'T' && data[1] == 'C') {
        confidence = 70;
    } else {
        return 0;
    }
    
    /* Check disk type is valid */
    uint8_t disk_type = data[UFT_TC_OFF_DISKTYPE];
    if (disk_type == 0x02 || disk_type == 0x03 || disk_type == 0x04 ||
        disk_type == 0x05 || disk_type == 0x06 || disk_type == 0x07 ||
        disk_type == 0x08 || disk_type == 0x0C) {
        confidence += 15;
    }
    
    /* Check track range is reasonable */
    uint8_t track_end = data[UFT_TC_OFF_TRACK_END];
    uint8_t sides = data[UFT_TC_OFF_SIDES];
    
    if (track_end <= 84 && (sides == 1 || sides == 2)) {
        confidence += 10;
    }
    
    /* Check track increment */
    uint8_t track_inc = data[UFT_TC_OFF_TRACK_INC];
    if (track_inc == 1 || track_inc == 2) {
        confidence += 5;
    }
    
    if (confidence > 100) confidence = 100;
    return confidence;
}

const char* uft_tc_disk_type_name(uft_tc_disk_type_t type)
{
    int idx = find_disk_info_index(type);
    return tc_disk_info[idx].name;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Reading Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

uft_tc_status_t uft_tc_open(const uint8_t* data, size_t size,
                             uft_tc_image_t* image)
{
    if (!data || !image) {
        return UFT_TC_E_INVALID;
    }
    
    if (size < UFT_TC_HEADER_SIZE) {
        return UFT_TC_E_TRUNC;
    }
    
    /* Check signature */
    if (data[0] != 'T' || data[1] != 'C') {
        return UFT_TC_E_SIGNATURE;
    }
    
    memset(image, 0, sizeof(*image));
    
    /* Parse header */
    memcpy(image->comment, &data[UFT_TC_OFF_COMMENT], UFT_TC_COMMENT_LEN);
    image->comment[UFT_TC_COMMENT_LEN] = '\0';
    
    memcpy(image->comment2, &data[UFT_TC_OFF_COMMENT2], UFT_TC_COMMENT_LEN);
    image->comment2[UFT_TC_COMMENT_LEN] = '\0';
    
    image->disk_type = (uft_tc_disk_type_t)data[UFT_TC_OFF_DISKTYPE];
    image->track_start = data[UFT_TC_OFF_TRACK_START];
    image->track_end = data[UFT_TC_OFF_TRACK_END];
    image->sides = data[UFT_TC_OFF_SIDES];
    image->track_increment = data[UFT_TC_OFF_TRACK_INC];
    
    if (image->sides == 0) image->sides = 1;
    if (image->track_increment == 0) image->track_increment = 1;
    
    /* Calculate total tracks */
    uint16_t total_tracks = (uint16_t)((image->track_end + 1) * image->sides);
    
    /* Allocate track array */
    image->tracks = (uft_tc_track_t*)calloc(total_tracks, sizeof(uft_tc_track_t));
    if (!image->tracks) {
        return UFT_TC_E_ALLOC;
    }
    image->track_count = total_tracks;
    
    /* Parse track tables */
    for (uint16_t i = 0; i < total_tracks && i < UFT_TC_MAX_TRACKS; i++) {
        uft_tc_track_t* trk = &image->tracks[i];
        
        trk->skew = data[UFT_TC_OFF_SKEWS + i];
        trk->offset = rd_le16(&data[UFT_TC_OFF_OFFSETS + i * 2]);
        trk->length = rd_le16(&data[UFT_TC_OFF_LENGTHS + i * 2]);
        trk->flags = data[UFT_TC_OFF_FLAGS + i];
        trk->timing = rd_le16(&data[UFT_TC_OFF_TIMINGS + i * 2]);
        trk->data = NULL;
        trk->has_weak_bits = (trk->flags & UFT_TC_FLAG_COPY_WEAK) != 0;
    }
    
    /* Store raw data reference */
    image->raw_data = data;
    image->raw_size = size;
    image->owns_data = false;
    
    return UFT_TC_OK;
}

uft_tc_status_t uft_tc_load_track(uft_tc_image_t* image,
                                   uint8_t track, uint8_t side)
{
    if (!image || !image->tracks) {
        return UFT_TC_E_INVALID;
    }
    
    uint16_t idx = get_track_index(image, track, side);
    if (idx == 0xFFFF || idx >= image->track_count) {
        return UFT_TC_E_TRACK;
    }
    
    uft_tc_track_t* trk = &image->tracks[idx];
    
    /* Already loaded? */
    if (trk->data) {
        return UFT_TC_OK;
    }
    
    /* Calculate data offset */
    size_t offset = UFT_TC_OFF_DATA + (size_t)trk->offset * 256;
    
    if (offset + trk->length > image->raw_size) {
        return UFT_TC_E_TRUNC;
    }
    
    /* Allocate and copy track data */
    trk->data = (uint8_t*)malloc(trk->length);
    if (!trk->data) {
        return UFT_TC_E_ALLOC;
    }
    
    memcpy(trk->data, &image->raw_data[offset], trk->length);
    
    return UFT_TC_OK;
}

uft_tc_status_t uft_tc_get_track(const uft_tc_image_t* image,
                                  uint8_t track, uint8_t side,
                                  const uint8_t** data, size_t* length)
{
    if (!image || !image->tracks || !data || !length) {
        return UFT_TC_E_INVALID;
    }
    
    uint16_t idx = get_track_index(image, track, side);
    if (idx == 0xFFFF || idx >= image->track_count) {
        return UFT_TC_E_TRACK;
    }
    
    const uft_tc_track_t* trk = &image->tracks[idx];
    
    if (trk->data) {
        *data = trk->data;
        *length = trk->length;
    } else {
        /* Return pointer into raw data */
        size_t offset = UFT_TC_OFF_DATA + (size_t)trk->offset * 256;
        if (offset + trk->length > image->raw_size) {
            return UFT_TC_E_TRUNC;
        }
        *data = &image->raw_data[offset];
        *length = trk->length;
    }
    
    return UFT_TC_OK;
}

uint8_t uft_tc_get_track_flags(const uft_tc_image_t* image,
                                uint8_t track, uint8_t side)
{
    if (!image || !image->tracks) {
        return 0;
    }
    
    uint16_t idx = get_track_index(image, track, side);
    if (idx == 0xFFFF || idx >= image->track_count) {
        return 0;
    }
    
    return image->tracks[idx].flags;
}

void uft_tc_close(uft_tc_image_t* image)
{
    if (!image) return;
    
    /* Free track data */
    if (image->tracks) {
        for (uint16_t i = 0; i < image->track_count; i++) {
            free(image->tracks[i].data);
        }
        free(image->tracks);
    }
    
    /* Free raw data if we own it */
    if (image->owns_data && image->raw_data) {
        free((void*)image->raw_data);
    }
    
    memset(image, 0, sizeof(*image));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Writing Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

uft_tc_status_t uft_tc_writer_init(uft_tc_writer_t* writer,
                                    uft_tc_disk_type_t disk_type,
                                    uint8_t tracks, uint8_t sides)
{
    if (!writer || tracks == 0 || sides == 0 || sides > 2) {
        return UFT_TC_E_INVALID;
    }
    
    memset(writer, 0, sizeof(*writer));
    
    writer->disk_type = disk_type;
    writer->track_start = 0;
    writer->track_end = tracks - 1;
    writer->sides = sides;
    writer->track_increment = 1;
    
    uint16_t total = (uint16_t)(tracks * sides);
    writer->tracks = (uft_tc_track_t*)calloc(total, sizeof(uft_tc_track_t));
    if (!writer->tracks) {
        return UFT_TC_E_ALLOC;
    }
    writer->track_count = total;
    
    return UFT_TC_OK;
}

void uft_tc_writer_set_comment(uft_tc_writer_t* writer, const char* comment)
{
    if (!writer || !comment) return;
    
    size_t len = strlen(comment);
    if (len > UFT_TC_COMMENT_LEN) len = UFT_TC_COMMENT_LEN;
    
    memset(writer->comment, 0, sizeof(writer->comment));
    memcpy(writer->comment, comment, len);
}

uft_tc_status_t uft_tc_writer_add_track(uft_tc_writer_t* writer,
                                         uint8_t track, uint8_t side,
                                         const uint8_t* data, size_t length,
                                         uint8_t flags)
{
    if (!writer || !writer->tracks || !data) {
        return UFT_TC_E_INVALID;
    }
    
    if (track > writer->track_end || side >= writer->sides) {
        return UFT_TC_E_TRACK;
    }
    
    uint16_t idx = (uint16_t)(track * writer->sides + side);
    if (idx >= writer->track_count) {
        return UFT_TC_E_TRACK;
    }
    
    uft_tc_track_t* trk = &writer->tracks[idx];
    
    /* Free existing data */
    free(trk->data);
    
    /* Copy new data */
    trk->data = (uint8_t*)malloc(length);
    if (!trk->data) {
        return UFT_TC_E_ALLOC;
    }
    
    memcpy(trk->data, data, length);
    trk->length = (uint16_t)length;
    trk->flags = flags;
    trk->has_weak_bits = (flags & UFT_TC_FLAG_COPY_WEAK) != 0;
    
    return UFT_TC_OK;
}

uft_tc_status_t uft_tc_writer_finish(uft_tc_writer_t* writer,
                                      uint8_t** out_data, size_t* out_size)
{
    if (!writer || !writer->tracks || !out_data || !out_size) {
        return UFT_TC_E_INVALID;
    }
    
    /* Calculate total size */
    size_t data_size = 0;
    for (uint16_t i = 0; i < writer->track_count; i++) {
        if (writer->tracks[i].data) {
            /* Align to 256-byte boundary */
            size_t aligned = (writer->tracks[i].length + 255) & ~255;
            data_size += aligned;
        }
    }
    
    size_t total_size = UFT_TC_HEADER_SIZE + data_size;
    
    /* Allocate output buffer */
    uint8_t* buf = (uint8_t*)calloc(1, total_size);
    if (!buf) {
        return UFT_TC_E_ALLOC;
    }
    
    /* Write signature */
    buf[0] = 'T';
    buf[1] = 'C';
    
    /* Write comments */
    memcpy(&buf[UFT_TC_OFF_COMMENT], writer->comment, UFT_TC_COMMENT_LEN);
    
    /* Write disk info */
    buf[UFT_TC_OFF_DISKTYPE] = (uint8_t)writer->disk_type;
    buf[UFT_TC_OFF_TRACK_START] = writer->track_start;
    buf[UFT_TC_OFF_TRACK_END] = writer->track_end;
    buf[UFT_TC_OFF_SIDES] = writer->sides;
    buf[UFT_TC_OFF_TRACK_INC] = writer->track_increment;
    
    /* Write track tables and data */
    uint16_t data_offset = 0;
    for (uint16_t i = 0; i < writer->track_count && i < UFT_TC_MAX_TRACKS; i++) {
        uft_tc_track_t* trk = &writer->tracks[i];
        
        /* Skew */
        buf[UFT_TC_OFF_SKEWS + i] = trk->skew;
        
        /* Offset (in 256-byte units) */
        wr_le16(&buf[UFT_TC_OFF_OFFSETS + i * 2], data_offset);
        trk->offset = data_offset;
        
        /* Length */
        wr_le16(&buf[UFT_TC_OFF_LENGTHS + i * 2], trk->length);
        
        /* Flags */
        buf[UFT_TC_OFF_FLAGS + i] = trk->flags;
        
        /* Timing */
        wr_le16(&buf[UFT_TC_OFF_TIMINGS + i * 2], trk->timing);
        
        /* Copy track data */
        if (trk->data && trk->length > 0) {
            size_t offset = UFT_TC_OFF_DATA + (size_t)data_offset * 256;
            memcpy(&buf[offset], trk->data, trk->length);
            
            /* Advance offset (aligned) */
            data_offset += (uint16_t)((trk->length + 255) / 256);
        }
    }
    
    *out_data = buf;
    *out_size = total_size;
    
    return UFT_TC_OK;
}

void uft_tc_writer_free(uft_tc_writer_t* writer)
{
    if (!writer) return;
    
    if (writer->tracks) {
        for (uint16_t i = 0; i < writer->track_count; i++) {
            free(writer->tracks[i].data);
        }
        free(writer->tracks);
    }
    
    free(writer->data);
    
    memset(writer, 0, sizeof(*writer));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Conversion Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_tc_get_encoding(uft_tc_disk_type_t disk_type)
{
    int idx = find_disk_info_index(disk_type);
    return tc_disk_info[idx].encoding;
}

size_t uft_tc_expected_track_length(uft_tc_disk_type_t disk_type, uint8_t track)
{
    int idx = find_disk_info_index(disk_type);
    
    /* Handle variable density formats */
    if (disk_type == UFT_TC_TYPE_C64_GCR) {
        /* C64 1541 speed zones */
        if (track < 17) return 7692;       /* Zone 0: 21 sectors */
        if (track < 24) return 7142;       /* Zone 1: 19 sectors */
        if (track < 30) return 6666;       /* Zone 2: 18 sectors */
        return 6250;                        /* Zone 3: 17 sectors */
    }
    
    if (disk_type == UFT_TC_TYPE_APPLE_GCR) {
        /* Apple II variable density */
        if (track < 16) return 6392;
        if (track < 32) return 6282;
        return 6172;
    }
    
    return tc_disk_info[idx].track_len;
}

bool uft_tc_is_variable_density(uft_tc_disk_type_t disk_type)
{
    int idx = find_disk_info_index(disk_type);
    return tc_disk_info[idx].variable;
}
