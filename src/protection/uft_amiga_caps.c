/**
 * @file uft_amiga_caps.c
 * @brief Amiga CAPS/SPS Protection Detection Implementation
 */

#include "uft/protection/uft_amiga_caps.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*===========================================================================
 * Constants
 *===========================================================================*/

/* Standard track sizes */
#define DD_TRACK_BITS   100000      /**< DD track ~100K bits */
#define HD_TRACK_BITS   200000      /**< HD track ~200K bits */
#define LONG_THRESHOLD  1.05f       /**< 5% over = long track */

/* Amiga disk constants */
#define UFT_AMIGA_SECTOR_SIZE   512     /**< Bytes per sector */
#define UFT_AMIGA_SECTORS       11      /**< Sectors per track */

/* Copylock signatures */
static const uint8_t COPYLOCK_SIG[] = { 0x4E, 0x75, 0x4E, 0x75 }; /* RTS RTS */
static const uint8_t RNC_SIG[] = { 0x52, 0x4E, 0x43 };  /* 'RNC' */

/*===========================================================================
 * CRC32 Implementation
 *===========================================================================*/

static uint32_t crc32_table[256];
static bool crc32_initialized = false;

static void init_crc32_table(void)
{
    if (crc32_initialized) return;
    
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
        }
        crc32_table[i] = crc;
    }
    crc32_initialized = true;
}

uint32_t uft_caps_crc32(const uint8_t *data, size_t size)
{
    init_crc32_table();
    
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < size; i++) {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return ~crc;
}

/*===========================================================================
 * Byte Order Helpers
 *===========================================================================*/

static uint32_t read_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

/*===========================================================================
 * IPF Parsing
 *===========================================================================*/

bool uft_caps_is_ipf(const uint8_t *data, size_t size)
{
    if (!data || size < 12) return false;
    return read_be32(data) == UFT_IPF_CAPS;
}

int uft_caps_parse_header(const uint8_t *data, size_t size,
                          uft_ipf_header_t *header)
{
    if (!data || !header || size < 12) return -1;
    
    header->magic = read_be32(data);
    header->length = read_be32(data + 4);
    header->crc = read_be32(data + 8);
    
    if (header->magic != UFT_IPF_CAPS) return -1;
    
    return 0;
}

int uft_caps_parse_info(const uint8_t *data, size_t size,
                        uft_ipf_info_t *info)
{
    if (!data || !info || size < 96) return -1;
    
    /* Skip header (12 bytes) + record type (4 bytes) */
    const uint8_t *p = data + 16;
    
    info->media_type = read_be32(p); p += 4;
    info->encoder_type = read_be32(p); p += 4;
    info->encoder_rev = read_be32(p); p += 4;
    info->file_key = read_be32(p); p += 4;
    info->file_rev = read_be32(p); p += 4;
    info->origin = read_be32(p); p += 4;
    info->min_track = read_be32(p); p += 4;
    info->max_track = read_be32(p); p += 4;
    info->min_side = read_be32(p); p += 4;
    info->max_side = read_be32(p); p += 4;
    info->creation_date = read_be32(p); p += 4;
    info->creation_time = read_be32(p); p += 4;
    info->platforms = read_be32(p); p += 4;
    info->disk_number = read_be32(p); p += 4;
    info->creator_id = read_be32(p); p += 4;
    
    return 0;
}

int uft_caps_parse_imge(const uint8_t *data, size_t size,
                        uft_ipf_imge_t *imge)
{
    if (!data || !imge || size < 80) return -1;
    
    const uint8_t *p = data;
    
    imge->track = read_be32(p); p += 4;
    imge->side = read_be32(p); p += 4;
    imge->density = read_be32(p); p += 4;
    imge->signal_type = read_be32(p); p += 4;
    imge->track_bytes = read_be32(p); p += 4;
    imge->start_byte_pos = read_be32(p); p += 4;
    imge->start_bit_pos = read_be32(p); p += 4;
    imge->data_bits = read_be32(p); p += 4;
    imge->gap_bits = read_be32(p); p += 4;
    imge->track_bits = read_be32(p); p += 4;
    imge->block_count = read_be32(p); p += 4;
    imge->encoder_process = read_be32(p); p += 4;
    imge->flags = read_be32(p); p += 4;
    imge->data_key = read_be32(p); p += 4;
    
    return 0;
}

