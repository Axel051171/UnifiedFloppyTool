// SPDX-License-Identifier: MIT
/*
 * hxc_hfe.c - HXC HFE Format Parser
 * 
 * Professional implementation of HFE (HxC Floppy Emulator) format.
 * HFE is the native format of HxC Floppy Emulator hardware.
 * 
 * Format Overview:
 *   - Header with disk geometry
 *   - Track table (LUT - Look Up Table)
 *   - Interleaved track data (side 0, side 1, side 0, ...)
 *   - Supports MFM, FM, GCR encoding
 * 
 * References:
 *   - HxC Floppy Emulator: http://hxc2001.com/
 *   - HFE Format Spec v3
 * 
 * @version 2.7.5
 * @date 2024-12-25
 */

#include "uft/uft_error.h"
#include "../include/hxc_format.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*============================================================================*
 * HFE FORMAT CONSTANTS
 *============================================================================*/

#define HFE_SIGNATURE        "HXCPICFE"
#define HFE_SIGNATURE_LEN    8
#define HFE_HEADER_SIZE      512
#define HFE_TRACK_BLOCK_SIZE 512

/*============================================================================*
 * HFE HEADER STRUCTURE
 *============================================================================*/

#pragma pack(push, 1)
typedef struct {
    char signature[8];          /* "HXCPICFE" */
    uint8_t format_revision;    /* Format revision */
    uint8_t number_of_tracks;   /* Number of tracks */
    uint8_t number_of_sides;    /* Number of sides (1 or 2) */
    uint8_t track_encoding;     /* Track encoding (see below) */
    uint16_t bitrate_kbps;      /* Bitrate in Kbps */
    uint16_t floppy_rpm;        /* RPM (typically 300 or 360) */
    uint8_t floppy_interface_mode; /* Interface mode */
    uint8_t track_list_offset;  /* Track list offset (in 512-byte blocks) */
    uint8_t write_allowed;      /* Write protection */
    uint8_t single_step;        /* Single step mode */
    uint8_t track0s0_altencoding; /* Track 0 Side 0 alt encoding */
    uint8_t track0s0_encoding;  /* Track 0 Side 0 encoding */
    uint8_t track0s1_altencoding; /* Track 0 Side 1 alt encoding */
    uint8_t track0s1_encoding;  /* Track 0 Side 1 encoding */
} hfe_header_t;
#pragma pack(pop)

/*
 * Track Encoding Values:
 *   0x00 = ISOIBM_MFM_ENCODING
 *   0x01 = AMIGA_MFM_ENCODING
 *   0x02 = ISOIBM_FM_ENCODING
 *   0x03 = EMU_FM_ENCODING
 *   0xFF = UNKNOWN_ENCODING
 */

/*============================================================================*
 * TRACK LUT (Look Up Table)
 *============================================================================*/

#pragma pack(push, 1)
typedef struct {
    uint16_t offset;            /* Track offset in 512-byte blocks */
    uint16_t track_len;         /* Track length in bytes */
} hfe_track_lut_entry_t;
#pragma pack(pop)

/*============================================================================*
 * UTILITIES
 *============================================================================*/

static inline uint16_t rd_le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

/*============================================================================*
 * HFE PARSING
 *============================================================================*/

/**
 * @brief Parse HFE header
 * 
 * @param file File buffer
 * @param file_len File size
 * @param hdr_out Parsed header (output)
 * @return 0 on success
 */
static int parse_header(
    const uint8_t *file,
    size_t file_len,
    hfe_header_t *hdr_out
) {
    if (file_len < HFE_HEADER_SIZE) {
        return -1;
    }
    
    /* Verify signature */
    if (memcmp(file, HFE_SIGNATURE, HFE_SIGNATURE_LEN) != 0) {
        return -1;
    }
    
    /* Copy header */
    memcpy(hdr_out, file, sizeof(*hdr_out));
    
    return 0;
}

