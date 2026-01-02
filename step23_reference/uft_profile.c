#include "uft/uft_profile.h"

#include "uft/uft_params.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>

/* -------------------------------------------------------------------------- */
/* Minimal JSON builder                                                        */
/* -------------------------------------------------------------------------- */

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} sb_t;

static int sb_grow(sb_t *sb, size_t add)
{
    if (sb->len + add + 1 <= sb->cap) return 1;
    size_t ncap = sb->cap ? sb->cap : 256;
    while (ncap < sb->len + add + 1) ncap *= 2;
    char *nb = (char*)realloc(sb->buf, ncap);
    if (!nb) return 0;
    sb->buf = nb;
    sb->cap = ncap;
    return 1;
}

static int sb_puts(sb_t *sb, const char *s)
{
    if (!s) return 1;
    const size_t n = strlen(s);
    if (!sb_grow(sb, n)) return 0;
    memcpy(sb->buf + sb->len, s, n);
    sb->len += n;
    sb->buf[sb->len] = '\0';
    return 1;
}

static int sb_printf(sb_t *sb, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char tmp[512];
    int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (n < 0) return 0;
    if ((size_t)n < sizeof(tmp)) {
        return sb_puts(sb, tmp);
    }
    /* Large: allocate exact */
    char *dyn = (char*)malloc((size_t)n + 1);
    if (!dyn) return 0;
    va_start(ap, fmt);
    vsnprintf(dyn, (size_t)n + 1, fmt, ap);
    va_end(ap);
    int ok = sb_puts(sb, dyn);
    free(dyn);
    return ok;
}

static int sb_put_json_string(sb_t *sb, const char *s)
{
    if (!sb_puts(sb, "\"")) return 0;
    if (!s) s = "";
    for (const unsigned char *p = (const unsigned char*)s; *p; ++p) {
        switch (*p) {
        case '"': if (!sb_puts(sb, "\\\"")) return 0; break;
        case '\\': if (!sb_puts(sb, "\\\\")) return 0; break;
        case '\n': if (!sb_puts(sb, "\\n")) return 0; break;
        case '\r': if (!sb_puts(sb, "\\r")) return 0; break;
        case '\t': if (!sb_puts(sb, "\\t")) return 0; break;
        default:
            if (*p < 0x20) {
                if (!sb_printf(sb, "\\u%04x", (unsigned)(*p))) return 0;
            } else {
                char c[2] = { (char)*p, 0 };
                if (!sb_puts(sb, c)) return 0;
            }
        }
    }
    return sb_puts(sb, "\"");
}

static int is_true_str(const char *s)
{
    if (!s) return 0;
    if (!strcmp(s, "1")) return 1;
    if (!strcmp(s, "true")) return 1;
    if (!strcmp(s, "TRUE")) return 1;
    if (!strcmp(s, "yes")) return 1;
    if (!strcmp(s, "on")) return 1;
    return 0;
}

static int is_false_str(const char *s)
{
    if (!s) return 0;
    if (!strcmp(s, "0")) return 1;
    if (!strcmp(s, "false")) return 1;
    if (!strcmp(s, "FALSE")) return 1;
    if (!strcmp(s, "no")) return 1;
    if (!strcmp(s, "off")) return 1;
    return 0;
}

static int sb_put_default_value(sb_t *sb, const uft_param_def_t *d)
{
    const char *v = d && d->default_value ? d->default_value : "";
    switch (d->type) {
    case UFT_PARAM_BOOL:
        if (is_true_str(v)) return sb_puts(sb, "true");
        if (is_false_str(v)) return sb_puts(sb, "false");
        return sb_puts(sb, "false");
    case UFT_PARAM_INT: {
        /* Allow empty -> 0 */
        char *end = NULL;
        long iv = strtol(v, &end, 10);
        if (!end || end == v) iv = 0;
        return sb_printf(sb, "%ld", iv);
    }
    case UFT_PARAM_FLOAT: {
        char *end = NULL;
        double dv = strtod(v, &end);
        if (!end || end == v) dv = 0.0;
        /* Keep it compact but stable enough */
        return sb_printf(sb, "%.6g", dv);
    }
    case UFT_PARAM_ENUM:
    case UFT_PARAM_STRING:
    default:
        return sb_put_json_string(sb, v);
    }
}

