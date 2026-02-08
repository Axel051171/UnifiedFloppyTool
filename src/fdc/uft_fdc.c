/**
 * @file uft_fdc.c
 * @brief Low-Level Floppy Disk Controller Implementation for UFT
 * 
 * @copyright UFT Project
 */

#include "uft_fdc.h"
#include <string.h>

/*============================================================================
 * Gap Length Tables
 * From ImageDisk source and PC floppy controller specifications
 *============================================================================*/

/* 8" FM (Single Density) gap lengths */
const uft_fdc_gap_entry_t uft_fdc_gaps_8in_fm[] = {
    { 0, 26, 0x07, 0x1B },  /* 128 bytes */
    { 1, 15, 0x0E, 0x2A },  /* 256 bytes */
    { 2,  8, 0x1B, 0x3A },  /* 512 bytes */
    { 3,  4, 0x47, 0x8A },  /* 1024 bytes */
    { 4,  2, 0xC8, 0xFF },  /* 2048 bytes */
    { 5,  1, 0xC8, 0xFF },  /* 4096 bytes */
    { 0xFF, 0, 0, 0 }       /* End marker */
};

/* 8" MFM (Double Density) gap lengths */
const uft_fdc_gap_entry_t uft_fdc_gaps_8in_mfm[] = {
    { 1, 26, 0x0E, 0x36 },  /* 256 bytes */
    { 2, 15, 0x1B, 0x54 },  /* 512 bytes */
    { 3,  8, 0x35, 0x74 },  /* 1024 bytes */
    { 4,  4, 0x99, 0xFF },  /* 2048 bytes */
    { 5,  2, 0xC8, 0xFF },  /* 4096 bytes */
    { 6,  1, 0xC8, 0xFF },  /* 8192 bytes */
    { 0xFF, 0, 0, 0 }       /* End marker */
};

/* 5.25" FM (Single Density) gap lengths */
const uft_fdc_gap_entry_t uft_fdc_gaps_5in_fm[] = {
    { 0, 18, 0x07, 0x09 },  /* 128 bytes, 18 sectors */
    { 0, 16, 0x10, 0x19 },  /* 128 bytes, 16 sectors */
    { 1,  8, 0x18, 0x30 },  /* 256 bytes */
    { 2,  4, 0x46, 0x87 },  /* 512 bytes */
    { 3,  2, 0xC8, 0xFF },  /* 1024 bytes */
    { 4,  1, 0xC8, 0xFF },  /* 2048 bytes */
    { 0xFF, 0, 0, 0 }       /* End marker */
};

/* 5.25"/3.5" MFM (Double Density) gap lengths */
const uft_fdc_gap_entry_t uft_fdc_gaps_5in_mfm[] = {
    { 1, 18, 0x0A, 0x0C },  /* 256 bytes, 18 sectors */
    { 1, 16, 0x20, 0x32 },  /* 256 bytes, 16 sectors */
    { 2,  8, 0x2A, 0x50 },  /* 512 bytes, 8 sectors */
    { 2,  9, 0x18, 0x40 },  /* 512 bytes, 9 sectors (720K) */
    { 2, 10, 0x07, 0x0E },  /* 512 bytes, 10 sectors */
    { 2, 18, 0x1B, 0x54 },  /* 512 bytes, 18 sectors (1.44MB) */
    { 2, 21, 0x0C, 0x1C },  /* 512 bytes, 21 sectors (DMF) */
    { 3,  4, 0x8D, 0xF0 },  /* 1024 bytes */
    { 4,  2, 0xC8, 0xFF },  /* 2048 bytes */
    { 5,  1, 0xC8, 0xFF },  /* 4096 bytes */
    { 0xFF, 0, 0, 0 }       /* End marker */
};

/*============================================================================
 * Standard Drive Parameters (from Linux kernel)
 *============================================================================*/

const uft_fdc_drive_params_t uft_fdc_drive_types[] = {
    /* Type 0: No drive */
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "none" },
    
    /* Type 1: 5.25" 360K DD */
    { 1, 300, 16, 16, 8000, 1000, 3000, 0, 26, 5, 40, 3000, 17, "5.25\" DD" },
    
    /* Type 2: 5.25" 1.2M HD */
    { 2, 500, 16, 16, 6000, 400, 3000, 0, 26, 6, 83, 3000, 17, "5.25\" HD" },
    
    /* Type 3: 3.5" 720K DD */
    { 3, 250, 16, 16, 3000, 1000, 3000, 0, 26, 5, 83, 3000, 20, "3.5\" DD" },
    
    /* Type 4: 3.5" 1.44M HD */
    { 4, 500, 16, 16, 4000, 400, 3000, 0, 26, 6, 83, 3000, 17, "3.5\" HD" },
    
    /* Type 5: 3.5" 2.88M ED */
    { 5, 1000, 16, 16, 4000, 400, 3000, 0, 26, 6, 83, 3000, 17, "3.5\" ED" },
    
    /* Type 6: 8" SD/DD */
    { 6, 500, 16, 16, 6000, 400, 3000, 0, 26, 6, 77, 3000, 17, "8\"" },
};

