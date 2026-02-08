/**
 * @file uft_msa.c
 * @brief Atari ST MSA format core
 * @version 3.8.0
 */
/*
 * uft_msa.c - Atari ST MSA Format Parser Implementation
 *
 * Part of UnifiedFloppyTool (UFT) v3.3.0
 * Based on msa-to-zip (Olivier Bruchez) - Algorithm extraction
 *
 * MSA uses simple RLE compression:
 *   - Marker byte: $E5
 *   - Format: $E5 <data> <run_hi> <run_lo>
 *   - Runs of 4+ identical bytes are compressed
 *   - Actual $E5 bytes are encoded as $E5 $E5 $00 $01
 */

#include "uft/uft_msa.h"
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper: Read big-endian uint16
 * ═══════════════════════════════════════════════════════════════════════════ */

static inline uint16_t read_be16(const uint8_t *p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

static inline void write_be16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)(v & 0xFF);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Error Strings
 * ═══════════════════════════════════════════════════════════════════════════ */

static const char *msa_error_strings[] = {
    "OK",
    "Null pointer",
    "Invalid MSA signature (expected 0x0E0F)",
    "Invalid geometry",
    "Buffer too small",
    "Truncated data",
    "RLE overflow",
    "Compression failed",
};

const char *uft_msa_strerror(uft_msa_error_t err) {
    if (err < 0 || err >= (int)(sizeof(msa_error_strings) / sizeof(msa_error_strings[0]))) {
        return "Unknown error";
    }
    return msa_error_strings[err];
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Header Parsing
 * ═══════════════════════════════════════════════════════════════════════════ */

uft_msa_error_t uft_msa_parse_header(const uint8_t *data, size_t len,
                                      uft_msa_header_t *header) {
    if (!data || !header) return UFT_MSA_ERR_NULL_PTR;
    if (len < UFT_MSA_HEADER_SIZE) return UFT_MSA_ERR_TRUNCATED_DATA;
    
    header->signature         = read_be16(data + 0);
    header->sectors_per_track = read_be16(data + 2);
    header->sides             = read_be16(data + 4);
    header->start_track       = read_be16(data + 6);
    header->end_track         = read_be16(data + 8);
    
    return UFT_MSA_OK;
}

uft_msa_error_t uft_msa_validate_header(const uft_msa_header_t *header,
                                         uft_msa_info_t *info) {
    if (!header || !info) return UFT_MSA_ERR_NULL_PTR;
    
    /* Check signature */
    if (header->signature != UFT_MSA_SIGNATURE) {
        return UFT_MSA_ERR_INVALID_SIGNATURE;
    }
    
    /* Validate geometry */
    if (header->sectors_per_track < 9 || header->sectors_per_track > UFT_MSA_MAX_SECTORS) {
        return UFT_MSA_ERR_INVALID_GEOMETRY;
    }
    if (header->sides > 1) {
        return UFT_MSA_ERR_INVALID_GEOMETRY;
    }
    if (header->start_track > header->end_track) {
        return UFT_MSA_ERR_INVALID_GEOMETRY;
    }
    if (header->end_track >= UFT_MSA_MAX_TRACKS) {
        return UFT_MSA_ERR_INVALID_GEOMETRY;
    }
    
    /* Fill info structure */
    info->sectors_per_track = header->sectors_per_track;
    info->side_count        = (uint8_t)(header->sides + 1);
    info->start_track       = header->start_track;
    info->end_track         = header->end_track;
    info->track_count       = header->end_track - header->start_track + 1;
    info->raw_size          = (uint32_t)info->track_count * info->side_count * 
                              info->sectors_per_track * UFT_MSA_SECTOR_SIZE;
    
    return UFT_MSA_OK;
}

int uft_msa_probe(const uint8_t *data, size_t len) {
    if (!data || len < UFT_MSA_HEADER_SIZE) return 0;
    
    uft_msa_header_t header;
    uft_msa_info_t info;
    
    if (uft_msa_parse_header(data, len, &header) != UFT_MSA_OK) return 0;
    if (uft_msa_validate_header(&header, &info) != UFT_MSA_OK) return 0;
    
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * RLE Decompression
 * ═══════════════════════════════════════════════════════════════════════════ */

uft_msa_error_t uft_msa_rle_decode(const uint8_t *src, size_t src_len,
                                    uint8_t *dst, size_t dst_len,
                                    size_t *out_written) {
    if (!src || !dst) return UFT_MSA_ERR_NULL_PTR;
    
    size_t src_pos = 0;
    size_t dst_pos = 0;
    
    while (src_pos < src_len) {
        uint8_t byte = src[src_pos++];
        
        if (byte != UFT_MSA_RLE_MARKER) {
            /* Literal byte */
            if (dst_pos >= dst_len) return UFT_MSA_ERR_BUFFER_TOO_SMALL;
            dst[dst_pos++] = byte;
        } else {
            /* RLE sequence: $E5 <data> <run_hi> <run_lo> */
            if (src_pos + 3 > src_len) return UFT_MSA_ERR_TRUNCATED_DATA;
            
            uint8_t data_byte = src[src_pos++];
            uint16_t run_length = read_be16(src + src_pos);
            src_pos += 2;
            
            if (dst_pos + run_length > dst_len) return UFT_MSA_ERR_BUFFER_TOO_SMALL;
            
            memset(dst + dst_pos, data_byte, run_length);
            dst_pos += run_length;
        }
    }
    
    if (out_written) *out_written = dst_pos;
    return UFT_MSA_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * RLE Compression
 * ═══════════════════════════════════════════════════════════════════════════ */

uft_msa_error_t uft_msa_rle_encode(const uint8_t *src, size_t src_len,
                                    uint8_t *dst, size_t dst_len,
                                    size_t *out_written) {
    if (!src || !dst || !out_written) return UFT_MSA_ERR_NULL_PTR;
    
    size_t src_pos = 0;
    size_t dst_pos = 0;
    
    while (src_pos < src_len) {
        uint8_t byte = src[src_pos];
        
        /* Count run length */
        size_t run_start = src_pos;
        while (src_pos < src_len && src[src_pos] == byte) {
            src_pos++;
        }
        size_t run_len = src_pos - run_start;
        
        /* Decide: compress or literal */
        /* MSA compresses runs of 4+ bytes, or any run of $E5 bytes */
        if (run_len >= 4 || (byte == UFT_MSA_RLE_MARKER && run_len >= 1)) {
            /* Compress: $E5 <data> <run_hi> <run_lo> */
            if (dst_pos + 4 > dst_len) return UFT_MSA_ERR_BUFFER_TOO_SMALL;
            
            dst[dst_pos++] = UFT_MSA_RLE_MARKER;
            dst[dst_pos++] = byte;
            write_be16(dst + dst_pos, (uint16_t)run_len);
            dst_pos += 2;
        } else {
            /* Literal */
            if (dst_pos + run_len > dst_len) return UFT_MSA_ERR_BUFFER_TOO_SMALL;
            
            for (size_t i = 0; i < run_len; i++) {
                dst[dst_pos++] = byte;
            }
        }
    }
    
    *out_written = dst_pos;
    return UFT_MSA_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Full Image Conversion: MSA -> ST
 * ═══════════════════════════════════════════════════════════════════════════ */

size_t uft_msa_get_st_size(const uint8_t *msa_data, size_t msa_len) {
    if (!msa_data || msa_len < UFT_MSA_HEADER_SIZE) return 0;
    
    uft_msa_header_t header;
    uft_msa_info_t info;
    
    if (uft_msa_parse_header(msa_data, msa_len, &header) != UFT_MSA_OK) return 0;
    if (uft_msa_validate_header(&header, &info) != UFT_MSA_OK) return 0;
    
    return info.raw_size;
}

uft_msa_error_t uft_msa_to_st(const uint8_t *msa_data, size_t msa_len,
                               uint8_t *st_data, size_t st_len,
                               size_t *out_written,
                               uft_msa_stats_t *stats) {
    if (!msa_data || !st_data) return UFT_MSA_ERR_NULL_PTR;
    
    /* Parse header */
    uft_msa_header_t header;
    uft_msa_error_t err = uft_msa_parse_header(msa_data, msa_len, &header);
    if (err != UFT_MSA_OK) return err;
    
    uft_msa_info_t info;
    err = uft_msa_validate_header(&header, &info);
    if (err != UFT_MSA_OK) return err;
    
    /* Check output buffer size */
    if (st_len < info.raw_size) return UFT_MSA_ERR_BUFFER_TOO_SMALL;
    
    /* Initialize stats */
    if (stats) {
        memset(stats, 0, sizeof(*stats));
        stats->uncompressed_size = info.raw_size;
    }
    
    /* Process tracks */
    size_t msa_pos = UFT_MSA_HEADER_SIZE;
    size_t st_pos = 0;
    uint32_t track_size = (uint32_t)info.sectors_per_track * UFT_MSA_SECTOR_SIZE;
    uint32_t tracks_compressed = 0;
    uint32_t tracks_uncompressed = 0;
    
    for (uint16_t track = info.start_track; track <= info.end_track; track++) {
        for (uint8_t side = 0; side < info.side_count; side++) {
            /* Read track header */
            if (msa_pos + 2 > msa_len) return UFT_MSA_ERR_TRUNCATED_DATA;
            uint16_t data_length = read_be16(msa_data + msa_pos);
            msa_pos += 2;
            
            if (msa_pos + data_length > msa_len) return UFT_MSA_ERR_TRUNCATED_DATA;
            
            if (data_length == track_size) {
                /* Uncompressed track - direct copy */
                memcpy(st_data + st_pos, msa_data + msa_pos, track_size);
                tracks_uncompressed++;
            } else {
                /* Compressed track - decompress */
                size_t written;
                err = uft_msa_rle_decode(msa_data + msa_pos, data_length,
                                         st_data + st_pos, track_size, &written);
                if (err != UFT_MSA_OK) return err;
                
                if (written != track_size) return UFT_MSA_ERR_RLE_OVERFLOW;
                tracks_compressed++;
            }
            
            msa_pos += data_length;
            st_pos += track_size;
        }
    }
    
    /* Fill stats */
    if (stats) {
        stats->compressed_size = (uint32_t)msa_len;
        stats->tracks_compressed = tracks_compressed;
        stats->tracks_uncompressed = tracks_uncompressed;
        if (stats->compressed_size > 0) {
            stats->compression_ratio = (float)stats->uncompressed_size / 
                                       (float)stats->compressed_size;
        }
    }
    
    if (out_written) *out_written = st_pos;
    return UFT_MSA_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Full Image Conversion: ST -> MSA
 * ═══════════════════════════════════════════════════════════════════════════ */

uft_msa_error_t uft_st_to_msa(const uint8_t *st_data, size_t st_len,
                               const uft_msa_info_t *info,
                               uint8_t *msa_data, size_t msa_len,
                               size_t *out_written,
                               uft_msa_stats_t *stats) {
    if (!st_data || !info || !msa_data) return UFT_MSA_ERR_NULL_PTR;
    
    uint32_t track_size = (uint32_t)info->sectors_per_track * UFT_MSA_SECTOR_SIZE;
    uint32_t expected_st_size = (uint32_t)info->track_count * info->side_count * track_size;
    
    if (st_len < expected_st_size) return UFT_MSA_ERR_TRUNCATED_DATA;
    if (msa_len < UFT_MSA_HEADER_SIZE) return UFT_MSA_ERR_BUFFER_TOO_SMALL;
    
    /* Initialize stats */
    if (stats) {
        memset(stats, 0, sizeof(*stats));
        stats->uncompressed_size = expected_st_size;
    }
    
    /* Write MSA header */
    size_t msa_pos = 0;
    write_be16(msa_data + msa_pos, UFT_MSA_SIGNATURE); msa_pos += 2;
    write_be16(msa_data + msa_pos, info->sectors_per_track); msa_pos += 2;
    write_be16(msa_data + msa_pos, info->side_count - 1); msa_pos += 2;
    write_be16(msa_data + msa_pos, info->start_track); msa_pos += 2;
    write_be16(msa_data + msa_pos, info->end_track); msa_pos += 2;
    
    /* Process tracks */
    size_t st_pos = 0;
    uint32_t tracks_compressed = 0;
    uint32_t tracks_uncompressed = 0;
    
    /* Temporary buffer for compressed track */
    uint8_t comp_buf[UFT_MSA_MAX_SECTORS * UFT_MSA_SECTOR_SIZE + 256];
    
    for (uint16_t track = info->start_track; track <= info->end_track; track++) {
        for (uint8_t side = 0; side < info->side_count; side++) {
            /* Try to compress track */
            size_t comp_len;
            uft_msa_error_t err = uft_msa_rle_encode(st_data + st_pos, track_size,
                                                      comp_buf, sizeof(comp_buf),
                                                      &comp_len);
            
            /* Use compressed if smaller, otherwise uncompressed */
            int use_compressed = (err == UFT_MSA_OK && comp_len < track_size);
            
            uint16_t data_length = use_compressed ? (uint16_t)comp_len : (uint16_t)track_size;
            
            /* Check buffer space */
            if (msa_pos + 2 + data_length > msa_len) return UFT_MSA_ERR_BUFFER_TOO_SMALL;
            
            /* Write track header */
            write_be16(msa_data + msa_pos, data_length);
            msa_pos += 2;
            
            /* Write track data */
            if (use_compressed) {
                memcpy(msa_data + msa_pos, comp_buf, comp_len);
                tracks_compressed++;
            } else {
                memcpy(msa_data + msa_pos, st_data + st_pos, track_size);
                tracks_uncompressed++;
            }
            
            msa_pos += data_length;
            st_pos += track_size;
        }
    }
    
    /* Fill stats */
    if (stats) {
        stats->compressed_size = (uint32_t)msa_pos;
        stats->tracks_compressed = tracks_compressed;
        stats->tracks_uncompressed = tracks_uncompressed;
        if (stats->compressed_size > 0) {
            stats->compression_ratio = (float)stats->uncompressed_size / 
                                       (float)stats->compressed_size;
        }
    }
    
    if (out_written) *out_written = msa_pos;
    return UFT_MSA_OK;
}
