/**
 * @file uft_disk_metadata.c
 * @brief Was das Plugin über ein geöffnetes Abbild sagt — oder schweigt
 *        (MF-662)
 *
 * Stufe 3 aus `docs/plans/VARIANTEN_UND_FAEHIGKEITEN.md`: beim Laden
 * soll UFT erkennen, **welche Variante** vorliegt, und es anzeigen.
 *
 * ── Warum eine eigene Funktion und nicht `plugin->read_metadata` direkt ──
 *
 * Weil die Oberfläche sonst drei Dinge selbst entscheiden müsste, die
 * genau einmal entschieden gehören:
 *
 *   1. **Kein Plugin, kein `read_metadata`, kein Wert** — drei
 *      verschiedene Wege, nichts zu wissen, mit derselben Folge. Wer
 *      sie einzeln behandelt, behandelt sie irgendwann verschieden.
 *   2. **Der Puffer bleibt in jedem Fall leer statt undefiniert.** Ein
 *      Aufrufer, der den Rückgabewert übersieht, zeigt dann nichts —
 *      nicht Speichermüll.
 *   3. **Es wird nie geraten.** Das ist die Regel des Plans, wörtlich:
 *      „ein Plugin, das keine Variante meldet, führt zu ‚nicht
 *      ermittelt' — nie zu einer geratenen. Ein erfundener
 *      Variantenname wäre exakt die Fabrikations-Klasse aus
 *      FMT-2/3/10/11/12, nur in der Oberfläche."
 *
 * ── Der Anlass, gemessen ─────────────────────────────────────────────────
 *
 * `src/diskanalyzerwindow.cpp` setzte `labelSide0Format` **fest** auf
 * `"ISO MFM"` — in beiden Zweigen, für jedes Format. Ein D64 ist GCR,
 * ein Amiga-ADF ist Amiga MFM, ein SD-ATR ist FM. Die Oberfläche
 * behauptete für alle dasselbe, und das Plugin wusste es oft besser:
 * HFE, G64, IMG und SCP beantworten `read_metadata("encoding")`.
 *
 * Dieselbe Fehlerklasse wie die HFE-Interface-Tabelle (MF-659) — eine
 * Aussage über das Medium, die niemand gemessen hat.
 *
 * ── Was hier NICHT passiert ──────────────────────────────────────────────
 *
 * Es wird nichts abgeleitet. Wenn ein Plugin `"version"` nicht kennt,
 * liefert diese Funktion `false` und der Aufrufer schreibt „nicht
 * ermittelt". Aus Dateigröße oder Endung eine Variante zu erschließen
 * wäre bequem und wäre geraten — genau das, was der Plan verbietet.
 */

#include "uft/uft_format_plugin.h"

#include <stddef.h>
#include <string.h>

bool uft_disk_metadata(const uft_disk_t *disk, const char *key,
                       char *out, size_t out_len)
{
    if (out && out_len > 0) out[0] = '\0';
    if (!disk || !key || !out || out_len == 0) return false;

    const uft_format_plugin_t *plugin = disk->plugin;
    if (!plugin || !plugin->read_metadata) return false;

    /* `read_metadata` nimmt einen nicht-konstanten Handle. Das Lesen von
     * Metadaten verändert das Abbild nicht; der Cast hält die Signatur
     * dieser Funktion ehrlich (`const`), statt den Zwang nach oben
     * durchzureichen. */
    uft_disk_t *nc = (uft_disk_t *)(uintptr_t)disk;
    if (plugin->read_metadata(nc, key, out, out_len) != UFT_OK) {
        out[0] = '\0';
        return false;
    }

    /* Ein leerer Wert ist keine Antwort. Manche Plugins geben UFT_OK
     * zurück und schreiben nichts — fuer den Aufrufer ist das dasselbe
     * wie „weiss ich nicht", und es soll sich auch so verhalten. */
    if (out[0] == '\0') return false;
    return true;
}

bool uft_disk_has_metadata(const uft_disk_t *disk)
{
    return disk && disk->plugin && disk->plugin->read_metadata;
}
