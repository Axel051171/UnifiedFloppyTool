#ifndef UFT_FORMAT_CONVERT_H
#define UFT_FORMAT_CONVERT_H

/**
 * @file uft_format_convert.h
 * @brief Format Conversion Matrix & Converter
 * 
 * FORMAT-KLASSIFIKATION:
 * ══════════════════════════════════════════════════════════════════════
 * 
 * FLUX (Raw Timing):
 *   SCP, Kryoflux, A2R
 *   → Höchste Präzision, alle Daten erhalten
 *   → Kann zu allem konvertiert werden (mit Dekodierung)
 * 
 * BITSTREAM (Encoded):
 *   HFE, G64, WOZ, NIB
 *   → Bit-genaue Darstellung
 *   → Kann zu Flux (synthetisch) oder Sector konvertiert werden
 * 
 * CONTAINER (Metadata + Data):
 *   IPF, STX
 *   → Format mit Timing-Hints und Kopierschutz-Info
 *   → Meist read-only, spezielle Dekoder nötig
 * 
 * SECTOR (Data only):
 *   D64, ADF, IMG, DSK, IMD
 *   → Nur Nutzdaten, kein Timing
 *   → Kann zu Bitstream/Flux nur synthetisch konvertiert werden
 * 
 * ARCHIVE (Compressed):
 *   TD0, NBZ
 *   → Komprimierte Container
 *   → Erst dekomprimieren, dann wie Sector/Bitstream
 * 
 * KONVERTER-PFADE:
 * ══════════════════════════════════════════════════════════════════════
 * 
 * VERLUSTFREI:
 *   SCP → HFE (Flux → Bitstream)
 *   G64 → D64 (wenn keine Kopierschutz-Features)
 *   ADF → IMG (Layout-Anpassung)
 * 
 * VERLUSTBEHAFTET:
 *   SCP → D64 (Flux → Sector): Timing-Info verloren
 *   G64 → D64 (Bitstream → Sector): Weak bits verloren
 *   IPF → ADF: Kopierschutz-Features verloren
 * 
 * SYNTHETISCH (Information hinzugefügt):
 *   D64 → G64: Timing wird geschätzt
 *   ADF → SCP: Flux wird synthetisiert
 *   IMG → HFE: Bit-Encoding hinzugefügt
 */


#include "uft_types.h"
#include "uft_error.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Format Classification
// ============================================================================

/* Guard against redefinition — also defined in uft_format_probe.h */
#ifndef UFT_FORMAT_CLASS_DEFINED
#define UFT_FORMAT_CLASS_DEFINED
typedef enum uft_format_class {
    UFT_FCLASS_FLUX,        // Raw flux timing
    UFT_FCLASS_BITSTREAM,   // Encoded bitstream
    UFT_FCLASS_CONTAINER,   // Container with metadata
    UFT_FCLASS_SECTOR,      // Sector data only
    UFT_FCLASS_ARCHIVE,     // Compressed archive
} uft_format_class_t;
#endif

// ============================================================================
// Conversion Quality
// ============================================================================

/* MF-1288: die Werte tragen jetzt ausdrueckliche Zahlen, und ein
 * fuenfter ist ANGEHAENGT.
 *
 * Die Zahlen, weil eine Aufzaehlung ohne sie eine ABI-Bombe ist: wer
 * einen Wert in die Mitte einfuegt, verschiebt alle folgenden, und kein
 * Uebersetzer warnt. Angehaengt wird deshalb, nicht eingefuegt — dieselbe
 * Regel wie bei `UFT_RT_NO_ROUNDTRIP` (MF-1283).
 *
 * `UNVERIFIED` fuellt eine Luecke, die der Tafel wehgetan hat: bisher
 * MUSSTE jeder Pfad eine Behauptung aufstellen. „Nicht gemessen" war
 * nicht sagbar, also stand dort im Zweifel LOSSLESS — gemessen bei
 * ACHT von dreizehn LOSSLESS-Pfaden ohne jeden Matrixeintrag. */
