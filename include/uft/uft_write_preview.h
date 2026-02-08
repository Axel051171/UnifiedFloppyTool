#ifndef UFT_WRITE_PREVIEW_H
#define UFT_WRITE_PREVIEW_H

/**
 * @file uft_write_preview.h
 * @brief Write Preview Mode - Dry-Run für sichere Schreiboperationen
 * 
 * @version 5.0.0
 * @date 2026-01-03
 * 
 * TICKET-001: Write Preview Mode
 * Priority: MUST
 * 
 * ZWECK:
 * Ermöglicht vollständige Vorschau aller Schreiboperationen BEVOR
 * Änderungen auf das Medium geschrieben werden.
 * 
 * FEATURES:
 * - Dry-Run ohne Dateisystem-Änderungen
 * - Byte-Differenz zu aktuellem Stand
 * - Track-Grid-Vorschau für GUI
 * - CLI --preview Flag
 * - Formatspezifische Validierung
 * 
 * USAGE:
 * ```c
 * // 1. Preview erstellen
 * uft_write_preview_t *preview = uft_write_preview_create(disk);
 * 
 * // 2. Änderungen hinzufügen
 * uft_write_preview_add_track(preview, cyl, head, track_data);
 * uft_write_preview_add_sector(preview, cyl, head, sector, data);
 * 
 * // 3. Analyse durchführen
 * uft_write_preview_report_t *report = uft_write_preview_analyze(preview);
 * 
 * // 4. GUI anzeigen oder CLI ausgeben
 * uft_write_preview_print(report);
 * 
 * // 5. Bei Bestätigung: tatsächlich schreiben
 * if (user_confirmed) {
 *     uft_write_preview_commit(preview);
 * }
 * 
 * // 6. Aufräumen
 * uft_write_preview_destroy(preview);
 * ```
 */


#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "uft_types.h"
#include "uft_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_PREVIEW_MAX_TRACKS      200     /* Max Tracks in Preview */
#define UFT_PREVIEW_MAX_SECTORS     64      /* Max Sectors per Track */

