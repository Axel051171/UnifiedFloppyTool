/**
 * @file uft_ipf.c
 * @brief IPF Container Implementation v2.0
 * 
 * Enhanced implementation with known record types and structured parsing.
 * 
 * @version 2.0.0
 * @date 2025-01-08
 */

#include "uft/formats/ipf/uft_ipf.h"
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * Diagnostic Callback
 * ═══════════════════════════════════════════════════════════════════════════════ */

static uft_ipf_diag_callback_t g_diag_callback = NULL;
static void *g_diag_user_data = NULL;

void uft_ipf_set_diag_callback(uft_ipf_diag_callback_t cb, void *user_data) {
    g_diag_callback = cb;
    g_diag_user_data = user_data;
}

#if 0  /* Prepared for future use */
static void diag(uft_ipf_diag_level_t level, const char *fmt, ...) {
    if (!g_diag_callback) return;
    
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    
    g_diag_callback(level, buf, g_diag_user_data);
}
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Error Strings
 * ═══════════════════════════════════════════════════════════════════════════════ */

const char *uft_ipf_strerror(uft_ipf_err_t err) {
    switch (err) {
        case UFT_IPF_OK:         return "Success";
        case UFT_IPF_EINVAL:     return "Invalid argument";
        case UFT_IPF_EIO:        return "I/O error";
        case UFT_IPF_EFORMAT:    return "Invalid IPF format";
        case UFT_IPF_ESHORT:     return "File too short";
        case UFT_IPF_EBOUNDS:    return "Record out of bounds";
        case UFT_IPF_EOVERLAP:   return "Records overlap";
        case UFT_IPF_ECRC:       return "CRC mismatch";
        case UFT_IPF_ENOMEM:     return "Out of memory";
        case UFT_IPF_ENOTSUP:    return "Not supported";
        case UFT_IPF_EVERSION:   return "Unsupported version";
        case UFT_IPF_ETRUNCATED: return "Truncated record";
        case UFT_IPF_EMAGIC:     return "Invalid magic";
        default:                 return "Unknown error";
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Record Type Names
 * ═══════════════════════════════════════════════════════════════════════════════ */

const char *uft_ipf_record_type_name(uint32_t type) {
    switch (type) {
        case UFT_IPF_REC_CAPS: return "CAPS";
        case UFT_IPF_REC_INFO: return "INFO";
        case UFT_IPF_REC_IMGE: return "IMGE";
        case UFT_IPF_REC_DATA: return "DATA";
        case UFT_IPF_REC_TRCK: return "TRCK";
        case UFT_IPF_REC_CTEI: return "CTEI";
        case UFT_IPF_REC_CTEX: return "CTEX";
        case UFT_IPF_REC_DUMP: return "DUMP";
        case UFT_IPF_REC_COMM: return "COMM";
        case UFT_IPF_REC_TEXT: return "TEXT";
        case UFT_IPF_REC_USER: return "USER";
        default:               return "????";
    }
}

bool uft_ipf_record_type_known(uint32_t type) {
    switch (type) {
        case UFT_IPF_REC_CAPS:
        case UFT_IPF_REC_INFO:
        case UFT_IPF_REC_IMGE:
        case UFT_IPF_REC_DATA:
        case UFT_IPF_REC_TRCK:
        case UFT_IPF_REC_CTEI:
        case UFT_IPF_REC_CTEX:
        case UFT_IPF_REC_DUMP:
        case UFT_IPF_REC_COMM:
        case UFT_IPF_REC_TEXT:
        case UFT_IPF_REC_USER:
            return true;
        default:
            return false;
    }
}

const char *uft_ipf_platform_name(uint32_t platform) {
    static char buf[256];
    size_t pos = 0;
    buf[0] = '\0';
    
    #define APPEND_PLATFORM(flag, name) \
        if ((platform & (flag)) && pos < sizeof(buf) - 16) { \
            pos += (size_t)snprintf(buf + pos, sizeof(buf) - pos, "%s", name); \
        }
    
    APPEND_PLATFORM(UFT_IPF_PLATFORM_AMIGA_OCS, "Amiga-OCS ");
    APPEND_PLATFORM(UFT_IPF_PLATFORM_AMIGA_ECS, "Amiga-ECS ");
    APPEND_PLATFORM(UFT_IPF_PLATFORM_AMIGA_AGA, "Amiga-AGA ");
    APPEND_PLATFORM(UFT_IPF_PLATFORM_ATARI_ST,  "Atari-ST ");
    APPEND_PLATFORM(UFT_IPF_PLATFORM_ATARI_STE, "Atari-STE ");
    APPEND_PLATFORM(UFT_IPF_PLATFORM_PC_DOS,    "PC-DOS ");
    APPEND_PLATFORM(UFT_IPF_PLATFORM_AMSTRAD_CPC, "CPC ");
    APPEND_PLATFORM(UFT_IPF_PLATFORM_SPECTRUM,  "Spectrum ");
    APPEND_PLATFORM(UFT_IPF_PLATFORM_C64,       "C64 ");
    APPEND_PLATFORM(UFT_IPF_PLATFORM_C128,      "C128 ");
    
    #undef APPEND_PLATFORM
    
    if (buf[0] == '\0') {
        (void)snprintf(buf, sizeof(buf), "(unknown)");
    }
    return buf;
}

const char *uft_ipf_density_name(uint32_t density) {
    switch (density) {
        case UFT_IPF_DENSITY_UNKNOWN:   return "Unknown";
        case UFT_IPF_DENSITY_NOISE:     return "Noise";
        case UFT_IPF_DENSITY_AUTO:      return "Auto";
        case UFT_IPF_DENSITY_AMIGA_DD:  return "Amiga DD";
        case UFT_IPF_DENSITY_AMIGA_HD:  return "Amiga HD";
        case UFT_IPF_DENSITY_ATARI_DD:  return "Atari DD";
        case UFT_IPF_DENSITY_PC_DD:     return "PC DD";
        case UFT_IPF_DENSITY_PC_HD:     return "PC HD";
        case UFT_IPF_DENSITY_C64:       return "C64 GCR";
        case UFT_IPF_DENSITY_APPLE_GCR: return "Apple GCR";
        default:                        return "Unknown";
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Internal Helpers
 * ═══════════════════════════════════════════════════════════════════════════════ */

static uint64_t get_file_size(FILE *fp) {
    if (!fp) return 0;
    long cur = ftell(fp);
    if (cur < 0) return 0;
    if (fseek(fp, 0, SEEK_END) != 0) return 0;
    long end = ftell(fp);
    if (end < 0) return 0;
    (void)fseek(fp, cur, SEEK_SET);
    return (uint64_t)end;
}

static bool read_exact(FILE *fp, void *buf, size_t n) {
    if (!fp || !buf || n == 0) return false;
    return fread(buf, 1, n, fp) == n;
}

/* Big-endian read (IPF is always big-endian) */
static uint32_t rd_u32_be(const uint8_t b[4]) {
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
           ((uint32_t)b[2] << 8)  | (uint32_t)b[3];
}

static void wr_u32_be(uint8_t b[4], uint32_t v) {
    b[0] = (v >> 24) & 0xFF;
    b[1] = (v >> 16) & 0xFF;
    b[2] = (v >> 8) & 0xFF;
    b[3] = v & 0xFF;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * CRC32 (IEEE polynomial)
 * ═══════════════════════════════════════════════════════════════════════════════ */

static uint32_t crc32_table[256];
static bool crc32_table_init = false;

static void init_crc32_table(void) {
    if (crc32_table_init) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++) {
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        }
        crc32_table[i] = c;
    }
    crc32_table_init = true;
}

uint32_t uft_ipf_crc32(const void *data, size_t len) {
    init_crc32_table();
    uint32_t c = 0xFFFFFFFFu;
    const uint8_t *p = (const uint8_t*)data;
    for (size_t i = 0; i < len; i++) {
        c = crc32_table[(c ^ p[i]) & 0xFFu] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFu;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Probe Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool uft_ipf_probe_fp(FILE *fp) {
    if (!fp) return false;
    
    long pos = ftell(fp);
    if (pos < 0) return false;
    
    if (fseek(fp, 0, SEEK_SET) != 0) return false;
    
    uint8_t hdr[12];
    bool ok = read_exact(fp, hdr, 12);
    (void)fseek(fp, pos, SEEK_SET);  /* Best-effort position restore */
    
    if (!ok) return false;
    
    /* Check for CAPS magic */
    uint32_t type = rd_u32_be(hdr);
    return (type == UFT_IPF_REC_CAPS);
}

bool uft_ipf_probe(const char *path) {
    if (!path) return false;
    
    FILE *fp = fopen(path, "rb");
    if (!fp) return false;
    
    bool result = uft_ipf_probe_fp(fp);
    fclose(fp);
    return result;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Parse INFO Record
 * ═══════════════════════════════════════════════════════════════════════════════ */

static bool parse_info_record(const uint8_t *data, size_t len, uft_ipf_info_t *info) {
    memset(info, 0, sizeof(*info));
    
    /* INFO record is typically 96 bytes */
    if (len < 96) return false;
    
    info->media_type     = rd_u32_be(data + 0);
    info->encoder_type   = rd_u32_be(data + 4);
    info->encoder_rev    = rd_u32_be(data + 8);
    info->file_key       = rd_u32_be(data + 12);
    info->file_rev       = rd_u32_be(data + 16);
    info->origin         = rd_u32_be(data + 20);
    info->min_track      = rd_u32_be(data + 24);
    info->max_track      = rd_u32_be(data + 28);
    info->min_side       = rd_u32_be(data + 32);
    info->max_side       = rd_u32_be(data + 36);
    info->creation_date  = rd_u32_be(data + 40);
    info->creation_time  = rd_u32_be(data + 44);
    info->platforms      = rd_u32_be(data + 48);
    info->disk_number    = rd_u32_be(data + 52);
    info->creator_id     = rd_u32_be(data + 56);
    info->reserved[0]    = rd_u32_be(data + 60);
    info->reserved[1]    = rd_u32_be(data + 64);
    info->reserved[2]    = rd_u32_be(data + 68);
    info->parsed = true;
    
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Parse IMGE Record
 * ═══════════════════════════════════════════════════════════════════════════════ */

static bool parse_imge_record(const uint8_t *data, size_t len, uft_ipf_imge_t *imge) {
    memset(imge, 0, sizeof(*imge));
    
    /* IMGE record is typically 80 bytes */
    if (len < 80) return false;
    
    imge->track           = rd_u32_be(data + 0);
    imge->side            = rd_u32_be(data + 4);
    imge->density         = rd_u32_be(data + 8);
    imge->signal_type     = rd_u32_be(data + 12);
    imge->track_bytes     = rd_u32_be(data + 16);
    imge->start_byte_pos  = rd_u32_be(data + 20);
    imge->start_bit_pos   = rd_u32_be(data + 24);
    imge->data_bits       = rd_u32_be(data + 28);
    imge->gap_bits        = rd_u32_be(data + 32);
    imge->track_bits      = rd_u32_be(data + 36);
    imge->block_count     = rd_u32_be(data + 40);
    imge->encoder_process = rd_u32_be(data + 44);
    imge->flags           = rd_u32_be(data + 48);
    imge->data_key        = rd_u32_be(data + 52);
    imge->parsed = true;
    
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Open & Parse
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_ipf_err_t uft_ipf_open(uft_ipf_t *ctx, const char *path) {
    if (!ctx || !path) return UFT_IPF_EINVAL;
    
    memset(ctx, 0, sizeof(*ctx));
    strncpy(ctx->path, path, sizeof(ctx->path) - 1);
    
    ctx->fp = fopen(path, "rb");
    if (!ctx->fp) return UFT_IPF_EIO;
    
    ctx->file_size = get_file_size(ctx->fp);
    
    /* Minimum IPF: CAPS header (12 bytes) */
    if (ctx->file_size < 12) {
        uft_ipf_close(ctx);
        return UFT_IPF_ESHORT;
    }
    
    /* Read and verify CAPS header */
    uint8_t hdr[12];
    if (fseek(ctx->fp, 0, SEEK_SET) != 0 || !read_exact(ctx->fp, hdr, 12)) {
        uft_ipf_close(ctx);
        return UFT_IPF_EIO;
    }
    
    uint32_t magic = rd_u32_be(hdr);
    if (magic != UFT_IPF_REC_CAPS) {
        snprintf(ctx->last_error, sizeof(ctx->last_error),
                 "Invalid magic: %08X (expected CAPS)", magic);
        uft_ipf_close(ctx);
        return UFT_IPF_EMAGIC;
    }
    
    ctx->is_valid_ipf = true;
    
    /* Parse all records */
    size_t cap = 64;
    ctx->records = (uft_ipf_record_t*)calloc(cap, sizeof(uft_ipf_record_t));
    if (!ctx->records) {
        uft_ipf_close(ctx);
        return UFT_IPF_ENOMEM;
    }
    
    /* Count IMGE records for allocation */
    size_t imge_count = 0;
    
    uint64_t offset = 0;
    while (offset + 12 <= ctx->file_size) {
        if (fseek(ctx->fp, (long)offset, SEEK_SET) != 0) break;
        
        uint8_t rec_hdr[12];
        if (!read_exact(ctx->fp, rec_hdr, 12)) break;
        
        uint32_t type   = rd_u32_be(rec_hdr + 0);
        uint32_t length = rd_u32_be(rec_hdr + 4);
        uint32_t crc    = rd_u32_be(rec_hdr + 8);
        
        /* Sanity check */
        if (length > ctx->file_size - offset - 12) {
            ctx->warnings |= UFT_IPF_WARN_TRUNCATED;
            break;
        }
        
        /* Grow array if needed */
        if (ctx->record_count >= cap) {
            cap *= 2;
            uft_ipf_record_t *newrec = (uft_ipf_record_t*)realloc(
                ctx->records, cap * sizeof(uft_ipf_record_t));
            if (!newrec) {
                uft_ipf_close(ctx);
                return UFT_IPF_ENOMEM;
            }
            ctx->records = newrec;
        }
        
        uft_ipf_record_t *r = &ctx->records[ctx->record_count];
        r->type = type;
        r->length = length;
        r->crc = crc;
        r->header_offset = offset;
        r->data_offset = offset + 12;
        r->index = (uint32_t)ctx->record_count;
        r->crc_valid = false;
        r->crc_checked = false;
        
        ctx->record_count++;
        
        /* Statistics */
        if (type == UFT_IPF_REC_DATA) ctx->data_records++;
        else if (type == UFT_IPF_REC_TRCK) ctx->track_records++;
        else if (!uft_ipf_record_type_known(type)) ctx->unknown_records++;
        
        if (type == UFT_IPF_REC_IMGE) imge_count++;
        
        ctx->total_data_size += length;
        
        offset += 12 + length;
    }
    
    if (ctx->unknown_records > 0) {
        ctx->warnings |= UFT_IPF_WARN_UNKNOWN_RECORDS;
    }
    
    /* Parse INFO record */
    size_t info_idx = uft_ipf_find_record(ctx, UFT_IPF_REC_INFO);
    if (info_idx != SIZE_MAX) {
        const uft_ipf_record_t *r = &ctx->records[info_idx];
        uint8_t *buf = (uint8_t*)malloc(r->length);
        if (buf) {
            size_t got;
            if (uft_ipf_read_record(ctx, info_idx, buf, r->length, &got) == UFT_IPF_OK) {
                parse_info_record(buf, got, &ctx->info);
            }
            free(buf);
        }
    } else {
        ctx->warnings |= UFT_IPF_WARN_MISSING_INFO;
    }
    
    /* Parse IMGE records */
    if (imge_count > 0) {
        ctx->images = (uft_ipf_imge_t*)calloc(imge_count, sizeof(uft_ipf_imge_t));
        if (ctx->images) {
            size_t img_idx = 0;
            for (size_t i = 0; i < ctx->record_count && img_idx < imge_count; i++) {
                if (ctx->records[i].type == UFT_IPF_REC_IMGE) {
                    const uft_ipf_record_t *r = &ctx->records[i];
                    uint8_t *buf = (uint8_t*)malloc(r->length);
                    if (buf) {
                        size_t got;
                        if (uft_ipf_read_record(ctx, i, buf, r->length, &got) == UFT_IPF_OK) {
                            if (parse_imge_record(buf, got, &ctx->images[img_idx])) {
                                img_idx++;
                            }
                        }
                        free(buf);
                    }
                }
            }
            ctx->image_count = img_idx;
        }
    } else {
        ctx->warnings |= UFT_IPF_WARN_MISSING_IMGE;
    }
    
    return UFT_IPF_OK;
}

void uft_ipf_close(uft_ipf_t *ctx) {
    if (!ctx) return;
    
    if (ctx->fp) {
        fclose(ctx->fp);
        ctx->fp = NULL;
    }
    
    free(ctx->records);
    ctx->records = NULL;
    ctx->record_count = 0;
    
    free(ctx->images);
    ctx->images = NULL;
    ctx->image_count = 0;
    
    ctx->file_size = 0;
    ctx->is_valid_ipf = false;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Validation
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_ipf_err_t uft_ipf_validate(uft_ipf_t *ctx, bool check_crc) {
    if (!ctx || !ctx->fp || !ctx->records) return UFT_IPF_EINVAL;
    
    /* Bounds check */
    for (size_t i = 0; i < ctx->record_count; i++) {
        const uft_ipf_record_t *r = &ctx->records[i];
        uint64_t end = r->data_offset + r->length;
        if (end > ctx->file_size) return UFT_IPF_EBOUNDS;
    }
    
    /* Overlap check */
    for (size_t i = 0; i < ctx->record_count; i++) {
        uint64_t a0 = ctx->records[i].header_offset;
        uint64_t a1 = ctx->records[i].data_offset + ctx->records[i].length;
        
        for (size_t j = i + 1; j < ctx->record_count; j++) {
            uint64_t b0 = ctx->records[j].header_offset;
            uint64_t b1 = ctx->records[j].data_offset + ctx->records[j].length;
            
            if (!(a1 <= b0 || b1 <= a0)) return UFT_IPF_EOVERLAP;
        }
    }
    
    /* CRC check */
    if (check_crc) {
        for (size_t i = 0; i < ctx->record_count; i++) {
            if (!uft_ipf_verify_record_crc(ctx, i)) {
                ctx->warnings |= UFT_IPF_WARN_CRC_MISMATCH;
                return UFT_IPF_ECRC;
            }
        }
    }
    
    return UFT_IPF_OK;
}

bool uft_ipf_verify_record_crc(const uft_ipf_t *ctx, size_t idx) {
    if (!ctx || !ctx->fp || idx >= ctx->record_count) return false;
    
    uft_ipf_record_t *r = (uft_ipf_record_t*)&ctx->records[idx];
    
    if (r->length == 0) {
        r->crc_checked = true;
        r->crc_valid = true;
        return true;
    }
    
    void *buf = malloc(r->length);
    if (!buf) return false;
    
    if (fseek(ctx->fp, (long)r->data_offset, SEEK_SET) != 0 ||
        fread(buf, 1, r->length, ctx->fp) != r->length) {
        free(buf);
        return false;
    }
    
    uint32_t calc_crc = uft_ipf_crc32(buf, r->length);
    free(buf);
    
    r->crc_checked = true;
    r->crc_valid = (calc_crc == r->crc);
    
    return r->crc_valid;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Record Access
 * ═══════════════════════════════════════════════════════════════════════════════ */

size_t uft_ipf_record_count(const uft_ipf_t *ctx) {
    return ctx ? ctx->record_count : 0;
}

const uft_ipf_record_t *uft_ipf_record_at(const uft_ipf_t *ctx, size_t idx) {
    if (!ctx || idx >= ctx->record_count) return NULL;
    return &ctx->records[idx];
}

size_t uft_ipf_find_record(const uft_ipf_t *ctx, uint32_t type) {
    if (!ctx) return SIZE_MAX;
    for (size_t i = 0; i < ctx->record_count; i++) {
        if (ctx->records[i].type == type) return i;
    }
    return SIZE_MAX;
}

size_t uft_ipf_find_next_record(const uft_ipf_t *ctx, uint32_t type, size_t after) {
    if (!ctx) return SIZE_MAX;
    for (size_t i = after + 1; i < ctx->record_count; i++) {
        if (ctx->records[i].type == type) return i;
    }
    return SIZE_MAX;
}

uft_ipf_err_t uft_ipf_read_record(const uft_ipf_t *ctx, size_t idx,
                                   void *buf, size_t buf_sz, size_t *out_read) {
    if (out_read) *out_read = 0;
    if (!ctx || !ctx->fp || !buf) return UFT_IPF_EINVAL;
    if (idx >= ctx->record_count) return UFT_IPF_EINVAL;
    
    const uft_ipf_record_t *r = &ctx->records[idx];
    size_t to_read = r->length;
    if (to_read > buf_sz) to_read = buf_sz;
    
    if (fseek(ctx->fp, (long)r->data_offset, SEEK_SET) != 0) return UFT_IPF_EIO;
    
    size_t got = fread(buf, 1, to_read, ctx->fp);
    if (out_read) *out_read = got;
    
    return (got == to_read) ? UFT_IPF_OK : UFT_IPF_EIO;
}

const uft_ipf_info_t *uft_ipf_get_info(const uft_ipf_t *ctx) {
    if (!ctx || !ctx->info.parsed) return NULL;
    return &ctx->info;
}

const uft_ipf_imge_t *uft_ipf_get_image(const uft_ipf_t *ctx, uint32_t track, uint32_t side) {
    if (!ctx || !ctx->images) return NULL;
    
    for (size_t i = 0; i < ctx->image_count; i++) {
        if (ctx->images[i].track == track && ctx->images[i].side == side) {
            return &ctx->images[i];
        }
    }
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Statistics & Dump
 * ═══════════════════════════════════════════════════════════════════════════════ */

void uft_ipf_get_stats(const uft_ipf_t *ctx, 
                       size_t *total_records,
                       size_t *data_records,
                       size_t *track_records,
                       uint64_t *total_bytes) {
    if (!ctx) return;
    if (total_records) *total_records = ctx->record_count;
    if (data_records) *data_records = ctx->data_records;
    if (track_records) *track_records = ctx->track_records;
    if (total_bytes) *total_bytes = ctx->total_data_size;
}

const char *uft_ipf_type_to_string(uint32_t type) {
    static char buf[8];
    buf[0] = (type >> 24) & 0xFF;
    buf[1] = (type >> 16) & 0xFF;
    buf[2] = (type >> 8) & 0xFF;
    buf[3] = type & 0xFF;
    buf[4] = '\0';
    return buf;
}

uint32_t uft_ipf_string_to_type(const char *str) {
    if (!str || strlen(str) < 4) return 0;
    return ((uint32_t)str[0] << 24) | ((uint32_t)str[1] << 16) |
           ((uint32_t)str[2] << 8)  | (uint32_t)str[3];
}

void uft_ipf_dump(const uft_ipf_t *ctx, FILE *out, bool verbose) {
    if (!ctx || !out) return;
    
    fprintf(out, "═══════════════════════════════════════════════════════════════════\n");
    fprintf(out, "IPF Container Analysis: %s\n", ctx->path);
    fprintf(out, "═══════════════════════════════════════════════════════════════════\n\n");
    
    fprintf(out, "File Size:     %llu bytes\n", (unsigned long long)ctx->file_size);
    fprintf(out, "Valid IPF:     %s\n", ctx->is_valid_ipf ? "Yes" : "No");
    fprintf(out, "Records:       %zu\n", ctx->record_count);
    fprintf(out, "  DATA:        %zu\n", ctx->data_records);
    fprintf(out, "  TRCK:        %zu\n", ctx->track_records);
    fprintf(out, "  Unknown:     %zu\n", ctx->unknown_records);
    fprintf(out, "Total Data:    %llu bytes\n", (unsigned long long)ctx->total_data_size);
    
    if (ctx->warnings) {
        fprintf(out, "\nWarnings:      ");
        if (ctx->warnings & UFT_IPF_WARN_CRC_MISMATCH) fprintf(out, "CRC-MISMATCH ");
        if (ctx->warnings & UFT_IPF_WARN_TRUNCATED) fprintf(out, "TRUNCATED ");
        if (ctx->warnings & UFT_IPF_WARN_UNKNOWN_RECORDS) fprintf(out, "UNKNOWN-RECORDS ");
        if (ctx->warnings & UFT_IPF_WARN_MISSING_INFO) fprintf(out, "NO-INFO ");
        if (ctx->warnings & UFT_IPF_WARN_MISSING_IMGE) fprintf(out, "NO-IMGE ");
        fprintf(out, "\n");
    }
    
    /* INFO record */
    if (ctx->info.parsed) {
        fprintf(out, "\n── INFO Record ──────────────────────────────────────────────────\n");
        fprintf(out, "  Tracks:      %u - %u\n", ctx->info.min_track, ctx->info.max_track);
        fprintf(out, "  Sides:       %u - %u\n", ctx->info.min_side, ctx->info.max_side);
        fprintf(out, "  Platforms:   %s\n", uft_ipf_platform_name(ctx->info.platforms));
        fprintf(out, "  Media Type:  %u\n", ctx->info.media_type);
        fprintf(out, "  Encoder:     %u (rev %u)\n", ctx->info.encoder_type, ctx->info.encoder_rev);
        fprintf(out, "  File Key:    %08X\n", ctx->info.file_key);
        fprintf(out, "  Disk #:      %u\n", ctx->info.disk_number);
    }
    
    /* IMGE records */
    if (ctx->image_count > 0) {
        fprintf(out, "\n── IMGE Records (%zu) ────────────────────────────────────────────\n", ctx->image_count);
        for (size_t i = 0; i < ctx->image_count && i < 10; i++) {
            const uft_ipf_imge_t *img = &ctx->images[i];
            fprintf(out, "  [%zu] Track %2u Side %u: %u bits, density=%s\n",
                    i, img->track, img->side, img->track_bits,
                    uft_ipf_density_name(img->density));
        }
        if (ctx->image_count > 10) {
            fprintf(out, "  ... and %zu more\n", ctx->image_count - 10);
        }
    }
    
    /* Record list */
    if (verbose) {
        fprintf(out, "\n── All Records ──────────────────────────────────────────────────\n");
        fprintf(out, "  #   Type  Offset       Length     CRC\n");
        fprintf(out, "  ─── ────  ──────────── ────────── ────────\n");
        for (size_t i = 0; i < ctx->record_count; i++) {
            const uft_ipf_record_t *r = &ctx->records[i];
            fprintf(out, "  %3zu %s  %12llu %10u %08X%s\n",
                    i, uft_ipf_type_to_string(r->type),
                    (unsigned long long)r->header_offset,
                    r->length, r->crc,
                    r->crc_checked ? (r->crc_valid ? " ✓" : " ✗") : "");
        }
    }
    
    fprintf(out, "\n═══════════════════════════════════════════════════════════════════\n");
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Writer Implementation
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_ipf_err_t uft_ipf_writer_open(uft_ipf_writer_t *w, const char *path) {
    if (!w || !path) return UFT_IPF_EINVAL;
    
    memset(w, 0, sizeof(*w));
    w->fp = fopen(path, "wb");
    if (!w->fp) return UFT_IPF_EIO;
    
    return UFT_IPF_OK;
}

uft_ipf_err_t uft_ipf_writer_write_header(uft_ipf_writer_t *w) {
    if (!w || !w->fp || w->header_written) return UFT_IPF_EINVAL;
    
    /* Write CAPS record with empty data */
    uint8_t hdr[12];
    wr_u32_be(hdr + 0, UFT_IPF_REC_CAPS);
    wr_u32_be(hdr + 4, 0);  /* length */
    wr_u32_be(hdr + 8, 0);  /* crc of empty */
    
    if (fwrite(hdr, 1, 12, w->fp) != 12) return UFT_IPF_EIO;
    
    w->bytes_written = 12;
    w->record_count = 1;
    w->header_written = true;
    
    return UFT_IPF_OK;
}

uft_ipf_err_t uft_ipf_writer_add_info(uft_ipf_writer_t *w, const uft_ipf_info_t *info) {
    if (!w || !w->fp || !info) return UFT_IPF_EINVAL;
    
    if (!w->header_written) {
        uft_ipf_err_t e = uft_ipf_writer_write_header(w);
        if (e != UFT_IPF_OK) return e;
    }
    
    /* Build INFO record data (96 bytes) */
    uint8_t data[96];
    memset(data, 0, sizeof(data));
    
    wr_u32_be(data + 0,  info->media_type);
    wr_u32_be(data + 4,  info->encoder_type);
    wr_u32_be(data + 8,  info->encoder_rev);
    wr_u32_be(data + 12, info->file_key);
    wr_u32_be(data + 16, info->file_rev);
    wr_u32_be(data + 20, info->origin);
    wr_u32_be(data + 24, info->min_track);
    wr_u32_be(data + 28, info->max_track);
    wr_u32_be(data + 32, info->min_side);
    wr_u32_be(data + 36, info->max_side);
    wr_u32_be(data + 40, info->creation_date);
    wr_u32_be(data + 44, info->creation_time);
    wr_u32_be(data + 48, info->platforms);
    wr_u32_be(data + 52, info->disk_number);
    wr_u32_be(data + 56, info->creator_id);
    
    return uft_ipf_writer_add_record(w, UFT_IPF_REC_INFO, data, sizeof(data));
}

uft_ipf_err_t uft_ipf_writer_add_record(uft_ipf_writer_t *w, uint32_t type,
                                         const void *data, uint32_t length) {
    if (!w || !w->fp) return UFT_IPF_EINVAL;
    if (!data && length > 0) return UFT_IPF_EINVAL;
    
    if (!w->header_written) {
        uft_ipf_err_t e = uft_ipf_writer_write_header(w);
        if (e != UFT_IPF_OK) return e;
    }
    
    uint32_t crc = (length > 0) ? uft_ipf_crc32(data, length) : 0;
    
    uint8_t hdr[12];
    wr_u32_be(hdr + 0, type);
    wr_u32_be(hdr + 4, length);
    wr_u32_be(hdr + 8, crc);
    
    if (fwrite(hdr, 1, 12, w->fp) != 12) return UFT_IPF_EIO;
    
    if (length > 0) {
        if (fwrite(data, 1, length, w->fp) != length) return UFT_IPF_EIO;
    }
    
    w->bytes_written += 12 + length;
    w->record_count++;
    
    return UFT_IPF_OK;
}

uft_ipf_err_t uft_ipf_writer_close(uft_ipf_writer_t *w) {
    if (!w) return UFT_IPF_EINVAL;
    
    if (w->fp) {
        fflush(w->fp);
        fclose(w->fp);
        w->fp = NULL;
    }
    
    return UFT_IPF_OK;
}
