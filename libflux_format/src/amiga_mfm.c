// SPDX-License-Identifier: MIT
/*
 * amiga_mfm.c - Amiga MFM Sector Decoder
 * 
 * Ported from FloppyControl (C# -> C)
 * Original: https://github.com/Skaizo/FloppyControl
 * 
 * Supports:
 *   - AmigaDOS (OFS/FFS) - 11 sectors/track
 *   - DiskSpare - 12 sectors/track
 *   - PFS (same structure as AmigaDOS)
 * 
 * @version 1.0.0
 * @date 2025-01-01
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/*============================================================================*
 * AMIGA CONSTANTS
 *============================================================================*/

/* AmigaDOS sync marker: 0x4489 0x4489 in MFM bits */
static const uint8_t AMIGA_MARKER[] = {
    0,1,0,0,0,1,0,0,1,0,0,0,1,0,0,1,  /* 0x4489 */
    0,1,0,0,0,1,0,0,1,0,0,0,1,0,0,1   /* 0x4489 */
};
#define AMIGA_MARKER_LEN 32

/* DiskSpare sync marker: 0x4489 0x4489 + 0x2AAA */
static const uint8_t DISKSPARE_MARKER[] = {
    0,1,0,0,0,1,0,0,1,0,0,0,1,0,0,1,  /* 0x4489 */
    0,1,0,0,0,1,0,0,1,0,0,0,1,0,0,1,  /* 0x4489 */
    0,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0   /* 0x2AAA */
};
#define DISKSPARE_MARKER_LEN 48

/* Sector sizes */
#define AMIGA_SECTOR_SIZE       512
#define AMIGA_SECTORS_PER_TRACK 11
#define DISKSPARE_SECTORS_PER_TRACK 12

/* MFM bits per sector */
#define AMIGA_MFM_BITS_PER_SECTOR   8704
#define DISKSPARE_MFM_BITS_PER_SECTOR 8336

/*============================================================================*
 * DATA STRUCTURES
 *============================================================================*/

typedef enum {
    AMIGA_FORMAT_UNKNOWN = 0,
    AMIGA_FORMAT_ADOS,          /* AmigaDOS OFS/FFS */
    AMIGA_FORMAT_DISKSPARE,     /* DiskSpare */
    AMIGA_FORMAT_PFS            /* Professional File System */
} amiga_format_t;

typedef struct {
    uint8_t format;             /* Sector format byte (0xFF = AmigaDOS) */
    uint8_t track;              /* Track number */
    uint8_t sector;             /* Sector number */
    uint8_t sectors_to_gap;     /* Sectors until track gap */
    uint16_t os_recovery;       /* OS recovery info */
    uint32_t header_checksum;   /* Header checksum */
    uint32_t data_checksum;     /* Data checksum */
    bool header_ok;             /* Header checksum valid */
    bool data_ok;               /* Data checksum valid */
    uint8_t data[AMIGA_SECTOR_SIZE];  /* Decoded sector data */
} amiga_sector_t;

typedef struct {
    int marker_position;        /* Position in MFM stream */
    int rxbuf_position;         /* Position in raw data */
    amiga_sector_t sector;      /* Decoded sector */
} amiga_marker_t;

/*============================================================================*
 * AMIGA MFM DECODING
 *============================================================================*/

/**
 * @brief Decode Amiga MFM odd/even encoding
 * 
 * Amiga uses a special MFM encoding where odd and even bits
 * are stored separately. This function combines them.
 * 
 * @param mfm MFM bitstream
 * @param offset Bit offset
 * @param out Output buffer
 * @param len Number of bytes to decode
 */
static void amiga_mfm_decode(const uint8_t *mfm, int offset, uint8_t *out, int len) {
    /* Odd bits at offset, even bits at offset + len*8 */
    int odd_offset = offset;
    int even_offset = offset + len * 8;
    
    for (int i = 0; i < len; i++) {
        uint8_t odd = 0;
        uint8_t even = 0;
        
        /* Extract 8 odd bits */
        for (int b = 0; b < 8; b++) {
            odd = (odd << 1) | (mfm[odd_offset + i * 8 + b] & 1);
        }
        
        /* Extract 8 even bits */
        for (int b = 0; b < 8; b++) {
            even = (even << 1) | (mfm[even_offset + i * 8 + b] & 1);
        }
        
        /* Interleave: odd bits in odd positions, even in even */
        out[i] = (odd & 0x55) | (even & 0xAA);
    }
}

/**
 * @brief Convert MFM bits to 16-bit value
 */
static uint16_t mfm_to_uint16(const uint8_t *mfm, int offset) {
    uint16_t result = 0;
    for (int i = 0; i < 16; i++) {
        result = (result << 1) | (mfm[offset + i] & 1);
    }
    return result;
}

