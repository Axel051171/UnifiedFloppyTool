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
 *
 * MF-265 (V415 fix): if `uft_format_registry.h` was already included
 * in this TU, it provides `typedef enum uft_format_id_t`. The
 * UFT_FORMAT_ID_T_DEFINED guard is shared so the two definitions never
 * conflict in the same TU. Both are uint32-width on every supported
 * ABI; explicit casts between them are no-ops.
 */
#ifndef UFT_FORMAT_ID_T_DEFINED
#define UFT_FORMAT_ID_T_DEFINED
typedef uint32_t uft_format_id_t;
#endif

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
    UFT_RT_NO_ROUNDTRIP      = 4,  ///< Ziel trägt keine Sektoren — nur Vorwärtsprüfung
} uft_roundtrip_status_t;

/*
 * ── WARUM ES VIER WERTE SIND UND NICHT DREI (MF-1283) ───────────────────
 *
 * Die Matrix hatte EINE Frage: ist der Rundlauf byteidentisch? Fuer ein
 * Ziel, das WENIGER traegt als die Quelle, ist das die falsche Frage.
 * `TD0 -> IMG -> TD0` kann nie identisch sein — nicht wegen eines
 * Fehlers, sondern weil IMG weder CRC-Zustand noch geloeschte Marken noch
 * variable Sektorgroessen noch den Kommentar tragen KANN. Ein Tor, das
 * dort Byteidentitaet verlangt, sperrt den Wandler nicht als ungeprueft,
 * sondern als unmoeglich; und dann bleibt jeder verlustbehaftete Wandler
 * fuer immer gesperrt.
 *
 * Die drei Aussagen, die es wirklich gibt:
 *
 *   LOSSLESS          Ziel traegt alles      A->B->A byteidentisch
 *   LOSSY_DOCUMENTED  Ziel traegt weniger    A->B->A' gleich A auf der
 *                     („projiziert identisch")  Merkmalsmenge von B
 *   NO_ROUNDTRIP      Ziel traegt keine      nur Vorwaertspruefung gegen
 *                     Sektoren                 ein Orakel
 *
 * `IMPOSSIBLE` bleibt daneben etwas anderes: dort wird gar nicht
 * gewandelt, weil das Ziel die Quelle nicht darstellen kann, ohne Daten
 * zu erfinden. `NO_ROUNDTRIP` heisst: die Wandlung wird angeboten, nur
 * ist der Rundlauf kein Pruefmittel.
 *
 * Damit „Ziel traegt weniger" pruefbar wird und nicht erzaehlt bleibt,
 * traegt der Eintrag seit MF-1283 eine MASKE — siehe `lost_features`.
 */

/**
 * @brief Eintrag in der Round-Trip-Matrix
 */
typedef struct uft_roundtrip_entry {
    uft_format_id_t        from;
    uft_format_id_t        to;
    uft_roundtrip_status_t status;
    const char            *note;  ///< Menschenlesbar, z.B. "weak-bits lost"

    /* ── ANGEHAENGT, nie dazwischen (MF-1283) ──────────────────────────
     *
     * Welche Merkmale diese Wandlung VERLIERT, als Bitmaske ueber
     * `uft_d2_feature_t`. Bestehende Eintraege ohne diesen Wert
     * uebersetzen unveraendert und bekommen implizit 0.
     *
     * Warum eine Maske neben der Notiz: `note` ist Fliesstext, und kein
     * Tor kann einen Satz pruefen. „Verlust bekannt & in Kategorien
     * dokumentiert" war damit eine Zusage ohne Pruefmittel — dieselbe
     * Lage wie eine Merkmalstafel, die „Read: SUPPORTED" sagt, waehrend
     * der Leser jede Datei abweist (MF-961, MF-1015).
     *
     * Die Maske muss die aus `uft_format_traegt()` GERECHNETE Differenz
     * ABDECKEN. Zu wenig zu erklaeren faellt auf; mehr zu erklaeren ist
     * erlaubt und nur pessimistisch. Geprueft wird also ausschliesslich
     * das ZUVIEL-Versprechen.
     *
     * 0 heisst „nichts benannt" und ist fuer ein Paar mit gemessener
     * Differenz ein Fehler — nicht „kein Verlust". */
    uint32_t               lost_features;
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
 * @brief Die benannte Verlustmaske eines Paares (MF-1283)
 *
 * @return Bitmaske über `uft_d2_feature_t`; 0, wenn kein Eintrag existiert
 *         ODER der Eintrag nichts benennt. Die beiden Fälle sind hier
 *         absichtlich nicht unterscheidbar — wer sie trennen muss, fragt
 *         zuerst `uft_roundtrip_status()`.
 */
uint32_t uft_roundtrip_lost_features(uft_format_id_t from,
                                     uft_format_id_t to);

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
