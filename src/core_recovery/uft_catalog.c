#include "uft/uft_catalog.h"

#include "uft/uft_profile.h"
#include "uft/uft_params.h"
#include "uft/uft_output.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* Reuse the tiny string builder used in uft_profile.c, duplicated here to
 * keep the module standalone and C99-friendly. */
typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} sb_t;

static int sb_grow(sb_t *sb, size_t add)
{
    if (sb->len + add + 1 <= sb->cap) return 1;
    size_t ncap = sb->cap ? sb->cap : 512;
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

static const char *param_type_name(uft_param_type_t t)
{
    switch (t) {
    case UFT_PARAM_BOOL: return "bool";
    case UFT_PARAM_INT: return "int";
    case UFT_PARAM_FLOAT: return "float";
    case UFT_PARAM_STRING: return "string";
    case UFT_PARAM_ENUM: return "enum";
    default: return "unknown";
    }
}

static int sb_put_param_def(sb_t *sb, const uft_param_def_t *d)
{
    if (!d) return 0;
    if (!sb_puts(sb, "{")) return 0;
    if (!sb_puts(sb, "\"key\":")) return 0;
    if (!sb_put_json_string(sb, d->key ? d->key : "")) return 0;

    if (!sb_puts(sb, ",\"label\":")) return 0;
    if (!sb_put_json_string(sb, d->label ? d->label : "")) return 0;

    if (!sb_puts(sb, ",\"type\":")) return 0;
    if (!sb_put_json_string(sb, param_type_name(d->type))) return 0;

    if (!sb_puts(sb, ",\"help\":")) return 0;
    if (!sb_put_json_string(sb, d->help ? d->help : "")) return 0;

    if (!sb_puts(sb, ",\"default\":")) return 0;
    if (!sb_put_json_string(sb, d->default_value ? d->default_value : "")) return 0;

    /* min/max/step: always present for predictable parsing */
    if (!sb_printf(sb, ",\"min\":%.10g,\"max\":%.10g,\"step\":%.10g",
                   d->min_value, d->max_value, d->step)) return 0;

    if (!sb_puts(sb, ",\"enum\":")) return 0;
    if (d->type == UFT_PARAM_ENUM && d->enum_values && d->enum_count) {
        if (!sb_puts(sb, "[")) return 0;
        for (size_t i = 0; i < d->enum_count; ++i) {
            if (i) { if (!sb_puts(sb, ",")) return 0; }
            if (!sb_put_json_string(sb, d->enum_values[i] ? d->enum_values[i] : "")) return 0;
        }
        if (!sb_puts(sb, "]")) return 0;
    } else {
        if (!sb_puts(sb, "[]")) return 0;
    }

    return sb_puts(sb, "}");
}

static int sb_put_param_defs_array(sb_t *sb, const uft_param_def_t *defs, size_t count)
{
    if (!sb_puts(sb, "[")) return 0;
    for (size_t i = 0; i < count; ++i) {
        if (i) { if (!sb_puts(sb, ",")) return 0; }
        if (!sb_put_param_def(sb, &defs[i])) return 0;
    }
    return sb_puts(sb, "]");
}

char *uft_catalog_profiles_json(void)
{
    size_t n = 0;
    const uft_format_spec_t *specs = uft_format_get_known_specs(&n);
    if (!specs || n == 0) return NULL;

    sb_t sb;
    memset(&sb, 0, sizeof(sb));

    if (!sb_puts(&sb, "{")) goto fail;
    if (!sb_puts(&sb, "\"version\":\"1.0\",")) goto fail;
    if (!sb_puts(&sb, "\"formats\":[")) goto fail;

    for (size_t i = 0; i < n; ++i) {
        char *p = uft_format_profile_json(specs[i].id);
        if (!p) continue;
        if (i) { if (!sb_puts(&sb, ",")) { uft_profile_free(p); goto fail; } }
        if (!sb_puts(&sb, p)) { uft_profile_free(p); goto fail; }
        uft_profile_free(p);
    }
    if (!sb_puts(&sb, "]}")) goto fail;
    return sb.buf;

fail:
    free(sb.buf);
    return NULL;
}

char *uft_catalog_schemas_json(void)
{
    sb_t sb;
    memset(&sb, 0, sizeof(sb));

    if (!sb_puts(&sb, "{")) goto fail;
    if (!sb_puts(&sb, "\"version\":\"1.0\",")) goto fail;

    /* Recovery */
    {
        size_t n = 0;
        const uft_param_def_t *defs = uft_recovery_param_defs(&n);
        if (!sb_puts(&sb, "\"recovery\":")) goto fail;
        if (!sb_put_param_defs_array(&sb, defs, n)) goto fail;
    }

    /* Outputs */
    if (!sb_puts(&sb, ",\"outputs\":{")) goto fail;
    {
        const uft_output_format_t outs[] = {
            UFT_OUTPUT_RAW_IMG,
            UFT_OUTPUT_ATARI_ST,
            UFT_OUTPUT_AMIGA_ADF,
            UFT_OUTPUT_C64_G64,
            UFT_OUTPUT_APPLE_WOZ,
            UFT_OUTPUT_SCP,
            UFT_OUTPUT_A2R
        };
        const size_t outN = sizeof(outs)/sizeof(outs[0]);
        for (size_t i = 0; i < outN; ++i) {
            const uft_output_format_t of = outs[i];
            size_t n = 0;
            const uft_param_def_t *defs = uft_output_param_defs(of, &n);
            if (i) { if (!sb_puts(&sb, ",")) goto fail; }
            if (!sb_put_json_string(&sb, uft_output_format_ext(of))) goto fail;
            if (!sb_puts(&sb, ":")) goto fail;
            if (!sb_put_param_defs_array(&sb, defs, n)) goto fail;
        }
    }
    if (!sb_puts(&sb, "}")) goto fail;

    /* Format-specific schemas (by format id) */
    if (!sb_puts(&sb, ",\"formats\":{")) goto fail;
    {
        size_t fn = 0;
        const uft_format_spec_t *specs = uft_format_get_known_specs(&fn);
        for (size_t i = 0; i < fn; ++i) {
            size_t n = 0;
            const uft_param_def_t *defs = uft_format_param_defs(specs[i].id, &n);
            if (i) { if (!sb_puts(&sb, ",")) goto fail; }
            char idbuf[32];
            snprintf(idbuf, sizeof(idbuf), "%d", (int)specs[i].id);
            if (!sb_put_json_string(&sb, idbuf)) goto fail;
            if (!sb_puts(&sb, ":")) goto fail;
            if (!sb_put_param_defs_array(&sb, defs, n)) goto fail;
        }
    }
    if (!sb_puts(&sb, "}")) goto fail;

    if (!sb_puts(&sb, "}")) goto fail;
    return sb.buf;

fail:
    free(sb.buf);
    return NULL;
}

void uft_catalog_free(char *p)
{
    free(p);
}
