/**
 * @file uft_bitstream_preserve.c
 * @brief UFT Bitstream Preservation Layer Implementation
 * 
 * @version 3.2.1
 * @date 2026-01-05
 * 
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 UnifiedFloppyTool Contributors
 * 
 * PERF-FIX: P2-PERF-001 - Buffered I/O statt byte-by-byte writes
 */

#include "uft/uft_bitstream_preserve.h"
#include "uft/uft_safe_io.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * CRC32 Implementation (IEEE 802.3 polynomial)
 * ═══════════════════════════════════════════════════════════════════════════ */

static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
    0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
    0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
    0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
    0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
    0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
    0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
    0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
    0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106,
    0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
    0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
    0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
    0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
    0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
    0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C, 0x270241AA,
    0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
    0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
    0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84,
    0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
    0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
    0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
    0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55,
    0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
    0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
    0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
    0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
    0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
    0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69,
    0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
    0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
    0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD706B3,
    0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
    0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Error Messages
 * ═══════════════════════════════════════════════════════════════════════════ */

static const char* bp_error_messages[] = {
    "Success",
    "Null pointer",
    "Invalid size",
    "Buffer overflow",
    "Checksum mismatch",
    "Data corrupted",
    "Out of memory",
    "Format error",
    "Version mismatch",
    "Feature unsupported",
    "I/O error"
};

const char* uft_bp_strerror(uft_bp_status_t status) {
    if (status >= 0) return bp_error_messages[0];
    int idx = -status;
    if (idx >= (int)(sizeof(bp_error_messages) / sizeof(bp_error_messages[0]))) {
        return "Unknown error";
    }
    return bp_error_messages[idx];
}

/* ═══════════════════════════════════════════════════════════════════════════
 * CRC32 & SHA256
 * ═══════════════════════════════════════════════════════════════════════════ */

