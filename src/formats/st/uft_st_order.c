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
#include "uft/core/uft_disk2.h"

#include <stdio.h>
#include <stdlib.h>
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

    /* The positions below are array indices, and they are physical slots
     * only if the track carries every number 1..n exactly once. A gap
     * (1..8,10) or a duplicate means a sector is missing or phantom; then
     * sector_count is not the sectors per track, and interleave/spiral
     * would be computed over the wrong ring. Report nothing rather than a
     * smaller geometry -- CERTIFY 1.0 let one failed read of sector 10
     * decide 9 vs 10 (H-18, MF-1339). A missing LAST sector leaves no
     * trace on the track and cannot be detected here; spt is what this
     * track carries, not the geometry of the disk. */
    if (spur->sector_count > UINT8_MAX) return;
    bool gesehen[UINT8_MAX + 1] = { false };
    for (size_t i = 0; i < spur->sector_count; i++) {
        const uint8_t nr = spur->sectors[i].id.sector;
        if (nr == 0 || nr > spur->sector_count || gesehen[nr]) return;
        gesehen[nr] = true;
    }

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

/* ─── Tuer in den Traegerbericht ─────────────────────────────────────── */

/* Reine Atari-Behaelter. Begruendung und Grenzen im Header. */
static bool atari_behaelter(const uft_disk2_t *d)
{
    const char *p = uft_d2_meta(d, "Plugin");
    return p && (strcmp(p, "ST") == 0 || strcmp(p, "MSA") == 0
                 || strcmp(p, "STX") == 0);
}

/**
 * Die physische Sicht einer Modellspur als `uft_track_t`, damit die
 * Messung oben sie lesen kann, ohne dass ihre Rechnung ein zweites Mal
 * geschrieben wird (MF-1177). Getragen wird nur `id.sector`, und die
 * Reihenfolge ist die der Bitlagen ab dem Index. false, wenn die Spur das
 * nicht belegt; dann ist @p sicht leer.
 */
static bool physische_sicht(const uft_d2_track_t *t, uft_track_t *sicht)
{
    memset(sicht, 0, sizeof *sicht);
    if (!t || !t->has_bitstream || t->sectors.count == 0) return false;
    const size_t nbits = t->bitstream.nbits;
    const size_t ix    = t->bitstream.index_bit;
    if (nbits == 0 || ix == SIZE_MAX || ix >= nbits) return false;

    const size_t n = t->sectors.count;
    size_t *lage = malloc(n * sizeof *lage);
    size_t *folge = malloc(n * sizeof *folge);
    if (!lage || !folge) { free(lage); free(folge); return false; }

    bool ok = true;
    for (size_t i = 0; i < n && ok; i++) {
        const size_t b = t->sectors.items[i].idam_bit;
        if (b == SIZE_MAX || b >= nbits) { ok = false; break; }
        lage[i] = (b + nbits - ix) % nbits;   /* Abstand hinter dem Index */
        folge[i] = i;
    }
    /* Einfuegesortierung: eine Spur traegt wenige Sektoren. Zwei Sektoren
     * an DERSELBEN Lage sind keine physische Folge — abgesagt. */
    for (size_t i = 1; i < n && ok; i++) {
        const size_t k = folge[i];
        size_t j = i;
        while (j > 0 && lage[folge[j - 1]] > lage[k]) {
            folge[j] = folge[j - 1];
            j--;
        }
        folge[j] = k;
    }
    for (size_t i = 1; i < n && ok; i++)
        if (lage[folge[i]] == lage[folge[i - 1]]) ok = false;

    if (ok) {
        sicht->sectors = calloc(n, sizeof *sicht->sectors);
        ok = sicht->sectors != NULL;
    }
    if (ok) {
        for (size_t i = 0; i < n; i++)
            sicht->sectors[i].id.sector = t->sectors.items[folge[i]].id_sec;
        sicht->sector_count = n;
        sicht->cylinder = t->cyl;
        sicht->head = t->head;
    }
    free(lage);
    free(folge);
    return ok;
}

static bool befund_da(const uft_disk2_t *d, const char *code, int head)
{
    for (size_t i = 0; i < uft_d2_diag_count(d); i++) {
        const uft_d2_diag_t *g = uft_d2_diag_at(d, i);
        if (g->head == head && strcmp(g->code, code) == 0) return true;
    }
    return false;
}

