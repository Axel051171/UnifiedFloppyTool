#pragma once
/*
 * fat_bpb.h — FAT BPB parser + layout calculator (C11)
 *
 * Clean-room implementation: uses public FAT BPB structure knowledge.
 * Purpose in UFT:
 * - Detect DOS/FAT formatted disk images quickly (floppy/hdd).
 * - Provide geometry + region offsets for recovery/validation.
 *
 * Non-negotiable:
 * - No fake success. Validation returns detailed error codes + diag.
 */
#include <stddef.h>
#include <stdint.h>
#include "uft/uft_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum uft_fat_type {
    UFT_FAT_UNKNOWN = 0,
    UFT_FAT12 = 12,
    UFT_FAT16 = 16,
    UFT_FAT32 = 32
} uft_fat_type_t;

typedef struct uft_fat_bpb {
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  fats;
    uint16_t root_entries;          /* FAT12/16 */
    uint32_t total_sectors;
    uint32_t sectors_per_fat;       /* FAT12/16 or FAT32 (fat32 has 32-bit field) */
    uint16_t sectors_per_track;     /* geometry hint */
    uint16_t heads;                 /* geometry hint */
    uint32_t hidden_sectors;
    uint32_t root_cluster;          /* FAT32 */
    uft_fat_type_t fat_type;
    int confidence;                 /* 0..100 */
} uft_fat_bpb_t;

typedef struct uft_fat_layout {
    uint32_t lba_fat0;
    uint32_t lba_root;     /* FAT12/16 root dir */
    uint32_t lba_data;
    uint32_t fat_sectors;
    uint32_t root_sectors;
    uint32_t data_sectors;
    uint32_t cluster_count;
} uft_fat_layout_t;

/* Parse BPB from sector 0 (or boot sector) buffer (>= 512 bytes). */
uft_rc_t uft_fat_bpb_parse(const uint8_t *sector0, size_t n, uft_fat_bpb_t *out, uft_diag_t *diag);

/* Validate BPB sanity + compute FAT type + layout. */
uft_rc_t uft_fat_bpb_validate_and_layout(const uft_fat_bpb_t *bpb, uft_fat_layout_t *layout, uft_diag_t *diag);

/* One-shot: parse+validate+layout from full image buffer. */
uft_rc_t uft_fat_probe_image(const uint8_t *img, size_t len, uft_fat_bpb_t *bpb, uft_fat_layout_t *layout, uft_diag_t *diag);

#ifdef __cplusplus
}
#endif
