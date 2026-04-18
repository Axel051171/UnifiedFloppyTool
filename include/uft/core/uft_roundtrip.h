/**
 * @file uft_roundtrip.h
 * @brief Prinzip 5 — Round-Trip-Matrix
 *
 * Für jedes Format-Paar (from, to) pflegt UFT einen Round-Trip-Status:
 *   LL (LOSSLESS) ............ byte-identischer Round-Trip getestet
 *   LD (LOSSY_DOCUMENTED) .... Verlust bekannt & dokumentiert
 *   IM (IMPOSSIBLE) .......... Ziel kann Quelle nicht repräsentieren
 *   UT (UNTESTED) ............ ungeprüft — Konvertierung wird nicht angeboten
 *
 * Default für nicht in der Matrix gelistete Paare ist `UT`. Das ist
 * Absicht (Prinzip 5): nichts ist implizit LL.
 *
 * Siehe docs/DESIGN_PRINCIPLES.md §5.
 */

#ifndef UFT_ROUNDTRIP_H
#define UFT_ROUNDTRIP_H

#include "uft/uft_error.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Format-Identifier (enum-agnostisch)
 *
 * Das Projekt hat historisch mehrere UFT_FORMAT_*-Enums in parallelen
 * Headern. Die Round-Trip-Matrix ist entkoppelt: sie akzeptiert jede
 * der kompatiblen uint32_t-Werte. Aufrufer übergibt z.B.
 * `(uft_format_id_t)UFT_FORMAT_SCP`.
 */
typedef uint32_t uft_format_id_t;

/**
 * @brief Round-Trip-Status eines Format-Paars
 *
 * Werte explizit nummeriert — ABI-stabil.
 */
typedef enum uft_roundtrip_status {
    UFT_RT_UNTESTED          = 0,  ///< Ungeprüft — Default
    UFT_RT_LOSSLESS          = 1,  ///< byte-identischer Round-Trip verifiziert
    UFT_RT_LOSSY_DOCUMENTED  = 2,  ///< Verlust bekannt & in Kategorien dokumentiert
    UFT_RT_IMPOSSIBLE        = 3,  ///< Ziel-Format kann Quelle nicht repräsentieren
} uft_roundtrip_status_t;

/**
 * @brief Eintrag in der Round-Trip-Matrix
 */
typedef struct uft_roundtrip_entry {
    uft_format_id_t        from;
    uft_format_id_t        to;
    uft_roundtrip_status_t status;
    const char            *note;  ///< Menschenlesbar, z.B. "weak-bits lost"
} uft_roundtrip_entry_t;

/**
 * @brief Status für ein Format-Paar abrufen
 *
 * Sucht den Eintrag in der Matrix. Nicht gelistete Paare liefern
 * `UFT_RT_UNTESTED`.
 *
 * @return einen der uft_roundtrip_status_t Werte
 */
uft_roundtrip_status_t uft_roundtrip_status(uft_format_id_t from,
                                             uft_format_id_t to);

/**
 * @brief Begleit-Notiz für ein Paar (optional)
 * @return statischer String, nie NULL (leerer String wenn kein Eintrag).
 */
const char *uft_roundtrip_note(uft_format_id_t from, uft_format_id_t to);

/**
 * @brief Status als String ("LOSSLESS", "LOSSY-DOCUMENTED", "IMPOSSIBLE", "UNTESTED")
 * @return statischer String, nie NULL
 */
const char *uft_roundtrip_status_string(uft_roundtrip_status_t s);

/**
 * @brief Kompakte Status-Notation ("LL", "LD", "IM", "UT") — für Tabellen/CLI
 * @return statischer 2-Zeichen-String, nie NULL
 */
const char *uft_roundtrip_status_short(uft_roundtrip_status_t s);

/**
 * @brief Alle Matrix-Einträge iterieren
 * @param count [out] Anzahl der Einträge
 * @return Read-only Zeiger auf das Array (niemals freigeben)
 */
const uft_roundtrip_entry_t *uft_roundtrip_entries(size_t *count);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ROUNDTRIP_H */
