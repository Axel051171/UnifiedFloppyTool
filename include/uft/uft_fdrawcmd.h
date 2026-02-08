/**
 * @file uft_fdrawcmd.h
 * @brief fdrawcmd.sys Driver Interface Definitions
 * 
 * 
 * Low-level Windows floppy filter driver for direct FDC access.
 * Allows raw sector operations, custom formats, and timing control.
 */

#ifndef UFT_FDRAWCMD_H
#define UFT_FDRAWCMD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * VERSION AND IOCTL BASE
 *============================================================================*/

#define FDRAWCMD_VERSION    0x0100010b  // v1.0.1.11

/* IOCTL code calculation (for non-Windows platforms, use values directly) */
#ifdef _WIN32
#include <winioctl.h>
#define FD_CTL_CODE(i,m) CTL_CODE(FILE_DEVICE_UNKNOWN, i, m, FILE_READ_DATA|FILE_WRITE_DATA)
#else
/* Pre-calculated IOCTL values for reference */
#define METHOD_BUFFERED     0
#define METHOD_IN_DIRECT    1
#define METHOD_OUT_DIRECT   2
#endif

/*============================================================================
 * FDC COMMAND IOCTLS
 *============================================================================*/

/* Raw FDC commands */
#define IOCTL_FDCMD_READ_TRACK          0x0022e00a
#define IOCTL_FDCMD_SPECIFY             0x0022e00c
#define IOCTL_FDCMD_SENSE_DRIVE_STATUS  0x0022e010
#define IOCTL_FDCMD_WRITE_DATA          0x0022e015
#define IOCTL_FDCMD_READ_DATA           0x0022e01a
#define IOCTL_FDCMD_RECALIBRATE         0x0022e01c
#define IOCTL_FDCMD_SENSE_INT_STATUS    0x0022e020
#define IOCTL_FDCMD_WRITE_DELETED_DATA  0x0022e025
#define IOCTL_FDCMD_READ_ID             0x0022e028
#define IOCTL_FDCMD_READ_DELETED_DATA   0x0022e032
#define IOCTL_FDCMD_FORMAT_TRACK        0x0022e034
#define IOCTL_FDCMD_DUMPREG             0x0022e038
#define IOCTL_FDCMD_SEEK                0x0022e03c
#define IOCTL_FDCMD_VERSION             0x0022e040
#define IOCTL_FDCMD_SCAN_EQUAL          0x0022e045  /* Not implemented */
#define IOCTL_FDCMD_PERPENDICULAR_MODE  0x0022e048
#define IOCTL_FDCMD_CONFIGURE           0x0022e04c
#define IOCTL_FDCMD_LOCK                0x0022e050
#define IOCTL_FDCMD_VERIFY              0x0022e058
#define IOCTL_FDCMD_POWERDOWN_MODE      0x0022e05c  /* Not implemented */
#define IOCTL_FDCMD_PART_ID             0x0022e060
#define IOCTL_FDCMD_SCAN_LOW_OR_EQUAL   0x0022e065  /* Not implemented */
#define IOCTL_FDCMD_SCAN_HIGH_OR_EQUAL  0x0022e075  /* Not implemented */
#define IOCTL_FDCMD_RELATIVE_SEEK       0x0022e23c
#define IOCTL_FDCMD_FORMAT_AND_WRITE    0x0022e3bc

/*============================================================================
 * EXTENDED OPERATIONS
 *============================================================================*/

#define IOCTL_FD_SCAN_TRACK             0x0022e400
#define IOCTL_FD_GET_RESULT             0x0022e404
#define IOCTL_FD_RESET                  0x0022e408
#define IOCTL_FD_SET_MOTOR_TIMEOUT      0x0022e40c
#define IOCTL_FD_SET_DATA_RATE          0x0022e410
#define IOCTL_FD_GET_FDC_INFO           0x0022e414
#define IOCTL_FD_GET_REMAIN_COUNT       0x0022e418
#define IOCTL_FD_SET_DISK_CHECK         0x0022e420
#define IOCTL_FD_SET_SHORT_WRITE        0x0022e424
#define IOCTL_FD_SET_SECTOR_OFFSET      0x0022e428
#define IOCTL_FD_SET_HEAD_SETTLE_TIME   0x0022e42c
#define IOCTL_FD_LOCK_FDC               0x0022e440  /* Obsolete */
#define IOCTL_FD_UNLOCK_FDC             0x0022e444  /* Obsolete */
#define IOCTL_FD_MOTOR_ON               0x0022e448
#define IOCTL_FD_MOTOR_OFF              0x0022e44c
#define IOCTL_FD_WAIT_INDEX             0x0022e450
#define IOCTL_FD_TIMED_SCAN_TRACK       0x0022e454
#define IOCTL_FD_RAW_READ_TRACK         0x0022e45a
#define IOCTL_FD_CHECK_DISK             0x0022e45c
#define IOCTL_FD_GET_TRACK_TIME         0x0022e460
#define IOCTL_FDRAWCMD_GET_VERSION      0x0022e220

