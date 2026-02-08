/**
 * @file uft_scp_hardened_impl.c
 * @brief Security-Hardened SCP Parser - Full Implementation
 */

#include "uft/core/uft_safe_math.h"
#include "uft/formats/scp_hardened.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

// ============================================================================
// INTERNAL STRUCTURE (opaque)
// ============================================================================

struct uft_scp_image_hardened {
    FILE               *f;
    size_t              file_size;
    uft_scp_header_t    hdr;
    uint32_t            track_offsets[UFT_SCP_MAX_TRACK_ENTRIES];
    bool                extended_mode;
    bool                validated;
    bool                closed;
};

// ============================================================================
// SAFE ARITHMETIC
// ============================================================================

static inline bool safe_add_u32(uint32_t a, uint32_t b, uint32_t *result) {
    if (a > UINT32_MAX - b) return false;
    *result = a + b;
    return true;
}

static inline bool safe_mul_size(size_t a, size_t b, size_t *result) {
    if (a != 0 && b > SIZE_MAX / a) return false;
    *result = a * b;
    return true;
}

// ============================================================================
// ENDIANNESS
// ============================================================================

static inline uint32_t rd_u32_le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static inline uint16_t rd_u16_be(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] << 8) | (uint16_t)p[1];
}

// ============================================================================
// FILE HELPERS
// ============================================================================

static size_t get_file_size(FILE *f) {
    long current = ftell(f);
    if (current < 0) return 0;
    if (fseek(f, 0, SEEK_END) != 0) return 0;
    long end = ftell(f);
    if (fseek(f, current, SEEK_SET) != 0) { /* seek error */ }
    return (end < 0) ? 0 : (size_t)end;
}

static bool fread_exact(FILE *f, void *buf, size_t n) {
    return fread(buf, 1, n, f) == n;
}

static int safe_fseek(FILE *f, size_t file_size, uint32_t offset) {
    if ((size_t)offset >= file_size) return -1;
    return (void)fseek(f, (long)offset, SEEK_SET);
}

// ============================================================================
// ERROR STRINGS
// ============================================================================

const char *uft_scp_error_string(uft_scp_error_t err) {
    switch (err) {
        case UFT_SCP_OK:        return "Success";
        case UFT_SCP_EINVAL:    return "Invalid argument";
        case UFT_SCP_EIO:       return "I/O error";
        case UFT_SCP_EFORMAT:   return "Invalid format";
        case UFT_SCP_EBOUNDS:   return "Out of bounds";
        case UFT_SCP_ENOMEM:    return "Out of memory";
        case UFT_SCP_EOVERFLOW: return "Integer overflow";
        default:                return "Unknown error";
    }
}

// ============================================================================
// OPEN
// ============================================================================

uft_scp_error_t uft_scp_open_safe(const char *path, 
                                   uft_scp_image_hardened_t **img_out) {
    if (!path || !img_out) {
        return UFT_SCP_EINVAL;
    }
    
    *img_out = NULL;
    
    // Allocate handle
    uft_scp_image_hardened_t *img = calloc(1, sizeof(*img));
    if (!img) {
        return UFT_SCP_ENOMEM;
    }
    
    // Open file
    img->f = fopen(path, "rb");
    if (!img->f) {
        free(img);
        return UFT_SCP_EIO;
    }
    
    // Get file size
    img->file_size = get_file_size(img->f);
    if (img->file_size == 0) {
        fclose(img->f);
        free(img);
        return UFT_SCP_EIO;
    }
    
    // Validate file size
    if (img->file_size < sizeof(uft_scp_header_t)) {
        fclose(img->f);
        free(img);
        return UFT_SCP_EFORMAT;
    }
    
    if (img->file_size > UFT_SCP_MAX_FILE_SIZE) {
        fclose(img->f);
        free(img);
        return UFT_SCP_EBOUNDS;
    }
    
    // Read header
    if (!fread_exact(img->f, &img->hdr, sizeof(img->hdr))) {
        fclose(img->f);
        free(img);
        return UFT_SCP_EIO;
    }
    
    // Validate magic
    if (memcmp(img->hdr.signature, "SCP", 3) != 0) {
        fclose(img->f);
        free(img);
        return UFT_SCP_EFORMAT;
    }
    
    // Validate revolutions
    if (img->hdr.num_revs > UFT_SCP_MAX_REVOLUTIONS) {
        fclose(img->f);
        free(img);
        return UFT_SCP_EBOUNDS;
    }
    
    img->extended_mode = (img->hdr.flags & 0x40u) != 0;
    
    // Convert track offsets from LE
    for (size_t i = 0; i < UFT_SCP_MAX_TRACK_ENTRIES; i++) {
        uint8_t *p = (uint8_t*)&img->hdr.track_offsets[i];
        img->track_offsets[i] = rd_u32_le(p);
    }
    
    // Extended mode: read extended offset table
    if (img->extended_mode) {
        size_t ext_table_end = 0x80 + sizeof(img->track_offsets);
        if (ext_table_end > img->file_size) {
            fclose(img->f);
            free(img);
            return UFT_SCP_EFORMAT;
        }
        
        if (fseek(img->f, 0x80, SEEK_SET) != 0) {
            fclose(img->f);
            free(img);
            return UFT_SCP_EIO;
        }
        
        uint8_t tmp[UFT_SCP_MAX_TRACK_ENTRIES * 4];
        if (!fread_exact(img->f, tmp, sizeof(tmp))) {
            fclose(img->f);
            free(img);
            return UFT_SCP_EIO;
        }
        
        for (size_t i = 0; i < UFT_SCP_MAX_TRACK_ENTRIES; i++) {
            img->track_offsets[i] = rd_u32_le(&tmp[i * 4]);
        }
    }
    
    // Validate all non-zero offsets are within file
    for (size_t i = 0; i < UFT_SCP_MAX_TRACK_ENTRIES; i++) {
        uint32_t off = img->track_offsets[i];
        if (off != 0 && (size_t)off >= img->file_size) {
            fclose(img->f);
            free(img);
            return UFT_SCP_EFORMAT;
        }
    }
    
    img->validated = true;
    img->closed = false;
    *img_out = img;
    
    return UFT_SCP_OK;
}

