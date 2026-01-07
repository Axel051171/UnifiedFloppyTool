/**
 * @file uft_protection.c
 * @brief Copy Protection Detection Implementation for UFT
 * 
 * - SPS/CAPS preservation documentation
 * 
 * @copyright UFT Project
 */

#include "uft_protection.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*============================================================================
 * CopyLock Sync Mark Table
 *============================================================================*/

const uint16_t uft_copylock_sync_marks[UFT_COPYLOCK_SECTORS] = {
    0x8a91,     /* Sector 0 */
    0x8a44,     /* Sector 1 */
    0x8a45,     /* Sector 2 */
    0x8a51,     /* Sector 3 */
    0x8912,     /* Sector 4 - FAST (5% shorter) */
    0x8911,     /* Sector 5 */
    0x8914,     /* Sector 6 - SLOW (5% longer) */
    0x8915,     /* Sector 7 */
    0x8944,     /* Sector 8 */
    0x8945,     /* Sector 9 */
    0x8951      /* Sector 10 */
};

/*============================================================================
 * LFSR Functions
 *============================================================================*/

uint32_t uft_lfsr_advance(uint32_t state, int steps)
{
    if (steps >= 0) {
        while (steps--) {
            state = uft_lfsr_next(state);
        }
    } else {
        while (steps++) {
            state = uft_lfsr_prev(state);
        }
    }
    return state;
}

/**
 * @brief Seek LFSR between sectors
 * 
 * Accounts for sector 6 signature interruption.
 */
static uint32_t lfsr_seek_sector(uint32_t state, int from, int to, bool old_style)
{
    while (from != to) {
        int step = (from < to) ? 1 : -1;
        
        if (step > 0) {
            /* Forward: process current sector then move */
            int sector_size = 512;
            
            /* In new-style CopyLock, sector 6 signature doesn't interrupt LFSR */
            /* In old-style, it does interrupt */
            if (from == 6 && !old_style) {
                sector_size -= UFT_COPYLOCK_SIG_LEN;
            }
            if (from == 5 && old_style) {
                sector_size += UFT_COPYLOCK_SIG_LEN;
            }
            
            state = uft_lfsr_advance(state, sector_size);
            from++;
        } else {
            /* Backward: move first then process */
            from--;
            int sector_size = 512;
            
            if (from == 6 && !old_style) {
                sector_size -= UFT_COPYLOCK_SIG_LEN;
            }
            if (from == 5 && old_style) {
                sector_size += UFT_COPYLOCK_SIG_LEN;
            }
            
            state = uft_lfsr_advance(state, -sector_size);
        }
    }
    
    return state;
}

/*============================================================================
 * Context Management
 *============================================================================*/

void uft_protection_init(uft_protection_ctx_t* ctx)
{
    if (!ctx) return;
    memset(ctx, 0, sizeof(*ctx));
}

void uft_protection_free(uft_protection_ctx_t* ctx)
{
    if (!ctx) return;
    
    if (ctx->weakbits.regions) {
        free(ctx->weakbits.regions);
        ctx->weakbits.regions = NULL;
    }
}

/*============================================================================
 * Bitstream Utilities
 *============================================================================*/

static inline int get_bit(const uint8_t* data, size_t offset)
{
    return (data[offset >> 3] >> (7 - (offset & 7))) & 1;
}

static inline uint16_t get_word(const uint8_t* data, size_t bit_offset)
{
    uint16_t word = 0;
    for (int i = 0; i < 16; i++) {
        word = (word << 1) | get_bit(data, bit_offset + i);
    }
    return word;
}

/**
 * @brief Find sync word in bitstream
 */
static int32_t find_sync(const uint8_t* data, size_t bits, 
                         size_t start, uint16_t sync)
{
    for (size_t i = start; i + 16 <= bits; i++) {
        if (get_word(data, i) == sync) {
            return (int32_t)i;
        }
    }
    return -1;
}

