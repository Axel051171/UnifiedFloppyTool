/**
 * @file uft_fdc.h
 * @brief Low-Level Floppy Disk Controller (FDC) Support for UFT
 * 
 * This header provides structures and definitions for working with
 * NEC µPD765 compatible floppy disk controllers and related hardware.
 * 
 * Based on analysis of fdformat, fdread, ImageDisk, and Linux kernel sources.
 * 
 * @copyright UFT Project
 */

#ifndef UFT_FDC_H
#define UFT_FDC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * FDC Register Addresses (PC Standard)
 *============================================================================*/

/** Primary FDC base address */
#define UFT_FDC_BASE_PRIMARY    0x3F0

/** Secondary FDC base address */
#define UFT_FDC_BASE_SECONDARY  0x370

/* Register offsets from base */
#define UFT_FDC_REG_SRA         0x00  /**< Status Register A (PS/2) */
#define UFT_FDC_REG_SRB         0x01  /**< Status Register B (PS/2) */
#define UFT_FDC_REG_DOR         0x02  /**< Digital Output Register */
#define UFT_FDC_REG_TDR         0x03  /**< Tape Drive Register */
#define UFT_FDC_REG_MSR         0x04  /**< Main Status Register */
#define UFT_FDC_REG_DSR         0x04  /**< Data Rate Select (write) */
#define UFT_FDC_REG_DATA        0x05  /**< Data (FIFO) Register */
#define UFT_FDC_REG_DIR         0x07  /**< Digital Input Register */
#define UFT_FDC_REG_CCR         0x07  /**< Config Control Register (write) */

/*============================================================================
 * Digital Output Register (DOR) Bits
 *============================================================================*/

#define UFT_DOR_DRIVE_SEL_MASK  0x03  /**< Drive select (0-3) */
#define UFT_DOR_RESET           0x04  /**< Reset FDC (0=reset) */
#define UFT_DOR_DMA_ENABLE      0x08  /**< Enable DMA and IRQ */
#define UFT_DOR_MOTOR_A         0x10  /**< Motor enable drive A */
#define UFT_DOR_MOTOR_B         0x20  /**< Motor enable drive B */
#define UFT_DOR_MOTOR_C         0x40  /**< Motor enable drive C */
#define UFT_DOR_MOTOR_D         0x80  /**< Motor enable drive D */

/*============================================================================
 * Main Status Register (MSR) Bits
 *============================================================================*/

#define UFT_MSR_BUSY_A          0x01  /**< Drive A busy */
#define UFT_MSR_BUSY_B          0x02  /**< Drive B busy */
#define UFT_MSR_BUSY_C          0x04  /**< Drive C busy */
#define UFT_MSR_BUSY_D          0x08  /**< Drive D busy */
#define UFT_MSR_CMD_BUSY        0x10  /**< Command in progress */
#define UFT_MSR_NON_DMA         0x20  /**< Non-DMA mode */
#define UFT_MSR_DIO             0x40  /**< Data direction (1=read) */
#define UFT_MSR_RQM             0x80  /**< Request for Master */

/*============================================================================
 * Status Register 0 (ST0) Bits
 *============================================================================*/

#define UFT_ST0_UNIT_SEL        0x03  /**< Unit select */
#define UFT_ST0_HEAD            0x04  /**< Head address */
#define UFT_ST0_NOT_READY       0x08  /**< Drive not ready */
#define UFT_ST0_EQUIP_CHECK     0x10  /**< Equipment check */
#define UFT_ST0_SEEK_END        0x20  /**< Seek end */
#define UFT_ST0_IC_MASK         0xC0  /**< Interrupt code mask */
#define UFT_ST0_IC_NORMAL       0x00  /**< Normal termination */
#define UFT_ST0_IC_ABNORMAL     0x40  /**< Abnormal termination */
#define UFT_ST0_IC_INVALID      0x80  /**< Invalid command */
#define UFT_ST0_IC_READY_CHG    0xC0  /**< Ready changed */

