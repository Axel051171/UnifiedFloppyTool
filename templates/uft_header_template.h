/**
 * @file uft_MODULNAME.h
 * @brief KURZE BESCHREIBUNG
 * 
 * AUSFÜHRLICHE BESCHREIBUNG
 * 
 * @author DEIN NAME
 * @date DATUM
 * @version 1.0.0
 * 
 * @note Erstellt nach UFT Developer Guide Richtlinien
 */

#ifndef UFT_MODULNAME_H
#define UFT_MODULNAME_H

// ═══════════════════════════════════════════════════════════════════════════════
// STANDARD INCLUDES (alphabetisch sortiert)
// ═══════════════════════════════════════════════════════════════════════════════

#include <stdbool.h>
#include <stddef.h>     // Für size_t - PFLICHT wenn verwendet!
#include <stdint.h>

// ═══════════════════════════════════════════════════════════════════════════════
// UFT CORE INCLUDES
// ═══════════════════════════════════════════════════════════════════════════════

#include "uft/uft_error.h"
#include "uft/uft_types.h"

// ═══════════════════════════════════════════════════════════════════════════════
// UFT MODUL INCLUDES (nur wenn benötigt)
// ═══════════════════════════════════════════════════════════════════════════════

// Für FloppyDevice, FluxTimingProfile, etc.:
// #include "uft/formats/uft_floppy_device.h"

// Für Safe-Math Funktionen:
// #include "uft/uft_safe.h"

// Für Endian-Funktionen:
// #include "uft/core/uft_endian.h"

// ═══════════════════════════════════════════════════════════════════════════════
// C++ KOMPATIBILITÄT
// ═══════════════════════════════════════════════════════════════════════════════

#ifdef __cplusplus
extern "C" {
#endif

// ═══════════════════════════════════════════════════════════════════════════════
// KONSTANTEN
// ═══════════════════════════════════════════════════════════════════════════════

#define UFT_MODULNAME_VERSION_MAJOR 1
#define UFT_MODULNAME_VERSION_MINOR 0
#define UFT_MODULNAME_VERSION_PATCH 0

// ═══════════════════════════════════════════════════════════════════════════════
// TYPEN
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief Konfigurations-Struktur für MODULNAME
 */
typedef struct {
    uint32_t option1;       ///< Beschreibung Option 1
    uint32_t option2;       ///< Beschreibung Option 2
    bool     enable_x;      ///< Beschreibung enable_x
} uft_modulname_config_t;

/**
 * @brief Ergebnis-Struktur für MODULNAME
 */
typedef struct {
    int      status;        ///< Status-Code
    uint32_t result_value;  ///< Ergebniswert
} uft_modulname_result_t;

/**
 * @brief Opaque Handle (für private Implementierung)
 * 
 * Die tatsächliche Struktur ist in der .c Datei definiert.
 */
typedef struct uft_modulname_s uft_modulname_t;

// ═══════════════════════════════════════════════════════════════════════════════
// LIFECYCLE FUNKTIONEN
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief Erstellt eine neue MODULNAME Instanz
 * 
 * @param[in]  config  Konfiguration (NULL für Defaults)
 * @param[out] handle  Output: Handle zur Instanz
 * @return UFT_OK bei Erfolg, Fehlercode sonst
 */
uft_error_t uft_modulname_create(
    const uft_modulname_config_t* config,
    uft_modulname_t** handle
);

/**
 * @brief Gibt MODULNAME Instanz frei
 * 
 * @param[in,out] handle  Pointer auf Handle (wird auf NULL gesetzt)
 */
void uft_modulname_destroy(uft_modulname_t** handle);

// ═══════════════════════════════════════════════════════════════════════════════
// KERN-FUNKTIONEN
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief Hauptfunktion von MODULNAME
 * 
 * @param[in]  handle  Handle zur Instanz
 * @param[in]  input   Eingabedaten
 * @param[in]  size    Größe der Eingabedaten
 * @param[out] result  Output: Ergebnis
 * @return UFT_OK bei Erfolg, Fehlercode sonst
 * 
 * @note Diese Funktion ist thread-safe / NICHT thread-safe
 */
uft_error_t uft_modulname_process(
    uft_modulname_t* handle,
    const uint8_t* input,
    size_t size,
    uft_modulname_result_t* result
);

// ═══════════════════════════════════════════════════════════════════════════════
// HILFSFUNKTIONEN
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief Gibt die Version als String zurück
 */
const char* uft_modulname_version(void);

/**
 * @brief Setzt Konfiguration auf Defaults
 */
void uft_modulname_config_defaults(uft_modulname_config_t* config);

// ═══════════════════════════════════════════════════════════════════════════════
// C++ KOMPATIBILITÄT ENDE
// ═══════════════════════════════════════════════════════════════════════════════

#ifdef __cplusplus
}
#endif

#endif // UFT_MODULNAME_H
