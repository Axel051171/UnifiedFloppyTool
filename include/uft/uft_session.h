/**
 * @file uft_session.h
 * @brief Session State Management - Persistenz, Auto-Save, Crash-Recovery
 * 
 * @version 5.0.0
 * @date 2026-01-03
 * 
 * TICKET-005: Session State Persistence
 * Priority: MUST
 * 
 * ZWECK:
 * Persistente Session-Verwaltung mit Auto-Save, Crash-Recovery
 * und Resume-Funktionalität für unterbrechungsfreie Workflows.
 * 
 * FEATURES:
 * - Auto-Save alle 60 Sekunden (konfigurierbar)
 * - Crash-Recovery-Dialog bei Neustart
 * - Speichert: Disk-Pfad, Position, Params, Ergebnisse
 * - JSON-Format für Interoperabilität
 * - Cleanup alter Sessions
 * 
 * SESSION STATE:
 * ```
 * session/
 * ├── session_<id>.json      # Haupt-Session-Datei
 * ├── session_<id>.backup    # Backup vor letztem Save
 * ├── session_<id>.lock      # Lock-File für Crash-Detection
 * └── results/
 *     ├── track_00_0.bin     # Track-Ergebnisse
 *     └── report.json        # Analyse-Report
 * ```
 * 
 * USAGE:
 * ```c
 * // Session starten
 * uft_session_t *session = uft_session_create("my_project");
 * 
 * // Auto-Save aktivieren
 * uft_session_enable_autosave(session, 60000); // 60s
 * 
 * // Status speichern
 * uft_session_set_disk(session, disk);
 * uft_session_set_position(session, 42, 0);
 * uft_session_set_params(session, params);
 * 
 * // Bei Crash-Recovery
 * if (uft_session_has_recovery()) {
 *     session = uft_session_recover();
 * }
 * ```
 */

#ifndef UFT_SESSION_H
#define UFT_SESSION_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <time.h>
#include "uft_types.h"
#include "uft_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Session State
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_SESSION_STATE_NEW           = 0,    /* Neu erstellt */
    UFT_SESSION_STATE_ACTIVE        = 1,    /* Aktiv */
    UFT_SESSION_STATE_PAUSED        = 2,    /* Pausiert */
    UFT_SESSION_STATE_COMPLETED     = 3,    /* Abgeschlossen */
    UFT_SESSION_STATE_FAILED        = 4,    /* Fehlgeschlagen */
    UFT_SESSION_STATE_CRASHED       = 5,    /* Crash (Lock vorhanden) */
    UFT_SESSION_STATE_RECOVERED     = 6,    /* Wiederhergestellt */
} uft_session_state_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Session Operation Type
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_SESSION_OP_READ             = 1,
    UFT_SESSION_OP_WRITE            = 2,
    UFT_SESSION_OP_ANALYZE          = 3,
    UFT_SESSION_OP_RECOVER          = 4,
    UFT_SESSION_OP_CONVERT          = 5,
    UFT_SESSION_OP_VERIFY           = 6,
} uft_session_op_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Track Status (für Progress-Tracking)
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_TRACK_STATUS_PENDING        = 0,
    UFT_TRACK_STATUS_PROCESSING     = 1,
    UFT_TRACK_STATUS_COMPLETE       = 2,
    UFT_TRACK_STATUS_FAILED         = 3,
    UFT_TRACK_STATUS_SKIPPED        = 4,
} uft_track_status_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Session Track Info
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uint8_t             cylinder;
    uint8_t             head;
    uft_track_status_t  status;
    
    int                 retry_count;
    double              process_time_ms;
    
    /* Ergebnis-Info */
    int                 sectors_good;
    int                 sectors_bad;
    bool                has_result;
    
} uft_session_track_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Session Info
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    char                *id;            /* Eindeutige Session-ID */
    char                *name;          /* Benutzer-Name */
    char                *path;          /* Session-Verzeichnis */
    
    uft_session_state_t state;
    uft_session_op_t    operation;
    
    /* Timestamps */
    time_t              created;
    time_t              last_modified;
    time_t              last_autosave;
    
    /* Source/Target */
    char                *source_path;   /* Input-Datei/Device */
    char                *target_path;   /* Output-Datei */
    uft_format_t        source_format;
    uft_format_t        target_format;
    
    /* Progress */
    int                 tracks_total;
    int                 tracks_completed;
    int                 tracks_failed;
    float               progress_percent;
    
    /* Aktuelle Position */
    int                 current_cylinder;
    int                 current_head;
    
} uft_session_info_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Session Context (Opaque)
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct uft_session uft_session_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Session Options
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    const char          *base_path;     /* Basis-Pfad für Sessions */
    
    int                 autosave_interval_ms;   /* 0 = disabled */
    bool                create_backup;  /* Backup vor Save */
    bool                compress;       /* JSON komprimieren */
    
    int                 max_sessions;   /* Max alte Sessions behalten */
    int                 max_age_days;   /* Max Alter für Cleanup */
    
    /* Callbacks */
    void                (*on_autosave)(uft_session_t *session, void *user);
    void                (*on_state_change)(uft_session_t *session, 
                                          uft_session_state_t old_state,
                                          uft_session_state_t new_state,
                                          void *user);
    void                *callback_user;
    
} uft_session_options_t;