/*============================================================================
 * Status Register 1 (ST1) Bits
 *============================================================================*/

#define UFT_ST1_MISSING_AM      0x01  /**< Missing address mark */
#define UFT_ST1_NOT_WRITABLE    0x02  /**< Not writable */
#define UFT_ST1_NO_DATA         0x04  /**< No data */
#define UFT_ST1_OVERRUN         0x10  /**< Overrun/underrun */
#define UFT_ST1_CRC_ERROR       0x20  /**< CRC error in data */
#define UFT_ST1_END_CYL         0x80  /**< End of cylinder */

/*============================================================================
 * Status Register 2 (ST2) Bits
 *============================================================================*/

#define UFT_ST2_MISSING_DAM     0x01  /**< Missing data address mark */
#define UFT_ST2_BAD_CYL         0x02  /**< Bad cylinder */
#define UFT_ST2_SCAN_NOT_SAT    0x04  /**< Scan not satisfied */
#define UFT_ST2_SCAN_EQUAL      0x08  /**< Scan equal hit */
#define UFT_ST2_WRONG_CYL       0x10  /**< Wrong cylinder */
#define UFT_ST2_CRC_ERROR_DATA  0x20  /**< CRC error in data field */
#define UFT_ST2_DELETED_DAM     0x40  /**< Deleted data address mark */

/*============================================================================
 * Status Register 3 (ST3) Bits
 *============================================================================*/

#define UFT_ST3_UNIT_SEL        0x03  /**< Unit select */
#define UFT_ST3_HEAD            0x04  /**< Side select */
#define UFT_ST3_TWO_SIDE        0x08  /**< Two-sided drive */
#define UFT_ST3_TRACK_0         0x10  /**< Track 0 */
#define UFT_ST3_READY           0x20  /**< Drive ready */
#define UFT_ST3_WRITE_PROT      0x40  /**< Write protected */
#define UFT_ST3_FAULT           0x80  /**< Fault */

/*============================================================================
 * FDC Commands
 *============================================================================*/

typedef enum {
    UFT_FDC_CMD_SPECIFY         = 0x03,  /**< Specify drive timing */
    UFT_FDC_CMD_SENSE_STATUS    = 0x04,  /**< Sense drive status */
    UFT_FDC_CMD_WRITE           = 0x05,  /**< Write data */
    UFT_FDC_CMD_READ            = 0x06,  /**< Read data */
    UFT_FDC_CMD_RECALIBRATE     = 0x07,  /**< Recalibrate (seek to 0) */
    UFT_FDC_CMD_SENSE_INT       = 0x08,  /**< Sense interrupt */
    UFT_FDC_CMD_WRITE_DELETED   = 0x09,  /**< Write deleted data */
    UFT_FDC_CMD_READ_ID         = 0x0A,  /**< Read sector ID */
    UFT_FDC_CMD_READ_DELETED    = 0x0C,  /**< Read deleted data */
    UFT_FDC_CMD_FORMAT          = 0x0D,  /**< Format track */
    UFT_FDC_CMD_DUMPREG         = 0x0E,  /**< Dump registers */
    UFT_FDC_CMD_SEEK            = 0x0F,  /**< Seek to cylinder */
    UFT_FDC_CMD_VERSION         = 0x10,  /**< Get version */
    UFT_FDC_CMD_SCAN_EQ         = 0x11,  /**< Scan equal */
    UFT_FDC_CMD_PERPENDICULAR   = 0x12,  /**< Perpendicular mode */
    UFT_FDC_CMD_CONFIGURE       = 0x13,  /**< Configure */
    UFT_FDC_CMD_LOCK            = 0x14,  /**< Lock configuration */
    UFT_FDC_CMD_VERIFY          = 0x16,  /**< Verify data */
    UFT_FDC_CMD_SCAN_LE         = 0x19,  /**< Scan low or equal */
    UFT_FDC_CMD_SCAN_GE         = 0x1D,  /**< Scan high or equal */
    UFT_FDC_CMD_RELATIVE_SEEK   = 0x8F   /**< Relative seek */
} uft_fdc_cmd_t;

