/**
 * @file uft_fdc_emu.h
 * @brief Floppy Disk Controller Emulation for UFT
 * @version 3.1.4.002
 *
 * Emulates WD179x (Western Digital) and i8272/NEC765 (Intel/NEC) FDCs
 * for accurate disk image interpretation and creation.
 *
 * Based on AltairZ80 SIMH implementation by Howard M. Harte (MIT License)
 * Adapted for UFT by extraction of core algorithms without simulator dependencies.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_FDC_EMU_H
#define UFT_FDC_EMU_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * FDC Types
 *============================================================================*/

/** FDC controller types */
typedef enum {
    UFT_FDC_WD1770      = 1770,  /**< WD1770 - No side select */
    UFT_FDC_WD1772      = 1772,  /**< WD1772 - Atari ST variant */
    UFT_FDC_WD1793      = 1793,  /**< WD1793 - Standard */
    UFT_FDC_WD1795      = 1795,  /**< WD1795 - With side select */
    UFT_FDC_WD1797      = 1797,  /**< WD1797 - With side select */
    UFT_FDC_WD2793      = 2793,  /**< WD2793 - Inverted bus */
    UFT_FDC_I8272       = 8272,  /**< Intel 8272 */
    UFT_FDC_NEC765      = 765,   /**< NEC 765 (i8272 compatible) */
    UFT_FDC_DP8473      = 8473,  /**< National DP8473 */
    UFT_FDC_PC8477      = 8477,  /**< National PC8477 */
} uft_fdc_type_t;

/*============================================================================
 * WD179x Status Register Bits (Type I Commands)
 *============================================================================*/

#define UFT_WD_STAT_NOT_READY   (1 << 7)  /**< Drive not ready */
#define UFT_WD_STAT_WPROT       (1 << 6)  /**< Write protected */
#define UFT_WD_STAT_HLD         (1 << 5)  /**< Head loaded (Type I) */
#define UFT_WD_STAT_SEEK_ERROR  (1 << 4)  /**< Seek error */
#define UFT_WD_STAT_CRC_ERROR   (1 << 3)  /**< CRC error */
#define UFT_WD_STAT_TRACK0      (1 << 2)  /**< Track 0 */
#define UFT_WD_STAT_INDEX       (1 << 1)  /**< Index pulse */
#define UFT_WD_STAT_BUSY        (1 << 0)  /**< Busy */

/* Type II/III specific */
#define UFT_WD_STAT_REC_TYPE    (1 << 5)  /**< Record type / Write fault */
#define UFT_WD_STAT_NOT_FOUND   (1 << 4)  /**< Record not found */
#define UFT_WD_STAT_LOST_DATA   (1 << 2)  /**< Lost data */
#define UFT_WD_STAT_DRQ         (1 << 1)  /**< Data request */

/*============================================================================
 * WD179x Command Opcodes
 *============================================================================*/

#define UFT_WD_CMD_RESTORE      0x00  /**< Type I: Restore (seek track 0) */
#define UFT_WD_CMD_SEEK         0x10  /**< Type I: Seek */
#define UFT_WD_CMD_STEP         0x20  /**< Type I: Step */
#define UFT_WD_CMD_STEP_U       0x30  /**< Type I: Step with update */
#define UFT_WD_CMD_STEP_IN      0x40  /**< Type I: Step in */
#define UFT_WD_CMD_STEP_IN_U    0x50  /**< Type I: Step in with update */
#define UFT_WD_CMD_STEP_OUT     0x60  /**< Type I: Step out */
#define UFT_WD_CMD_STEP_OUT_U   0x70  /**< Type I: Step out with update */
#define UFT_WD_CMD_READ_SEC     0x80  /**< Type II: Read sector */
#define UFT_WD_CMD_READ_SEC_M   0x90  /**< Type II: Read multiple sectors */
#define UFT_WD_CMD_WRITE_SEC    0xA0  /**< Type II: Write sector */
#define UFT_WD_CMD_WRITE_SEC_M  0xB0  /**< Type II: Write multiple sectors */
#define UFT_WD_CMD_READ_ADDR    0xC0  /**< Type III: Read address */
#define UFT_WD_CMD_FORCE_INT    0xD0  /**< Type IV: Force interrupt */
#define UFT_WD_CMD_READ_TRACK   0xE0  /**< Type III: Read track */
#define UFT_WD_CMD_WRITE_TRACK  0xF0  /**< Type III: Write track (format) */

