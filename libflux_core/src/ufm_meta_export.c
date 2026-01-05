#include "uft/uft_error.h"
#include "ufm_meta_export.h"

#include "flux_core.h"
#include "flux_logical.h"
#include "cpc_meta.h"

#include <stdio.h>
#include <string.h>

static void j_str(FILE *f, const char *s)
{
    if (!s) { fputs("null", f); return; }
    fputc('"', f);
    for (const unsigned char *p=(const unsigned char*)s; *p; ++p) {
        switch (*p) {
        case '\\\\': fputs("\\\\\\\\", f); break;
        case '"':   fputs("\\\\\"", f); break;
        case '\\n': fputs("\\\\n", f); break;
        case '\\r': fputs("\\\\r", f); break;
        case '\\t': fputs("\\\\t", f); break;
        default:
            if (*p < 0x20) fprintf(f, "\\\\u%04x", (unsigned)*p);
            else fputc(*p, f);
        }
    }
    fputc('"', f);
}

static void j_bool(FILE *f, int b) { fputs(b ? "true":"false", f); }

static void j_u32(FILE *f, uint32_t v) { fprintf(f, "%u", (unsigned)v); }
static void j_u16(FILE *f, uint16_t v) { fprintf(f, "%u", (unsigned)v); }
static void j_f(FILE *f, float v) { fprintf(f, "%.6g", v); }

static void export_media(FILE *f, const ufm_disk_t *d)
{
    fputs("\"media\":{", f);
    if (!d->profile_valid) {
        fputs("\"valid\":false}", f);
        return;
    }
    fputs("\"valid\":true,", f);
    fputs("\"name\":", f); j_str(f, d->profile.name); fputc(',', f);
    fputs("\"title\":", f); j_str(f, d->profile.title); fputc(',', f);
    fputs("\"encoding\":", f); j_u32(f, (uint32_t)d->profile.encoding); fputc(',', f);
    fputs("\"rpm\":", f); j_u16(f, d->profile.rpm); fputc(',', f);
    fputs("\"bitrate_kbps\":", f); j_u32(f, d->profile.bitrate_kbps); fputc(',', f);
    fputs("\"cylinders\":", f); j_u16(f, d->profile.cylinders); fputc(',', f);
    fputs("\"heads\":", f); j_u16(f, d->profile.heads); fputc(',', f);
    fputs("\"spt\":", f); j_u16(f, d->profile.sectors_per_track); fputc(',', f);
    fputs("\"sector_size\":", f); j_u16(f, d->profile.sector_size); fputc(',', f);
    fputs("\"has_index\":", f); j_bool(f, d->profile.has_index); fputc(',', f);
    fputs("\"variable_spt\":", f); j_bool(f, d->profile.variable_spt);
    fputs("}", f);
}

int ufm_export_meta_json(const ufm_disk_t *d, const char *json_path)
{
    if (!d || !json_path) return -1;

    FILE *f = fopen(json_path, "wb");
    if (!f) return -2;

    fputs("{", f);

    /* --- top-level disk info --- */
    fputs("\"ufm\":{", f);
    fputs("\"cyls\":", f); j_u16(f, d->cyls); fputc(',', f);
    fputs("\"heads\":", f); j_u16(f, d->heads); fputc(',', f);
    fputs("\"cp_flags\":", f); j_u32(f, (uint32_t)d->cp_flags);
    fputs("},", f);

    /* hw */
    fputs("\"hw\":{", f);
    fputs("\"vendor\":", f); j_str(f, d->hw.vendor); fputc(',', f);
    fputs("\"product\":", f); j_str(f, d->hw.product); fputc(',', f);
    fputs("\"serial\":", f); j_str(f, d->hw.serial); fputc(',', f);
    fputs("\"fw\":", f); j_str(f, d->hw.fw_version); fputc(',', f);
    fputs("\"revision\":", f); j_u32(f, d->hw.hw_revision); fputc(',', f);
    fputs("\"sample_clock_hz\":", f); j_u32(f, d->hw.sample_clock_hz);
    fputs("},", f);

    /* retry */
    fputs("\"retry\":{", f);
    fputs("\"read_attempts\":", f); j_u32(f, d->retry.read_attempts); fputc(',', f);
    fputs("\"read_success\":", f); j_u32(f, d->retry.read_success); fputc(',', f);
    fputs("\"write_attempts\":", f); j_u32(f, d->retry.write_attempts); fputc(',', f);
    fputs("\"write_success\":", f); j_u32(f, d->retry.write_success); fputc(',', f);
    fputs("\"seek_retries\":", f); j_u32(f, d->retry.seek_retries);
    fputs("},", f);

    /* quality (optional; may be 0) */
    fputs("\"quality\":{", f);
    fputs("\"snr_est\":", f); j_f(f, d->quality.snr_est); fputc(',', f);
    fputs("\"jitter_rms_ns\":", f); j_f(f, d->quality.jitter_rms_ns); fputc(',', f);
    fputs("\"dropout_rate\":", f); j_f(f, d->quality.dropout_rate);
    fputs("},", f);

    export_media(f, d);
    fputc(',', f);

    /* tracks + optional sector map */
    fputs("\"tracks\":[", f);
    int first_tr = 1;
    for (uint16_t c=0;c<d->cyls;++c) {
        for (uint16_t h=0;h<d->heads;++h) {
            ufm_track_t *t = ufm_disk_track((ufm_disk_t*)d, c, h);
            if (!t) continue;

            if (!first_tr) fputc(',', f);
            first_tr = 0;

            fprintf(f, "{");
            fputs("\"c\":", f); j_u16(f, c); fputc(',', f);
            fputs("\"h\":", f); j_u16(f, h); fputc(',', f);

            fputs("\"revs\":", f); j_u16(f, (uint16_t)t->revs_count); fputc(',', f);

            /* per-track decode stats if present (CPC pipeline attaches this) */
            if (d->logical) {
                fputs("\"sector_map\":", f);
                cpc_write_sector_map_json(f, c, h, d->logical, NULL);
            } else {
                fputs("\"sector_map\":null", f);
            }

            fputs("}", f);
        }
    }
    fputs("]}", f);

    fclose(f);
    return 0;
}
