/**
 * @file sector_search.h
 * @brief Sector search and extraction for UFT compatibility
 */

#ifndef SECTOR_SEARCH_H
#define SECTOR_SEARCH_H

#include "libhxcfe.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Search Flags
 * ============================================================================ */

#define SECTOR_SEARCH_IGNORE_CRC        0x0001
#define SECTOR_SEARCH_IGNORE_DELETED    0x0002
#define SECTOR_SEARCH_FIRST_ONLY        0x0004
#define SECTOR_SEARCH_ALL_REVOLUTIONS   0x0008
#define SECTOR_SEARCH_WEAK_BITS         0x0010

/* ============================================================================
 * Sector Search Result
 * ============================================================================ */

typedef struct {
    int32_t  cylinder;
    int32_t  head;
    int32_t  sector;
    int32_t  size;          /* Sector size code (0=128, 1=256, ...) */
    int32_t  sectorsize;    /* Actual size in bytes */
    
    uint8_t* data;          /* Sector data (caller must free) */
    
    int32_t  crc_status;    /* 0=OK, -1=header CRC error, -2=data CRC error */
    uint16_t header_crc;
    uint16_t data_crc;
    
    int32_t  deleted;       /* Deleted data address mark? */
    int32_t  encoding;      /* Track encoding type */
    
    int32_t  bitposition;   /* Start position in track (bits) */
    int32_t  bitlength;     /* Total sector length (bits) */
    
    int32_t  revolution;    /* Which revolution (multi-rev captures) */
    int32_t  alternate;     /* Alternate sector (weak bits) */
    
} sector_search_result_t;

/* ============================================================================
 * Sector Search Context
 * ============================================================================ */

typedef struct {
    HXCFE* hxcfe;
    HXCFE_FLOPPY* floppy;
    
    int32_t flags;
    
    /* Current position */
    int32_t current_track;
    int32_t current_side;
    int32_t current_bitpos;
    
    /* Results */
    sector_search_result_t* results;
    int32_t result_count;
    int32_t result_capacity;
    
} sector_search_ctx_t;

/* ============================================================================
 * Function Prototypes
 * ============================================================================ */

/**
 * Initialize sector search context
 */
sector_search_ctx_t* sector_search_init(HXCFE* hxcfe, HXCFE_FLOPPY* floppy);

/**
 * Free sector search context
 */
void sector_search_free(sector_search_ctx_t* ctx);

/**
 * Search for sectors on a track
 */
int32_t sector_search_track(
    sector_search_ctx_t* ctx,
    int32_t track,
    int32_t side,
    int32_t encoding,
    int32_t flags
);

/**
 * Get sector by C/H/S
 */
sector_search_result_t* sector_search_get(
    sector_search_ctx_t* ctx,
    int32_t cylinder,
    int32_t head,
    int32_t sector
);

/**
 * Read sector data
 */
int32_t sector_read(
    HXCFE_SECTORACCESS* ss,
    int32_t track,
    int32_t side,
    int32_t sector,
    int32_t sectorsize,
    int32_t encoding,
    uint8_t* buffer
);

/**
 * Write sector data
 */
int32_t sector_write(
    HXCFE_SECTORACCESS* ss,
    int32_t track,
    int32_t side,
    int32_t sector,
    int32_t sectorsize,
    int32_t encoding,
    const uint8_t* buffer
);

/**
 * Get sector count on track
 */
int32_t sector_count(
    HXCFE* hxcfe,
    HXCFE_FLOPPY* floppy,
    int32_t track,
    int32_t side,
    int32_t encoding
);

#ifdef __cplusplus
}
#endif

#endif /* SECTOR_SEARCH_H */