/**
 * @brief Calculate Amiga checksum (XOR of all words)
 */
static uint32_t amiga_checksum(const uint8_t *mfm, int offset, int words) {
    uint32_t checksum = 0;
    
    for (int i = 0; i < words; i++) {
        uint16_t word = mfm_to_uint16(mfm, offset + i * 16);
        checksum ^= word;
    }
    
    return checksum & 0x55555555;  /* Mask out clock bits */
}

/**
 * @brief Find all AmigaDOS markers in MFM stream
 * 
 * @param mfm MFM bitstream
 * @param mfm_len MFM length
 * @param markers Output marker array
 * @param max_markers Maximum markers to find
 * @param format Format to search for (ADOS or DiskSpare)
 * @return Number of markers found
 */
int amiga_find_markers(
    const uint8_t *mfm,
    size_t mfm_len,
    amiga_marker_t *markers,
    int max_markers,
    amiga_format_t format)
{
    if (!mfm || !markers || mfm_len == 0) return 0;
    
    const uint8_t *marker;
    int marker_len;
    
    if (format == AMIGA_FORMAT_DISKSPARE) {
        marker = DISKSPARE_MARKER;
        marker_len = DISKSPARE_MARKER_LEN;
    } else {
        marker = AMIGA_MARKER;
        marker_len = AMIGA_MARKER_LEN;
    }
    
    int found = 0;
    
    for (size_t i = 0; i < mfm_len - marker_len && found < max_markers; i++) {
        bool match = true;
        
        for (int j = 0; j < marker_len && match; j++) {
            if (mfm[i + j] != marker[j]) {
                match = false;
            }
        }
        
        if (match) {
            markers[found].marker_position = i - 32;  /* Back to AAAA preamble */
            markers[found].rxbuf_position = 0;
            found++;
            
            /* Skip past this marker */
            i += marker_len;
        }
    }
    
    return found;
}

/**
 * @brief Decode AmigaDOS sector from MFM
 * 
 * @param mfm MFM bitstream
 * @param marker_pos Marker position
 * @param sector Output sector data
 * @return 0 on success, -1 on error
 */
int amiga_decode_sector_ados(
    const uint8_t *mfm,
    int marker_pos,
    amiga_sector_t *sector)
{
    if (!mfm || !sector) return -1;
    
    /* Header starts after sync (0x4489 0x4489) at marker_pos + 64 */
    int header_offset = marker_pos + 64;
    
    /* Decode header (4 bytes: format, track, sector, sectors_to_gap) */
    uint8_t header[4];
    amiga_mfm_decode(mfm, header_offset, header, 4);
    
    sector->format = header[0];
    sector->track = header[1];
    sector->sector = header[2];
    sector->sectors_to_gap = header[3];
    
    /* Verify format byte (should be 0xFF for AmigaDOS) */
    if (sector->format != 0xFF) {
        /* Could be a different format or corrupt */
    }
    
    /* Header checksum at header_offset + 64 */
    uint8_t hdr_csum[4];
    amiga_mfm_decode(mfm, header_offset + 64, hdr_csum, 4);
    sector->header_checksum = (hdr_csum[0] << 24) | (hdr_csum[1] << 16) | 
                              (hdr_csum[2] << 8) | hdr_csum[3];
    
    /* Calculate expected header checksum */
    uint32_t calc_hdr_csum = amiga_checksum(mfm, header_offset, 4);
    sector->header_ok = (sector->header_checksum == calc_hdr_csum);
    
    /* Data checksum at header_offset + 128 */
    uint8_t data_csum[4];
    amiga_mfm_decode(mfm, header_offset + 128, data_csum, 4);
    sector->data_checksum = (data_csum[0] << 24) | (data_csum[1] << 16) | 
                            (data_csum[2] << 8) | data_csum[3];
    
    /* Decode sector data (512 bytes) at header_offset + 192 */
    amiga_mfm_decode(mfm, header_offset + 192, sector->data, AMIGA_SECTOR_SIZE);
    
    /* Calculate data checksum */
    uint32_t calc_data_csum = 0;
    for (int i = 0; i < AMIGA_SECTOR_SIZE / 4; i++) {
        uint32_t word = (sector->data[i*4] << 24) | (sector->data[i*4+1] << 16) |
                        (sector->data[i*4+2] << 8) | sector->data[i*4+3];
        calc_data_csum ^= word;
    }
    sector->data_ok = (sector->data_checksum == calc_data_csum);
    
    return 0;
}

