/**
 * @file uft_core_stubs.c
 * @brief Core glue layer — high-level disk API built on plugin system
 *
 * Previously all functions here were NULL-returning stubs. Phase 2 of
 * core consolidation replaces them with real plugin-delegation where
 * possible. Remaining stubs (flux decoders, provenance, compression)
 * return UFT_ERROR_NOT_SUPPORTED or safe defaults until the respective
 * subsystems are implemented.
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_error.h"
#include "uft/uft_format_autodetect.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Track/Sector helpers
 * ============================================================================ */

const uft_sector_t* uft_track_find_sector(const uft_track_t* track, int sector) {
    if (!track) return NULL;
    for (size_t i = 0; i < track->sector_count; i++) {
        if (track->sectors[i].id.sector == (uint8_t)sector)
            return &track->sectors[i];
    }
    return NULL;
}

uint32_t uft_track_get_status(const void* track_v) {
    const uft_track_t *t = (const uft_track_t *)track_v;
    if (!t) return 0;
    uint32_t status = 0;
    for (size_t i = 0; i < t->sector_count; i++)
        status |= t->sectors[i].status;
    return status;
}

/* Declared in include/uft/uft_core.h — was a Kat-A gap (declared+called,
 * no impl anywhere). Implementations are direct field reads; invariants
 * match how uft_track_t is populated by every parser in the tree. */

size_t uft_track_get_sector_count(const uft_track_t *track) {
    return track ? track->sector_count : 0;
}

bool uft_track_has_flux(const uft_track_t *track) {
    return track && track->flux != NULL && track->flux_count > 0;
}

const uft_sector_t *uft_track_get_sector(const uft_track_t *track, size_t index) {
    if (!track || !track->sectors) return NULL;
    if (index >= track->sector_count) return NULL;
    return &track->sectors[index];
}

/* Copies the track's flux buffer into a newly-allocated array, so the
 * caller is free to modify / outlive the source track. Returns UFT_OK
 * with *count_out = 0 and *flux_out = NULL if the track has no flux. */
uft_error_t uft_track_get_flux(const uft_track_t *track,
                                 uint32_t **flux_out, size_t *count_out)
{
    if (!track || !flux_out || !count_out) return UFT_ERROR_NULL_POINTER;
    *flux_out = NULL;
    *count_out = 0;
    if (!track->flux || track->flux_count == 0) return UFT_OK;

    size_t bytes = track->flux_count * sizeof(uint32_t);
    uint32_t *copy = (uint32_t *)malloc(bytes);
    if (!copy) return UFT_ERROR_NO_MEMORY;
    memcpy(copy, track->flux, bytes);
    *flux_out = copy;
    *count_out = track->flux_count;
    return UFT_OK;
}

/* uft_detect_result_t is a fixed-layout POD — init is memset, free is
 * a no-op. Declared in uft_format_autodetect.h, never implemented until
 * now; callers relied on zero-initialized stack state which worked
 * incidentally but broke as soon as a static/global was used. */
void uft_detect_result_init(uft_detect_result_t *result)
{
    if (!result) return;
    memset(result, 0, sizeof(*result));
}

void uft_detect_result_free(uft_detect_result_t *result)
{
    if (!result) return;
    /* No heap-owned members — all arrays are inline. Reset the counts
     * so a freed result can't be accidentally reused with stale data. */
    result->candidate_count = 0;
    result->warning_count   = 0;
}

/* ============================================================================
 * Disk Open / Close / Get-Geometry — real plugin delegation
 * ============================================================================ */

void* uft_disk_create(void) {
    uft_disk_t *disk = calloc(1, sizeof(uft_disk_t));
    if (!disk) return NULL;
    disk->read_only = true;
    return disk;
}

