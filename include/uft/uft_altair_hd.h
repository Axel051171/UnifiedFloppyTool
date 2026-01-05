/**
 * @file uft_altair_hd.h
 * @brief Altair High-Density Floppy Format Support for UFT
 * 
 * Support for Altair 8800 high-density floppy disk format used with
 * the FDC+ controller. Based on analysis of FLOP2PC.ASM and PC2FLOP.ASM.
 * 
 * The Altair HD format uses 8" drives with a unique track layout:
 * - 149 tracks total (77 cylinders × 2 sides, minus top side of cyl 72+)
 * - 1 sector per track
 * - 10,240 bytes per sector (track)
 * - ~1.5MB total capacity
 * 
 * @copyright UFT Project
 */

#ifndef UFT_ALTAIR_HD_H
#define UFT_ALTAIR_HD_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Altair HD Format Constants
 *============================================================================*/

/** Number of tracks in Altair HD format */
#define UFT_ALTAIR_NUM_TRACKS       149

/** Track number where top side ends (cylinder 72 * 2) */
#define UFT_ALTAIR_END_TOP          144

/** Number of sectors per track */
#define UFT_ALTAIR_SECTORS_PER_TRACK 1

/** Sector/track length in bytes */
#define UFT_ALTAIR_SECTOR_LENGTH    10240

/** Track length in bytes (same as sector for 1 sector/track) */
#define UFT_ALTAIR_TRACK_LENGTH     10240

/** Total disk capacity */
#define UFT_ALTAIR_DISK_SIZE        (UFT_ALTAIR_NUM_TRACKS * UFT_ALTAIR_TRACK_LENGTH)

/** Minimum drive number */
#define UFT_ALTAIR_MIN_DRIVE        0

/** Maximum drive number */
#define UFT_ALTAIR_MAX_DRIVE        3

/** Number of read retry attempts */
#define UFT_ALTAIR_READ_RETRIES     6

/** Number of write retry attempts */
#define UFT_ALTAIR_WRITE_RETRIES    4

/** CPU speed for timing (kHz) */
#define UFT_ALTAIR_CPU_SPEED        2000

/*============================================================================
 * FDC+ Controller I/O Ports
 *============================================================================*/

/** Drive select register */
#define UFT_ALTAIR_PORT_DRV_SEL     0x08

/** Drive control register */
#define UFT_ALTAIR_PORT_DRV_CTL     0x09

/** Drive status register */
#define UFT_ALTAIR_PORT_DRV_STAT    0x08

/** Track number register */
#define UFT_ALTAIR_PORT_DRV_TRK     0x0A

/*============================================================================
 * Drive Control Commands
 *============================================================================*/

/** Step head inward (toward center) */
#define UFT_ALTAIR_CMD_STEP_IN      0x01

/** Step head outward (toward edge) */
#define UFT_ALTAIR_CMD_STEP_OUT     0x02

/** Load head */
#define UFT_ALTAIR_CMD_HEAD_LOAD    0x04

/** Unload head */
#define UFT_ALTAIR_CMD_HEAD_UNLOAD  0x08

/** Deselect drive */
#define UFT_ALTAIR_DESELECT         0x00

/*============================================================================
 * Drive Status Bits
 *============================================================================*/

/** Track 0 detected */
#define UFT_ALTAIR_STAT_TRACK0      0x01

/** Head movement in progress */
#define UFT_ALTAIR_STAT_MOVING      0x02

/** Head is loaded */
#define UFT_ALTAIR_STAT_HEAD_LOADED 0x04

/** Drive is selected */
#define UFT_ALTAIR_STAT_SELECTED    0x08

/** Write protected */
#define UFT_ALTAIR_STAT_WRITE_PROT  0x10

/*============================================================================
 * Track/Cylinder Conversion
 *============================================================================*/

/**
 * @brief Convert track number to cylinder
 * 
 * Tracks 0-143: Normal interleaved (cylinder = track / 2)
 * Tracks 144-148: Bottom side only (cylinder = track - 72)
 * 
 * @param track Track number (0-148)
 * @return Cylinder number (0-76)
 */
static inline uint8_t uft_altair_track_to_cylinder(uint8_t track)
{
    if (track < UFT_ALTAIR_END_TOP) {
        return track / 2;
    } else {
        return track - (UFT_ALTAIR_END_TOP / 2);
    }
}

/**
 * @brief Get side/head from track number
 * 
 * @param track Track number (0-148)
 * @return 0 for bottom, 1 for top (tracks >= 144 are always bottom)
 */
static inline uint8_t uft_altair_track_to_side(uint8_t track)
{
    if (track < UFT_ALTAIR_END_TOP) {
        return track & 1;
    } else {
        return 0;  /* Top side not used for cylinders >= 72 */
    }
}

