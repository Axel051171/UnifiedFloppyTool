/*
 * uft_c64_track_align.c
 *
 * Track alignment functions for C64/1541 disk preservation.
 *
 * Original Source:
 * - GCR routines (C) Markus Brenner <markus(at)brenner.de>
 *
 */

#include "c64/uft_c64_track_align.h"
#include <string.h>
#include <stdlib.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * STATIC: GCR DECODE TABLE (for bad GCR detection)
 * ═══════════════════════════════════════════════════════════════════════════ */

/* GCR-to-Nibble decode table (0xFF = invalid) */
static const uint8_t gcr_decode_low[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0x08, 0x00, 0x01, 0xFF, 0x0C, 0x04, 0x05,
    0xFF, 0xFF, 0x02, 0x03, 0xFF, 0x0F, 0x06, 0x07,
    0xFF, 0x09, 0x0A, 0x0B, 0xFF, 0x0D, 0x0E, 0xFF
};

static const uint8_t gcr_decode_high[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0x80, 0x00, 0x10, 0xFF, 0xC0, 0x40, 0x50,
    0xFF, 0xFF, 0x20, 0x30, 0xFF, 0xF0, 0x60, 0x70,
    0xFF, 0x90, 0xA0, 0xB0, 0xFF, 0xD0, 0xE0, 0xFF
};

/* Alignment method names */
static const char *align_method_names[] = {
    "NONE",
    "GAP",
    "SEC0",
    "LONGSYNC",
    "BADGCR",
    "VMAX",
    "AUTOGAP",
    "VMAX-CW",
    "RAW",
    "PIRATESLAYER",
    "RAPIDLOK"
};

/* ═══════════════════════════════════════════════════════════════════════════
 * HELPER: Check if byte is a V-MAX marker
 * ═══════════════════════════════════════════════════════════════════════════ */