#define UFT_SESSION_OPTIONS_DEFAULT { \
    .base_path = NULL, \
    .autosave_interval_ms = 60000, \
    .create_backup = true, \
    .compress = false, \
    .max_sessions = 10, \
    .max_age_days = 30, \
    .on_autosave = NULL, \
    .on_state_change = NULL, \
    .callback_user = NULL \
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Session Lifecycle
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Neue Session erstellen
 * @param name Benutzer-Name für Session
 * @return Session-Handle oder NULL
 */
uft_session_t *uft_session_create(const char *name);

/**
 * @brief Session mit Optionen erstellen
 */
uft_session_t *uft_session_create_ex(
    const char *name,
    const uft_session_options_t *options
);

/**
 * @brief Bestehende Session öffnen
 * @param session_id Session-ID
 * @return Session-Handle oder NULL
 */
uft_session_t *uft_session_open(const char *session_id);

/**
 * @brief Session aus Datei laden
 */
uft_session_t *uft_session_load(const char *path);

/**
 * @brief Session speichern
 */
uft_error_t uft_session_save(uft_session_t *session);

/**
 * @brief Session schließen (mit Save)
 */
void uft_session_close(uft_session_t *session);

/**
 * @brief Session löschen
 */
uft_error_t uft_session_delete(uft_session_t *session);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Auto-Save
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Auto-Save aktivieren
 * @param session Session-Handle
 * @param interval_ms Intervall in Millisekunden
 */
uft_error_t uft_session_enable_autosave(
    uft_session_t *session,
    int interval_ms
);

/**
 * @brief Auto-Save deaktivieren
 */
void uft_session_disable_autosave(uft_session_t *session);

/**
 * @brief Auto-Save manuell triggern
 */
uft_error_t uft_session_autosave_now(uft_session_t *session);

/**
 * @brief Zeit seit letztem Save
 */
int uft_session_time_since_save(const uft_session_t *session);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Crash Recovery
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Prüfen ob Recovery-Session existiert
 */
bool uft_session_has_recovery(void);

/**
 * @brief Recovery-Session-Info abrufen
 */
uft_session_info_t *uft_session_get_recovery_info(void);

/**
 * @brief Recovery-Session wiederherstellen
 */
uft_session_t *uft_session_recover(void);

/**
 * @brief Recovery-Session verwerfen
 */
uft_error_t uft_session_discard_recovery(void);

/**
 * @brief Alle crashed Sessions auflisten
 */
uft_session_info_t **uft_session_list_crashed(int *count);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - State Management
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Session-Info abrufen
 */