/*============================================================================
 * MFM Decoding Utilities
 *============================================================================*/

/**
 * @brief Decode MFM word (even/odd interleaved)
 */
static uint8_t mfm_decode_byte(uint16_t mfm)
{
    uint8_t byte = 0;
    /* Extract odd bits (data bits) */
    for (int i = 0; i < 8; i++) {
        if (mfm & (1 << (14 - i * 2))) {
            byte |= (1 << (7 - i));
        }
    }
    return byte;
}

/**
 * @brief Read and decode MFM bytes from bitstream
 */
static void read_mfm_bytes(const uint8_t* bits, size_t bit_offset,
                           uint8_t* out, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        uint16_t mfm = get_word(bits, bit_offset + i * 16);
        out[i] = mfm_decode_byte(mfm);
    }
}

/*============================================================================
 * CopyLock Detection
 *============================================================================*/

bool uft_detect_copylock(uft_protection_ctx_t* ctx)
{
    if (!ctx || !ctx->track_data || ctx->track_bits < 50000) {
        return false;
    }
    
    memset(&ctx->copylock, 0, sizeof(ctx->copylock));
    
    /* Try to find CopyLock sectors */
    size_t bit_pos = 0;
    
    for (int attempt = 0; attempt < 2; attempt++) {
        bool old_style = (attempt == 1);
        uint32_t lfsr_seed = 0;
        uint8_t valid_mask = 0;
        int sectors_found = 0;
        
        bit_pos = 0;
        
        while (bit_pos + 1024 * 8 * 2 < ctx->track_bits) {
            /* Search for any CopyLock sync mark */
            int found_sector = -1;
            int32_t sync_pos = -1;
            
            if (old_style) {
                /* Old-style: look for 0x65xx pattern */
                for (size_t i = bit_pos; i + 32 < ctx->track_bits; i++) {
                    uint16_t word = get_word(ctx->track_data, i);
                    if ((word & 0xFF00) == 0x6500) {
                        uint8_t sec = mfm_decode_byte(word) & 0x0F;
                        if (sec < UFT_COPYLOCK_SECTORS) {
                            /* Verify expected pattern */
                            uint16_t expected = 0x65B0 | sec;
                            if (word == expected) {
                                found_sector = sec;
                                sync_pos = i;
                                break;
                            }
                        }
                    }
                }
            } else {
                /* New-style: look for specific sync marks */
                for (int s = 0; s < UFT_COPYLOCK_SECTORS; s++) {
                    int32_t pos = find_sync(ctx->track_data, ctx->track_bits,
                                            bit_pos, uft_copylock_sync_marks[s]);
                    if (pos >= 0 && (sync_pos < 0 || pos < sync_pos)) {
                        sync_pos = pos;
                        found_sector = s;
                    }
                }
            }
            
            if (found_sector < 0 || sync_pos < 0) {
                break;
            }
            
            /* Skip sync and read sector header */
            bit_pos = sync_pos + 16;
            
            /* Read and verify sector number */
            uint16_t sec_mfm = get_word(ctx->track_data, bit_pos);
            uint8_t sec_num = mfm_decode_byte(sec_mfm);
            
            if (sec_num != found_sector) {
                bit_pos += 16;
                continue;
            }
            
            bit_pos += 16;
            
            /* Read sector data (512 bytes = 8192 bits in MFM) */
            uint8_t sector_data[512];
            read_mfm_bytes(ctx->track_data, bit_pos, sector_data, 512);
            
            /* Check for signature in sector 6 */
            if (found_sector == UFT_COPYLOCK_SIG_SECTOR) {
                if (memcmp(sector_data, UFT_COPYLOCK_SIGNATURE, 
                           UFT_COPYLOCK_SIG_LEN) == 0) {
                    ctx->copylock.signature_found = true;
                }
            }
            
            /* Extract LFSR state from data */
            size_t data_start = (found_sector == 6) ? UFT_COPYLOCK_SIG_LEN : 0;
            
            if (lfsr_seed == 0) {
                /* Derive seed from first valid sector */
                uint32_t lfsr = ((uint32_t)sector_data[data_start] << 15) |
                                ((uint32_t)sector_data[data_start + 8] << 7) |
                                ((uint32_t)sector_data[data_start + 16] >> 1);
                
                /* Seek back to sector 0 to get seed */
                lfsr_seed = lfsr_seek_sector(lfsr, found_sector, 0, old_style);
                
                if (lfsr_seed == 0) lfsr_seed = 1;  /* Avoid degenerate case */
            }
            
            /* Verify sector data against LFSR */
            uint32_t expected_lfsr = lfsr_seek_sector(lfsr_seed, 0, 
                                                       found_sector, old_style);
            bool valid = true;
            
            for (size_t i = data_start; i < 512 && valid; i++) {
                if (sector_data[i] != uft_lfsr_byte(expected_lfsr)) {
                    valid = false;
                }
                expected_lfsr = uft_lfsr_next(expected_lfsr);
            }
            
            if (valid) {
                valid_mask |= (1 << found_sector);
                sectors_found++;
            }
            
            bit_pos += 512 * 16;  /* Skip sector data in MFM */
        }
        
        /* Check if this is valid CopyLock */
        if (sectors_found >= 3 && ctx->copylock.signature_found) {
            ctx->copylock.type = old_style ? UFT_PROT_COPYLOCK_OLD : UFT_PROT_COPYLOCK;
            ctx->copylock.lfsr_seed = lfsr_seed;
            ctx->copylock.valid_sectors = valid_mask;
            ctx->copylock.sectors_found = sectors_found;
            ctx->primary = ctx->copylock.type;
            return true;
        }
    }
    
    return false;
}