typedef enum uft_conv_quality {
    UFT_CONV_LOSSLESS   = 0,  // No data loss — braucht einen Matrixeintrag
    UFT_CONV_LOSSY      = 1,  // Some data/timing lost
    UFT_CONV_SYNTHETIC  = 2,  // Data synthesized/estimated
    UFT_CONV_IMPOSSIBLE = 3,  // Cannot convert
    UFT_CONV_UNVERIFIED = 4,  // Nicht gemessen — KEINE Aussage ueber Verlust
} uft_conv_quality_t;

// ============================================================================
// Conversion Path Info
// ============================================================================

typedef struct uft_conversion_path {
    uft_format_t        source;
    uft_format_t        target;
    uft_conv_quality_t  quality;
    bool                requires_decode;    // Needs format-specific decoder
    bool                preserves_timing;
    bool                preserves_errors;
    bool                preserves_weak;
    const char*         warning;            // NULL if none
    const char*         description;
} uft_conversion_path_t;

// ============================================================================
// Conversion Options
// ============================================================================

typedef struct uft_convert_options_ext {
    // General
    bool                verify_after;
    bool                preserve_errors;       // Carry error-flags forward in output
    bool                preserve_weak_bits;

    // Flux synthesis (Sector → Flux)
    double              synthetic_cell_time_us;
    double              synthetic_jitter_percent;
    int                 synthetic_revolutions;

    // Sector extraction (Flux → Sector)
    int                 decode_retries;
    bool                use_multiple_revs;
    bool                interpolate_errors;

    // Progress
    void (*progress_cb)(int percent, const char* stage, void* user);
    void* progress_user;
    volatile bool* cancel;

    /* UFT-A05 (appended for ABI safety): explicit consent for
     * LOSSY_DOCUMENTED paths. See uft_convert_options_t in uft_types.h
     * for rationale. */
    bool                accept_data_loss;

    /* MF-480 (appended for ABI safety): percent nudge for the decoder's cell
     * time, 50…200; 0 or 100 means unchanged. Mirrors
     * uft_convert_options_t::decode_cell_adjust_pct — see the rationale
     * there. */
    double              decode_cell_adjust_pct;

    /* MF-673: Strenge der Umdrehungs-Abstimmung in Prozent,
     * 50…100; 0 = unveraendert. Derselbe Weg wie die Zeile
     * darueber; die Begruendung steht in uft_types.h. */
    double              decode_vote_confidence_pct;

    /* MF-482 (appended for ABI safety): explicit read range. 0 in a field
     * means "derive it from the source". Mirrors
     * uft_convert_options_t::target_geometry — see the rationale there. */
    uft_geometry_t      target_geometry;

    /* MF-484 (appended for ABI safety): decode the flux time axis reversed
     * — the back side of a flippy disk. Mirrors
     * uft_convert_options_t::reverse_decode. */
    bool                reverse_decode;
} uft_convert_options_ext_t;

// ============================================================================
// Conversion Result
// ============================================================================

typedef struct uft_convert_result {
    bool                success;
    uft_error_t         error;
    
    // Statistics
    int                 tracks_converted;
    int                 tracks_failed;
    int                 sectors_converted;
    int                 sectors_failed;
    int                 bytes_written;
    
    // Warnings
    int                 warning_count;
    char                warnings[8][256];
} uft_convert_result_t;

// ============================================================================
// API
// ============================================================================

/**
 * @brief Get conversion path info
 */
const uft_conversion_path_t* uft_convert_get_path(uft_format_t src, 
                                                    uft_format_t dst);

/**
 * @brief Check if conversion is possible
 */
bool uft_convert_can(uft_format_t src, uft_format_t dst,
                      uft_conv_quality_t* quality,
                      const char** warning);

/**
 * @brief List all possible target formats for source
 */
