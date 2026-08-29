/**
 * @file uft_plugin_capability.c
 * @brief Das Fähigkeits-Manifest abfragbar machen (MF-660)
 *
 * ── Warum es diese Datei gibt ────────────────────────────────────────────
 *
 * 88 Plugins deklarieren zusammen rund 590 Fähigkeits-Aussagen
 * (`uft_plugin_feature_t`: `Read`, `Write`, `Create`, `Flux`, `Timing`,
 * `Weak Bits`, `MultiRev`, je `UNSUPPORTED / PARTIAL / SUPPORTED`).
 *
 * Bis MF-658 las sie **niemand** — gemessen: 88 Deklarationen, 0 Leser.
 * MF-658 gab ihnen einen Prüfer; diese Datei gibt ihnen einen
 * **Benutzer**. Sie ist Stufe 2 aus
 * `docs/plans/VARIANTEN_UND_FAEHIGKEITEN.md`.
 *
 * ── Der Befund, der den Zuschnitt bestimmt ───────────────────────────────
 *
 * Die Oberfläche führt eine **zweite** Fähigkeits-Tabelle: `m_formatInfo`
 * in `src/formattab.cpp`, 25 handgeschriebene Einträge mit den Feldern
 * `supportsHalfTracks`, `supportsWeakBits`, `supportsFlux`,
 * `supportsGCR`, `supportsMFM`.
 *
 * Gemessen widersprechen sich die beiden an **fünf** Stellen — EDSK,
 * G64, NIB, SCP und TD0 versprechen in der Oberfläche Weak-Bit-Fähigkeit,
 * die das Plugin als `UNSUPPORTED` führt. Der Benutzer sieht dort heute
 * schon eine Zusage, die der Code nicht hält.
 *
 * **Die beiden Tabellen beantworten verschiedene Fragen**, und das ist
 * der Grund, warum eine davon gewinnen muss:
 *
 *   `m_formatInfo`     — kann das FORMAT das tragen?
 *   Plugin-Manifest    — tut UNSER CODE das?
 *
 * Für die Oberfläche zählt das Zweite. Ein Bedienelement, das ein
 * Format-theoretisches Können anbietet, welches unser Leser nicht
 * einlöst, ist genau der tote Knopf, den Stufe 5 beseitigt. Das
 * Manifest ist deshalb die Quelle, und `m_formatInfo` verliert seine
 * Fähigkeits-Felder — seine `versions`-Liste und die Standard-Geometrie
 * bleiben nützlich (Stufe 3/4).
 */

#include "uft/uft_format_plugin.h"

#include <string.h>

/* ==========================================================================
 * Abfrage
 * ========================================================================== */

uft_feature_support_t uft_plugin_feature_state(const uft_format_plugin_t *plugin,
                                                const char *feature)
{
    if (!plugin || !feature || !plugin->features) return UFT_FEATURE_UNSUPPORTED;
    for (size_t i = 0; i < plugin->feature_count; i++) {
        const char *n = plugin->features[i].name;
        if (n && strcmp(n, feature) == 0) return plugin->features[i].status;
    }
    /* Nicht deklariert heisst nicht unterstuetzt. Das ist die sichere
     * Richtung: ein Plugin, das schweigt, bekommt kein Bedienelement
     * angeboten — statt eines, das nichts tut. */
    return UFT_FEATURE_UNSUPPORTED;
}

const char *uft_plugin_feature_note(const uft_format_plugin_t *plugin,
                                     const char *feature)
{
    if (!plugin || !feature || !plugin->features) return NULL;
    for (size_t i = 0; i < plugin->feature_count; i++) {
        const char *n = plugin->features[i].name;
        if (n && strcmp(n, feature) == 0) return plugin->features[i].note;
    }
    return NULL;
}

bool uft_plugin_declares_feature(const uft_format_plugin_t *plugin,
                                  const char *feature)
{
    if (!plugin || !feature || !plugin->features) return false;
    for (size_t i = 0; i < plugin->feature_count; i++) {
        const char *n = plugin->features[i].name;
        if (n && strcmp(n, feature) == 0) return true;
    }
    return false;
}

/* ==========================================================================
 * Sichtbarkeit
 * ========================================================================== */

uft_control_visibility_t uft_plugin_control_visibility(
        const uft_format_plugin_t *plugin, const char *feature)
{
    /* Ein Plugin, das die Fähigkeit gar nicht erwähnt, ist NICHT
     * dasselbe wie eines, das sie ausdrücklich verneint — aber für die
     * Oberfläche ist die Folge dieselbe: nichts anbieten. Der
     * Unterschied bleibt über `uft_plugin_declares_feature()` abfragbar,
     * damit ein Bericht ihn benennen kann. */
    switch (uft_plugin_feature_state(plugin, feature)) {
    case UFT_FEATURE_SUPPORTED:
        return UFT_CONTROL_SHOW;
    case UFT_FEATURE_PARTIAL:
        /* Sichtbar UND bedienbar, mit Hinweis. Eine eingeschränkte
         * Fähigkeit zu verstecken nimmt dem Benutzer eine Information,
         * die er hat — und der Header verlangt für PARTIAL ohnehin eine
         * Begründung (`uft_format_plugin.h:90`), die als Hinweis taugt. */
        return UFT_CONTROL_SHOW_LIMITED;
    case UFT_FEATURE_UNSUPPORTED:
    default:
        return UFT_CONTROL_HIDE;
    }
}