/*============================================================================
 * COMMAND FLAGS
 *============================================================================*/

#define FD_OPTION_MT        0x80    /* Multi-track */
#define FD_OPTION_MFM       0x40    /* MFM encoding */
#define FD_OPTION_SK        0x20    /* Skip deleted sectors */
#define FD_OPTION_DIR       0x40    /* Relative seek direction */
#define FD_OPTION_EC        0x01    /* Verify enable count */
#define FD_OPTION_FM        0x00    /* FM encoding */
#define FD_ENCODING_MASK    FD_OPTION_MFM

/*============================================================================
 * DATA RATES
 *============================================================================*/

#define FD_RATE_MASK        3
#define FD_RATE_500K        0       /* HD 3.5" / HD 5.25" */
#define FD_RATE_300K        1       /* DD in HD drive */
#define FD_RATE_250K        2       /* DD 3.5" / DD 5.25" */
#define FD_RATE_1M          3       /* ED 3.5" */

/*============================================================================
 * FDC CONTROLLER TYPES
 *============================================================================*/

typedef enum {
    FDC_TYPE_UNKNOWN    = 0,
    FDC_TYPE_UNKNOWN2   = 1,
    FDC_TYPE_NORMAL     = 2,
    FDC_TYPE_ENHANCED   = 3,
    FDC_TYPE_82077      = 4,
    FDC_TYPE_82077AA    = 5,
    FDC_TYPE_82078_44   = 6,
    FDC_TYPE_82078_64   = 7,
    FDC_TYPE_NATIONAL   = 8
} fdc_controller_type_t;

/*============================================================================
 * SUPPORTED DATA RATE FLAGS
 *============================================================================*/

#define FDC_SPEED_250K      0x01
#define FDC_SPEED_300K      0x02
#define FDC_SPEED_500K      0x04
#define FDC_SPEED_1M        0x08
#define FDC_SPEED_2M        0x10

/*============================================================================
 * DATA STRUCTURES
 *============================================================================*/

#pragma pack(push, 1)

/**
 * Sector ID header (CHRN)
 */
typedef struct {
    uint8_t cyl;        /* Cylinder number */
    uint8_t head;       /* Head number (usually matches physical) */
    uint8_t sector;     /* Sector number */
    uint8_t size;       /* Size code: 0=128, 1=256, 2=512, 3=1024, ... */
} fd_id_header_t;

/**
 * Seek parameters
 */
typedef struct {
    uint8_t cyl;        /* Target cylinder */
    uint8_t head;       /* Physical head to select */
} fd_seek_params_t;

/**
 * Relative seek parameters
 */
typedef struct {
    uint8_t flags;      /* DIR flag for direction */
    uint8_t head;       /* Physical head */
    uint8_t offset;     /* Number of cylinders to move */
} fd_relative_seek_params_t;

/**
 * Read/Write command parameters
 */
typedef struct {
    uint8_t flags;      /* MT MFM SK flags */
    uint8_t phead;      /* Physical head */
    uint8_t cyl;        /* Cylinder from ID */
    uint8_t head;       /* Head from ID */
    uint8_t sector;     /* Sector number */
    uint8_t size;       /* Size code */
    uint8_t eot;        /* End of track (last sector) */
    uint8_t gap;        /* Gap3 length (for read/write) */
    uint8_t datalen;    /* Data length if size=0 */
} fd_read_write_params_t;

/**
 * FDC command result (ST0-ST2 + CHRN)
 */
typedef struct {
    uint8_t st0;        /* Status register 0 */
    uint8_t st1;        /* Status register 1 */
    uint8_t st2;        /* Status register 2 */
    uint8_t cyl;        /* Result cylinder */
    uint8_t head;       /* Result head */
    uint8_t sector;     /* Result sector */
    uint8_t size;       /* Result size */
} fd_cmd_result_t;

