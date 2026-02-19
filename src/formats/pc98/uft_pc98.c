/**
 * @file uft_pc98.c
 * @brief NEC PC-98 Format Support Implementation
 * @version 3.6.0
 * 
 * Implements PC-98 disk format support including:
 * - Multiple geometries (2DD 640KB, 2HD 1.2MB, 2HC, 2HQ 1.44MB)
 * - Shift-JIS encoding for Japanese labels
 * - FDI-98 (Anex86) container format
 * - Auto-detection with confidence scoring
 * 
 * SPDX-License-Identifier: MIT
 */

#include "uft/uft_safe_io.h"
#include "uft/formats/uft_pc98.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/*============================================================================
 * Geometry Tables
 *============================================================================*/

static const uft_pc98_geometry_t g_pc98_geometries[] = {
    [UFT_PC98_GEOM_UNKNOWN] = {
        .type = UFT_PC98_GEOM_UNKNOWN,
        .tracks = 0, .heads = 0, .sectors = 0, .sector_size = 0,
        .total_bytes = 0, .name = "Unknown", .media_byte = 0x00
    },
    [UFT_PC98_GEOM_2DD_640] = {
        .type = UFT_PC98_GEOM_2DD_640,
        .tracks = 80, .heads = 2, .sectors = 8, .sector_size = 512,
        .total_bytes = 655360, .name = "2DD 640KB", .media_byte = 0xFE
    },
    [UFT_PC98_GEOM_2HD_1232] = {
        .type = UFT_PC98_GEOM_2HD_1232,
        .tracks = 77, .heads = 2, .sectors = 8, .sector_size = 1024,
        .total_bytes = 1261568, .name = "2HD 1.2MB (Native)", .media_byte = 0xFE
    },
    [UFT_PC98_GEOM_2HC_1200] = {
        .type = UFT_PC98_GEOM_2HC_1200,
        .tracks = 80, .heads = 2, .sectors = 15, .sector_size = 512,
        .total_bytes = 1228800, .name = "2HC 1.2MB (IBM)", .media_byte = 0xF9
    },
    [UFT_PC98_GEOM_2HQ_1440] = {
        .type = UFT_PC98_GEOM_2HQ_1440,
        .tracks = 80, .heads = 2, .sectors = 18, .sector_size = 512,
        .total_bytes = 1474560, .name = "2HQ 1.44MB", .media_byte = 0xF0
    },
    [UFT_PC98_GEOM_2DD_320] = {
        .type = UFT_PC98_GEOM_2DD_320,
        .tracks = 40, .heads = 2, .sectors = 8, .sector_size = 512,
        .total_bytes = 327680, .name = "2DD 320KB", .media_byte = 0xFF
    },
    [UFT_PC98_GEOM_2D_360] = {
        .type = UFT_PC98_GEOM_2D_360,
        .tracks = 40, .heads = 2, .sectors = 9, .sector_size = 512,
        .total_bytes = 368640, .name = "2D 360KB", .media_byte = 0xFD
    }
};

/*============================================================================
 * Shift-JIS Conversion Tables (subset)
 *============================================================================*/

/* Half-width Katakana (0xA1-0xDF) to Unicode */
static const uint16_t g_hwkatakana_to_unicode[] = {
    0x3002, 0x300C, 0x300D, 0x3001, 0x30FB, 0x30F2, 0x30A1, 0x30A3, /* A1-A8 */
    0x30A5, 0x30A7, 0x30A9, 0x30E3, 0x30E5, 0x30E7, 0x30C3, 0x30FC, /* A9-B0 */
    0x30A2, 0x30A4, 0x30A6, 0x30A8, 0x30AA, 0x30AB, 0x30AD, 0x30AF, /* B1-B8 */
    0x30B1, 0x30B3, 0x30B5, 0x30B7, 0x30B9, 0x30BB, 0x30BD, 0x30BF, /* B9-C0 */
    0x30C1, 0x30C4, 0x30C6, 0x30C8, 0x30CA, 0x30CB, 0x30CC, 0x30CD, /* C1-C8 */
    0x30CE, 0x30CF, 0x30D2, 0x30D5, 0x30D8, 0x30DB, 0x30DE, 0x30DF, /* C9-D0 */
    0x30E0, 0x30E1, 0x30E2, 0x30E4, 0x30E6, 0x30E8, 0x30E9, 0x30EA, /* D1-D8 */
    0x30EB, 0x30EC, 0x30ED, 0x30EF, 0x30F3, 0x309B, 0x309C          /* D9-DF */
};

/*============================================================================
 * Geometry API
 *============================================================================*/

const uft_pc98_geometry_t* uft_pc98_get_geometry(uft_pc98_geometry_type_t type) {
    if (type < 0 || type >= UFT_PC98_GEOM_COUNT) {
        return &g_pc98_geometries[UFT_PC98_GEOM_UNKNOWN];
    }
    return &g_pc98_geometries[type];
}