/**
 * @brief Parse track LUT
 * 
 * @param file File buffer
 * @param file_len File size
 * @param header HFE header
 * @param lut_out Track LUT (allocated by this function)
 * @param count_out Number of tracks
 * @return 0 on success
 */
static int parse_track_lut(
    const uint8_t *file,
    size_t file_len,
    const hfe_header_t *header,
    hfe_track_lut_entry_t **lut_out,
    uint32_t *count_out
) {
    /* LUT starts at offset specified in header */
    size_t lut_offset = (size_t)header->track_list_offset * HFE_TRACK_BLOCK_SIZE;
    
    if (lut_offset >= file_len) {
        return -1;
    }
    
    /* Each track has entry for each side */
    uint32_t num_entries = header->number_of_tracks * header->number_of_sides;
    size_t lut_size = num_entries * sizeof(hfe_track_lut_entry_t);
    
    if (lut_offset + lut_size > file_len) {
        return -1;
    }
    
    /* Allocate LUT */
    hfe_track_lut_entry_t *lut = calloc(num_entries, sizeof(*lut));
    if (!lut) {
        return -1;
    }
    
    /* Parse LUT entries */
    const uint8_t *lut_data = file + lut_offset;
    
    for (uint32_t i = 0; i < num_entries; i++) {
        lut[i].offset = rd_le16(lut_data + i * 4 + 0);
        lut[i].track_len = rd_le16(lut_data + i * 4 + 2);
    }
    
    *lut_out = lut;
    *count_out = num_entries;
    
    return 0;
}

/**
 * @brief Extract track data
 * 
 * HFE stores tracks interleaved: 256 bytes side 0, 256 bytes side 1, repeat.
 * 
 * @param file File buffer
 * @param file_len File size
 * @param lut_entry LUT entry for this track
 * @param track_out Track data (allocated by this function)
 * @return 0 on success
 */
static int extract_track_data(
    const uint8_t *file,
    size_t file_len,
    const hfe_track_lut_entry_t *lut_entry,
    hxc_track_t *track_out
) {
    if (lut_entry->track_len == 0) {
        track_out->data = NULL;
        track_out->size = 0;
        return 0;
    }
    
    size_t track_offset = (size_t)lut_entry->offset * HFE_TRACK_BLOCK_SIZE;
    
    if (track_offset + lut_entry->track_len > file_len) {
        return -1;
    }
    
    /* Allocate track data */
    uint8_t *data = malloc(lut_entry->track_len);
    if (!data) {
        return -1;
    }
    
    /* Copy track data */
    memcpy(data, file + track_offset, lut_entry->track_len);
    
    track_out->data = data;
    track_out->size = lut_entry->track_len;
    track_out->bitrate = 0;  /* Set by caller */
    track_out->encoding = 0;
    
    return 0;
}

/*============================================================================*
 * PUBLIC API
 *============================================================================*/

/**
 * @brief Parse HFE file
 * 
 * Professional HFE parser with complete error handling.
 * 
 * @param file File buffer
 * @param file_len File size
 * @param hfe_out Parsed HFE image (output)
 * @return 0 on success
 */