/*============================================================================
 * Speedlock Detection
 *============================================================================*/

bool uft_detect_speedlock(uft_protection_ctx_t* ctx)
{
    if (!ctx || !ctx->flux_times || ctx->num_flux < 4000) {
        return false;
    }
    
    memset(&ctx->speedlock, 0, sizeof(ctx->speedlock));
    
    /* Calculate average flux time (32-bit samples) */
    uint64_t total_time = 0;
    for (size_t i = 0; i < 2000 && i < ctx->num_flux; i++) {
        total_time += ctx->flux_times[i];
    }
    uint32_t avg_time = (uint32_t)(total_time / 2000);
    
    if (avg_time == 0) return false;
    
    /* Thresholds for detection */
    uint32_t long_threshold = (avg_time * 108) / 100;   /* +8% */
    uint32_t short_threshold = (avg_time * 92) / 100;   /* -8% */
    uint32_t normal_threshold = (avg_time * 98) / 100;  /* -2% */
    
    /* Scan for long region */
    size_t i = 0;
    while (i < ctx->num_flux - 32) {
        /* Get average of 32 consecutive fluxes */
        uint64_t sum = 0;
        for (int j = 0; j < 32; j++) {
            sum += ctx->flux_times[i + j];
        }
        uint32_t local_avg = (uint32_t)(sum / 32);
        
        if (local_avg > long_threshold) {
            ctx->speedlock.long_region_start = (uint32_t)i;
            break;
        }
        i++;
    }
    
    if (ctx->speedlock.long_region_start == 0) return false;
    
    /* Scan for short region */
    while (i < ctx->num_flux - 32) {
        uint64_t sum = 0;
        for (int j = 0; j < 32; j++) {
            sum += ctx->flux_times[i + j];
        }
        uint32_t local_avg = (uint32_t)(sum / 32);
        
        if (local_avg < short_threshold) {
            ctx->speedlock.short_region_start = (uint32_t)i;
            break;
        }
        i++;
    }
    
    if (ctx->speedlock.short_region_start <= ctx->speedlock.long_region_start) {
        return false;
    }
    
    /* Scan for return to normal */
    while (i < ctx->num_flux - 32) {
        uint64_t sum = 0;
        for (int j = 0; j < 32; j++) {
            sum += ctx->flux_times[i + j];
        }
        uint32_t local_avg = (uint32_t)(sum / 32);
        
        if (local_avg > normal_threshold) {
            ctx->speedlock.normal_region_start = (uint32_t)i;
            break;
        }
        i++;
    }
    
    if (ctx->speedlock.normal_region_start <= ctx->speedlock.short_region_start) {
        return false;
    }
    
    /* Validate relative positions */
    /* Long region should start around 77500 bits after index */
    uint32_t long_offset = ctx->speedlock.long_region_start * 2;  /* ~2 bits per flux */
    if (long_offset < 75000 || long_offset > 80000) {
        return false;
    }
    
    /* Calculate sector lengths */
    uint32_t region_len = (ctx->speedlock.normal_region_start - 
                           ctx->speedlock.long_region_start) / 2;
    if (region_len < 500 || region_len > 800) {
        return false;
    }
    
    ctx->speedlock.sector_length = (uint16_t)region_len;
    ctx->speedlock.detected = true;
    ctx->primary = UFT_PROT_SPEEDLOCK;
    ctx->all_protections |= (1 << 8);  /* Speedlock flag */
    
    return true;
}