void* uft_disk_open(const char *path, int read_only) {
    if (!path) return NULL;

    /* 1. Probe to find the right plugin */
    const uft_format_plugin_t *plugin = uft_probe_file_format(path);
    if (!plugin || !plugin->open) return NULL;

    /* 2. Allocate disk handle */
    uft_disk_t *disk = calloc(1, sizeof(uft_disk_t));
    if (!disk) return NULL;

    /* 3. Store path (use internal buffer + pointer for legacy compat) */
    strncpy(disk->path_buf, path, sizeof(disk->path_buf) - 1);
    disk->path_buf[sizeof(disk->path_buf) - 1] = '\0';
    disk->path = disk->path_buf;
    disk->format = plugin->format;
    disk->read_only = read_only ? true : false;

    /* 4. Delegate to plugin */
    uft_error_t err = plugin->open(disk, path, disk->read_only);
    if (err != UFT_OK) {
        free(disk);
        return NULL;
    }

    disk->is_open = true;
    return disk;
}

/* Forward decl — uft_metadata.c */
extern void uft_meta_free(uft_disk_t *disk);

void uft_disk_close(void *disk_v) {
    uft_disk_t *disk = (uft_disk_t *)disk_v;
    if (!disk) return;

    /* Plugin close if opened */
    if (disk->is_open) {
        const uft_format_plugin_t *plugin = uft_get_format_plugin(disk->format);
        if (plugin && plugin->close)
            plugin->close(disk);
    }
    disk->is_open = false;
    uft_meta_free(disk);       /* Metadata store, if any */
    free(disk->image_data);     /* If GUI stored raw image */
    free(disk);
}

int uft_disk_get_geometry(const void *disk_v, void *geom_v) {
    const uft_disk_t *disk = (const uft_disk_t *)disk_v;
    uft_geometry_t *geom = (uft_geometry_t *)geom_v;
    if (!disk || !geom) return -1;
    *geom = disk->geometry;
    return 0;
}

/* ============================================================================
 * Format detection — real via plugin probe chain
 * ============================================================================ */

int uft_detect_format(const char *path) {
    if (!path) return 0;
    const uft_format_plugin_t *plugin = uft_probe_file_format(path);
    return plugin ? (int)plugin->format : 0;
}

int uft_detect_buffer(const uint8_t *data, size_t size) {
    if (!data || size == 0) return 0;
    const uft_format_plugin_t *plugin =
        uft_probe_buffer_format(data, size, size);
    return plugin ? (int)plugin->format : 0;
}

int uft_probe_format(const char *path, void *result) {
    if (!path) return -1;
    const uft_format_plugin_t *plugin = uft_probe_file_format(path);
    if (!plugin) return -1;
    /* result is opaque to callers — they either cast or don't use it */
    if (result) {
        *(const uft_format_plugin_t **)result = plugin;
    }
    return (int)plugin->format;
}

/* ============================================================================
 * File injection stubs
 * ============================================================================ */

int uft_inject_file(void *disk, const char *path, const uint8_t *data,
                    size_t size) {
    (void)disk; (void)path; (void)data; (void)size;
    return -1;
}

/* ============================================================================
 * ADF stubs
 * ============================================================================ */

int uft_adf_mkdir(void *disk, const char *name) {
    (void)disk; (void)name;
    return -1;
}

int uft_adf_read_file(void *disk, const char *path, uint8_t **data,
                      size_t *size) {
    (void)disk; (void)path; (void)data; (void)size;
    return -1;
}

int uft_adf_rename(void *disk, const char *old_name, const char *new_name) {
    (void)disk; (void)old_name; (void)new_name;
    return -1;
}

int uft_validate_adf(const uint8_t *data, size_t size) {
    (void)data; (void)size;
    return -1;
}

/* ============================================================================
 * FAT12 stubs
 * ============================================================================ */

void* uft_fat12_init(const uint8_t *data, size_t size) {
    (void)data; (void)size;
    return NULL;
}

void uft_fat12_free(void *fat) {
    free(fat);
}

