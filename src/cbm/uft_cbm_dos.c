/**
 * @file uft_cbm_dos.c
 * @brief Commodore DOS Command Builders Implementation
 * 
 * 
 * @copyright UFT Project 2026
 */

#include "uft/cbm/uft_cbm_dos.h"
#include <string.h>
#include <stdio.h>

/*============================================================================
 * Command Builders
 *============================================================================*/

uft_cbm_status_t uft_cbm_build_mw(uint16_t addr, const uint8_t *data, 
                                   uint8_t data_len, uint8_t *out, 
                                   size_t out_cap, size_t *out_len)
{
    if (!out || !out_len) return UFT_CBM_E_INVALID;
    if (data_len > 34) return UFT_CBM_E_INVALID;
    if (data_len > 0 && !data) return UFT_CBM_E_INVALID;
    
    size_t need = 3 + 3 + data_len;  /* "M-W" + addr_lo + addr_hi + len + data */
    if (out_cap < need) {
        *out_len = need;
        return UFT_CBM_E_BUF;
    }
    
    out[0] = 'M';
    out[1] = '-';
    out[2] = 'W';
    out[3] = (uint8_t)(addr & 0xFF);        /* addr_lo */
    out[4] = (uint8_t)((addr >> 8) & 0xFF); /* addr_hi */
    out[5] = data_len;
    
    if (data_len > 0 && data) {
        memcpy(&out[6], data, data_len);
    }
    
    *out_len = need;
    return UFT_CBM_OK;
}

uft_cbm_status_t uft_cbm_build_mr(uint16_t addr, uint8_t len,
                                   uint8_t *out, size_t out_cap, 
                                   size_t *out_len)
{
    if (!out || !out_len) return UFT_CBM_E_INVALID;
    
    size_t need = 6;  /* "M-R" + addr_lo + addr_hi + len */
    if (out_cap < need) {
        *out_len = need;
        return UFT_CBM_E_BUF;
    }
    
    out[0] = 'M';
    out[1] = '-';
    out[2] = 'R';
    out[3] = (uint8_t)(addr & 0xFF);
    out[4] = (uint8_t)((addr >> 8) & 0xFF);
    out[5] = len;
    
    *out_len = need;
    return UFT_CBM_OK;
}

uft_cbm_status_t uft_cbm_build_bp(uint8_t buffer, uint8_t offset,
                                   char *out, size_t out_cap)
{
    if (!out) return UFT_CBM_E_INVALID;
    if (out_cap < 12) return UFT_CBM_E_BUF;
    
    snprintf(out, out_cap, "B-P %u %u", (unsigned)buffer, (unsigned)offset);
    return UFT_CBM_OK;
}

uft_cbm_status_t uft_cbm_build_u1(uint8_t channel, uint8_t drive,
                                   uint8_t track, uint8_t sector,
                                   char *out, size_t out_cap)
{
    if (!out) return UFT_CBM_E_INVALID;
    if (out_cap < 20) return UFT_CBM_E_BUF;
    
    snprintf(out, out_cap, "U1:%u %u %u %u", 
             (unsigned)channel, (unsigned)drive, 
             (unsigned)track, (unsigned)sector);
    return UFT_CBM_OK;
}

uft_cbm_status_t uft_cbm_build_u2(uint8_t channel, uint8_t drive,
                                   uint8_t track, uint8_t sector,
                                   char *out, size_t out_cap)
{
    if (!out) return UFT_CBM_E_INVALID;
    if (out_cap < 20) return UFT_CBM_E_BUF;
    
    snprintf(out, out_cap, "U2:%u %u %u %u", 
             (unsigned)channel, (unsigned)drive, 
             (unsigned)track, (unsigned)sector);
    return UFT_CBM_OK;
}

/*============================================================================
 * Command Sequences
 *============================================================================*/

