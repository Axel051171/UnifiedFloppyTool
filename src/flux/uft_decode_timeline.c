/**
 * @file uft_decode_timeline.c
 * @brief Wo auf der Spur steht was — Scheibenkarte einer Dekodierung (MF-501).
 *
 * Zweck, Zusicherungen und Grenzen stehen im Header.
 */

#include "uft/flux/uft_decode_timeline.h"

#include <stdlib.h>
#include <string.h>

/**
 * Ein Sektor mit seiner Anfangsposition, zum Sortieren.
 *
 * Sortiert wird nach der Bitposition und nicht nach der Sektornummer: auf
 * der Spur liegen die Sektoren in Schreibreihenfolge, und die muss nicht
 * die Nummernfolge sein (Interleave). Eine Karte nach Nummern sortiert
 * waere eine Karte, die nicht die Spur beschreibt.
 */
typedef struct {
    size_t             pos;
    int                sector;
    uft_slice_status_t status;
} tl_mark_t;

static int tl_cmp(const void *a, const void *b)
{
    const tl_mark_t *x = (const tl_mark_t *)a;
    const tl_mark_t *y = (const tl_mark_t *)b;
    if (x->pos < y->pos) return -1;
    if (x->pos > y->pos) return 1;
    return 0;
}

double uft_timeline_angle_error(const uft_decode_timeline_t *t)
{
    if (!t) return -1.0;
    /* Ohne Zeitbasis gibt es keinen Winkel — und damit auch keinen
     * Fehler, sondern eine Nichtaussage. */
    if (t->cell_ns <= 0.0 || t->revolution_ns <= 0.0) return -1.0;
    if (t->warp_span <= 1.0) return 0.0;
    return (t->warp_span - 1.0) * UFT_TIMELINE_ANGLE_ERR_PER_SPAN;
}

bool uft_timeline_build(const flux_decoded_track_t *track, size_t bit_count,
                        double cell_ns, double revolution_ns,
                        uft_decode_timeline_t *out)
{
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    /* Ohne Stromlaenge laesst sich „lueckenlos" nicht zusichern — und eine
     * Karte ohne diese Zusicherung ist keine. */
    if (!track || bit_count == 0) return false;

    const size_t n_sec = track->sector_count;

    tl_mark_t *marks = NULL;
    size_t n_marks = 0;
    if (n_sec > 0) {
        marks = (tl_mark_t *)calloc(n_sec, sizeof(tl_mark_t));
        if (!marks) return false;

        for (size_t i = 0; i < n_sec; i++) {
            const flux_decoded_sector_t *s = &track->sectors[i];
            /* Ein Sektor, dessen Anfang jenseits des Stroms liegt, ist
             * abgeschnitten. Er bekommt keine Scheibe — eine Scheibe
             * ausserhalb des Stroms waere eine Aussage ueber Bits, die es
             * nicht gibt. */
            if ((size_t)s->id_position >= bit_count) continue;

            marks[n_marks].pos    = (size_t)s->id_position;
            marks[n_marks].sector = (int)s->sector;
            /* „Heil" ist eine Aussage ueber gelesene Daten, nicht ueber
             * gefundene Kandidaten (MF-466): beide Pruefsummen muessen
             * stimmen. */
            marks[n_marks].status = (s->id_crc_ok && s->data_crc_ok)
                                        ? UFT_SLICE_DECODED
                                        : UFT_SLICE_DAMAGED;
            n_marks++;
        }
        if (n_marks > 1)
            qsort(marks, n_marks, sizeof(tl_mark_t), tl_cmp);
    }

    /* Hoechstens eine Scheibe je Marke, plus eine fuehrende. */
    uft_decode_slice_t *sl =
        (uft_decode_slice_t *)calloc(n_marks + 1, sizeof(uft_decode_slice_t));
    if (!sl) { free(marks); return false; }

    size_t count = 0;

    /* Vor der ersten Marke hat niemand etwas behauptet. Das koennte der
     * Rest des letzten Sektors der Umdrehung sein — aber „koennte" ist
     * keine Messung, und die Karte sagt lieber „unberuehrt". */
    if (n_marks == 0 || marks[0].pos > 0) {
        sl[count].first_bit = 0;
        sl[count].end_bit   = (n_marks == 0) ? bit_count : marks[0].pos;
        sl[count].status    = UFT_SLICE_UNTOUCHED;
        sl[count].sector    = -1;
        count++;
    }

    for (size_t i = 0; i < n_marks; i++) {
        size_t end = (i + 1 < n_marks) ? marks[i + 1].pos : bit_count;
        /* Zwei Marken auf derselben Bitposition ergaeben eine leere
         * Scheibe. Die erste behaelt den Platz; die zweite waere ohnehin
         * derselbe Abschnitt unter anderem Namen. */
        if (end <= marks[i].pos) continue;

        sl[count].first_bit = marks[i].pos;
        sl[count].end_bit   = end;
        sl[count].status    = marks[i].status;
        sl[count].sector    = marks[i].sector;
        count++;
    }

    free(marks);

    out->slices        = sl;
    out->count         = count;
    out->bit_count     = bit_count;
    out->cell_ns       = (cell_ns > 0.0) ? cell_ns : 0.0;
    out->revolution_ns = (revolution_ns > 0.0) ? revolution_ns : 0.0;
    /* Die Spur weiss selbst, wie gleichmaessig sie lief (MF-495). Ohne
     * diesen Wert waere die Winkelangabe eine Zahl ohne Fehlerschranke. */
    out->warp_span     = track->warp_span;
    return true;
}

void uft_timeline_free(uft_decode_timeline_t *t)
{
    if (!t) return;
    free(t->slices);
    memset(t, 0, sizeof(*t));
}

double uft_timeline_angle(const uft_decode_timeline_t *t, size_t slice)
{
    if (!t || !t->slices || slice >= t->count) return -1.0;
    /* Ohne Zellendauer oder Umdrehungsdauer gibt es keine Winkellage,
     * sondern nur eine Bitnummer. Eine Zahl zwischen 0 und 1 auszugeben
     * waere hier eine Erfindung mit Nachkommastellen. */
    if (t->cell_ns <= 0.0 || t->revolution_ns <= 0.0) return -1.0;

    double ns = (double)t->slices[slice].first_bit * t->cell_ns;
    double frac = ns / t->revolution_ns;
    /* Mehrere Umdrehungen im Strom sind der Normalfall (MF-478). Der
     * Winkel ist eine Lage auf der Scheibe, kein Fortschritt im Strom. */
    frac -= (double)(long long)frac;
    if (frac < 0.0) frac = 0.0;
    return frac;
}

double uft_timeline_fraction(const uft_decode_timeline_t *t,
                             uft_slice_status_t status)
{
    if (!t || !t->slices || t->bit_count == 0) return 0.0;
    size_t bits = 0;
    for (size_t i = 0; i < t->count; i++)
        if (t->slices[i].status == status)
            bits += t->slices[i].end_bit - t->slices[i].first_bit;
    /* In Bits gemessen, nicht in Scheiben: zehn winzige beschaedigte
     * Abschnitte sind nicht dasselbe wie ein halb zerstoerter Sektor. */
    return (double)bits / (double)t->bit_count;
}
