/**
 * @file uft_c64_gcr.h
 * @brief Commodore 64/1541 GCR Encoding Support
 * @version 3.1.4.004
 *
 * Commodore-specific GCR (Group Coded Recording) for 1541 and compatible drives.
 *
 * Key differences from Apple GCR:
 * - 4 bits → 5 bits encoding (vs 6-and-2)
 * - Variable speed zones (17-21 sectors per track)
 * - XOR checksum (vs Apple's different algorithm)
 * - Different sync patterns and markers
 *
 * Track Layout:
 * - Tracks 1-17: 21 sectors, 307.69 kbit/s
 * - Tracks 18-24: 19 sectors, 285.71 kbit/s
 * - Tracks 25-30: 18 sectors, 266.67 kbit/s
 * - Tracks 31-35: 17 sectors, 250 kbit/s
 *
 * Based on bbc-fdc implementation and 1541 technical documentation.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_C64_GCR_H
#define UFT_C64_GCR_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * C64 GCR Constants
 *============================================================================*/

/** Sector size in bytes */
#define UFT_C64_SECTOR_SIZE     256

/** Maximum tracks on 1541 */
#define UFT_C64_MAX_TRACKS      35

/** Extended tracks (some disks use 40) */
#define UFT_C64_EXT_TRACKS      40

/** Total sectors on standard disk (683) */
#define UFT_C64_TOTAL_SECTORS   683

/** BAM track */
#define UFT_C64_BAM_TRACK       18

/** BAM sector */
#define UFT_C64_BAM_SECTOR      0

/** Directory starts at */
#define UFT_C64_DIR_TRACK       18
#define UFT_C64_DIR_SECTOR      1

/*============================================================================
 * GCR Encoding
 *============================================================================*/

/** Sync byte (NOT GCR encoded) */
#define UFT_C64_SYNC_BYTE       0xFF

/** Number of sync bytes before sector */
#define UFT_C64_SYNC_COUNT      5

/** ID block marker (GCR encoded as 0x52) */
#define UFT_C64_ID_MARKER       0x08

/** Data block marker (GCR encoded as 0x55) */
#define UFT_C64_DATA_MARKER     0x07

/** Off byte (gap filler) */
#define UFT_C64_OFF_BYTE        0x0F

/** Gap byte (between sectors) */
#define UFT_C64_GAP_BYTE        0x55

/*============================================================================
 * Speed Zones
 *============================================================================*/

/**
 * Get sectors per track for given track number.
 *
 * SSOT — canonical definition. Other headers (uft_floppy_encoding.h,
 * formats/uft_floppy_encoding.h, flux/uft_gcr.h) `#include` this header
 * and inherit the function; do not duplicate this body.
 *
 * Range matches uft_c64_speed_zone() (1..42 to cover 1541-IIC and modded
 * drives). Returns 0 for invalid track — never invent a sector count.
 */
static inline int uft_c64_sectors_per_track(int track) {
    if (track < 1 || track > 42) return 0;
    if (track <= 17) return 21;
    if (track <= 24) return 19;
    if (track <= 30) return 18;
    return 17;
}

/**
 * Get speed zone for C64/1541 track.
 *
 * SSOT (Single Source of Truth) — this is the canonical definition.
 * Other headers (uft_floppy_encoding.h, formats/uft_floppy_encoding.h,
 * uft_cbm_protection.h) `#include` this header and inherit the function;
 * they no longer carry their own copies. Do not duplicate this body.
 *
 *   Zone 3: tracks  1..17  (21 sectors)
 *   Zone 2: tracks 18..24  (19 sectors)
 *   Zone 1: tracks 25..30  (18 sectors)
 *   Zone 0: tracks 31..42  (17 sectors)  — 36..42 are the extended
 *                                          range used by 1541-IIC and
 *                                          host-modded drives; native
 *                                          1541 stops at 35.
 *   < 1 or > 42: -1 (invalid sentinel — callers MUST bounds-check).
 */
static inline int uft_c64_speed_zone(int track) {
    if (track < 1 || track > 42) return -1;
    if (track <= 17) return 3;
    if (track <= 24) return 2;
    if (track <= 30) return 1;
    return 0;
}

/**
 * Get bitrate for track (bits per second). Returns 0 for invalid track.
 */
static inline int uft_c64_track_bitrate(int track) {
    int zone = uft_c64_speed_zone(track);
    if (zone < 0) return 0;
    static const int rates[] = { 250000, 266667, 285714, 307692 };
    return rates[zone];
}

/**
 * Get bytes per track (approximately). Returns 0 for invalid track.
 */