/*============================================================================
 * Long Track Detection
 *============================================================================*/

bool uft_detect_longtrack(uft_protection_ctx_t* ctx)
{
    if (!ctx) return false;
    
    memset(&ctx->longtrack, 0, sizeof(ctx->longtrack));
    
    ctx->longtrack.track_bits = (uint32_t)ctx->track_bits;
    ctx->longtrack.percent = (uint16_t)((ctx->track_bits * 100) / 
                                         UFT_STANDARD_TRACK_BITS);
    
    if (ctx->longtrack.percent >= UFT_LONGTRACK_THRESHOLD) {
        ctx->longtrack.detected = true;
        ctx->longtrack.extra_bits = (uint32_t)(ctx->track_bits - 
                                                UFT_STANDARD_TRACK_BITS);
        
        if (ctx->primary == UFT_PROT_NONE) {
            ctx->primary = UFT_PROT_LONGTRACK;
        }
        ctx->all_protections |= (1 << 9);  /* Longtrack flag */
        return true;
    }
    
    return false;
}

/*============================================================================
 * Weak Bits Detection
 *============================================================================*/

bool uft_detect_weakbits(uft_protection_ctx_t* ctx)
{
    if (!ctx || !ctx->revolutions || ctx->num_revolutions < 2) {
        return false;
    }
    
    memset(&ctx->weakbits, 0, sizeof(ctx->weakbits));
    
    /* Compare revolutions byte by byte */
    size_t track_bytes = (ctx->track_bits + 7) / 8;
    uint8_t* diff_map = calloc(track_bytes, 1);
    if (!diff_map) return false;
    
    /* Find differences between revolutions */
    for (size_t r = 1; r < ctx->num_revolutions; r++) {
        for (size_t i = 0; i < track_bytes; i++) {
            diff_map[i] |= ctx->revolutions[0][i] ^ ctx->revolutions[r][i];
        }
    }
    
    /* Count regions with differences */
    int num_regions = 0;
    bool in_region = false;
    size_t region_start = 0;
    
    for (size_t i = 0; i < track_bytes; i++) {
        if (diff_map[i] != 0) {
            if (!in_region) {
                in_region = true;
                region_start = i;
            }
        } else {
            if (in_region) {
                num_regions++;
                in_region = false;
            }
        }
    }
    if (in_region) num_regions++;
    
    if (num_regions == 0) {
        free(diff_map);
        return false;
    }
    
    /* Allocate region array */
    ctx->weakbits.regions = calloc(num_regions, sizeof(uft_weak_region_t));
    if (!ctx->weakbits.regions) {
        free(diff_map);
        return false;
    }
    
    /* Fill region info */
    int region_idx = 0;
    in_region = false;
    
    for (size_t i = 0; i < track_bytes; i++) {
        if (diff_map[i] != 0) {
            if (!in_region) {
                in_region = true;
                ctx->weakbits.regions[region_idx].bit_offset = (uint32_t)(i * 8);
            }
        } else {
            if (in_region) {
                ctx->weakbits.regions[region_idx].bit_length = 
                    (uint32_t)((i * 8) - ctx->weakbits.regions[region_idx].bit_offset);
                region_idx++;
                in_region = false;
            }
        }
    }
    if (in_region) {
        ctx->weakbits.regions[region_idx].bit_length = 
            (uint32_t)((track_bytes * 8) - ctx->weakbits.regions[region_idx].bit_offset);
        region_idx++;
    }
    
    free(diff_map);
    
    ctx->weakbits.num_regions = (uint16_t)num_regions;
    ctx->weakbits.detected = true;
    
    if (ctx->primary == UFT_PROT_NONE) {
        ctx->primary = UFT_PROT_WEAK_BITS;
    }
    ctx->all_protections |= (1 << 10);  /* Weak bits flag */
    
    return true;
}

