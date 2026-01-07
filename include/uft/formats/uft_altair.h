/**
 * @file uft_altair.h
 * @brief Altair 8800 HD Floppy Disk Support for UFT
 * 
 * Support for Altair 8800 high-density floppy disk format as used
 * PC2FLOP.ASM transfer utilities.
 * 
 * The Altair HD format uses a unique track layout with 149 tracks
 * total, with the top side limited to tracks 0-71 and bottom side
 * covering tracks 72-148.
 * 
 * @copyright UFT Project
 */

#ifndef UFT_ALTAIR_H
#define UFT_ALTAIR_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Altair HD Floppy Constants
 *============================================================================*/

/** Total number of tracks */
#define UFT_ALTAIR_NUM_TRACKS       149

/** End of top side tracks (cylinder 72 and up use bottom only) */
#define UFT_ALTAIR_END_TOP          144  /* 72 * 2 */

/** Number of sectors per track */
#define UFT_ALTAIR_SECTORS_PER_TRACK 1

/** Bytes per sector (as transmitted) */
#define UFT_ALTAIR_SECTOR_SIZE      10240

/** Formatted data per sector */
#define UFT_ALTAIR_DATA_SIZE        9984

/** Track length in bytes */
#define UFT_ALTAIR_TRACK_LENGTH     (UFT_ALTAIR_SECTORS_PER_TRACK * UFT_ALTAIR_SECTOR_SIZE)

/** Total disk capacity */
#define UFT_ALTAIR_DISK_SIZE        (UFT_ALTAIR_NUM_TRACKS * UFT_ALTAIR_TRACK_LENGTH)

/** Minimum drive number */
#define UFT_ALTAIR_MIN_DRIVE        0

/** Maximum drive number */
#define UFT_ALTAIR_MAX_DRIVE        3

/** Number of read retries */
#define UFT_ALTAIR_READ_RETRIES     6

/** Number of write retries */
#define UFT_ALTAIR_WRITE_RETRIES    4

/*============================================================================
 * Altair FDC+ Controller Commands
 *============================================================================*/

/**
 * @brief FDC+ disk controller commands
 */
typedef enum {
    UFT_ALTAIR_CMD_STEP_IN  = 0x01,  /**< Step head toward center */
    UFT_ALTAIR_CMD_STEP_OUT = 0x02,  /**< Step head toward edge */
    UFT_ALTAIR_CMD_LOAD     = 0x04,  /**< Load head */
    UFT_ALTAIR_CMD_UNLOAD   = 0x08,  /**< Unload head */
    UFT_ALTAIR_CMD_WRITE    = 0x80,  /**< Write sector */
    UFT_ALTAIR_CMD_READ     = 0x40   /**< Read sector */
} uft_altair_cmd_t;

/*============================================================================
 * Altair FDC+ Status Bits
 *============================================================================*/

/** Track 0 detected */
#define UFT_ALTAIR_STAT_TRK0        0x01

/** Head movement in progress */
#define UFT_ALTAIR_STAT_MOVE        0x02

/** Drive selected */
#define UFT_ALTAIR_STAT_SELECT      0x04

/** Head loaded */
#define UFT_ALTAIR_STAT_HEAD        0x08

/** Write protect */
#define UFT_ALTAIR_STAT_WPROT       0x10

/** Sector found */
#define UFT_ALTAIR_STAT_FOUND       0x20

/** Data ready */
#define UFT_ALTAIR_STAT_READY       0x40

/** Error occurred */
#define UFT_ALTAIR_STAT_ERROR       0x80

/*============================================================================
 * Altair FDC+ I/O Ports (directly memory-mapped on Altair)
 *============================================================================*/

/** Drive select register */
#define UFT_ALTAIR_PORT_SELECT      0x08

/** Drive control register */
#define UFT_ALTAIR_PORT_CONTROL     0x09

/** Drive status register */
#define UFT_ALTAIR_PORT_STATUS      0x08

/** Track register */
#define UFT_ALTAIR_PORT_TRACK       0x09

/** Sector register */
#define UFT_ALTAIR_PORT_SECTOR      0x0A

/** Data register */
#define UFT_ALTAIR_PORT_DATA        0x0B

/*============================================================================
 * Timing Constants
 *============================================================================*/

/** Side change delay (microseconds) */
#define UFT_ALTAIR_SIDE_DELAY_US    200

/** Step settle time (milliseconds) */
#define UFT_ALTAIR_STEP_SETTLE_MS   20

/** Direction change delay (milliseconds) */
#define UFT_ALTAIR_DIR_CHANGE_MS    20

/** Trim erase wait (microseconds) */
#define UFT_ALTAIR_TRIM_ERASE_US    700

/** Head change delay (microseconds) */
#define UFT_ALTAIR_HEAD_CHANGE_US   200

/*============================================================================
 * Altair Disk Structures
 *============================================================================*/

/**
 * @brief Altair track structure
 */