int uft_convert_list_targets(uft_format_t src,
                              const uft_conversion_path_t** paths,
                              int max);

/**
 * @brief Convert file
 */

struct uft_format_plugin;   /* MF-1307: nur als Zeiger gebraucht — dieser
                             * Kopf kennt `uft_format_plugin_t` nicht, und
                             * ihn einzubinden waere eine Abhaengigkeit fuer
                             * einen Parameter. */

/* ── Nachpruefung nach der Wandlung (E-7, MF-1307) ──────────────
 *
 * Sie stand zuerst in `uft_format_plugin.h` neben
 * `uft_generic_verify_track()`. Das war falsch einsortiert, und es
 * hatte Folgen im BINDER: `src/core/uft_format_verify.c` wird von
 * **47** Testzielen gebunden, die nur den Spurvergleich brauchen —
 * und jedes davon zog ueber die neue Funktion die ganze
 * Disk-Oeffnungs-Maschinerie nach (`undefined reference to
 * uft_meta_free`, gemessen an elf Zielen). Eine Funktion gehoert in
 * die Schicht, die sie braucht. */
/**
 * @brief Bilanz einer Wandlungs-Nachpruefung (MF-1307).
 *
 * ZAHLEN statt eines Urteils: der Aufrufer soll sagen koennen, WAS
 * nicht stimmte. `uft_convert_verify_after()` gibt zwar einen
 * Fehlerkode zurueck, aber ein Kode allein laesst den Bediener raten.
 */
typedef struct {
    bool     quelle_offen;        /**< liess sich die Quelle oeffnen */
    bool     ziel_offen;          /**< liess sich das GESCHRIEBENE oeffnen */
    unsigned quell_zylinder, quell_koepfe;
    unsigned ziel_zylinder,  ziel_koepfe;
    size_t   spuren_geprueft;
    size_t   spuren_abweichend;
    size_t   spuren_unlesbar;     /**< in der QUELLE nicht lesbar */
    /** Spuren, die die Quelle OHNE einen einzigen Sektor liefert.
     *
     * Sie werden NICHT verglichen, und das ist keine Nachsicht: eine
     * Spur ohne Sektoren traegt nichts, dessen Verlust man feststellen
     * koennte. Sie zaehlen dennoch eigens, weil eine leere Spalte und
     * eine bestandene Spalte zwei verschiedene Aussagen sind (Regel D6).
     *
     * Gemessen MF-1307 an `hxcfe_pc160.imd`: die Quelle sagt **42**
     * Zylinder an und traegt auf **40** Sektoren (320 = 40 x 8). Ohne
     * diese Unterscheidung faellt jede byteweise richtige Wandlung, deren
     * QUELLE ihre Zylinderzahl zu hoch angibt. */
    size_t   spuren_leer;
    size_t   sektoren_geprueft;
} uft_verify_bilanz_t;

/**
 * @brief Prueft NACH einer Wandlung, ob das Geschriebene die Quelle traegt.
 *
 * Oeffnet @p ziel_pfad erneut und haelt ihn Spur fuer Spur gegen
 * @p quell_pfad. Der Vergleich ist `uft_generic_verify_track()` — Sektorzahl
 * und jedes Datenbyte.
 *
 * **Vergleicht DATEN, nicht Merkmale.** Bei einer verlustbehafteten
 * Wandlung ist es richtig, dass Flaggen und Metadaten fehlen; dafuer gibt
 * es `lost_features` in der Rundlauf-Matrix. Weichen die DATEN ab, ist das
 * immer ein Befund.
 *
 * @return `UFT_OK` nur, wenn beide Dateien offen sind, mindestens eine
 *         Spur geprueft wurde und KEINE abweicht. Sonst ein Fehlerkode;
 *         die Zahlen stehen in @p bilanz, auch im Fehlerfall.
 *
 * **Die angesagten Geometrien entscheiden NICHT.** Gelaufen wird ueber
 * die Spuren der Quelle; kann das Ziel eine davon nicht liefern, faellt
 * sie als `spuren_abweichend` auf. Eine Kuerzung wird damit weiterhin
 * gefangen — am Inhalt statt an einer Zahl, die zwei Leser gemessen
 * verschieden bilden (MF-1307: 42 gegen 40 an derselben richtigen Datei).
 */
