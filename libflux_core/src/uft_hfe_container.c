/**
 * @file uft_hfe_container.c
 * @brief HFE Container Parser Implementation (Layer 1)
 * 
 * STRICT LAYER 1 ONLY:
 * - Parse header structure
 * - Read track lookup table
 * - Provide raw track data access
 * - NO geometry assumptions
 * - NO decoding (that's Layer 3)
 */

#include "uft_hfe_container.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* HFE v1 header structure (512 bytes) */
typedef struct __attribute__((packed)) {
    uint8_t signature[8];           /* "HXCPICFE" */
    uint8_t format_revision;        /* 0x00 */
    uint8_t track_count;
    uint8_t side_count;
    uint8_t track_encoding;
    uint16_t bitrate;               /* LE */
    uint16_t rpm;                   /* LE */
    uint8_t interface_mode;
    uint8_t reserved;
    uint16_t track_list_offset;     /* LE, in 512-byte blocks */
    uint8_t write_allowed;
    uint8_t single_step;            /* v1: always 0xFF */
    uint8_t track0_s0_altencoding;
    uint8_t track0_s0_encoding;
    uint8_t track0_s1_altencoding;
    uint8_t track0_s1_encoding;
} hfe_v1_header_t;

/* HFE v3 extends this to 1024 bytes with additional fields */

/* ========================================================================
 * HELPER FUNCTIONS
 * ======================================================================== */

static uint16_t read_le16(const uint8_t* buf) {
    return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}

const char* uft_hfe_version_string(uft_hfe_version_t version) {
    switch (version) {
    case UFT_HFE_VERSION_1: return "HFE v1";
    case UFT_HFE_VERSION_3: return "HFE v3";
    default: return "HFE (unknown version)";
    }
}

const char* uft_hfe_encoding_name(uft_hfe_encoding_t encoding) {
    switch (encoding) {
    case UFT_HFE_ENCODING_ISOIBM_MFM: return "ISO/IBM MFM";
    case UFT_HFE_ENCODING_AMIGA_MFM: return "Amiga MFM";
    case UFT_HFE_ENCODING_ISOIBM_FM: return "ISO/IBM FM";
    case UFT_HFE_ENCODING_EMU_FM: return "Emu FM";
    default: return "Unknown";
    }
}

/* ========================================================================
 * CONTAINER PARSING
 * ======================================================================== */

uft_rc_t uft_hfe_container_open(
    const char* path,
    uft_hfe_container_t** container
) {
    UFT_CHECK_NULL(path);
    UFT_CHECK_NULL(container);
    
    *container = NULL;
    
    /* Allocate container */
    uft_hfe_container_t* hfe = calloc(1, sizeof(uft_hfe_container_t));
    if (!hfe) {
        return UFT_ERR_MEMORY;
    }
    
    /* Open file */
    hfe->fp = fopen(path, "rb");
    if (!hfe->fp) {
        free(hfe);
        return UFT_ERR_NOT_FOUND;
    }
    
    /* Read potential header (first 512 bytes minimum) */
    uint8_t header_buf[1024];
    size_t read_size = fread(header_buf, 1, sizeof(header_buf), hfe->fp);
    
    if (read_size < 512) {
        fclose(hfe->fp);
        free(hfe);
        return UFT_ERR_CORRUPTED;
    }
    
    /* Verify signature */
    if (memcmp(header_buf, "HXCPICFE", 8) != 0) {
        fclose(hfe->fp);
        free(hfe);
        return UFT_ERR_INVALID_FORMAT;
    }
    
    /* Copy signature */
    memcpy(hfe->header.signature, header_buf, 8);
    
    /* Read format revision (determines version) */
    hfe->header.format_revision = header_buf[8];
    
    /* Parse common fields */
    hfe->header.track_count = header_buf[9];
    hfe->header.side_count = header_buf[10];
    hfe->header.track_encoding = header_buf[11];
    hfe->header.bitrate = read_le16(&header_buf[12]);
    hfe->header.rpm = read_le16(&header_buf[14]);
    hfe->header.interface_mode = header_buf[16];
    /* header_buf[17] = reserved */
    hfe->header.track_list_offset = read_le16(&header_buf[18]);
    hfe->header.write_allowed = (header_buf[20] == 0xFF);
    
    /* Version-specific parsing */
    if (hfe->header.format_revision == UFT_HFE_VERSION_1) {
        /* HFE v1: 512-byte header, 256-byte track encoding */
        hfe->header.header_size = 512;
        hfe->header.track_encoding_size = 256;
        hfe->header.has_extended_header = false;
        
        /* v1 has these at offset 21-24 but they're often 0xFF (unused) */
        hfe->header.single_step = (header_buf[21] == 0xFF);
        hfe->header.track0_s0_altencoding = header_buf[22];
        hfe->header.track0_s0_encoding = header_buf[23];
        hfe->header.track0_s1_altencoding = header_buf[24];
        hfe->header.track0_s1_encoding = header_buf[25];
        
    } else if (hfe->header.format_revision == UFT_HFE_VERSION_3) {
        /* HFE v3: 1024-byte header, 512-byte track encoding */
        hfe->header.header_size = 1024;
        hfe->header.track_encoding_size = 512;
        hfe->header.has_extended_header = true;
        
        /* v3 extended fields */
        hfe->header.single_step = (header_buf[21] != 0xFF);
        hfe->header.track0_s0_altencoding = header_buf[22];
        hfe->header.track0_s0_encoding = header_buf[23];
        hfe->header.track0_s1_altencoding = header_buf[24];
        hfe->header.track0_s1_encoding = header_buf[25];
        
        /* Preserve extended header (bytes 512-1023) */
        if (read_size >= 1024) {
            hfe->header.extended_size = 512;
            hfe->header.extended_data = malloc(512);
            if (hfe->header.extended_data) {
                memcpy(hfe->header.extended_data, &header_buf[512], 512);
            }
        }
        
    } else {
        /* Unknown version - use conservative defaults */
        hfe->header.header_size = 512;
        hfe->header.track_encoding_size = 256;
        hfe->header.has_extended_header = false;
        
        /* Preserve unknown fields for forward compatibility */
        hfe->header.extended_size = 512 - 26;
        hfe->header.extended_data = malloc(hfe->header.extended_size);
        if (hfe->header.extended_data) {
            memcpy(hfe->header.extended_data, &header_buf[26], 
                   hfe->header.extended_size);
        }
    }
    
    /* Read track lookup table (LUT) */
    uint32_t lut_offset = hfe->header.track_list_offset * 512;
    
    if (fseek(hfe->fp, lut_offset, SEEK_SET) != 0) {
        if (hfe->header.extended_data) free(hfe->header.extended_data);
        fclose(hfe->fp);
        free(hfe);
        return UFT_ERR_CORRUPTED;
    }
    
    /* LUT size: track_count * side_count * 4 bytes (offset + length per track/side) */
    hfe->track_lut_size = hfe->header.track_count * hfe->header.side_count;
    hfe->track_lut = malloc(hfe->track_lut_size * sizeof(uft_hfe_track_offset_t));
    
    if (!hfe->track_lut) {
        if (hfe->header.extended_data) free(hfe->header.extended_data);
        fclose(hfe->fp);
        free(hfe);
        return UFT_ERR_MEMORY;
    }
    
    /* Read LUT entries */
    for (uint32_t i = 0; i < hfe->track_lut_size; i++) {
        uint8_t entry[4];
        if (fread(entry, 1, 4, hfe->fp) != 4) {
            free(hfe->track_lut);
            if (hfe->header.extended_data) free(hfe->header.extended_data);
            fclose(hfe->fp);
            free(hfe);
            return UFT_ERR_CORRUPTED;
        }
        
        hfe->track_lut[i].offset = read_le16(&entry[0]);
        hfe->track_lut[i].length = read_le16(&entry[2]);
    }
    
    *container = hfe;
    return UFT_SUCCESS;
}