uft_pc98_geometry_type_t uft_pc98_detect_geometry_by_size(uint64_t file_size, uint8_t* confidence) {
    uint8_t conf = 0;
    uft_pc98_geometry_type_t result = UFT_PC98_GEOM_UNKNOWN;
    
    /* Exact matches get high confidence */
    for (int i = 1; i < UFT_PC98_GEOM_COUNT; i++) {
        if (file_size == g_pc98_geometries[i].total_bytes) {
            conf = 95;
            result = (uft_pc98_geometry_type_t)i;
            break;
        }
    }
    
    /* Check for FDI-98 header offset */
    if (result == UFT_PC98_GEOM_UNKNOWN) {
        for (int i = 1; i < UFT_PC98_GEOM_COUNT; i++) {
            if (file_size == g_pc98_geometries[i].total_bytes + UFT_FDI98_HEADER_SIZE) {
                conf = 90;
                result = (uft_pc98_geometry_type_t)i;
                break;
            }
        }
    }
    
    /* Approximate matches */
    if (result == UFT_PC98_GEOM_UNKNOWN) {
        if (file_size >= 1260000 && file_size <= 1270000) {
            result = UFT_PC98_GEOM_2HD_1232;
            conf = 70;
        } else if (file_size >= 650000 && file_size <= 660000) {
            result = UFT_PC98_GEOM_2DD_640;
            conf = 70;
        } else if (file_size >= 1470000 && file_size <= 1480000) {
            result = UFT_PC98_GEOM_2HQ_1440;
            conf = 70;
        }
    }
    
    if (confidence) *confidence = conf;
    return result;
}

uft_pc98_rc_t uft_pc98_validate_geometry(uint16_t tracks, uint8_t heads, 
                                          uint8_t sectors, uint16_t sector_size) {
    if (tracks == 0 || tracks > 85) return UFT_PC98_ERR_GEOMETRY;
    if (heads == 0 || heads > 2) return UFT_PC98_ERR_GEOMETRY;
    if (sectors == 0 || sectors > 26) return UFT_PC98_ERR_GEOMETRY;
    if (sector_size != 128 && sector_size != 256 && 
        sector_size != 512 && sector_size != 1024) {
        return UFT_PC98_ERR_GEOMETRY;
    }
    return UFT_PC98_SUCCESS;
}

/*============================================================================
 * Shift-JIS Functions
 *============================================================================*/

