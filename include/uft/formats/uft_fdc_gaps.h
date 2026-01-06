/**
 * @file uft_fdc_gaps.h
 * @brief FDC Gap Tables and Format Parameters
 * 
 * EXT-006: Comprehensive FDC Gap tables for all common formats
 * NEC µPD765/Intel 8272 compatible
 */

#ifndef UFT_FDC_GAPS_H
#define UFT_FDC_GAPS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * FDC Register Definitions (NEC µPD765)
 *===========================================================================*/

/* Main Status Register (MSR) */
#define FDC_MSR_RQM         0x80    /**< Request for Master */
#define FDC_MSR_DIO         0x40    /**< Data Input/Output */
#define FDC_MSR_EXM         0x20    /**< Execution Mode */
#define FDC_MSR_CB          0x10    /**< FDC Busy */
#define FDC_MSR_D3B         0x08    /**< Drive 3 Busy */
#define FDC_MSR_D2B         0x04    /**< Drive 2 Busy */
#define FDC_MSR_D1B         0x02    /**< Drive 1 Busy */
#define FDC_MSR_D0B         0x01    /**< Drive 0 Busy */

/* Status Register 0 (ST0) */
#define FDC_ST0_IC_MASK     0xC0    /**< Interrupt Code */
#define FDC_ST0_IC_NORMAL   0x00    /**< Normal termination */
#define FDC_ST0_IC_ABNORMAL 0x40    /**< Abnormal termination */
#define FDC_ST0_IC_INVALID  0x80    /**< Invalid command */
#define FDC_ST0_IC_READY    0xC0    /**< Ready changed */
#define FDC_ST0_SE          0x20    /**< Seek End */
#define FDC_ST0_EC          0x10    /**< Equipment Check */
#define FDC_ST0_NR          0x08    /**< Not Ready */
#define FDC_ST0_HD          0x04    /**< Head Address */
#define FDC_ST0_US_MASK     0x03    /**< Unit Select */

/* Status Register 1 (ST1) */
#define FDC_ST1_EN          0x80    /**< End of Cylinder */
#define FDC_ST1_DE          0x20    /**< Data Error (CRC) */
#define FDC_ST1_OR          0x10    /**< Overrun */
#define FDC_ST1_ND          0x04    /**< No Data */
#define FDC_ST1_NW          0x02    /**< Not Writable */
#define FDC_ST1_MA          0x01    /**< Missing Address Mark */

/* Status Register 2 (ST2) */
#define FDC_ST2_CM          0x40    /**< Control Mark */
#define FDC_ST2_DD          0x20    /**< Data Error in Data */
#define FDC_ST2_WC          0x10    /**< Wrong Cylinder */
#define FDC_ST2_SH          0x08    /**< Scan Equal Hit */
#define FDC_ST2_SN          0x04    /**< Scan Not Satisfied */
#define FDC_ST2_BC          0x02    /**< Bad Cylinder */
#define FDC_ST2_MD          0x01    /**< Missing Data AM */

/* Status Register 3 (ST3) */
#define FDC_ST3_WP          0x40    /**< Write Protected */
#define FDC_ST3_RY          0x20    /**< Ready */
#define FDC_ST3_T0          0x10    /**< Track 0 */
#define FDC_ST3_TS          0x08    /**< Two Side */
#define FDC_ST3_HD          0x04    /**< Head Address */
#define FDC_ST3_US_MASK     0x03    /**< Unit Select */