uft_cbm_status_t uft_cbm_build_read_cmds(uint8_t track, uint8_t sector,
                                          uft_cbm_read_cmds_t *out)
{
    if (!out) return UFT_CBM_E_INVALID;
    
    memset(out, 0, sizeof(*out));
    
    /* M-W: Write track/sector to $000A (buffer 2 job T/S) */
    uint8_t ts_data[2] = { track, sector };
    uft_cbm_status_t st = uft_cbm_build_mw(UFT_CBM_JOB_TS_BUF2, ts_data, 2,
                                           out->mw_ts, sizeof(out->mw_ts),
                                           &out->mw_ts_len);
    if (st != UFT_CBM_OK) return st;
    
    /* M-W: Write job code 0x80 (read) to $0002 (buffer 2 job code) */
    uint8_t job_data[1] = { UFT_CBM_JOB_READ };
    st = uft_cbm_build_mw(UFT_CBM_JOB_CODE_BUF2, job_data, 1,
                          out->mw_job, sizeof(out->mw_job),
                          &out->mw_job_len);
    if (st != UFT_CBM_OK) return st;
    
    /* M-R: Read status from $0002 */
    st = uft_cbm_build_mr(UFT_CBM_JOB_CODE_BUF2, 1,
                          out->mr_status, sizeof(out->mr_status),
                          &out->mr_status_len);
    if (st != UFT_CBM_OK) return st;
    
    /* B-P: Set buffer pointer */
    st = uft_cbm_build_bp(2, 0, out->bp_cmd, sizeof(out->bp_cmd));
    
    return st;
}

/*============================================================================
 * Error Strings
 *============================================================================*/

const char* uft_cbm_error_string(uint8_t status)
{
    switch (status) {
        case UFT_CBM_ERR_00: return "OK";
        case UFT_CBM_ERR_20: return "Header block not found";
        case UFT_CBM_ERR_21: return "No sync character";
        case UFT_CBM_ERR_22: return "Data block not present";
        case UFT_CBM_ERR_23: return "Checksum error in data";
        case UFT_CBM_ERR_24: return "Write verify error";
        case UFT_CBM_ERR_25: return "Write error";
        case UFT_CBM_ERR_26: return "Write protect on";
        case UFT_CBM_ERR_27: return "Checksum error in header";
        case UFT_CBM_ERR_28: return "Write error (long)";
        case UFT_CBM_ERR_29: return "Disk ID mismatch";
        default:
            if (status & 0x80) return "Busy";
            return "Unknown error";
    }
}

/*============================================================================
 * CBM DOS High-Level Error Messages
 *============================================================================*/

static const struct {
    int code;
    const char* message;
} cbm_dos_errors[] = {
    /* Non-errors */
    {0,  "ok"},
    {1,  "files scratched"},
    {2,  "partition selected"},
    /* Read/Write errors */
    {20, "read error (block header not found)"},
    {21, "read error (drive not ready)"},
    {22, "read error (data block not found)"},
    {23, "read error (crc error in data block)"},
    {24, "read error (byte sector header)"},
    {25, "write error (write-verify error)"},
    {26, "write protect on"},
    {27, "read error (crc error in header)"},
    /* Syntax errors */
    {30, "syntax error (general syntax)"},
    {31, "syntax error (invalid command)"},
    {32, "syntax error (long line)"},
    {33, "syntax error (invalid file name)"},
    {34, "syntax error (no file given)"},
    {39, "syntax error (invalid command)"},
    /* Record errors */
    {50, "record not present"},
    {51, "overflow in record"},
    {52, "file too large"},
    /* File errors */
    {60, "write file open"},
    {61, "file not open"},
    {62, "file not found"},
    {63, "file exists"},
    {64, "file type mismatch"},
    {65, "no block"},
    {66, "illegal track and sector"},
    {67, "illegal system t or s"},
    /* System errors */
    {70, "no channel"},
    {71, "directory error"},
    {72, "disk full"},
    {73, "dos mismatch"},
    {74, "drive not ready"},
    {75, "format error"},
    {76, "controller error"},
    {77, "selected partition illegal"},
    {-1, NULL}
};

const char* uft_cbm_dos_error_string(uint8_t error_code)
{
    for (size_t i = 0; cbm_dos_errors[i].code >= 0; i++) {
        if (cbm_dos_errors[i].code == error_code) {
            return cbm_dos_errors[i].message;
        }
    }
    return "unknown error";
}

int uft_cbm_dos_format_status(uint8_t error_code, uint8_t track, 
                               uint8_t sector, char* out, size_t out_cap)
{
    if (!out || out_cap < 32) return -1;
    
    const char* msg = uft_cbm_dos_error_string(error_code);
    snprintf(out, out_cap, "%02d,%s,%02d,%02d", 
             error_code, msg, track, sector);
    return 0;
}
