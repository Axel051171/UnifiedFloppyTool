#include "uft/uft_error.h"
#include "imd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------- internal data model ---------------- */

struct uft_imd_sector {
    uint16_t cyl_log;
    uint8_t  head_log;
    uint8_t  sec_id;         /* sector ID from sector numbering map */

    uint32_t size;           /* byte length of this sector */
    uint8_t  deleted_dam;    /* 0/1 */
    uint8_t  bad_crc;        /* 0/1 */
    uint8_t  unavailable;    /* 0/1 */

    uint8_t* data;           /* expanded data bytes (NULL if unavailable) */
};

struct uft_imd_track {
    /* Original track header fields */
    uint8_t  mode;
    uint8_t  cyl_phys;
    uint8_t  head_flags;      /* includes phys head and optional map flags */
    uint8_t  nsec;
    uint8_t  ssize_code;

    /* Maps as stored (sector ids + optional cyl/head maps + optional size table) */
    uint8_t* sec_map;         /* [nsec] */
    uint8_t* cyl_map;         /* [nsec] or NULL */
    uint8_t* head_map;        /* [nsec] or NULL */
    uint16_t* size_tbl;       /* [nsec] or NULL (ssize_code==0xFF) */

    /* Parsed sectors in the same order as sec_map (so we can rebuild) */
    uft_imd_sector_t* sectors; /* [nsec] */
};

/* ---------------- helpers ---------------- */

static uint32_t ssize_from_code(uint8_t code)
{
    if (code <= 6) return (uint32_t)(128U << code);
    return 0;
}

static int read_u8(FILE* fp, uint8_t* out)
{
    int c = fgetc(fp);
    if (c == EOF) return UFT_ERR_FORMAT;
    *out = (uint8_t)c;
    return UFT_SUCCESS;
}

static int read_exact(FILE* fp, void* buf, size_t n)
{
    if (n == 0) return UFT_SUCCESS;
    size_t r = fread(buf, 1, n, fp);
    return (r == n) ? UFT_SUCCESS : UFT_ERR_FORMAT;
}

static int write_exact(FILE* fp, const void* buf, size_t n)
{
    if (n == 0) return UFT_SUCCESS;
    size_t w = fwrite(buf, 1, n, fp);
    return (w == n) ? UFT_SUCCESS : UFT_ERR_IO;
}

static uint16_t le16_read(const uint8_t b[2])
{
    return (uint16_t)b[0] | ((uint16_t)b[1] << 8);
}

static void le16_write(uint8_t b[2], uint16_t v)
{
    b[0] = (uint8_t)(v & 0xFF);
    b[1] = (uint8_t)((v >> 8) & 0xFF);
}

static int ensure_header_ends_with_1a(const uint8_t* hdr, size_t len)
{
    if (!hdr || len < 5) return UFT_ERR_FORMAT;
    if (memcmp(hdr, "IMD ", 4) != 0) return UFT_ERR_FORMAT;
    if (hdr[len - 1] != 0x1A) return UFT_ERR_FORMAT;
    return UFT_SUCCESS;
}

static void observe_geom(uft_imd_ctx_t* ctx, uint16_t cyl, uint8_t head)
{
    if (!ctx) return;
    if ((uint32_t)cyl + 1U > ctx->max_track_plus1) ctx->max_track_plus1 = (uint16_t)(cyl + 1U);
    if ((uint32_t)head + 1U > ctx->max_head_plus1) ctx->max_head_plus1 = (uint8_t)(head + 1U);
}

static uft_imd_sector_t* find_sector(uft_imd_ctx_t* ctx, uint8_t head, uint8_t track, uint8_t sector_id)
{
    if (!ctx || !ctx->tracks) return NULL;
    for (size_t t = 0; t < ctx->track_count; t++) {
        uft_imd_track_t* tr = &ctx->tracks[t];
        for (uint8_t i = 0; i < tr->nsec; i++) {
            uft_imd_sector_t* s = &tr->sectors[i];
            if (s->cyl_log == (uint16_t)track && s->head_log == head && s->sec_id == sector_id) {
                return s;
            }
        }
    }
    return NULL;
}