/* Encode single Unicode codepoint to UTF-8 */
static size_t unicode_to_utf8(uint32_t cp, char* buf) {
    if (cp < 0x80) {
        buf[0] = (char)cp;
        return 1;
    } else if (cp < 0x800) {
        buf[0] = (char)(0xC0 | (cp >> 6));
        buf[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    } else if (cp < 0x10000) {
        buf[0] = (char)(0xE0 | (cp >> 12));
        buf[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        buf[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    } else {
        buf[0] = (char)(0xF0 | (cp >> 18));
        buf[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        buf[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        buf[3] = (char)(0x80 | (cp & 0x3F));
        return 4;
    }
}

/* Convert Shift-JIS double-byte to Unicode (simplified JIS X 0208 mapping) */
static uint32_t sjis_to_unicode(uint8_t high, uint8_t low) {
    /* Row and cell calculation for JIS X 0208 */
    int row, cell;
    
    if (high >= 0x81 && high <= 0x9F) {
        row = (high - 0x81) * 2;
    } else if (high >= 0xE0 && high <= 0xEF) {
        row = (high - 0xE0 + 0x1F) * 2;
    } else {
        return 0xFFFD; /* Replacement character */
    }
    
    if (low >= 0x40 && low <= 0x7E) {
        cell = low - 0x40;
    } else if (low >= 0x80 && low <= 0xFC) {
        cell = low - 0x41;
        if (cell >= 0x3F) row++;
    } else {
        return 0xFFFD;
    }
    
    /* Map to Unicode - simplified, covers common characters */
    /* Full mapping would require ~7000 entry table */
    uint32_t jis = (row + 0x21) * 256 + (cell % 94) + 0x21;
    
    /* Basic mapping for common ranges */
    if (jis >= 0x2421 && jis <= 0x2473) {
        /* Hiragana */
        return 0x3041 + (jis - 0x2421);
    } else if (jis >= 0x2521 && jis <= 0x2576) {
        /* Katakana */
        return 0x30A1 + (jis - 0x2521);
    } else if (jis >= 0x2330 && jis <= 0x2339) {
        /* Fullwidth digits */
        return 0xFF10 + (jis - 0x2330);
    } else if (jis >= 0x2341 && jis <= 0x235A) {
        /* Fullwidth uppercase */
        return 0xFF21 + (jis - 0x2341);
    } else if (jis >= 0x2361 && jis <= 0x237A) {
        /* Fullwidth lowercase */
        return 0xFF41 + (jis - 0x2361);
    }
    
    /* Unknown character - return replacement */
    return 0xFFFD;
}

uft_pc98_rc_t uft_pc98_sjis_to_utf8(const uint8_t* sjis_data, size_t sjis_len, 
                                    uft_sjis_result_t* result) {
    if (!sjis_data || !result) return UFT_PC98_ERR_ARG;
    
    /* Allocate worst case: 4 bytes UTF-8 per input byte */
    result->utf8_str = (char*)calloc(sjis_len, 4) ? calloc(sjis_len, 4) : malloc(1);
    if (!result->utf8_str) return UFT_PC98_ERR_NOMEM;
    
    result->utf8_len = 0;
    result->errors = 0;
    result->has_fullwidth = false;
    
    size_t i = 0;
    while (i < sjis_len) {
        uint8_t c = sjis_data[i];
        
        if (c == 0) {
            /* End of string */
            break;
        } else if (c < 0x80) {
            /* ASCII */
            result->utf8_str[result->utf8_len++] = (char)c;
            i++;
        } else if (c >= 0xA1 && c <= 0xDF) {
            /* Half-width Katakana */
            uint16_t unicode = g_hwkatakana_to_unicode[c - 0xA1];
            result->utf8_len += unicode_to_utf8(unicode, 
                &result->utf8_str[result->utf8_len]);
            i++;
        } else if ((c >= 0x81 && c <= 0x9F) || (c >= 0xE0 && c <= 0xEF)) {
            /* Double-byte character */
            if (i + 1 >= sjis_len) {
                result->errors++;
                break;
            }
            uint8_t c2 = sjis_data[i + 1];
            uint32_t unicode = sjis_to_unicode(c, c2);
            if (unicode == 0xFFFD) {
                result->errors++;
            }
            result->utf8_len += unicode_to_utf8(unicode,
                &result->utf8_str[result->utf8_len]);
            result->has_fullwidth = true;
            i += 2;
        } else {
            /* Invalid byte */
            result->errors++;
            i++;
        }
    }
    
    result->utf8_str[result->utf8_len] = '\0';
    return UFT_PC98_SUCCESS;
}

uft_pc98_rc_t uft_pc98_decode_disk_label(const uint8_t* raw_label, size_t raw_len,
                                          char* utf8_label, size_t utf8_capacity) {
    if (!raw_label || !utf8_label || utf8_capacity < 1) {
        return UFT_PC98_ERR_ARG;
    }
    
    uft_sjis_result_t result = {0};
    uft_pc98_rc_t rc = uft_pc98_sjis_to_utf8(raw_label, raw_len, &result);
    
    if (rc == UFT_PC98_SUCCESS && result.utf8_str) {
        size_t copy_len = result.utf8_len < utf8_capacity - 1 ? 
                          result.utf8_len : utf8_capacity - 1;
        memcpy(utf8_label, result.utf8_str, copy_len);
        utf8_label[copy_len] = '\0';
        free(result.utf8_str);
    } else {
        utf8_label[0] = '\0';
    }
    
    return rc;
}

bool uft_pc98_is_valid_sjis(const uint8_t* data, size_t len, uint8_t* confidence) {
    if (!data || len == 0) {
        if (confidence) *confidence = 0;
        return false;
    }
    
    uint32_t valid = 0, invalid = 0;
    size_t i = 0;
    
    while (i < len && data[i] != 0) {
        uint8_t c = data[i];
        
        if (c < 0x80) {
            /* ASCII */
            if (c >= 0x20 && c < 0x7F) valid++;
            else if (c == 0x09 || c == 0x0A || c == 0x0D) valid++;
            else invalid++;
            i++;
        } else if (c >= 0xA1 && c <= 0xDF) {
            /* Half-width Katakana */
            valid++;
            i++;
        } else if ((c >= 0x81 && c <= 0x9F) || (c >= 0xE0 && c <= 0xEF)) {
            /* Double-byte lead */
            if (i + 1 < len) {
                uint8_t c2 = data[i + 1];
                if ((c2 >= 0x40 && c2 <= 0x7E) || (c2 >= 0x80 && c2 <= 0xFC)) {
                    valid++;
                } else {
                    invalid++;
                }
                i += 2;
            } else {
                invalid++;
                i++;
            }
        } else {
            invalid++;
            i++;
        }
    }
    
    uint32_t total = valid + invalid;
    if (total == 0) {
        if (confidence) *confidence = 0;
        return false;
    }
    
    uint8_t conf = (uint8_t)((valid * 100) / total);
    if (confidence) *confidence = conf;
    return conf >= 80;
}

/*============================================================================
 * FDI-98 Format Functions
 *============================================================================*/

bool uft_fdi98_detect(const uint8_t* buffer, size_t size, uint8_t* confidence) {
    if (!buffer || size < sizeof(uft_fdi98_header_t)) {
        if (confidence) *confidence = 0;
        return false;
    }
    
    const uft_fdi98_header_t* hdr = (const uft_fdi98_header_t*)buffer;
    uint8_t conf = 0;
    
    /* Check header size field */
    if (hdr->header_size == UFT_FDI98_HEADER_SIZE) {
        conf += 40;
    } else if (hdr->header_size == 256 || hdr->header_size == 512) {
        conf += 20; /* Some variants use smaller headers */
    }
    
    /* Check FDD type */
    uint32_t fdd_type = hdr->fdd_type & 0xF0;
    if (fdd_type == UFT_FDI98_TYPE_2DD_640 ||
        fdd_type == UFT_FDI98_TYPE_2HD_1232 ||
        fdd_type == UFT_FDI98_TYPE_2HC_1200 ||
        fdd_type == UFT_FDI98_TYPE_2HQ_1440) {
        conf += 30;
    }
    
    /* Check geometry values */
    if (hdr->tracks > 0 && hdr->tracks <= 85 &&
        hdr->heads > 0 && hdr->heads <= 2 &&
        hdr->sectors_per_track > 0 && hdr->sectors_per_track <= 26) {
        conf += 20;
    }
    
    /* Check sector size */
    if (hdr->sector_size == 128 || hdr->sector_size == 256 ||
        hdr->sector_size == 512 || hdr->sector_size == 1024) {
        conf += 10;
    }
    
    if (confidence) *confidence = conf;
    return conf >= 60;
}

uft_pc98_rc_t uft_fdi98_open(uft_fdi98_ctx_t* ctx, const char* path, bool writable) {
    if (!ctx || !path) return UFT_PC98_ERR_ARG;
    
    memset(ctx, 0, sizeof(*ctx));
    
    FILE* fp = fopen(path, "rb");
    if (!fp) return UFT_PC98_ERR_IO;
    
    /* Get file size */
    if (fseek(fp, 0, SEEK_END) != 0) { /* I/O error */ }
    ctx->file_size = (uint64_t)ftell(fp);
    if (fseek(fp, 0, SEEK_SET) != 0) { /* I/O error */ }
    
    /* Read header */
    if (fread(&ctx->header, sizeof(ctx->header), 1, fp) != 1) {
        fclose(fp);
        return UFT_PC98_ERR_IO;
    }
    fclose(fp);
    
    /* Validate */
    uint8_t conf;
    if (!uft_fdi98_detect((uint8_t*)&ctx->header, sizeof(ctx->header), &conf)) {
        return UFT_PC98_ERR_FORMAT;
    }
    
    /* Store path */
    ctx->path = strdup(path);
    if (!ctx->path) return UFT_PC98_ERR_NOMEM;
    
    ctx->writable = writable;
    ctx->data_offset = ctx->header.header_size;
    
    /* Determine geometry */
    ctx->geometry.type = UFT_PC98_GEOM_UNKNOWN;
    ctx->geometry.tracks = (uint16_t)ctx->header.tracks;
    ctx->geometry.heads = (uint8_t)ctx->header.heads;
    ctx->geometry.sectors = (uint8_t)ctx->header.sectors_per_track;
    ctx->geometry.sector_size = (uint16_t)ctx->header.sector_size;
    ctx->geometry.total_bytes = ctx->geometry.tracks * ctx->geometry.heads *
                                 ctx->geometry.sectors * ctx->geometry.sector_size;
    
    /* Match to known geometry */
    uint32_t fdd_type = ctx->header.fdd_type & 0xF0;
    switch (fdd_type) {
        case UFT_FDI98_TYPE_2DD_640:
            ctx->geometry.type = UFT_PC98_GEOM_2DD_640;
            break;
        case UFT_FDI98_TYPE_2HD_1232:
            ctx->geometry.type = UFT_PC98_GEOM_2HD_1232;
            break;
        case UFT_FDI98_TYPE_2HC_1200:
            ctx->geometry.type = UFT_PC98_GEOM_2HC_1200;
            break;
        case UFT_FDI98_TYPE_2HQ_1440:
            ctx->geometry.type = UFT_PC98_GEOM_2HQ_1440;
            break;
    }
    
    return UFT_PC98_SUCCESS;
}

uft_pc98_rc_t uft_fdi98_read_sector(uft_fdi98_ctx_t* ctx, uint16_t track, 
                                     uint8_t head, uint8_t sector,
                                     uint8_t* buffer, size_t buffer_size) {
    if (!ctx || !ctx->path || !buffer) return UFT_PC98_ERR_ARG;
    
    /* Validate parameters */
    if (track >= ctx->geometry.tracks || 
        head >= ctx->geometry.heads ||
        sector == 0 || sector > ctx->geometry.sectors) {
        return UFT_PC98_ERR_RANGE;
    }
    
    if (buffer_size < ctx->geometry.sector_size) {
        return UFT_PC98_ERR_ARG;
    }
    
    /* Calculate offset */
    uint32_t linear_sector = (track * ctx->geometry.heads + head) * 
                             ctx->geometry.sectors + (sector - 1);
    uint64_t offset = ctx->data_offset + 
                      (uint64_t)linear_sector * ctx->geometry.sector_size;
    
    FILE* fp = fopen(ctx->path, "rb");
    if (!fp) return UFT_PC98_ERR_IO;
    
    if (fseek(fp, (long)offset, SEEK_SET) != 0) {
        fclose(fp);
        return UFT_PC98_ERR_IO;
    }
    
    if (fread(buffer, ctx->geometry.sector_size, 1, fp) != 1) {
        fclose(fp);
        return UFT_PC98_ERR_IO;
    }
    
    fclose(fp);
    return UFT_PC98_SUCCESS;
}

uft_pc98_rc_t uft_fdi98_write_sector(uft_fdi98_ctx_t* ctx, uint16_t track,
                                      uint8_t head, uint8_t sector,
                                      const uint8_t* data, size_t data_size) {
    if (!ctx || !ctx->path || !data) return UFT_PC98_ERR_ARG;
    if (!ctx->writable) return UFT_PC98_ERR_READONLY;
    
    /* Validate parameters */
    if (track >= ctx->geometry.tracks ||
        head >= ctx->geometry.heads ||
        sector == 0 || sector > ctx->geometry.sectors) {
        return UFT_PC98_ERR_RANGE;
    }
    
    if (data_size < ctx->geometry.sector_size) {
        return UFT_PC98_ERR_ARG;
    }
    
    /* Calculate offset */
    uint32_t linear_sector = (track * ctx->geometry.heads + head) *
                             ctx->geometry.sectors + (sector - 1);
    uint64_t offset = ctx->data_offset +
                      (uint64_t)linear_sector * ctx->geometry.sector_size;
    
    FILE* fp = fopen(ctx->path, "r+b");
    if (!fp) return UFT_PC98_ERR_IO;
    
    if (fseek(fp, (long)offset, SEEK_SET) != 0) {
        fclose(fp);
        return UFT_PC98_ERR_IO;
    }
    
    if (fwrite(data, ctx->geometry.sector_size, 1, fp) != 1) {
        fclose(fp);
        return UFT_PC98_ERR_IO;
    }
    
    fclose(fp);
    return UFT_PC98_SUCCESS;
}

void uft_fdi98_close(uft_fdi98_ctx_t* ctx) {
    if (ctx) {
        free(ctx->path);
        memset(ctx, 0, sizeof(*ctx));
    }
}

uft_pc98_rc_t uft_fdi98_to_raw(uft_fdi98_ctx_t* ctx, const char* output_path) {
    if (!ctx || !ctx->path || !output_path) return UFT_PC98_ERR_ARG;
    
    FILE* fin = fopen(ctx->path, "rb");
    if (!fin) return UFT_PC98_ERR_IO;
    
    FILE* fout = fopen(output_path, "wb");
    if (!fout) {
        fclose(fin);
        return UFT_PC98_ERR_IO;
    }
    
    /* Skip header */
    if (fseek(fin, (long)ctx->data_offset, SEEK_SET) != 0) { /* I/O error */ }
    
    /* Copy data */
    uint8_t buffer[4096];
    size_t remaining = ctx->geometry.total_bytes;
    
    while (remaining > 0) {
        size_t chunk = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
        size_t read = fread(buffer, 1, chunk, fin);
        if (read == 0) break;
        
        if (fwrite(buffer, 1, read, fout) != read) {
            fclose(fin);
            fclose(fout);
            return UFT_PC98_ERR_IO;
        }
        remaining -= read;
    }
    
    fclose(fin);
    fclose(fout);
    return UFT_PC98_SUCCESS;
}

uft_pc98_rc_t uft_fdi98_create_from_raw(const char* raw_path, const char* fdi98_path,
                                         uft_pc98_geometry_type_t geometry) {
    if (!raw_path || !fdi98_path) return UFT_PC98_ERR_ARG;
    if (geometry <= UFT_PC98_GEOM_UNKNOWN || geometry >= UFT_PC98_GEOM_COUNT) {
        return UFT_PC98_ERR_GEOMETRY;
    }
    
    const uft_pc98_geometry_t* geom = uft_pc98_get_geometry(geometry);
    
    FILE* fin = fopen(raw_path, "rb");
    if (!fin) return UFT_PC98_ERR_IO;
    
    FILE* fout = fopen(fdi98_path, "wb");
    if (!fout) {
        fclose(fin);
        return UFT_PC98_ERR_IO;
    }
    
    /* Create header */
    uft_fdi98_header_t hdr = {0};
    hdr.header_size = UFT_FDI98_HEADER_SIZE;
    hdr.sector_size = geom->sector_size;
    hdr.sectors_per_track = geom->sectors;
    hdr.heads = geom->heads;
    hdr.tracks = geom->tracks;
    
    /* Set FDD type */
    switch (geometry) {
        case UFT_PC98_GEOM_2DD_640:
            hdr.fdd_type = UFT_FDI98_TYPE_2DD_640;
            break;
        case UFT_PC98_GEOM_2HD_1232:
            hdr.fdd_type = UFT_FDI98_TYPE_2HD_1232;
            break;
        case UFT_PC98_GEOM_2HC_1200:
            hdr.fdd_type = UFT_FDI98_TYPE_2HC_1200;
            break;
        case UFT_PC98_GEOM_2HQ_1440:
            hdr.fdd_type = UFT_FDI98_TYPE_2HQ_1440;
            break;
        default:
            hdr.fdd_type = 0;
    }
    
    /* Write header with padding */
    uint8_t header_buf[UFT_FDI98_HEADER_SIZE] = {0};
    memcpy(header_buf, &hdr, sizeof(hdr));
    
    if (fwrite(header_buf, UFT_FDI98_HEADER_SIZE, 1, fout) != 1) {
        fclose(fin);
        fclose(fout);
        return UFT_PC98_ERR_IO;
    }
    
    /* Copy raw data */
    uint8_t buffer[4096];
    size_t read;
    while ((read = fread(buffer, 1, sizeof(buffer), fin)) > 0) {
        if (fwrite(buffer, 1, read, fout) != read) {
            fclose(fin);
            fclose(fout);
            return UFT_PC98_ERR_IO;
        }
    }
    
    fclose(fin);
    fclose(fout);
    return UFT_PC98_SUCCESS;
}

/*============================================================================
 * Auto-Detection and Analysis
 *============================================================================*/

uft_pc98_rc_t uft_pc98_detect(const char* path, uft_pc98_detect_result_t* result) {
    if (!path || !result) return UFT_PC98_ERR_ARG;
    
    memset(result, 0, sizeof(*result));
    
    FILE* fp = fopen(path, "rb");
    if (!fp) return UFT_PC98_ERR_IO;
    
    /* Get file size */
    if (fseek(fp, 0, SEEK_END) != 0) { /* I/O error */ }
    uint64_t file_size = (uint64_t)ftell(fp);
    if (fseek(fp, 0, SEEK_SET) != 0) { /* I/O error */ }
    
    /* Read header for format detection */
    uint8_t header[4096];
    size_t header_read = fread(header, 1, sizeof(header), fp);
    fclose(fp);
    
    if (header_read < 16) {
        return UFT_PC98_ERR_FORMAT;
    }
    
    /* Try FDI-98 detection */
    uint8_t fdi_conf = 0;
    if (uft_fdi98_detect(header, header_read, &fdi_conf) && fdi_conf >= 60) {
        result->format = UFT_PC98_FMT_FDI98;
        result->format_confidence = fdi_conf;
        
        /* Get geometry from header */
        const uft_fdi98_header_t* hdr = (const uft_fdi98_header_t*)header;
        uint32_t fdd_type = hdr->fdd_type & 0xF0;
        switch (fdd_type) {
            case UFT_FDI98_TYPE_2DD_640:
                result->geometry = UFT_PC98_GEOM_2DD_640;
                break;
            case UFT_FDI98_TYPE_2HD_1232:
                result->geometry = UFT_PC98_GEOM_2HD_1232;
                break;
            case UFT_FDI98_TYPE_2HC_1200:
                result->geometry = UFT_PC98_GEOM_2HC_1200;
                break;
            case UFT_FDI98_TYPE_2HQ_1440:
                result->geometry = UFT_PC98_GEOM_2HQ_1440;
                break;
            default:
                result->geometry = UFT_PC98_GEOM_UNKNOWN;
        }
        result->geometry_confidence = fdi_conf;
    }
    
    /* Try D88 detection */
    else if (header_read >= 0x2B0 && 
             memcmp(header + 0x1A, "\x00\x00\x00\x00", 4) != 0) {
        /* D88 has track offset table starting at 0x20 */
        uint32_t first_offset = header[0x20] | (header[0x21] << 8) |
                                ((uint32_t)header[0x22] << 16) | ((uint32_t)header[0x23] << 24);
        if (first_offset >= 0x2B0 && first_offset < file_size) {
            result->format = UFT_PC98_FMT_D88;
            result->format_confidence = 75;
            
            /* Check disk name for Shift-JIS */
            if (uft_pc98_is_valid_sjis(header, 17, NULL)) {
                result->has_sjis_label = true;
                uft_pc98_decode_disk_label(header, 17, result->label_utf8, 
                                            sizeof(result->label_utf8));
            }
            
            /* Geometry from file size */
            result->geometry = uft_pc98_detect_geometry_by_size(
                file_size, &result->geometry_confidence);
        }
    }
    
    /* Try raw image by size */
    if (result->format == UFT_PC98_FMT_UNKNOWN) {
        uint8_t size_conf = 0;
        result->geometry = uft_pc98_detect_geometry_by_size(file_size, &size_conf);
        if (result->geometry != UFT_PC98_GEOM_UNKNOWN && size_conf >= 70) {
            result->format = UFT_PC98_FMT_RAW;
            result->format_confidence = size_conf;
            result->geometry_confidence = size_conf;
        }
    }
    
    return UFT_PC98_SUCCESS;
}

uft_pc98_rc_t uft_pc98_analyze(const char* path, uft_pc98_report_t* report) {
    if (!path || !report) return UFT_PC98_ERR_ARG;
    
    memset(report, 0, sizeof(*report));
    
    /* First detect format */
    uft_pc98_detect_result_t detect;
    uft_pc98_rc_t rc = uft_pc98_detect(path, &detect);
    if (rc != UFT_PC98_SUCCESS) return rc;
    
    report->format = detect.format;
    report->geometry = *uft_pc98_get_geometry(detect.geometry);
    
    if (detect.has_sjis_label) {
        strncpy(report->label_utf8, detect.label_utf8, sizeof(report->label_utf8) - 1);
        report->label_utf8[sizeof(report->label_utf8) - 1] = '\0';
    }
    
    /* Open file for analysis */
    FILE* fp = fopen(path, "rb");
    if (!fp) return UFT_PC98_ERR_IO;
    
    /* Skip header if FDI-98 */
    long data_offset = 0;
    if (report->format == UFT_PC98_FMT_FDI98) {
        data_offset = UFT_FDI98_HEADER_SIZE;
    }
    
    /* Read boot sector */
    uint8_t boot_sector[1024];
    if (fseek(fp, data_offset, SEEK_SET) != 0) { /* I/O error */ }
    size_t boot_read = fread(boot_sector, 1, report->geometry.sector_size, fp);
    
    if (boot_read >= 512) {
        report->has_boot_sector = true;
        
        /* Check for FAT filesystem */
        if (boot_sector[510] == 0x55 && boot_sector[511] == 0xAA) {
            report->is_bootable = true;
        }
        
        /* Check FAT BPB */
        if (boot_sector[0] == 0xEB || boot_sector[0] == 0xE9) {
            uint16_t bytes_per_sector = boot_sector[11] | (boot_sector[12] << 8);
            if (bytes_per_sector == 512 || bytes_per_sector == 1024) {
                report->has_fat = true;
                strncpy(report->filesystem, "FAT12/16", sizeof(report->filesystem) - 1);
            }
        }
        
        /* Check for PC-98 IPL */
        if (memcmp(boot_sector, "\xEB\x76\x90", 3) == 0 ||
            memcmp(boot_sector, "\xE9", 1) == 0) {
            strncpy(report->filesystem, "PC-98 DOS", sizeof(report->filesystem) - 1);
        }
    }
    
    /* Calculate sector counts */
    report->total_sectors = report->geometry.tracks * report->geometry.heads *
                            report->geometry.sectors;
    report->readable_sectors = report->total_sectors; /* Assume all readable */
    
    fclose(fp);
    return UFT_PC98_SUCCESS;
}

int uft_pc98_report_to_json(const uft_pc98_report_t* report, 
                             char* json_out, size_t json_capacity) {
    if (!report || !json_out || json_capacity < 256) return -1;
    
    static const char* format_names[] = {
        "Unknown", "D88", "FDI-98", "NFD", "HDM", "RAW", "DIM", "FDD"
    };
    
    const char* fmt_name = (report->format < 8) ? format_names[report->format] : "Unknown";
    
    return snprintf(json_out, json_capacity,
        "{\n"
        "  \"format\": \"%s\",\n"
        "  \"geometry\": {\n"
        "    \"type\": \"%s\",\n"
        "    \"tracks\": %u,\n"
        "    \"heads\": %u,\n"
        "    \"sectors\": %u,\n"
        "    \"sector_size\": %u,\n"
        "    \"total_bytes\": %u\n"
        "  },\n"
        "  \"label\": \"%s\",\n"
        "  \"total_sectors\": %u,\n"
        "  \"readable_sectors\": %u,\n"
        "  \"error_sectors\": %u,\n"
        "  \"has_boot_sector\": %s,\n"
        "  \"is_bootable\": %s,\n"
        "  \"has_fat\": %s,\n"
        "  \"filesystem\": \"%s\"\n"
        "}",
        fmt_name,
        report->geometry.name ? report->geometry.name : "Unknown",
        report->geometry.tracks,
        report->geometry.heads,
        report->geometry.sectors,
        report->geometry.sector_size,
        report->geometry.total_bytes,
        report->label_utf8,
        report->total_sectors,
        report->readable_sectors,
        report->error_sectors,
        report->has_boot_sector ? "true" : "false",
        report->is_bootable ? "true" : "false",
        report->has_fat ? "true" : "false",
        report->filesystem
    );
}

int uft_pc98_report_to_markdown(const uft_pc98_report_t* report,
                                 char* md_out, size_t md_capacity) {
    if (!report || !md_out || md_capacity < 512) return -1;
    
    static const char* format_names[] = {
        "Unknown", "D88", "FDI-98", "NFD", "HDM", "RAW", "DIM", "FDD"
    };
    
    const char* fmt_name = (report->format < 8) ? format_names[report->format] : "Unknown";
    
    return snprintf(md_out, md_capacity,
        "# PC-98 Disk Analysis Report\n\n"
        "## Format Information\n"
        "- **Container**: %s\n"
        "- **Geometry**: %s\n"
        "- **Tracks**: %u\n"
        "- **Heads**: %u\n"
        "- **Sectors/Track**: %u\n"
        "- **Sector Size**: %u bytes\n"
        "- **Total Size**: %u bytes\n\n"
        "## Label\n"
        "- **Disk Label**: %s\n\n"
        "## Sector Statistics\n"
        "| Metric | Value |\n"
        "|--------|-------|\n"
        "| Total Sectors | %u |\n"
        "| Readable | %u |\n"
        "| Errors | %u |\n\n"
        "## Boot Information\n"
        "- **Has Boot Sector**: %s\n"
        "- **Bootable**: %s\n"
        "- **FAT Filesystem**: %s\n"
        "- **Detected FS**: %s\n",
        fmt_name,
        report->geometry.name ? report->geometry.name : "Unknown",
        report->geometry.tracks,
        report->geometry.heads,
        report->geometry.sectors,
        report->geometry.sector_size,
        report->geometry.total_bytes,
        report->label_utf8[0] ? report->label_utf8 : "(none)",
        report->total_sectors,
        report->readable_sectors,
        report->error_sectors,
        report->has_boot_sector ? "Yes" : "No",
        report->is_bootable ? "Yes" : "No",
        report->has_fat ? "Yes" : "No",
        report->filesystem[0] ? report->filesystem : "Unknown"
    );
}

/*============================================================================
 * Format Conversion
 *============================================================================*/

uft_pc98_rc_t uft_pc98_convert(const char* input_path, const char* output_path,
                                uft_pc98_format_t output_format) {
    if (!input_path || !output_path) return UFT_PC98_ERR_ARG;
    
    /* Detect input format */
    uft_pc98_detect_result_t detect;
    uft_pc98_rc_t rc = uft_pc98_detect(input_path, &detect);
    if (rc != UFT_PC98_SUCCESS) return rc;
    
    if (detect.format == UFT_PC98_FMT_UNKNOWN) {
        return UFT_PC98_ERR_FORMAT;
    }
    
    /* Handle conversion */
    if (detect.format == UFT_PC98_FMT_FDI98 && output_format == UFT_PC98_FMT_RAW) {
        /* FDI-98 to RAW */
        uft_fdi98_ctx_t ctx;
        rc = uft_fdi98_open(&ctx, input_path, false);
        if (rc != UFT_PC98_SUCCESS) return rc;
        
        rc = uft_fdi98_to_raw(&ctx, output_path);
        uft_fdi98_close(&ctx);
        return rc;
    }
    else if (detect.format == UFT_PC98_FMT_RAW && output_format == UFT_PC98_FMT_FDI98) {
        /* RAW to FDI-98 */
        return uft_fdi98_create_from_raw(input_path, output_path, detect.geometry);
    }
    else if (detect.format == output_format) {
        /* Same format - just copy */
        FILE* fin = fopen(input_path, "rb");
        if (!fin) return UFT_PC98_ERR_IO;
        
        FILE* fout = fopen(output_path, "wb");
        if (!fout) {
            fclose(fin);
            return UFT_PC98_ERR_IO;
        }
        
        uint8_t buffer[4096];
        size_t read;
        while ((read = fread(buffer, 1, sizeof(buffer), fin)) > 0) {
            if (fwrite(buffer, 1, read, fout) != read) {
                fclose(fin);
                fclose(fout);
                return UFT_PC98_ERR_IO;
            }
        }
        
        fclose(fin);
        fclose(fout);
        return UFT_PC98_SUCCESS;
    }
    
    /* Unsupported conversion */
    return UFT_PC98_ERR_FORMAT;
}

/* UTF-8 to Shift-JIS (stub - would need full mapping table) */
uft_pc98_rc_t uft_pc98_utf8_to_sjis(const char* utf8_str, uint8_t* sjis_data,
                                     size_t sjis_capacity, size_t* sjis_len) {
    if (!utf8_str || !sjis_data || sjis_capacity == 0) {
        return UFT_PC98_ERR_ARG;
    }
    
    /* Simple ASCII-only implementation for now */
    size_t len = 0;
    const uint8_t* src = (const uint8_t*)utf8_str;
    
    while (*src && len < sjis_capacity - 1) {
        if (*src < 0x80) {
            sjis_data[len++] = *src++;
        } else {
            /* Skip multi-byte UTF-8 sequences */
            if ((*src & 0xE0) == 0xC0) src += 2;
            else if ((*src & 0xF0) == 0xE0) src += 3;
            else if ((*src & 0xF8) == 0xF0) src += 4;
            else src++;
            sjis_data[len++] = '?'; /* Replacement */
        }
    }
    
    sjis_data[len] = '\0';
    if (sjis_len) *sjis_len = len;
    
    return UFT_PC98_SUCCESS;
}
