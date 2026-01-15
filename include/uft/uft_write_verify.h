/**
 * @file uft_write_verify.h
 * @brief Write Verification System - Bitgenaue Prüfung nach Schreiboperationen
 * 
 * @version 5.0.0
 * @date 2026-01-03
 * 
 * TICKET-002: Verify After Write
 * Priority: MUST
 * 
 * ZWECK:
 * Automatische Verifikation nach jeder Schreiboperation durch
 * Rücklesen und bitgenauen Vergleich.
 * 
 * FEATURES:
 * - Bitgenauer Vergleich für alle Formate
 * - CRC/Checksum-Prüfung
 * - Retry-Option bei Verify-Fehler
 * - Detaillierter Verify-Report
 * - Format-spezifische Verifier
 * 
 * USAGE:
 * ```c
 * // Option 1: Automatisch mit Write
 * uft_write_verify_options_t opts = UFT_WRITE_VERIFY_OPTIONS_DEFAULT;
 * opts.verify = true;
 * opts.verify_retries = 3;
 * uft_disk_write_track_ex(disk, cyl, head, data, &opts);
 * 
 * // Option 2: Manuelle Verifikation
 * uft_verify_result_t *result = uft_verify_track(disk, cyl, head, expected);
 * if (result->status != UFT_VERIFY_OK) {
 *     printf("Verify failed at byte %zu\n", result->first_mismatch);
 * }
 * uft_verify_result_free(result);
 * ```
 */

#ifndef UFT_WRITE_VERIFY_H
#define UFT_WRITE_VERIFY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "uft_types.h"
#include "uft_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Verify Status
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_VERIFY_OK               = 0,    /* Verifizierung erfolgreich */
    UFT_VERIFY_MISMATCH         = 1,    /* Daten stimmen nicht überein */
    UFT_VERIFY_CRC_ERROR        = 2,    /* CRC-Fehler */
    UFT_VERIFY_READ_ERROR       = 3,    /* Lesefehler beim Verify */
    UFT_VERIFY_SIZE_MISMATCH    = 4,    /* Größe stimmt nicht */
    UFT_VERIFY_FORMAT_ERROR     = 5,    /* Format-Fehler */
    UFT_VERIFY_TIMEOUT          = 6,    /* Timeout */
    UFT_VERIFY_ABORTED          = 7,    /* Vom Benutzer abgebrochen */
} uft_verify_status_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Verify Mode
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_VERIFY_MODE_BITWISE     = 0,    /* Bitgenauer Vergleich */
    UFT_VERIFY_MODE_CRC         = 1,    /* Nur CRC prüfen */
    UFT_VERIFY_MODE_SECTOR      = 2,    /* Sektor-Level (ignoriert Gaps) */
    UFT_VERIFY_MODE_FLUX        = 3,    /* Flux-Level (mit Toleranz) */
} uft_verify_mode_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Mismatch Entry
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    size_t              offset;         /* Byte-Offset */
    uint8_t             expected;       /* Erwarteter Wert */
    uint8_t             actual;         /* Tatsächlicher Wert */
    uint8_t             xor_diff;       /* XOR-Differenz */
} uft_verify_mismatch_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Sector Verify Result
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uint8_t             sector;         /* Sektor-Nummer */
    uft_verify_status_t status;         /* Verify-Status */
    
    /* CRC Info */
    uint32_t            crc_expected;
    uint32_t            crc_actual;
    bool                crc_valid;
    
    /* Mismatch Details */
    int                 mismatch_count;
    uft_verify_mismatch_t *mismatches;  /* Array (NULL wenn OK) */
    int                 max_mismatches; /* Limit für Array */
    
    /* Byte-Level */
    size_t              bytes_total;
    size_t              bytes_matching;
    float               match_percent;  /* 0.0 - 100.0 */
    
} uft_sector_verify_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Track Verify Result
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uint8_t             cylinder;
    uint8_t             head;
    uft_verify_status_t status;         /* Gesamt-Status */
    
    /* Sektor-Results */
    uft_sector_verify_t *sectors;
    int                 sector_count;
    int                 sectors_ok;
    int                 sectors_failed;
    
    /* Track-Level Stats */
    size_t              bytes_total;
    size_t              bytes_matching;
    float               match_percent;
    
    /* Timing (für Performance) */
    double              write_time_ms;
    double              read_time_ms;
    double              verify_time_ms;
    
    /* Retry Info */
    int                 retry_count;
    
} uft_track_verify_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Full Verify Result
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uft_verify_status_t status;         /* Gesamt-Status */
    
    /* Track-Results */
    uft_track_verify_t  *tracks;
    int                 track_count;
    int                 tracks_ok;
    int                 tracks_failed;
    
    /* Gesamt-Statistik */
    size_t              bytes_total;
    size_t              bytes_verified;
    size_t              bytes_matching;
    float               overall_match_percent;
    
    /* Erstes Problem */
    bool                has_first_mismatch;
    uint8_t             first_mismatch_cyl;
    uint8_t             first_mismatch_head;
    uint8_t             first_mismatch_sector;
    size_t              first_mismatch_offset;
    
    /* Hashes */
    char                hash_expected[65];  /* SHA-256 */
    char                hash_actual[65];
    
    /* Timing */
    double              total_time_ms;
    
} uft_verify_result_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Verify Options
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uft_verify_mode_t   mode;           /* Verify-Modus */
    int                 max_retries;    /* Retries bei Fehler */
    int                 retry_delay_ms; /* Delay zwischen Retries */
    
    bool                stop_on_first;  /* Bei erstem Fehler stoppen */
    int                 max_mismatches; /* Max Mismatches pro Sektor */
    
    bool                compute_hashes; /* SHA-256 berechnen */
    bool                include_gaps;   /* Gap-Daten mitvergleichen */
    
    /* Für Flux-Level */
    float               flux_tolerance; /* Toleranz in % (z.B. 5.0) */
    int                 flux_window;    /* Sample-Window für Vergleich */
    
    /* Callback für Progress */
    void                (*progress_fn)(int track, int total, void *user);
    void                *progress_user;
    
} uft_verify_options_t;

