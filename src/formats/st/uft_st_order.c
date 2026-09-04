/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_st_order.c
 * @brief Interleave und Spiralfaktor auf dem Atari ST (MF-858).
 *
 * Quelle: Juergen Stessun, „Wie schnell sind Disketten zu laden?",
 * ST-Computer 12/1989, abgedruckt in `LESETEST.HLP` (FCopy Pro 1.2).
 * Eigenstaendige Umsetzung; Begruendung und Grenzen im Header.
 */
#include "uft/formats/st/uft_st_order.h"

#include <string.h>

/* ─── Die Formel ────────────────────────────────────────────────────── */

double uft_st_speed_konstante(uint16_t sektorgroesse, double upm)
{
    if (sektorgroesse == 0 || upm <= 0.0) return 0.0;
    /* Sektorlaenge in kB mal Umdrehungen je Sekunde.
     * 512 B bei 300 U/min: 0,5 * 5 = 2,5 — der Wert, den die Quelle
     * einsetzt und dessen Herkunft sie ausdruecklich nennt. */
    return ((double)sektorgroesse / 1024.0) * (upm / 60.0);
}

double uft_st_speed(uint8_t spt, uint8_t il, int16_t spiral,
                    uint16_t sektorgroesse, double upm)
{
    if (spt == 0 || il == 0) return 0.0;
    const double k = uft_st_speed_konstante(sektorgroesse, upm);
    if (k <= 0.0) return 0.0;

    const double nenner = (double)spt * (double)il
                        + (spiral < 0 ? 0.0 : (double)spiral);
    if (nenner <= 0.0) return 0.0;
    return ((double)spt * (double)spt * k) / nenner;
}

double uft_st_speed_no_fastload(uint8_t spt, uint8_t il, int16_t spiral,
                                uint16_t sektorgroesse, double upm)
{
    const double v = uft_st_speed(spt, il, spiral, sektorgroesse, upm);
    if (v <= 0.0) return 0.0;

    /* Ab Spiralfaktor 2 kostet der Spurwechsel nichts — der
     * Verifikations-Header steht dann rechtzeitig vor dem naechsten zu
     * lesenden Sektor. Gemessen: 0,00 Umdrehungen Zusatz bei SPIR 2 und
     * 3, fuer 9 wie fuer 10 Sektoren. */
    if (spiral >= UFT_ST_SPIRAL_NO_PENALTY) return v;

    /* Darunter geht eine ganze Umdrehung verloren: „eine Runde Pause,
     * macht 200 ms". Gemessen 0,99 Umdrehungen — mit EINER Ausnahme,
     * die im Header als UNRESOLVED steht (9 SpT bei SPIR 1: 0,88). */
    if (upm <= 0.0) return 0.0;
    const double umdrehung_s = 60.0 / upm;

    const double kb = (double)spt * ((double)sektorgroesse / 1024.0);
    const double t = kb / v + umdrehung_s;
    if (t <= 0.0) return 0.0;
    return kb / t;
}

/* ─── Messung aus der physikalischen Reihenfolge ────────────────────── */

/** Position der Sektornummer @p nr in der Spur, oder -1. */
static int position_von(const uft_track_t *spur, uint8_t nr)
{
    if (!spur) return -1;
    for (size_t i = 0; i < spur->sector_count; i++)
        if (spur->sectors[i].id.sector == nr) return (int)i;
    return -1;
}

uint8_t uft_st_interleave_messen(const uft_track_t *spur)
{
    if (!spur || spur->sector_count < 2) return 0;

    /* Der Abstand von Sektor 1 zu Sektor 2 IN DER REIHENFOLGE, in der
     * sie auf der Spur liegen. Das ist der „Interleave-Faktor" der
     * Formatiertabelle — nicht der „Interleave" der Formel (dort:
     * Umdrehungen je Spur). Die Quelle warnt ausdruecklich vor der
     * Verwechslung: „Interleave 2 bei 11 Sektoren = Interleave-Faktor
     * 6". */
    const int p1 = position_von(spur, 1);
    const int p2 = position_von(spur, 2);
    if (p1 < 0 || p2 < 0) return 0;

    int d = p2 - p1;
    if (d < 0) d += (int)spur->sector_count;
    return (uint8_t)d;
}

