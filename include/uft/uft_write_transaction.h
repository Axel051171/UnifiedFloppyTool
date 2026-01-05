/**
 * @file uft_write_transaction.h
 * @brief Write Transaction System - Abort, Rollback, Atomare Operationen
 * 
 * @version 5.0.0
 * @date 2026-01-03
 * 
 * TICKET-003: Write Abort/Rollback
 * Priority: MUST
 * 
 * ZWECK:
 * Transaction-Modell für Schreiboperationen mit sauberem Abort
 * und Rollback ohne Partial-Writes.
 * 
 * FEATURES:
 * - Atomare Multi-Track-Writes
 * - Sauberer Abort ohne Partial-Writes
 * - Backup vor Write (optional)
 * - Rollback bei Fehler
 * - Transaction-Log für Recovery
 * 
 * USAGE:
 * ```c
 * // 1. Transaction starten
 * uft_write_txn_t *txn = uft_write_txn_begin(disk);
 * 
 * // 2. Operationen hinzufügen
 * uft_write_txn_add_track(txn, cyl, head, data, size);
 * uft_write_txn_add_track(txn, cyl+1, head, data2, size2);
 * 
 * // 3. Commit (schreibt alle oder keine)
 * uft_error_t err = uft_write_txn_commit(txn);
 * if (err != UFT_OK) {
 *     // Automatisches Rollback bei Fehler
 * }
 * 
 * // Oder: Expliziter Abort
 * uft_write_txn_abort(txn);
 * 
 * // 4. Aufräumen
 * uft_write_txn_free(txn);
 * ```
 */

#ifndef UFT_WRITE_TRANSACTION_H
#define UFT_WRITE_TRANSACTION_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "uft_types.h"
#include "uft_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Transaction State
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_TXN_STATE_IDLE          = 0,    /* Nicht gestartet */
    UFT_TXN_STATE_PENDING       = 1,    /* Operationen hinzugefügt */
    UFT_TXN_STATE_COMMITTING    = 2,    /* Commit läuft */
    UFT_TXN_STATE_COMMITTED     = 3,    /* Erfolgreich committed */
    UFT_TXN_STATE_ABORTING      = 4,    /* Abort läuft */
    UFT_TXN_STATE_ABORTED       = 5,    /* Abgebrochen */
    UFT_TXN_STATE_ROLLING_BACK  = 6,    /* Rollback läuft */
    UFT_TXN_STATE_ROLLED_BACK   = 7,    /* Rollback abgeschlossen */
    UFT_TXN_STATE_FAILED        = 8,    /* Fehler (kein Rollback möglich) */
} uft_txn_state_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Operation Type
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_TXN_OP_WRITE_TRACK      = 1,
    UFT_TXN_OP_WRITE_SECTOR     = 2,
    UFT_TXN_OP_WRITE_FLUX       = 3,
    UFT_TXN_OP_FORMAT_TRACK     = 4,
    UFT_TXN_OP_ERASE_TRACK      = 5,
} uft_txn_op_type_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Transaction Operation
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uft_txn_op_type_t   type;
    uint8_t             cylinder;
    uint8_t             head;
    uint8_t             sector;         /* Für Sektor-Ops */
    
    uint8_t             *data;          /* Neue Daten */
    size_t              data_size;
    
    uint8_t             *backup;        /* Backup der alten Daten */
    size_t              backup_size;
    bool                backup_valid;
    
    bool                executed;       /* Wurde ausgeführt? */
    uft_error_t         result;         /* Ergebnis der Operation */
} uft_txn_operation_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Transaction Context
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct uft_write_txn uft_write_txn_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Transaction Options
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    bool                create_backup;      /* Backup vor Write */
    bool                verify_after;       /* Verify nach jedem Write */
    bool                auto_rollback;      /* Rollback bei Fehler */
    
    const char          *log_path;          /* Pfad für Transaction-Log */
    bool                log_enabled;        /* Logging aktivieren */
    
    int                 timeout_ms;         /* Timeout für gesamte Transaction */
    
    /* Abort-Handler */
    bool                (*abort_check)(void *user);
    void                *abort_user;
    
    /* Progress-Callback */
    void                (*progress_fn)(int op_current, int op_total, 
                                       const char *status, void *user);
    void                *progress_user;
    
} uft_txn_options_t;