static inline int uft_c64_bytes_per_track(int track) {
    int zone = uft_c64_speed_zone(track);
    if (zone < 0) return 0;
    /* MF-878: hier stand `{ 6250, 6667, 7143, 7692 }` — die aufgerundeten
     * Nominalwerte. Gemessen an `tests/corpus_free/vice_c1541_35trk.g64`
     * speichert eine echte, von VICEs c1541 erzeugte G64 6666 und 7142.
     * Die SSOT des Baums fuehrt dieselben Werte
     * (`src/formats/cbm/uft_cbm_geometry.c:64`). Diese Funktion hat
     * ausserhalb ihrer eigenen Definition keinen Aufrufer — die falschen
     * Zahlen hatten also keine Wirkung, standen aber in einem Header
     * bereit, sie zu bekommen. */
    static const int bytes[] = { 6250, 6666, 7142, 7692 };
    return bytes[zone];
}

/*============================================================================
 * GCR Encoding Tables
 *============================================================================*/

/**
 * 4-bit value to 5-bit GCR encoding
 *
 * Standard C64 GCR table:
 *   0x0→0x0A, 0x1→0x0B, 0x2→0x12, 0x3→0x13
 *   0x4→0x0E, 0x5→0x0F, 0x6→0x16, 0x7→0x17
 *   0x8→0x09, 0x9→0x19, 0xA→0x1A, 0xB→0x1B
 *   0xC→0x0D, 0xD→0x1D, 0xE→0x1E, 0xF→0x15
 */
extern const uint8_t UFT_C64_GCR_ENCODE[16];

/**
 * 5-bit GCR to 4-bit value decoding
 * Invalid entries are 0xFF
 */
extern const uint8_t UFT_C64_GCR_DECODE[32];

/*============================================================================
 * GCR Structures
 *============================================================================*/

/** Sector header (ID block) - before GCR encoding */
typedef struct {
    uint8_t     marker;         /**< 0x08 */
    uint8_t     checksum;       /**< XOR of sector, track, id2, id1 */
    uint8_t     sector;         /**< Sector number (0-20) */
    uint8_t     track;          /**< Track number (1-35) */
    uint8_t     id2;            /**< Disk ID byte 2 */
    uint8_t     id1;            /**< Disk ID byte 1 */
    uint8_t     off1;           /**< 0x0F */
    uint8_t     off2;           /**< 0x0F */
} uft_c64_sector_header_t;

/** Data block - before GCR encoding */
typedef struct {
    uint8_t     marker;         /**< 0x07 */
    uint8_t     data[UFT_C64_SECTOR_SIZE];
    uint8_t     checksum;       /**< XOR of all 256 data bytes */
    uint8_t     off1;           /**< 0x00 */
    uint8_t     off2;           /**< 0x00 */
} uft_c64_data_block_t;

/** Decoded sector */
typedef struct {
    int         track;
    int         sector;
    uint16_t    disk_id;
    uint8_t     data[UFT_C64_SECTOR_SIZE];
    bool        header_valid;
    bool        data_valid;
    bool        header_checksum_ok;
    bool        data_checksum_ok;
} uft_c64_sector_t;

/*============================================================================
 * GCR Encoding/Decoding Functions
 *============================================================================*/





/*============================================================================
 * Sector Operations
 *============================================================================*/






/*============================================================================
 * Bitstream Processing
 *============================================================================*/

/* MF-565: `uft_c64_parser_t` + `uft_c64_parser_init/add_bit` ENTFERNT.
 *
 * Der Parser war ein Stub — add_bit() schob Bits in ein Schieberegister,
 * "Full sync-pattern detection + sector extraction deferred" stand als
 * Kommentar daneben. Sein einziger Aufrufer, der Fluss-Pfad
 * `uftc_convert_scp_to_d64()`, benutzt seit MF-565 den GCR-Dekoder, den
 * der G64-Pfad schon hatte und der seit MF-536 gegen VICE gemessen ist:
 * @ref uft_cbm_gcr_track_to_sectors (include/uft/formats/c64/uft_d64_g64.h).
 *
 * Gemessen an einer fehlerfreien synthetischen Aufnahme: mit dem Stub
 * 0 von 683 Sektoren, mit dem geprueften Dekoder 683 von 683.
 * Nicht wieder einfuehren — ein zweiter GCR-Parser ist die Krankheit,
 * gegen die MF-436 den einen herausgezogen hat.
 */


/*============================================================================
 * D64/G64 Format Support
 *============================================================================*/

/** D64 file size (35 tracks, no error info) */
#define UFT_D64_SIZE_35         174848

/** D64 file size (35 tracks, with error info) */
#define UFT_D64_SIZE_35_ERR     175531

/** D64 file size (40 tracks, no error info) */
#define UFT_D64_SIZE_40         196608

/** D64 file size (40 tracks, with error info) */
#define UFT_D64_SIZE_40_ERR     197376



#ifdef __cplusplus
}
#endif

#endif /* UFT_C64_GCR_H */