static int rec_to_flags(uint8_t rec, uint8_t* out_deleted, uint8_t* out_bad)
{
    if (!out_deleted || !out_bad) return UFT_ERR_INVALID_ARG;
    *out_deleted = 0;
    *out_bad = 0;

    switch (rec) {
        case UFT_IMD_REC_UNAVAILABLE:
            return UFT_SUCCESS;

        case UFT_IMD_REC_NORMAL:
        case UFT_IMD_REC_COMPRESSED:
            return UFT_SUCCESS;

        case UFT_IMD_REC_NORMAL_DELETED_DAM:
        case UFT_IMD_REC_COMPRESSED_DELETED_DAM:
            *out_deleted = 1;
            return UFT_SUCCESS;

        case UFT_IMD_REC_NORMAL_DATA_ERROR:
        case UFT_IMD_REC_COMPRESSED_DATA_ERROR:
            *out_bad = 1;
            return UFT_SUCCESS;

        case UFT_IMD_REC_DELETED_DATA_ERROR:
        case UFT_IMD_REC_COMPRESSED_DEL_DATA_ERR:
            *out_deleted = 1;
            *out_bad = 1;
            return UFT_SUCCESS;

        default:
            return UFT_ERR_FORMAT;
    }
}

static uint8_t flags_to_normal_rec(uint8_t deleted, uint8_t bad)
{
    if (deleted && bad) return UFT_IMD_REC_DELETED_DATA_ERROR; /* 0x07 */
    if (deleted)        return UFT_IMD_REC_NORMAL_DELETED_DAM; /* 0x03 */
    if (bad)            return UFT_IMD_REC_NORMAL_DATA_ERROR;  /* 0x05 */
    return UFT_IMD_REC_NORMAL;                                 /* 0x01 */
}

static int ctx_add_track(uft_imd_ctx_t* ctx, const uft_imd_track_t* tr_in)
{
    size_t new_count = ctx->track_count + 1;
    uft_imd_track_t* p = (uft_imd_track_t*)realloc(ctx->tracks, new_count * sizeof(uft_imd_track_t));
    if (!p) return UFT_IMD_ERR_NOMEM;
    ctx->tracks = p;
    ctx->tracks[ctx->track_count] = *tr_in;
    ctx->track_count = new_count;
    return UFT_SUCCESS;
}

static int read_comment_block(FILE* fp, uint8_t** out_hdr, size_t* out_len)
{
    /* We already read "IMD " elsewhere; here we rewind and read full header up to 0x1A. */
    if (!fp || !out_hdr || !out_len) return UFT_ERR_INVALID_ARG;

    if (fseek(fp, 0, SEEK_SET) != 0) return UFT_ERR_IO;

    size_t cap = 4096;
    uint8_t* buf = (uint8_t*)malloc(cap);
    if (!buf) return UFT_IMD_ERR_NOMEM;

    size_t len = 0;
    int c;
    while ((c = fgetc(fp)) != EOF) {
        if (len == cap) {
            cap *= 2;
            uint8_t* nb = (uint8_t*)realloc(buf, cap);
            if (!nb) { free(buf); return UFT_IMD_ERR_NOMEM; }
            buf = nb;
        }
        buf[len++] = (uint8_t)c;
        if ((uint8_t)c == 0x1A) break;
        /* avoid infinite on corrupt file */
        if (len > (1024U * 1024U)) { free(buf); return UFT_ERR_FORMAT; }
    }

    if (len < 5 || buf[len - 1] != 0x1A) { free(buf); return UFT_ERR_FORMAT; }
    if (memcmp(buf, "IMD ", 4) != 0) { free(buf); return UFT_ERR_FORMAT; }

    *out_hdr = buf;
    *out_len = len;
    return UFT_SUCCESS;
}