/* Command modifier bits */
#define UFT_FDC_MOD_MT          0x80  /**< Multi-track */
#define UFT_FDC_MOD_MFM         0x40  /**< MFM mode (vs FM) */
#define UFT_FDC_MOD_SK          0x20  /**< Skip deleted */

/*============================================================================
 * Data Rate Selection
 *============================================================================*/

typedef enum {
    UFT_FDC_RATE_500K   = 0x00,  /**< 500 kbps (HD, 8") */
    UFT_FDC_RATE_300K   = 0x01,  /**< 300 kbps (DD on HD drive) */
    UFT_FDC_RATE_250K   = 0x02,  /**< 250 kbps (DD) */
    UFT_FDC_RATE_1M     = 0x03   /**< 1 Mbps (ED) */
} uft_fdc_rate_t;

/*============================================================================
 * Floppy Drive Parameters (from Linux kernel)
 *============================================================================*/

/**
 * @brief Floppy drive parameters
 * Based on Linux floppy_drive_params structure
 */
typedef struct {
    uint8_t  cmos_type;         /**< CMOS drive type code */
    uint16_t max_rate;          /**< Maximum data rate */
    uint16_t head_load_time;    /**< Head load time (ms) */
    uint16_t head_unload_time;  /**< Head unload time (ms) */
    uint16_t step_rate;         /**< Step rate (µs) */
    uint16_t spinup_time;       /**< Spinup time (ms) */
    uint16_t spindown_time;     /**< Spindown time (ms) */
    uint8_t  spindown_offset;   /**< Spindown offset */
    uint8_t  select_delay;      /**< Select delay */
    uint8_t  rps;               /**< Rotations per second */
    uint8_t  max_tracks;        /**< Maximum tracks */
    uint16_t timeout;           /**< Operation timeout */
    uint8_t  interleave;        /**< Maximum non-interleaved sectors */
    const char* name;           /**< Drive name */
} uft_fdc_drive_params_t;

/**
 * @brief Floppy format parameters
 * Based on Linux floppy_struct
 */
typedef struct {
    uint32_t size;              /**< Total size in sectors */
    uint8_t  sect_per_track;    /**< Sectors per track */
    uint8_t  heads;             /**< Number of heads */
    uint8_t  tracks;            /**< Number of tracks */
    uint8_t  stretch;           /**< Double-step flag */
    uint8_t  gap1;              /**< Gap 1 size */
    uint8_t  data_rate;         /**< Data rate + 0x40 for perpendicular */
    uint8_t  spec1;             /**< Stepping rate, head unload */
    uint8_t  gap2;              /**< Format gap (gap 2) */
    const char* name;           /**< Format name */
} uft_fdc_format_t;

/*============================================================================
 * Standard Drive Parameters
 *============================================================================*/

/**
 * @brief Standard floppy drive types
 */
extern const uft_fdc_drive_params_t uft_fdc_drive_types[];

/**
 * @brief Number of standard drive types
 */
#define UFT_FDC_NUM_DRIVE_TYPES 7

/*============================================================================
 * Standard Format Definitions
 *============================================================================*/

/**
 * @brief Standard floppy formats
 */
extern const uft_fdc_format_t uft_fdc_formats[];

/**
 * @brief Number of standard formats
 */
#define UFT_FDC_NUM_FORMATS     32

/*============================================================================
 * Timing Parameters
 *============================================================================*/

/**
 * @brief Calculate SPECIFY command byte 1
 * @param step_rate Step rate in ms (1-16)
 * @param head_unload Head unload time in ms (16-480, step 16)
 * @return Spec1 byte
 */
static inline uint8_t uft_fdc_spec1(uint8_t step_rate, uint16_t head_unload) {
    uint8_t srt = (16 - step_rate) & 0x0F;
    uint8_t hut = (head_unload / 16) & 0x0F;
    return (srt << 4) | hut;
}

