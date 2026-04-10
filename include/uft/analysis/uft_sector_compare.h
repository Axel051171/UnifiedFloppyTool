#ifndef UFT_SECTOR_COMPARE_H
#define UFT_SECTOR_COMPARE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UFT_COMPARE_MAX_DIFFS  4096

typedef struct {
    uint8_t  track;
    uint8_t  head;
    uint8_t  sector;
    uint32_t offset;          /* Byte offset in image */
    uint16_t diff_count;      /* Number of differing bytes in this sector */
    uint16_t sector_size;
    bool     only_in_a;       /* Sector exists only in image A */
    bool     only_in_b;       /* Sector exists only in image B */
} uft_sector_diff_t;

typedef struct {
    /* Summary */
    uint32_t total_sectors;
    uint32_t identical_sectors;
    uint32_t different_sectors;
    uint32_t only_in_a;
    uint32_t only_in_b;
    float    similarity_percent;   /* 0-100 */

    /* Byte-level stats */
    uint64_t total_bytes;
    uint64_t identical_bytes;
    uint64_t different_bytes;

    /* Detailed diffs */
    uft_sector_diff_t diffs[UFT_COMPARE_MAX_DIFFS];
    uint32_t diff_count;

    /* Format info */
    char format_a[32];
    char format_b[32];
    uint32_t size_a;
    uint32_t size_b;
} uft_compare_result_t;

/* Compare two disk image files */
int uft_sector_compare_files(const char *path_a, const char *path_b,
                              uft_compare_result_t *result);

/* Compare two in-memory buffers */
int uft_sector_compare_buffers(const uint8_t *data_a, size_t size_a,
                                const uint8_t *data_b, size_t size_b,
                                int sector_size,
                                uft_compare_result_t *result);

/* Export comparison as text report */
int uft_compare_export_text(const uft_compare_result_t *result,
                             const char *output_path);

/* Export as JSON */
int uft_compare_export_json(const uft_compare_result_t *result,
                             const char *output_path);

#ifdef __cplusplus
}
#endif
#endif