#define UFT_VERIFY_OPTIONS_DEFAULT { \
    .mode = UFT_VERIFY_MODE_BITWISE, \
    .max_retries = 3, \
    .retry_delay_ms = 100, \
    .stop_on_first = false, \
    .max_mismatches = 100, \
    .compute_hashes = true, \
    .include_gaps = false, \
    .flux_tolerance = 5.0f, \
    .flux_window = 10, \
    .progress_fn = NULL, \
    .progress_user = NULL \
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Write with Verify Options
 * 
 * Note: This is uft_write_verify_options_t, not uft_write_options_t
 * (which is defined in uft_types.h for general write operations)
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    bool                verify;         /* Verify nach Write */
    uft_verify_options_t verify_options;/* Verify-Optionen */
    
    bool                precomp;        /* Write Pre-Compensation */
    int                 precomp_ns;     /* Pre-Compensation in ns */
    
    bool                erase_first;    /* Track vorher löschen */
    
    /* Abort-Handler */
    bool                (*abort_check)(void *user);
    void                *abort_user;
    
} uft_write_verify_options_t;

#define UFT_WRITE_VERIFY_OPTIONS_DEFAULT { \
    .verify = true, \
    .verify_options = UFT_VERIFY_OPTIONS_DEFAULT, \
    .precomp = false, \
    .precomp_ns = 0, \
    .erase_first = false, \
    .abort_check = NULL, \
    .abort_user = NULL \
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Track-Level Verification
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Einzelnen Track verifizieren
 * @param disk Disk-Handle
 * @param cylinder Zylinder
 * @param head Kopf
 * @param expected Erwartete Daten
 * @param size Datengröße
 * @return Verify-Result (muss freigegeben werden)
 */
uft_track_verify_t *uft_verify_track(
    uft_disk_t *disk,
    uint8_t cylinder,
    uint8_t head,
    const uint8_t *expected,
    size_t size
);

/**
 * @brief Track mit Optionen verifizieren
 */
uft_track_verify_t *uft_verify_track_ex(
    uft_disk_t *disk,
    uint8_t cylinder,
    uint8_t head,
    const uint8_t *expected,
    size_t size,
    const uft_verify_options_t *options
);

/**
 * @brief Track-Result freigeben
 */
void uft_track_verify_free(uft_track_verify_t *result);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Sector-Level Verification
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Einzelnen Sektor verifizieren
 */
uft_sector_verify_t *uft_verify_sector(
    uft_disk_t *disk,
    uint8_t cylinder,
    uint8_t head,
    uint8_t sector,
    const uint8_t *expected,
    size_t size
);