/* ═══════════════════════════════════════════════════════════════════════════════
 * Change Types
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_CHANGE_NONE             = 0,    /* Keine Änderung */
    UFT_CHANGE_MODIFY           = 1,    /* Daten modifiziert */
    UFT_CHANGE_CREATE           = 2,    /* Neu erstellt */
    UFT_CHANGE_DELETE           = 3,    /* Gelöscht/überschrieben */
    UFT_CHANGE_FORMAT           = 4,    /* Formatierung geändert */
} uft_change_type_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Validation Result
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_VALIDATE_OK             = 0,    /* Validierung erfolgreich */
    UFT_VALIDATE_WARN           = 1,    /* Warnung, aber erlaubt */
    UFT_VALIDATE_ERROR          = 2,    /* Fehler, nicht erlaubt */
    UFT_VALIDATE_FATAL          = 3,    /* Fataler Fehler */
} uft_validate_result_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Sector Change Entry
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uint8_t             sector;         /* Sektor-Nummer */
    uft_change_type_t   change_type;    /* Art der Änderung */
    
    /* Byte-Level Differenz */
    size_t              bytes_total;    /* Gesamtgröße */
    size_t              bytes_changed;  /* Geänderte Bytes */
    size_t              bytes_added;    /* Neue Bytes */
    size_t              bytes_removed;  /* Entfernte Bytes */
    
    /* CRC/Checksum */
    uint32_t            crc_before;     /* CRC vor Änderung */
    uint32_t            crc_after;      /* CRC nach Änderung */
    bool                crc_valid;      /* CRC gültig? */
    
    /* Daten-Pointer (für Diff-View) */
    const uint8_t       *data_before;   /* Alte Daten (NULL wenn neu) */
    const uint8_t       *data_after;    /* Neue Daten */
    
    /* Diff-Bitmap (1 Bit pro Byte: 1=geändert) */
    uint8_t             *diff_bitmap;
    size_t              diff_bitmap_size;
    
} uft_sector_change_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Track Change Entry
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uint8_t             cylinder;       /* Zylinder */
    uint8_t             head;           /* Kopf (0/1) */
    uft_change_type_t   change_type;    /* Track-Level Änderung */
    
    /* Sektor-Änderungen */
    uft_sector_change_t *sectors;       /* Array der Sektor-Änderungen */
    int                 sector_count;   /* Anzahl geänderter Sektoren */
    
    /* Track-Level Statistik */
    size_t              bytes_total;
    size_t              bytes_changed;
    float               change_percent; /* 0.0 - 100.0 */
    
    /* Für Flux-Level Formate */
    bool                flux_level;     /* Flux-Daten? */
    size_t              flux_samples;   /* Anzahl Flux-Samples */
    
    /* Validierung */
    uft_validate_result_t validation;
    char                *validation_message;
    
} uft_track_change_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Preview Report
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    /* Disk Info */
    const char          *disk_path;
    uft_format_t        format;
    
    /* Zusammenfassung */
    int                 tracks_total;       /* Tracks auf Disk */
    int                 tracks_modified;    /* Zu ändernde Tracks */
    int                 sectors_modified;   /* Zu ändernde Sektoren */
    
    size_t              bytes_total;        /* Bytes auf Disk */
    size_t              bytes_to_write;     /* Zu schreibende Bytes */
    size_t              bytes_changed;      /* Tatsächlich geändert */
    
    /* Track-Änderungen */
    uft_track_change_t  *tracks;            /* Array der Track-Änderungen */
    int                 track_count;
    
    /* Validierung */
    uft_validate_result_t overall_validation;
    int                 warning_count;
    int                 error_count;
    char                **messages;         /* Validierungs-Meldungen */
    int                 message_count;
    
    /* Risiko-Bewertung */
    int                 risk_score;         /* 0-100 */
    const char          *risk_description;
    
    /* Hashes für Forensik */
    char                hash_before[65];    /* SHA-256 vor Änderung */
    char                hash_after[65];     /* SHA-256 nach Änderung */
    
} uft_write_preview_report_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Preview Options
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    bool                validate_crc;       /* CRCs prüfen */
    bool                validate_format;    /* Format-Constraints prüfen */
    bool                generate_diff;      /* Diff-Bitmaps erstellen */
    bool                compute_hashes;     /* SHA-256 Hashes */
    bool                include_unchanged;  /* Auch ungeänderte Tracks */
    int                 max_diff_bytes;     /* Max Bytes im Diff (0=alle) */
} uft_preview_options_t;

#define UFT_PREVIEW_OPTIONS_DEFAULT { \
    .validate_crc = true, \
    .validate_format = true, \
    .generate_diff = true, \
    .compute_hashes = true, \
    .include_unchanged = false, \
    .max_diff_bytes = 4096 \
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Preview Context (Opaque)
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct uft_write_preview uft_write_preview_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Preview Lifecycle
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Preview-Kontext erstellen
 * @param disk Ziel-Disk (read-only verwendet)
 * @return Preview-Handle oder NULL bei Fehler
 */
uft_write_preview_t *uft_write_preview_create(uft_disk_t *disk);

/**
 * @brief Preview mit Optionen erstellen
 */
uft_write_preview_t *uft_write_preview_create_ex(
    uft_disk_t *disk,
    const uft_preview_options_t *options
);

/**
 * @brief Preview freigeben
 */
void uft_write_preview_destroy(uft_write_preview_t *preview);

/**
 * @brief Preview zurücksetzen (für Neustart)
 */
void uft_write_preview_reset(uft_write_preview_t *preview);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Änderungen hinzufügen
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Track-Daten für Preview hinzufügen
 * @param preview Preview-Handle
 * @param cylinder Zylinder-Nummer
 * @param head Kopf (0/1)
 * @param data Track-Daten
 * @param size Datengröße
 * @return UFT_OK oder Fehlercode
 */