/*============================================================================
 * i8272/NEC765 Command Opcodes
 *============================================================================*/

#define UFT_I82_CMD_READ_TRACK          0x02
#define UFT_I82_CMD_SPECIFY             0x03
#define UFT_I82_CMD_SENSE_DRIVE_STATUS  0x04
#define UFT_I82_CMD_WRITE_DATA          0x05
#define UFT_I82_CMD_READ_DATA           0x06
#define UFT_I82_CMD_RECALIBRATE         0x07
#define UFT_I82_CMD_SENSE_INT_STATUS    0x08
#define UFT_I82_CMD_WRITE_DELETED       0x09
#define UFT_I82_CMD_READ_ID             0x0A
#define UFT_I82_CMD_READ_DELETED        0x0C
#define UFT_I82_CMD_FORMAT_TRACK        0x0D
#define UFT_I82_CMD_SEEK                0x0F
#define UFT_I82_CMD_SCAN_EQUAL          0x11
#define UFT_I82_CMD_SCAN_LOW_EQUAL      0x19
#define UFT_I82_CMD_SCAN_HIGH_EQUAL     0x1D

/*============================================================================
 * Data Structures
 *============================================================================*/

/** Maximum sectors per track */
#define UFT_FDC_MAX_SECTORS     26

/** Maximum sector size (8KB) */
#define UFT_FDC_MAX_SECTOR_SIZE 8192

/** Drive information */
typedef struct uft_fdc_drive {
    uint8_t     tracks;         /**< Number of tracks */
    uint8_t     heads;          /**< Number of heads (1 or 2) */
    uint8_t     current_track;  /**< Current head position */
    uint8_t     ready;          /**< Drive ready flag */
    uint8_t     write_protect;  /**< Write protect flag */
    uint32_t    sector_size;    /**< Sector size in bytes */
    uint8_t     sectors_per_track; /**< Sectors per track */
    void       *user_data;      /**< User data pointer */
} uft_fdc_drive_t;

/** FDC state */
typedef struct uft_fdc_state {
    uft_fdc_type_t  type;           /**< Controller type */
    
    /* Registers */
    uint8_t     status;             /**< Status register */
    uint8_t     track;              /**< Track register */
    uint8_t     sector;             /**< Sector register */
    uint8_t     data;               /**< Data register */
    uint8_t     command;            /**< Last command */
    
    /* State */
    uint8_t     head;               /**< Selected head */
    uint8_t     sel_drive;          /**< Selected drive (0-3) */
    uint8_t     double_density;     /**< DD mode (MFM) */
    uint8_t     step_dir;           /**< Step direction (+1/-1) */
    uint8_t     busy;               /**< Busy flag */
    uint8_t     drq;                /**< Data request */
    uint8_t     irq;                /**< Interrupt request */
    
    /* Transfer state */
    uint16_t    data_count;         /**< Remaining bytes */
    uint16_t    data_index;         /**< Current position */
    uint8_t     sector_buffer[UFT_FDC_MAX_SECTOR_SIZE];
    
    /* Format track state */
    uint8_t     fmt_state;          /**< Format state machine */
    uint8_t     fmt_sector_count;   /**< Sectors formatted */
    uint8_t     fmt_sector_map[UFT_FDC_MAX_SECTORS];
    uint8_t     gap[4];             /**< Gap lengths */
    
    /* Drives */
    uft_fdc_drive_t drives[4];
    
    /* Callbacks */
    int (*read_sector)(struct uft_fdc_state *fdc, uint8_t drive, 
                       uint8_t track, uint8_t head, uint8_t sector,
                       uint8_t *buffer, size_t *len);
    int (*write_sector)(struct uft_fdc_state *fdc, uint8_t drive,
                        uint8_t track, uint8_t head, uint8_t sector,
                        const uint8_t *buffer, size_t len);
    int (*format_track)(struct uft_fdc_state *fdc, uint8_t drive,
                        uint8_t track, uint8_t head, uint8_t fill,
                        const uint8_t *sector_map, uint8_t sector_count);
} uft_fdc_state_t;