/*===========================================================================
 * Protection Detection
 *===========================================================================*/

int uft_caps_detect_copylock(const uint8_t *track_data, size_t size,
                             uint8_t track, float *confidence)
{
    if (!track_data || !confidence) return -1;
    
    *confidence = 0.0f;
    
    /* Copylock is typically on track 0 or track 79 */
    if (track != 0 && track != 79) return 0;
    
    /* Search for Copylock signatures */
    for (size_t i = 0; i < size - 4; i++) {
        if (memcmp(track_data + i, COPYLOCK_SIG, 4) == 0) {
            *confidence += 0.3f;
            break;
        }
    }
    
    /* Search for RNC signature */
    for (size_t i = 0; i < size - 3; i++) {
        if (memcmp(track_data + i, RNC_SIG, 3) == 0) {
            *confidence += 0.3f;
            break;
        }
    }
    
    /* Check for long track (Copylock often uses extended tracks) */
    if (size > DD_TRACK_BITS / 8 * LONG_THRESHOLD) {
        *confidence += 0.2f;
    }
    
    if (*confidence > 1.0f) *confidence = 1.0f;
    
    return 0;
}

int uft_caps_detect_weak_bits(const uint8_t *track_data, size_t size,
                              uft_caps_weak_region_t *regions,
                              size_t max_regions, size_t *count)
{
    if (!track_data || !regions || !count) return -1;
    
    *count = 0;
    
    /* Look for patterns that indicate weak bits */
    /* In raw data, weak bits often show as alternating patterns */
    size_t region_start = 0;
    bool in_region = false;
    int consecutive = 0;
    
    for (size_t i = 1; i < size; i++) {
        /* Check for instability pattern (00/FF alternation) */
        if ((track_data[i] == 0x00 && track_data[i-1] == 0xFF) ||
            (track_data[i] == 0xFF && track_data[i-1] == 0x00)) {
            consecutive++;
            if (!in_region && consecutive >= 4) {
                in_region = true;
                region_start = i - 4;
            }
        } else {
            if (in_region && *count < max_regions) {
                regions[*count].bit_position = region_start * 8;
                regions[*count].bit_length = (i - region_start) * 8;
                regions[*count].variation_count = 2;
                regions[*count].decay_rate = 128;
                (*count)++;
                in_region = false;
            }
            consecutive = 0;
        }
    }
    
    return 0;
}

bool uft_caps_is_long_track(const uft_ipf_imge_t *imge, uft_caps_density_t density)
{
    if (!imge) return false;
    
    uint32_t expected = (density == UFT_CAPS_DENSITY_HD) ? HD_TRACK_BITS : DD_TRACK_BITS;
    float ratio = (float)imge->track_bits / expected;
    
    return ratio > LONG_THRESHOLD;
}

int uft_caps_detect_noflux(const uint8_t *track_data, size_t size,
                           uint32_t *positions, size_t max_pos, size_t *count)
{
    if (!track_data || !positions || !count) return -1;
    
    *count = 0;
    
    /* No-flux areas appear as long runs of 0x00 or 0xAA */
    int run_length = 0;
    size_t run_start = 0;
    uint8_t run_byte = 0;
    
    for (size_t i = 0; i < size; i++) {
        if (track_data[i] == 0x00 || track_data[i] == 0xAA) {
            if (run_length == 0) {
                run_start = i;
                run_byte = track_data[i];
            }
            if (track_data[i] == run_byte) {
                run_length++;
            } else {
                run_length = 1;
                run_start = i;
                run_byte = track_data[i];
            }
        } else {
            if (run_length >= 64 && *count < max_pos) {  /* 64 bytes = significant gap */
                positions[*count] = run_start;
                (*count)++;
            }
            run_length = 0;
        }
    }
    
    return 0;
}