uft_error_t uft_write_preview_add_track(
    uft_write_preview_t *preview,
    uint8_t cylinder,
    uint8_t head,
    const uint8_t *data,
    size_t size
);

/**
 * @brief Sektor-Daten für Preview hinzufügen
 */
uft_error_t uft_write_preview_add_sector(
    uft_write_preview_t *preview,
    uint8_t cylinder,
    uint8_t head,
    uint8_t sector,
    const uint8_t *data,
    size_t size
);

/**
 * @brief Flux-Daten für Preview hinzufügen
 */
uft_error_t uft_write_preview_add_flux(
    uft_write_preview_t *preview,
    uint8_t cylinder,
    uint8_t head,
    const uint32_t *flux_samples,
    size_t sample_count
);

/**
 * @brief Ganzes Disk-Image als Preview
 */
uft_error_t uft_write_preview_set_image(
    uft_write_preview_t *preview,
    const uint8_t *image_data,
    size_t image_size
);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Analyse
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Preview analysieren und Report erstellen
 * @param preview Preview-Handle
 * @return Report (muss mit uft_write_preview_report_free() freigegeben werden)
 */
uft_write_preview_report_t *uft_write_preview_analyze(
    uft_write_preview_t *preview
);

/**
 * @brief Report freigeben
 */
void uft_write_preview_report_free(uft_write_preview_report_t *report);

/**
 * @brief Schnelle Validierung ohne vollständigen Report
 * @return true wenn alle Änderungen valide
 */
bool uft_write_preview_validate(uft_write_preview_t *preview);

/**
 * @brief Änderungs-Anzahl abfragen
 */
int uft_write_preview_get_change_count(const uft_write_preview_t *preview);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Commit
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Preview-Änderungen tatsächlich schreiben
 * @param preview Preview-Handle
 * @return UFT_OK oder Fehlercode
 * 
 * WARNUNG: Diese Funktion führt die Schreiboperationen aus!
 */
uft_error_t uft_write_preview_commit(uft_write_preview_t *preview);

/**
 * @brief Commit mit Progress-Callback
 */
typedef void (*uft_preview_progress_fn)(
    int track_current,
    int track_total,
    void *user_data
);

uft_error_t uft_write_preview_commit_ex(
    uft_write_preview_t *preview,
    uft_preview_progress_fn progress,
    void *user_data
);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Output
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Report als Text ausgeben (für CLI)
 */
void uft_write_preview_print(const uft_write_preview_report_t *report);

/**
 * @brief Report als JSON exportieren
 */
char *uft_write_preview_to_json(const uft_write_preview_report_t *report);

/**
 * @brief Report in Datei schreiben
 */
uft_error_t uft_write_preview_save_report(
    const uft_write_preview_report_t *report,
    const char *path
);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Track Grid Data (für GUI)
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Track-Status für GUI-Grid abrufen
 * @param report Preview-Report
 * @param cylinder Zylinder
 * @param head Kopf
 * @return Change-Type für diesen Track
 */
uft_change_type_t uft_write_preview_get_track_status(
    const uft_write_preview_report_t *report,
    uint8_t cylinder,
    uint8_t head
);

/**
 * @brief Änderungs-Prozent für Track (für Heatmap)
 */
float uft_write_preview_get_track_change_percent(
    const uft_write_preview_report_t *report,
    uint8_t cylinder,
    uint8_t head
);

/**
 * @brief Sektor-Diff für bestimmten Track
 */
const uft_sector_change_t *uft_write_preview_get_sector_changes(
    const uft_write_preview_report_t *report,
    uint8_t cylinder,
    uint8_t head,
    int *count
);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Change-Type als String
 */
const char *uft_change_type_string(uft_change_type_t type);

/**
 * @brief Validation-Result als String
 */
const char *uft_validate_result_string(uft_validate_result_t result);

/**
 * @brief Risiko-Score-Beschreibung
 */
const char *uft_risk_score_description(int score);

#ifdef __cplusplus
}
#endif

#endif /* UFT_WRITE_PREVIEW_H */