/* FDC Commands */
#define FDC_CMD_READ_DATA       0x06
#define FDC_CMD_READ_DEL_DATA   0x0C
#define FDC_CMD_WRITE_DATA      0x05
#define FDC_CMD_WRITE_DEL_DATA  0x09
#define FDC_CMD_READ_TRACK      0x02
#define FDC_CMD_READ_ID         0x0A
#define FDC_CMD_FORMAT_TRACK    0x0D
#define FDC_CMD_SCAN_EQUAL      0x11
#define FDC_CMD_SCAN_LOW_EQ     0x19
#define FDC_CMD_SCAN_HIGH_EQ    0x1D
#define FDC_CMD_RECALIBRATE     0x07
#define FDC_CMD_SENSE_INT       0x08
#define FDC_CMD_SPECIFY         0x03
#define FDC_CMD_SENSE_DRIVE     0x04
#define FDC_CMD_SEEK            0x0F

/* Command modifiers */
#define FDC_CMD_MT              0x80    /**< Multi-Track */
#define FDC_CMD_MFM             0x40    /**< MFM mode (vs FM) */
#define FDC_CMD_SK              0x20    /**< Skip deleted */

/*===========================================================================
 * Data Rate Constants
 *===========================================================================*/

typedef enum {
    UFT_FDC_RATE_500K = 0,      /**< 500 kbps (HD) */
    UFT_FDC_RATE_300K = 1,      /**< 300 kbps (DD 5.25") */
    UFT_FDC_RATE_250K = 2,      /**< 250 kbps (DD 3.5") */
    UFT_FDC_RATE_1M   = 3       /**< 1 Mbps (ED) */
} uft_fdc_rate_t;

/*===========================================================================
 * Gap Definitions
 *===========================================================================*/

/**
 * @brief Gap sizes for various formats
 * 
 * Gap structure:
 * - GAP4a: Post-Index Gap (after index pulse)
 * - GAP1:  Pre-ID Gap (before sector ID)
 * - GAP2:  Post-ID Gap (after ID, before data)
 * - GAP3:  Post-Data Gap (between sectors)
 * - GAP4b: Pre-Index Gap (before index pulse)
 */
typedef struct {
    uint8_t gap4a;              /**< Post-index gap (sync) */
    uint8_t gap1;               /**< Pre-ID gap */
    uint8_t gap2;               /**< Post-ID gap (22 for MFM) */
    uint8_t gap3_rw;            /**< Post-data gap (read/write) */
    uint8_t gap3_fmt;           /**< Post-data gap (format) */
    uint16_t gap4b;             /**< Pre-index gap (fill) */
} uft_fdc_gaps_t;

/**
 * @brief Complete format specification
 */
typedef struct {
    const char *name;           /**< Format name */
    
    /* Physical parameters */
    uint8_t tracks;             /**< Tracks per side */
    uint8_t sides;              /**< Number of sides */
    uint8_t sectors;            /**< Sectors per track */
    uint16_t sector_size;       /**< Bytes per sector */
    uint8_t size_code;          /**< FDC size code (0-7) */
    
    /* Timing */
    uft_fdc_rate_t data_rate;   /**< Data rate */
    uint16_t rpm;               /**< Rotation speed */
    bool mfm;                   /**< MFM (true) or FM (false) */
    
    /* Gaps */
    uft_fdc_gaps_t gaps;
    
    /* Track capacity */
    uint32_t track_bytes;       /**< Total bytes per track */
    uint32_t raw_bits;          /**< Raw bits per track */
} uft_fdc_format_t;

/*===========================================================================
 * Standard Format Definitions
 *===========================================================================*/

/* PC/DOS Formats */
static const uft_fdc_format_t UFT_FDC_PC_360K = {
    .name = "PC 360K (5.25\" DD)",
    .tracks = 40, .sides = 2, .sectors = 9, .sector_size = 512, .size_code = 2,
    .data_rate = UFT_FDC_RATE_300K, .rpm = 300, .mfm = true,
    .gaps = { .gap4a = 80, .gap1 = 50, .gap2 = 22, .gap3_rw = 80, .gap3_fmt = 80, .gap4b = 664 },
    .track_bytes = 6250, .raw_bits = 100000
};

