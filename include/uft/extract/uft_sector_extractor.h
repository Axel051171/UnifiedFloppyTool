/**
 * @file uft_sector_extractor.h
 * @brief Sector Extraction API
 */

#ifndef UFT_SECTOR_EXTRACTOR_H
#define UFT_SECTOR_EXTRACTOR_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Structures
 *===========================================================================*/

typedef struct {
    int cylinder;
    int head;
    int sector_id;
    int size_code;
    int data_size;
    
    size_t idam_offset;     /* Position of ID address mark */
    size_t dam_offset;      /* Position of data address mark */
    
    bool valid;
    bool id_crc_ok;
    bool data_crc_ok;
    bool deleted;
    bool weak;
    
    uint8_t *data;          /* Extracted sector data (caller must free) */
} uft_extracted_sector_t;

typedef struct {
    size_t total_sectors;
    size_t sectors_with_data;
    size_t id_crc_ok;
    size_t data_crc_ok;
    size_t deleted_sectors;
    size_t weak_sectors;
    size_t valid_sectors;
    double success_rate;
} uft_extraction_stats_t;

/*===========================================================================
 * IBM/PC Extraction
 *===========================================================================*/

int uft_extract_ibm_sectors(const uint8_t *track_data, size_t track_size,
                            uft_extracted_sector_t *sectors, size_t max_sectors,
                            size_t *sector_count);

/*===========================================================================
 * Amiga Extraction
 *===========================================================================*/

int uft_extract_amiga_sectors(const uint8_t *track_data, size_t track_size,
                              uft_extracted_sector_t *sectors, size_t max_sectors,
                              size_t *sector_count);

/*===========================================================================
 * Multi-Revolution Fusion
 *===========================================================================*/

int uft_extract_fuse_revolutions(uft_extracted_sector_t **rev_sectors,
                                 size_t *rev_counts, int rev_count,
                                 uft_extracted_sector_t *fused, size_t max_sectors,
                                 size_t *fused_count);

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

void uft_extracted_sector_free(uft_extracted_sector_t *sector);
void uft_extracted_sectors_free(uft_extracted_sector_t *sectors, size_t count);
void uft_extracted_sectors_sort(uft_extracted_sector_t *sectors, size_t count);

int uft_extraction_stats(const uft_extracted_sector_t *sectors, size_t count,
                         uft_extraction_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif /* UFT_SECTOR_EXTRACTOR_H */
