#pragma once
/*
 * flux_logical.h — Logical (sector) view attached to the UFM container.
 *
 * Why this exists:
 *  - Many "classic" disk image formats are sector containers (ADF/IMG/DSK/...) and
 *    can be losslessly represented as a CHS-addressed sector map.
 *  - Phase 1 needs WRITERS. Writers must have something deterministic to emit.
 *  - Flux stays primary; this layer is optional and never implies that flux
 *    (if present) can be discarded.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Sector flags (per sector). Keep minimal; extend later. */
typedef enum ufm_sec_flags {
    UFM_SEC_OK          = 0u,
    UFM_SEC_BAD_CRC     = 1u << 0,
    UFM_SEC_DELETED_DAM = 1u << 1,
    UFM_SEC_WEAK        = 1u << 2,
} ufm_sec_flags_t;

/* Optional per-sector metadata (decoder-specific). */
typedef enum ufm_sector_meta_type {
    UFM_SECTOR_META_NONE = 0,
    UFM_SECTOR_META_MFM_IBM = 1, /* IDAM/DAM style sector framing */
} ufm_sector_meta_type_t;

/* IBM-style MFM sector framing metadata (used by CPC/MS-DOS/Atari ST, etc.).
 * All fields are captured as-seen; do not "repair" anything here.
 */
typedef struct ufm_sector_meta_mfm_ibm {
    /* ID fields (from IDAM: FE C H R N). */
    uint8_t id_c, id_h, id_r, id_n;

    /* Data Address Mark (DAM/F8 normal or F8? actually FB, F8 deleted). */
    uint8_t dam_mark; /* 0xFB (normal) or 0xF8 (deleted) */

    /* CRCs as read (big-endian) and calculated over address mark framing. */
    uint16_t idam_crc_read;
    uint16_t idam_crc_calc;
    uint16_t dam_crc_read;
    uint16_t dam_crc_calc;

    /* Bit offsets within the input MFM bitstream for forensic correlation. */
    uint32_t idam_bitpos;
    uint32_t dam_bitpos;

    /* Gap/Sync hints (best-effort). */
    uint16_t pre_idam_sync_zeros;  /* count of 0 bits before A1 sync */
    uint16_t pre_dam_sync_zeros;

    /* Quality hints */
    uint8_t  weak_score; /* 0=unknown, higher=more suspicious */
} ufm_sector_meta_mfm_ibm_t;


/* Confidence scale: 0=unknown, 1..100=usable.
 * 100 means: CRC OK + stable consensus; lower values indicate risk.
 */
#define UFM_CONF_UNKNOWN 0u

typedef struct ufm_sector {
    /* CHS as addressed by the format (logical). */
    uint16_t cyl;
    uint16_t head;
    uint16_t sec;      /* 1-based sector number in most formats */

    /* Payload */
    uint32_t size;     /* bytes */
    uint8_t *data;     /* owned */

    ufm_sec_flags_t flags;

    uint8_t confidence; /* 0=unknown, else 1..100 */

    ufm_sector_meta_type_t meta_type;
    void *meta; /* owned; free()'d by ufm_logical_free */
} ufm_sector_t;

typedef struct ufm_logical_image {
    /* Optional canonical geometry hints. */
    uint16_t cyls;
    uint16_t heads;
    uint16_t spt;         /* sectors per track if constant, else 0 */
    uint32_t sector_size; /* 512 typical; can vary */

    /* Sector map (sparse or dense). */
    ufm_sector_t *sectors;
    uint32_t count;
    uint32_t cap;
} ufm_logical_image_t;

void ufm_logical_init(ufm_logical_image_t *li);
void ufm_logical_free(ufm_logical_image_t *li);

/* Ensures capacity, returns 0 on success. */
int ufm_logical_reserve(ufm_logical_image_t *li, uint32_t want);

/* Append a sector (deep copy of data). Returns 0 on success. */
/* Add a sector and return a pointer to the owned entry (or NULL on error). */
ufm_sector_t* ufm_logical_add_sector_ref(
    ufm_logical_image_t *li,
    uint16_t cyl, uint16_t head, uint16_t sec,
    const uint8_t *data, uint32_t size,
    ufm_sec_flags_t flags);

int ufm_logical_add_sector(
    ufm_logical_image_t *li,
    uint16_t cyl, uint16_t head, uint16_t sec,
    const uint8_t *data, uint32_t size,
    ufm_sec_flags_t flags);

/* Find a sector; returns NULL if not found. */
ufm_sector_t* ufm_logical_find(ufm_logical_image_t *li, uint16_t cyl, uint16_t head, uint16_t sec);
const ufm_sector_t* ufm_logical_find_const(const ufm_logical_image_t *li, uint16_t cyl, uint16_t head, uint16_t sec);

/* Convenience: create/attach logical image to ufm_disk_t (allocated). */
struct ufm_disk;
int ufm_disk_attach_logical(struct ufm_disk *d);

#ifdef __cplusplus
}
#endif
