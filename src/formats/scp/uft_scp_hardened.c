/**
 * @file uft_scp_hardened.c
 * @brief Security-Hardened SCP Parser
 * 
 * Fixes:
 * - BUG-001: Integer Overflow Protection
 * - BUG-004: File Size Validation
 * - BUG-005: Safe Multiplication
 * - BUG-007: Resource Limits
 * - BUG-010: Endianness Handling
 * 
 * @author Superman QA
 * @date 2026
 */

#include "uft/core/uft_safe_math.h"
#include "uft/formats/uft_scp.h"
#include <string.h>
#include <limits.h>

// ============================================================================
// SECURITY LIMITS
// ============================================================================

#define SCP_MAX_FILE_SIZE       (512UL * 1024 * 1024)   // 512 MB
#define SCP_MAX_TRACK_DATA      (2UL * 1024 * 1024)     // 2 MB per track
#define SCP_MAX_REVOLUTIONS     32
#define SCP_MAX_FLUX_PER_REV    500000
#define SCP_MAX_TOTAL_FLUX      (SCP_MAX_REVOLUTIONS * SCP_MAX_FLUX_PER_REV)

// ============================================================================
// SAFE ARITHMETIC
// ============================================================================

/**
 * @brief Safe uint32_t addition with overflow check
 * @return true on success, false on overflow
 */
static inline bool safe_add_u32(uint32_t a, uint32_t b, uint32_t *result) {
    if (a > UINT32_MAX - b) {
        return false;
    }
    *result = a + b;
    return true;
}

/**
 * @brief Safe size_t multiplication with overflow check
 * @return true on success, false on overflow
 */
static inline bool safe_mul_size(size_t a, size_t b, size_t *result) {
    if (a != 0 && b > SIZE_MAX / a) {
        return false;
    }
    *result = a * b;
    return true;
}

// ============================================================================
// ENDIANNESS HANDLING
// ============================================================================

/**
 * @brief Read uint32_t from little-endian buffer (portable)
 */
static inline uint32_t rd_u32_le(const uint8_t *p) {
    return (uint32_t)p[0] 
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

/**
 * @brief Read uint16_t from big-endian buffer (SCP flux data)
 */
static inline uint16_t rd_u16_be(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] << 8) | (uint16_t)p[1];
}

// ============================================================================
// SAFE FILE OPERATIONS
// ============================================================================

/**
 * @brief Get file size safely
 * @return File size or 0 on error
 */
static size_t get_file_size(FILE *f) {
    long current = ftell(f);
    if (current < 0) return 0;
    
    if (fseek(f, 0, SEEK_END) != 0) return 0;
    
    long end = ftell(f);
    if (end < 0) {
        if (fseek(f, current, SEEK_SET) != 0) { /* I/O error */ }
        return 0;
    }
    
    if (fseek(f, current, SEEK_SET) != 0) { /* I/O error */ }
    return (size_t)end;
}

/**
 * @brief Safe seek with bounds check
 */
static int safe_fseek(FILE *f, size_t file_size, uint32_t offset) {
    if ((size_t)offset >= file_size) {
        return -1;  // Out of bounds
    }
#if defined(_WIN32)
    return _fseeki64(f, (long long)offset, SEEK_SET);
#else
    return (void)fseek(f, (long)offset, SEEK_SET);
#endif
}

/**
 * @brief Read exact number of bytes
 */
static bool fread_exact(FILE *f, void *buf, size_t n) {
    return fread(buf, 1, n, f) == n;
}

// ============================================================================
// HARDENED SCP IMAGE STRUCTURE
// ============================================================================

typedef struct {
    FILE           *f;
    size_t          file_size;           // ADDED: For bounds checking
    uft_scp_header_t hdr;
    uint32_t        track_offsets[UFT_SCP_MAX_TRACK_ENTRIES];
    uint8_t         extended_mode;
    bool            validated;           // ADDED: Validation flag
} uft_scp_image_hardened_t;

// ============================================================================
// OPEN (HARDENED)
// ============================================================================

