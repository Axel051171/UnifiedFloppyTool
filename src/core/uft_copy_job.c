/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_copy_job.c
 * @brief Gibt das Ziel die Schutzabsicht her? (MF-1333, Stufe 5)
 *
 * Die Begruendung steht im Header. Hier nur, was die UMSETZUNG
 * entscheidet:
 *
 * 1. ES WIRD ABGELEITET, NICHT GETAFELT. Die Frage "traegt dieses
 *    Format Luecken" ist dieselbe wie "traegt es die Bitstrom- oder
 *    Flussebene", und das steht in `uft_format_traegt()`. Eine eigene
 *    Liste hier waere eine zweite Rechnung derselben Groesse — die
 *    Bauform, an der dieser Baum MF-1015 (drei Pruefsummen), MF-1026
 *    (drei Victor-Geometrien) und MF-1177 (drei Spurhaushalte) bezahlt
 *    hat.
 *
 * 2. "KEINE ZEILE" IST NICHT "TRAEGT NICHTS". Die Tafel sagt es selbst:
 *    ein Format ohne Zeile gilt als UNBEKANNT. Diese Funktion gibt das
 *    weiter, statt eine Absage zu erfinden — sonst waere jedes
 *    ungetafelte Zielformat gesperrt, und das waere eine Mauer aus
 *    falschen Urteilen.
 */

#include "uft/core/uft_copy_job.h"
#include "uft/core/uft_format_traegt.h"

/* Erst ab diesen Ebenen gibt es ueberhaupt Bytes ZWISCHEN den
 * Sektoren. Eine Sektorablage hat dort nichts, wo eine Luecke waere. */
#define EBENEN_MIT_LUECKEN ((1u << UFT_D2_LAYER_BITSTREAM) | \
                            (1u << UFT_D2_LAYER_FLUX))

uft_geos_ziel_urteil_t uft_geos_ziel_pruefen(uft_geos_aktion_t aktion,
                                             uft_format_id_t ziel,
                                             bool quelle_hat_rohspur)
{
    /* Wer nichts erhalten will, dem steht jedes Ziel offen. */
    if (aktion == UFT_GEOS_AKTION_AUS) return UFT_GEOS_ZIEL_OK;

    uft_format_traegt_t t = uft_format_traegt(ziel);
    if (!t.bekannt) return UFT_GEOS_ZIEL_UNBEKANNT;

    if ((t.layers & EBENEN_MIT_LUECKEN) == 0u)
        return UFT_GEOS_ZIEL_KEINE_LUECKEN;

    /* `EXAKT` heisst: die vorhandene Rohspur unveraendert uebernehmen.
     * Ohne Rohspur in der QUELLE gibt es nichts zu uebernehmen — und
     * eine aus Sektoren rekonstruierte Spur waere ein NACHBAU, der
     * unter dem Namen "exakt" liefe. Genau diese Verwechslung benennt
     * `uft_copy_job.h`: beides darf im Bericht nicht gleich heissen. */
    if (aktion == UFT_GEOS_AKTION_EXAKT && !quelle_hat_rohspur)
        return UFT_GEOS_ZIEL_QUELLE_OHNE_ROHSPUR;

    return UFT_GEOS_ZIEL_OK;
}

const char *uft_geos_ziel_urteil_text(uft_geos_ziel_urteil_t urteil)
{
    switch (urteil) {
        case UFT_GEOS_ZIEL_OK:
            return "Das Ziel kann den Bootschutz tragen.";
        case UFT_GEOS_ZIEL_KEINE_LUECKEN:
            return "Das Zielformat speichert nur Sektoren. Der "
                   "GEOS-Bootschutz liegt in den Luecken ZWISCHEN den "
                   "Sektoren und ist darin prinzipiell nicht "
                   "darstellbar — G64 oder eine Flussaufnahme waehlen.";
        case UFT_GEOS_ZIEL_UNBEKANNT:
            return "Fuer dieses Zielformat ist nicht gemessen, welche "
                   "Ebenen es traegt. Es wird nicht geraten.";
        case UFT_GEOS_ZIEL_QUELLE_OHNE_ROHSPUR:
            return "\"Exakt erhalten\" braucht eine Rohspur in der "
                   "Quelle. Diese Quelle liefert nur Sektoren; aus "
                   "ihnen laesst sich der Schutz nachbauen, aber nicht "
                   "uebernehmen.";
    }
    return "Unbekanntes Urteil.";
}

const char *uft_geos_aktion_name(uft_geos_aktion_t aktion)
{
    switch (aktion) {
        case UFT_GEOS_AKTION_AUS:     return "Ignorieren";
        case UFT_GEOS_AKTION_AUTO:    return "Automatisch";
        case UFT_GEOS_AKTION_EXAKT:   return "Exakt erhalten";
        case UFT_GEOS_AKTION_NACHBAU: return "Kompatibel rekonstruieren";
    }
    return "Unbekannt";
}

const char *uft_geos_gap_name(uft_geos_gap_variante_t v)
{
    switch (v) {
        case UFT_GEOS_GAP_UNBEKANNT: return "nicht gemessen";
        case UFT_GEOS_GAP_STANDARD:  return "Standardformat ($55)";
        case UFT_GEOS_GAP_ORIGINAL:  return "Original ($55 $55 $67)";
        case UFT_GEOS_GAP_NACHBAU:   return "GeoCopy-Nachbau ($67)";
    }
    return "nicht gemessen";
}
