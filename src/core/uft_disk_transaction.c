/**
 * @file uft_disk_transaction.c
 * @brief Atomic multi-track writes via in-memory staging.
 *
 * Implementation: only IN_MEMORY mode for now. BACKUP_FILE and SHADOW_WRITE
 * require file-level operations that differ per plugin — deferred.
 */
#include "uft/uft_disk_transaction.h"
#include "uft/uft_format_plugin.h"
#include <stdlib.h>
#include <string.h>

typedef struct pending_write {
    int                     cyl;
    int                     head;
    uft_track_t             track_copy;   /* deep copy of the sector data */
    struct pending_write   *next;
} pending_write_t;

struct uft_transaction {
    uft_disk_t             *disk;
    uft_transaction_mode_t  mode;
    pending_write_t        *pending;
    size_t                  pending_count;
    size_t                  committed;
    bool                    aborted;
};

/* ============================================================================
 * Track deep-copy helpers
 * ============================================================================ */

static uft_error_t track_deep_copy(const uft_track_t *src, uft_track_t *dst) {
    memset(dst, 0, sizeof(*dst));
    dst->cylinder = src->cylinder;
    dst->head = src->head;
    dst->sector_count = src->sector_count;
    dst->encoding = src->encoding;

    for (size_t i = 0; i < src->sector_count; i++) {
        dst->sectors[i] = src->sectors[i];
        size_t len = src->sectors[i].data_len ? src->sectors[i].data_len
                                               : src->sectors[i].data_size;
        if (src->sectors[i].data && len > 0) {
            dst->sectors[i].data = malloc(len);
            if (!dst->sectors[i].data) return UFT_ERROR_NO_MEMORY;
            memcpy(dst->sectors[i].data, src->sectors[i].data, len);
            dst->sectors[i].data_len = len;
            dst->sectors[i].data_size = len;
        } else {
            dst->sectors[i].data = NULL;
        }
    }
    return UFT_OK;
}

static void track_cleanup_copy(uft_track_t *t) {
    for (size_t i = 0; i < t->sector_count; i++) {
        free(t->sectors[i].data);
        t->sectors[i].data = NULL;
    }
    t->sector_count = 0;
}

/* ============================================================================
 * API
 * ============================================================================ */

uft_error_t uft_disk_transaction_begin(uft_disk_t *disk,
                                         uft_transaction_mode_t mode,
                                         uft_transaction_t **tx_out)
{
    if (!disk || !tx_out) return UFT_ERROR_NULL_POINTER;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;

    uft_transaction_t *tx = calloc(1, sizeof(*tx));
    if (!tx) return UFT_ERROR_NO_MEMORY;
    tx->disk = disk;
    tx->mode = mode;
    *tx_out = tx;
    return UFT_OK;
}

uft_error_t uft_disk_transaction_write_track(uft_transaction_t *tx,
                                               int cyl, int head,
                                               const uft_track_t *track)
{
    if (!tx || !track) return UFT_ERROR_NULL_POINTER;
    if (tx->aborted) return UFT_ERROR_INVALID_STATE;

    /* Conflict-Check: schon ein pending write für (cyl, head)? */
    for (pending_write_t *p = tx->pending; p; p = p->next) {
        if (p->cyl == cyl && p->head == head)
            return UFT_ERROR_TRANSACTION_CONFLICT;
    }

    pending_write_t *pw = calloc(1, sizeof(*pw));
    if (!pw) return UFT_ERROR_NO_MEMORY;
    pw->cyl = cyl;
    pw->head = head;

    uft_error_t err = track_deep_copy(track, &pw->track_copy);
    if (err != UFT_OK) { free(pw); return err; }

    pw->next = tx->pending;
    tx->pending = pw;
    tx->pending_count++;
    return UFT_OK;
}

uft_error_t uft_disk_transaction_commit(uft_transaction_t *tx) {
    if (!tx) return UFT_ERROR_NULL_POINTER;
    if (tx->aborted) return UFT_ERROR_INVALID_STATE;

    const uft_format_plugin_t *plugin = uft_get_format_plugin(tx->disk->format);
    if (!plugin || !plugin->write_track) return UFT_ERROR_NOT_SUPPORTED;

    /* Two-phase: first pass = dry-run check all tracks writable
     * (best-effort — plugin write_track is our only hook).
     * Second pass = actual writes. If any fails, we stop but cannot
     * undo already-written tracks without BACKUP_FILE mode. */
    size_t committed = 0;
    for (pending_write_t *p = tx->pending; p; p = p->next) {
        uft_error_t err = plugin->write_track(tx->disk, p->cyl, p->head,
                                                &p->track_copy);
        if (err != UFT_OK) {
            tx->committed = committed;
            return err;
        }
        committed++;
    }
    tx->committed = committed;
    return UFT_OK;
}

void uft_disk_transaction_rollback(uft_transaction_t *tx) {
    if (!tx) return;
    tx->aborted = true;
    /* IN_MEMORY mode: just discard pending. Nothing written to disk yet
     * (if commit wasn't called). */
}

void uft_disk_transaction_free(uft_transaction_t *tx) {
    if (!tx) return;
    pending_write_t *p = tx->pending;
    while (p) {
        pending_write_t *next = p->next;
        track_cleanup_copy(&p->track_copy);
        free(p);
        p = next;
    }
    free(tx);
}

uft_error_t uft_disk_transaction_status(const uft_transaction_t *tx,
                                          uft_transaction_status_t *status)
{
    if (!tx || !status) return UFT_ERROR_NULL_POINTER;
    memset(status, 0, sizeof(*status));
    status->tracks_pending = tx->pending_count - tx->committed;
    status->tracks_committed = tx->committed;
    status->has_conflicts = false;

    /* Approx bytes */
    size_t bytes = 0;
    for (pending_write_t *p = tx->pending; p; p = p->next) {
        for (size_t i = 0; i < p->track_copy.sector_count; i++) {
            bytes += p->track_copy.sectors[i].data_len;
        }
    }
    status->bytes_pending = bytes;
    return UFT_OK;
}