static int parse_track(FILE* fp, uft_imd_ctx_t* ctx, uft_imd_track_t* out_tr)
{
    memset(out_tr, 0, sizeof(*out_tr));

    uint8_t mode=0, cyl=0, head_flags=0, nsec=0, ssize_code=0;
    int rc = read_u8(fp, &mode);
    if (rc != UFT_SUCCESS) return rc; /* EOF / corrupt */

    if ((rc = read_u8(fp, &cyl)) != UFT_SUCCESS) return rc;
    if ((rc = read_u8(fp, &head_flags)) != UFT_SUCCESS) return rc;
    if ((rc = read_u8(fp, &nsec)) != UFT_SUCCESS) return rc;
    if ((rc = read_u8(fp, &ssize_code)) != UFT_SUCCESS) return rc;

    if (nsec == 0) return UFT_ERR_FORMAT;

    const bool has_cyl_map  = (head_flags & 0x80) != 0;
    const bool has_head_map = (head_flags & 0x40) != 0;
    const uint8_t phys_head = (uint8_t)(head_flags & 0x01);

    out_tr->mode = mode;
    out_tr->cyl_phys = cyl;
    out_tr->head_flags = head_flags;
    out_tr->nsec = nsec;
    out_tr->ssize_code = ssize_code;

    out_tr->sec_map = (uint8_t*)malloc((size_t)nsec);
    if (!out_tr->sec_map) return UFT_IMD_ERR_NOMEM;
    if ((rc = read_exact(fp, out_tr->sec_map, (size_t)nsec)) != UFT_SUCCESS) return rc;

    if (has_cyl_map) {
        out_tr->cyl_map = (uint8_t*)malloc((size_t)nsec);
        if (!out_tr->cyl_map) return UFT_IMD_ERR_NOMEM;
        if ((rc = read_exact(fp, out_tr->cyl_map, (size_t)nsec)) != UFT_SUCCESS) return rc;
    }
    if (has_head_map) {
        out_tr->head_map = (uint8_t*)malloc((size_t)nsec);
        if (!out_tr->head_map) return UFT_IMD_ERR_NOMEM;
        if ((rc = read_exact(fp, out_tr->head_map, (size_t)nsec)) != UFT_SUCCESS) return rc;
    }

    uint32_t fixed = 0;
    if (ssize_code == 0xFF) {
        out_tr->size_tbl = (uint16_t*)calloc((size_t)nsec, sizeof(uint16_t));
        if (!out_tr->size_tbl) return UFT_IMD_ERR_NOMEM;

        for (uint8_t i = 0; i < nsec; i++) {
            uint8_t b[2];
            if ((rc = read_exact(fp, b, 2)) != UFT_SUCCESS) return rc;
            out_tr->size_tbl[i] = le16_read(b);
            if (out_tr->size_tbl[i] == 0) return UFT_ERR_FORMAT;
        }
    } else {
        fixed = ssize_from_code(ssize_code);
        if (fixed == 0) return UFT_ERR_FORMAT;
    }

    out_tr->sectors = (uft_imd_sector_t*)calloc((size_t)nsec, sizeof(uft_imd_sector_t));
    if (!out_tr->sectors) return UFT_IMD_ERR_NOMEM;

    for (uint8_t i = 0; i < nsec; i++) {
        uint8_t rec = 0;
        if ((rc = read_u8(fp, &rec)) != UFT_SUCCESS) return rc;

        uint8_t deleted = 0, bad = 0;
        rc = rec_to_flags(rec, &deleted, &bad);
        if (rc != UFT_SUCCESS) return rc;

        uint16_t cyl_log = out_tr->cyl_map ? (uint16_t)out_tr->cyl_map[i] : (uint16_t)cyl;
        uint8_t head_log = out_tr->head_map ? out_tr->head_map[i] : phys_head;
        uint8_t sec_id   = out_tr->sec_map[i];
        uint32_t sz      = out_tr->size_tbl ? (uint32_t)out_tr->size_tbl[i] : fixed;

        uft_imd_sector_t* s = &out_tr->sectors[i];
        memset(s, 0, sizeof(*s));
        s->cyl_log = cyl_log;
        s->head_log = head_log;
        s->sec_id = sec_id;
        s->size = sz;
        s->deleted_dam = deleted;
        s->bad_crc = bad;

        if (rec == UFT_IMD_REC_UNAVAILABLE) {
            s->unavailable = 1;
            s->data = NULL;
        } else if (rec == UFT_IMD_REC_COMPRESSED ||
                   rec == UFT_IMD_REC_COMPRESSED_DELETED_DAM ||
                   rec == UFT_IMD_REC_COMPRESSED_DATA_ERROR ||
                   rec == UFT_IMD_REC_COMPRESSED_DEL_DATA_ERR) {

            uint8_t fill = 0;
            if ((rc = read_u8(fp, &fill)) != UFT_SUCCESS) return rc;

            s->data = (uint8_t*)malloc((size_t)sz);
            if (!s->data) return UFT_IMD_ERR_NOMEM;
            memset(s->data, (int)fill, (size_t)sz);
        } else {
            /* normal sector payload bytes */
            s->data = (uint8_t*)malloc((size_t)sz);
            if (!s->data) return UFT_IMD_ERR_NOMEM;
            if ((rc = read_exact(fp, s->data, (size_t)sz)) != UFT_SUCCESS) return rc;
        }

        observe_geom(ctx, cyl_log, head_log);
    }

    return UFT_SUCCESS;
}