// ============================================================================
// CLOSE
// ============================================================================

void uft_scp_close_safe(uft_scp_image_hardened_t **img) {
    if (!img || !*img) return;
    
    uft_scp_image_hardened_t *p = *img;
    p->closed = true;
    
    if (p->f) {
        fclose(p->f);
        p->f = NULL;
    }
    
    memset(p, 0, sizeof(*p));
    free(p);
    *img = NULL;
}

// ============================================================================
// GETTERS
// ============================================================================

uft_scp_error_t uft_scp_get_header(const uft_scp_image_hardened_t *img,
                                    uft_scp_header_t *hdr_out) {
    if (!img || !hdr_out) return UFT_SCP_EINVAL;
    if (!img->validated || img->closed) return UFT_SCP_EINVAL;
    
    *hdr_out = img->hdr;
    return UFT_SCP_OK;
}

size_t uft_scp_get_file_size(const uft_scp_image_hardened_t *img) {
    return (img && img->validated && !img->closed) ? img->file_size : 0;
}

bool uft_scp_is_valid(const uft_scp_image_hardened_t *img) {
    return img && img->validated && !img->closed && img->f;
}

uft_scp_error_t uft_scp_get_track_info_safe(const uft_scp_image_hardened_t *img,
                                             uint8_t track_index,
                                             uft_scp_track_info_t *info_out) {
    if (!img || !info_out) return UFT_SCP_EINVAL;
    if (!img->validated || img->closed) return UFT_SCP_EINVAL;
    if (track_index >= UFT_SCP_MAX_TRACK_ENTRIES) return UFT_SCP_EBOUNDS;
    
    memset(info_out, 0, sizeof(*info_out));
    info_out->track_index = track_index;
    info_out->file_offset = img->track_offsets[track_index];
    info_out->present = (info_out->file_offset != 0) ? 1 : 0;
    info_out->num_revs = img->hdr.num_revs;
    
    if (!info_out->present) {
        return UFT_SCP_OK;
    }
    
    // Read track header to get track number
    if (safe_fseek(img->f, img->file_size, info_out->file_offset) != 0) {
        return UFT_SCP_EIO;
    }
    
    uft_scp_track_header_t trk;
    if (!fread_exact(img->f, &trk, sizeof(trk))) {
        return UFT_SCP_EIO;
    }
    
    if (memcmp(trk.signature, "TRK", 3) != 0) {
        return UFT_SCP_EFORMAT;
    }
    
    info_out->track_number = trk.track_number;
    return UFT_SCP_OK;
}

// ============================================================================
// READ REVOLUTIONS
// ============================================================================