uint32_t uft_bp_crc32(const uint8_t* data, size_t len) {
    if (!data || len == 0) return 0;
    
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

/* Simple SHA256 implementation for integrity checking */
/* In production, use a proper crypto library */
void uft_bp_sha256(const uint8_t* data, size_t len, uint8_t* out_hash) {
    if (!data || !out_hash) return;
    
    /* Placeholder: XOR-based hash for demo (replace with real SHA256) */
    memset(out_hash, 0, 32);
    for (size_t i = 0; i < len; i++) {
        out_hash[i % 32] ^= data[i];
        out_hash[(i + 13) % 32] ^= (data[i] >> 4) | (data[i] << 4);
    }
    
    /* Mix */
    for (int round = 0; round < 4; round++) {
        for (int i = 0; i < 31; i++) {
            out_hash[i] ^= out_hash[i + 1];
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Track Operations
 * ═══════════════════════════════════════════════════════════════════════════ */

uft_bp_status_t uft_bp_track_init(
    uft_preserved_track_t* track,
    uint8_t cylinder,
    uint8_t head,
    uint8_t flags
) {
    if (!track) return UFT_BP_ERR_NULL_PTR;
    
    memset(track, 0, sizeof(*track));
    track->cylinder = cylinder;
    track->head = head;
    track->preserve_flags = flags;
    track->capture_time = time(NULL);
    snprintf(track->software_version, sizeof(track->software_version), "UFT-3.2.0");
    
    return UFT_BP_OK;
}

void uft_bp_track_free(uft_preserved_track_t* track) {
    if (!track) return;
    
    /* Free revolution bitstreams if we own them */
    if (track->_owns_bitstreams) {
        for (uint8_t i = 0; i < track->revolution_count; i++) {
            free(track->revolutions[i].bitstream);
            track->revolutions[i].bitstream = NULL;
        }
    }
    
    /* Free fused bitstream */
    free(track->fused_bitstream);
    track->fused_bitstream = NULL;
    
    /* Free timing deltas if we own them */
    if (track->_owns_timing) {
        free(track->timing_deltas);
        track->timing_deltas = NULL;
    }
    
    memset(track, 0, sizeof(*track));
}

int uft_bp_track_add_revolution(
    uft_preserved_track_t* track,
    const uint8_t* bitstream,
    uint32_t bit_count,
    bool copy
) {
    if (!track || !bitstream) return UFT_BP_ERR_NULL_PTR;
    if (track->revolution_count >= UFT_MAX_REVOLUTIONS) return UFT_BP_ERR_OVERFLOW;
    if (bit_count > UFT_MAX_TRACK_BITS) return UFT_BP_ERR_INVALID_SIZE;
    
    uint8_t idx = track->revolution_count;
    uft_revolution_data_t* rev = &track->revolutions[idx];
    
    size_t byte_count = (bit_count + 7) / 8;
    
    if (copy) {
        rev->bitstream = (uint8_t*)malloc(byte_count);
        if (!rev->bitstream) return UFT_BP_ERR_NO_MEMORY;
        memcpy(rev->bitstream, bitstream, byte_count);
        track->_owns_bitstreams = 1;
    } else {
        rev->bitstream = (uint8_t*)bitstream;  /* Take ownership */
    }
    
    rev->bit_count = bit_count;
    rev->crc32 = uft_bp_crc32(rev->bitstream, byte_count);
    rev->quality_score = 100;  /* Default, will be refined */
    
    track->revolution_count++;
    return idx;
}

uft_bp_status_t uft_bp_track_mark_weak(
    uft_preserved_track_t* track,
    uint32_t start_bit,
    uint32_t length_bits,
    uint8_t confidence
) {
    if (!track) return UFT_BP_ERR_NULL_PTR;
    if (track->weak_region_count >= UFT_MAX_WEAK_REGIONS) return UFT_BP_ERR_OVERFLOW;
    
    uft_weak_region_t* region = &track->weak_regions[track->weak_region_count];
    region->start_bit = start_bit;
    region->length_bits = length_bits;
    region->confidence = confidence;
    region->pattern_type = 0;  /* Random */
    region->revolution_variance = 0;
    region->occurrence_mask = 0xFFFF;  /* All revolutions */
    
    track->weak_region_count++;
    return UFT_BP_OK;
}

uft_bp_status_t uft_bp_track_add_timing(
    uft_preserved_track_t* track,
    uint32_t bit_position,
    int16_t delta_ns,
    uint8_t revolution
) {
    if (!track) return UFT_BP_ERR_NULL_PTR;
    
    /* Grow timing array if needed */
    uint32_t new_count = track->timing_delta_count + 1;
    uft_timing_delta_t* new_array = (uft_timing_delta_t*)realloc(
        track->timing_deltas, 
        new_count * sizeof(uft_timing_delta_t)
    );
    if (!new_array) return UFT_BP_ERR_NO_MEMORY;
    
    track->timing_deltas = new_array;
    track->_owns_timing = 1;
    
    uft_timing_delta_t* delta = &track->timing_deltas[track->timing_delta_count];
    delta->bit_position = bit_position;
    delta->delta_ns = delta_ns;
    delta->flags = 0;
    delta->source_revolution = revolution;
    
    track->timing_delta_count = new_count;
    return UFT_BP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Multi-Revolution Fusion
 * ═══════════════════════════════════════════════════════════════════════════ */

uft_bp_status_t uft_bp_fuse_revolutions(uft_preserved_track_t* track) {
    if (!track) return UFT_BP_ERR_NULL_PTR;
    if (track->revolution_count == 0) return UFT_BP_ERR_INVALID_SIZE;
    
    /* Find minimum bit count across revolutions */
    uint32_t min_bits = track->revolutions[0].bit_count;
    for (uint8_t i = 1; i < track->revolution_count; i++) {
        if (track->revolutions[i].bit_count < min_bits) {
            min_bits = track->revolutions[i].bit_count;
        }
    }
    
    size_t byte_count = (min_bits + 7) / 8;
    
    /* Allocate fused output */
    free(track->fused_bitstream);
    track->fused_bitstream = (uint8_t*)calloc(byte_count, 1);
    if (!track->fused_bitstream) return UFT_BP_ERR_NO_MEMORY;
    
    track->fused_bit_count = min_bits;
    
    /* Voting algorithm: majority wins */
    uint32_t confident_bits = 0;
    
    for (uint32_t bit = 0; bit < min_bits; bit++) {
        int ones = 0;
        int zeros = 0;
        
        for (uint8_t rev = 0; rev < track->revolution_count; rev++) {
            if (uft_bp_get_bit_raw(track->revolutions[rev].bitstream, bit)) {
                ones++;
            } else {
                zeros++;
            }
        }
        
        uint8_t result = (ones > zeros) ? 1 : 0;
        uft_bp_set_bit_raw(track->fused_bitstream, bit, result);
        
        /* Track confidence */
        int total = ones + zeros;
        int majority = (ones > zeros) ? ones : zeros;
        if (majority == total) {
            confident_bits++;
        }
    }
    
    /* Calculate overall confidence */
    track->fused_confidence = (uint8_t)((confident_bits * 100) / min_bits);
    
    /* Mark best revolution (one with most matches to fused result) */
    int best_match = 0;
    track->best_revolution = 0;
    
    for (uint8_t rev = 0; rev < track->revolution_count; rev++) {
        int matches = 0;
        for (uint32_t bit = 0; bit < min_bits; bit++) {
            if (uft_bp_get_bit_raw(track->revolutions[rev].bitstream, bit) ==
                uft_bp_get_bit_raw(track->fused_bitstream, bit)) {
                matches++;
            }
        }
        if (matches > best_match) {
            best_match = matches;
            track->best_revolution = rev;
        }
    }
    
    return UFT_BP_OK;
}

uft_bp_status_t uft_bp_get_bit(
    const uft_preserved_track_t* track,
    uint32_t bit_position,
    uint8_t* value,
    uint8_t* confidence
) {
    if (!track || !value || !confidence) return UFT_BP_ERR_NULL_PTR;
    
    if (track->fused_bitstream && bit_position < track->fused_bit_count) {
        *value = uft_bp_get_bit_raw(track->fused_bitstream, bit_position);
        
        /* Calculate per-bit confidence */
        int ones = 0;
        for (uint8_t rev = 0; rev < track->revolution_count; rev++) {
            if (bit_position < track->revolutions[rev].bit_count) {
                if (uft_bp_get_bit_raw(track->revolutions[rev].bitstream, bit_position)) {
                    ones++;
                }
            }
        }
        int agree = (*value) ? ones : (track->revolution_count - ones);
        *confidence = (uint8_t)((agree * 100) / track->revolution_count);
        
        return UFT_BP_OK;
    }
    
    /* Fall back to best revolution */
    if (track->revolution_count > 0) {
        const uft_revolution_data_t* rev = &track->revolutions[track->best_revolution];
        if (bit_position < rev->bit_count) {
            *value = uft_bp_get_bit_raw(rev->bitstream, bit_position);
            *confidence = rev->quality_score;
            return UFT_BP_OK;
        }
    }
    
    return UFT_BP_ERR_INVALID_SIZE;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Integrity Verification
 * ═══════════════════════════════════════════════════════════════════════════ */

uft_bp_status_t uft_bp_verify_track(const uft_preserved_track_t* track) {
    if (!track) return UFT_BP_ERR_NULL_PTR;
    
    for (uint8_t i = 0; i < track->revolution_count; i++) {
        const uft_revolution_data_t* rev = &track->revolutions[i];
        if (!rev->bitstream) continue;
        
        size_t byte_count = (rev->bit_count + 7) / 8;
        uint32_t computed = uft_bp_crc32(rev->bitstream, byte_count);
        
        if (computed != rev->crc32) {
            return UFT_BP_ERR_CHECKSUM;
        }
    }
    
    return UFT_BP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Bitstream Comparison
 * ═══════════════════════════════════════════════════════════════════════════ */

uint32_t uft_bp_compare_bitstreams(
    const uint8_t* a,
    const uint8_t* b,
    uint32_t bit_count,
    int32_t* first_diff
) {
    if (!a || !b) return 0;
    
    uint32_t differences = 0;
    if (first_diff) *first_diff = -1;
    
    for (uint32_t i = 0; i < bit_count; i++) {
        uint8_t bit_a = uft_bp_get_bit_raw(a, i);
        uint8_t bit_b = uft_bp_get_bit_raw(b, i);
        
        if (bit_a != bit_b) {
            differences++;
            if (first_diff && *first_diff < 0) {
                *first_diff = (int32_t)i;
            }
        }
    }
    
    return differences;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Disk Operations
 * ═══════════════════════════════════════════════════════════════════════════ */

uft_preserved_disk_t* uft_bp_disk_create(uint8_t cylinders, uint8_t heads) {
    if (cylinders == 0 || heads == 0 || heads > 2) return NULL;
    
    uft_preserved_disk_t* disk = (uft_preserved_disk_t*)calloc(1, sizeof(*disk));
    if (!disk) return NULL;
    
    disk->cylinders = cylinders;
    disk->heads = heads;
    disk->track_count = cylinders * heads;
    
    disk->tracks = (uft_preserved_track_t**)calloc(disk->track_count, sizeof(void*));
    if (!disk->tracks) {
        free(disk);
        return NULL;
    }
    
    disk->preservation_time = time(NULL);
    
    return disk;
}

void uft_bp_disk_free(uft_preserved_disk_t* disk) {
    if (!disk) return;
    
    if (disk->tracks) {
        for (uint32_t i = 0; i < disk->track_count; i++) {
            if (disk->tracks[i]) {
                uft_bp_track_free(disk->tracks[i]);
                free(disk->tracks[i]);
            }
        }
        free(disk->tracks);
    }
    
    free(disk);
}

uft_preserved_track_t* uft_bp_disk_get_track(
    uft_preserved_disk_t* disk,
    uint8_t cylinder,
    uint8_t head
) {
    if (!disk || cylinder >= disk->cylinders || head >= disk->heads) return NULL;
    
    uint32_t idx = cylinder * disk->heads + head;
    
    if (!disk->tracks[idx]) {
        disk->tracks[idx] = (uft_preserved_track_t*)calloc(1, sizeof(uft_preserved_track_t));
        if (disk->tracks[idx]) {
            uft_bp_track_init(disk->tracks[idx], cylinder, head, UFT_PRESERVE_FULL);
        }
    }
    
    return disk->tracks[idx];
}

uft_bp_status_t uft_bp_disk_finalize(uft_preserved_disk_t* disk, uint8_t type) {
    if (!disk) return UFT_BP_ERR_NULL_PTR;
    
    disk->global_checksum_type = type;
    
    /* Hash all track CRCs together */
    uint8_t hash_input[4096];
    size_t hash_len = 0;
    
    for (uint32_t i = 0; i < disk->track_count && hash_len < sizeof(hash_input) - 4; i++) {
        if (disk->tracks[i] && disk->tracks[i]->revolution_count > 0) {
            uint32_t crc = disk->tracks[i]->revolutions[0].crc32;
            memcpy(hash_input + hash_len, &crc, 4);
            hash_len += 4;
        }
    }
    
    if (type == UFT_CHECKSUM_CRC32) {
        uint32_t global_crc = uft_bp_crc32(hash_input, hash_len);
        memcpy(disk->global_checksum, &global_crc, 4);
    } else if (type == UFT_CHECKSUM_SHA256) {
        uft_bp_sha256(hash_input, hash_len, disk->global_checksum);
    }
    
    return UFT_BP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * File I/O (UFT Native Format)
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_PRESERVE_MAGIC  0x55465450  /* "UFTP" */
#define UFT_PRESERVE_VERSION 0x0100

/**
 * @brief Save preserved disk to file (optimized with buffered I/O)
 * 
 * PERF-FIX: P2-PERF-001 - Uses uft_buf_writer_t instead of 26 individual fwrite calls
 * Before: 26+ syscalls per track, ~4000+ syscalls for 80-track disk
 * After:  ~80 syscalls total (one flush per track + header)
 */
uft_bp_status_t uft_bp_disk_save(const uft_preserved_disk_t* disk, const char* filename) {
    if (!disk || !filename) return UFT_BP_ERR_NULL_PTR;
    
    FILE* f = fopen(filename, "wb");
    if (!f) return UFT_BP_ERR_IO;
    
    /* Initialize buffered writer */
    uft_buf_writer_t writer;
    if (uft_buf_writer_init(&writer, f) != UFT_IO_OK) {
        fclose(f);
        return UFT_BP_ERR_IO;
    }
    
    /* Write header - all buffered */
    uint32_t magic = UFT_PRESERVE_MAGIC;
    uint16_t version = UFT_PRESERVE_VERSION;
    
    uft_buf_writer_u32(&writer, magic);
    uft_buf_writer_u16(&writer, version);
    uft_buf_writer_u8(&writer, disk->cylinders);
    uft_buf_writer_u8(&writer, disk->heads);
    uft_buf_writer_u32(&writer, disk->track_count);
    uft_buf_writer_bytes(&writer, disk->disk_label, 64);
    uft_buf_writer_bytes(&writer, disk->source_format, 16);
    uft_buf_writer_bytes(&writer, disk->source_file, 256);
    uft_buf_writer_bytes(&writer, &disk->preservation_time, sizeof(time_t));
    uft_buf_writer_u8(&writer, disk->global_checksum_type);
    uft_buf_writer_bytes(&writer, disk->global_checksum, 32);
    
    /* Write tracks */
    for (uint32_t i = 0; i < disk->track_count; i++) {
        uft_preserved_track_t* track = disk->tracks[i];
        uint8_t has_track = (track != NULL) ? 1 : 0;
        uft_buf_writer_u8(&writer, has_track);
        
        if (!track) continue;
        
        /* Track header - buffered */
        uft_buf_writer_u8(&writer, track->cylinder);
        uft_buf_writer_u8(&writer, track->head);
        uft_buf_writer_u8(&writer, track->format_type);
        uft_buf_writer_u8(&writer, track->preserve_flags);
        uft_buf_writer_u8(&writer, track->revolution_count);
        uft_buf_writer_u8(&writer, track->best_revolution);
        uft_buf_writer_u16(&writer, track->weak_region_count);
        
        /* Revolutions */
        for (uint8_t r = 0; r < track->revolution_count; r++) {
            uft_revolution_data_t* rev = &track->revolutions[r];
            uft_buf_writer_u32(&writer, rev->bit_count);
            uft_buf_writer_u32(&writer, rev->crc32);
            uft_buf_writer_u8(&writer, rev->quality_score);
            
            /* Bitstream data - may be large, buffer handles it */
            size_t bytes = (rev->bit_count + 7) / 8;
            uft_buf_writer_bytes(&writer, rev->bitstream, bytes);
        }
        
        /* Weak regions */
        if (track->weak_region_count > 0) {
            uft_buf_writer_bytes(&writer, track->weak_regions, 
                                 sizeof(uft_weak_region_t) * track->weak_region_count);
        }
    }
    
    /* Final flush */
    uft_io_error_t err = uft_buf_writer_flush(&writer);
    fclose(f);
    
    return (err == UFT_IO_OK) ? UFT_BP_OK : UFT_BP_ERR_IO;
}

/**
 * @brief Load preserved disk from file (optimized with buffered I/O)
 * 
 * PERF-FIX: P2-PERF-001 - Uses uft_buf_reader_t instead of individual fread calls
 */
uft_preserved_disk_t* uft_bp_disk_load(const char* filename) {
    if (!filename) return NULL;
    
    FILE* f = fopen(filename, "rb");
    if (!f) return NULL;
    
    /* Initialize buffered reader */
    uft_buf_reader_t reader;
    if (uft_buf_reader_init(&reader, f) != UFT_IO_OK) {
        fclose(f);
        return NULL;
    }
    
    /* Read header */
    uint32_t magic;
    uint16_t version;
    uint8_t cylinders, heads;
    uint32_t track_count;
    
    if (uft_buf_reader_u32(&reader, &magic) != UFT_IO_OK || magic != UFT_PRESERVE_MAGIC) {
        fclose(f);
        return NULL;
    }
    
    if (uft_buf_reader_u16(&reader, &version) != UFT_IO_OK || version > UFT_PRESERVE_VERSION) {
        fclose(f);
        return NULL;
    }
    
    uft_buf_reader_u8(&reader, &cylinders);
    uft_buf_reader_u8(&reader, &heads);
    uft_buf_reader_u32(&reader, &track_count);
    
    uft_preserved_disk_t* disk = uft_bp_disk_create(cylinders, heads);
    if (!disk) {
        fclose(f);
        return NULL;
    }
    
    uft_buf_reader_bytes(&reader, disk->disk_label, 64);
    uft_buf_reader_bytes(&reader, disk->source_format, 16);
    uft_buf_reader_bytes(&reader, disk->source_file, 256);
    uft_buf_reader_bytes(&reader, &disk->preservation_time, sizeof(time_t));
    uft_buf_reader_u8(&reader, &disk->global_checksum_type);
    uft_buf_reader_bytes(&reader, disk->global_checksum, 32);
    
    /* Read tracks */
    for (uint32_t i = 0; i < track_count; i++) {
        uint8_t has_track;
        if (uft_buf_reader_u8(&reader, &has_track) != UFT_IO_OK || !has_track) continue;
        
        uint8_t cyl, head, format_type, flags, rev_count, best_rev;
        uint16_t weak_count;
        
        uft_buf_reader_u8(&reader, &cyl);
        uft_buf_reader_u8(&reader, &head);
        uft_buf_reader_u8(&reader, &format_type);
        uft_buf_reader_u8(&reader, &flags);
        uft_buf_reader_u8(&reader, &rev_count);
        uft_buf_reader_u8(&reader, &best_rev);
        uft_buf_reader_u16(&reader, &weak_count);
        
        uft_preserved_track_t* track = uft_bp_disk_get_track(disk, cyl, head);
        if (!track) continue;
        
        track->format_type = format_type;
        track->preserve_flags = flags;
        track->best_revolution = best_rev;
        
        /* Read revolutions */
        for (uint8_t r = 0; r < rev_count; r++) {
            uint32_t bit_count, crc32;
            uint8_t quality;
            
            uft_buf_reader_u32(&reader, &bit_count);
            uft_buf_reader_u32(&reader, &crc32);
            uft_buf_reader_u8(&reader, &quality);
            
            size_t bytes = (bit_count + 7) / 8;
            uint8_t* bitstream = (uint8_t*)malloc(bytes);
            if (bitstream && uft_buf_reader_bytes(&reader, bitstream, bytes) == UFT_IO_OK) {
                int idx = uft_bp_track_add_revolution(track, bitstream, bit_count, false);
                if (idx >= 0) {
                    track->revolutions[idx].crc32 = crc32;
                    track->revolutions[idx].quality_score = quality;
                }
            } else {
                free(bitstream);
            }
        }
        
        /* Read weak regions */
        if (weak_count > 0 && weak_count <= UFT_MAX_WEAK_REGIONS) {
            uft_buf_reader_bytes(&reader, track->weak_regions, 
                                 sizeof(uft_weak_region_t) * weak_count);
            track->weak_region_count = weak_count;
        }
    }
    
    fclose(f);
    return disk;
}
