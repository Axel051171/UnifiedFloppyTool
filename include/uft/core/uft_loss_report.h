/**
 * @file uft_loss_report.h
 * @brief Prinzip 1 — Lossy-Documented Sidecar (.loss.json)
 *
 * Wenn eine Konvertierung LOSSY-DOCUMENTED ist (siehe Round-Trip-Matrix),
 * schreibt UFT neben der Ziel-Datei einen `<target>.loss.json` Sidecar
 * mit strukturierter Liste was verloren ging.
 *
 * Schema-Version: uft-loss-report-v1
 *
 * Siehe docs/DESIGN_PRINCIPLES.md §1.
 */

#ifndef UFT_LOSS_REPORT_H
#define UFT_LOSS_REPORT_H

#include "uft/uft_error.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Kategorie des Informationsverlusts
 *
 * Werte explizit nummeriert — ABI-stabil und als String im JSON.
 */
typedef enum uft_loss_category {
    UFT_LOSS_WEAK_BITS         = 0,  ///< Weak/Fuzzy-Bits verloren
    UFT_LOSS_FLUX_TIMING       = 1,  ///< Sub-Bit-Timing / Jitter verloren
    UFT_LOSS_INDEX_PULSES      = 2,  ///< Index-Puls-Positionen verloren
    UFT_LOSS_SYNC_PATTERNS     = 3,  ///< Custom Sync-Marker verloren
    UFT_LOSS_MULTI_REVOLUTION  = 4,  ///< Nur 1 Revolution kopiert
    UFT_LOSS_CUSTOM_METADATA   = 5,  ///< Format-spezifische Metadaten
    UFT_LOSS_COPY_PROTECTION   = 6,  ///< Protection-Signaturen
    UFT_LOSS_LONG_TRACKS       = 7,  ///< Long-Track Information
    UFT_LOSS_HALF_TRACKS       = 8,  ///< Halb-/Quarter-Tracks
    UFT_LOSS_WRITE_SPLICE      = 9,  ///< Write-Splice Position
    UFT_LOSS_OTHER             = 99, ///< Anderes — description erklärt
} uft_loss_category_t;

/**
 * @brief Ein einzelner Verlust-Eintrag
 *
 * `description` ist für alle Kategorien empfohlen, aber Pflicht für
 * UFT_LOSS_OTHER. Zeigt auf einen statischen oder Lebensdauer-konformen
 * String (wird nicht kopiert).
 */
typedef struct uft_loss_entry {
    uft_loss_category_t category;
    uint32_t            count;        ///< Anzahl betroffener Einheiten
    const char         *description;  ///< Menschenlesbar
} uft_loss_entry_t;

/**
 * @brief Vollständiger Loss-Report
 *
 * Alle String-Felder zeigen auf Puffer, die bis zum Ende des
 * `uft_loss_report_write()` Aufrufs gültig bleiben müssen.
 */
typedef struct uft_loss_report {
    const char             *source_path;
    const char             *target_path;
    const char             *source_format;  ///< z.B. "SCP"
    const char             *target_format;  ///< z.B. "IMG"
    uint64_t                timestamp_unix;
    const char             *uft_version;    ///< Producer-Version
    const uft_loss_entry_t *entries;
    size_t                  entry_count;
} uft_loss_report_t;

/**
 * @brief Kategorie als String (z.B. "WEAK_BITS")
 * @return statischer String, nie NULL
 */
const char *uft_loss_category_string(uft_loss_category_t c);

/**
 * @brief Loss-Report als JSON in einen FILE* schreiben
 *
 * @param report  Nicht-NULL. entries darf NULL sein wenn entry_count==0.
 * @param out     geöffnete Datei (Text-Modus bevorzugt)
 * @return UFT_OK oder Fehlercode
 */
uft_error_t uft_loss_report_write_stream(const uft_loss_report_t *report,
                                          void *out_file);

/**
 * @brief Loss-Report neben `target_path` als `<target_path>.loss.json` ablegen
 *
 * @param report       Der Report. `report->target_path` sollte passen.
 * @param sidecar_path Optional. NULL → `<target_path>.loss.json`.
 * @return UFT_OK oder Fehlercode
 */
uft_error_t uft_loss_report_write(const uft_loss_report_t *report,
                                    const char *sidecar_path);

/**
 * @brief Schema-Version-String (aktuell "uft-loss-report-v1")
 */
const char *uft_loss_report_schema_version(void);

#ifdef __cplusplus
}
#endif

#endif /* UFT_LOSS_REPORT_H */