int16_t uft_st_spiral_messen(const uft_track_t *vorspur,
                             const uft_track_t *spur)
{
    if (!vorspur || !spur) return -1;
    if (vorspur->sector_count == 0 || spur->sector_count == 0) return -1;

    /* Welche Sektornummer steht am Spuranfang, verglichen mit der
     * Vorspur. */
    int d = (int)spur->sectors[0].id.sector
          - (int)vorspur->sectors[0].id.sector;
    if (d < 0) d += (int)spur->sector_count;
    return (int16_t)d;
}

void uft_st_order_messen(const uft_track_t *vorspur, const uft_track_t *spur,
                         uft_st_order_t *aus)
{
    if (!aus) return;
    memset(aus, 0, sizeof *aus);
    aus->spiral = -1;

    if (!spur || spur->sector_count == 0) return;

    aus->spt        = (uint8_t)spur->sector_count;
    aus->interleave = uft_st_interleave_messen(spur);
    aus->spiral     = uft_st_spiral_messen(vorspur, spur);

    /* `gemessen` sagt NICHT „die Zahlen sind plausibel", sondern „sie
     * stammen aus einer Reihenfolge, die dieser Spurdatensatz wirklich
     * traegt". Ob diese Reihenfolge physikalisch ist, entscheidet der
     * Leser, der die Spur gefuellt hat: aus `.ST`/`.MSA` ist sie
     * logisch normalisiert und damit ohne Aussage. */
    aus->gemessen = true;
}

/* ─── Herkunft ──────────────────────────────────────────────────────── */

uft_tos_herkunft_t uft_st_tos_herkunft(int16_t spiral_s0, int16_t spiral_s1,
                                       uint8_t spt)
{
    /* „Maximal 10 Sektoren passen daher auf ‚Desktop-formatierte'
     * Disketten." TOS unterstuetzt Interleaving, steigt aber bei 11
     * Sektoren aus und verschenkt den Platz in die Zwischenraeume. */
    if (spt > 10) return UFT_TOS_NICHT_DESKTOP;
    if (spiral_s0 < 0) return UFT_TOS_UNBEKANNT;

    if (spiral_s0 == 3 && spiral_s1 == 2) return UFT_TOS_104_INOFF;
    if (spiral_s0 == 0 && spiral_s1 <= 0)  return UFT_TOS_100;
    if (spiral_s0 == 2 && (spiral_s1 == 2 || spiral_s1 < 0))
        return UFT_TOS_102_PLUS;

    return UFT_TOS_NICHT_DESKTOP;
}

const char *uft_st_tos_herkunft_text(uft_tos_herkunft_t h)
{
    switch (h) {
    case UFT_TOS_100:
        return "Spiralfaktor 0 — entspricht TOS 1.0 (spaetere Fassungen "
               "formatieren mit Spiralfaktor 2)";
    case UFT_TOS_102_PLUS:
        return "Spiralfaktor 2 — entspricht TOS 1.02 (Blitter) oder hoeher";
    case UFT_TOS_104_INOFF:
        return "Spiralfaktor 3 auf Seite 0, 2 auf Seite 1 — entspricht "
               "einer inoffiziellen TOS-1.04-Fassung";
    case UFT_TOS_NICHT_DESKTOP:
        return "passt zu keiner TOS-Vorgabe — vermutlich mit einem "
               "Fremdformatierer erstellt";
    case UFT_TOS_UNBEKANNT:
    default:
        return "Spiralfaktor nicht bestimmbar";
    }
}