static void free_track(uft_imd_track_t* tr)
{
    if (!tr) return;

    if (tr->sectors) {
        for (uint8_t i = 0; i < tr->nsec; i++) {
            free(tr->sectors[i].data);
        }
    }

    free(tr->sectors);
    free(tr->sec_map);
    free(tr->cyl_map);
    free(tr->head_map);
    free(tr->size_tbl);

    memset(tr, 0, sizeof(*tr));
}

static int write_track(FILE* fp, const uft_imd_track_t* tr)
{
    if (!fp || !tr) return UFT_ERR_INVALID_ARG;

    int rc = 0;
    uint8_t hdr[5] = { tr->mode, tr->cyl_phys, tr->head_flags, tr->nsec, tr->ssize_code };
    if ((rc = write_exact(fp, hdr, sizeof(hdr))) != UFT_SUCCESS) return rc;

    if ((rc = write_exact(fp, tr->sec_map, (size_t)tr->nsec)) != UFT_SUCCESS) return rc;
    if (tr->cyl_map)  if ((rc = write_exact(fp, tr->cyl_map, (size_t)tr->nsec)) != UFT_SUCCESS) return rc;
    if (tr->head_map) if ((rc = write_exact(fp, tr->head_map, (size_t)tr->nsec)) != UFT_SUCCESS) return rc;

    if (tr->ssize_code == 0xFF) {
        /* size table */
        for (uint8_t i = 0; i < tr->nsec; i++) {
            uint8_t b[2];
            le16_write(b, tr->size_tbl[i]);
            if ((rc = write_exact(fp, b, 2)) != UFT_SUCCESS) return rc;
        }
    }

    /* sector records: we write normal records (uncompressed), preserving deleted/bad flags */
    for (uint8_t i = 0; i < tr->nsec; i++) {
        const uft_imd_sector_t* s = &tr->sectors[i];

        if (s->unavailable || !s->data) {
            uint8_t rec = UFT_IMD_REC_UNAVAILABLE;
            if ((rc = write_exact(fp, &rec, 1)) != UFT_SUCCESS) return rc;
            continue;
        }

        uint8_t rec = flags_to_normal_rec(s->deleted_dam, s->bad_crc);
        if ((rc = write_exact(fp, &rec, 1)) != UFT_SUCCESS) return rc;
        if ((rc = write_exact(fp, s->data, (size_t)s->size)) != UFT_SUCCESS) return rc;
    }

    return UFT_SUCCESS;
}

/* ---------------- public API ---------------- */

bool uft_imd_detect(const uint8_t* buffer, size_t size)
{
    if (!buffer || size < 5) return false;
    if (memcmp(buffer, "IMD ", 4) != 0) return false;

    /* Find 0x1A within a reasonable prefix. */
    size_t limit = (size < (64 * 1024)) ? size : (64 * 1024);
    for (size_t i = 4; i < limit; i++) {
        if (buffer[i] == 0x1A) return true;
    }
    return false;
}