/**
 * @brief Sektor-Result freigeben
 */
void uft_sector_verify_free(uft_sector_verify_t *result);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Disk-Level Verification
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Komplette Disk verifizieren
 * @param disk Disk-Handle
 * @param reference Referenz-Disk oder Image-Daten
 * @param ref_size Referenz-Größe
 * @return Verify-Result
 */
uft_verify_result_t *uft_verify_disk(
    uft_disk_t *disk,
    const uint8_t *reference,
    size_t ref_size
);

/**
 * @brief Disk mit Optionen verifizieren
 */
uft_verify_result_t *uft_verify_disk_ex(
    uft_disk_t *disk,
    const uint8_t *reference,
    size_t ref_size,
    const uft_verify_options_t *options
);

/**
 * @brief Zwei Disk-Images vergleichen
 */
uft_verify_result_t *uft_verify_compare_disks(
    uft_disk_t *disk1,
    uft_disk_t *disk2,
    const uft_verify_options_t *options
);

/**
 * @brief Result freigeben
 */
void uft_verify_result_free(uft_verify_result_t *result);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Write with Verify
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Track schreiben mit automatischer Verifikation
 * @param disk Disk-Handle
 * @param cylinder Zylinder
 * @param head Kopf
 * @param data Track-Daten
 * @param size Datengröße
 * @param options Write-Optionen (inkl. Verify)
 * @param verify_result Output: Verify-Result (optional, NULL wenn nicht benötigt)
 * @return UFT_OK wenn Write und Verify erfolgreich
 */
uft_error_t uft_disk_write_track_verified(
    uft_disk_t *disk,
    uint8_t cylinder,
    uint8_t head,
    const uint8_t *data,
    size_t size,
    const uft_write_verify_options_t *options,
    uft_track_verify_t **verify_result
);

/**
 * @brief Sektor schreiben mit Verifikation
 */
uft_error_t uft_disk_write_sector_verified(
    uft_disk_t *disk,
    uint8_t cylinder,
    uint8_t head,
    uint8_t sector,
    const uint8_t *data,
    size_t size,
    const uft_write_verify_options_t *options,
    uft_sector_verify_t **verify_result
);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Format-Specific Verifiers
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Format-spezifischen Verifier registrieren
 */
typedef uft_verify_status_t (*uft_format_verifier_fn)(
    const uint8_t *expected,
    const uint8_t *actual,
    size_t size,
    void *format_ctx
);

int uft_verify_register_format(
    uft_format_t format,
    uft_format_verifier_fn verifier
);

/**
 * @brief Amiga-spezifische Verifikation (Checksum)
 */
uft_verify_status_t uft_verify_amiga_track(
    const uint8_t *expected,
    const uint8_t *actual,
    size_t size
);

/**
 * @brief Commodore GCR Verifikation
 */
uft_verify_status_t uft_verify_c64_track(
    const uint8_t *expected,
    const uint8_t *actual,
    size_t size
);

/**
 * @brief Apple GCR Verifikation
 */
uft_verify_status_t uft_verify_apple_track(
    const uint8_t *expected,
    const uint8_t *actual,
    size_t size
);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Reporting
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Verify-Result als Text ausgeben
 */
void uft_verify_result_print(const uft_verify_result_t *result);

/**
 * @brief Verify-Result als JSON
 */
char *uft_verify_result_to_json(const uft_verify_result_t *result);

/**
 * @brief Verify-Report in Datei speichern
 */
uft_error_t uft_verify_result_save(
    const uft_verify_result_t *result,
    const char *path
);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Status als String
 */
const char *uft_verify_status_string(uft_verify_status_t status);

/**
 * @brief Mode als String
 */
const char *uft_verify_mode_string(uft_verify_mode_t mode);

/**
 * @brief Schneller Byte-Vergleich mit erstem Mismatch
 */
bool uft_verify_bytes(
    const uint8_t *expected,
    const uint8_t *actual,
    size_t size,
    size_t *first_mismatch
);

/**
 * @brief CRC-basierte Schnellprüfung
 */
bool uft_verify_crc(
    const uint8_t *data,
    size_t size,
    uint32_t expected_crc
);

#ifdef __cplusplus
}
#endif

#endif /* UFT_WRITE_VERIFY_H */