typedef struct {
    unsigned paare;      /**< Paare, in denen BEIDE Spuren messen */
    bool     einig;
    uint8_t  spt, interleave;
    int16_t  spiral;
} kopf_t;

static void kopf_messen(const uft_disk2_t *d, uint16_t max_cyl, uint8_t h,
                        kopf_t *k)
{
    memset(k, 0, sizeof *k);
    k->einig = true;
    k->spiral = -1;

    uft_track_t vor;
    memset(&vor, 0, sizeof vor);
    bool vor_misst = false;

    for (unsigned c = 0; c <= max_cyl; c++) {
        uft_track_t cur;
        uft_st_order_t o;
        const bool da = physische_sicht(uft_d2_track_get(d, (uint16_t)c, h),
                                        &cur);
        /* Die Vorspur nur, wenn sie selbst misst: eine Spur mit Luecke hat
         * keinen belegten Anfang (H-18). */
        uft_st_order_messen(da && vor_misst ? &vor : NULL, da ? &cur : NULL,
                            &o);
        if (o.gemessen && o.spiral >= 0) {
            if (k->paare == 0) {
                k->spt = o.spt; k->interleave = o.interleave;
                k->spiral = o.spiral;
            } else if (o.spt != k->spt || o.interleave != k->interleave
                       || o.spiral != k->spiral) {
                k->einig = false;
            }
            k->paare++;
        }
        free(vor.sectors);
        vor = cur;                  /* uebernimmt den Speicher, oder leer */
        vor_misst = da && o.gemessen;
    }
    free(vor.sectors);
}

size_t uft_st_order_befunde(struct uft_disk2 *d)
{
    if (!d || !atari_behaelter(d)) return 0;
    uint16_t mc; uint8_t mh;
    if (!uft_d2_extent(d, &mc, &mh)) return 0;
    const size_t vorher = uft_d2_diag_count(d);

    kopf_t k[2];
    const unsigned koepfe = mh >= 1 ? 2u : 1u;
    for (unsigned h = 0; h < 2; h++) {
        memset(&k[h], 0, sizeof k[h]);
        k[h].spiral = -1;
        if (h < koepfe) kopf_messen(d, mc, (uint8_t)h, &k[h]);
        if (k[h].paare == 0 || befund_da(d, "ST_REIHENFOLGE", (int)h))
            continue;
        if (k[h].einig)
            uft_d2_diag(d, UFT_D2_DIAG_INFO, UFT_D2_LAYER_SECTORS, -1, (int)h,
                        -1, "ST_REIHENFOLGE",
                        "Kopf %u: Interleave %u, Spiralfaktor %d bei %u "
                        "Sektoren je Spur (aus %u Spurpaaren; physische "
                        "Reihenfolge aus den Bitlagen ab dem Index)",
                        h, (unsigned)k[h].interleave, (int)k[h].spiral,
                        (unsigned)k[h].spt, k[h].paare);
        else
            uft_d2_diag(d, UFT_D2_DIAG_INFO, UFT_D2_LAYER_SECTORS, -1, (int)h,
                        -1, "ST_REIHENFOLGE",
                        "Kopf %u: Interleave/Spiralfaktor/Sektorzahl ueber "
                        "%u Spurpaare uneinheitlich — keine Herkunftsaussage",
                        h, k[h].paare);
    }

    /* Herkunft nur aus einheitlich gemessenen Koepfen mit gleicher
     * Sektorzahl. Seite 1 fehlt -> -1, wie `uft_st_tos_herkunft()` es
     * vorsieht. */
    const bool s0 = k[0].paare && k[0].einig;
    const bool s1 = k[1].paare && k[1].einig;
    if (s0 && (!s1 || k[1].spt == k[0].spt)
        && !befund_da(d, "ST_TOS_HERKUNFT", -1)) {
        const uft_tos_herkunft_t her =
            uft_st_tos_herkunft(k[0].spiral, s1 ? k[1].spiral : -1, k[0].spt);
        if (her != UFT_TOS_UNBEKANNT)
            uft_d2_diag(d, UFT_D2_DIAG_INFO, UFT_D2_LAYER_SECTORS, -1, -1, -1,
                        "ST_TOS_HERKUNFT",
                        "%s (Hinweis, kein Beweis: gilt nur fuer Disketten, "
                        "die das TOS-Desktop formatiert hat)",
                        uft_st_tos_herkunft_text(her));
    }
    return uft_d2_diag_count(d) - vorher;
}