static inline bool is_vmax_marker(uint8_t b)
{
    return (b == UFT_VMAX_MARKER_4B ||
            b == UFT_VMAX_MARKER_49 ||
            b == UFT_VMAX_MARKER_69 ||
            b == UFT_VMAX_MARKER_5A ||
            b == UFT_VMAX_MARKER_A5);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * GCR VALIDATION
 * ═══════════════════════════════════════════════════════════════════════════ */

bool uft_c64_is_bad_gcr(const uint8_t *buffer, size_t length, size_t pos)
{
    if (pos >= length) return false;
    
    /* Get current and next byte for GCR boundary check */
    uint8_t b0 = buffer[pos];
    uint8_t b1 = (pos + 1 < length) ? buffer[pos + 1] : buffer[0];
    
    /* Check high nibble of current byte */
    uint8_t gcr_hi = (b0 >> 3) & 0x1F;
    if (gcr_decode_high[gcr_hi] == 0xFF && gcr_hi != 0x1F) {
        return true;  /* Invalid GCR (not sync) */
    }
    
    /* Check boundary between bytes */
    uint16_t pair = ((uint16_t)b0 << 8) | b1;
    uint8_t gcr_mid = (pair >> 6) & 0x1F;
    if (gcr_decode_low[gcr_mid] == 0xFF && gcr_mid != 0x1F) {
        return true;
    }
    
    return false;
}

size_t uft_c64_count_bad_gcr(const uint8_t *buffer, size_t length)
{
    size_t count = 0;
    for (size_t i = 0; i < length; i++) {
        if (uft_c64_is_bad_gcr(buffer, length, i)) {
            count++;
        }
    }
    return count;
}

size_t uft_c64_find_sync(const uint8_t *buffer, size_t length, size_t start)
{
    for (size_t i = start; i < length - 1; i++) {
        /* Sync: $FF followed by byte with MSB set (10+ bits of 1) */
        if (buffer[i] == 0xFF && (buffer[i + 1] & 0x80)) {
            return i;
        }
    }
    return length;  /* Not found */
}

/* ═══════════════════════════════════════════════════════════════════════════
 * BUFFER MANIPULATION
 * ═══════════════════════════════════════════════════════════════════════════ */

void uft_c64_shift_buffer_left(uint8_t *buffer, size_t length, int bits)
{
    if (bits <= 0 || bits >= 8 || length == 0) return;
    
    int carryshift = 8 - bits;
    for (size_t i = 0; i < length - 1; i++) {
        buffer[i] = (buffer[i] << bits) | (buffer[i + 1] >> carryshift);
    }
    buffer[length - 1] <<= bits;
}

void uft_c64_shift_buffer_right(uint8_t *buffer, size_t length, int bits)
{
    if (bits <= 0 || bits >= 8 || length == 0) return;
    
    int carryshift = 8 - bits;
    uint8_t carry = 0;
    for (size_t i = 0; i < length; i++) {
        uint8_t next_carry = buffer[i];
        buffer[i] = (buffer[i] >> bits) | (carry << carryshift);
        carry = next_carry;
    }
}

void uft_c64_rotate_buffer(uint8_t *buffer, size_t length, size_t offset)
{
    if (offset == 0 || offset >= length) return;
    
    /* Allocate temp buffer */
    uint8_t *temp = (uint8_t *)malloc(length);
    if (!temp) return;
    
    /* Copy with rotation */
    memcpy(temp, buffer + offset, length - offset);
    memcpy(temp + length - offset, buffer, offset);
    memcpy(buffer, temp, length);
    
    free(temp);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * V-MAX ALIGNMENT
 * ═══════════════════════════════════════════════════════════════════════════ */

size_t uft_c64_align_vmax(const uint8_t *buffer, size_t length,
                          uft_c64_align_result_t *result)
{
    if (result) {
        memset(result, 0, sizeof(*result));
        result->method = UFT_C64_ALIGN_VMAX;
    }
    
    size_t best_offset = 0;
    size_t best_run = 0;
    size_t current_run = 0;
    size_t run_start = 0;
    
    for (size_t i = 0; i < length; i++) {
        if (is_vmax_marker(buffer[i])) {
            if (current_run == 0) {
                run_start = i;
            }
            current_run++;
            
            /* Need at least 6 consecutive markers */
            if (current_run > 5 && current_run > best_run) {
                best_run = current_run;
                best_offset = run_start;
            }
        } else {
            current_run = 0;
        }
    }
    
    if (result) {
        result->found = (best_run > 5);
        result->offset = best_offset;
        result->marker_run = best_run;
    }
    
    return best_run > 5 ? best_offset : 0;
}

size_t uft_c64_align_vmax_cw(const uint8_t *buffer, size_t length,
                             uft_c64_align_result_t *result)
{
    if (result) {
        memset(result, 0, sizeof(*result));
        result->method = UFT_C64_ALIGN_VMAX_CW;
    }
    
    /* Cinemaware pattern: $64 $A5 $A5 $A5 */
    for (size_t i = 0; i + 3 < length; i++) {
        if (buffer[i] == 0x64 &&
            buffer[i + 1] == 0xA5 &&
            buffer[i + 2] == 0xA5 &&
            buffer[i + 3] == 0xA5) {
            
            if (result) {
                result->found = true;
                result->offset = i;
                result->marker_run = 4;
            }
            return i;
        }
    }
    
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PIRATESLAYER ALIGNMENT
 * ═══════════════════════════════════════════════════════════════════════════ */

size_t uft_c64_align_pirateslayer(uint8_t *buffer, size_t length,
                                  uft_c64_align_result_t *result)
{
    if (result) {
        memset(result, 0, sizeof(*result));
        result->method = UFT_C64_ALIGN_PSLAYER;
    }
    
    /* Backup buffer since we may shift it */
    uint8_t *backup = (uint8_t *)malloc(length);
    if (!backup) return 0;
    memcpy(backup, buffer, length);
    
    /* Try up to 8 bit shifts */
    for (int shift = 0; shift < 8; shift++) {
        for (size_t i = 0; i + 5 < length; i++) {
            /* Version 1/2 signature: $D7 $D7 $EB $CC $AD */
            if (buffer[i] == 0xD7 &&
                buffer[i + 1] == 0xD7 &&
                buffer[i + 2] == 0xEB &&
                buffer[i + 3] == 0xCC &&
                buffer[i + 4] == 0xAD) {
                
                free(backup);
                size_t align_pos = (i >= 5) ? i - 5 : 0;
                
                if (result) {
                    result->found = true;
                    result->offset = align_pos;
                    result->ps_version = 1;
                }
                return align_pos;
            }
            
            /* Version 1 alternate: $EB $D7 $AA $55 */
            if (buffer[i] == 0xEB &&
                buffer[i + 1] == 0xD7 &&
                buffer[i + 2] == 0xAA &&
                buffer[i + 3] == 0x55) {
                
                free(backup);
                size_t align_pos = (i >= 5) ? i - 5 : 0;
                
                if (result) {
                    result->found = true;
                    result->offset = align_pos;
                    result->ps_version = 1;
                }
                return align_pos;
            }
        }
        
        /* Shift right by 1 bit and try again */
        uft_c64_shift_buffer_right(buffer, length, 1);
    }
    
    /* Restore original buffer */
    memcpy(buffer, backup, length);
    free(backup);
    
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * RAPIDLOK ALIGNMENT
 * ═══════════════════════════════════════════════════════════════════════════ */

size_t uft_c64_align_rapidlok(const uint8_t *buffer, size_t length,
                              uft_c64_align_result_t *result)
{
    if (result) {
        memset(result, 0, sizeof(*result));
        result->method = UFT_C64_ALIGN_RAPIDLOK;
    }
    
    /* Rapidlok track header structure:
     * - 14-24 sync bytes ($FF)
     * - 1 $55 ID byte
     * - 60-300 $7B/$4B bytes
     */
    
    size_t best_offset = 0;
    size_t best_length = 0;
    int best_ff = 0, best_55 = 0, best_7b = 0, best_4b = 0;
    
    size_t i = 0;
    while (i < length - 100) {  /* Need at least 100 bytes for valid header */
        /* Count sync bytes */
        size_t ff_count = 0;
        size_t start = i;
        while (i < length && buffer[i] == 0xFF && ff_count < 25) {
            ff_count++;
            i++;
        }
        
        /* Need 14-24 sync bytes */
        if (ff_count < 14 || ff_count >= 25) {
            if (ff_count == 0) i++;
            continue;
        }
        
        /* Check for $55 ID byte */
        if (i >= length || buffer[i] != 0x55) {
            continue;
        }
        i++;
        
        /* Count $7B/$4B bytes */
        size_t b7b_count = 0;
        size_t b4b_count = 0;
        while (i < length && (buffer[i] == 0x7B || buffer[i] == 0x4B)) {
            if (buffer[i] == 0x4B) b4b_count++;
            b7b_count++;
            i++;
        }
        
        /* Need 60-300 $7B/$4B bytes */
        if (b7b_count >= 60 && b7b_count <= 300) {
            size_t total_len = ff_count + 1 + b7b_count;
            if (total_len > best_length) {
                best_length = total_len;
                best_offset = start;
                best_ff = (int)ff_count;
                best_55 = 1;
                best_7b = (int)b7b_count;
                best_4b = (int)b4b_count;
            }
        }
    }
    
    if (best_length > 0 && result) {
        result->found = true;
        result->offset = best_offset;
        result->rl_th_length = best_length;
        /* Version detection would require full sector analysis */
        result->rl_version = 0;  /* Unknown */
        result->rl_tv = UFT_RL_TV_UNKNOWN;
    }
    
    return best_length > 0 ? best_offset : 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * SYNC/GAP ALIGNMENT
 * ═══════════════════════════════════════════════════════════════════════════ */

size_t uft_c64_align_longsync(const uint8_t *buffer, size_t length,
                              uft_c64_align_result_t *result)
{
    if (result) {
        memset(result, 0, sizeof(*result));
        result->method = UFT_C64_ALIGN_LONGSYNC;
    }
    
    size_t best_offset = 0;
    size_t best_run = 0;
    size_t current_run = 0;
    size_t run_start = 0;
    
    for (size_t i = 0; i < length; i++) {
        if (buffer[i] == 0xFF) {
            if (current_run == 0) {
                run_start = i;
            }
            current_run++;
        } else {
            if (current_run > best_run) {
                best_run = current_run;
                best_offset = run_start;
            }
            current_run = 0;
        }
    }
    
    /* Check final run */
    if (current_run > best_run) {
        best_run = current_run;
        best_offset = run_start;
    }
    
    if (result) {
        result->found = (best_run > 0);
        result->offset = best_offset;
        result->marker_run = best_run;
    }
    
    return best_offset;
}

size_t uft_c64_align_autogap(const uint8_t *buffer, size_t length,
                             uft_c64_align_result_t *result)
{
    if (result) {
        memset(result, 0, sizeof(*result));
        result->method = UFT_C64_ALIGN_AUTOGAP;
    }
    
    size_t best_offset = 0;
    size_t best_run = 0;
    size_t current_run = 0;
    
    for (size_t i = 0; i + 1 < length; i++) {
        if (buffer[i] == buffer[i + 1]) {
            current_run++;
        } else {
            if (current_run > best_run) {
                best_run = current_run;
                /* Point to end of gap, minus 5 bytes */
                best_offset = (i >= 5) ? i - 5 : 0;
            }
            current_run = 0;
        }
    }
    
    if (result) {
        result->found = (best_run > 0);
        result->offset = best_offset;
        result->marker_run = best_run;
    }
    
    return best_offset;
}

size_t uft_c64_align_badgcr(const uint8_t *buffer, size_t length,
                            uft_c64_align_result_t *result)
{
    if (result) {
        memset(result, 0, sizeof(*result));
        result->method = UFT_C64_ALIGN_BADGCR;
    }
    
    size_t best_offset = 0;
    size_t best_run = 0;
    size_t current_run = 0;
    size_t run_end = 0;
    
    for (size_t i = 0; i < length; i++) {
        if (uft_c64_is_bad_gcr(buffer, length, i)) {
            current_run++;
            run_end = i + 1;
        } else {
            if (current_run > best_run) {
                best_run = current_run;
                best_offset = run_end;  /* First byte after bad run */
            }
            current_run = 0;
        }
    }
    
    if (result) {
        result->found = (best_run > 0);
        result->offset = best_offset;
        result->marker_run = best_run;
    }
    
    return best_offset;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * SYNC ALIGNMENT (Kryoflux support)
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_c64_sync_align(uint8_t *buffer, size_t length)
{
    if (!buffer || length < 16) return 0;
    
    /* Find first sync mark */
    size_t sync_pos = 0;
    while (sync_pos < length - 1) {
        if (buffer[sync_pos] == 0xFF && (buffer[sync_pos + 1] & 0x80)) {
            break;
        }
        sync_pos++;
    }
    
    if (sync_pos >= length - 1) return 0;  /* No sync found */
    
    /* Skip if we're in a header (first 15 bytes) */
    if (sync_pos < 15) {
        size_t next_sync = sync_pos + 1;
        while (next_sync < length - 1) {
            if (buffer[next_sync] == 0xFF && (buffer[next_sync + 1] & 0x80)) {
                break;
            }
            next_sync++;
        }
        if (next_sync < length - 1) {
            sync_pos = next_sync;
        }
    }
    
    /* Rotate buffer to start at sync */
    if (sync_pos > 0) {
        uft_c64_rotate_buffer(buffer, length, sync_pos);
    }
    
    /* Now bit-shift data between syncs to proper byte boundaries */
    for (size_t i = 0; i < length - 1; i++) {
        /* Find sync boundary (end of sync, start of data) */
        if ((buffer[i] == 0xFF && (buffer[i + 1] & 0x80) == 0x80 && buffer[i + 1] != 0xFF) ||
            (buffer[i] == 0x7F && (buffer[i + 1] & 0xC0) == 0xC0 && buffer[i + 1] != 0xFF)) {
            
            i++;  /* Move to first byte to shift */
            
            /* Find length to next sync */
            size_t bytes = 0;
            while (i + bytes < length - 1) {
                if (buffer[i + bytes] == 0xFF && (buffer[i + bytes + 1] & 0x80)) {
                    break;
                }
                bytes++;
            }
            
            /* Shift left until MSB is 0 */
            int bits = 0;
            while ((buffer[i] & 0x80) && bits < 8) {
                for (size_t j = 0; j < bytes && i + j + 1 < length; j++) {
                    buffer[i + j] = (buffer[i + j] << 1) | 
                                    ((buffer[i + j + 1] & 0x80) >> 7);
                }
                bits++;
            }
            
            i += bytes;  /* Skip processed region */
        }
    }
    
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * FAT TRACK DETECTION
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_c64_search_fat_tracks(const uint8_t *track_buffer,
                              const uint8_t *track_density,
                              const size_t *track_length,
                              int *fat_track_out)
{
    (void)track_density;  /* Not used in simple detection */
    
    if (fat_track_out) *fat_track_out = -1;
    
    int num_fats = 0;
    int first_fat = -1;
    
    /* Compare adjacent whole tracks */
    for (int track = 2; track <= UFT_MAX_HALFTRACKS_1541 - 3; track += 2) {
        size_t len1 = track_length[track];
        size_t len2 = track_length[track + 2];
        
        /* Skip empty or placeholder tracks */
        if (len1 == 0 || len2 == 0) continue;
        if (len1 == UFT_NIB_TRACK_LENGTH || len2 == UFT_NIB_TRACK_LENGTH) continue;
        
        /* Compare track data */
        const uint8_t *t1 = track_buffer + (track * UFT_NIB_TRACK_LENGTH);
        const uint8_t *t2 = track_buffer + ((track + 2) * UFT_NIB_TRACK_LENGTH);
        
        size_t match = 0;
        size_t min_len = (len1 < len2) ? len1 : len2;
        
        for (size_t i = 0; i < min_len; i++) {
            if (t1[i] == t2[i]) match++;
        }
        
        /* Fat track: tracks are nearly identical (within 20-40 bytes) */
        size_t diff = (len1 > match) ? len1 - match : 0;
        bool is_fat = (diff <= 20) || (track >= 70 && diff <= 40);
        
        if (is_fat) {
            num_fats++;
            if (first_fat < 0) {
                first_fat = track;
            }
        }
    }
    
    /* Only report if exactly one fat track region found */
    if (num_fats == 1 && fat_track_out) {
        *fat_track_out = first_fat;
    }
    
    return num_fats;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * AUTO-ALIGNMENT
 * ═══════════════════════════════════════════════════════════════════════════ */

uft_c64_align_method_t uft_c64_auto_align(uint8_t *buffer, size_t length,
                                          uft_c64_align_result_t *result)
{
    uft_c64_align_result_t temp;
    
    /* Try V-MAX first */
    if (uft_c64_align_vmax(buffer, length, &temp) > 0 && temp.found) {
        if (result) *result = temp;
        return UFT_C64_ALIGN_VMAX;
    }
    
    /* Try V-MAX Cinemaware */
    if (uft_c64_align_vmax_cw(buffer, length, &temp) > 0 && temp.found) {
        if (result) *result = temp;
        return UFT_C64_ALIGN_VMAX_CW;
    }
    
    /* Try PirateSlayer (modifies buffer!) */
    if (uft_c64_align_pirateslayer(buffer, length, &temp) > 0 && temp.found) {
        if (result) *result = temp;
        return UFT_C64_ALIGN_PSLAYER;
    }
    
    /* Try Rapidlok */
    if (uft_c64_align_rapidlok(buffer, length, &temp) > 0 && temp.found) {
        if (result) *result = temp;
        return UFT_C64_ALIGN_RAPIDLOK;
    }
    
    /* Fall back to longest sync */
    uft_c64_align_longsync(buffer, length, &temp);
    if (temp.marker_run >= 10) {
        if (result) *result = temp;
        return UFT_C64_ALIGN_LONGSYNC;
    }
    
    /* Fall back to auto gap */
    uft_c64_align_autogap(buffer, length, &temp);
    if (result) *result = temp;
    return UFT_C64_ALIGN_AUTOGAP;
}

const char *uft_c64_align_method_name(uft_c64_align_method_t method)
{
    if (method <= UFT_C64_ALIGN_RAPIDLOK) {
        return align_method_names[method];
    }
    return "UNKNOWN";
}