/*============================================================================
 * Custom Sync Detection
 *============================================================================*/

bool uft_detect_custom_sync(uft_protection_ctx_t* ctx)
{
    if (!ctx || !ctx->track_data || ctx->track_bits < 1000) {
        return false;
    }
    
    /* Standard MFM sync: 0x4489 (A1 with missing clock) */
    const uint16_t standard_sync = 0x4489;
    
    /* Search for non-standard sync patterns */
    int custom_count = 0;
    
    for (size_t i = 0; i + 16 < ctx->track_bits; i++) {
        uint16_t word = get_word(ctx->track_data, i);
        
        /* Check for sync-like patterns (multiple consecutive same bits) */
        if ((word & 0xFF00) == 0x4400 && word != standard_sync) {
            /* Found a sync-like pattern that's not standard */
            custom_count++;
            i += 15;  /* Skip past this pattern */
        }
    }
    
    if (custom_count >= 3) {
        if (ctx->primary == UFT_PROT_NONE) {
            ctx->primary = UFT_PROT_CUSTOM_SYNC;
        }
        ctx->all_protections |= (1 << 11);  /* Custom sync flag */
        return true;
    }
    
    return false;
}

/*============================================================================
 * Run All Detection
 *============================================================================*/

uft_protection_type_t uft_detect_all_protections(uft_protection_ctx_t* ctx)
{
    if (!ctx) return UFT_PROT_NONE;
    
    ctx->primary = UFT_PROT_NONE;
    ctx->all_protections = 0;
    
    /* Run detectors in priority order */
    uft_detect_copylock(ctx);
    uft_detect_speedlock(ctx);
    uft_detect_longtrack(ctx);
    uft_detect_weakbits(ctx);
    uft_detect_custom_sync(ctx);
    
    return ctx->primary;
}

/*============================================================================
 * CopyLock Reconstruction
 *============================================================================*/

size_t uft_copylock_reconstruct(uint32_t seed, uint8_t* output, bool old_style)
{
    if (!output || seed == 0) return 0;
    
    size_t pos = 0;
    uint32_t lfsr = seed;
    
    for (int sec = 0; sec < UFT_COPYLOCK_SECTORS; sec++) {
        /* Header */
        if (!old_style) {
            output[pos++] = 0xA0 + sec;
            output[pos++] = 0x00;
            output[pos++] = 0x00;
            /* Sync mark would go here in raw data */
        }
        
        output[pos++] = sec;  /* Sector number */
        
        /* Data */
        for (int i = 0; i < 512; i++) {
            if (sec == 6 && i < UFT_COPYLOCK_SIG_LEN) {
                output[pos++] = (uint8_t)UFT_COPYLOCK_SIGNATURE[i];
            } else {
                output[pos++] = uft_lfsr_byte(lfsr);
                lfsr = uft_lfsr_next(lfsr);
            }
        }
        
        output[pos++] = 0x00;  /* Footer */
    }
    
    return pos;
}

