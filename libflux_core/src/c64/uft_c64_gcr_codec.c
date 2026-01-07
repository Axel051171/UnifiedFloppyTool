/*
 * uft_c64_gcr_codec.c
 *
 * Complete GCR (Group Code Recording) codec for Commodore 64/1541.
 *
 * Original Source:
 *
 */

#include "c64/uft_c64_gcr_codec.h"
#include "c64/uft_c64_track_format.h"
#include <string.h>
#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * GCR ENCODING/DECODING TABLES
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * Nibble-to-GCR conversion table.
 * Each 4-bit nibble (0-15) maps to a 5-bit GCR value.
 * The GCR values are chosen to ensure no more than 2 consecutive zeros.
 */
static const uint8_t gcr_encode[16] = {
    0x0A, 0x0B, 0x12, 0x13,   /* 0-3:  01010, 01011, 10010, 10011 */
    0x0E, 0x0F, 0x16, 0x17,   /* 4-7:  01110, 01111, 10110, 10111 */
    0x09, 0x19, 0x1A, 0x1B,   /* 8-11: 01001, 11001, 11010, 11011 */
    0x0D, 0x1D, 0x1E, 0x15    /* 12-15: 01101, 11101, 11110, 10101 */
};

/*
 * GCR-to-Nibble decode table (high nibble).
 * 0xFF indicates invalid GCR value.
 */
static const uint8_t gcr_decode_high[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0x80, 0x00, 0x10, 0xFF, 0xC0, 0x40, 0x50,
    0xFF, 0xFF, 0x20, 0x30, 0xFF, 0xF0, 0x60, 0x70,
    0xFF, 0x90, 0xA0, 0xB0, 0xFF, 0xD0, 0xE0, 0xFF
};

/*
 * GCR-to-Nibble decode table (low nibble).
 * 0xFF indicates invalid GCR value.
 */
static const uint8_t gcr_decode_low[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0x08, 0x00, 0x01, 0xFF, 0x0C, 0x04, 0x05,
    0xFF, 0xFF, 0x02, 0x03, 0xFF, 0x0F, 0x06, 0x07,
    0xFF, 0x09, 0x0A, 0x0B, 0xFF, 0x0D, 0x0E, 0xFF
};

/* ═══════════════════════════════════════════════════════════════════════════
 * TABLE ACCESS FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

const uint8_t *uft_c64_gcr_encode_table(void)
{
    return gcr_encode;
}

const uint8_t *uft_c64_gcr_decode_high_table(void)
{
    return gcr_decode_high;
}

const uint8_t *uft_c64_gcr_decode_low_table(void)
{
    return gcr_decode_low;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * LOW-LEVEL GCR CONVERSION
 * ═══════════════════════════════════════════════════════════════════════════ */

bool uft_c64_gcr_is_valid(uint8_t gcr5)
{
    return (gcr5 < 32) && (gcr_decode_low[gcr5] != 0xFF);
}

uint8_t uft_c64_gcr_encode_nibble(uint8_t nibble)
{
    return gcr_encode[nibble & 0x0F];
}

uint8_t uft_c64_gcr_decode_nibble(uint8_t gcr5)
{
    if (gcr5 >= 32) return 0xFF;
    return gcr_decode_low[gcr5];
}

