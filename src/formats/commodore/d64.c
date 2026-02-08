/**
 * @file d64.c
 * @brief Commodore 1541 D64 disk image format
 * @version 3.8.0
 */
// d64.c - D64 implementation (C11)

#include "uft/floppy/uft_floppy_device.h"
#include "uft/formats/d64.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Error codes provided by uft_floppy_device.h */

/**
 * D64 Error Byte Codes (from 1541 drive controller)
 * Reference: Peter Schepers, "D64 (Electronic form of a 1541 disk)"
 *
 *  Code  1541 Err  Type   Description
 *  ----  --------  ----   -----------------------------------
 *   01      00     N/A    No error
 *   02      20     Seek   Header descriptor byte not found ($08)
 *   03      21     Seek   No SYNC sequence found
 *   04      22     Read   Data descriptor byte not found ($07)
 *   05      23     Read   Checksum error in data block
 *   06      24     Write  Write verify (on format)
 *   07      25     Write  Write verify error
 *   08      26     Write  Write protect on
 *   09      27     Seek   Checksum error in header block
 *   0B      29     Seek   Disk ID mismatch
 *   0F      74     Read   Drive not ready
 *
 * File sizes:
 *   35 track, no errors   = 174848
 *   35 track, 683 errors  = 175531
 *   40 track, no errors   = 196608
 *   40 track, 768 errors  = 197376
 */

#define D64_ERR_OK           1   /**< No error */
#define D64_ERR_HEADER_DESC  2   /**< Header descriptor byte not found */
#define D64_ERR_NO_SYNC      3   /**< No SYNC sequence found */
#define D64_ERR_DATA_DESC    4   /**< Data descriptor byte not found */
#define D64_ERR_DATA_CRC     5   /**< Checksum error in data block */
#define D64_ERR_WRITE_FMT    6   /**< Write verify (on format) */
#define D64_ERR_WRITE_VER    7   /**< Write verify error */
#define D64_ERR_WRITE_PROT   8   /**< Write protect on */
#define D64_ERR_HEADER_CRC   9   /**< Checksum error in header block */
#define D64_ERR_ID_MISMATCH  11  /**< Disk ID mismatch */
#define D64_ERR_NOT_READY    15  /**< Drive not ready */

static const char *d64_error_name(uint8_t code) {
    switch (code) {
        case 0:  return "no error (implicit)";
        case 1:  return "OK (00)";
        case 2:  return "Header block not found (20)";
        case 3:  return "No SYNC sequence (21)";
        case 4:  return "Data block not found (22)";
        case 5:  return "Data checksum error (23)";
        case 6:  return "Write verify on format (24)";
        case 7:  return "Write verify error (25)";
        case 8:  return "Write protect on (26)";
        case 9:  return "Header checksum error (27)";
        case 11: return "Disk ID mismatch (29)";
        case 15: return "Drive not ready (74)";
        default: return "Unknown error";
    }
}

/* Total sector counts */
#define D64_SECTORS_35  683
#define D64_SECTORS_40  768

typedef struct {
    FILE *fp;
    bool read_only;
    uint32_t tracks;
    bool has_errors;           /**< true if error bytes appended */
    uint32_t total_sectors;    /**< 683 or 768 */
    uint8_t *error_bytes;      /**< per-sector error codes, or NULL */
} D64Ctx;

/* sectors per track table for 1541 */
static const uint8_t spt[41] = {
    0,
    21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,
    19,19,19,19,19,19,19,
    18,18,18,18,18,18,
    17,17,17,17,17
};

static void log_msg(FloppyDevice *d, const char *m){ if(d&&d->log_callback) d->log_callback(m); }

static uint32_t lba_from_ts(uint32_t track, uint32_t sector){
    uint32_t lba=0;
    for(uint32_t t=1;t<track;t++) lba += spt[t];
    return lba + sector;
}

