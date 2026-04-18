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
#include "uft/uft_file_ops.h"                /* uft_file_type_t */
#include "uft/uft_format_parsers.h"      /* uft_scp_file_t, uft_kfx_stream_t */
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

uft_disk_t* uft_disk_open(const char *path, bool read_only) {
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
    disk->read_only = read_only;

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
 * Format detection — ABI-correct impls split by header (ABI-001/002 P0 fix).
 *
 * uft_detect_format  → src/core/uft_detect_format_impl.c  (detect/ header)
 * uft_detect_buffer  → src/core/uft_detect_buffer_impl.c  (uft_format_detect.h)
 * uft_probe_format   → src/core/uft_probe_format_impl.c   (uft_format_probe.h)
 *
 * Each lives in its own TU because the three sibling result-struct types
 * (uft_detect_result_t × 2, uft_probe_result_t) have incompatible layouts.
 * Keeping them separate means each TU only sees one definition.
 *
 * Previous stubs here silently corrupted callers: the old 2-arg
 * uft_detect_buffer was called with 4 args — extra 2 sat in unused
 * registers, stub returned 0 → callers thought every buffer was
 * "not detected" regardless of content. Fixed by the three new files. */

/* ============================================================================
 * File injection — ABI-correct stub
 *
 * Declared in include/uft_file_ops.h as 4-arg with 3 char* + a
 * uft_file_type_t — NOT the (void*disk, path, data, size) form this
 * stub used to claim. Previous stub was another §1.3 ABI-mismatch.
 * Fix: match the real signature, return -1, defer real impl until
 * FS write-paths are in place (same wave as ADF write-ops).
 * ============================================================================ */

int uft_inject_file(const char *image_path, const char *filename,
                     const char *input_path, uft_file_type_t type) {
    (void)image_path; (void)filename; (void)input_path; (void)type;
    return -1;
}

/* ============================================================================
 * ADF stubs
 * ============================================================================ */

/* ADF write-ops uft_adf_mkdir / uft_adf_rename / uft_adf_read_file
 * moved to src/formats/uft_adf.c with correct (uft_adf_volume_t*)
 * signatures. read_file is a real read-only impl; mkdir/rename stay
 * honest NOT_IMPLEMENTED until the ADF write-path lands. Previous
 * (void*disk) stubs here were §1.3 ABI-mismatch violations. */

/* Legacy uft_fat12_* API moved to src/formats/uft_fat12_legacy.c with
 * correct signatures that match include/uft/formats/uft_fat12.h.
 * Four previous stubs here had mismatched arity (caller passes 4 args,
 * stub read 2) — a silent spec §1.3 violation. */

/* Validators uft_validate_{d64,adf,g64,scp} moved to
 * src/formats/uft_format_validators.c with correct 4-argument
 * signatures (level + result). Old 2-arg stubs were another §1.3
 * ABI-mismatch case. */

/* ============================================================================
 * SCP — ABI-correct stubs
 *
 * Previous stubs had DIFFERENT signatures than the header declarations
 * in uft_format_parsers.h. Callers passed (data, size, &scp) expecting
 * int → the 1-arg(path) stub returned NULL = 0, which matches
 * "SUCCESS" in the int-return contract. That's a catastrophic §1.3
 * violation: the caller proceeds with uninitialized uft_scp_file_t.
 *
 * Fix: match the real signature, return -1 so callers see failure.
 * Real impl lives in src/flux/uft_scp_parser.c but under different
 * type (uft_scp_ctx_t not uft_scp_file_t) — unifying those types is
 * a separate port.
 * ============================================================================ */

int uft_scp_read(const uint8_t *data, size_t size, uft_scp_file_t *scp) {
    (void)data; (void)size;
    if (scp) memset(scp, 0, sizeof(*scp));
    return -1;   /* HONEST: caller should fall back, not proceed */
}

void uft_scp_free(uft_scp_file_t *scp) {
    if (!scp) return;
    free(scp->track_offsets);
    free(scp->data);
    scp->track_offsets = NULL;
    scp->data = NULL;
    scp->track_count = 0;
    scp->data_size = 0;
}

int uft_scp_get_track_flux(const uft_scp_file_t *scp, int track, int revolution,
                            double *out_deltas, size_t max_deltas) {
    (void)scp; (void)track; (void)revolution; (void)out_deltas; (void)max_deltas;
    return -1;
}

/* ============================================================================
 * KryoFlux — ABI-correct stub
 *
 * Same story: declared int-return 3-arg, stub was void*-return 2-arg.
 * Callers got NULL cast to int = 0 = "success". Fixed.
 * ============================================================================ */

int uft_kfx_read_stream(const uint8_t *data, size_t size, uft_kfx_stream_t *stream) {
    (void)data; (void)size;
    if (stream) memset(stream, 0, sizeof(*stream));
    return -1;
}

void uft_kfx_free(uft_kfx_stream_t *stream) {
    if (!stream) return;
    free(stream->flux_deltas);
    free(stream->index_times);
    stream->flux_deltas = NULL;
    stream->index_times = NULL;
    stream->flux_count = 0;
    stream->index_count = 0;
}

/* Format conversion impls moved to src/formats/uft_format_converters.c
 * with correct-typed signatures (const uft_imd_image_t* / const
 * uft_td0_image_t* instead of the ABI-broken const void*). */

/* ============================================================================
 * C64 GCR parser — ABI-correct honest impl
 *
 * Previous (void*) 1-arg + (void*,int) stubs were §1.3 mismatches
 * against include/uft/uft_c64_gcr.h which declares:
 *   void uft_c64_parser_init(uft_c64_parser_t *p, int track);
 *   void uft_c64_parser_add_bit(uft_c64_parser_t *p, uint8_t bit,
 *                                unsigned long position);
 * The single caller (uft_format_convert_flux.c) passes those types;
 * the old stubs silently did the wrong thing.
 *
 * init zeros the struct and sets the start state. add_bit folds the
 * bit into the sliding window and counts bits/position. Full C64 GCR
 * sync + sector extraction (~200 lines) is deferred — this minimal
 * honest impl keeps counters consistent without inventing sectors.
 * ============================================================================ */

#include "uft/uft_c64_gcr.h"

void uft_c64_parser_init(uft_c64_parser_t *parser, int track) {
    if (!parser) return;
    memset(parser, 0, sizeof(*parser));
    parser->state = UFT_C64_STATE_IDLE;
    parser->last_track = track;
}

void uft_c64_parser_add_bit(uft_c64_parser_t *parser,
                             uint8_t bit, unsigned long position) {
    if (!parser) return;
    parser->datacells = (uint16_t)((parser->datacells << 1) | (bit & 1));
    parser->bits++;
    parser->data_position = position;
    /* Full sync-pattern detection + sector extraction deferred. */
}

/* Six SIMD decoder stubs (uft_gcr_decode_5to4_{scalar,sse2,avx2} +
 * uft_mfm_decode_flux_{scalar,sse2,avx2}) deleted per spec §1.4 — zero
 * callers anywhere in the tree. Declarations in include/uft/uft_simd.h
 * removed in the same commit so future readers don't expect them. */

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

/* LZHUF decompressor for DMS-Heavy / NBZ archives.
 *
 * Honest NOT_IMPLEMENTED per spec §1.3 Option 1. Full Okumura/Yoshizaki
 * LZHUF = LZSS(4 KB, 60-byte lookahead) + adaptive Huffman is ~400 lines
 * and can't be landed without verified test vectors (DMS-Heavy/NBZ
 * sample files). Callers in uft_format_convert_archive.c handle -1 via
 * a raw-D64 fallback path.
 *
 * Signature now matches the 5-arg declaration in uft_format_parsers.h
 * (uft_lzhuf_options_t pulled in via the #include above).
 */
int uft_lzhuf_decompress(const uint8_t *src, size_t src_size,
                         uint8_t *dst, size_t dst_size,
                         const uft_lzhuf_options_t *opts)
{
    (void)src; (void)src_size; (void)dst; (void)dst_size; (void)opts;
    return -1;
}

/* Provenance chain impls moved to src/forensic/uft_provenance.c with
 * correct-signature implementations (uft_provenance_chain_t, 6-arg
 * add, SHA-256 chain hashing, JSON export). The previous stubs here
 * had a local uft_prov_chain_t typedef + 3-arg add(event, detail) —
 * another §1.3 ABI mismatch against include/uft/forensic/uft_provenance.h. */

/* ============================================================================
 * UFI Backend-Initialisierung — documented NOT_IMPLEMENTED
 *
 * UFI (USB Floppy Interface via SCSI-CDB) needs platform-specific
 * backend code that doesn't exist in the tree:
 *
 *   Linux:   SG_IO ioctl path     (src/hal/ufi_linux.c — absent)
 *   Windows: SCSI_PASS_THROUGH   (src/hal/ufi_win.c    — absent)
 *   macOS:   IOKit SCSITaskUserClient (src/hal/ufi_mac.c  — absent)
 *
 * Returning -1 lets callers see "no UFI backend" cleanly instead of
 * crashing on an uninitialized device. Implementation lands when the
 * platform-specific files arrive (HAL work package HW2).
 * ============================================================================ */

int uft_ufi_backend_init(void) {
    return -1;   /* No platform backend registered. */
}

/* ============================================================================
 * CPU feature detection — real impl via __builtin_cpu_supports
 *
 * ABI now matches include/uft/uft_simd.h:
 *   bool uft_cpu_has_feature(uft_cpu_features_t feature);
 * (previously declared `int`-return and `int` arg — silent mismatch).
 *
 * Uses GCC/Clang's __builtin_cpu_supports where available, returns
 * false on non-x86 or other compilers so callers take the scalar
 * path. This is the full expected behaviour for the SIMD dispatch
 * code everywhere else in the tree.
 * ============================================================================ */

#include "uft/uft_simd.h"

bool uft_cpu_has_feature(uft_cpu_features_t feature) {
#if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
    switch (feature) {
        case UFT_CPU_SSE2:     return __builtin_cpu_supports("sse2");
        case UFT_CPU_SSE3:     return __builtin_cpu_supports("sse3");
        case UFT_CPU_SSSE3:    return __builtin_cpu_supports("ssse3");
        case UFT_CPU_SSE41:    return __builtin_cpu_supports("sse4.1");
        case UFT_CPU_SSE42:    return __builtin_cpu_supports("sse4.2");
        case UFT_CPU_AVX:      return __builtin_cpu_supports("avx");
        case UFT_CPU_AVX2:     return __builtin_cpu_supports("avx2");
        case UFT_CPU_AVX512F:  return __builtin_cpu_supports("avx512f");
        case UFT_CPU_AVX512BW: return __builtin_cpu_supports("avx512bw");
        case UFT_CPU_FMA:      return __builtin_cpu_supports("fma");
        case UFT_CPU_POPCNT:   return __builtin_cpu_supports("popcnt");
        case UFT_CPU_BMI1:     return __builtin_cpu_supports("bmi");
        case UFT_CPU_BMI2:     return __builtin_cpu_supports("bmi2");
        default:               return false;
    }
#else
    (void)feature;
    return false;
#endif
}