/**
 * @brief Decode DiskSpare sector from MFM
 * 
 * DiskSpare uses a different checksum algorithm than AmigaDOS.
 * 
 * @param mfm MFM bitstream
 * @param marker_pos Marker position
 * @param sector Output sector data
 * @return 0 on success
 */
int amiga_decode_sector_diskspare(
    const uint8_t *mfm,
    int marker_pos,
    amiga_sector_t *sector)
{
    if (!mfm || !sector) return -1;
    
    /* DiskSpare header structure is slightly different */
    int header_offset = marker_pos + 80;  /* After longer sync */
    
    /* Decode header */
    uint8_t header[4];
    amiga_mfm_decode(mfm, header_offset, header, 4);
    
    sector->format = header[0];
    sector->track = header[1];
    sector->sector = header[2];
    sector->sectors_to_gap = header[3];
    
    /* DiskSpare data checksum (different algorithm) */
    int data_offset = header_offset + 128;
    amiga_mfm_decode(mfm, data_offset, sector->data, AMIGA_SECTOR_SIZE);
    
    /* DiskSpare uses XOR checksum of all words, AND with 0x7FFF */
    uint32_t checksum = 0;
    for (int i = 0; i < AMIGA_SECTOR_SIZE; i++) {
        checksum ^= sector->data[i];
    }
    
    /* DiskSpare doesn't have a separate header checksum */
    sector->header_ok = true;  /* Assume valid if we found marker */
    
    /* Read stored checksum */
    uint8_t stored_csum[2];
    amiga_mfm_decode(mfm, header_offset + 64, stored_csum, 2);
    sector->data_checksum = (stored_csum[0] << 8) | stored_csum[1];
    
    sector->data_ok = ((checksum & 0x7FFF) == (sector->data_checksum & 0x7FFF));
    
    return 0;
}

/*============================================================================*
 * AMIGA DISK IMAGE CREATION
 *============================================================================*/

/**
 * @brief Create ADF disk image from decoded sectors
 * 
 * @param sectors Array of decoded sectors
 * @param num_sectors Number of sectors
 * @param format Disk format
 * @param image_out Output image buffer (must be pre-allocated)
 * @param image_size Image buffer size
 * @return Bytes written, or -1 on error
 */
int amiga_create_adf(
    const amiga_sector_t *sectors,
    int num_sectors,
    amiga_format_t format,
    uint8_t *image_out,
    size_t image_size)
{
    if (!sectors || !image_out) return -1;
    
    int sectors_per_track = (format == AMIGA_FORMAT_DISKSPARE) ? 
                            DISKSPARE_SECTORS_PER_TRACK : 
                            AMIGA_SECTORS_PER_TRACK;
    
    size_t expected_size = 80 * 2 * sectors_per_track * AMIGA_SECTOR_SIZE;
    if (image_size < expected_size) return -1;
    
    /* Initialize to zeros */
    memset(image_out, 0, expected_size);
    
    int written = 0;
    
    for (int i = 0; i < num_sectors; i++) {
        const amiga_sector_t *s = &sectors[i];
        
        if (!s->data_ok) continue;  /* Skip bad sectors */
        
        /* Calculate offset in image */
        int track = s->track / 2;
        int head = s->track % 2;
        int sector = s->sector;
        
        size_t offset = ((track * 2 + head) * sectors_per_track + sector) * AMIGA_SECTOR_SIZE;
        
        if (offset + AMIGA_SECTOR_SIZE <= image_size) {
            memcpy(&image_out[offset], s->data, AMIGA_SECTOR_SIZE);
            written++;
        }
    }
    
    return written;
}

/*============================================================================*
 * FORMAT DETECTION
 *============================================================================*/

/**
 * @brief Detect Amiga disk format from MFM data
 * 
 * @param mfm MFM bitstream
 * @param mfm_len MFM length
 * @return Detected format
 */
amiga_format_t amiga_detect_format(const uint8_t *mfm, size_t mfm_len) {
    if (!mfm || mfm_len < 1000) return AMIGA_FORMAT_UNKNOWN;
    
    amiga_marker_t markers[24];
    
    /* Try DiskSpare first (longer marker) */
    int ds_count = amiga_find_markers(mfm, mfm_len, markers, 24, AMIGA_FORMAT_DISKSPARE);
    if (ds_count >= 10) {
        return AMIGA_FORMAT_DISKSPARE;
    }
    
    /* Try AmigaDOS */
    int ados_count = amiga_find_markers(mfm, mfm_len, markers, 24, AMIGA_FORMAT_ADOS);
    if (ados_count >= 10) {
        return AMIGA_FORMAT_ADOS;
    }
    
    return AMIGA_FORMAT_UNKNOWN;
}