#define UFT_TXN_OPTIONS_DEFAULT { \
    .create_backup = true, \
    .verify_after = true, \
    .auto_rollback = true, \
    .log_path = NULL, \
    .log_enabled = false, \
    .timeout_ms = 300000, \
    .abort_check = NULL, \
    .abort_user = NULL, \
    .progress_fn = NULL, \
    .progress_user = NULL \
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Transaction Result
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uft_txn_state_t     final_state;
    uft_error_t         error;              /* Fehlercode bei Failure */
    
    int                 operations_total;
    int                 operations_executed;
    int                 operations_succeeded;
    int                 operations_failed;
    int                 operations_rolled_back;
    
    /* Timing */
    double              total_time_ms;
    double              commit_time_ms;
    double              rollback_time_ms;
    
    /* Bei Fehler: Details */
    int                 failed_op_index;
    uint8_t             failed_cyl;
    uint8_t             failed_head;
    const char          *error_message;
    
} uft_txn_result_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Transaction Lifecycle
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Transaction starten
 * @param disk Ziel-Disk
 * @return Transaction-Handle oder NULL bei Fehler
 */
uft_write_txn_t *uft_write_txn_begin(uft_disk_t *disk);

/**
 * @brief Transaction mit Optionen starten
 */
uft_write_txn_t *uft_write_txn_begin_ex(
    uft_disk_t *disk,
    const uft_txn_options_t *options
);

/**
 * @brief Transaction-Status abfragen
 */
uft_txn_state_t uft_write_txn_get_state(const uft_write_txn_t *txn);

/**
 * @brief Transaction freigeben
 * 
 * WICHTIG: Bei PENDING-State wird implizit abort() aufgerufen!
 */
void uft_write_txn_free(uft_write_txn_t *txn);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Operationen hinzufügen
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Track-Write zur Transaction hinzufügen
 * @param txn Transaction-Handle
 * @param cylinder Zylinder
 * @param head Kopf
 * @param data Track-Daten
 * @param size Datengröße
 * @return UFT_OK oder Fehler
 */
uft_error_t uft_write_txn_add_track(
    uft_write_txn_t *txn,
    uint8_t cylinder,
    uint8_t head,
    const uint8_t *data,
    size_t size
);

/**
 * @brief Sektor-Write hinzufügen
 */
uft_error_t uft_write_txn_add_sector(
    uft_write_txn_t *txn,
    uint8_t cylinder,
    uint8_t head,
    uint8_t sector,
    const uint8_t *data,
    size_t size
);

/**
 * @brief Flux-Write hinzufügen
 */
uft_error_t uft_write_txn_add_flux(
    uft_write_txn_t *txn,
    uint8_t cylinder,
    uint8_t head,
    const uint32_t *flux_samples,
    size_t sample_count
);

/**
 * @brief Track formatieren
 */
uft_error_t uft_write_txn_add_format(
    uft_write_txn_t *txn,
    uint8_t cylinder,
    uint8_t head,
    uft_format_t format
);

/**
 * @brief Track löschen
 */
uft_error_t uft_write_txn_add_erase(
    uft_write_txn_t *txn,
    uint8_t cylinder,
    uint8_t head
);

/**
 * @brief Anzahl Operationen abfragen
 */
int uft_write_txn_get_operation_count(const uft_write_txn_t *txn);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Commit/Abort/Rollback
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Transaction committen (alle Writes ausführen)
 * @param txn Transaction-Handle
 * @return UFT_OK wenn alle erfolgreich, sonst Fehlercode
 * 
 * Bei Fehler und auto_rollback=true: automatisches Rollback
 */
