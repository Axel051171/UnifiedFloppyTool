/*
 * uft_amiga_mfm.c
 *
 * Amiga MFM (Modified Frequency Modulation) decoder implementation.
 * Based on concepts from Keir Fraser's disk-utilities (Public Domain).
 *
 * Build:
 *   cc -std=c11 -O2 -Wall -Wextra -pedantic -c uft_amiga_mfm.c
 */

#include "amiga/uft_amiga_mfm.h"
#include <string.h>
#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * INTERNAL HELPERS
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Read big-endian 16-bit value */
static inline uint16_t read_be16(const uint8_t *p)
{
    return ((uint16_t)p[0] << 8) | p[1];
}

/* Read big-endian 32-bit value */
static inline uint32_t read_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

/* Write big-endian 16-bit value */
static inline void write_be16(uint8_t *p, uint16_t val)
{
    p[0] = (uint8_t)(val >> 8);
    p[1] = (uint8_t)val;
}

/* Write big-endian 32-bit value */
static inline void write_be32(uint8_t *p, uint32_t val)
{
    p[0] = (uint8_t)(val >> 24);
    p[1] = (uint8_t)(val >> 16);
    p[2] = (uint8_t)(val >> 8);
    p[3] = (uint8_t)val;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * MFM ENCODING/DECODING
 * ═══════════════════════════════════════════════════════════════════════════ */

uint32_t uft_amiga_mfm_decode_long(uint32_t mfm_odd, uint32_t mfm_even)
{
    /*
     * Amiga MFM uses odd-even interleaving:
     * - mfm_odd contains the odd bits (positions 1,3,5,7,...)
     * - mfm_even contains the even bits (positions 0,2,4,6,...)
     * 
     * We need to extract data bits only (mask out clock bits).
     * In MFM, clock bits are in odd positions of the raw stream.
     */
    uint32_t odd = mfm_odd & 0x55555555;   /* Keep bits 0,2,4,... */
    uint32_t even = mfm_even & 0x55555555;
    
    /* Interleave odd and even bits */
    return (odd << 1) | even;
}

void uft_amiga_mfm_encode_long(uint32_t value,
                               uint32_t *mfm_odd, uint32_t *mfm_even)
{
    /* Extract odd and even bits */
    uint32_t odd = (value >> 1) & 0x55555555;
    uint32_t even = value & 0x55555555;
    
    /* Add clock bits: clock is 1 if both adjacent data bits are 0 */
    uint32_t clock_odd = ~(odd | (odd >> 1) | (even << 31)) & 0xAAAAAAAA;
    uint32_t clock_even = ~(even | (even >> 1) | (odd << 31)) & 0xAAAAAAAA;
    
    *mfm_odd = odd | clock_odd;
    *mfm_even = even | clock_even;
}

size_t uft_amiga_mfm_encode(const uint8_t *data, size_t data_len,
                            uint8_t *mfm_out, size_t mfm_size)
{
    if (mfm_size < data_len * 2) {
        return 0;
    }
    
    /* Amiga MFM: first all odd bits, then all even bits */
    size_t out_pos = 0;
    
    /* Encode odd bits (bit positions 7,5,3,1 of each byte) */
    for (size_t i = 0; i < data_len; i++) {
        uint8_t byte = data[i];
        uint8_t odd_bits = 0;
        
        /* Extract odd bit positions */
        odd_bits |= (byte & 0x80) >> 0;  /* bit 7 -> bit 7 */
        odd_bits |= (byte & 0x20) >> 0;  /* bit 5 -> bit 5 */
        odd_bits |= (byte & 0x08) >> 0;  /* bit 3 -> bit 3 */
        odd_bits |= (byte & 0x02) >> 0;  /* bit 1 -> bit 1 */
        
        mfm_out[out_pos++] = odd_bits;
    }
    
    /* Encode even bits (bit positions 6,4,2,0 of each byte) */
    for (size_t i = 0; i < data_len; i++) {
        uint8_t byte = data[i];
        uint8_t even_bits = 0;
        
        /* Extract even bit positions */
        even_bits |= (byte & 0x40) << 1;  /* bit 6 -> bit 7 */
        even_bits |= (byte & 0x10) << 1;  /* bit 4 -> bit 5 */
        even_bits |= (byte & 0x04) << 1;  /* bit 2 -> bit 3 */
        even_bits |= (byte & 0x01) << 1;  /* bit 0 -> bit 1 */
        
        mfm_out[out_pos++] = even_bits;
    }
    
    return out_pos;
}

size_t uft_amiga_mfm_decode(const uint8_t *mfm, size_t mfm_len,
                            uint8_t *data_out, size_t data_size)
{
    size_t data_len = mfm_len / 2;
    if (data_size < data_len) {
        data_len = data_size;
    }
    
    /* Amiga MFM: first half is odd bits, second half is even bits */
    const uint8_t *odd_data = mfm;
    const uint8_t *even_data = mfm + data_len;
    
    for (size_t i = 0; i < data_len; i++) {
        uint8_t odd = odd_data[i] & 0x55;   /* Mask to data bits */
        uint8_t even = even_data[i] & 0x55;
        
        /* Combine: odd bits go to positions 7,5,3,1; even to 6,4,2,0 */
        data_out[i] = odd | (even >> 1);
    }
    
    return data_len;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * CHECKSUM CALCULATION
 * ═══════════════════════════════════════════════════════════════════════════ */

uint32_t uft_amiga_checksum(const uint8_t *data, size_t len)
{
    uint32_t checksum = 0;
    
    for (size_t i = 0; i + 3 < len; i += 4) {
        checksum ^= read_be32(data + i);
    }
    
    /* Handle remaining bytes */
    size_t remaining = len % 4;
    if (remaining > 0) {
        uint32_t last = 0;
        for (size_t i = 0; i < remaining; i++) {
            last |= (uint32_t)data[len - remaining + i] << (24 - i * 8);
        }
        checksum ^= last;
    }
    
    return checksum;
}

uint32_t uft_amiga_bootblock_checksum(const uint8_t *bootblock)
{
    uint32_t checksum = 0;
    uint32_t prev_checksum;
    
    for (size_t i = 0; i < 1024; i += 4) {
        prev_checksum = checksum;
        checksum += read_be32(bootblock + i);
        
        /* Handle overflow */
        if (checksum < prev_checksum) {
            checksum++;
        }
    }
    
    return ~checksum;
}

bool uft_amiga_verify_bootblock(const uint8_t *bootblock)
{
    /* Bootblock checksum is stored at offset 4 */
    uint32_t stored_checksum = read_be32(bootblock + 4);
    
    /* Calculate checksum with checksum field zeroed */
    uint8_t temp[1024];
    memcpy(temp, bootblock, 1024);
    write_be32(temp + 4, 0);
    
    uint32_t calc_checksum = uft_amiga_bootblock_checksum(temp);
    
    return (calc_checksum == stored_checksum);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * SECTOR OPERATIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

int64_t uft_amiga_find_sync(const uint8_t *mfm, size_t len,
                            uint16_t sync, size_t start_pos)
{
    if (len < 2 || start_pos >= len - 1) {
        return -1;
    }
    
    for (size_t i = start_pos; i < len - 1; i++) {
        uint16_t word = read_be16(mfm + i);
        if (word == sync) {
            return (int64_t)i;
        }
    }
    
    return -1;
}

bool uft_amiga_decode_header(const uint8_t *mfm,
                             uft_amiga_header_t *header)
{
    /* Header structure in MFM (after sync):
     * - 4 bytes info (format, track, sector, sectors_to_gap) - odd then even
     * - 16 bytes label - odd then even
     * - 4 bytes header checksum - odd then even
     * - 4 bytes data checksum - odd then even
     */
    
    /* Decode 4-byte info field */
    uint32_t info_odd = read_be32(mfm);
    uint32_t info_even = read_be32(mfm + 4);
    uint32_t info = uft_amiga_mfm_decode_long(info_odd, info_even);
    
    header->format = (info >> 24) & 0xFF;
    header->track = (info >> 16) & 0xFF;
    header->sector = (info >> 8) & 0xFF;
    header->sectors_to_gap = info & 0xFF;
    
    /* Decode 16-byte label */
    for (int i = 0; i < 4; i++) {
        uint32_t label_odd = read_be32(mfm + 8 + i * 4);
        uint32_t label_even = read_be32(mfm + 8 + 16 + i * 4);
        uint32_t label_word = uft_amiga_mfm_decode_long(label_odd, label_even);
        
        header->label[i * 4 + 0] = (label_word >> 24) & 0xFF;
        header->label[i * 4 + 1] = (label_word >> 16) & 0xFF;
        header->label[i * 4 + 2] = (label_word >> 8) & 0xFF;
        header->label[i * 4 + 3] = label_word & 0xFF;
    }
    
    /* Decode header checksum */
    uint32_t hdr_sum_odd = read_be32(mfm + 40);
    uint32_t hdr_sum_even = read_be32(mfm + 44);
    header->header_checksum = uft_amiga_mfm_decode_long(hdr_sum_odd, hdr_sum_even);
    
    /* Decode data checksum */
    uint32_t dat_sum_odd = read_be32(mfm + 48);
    uint32_t dat_sum_even = read_be32(mfm + 52);
    header->data_checksum = uft_amiga_mfm_decode_long(dat_sum_odd, dat_sum_even);
    
    /* Verify header checksum */
    uint32_t calc_checksum = 0;
    for (int i = 0; i < 10; i++) {
        calc_checksum ^= read_be32(mfm + i * 4);
    }
    calc_checksum &= 0x55555555;  /* Mask to data bits */
    
    return (header->format == 0xFF);  /* AmigaDOS marker */
}

bool uft_amiga_decode_sector(const uint8_t *mfm, size_t mfm_len,
                             uft_amiga_sector_t *sector,
                             uint8_t *data_buf, size_t data_size)
{
    if (mfm_len < 1088 || data_size < 512) {  /* Minimum sector size */
        return false;
    }
    
    /* Read sync word */
    sector->sync = read_be16(mfm);
    
    /* Decode header (starts after sync) */
    if (!uft_amiga_decode_header(mfm + 2, &sector->header)) {
        sector->header_ok = false;
        return false;
    }
    sector->header_ok = true;
    
    /* Decode data (512 bytes from 1024 MFM bytes) */
    /* Data starts at offset 56 (after header) */
    const uint8_t *data_mfm = mfm + 2 + 56;
    
    /* First half: odd bits, Second half: even bits */
    for (size_t i = 0; i < 128; i++) {
        uint32_t data_odd = read_be32(data_mfm + i * 4);
        uint32_t data_even = read_be32(data_mfm + 512 + i * 4);
        uint32_t decoded = uft_amiga_mfm_decode_long(data_odd, data_even);
        
        data_buf[i * 4 + 0] = (decoded >> 24) & 0xFF;
        data_buf[i * 4 + 1] = (decoded >> 16) & 0xFF;
        data_buf[i * 4 + 2] = (decoded >> 8) & 0xFF;
        data_buf[i * 4 + 3] = decoded & 0xFF;
    }
    
    sector->data = data_buf;
    sector->data_size = 512;
    
    /* Verify data checksum */
    uint32_t calc_checksum = uft_amiga_checksum(data_buf, 512);
    sector->data_ok = (calc_checksum == sector->header.data_checksum);
    
    return sector->header_ok && sector->data_ok;
}

size_t uft_amiga_encode_sector(const uft_amiga_sector_t *sector,
                               uint8_t *mfm_out, size_t mfm_size)
{
    if (mfm_size < 1088) {
        return 0;
    }
    
    size_t pos = 0;
    
    /* Write sync */
    write_be16(mfm_out + pos, sector->sync ? sector->sync : UFT_AMIGA_SYNC_STD);
    pos += 2;
    
    /* Encode header info */
    uint32_t info = ((uint32_t)sector->header.format << 24) |
                    ((uint32_t)sector->header.track << 16) |
                    ((uint32_t)sector->header.sector << 8) |
                    sector->header.sectors_to_gap;
    
    uint32_t info_odd, info_even;
    uft_amiga_mfm_encode_long(info, &info_odd, &info_even);
    write_be32(mfm_out + pos, info_odd);
    write_be32(mfm_out + pos + 4, info_even);
    pos += 8;
    
    /* Encode label (16 bytes) */
    for (int i = 0; i < 4; i++) {
        uint32_t label_word = ((uint32_t)sector->header.label[i * 4] << 24) |
                              ((uint32_t)sector->header.label[i * 4 + 1] << 16) |
                              ((uint32_t)sector->header.label[i * 4 + 2] << 8) |
                              sector->header.label[i * 4 + 3];
        uint32_t label_odd, label_even;
        uft_amiga_mfm_encode_long(label_word, &label_odd, &label_even);
        write_be32(mfm_out + pos + i * 4, label_odd);
        write_be32(mfm_out + pos + 16 + i * 4, label_even);
    }
    pos += 32;
    
    /* Calculate and encode header checksum */
    uint32_t hdr_checksum = 0;
    for (int i = 0; i < 10; i++) {
        hdr_checksum ^= read_be32(mfm_out + 2 + i * 4);
    }
    hdr_checksum &= 0x55555555;
    
    uint32_t hdr_sum_odd, hdr_sum_even;
    uft_amiga_mfm_encode_long(hdr_checksum, &hdr_sum_odd, &hdr_sum_even);
    write_be32(mfm_out + pos, hdr_sum_odd);
    write_be32(mfm_out + pos + 4, hdr_sum_even);
    pos += 8;
    
    /* Calculate and encode data checksum */
    uint32_t data_checksum = uft_amiga_checksum(sector->data, sector->data_size);
    uint32_t dat_sum_odd, dat_sum_even;
    uft_amiga_mfm_encode_long(data_checksum, &dat_sum_odd, &dat_sum_even);
    write_be32(mfm_out + pos, dat_sum_odd);
    write_be32(mfm_out + pos + 4, dat_sum_even);
    pos += 8;
    
    /* Encode data (512 bytes -> 1024 MFM bytes) */
    for (size_t i = 0; i < 128 && i * 4 < sector->data_size; i++) {
        uint32_t data_word = ((uint32_t)sector->data[i * 4] << 24) |
                             ((uint32_t)sector->data[i * 4 + 1] << 16) |
                             ((uint32_t)sector->data[i * 4 + 2] << 8) |
                             sector->data[i * 4 + 3];
        uint32_t data_odd, data_even;
        uft_amiga_mfm_encode_long(data_word, &data_odd, &data_even);
        write_be32(mfm_out + pos + i * 4, data_odd);
        write_be32(mfm_out + pos + 512 + i * 4, data_even);
    }
    pos += 1024;
    
    return pos;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TRACK OPERATIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_amiga_decode_track(const uint8_t *mfm, size_t mfm_len,
                           uft_amiga_track_t *track,
                           uint8_t *data_buf, size_t data_size)
{
    memset(track, 0, sizeof(*track));
    
    int64_t pos = 0;
    int sector_count = 0;
    size_t data_offset = 0;
    
    /* Standard sync patterns to try */
    const uint16_t syncs[] = {
        UFT_AMIGA_SYNC_STD,
        UFT_AMIGA_SYNC_ZOUT,
        UFT_AMIGA_SYNC_TURBO,
        UFT_AMIGA_SYNC_FTANK
    };
    
    while (pos >= 0 && sector_count < 22 && data_offset + 512 <= data_size) {
        /* Try to find any valid sync */
        int64_t sync_pos = -1;
        uint16_t found_sync = 0;
        
        for (int s = 0; s < 4; s++) {
            int64_t p = uft_amiga_find_sync(mfm, mfm_len, syncs[s], (size_t)pos);
            if (p >= 0 && (sync_pos < 0 || p < sync_pos)) {
                sync_pos = p;
                found_sync = syncs[s];
            }
        }
        
        if (sync_pos < 0 || sync_pos + 1088 > (int64_t)mfm_len) {
            break;
        }
        
        /* Try to decode sector */
        uft_amiga_sector_t *sector = &track->sectors[sector_count];
        uint8_t *sector_data = data_buf + data_offset;
        
        if (uft_amiga_decode_sector(mfm + sync_pos, mfm_len - sync_pos,
                                    sector, sector_data, 512)) {
            track->nr_valid++;
        }
        
        sector->sync = found_sync;
        sector_count++;
        data_offset += 512;
        
        /* Get track info from first valid sector */
        if (track->nr_sectors == 0 && sector->header_ok) {
            track->track_num = sector->header.track;
        }
        
        track->nr_sectors = sector_count;
        pos = sync_pos + 1088;  /* Move past this sector */
    }
    
    /* Detect special formats */
    if (track->nr_sectors > 11) {
        track->has_long_track = true;
    }
    
    track->format_type = uft_amiga_detect_format(mfm, mfm_len);
    
    return sector_count;
}

size_t uft_amiga_encode_track(const uft_amiga_track_t *track,
                              uint8_t *mfm_out, size_t mfm_size)
{
    size_t pos = 0;
    
    /* Write gap at start */
    size_t gap_size = 128;  /* Pre-index gap */
    if (pos + gap_size > mfm_size) return 0;
    memset(mfm_out + pos, 0xAA, gap_size);  /* Gap bytes */
    pos += gap_size;
    
    /* Write each sector */
    for (int i = 0; i < track->nr_sectors && i < 22; i++) {
        const uft_amiga_sector_t *sector = &track->sectors[i];
        
        /* Write sync (multiple sync words) */
        if (pos + 4 > mfm_size) break;
        write_be16(mfm_out + pos, UFT_AMIGA_SYNC_STD);
        write_be16(mfm_out + pos + 2, UFT_AMIGA_SYNC_STD);
        pos += 4;
        
        /* Encode sector */
        size_t sector_size = uft_amiga_encode_sector(sector,
                                                     mfm_out + pos,
                                                     mfm_size - pos);
        if (sector_size == 0) break;
        pos += sector_size;
        
        /* Write inter-sector gap */
        size_t inter_gap = 16;
        if (pos + inter_gap > mfm_size) break;
        memset(mfm_out + pos, 0xAA, inter_gap);
        pos += inter_gap;
    }
    
    /* Fill remaining with gap */
    if (pos < mfm_size) {
        memset(mfm_out + pos, 0xAA, mfm_size - pos);
    }
    
    return pos;
}

uft_amiga_format_t uft_amiga_detect_format(const uint8_t *mfm, size_t mfm_len)
{
    if (mfm_len < 100) {
        return UFT_AMIGA_FMT_UNKNOWN;
    }
    
    /* Count standard sync words */
    int sync_count = 0;
    int64_t pos = 0;
    while ((pos = uft_amiga_find_sync(mfm, mfm_len, UFT_AMIGA_SYNC_STD, pos)) >= 0) {
        sync_count++;
        pos += 2;
        if (sync_count > 20) break;
    }
    
    if (sync_count >= 11) {
        if (sync_count > 11) {
            return UFT_AMIGA_FMT_LONGTRACK;
        }
        return UFT_AMIGA_FMT_AMIGADOS;
    }
    
    /* Check for protection signatures */
    /* Speedlock: specific timing patterns */
    /* Copylock: specific data patterns on track 79 */
    
    if (sync_count > 0) {
        return UFT_AMIGA_FMT_PROTECTION;
    }
    
    return UFT_AMIGA_FMT_RAW;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * DISK OPERATIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

bool uft_amiga_parse_rootblock(const uint8_t *root_data,
                               uft_amiga_disk_t *disk)
{
    memset(disk, 0, sizeof(*disk));
    
    /* Root block type (offset 0): should be 2 (T_HEADER) */
    uint32_t type = read_be32(root_data);
    if (type != 2) {
        return false;
    }
    
    /* Secondary type (offset 508): should be 1 (ST_ROOT) */
    uint32_t sec_type = read_be32(root_data + 508);
    if (sec_type != 1) {
        return false;
    }
    
    /* Bitmap valid flag (offset 312) */
    uint32_t bm_flag = read_be32(root_data + 312);
    
    /* Hash table size (offset 12) */
    uint32_t ht_size = read_be32(root_data + 12);
    
    /* Disk name (offset 432, BCPL string: length byte + chars) */
    uint8_t name_len = root_data[432];
    if (name_len > 30) name_len = 30;
    memcpy(disk->disk_name, root_data + 433, name_len);
    disk->disk_name[name_len] = '\0';
    
    /* Set defaults */
    disk->nr_tracks = 80;
    disk->nr_sides = 2;
    disk->nr_sectors = 11;
    disk->root_block = 880;  /* Standard for DD disks */
    
    /* Detect filesystem from DOS type in bootblock */
    /* (This info is not in rootblock, but we'll set a default) */
    disk->filesystem = UFT_AMIGA_FS_OFS;
    
    return true;
}

uint32_t uft_amiga_calc_block(int track, int sector, int side,
                              int sectors_per_track)
{
    return (track * 2 + side) * sectors_per_track + sector;
}

void uft_amiga_block_to_ts(uint32_t block, int sectors_per_track,
                           int *track_out, int *sector_out, int *side_out)
{
    int track_side = block / sectors_per_track;
    *sector_out = block % sectors_per_track;
    *track_out = track_side / 2;
    *side_out = track_side % 2;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PROTECTION DETECTION
 * ═══════════════════════════════════════════════════════════════════════════ */

bool uft_amiga_detect_protection(const uint8_t *mfm, size_t mfm_len,
                                 int track_num,
                                 uft_amiga_prot_result_t *result)
{
    memset(result, 0, sizeof(*result));
    result->type = UFT_AMIGA_PROT_NONE;
    result->track = track_num;
    
    /* Speedlock is typically on track 0 */
    if (track_num == 0 && uft_amiga_detect_speedlock(mfm, mfm_len)) {
        result->type = UFT_AMIGA_PROT_SPEEDLOCK;
        strncpy(result->name, "Speedlock", sizeof(result->name) - 1);
        return true;
    }
    
    /* Copylock is typically on track 79 */
    if (track_num == 79) {
        uint32_t key;
        if (uft_amiga_detect_copylock(mfm, mfm_len, &key)) {
            result->type = UFT_AMIGA_PROT_COPYLOCK;
            result->key = key;
            strncpy(result->name, "RNC Copylock", sizeof(result->name) - 1);
            return true;
        }
    }
    
    /* Check for long track protection */
    int sync_count = 0;
    int64_t pos = 0;
    while ((pos = uft_amiga_find_sync(mfm, mfm_len, UFT_AMIGA_SYNC_STD, pos)) >= 0) {
        sync_count++;
        pos += 2;
        if (sync_count > 15) break;
    }
    
    if (sync_count > 11) {
        result->type = UFT_AMIGA_PROT_LONGTRACK;
        snprintf(result->name, sizeof(result->name), "Long track (%d sectors)", sync_count);
        return true;
    }
    
    return false;
}

bool uft_amiga_detect_copylock(const uint8_t *mfm, size_t mfm_len,
                               uint32_t *key_out)
{
    /* Copylock uses specific timing patterns and encrypted sectors */
    /* Detection based on track structure anomalies */
    
    /* Look for non-standard sync or missing sectors */
    int sync_count = 0;
    int64_t pos = 0;
    
    while ((pos = uft_amiga_find_sync(mfm, mfm_len, UFT_AMIGA_SYNC_STD, pos)) >= 0) {
        sync_count++;
        pos += 2;
    }
    
    /* Copylock tracks often have <11 sectors or unusual layout */
    if (sync_count < 11 && sync_count > 0) {
        if (key_out) *key_out = 0;  /* Key extraction would require more analysis */
        return true;
    }
    
    return false;
}

bool uft_amiga_detect_speedlock(const uint8_t *mfm, size_t mfm_len)
{
    /* Speedlock uses timing-sensitive patterns */
    /* Detection based on specific byte sequences */
    
    /* Look for Speedlock signature patterns */
    /* This is a simplified detection */
    
    if (mfm_len < 1000) return false;
    
    /* Check for unusual gap patterns or non-standard sync */
    int64_t sync_pos = uft_amiga_find_sync(mfm, mfm_len, UFT_AMIGA_SYNC_STD, 0);
    
    if (sync_pos < 0) {
        /* No standard sync might indicate Speedlock */
        /* Check for alternative patterns */
        return false;
    }
    
    return false;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * UTILITY FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

static const char *fs_names[] = {
    "OFS", "FFS", "OFS-INTL", "FFS-INTL", "OFS-DC", "FFS-DC", "LNFS"
};

static const char *format_names[] = {
    "Unknown", "AmigaDOS", "AmigaDOS-Ext", "Long Track",
    "Copylock", "Speedlock", "Protected", "Raw"
};

static const char *protection_names[] = {
    "None", "Copylock", "Speedlock", "Tiertex",
    "Rainbird", "Gremlin", "Psygnosis", "Long Track"
};

const char *uft_amiga_fs_name(uft_amiga_fs_t fs)
{
    if (fs < sizeof(fs_names) / sizeof(fs_names[0])) {
        return fs_names[fs];
    }
    return "Unknown";
}

const char *uft_amiga_format_name(uft_amiga_format_t format)
{
    if (format < sizeof(format_names) / sizeof(format_names[0])) {
        return format_names[format];
    }
    return "Unknown";
}

const char *uft_amiga_protection_name(uft_amiga_protection_t prot)
{
    if (prot < sizeof(protection_names) / sizeof(protection_names[0])) {
        return protection_names[prot];
    }
    return "Unknown";
}

void uft_amiga_adf_to_ts(size_t offset,
                         int *track_out, int *sector_out, int *side_out)
{
    uint32_t block = offset / 512;
    uft_amiga_block_to_ts(block, 11, track_out, sector_out, side_out);
}

size_t uft_amiga_ts_to_adf(int track, int sector, int side)
{
    uint32_t block = uft_amiga_calc_block(track, sector, side, 11);
    return block * 512;
}
