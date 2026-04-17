/**
 * @file uft_metadata.h
 * @brief Key-Value Metadata and per-track/sector Annotations for disks.
 *
 * Phase P1b of the API consolidation. Fills a gap in the preservation
 * workflow: disks need attachable notes ("this sector is protected
 * with scheme X", "Track 35 had CRC errors") that aren't part of the
 * file format itself.
 *
 * Uses sidecar JSON file ("<image>.uft-meta.json") for persistence.
 */
#ifndef UFT_METADATA_H
#define UFT_METADATA_H

#include "uft/uft_types.h"
#include "uft/uft_error.h"
#include <stdint.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Annotation Levels
 * ============================================================================ */

typedef enum {
    UFT_NOTE_INFO       = 0,   /* Purely informational */
    UFT_NOTE_WARNING    = 1,   /* Non-fatal concern */
    UFT_NOTE_ERROR      = 2,   /* Sector unreadable / corrupted */
    UFT_NOTE_PROTECTION = 3,   /* Copy-protection observed */
    UFT_NOTE_DEFECT     = 4,   /* Physical defect suspected */
} uft_note_level_t;

typedef struct {
    int              cylinder;
    int              head;
    int              sector;        /* -1 = track-level */
    uft_note_level_t level;
    char            *text;          /* malloc'd */
    time_t           created;
} uft_annotation_t;

/* ============================================================================
 * Disk-Level Key-Value API
 * ============================================================================ */

/**
 * @brief Set (or overwrite) a metadata key-value pair.
 * @param disk target disk
 * @param key NUL-terminated, max 64 chars
 * @param value NUL-terminated, max 4096 chars
 */
uft_error_t uft_meta_set(uft_disk_t *disk, const char *key, const char *value);

/**
 * @brief Read metadata value into caller-provided buffer.
 * @return UFT_OK on success, UFT_ERR_NOT_FOUND if key missing
 */
uft_error_t uft_meta_get(uft_disk_t *disk, const char *key,
                          char *value_out, size_t value_size);

/**
 * @brief Remove a key.
 */
uft_error_t uft_meta_remove(uft_disk_t *disk, const char *key);

/**
 * @brief Enumerate all keys. Caller frees via uft_meta_free_keys.
 */
uft_error_t uft_meta_list_keys(uft_disk_t *disk,
                                char ***keys_out, size_t *count_out);

void uft_meta_free_keys(char **keys, size_t count);

/* ============================================================================
 * Per-Track/Per-Sector Annotations
 * ============================================================================ */

uft_error_t uft_meta_annotate_track(uft_disk_t *disk, int cyl, int head,
                                      uft_note_level_t level, const char *text);

uft_error_t uft_meta_annotate_sector(uft_disk_t *disk, int cyl, int head,
                                       int sector, uft_note_level_t level,
                                       const char *text);

uft_error_t uft_meta_get_annotations(uft_disk_t *disk,
                                       uft_annotation_t **out, size_t *count);

void uft_meta_free_annotations(uft_annotation_t *annotations, size_t count);

/* ============================================================================
 * Sidecar Persistence
 * ============================================================================ */

/**
 * @brief Save all metadata + annotations to a JSON file.
 */
uft_error_t uft_meta_save(uft_disk_t *disk, const char *sidecar_path);

/**
 * @brief Load metadata + annotations from a JSON file.
 */
uft_error_t uft_meta_load(uft_disk_t *disk, const char *sidecar_path);

/**
 * @brief Convenience: use disk's path + ".uft-meta.json" suffix.
 */
uft_error_t uft_meta_save_sidecar(uft_disk_t *disk);
uft_error_t uft_meta_load_sidecar(uft_disk_t *disk);

/* ============================================================================
 * Cleanup (called automatically on uft_disk_close)
 * ============================================================================ */

void uft_meta_free(uft_disk_t *disk);

#ifdef __cplusplus
}
#endif
#endif /* UFT_METADATA_H */
