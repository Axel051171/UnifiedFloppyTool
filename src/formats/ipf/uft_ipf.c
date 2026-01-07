/**
 * @file uft_ipf.c
 * @brief IPF Container Implementation
 * 
 * @version 1.0.0
 * @date 2026-01-07
 */

#include "uft/formats/ipf/uft_ipf.h"
#include <string.h>
#include <stdlib.h>
#include <errno.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * Error Strings
 * ═══════════════════════════════════════════════════════════════════════════════ */

const char *uft_ipf_strerror(uft_ipf_err_t err) {
    switch (err) {
        case UFT_IPF_OK:       return "Success";
        case UFT_IPF_EINVAL:   return "Invalid argument";
        case UFT_IPF_EIO:      return "I/O error";
        case UFT_IPF_EFORMAT:  return "Invalid format";
        case UFT_IPF_ESHORT:   return "File too short";
        case UFT_IPF_EBOUNDS:  return "Chunk out of bounds";
        case UFT_IPF_EOVERLAP: return "Chunks overlap";
        case UFT_IPF_ECRC:     return "CRC mismatch";
        case UFT_IPF_ENOMEM:   return "Out of memory";
        case UFT_IPF_ENOTSUP:  return "Not supported";
        default:               return "Unknown error";
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

static uint32_t rd_u32_be(const uint8_t b[4]) {
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
           ((uint32_t)b[2] << 8)  | (uint32_t)b[3];
}

static uint32_t rd_u32_le(const uint8_t b[4]) {
    return ((uint32_t)b[3] << 24) | ((uint32_t)b[2] << 16) |
           ((uint32_t)b[1] << 8)  | (uint32_t)b[0];
}

static void wr_u32(FILE *fp, uint32_t v, bool big_endian) {
    uint8_t b[4];
    if (big_endian) {
        b[0] = (v >> 24) & 0xFF;
        b[1] = (v >> 16) & 0xFF;
        b[2] = (v >> 8) & 0xFF;
        b[3] = v & 0xFF;
    } else {
        b[0] = v & 0xFF;
        b[1] = (v >> 8) & 0xFF;
        b[2] = (v >> 16) & 0xFF;
        b[3] = (v >> 24) & 0xFF;
    }
    fwrite(b, 1, 4, fp);
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
 * Reader Implementation
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_ipf_err_t uft_ipf_open(uft_ipf_t *ctx, const char *path) {
    if (!ctx || !path) return UFT_IPF_EINVAL;
    
    memset(ctx, 0, sizeof(*ctx));
    strncpy(ctx->path, path, sizeof(ctx->path) - 1);
    
    ctx->fp = fopen(path, "rb");
    if (!ctx->fp) return UFT_IPF_EIO;
    
    ctx->file_size = get_file_size(ctx->fp);
    if (ctx->file_size < 16) {
        uft_ipf_close(ctx);
        return UFT_IPF_ESHORT;
    }
    
    /* Read first 16 bytes for magic/endian detection */
    uint8_t hdr[16];
    if (fseek(ctx->fp, 0, SEEK_SET) != 0 || !read_exact(ctx->fp, hdr, sizeof(hdr))) {
        uft_ipf_close(ctx);
        return UFT_IPF_EIO;
    }
    
    ctx->magic.b[0] = hdr[0];
    ctx->magic.b[1] = hdr[1];
    ctx->magic.b[2] = hdr[2];
    ctx->magic.b[3] = hdr[3];
    
    /* Endianness heuristic: check if bytes 4-7 look like valid BE or LE size */
    uint32_t be = rd_u32_be(&hdr[4]);
    uint32_t le = rd_u32_le(&hdr[4]);
    uint64_t remain = ctx->file_size >= 8 ? (ctx->file_size - 8) : 0;
    
    bool be_ok = (be > 0 && be <= remain);
    bool le_ok = (le > 0 && le <= remain);
    
    ctx->big_endian = (be_ok && !le_ok);
    
    return UFT_IPF_OK;
}

void uft_ipf_close(uft_ipf_t *ctx) {
    if (!ctx) return;
    
    if (ctx->fp) {
        fclose(ctx->fp);
        ctx->fp = NULL;
    }
    
    free(ctx->chunks);
    ctx->chunks = NULL;
    ctx->chunk_count = 0;
    ctx->file_size = 0;
    ctx->big_endian = false;
    ctx->magic = uft_fourcc_make(0, 0, 0, 0);
    ctx->path[0] = '\0';
}

uft_ipf_err_t uft_ipf_parse(uft_ipf_t *ctx) {
    if (!ctx || !ctx->fp) return UFT_IPF_EINVAL;
    
    /* Free existing chunks */
    free(ctx->chunks);
    ctx->chunks = NULL;
    ctx->chunk_count = 0;
    
    if (fseek(ctx->fp, 0, SEEK_SET) != 0) return UFT_IPF_EIO;
    
    uint64_t off = 0;
    size_t cap = 64;
    ctx->chunks = (uft_ipf_chunk_t*)calloc(cap, sizeof(uft_ipf_chunk_t));
    if (!ctx->chunks) return UFT_IPF_ENOMEM;
    
    while (off + 8 <= ctx->file_size) {
        uint8_t idb[4], szb[4];
        if (!read_exact(ctx->fp, idb, 4) || !read_exact(ctx->fp, szb, 4)) {
            break;
        }
        
        uint32_t size = ctx->big_endian ? rd_u32_be(szb) : rd_u32_le(szb);
        
        uft_ipf_chunk_t ck;
        memset(&ck, 0, sizeof(ck));
        ck.id.b[0] = idb[0];
        ck.id.b[1] = idb[1];
        ck.id.b[2] = idb[2];
        ck.id.b[3] = idb[3];
        ck.header_offset = off;
        ck.header_size = 8;
        
        /* Sanity check size */
        if (size > (uint32_t)(ctx->file_size - (off + 8))) {
            /* Try 12-byte header mode: [id][size][crc][data] */
            if (off + 12 <= ctx->file_size) {
                uint8_t crcb[4];
                if (!read_exact(ctx->fp, crcb, 4)) {
                    free(ctx->chunks);
                    ctx->chunks = NULL;
                    ctx->chunk_count = 0;
                    return UFT_IPF_EIO;
                }
                
                uint32_t crc = ctx->big_endian ? rd_u32_be(crcb) : rd_u32_le(crcb);
                ck.header_size = 12;
                ck.crc32 = crc;
                ck.data_offset = off + 12;
                
                /* Re-validate size */
                if (size > (uint32_t)(ctx->file_size - ck.data_offset)) {
                    free(ctx->chunks);
                    ctx->chunks = NULL;
                    ctx->chunk_count = 0;
                    return UFT_IPF_EFORMAT;
                }
                
                ck.data_size = size;
                if (fseek(ctx->fp, (long)size, SEEK_CUR) != 0) break;
                off = ck.data_offset + size;
            } else {
                free(ctx->chunks);
                ctx->chunks = NULL;
                ctx->chunk_count = 0;
                return UFT_IPF_EFORMAT;
            }
        } else {
            ck.data_offset = off + 8;
            ck.data_size = size;
            if (fseek(ctx->fp, (long)size, SEEK_CUR) != 0) break;
            off = ck.data_offset + size;
        }
        
        /* Grow array if needed */
        if (ctx->chunk_count == cap) {
            cap *= 2;
            uft_ipf_chunk_t *n = (uft_ipf_chunk_t*)realloc(ctx->chunks, 
                                                           cap * sizeof(uft_ipf_chunk_t));
            if (!n) {
                free(ctx->chunks);
                ctx->chunks = NULL;
                ctx->chunk_count = 0;
                return UFT_IPF_ENOMEM;
            }
            ctx->chunks = n;
        }
        
        ctx->chunks[ctx->chunk_count++] = ck;
    }
    
    return ctx->chunk_count > 0 ? UFT_IPF_OK : UFT_IPF_EFORMAT;
}

uft_ipf_err_t uft_ipf_validate(const uft_ipf_t *ctx, bool strict) {
    if (!ctx || !ctx->fp || !ctx->chunks) return UFT_IPF_EINVAL;
    
    /* Bounds check */
    for (size_t i = 0; i < ctx->chunk_count; i++) {
        const uft_ipf_chunk_t *c = &ctx->chunks[i];
        uint64_t end = c->data_offset + (uint64_t)c->data_size;
        
        if (c->data_offset < c->header_offset) return UFT_IPF_EBOUNDS;
        if (end > ctx->file_size) return UFT_IPF_EBOUNDS;
    }
    
    if (strict) {
        /* Overlap check (O(n²) - ok for small chunk counts) */
        for (size_t i = 0; i < ctx->chunk_count; i++) {
            const uft_ipf_chunk_t *a = &ctx->chunks[i];
            uint64_t a0 = a->header_offset;
            uint64_t a1 = a->data_offset + (uint64_t)a->data_size;
            
            for (size_t j = i + 1; j < ctx->chunk_count; j++) {
                const uft_ipf_chunk_t *b = &ctx->chunks[j];
                uint64_t b0 = b->header_offset;
                uint64_t b1 = b->data_offset + (uint64_t)b->data_size;
                
                if (!(a1 <= b0 || b1 <= a0)) {
                    return UFT_IPF_EOVERLAP;
                }
            }
        }
        
        /* CRC check if header_size == 12 */
        for (size_t i = 0; i < ctx->chunk_count; i++) {
            const uft_ipf_chunk_t *c = &ctx->chunks[i];
            if (c->header_size == 12 && c->crc32 != 0) {
                void *buf = malloc(c->data_size);
                if (!buf) return UFT_IPF_ENOMEM;
                
                if (fseek(ctx->fp, (long)c->data_offset, SEEK_SET) != 0 ||
                    fread(buf, 1, c->data_size, ctx->fp) != c->data_size) {
                    free(buf);
                    return UFT_IPF_EIO;
                }
                
                uint32_t crc = uft_ipf_crc32(buf, c->data_size);
                free(buf);
                
                if (crc != c->crc32) {
                    return UFT_IPF_ECRC;
                }
            }
        }
    }
    
    return UFT_IPF_OK;
}

size_t uft_ipf_chunk_count(const uft_ipf_t *ctx) {
    return ctx ? ctx->chunk_count : 0;
}

const uft_ipf_chunk_t *uft_ipf_chunk_at(const uft_ipf_t *ctx, size_t idx) {
    if (!ctx || !ctx->chunks || idx >= ctx->chunk_count) return NULL;
    return &ctx->chunks[idx];
}

size_t uft_ipf_find_chunk(const uft_ipf_t *ctx, uft_fourcc_t id) {
    if (!ctx || !ctx->chunks) return SIZE_MAX;
    
    for (size_t i = 0; i < ctx->chunk_count; i++) {
        if (uft_fourcc_eq(ctx->chunks[i].id, id)) {
            return i;
        }
    }
    return SIZE_MAX;
}

uft_ipf_err_t uft_ipf_read_chunk_data(const uft_ipf_t *ctx, size_t idx,
                                       void *buf, size_t buf_sz, size_t *out_read) {
    if (out_read) *out_read = 0;
    if (!ctx || !ctx->fp || !buf) return UFT_IPF_EINVAL;
    if (idx >= ctx->chunk_count) return UFT_IPF_EINVAL;
    
    const uft_ipf_chunk_t *c = &ctx->chunks[idx];
    size_t to_read = c->data_size;
    if (to_read > buf_sz) to_read = buf_sz;
    
    if (fseek(ctx->fp, (long)c->data_offset, SEEK_SET) != 0) return UFT_IPF_EIO;
    
    size_t got = fread(buf, 1, to_read, ctx->fp);
    if (out_read) *out_read = got;
    
    return (got == to_read) ? UFT_IPF_OK : UFT_IPF_EIO;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Writer Implementation
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_ipf_err_t uft_ipf_writer_open(uft_ipf_writer_t *w, const char *path,
                                   uft_fourcc_t magic, bool big_endian,
                                   uint32_t header_mode) {
    if (!w || !path) return UFT_IPF_EINVAL;
    if (header_mode != 8 && header_mode != 12) return UFT_IPF_EINVAL;
    
    memset(w, 0, sizeof(*w));
    
    w->fp = fopen(path, "wb");
    if (!w->fp) return UFT_IPF_EIO;
    
    w->big_endian = big_endian;
    w->magic = magic;
    w->header_mode = header_mode;
    w->bytes_written = 0;
    w->chunk_count = 0;
    
    return UFT_IPF_OK;
}

uft_ipf_err_t uft_ipf_writer_write_header(uft_ipf_writer_t *w) {
    if (!w || !w->fp) return UFT_IPF_EINVAL;
    
    /* Write magic as first chunk ID */
    fwrite(w->magic.b, 1, 4, w->fp);
    w->bytes_written += 4;
    
    return UFT_IPF_OK;
}

uft_ipf_err_t uft_ipf_writer_add_chunk(uft_ipf_writer_t *w, uft_fourcc_t id,
                                        const void *data, uint32_t data_size,
                                        bool add_crc32) {
    if (!w || !w->fp) return UFT_IPF_EINVAL;
    if (!data && data_size > 0) return UFT_IPF_EINVAL;
    
    /* Write chunk ID */
    fwrite(id.b, 1, 4, w->fp);
    
    /* Write size */
    wr_u32(w->fp, data_size, w->big_endian);
    
    /* Write CRC if 12-byte header mode */
    if (w->header_mode == 12) {
        uint32_t crc = 0;
        if (add_crc32 && data && data_size > 0) {
            crc = uft_ipf_crc32(data, data_size);
        }
        wr_u32(w->fp, crc, w->big_endian);
    }
    
    /* Write data */
    if (data_size > 0) {
        if (fwrite(data, 1, data_size, w->fp) != data_size) {
            return UFT_IPF_EIO;
        }
    }
    
    w->bytes_written += (uint64_t)w->header_mode + data_size;
    w->chunk_count++;
    
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

/* ═══════════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

void uft_ipf_dump_info(const uft_ipf_t *ctx, FILE *out) {
    if (!ctx || !out) return;
    
    fprintf(out, "IPF Container: %s\n", ctx->path);
    fprintf(out, "  File size:  %llu bytes\n", (unsigned long long)ctx->file_size);
    fprintf(out, "  Endian:     %s\n", ctx->big_endian ? "Big" : "Little");
    fprintf(out, "  Magic:      %s\n", uft_fourcc_str(ctx->magic));
    fprintf(out, "  Chunks:     %zu\n", ctx->chunk_count);
    fprintf(out, "\n");
    
    for (size_t i = 0; i < ctx->chunk_count; i++) {
        const uft_ipf_chunk_t *c = &ctx->chunks[i];
        fprintf(out, "  [%02zu] %s  offset=%llu  size=%u  hdr=%u  crc=%08x\n",
                i, uft_fourcc_str(c->id),
                (unsigned long long)c->data_offset,
                c->data_size,
                c->header_size,
                c->crc32);
    }
}