int uft_scp_open_hardened(uft_scp_image_hardened_t *img, const char *path) {
    // NULL checks
    if (!img || !path) {
        return UFT_SCP_EINVAL;
    }
    
    memset(img, 0, sizeof(*img));
    
    // Open file
    img->f = fopen(path, "rb");
    if (!img->f) {
        return UFT_SCP_EIO;
    }
    
    // Get and validate file size
    img->file_size = get_file_size(img->f);
    if (img->file_size == 0) {
        fclose(img->f);
        img->f = NULL;
        return UFT_SCP_EIO;
    }
    
    // FIX BUG-004: Validate file size
    if (img->file_size < sizeof(uft_scp_header_t)) {
        fclose(img->f);
        img->f = NULL;
        return UFT_SCP_EFORMAT;
    }
    
    if (img->file_size > SCP_MAX_FILE_SIZE) {
        fclose(img->f);
        img->f = NULL;
        return UFT_SCP_EBOUNDS;
    }
    
    // Read header
    if (!fread_exact(img->f, &img->hdr, sizeof(img->hdr))) {
        fclose(img->f);
        img->f = NULL;
        return UFT_SCP_EIO;
    }
    
    // Validate magic
    if (memcmp(img->hdr.signature, "SCP", 3) != 0) {
        fclose(img->f);
        img->f = NULL;
        return UFT_SCP_EFORMAT;
    }
    
    // FIX BUG-007: Validate revolution count
    if (img->hdr.num_revs > SCP_MAX_REVOLUTIONS) {
        fclose(img->f);
        img->f = NULL;
        return UFT_SCP_EBOUNDS;
    }
    
    img->extended_mode = (img->hdr.flags & 0x40u) ? 1u : 0u;
    
    // FIX BUG-010: Convert track offsets from LE to host
    for (size_t i = 0; i < UFT_SCP_MAX_TRACK_ENTRIES; i++) {
        // Header track_offsets are already in memory, convert from LE
        uint8_t *p = (uint8_t*)&img->hdr.track_offsets[i];
        img->track_offsets[i] = rd_u32_le(p);
    }
    
    // Extended mode: offsets table at 0x80
    if (img->extended_mode) {
        // Validate offset
        if (0x80 + sizeof(img->track_offsets) > img->file_size) {
            fclose(img->f);
            img->f = NULL;
            return UFT_SCP_EFORMAT;
        }
        
        if (fseek(img->f, 0x80, SEEK_SET) != 0) {
            fclose(img->f);
            img->f = NULL;
            return UFT_SCP_EIO;
        }
        
        uint8_t tmp[UFT_SCP_MAX_TRACK_ENTRIES * 4];
        if (!fread_exact(img->f, tmp, sizeof(tmp))) {
            fclose(img->f);
            img->f = NULL;
            return UFT_SCP_EIO;
        }
        
        for (size_t i = 0; i < UFT_SCP_MAX_TRACK_ENTRIES; i++) {
            img->track_offsets[i] = rd_u32_le(&tmp[i * 4]);
        }
    }
    
    // Validate all track offsets are within file bounds
    for (size_t i = 0; i < UFT_SCP_MAX_TRACK_ENTRIES; i++) {
        uint32_t off = img->track_offsets[i];
        if (off != 0 && (size_t)off >= img->file_size) {
            fclose(img->f);
            img->f = NULL;
            return UFT_SCP_EFORMAT;
        }
    }
    
    img->validated = true;
    return UFT_SCP_OK;
}

// ============================================================================
// READ TRACK REVOLUTIONS (HARDENED)
// ============================================================================

int uft_scp_read_track_revs_hardened(uft_scp_image_hardened_t *img, 
                                      uint8_t track_index,
                                      uft_scp_track_rev_t *revs, 
                                      size_t revs_cap,
                                      uft_scp_track_header_t *out_trk_hdr) {
    // Validation
    if (!img || !img->f || !img->validated || !revs) {
        return UFT_SCP_EINVAL;
    }
    
    if (track_index >= UFT_SCP_MAX_TRACK_ENTRIES) {
        return UFT_SCP_EBOUNDS;
    }
    
    if (revs_cap < (size_t)img->hdr.num_revs) {
        return UFT_SCP_EBOUNDS;
    }
    
    uint32_t off = img->track_offsets[track_index];
    if (off == 0) {
        return UFT_SCP_EFORMAT;  // Track not present
    }
    
    // FIX BUG-001: Safe offset validation
    if ((size_t)off + sizeof(uft_scp_track_header_t) > img->file_size) {
        return UFT_SCP_EBOUNDS;
    }
    
    if (safe_fseek(img->f, img->file_size, off) != 0) {
        return UFT_SCP_EIO;
    }
    
    // Read track header
    uft_scp_track_header_t trk = {0};
    if (!fread_exact(img->f, &trk, sizeof(trk))) {
        return UFT_SCP_EIO;
    }
    
    if (memcmp(trk.signature, "TRK", 3) != 0) {
        return UFT_SCP_EFORMAT;
    }
    
    if (out_trk_hdr) {
        *out_trk_hdr = trk;
    }
    
    // Read revolution entries with validation
    for (size_t i = 0; i < (size_t)img->hdr.num_revs; i++) {
        uint8_t raw[12];
        if (!fread_exact(img->f, raw, sizeof(raw))) {
            return UFT_SCP_EIO;
        }
        
        revs[i].time_duration = rd_u32_le(&raw[0]);
        revs[i].data_length   = rd_u32_le(&raw[4]);
        revs[i].data_offset   = rd_u32_le(&raw[8]);
        
        // FIX BUG-005: Validate data_length
        if (revs[i].data_length > SCP_MAX_FLUX_PER_REV) {
            return UFT_SCP_EBOUNDS;
        }
        
        // FIX BUG-001: Validate data_offset doesn't overflow
        uint32_t data_abs;
        if (!safe_add_u32(off, revs[i].data_offset, &data_abs)) {
            return UFT_SCP_EBOUNDS;
        }
        
        // Validate data is within file
        size_t data_size;
        if (!safe_mul_size(revs[i].data_length, 2, &data_size)) {
            return UFT_SCP_EBOUNDS;
        }
        
        if ((size_t)data_abs + data_size > img->file_size) {
            return UFT_SCP_EBOUNDS;
        }
    }
    
    return UFT_SCP_OK;
}