int uft_imd_open(uft_imd_ctx_t* ctx, const char* path)
{
    if (!ctx || !path) return UFT_ERR_INVALID_ARG;
    memset(ctx, 0, sizeof(*ctx));

    FILE* fp = fopen(path, "rb");
    if (!fp) return UFT_ERR_IO;

    /* read header/comment block bytes */
    int rc = read_comment_block(fp, &ctx->header, &ctx->header_len);
    if (rc != UFT_SUCCESS) { fclose(fp); uft_imd_close(ctx); return rc; }
    if (ensure_header_ends_with_1a(ctx->header, ctx->header_len) != UFT_SUCCESS) { fclose(fp); uft_imd_close(ctx); return UFT_ERR_FORMAT; }

    /* file position is at end of comment (right after 0x1A) */
    /* parse tracks until EOF */
    while (1) {
        int c = fgetc(fp);
        if (c == EOF) break;
        ungetc(c, fp);

        uft_imd_track_t tr;
        memset(&tr, 0, sizeof(tr));

        rc = parse_track(fp, ctx, &tr);
        if (rc != UFT_SUCCESS) { free_track(&tr); fclose(fp); uft_imd_close(ctx); return rc; }

        rc = ctx_add_track(ctx, &tr);
        if (rc != UFT_SUCCESS) { free_track(&tr); fclose(fp); uft_imd_close(ctx); return rc; }
    }

    fclose(fp);

    size_t path_len = strlen(path);
    ctx->path = (char*)malloc(path_len + 1);
    if (!ctx->path) { uft_imd_close(ctx); return UFT_IMD_ERR_NOMEM; }
    memcpy(ctx->path, path, path_len + 1);

    ctx->dirty = false;

    if (ctx->track_count == 0) { uft_imd_close(ctx); return UFT_ERR_FORMAT; }
    return UFT_SUCCESS;
}

int uft_imd_read_sector(uft_imd_ctx_t* ctx,
                    uint8_t head,
                    uint8_t track,
                    uint8_t sector,
                    uint8_t* out_data,
                    size_t out_len,
                    uft_imd_sector_meta_t* meta)
{
    if (!ctx || !out_data) return UFT_ERR_INVALID_ARG;

    uft_imd_sector_t* s = find_sector(ctx, head, track, sector);
    if (!s) return UFT_IMD_ERR_NOTFOUND;
    if (s->unavailable || !s->data) return UFT_ERR_FORMAT;
    if (out_len < (size_t)s->size) return UFT_ERR_INVALID_ARG;

    memcpy(out_data, s->data, (size_t)s->size);

    if (meta) {
        memset(meta, 0, sizeof(*meta));
        meta->deleted_dam = s->deleted_dam;
        meta->bad_crc = s->bad_crc;
        meta->has_weak_bits = 0;
        meta->has_timing = 0;
    }

    return (int)s->size;
}

int uft_imd_write_sector(uft_imd_ctx_t* ctx,
                     uint8_t head,
                     uint8_t track,
                     uint8_t sector,
                     const uint8_t* in_data,
                     size_t in_len,
                     const uft_imd_sector_meta_t* meta)
{
    if (!ctx || !in_data) return UFT_ERR_INVALID_ARG;

    uft_imd_sector_t* s = find_sector(ctx, head, track, sector);
    if (!s) return UFT_IMD_ERR_NOTFOUND;
    if (s->unavailable) return UFT_ERR_FORMAT;
    if (!s->data) return UFT_ERR_FORMAT;
    if (in_len != (size_t)s->size) return UFT_IMD_ERR_RANGE;

    memcpy(s->data, in_data, (size_t)s->size);

    if (meta) {
        s->deleted_dam = (meta->deleted_dam != 0);
        s->bad_crc = (meta->bad_crc != 0);
        /* weak/timing not supported */
    }

    ctx->dirty = true;
    return (int)s->size;
}