/**
 * @brief Calculate SPECIFY command byte 2
 * @param head_load Head load time in ms (2-254, step 2)
 * @param dma Use DMA mode
 * @return Spec2 byte
 */
static inline uint8_t uft_fdc_spec2(uint16_t head_load, bool dma) {
    uint8_t hlt = (head_load / 2) & 0x7F;
    return (hlt << 1) | (dma ? 0 : 1);
}

/*============================================================================
 * Gap Length Tables
 *============================================================================*/

/**
 * @brief Gap length entry
 */
typedef struct {
    uint8_t  ssize;         /**< Sector size code */
    uint8_t  max_sect;      /**< Maximum sectors */
    uint8_t  gap3_rw;       /**< Gap 3 for read/write */
    uint8_t  gap3_fmt;      /**< Gap 3 for format */
} uft_fdc_gap_entry_t;

/**
 * @brief 8" FM gap lengths
 */
extern const uft_fdc_gap_entry_t uft_fdc_gaps_8in_fm[];

/**
 * @brief 8" MFM gap lengths
 */
extern const uft_fdc_gap_entry_t uft_fdc_gaps_8in_mfm[];

/**
 * @brief 5.25" FM gap lengths
 */
extern const uft_fdc_gap_entry_t uft_fdc_gaps_5in_fm[];

/**
 * @brief 5.25" MFM gap lengths
 */
extern const uft_fdc_gap_entry_t uft_fdc_gaps_5in_mfm[];

/**
 * @brief Get recommended gap lengths
 * @param mfm MFM encoding (vs FM)
 * @param inch8 8" media (vs 5.25"/3.5")
 * @param ssize Sector size code
 * @param nsect Number of sectors
 * @param gap_rw Output gap for read/write
 * @param gap_fmt Output gap for format
 * @return true if valid combination found
 */
bool uft_fdc_get_gaps(bool mfm, bool inch8, uint8_t ssize, uint8_t nsect,
                      uint8_t* gap_rw, uint8_t* gap_fmt);

/*============================================================================
 * Result/Status Parsing
 *============================================================================*/

/**
 * @brief FDC result structure
 */
typedef struct {
    uint8_t  st0;           /**< Status register 0 */
    uint8_t  st1;           /**< Status register 1 */
    uint8_t  st2;           /**< Status register 2 */
    uint8_t  cylinder;      /**< Cylinder from ID */
    uint8_t  head;          /**< Head from ID */
    uint8_t  sector;        /**< Sector from ID */
    uint8_t  size;          /**< Size code from ID */
} uft_fdc_result_t;

/**
 * @brief Check if result indicates success
 */
static inline bool uft_fdc_result_ok(const uft_fdc_result_t* r) {
    return (r->st0 & UFT_ST0_IC_MASK) == UFT_ST0_IC_NORMAL;
}

/**
 * @brief Get error description from result
 */
const char* uft_fdc_result_error(const uft_fdc_result_t* r);

/*============================================================================
 * Track Capacity Calculation
 *============================================================================*/

/**
 * @brief Calculate raw track capacity in bytes
 * @param rate Data rate (250, 300, 500, 1000 kbps)
 * @param rpm Rotation speed (300 or 360 RPM)
 * @return Bytes per track
 */
static inline uint32_t uft_fdc_track_capacity(uint16_t rate, uint16_t rpm) {
    /* bits/second / (rpm/60) / 8 = bytes/track */
    return (uint32_t)rate * 1000 * 60 / rpm / 8;
}

/**
 * @brief Calculate formatted track capacity
 * @param nsect Number of sectors
 * @param ssize Sector size in bytes
 * @param mfm MFM encoding
 * @return Bytes required for formatted track
 */
uint32_t uft_fdc_formatted_size(uint8_t nsect, uint16_t ssize, bool mfm);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FDC_H */
