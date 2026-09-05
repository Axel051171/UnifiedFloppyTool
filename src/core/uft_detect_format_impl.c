/**
 * @file uft_detect_format_impl.c
 * @brief Correct-signature impl of uft_detect_format (detect/ variant).
 *
 * Replaces the ABI-broken stub in uft_core_stubs.c which had
 * `int uft_detect_format(const char *path)` — a 1-arg variant that
 * accepted a pointer expecting filename but was called with a data
 * buffer + length + result-struct (3 args per the canonical
 * declaration). Callers got their extra args dropped silently.
 *
 * Canonical declaration: include/uft/detect/uft_format_detect.h:173
 *   int uft_detect_format(const uint8_t *data, size_t len,
 *                          uft_detect_result_t *result);
 *
 * This file includes ONLY that header so we see the correct
 * uft_detect_result_t layout (one of 3 structs sharing this name —
 * a separate ABI-bomb tracked as ABI-003 in the detector report).
 */

#include "uft/detect/uft_format_detect.h"
#include "uft/uft_format_plugin.h"   /* for uft_probe_buffer_format */

#include <stddef.h>
#include <string.h>

/* Forward-decl the plugin-probe helper (defined in uft_format_plugin_*.c). */
extern const uft_format_plugin_t *uft_probe_buffer_format(const uint8_t *data,
                                                            size_t size,
                                                            size_t file_size);

int uft_detect_format(const uint8_t *data, size_t len,
                       uft_detect_result_t *result)
{
    if (!data || !result || len == 0) return -1;
    memset(result, 0, sizeof(*result));

    const uft_format_plugin_t *plugin = uft_probe_buffer_format(data, len, len);
    if (!plugin) {
        result->format = UFT_FORMAT_UNKNOWN;
        result->confidence = 0;
        return -1;
    }

    /* MF-881: hier stand, die beiden `format`-Aufzaehlungen "share numeric
     * values via the UFT_FORMAT_ENUM_DEFINED guard, so the int cast is
     * safe".
     *
     * Das ist die Umkehrung dessen, was der Guard tut. Er sorgt nicht
     * dafuer, dass zwei Aufzaehlungen ihre Werte TEILEN — er sorgt dafuer,
     * dass je Uebersetzungseinheit nur EINE ueberhaupt existiert. Welche,
     * entscheidet die Include-Reihenfolge.
     *
     * Am Praeprozessor gemessen (`gcc -E`, derselbe Bau):
     *
     *     UFT_FORMAT_D64 = 1   in DIESER Datei
     *     UFT_FORMAT_D64 = 4   in src/core/uft_format_plugin.c,
     *                             src/formats/d64/uft_d64_plugin.c und
     *                             vier weiteren
     *
     * Diese Datei bindet `detect/uft_format_detect.h` zuerst ein (Zeile 20)
     * und sieht deshalb dessen Fassung. `plugin->format` traegt aber einen
     * Wert aus der anderen. Die Zuweisung unten schreibt also eine Zahl,
     * die im Zahlenraum des Lesers etwas anderes bedeutet.
     *
     * FOLGE, gemessen: `src/analysis/uft_format_suggest.c:333` liest
     * `det.format` und vergleicht es in `is_flux_format()` (`:96`) und
     * `native_sector_format()` (`:109`) gegen seine EIGENEN Konstanten —
     * ebenfalls die detect-Fassung. Ein D64 kommt als 4 an und wird gegen
     * die 1 gehalten. Erreichbar von der Oberflaeche ueber
     * `src/gui/uft_recovery_dialog.cpp:509` und
     * `src/gui/uft_smart_export_dialog.cpp:107`.
     *
     * NICHT hier behoben, und zwar bewusst: die Aufloesung ist das
     * Zusammenfuehren der drei `uft_format_t` auf eine Definition, und das
     * ist kein Einzeiler — 20 Namen gibt es nur in
     * `detect/uft_format_detect.h`, 10 nur in `uft_format_parsers.h`, und
     * `uft_format_suggest.c` benutzt zwei davon. Die fehlenden muessen an
     * `uft_types.h` ANGEHAENGT werden, damit bestehende Werte sich nicht
     * verschieben. Gefuehrt als P3-155; `scripts/audit_guard_kollision.py`
     * (Tor 56) haelt die Zahl fest, damit kein dritter Fall dazukommt.
     *
     * Bis dahin steht hier, was gilt: diese Zuweisung ist NICHT sicher. */
    result->format       = (uft_format_t)plugin->format;
    result->format_name  = plugin->name;
    result->extensions   = plugin->extensions;
    result->confidence   = 90;  /* plugin probe matched — high confidence */
    result->probe_score  = 90;
    snprintf(result->reason, sizeof(result->reason),
             "Plugin '%s' accepted the buffer.", plugin->name ? plugin->name : "?");
    return 0;  /* UFT_OK */
}
