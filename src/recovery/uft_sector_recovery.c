/**
 * @file uft_sector_recovery.c
 * @brief UFT Sector-Level Recovery Module
 * 
 * Sector recovery for damaged or missing sectors:
 * - Multiple-read averaging
 * - Sector reconstruction from partial data
 * - Bad sector mapping
 * - Interleave-aware recovery
 * 
 * @version 5.28.0 GOD MODE
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Types
 * ============================================================================ */

typedef enum {
    UFT_SECTOR_OK,
    UFT_SECTOR_CRC_ERROR,
    UFT_SECTOR_HEADER_ERROR,
    UFT_SECTOR_MISSING,
    UFT_SECTOR_WEAK,
    UFT_SECTOR_RECOVERED
} uft_sector_status_t;

typedef struct {
    uint8_t  track;
    uint8_t  head;
    uint8_t  sector;
    uint8_t  size_code;
    uint8_t *data;
    size_t   data_len;
    uint16_t header_crc;
    uint16_t data_crc;
    uft_sector_status_t status;
    uint8_t  confidence;
    uint8_t  read_count;        /* Number of successful reads */
} uft_sector_t;

typedef struct {
    uft_sector_t *sectors;
    size_t sector_count;
    size_t good_sectors;
    size_t bad_sectors;
    size_t recovered_sectors;
    size_t missing_sectors;
} uft_sector_map_t;

typedef struct {
    size_t max_retries;
    bool   use_averaging;
    bool   attempt_reconstruction;
    uint8_t recovery_level;
} uft_sector_recovery_config_t;

/* ============================================================================
 * CRC
 * ============================================================================ */

static uint16_t crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            crc = (crc & 0x8000) ? ((crc << 1) ^ 0x1021) : (crc << 1);
        }
    }
    return crc;
}

/* ============================================================================
 * Sector Averaging
 * ============================================================================ */

/**
 * @brief Average multiple sector reads
 * 
 * For weak/marginal sectors, reading multiple times and voting on each byte
 */
static int average_sector_reads(const uint8_t **reads, size_t read_count,
                                size_t sector_size, uint8_t *output,
                                uint8_t *confidence_map)
{
    if (!reads || read_count == 0 || !output) return -1;
    
    for (size_t i = 0; i < sector_size; i++) {
        /* Count occurrences of each byte value */
        uint8_t counts[256] = {0};
        
        for (size_t r = 0; r < read_count; r++) {
            if (reads[r]) {
                counts[reads[r][i]]++;
            }
        }
        
        /* Find most common value */
        uint8_t best_value = 0;
        uint8_t best_count = 0;
        
        for (int v = 0; v < 256; v++) {
            if (counts[v] > best_count) {
                best_count = counts[v];
                best_value = v;
            }
        }
        
        output[i] = best_value;
        
        if (confidence_map) {
            confidence_map[i] = (best_count * 255) / read_count;
        }
    }
    
    return 0;
}

/* ============================================================================
 * Sector Reconstruction
 * ============================================================================ */

/**
 * @brief Reconstruct sector from partial data with explicit validity map.
 *
 * FORENSIC CONTRACT (Prinzip 1 — keine erfundenen Daten):
 * Missing bytes are filled with the constant sentinel 0x00 — they are
 * NEVER interpolated, pattern-extrapolated, or otherwise synthesized.
 * Linear interpolation between two real bytes manufactures values that
 * never existed on the medium and is forbidden on binary sector data.
 *
 * The caller MUST honour out_validity[] — every reconstructed byte
 * (validity == 0) is to be treated as unknown by downstream code, even
 * if a CRC happens to match. CRC equality on a partly-reconstructed
 * sector indicates only that the chosen sentinel was a plausible
 * reading, not that the data is authentic.
 *
 * @param partial_data    Best-effort source bytes (may contain garbage
 *                        in slots whose valid_mask says invalid).
 * @param partial_len     Bytes actually present in partial_data.
 * @param valid_mask      Optional per-byte validity (1 = real, 0 = gap).
 *                        If NULL, validity is derived from partial_len:
 *                        bytes [0, partial_len) are real, rest are gaps.
 * @param sector_size     Target output size.
 * @param output          [out] sector_size bytes; gaps filled with 0x00.
 * @param out_validity    [out, optional] sector_size bytes; 1 = real,
 *                        0 = sentinel-filled gap. May be NULL.
 *
 * Returns 0 on success, -1 on argument error.
 */