/**
 * @brief Convert cylinder and side to track number
 * 
 * @param cylinder Cylinder number (0-76)
 * @param side Side (0=bottom, 1=top)
 * @return Track number (0-148) or 0xFF if invalid
 */
static inline uint8_t uft_altair_cyl_side_to_track(uint8_t cylinder, uint8_t side)
{
    if (cylinder >= 77) return 0xFF;
    
    if (cylinder < 72) {
        /* Normal interleaved area */
        return cylinder * 2 + side;
    } else {
        /* Beyond cylinder 71, only bottom side */
        if (side != 0) return 0xFF;
        return cylinder + (UFT_ALTAIR_END_TOP / 2);
    }
}

/*============================================================================
 * Altair HD Image Structure
 *============================================================================*/

/**
 * @brief Altair HD disk image
 */
typedef struct {
    uint8_t* data;              /**< Raw disk data (149 * 10240 bytes) */
    size_t   size;              /**< Data size */
    
    /* Track status */
    uint8_t  track_status[UFT_ALTAIR_NUM_TRACKS]; /**< Status per track */
    
    /* Metadata */
    bool     write_protected;   /**< Write protect flag */
    uint8_t  drive_num;         /**< Original drive number */
} uft_altair_image_t;

/** Track status flags */
#define UFT_ALTAIR_TRACK_OK         0x00
#define UFT_ALTAIR_TRACK_ERROR      0x01
#define UFT_ALTAIR_TRACK_MISSING    0x02

/*============================================================================
 * XMODEM Protocol Constants (for serial transfer)
 *============================================================================*/

/** XMODEM packet size */
#define UFT_XMODEM_PACKET_SIZE      128

/** Start of Header */
#define UFT_XMODEM_SOH              0x01

/** End of Transmission */
#define UFT_XMODEM_EOT              0x04

/** Acknowledge */
#define UFT_XMODEM_ACK              0x06

/** Negative Acknowledge */
#define UFT_XMODEM_NAK              0x15

/** End of File marker (Ctrl-Z) */
#define UFT_XMODEM_EOF              0x1A

/*============================================================================
 * Altair HD API Functions
 *============================================================================*/

/**
 * @brief Initialize Altair HD image structure
 */
int uft_altair_init(uft_altair_image_t* img);

/**
 * @brief Free Altair HD image resources
 */
void uft_altair_free(uft_altair_image_t* img);

/**
 * @brief Create empty Altair HD image
 * @param img Image structure to initialize
 * @param fill Fill byte for empty sectors
 * @return 0 on success
 */
int uft_altair_create(uft_altair_image_t* img, uint8_t fill);

/**
 * @brief Read Altair HD image from raw file
 * 
 * Expects a 1,525,760 byte raw image file.
 */
int uft_altair_read(const char* filename, uft_altair_image_t* img);

/**
 * @brief Read Altair HD image from memory
 */
int uft_altair_read_mem(const uint8_t* data, size_t size, uft_altair_image_t* img);

/**
 * @brief Write Altair HD image to file
 */
int uft_altair_write(const char* filename, const uft_altair_image_t* img);

/**
 * @brief Read track data
 * @param img Altair image
 * @param track Track number (0-148)
 * @param buffer Output buffer (must be 10240 bytes)
 * @return Bytes read or negative error
 */
int uft_altair_read_track(const uft_altair_image_t* img, uint8_t track,
                          uint8_t* buffer);

/**
 * @brief Write track data
 * @param img Altair image
 * @param track Track number (0-148)
 * @param buffer Input buffer (10240 bytes)
 * @return 0 on success
 */
int uft_altair_write_track(uft_altair_image_t* img, uint8_t track,
                           const uint8_t* buffer);

/**
 * @brief Get track offset in image
 * @param track Track number
 * @return Byte offset or -1 if invalid
 */
static inline int32_t uft_altair_track_offset(uint8_t track)
{
    if (track >= UFT_ALTAIR_NUM_TRACKS) return -1;
    return (int32_t)track * UFT_ALTAIR_TRACK_LENGTH;
}

/**
 * @brief Print Altair HD image information
 */
void uft_altair_print_info(const uft_altair_image_t* img, bool verbose);

/*============================================================================
 * Timing Functions
 *============================================================================*/

/**
 * @brief Calculate delay loop count for given microseconds
 * @param us Microseconds to delay
 * @param cpu_khz CPU speed in kHz
 * @return Loop iteration count
 */
static inline uint16_t uft_altair_delay_count(uint16_t us, uint16_t cpu_khz)
{
    /* 20 cycles per loop iteration */
    return (uint16_t)((uint32_t)us * cpu_khz / 20000);
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_ALTAIR_HD_H */