typedef struct {
    uint8_t  cylinder;          /**< Logical cylinder (0-76) */
    uint8_t  head;              /**< Logical head (0-1) */
    uint8_t  track_num;         /**< Physical track number (0-148) */
    uint8_t  data[UFT_ALTAIR_SECTOR_SIZE];
    bool     valid;             /**< Data valid flag */
    bool     error;             /**< Read error flag */
} uft_altair_track_t;

/**
 * @brief Altair disk image
 */
typedef struct {
    uint8_t  num_tracks;        /**< Number of tracks (usually 149) */
    uint8_t  max_cylinder;      /**< Maximum cylinder used */
    bool     write_protected;   /**< Write protect flag */
    
    uft_altair_track_t tracks[UFT_ALTAIR_NUM_TRACKS];
    
    /* Statistics */
    uint32_t read_errors;       /**< Count of read errors */
    uint32_t write_errors;      /**< Count of write errors */
} uft_altair_image_t;

/*============================================================================
 * Track/Cylinder Conversion
 *============================================================================*/

/**
 * @brief Convert track number to cylinder and head
 * 
 * Track layout:
 * - Tracks 0-143: Cylinders 0-71, alternating heads (0,1,0,1,...)
 * - Tracks 144-148: Cylinders 72-76, bottom side only (head 1)
 * 
 * @param track Track number (0-148)
 * @param cylinder Output cylinder number
 * @param head Output head number
 * @return true if valid track number
 */
static inline bool uft_altair_track_to_chs(uint8_t track, 
                                            uint8_t* cylinder, 
                                            uint8_t* head)
{
    if (track >= UFT_ALTAIR_NUM_TRACKS) return false;
    
    if (track < UFT_ALTAIR_END_TOP) {
        /* Normal interleaved area */
        *cylinder = track / 2;
        *head = track & 1;
    } else {
        /* Bottom-only area (tracks 144-148 = cylinders 72-76) */
        *cylinder = (track - UFT_ALTAIR_END_TOP / 2);
        *head = 1;  /* Bottom side only */
    }
    
    return true;
}

/**
 * @brief Convert cylinder and head to track number
 * 
 * @param cylinder Cylinder number (0-76)
 * @param head Head number (0-1)
 * @return Track number or -1 if invalid
 */
static inline int uft_altair_chs_to_track(uint8_t cylinder, uint8_t head)
{
    if (cylinder > 76) return -1;
    if (head > 1) return -1;
    
    if (cylinder < 72) {
        /* Normal interleaved area */
        return cylinder * 2 + head;
    } else {
        /* Cylinders 72-76: only bottom side available */
        if (head == 0) return -1;  /* Top side not available here */
        return UFT_ALTAIR_END_TOP / 2 + cylinder;
    }
}

/*============================================================================
 * Altair API Functions
 *============================================================================*/

/**
 * @brief Initialize Altair disk image
 */
int uft_altair_init(uft_altair_image_t* img);

/**
 * @brief Free Altair disk image resources
 */
void uft_altair_free(uft_altair_image_t* img);

/**
 * @brief Read Altair disk image from file
 * 
 * Supports raw binary format (sequential tracks).
 */
int uft_altair_read(const char* filename, uft_altair_image_t* img);

/**
 * @brief Read Altair disk image from memory
 */
int uft_altair_read_mem(const uint8_t* data, size_t size, 
                        uft_altair_image_t* img);

/**
 * @brief Write Altair disk image to file
 */
int uft_altair_write(const char* filename, const uft_altair_image_t* img);

/**
 * @brief Get track by cylinder and head
 */
uft_altair_track_t* uft_altair_get_track(uft_altair_image_t* img,
                                          uint8_t cylinder, uint8_t head);

/**
 * @brief Read track data
 */
int uft_altair_read_track(const uft_altair_image_t* img, uint8_t track_num,
                          uint8_t* buffer, size_t size);

/**
 * @brief Convert to raw binary image
 */
int uft_altair_to_raw(const uft_altair_image_t* img, uint8_t** data,
                      size_t* size);

/**
 * @brief Print Altair image information
 */
void uft_altair_print_info(const uft_altair_image_t* img, bool verbose);

/*============================================================================
 * XMODEM Transfer Support (for original hardware)
 *============================================================================*/

/** XMODEM packet size */
#define UFT_XMODEM_PACKET_SIZE      128

/** XMODEM SOH (Start of Header) */
#define UFT_XMODEM_SOH              0x01

/** XMODEM EOT (End of Transmission) */
#define UFT_XMODEM_EOT              0x04

/** XMODEM ACK */
#define UFT_XMODEM_ACK              0x06

/** XMODEM NAK */
#define UFT_XMODEM_NAK              0x15

/** XMODEM EOF character */
#define UFT_XMODEM_EOF              0x1A

/**
 * @brief XMODEM packet structure
 */
typedef struct {
    uint8_t soh;                /**< Start of header (0x01) */
    uint8_t block_num;          /**< Block number (1-255, wraps) */
    uint8_t block_num_inv;      /**< Inverted block number */
    uint8_t data[UFT_XMODEM_PACKET_SIZE];
    uint8_t checksum;           /**< Simple sum checksum */
} uft_xmodem_packet_t;

#ifdef __cplusplus
}
#endif

#endif /* UFT_ALTAIR_H */
