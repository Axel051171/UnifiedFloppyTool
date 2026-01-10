/**
 * @file uft_scl.c
 * @brief SCL Format Parser (Sinclair/TR-DOS Container)
 * @version 2.0.0 - Integrated from uft_drive_trx_scl_pack_v2
 * 
 * Clean-room implementation supporting:
 * - Parse/Validate/GetFile/Build operations
 * - Full TR-DOS compatibility
 * - Zero-copy data access
 */

#include "uft/formats/scl/uft_scl.h"
#include <string.h>
#include <stdlib.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * Internal Helpers
 * ═══════════════════════════════════════════════════════════════════════════════ */

static int scl_is_magic(const uint8_t *b, size_t n) {
    static const uint8_t m[8] = {'S','I','N','C','L','A','I','R'};
    return (n >= 9 && memcmp(b, m, 8) == 0);
}

static void scl_name_to_c(char out[9], const uint8_t in[8]) {
    size_t i;
    for (i = 0; i < 8; i++) {
        out[i] = (char)in[i];
    }
    out[8] = '\0';
    
    /* Trim trailing spaces */
    for (i = 8; i > 0; i--) {
        if (out[i-1] == ' ' || out[i-1] == '\0') {
            out[i-1] = '\0';
        } else {
            break;
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Public API
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_scl_validate(const uint8_t *buf, size_t len) {
    if (!buf) return UFT_ERR_INVALID_ARG;
    if (!scl_is_magic(buf, len)) return UFT_ERR_FORMAT;
    if (len < 9) return UFT_ERR_BUFFER_TOO_SMALL;

    uint8_t n = buf[8];
    size_t headers_off = 9;
    size_t headers_len = (size_t)n * 14u;
    if (len < headers_off + headers_len) return UFT_ERR_BUFFER_TOO_SMALL;

    size_t data_off = headers_off + headers_len;
    size_t needed = data_off;

    for (uint8_t i = 0; i < n; i++) {
        const uint8_t *h = buf + headers_off + (size_t)i * 14u;
        uint8_t sectors = h[13];
        needed += (size_t)sectors * 256u;
        if (needed > len) return UFT_ERR_CORRUPTED;
    }

    /* Some tools append extra bytes; we accept them but require core to be valid */
    return UFT_OK;
}

int uft_scl_parse(const uint8_t *buf, size_t len, uft_scl_t *out) {
    if (!buf || !out) return UFT_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));

    int st = uft_scl_validate(buf, len);
    if (st != UFT_OK) return st;

    uint8_t n = buf[8];
    out->file_count = n;

    if (n == 0) return UFT_OK;

    out->entries = (uft_scl_entry_t*)calloc((size_t)n, sizeof(uft_scl_entry_t));
    if (!out->entries) return UFT_ERR_NOMEM;

    size_t headers_off = 9;
    size_t headers_len = (size_t)n * 14u;
    size_t data_off = headers_off + headers_len;

    const uint8_t *p = buf + data_off;
    size_t remaining = len - data_off;

    for (uint8_t i = 0; i < n; i++) {
        const uint8_t *h = buf + headers_off + (size_t)i * 14u;
        uft_scl_entry_t *e = &out->entries[i];

        scl_name_to_c(e->name, h);
        e->type = h[8];
        e->param[0] = h[9];
        e->param[1] = h[10];
        e->param[2] = h[11];
        e->length_sectors = h[13];

        size_t need = (size_t)e->length_sectors * 256u;
        if (need > remaining) {
            uft_scl_free(out);
            return UFT_ERR_CORRUPTED;
        }

        p += need;
        remaining -= need;
    }

    out->data = buf + data_off;
    out->data_len = (size_t)(p - (buf + data_off));
    return UFT_OK;
}

void uft_scl_free(uft_scl_t *scl) {
    if (!scl) return;
    free(scl->entries);
    memset(scl, 0, sizeof(*scl));
}

int uft_scl_get_file(const uint8_t *buf, size_t len, size_t index,
                     uft_scl_entry_t *meta_out,
                     const uint8_t **data_out, size_t *data_len_out) {
    if (!buf || !data_out || !data_len_out) return UFT_ERR_INVALID_ARG;

    int st = uft_scl_validate(buf, len);
    if (st != UFT_OK) return st;

    uint8_t n = buf[8];
    if (index >= (size_t)n) return UFT_ERR_INVALID_ARG;

    size_t headers_off = 9;
    size_t headers_len = (size_t)n * 14u;
    size_t data_off = headers_off + headers_len;

    /* Walk to file offset */
    size_t off = data_off;
    for (size_t i = 0; i < index; i++) {
        const uint8_t *h = buf + headers_off + i * 14u;
        off += (size_t)h[13] * 256u;
    }
    
    const uint8_t *h = buf + headers_off + index * 14u;
    size_t file_len = (size_t)h[13] * 256u;
    if (off + file_len > len) return UFT_ERR_CORRUPTED;

    if (meta_out) {
        scl_name_to_c(meta_out->name, h);
        meta_out->type = h[8];
        meta_out->param[0] = h[9];
        meta_out->param[1] = h[10];
        meta_out->param[2] = h[11];
        meta_out->length_sectors = h[13];
    }

    *data_out = buf + off;
    *data_len_out = file_len;
    return UFT_OK;
}

int uft_scl_build(const uft_scl_entry_t *entries,
                  const uint8_t *const *file_data,
                  const size_t *file_sizes,
                  size_t file_count,
                  uint8_t **out_buf, size_t *out_len) {
    if (!out_buf || !out_len) return UFT_ERR_INVALID_ARG;
    *out_buf = NULL;
    *out_len = 0;

    if (file_count > 255) return UFT_ERR_INVALID_ARG;
    if (file_count && (!entries || !file_data || !file_sizes)) return UFT_ERR_INVALID_ARG;

    size_t headers_off = 9;
    size_t headers_len = file_count * 14u;
    size_t data_len = 0;

    for (size_t i = 0; i < file_count; i++) {
        /* Enforce sector-sized payload */
        if (file_sizes[i] % 256u != 0) return UFT_ERR_FORMAT;
        size_t sectors = file_sizes[i] / 256u;
        if (sectors > 255) return UFT_ERR_FORMAT;
        data_len += file_sizes[i];
    }

    size_t total = headers_off + headers_len + data_len;
    uint8_t *b = (uint8_t*)calloc(1, total);
    if (!b) return UFT_ERR_NOMEM;

    /* Magic */
    memcpy(b, "SINCLAIR", 8);
    b[8] = (uint8_t)file_count;

    /* Headers */
    size_t ho = headers_off;
    for (size_t i = 0; i < file_count; i++) {
        const uft_scl_entry_t *e = &entries[i];
        uint8_t *h = b + ho + i * 14u;

        /* Name: pad with spaces */
        for (size_t j = 0; j < 8; j++) {
            char c = e->name[j];
            h[j] = (uint8_t)(c ? c : ' ');
        }
        h[8] = e->type;
        h[9] = e->param[0];
        h[10] = e->param[1];
        h[11] = e->param[2];
        h[12] = 0; /* TR-DOS in-disk start sector; SCL omits */
        h[13] = (uint8_t)(file_sizes[i] / 256u);
    }

    /* Data concatenation */
    size_t doff = headers_off + headers_len;
    size_t pos = doff;
    for (size_t i = 0; i < file_count; i++) {
        memcpy(b + pos, file_data[i], file_sizes[i]);
        pos += file_sizes[i];
    }

    *out_buf = b;
    *out_len = total;
    return UFT_OK;
}