int uft_caps_analyze_density(const uint8_t *track_data, size_t size,
                             float *density_map, size_t map_size)
{
    if (!track_data || !density_map || map_size == 0) return -1;
    
    size_t chunk_size = size / map_size;
    if (chunk_size == 0) chunk_size = 1;
    
    for (size_t i = 0; i < map_size; i++) {
        size_t start = i * chunk_size;
        size_t end = start + chunk_size;
        if (end > size) end = size;
        
        /* Count transitions (bit changes) */
        int transitions = 0;
        for (size_t j = start; j < end - 1; j++) {
            uint8_t xor = track_data[j] ^ track_data[j + 1];
            /* Count bits in XOR (popcount) */
            while (xor) {
                transitions += xor & 1;
                xor >>= 1;
            }
        }
        
        density_map[i] = (float)transitions / (end - start);
    }
    
    return 0;
}

/*===========================================================================
 * Full Analysis
 *===========================================================================*/

int uft_caps_analyze_ipf(const uint8_t *data, size_t size,
                         uft_caps_analysis_t *result)
{
    if (!data || !result || size < 100) return -1;
    
    memset(result, 0, sizeof(*result));
    
    /* Verify IPF signature */
    if (!uft_caps_is_ipf(data, size)) {
        return -1;
    }
    result->valid_ipf = true;
    
    /* Parse through records */
    size_t pos = 0;
    while (pos + 12 <= size) {
        uint32_t record_type = read_be32(data + pos);
        uint32_t record_len = read_be32(data + pos + 4);
        
        if (record_len == 0 || pos + record_len > size) break;
        
        switch (record_type) {
            case UFT_IPF_CAPS:
                /* Main header - skip */
                break;
                
            case UFT_IPF_INFO:
                uft_caps_parse_info(data + pos, record_len, &result->info);
                result->min_track = result->info.min_track;
                result->max_track = result->info.max_track;
                result->sides = result->info.max_side - result->info.min_side + 1;
                break;
                
            case UFT_IPF_IMGE:
                if (result->track_count < UFT_CAPS_MAX_TRACKS) {
                    uft_ipf_imge_t imge;
                    if (uft_caps_parse_imge(data + pos + 16, record_len - 16, &imge) == 0) {
                        uft_caps_track_analysis_t *ta = &result->tracks[result->track_count];
                        ta->track = imge.track;
                        ta->side = imge.side;
                        ta->total_bits = imge.track_bits;
                        ta->data_bits = imge.data_bits;
                        ta->gap_bits = imge.gap_bits;
                        
                        /* Check for long track */
                        if (uft_caps_is_long_track(&imge, result->density)) {
                            ta->is_longtrack = true;
                            ta->protection = UFT_CAPS_PROT_LONGTRACK;
                            ta->protection_confidence = 0.9f;
                            result->long_tracks++;
                        }
                        
                        result->track_count++;
                    }
                }
                break;
                
            case UFT_IPF_CTEI:
            case UFT_IPF_CTEX:
                /* CTRaw data present */
                break;
        }
        
        pos += record_len;
    }
    
    /* Determine primary protection */
    if (result->long_tracks > 0) {
        result->has_protection = true;
        result->primary_protection = UFT_CAPS_PROT_LONGTRACK;
        strncpy(result->protection_name, "Long Tracks", 63);
    } else if (result->weak_tracks > 0) {
        result->has_protection = true;
        result->primary_protection = UFT_CAPS_PROT_WEAK;
        strncpy(result->protection_name, "Weak Bits", 63);
    }
    
    result->overall_confidence = result->has_protection ? 0.85f : 0.3f;
    
    return 0;
}