/**
 * Format track parameters
 * Note: Followed by array of fd_id_header_t for each sector
 */
typedef struct {
    uint8_t flags;      /* MFM flag */
    uint8_t phead;      /* Physical head */
    uint8_t size;       /* Sector size code */
    uint8_t sectors;    /* Number of sectors */
    uint8_t gap;        /* Gap3 length */
    uint8_t fill;       /* Fill byte */
    /* fd_id_header_t headers[]; - follows immediately */
} fd_format_params_t;

/**
 * Read ID parameters
 */
typedef struct {
    uint8_t flags;      /* MFM flag */
    uint8_t head;       /* Physical head */
} fd_read_id_params_t;

/**
 * Configure command parameters
 */
typedef struct {
    uint8_t eis_efifo_poll_fifothr;  /* b6=implied seek, b5=fifo, b4=poll disable, b3-0=fifo threshold */
    uint8_t pretrk;                   /* Precompensation start track */
} fd_configure_params_t;

/**
 * Specify command parameters
 */
typedef struct {
    uint8_t srt_hut;    /* b7-4=step rate, b3-0=head unload time */
    uint8_t hlt_nd;     /* b7-1=head load time, b0=non-DMA (unsupported) */
} fd_specify_params_t;

/**
 * Sense drive status parameters
 */
typedef struct {
    uint8_t head;       /* Physical head */
} fd_sense_params_t;

/**
 * Drive status result
 */
typedef struct {
    uint8_t st3;        /* Status register 3 */
} fd_drive_status_t;

/**
 * Interrupt status result
 */
typedef struct {
    uint8_t st0;        /* Status register 0 */
    uint8_t pcn;        /* Present cylinder number */
} fd_interrupt_status_t;

/**
 * Perpendicular mode parameters
 */
typedef struct {
    uint8_t ow_ds_gap_wgate;  /* b7=OW, b5-2=drive select, b1=gap2, b0=write gate */
} fd_perpendicular_params_t;

/**
 * Lock parameters/result
 */
typedef struct {
    uint8_t lock;       /* b7=lock (params), b4=lock (result) */
} fd_lock_params_t;

/**
 * Dump registers result
 */
typedef struct {
    uint8_t pcn0, pcn1, pcn2, pcn3;  /* Present cylinder numbers */
    uint8_t srt_hut;                  /* b7-4=step rate, b3-0=head unload */
    uint8_t hlt_nd;                   /* b7-1=head load time, b0=non-dma */
    uint8_t sceot;                    /* Sector count / end of track */
    uint8_t lock_d0123_gap_wgate;     /* b7=lock, b5-2=drive sel, b1=gap2, b0=wgate */
    uint8_t eis_efifo_poll_fifothr;   /* b6=implied seeks, b5=fifo, b4=poll, b3-0=threshold */
    uint8_t pretrk;                   /* Precompensation start track */
} fd_dumpreg_result_t;

/**
 * Sector offset parameters
 */
typedef struct {
    uint8_t sectors;    /* Number of sectors to skip after index */
} fd_sector_offset_params_t;

/**
 * Short write parameters (for reproducing CRC errors)
 */
typedef struct {
    uint32_t length;    /* Bytes to write before interrupting */
    uint32_t finetune;  /* Fine-tune delay in microseconds */
} fd_short_write_params_t;

/**
 * Track scan parameters
 */
typedef struct {
    uint8_t flags;      /* MFM flag */
    uint8_t head;       /* Physical head */
} fd_scan_params_t;

/**
 * Track scan result
 * Note: Followed by array of fd_id_header_t
 */
typedef struct {
    uint8_t count;      /* Number of headers found */
    /* fd_id_header_t headers[]; */
} fd_scan_result_t;

/**
 * Timed ID header (with position from index)
 */
typedef struct {
    uint32_t reltime;   /* Time from index in microseconds */
    uint8_t cyl;
    uint8_t head;
    uint8_t sector;
    uint8_t size;
} fd_timed_id_header_t;

/**
 * Timed track scan result
 */
typedef struct {
    uint8_t count;          /* Number of headers */
    uint8_t firstseen;      /* Offset of first sector detected */
    uint32_t tracktime;     /* Total track time in microseconds */
    /* fd_timed_id_header_t headers[]; */
} fd_timed_scan_result_t;