/**
 * @param ziel_plugin Das Plugin, mit dem @p ziel_pfad geschrieben wurde,
 *        oder `NULL`. **Mit `NULL` wird die Sonde gefragt, und das ist
 *        bei einem KOPFLOSEN Ziel eine Rate-Aufgabe** — gemessen
 *        MF-1307 an `IMD -> IMG`: die byteweise richtige 163 840-Byte-
 *        Ausgabe teilt ihre Groesse mit TR-DOS (40x1x16x256), und die
 *        Nachpruefung meldete einen Verlust, den es nicht gab. Ein
 *        FALSCHER Alarm ist in diesem Baum so teuer wie ein stiller
 *        Verlust. Wer das Zielformat kennt, nennt es.
 */
uft_error_t uft_convert_verify_after(const char *quell_pfad,
                                     const char *ziel_pfad,
                                     const struct uft_format_plugin *ziel_plugin,
                                     uft_verify_bilanz_t *bilanz);

uft_error_t uft_convert_file(const char* src_path,
                              const char* dst_path,
                              uft_format_t dst_format,
                              const uft_convert_options_t* options,
                              uft_convert_result_t* result);

/**
 * @brief Convert in-memory
 */
uft_error_t uft_convert_memory(const uint8_t* src_data, size_t src_size,
                                uft_format_t src_format,
                                uint8_t** dst_data, size_t* dst_size,
                                uft_format_t dst_format,
                                const uft_convert_options_ext_t* options,
                                uft_convert_result_t* result);

/**
 * @brief Get default options
 */
uft_convert_options_t uft_convert_default_options(void);

/**
 * @brief Get format class
 */
uft_format_class_t uft_format_get_class(uft_format_t format);

/**
 * @brief Get format name
 */
#ifndef UFT_FORMAT_GET_NAME_DECLARED
#define UFT_FORMAT_GET_NAME_DECLARED
const char* uft_format_get_name(uft_format_t format);

/**
 * @brief Format zu einem Kuerzel — die Rueckrichtung zu
 *        @ref uft_format_get_name (MF-568).
 *
 * Gross-/Kleinschreibung egal; ein fuehrender Punkt wird ueberlesen, damit
 * sowohl `"D64"` als auch `".d64"` gehen.
 *
 * ── Wofuer das gebraucht wird ────────────────────────────────────────────
 *
 * Die Oberflaeche fuehrte bis MF-568 eine EIGENE, von Hand gepflegte
 * Wandlungsliste (`ToolsTab::m_conversionMap`) — die vierte Liste dessen,
 * was gewandelt werden kann, nach Wandlungstabelle, Rundlauf-Matrix und
 * Verteiler. Sie bot Paare an, die die Maschine nicht hat (`SCP→ATR`,
 * `SCP→WOZ`, `TRD→SCL`), und sie war die einzige, die der Benutzer je sah.
 *
 * Damit die Oberflaeche stattdessen `uft_convert_list_targets()` benutzen
 * kann, braucht sie den Weg vom angezeigten Kuerzel zurueck zum Format.
 *
 * @return das Format, oder @ref UFT_FORMAT_UNKNOWN wenn das Kuerzel in
 *         `g_format_info[]` nicht vorkommt. **Kein Raten** — ein
 *         unbekanntes Kuerzel ist unbekannt, nicht „vermutlich IMG".
 */
uft_format_t uft_format_from_name(const char* name);
#endif /* UFT_FORMAT_GET_NAME_DECLARED */

#ifdef __cplusplus
}
#endif

#endif /* UFT_FORMAT_CONVERT_H */