static int sb_put_defaults_object(sb_t *sb, const uft_param_def_t *defs, size_t count)
{
    if (!sb_puts(sb, "{")) return 0;
    int first = 1;
    for (size_t i = 0; i < count; ++i) {
        const uft_param_def_t *d = &defs[i];
        if (!d->key) continue;
        if (!first) { if (!sb_puts(sb, ",")) return 0; }
        first = 0;
        if (!sb_put_json_string(sb, d->key)) return 0;
        if (!sb_puts(sb, ":")) return 0;
        if (!sb_put_default_value(sb, d)) return 0;
    }
    return sb_puts(sb, "}");
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                  */
/* -------------------------------------------------------------------------- */

char *uft_format_profile_json(uft_disk_format_id_t fmt)
{
    const uft_format_spec_t *spec = uft_format_find_by_id(fmt);
    if (!spec) spec = uft_format_find_by_id(UFT_DISK_FORMAT_UNKNOWN);
    if (!spec) return NULL;

    sb_t sb;
    memset(&sb, 0, sizeof(sb));

    /* Format core */
    if (!sb_puts(&sb, "{")) goto fail;
    if (!sb_puts(&sb, "\"format\":{")) goto fail;
    if (!sb_puts(&sb, "\"id\":")) goto fail;
    if (!sb_printf(&sb, "%d", (int)spec->id)) goto fail;
    if (!sb_puts(&sb, ",\"name\":")) goto fail;
    if (!sb_put_json_string(&sb, spec->name ? spec->name : "")) goto fail;
    if (!sb_puts(&sb, ",\"description\":")) goto fail;
    if (!sb_put_json_string(&sb, spec->description ? spec->description : "")) goto fail;
    if (!sb_printf(&sb, ",\"tracks\":%u,\"heads\":%u,\"sectors_per_track\":%u,\"sector_size\":%u", (unsigned)spec->tracks, (unsigned)spec->heads, (unsigned)spec->sectors_per_track, (unsigned)spec->sector_size)) goto fail;
    if (!sb_printf(&sb, ",\"encoding\":%d,\"bitrate\":%u,\"rpm\":%u", (int)spec->encoding, (unsigned)spec->bitrate, (unsigned)spec->rpm)) goto fail;
    if (!sb_puts(&sb, "}")) goto fail;

    /* Outputs */
    if (!sb_puts(&sb, ",\"outputs\":[")) goto fail;
    {
        uft_output_format_t outs[16];
        size_t n = uft_output_mask_to_list(spec->output_mask, outs, 16);
        if (n == 0) {
            outs[0] = UFT_OUTPUT_RAW_IMG;
            n = 1;
        }
        for (size_t i = 0; i < n; ++i) {
            const uft_output_format_t of = outs[i];
            if (i) { if (!sb_puts(&sb, ",")) goto fail; }
            if (!sb_puts(&sb, "{")) goto fail;
            if (!sb_printf(&sb, "\"id\":%d,", (int)of)) goto fail;
            if (!sb_puts(&sb, "\"name\":")) goto fail;
            if (!sb_put_json_string(&sb, uft_output_format_name(of))) goto fail;
            if (!sb_puts(&sb, ",\"ext\":")) goto fail;
            if (!sb_put_json_string(&sb, uft_output_format_ext(of))) goto fail;
            if (!sb_puts(&sb, "}")) goto fail;
        }
    }
    if (!sb_puts(&sb, "]")) goto fail;

    /* Defaults */
    if (!sb_puts(&sb, ",\"defaults\":{")) goto fail;

    /* Recovery defaults */
    {
        size_t n = 0;
        const uft_param_def_t *defs = uft_recovery_param_defs(&n);
        if (!sb_puts(&sb, "\"recovery\":")) goto fail;
        if (!sb_put_defaults_object(&sb, defs, n)) goto fail;
    }

    /* Per-format defaults */
    {
        size_t n = 0;
        const uft_param_def_t *defs = uft_format_param_defs(fmt, &n);
        if (!sb_puts(&sb, ",\"format\":")) goto fail;
        if (!sb_put_defaults_object(&sb, defs, n)) goto fail;
    }

    /* Export defaults per supported output */
    if (!sb_puts(&sb, ",\"export\":{")) goto fail;
    {
        uft_output_format_t outs[16];
        size_t nouts = uft_output_mask_to_list(spec->output_mask, outs, 16);
        if (nouts == 0) {
            outs[0] = UFT_OUTPUT_RAW_IMG;
            nouts = 1;
        }
        for (size_t i = 0; i < nouts; ++i) {
            const uft_output_format_t of = outs[i];
            size_t n = 0;
            const uft_param_def_t *defs = uft_output_param_defs(of, &n);
            const char *ext = uft_output_format_ext(of);
            if (i) { if (!sb_puts(&sb, ",")) goto fail; }
            if (!sb_put_json_string(&sb, ext)) goto fail;
            if (!sb_puts(&sb, ":")) goto fail;
            if (!sb_put_defaults_object(&sb, defs, n)) goto fail;
        }
    }
    if (!sb_puts(&sb, "}")) goto fail; /* export */
    if (!sb_puts(&sb, "}")) goto fail; /* defaults */
    if (!sb_puts(&sb, "}")) goto fail; /* root */

    return sb.buf;

fail:
    free(sb.buf);
    return NULL;
}

void uft_profile_free(char *p)
{
    free(p);
}
