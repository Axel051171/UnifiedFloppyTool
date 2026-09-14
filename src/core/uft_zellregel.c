/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_zellregel.c
 * @brief Zell-Laufregeln messen (MF-1128)
 *
 * Referenzen, Herleitung der Schranken und die Abgrenzung gegen den
 * Fremdbestand stehen vollstaendig im Kopf von
 * `include/uft/core/uft_zellregel.h`. Kurz: die Schranken kommen vom
 * AUFRUFER, diese Datei misst nur; verglichen wird gegen
 * `dtc_find_run_violation` aus `src/dtc_components/src/bitbuffer.c`
 * (nur ausgefuehrt, nichts uebernommen), gemessen in
 * `tests/test_zellregel.c`.
 *
 * EIGENSTAENDIGE Umsetzung. Sie ist bewusst anders gebaut als die
 * Vorlage — diese hier laeuft in EINEM Durchgang und gibt ALLE
 * Messwerte zurueck (Einsen, Paare, beide Maxima, Zahl der Verstoesse
 * und die erste Stelle), waehrend die Vorlage entweder die erste
 * Fundstelle ODER die Trefferzahl liefert und die uebrigen Groessen
 * nicht kennt. Der Grund ist die Lehre aus MF-1000: eine Null ohne
 * Nenner ist keine Auskunft, und ein „erster Verstoss" ohne die
 * Verteilung dahinter laedt zum Fehlschluss ein (MF-1079: jede
 * Spurlaenge stimmte, und trotzdem lagen 587 unmoegliche Paare drin).
 */

#include "uft/core/uft_zellregel.h"

/** Eine Zelle lesen. `ordnung` entscheidet, wo im Byte sie liegt. */
static unsigned zelle(const uint8_t *daten, size_t i, uft_zellordnung_t o)
{
    const unsigned schieben = (o == UFT_ZELL_LSB_ZUERST)
                              ? (unsigned)(i & 7u)
                              : (unsigned)(7u - (i & 7u));
    return (unsigned)((daten[i >> 3] >> schieben) & 1u);
}

bool uft_zellregel_messen(const uint8_t *daten, size_t zellen,
                          uft_zellordnung_t ordnung,
                          unsigned max_null, unsigned max_eins,
                          uft_zellregel_t *aus)
{
    if (!aus)
        return false;

    aus->zellen = 0;
    aus->einsen = 0;
    aus->paare = 0;
    aus->max_null = 0;
    aus->max_eins = 0;
    aus->verstoesse = 0;
    aus->erster_bruch = SIZE_MAX;

    if (!daten)
        return false;
    if (zellen == 0)
        return true;          /* leer ist kein Fehler und kein Verstoss */

    unsigned vorher = 2u;     /* 2 = „noch keine Zelle gesehen"         */
    unsigned lauf = 0u;

    for (size_t i = 0; i < zellen; i++) {
        const unsigned v = zelle(daten, i, ordnung);

        lauf = (v == vorher) ? (lauf + 1u) : 1u;

        if (v) {
            aus->einsen++;
            if (vorher == 1u)
                aus->paare++;
            if (lauf > aus->max_eins)
                aus->max_eins = lauf;
            if (max_eins != UFT_ZELL_UNBEGRENZT && lauf > max_eins) {
                aus->verstoesse++;
                if (aus->erster_bruch == SIZE_MAX)
                    aus->erster_bruch = i;
            }
        } else {
            if (lauf > aus->max_null)
                aus->max_null = lauf;
            if (max_null != UFT_ZELL_UNBEGRENZT && lauf > max_null) {
                aus->verstoesse++;
                if (aus->erster_bruch == SIZE_MAX)
                    aus->erster_bruch = i;
            }
        }

        vorher = v;
    }

    aus->zellen = zellen;
    return true;
}

bool uft_zellregel_haelt(const uint8_t *daten, size_t zellen,
                         uft_zellordnung_t ordnung,
                         unsigned max_null, unsigned max_eins)
{
    uft_zellregel_t r;
    if (!uft_zellregel_messen(daten, zellen, ordnung, max_null, max_eins, &r))
        return false;
    return r.verstoesse == 0;
}