int hxc_parse_hfe(
    const uint8_t *file,
    size_t file_len,
    hxc_hfe_image_t *hfe_out
) {
    if (!file || !hfe_out || file_len < HFE_HEADER_SIZE) {
        return HXC_ERR_INVALID;
    }
    
    memset(hfe_out, 0, sizeof(*hfe_out));
    
    /* Parse header */
    hfe_header_t header;
    if (parse_header(file, file_len, &header) < 0) {
        return HXC_ERR_FORMAT;
    }
    
    /* Copy header info */
    hfe_out->format_revision = header.format_revision;
    hfe_out->number_of_tracks = header.number_of_tracks;
    hfe_out->number_of_sides = header.number_of_sides;
    hfe_out->track_encoding = header.track_encoding;
    hfe_out->bitrate_kbps = header.bitrate_kbps;
    hfe_out->rpm = header.floppy_rpm;
    hfe_out->write_protected = (header.write_allowed == 0) ? 1 : 0;
    
    /* Parse track LUT */
    hfe_track_lut_entry_t *lut = NULL;
    uint32_t lut_count = 0;
    
    if (parse_track_lut(file, file_len, &header, &lut, &lut_count) < 0) {
        return HXC_ERR_FORMAT;
    }
    
    /* Allocate tracks */
    uint32_t total_tracks = header.number_of_tracks * header.number_of_sides;
    hfe_out->tracks = calloc(total_tracks, sizeof(*hfe_out->tracks));
    
    if (!hfe_out->tracks) {
        free(lut);
        return HXC_ERR_NOMEM;
    }
    
    hfe_out->track_count = total_tracks;
    
    /* Extract each track */
    for (uint32_t i = 0; i < lut_count && i < total_tracks; i++) {
        if (extract_track_data(file, file_len, &lut[i], &hfe_out->tracks[i]) < 0) {
            /* Cleanup on error */
            for (uint32_t j = 0; j < i; j++) {
                free(hfe_out->tracks[j].data);
            }
            free(hfe_out->tracks);
            free(lut);
            return HXC_ERR_FORMAT;
        }
        
        hfe_out->tracks[i].bitrate = header.bitrate_kbps;
        hfe_out->tracks[i].encoding = header.track_encoding;
    }
    
    free(lut);
    
    return HXC_OK;
}

/**
 * @brief Free HFE image
 */
void hxc_free_hfe(hxc_hfe_image_t *hfe)
{
    if (!hfe) {
        return;
    }
    
    if (hfe->tracks) {
        for (uint32_t i = 0; i < hfe->track_count; i++) {
            free(hfe->tracks[i].data);
        }
        free(hfe->tracks);
    }
    
    memset(hfe, 0, sizeof(*hfe));
}

/**
 * @brief Load HFE from file
 */
int hxc_load_hfe_file(
    const char *path,
    hxc_hfe_image_t *hfe_out
) {
    if (!path || !hfe_out) {
        return HXC_ERR_INVALID;
    }
    
    FILE *f = fopen(path, "rb");
    if (!f) {
        return HXC_ERR_INVALID;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (size <= 0) {
        fclose(f);
        return HXC_ERR_INVALID;
    }
    
    uint8_t *buf = malloc(size);
    if (!buf) {
        fclose(f);
        return HXC_ERR_NOMEM;
    }
    
    if (fread(buf, 1, size, f) != (size_t)size) {
        free(buf);
        fclose(f);
        return HXC_ERR_INVALID;
    }
    
    fclose(f);
    
    int rc = hxc_parse_hfe(buf, size, hfe_out);
    
    free(buf);
    
    return rc;
}

/**
 * @brief Print HFE info
 */
void hxc_hfe_print_info(const hxc_hfe_image_t *hfe)
{
    if (!hfe) {
        return;
    }
    
    printf("HFE Image Info:\n");
    printf("  Format Revision: %u\n", hfe->format_revision);
    printf("  Tracks:          %u\n", hfe->number_of_tracks);
    printf("  Sides:           %u\n", hfe->number_of_sides);
    printf("  Encoding:        ");
    
    switch (hfe->track_encoding) {
        case 0x00: printf("ISO/IBM MFM\n"); break;
        case 0x01: printf("Amiga MFM\n"); break;
        case 0x02: printf("ISO/IBM FM\n"); break;
        case 0x03: printf("EMU FM\n"); break;
        default: printf("Unknown (0x%02X)\n", hfe->track_encoding);
    }
    
    printf("  Bitrate:         %u Kbps\n", hfe->bitrate_kbps);
    printf("  RPM:             %u\n", hfe->rpm);
    printf("  Write Protected: %s\n", hfe->write_protected ? "Yes" : "No");
    printf("  Total Tracks:    %u\n", hfe->track_count);
    printf("\n");
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "Linux"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
