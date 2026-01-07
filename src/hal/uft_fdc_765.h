#ifndef UFT_FDC_765_H
#define UFT_FDC_765_H
/**
 * @file uft_fdc_765.h
 * @brief NEC 765 / Intel 8272 FDC Register Definitions
 *
 * Provides portable FDC register definitions and backend API interface
 * for UFT Hardware Abstraction Layer.
 */

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * FDC I/O Ports (PC/AT)
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_FDC_PORT_DOR        0x3F2   /* Digital Output Register */
#define UFT_FDC_PORT_MSR        0x3F4   /* Main Status Register */
#define UFT_FDC_PORT_DATA       0x3F5   /* Data Register (FIFO) */
#define UFT_FDC_PORT_DIR        0x3F7   /* Digital Input Register */
#define UFT_FDC_PORT_CCR        0x3F7   /* Configuration Control Register */

/* ═══════════════════════════════════════════════════════════════════════════
 * FDC Commands
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum uft_fdc_cmd {
    UFT_FDC_CMD_SPECIFY         = 0x03,
    UFT_FDC_CMD_SENSE_DRIVE     = 0x04,
    UFT_FDC_CMD_RECALIBRATE     = 0x07,
    UFT_FDC_CMD_SENSE_INT       = 0x08,
    UFT_FDC_CMD_SEEK            = 0x0F,
    UFT_FDC_CMD_VERSION         = 0x10,
    UFT_FDC_CMD_CONFIGURE       = 0x13,
    UFT_FDC_CMD_LOCK            = 0x14,
    UFT_FDC_CMD_READ_ID         = 0x4A,  /* + MFM */
    UFT_FDC_CMD_FORMAT          = 0x4D,  /* + MFM */
    UFT_FDC_CMD_READ            = 0x46,  /* + MFM */
    UFT_FDC_CMD_WRITE           = 0x45,  /* + MFM */
    UFT_FDC_CMD_READ_MT         = 0xE6,  /* + MFM + MT + SK */
    UFT_FDC_CMD_WRITE_MT        = 0xC5,  /* + MFM + MT */
} uft_fdc_cmd_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Status Register 0 (ST0) Bits
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_FDC_ST0_IC_MASK     0xC0    /* Interrupt Code */
#define UFT_FDC_ST0_IC_NORMAL   0x00    /* Normal termination */
#define UFT_FDC_ST0_IC_ABNORMAL 0x40    /* Abnormal termination */
#define UFT_FDC_ST0_IC_INVALID  0x80    /* Invalid command */
#define UFT_FDC_ST0_IC_READY    0xC0    /* Drive not ready */
#define UFT_FDC_ST0_SE          0x20    /* Seek End */
#define UFT_FDC_ST0_EC          0x10    /* Equipment Check */
#define UFT_FDC_ST0_NR          0x08    /* Not Ready */
#define UFT_FDC_ST0_HD          0x04    /* Head Address */
#define UFT_FDC_ST0_DS_MASK     0x03    /* Drive Select */

/* ═══════════════════════════════════════════════════════════════════════════
 * Status Register 1 (ST1) Bits
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_FDC_ST1_EN          0x80    /* End of Cylinder */
#define UFT_FDC_ST1_DE          0x20    /* Data Error (CRC) */
#define UFT_FDC_ST1_OR          0x10    /* Overrun */
#define UFT_FDC_ST1_ND          0x04    /* No Data */
#define UFT_FDC_ST1_NW          0x02    /* Not Writable */
#define UFT_FDC_ST1_MA          0x01    /* Missing Address Mark */

/* ═══════════════════════════════════════════════════════════════════════════
 * Status Register 2 (ST2) Bits
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_FDC_ST2_CM          0x40    /* Control Mark */
#define UFT_FDC_ST2_DD          0x20    /* Data Error in Data Field */
#define UFT_FDC_ST2_WC          0x10    /* Wrong Cylinder */
#define UFT_FDC_ST2_SH          0x08    /* Scan Equal Hit */
#define UFT_FDC_ST2_SN          0x04    /* Scan Not Satisfied */
#define UFT_FDC_ST2_BC          0x02    /* Bad Cylinder */
#define UFT_FDC_ST2_MD          0x01    /* Missing Data Address Mark */

/* ═══════════════════════════════════════════════════════════════════════════
 * Main Status Register (MSR) Bits
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_FDC_MSR_RQM         0x80    /* Request for Master */
#define UFT_FDC_MSR_DIO         0x40    /* Data Input/Output (1=read, 0=write) */
#define UFT_FDC_MSR_NDMA        0x20    /* Non-DMA mode */
#define UFT_FDC_MSR_CB          0x10    /* Controller Busy */
#define UFT_FDC_MSR_D3B         0x08    /* Drive 3 Busy */
#define UFT_FDC_MSR_D2B         0x04    /* Drive 2 Busy */
#define UFT_FDC_MSR_D1B         0x02    /* Drive 1 Busy */
#define UFT_FDC_MSR_D0B         0x01    /* Drive 0 Busy */

/* ═══════════════════════════════════════════════════════════════════════════
 * Data Types
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Standard 7-byte result from read/write commands
 */
typedef struct uft_fdc_result7 {
    uint8_t st0;    /**< Status Register 0 */
    uint8_t st1;    /**< Status Register 1 */
    uint8_t st2;    /**< Status Register 2 */
    uint8_t c;      /**< Cylinder */
    uint8_t h;      /**< Head */
    uint8_t r;      /**< Record (Sector) */
    uint8_t n;      /**< Number (Sector Size Code) */
} uft_fdc_result7_t;

/**
 * CHS address with sector size code
 */
typedef struct uft_fdc_chs {
    uint8_t c;      /**< Cylinder */
    uint8_t h;      /**< Head */
    uint8_t s;      /**< Sector (1-based) */
    uint8_t n;      /**< Sector size: 0=128, 1=256, 2=512, 3=1024 */
} uft_fdc_chs_t;

/**
 * Read/Write command parameters
 */
typedef struct uft_fdc_rw_params {
    uft_fdc_chs_t chs;
    uint8_t eot;    /**< End of Track sector number */
    uint8_t gap;    /**< Gap length */
    uint8_t dtl;    /**< Data length (when N=0) */
} uft_fdc_rw_params_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Backend I/O Interface (for portability)
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * FDC I/O backend interface
 * Allows portable access to FDC without direct port I/O
 */
typedef struct uft_fdc_io {
    uint8_t (*in8)(void *ctx, uint16_t port);
    void    (*out8)(void *ctx, uint16_t port, uint8_t value);
    void    (*udelay)(void *ctx, uint32_t usec);
    void    (*sleep_ms)(void *ctx, uint32_t ms);
    void    *ctx;
} uft_fdc_io_t;

/**
 * Transfer direction
 */
typedef enum uft_fdc_xfer_dir {
    UFT_FDC_XFER_READ  = 0,
    UFT_FDC_XFER_WRITE = 1,
} uft_fdc_xfer_dir_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Utility Macros
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Convert sector size code to bytes: 128 << n */
#define UFT_FDC_SECTOR_SIZE(n)  (128u << (n))

/** Check if ST0 indicates success */
#define UFT_FDC_ST0_OK(st0)     (((st0) & UFT_FDC_ST0_IC_MASK) == UFT_FDC_ST0_IC_NORMAL)

#ifdef __cplusplus
}
#endif

#endif /* UFT_FDC_765_H */
