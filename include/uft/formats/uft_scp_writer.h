/**
 * @file uft_scp_writer.h
 * @brief SCP (SuperCard Pro) Writer API (W-P0-003)
 * 
 * @version 1.0.0
 * @date 2026-01-15
 */

#ifndef UFT_SCP_WRITER_H
#define UFT_SCP_WRITER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * TYPES
 *===========================================================================*/

typedef struct scp_writer scp_writer_t;

/* Disk Types */
#define SCP_TYPE_C64        0x00
#define SCP_TYPE_AMIGA      0x04
#define SCP_TYPE_ATARI_ST   0x08
#define SCP_TYPE_ATARI_800  0x0C
#define SCP_TYPE_APPLE_II   0x10
#define SCP_TYPE_APPLE_35   0x14
#define SCP_TYPE_PC_DD      0x20
#define SCP_TYPE_PC_HD      0x30

/*===========================================================================
 * LIFECYCLE
 *===========================================================================*/

/**
 * @brief Create SCP writer
 * 
 * @param disk_type SCP disk type (SCP_TYPE_*)
 * @param revolutions Number of revolutions to store (1-5)
 * @return Writer handle or NULL on error
 */
scp_writer_t* scp_writer_create(uint8_t disk_type, uint8_t revolutions);

/**
 * @brief Free writer resources
 */
void scp_writer_free(scp_writer_t *w);

/*===========================================================================
 * DATA FUNCTIONS
 *===========================================================================*/

/**
 * @brief Add track flux data
 * 
 * @param w Writer handle
 * @param track_num Track number (0-83)
 * @param side Side (0 or 1)
 * @param flux_ns Array of flux transition times in nanoseconds
 * @param flux_count Number of flux transitions
 * @param duration_ns Total track duration in nanoseconds
 * @param revolution Revolution number (0-4)
 * @return 0 on success
 */
int scp_writer_add_track(
    scp_writer_t *w,
    int track_num,
    int side,
    const uint32_t *flux_ns,
    size_t flux_count,
    uint32_t duration_ns,
    int revolution);

/**
 * @brief Write SCP file to disk
 * 
 * @param w Writer handle
 * @param path Output file path
 * @return 0 on success
 */
int scp_writer_save(scp_writer_t *w, const char *path);

/*===========================================================================
 * CONVENIENCE
 *===========================================================================*/

/**
 * @brief Get disk type from format hint string
 */
uint8_t scp_disk_type_from_hint(const char *hint);

/**
 * @brief Quick write - create SCP from flux arrays
 */
int scp_write_quick(
    const char *path,
    uint8_t disk_type,
    int sides,
    int tracks_per_side,
    const uint32_t **flux_data,
    const size_t *flux_counts,
    const uint32_t *durations,
    int revolutions);

#ifdef __cplusplus
}
#endif

#endif /* UFT_SCP_WRITER_H */
