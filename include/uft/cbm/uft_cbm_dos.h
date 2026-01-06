/**
 * @file uft_cbm_dos.h
 * @brief Commodore DOS Command Builders for UFT
 * 
 * Helpers to build M-W (Memory Write), M-R (Memory Read), and B-P 
 * (Buffer Pointer) command strings for communication with Commodore
 * disk drives over IEC bus.
 * 
 * These commands are used for direct drive memory access, which is
 * essential for:
 * - Reading raw GCR data
 * - Writing custom track layouts
 * - Copy protection analysis
 * - Drive diagnostics
 * 
 * Source: cbm_dos_mw_mr.h (UFT Project)
 * 
 * @copyright UFT Project 2026
 */

#ifndef UFT_CBM_DOS_H
#define UFT_CBM_DOS_H

#include <stdint.h>
#include <stddef.h>


#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Status Codes
 *============================================================================*/

typedef enum uft_cbm_status {
    UFT_CBM_OK          = 0,
    UFT_CBM_E_INVALID   = 1,    /**< Invalid parameter */
    UFT_CBM_E_BUF       = 2     /**< Buffer too small */
} uft_cbm_status_t;

/*============================================================================
 * Drive Job Status
 *============================================================================*/

static inline int uft_cbm_is_busy(uint8_t status) {
    return (status & 0x80) != 0;
}

static inline int uft_cbm_is_ok(uint8_t status) {
    return status == 0x01;
}

/*============================================================================
 * Command Builders
 *============================================================================*/

uft_cbm_status_t uft_cbm_build_mw(uint16_t addr, const uint8_t *data, 
                                   uint8_t data_len, uint8_t *out, 
                                   size_t out_cap, size_t *out_len);

uft_cbm_status_t uft_cbm_build_mr(uint16_t addr, uint8_t len,
                                   uint8_t *out, size_t out_cap, 
                                   size_t *out_len);

uft_cbm_status_t uft_cbm_build_bp(uint8_t buffer, uint8_t offset,
                                   char *out, size_t out_cap);

uft_cbm_status_t uft_cbm_build_u1(uint8_t channel, uint8_t drive,
                                   uint8_t track, uint8_t sector,
                                   char *out, size_t out_cap);

uft_cbm_status_t uft_cbm_build_u2(uint8_t channel, uint8_t drive,
                                   uint8_t track, uint8_t sector,
                                   char *out, size_t out_cap);

/*============================================================================
 * Drive Memory Map Constants
 *============================================================================*/

#define UFT_CBM_JOB_QUEUE       0x0000
#define UFT_CBM_JOB_TS_BUF0     0x0006
#define UFT_CBM_JOB_TS_BUF2     0x000A
#define UFT_CBM_JOB_CODE_BUF0   0x0000
#define UFT_CBM_JOB_CODE_BUF2   0x0002
#define UFT_CBM_BUFFER_0        0x0300
#define UFT_CBM_BUFFER_2        0x0500

#define UFT_CBM_JOB_READ        0x80
#define UFT_CBM_JOB_WRITE       0x90
#define UFT_CBM_JOB_VERIFY      0xA0
#define UFT_CBM_JOB_SEEK        0xB0
#define UFT_CBM_JOB_BUMP        0xC0
#define UFT_CBM_JOB_EXEC        0xE0

#define UFT_CBM_STATUS_OK       0x01
#define UFT_CBM_STATUS_BUSY     0x80

/*============================================================================
 * Low-Level Error Codes (Drive Job Errors)
 *============================================================================*/

#define UFT_CBM_ERR_00  0x01    /**< OK */
#define UFT_CBM_ERR_20  0x02    /**< Header block not found */
#define UFT_CBM_ERR_21  0x03    /**< No sync character */
#define UFT_CBM_ERR_22  0x04    /**< Data block not present */
#define UFT_CBM_ERR_23  0x05    /**< Checksum error in data */
#define UFT_CBM_ERR_24  0x06    /**< Write verify error */
#define UFT_CBM_ERR_25  0x07    /**< Write error */
#define UFT_CBM_ERR_26  0x08    /**< Write protect on */
#define UFT_CBM_ERR_27  0x09    /**< Checksum error in header */
#define UFT_CBM_ERR_28  0x0A    /**< Write error (long) */
#define UFT_CBM_ERR_29  0x0B    /**< Disk ID mismatch */