static const uft_fdc_format_t UFT_FDC_PC_720K = {
    .name = "PC 720K (3.5\" DD)",
    .tracks = 80, .sides = 2, .sectors = 9, .sector_size = 512, .size_code = 2,
    .data_rate = UFT_FDC_RATE_250K, .rpm = 300, .mfm = true,
    .gaps = { .gap4a = 80, .gap1 = 50, .gap2 = 22, .gap3_rw = 80, .gap3_fmt = 80, .gap4b = 664 },
    .track_bytes = 6250, .raw_bits = 100000
};

static const uft_fdc_format_t UFT_FDC_PC_1200K = {
    .name = "PC 1.2M (5.25\" HD)",
    .tracks = 80, .sides = 2, .sectors = 15, .sector_size = 512, .size_code = 2,
    .data_rate = UFT_FDC_RATE_500K, .rpm = 360, .mfm = true,
    .gaps = { .gap4a = 80, .gap1 = 50, .gap2 = 22, .gap3_rw = 54, .gap3_fmt = 84, .gap4b = 400 },
    .track_bytes = 10416, .raw_bits = 166666
};

static const uft_fdc_format_t UFT_FDC_PC_1440K = {
    .name = "PC 1.44M (3.5\" HD)",
    .tracks = 80, .sides = 2, .sectors = 18, .sector_size = 512, .size_code = 2,
    .data_rate = UFT_FDC_RATE_500K, .rpm = 300, .mfm = true,
    .gaps = { .gap4a = 80, .gap1 = 50, .gap2 = 22, .gap3_rw = 108, .gap3_fmt = 84, .gap4b = 400 },
    .track_bytes = 12500, .raw_bits = 200000
};

static const uft_fdc_format_t UFT_FDC_PC_2880K = {
    .name = "PC 2.88M (3.5\" ED)",
    .tracks = 80, .sides = 2, .sectors = 36, .sector_size = 512, .size_code = 2,
    .data_rate = UFT_FDC_RATE_1M, .rpm = 300, .mfm = true,
    .gaps = { .gap4a = 80, .gap1 = 50, .gap2 = 22, .gap3_rw = 54, .gap3_fmt = 84, .gap4b = 400 },
    .track_bytes = 25000, .raw_bits = 400000
};

/* Atari ST Formats */
static const uft_fdc_format_t UFT_FDC_ATARI_SS = {
    .name = "Atari ST SS (360K)",
    .tracks = 80, .sides = 1, .sectors = 9, .sector_size = 512, .size_code = 2,
    .data_rate = UFT_FDC_RATE_250K, .rpm = 300, .mfm = true,
    .gaps = { .gap4a = 60, .gap1 = 60, .gap2 = 22, .gap3_rw = 40, .gap3_fmt = 40, .gap4b = 664 },
    .track_bytes = 6250, .raw_bits = 100000
};

static const uft_fdc_format_t UFT_FDC_ATARI_DS = {
    .name = "Atari ST DS (720K)",
    .tracks = 80, .sides = 2, .sectors = 9, .sector_size = 512, .size_code = 2,
    .data_rate = UFT_FDC_RATE_250K, .rpm = 300, .mfm = true,
    .gaps = { .gap4a = 60, .gap1 = 60, .gap2 = 22, .gap3_rw = 40, .gap3_fmt = 40, .gap4b = 664 },
    .track_bytes = 6250, .raw_bits = 100000
};

static const uft_fdc_format_t UFT_FDC_ATARI_HD = {
    .name = "Atari ST HD",
    .tracks = 80, .sides = 2, .sectors = 18, .sector_size = 512, .size_code = 2,
    .data_rate = UFT_FDC_RATE_500K, .rpm = 300, .mfm = true,
    .gaps = { .gap4a = 60, .gap1 = 60, .gap2 = 22, .gap3_rw = 40, .gap3_fmt = 84, .gap4b = 400 },
    .track_bytes = 12500, .raw_bits = 200000
};

