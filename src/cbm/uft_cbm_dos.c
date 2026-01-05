/**
 * @file uft_cbm_dos.c
 * @brief Commodore DOS Command Builders Implementation
 * 
 * Source: cbm_dos_mw_mr.h (UFT Project)
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
