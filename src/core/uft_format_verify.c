/**
 * @file uft_format_verify.c
 * @brief Generische verify_track-Implementierung für Format-Plugins
 *
 * Wird von allen Plugins verwendet die keine formatspezifische
 * Verify-Logik benötigen. Liest den Track und vergleicht Sektor-für-Sektor
 * mit der Referenz.
 *
 * Für Formate mit Weak-Bits, Timing oder Fuzzy-Bits muss eine eigene
 * verify_track-Funktion geschrieben werden (z.B. uft_atx_verify_track,
 * uft_stx_verify_track).
 *
 * MF-1616: "Sektor-für-Sektor" hiess hier "der s-te gegen den s-ten". Ein
 * Sektorabbild liefert seine Sektoren in ID-Folge, eine Aufnahme in
 * PLATTENfolge — und die beginnt, wo der Indexpuls lag. Gemessen an
 * CT-Raw -> ADF (sq1_amiga.ctr): 125 von 160 Spuren „wichen ab", und
 * genau die 35 Spuren, deren Plattenfolge zufaellig bei Sektor 0 beginnt,
 * bestanden; nach ID verglichen waren 1760 von 1760 Sektoren gleich.
 * Seit MF-1616 paaren alle drei Vergleiche ueber
 * uft_verify_find_partner().
 */
#include "uft/uft_format_common.h"
#include "uft/uft_format_plugin.h"
#include "uft/uft_track.h"   /* uft_track_release (MF-433) */

#include <stdlib.h>
#include <string.h>

long uft_verify_find_partner(const uft_track_t *actual, const uft_sector_t *r,
                             bool *used)
{
    if (!actual || !r || !used) return -1;
    for (size_t i = 0; i < actual->sector_count; i++) {
        if (!used[i] && actual->sectors[i].id.sector == r->id.sector) {
            used[i] = true;
            return (long)i;
        }
    }
    return -1;
}

/* Liest die Spur und legt das used[]-Feld fuer die Paarung an. Bei jedem
 * Fehler ist `actual` freigegeben und *used NULL. */
static uft_error_t verify_lesen(uft_disk_t *disk, int cyl, int head,
                                const uft_track_t *reference,
                                uft_track_t *actual, bool **used)
{
    *used = NULL;
    const uft_format_plugin_t *plugin = uft_disk_plugin(disk);   /* MF-445 */
    if (!plugin || !plugin->read_track) return UFT_ERROR_NOT_SUPPORTED;

    memset(actual, 0, sizeof(*actual));
    uft_error_t err = plugin->read_track(disk, cyl, head, actual);
    if (err != UFT_OK) {
        uft_track_release(actual);
        return err;
    }
    if (actual->sector_count != reference->sector_count) {
        uft_track_release(actual);
        return UFT_ERROR_VERIFY_FAILED;
    }
    *used = calloc(actual->sector_count ? actual->sector_count : 1, sizeof(bool));
    if (!*used) {
        uft_track_release(actual);
        return UFT_ERROR_NO_MEMORY;
    }
    return UFT_OK;
}

uft_error_t uft_generic_verify_track(uft_disk_t *disk, int cyl, int head,
                                      const uft_track_t *reference) {
    if (!disk || !reference) return UFT_ERROR_INVALID_STATE;

    uft_track_t actual;
    bool *used;
    uft_error_t err = verify_lesen(disk, cyl, head, reference, &actual, &used);
    if (err != UFT_OK) return err;

    uft_error_t ergebnis = UFT_OK;
    for (size_t s = 0; s < reference->sector_count && ergebnis == UFT_OK; s++) {
        const uft_sector_t *r = &reference->sectors[s];
        long p = uft_verify_find_partner(&actual, r, used);
        if (p < 0) { ergebnis = UFT_ERROR_VERIFY_FAILED; break; }
        const uft_sector_t *a = &actual.sectors[p];

        /* Datenlänge muss passen */
        size_t alen = a->data_len ? a->data_len : a->data_size;
        size_t rlen = r->data_len ? r->data_len : r->data_size;
        if (alen != rlen || !a->data || !r->data)
            ergebnis = UFT_ERROR_VERIFY_FAILED;
        /* Byte-genauer Vergleich */
        else if (memcmp(a->data, r->data, rlen) != 0)
            ergebnis = UFT_ERROR_VERIFY_FAILED;
    }

    free(used);
    uft_track_release(&actual);
    return ergebnis;
}

/* ============================================================================
 * Weak-bit-tolerant verify (ATX/STX/PRO)
 *
 * Diese Formate speichern Weak-Bit-Positionen die bei jedem Read unterschiedliche
 * Werte liefern können. Verify ignoriert Bytes mit weak_mask != 0.
 * Wenn der Sektor als weak markiert ist aber keine weak_mask hat, wird
 * der Sektor-Inhalt akzeptiert (kann nicht reproduzierbar verifiziert werden).
 * ============================================================================ */