void uft_hfe_container_close(uft_hfe_container_t** container) {
    if (container && *container) {
        if ((*container)->track_lut) {
            free((*container)->track_lut);
        }
        if ((*container)->header.extended_data) {
            free((*container)->header.extended_data);
        }
        if ((*container)->fp) {
            fclose((*container)->fp);
        }
        free(*container);
        *container = NULL;
    }
}

/* ========================================================================
 * TRACK ACCESS
 * ======================================================================== */

bool uft_hfe_has_track(
    const uft_hfe_container_t* container,
    uint8_t track,
    uint8_t side
) {
    if (!container || track >= container->header.track_count || 
        side >= container->header.side_count) {
        return false;
    }
    
    uint32_t idx = track * container->header.side_count + side;
    
    if (idx >= container->track_lut_size) {
        return false;
    }
    
    return (container->track_lut[idx].offset != 0);
}

uft_rc_t uft_hfe_get_track_offset(
    const uft_hfe_container_t* container,
    uint8_t track,
    uint8_t side,
    uft_hfe_track_offset_t* offset
) {
    UFT_CHECK_NULLS(container, offset);
    
    if (track >= container->header.track_count || 
        side >= container->header.side_count) {
        return UFT_ERR_INVALID_ARG;
    }
    
    uint32_t idx = track * container->header.side_count + side;
    
    if (idx >= container->track_lut_size) {
        return UFT_ERR_INVALID_ARG;
    }
    
    *offset = container->track_lut[idx];
    return UFT_SUCCESS;
}

uft_rc_t uft_hfe_read_track_raw(
    uft_hfe_container_t* container,
    uint8_t track,
    uint8_t side,
    uint8_t** data,
    size_t* size
) {
    UFT_CHECK_NULLS(container, data, size);
    
    *data = NULL;
    *size = 0;
    
    /* Get track offset */
    uft_hfe_track_offset_t offset;
    uft_rc_t rc = uft_hfe_get_track_offset(container, track, side, &offset);
    if (uft_failed(rc)) {
        return rc;
    }
    
    if (offset.offset == 0 || offset.length == 0) {
        return UFT_ERR_NOT_FOUND;
    }
    
    /* Calculate file offset */
    uint32_t file_offset = offset.offset * 512;
    
    /* Seek to track */
    if (fseek(container->fp, file_offset, SEEK_SET) != 0) {
        return UFT_ERR_IO;
    }
    
    /* Allocate buffer */
    uint8_t* track_data = malloc(offset.length);
    if (!track_data) {
        return UFT_ERR_MEMORY;
    }
    
    /* Read track data (HFE interleaved encoding) */
    if (fread(track_data, 1, offset.length, container->fp) != offset.length) {
        free(track_data);
        return UFT_ERR_IO;
    }
    
    *data = track_data;
    *size = offset.length;
    container->tracks_read++;
    
    return UFT_SUCCESS;
}