/*============================================================================
 * Common Command Sequences
 *============================================================================*/

typedef struct uft_cbm_read_cmds {
    uint8_t mw_ts[8];
    size_t  mw_ts_len;
    uint8_t mw_job[7];
    size_t  mw_job_len;
    uint8_t mr_status[6];
    size_t  mr_status_len;
    char    bp_cmd[16];
} uft_cbm_read_cmds_t;

uft_cbm_status_t uft_cbm_build_read_cmds(uint8_t track, uint8_t sector,
                                          uft_cbm_read_cmds_t *out);

const char* uft_cbm_error_string(uint8_t status);

/*============================================================================
 * CBM DOS High-Level Error Codes (Channel 15 Status)
 * Source: disk2easyflash (Per Olofsson, BSD License)
 *============================================================================*/

/* Non-errors */
#define UFT_CBM_DOS_OK              0
#define UFT_CBM_DOS_SCRATCHED       1
#define UFT_CBM_DOS_PARTITION       2

/* Read/Write errors (20-29) */
#define UFT_CBM_DOS_READ_HDR        20
#define UFT_CBM_DOS_READ_NOREADY    21
#define UFT_CBM_DOS_READ_DATA       22
#define UFT_CBM_DOS_READ_CRC_DATA   23
#define UFT_CBM_DOS_READ_BYTE_HDR   24
#define UFT_CBM_DOS_WRITE_VERIFY    25
#define UFT_CBM_DOS_WRITE_PROTECT   26
#define UFT_CBM_DOS_READ_CRC_HDR    27

/* Syntax errors (30-39) */
#define UFT_CBM_DOS_SYNTAX          30
#define UFT_CBM_DOS_SYNTAX_CMD      31
#define UFT_CBM_DOS_SYNTAX_LONG     32
#define UFT_CBM_DOS_SYNTAX_NAME     33
#define UFT_CBM_DOS_SYNTAX_NOFILE   34
#define UFT_CBM_DOS_SYNTAX_CMD2     39

/* Record/File errors (50-67) */
#define UFT_CBM_DOS_RECORD_ABSENT   50
#define UFT_CBM_DOS_RECORD_OVERFLOW 51
#define UFT_CBM_DOS_FILE_TOO_LARGE  52
#define UFT_CBM_DOS_FILE_OPEN       60
#define UFT_CBM_DOS_FILE_NOT_OPEN   61
#define UFT_CBM_DOS_FILE_NOT_FOUND  62
#define UFT_CBM_DOS_FILE_EXISTS     63
#define UFT_CBM_DOS_FILE_TYPE       64
#define UFT_CBM_DOS_NO_BLOCK        65
#define UFT_CBM_DOS_ILLEGAL_TS      66
#define UFT_CBM_DOS_ILLEGAL_SYS_TS  67

/* System errors (70-77) */
#define UFT_CBM_DOS_NO_CHANNEL      70
#define UFT_CBM_DOS_DIR_ERROR       71
#define UFT_CBM_DOS_DISK_FULL       72
#define UFT_CBM_DOS_DOS_MISMATCH    73
#define UFT_CBM_DOS_DRIVE_NOT_READY 74
#define UFT_CBM_DOS_FORMAT_ERROR    75
#define UFT_CBM_DOS_CONTROLLER      76
#define UFT_CBM_DOS_PARTITION_ILLEGAL 77

const char* uft_cbm_dos_error_string(uint8_t error_code);

int uft_cbm_dos_format_status(uint8_t error_code, uint8_t track, 
                               uint8_t sector, char* out, size_t out_cap);

#ifdef __cplusplus
}
#endif

#endif /* UFT_CBM_DOS_H */