void uft_c64_gcr_encode_4bytes(const uint8_t *plain, uint8_t *gcr_out)
{
    /*
     * Encoding layout (4 bytes → 5 bytes):
     *
     * Input bytes:  [A] [B] [C] [D]
     * Nibbles:      a1 a0 b1 b0 c1 c0 d1 d0
     * GCR (5-bit):  A1 A0 B1 B0 C1 C0 D1 D0
     *
     * Output layout:
     * Byte 0: A1[4:0] A0[4:2]
     * Byte 1: A0[1:0] B1[4:0] B0[4]
     * Byte 2: B0[3:0] C1[4:1]
     * Byte 3: C1[0] C0[4:0] D1[4:2]
     * Byte 4: D1[1:0] D0[4:0]
     */
    
    uint8_t a1 = gcr_encode[plain[0] >> 4];
    uint8_t a0 = gcr_encode[plain[0] & 0x0F];
    uint8_t b1 = gcr_encode[plain[1] >> 4];
    uint8_t b0 = gcr_encode[plain[1] & 0x0F];
    uint8_t c1 = gcr_encode[plain[2] >> 4];
    uint8_t c0 = gcr_encode[plain[2] & 0x0F];
    uint8_t d1 = gcr_encode[plain[3] >> 4];
    uint8_t d0 = gcr_encode[plain[3] & 0x0F];
    
    gcr_out[0] = (a1 << 3) | (a0 >> 2);
    gcr_out[1] = (a0 << 6) | (b1 << 1) | (b0 >> 4);
    gcr_out[2] = (b0 << 4) | (c1 >> 1);
    gcr_out[3] = (c1 << 7) | (c0 << 2) | (d1 >> 3);
    gcr_out[4] = (d1 << 5) | d0;
}