int uft_fat12_create_entry(void *fat, const char *name, const uint8_t *data,
                           size_t size) {
    (void)fat; (void)name; (void)data; (void)size;
    return -1;
}

int uft_fat12_delete(void *fat, const char *name) {
    (void)fat; (void)name;
    return -1;
}

/* ============================================================================
 * Validation stubs
 * ============================================================================ */

int uft_validate_d64(const uint8_t *data, size_t size) {
    (void)data; (void)size;
    return -1;
}

int uft_validate_g64(const uint8_t *data, size_t size) {
    (void)data; (void)size;
    return -1;
}

int uft_validate_scp(const uint8_t *data, size_t size) {
    (void)data; (void)size;
    return -1;
}

/* ============================================================================
 * SCP stubs
 * ============================================================================ */

void* uft_scp_read(const char *path) {
    (void)path;
    return NULL;
}

void uft_scp_free(void *scp) {
    free(scp);
}

int uft_scp_get_track_flux(const void *scp, int track, uint32_t **flux,
                           size_t *count) {
    (void)scp; (void)track; (void)flux; (void)count;
    return -1;
}

/* ============================================================================
 * KryoFlux stubs
 * ============================================================================ */

void* uft_kfx_read_stream(const char *path, int track) {
    (void)path; (void)track;
    return NULL;
}

void uft_kfx_free(void *kfx) {
    free(kfx);
}

/* ============================================================================
 * Format conversion stubs
 * ============================================================================ */

int uft_imd_to_raw(const void *img, uint8_t **data, size_t *size,
                   uint8_t fill) {
    (void)img; (void)data; (void)size; (void)fill;
    return -1;
}

int uft_td0_to_raw(const void *img, uint8_t **data, size_t *size,
                   uint8_t fill) {
    (void)img; (void)data; (void)size; (void)fill;
    return -1;
}

/* ============================================================================
 * C64 parser stubs
 * ============================================================================ */

void uft_c64_parser_init(void *parser) {
    (void)parser;
}

int uft_c64_parser_add_bit(void *parser, int bit) {
    (void)parser; (void)bit;
    return -1;
}

/* ============================================================================
 * GCR/MFM SIMD dispatch stubs
 * ============================================================================ */

void uft_gcr_decode_5to4_scalar(const uint8_t *in, uint8_t *out, size_t n) {
    (void)in; (void)out; (void)n;
}

void uft_gcr_decode_5to4_sse2(const uint8_t *in, uint8_t *out, size_t n) {
    (void)in; (void)out; (void)n;
}

void uft_gcr_decode_5to4_avx2(const uint8_t *in, uint8_t *out, size_t n) {
    (void)in; (void)out; (void)n;
}

void uft_mfm_decode_flux_scalar(const uint32_t *flux, size_t count,
                                uint8_t *bits, size_t *bit_count) {
    (void)flux; (void)count; (void)bits; (void)bit_count;
}

void uft_mfm_decode_flux_sse2(const uint32_t *flux, size_t count,
                              uint8_t *bits, size_t *bit_count) {
    (void)flux; (void)count; (void)bits; (void)bit_count;
}

void uft_mfm_decode_flux_avx2(const uint32_t *flux, size_t count,
                               uint8_t *bits, size_t *bit_count) {
    (void)flux; (void)count; (void)bits; (void)bit_count;
}

/* ============================================================================
 * PLL stubs (uft_pll.h — real impl in src/core/uft_pll.c, not in build)
 * ============================================================================ */

typedef struct {
    uint32_t cell_ns;
    uint32_t cell_ns_min;
    uint32_t cell_ns_max;
    uint32_t alpha_q16;
    uint32_t max_run_cells;
} uft_pll_cfg_stub_t;

uft_pll_cfg_stub_t uft_pll_cfg_default_mfm_dd(void) {
    uft_pll_cfg_stub_t cfg = { 4000, 3000, 5600, 3277, 4 };
    return cfg;  /* 4µs cell = 250kbps DD */
}

