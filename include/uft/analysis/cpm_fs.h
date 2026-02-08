/**
 * @file cpm_fs.h
 * @brief CP/M Filesystem Analysis
 */
#ifndef UFT_CPM_FS_H
#define UFT_CPM_FS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CPM_DIR_ENTRY_SIZE  32
#define CPM_MAX_EXTENTS     512

typedef struct {
    uint8_t  user_number;
    char     filename[8];
    char     extension[3];
    uint8_t  extent_low;
    uint8_t  reserved1;
    uint8_t  extent_high;
    uint8_t  record_count;
    uint8_t  allocation[16];
} cpm_dir_entry_t;

typedef struct {
    uint16_t block_size;
    uint16_t dir_entries;
    uint16_t total_blocks;
    uint8_t  tracks_offset;     /* system tracks */
    uint8_t  sectors_per_track;
    uint8_t  block_shift;
    bool     detected;
} cpm_dpb_t;

/**
 * @brief Detect CP/M filesystem parameters
 */
int cpm_detect_format(const uint8_t *data, size_t size, cpm_dpb_t *dpb);

/**
 * @brief List directory entries
 */
int cpm_read_directory(const uint8_t *data, size_t size,
                       const cpm_dpb_t *dpb,
                       cpm_dir_entry_t *entries, int max_entries);

#ifdef __cplusplus
}
#endif

#endif /* UFT_CPM_FS_H */