/*============================================================================
 * Protection Names
 *============================================================================*/

const char* uft_protection_name(uft_protection_type_t type)
{
    switch (type) {
        case UFT_PROT_NONE:         return "None";
        case UFT_PROT_COPYLOCK:     return "CopyLock";
        case UFT_PROT_COPYLOCK_OLD: return "CopyLock (Old)";
        case UFT_PROT_SPEEDLOCK:    return "Speedlock";
        case UFT_PROT_LONGTRACK:    return "Long Track";
        case UFT_PROT_RNC_PROTECT:  return "RNC Protection";
        case UFT_PROT_SOFTLOCK:     return "Softlock";
        case UFT_PROT_V_MAX:        return "V-MAX!";
        case UFT_PROT_PIRATESLAYER: return "PirateSlayer";
        case UFT_PROT_RAPIDLOK:     return "RapidLok";
        case UFT_PROT_VORPAL:       return "Vorpal";
        case UFT_PROT_WEAK_BITS:    return "Weak Bits";
        case UFT_PROT_CUSTOM_SYNC:  return "Custom Sync";
        case UFT_PROT_TIMING_BASED: return "Timing-Based";
        case UFT_PROT_DUPLICATE_SECTOR: return "Duplicate Sector";
        case UFT_PROT_UNKNOWN:
        default:                    return "Unknown";
    }
}

void uft_protection_print(const uft_protection_ctx_t* ctx, bool verbose)
{
    if (!ctx) return;
    
    printf("Protection Detection Results:\n");
    printf("  Primary: %s\n", uft_protection_name(ctx->primary));
    
    if (ctx->copylock.type != UFT_PROT_NONE) {
        printf("\n  CopyLock Details:\n");
        printf("    LFSR Seed: 0x%06X\n", ctx->copylock.lfsr_seed);
        printf("    Sectors Found: %d/11\n", ctx->copylock.sectors_found);
        printf("    Signature: %s\n", ctx->copylock.signature_found ? "Found" : "Not found");
        
        if (verbose) {
            printf("    Valid Sectors: ");
            for (int i = 0; i < 11; i++) {
                printf("%c", (ctx->copylock.valid_sectors & (1 << i)) ? '0' + i : '.');
            }
            printf("\n");
        }
    }
    
    if (ctx->speedlock.detected) {
        printf("\n  Speedlock Details:\n");
        printf("    Long Region: bit %u\n", ctx->speedlock.long_region_start);
        printf("    Short Region: bit %u\n", ctx->speedlock.short_region_start);
        printf("    Region Length: %u bits\n", ctx->speedlock.sector_length);
    }
    
    if (ctx->longtrack.detected) {
        printf("\n  Long Track Details:\n");
        printf("    Track Length: %u bits (%u%%)\n", 
               ctx->longtrack.track_bits, ctx->longtrack.percent);
        printf("    Extra Bits: %u\n", ctx->longtrack.extra_bits);
    }
    
    if (ctx->weakbits.detected) {
        printf("\n  Weak Bits Details:\n");
        printf("    Regions: %u\n", ctx->weakbits.num_regions);
        
        if (verbose && ctx->weakbits.regions) {
            for (int i = 0; i < ctx->weakbits.num_regions && i < 10; i++) {
                printf("    Region %d: offset=%u len=%u\n", i,
                       ctx->weakbits.regions[i].bit_offset,
                       ctx->weakbits.regions[i].bit_length);
            }
            if (ctx->weakbits.num_regions > 10) {
                printf("    ... and %d more\n", ctx->weakbits.num_regions - 10);
            }
        }
    }
}