/*===========================================================================
 * CTRaw Analysis
 *===========================================================================*/

bool uft_caps_has_ctraw(const uint8_t *data, size_t size)
{
    if (!data || size < 100) return false;
    
    size_t pos = 0;
    while (pos + 12 <= size) {
        uint32_t record_type = read_be32(data + pos);
        uint32_t record_len = read_be32(data + pos + 4);
        
        if (record_len == 0 || pos + record_len > size) break;
        
        if (record_type == UFT_IPF_CTEI || record_type == UFT_IPF_CTEX) {
            return true;
        }
        
        pos += record_len;
    }
    
    return false;
}

int uft_caps_analyze_ctraw(const uint8_t *data, size_t size,
                           uft_ctraw_analysis_t *result)
{
    if (!data || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    result->has_ctraw = uft_caps_has_ctraw(data, size);
    
    if (!result->has_ctraw) return 0;
    
    /* Default CTRaw parameters */
    result->sample_rate_mhz = 25.0f;  /* 25 MHz typical */
    result->index_time_ms = 200.0f;   /* 200ms rotation */
    result->signal_quality = 0.9f;
    
    return 0;
}

int uft_ctraw_extract_flux(const uint8_t *ctraw_data, size_t size,
                           uint32_t *flux_times, size_t max_flux,
                           size_t *flux_count)
{
    if (!ctraw_data || !flux_times || !flux_count) return -1;
    
    *flux_count = 0;
    
    /* CTRaw format: variable-length encoded flux times */
    size_t pos = 0;
    while (pos < size && *flux_count < max_flux) {
        uint8_t b = ctraw_data[pos++];
        
        if (b < 0x80) {
            /* 7-bit value */
            flux_times[*flux_count] = b;
            (*flux_count)++;
        } else if (b < 0xC0 && pos < size) {
            /* 14-bit value */
            uint32_t val = ((b & 0x3F) << 8) | ctraw_data[pos++];
            flux_times[*flux_count] = val;
            (*flux_count)++;
        } else if (pos + 2 < size) {
            /* 21-bit value */
            uint32_t val = ((b & 0x1F) << 16) | 
                          (ctraw_data[pos] << 8) | 
                          ctraw_data[pos + 1];
            pos += 2;
            flux_times[*flux_count] = val;
            (*flux_count)++;
        }
    }
    
    return 0;
}

/*===========================================================================
 * Conversion
 *===========================================================================*/

int uft_caps_to_adf_track(const uint8_t *ipf_track, size_t size,
                          const uft_ipf_imge_t *imge,
                          uint8_t *adf_data, size_t *adf_size)
{
    if (!ipf_track || !imge || !adf_data || !adf_size) return -1;
    
    /* ADF track = 11 sectors * 512 bytes = 5632 bytes */
    const size_t ADF_TRACK_SIZE = UFT_AMIGA_SECTORS * UFT_AMIGA_SECTOR_SIZE;
    
    if (*adf_size < ADF_TRACK_SIZE) {
        *adf_size = ADF_TRACK_SIZE;
        return -1;
    }
    
    /* Initialize output */
    memset(adf_data, 0, ADF_TRACK_SIZE);
    
    /* 
     * Amiga MFM sector format:
     * - Sync: 0xAAAA 0x4489 0x4489 (MFM encoded)
     * - Header: format, track, sector, sectors_to_gap (4 bytes, odd/even split)
     * - Header checksum (4 bytes, odd/even)
     * - Data checksum (4 bytes, odd/even)
     * - Data (512 bytes, odd/even split = 1024 MFM bytes)
     */
    
    /* Sync pattern in MFM: 0x4489 0x4489 */
    const uint8_t sync_pattern[] = { 0x44, 0x89, 0x44, 0x89 };
    (void)sync_pattern;  /* Used for documentation, comparison uses literals */
    
    size_t pos = 0;
    int sectors_found = 0;
    uint8_t sector_bitmap = 0;  /* Track which sectors we found */
    
    while (pos + 4 < size && sectors_found < UFT_AMIGA_SECTORS) {
        /* Search for sync pattern */
        size_t sync_pos = pos;
        bool found = false;
        
        while (sync_pos + 4 < size) {
            if (ipf_track[sync_pos] == 0x44 && ipf_track[sync_pos+1] == 0x89 &&
                ipf_track[sync_pos+2] == 0x44 && ipf_track[sync_pos+3] == 0x89) {
                found = true;
                break;
            }
            sync_pos++;
        }
        
        if (!found) break;
        
        /* Position after sync */
        pos = sync_pos + 4;
        
        /* Need at least header (8) + header checksum (8) + data checksum (8) + data (1024) */
        if (pos + 1048 > size) break;
        
        /* 
         * Decode header (odd/even split MFM)
         * Odd bits at pos+0..3, even bits at pos+4..7
         */
        uint8_t header[4];
        for (int i = 0; i < 4; i++) {
            uint8_t odd = ipf_track[pos + i];
            uint8_t even = ipf_track[pos + 4 + i];
            /* Combine odd/even bits */
            header[i] = ((odd & 0x55) << 1) | (even & 0x55);
        }
        
        uint8_t format = header[0];
        uint8_t track_num = header[1];
        uint8_t sector_num = header[2];
        uint8_t sectors_to_gap = header[3];
        
        /* Validate */
        if (format != 0xFF || sector_num >= UFT_AMIGA_SECTORS) {
            pos++;
            continue;
        }
        
        /* Skip header (8 bytes) + header checksum (8 bytes) + data checksum (8 bytes) */
        size_t data_pos = pos + 8 + 8 + 8;
        
        if (data_pos + 1024 > size) break;
        
        /*
         * Decode sector data (odd/even split MFM)
         * Odd bits at data_pos+0..511, even bits at data_pos+512..1023
         */
        uint8_t sector_data[UFT_AMIGA_SECTOR_SIZE];
        const uint8_t *odd_data = ipf_track + data_pos;
        const uint8_t *even_data = ipf_track + data_pos + 512;
        
        for (int i = 0; i < UFT_AMIGA_SECTOR_SIZE; i++) {
            uint8_t odd = odd_data[i];
            uint8_t even = even_data[i];
            sector_data[i] = ((odd & 0x55) << 1) | (even & 0x55);
        }
        
        /* Store in ADF output */
        size_t adf_offset = sector_num * UFT_AMIGA_SECTOR_SIZE;
        memcpy(adf_data + adf_offset, sector_data, UFT_AMIGA_SECTOR_SIZE);
        
        sector_bitmap |= (1 << sector_num);
        sectors_found++;
        
        /* Move past this sector */
        pos = data_pos + 1024;
        
        (void)track_num;
        (void)sectors_to_gap;
    }
    
    /* Check if we found all sectors */
    if (sectors_found < UFT_AMIGA_SECTORS) {
        /* Fill missing sectors with pattern */
        for (int s = 0; s < UFT_AMIGA_SECTORS; s++) {
            if (!(sector_bitmap & (1 << s))) {
                /* Mark as unreadable with pattern */
                memset(adf_data + s * UFT_AMIGA_SECTOR_SIZE, 0xAA, UFT_AMIGA_SECTOR_SIZE);
            }
        }
    }
    
    *adf_size = ADF_TRACK_SIZE;
    (void)size;
    (void)imge;
    
    return (sectors_found == UFT_AMIGA_SECTORS) ? 0 : sectors_found;
}

int uft_adf_to_caps_track(const uint8_t *adf_data, size_t adf_size,
                          uint8_t track, uint8_t side,
                          uint8_t *ipf_data, size_t *ipf_size)
{
    if (!adf_data || !ipf_data || !ipf_size) return -1;
    
    const size_t ADF_TRACK_SIZE = UFT_AMIGA_SECTORS * UFT_AMIGA_SECTOR_SIZE;
    
    /* Check input */
    if (adf_size < ADF_TRACK_SIZE) return -1;
    
    /*
     * Amiga MFM track structure per sector:
     * - Gap: ~2 bytes (0xAAAA)
     * - Sync: 4 bytes (0x4489 0x4489)
     * - Header odd: 4 bytes
     * - Header even: 4 bytes
     * - Header checksum odd/even: 8 bytes
     * - Data checksum odd/even: 8 bytes
     * - Data odd: 512 bytes
     * - Data even: 512 bytes
     * Total per sector: ~1050 bytes MFM
     * Track total: ~11550 bytes + gaps
     */
    
    /* Estimate output size: ~12800 bytes for safety */
    const size_t ESTIMATED_SIZE = 12800;
    if (*ipf_size < ESTIMATED_SIZE) {
        *ipf_size = ESTIMATED_SIZE;
        return -1;
    }
    
    size_t pos = 0;
    
    /* Initial gap */
    for (int i = 0; i < 80; i++) {
        ipf_data[pos++] = 0xAA;
    }
    
    for (int sector = 0; sector < UFT_AMIGA_SECTORS; sector++) {
        /* Pre-gap */
        for (int i = 0; i < 2; i++) {
            ipf_data[pos++] = 0xAA;
        }
        
        /* Sync pattern */
        ipf_data[pos++] = 0x44;
        ipf_data[pos++] = 0x89;
        ipf_data[pos++] = 0x44;
        ipf_data[pos++] = 0x89;
        
        /* Header: format=0xFF, track, sector, sectors_to_gap */
        uint8_t header[4] = { 0xFF, (track * 2) + side, (uint8_t)sector, 
                              (uint8_t)(UFT_AMIGA_SECTORS - sector) };
        
        /* Encode header as odd/even MFM */
        uint8_t header_odd[4], header_even[4];
        for (int i = 0; i < 4; i++) {
            header_odd[i] = (header[i] >> 1) & 0x55;
            header_even[i] = header[i] & 0x55;
            /* Add clock bits */
            header_odd[i] |= 0xAA & ~(header_odd[i] << 1) & ~(header_odd[i] >> 1);
            header_even[i] |= 0xAA & ~(header_even[i] << 1) & ~(header_even[i] >> 1);
        }
        
        memcpy(ipf_data + pos, header_odd, 4);
        pos += 4;
        memcpy(ipf_data + pos, header_even, 4);
        pos += 4;
        
        /* Header checksum (XOR of header odd/even words) */
        uint32_t hdr_check = 0;
        for (int i = 0; i < 4; i++) {
            hdr_check ^= header_odd[i];
            hdr_check ^= header_even[i];
        }
        uint8_t hdr_check_odd[4] = {0, 0, 0, (uint8_t)((hdr_check >> 1) & 0x55)};
        uint8_t hdr_check_even[4] = {0, 0, 0, (uint8_t)(hdr_check & 0x55)};
        memcpy(ipf_data + pos, hdr_check_odd, 4);
        pos += 4;
        memcpy(ipf_data + pos, hdr_check_even, 4);
        pos += 4;
        
        /* Get sector data */
        const uint8_t *sect_data = adf_data + sector * UFT_AMIGA_SECTOR_SIZE;
        
        /* Data checksum */
        uint32_t data_check = 0;
        for (int i = 0; i < UFT_AMIGA_SECTOR_SIZE; i++) {
            data_check ^= sect_data[i];
        }
        uint8_t data_check_odd[4] = {0, 0, 0, (uint8_t)((data_check >> 1) & 0x55)};
        uint8_t data_check_even[4] = {0, 0, 0, (uint8_t)(data_check & 0x55)};
        memcpy(ipf_data + pos, data_check_odd, 4);
        pos += 4;
        memcpy(ipf_data + pos, data_check_even, 4);
        pos += 4;
        
        /* Encode data as odd/even MFM */
        for (int i = 0; i < UFT_AMIGA_SECTOR_SIZE; i++) {
            ipf_data[pos + i] = (sect_data[i] >> 1) & 0x55;
            ipf_data[pos + i] |= 0xAA & ~(ipf_data[pos + i] << 1);
        }
        pos += UFT_AMIGA_SECTOR_SIZE;
        
        for (int i = 0; i < UFT_AMIGA_SECTOR_SIZE; i++) {
            ipf_data[pos + i] = sect_data[i] & 0x55;
            ipf_data[pos + i] |= 0xAA & ~(ipf_data[pos + i] << 1);
        }
        pos += UFT_AMIGA_SECTOR_SIZE;
    }
    
    /* Final gap */
    while (pos < ESTIMATED_SIZE - 80) {
        ipf_data[pos++] = 0xAA;
    }
    
    *ipf_size = pos;
    return 0;
}

/*===========================================================================
 * Utilities
 *===========================================================================*/

const char *uft_caps_protection_name(uft_caps_protection_t prot)
{
    switch (prot) {
        case UFT_CAPS_PROT_NONE:     return "None";
        case UFT_CAPS_PROT_COPYLOCK: return "Copylock";
        case UFT_CAPS_PROT_SPEEDLOCK:return "Speedlock";
        case UFT_CAPS_PROT_LONGTRACK:return "Long Track";
        case UFT_CAPS_PROT_WEAK:     return "Weak Bits";
        case UFT_CAPS_PROT_DENSITY:  return "Variable Density";
        case UFT_CAPS_PROT_NOFLUX:   return "No-Flux Area";
        case UFT_CAPS_PROT_CUSTOM:   return "Custom";
        default:                     return "Unknown";
    }
}

const char *uft_caps_density_name(uft_caps_density_t density)
{
    switch (density) {
        case UFT_CAPS_DENSITY_AUTO: return "Auto";
        case UFT_CAPS_DENSITY_DD:   return "Double Density";
        case UFT_CAPS_DENSITY_HD:   return "High Density";
        case UFT_CAPS_DENSITY_ED:   return "Extended Density";
        default:                    return "Unknown";
    }
}

const char *uft_caps_encoder_name(uft_ipf_encoder_t encoder)
{
    switch (encoder) {
        case UFT_IPF_ENC_CAPS:  return "CAPS";
        case UFT_IPF_ENC_SPS:   return "SPS";
        case UFT_IPF_ENC_CTRAW: return "CTRaw";
        default:               return "Unknown";
    }
}

int uft_caps_report_json(const uft_caps_analysis_t *analysis,
                         char *buffer, size_t size)
{
    if (!analysis || !buffer || size == 0) return -1;
    
    int written = snprintf(buffer, size,
        "{\n"
        "  \"valid_ipf\": %s,\n"
        "  \"tracks\": { \"min\": %u, \"max\": %u, \"sides\": %u },\n"
        "  \"track_count\": %u,\n"
        "  \"protection\": {\n"
        "    \"detected\": %s,\n"
        "    \"type\": \"%s\",\n"
        "    \"name\": \"%s\",\n"
        "    \"confidence\": %.4f\n"
        "  },\n"
        "  \"statistics\": {\n"
        "    \"long_tracks\": %u,\n"
        "    \"weak_tracks\": %u,\n"
        "    \"noflux_tracks\": %u,\n"
        "    \"total_weak_bits\": %u\n"
        "  }\n"
        "}",
        analysis->valid_ipf ? "true" : "false",
        analysis->min_track, analysis->max_track, analysis->sides,
        analysis->track_count,
        analysis->has_protection ? "true" : "false",
        uft_caps_protection_name(analysis->primary_protection),
        analysis->protection_name,
        analysis->overall_confidence,
        analysis->long_tracks,
        analysis->weak_tracks,
        analysis->noflux_tracks,
        analysis->total_weak_bits
    );
    
    return (written > 0 && (size_t)written < size) ? 0 : -1;
}