/*============================================================================
 * Standard Format Parameters (from Linux kernel uft_floppy_type[])
 *============================================================================*/

const uft_fdc_format_t uft_fdc_formats[] = {
    /* 0: Unused */
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL },
    
    /* 1: 5.25" 360K (9 sect/track, 40 tracks, DS) */
    { 720, 9, 2, 40, 0, 0x2A, 0x02, 0xDF, 0x50, "d360" },
    
    /* 2: 5.25" 1.2M (15 sect/track, 80 tracks, DS) */
    { 2400, 15, 2, 80, 0, 0x1B, 0x00, 0xDF, 0x54, "h1200" },
    
    /* 3: 3.5" 360K (9 sect/track, 80 tracks, SS) */
    { 720, 9, 1, 80, 0, 0x2A, 0x02, 0xDF, 0x50, "D360" },
    
    /* 4: 3.5" 720K (9 sect/track, 80 tracks, DS) */
    { 1440, 9, 2, 80, 0, 0x2A, 0x02, 0xDF, 0x50, "D720" },
    
    /* 5: 3.5" 1.44M (18 sect/track, 80 tracks, DS) */
    { 2880, 18, 2, 80, 0, 0x1B, 0x00, 0xCF, 0x6C, "H1440" },
    
    /* 6: 3.5" 2.88M (36 sect/track, 80 tracks, DS) */
    { 5760, 36, 2, 80, 0, 0x38, 0x43, 0xAF, 0x54, "E2880" },
    
    /* 7: 5.25" 160K (8 sect/track, 40 tracks, SS) */
    { 320, 8, 1, 40, 0, 0x2A, 0x02, 0xDF, 0x50, "d160" },
    
    /* 8: 5.25" 180K (9 sect/track, 40 tracks, SS) */
    { 360, 9, 1, 40, 0, 0x2A, 0x02, 0xDF, 0x50, "d180" },
    
    /* 9: 5.25" 320K (8 sect/track, 40 tracks, DS) */
    { 640, 8, 2, 40, 0, 0x2A, 0x02, 0xDF, 0x50, "d320" },
    
    /* 10: 3.5" 1.68M DMF (21 sect/track, 80 tracks, DS) */
    { 3360, 21, 2, 80, 0, 0x0C, 0x00, 0xCF, 0x1C, "H1680" },
    
    /* 11: 3.5" 1.72M (21 sect/track, 82 tracks, DS) */
    { 3444, 21, 2, 82, 0, 0x0C, 0x00, 0xCF, 0x1C, "H1722" },
    
    /* 12: 8" SD (26 sect/track, 77 tracks, SS) - IBM 3740 */
    { 2002, 26, 1, 77, 0, 0x07, 0x00, 0xDF, 0x1B, "8SD" },
    
    /* 13: 8" SD (26 sect/track, 77 tracks, DS) */
    { 4004, 26, 2, 77, 0, 0x07, 0x00, 0xDF, 0x1B, "8SD-DS" },
    
    /* 14: 8" DD (26 sect/track, 77 tracks, SS) - 256 bytes/sector */
    { 2002, 26, 1, 77, 0, 0x0E, 0x00, 0xDF, 0x36, "8DD" },
    
    /* 15: 8" DD (26 sect/track, 77 tracks, DS) - 256 bytes/sector */
    { 4004, 26, 2, 77, 0, 0x0E, 0x00, 0xDF, 0x36, "8DD-DS" },
    
    /* 16: 5.25" 1.2M with 360K media (9 sect, 40 tracks, double step) */
    { 720, 9, 2, 40, 1, 0x23, 0x01, 0xDF, 0x50, "h360" },
    
    /* 17: 3.5" 820K (10 sect/track, 82 tracks, DS) */
    { 1640, 10, 2, 82, 0, 0x10, 0x02, 0xDF, 0x2E, "D820" },
    
    /* 18: 3.5" 1.48M (18 sect/track, 82 tracks, DS) */
    { 2952, 18, 2, 82, 0, 0x1B, 0x00, 0xCF, 0x6C, "h1476" },
    
    /* 19: 3.5" 1.6M (20 sect/track, 80 tracks, DS) */
    { 3200, 20, 2, 80, 0, 0x1C, 0x00, 0xCF, 0x50, "H1600" },
    
    /* 20: 5.25" 410K (10 sect/track, 41 tracks, DS) */
    { 820, 10, 2, 41, 1, 0x25, 0x01, 0xDF, 0x2E, "h410" },
    
    /* 21: 3.5" 800K (10 sect/track, 80 tracks, DS) - Atari/Amiga compat */
    { 1600, 10, 2, 80, 0, 0x10, 0x02, 0xDF, 0x2E, "D800" },
    
    /* 22: 3.5" XDF format (23 sect equiv, 80 tracks, DS) */
    { 3680, 23, 2, 80, 0, 0x1B, 0x00, 0xCF, 0x54, "H1840" },
    
    /* 23: 3.5" 1.88M (23 sect/track, 82 tracks, DS) */
    { 3772, 23, 2, 82, 0, 0x1B, 0x00, 0xCF, 0x54, "h1886" },
    
    /* 24: CP/M-86 format (8 sect/track, 40 tracks, SS, 512 bytes) */
    { 320, 8, 1, 40, 0, 0x2A, 0x02, 0xDF, 0x50, "CPM-86" },
    
    /* 25: NEC PC-98 2DD (8 sect/track, 77 tracks, DS, 1024 bytes) */
    { 1232, 8, 2, 77, 0, 0x35, 0x02, 0xDF, 0x74, "PC98-2DD" },
    
    /* 26: NEC PC-98 2HD (8 sect/track, 77 tracks, DS, 1024 bytes) */
    { 1232, 8, 2, 77, 0, 0x35, 0x00, 0xDF, 0x74, "PC98-2HD" },
    
    /* 27: Acorn DFS (10 sect/track, 80 tracks, DS, 256 bytes) */
    { 1600, 10, 2, 80, 0, 0x20, 0x02, 0xDF, 0x32, "Acorn" },
    
    /* End marker */
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL },
};