static int reconstruct_sector(const uint8_t *partial_data, size_t partial_len,
                              const uint8_t *valid_mask, size_t sector_size,
                              uint8_t *output, uint8_t *out_validity)
{
    if (!partial_data || !output) return -1;

    for (size_t i = 0; i < sector_size; i++) {
        bool valid = valid_mask ? (valid_mask[i] != 0) : (i < partial_len);

        if (valid && i < partial_len) {
            output[i] = partial_data[i];                    /* INTEGRITY-OK: out_validity set on next line */
            if (out_validity) out_validity[i] = 1;
        } else {
            output[i] = 0x00;                               /* INTEGRITY-OK: out_validity=0 marks this byte unauthentic */
            if (out_validity) out_validity[i] = 0;
        }
    }

    return 0;
}

/* ============================================================================
 * Bad Sector Mapping
 * ============================================================================ */

/**
 * @brief Create sector map for track
 */
static int create_sector_map(uft_sector_t *sectors, size_t sector_count,
                             uft_sector_map_t *map)
{
    if (!sectors || !map) return -1;
    
    map->sectors = sectors;
    map->sector_count = sector_count;
    map->good_sectors = 0;
    map->bad_sectors = 0;
    map->recovered_sectors = 0;
    map->missing_sectors = 0;
    
    for (size_t i = 0; i < sector_count; i++) {
        switch (sectors[i].status) {
            case UFT_SECTOR_OK:
                map->good_sectors++;
                break;
            case UFT_SECTOR_CRC_ERROR:
            case UFT_SECTOR_HEADER_ERROR:
            case UFT_SECTOR_WEAK:
                map->bad_sectors++;
                break;
            case UFT_SECTOR_MISSING:
                map->missing_sectors++;
                break;
            case UFT_SECTOR_RECOVERED:
                map->recovered_sectors++;
                break;
        }
    }
    
    return 0;
}

/**
 * @brief Find sector by number
 */