int uft_imd_to_raw(uft_imd_ctx_t* ctx, const char* output_path)
{
    if (!ctx || !output_path) return UFT_ERR_INVALID_ARG;

    FILE* out = fopen(output_path, "wb");
    if (!out) return UFT_ERR_IO;

    int rc = UFT_SUCCESS;

    for (size_t t = 0; t < ctx->track_count; t++) {
        const uft_imd_track_t* tr = &ctx->tracks[t];
        for (uint8_t i = 0; i < tr->nsec; i++) {
            const uft_imd_sector_t* s = &tr->sectors[i];
            if (s->unavailable || !s->data) { rc = UFT_ERR_FORMAT; goto done; }
            if ((rc = write_exact(out, s->data, (size_t)s->size)) != UFT_SUCCESS) goto done;
        }
    }

done:
    fclose(out);
    return rc;
}

static uint8_t ssize_code_from_bytes(uint16_t sec_size)
{
    /* return code such that 128<<code == sec_size, else 0xFF */
    uint16_t v = 128;
    for (uint8_t c = 0; c <= 6; c++) {
        if (v == sec_size) return c;
        v <<= 1;
    }
    return 0xFF;
}

int uft_imd_from_raw_pc(const char* raw_path,
                    const char* output_imd_path,
                    const uft_imd_pc_geom_t* geom,
                    const char* comment_ascii)
{
    if (!raw_path || !output_imd_path || !geom) return UFT_ERR_INVALID_ARG;
    if (geom->cylinders == 0 || geom->heads == 0 || geom->spt == 0) return UFT_ERR_INVALID_ARG;
    if (!(geom->heads == 1 || geom->heads == 2)) return UFT_ERR_INVALID_ARG;
    if (geom->sector_size < 128 || geom->sector_size > 8192) return UFT_ERR_INVALID_ARG;

    FILE* in = fopen(raw_path, "rb");
    if (!in) return UFT_ERR_IO;

    /* verify input size */
    if (fseek(in, 0, SEEK_END) != 0) { fclose(in); return UFT_ERR_IO; }
    long sz = ftell(in);
    if (sz < 0) { fclose(in); return UFT_ERR_IO; }
    if (fseek(in, 0, SEEK_SET) != 0) { fclose(in); return UFT_ERR_IO; }

    uint64_t expected = (uint64_t)geom->cylinders * (uint64_t)geom->heads * (uint64_t)geom->spt * (uint64_t)geom->sector_size;
    if ((uint64_t)sz != expected) { fclose(in); return UFT_ERR_FORMAT; }

    FILE* out = fopen(output_imd_path, "wb");
    if (!out) { fclose(in); return UFT_ERR_IO; }

    /* Write comment header. Must start with "IMD " and end with 0x1A. */
    const char* cmt = comment_ascii ? comment_ascii : "IMD UFT v2.8.7\r\n";
    /* Ensure it begins with "IMD " (ImageDisk requires it) */
    if (memcmp(cmt, "IMD ", 4) != 0) {
        const char* prefix = "IMD UFT v2.8.7\r\n";
        if (write_exact(out, prefix, strlen(prefix)) != UFT_SUCCESS) { fclose(in); fclose(out); return UFT_ERR_IO; }
    } else {
        if (write_exact(out, cmt, strlen(cmt)) != UFT_SUCCESS) { fclose(in); fclose(out); return UFT_ERR_IO; }
    }
    uint8_t eot = 0x1A;
    if (write_exact(out, &eot, 1) != UFT_SUCCESS) { fclose(in); fclose(out); return UFT_ERR_IO; }

    /* Determine sector size code. If not representable, we write size table. */
    uint8_t code = ssize_code_from_bytes(geom->sector_size);
    const bool use_tbl = (code == 0xFF);

    /* per track: mode=2 (MFM) is a sane default for PC DD/HD. */
    uint8_t* sec_map = (uint8_t*)malloc((size_t)geom->spt);
    if (!sec_map) { fclose(in); fclose(out); return UFT_IMD_ERR_NOMEM; }
    for (uint16_t s = 0; s < geom->spt; s++) sec_map[s] = (uint8_t)(geom->start_sector_id + s);

    uint16_t* size_tbl = NULL;
    if (use_tbl) {
        size_tbl = (uint16_t*)malloc((size_t)geom->spt * sizeof(uint16_t));
        if (!size_tbl) { free(sec_map); fclose(in); fclose(out); return UFT_IMD_ERR_NOMEM; }
        for (uint16_t s = 0; s < geom->spt; s++) size_tbl[s] = geom->sector_size;
    }

    uint8_t* buf = (uint8_t*)malloc((size_t)geom->sector_size);
    if (!buf) { free(sec_map); free(size_tbl); fclose(in); fclose(out); return UFT_IMD_ERR_NOMEM; }

    int rc = UFT_SUCCESS;

    for (uint16_t cyl = 0; cyl < geom->cylinders; cyl++) {
        for (uint8_t head = 0; head < geom->heads; head++) {
            /* track header */
            uint8_t tr_hdr[5];
            tr_hdr[0] = 2; /* mode: MFM 500kbps (typical PC) */
            tr_hdr[1] = (uint8_t)cyl;
            tr_hdr[2] = (uint8_t)(head & 0x01);
            tr_hdr[3] = (uint8_t)geom->spt;
            tr_hdr[4] = use_tbl ? 0xFF : code;

            if ((rc = write_exact(out, tr_hdr, sizeof(tr_hdr))) != UFT_SUCCESS) goto done;
            if ((rc = write_exact(out, sec_map, (size_t)geom->spt)) != UFT_SUCCESS) goto done;

            if (use_tbl) {
                for (uint16_t i = 0; i < geom->spt; i++) {
                    uint8_t b[2];
                    le16_write(b, size_tbl[i]);
                    if ((rc = write_exact(out, b, 2)) != UFT_SUCCESS) goto done;
                }
            }

            /* sector records */
            for (uint16_t i = 0; i < geom->spt; i++) {
                uint8_t rec = UFT_IMD_REC_NORMAL;
                if ((rc = write_exact(out, &rec, 1)) != UFT_SUCCESS) goto done;

                size_t r = fread(buf, 1, (size_t)geom->sector_size, in);
                if (r != (size_t)geom->sector_size) { rc = UFT_ERR_IO; goto done; }

                if ((rc = write_exact(out, buf, (size_t)geom->sector_size)) != UFT_SUCCESS) goto done;
            }
        }
    }

done:
    free(buf);
    free(sec_map);
    free(size_tbl);
    fclose(in);
    fclose(out);
    return rc;
}