const uft_session_info_t *uft_session_get_info(const uft_session_t *session);

/**
 * @brief Session-State setzen
 */
void uft_session_set_state(uft_session_t *session, uft_session_state_t state);

/**
 * @brief Operation setzen
 */
void uft_session_set_operation(uft_session_t *session, uft_session_op_t op);

/**
 * @brief Source-Pfad setzen
 */
void uft_session_set_source(uft_session_t *session, const char *path, uft_format_t format);

/**
 * @brief Target-Pfad setzen
 */
void uft_session_set_target(uft_session_t *session, const char *path, uft_format_t format);

/**
 * @brief Aktuelle Position setzen
 */
void uft_session_set_position(uft_session_t *session, int cylinder, int head);

/**
 * @brief Track-Status setzen
 */
void uft_session_set_track_status(
    uft_session_t *session,
    int cylinder,
    int head,
    uft_track_status_t status
);

/**
 * @brief Alle Track-Status abrufen
 */
uft_session_track_t *uft_session_get_tracks(
    const uft_session_t *session,
    int *count
);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Parameter Integration
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Parameter-Set speichern
 */
uft_error_t uft_session_set_params(
    uft_session_t *session,
    const struct uft_params *params
);

/**
 * @brief Parameter-Set laden
 */
struct uft_params *uft_session_get_params(const uft_session_t *session);

/**
 * @brief Preset-Name speichern
 */
void uft_session_set_preset(uft_session_t *session, const char *preset_name);

/**
 * @brief Preset-Name abrufen
 */
const char *uft_session_get_preset(const uft_session_t *session);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Results Storage
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Track-Ergebnis speichern
 */
uft_error_t uft_session_save_track_result(
    uft_session_t *session,
    int cylinder,
    int head,
    const uint8_t *data,
    size_t size
);

/**
 * @brief Track-Ergebnis laden
 */
uft_error_t uft_session_load_track_result(
    const uft_session_t *session,
    int cylinder,
    int head,
    uint8_t **data,
    size_t *size
);

/**
 * @brief Analyse-Report speichern
 */
uft_error_t uft_session_save_report(
    uft_session_t *session,
    const char *report_json
);

/**
 * @brief Analyse-Report laden
 */
char *uft_session_load_report(const uft_session_t *session);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Session List Management
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Alle Sessions auflisten
 */
uft_session_info_t **uft_session_list_all(int *count);

/**
 * @brief Sessions nach Status filtern
 */
uft_session_info_t **uft_session_list_by_state(
    uft_session_state_t state,
    int *count
);

/**
 * @brief Session-Info freigeben
 */
void uft_session_info_free(uft_session_info_t *info);

/**
 * @brief Session-Info-Liste freigeben
 */
void uft_session_info_list_free(uft_session_info_t **list, int count);

/**
 * @brief Alte Sessions aufräumen
 */
int uft_session_cleanup(int max_age_days, int max_count);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Export
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Session als CLI-Script exportieren
 */
uft_error_t uft_session_export_cli(
    const uft_session_t *session,
    const char *script_path
);

/**
 * @brief Session als JSON exportieren
 */
char *uft_session_to_json(const uft_session_t *session);

/**
 * @brief Session-Zusammenfassung
 */
void uft_session_print_summary(const uft_session_t *session);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief State als String
 */
const char *uft_session_state_string(uft_session_state_t state);

/**
 * @brief Operation als String
 */
const char *uft_session_op_string(uft_session_op_t op);

/**
 * @brief Track-Status als String
 */
const char *uft_track_status_string(uft_track_status_t status);

/**
 * @brief Default Session-Pfad abrufen
 */
const char *uft_session_get_default_path(void);

/**
 * @brief Session-Pfad setzen
 */
void uft_session_set_default_path(const char *path);

/**
 * @brief Eindeutige Session-ID generieren
 */
char *uft_session_generate_id(void);

#ifdef __cplusplus
}
#endif

#endif /* UFT_SESSION_H */
