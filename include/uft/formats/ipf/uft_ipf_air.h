/**
 * @file uft_ipf_air.h
 * @brief IPF AIR-enhanced parser — opaque-handle public API
 *
 * Wraps the SPS/CAPS IPF parser implemented in src/formats/ipf/uft_ipf_air.c.
 * Internal types (ipf_track_t, ipf_block_desc_t, ipf_data_elem_t, etc.) stay
 * private to that TU. Callers interact through:
 *
 *   1. lifecycle:   ipf_air_alloc / ipf_air_parse / ipf_air_free
 *   2. queries:     ipf_air_get_geometry / ipf_air_track_present
 *                   ipf_air_get_track_meta
 *   3. payload:     ipf_air_get_track_raw — concatenates decoded data-element
 *                   value bytes from all blocks of a track into a single
 *                   buffer. Honest scope: this returns the *data-element*
 *                   payload (SYNC / DATA / RAW / IGAP byte sequences as
 *                   recorded in the IPF DATA records). It does NOT
 *                   reconstruct the full track bitstream including gap
 *                   padding — that requires gap-element synthesis from
 *                   gap_default values, which is a separate task.
 *
 * The data_elements path is currently populated only for the SPS encoder
 * (info->encoder_type == IPF_ENC_SPS = 2). Older CAPS-encoded IPFs route
 * data through a different block layout that is not yet decoded here;
 * ipf_air_get_track_raw will return UFT_ERR_NOT_IMPLEMENTED for those.
 */
#ifndef UFT_IPF_AIR_H
#define UFT_IPF_AIR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque parsed-IPF handle. Allocated by ipf_air_alloc(),
 * freed by ipf_air_free(). */
typedef struct ipf_air_disk ipf_air_disk_t;

/* Status codes (mirror the file-local enum in uft_ipf_air.c). */
typedef enum {
    IPF_AIR_OK = 0,
    IPF_AIR_NOT_IPF,
    IPF_AIR_BAD_CRC,
    IPF_AIR_TRUNCATED,
    IPF_AIR_BAD_RECORD,
    IPF_AIR_KEY_MISMATCH,
    IPF_AIR_FILE_ERROR
} ipf_air_status_t;

/* ── Lifecycle ───────────────────────────────────────────────────────── */

/**
 * @brief Allocate a zeroed parsed-IPF container.
 * @return malloc'd handle (caller frees with ipf_air_free + free()) or NULL.
 */
ipf_air_disk_t *ipf_air_alloc(void);

/**
 * @brief Parse an IPF byte buffer into a pre-allocated disk handle.
 * @param data IPF file bytes
 * @param size Size of the buffer
 * @param disk Pre-allocated handle from ipf_air_alloc()
 * @return IPF_AIR_OK on success
 */
ipf_air_status_t ipf_air_parse(const uint8_t *data, size_t size,
                                ipf_air_disk_t *disk);

/**
 * @brief Release internal allocations referenced by the handle.
 *        Does NOT free the handle itself — caller must call free(disk).
 */
void ipf_air_free(ipf_air_disk_t *disk);

/* ── Disk-level queries ──────────────────────────────────────────────── */

/**
 * @brief Geometry derived from INFO record.
 * @param out_cylinders Set to (max_track - min_track + 1), or 0
 * @param out_sides     Set to (max_side  - min_side  + 1), or 0
 * @param out_primary_platform Set to platforms[0] (caps_platform_t value)
 * @return 0 on success, -1 on invalid handle / not parsed
 */
int ipf_air_get_geometry(const ipf_air_disk_t *disk,
                          int *out_cylinders, int *out_sides,
                          uint32_t *out_primary_platform);

/**
 * @brief Whether (cyl, head) has an IMGE record in the file.
 */
bool ipf_air_track_present(const ipf_air_disk_t *disk, int cyl, int head);

/* ── Per-track queries ───────────────────────────────────────────────── */

/**
 * @brief Per-track metadata fields straight from the IMGE descriptor.
 * @return 0 if the track exists, -1 otherwise. Out-pointers may be NULL.
 */
int ipf_air_get_track_meta(const ipf_air_disk_t *disk, int cyl, int head,
                            uint32_t *out_track_bits,
                            uint32_t *out_density,
                            uint32_t *out_track_flags,
                            bool     *out_has_fuzzy);

/**
 * @brief Concatenate decoded data-element payload bytes from every block of
 *        a track into a single contiguous buffer.
 *
 * The returned buffer is malloc'd and ownership transfers to the caller —
 * free with free() when done. *out_buf is set to NULL and *out_bits to 0
 * when the track has no decoded data elements (e.g. CAPS-encoded files
 * where SPS data-element parsing did not run).
 *
 * The bit count corresponds to the sum of data_bits across data_elements.
 * Element ordering follows block order, then data-element order within
 * each block. FUZZY elements (which carry no value bytes) contribute
 * their bit count but no buffer bytes — the byte buffer therefore
 * represents the deterministic-decoded portion only.
 *
 * @return 0 on success (including "no data elements" → buf=NULL, bits=0),
 *         -1 on invalid args / track not present,
 *         -2 if data-element decoding was not run for this file (CAPS
 *            encoder fallback path not yet implemented).
 */
int ipf_air_get_track_raw(const ipf_air_disk_t *disk, int cyl, int head,
                           uint8_t **out_buf, uint32_t *out_bits);

#ifdef __cplusplus
}
#endif

#endif /* UFT_IPF_AIR_H */