int uft_imd_save(uft_imd_ctx_t* ctx)
{
    if (!ctx || !ctx->path) return UFT_ERR_INVALID_ARG;
    if (!ctx->dirty) return UFT_SUCCESS;

    /* Write to temp then replace (best-effort, no atomic rename guarantees on all FS) */
    size_t tmp_len = strlen(ctx->path) + 16;
    char* tmp = (char*)malloc(tmp_len);
    if (!tmp) return UFT_IMD_ERR_NOMEM;
    snprintf(tmp, tmp_len, "%s.tmp", ctx->path);

    FILE* out = fopen(tmp, "wb");
    if (!out) { free(tmp); return UFT_ERR_IO; }

    int rc = UFT_SUCCESS;

    if ((rc = write_exact(out, ctx->header, ctx->header_len)) != UFT_SUCCESS) goto done;

    for (size_t t = 0; t < ctx->track_count; t++) {
        rc = write_track(out, &ctx->tracks[t]);
        if (rc != UFT_SUCCESS) goto done;
    }

done:
    fclose(out);
    if (rc == UFT_SUCCESS) {
        /* replace original */
        remove(ctx->path); /* ignore errors */
        if (rename(tmp, ctx->path) != 0) {
            /* if rename fails, keep tmp as evidence */
            rc = UFT_ERR_IO;
        } else {
            ctx->dirty = false;
        }
    }
    free(tmp);
    return rc;
}

void uft_imd_close(uft_imd_ctx_t* ctx)
{
    if (!ctx) return;

    if (ctx->tracks) {
        for (size_t t = 0; t < ctx->track_count; t++) {
            free_track(&ctx->tracks[t]);
        }
    }
    free(ctx->tracks);
    free(ctx->header);
    free(ctx->path);

    memset(ctx, 0, sizeof(*ctx));
}