int uft_c64_gcr_decode_4bytes(const uint8_t *gcr, uint8_t *plain_out)
{
    uint8_t hnibble, lnibble;
    int bad_gcr = 0;
    int n_converted = 4;
    
    /* Byte 0: A1[4:0] from gcr[0][7:3], A0[4:0] from gcr[0][2:0]|gcr[1][7:6] */
    hnibble = gcr_decode_high[gcr[0] >> 3];
    lnibble = gcr_decode_low[((gcr[0] << 2) | (gcr[1] >> 6)) & 0x1F];
    if ((hnibble == 0xFF || lnibble == 0xFF) && !bad_gcr) {
        bad_gcr = 1;
        n_converted = 0;
    }
    plain_out[0] = hnibble | lnibble;
    
    /* Byte 1: B1[4:0] from gcr[1][5:1], B0[4:0] from gcr[1][0]|gcr[2][7:4] */
    hnibble = gcr_decode_high[(gcr[1] >> 1) & 0x1F];
    lnibble = gcr_decode_low[((gcr[1] << 4) | (gcr[2] >> 4)) & 0x1F];
    if ((hnibble == 0xFF || lnibble == 0xFF) && !bad_gcr) {
        bad_gcr = 2;
        n_converted = 1;
    }
    plain_out[1] = hnibble | lnibble;
    
    /* Byte 2: C1[4:0] from gcr[2][3:0]|gcr[3][7], C0[4:0] from gcr[3][6:2] */
    hnibble = gcr_decode_high[((gcr[2] << 1) | (gcr[3] >> 7)) & 0x1F];
    lnibble = gcr_decode_low[(gcr[3] >> 2) & 0x1F];
    if ((hnibble == 0xFF || lnibble == 0xFF) && !bad_gcr) {
        bad_gcr = 3;
        n_converted = 2;
    }
    plain_out[2] = hnibble | lnibble;
    
    /* Byte 3: D1[4:0] from gcr[3][1:0]|gcr[4][7:5], D0[4:0] from gcr[4][4:0] */
    hnibble = gcr_decode_high[((gcr[3] << 3) | (gcr[4] >> 5)) & 0x1F];
    lnibble = gcr_decode_low[gcr[4] & 0x1F];
    if ((hnibble == 0xFF || lnibble == 0xFF) && !bad_gcr) {
        bad_gcr = 4;
        n_converted = 3;
    }
    plain_out[3] = hnibble | lnibble;
    
    return n_converted;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * SYNC DETECTION
 * ═══════════════════════════════════════════════════════════════════════════ */

bool uft_c64_gcr_find_sync(const uint8_t **gcr, const uint8_t *gcr_end)
{
    const uint8_t *ptr = *gcr;
    
    while (ptr < gcr_end - 1) {
        /* Sync is $FF followed by byte with MSB set */
        if (ptr[0] == 0xFF && (ptr[1] & 0x80)) {
            /* Skip all $FF bytes */
            while (ptr < gcr_end && *ptr == 0xFF) {
                ptr++;
            }
            *gcr = ptr;
            return true;
        }
        ptr++;
    }
    
    *gcr = gcr_end;
    return false;
}

bool uft_c64_gcr_find_header(const uint8_t **gcr, const uint8_t *gcr_end)
{
    const uint8_t *ptr = *gcr;
    
    while (ptr < gcr_end - 10) {
        /* Header marker: sync followed by $52 (GCR for $08) */
        if (ptr[0] == 0xFF && ptr[1] == 0x52) {
            *gcr = ptr + 1;  /* Point to $52 */
            return true;
        }
        ptr++;
    }
    
    *gcr = gcr_end;
    return false;
}

size_t uft_c64_gcr_count_sync(const uint8_t *gcr, const uint8_t *gcr_end)
{
    size_t count = 0;
    while (gcr < gcr_end && *gcr == 0xFF) {
        count++;
        gcr++;
    }
    return count;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * HEADER OPERATIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

bool uft_c64_gcr_decode_header(const uint8_t *gcr, uint8_t *header_out)
{
    /* Header is 10 GCR bytes = 8 decoded bytes */
    int result1 = uft_c64_gcr_decode_4bytes(gcr, header_out);
    int result2 = uft_c64_gcr_decode_4bytes(gcr + 5, header_out + 4);
    
    return (result1 == 4 && result2 == 4);
}

void uft_c64_gcr_encode_header(int track, int sector,
                               const uint8_t *disk_id,
                               uint8_t *gcr_out)
{
    uint8_t header[8];
    uint8_t checksum;
    
    /* Header format:
     * [0] $08 = header ID
     * [1] checksum (sector ^ track ^ id1 ^ id0)
     * [2] sector
     * [3] track
     * [4] id1
     * [5] id0
     * [6] $0F (padding)
     * [7] $0F (padding)
     */
    
    checksum = (uint8_t)(sector ^ track ^ disk_id[1] ^ disk_id[0]);
    
    header[0] = 0x08;
    header[1] = checksum;
    header[2] = (uint8_t)sector;
    header[3] = (uint8_t)track;
    header[4] = disk_id[1];
    header[5] = disk_id[0];
    header[6] = 0x0F;
    header[7] = 0x0F;
    
    uft_c64_gcr_encode_4bytes(header, gcr_out);
    uft_c64_gcr_encode_4bytes(header + 4, gcr_out + 5);
}

bool uft_c64_gcr_extract_id(const uint8_t *gcr_track, size_t length,
                            uint8_t *id_out)
{
    const uint8_t *ptr = gcr_track;
    const uint8_t *end = gcr_track + length;
    uint8_t header[8];
    
    while (ptr < end - 10) {
        /* Look for header marker */
        if (ptr[0] == 0xFF && ptr[1] == 0x52) {
            if (uft_c64_gcr_decode_header(ptr + 1, header)) {
                if (header[0] == 0x08) {
                    id_out[0] = header[5];  /* id0 */
                    id_out[1] = header[4];  /* id1 */
                    return true;
                }
            }
        }
        ptr++;
    }
    
    return false;
}

bool uft_c64_gcr_extract_cosmetic_id(const uint8_t *gcr_track, size_t length,
                                     uint8_t *id_out)
{
    /* Cosmetic ID is in BAM sector (track 18, sector 0) at offset $A2-$A3 */
    /* This requires decoding the entire sector, which is more complex */
    /* For now, fall back to physical ID */
    return uft_c64_gcr_extract_id(gcr_track, length, id_out);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * SECTOR DECODE
 * ═══════════════════════════════════════════════════════════════════════════ */

uint8_t uft_c64_gcr_decode_sector(const uint8_t *gcr_start,
                                  const uint8_t *gcr_end,
                                  int track, int sector,
                                  const uint8_t *disk_id,
                                  uint8_t *data_out,
                                  uft_c64_sector_result_t *result)
{
    const uint8_t *ptr;
    uint8_t header[8];
    uint8_t sector_buf[260];
    uint8_t error_code = 0x01;  /* SECTOR_OK */
    uint8_t hdr_checksum, blk_checksum;
    int i;
    
    if (result) {
        memset(result, 0, sizeof(*result));
    }
    
    if (!gcr_start || !gcr_end || gcr_end <= gcr_start) {
        return 0x03;  /* SYNC_NOT_FOUND */
    }
    
    /* Initialize output with "unformatted" pattern */
    memset(data_out, 0x01, 256);
    
    /* Search for matching sector header */
    ptr = gcr_start;
    while (ptr < gcr_end - 10) {
        if (ptr[0] == 0xFF && ptr[1] == 0x52) {
            /* Decode header */
            if (uft_c64_gcr_decode_header(ptr + 1, header)) {
                /* Check if this is our sector */
                if (header[0] == 0x08 && 
                    header[2] == sector && 
                    header[3] == track) {
                    
                    if (result) {
                        result->header_track = header[3];
                        result->header_sector = header[2];
                        result->disk_id[0] = header[5];
                        result->disk_id[1] = header[4];
                        result->header_ok = true;
                    }
                    
                    /* Verify header checksum */
                    hdr_checksum = header[1] ^ header[2] ^ header[3] ^ 
                                   header[4] ^ header[5];
                    if (hdr_checksum != 0) {
                        error_code = 0x09;  /* BAD_HEADER_CHECKSUM */
                    }
                    
                    /* Check disk ID */
                    if (disk_id && (header[5] != disk_id[0] || 
                                    header[4] != disk_id[1])) {
                        if (error_code == 0x01) {
                            error_code = 0x0B;  /* ID_MISMATCH */
                        }
                    }
                    
                    /* Find data block after header */
                    ptr += 11;  /* Skip header GCR */
                    
                    /* Skip header gap, find data sync */
                    while (ptr < gcr_end && *ptr != 0xFF) {
                        ptr++;
                    }
                    while (ptr < gcr_end && *ptr == 0xFF) {
                        ptr++;
                    }
                    
                    if (ptr >= gcr_end - 325) {
                        return 0x04;  /* DATA_NOT_FOUND */
                    }
                    
                    /* Decode data block (65 groups of 4 bytes = 260 bytes) */
                    for (i = 0; i < 65; i++) {
                        int decoded = uft_c64_gcr_decode_4bytes(ptr, 
                                                                sector_buf + i * 4);
                        if (decoded < 4 && result) {
                            result->bad_gcr_count++;
                        }
                        ptr += 5;
                    }
                    
                    /* Check data block marker */
                    if (sector_buf[0] != 0x07) {
                        if (error_code == 0x01) {
                            error_code = 0x04;  /* DATA_NOT_FOUND */
                        }
                    }
                    
                    /* Calculate and verify data checksum */
                    blk_checksum = 0;
                    for (i = 1; i <= 256; i++) {
                        blk_checksum ^= sector_buf[i];
                    }
                    
                    if (blk_checksum != sector_buf[257]) {
                        if (error_code == 0x01) {
                            error_code = 0x05;  /* BAD_DATA_CHECKSUM */
                        }
                    }
                    
                    /* Copy data to output (bytes 1-256) */
                    memcpy(data_out, sector_buf + 1, 256);
                    
                    if (result) {
                        result->error_code = error_code;
                        result->data_checksum = sector_buf[257];
                        result->data_ok = (error_code == 0x01);
                    }
                    
                    return error_code;
                }
            }
        }
        ptr++;
    }
    
    return 0x02;  /* HEADER_NOT_FOUND */
}

/* ═══════════════════════════════════════════════════════════════════════════
 * SECTOR ENCODE
 * ═══════════════════════════════════════════════════════════════════════════ */

size_t uft_c64_gcr_encode_sector(const uint8_t *data,
                                 int track, int sector,
                                 const uint8_t *disk_id,
                                 uint8_t error,
                                 uint8_t *gcr_out,
                                 size_t gcr_size)
{
    uint8_t *ptr = gcr_out;
    uint8_t data_buf[260];
    uint8_t checksum;
    int i;
    uint8_t gap_len;
    
    if (gcr_size < 360) return 0;
    
    gap_len = uft_c64_sector_gap_length(track);
    
    /* Initialize with gap bytes */
    memset(gcr_out, 0x55, gcr_size);
    
    /* Handle SYNC_NOT_FOUND error - return unformatted sector */
    if (error == 0x03) {
        return gap_len + UFT_C64_SECTOR_SIZE;
    }
    
    /* Handle HEADER_NOT_FOUND error - skip header */
    if (error == 0x02) {
        ptr += 24;  /* Skip header area */
    } else {
        /* Write header sync (5 bytes of $FF) */
        memset(ptr, 0xFF, UFT_C64_SYNC_LENGTH);
        ptr += UFT_C64_SYNC_LENGTH;
        
        /* Encode header */
        uft_c64_gcr_encode_header(track, sector, disk_id, ptr);
        
        /* Handle ID_MISMATCH error */
        if (error == 0x0B) {
            /* XOR the ID in the header */
            /* This is a simplification - proper implementation would
               re-encode with modified ID */
        }
        
        /* Handle BAD_HEADER_CHECKSUM error */
        if (error == 0x09) {
            /* Corrupt the checksum byte */
            /* This would require re-encoding */
        }
        
        ptr += 10;  /* Header GCR */
        
        /* Header gap (9 bytes of $55) */
        memset(ptr, 0x55, UFT_C64_HEADER_GAP_LENGTH);
        ptr += UFT_C64_HEADER_GAP_LENGTH;
    }
    
    /* Handle DATA_NOT_FOUND error - skip data block */
    if (error == 0x04) {
        return (size_t)(ptr - gcr_out) + gap_len;
    }
    
    /* Write data sync */
    memset(ptr, 0xFF, UFT_C64_SYNC_LENGTH);
    ptr += UFT_C64_SYNC_LENGTH;
    
    /* Build data block */
    data_buf[0] = 0x07;  /* Data block marker */
    memcpy(data_buf + 1, data, 256);
    
    /* Calculate checksum */
    checksum = 0;
    for (i = 0; i < 256; i++) {
        checksum ^= data[i];
    }
    
    /* Handle BAD_DATA_CHECKSUM error */
    if (error == 0x05) {
        checksum ^= 0xFF;
    }
    
    data_buf[257] = checksum;
    data_buf[258] = 0x00;  /* Padding */
    data_buf[259] = 0x00;
    
    /* Encode data block (65 groups) */
    for (i = 0; i < 65; i++) {
        uft_c64_gcr_encode_4bytes(data_buf + i * 4, ptr);
        ptr += 5;
    }
    
    /* Tail gap */
    memset(ptr, 0x55, gap_len);
    ptr += gap_len;
    
    return (size_t)(ptr - gcr_out);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * GCR VALIDATION
 * ═══════════════════════════════════════════════════════════════════════════ */

bool uft_c64_gcr_is_bad_at(const uint8_t *gcr_data, size_t length, size_t pos)
{
    if (pos >= length) return false;
    
    uint8_t b0 = gcr_data[pos];
    uint8_t b1 = (pos + 1 < length) ? gcr_data[pos + 1] : gcr_data[0];
    
    /* Check high 5 bits of current byte */
    uint8_t gcr_hi = (b0 >> 3) & 0x1F;
    if (gcr_decode_high[gcr_hi] == 0xFF && gcr_hi != 0x1F) {
        return true;
    }
    
    /* Check 5 bits spanning byte boundary */
    uint16_t pair = ((uint16_t)b0 << 8) | b1;
    uint8_t gcr_mid = (pair >> 6) & 0x1F;
    if (gcr_decode_low[gcr_mid] == 0xFF && gcr_mid != 0x1F) {
        return true;
    }
    
    return false;
}

size_t uft_c64_gcr_count_bad(const uint8_t *gcr_data, size_t length)
{
    size_t count = 0;
    for (size_t i = 0; i < length; i++) {
        if (uft_c64_gcr_is_bad_at(gcr_data, length, i)) {
            count++;
        }
    }
    return count;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TRACK OPERATIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

bool uft_c64_gcr_is_formatted(const uint8_t *gcr_data, size_t length)
{
    /* A formatted track should have at least some valid GCR sequences */
    /* Look for minimum 16 consecutive valid GCR bytes */
    
    size_t valid_run = 0;
    
    for (size_t i = 0; i < length; i++) {
        if (!uft_c64_gcr_is_bad_at(gcr_data, length, i)) {
            valid_run++;
            if (valid_run >= 16) {
                return true;
            }
        } else {
            valid_run = 0;
        }
    }
    
    return false;
}

const uint8_t *uft_c64_gcr_find_sector0(const uint8_t *gcr_data,
                                        size_t length,
                                        size_t *sector_len_out)
{
    const uint8_t *ptr = gcr_data;
    const uint8_t *end = gcr_data + length;
    uint8_t header[8];
    
    while (ptr < end - 10) {
        if (ptr[0] == 0xFF && ptr[1] == 0x52) {
            if (uft_c64_gcr_decode_header(ptr + 1, header)) {
                if (header[0] == 0x08 && header[2] == 0) {
                    /* Found sector 0 */
                    if (sector_len_out) {
                        /* Estimate sector length */
                        *sector_len_out = UFT_C64_SECTOR_SIZE;
                    }
                    return ptr;
                }
            }
        }
        ptr++;
    }
    
    return NULL;
}

const uint8_t *uft_c64_gcr_find_sector_gap(const uint8_t *gcr_data,
                                           size_t length,
                                           size_t *gap_len_out)
{
    const uint8_t *ptr = gcr_data;
    const uint8_t *end = gcr_data + length;
    const uint8_t *best_gap = NULL;
    size_t best_len = 0;
    size_t current_len = 0;
    const uint8_t *current_start = NULL;
    
    /* Find longest run of $55 (gap byte) */
    while (ptr < end) {
        if (*ptr == 0x55) {
            if (current_len == 0) {
                current_start = ptr;
            }
            current_len++;
        } else {
            if (current_len > best_len) {
                best_len = current_len;
                best_gap = current_start;
            }
            current_len = 0;
        }
        ptr++;
    }
    
    if (current_len > best_len) {
        best_len = current_len;
        best_gap = current_start;
    }
    
    if (gap_len_out) {
        *gap_len_out = best_len;
    }
    
    return best_gap;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TRACK MANIPULATION
 * ═══════════════════════════════════════════════════════════════════════════ */

size_t uft_c64_gcr_replace_bytes(uint8_t *buffer, size_t length,
                                 uint8_t old_byte, uint8_t new_byte)
{
    size_t count = 0;
    for (size_t i = 0; i < length; i++) {
        if (buffer[i] == old_byte) {
            buffer[i] = new_byte;
            count++;
        }
    }
    return count;
}

size_t uft_c64_gcr_strip_runs(uint8_t *buffer, size_t length,
                              size_t max_length, size_t min_run,
                              uint8_t target)
{
    (void)max_length;  /* Not used in strip */
    
    size_t write_pos = 0;
    size_t run_start = 0;
    size_t run_len = 0;
    
    for (size_t i = 0; i < length; i++) {
        if (buffer[i] == target) {
            if (run_len == 0) {
                run_start = i;
            }
            run_len++;
        } else {
            if (run_len > 0 && run_len < min_run) {
                /* Keep short runs */
                memmove(buffer + write_pos, buffer + run_start, run_len);
                write_pos += run_len;
            }
            /* Skip long runs (strip them) */
            buffer[write_pos++] = buffer[i];
            run_len = 0;
        }
    }
    
    return write_pos;
}

size_t uft_c64_gcr_reduce_runs(uint8_t *buffer, size_t length,
                               size_t max_length, size_t min_run,
                               uint8_t target)
{
    (void)max_length;
    
    size_t write_pos = 0;
    size_t run_len = 0;
    
    for (size_t i = 0; i < length; i++) {
        if (buffer[i] == target) {
            run_len++;
            if (run_len <= min_run) {
                buffer[write_pos++] = buffer[i];
            }
        } else {
            buffer[write_pos++] = buffer[i];
            run_len = 0;
        }
    }
    
    return write_pos;
}

size_t uft_c64_gcr_strip_gaps(uint8_t *buffer, size_t length)
{
    return uft_c64_gcr_strip_runs(buffer, length, length, 20, 0x55);
}

size_t uft_c64_gcr_reduce_gaps(uint8_t *buffer, size_t length,
                               size_t max_length)
{
    return uft_c64_gcr_reduce_runs(buffer, length, max_length, 8, 0x55);
}

size_t uft_c64_gcr_lengthen_sync(uint8_t *buffer, size_t length,
                                 size_t max_length)
{
    /* This is a complex operation that would require shifting data */
    /* For now, return unchanged */
    (void)max_length;
    return length;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PETSCII CONVERSION
 * ═══════════════════════════════════════════════════════════════════════════ */

char uft_c64_to_petscii(char ascii)
{
    if (ascii >= 'a' && ascii <= 'z') {
        return ascii - 32;  /* Lowercase to uppercase */
    }
    if (ascii >= 'A' && ascii <= 'Z') {
        return ascii + 32;  /* Uppercase to lowercase (shifted) */
    }
    return ascii;
}

char uft_c64_from_petscii(char petscii)
{
    if (petscii >= 'A' && petscii <= 'Z') {
        return petscii + 32;  /* PETSCII uppercase to ASCII lowercase */
    }
    if (petscii >= 'a' && petscii <= 'z') {
        return petscii - 32;  /* PETSCII shifted to ASCII uppercase */
    }
    return petscii;
}

void uft_c64_str_to_petscii(char *str, size_t len)
{
    for (size_t i = 0; i < len && str[i]; i++) {
        str[i] = uft_c64_to_petscii(str[i]);
    }
}

void uft_c64_str_from_petscii(char *str, size_t len)
{
    for (size_t i = 0; i < len && str[i]; i++) {
        str[i] = uft_c64_from_petscii(str[i]);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TRACK CYCLE DETECTION
 * ═══════════════════════════════════════════════════════════════════════════ */

size_t uft_c64_gcr_find_cycle_headers(const uint8_t *gcr_data,
                                      size_t max_length,
                                      size_t cap_min, size_t cap_max,
                                      const uint8_t **cycle_start,
                                      const uint8_t **cycle_end)
{
    /* Find track cycle by looking for repeating sector headers */
    const uint8_t *ptr = gcr_data;
    const uint8_t *end = gcr_data + max_length;
    const uint8_t *first_header = NULL;
    uint8_t first_hdr_data[8];
    
    /* Find first header */
    while (ptr < end - 10) {
        if (ptr[0] == 0xFF && ptr[1] == 0x52) {
            if (uft_c64_gcr_decode_header(ptr + 1, first_hdr_data)) {
                if (first_hdr_data[0] == 0x08) {
                    first_header = ptr;
                    break;
                }
            }
        }
        ptr++;
    }
    
    if (!first_header) {
        return 0;
    }
    
    /* Search for same header again within expected range */
    ptr = first_header + cap_min;
    end = first_header + cap_max;
    if (end > gcr_data + max_length) {
        end = gcr_data + max_length;
    }
    
    while (ptr < end - 10) {
        if (ptr[0] == 0xFF && ptr[1] == 0x52) {
            uint8_t hdr_data[8];
            if (uft_c64_gcr_decode_header(ptr + 1, hdr_data)) {
                /* Check if same sector */
                if (hdr_data[0] == first_hdr_data[0] &&
                    hdr_data[2] == first_hdr_data[2] &&
                    hdr_data[3] == first_hdr_data[3]) {
                    
                    if (cycle_start) *cycle_start = first_header;
                    if (cycle_end) *cycle_end = ptr;
                    return (size_t)(ptr - first_header);
                }
            }
        }
        ptr++;
    }
    
    return 0;
}

size_t uft_c64_gcr_find_cycle_syncs(const uint8_t *gcr_data,
                                    size_t max_length,
                                    size_t cap_min, size_t cap_max,
                                    const uint8_t **cycle_start,
                                    const uint8_t **cycle_end)
{
    /* Find cycle by counting sync marks */
    const uint8_t *ptr = gcr_data;
    const uint8_t *end = gcr_data + max_length;
    size_t sync_count = 0;
    size_t first_sync_count = 0;
    const uint8_t *first_pos = NULL;
    
    /* Count syncs in first pass */
    while (ptr < end - 1) {
        if (ptr[0] == 0xFF && (ptr[1] & 0x80) && ptr[1] != 0xFF) {
            if (first_pos == NULL) {
                first_pos = ptr;
            }
            sync_count++;
            while (ptr < end && *ptr == 0xFF) ptr++;
        } else {
            ptr++;
        }
        
        size_t offset = (size_t)(ptr - gcr_data);
        if (offset >= cap_min && offset <= cap_max) {
            if (first_sync_count == 0) {
                first_sync_count = sync_count;
            } else if (sync_count == first_sync_count * 2) {
                /* Found cycle */
                if (cycle_start) *cycle_start = first_pos;
                if (cycle_end) *cycle_end = ptr;
                return offset;
            }
        }
    }
    
    return 0;
}

size_t uft_c64_gcr_find_cycle_raw(const uint8_t *gcr_data,
                                  size_t max_length,
                                  size_t cap_min, size_t cap_max,
                                  const uint8_t **cycle_start,
                                  const uint8_t **cycle_end)
{
    /* Brute force: compare bytes at offset with start */
    for (size_t len = cap_min; len <= cap_max && len < max_length; len++) {
        int match = 1;
        for (size_t i = 0; i < 32 && i + len < max_length; i++) {
            if (gcr_data[i] != gcr_data[i + len]) {
                match = 0;
                break;
            }
        }
        if (match) {
            if (cycle_start) *cycle_start = gcr_data;
            if (cycle_end) *cycle_end = gcr_data + len;
            return len;
        }
    }
    
    return 0;
}

size_t uft_c64_gcr_extract_track(uint8_t *dest,
                                 const uint8_t *source,
                                 size_t source_len,
                                 int track,
                                 uint8_t *align_out)
{
    uft_c64_speed_zone_t zone = uft_c64_speed_zone(track);
    size_t cap_min = uft_c64_track_capacity_min(zone);
    size_t cap_max = uft_c64_track_capacity_max(zone);
    
    const uint8_t *cycle_start = NULL;
    const uint8_t *cycle_end = NULL;
    size_t cycle_len;
    
    /* Try different cycle detection methods */
    cycle_len = uft_c64_gcr_find_cycle_headers(source, source_len,
                                               cap_min, cap_max,
                                               &cycle_start, &cycle_end);
    
    if (cycle_len == 0) {
        cycle_len = uft_c64_gcr_find_cycle_syncs(source, source_len,
                                                 cap_min, cap_max,
                                                 &cycle_start, &cycle_end);
    }
    
    if (cycle_len == 0) {
        cycle_len = uft_c64_gcr_find_cycle_raw(source, source_len,
                                               cap_min, cap_max,
                                               &cycle_start, &cycle_end);
    }
    
    if (cycle_len == 0 || !cycle_start) {
        /* No cycle found, use minimum capacity */
        cycle_len = (source_len < cap_max) ? source_len : cap_max;
        cycle_start = source;
    }
    
    memcpy(dest, cycle_start, cycle_len);
    
    if (align_out) {
        *align_out = 0;  /* ALIGN_NONE */
    }
    
    return cycle_len;
}

size_t uft_c64_gcr_check_errors(const uint8_t *gcr_data, size_t length,
                                int track, const uint8_t *disk_id,
                                char *error_str, size_t error_size)
{
    size_t error_count = 0;
    uint8_t sector_data[256];
    uft_c64_sector_result_t result;
    int num_sectors = uft_c64_sectors_per_track(track);
    
    if (error_str && error_size > 0) {
        error_str[0] = '\0';
    }
    
    for (int s = 0; s < num_sectors; s++) {
        uint8_t err = uft_c64_gcr_decode_sector(gcr_data, gcr_data + length,
                                                track, s, disk_id,
                                                sector_data, &result);
        if (err != 0x01) {
            error_count++;
            if (error_str && error_size > 0) {
                char buf[64];
                size_t cur_len = strlen(error_str);
                snprintf(buf, sizeof(buf), "T%d/S%d:E%02X ", track, s, err);
                if (cur_len + strlen(buf) < error_size) {
                    /* Safe concatenation with explicit bounds check */
                    snprintf(error_str + cur_len, error_size - cur_len, "%s", buf);
                }
            }
        }
    }
    
    return error_count;
}