uft_error_t uft_weak_bit_verify_track(uft_disk_t *disk, int cyl, int head,
                                       const uft_track_t *reference) {
    if (!disk || !reference) return UFT_ERROR_INVALID_STATE;

    uft_track_t actual;
    bool *used;
    uft_error_t err = verify_lesen(disk, cyl, head, reference, &actual, &used);
    if (err != UFT_OK) return err;

    uft_error_t ergebnis = UFT_OK;
    for (size_t s = 0; s < reference->sector_count && ergebnis == UFT_OK; s++) {
        const uft_sector_t *r = &reference->sectors[s];
        long p = uft_verify_find_partner(&actual, r, used);
        if (p < 0) { ergebnis = UFT_ERROR_VERIFY_FAILED; break; }
        const uft_sector_t *a = &actual.sectors[p];

        size_t alen = a->data_len ? a->data_len : a->data_size;
        size_t rlen = r->data_len ? r->data_len : r->data_size;
        if (alen != rlen || !a->data || !r->data) {
            ergebnis = UFT_ERROR_VERIFY_FAILED;
            break;
        }

        /* Wenn Weak-Sektor ohne Mask: überspringen (nicht reproduzierbar) */
        if (a->weak && !a->weak_mask) continue;

        /* Mit Weak-Mask: nur Non-Weak-Bytes vergleichen.
         *
         * weak_mask Semantik (per uft_sector_t.weak_mask doc-comment in
         * include/uft/uft_types.h:340 "Per-byte weak bit flags"):
         *   weak_mask[i] == 0  ⇒  Byte i ist solid, byte-vergleichen
         *   weak_mask[i] != 0  ⇒  Byte i ist weak, überspringen
         *
         * Producers folgen alle dem per-byte-Pattern:
         *   - src/formats/atx/uft_atx.c:297       memset(..., 0xFF, ...)
         *   - src/recovery/uft_multiread_pipeline.c:305  weak_mask[i] = 1
         *
         * Frühere per-bit-Lesart (`weak_mask[b/8] & (1 << (b%8))`) hatte
         * 7/8 der wirklich-weak-Bytes fälschlich byte-verglichen — bei
         * multiread-Output (per-byte 1) hätten reine zufällige Treffer
         * den Verify zum Scheitern gebracht. ATX (per-byte 0xFF) hat
         * by-accident funktioniert weil 0xFF alle Bits gesetzt hat.
         * Per docs/AI_COLLABORATION.md / structured-reviewer Finding #2. */
        if (a->weak_mask) {
            for (size_t b = 0; b < rlen; b++) {
                if (a->weak_mask[b] == 0 && a->data[b] != r->data[b]) {
                    ergebnis = UFT_ERROR_VERIFY_FAILED;
                    break;
                }
            }
        } else {
            /* Kein Weak-Sektor: normaler Vergleich */
            if (memcmp(a->data, r->data, rlen) != 0)
                ergebnis = UFT_ERROR_VERIFY_FAILED;
        }
    }

    free(used);
    uft_track_release(&actual);
    return ergebnis;
}

/* ============================================================================
 * Flux-level verify (WOZ/IPF/KFX/MFI/PRI/UDI)
 *
 * Für Flux/Bitstream-Formate vergleicht nur Track-Größe und Raw-Daten,
 * da Sektor-Extraktion nicht immer möglich ist. Wenn beide Tracks raw_data
 * haben, wird memcmp verwendet. Ansonsten wird Sektor-Vergleich versucht.
 * ============================================================================ */

uft_error_t uft_flux_verify_track(uft_disk_t *disk, int cyl, int head,
                                   const uft_track_t *reference) {
    if (!disk || !reference) return UFT_ERROR_INVALID_STATE;
    const uft_format_plugin_t *plugin = uft_disk_plugin(disk);   /* MF-445 */
    if (!plugin || !plugin->read_track) return UFT_ERROR_NOT_SUPPORTED;

    uft_track_t actual;
    memset(&actual, 0, sizeof(actual));

    uft_error_t err = plugin->read_track(disk, cyl, head, &actual);
    if (err != UFT_OK) { uft_track_release(&actual); return err; }

    /* Primär: raw_data vergleichen wenn vorhanden */
    if (actual.raw_data && reference->raw_data) {
        size_t aln = actual.raw_bits ? (actual.raw_bits + 7) / 8 : actual.raw_size;
        size_t rln = reference->raw_bits ? (reference->raw_bits + 7) / 8 : reference->raw_size;
        if (aln != rln || memcmp(actual.raw_data, reference->raw_data, rln) != 0) {
            uft_track_release(&actual);
            return UFT_ERROR_VERIFY_FAILED;
        }
        uft_track_release(&actual);
        return UFT_OK;
    }

    /* Fallback: Sektor-Vergleich mit Weak-Bit-Toleranz, gepaart nach ID */
    if (actual.sector_count != reference->sector_count) {
        uft_track_release(&actual);
        return UFT_ERROR_VERIFY_FAILED;
    }
    bool *used = calloc(actual.sector_count ? actual.sector_count : 1, sizeof(bool));
    if (!used) { uft_track_release(&actual); return UFT_ERROR_NO_MEMORY; }

    uft_error_t ergebnis = UFT_OK;
    for (size_t s = 0; s < reference->sector_count && ergebnis == UFT_OK; s++) {
        const uft_sector_t *r = &reference->sectors[s];
        long p = uft_verify_find_partner(&actual, r, used);
        if (p < 0) { ergebnis = UFT_ERROR_VERIFY_FAILED; break; }
        const uft_sector_t *a = &actual.sectors[p];
        size_t alen = a->data_len ? a->data_len : a->data_size;
        size_t rlen = r->data_len ? r->data_len : r->data_size;
        if (alen != rlen || !a->data || !r->data)
            ergebnis = UFT_ERROR_VERIFY_FAILED;
        else if (a->weak)
            continue;  /* Flux: weak-sectors gelten immer als OK */
        else if (memcmp(a->data, r->data, rlen) != 0)
            ergebnis = UFT_ERROR_VERIFY_FAILED;
    }
    free(used);
    uft_track_release(&actual);
    return ergebnis;
}