uft_pll_cfg_stub_t uft_pll_cfg_default_mfm_hd(void) {
    uft_pll_cfg_stub_t cfg = { 2000, 1500, 2800, 3277, 4 };
    return cfg;  /* 2µs cell = 500kbps HD */
}

size_t uft_flux_to_bits_pll(
    const uint64_t *timestamps_ns, size_t count,
    const void *cfg,
    uint8_t *out_bits, size_t out_bits_capacity_bits,
    uint32_t *out_final_cell_ns, size_t *out_dropped_transitions)
{
    if (!timestamps_ns || count < 2 || !cfg || !out_bits)
        return 0;

    const uft_pll_cfg_stub_t *c = (const uft_pll_cfg_stub_t *)cfg;
    uint32_t cell = c->cell_ns;
    uint32_t alpha = c->alpha_q16;
    size_t bits = 0;
    size_t dropped = 0;

    for (size_t i = 1; i < count && bits < out_bits_capacity_bits; i++) {
        uint64_t delta = timestamps_ns[i] - timestamps_ns[i - 1];
        if (delta == 0) { dropped++; continue; }

        /* How many cells fit in this interval? */
        uint32_t n = (uint32_t)((delta + cell / 2) / cell);
        if (n == 0) n = 1;
        if (n > c->max_run_cells) { n = c->max_run_cells; dropped++; }

        /* Write n-1 zero bits + 1 one bit */
        for (uint32_t b = 0; b < n - 1 && bits < out_bits_capacity_bits; b++) {
            /* zero bit: already 0 from calloc */
            bits++;
        }
        if (bits < out_bits_capacity_bits) {
            size_t byte_idx = bits / 8;
            size_t bit_idx = 7 - (bits % 8);
            out_bits[byte_idx] |= (1u << bit_idx);
            bits++;
        }

        /* Adjust cell size (PI loop, Q16 fixed point) */
        int32_t err = (int32_t)(delta - (uint64_t)n * cell);
        int32_t adj = (int32_t)((int64_t)err * alpha >> 16);
        cell = (uint32_t)((int32_t)cell + adj);
        if (cell < c->cell_ns_min) cell = c->cell_ns_min;
        if (cell > c->cell_ns_max) cell = c->cell_ns_max;
    }

    if (out_final_cell_ns) *out_final_cell_ns = cell;
    if (out_dropped_transitions) *out_dropped_transitions = dropped;
    return bits;
}

/* ============================================================================
 * Misc stubs
 * ============================================================================ */

const char* uft_longtrack_type_name(int type) {
    (void)type;
    return "Unknown";
}

int uft_lzhuf_decompress(const uint8_t *src, size_t src_len,
                         uint8_t *dst, size_t dst_len) {
    (void)src; (void)src_len; (void)dst; (void)dst_len;
    return -1;
}

/* ============================================================================
 * Provenance chain stubs (uft_provenance.c not yet created)
 * ============================================================================ */

typedef struct uft_prov_chain uft_prov_chain_t;

uft_prov_chain_t* uft_prov_create(void) { return NULL; }
void uft_prov_free(uft_prov_chain_t *chain) { (void)chain; }
int uft_prov_add(uft_prov_chain_t *chain, const char *event,
                 const char *detail) {
    (void)chain; (void)event; (void)detail; return -1;
}
int uft_prov_export_json(const uft_prov_chain_t *chain,
                          const char *path) {
    (void)chain; (void)path; return -1;
}

/* ============================================================================
 * UFI backend stub (uft_ufi_backend.c not yet created)
 * ============================================================================ */

int uft_ufi_backend_init(void) { return -1; }

/* ============================================================================
 * CPU feature detection stub (uft_simd.c not yet created)
 * ============================================================================ */

int uft_cpu_has_feature(int feature) { (void)feature; return 0; }
