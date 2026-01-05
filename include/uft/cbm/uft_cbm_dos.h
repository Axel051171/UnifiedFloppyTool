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

/**
 * @brief Check if drive job is busy
 * @param status Status byte from M-R
 * @return true if bit 7 is set (busy)
 */
static inline int uft_cbm_is_busy(uint8_t status) {
    return (status & 0x80) != 0;
}

/**
 * @brief Check if drive job completed successfully
 * @param status Status byte from M-R
 * @return true if status == 0x01 (OK)
 */
static inline int uft_cbm_is_ok(uint8_t status) {
    return status == 0x01;
}

/*============================================================================
 * Command Builders
 *============================================================================*/

/**
 * @brief Build M-W (Memory Write) command
 * 
 * Format: "M-W" + addr_lo + addr_hi + len + data[len]
 * 
 * @param addr Target address in drive memory
 * @param data Data to write
 * @param data_len Length of data (1-34 bytes)
 * @param out Output buffer
 * @param out_cap Output buffer capacity
 * @param out_len Output: bytes written
 * @return Status code
 */
uft_cbm_status_t uft_cbm_build_mw(uint16_t addr, const uint8_t *data, 
                                   uint8_t data_len, uint8_t *out, 
                                   size_t out_cap, size_t *out_len);

/**
 * @brief Build M-R (Memory Read) command
 * 
 * Format: "M-R" + addr_lo + addr_hi + len
 * 
 * @param addr Source address in drive memory
 * @param len Number of bytes to read (1-255)
 * @param out Output buffer
 * @param out_cap Output buffer capacity
 * @param out_len Output: bytes written
 * @return Status code
 */
uft_cbm_status_t uft_cbm_build_mr(uint16_t addr, uint8_t len,
                                   uint8_t *out, size_t out_cap, 
                                   size_t *out_len);

/**
 * @brief Build B-P (Buffer Pointer) command
 * 
 * Format: "B-P <buffer> <offset>" (ASCII, space-separated)
 * 
 * @param buffer Buffer number (0-4)
 * @param offset Offset within buffer (0-255)
 * @param out Output buffer (ASCII string)
 * @param out_cap Output buffer capacity
 * @return Status code
 */
uft_cbm_status_t uft_cbm_build_bp(uint8_t buffer, uint8_t offset,
                                   char *out, size_t out_cap);

/**
 * @brief Build U1 (Block Read) command
 * 
 * Format: "U1:<channel> <drive> <track> <sector>"
 * 
 * @param channel Channel number (2-14)
 * @param drive Drive number (0-1)
 * @param track Track number (1-40)
 * @param sector Sector number (0-20)
 * @param out Output buffer (ASCII string)
 * @param out_cap Output buffer capacity
 * @return Status code
 */
uft_cbm_status_t uft_cbm_build_u1(uint8_t channel, uint8_t drive,
                                   uint8_t track, uint8_t sector,
                                   char *out, size_t out_cap);

/**
 * @brief Build U2 (Block Write) command
 * 
 * Format: "U2:<channel> <drive> <track> <sector>"
 */
uft_cbm_status_t uft_cbm_build_u2(uint8_t channel, uint8_t drive,
                                   uint8_t track, uint8_t sector,
                                   char *out, size_t out_cap);

/*============================================================================
 * Common Command Sequences
 *============================================================================*/

/**
 * @brief Command sequence for raw sector read
 * 
 * Standard BASIC sequence:
 *   OPEN 1,8,15
 *   OPEN 2,8,2,"#2"
 *   PRINT#1,"M-W"+chr$(10)+chr$(0)+chr$(2)+chr$(track)+chr$(sector)
 *   PRINT#1,"M-W"+chr$(2)+chr$(0)+chr$(1)+chr$(128)
 *   PRINT#1,"M-R"+chr$(2)+chr$(0)+chr$(1)
 *   GET#1,a$ : ... poll until not busy ...
 *   PRINT#1,"B-P 2 0"
 *   FOR i=0 TO 255: GET#2,a$: ... : NEXT
 */
typedef struct uft_cbm_read_cmds {
    uint8_t mw_ts[8];       /**< M-W command: set track/sector at $000A */
    size_t  mw_ts_len;
    
    uint8_t mw_job[7];      /**< M-W command: set job code at $0002 */
    size_t  mw_job_len;
    
    uint8_t mr_status[6];   /**< M-R command: read status from $0002 */
    size_t  mr_status_len;
    
    char    bp_cmd[16];     /**< B-P command: "B-P 2 0" */
} uft_cbm_read_cmds_t;

/**
 * @brief Build command sequence for raw sector read
 * 
 * @param track Track number (1-40)
 * @param sector Sector number (0-20)
 * @param out Output structure
 * @return Status code
 */
uft_cbm_status_t uft_cbm_build_read_cmds(uint8_t track, uint8_t sector,
                                          uft_cbm_read_cmds_t *out);

/*============================================================================
 * Drive Memory Map Constants
 *============================================================================*/

/** Job queue start address */
#define UFT_CBM_JOB_QUEUE       0x0000

/** Job track/sector for buffer 0 */
#define UFT_CBM_JOB_TS_BUF0     0x0006

/** Job track/sector for buffer 2 */
#define UFT_CBM_JOB_TS_BUF2     0x000A

/** Job code/status for buffer 0 */
#define UFT_CBM_JOB_CODE_BUF0   0x0000

/** Job code/status for buffer 2 */
#define UFT_CBM_JOB_CODE_BUF2   0x0002

/** Buffer 0 data area */
#define UFT_CBM_BUFFER_0        0x0300

/** Buffer 2 data area */
#define UFT_CBM_BUFFER_2        0x0500

/** Job codes */
#define UFT_CBM_JOB_READ        0x80    /**< Read sector */
#define UFT_CBM_JOB_WRITE       0x90    /**< Write sector */
#define UFT_CBM_JOB_VERIFY      0xA0    /**< Verify sector */
#define UFT_CBM_JOB_SEEK        0xB0    /**< Seek track */
#define UFT_CBM_JOB_BUMP        0xC0    /**< Bump head */
#define UFT_CBM_JOB_EXEC        0xE0    /**< Execute code */

/** Job status codes */
#define UFT_CBM_STATUS_OK       0x01    /**< Success */
#define UFT_CBM_STATUS_BUSY     0x80    /**< Busy (bit 7 set) */

/*============================================================================
 * Error Codes (from 1541 DOS)
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

/**
 * @brief Get error description
 * @param status Drive status code
 * @return Static error string
 */
const char* uft_cbm_error_string(uint8_t status);

#ifdef __cplusplus
}
#endif

#endif /* UFT_CBM_DOS_H */