uft_error_t uft_write_txn_commit(uft_write_txn_t *txn);

/**
 * @brief Commit mit Result
 */
uft_error_t uft_write_txn_commit_ex(
    uft_write_txn_t *txn,
    uft_txn_result_t *result
);

/**
 * @brief Transaction abbrechen (vor Commit)
 * 
 * Verwirft alle geplanten Operationen ohne zu schreiben.
 */
uft_error_t uft_write_txn_abort(uft_write_txn_t *txn);

/**
 * @brief Rollback (nach partiellem Commit)
 * 
 * Stellt alle bereits geschriebenen Daten aus Backup wieder her.
 * Nur möglich wenn create_backup=true war.
 */
uft_error_t uft_write_txn_rollback(uft_write_txn_t *txn);

/**
 * @brief Abort-Request setzen (für async Abort)
 * 
 * Setzt Flag, das beim nächsten Check geprüft wird.
 */
void uft_write_txn_request_abort(uft_write_txn_t *txn);

/**
 * @brief Prüfen ob Abort angefordert wurde
 */
bool uft_write_txn_abort_requested(const uft_write_txn_t *txn);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Backup Management
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Backup für Track erstellen
 */
uft_error_t uft_write_txn_backup_track(
    uft_write_txn_t *txn,
    uint8_t cylinder,
    uint8_t head
);

/**
 * @brief Backup für alle betroffenen Tracks erstellen
 */
uft_error_t uft_write_txn_backup_all(uft_write_txn_t *txn);

/**
 * @brief Backup-Größe abfragen
 */
size_t uft_write_txn_get_backup_size(const uft_write_txn_t *txn);

/**
 * @brief Backup in Datei speichern
 */
uft_error_t uft_write_txn_save_backup(
    const uft_write_txn_t *txn,
    const char *path
);

/**
 * @brief Backup aus Datei laden
 */
uft_error_t uft_write_txn_load_backup(
    uft_write_txn_t *txn,
    const char *path
);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Transaction Log
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Transaction-Log aktivieren
 */
uft_error_t uft_write_txn_enable_log(
    uft_write_txn_t *txn,
    const char *log_path
);

/**
 * @brief Transaction-Log schließen
 */
uft_error_t uft_write_txn_close_log(uft_write_txn_t *txn);

/**
 * @brief Unterbrochene Transaction aus Log wiederherstellen
 */
uft_write_txn_t *uft_write_txn_recover(
    uft_disk_t *disk,
    const char *log_path
);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Info/Debug
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Transaction-Status als String
 */
const char *uft_txn_state_string(uft_txn_state_t state);

/**
 * @brief Transaction-Info ausgeben
 */
void uft_write_txn_print_info(const uft_write_txn_t *txn);

/**
 * @brief Transaction als JSON
 */
char *uft_write_txn_to_json(const uft_write_txn_t *txn);

/**
 * @brief Result als JSON
 */
char *uft_txn_result_to_json(const uft_txn_result_t *result);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Convenience Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Single-Track atomic Write
 * 
 * Shortcut für: begin → add_track → commit
 */
uft_error_t uft_write_track_atomic(
    uft_disk_t *disk,
    uint8_t cylinder,
    uint8_t head,
    const uint8_t *data,
    size_t size,
    bool create_backup
);

/**
 * @brief Multi-Track atomic Write
 */
typedef struct {
    uint8_t cylinder;
    uint8_t head;
    const uint8_t *data;
    size_t size;
} uft_track_write_t;

uft_error_t uft_write_tracks_atomic(
    uft_disk_t *disk,
    const uft_track_write_t *tracks,
    int track_count,
    bool create_backup
);

#ifdef __cplusplus
}
#endif

#endif /* UFT_WRITE_TRANSACTION_H */