int uft_cbm_d64_open(FloppyDevice *dev, const char *path){
    if(!dev||!path) return UFT_EINVAL;
    D64Ctx *ctx = calloc(1,sizeof(D64Ctx));
    if(!ctx) return UFT_EIO;

    FILE *fp=fopen(path,"r+b");
    bool ro=false;
    if(!fp){ fp=fopen(path,"rb"); ro=true; }
    if(!fp){ free(ctx); return UFT_ENOENT; }

    ctx->fp=fp; ctx->read_only=ro;

    if (fseek(fp,0,SEEK_END) != 0) { /* seek error */ }
    long size=ftell(fp);
    if (fseek(fp,0,SEEK_SET) != 0) { /* seek error */ }
    if(size==174848)      { ctx->tracks=35; ctx->total_sectors=D64_SECTORS_35; ctx->has_errors=false; }
    else if(size==175531) { ctx->tracks=35; ctx->total_sectors=D64_SECTORS_35; ctx->has_errors=true; }
    else if(size==196608) { ctx->tracks=40; ctx->total_sectors=D64_SECTORS_40; ctx->has_errors=false; }
    else if(size==197376) { ctx->tracks=40; ctx->total_sectors=D64_SECTORS_40; ctx->has_errors=true; }
    else { fclose(fp); free(ctx); return UFT_EINVAL; }

    /* Read error bytes if present (appended after sector data) */
    if (ctx->has_errors) {
        ctx->error_bytes = calloc(ctx->total_sectors, 1);
        if (ctx->error_bytes) {
            long err_offset = (long)ctx->total_sectors * 256;
            if (fseek(fp, err_offset, SEEK_SET) == 0) {
                if (fread(ctx->error_bytes, 1, ctx->total_sectors, fp) != ctx->total_sectors) {
                    free(ctx->error_bytes);
                    ctx->error_bytes = NULL;
                    ctx->has_errors = false;
                }
            }
            if (fseek(fp, 0, SEEK_SET) != 0) { /* reset */ }

            /* Log error byte summary */
            if (ctx->error_bytes) {
                uint32_t err_count = 0;
                for (uint32_t i = 0; i < ctx->total_sectors; i++) {
                    if (ctx->error_bytes[i] != D64_ERR_OK && ctx->error_bytes[i] != 0)
                        err_count++;
                }
                if (err_count > 0) {
                    char msg[128];
                    snprintf(msg, sizeof(msg),
                             "D64: %u sector error(s) detected in error byte table", err_count);
                    log_msg(dev, msg);
                }
            }
        }
    } else {
        ctx->error_bytes = NULL;
    }

    dev->tracks=ctx->tracks;
    dev->heads=1;
    dev->sectors=0; /* variable */
    dev->sectorSize=256;
    dev->flux_supported=false;
    dev->internal_ctx=ctx;

    char open_msg[128];
    snprintf(open_msg, sizeof(open_msg),
             "D64 opened: %u tracks, %s%s",
             ctx->tracks,
             ctx->has_errors ? "with error bytes" : "no error bytes",
             ctx->read_only ? " (read-only)" : "");
    log_msg(dev, open_msg);
    return UFT_OK;
}

int uft_cbm_d64_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    D64Ctx *ctx=dev->internal_ctx;
    if(ctx->fp) fclose(ctx->fp);
    free(ctx->error_bytes);
    free(ctx);
    dev->internal_ctx=NULL;
    return UFT_OK;
}

int uft_cbm_d64_read_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    (void)h;
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    D64Ctx *ctx=dev->internal_ctx;
    if(t==0||t>ctx->tracks||s>=spt[t]) return UFT_EBOUNDS;

    uint32_t lba=lba_from_ts(t,s);
    uint32_t off=lba*256;
    if(fseek(ctx->fp,off,SEEK_SET)!=0) return UFT_EIO;
    if(fread(buf,1,256,ctx->fp)!=256) return UFT_EIO;
    return UFT_OK;
}

int uft_cbm_d64_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    (void)h;
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    D64Ctx *ctx=dev->internal_ctx;
    if(ctx->read_only) return UFT_ENOTSUP;
    if(t==0||t>ctx->tracks||s>=spt[t]) return UFT_EBOUNDS;

    uint32_t lba=lba_from_ts(t,s);
    uint32_t off=lba*256;
    if(fseek(ctx->fp,off,SEEK_SET)!=0) return UFT_EIO;
    if(fwrite(buf,1,256,ctx->fp)!=256) return UFT_EIO;
    fflush(ctx->fp);
    return UFT_OK;
}

int uft_cbm_d64_analyze_protection(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    D64Ctx *ctx=dev->internal_ctx;

    if (!ctx->has_errors || !ctx->error_bytes) {
        log_msg(dev,"Analyzer(D64): No error bytes present. Sector dump only; "
                     "GCR timing, weak bits and long tracks lost. Use G64/SCP for protection.");
        return UFT_OK;
    }

    /* Report per-track error summary (Schepers D64.TXT reference) */
    log_msg(dev,"Analyzer(D64): Error byte analysis (1541 controller codes):");
    uint32_t lba = 0;
    uint32_t total_errors = 0;
    for (uint32_t t = 1; t <= ctx->tracks; t++) {
        uint32_t track_errors = 0;
        for (uint32_t s = 0; s < spt[t]; s++) {
            uint8_t code = ctx->error_bytes[lba + s];
            if (code != D64_ERR_OK && code != 0) track_errors++;
        }
        if (track_errors > 0) {
            char msg[256];
            snprintf(msg, sizeof(msg), "  Track %2u: %u error(s)", t, track_errors);
            /* Detail first few errors */
            int detail = 0;
            for (uint32_t s = 0; s < spt[t] && detail < 4; s++) {
                uint8_t code = ctx->error_bytes[lba + s];
                if (code != D64_ERR_OK && code != 0) {
                    char detail_msg[128];
                    snprintf(detail_msg, sizeof(detail_msg),
                             "    T%u/S%u: code %u - %s", t, s, code, d64_error_name(code));
                    log_msg(dev, detail_msg);
                    detail++;
                }
            }
            total_errors += track_errors;
        }
        lba += spt[t];
    }

    char summary[128];
    snprintf(summary, sizeof(summary),
             "Analyzer(D64): %u total sector error(s) across %u tracks",
             total_errors, ctx->tracks);
    log_msg(dev, summary);

    if (total_errors > 0) {
        log_msg(dev,"Analyzer(D64): Error bytes indicate copy protection or disk damage. "
                     "Common protection: sectors with code 3 (no sync), 5 (data CRC), 9 (header CRC).");
    }
    return UFT_OK;
}
