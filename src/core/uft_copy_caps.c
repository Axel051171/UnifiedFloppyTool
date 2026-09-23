/*
 * MF-1311 — Faehigkeitsmaske aus einem Format-PAAR ableiten.
 *
 * Warum diese Datei getrennt von `uft_copy_plan.c` steht:
 * `uft_copy_plan.c` ist absichtlich abhaengigkeitsarm — es bindet nur
 * `uft_copy_plan.h`, `uft_types.h` und `<string.h>` ein, und
 * `tests/test_copy_plan.c` uebersetzt es genau so, allein. Wer die
 * Plugin-Registrierung dort hineinzoege, risse diesen Test mit.
 *
 * Referenz fuer die Abbildung ist das Manifest selbst:
 * `uft_format_caps_t` in `include/uft/uft_format_plugin.h:29-42`.
 *
 * DREI MESSUNGEN, die diese Datei praegen:
 *
 * 1. Das Manifest hat NEUN Flaggen — READ, WRITE, CREATE, FLUX, TIMING,
 *    WEAK_BITS, MULTI_REV, STREAMING, VERIFY. Drei davon tragen eine
 *    RICHTUNG (READ, WRITE, CREATE), und genau die hat
 *    `FormatTab::copyPlanCaps()` nie gelesen.
 *
 * 2. Es hat KEIN Bit fuer GCR, Bitstrom, Dateisystem oder CBM-BAM.
 *    Die vier gleichnamigen `uft_copy_caps_t`-Flaggen sind ueber diesen
 *    Weg nicht bloss ungesetzt, sondern UNERREICHBAR. Sie werden hier
 *    deshalb nie gesetzt — das ist keine Luecke, sondern die ehrliche
 *    Folge. Wer sie braucht, muss das Manifest erweitern.
 *
 * 3. Ueber alle Plugins gemessen: READ 87, VERIFY 83, WRITE 61, FLUX 7,
 *    TIMING 4, CREATE 3, WEAK_BITS 2, MULTI_REV 1. Und kein einziges
 *    Plugin sagt TIMING und WEAK_BITS ZUGLEICH zu (TIMING: g64, hfe,
 *    scp, stx; WEAK_BITS: pro, atx; Schnittmenge leer). Der harte
 *    Befund `schutz_nicht_tragbar` ist damit heute fuer jedes Paar
 *    wahr — eine Aussage ueber den Bestand, nicht ueber diese Funktion.
 *
 * Die Maske ist eine SCHNITTMENGE ueber ein PAAR: eine Faehigkeit gilt
 * nur, wenn die Quelle sie liefern UND das Ziel sie aufnehmen kann.
 * Genau so sprechen die Befunde von `uft_copy_plan_check()` sie an
 * ("verlangt eine Flussquelle UND ein Flussziel").
 */

#include "uft/core/uft_copy_plan.h"
#include "uft/uft_format_plugin.h"

#include <stddef.h>

bool uft_copy_caps_von_plugins(const uft_format_plugin_t *quelle,
                               const uft_format_plugin_t *ziel,
                               uint32_t *aus)
{
    if (aus) *aus = 0u;
    if (!quelle || !ziel || !aus) return false;

    const uint32_t q = (uint32_t)quelle->capabilities;
    const uint32_t z = (uint32_t)ziel->capabilities;

    /* MF-1315 — die Flagge allein genuegt nicht, der RUECKRUF muss da
     * sein.
     *
     * "Ein gesetztes Flag allein beweist noch keine funktionierende
     * End-to-End-Kopie" — und der Baum hat den Beleg dafuer selbst:
     * `src/formats/scp/uft_scp_plugin.c:606` setzt
     * `READ | FLUX | TIMING | MULTI_REV`, und zwei Zeilen tiefer stehen
     * `.create = NULL` und `.write_track = NULL`, beide mit dem
     * Kommentar "capability absent — honest NULL".
     *
     * SCP faellt hier schon ueber die fehlende WRITE-Flagge heraus. Die
     * Ruecksicht auf den Rueckruf ist trotzdem die richtige Stufe: eine
     * Flagge ist eine ZUSAGE, ein Funktionszeiger ist eine TAT. Wo
     * beides auseinanderfaellt, gilt die Tat — sonst verspricht das Tor
     * eine Kopie, die der Wandler nicht ausfuehren kann.
     *
     * Ohne Richtung keine Faehigkeit: eine Quelle, die nicht gelesen,
     * oder ein Ziel, das weder geschrieben noch angelegt werden kann,
     * traegt gar nichts. Das ist kein "unbekannt", sondern ein
     * gemessenes Nein — deshalb `true` mit leerer Maske, nicht `false`.
     * Der Unterschied ist der Punkt: `false` hiesse "nicht gemessen"
     * und wuerde das Tor zu NEEDS_MEASUREMENT fuehren. */
    const bool liest = (q & (uint32_t)UFT_FORMAT_CAP_READ) != 0u &&
                       (quelle->open != NULL) &&
                       (quelle->read_track != NULL);

    const bool schreibt = (z & ((uint32_t)UFT_FORMAT_CAP_WRITE |
                                (uint32_t)UFT_FORMAT_CAP_CREATE)) != 0u &&
                          (ziel->write_track != NULL || ziel->create != NULL);

    if (!liest || !schreibt) return true;

    uint32_t caps = 0u;

    /* Fluss: die Quelle muss ihn liefern, das Ziel ihn aufnehmen. */
    if ((q & (uint32_t)UFT_FORMAT_CAP_FLUX) &&
        (z & (uint32_t)UFT_FORMAT_CAP_FLUX))
        caps |= (uint32_t)UFT_CAP_FLUX_IO;

    /* Zeiten, schwache Bits, Mehrfachumdrehung: dieselbe Schnittmenge. */
    if ((q & (uint32_t)UFT_FORMAT_CAP_TIMING) &&
        (z & (uint32_t)UFT_FORMAT_CAP_TIMING))
        caps |= (uint32_t)UFT_CAP_TIMING;

    if ((q & (uint32_t)UFT_FORMAT_CAP_WEAK_BITS) &&
        (z & (uint32_t)UFT_FORMAT_CAP_WEAK_BITS))
        caps |= (uint32_t)UFT_CAP_WEAK_BITS;

    if ((q & (uint32_t)UFT_FORMAT_CAP_MULTI_REV) &&
        (z & (uint32_t)UFT_FORMAT_CAP_MULTI_REV))
        caps |= (uint32_t)UFT_CAP_MULTI_REV;

    /* UFT_CAP_BITSTREAM_IO, UFT_CAP_GCR, UFT_CAP_FILESYSTEM und
     * UFT_CAP_CBM_BAM bleiben ungesetzt — siehe Messung 2 im Kopf.
     * Sie hier zu raten waere eine erfundene Zusage. */

    *aus = caps;
    return true;
}