// ============================================================================
// READ TRANSITIONS (HARDENED)
// ============================================================================

int uft_scp_read_rev_transitions_hardened(uft_scp_image_hardened_t *img,
                                           uint8_t track_index,
                                           uint8_t rev_index,
                                           uint32_t *transitions_out,
                                           size_t transitions_cap,
                                           size_t *out_count,
                                           uint32_t *out_total_time) {
    // Validation
    if (!img || !img->f || !img->validated) {
        return UFT_SCP_EINVAL;
    }
    
    if (!transitions_out || transitions_cap == 0) {
        return UFT_SCP_EINVAL;
    }
    
    if (track_index >= UFT_SCP_MAX_TRACK_ENTRIES) {
        return UFT_SCP_EBOUNDS;
    }
    
    if (rev_index >= img->hdr.num_revs) {
        return UFT_SCP_EBOUNDS;
    }
    
    uint32_t track_off = img->track_offsets[track_index];
    if (track_off == 0) {
        return UFT_SCP_EFORMAT;
    }
    
    // Read revolution info
    uft_scp_track_rev_t revs[SCP_MAX_REVOLUTIONS];
    int rc = uft_scp_read_track_revs_hardened(img, track_index, revs, 
                                              SCP_MAX_REVOLUTIONS, NULL);
    if (rc != UFT_SCP_OK) {
        return rc;
    }
    
    uft_scp_track_rev_t rev = revs[rev_index];
    
    // FIX BUG-001: Safe offset calculation
    uint32_t data_off_abs;
    if (!safe_add_u32(track_off, rev.data_offset, &data_off_abs)) {
        return UFT_SCP_EBOUNDS;
    }
    
    if (safe_fseek(img->f, img->file_size, data_off_abs) != 0) {
        return UFT_SCP_EIO;
    }
    
    // FIX BUG-005: Safe byte count calculation
    size_t nvals = (size_t)rev.data_length;
    size_t remaining_bytes;
    if (!safe_mul_size(nvals, 2, &remaining_bytes)) {
        return UFT_SCP_EBOUNDS;
    }
    
    uint32_t time = 0;
    size_t outn = 0;
    
    // Stream read in chunks
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
                // Accumulate time
                uint32_t new_time;
                if (!safe_add_u32(time, (uint32_t)be, &new_time)) {
                    // Time overflow - very long track
                    if (out_count) *out_count = outn;
                    if (out_total_time) *out_total_time = time;
                    return UFT_SCP_EBOUNDS;
                }
                time = new_time;
                
                if (outn < transitions_cap) {
                    transitions_out[outn++] = time;
                } else {
                    if (out_count) *out_count = outn;
                    if (out_total_time) *out_total_time = time;
                    return UFT_SCP_EBOUNDS;
                }
            } else {
                // Overflow marker: add 0x10000
                uint32_t new_time;
                if (!safe_add_u32(time, 0x10000u, &new_time)) {
                    if (out_count) *out_count = outn;
                    if (out_total_time) *out_total_time = time;
                    return UFT_SCP_EBOUNDS;
                }
                time = new_time;
            }
        }
    }
    
    if (out_count) *out_count = outn;
    if (out_total_time) *out_total_time = time;
    return UFT_SCP_OK;
}

// ============================================================================
// CLOSE (HARDENED)
// ============================================================================

void uft_scp_close_hardened(uft_scp_image_hardened_t *img) {
    if (!img) return;
    
    if (img->f) {
        fclose(img->f);
        img->f = NULL;
    }
    
    // Clear sensitive data
    memset(img, 0, sizeof(*img));
}