/**
 * FDC information
 */
typedef struct {
    uint8_t controller_type;    /* FDC_TYPE_* */
    uint8_t speeds_available;   /* FDC_SPEED_* flags */
    uint8_t bus_type;
    uint32_t bus_number;
    uint32_t controller_number;
    uint32_t peripheral_number;
} fd_fdc_info_t;

/**
 * Raw read parameters
 */
typedef struct {
    uint8_t flags;      /* MFM flag */
    uint8_t head;       /* Physical head */
    uint8_t size;       /* Size code (determines bytes to read) */
} fd_raw_read_params_t;

#pragma pack(pop)

/*============================================================================
 * STATUS REGISTER BIT DEFINITIONS
 *============================================================================*/

/* Status Register 0 (ST0) */
#define FDC_ST0_IC_MASK     0xC0    /* Interrupt code */
#define FDC_ST0_IC_NORMAL   0x00    /* Normal termination */
#define FDC_ST0_IC_ABNORMAL 0x40    /* Abnormal termination */
#define FDC_ST0_IC_INVALID  0x80    /* Invalid command */
#define FDC_ST0_IC_READY    0xC0    /* Ready change */
#define FDC_ST0_SE          0x20    /* Seek end */
#define FDC_ST0_EC          0x10    /* Equipment check */
#define FDC_ST0_NR          0x08    /* Not ready */
#define FDC_ST0_HD          0x04    /* Head address */
#define FDC_ST0_US_MASK     0x03    /* Unit select */

/* Status Register 1 (ST1) */
#define FDC_ST1_EN          0x80    /* End of cylinder */
#define FDC_ST1_DE          0x20    /* Data error (CRC) */
#define FDC_ST1_OR          0x10    /* Overrun */
#define FDC_ST1_ND          0x04    /* No data */
#define FDC_ST1_NW          0x02    /* Not writable */
#define FDC_ST1_MA          0x01    /* Missing address mark */

/* Status Register 2 (ST2) */
#define FDC_ST2_CM          0x40    /* Control mark (deleted data) */
#define FDC_ST2_DD          0x20    /* Data error in data field */
#define FDC_ST2_WC          0x10    /* Wrong cylinder */
#define FDC_ST2_SH          0x08    /* Scan equal hit */
#define FDC_ST2_SN          0x04    /* Scan not satisfied */
#define FDC_ST2_BC          0x02    /* Bad cylinder */
#define FDC_ST2_MD          0x01    /* Missing data address mark */

/* Status Register 3 (ST3) */
#define FDC_ST3_WP          0x40    /* Write protected */
#define FDC_ST3_T0          0x10    /* Track 0 */
#define FDC_ST3_HD          0x04    /* Head */
#define FDC_ST3_US_MASK     0x03    /* Unit select */

/*============================================================================
 * HELPER MACROS
 *============================================================================*/

/**
 * Calculate sector size from size code
 * size_code: 0=128, 1=256, 2=512, 3=1024, etc.
 */
#define FD_SECTOR_SIZE(size_code) (128U << (size_code))

/**
 * Calculate size code from sector size
 */
static inline uint8_t fd_size_code(uint32_t bytes) {
    uint8_t code = 0;
    while ((128U << code) < bytes && code < 7) code++;
    return code;
}

/**
 * Check if result indicates error
 */
static inline int fd_result_error(const fd_cmd_result_t *r) {
    return (r->st0 & FDC_ST0_IC_MASK) != FDC_ST0_IC_NORMAL ||
           (r->st1 & (FDC_ST1_DE | FDC_ST1_OR | FDC_ST1_ND | FDC_ST1_NW | FDC_ST1_MA)) ||
           (r->st2 & (FDC_ST2_DD | FDC_ST2_WC | FDC_ST2_BC | FDC_ST2_MD));
}

/**
 * Check for CRC error in data field
 */
static inline int fd_result_crc_error(const fd_cmd_result_t *r) {
    return (r->st1 & FDC_ST1_DE) || (r->st2 & FDC_ST2_DD);
}

/**
 * Check for deleted data mark
 */
static inline int fd_result_deleted(const fd_cmd_result_t *r) {
    return r->st2 & FDC_ST2_CM;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_FDRAWCMD_H */