/*============================================================================
 * Sector Size Encoding
 *============================================================================*/

/**
 * @brief Convert sector size code to bytes
 * 
 * WD179x/i8272 use: bytes = 128 << N
 *   N=0: 128, N=1: 256, N=2: 512, N=3: 1024, N=4: 2048, N=5: 4096, N=6: 8192
 */
static inline uint32_t uft_fdc_sector_size(uint8_t n) {
    if (n > 6) n = 6;
    return 128u << n;
}

/**
 * @brief Convert sector size bytes to code
 */
static inline uint8_t uft_fdc_sector_code(uint32_t bytes) {
    uint8_t n = 0;
    bytes >>= 7;  /* Divide by 128 */
    while (bytes > 1 && n < 6) {
        bytes >>= 1;
        n++;
    }
    return n;
}

/*============================================================================
 * API Functions
 *============================================================================*/

/**
 * @brief Initialize FDC state
 * @param fdc FDC state structure to initialize
 * @param type Controller type
 * @return 0 on success, -1 on error
 */
int uft_fdc_init(uft_fdc_state_t *fdc, uft_fdc_type_t type);

/**
 * @brief Reset FDC to initial state
 * @param fdc FDC state
 */
void uft_fdc_reset(uft_fdc_state_t *fdc);

/**
 * @brief Write to FDC register (WD179x style)
 * @param fdc FDC state
 * @param reg Register (0=command/status, 1=track, 2=sector, 3=data)
 * @param value Value to write
 * @return 0 on success
 */
int uft_fdc_wd_write(uft_fdc_state_t *fdc, uint8_t reg, uint8_t value);

/**
 * @brief Read from FDC register (WD179x style)
 * @param fdc FDC state
 * @param reg Register (0=status, 1=track, 2=sector, 3=data)
 * @return Register value
 */
uint8_t uft_fdc_wd_read(uft_fdc_state_t *fdc, uint8_t reg);

/**
 * @brief Execute FDC command byte (i8272 style)
 * @param fdc FDC state
 * @param data Command or data byte
 * @return Result byte or -1 if none available
 */
int uft_fdc_i82_write(uft_fdc_state_t *fdc, uint8_t data);

/**
 * @brief Read result byte (i8272 style)
 * @param fdc FDC state
 * @return Result byte or -1 if none available
 */
int uft_fdc_i82_read(uft_fdc_state_t *fdc);

/**
 * @brief Get command type for WD179x command
 * @param cmd Command byte
 * @return Command type (1-4)
 */
int uft_fdc_wd_cmd_type(uint8_t cmd);

/**
 * @brief Get command name for debugging
 * @param fdc FDC state
 * @param cmd Command byte
 * @return Command name string
 */
const char *uft_fdc_cmd_name(uft_fdc_state_t *fdc, uint8_t cmd);

/*============================================================================
 * Track Layout Constants
 *============================================================================*/

/** Standard gap lengths for various formats */
typedef struct uft_gap_lengths {
    uint8_t gap1;   /**< Post-index gap */
    uint8_t gap2;   /**< Post-ID gap */
    uint8_t gap3;   /**< Post-data gap */
    uint8_t gap4;   /**< Pre-index gap */
} uft_gap_lengths_t;

/** Standard IBM PC 1.44MB format */
extern const uft_gap_lengths_t UFT_GAP_IBM_HD;

/** Standard IBM PC 720KB format */
extern const uft_gap_lengths_t UFT_GAP_IBM_DD;

/** Atari ST format */
extern const uft_gap_lengths_t UFT_GAP_ATARI_ST;

/** Amiga format */
extern const uft_gap_lengths_t UFT_GAP_AMIGA;

#ifdef __cplusplus
}
#endif

#endif /* UFT_FDC_EMU_H */