/*============================================================================
 * Gap Length Lookup
 *============================================================================*/

bool uft_fdc_get_gaps(bool mfm, bool inch8, uint8_t ssize, uint8_t nsect,
                      uint8_t* gap_rw, uint8_t* gap_fmt)
{
    const uft_fdc_gap_entry_t* table;
    
    if (inch8) {
        table = mfm ? uft_fdc_gaps_8in_mfm : uft_fdc_gaps_8in_fm;
    } else {
        table = mfm ? uft_fdc_gaps_5in_mfm : uft_fdc_gaps_5in_fm;
    }
    
    /* Find best match */
    for (int i = 0; table[i].ssize != 0xFF; i++) {
        if (table[i].ssize == ssize && nsect <= table[i].max_sect) {
            if (gap_rw) *gap_rw = table[i].gap3_rw;
            if (gap_fmt) *gap_fmt = table[i].gap3_fmt;
            return true;
        }
    }
    
    /* Default values */
    if (gap_rw) *gap_rw = mfm ? 0x1B : 0x07;
    if (gap_fmt) *gap_fmt = mfm ? 0x54 : 0x1B;
    return false;
}

/*============================================================================
 * Result Error Descriptions
 *============================================================================*/

const char* uft_fdc_result_error(const uft_fdc_result_t* r)
{
    if (!r) return "Invalid result";
    
    /* Check interrupt code */
    switch (r->st0 & UFT_ST0_IC_MASK) {
        case UFT_ST0_IC_NORMAL:
            return NULL;  /* No error */
        case UFT_ST0_IC_INVALID:
            return "Invalid command";
        case UFT_ST0_IC_READY_CHG:
            return "Ready signal changed";
        case UFT_ST0_IC_ABNORMAL:
            /* Check ST1/ST2 for specific error */
            break;
    }
    
    /* ST1 errors */
    if (r->st1 & UFT_ST1_END_CYL)
        return "End of cylinder";
    if (r->st1 & UFT_ST1_CRC_ERROR)
        return "CRC error in ID field";
    if (r->st1 & UFT_ST1_OVERRUN)
        return "Overrun/underrun";
    if (r->st1 & UFT_ST1_NO_DATA)
        return "Sector not found";
    if (r->st1 & UFT_ST1_NOT_WRITABLE)
        return "Write protected";
    if (r->st1 & UFT_ST1_MISSING_AM)
        return "Missing address mark";
    
    /* ST2 errors */
    if (r->st2 & UFT_ST2_CRC_ERROR_DATA)
        return "CRC error in data field";
    if (r->st2 & UFT_ST2_WRONG_CYL)
        return "Wrong cylinder";
    if (r->st2 & UFT_ST2_BAD_CYL)
        return "Bad cylinder";
    if (r->st2 & UFT_ST2_MISSING_DAM)
        return "Missing data address mark";
    
    return "Unknown error";
}

/*============================================================================
 * Track Capacity Calculation
 *============================================================================*/

uint32_t uft_fdc_formatted_size(uint8_t nsect, uint16_t ssize, bool mfm)
{
    /* Overhead per sector:
     * FM:  6 (sync) + 1 (IDAM) + 4 (ID) + 2 (CRC) + 11 (gap2) +
     *      6 (sync) + 1 (DAM) + data + 2 (CRC) + gap3
     * MFM: 12 (sync) + 3 (A1) + 1 (IDAM) + 4 (ID) + 2 (CRC) + 22 (gap2) +
     *      12 (sync) + 3 (A1) + 1 (DAM) + data + 2 (CRC) + gap3
     */
    
    uint32_t overhead_per_sector = mfm ? 62 : 33;
    uint32_t gap3 = 0;
    
    /* Get typical gap3 */
    uint8_t ssize_code = 0;
    while ((128U << ssize_code) < ssize && ssize_code < 7) ssize_code++;
    
    uft_fdc_get_gaps(mfm, false, ssize_code, nsect, NULL, (uint8_t*)&gap3);
    
    /* Track overhead (gap4a + IAM + gap1) */
    uint32_t track_overhead = mfm ? 146 : 73;
    
    return track_overhead + nsect * (overhead_per_sector + ssize + gap3);
}