/* Amstrad CPC Formats */
static const uft_fdc_format_t UFT_FDC_AMSTRAD_DATA = {
    .name = "Amstrad CPC Data",
    .tracks = 40, .sides = 1, .sectors = 9, .sector_size = 512, .size_code = 2,
    .data_rate = UFT_FDC_RATE_250K, .rpm = 300, .mfm = true,
    .gaps = { .gap4a = 82, .gap1 = 50, .gap2 = 22, .gap3_rw = 82, .gap3_fmt = 82, .gap4b = 400 },
    .track_bytes = 6250, .raw_bits = 100000
};

static const uft_fdc_format_t UFT_FDC_AMSTRAD_SYS = {
    .name = "Amstrad CPC System",
    .tracks = 40, .sides = 1, .sectors = 9, .sector_size = 512, .size_code = 2,
    .data_rate = UFT_FDC_RATE_250K, .rpm = 300, .mfm = true,
    .gaps = { .gap4a = 82, .gap1 = 50, .gap2 = 22, .gap3_rw = 82, .gap3_fmt = 82, .gap4b = 400 },
    .track_bytes = 6250, .raw_bits = 100000
};

/* BBC Micro Formats */
static const uft_fdc_format_t UFT_FDC_BBC_DFS = {
    .name = "BBC DFS (200K)",
    .tracks = 80, .sides = 1, .sectors = 10, .sector_size = 256, .size_code = 1,
    .data_rate = UFT_FDC_RATE_250K, .rpm = 300, .mfm = false,  /* FM! */
    .gaps = { .gap4a = 40, .gap1 = 26, .gap2 = 11, .gap3_rw = 21, .gap3_fmt = 21, .gap4b = 300 },
    .track_bytes = 3125, .raw_bits = 50000
};

static const uft_fdc_format_t UFT_FDC_BBC_ADFS = {
    .name = "BBC ADFS (640K)",
    .tracks = 80, .sides = 2, .sectors = 16, .sector_size = 256, .size_code = 1,
    .data_rate = UFT_FDC_RATE_250K, .rpm = 300, .mfm = true,
    .gaps = { .gap4a = 80, .gap1 = 50, .gap2 = 22, .gap3_rw = 57, .gap3_fmt = 57, .gap4b = 400 },
    .track_bytes = 6250, .raw_bits = 100000
};

/* FM Formats (single density) */
static const uft_fdc_format_t UFT_FDC_FM_SD = {
    .name = "FM Single Density",
    .tracks = 77, .sides = 1, .sectors = 26, .sector_size = 128, .size_code = 0,
    .data_rate = UFT_FDC_RATE_250K, .rpm = 360, .mfm = false,
    .gaps = { .gap4a = 40, .gap1 = 26, .gap2 = 11, .gap3_rw = 27, .gap3_fmt = 27, .gap4b = 247 },
    .track_bytes = 3125, .raw_bits = 50000
};

/* NEC PC-98 Formats */
static const uft_fdc_format_t UFT_FDC_PC98_2DD = {
    .name = "PC-98 2DD (640K)",
    .tracks = 80, .sides = 2, .sectors = 8, .sector_size = 512, .size_code = 2,
    .data_rate = UFT_FDC_RATE_250K, .rpm = 300, .mfm = true,
    .gaps = { .gap4a = 80, .gap1 = 50, .gap2 = 22, .gap3_rw = 116, .gap3_fmt = 116, .gap4b = 600 },
    .track_bytes = 6250, .raw_bits = 100000
};

static const uft_fdc_format_t UFT_FDC_PC98_2HD = {
    .name = "PC-98 2HD (1.23M)",
    .tracks = 77, .sides = 2, .sectors = 8, .sector_size = 1024, .size_code = 3,
    .data_rate = UFT_FDC_RATE_500K, .rpm = 360, .mfm = true,
    .gaps = { .gap4a = 80, .gap1 = 50, .gap2 = 22, .gap3_rw = 116, .gap3_fmt = 116, .gap4b = 600 },
    .track_bytes = 10416, .raw_bits = 166666
};