uft_scp_error_t uft_scp_read_revolutions_safe(const uft_scp_image_hardened_t *img,
                                               uint8_t track_index,
                                               uft_scp_track_rev_t *revs,
                                               size_t revs_cap,
                                               size_t *out_count) {
    if (!img || !revs) return UFT_SCP_EINVAL;
    if (!img->validated || img->closed) return UFT_SCP_EINVAL;
    if (track_index >= UFT_SCP_MAX_TRACK_ENTRIES) return UFT_SCP_EBOUNDS;
    if (revs_cap < img->hdr.num_revs) return UFT_SCP_EBOUNDS;
    
    uint32_t off = img->track_offsets[track_index];
    if (off == 0) return UFT_SCP_EFORMAT;
    
    // Validate we can read track header + all revolution entries
    size_t needed = sizeof(uft_scp_track_header_t) + 
                    (size_t)img->hdr.num_revs * 12;
    if ((size_t)off + needed > img->file_size) {
        return UFT_SCP_EBOUNDS;
    }
    
    if (safe_fseek(img->f, img->file_size, off) != 0) {
        return UFT_SCP_EIO;
    }
    
    // Read track header
    uft_scp_track_header_t trk;
    if (!fread_exact(img->f, &trk, sizeof(trk))) {
        return UFT_SCP_EIO;
    }
    
    if (memcmp(trk.signature, "TRK", 3) != 0) {
        return UFT_SCP_EFORMAT;
    }
    
    // Read revolution entries
    for (size_t i = 0; i < img->hdr.num_revs; i++) {
        uint8_t raw[12];
        if (!fread_exact(img->f, raw, sizeof(raw))) {
            return UFT_SCP_EIO;
        }
        
        revs[i].time_duration = rd_u32_le(&raw[0]);
        revs[i].data_length   = rd_u32_le(&raw[4]);
        revs[i].data_offset   = rd_u32_le(&raw[8]);
        
        // Validate
        if (revs[i].data_length > UFT_SCP_MAX_FLUX_PER_REV) {
            return UFT_SCP_EBOUNDS;
        }
        
        uint32_t data_abs;
        if (!safe_add_u32(off, revs[i].data_offset, &data_abs)) {
            return UFT_SCP_EOVERFLOW;
        }
        
        size_t data_size;
        if (!safe_mul_size(revs[i].data_length, 2, &data_size)) {
            return UFT_SCP_EOVERFLOW;
        }
        
        if ((size_t)data_abs + data_size > img->file_size) {
            return UFT_SCP_EBOUNDS;
        }
    }
    
    if (out_count) *out_count = img->hdr.num_revs;
    return UFT_SCP_OK;
}

// ============================================================================
// READ FLUX
// ============================================================================

uft_scp_error_t uft_scp_read_flux_safe(const uft_scp_image_hardened_t *img,
                                        uint8_t track_index,
                                        uint8_t rev_index,
                                        uint32_t *transitions,
                                        size_t trans_cap,
                                        size_t *out_count,
                                        uint32_t *out_total_time) {
    if (!img || !transitions || trans_cap == 0) return UFT_SCP_EINVAL;
    if (!img->validated || img->closed) return UFT_SCP_EINVAL;
    if (track_index >= UFT_SCP_MAX_TRACK_ENTRIES) return UFT_SCP_EBOUNDS;
    if (rev_index >= img->hdr.num_revs) return UFT_SCP_EBOUNDS;
    
    uint32_t track_off = img->track_offsets[track_index];
    if (track_off == 0) return UFT_SCP_EFORMAT;
    
    // Read revolution info
    uft_scp_track_rev_t revs[UFT_SCP_MAX_REVOLUTIONS];
    size_t rev_count;
    uft_scp_error_t rc = uft_scp_read_revolutions_safe(img, track_index, 
                                                        revs, UFT_SCP_MAX_REVOLUTIONS,
                                                        &rev_count);
    if (rc != UFT_SCP_OK) return rc;
    
    uft_scp_track_rev_t *rev = &revs[rev_index];
    
    // Calculate absolute data offset
    uint32_t data_off_abs;
    if (!safe_add_u32(track_off, rev->data_offset, &data_off_abs)) {
        return UFT_SCP_EOVERFLOW;
    }
    
    if (safe_fseek(img->f, img->file_size, data_off_abs) != 0) {
        return UFT_SCP_EIO;
    }
    
    // Calculate bytes to read
    size_t nvals = rev->data_length;
    size_t remaining_bytes;
    if (!safe_mul_size(nvals, 2, &remaining_bytes)) {
        return UFT_SCP_EOVERFLOW;
    }
    
    uint32_t time = 0;
    size_t outn = 0;
    uint8_t buf[4096];
    
    while (remaining_bytes > 0) {
        size_t toread = (remaining_bytes > sizeof(buf)) ? sizeof(buf) : remaining_bytes;
        
        if (!fread_exact(img->f, buf, toread)) {
            return UFT_SCP_EIO;
        }
        remaining_bytes -= toread;
        
        for (size_t i = 0; i + 1 < toread; i += 2) {
            uint16_t be = rd_u16_be(&buf[i]);
            
            if (be != 0) {
                uint32_t new_time;
                if (!safe_add_u32(time, (uint32_t)be, &new_time)) {
                    if (out_count) *out_count = outn;
                    if (out_total_time) *out_total_time = time;
                    return UFT_SCP_EOVERFLOW;
                }
                time = new_time;
                
                if (outn < trans_cap) {
                    transitions[outn++] = time;
                } else {
                    if (out_count) *out_count = outn;
                    if (out_total_time) *out_total_time = time;
                    return UFT_SCP_EBOUNDS;
                }
            } else {
                uint32_t new_time;
                if (!safe_add_u32(time, 0x10000u, &new_time)) {
                    if (out_count) *out_count = outn;
                    if (out_total_time) *out_total_time = time;
                    return UFT_SCP_EOVERFLOW;
                }
                time = new_time;
            }
        }
    }
    
    if (out_count) *out_count = outn;
    if (out_total_time) *out_total_time = time;
    return UFT_SCP_OK;
}
