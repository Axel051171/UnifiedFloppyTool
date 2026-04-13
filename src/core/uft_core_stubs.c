/**
 * @file uft_core_stubs.c
 * @brief Minimal core function stubs for linking
 *
 * These stub functions avoid including heavy headers that cause
 * enum/struct redefinition conflicts. They use opaque void* pointers
 * and return safe default values (NULL, 0, -1).
 *
 * TODO: Replace stubs with real implementations once header
 * consolidation is complete (L-refactoring).
 */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Track/Sector stubs
 * ============================================================================ */

const void* uft_track_find_sector(const void* track, int sector) {
    (void)track; (void)sector;
    return NULL;
}

uint32_t uft_track_get_status(const void* track) {
    (void)track;
    return 0;
}

/* ============================================================================
 * Disk stubs
 * ============================================================================ */

void uft_disk_close(void *disk) {
    free(disk);
}

void* uft_disk_create(void) {
    return NULL;
}

void* uft_disk_open(const char *path, int read_only) {
    (void)path; (void)read_only;
    return NULL;
}

int uft_disk_get_geometry(const void *disk, void *geometry) {
    (void)disk; (void)geometry;
    return -1;
}

/* ============================================================================
 * Format detection stubs
 * ============================================================================ */

int uft_detect_format(const char *path) {
    (void)path;
    return 0;  /* UFT_FMT_UNKNOWN */
}

int uft_detect_buffer(const uint8_t *data, size_t size) {
    (void)data; (void)size;
    return 0;
}

int uft_probe_format(const char *path, void *result) {
    (void)path; (void)result;
    return -1;
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