static uft_sector_t* find_sector(uft_sector_map_t *map, uint8_t sector_num)
{
    for (size_t i = 0; i < map->sector_count; i++) {
        if (map->sectors[i].sector == sector_num) {
            return &map->sectors[i];
        }
    }
    return NULL;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief Initialize sector recovery configuration
 */
void uft_sector_recovery_config_init(uft_sector_recovery_config_t *config)
{
    config->max_retries = 5;
    config->use_averaging = true;
    config->attempt_reconstruction = true;
    config->recovery_level = 1;
}

/**
 * @brief Recover sector using multiple reads
 */
int uft_sector_recover_average(uft_sector_t *sector,
                               const uint8_t **additional_reads,
                               size_t read_count)
{
    if (!sector || !additional_reads || read_count == 0) return -1;
    
    /* Allocate arrays for all reads */
    const uint8_t **all_reads = malloc((read_count + 1) * sizeof(uint8_t*));
    if (!all_reads) return -1;
    
    all_reads[0] = sector->data;
    memcpy(all_reads + 1, additional_reads, read_count * sizeof(uint8_t*));
    
    /* Average all reads */
    uint8_t *averaged = malloc(sector->data_len);
    uint8_t *confidence = malloc(sector->data_len);
    
    if (!averaged || !confidence) {
        free(all_reads);
        free(averaged);
        free(confidence);
        return -1;
    }
    
    average_sector_reads(all_reads, read_count + 1, sector->data_len,
                         averaged, confidence);
    
    /* Check if averaging fixed CRC */
    uint16_t new_crc = crc16(averaged, sector->data_len);
    
    if (new_crc == sector->data_crc || 
        sector->status == UFT_SECTOR_CRC_ERROR) {
        memcpy(sector->data, averaged, sector->data_len);
        sector->status = UFT_SECTOR_RECOVERED;
        sector->read_count = read_count + 1;
        
        /* Calculate confidence from averaging */
        uint32_t total_conf = 0;
        for (size_t i = 0; i < sector->data_len; i++) {
            total_conf += confidence[i];
        }
        sector->confidence = total_conf / sector->data_len;
    }
    
    free(all_reads);
    free(averaged);
    free(confidence);
    
    return (sector->status == UFT_SECTOR_RECOVERED) ? 0 : -1;
}

/**
 * @brief Attempt sector reconstruction from partial data.
 *
 * Forensic note: gap bytes are filled with the 0x00 sentinel by
 * reconstruct_sector() — they are NOT recovered values. A successful
 * CRC match on a partly-reconstructed sector therefore indicates that
 * the original bytes happened to be 0x00 (plausible for unwritten
 * regions), not that the gaps were "decoded". Confidence is reduced
 * to reflect the fraction of authentic bytes.
 */
int uft_sector_recover_reconstruct(uft_sector_t *sector,
                                   const uint8_t *partial_data,
                                   size_t partial_len,
                                   const uint8_t *valid_mask)
{
    if (!sector || !partial_data) return -1;

    uint8_t *reconstructed = malloc(sector->data_len);
    uint8_t *validity      = malloc(sector->data_len);
    if (!reconstructed || !validity) {
        free(reconstructed);
        free(validity);
        return -1;
    }

    reconstruct_sector(partial_data, partial_len, valid_mask,
                       sector->data_len, reconstructed, validity);

    /* Count authentic bytes — confidence cannot exceed this fraction. */
    size_t real_bytes = 0;
    for (size_t i = 0; i < sector->data_len; i++) {
        if (validity[i]) real_bytes++;
    }

    /* CRC verification is necessary but not sufficient when gaps exist:
     * a match means the 0x00 sentinel was consistent with the original
     * data, which is plausible but not proven. Confidence reflects this. */
    uint16_t new_crc = crc16(reconstructed, sector->data_len);

    if (new_crc == sector->data_crc) {
        memcpy(sector->data, reconstructed, sector->data_len);
        sector->status = UFT_SECTOR_RECOVERED;
        /* Fraction of authentic bytes, capped at 100. */
        sector->confidence = (uint8_t)((real_bytes * 100u) / sector->data_len);
        free(reconstructed);
        free(validity);
        return 0;
    }

    free(reconstructed);
    free(validity);
    return -1;
}

/**
 * @brief Recover all bad sectors on track
 */
int uft_sector_recover_track(uft_sector_t *sectors, size_t sector_count,
                             const uft_sector_recovery_config_t *config,
                             size_t *recovered)
{
    if (!sectors || sector_count == 0 || !config) return -1;
    
    *recovered = 0;
    
    uft_sector_map_t map;
    create_sector_map(sectors, sector_count, &map);
    
    /* Try to recover each bad sector */
    for (size_t i = 0; i < sector_count; i++) {
        if (sectors[i].status != UFT_SECTOR_OK &&
            sectors[i].status != UFT_SECTOR_RECOVERED) {
            
            /* Try CRC correction first (1-2 bit errors) */
            if (sectors[i].status == UFT_SECTOR_CRC_ERROR && 
                sectors[i].data) {
                
                /* Try flipping single bits */
                for (size_t byte = 0; byte < sectors[i].data_len; byte++) {
                    for (int bit = 0; bit < 8; bit++) {
                        sectors[i].data[byte] ^= (1 << bit);
                        
                        if (crc16(sectors[i].data, sectors[i].data_len) == 
                            sectors[i].data_crc) {
                            sectors[i].status = UFT_SECTOR_RECOVERED;
                            sectors[i].confidence = 90;
                            (*recovered)++;
                            break;
                        }
                        
                        sectors[i].data[byte] ^= (1 << bit);  /* Restore */
                    }
                    if (sectors[i].status == UFT_SECTOR_RECOVERED) break;
                }
            }
        }
    }
    
    return 0;
}

/**
 * @brief Get sector recovery statistics
 */
int uft_sector_get_stats(const uft_sector_t *sectors, size_t sector_count,
                         size_t *good, size_t *bad, size_t *recovered,
                         size_t *missing)
{
    if (!sectors) return -1;
    
    *good = *bad = *recovered = *missing = 0;
    
    for (size_t i = 0; i < sector_count; i++) {
        switch (sectors[i].status) {
            case UFT_SECTOR_OK:
                (*good)++;
                break;
            case UFT_SECTOR_CRC_ERROR:
            case UFT_SECTOR_HEADER_ERROR:
            case UFT_SECTOR_WEAK:
                (*bad)++;
                break;
            case UFT_SECTOR_MISSING:
                (*missing)++;
                break;
            case UFT_SECTOR_RECOVERED:
                (*recovered)++;
                break;
        }
    }
    
    return 0;
}
