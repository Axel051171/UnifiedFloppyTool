/**
 * @file uft_disk_transaction.h
 * @brief Atomic multi-track writes with rollback (Master-API Phase 6).
 */
#ifndef UFT_DISK_TRANSACTION_H
#define UFT_DISK_TRANSACTION_H

#include "uft/uft_disk_api_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct uft_transaction uft_transaction_t;  /* opaque */

typedef enum {
    UFT_TX_BACKUP_FILE   = 0,   /* Backup-Datei vor Änderung */
    UFT_TX_SHADOW_WRITE  = 1,   /* Änderungen in Shadow-File, dann rename */
    UFT_TX_IN_MEMORY     = 2,   /* Alle Änderungen in RAM bis commit */
} uft_transaction_mode_t;

typedef struct {
    size_t    tracks_pending;
    size_t    tracks_committed;
    size_t    bytes_pending;
    bool      has_conflicts;
} uft_transaction_status_t;

/* ============================================================================
 * API
 * ============================================================================ */

uft_error_t uft_disk_transaction_begin(uft_disk_t *disk,
                                         uft_transaction_mode_t mode,
                                         uft_transaction_t **tx_out);

uft_error_t uft_disk_transaction_write_track(uft_transaction_t *tx,
                                               int cyl, int head,
                                               const uft_track_t *track);

uft_error_t uft_disk_transaction_commit(uft_transaction_t *tx);

void uft_disk_transaction_rollback(uft_transaction_t *tx);

void uft_disk_transaction_free(uft_transaction_t *tx);

uft_error_t uft_disk_transaction_status(const uft_transaction_t *tx,
                                          uft_transaction_status_t *status);

#ifdef __cplusplus
}
#endif

#endif /* UFT_DISK_TRANSACTION_H */