/* MSX Formats */
static const uft_fdc_format_t UFT_FDC_MSX_1DD = {
    .name = "MSX 1DD (360K)",
    .tracks = 80, .sides = 1, .sectors = 9, .sector_size = 512, .size_code = 2,
    .data_rate = UFT_FDC_RATE_250K, .rpm = 300, .mfm = true,
    .gaps = { .gap4a = 80, .gap1 = 50, .gap2 = 22, .gap3_rw = 80, .gap3_fmt = 80, .gap4b = 664 },
    .track_bytes = 6250, .raw_bits = 100000
};

static const uft_fdc_format_t UFT_FDC_MSX_2DD = {
    .name = "MSX 2DD (720K)",
    .tracks = 80, .sides = 2, .sectors = 9, .sector_size = 512, .size_code = 2,
    .data_rate = UFT_FDC_RATE_250K, .rpm = 300, .mfm = true,
    .gaps = { .gap4a = 80, .gap1 = 50, .gap2 = 22, .gap3_rw = 80, .gap3_fmt = 80, .gap4b = 664 },
    .track_bytes = 6250, .raw_bits = 100000
};

/*===========================================================================
 * Format Table
 *===========================================================================*/

static const uft_fdc_format_t *UFT_FDC_FORMATS[] = {
    &UFT_FDC_PC_360K,
    &UFT_FDC_PC_720K,
    &UFT_FDC_PC_1200K,
    &UFT_FDC_PC_1440K,
    &UFT_FDC_PC_2880K,
    &UFT_FDC_ATARI_SS,
    &UFT_FDC_ATARI_DS,
    &UFT_FDC_ATARI_HD,
    &UFT_FDC_AMSTRAD_DATA,
    &UFT_FDC_AMSTRAD_SYS,
    &UFT_FDC_BBC_DFS,
    &UFT_FDC_BBC_ADFS,
    &UFT_FDC_FM_SD,
    &UFT_FDC_PC98_2DD,
    &UFT_FDC_PC98_2HD,
    &UFT_FDC_MSX_1DD,
    &UFT_FDC_MSX_2DD,
    NULL
};

#define UFT_FDC_FORMAT_COUNT  17

/*===========================================================================
 * Function Prototypes
 *===========================================================================*/

/**
 * @brief Get format by name
 */
const uft_fdc_format_t *uft_fdc_get_format(const char *name);

/**
 * @brief Get format by parameters
 */
const uft_fdc_format_t *uft_fdc_detect_format(uint8_t tracks, uint8_t sides,
                                              uint8_t sectors, uint16_t sector_size);

/**
 * @brief Calculate track layout
 * 
 * @param fmt Format specification
 * @param sector_offsets Output array for sector byte offsets
 * @param max_sectors Maximum sectors to return
 * @return Number of sectors
 */
int uft_fdc_calc_track_layout(const uft_fdc_format_t *fmt,
                              uint32_t *sector_offsets, int max_sectors);

/**
 * @brief Calculate optimal Gap3 for custom format
 * 
 * @param track_capacity Track capacity in bytes
 * @param sectors Sectors per track
 * @param sector_size Bytes per sector
 * @param mfm MFM (true) or FM (false)
 * @return Calculated Gap3 size
 */
uint8_t uft_fdc_calc_gap3(uint32_t track_capacity, uint8_t sectors,
                          uint16_t sector_size, bool mfm);

/**
 * @brief Get size code from sector size
 */
uint8_t uft_fdc_size_code(uint16_t sector_size);

/**
 * @brief Get sector size from size code
 */
uint16_t uft_fdc_sector_size(uint8_t size_code);

/**
 * @brief List all supported formats
 */
void uft_fdc_list_formats(void);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FDC_GAPS_H */
