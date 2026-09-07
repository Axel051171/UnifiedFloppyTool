/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_rev_grenzen.c
 * @brief Umdrehungsgrenzen in einem flachen Flussstrom (MF-951)
 *
 * Begruendung, Messzahlen und der Fehler, den dieses Modul verhindert,
 * stehen im Header `include/uft/flux/uft_rev_grenzen.h`.
 */
#include "uft/flux/uft_rev_grenzen.h"

size_t uft_rev_grenzen_aus_dauern(const uint32_t *samples,
                                  size_t sample_count,
                                  const uint32_t *dauern,
                                  size_t dauer_count,
                                  size_t *versaetze,
                                  size_t max_versaetze)
{
    if (!samples || !dauern || !versaetze) return 0;
    if (sample_count == 0 || dauer_count == 0 || max_versaetze == 0) return 0;

    size_t   n = 0;
    size_t   i = 0;
    uint64_t gelaufen = 0;      /* Ticks seit Beginn der laufenden Umdrehung */

    versaetze[n++] = 0;         /* Umdrehung 0 beginnt am Anfang */

    for (size_t k = 0; k < dauer_count && n < max_versaetze; k++) {
        const uint64_t soll = dauern[k];

        /* Eine FUEHRENDE Null ist protokollgemaess und keine Umdrehung:
         * Greaseweazle setzt bei `index_sync` einen Indexpuls an den
         * Anfang des Stroms, und die Zeit von dort bis dorthin ist null.
         * `index_times[0]` ist deshalb 0, und die Dauer der ERSTEN
         * Umdrehung steht in `index_times[1]`.
         *
         * Gemessen am erzeugten Strom (MF-951):
         *     index_times = 0, 17 867 232, 3 437 856, 3 463 488
         *
         * Eine erste Fassung dieser Schleife brach bei jeder Null ab und
         * lieferte deshalb genau EINE Grenze — der Kettentest hat das
         * gefunden. Eine Null MITTEN in der Liste bleibt ein Abbruch:
         * dort waere sie eine Umdrehung ohne Dauer, und die gibt es
         * nicht. */
        if (soll == 0) {
            if (n == 1 && i == 0) continue;   /* fuehrende Null */
            break;
        }

        /* Bis zum Ende dieser Umdrehung laufen. */
        while (i < sample_count && gelaufen < soll)
            gelaufen += samples[i++];

        /* Ist der Strom hier zu Ende, beginnt keine weitere Umdrehung
         * mehr — entweder weil er mitten in dieser endet, oder weil er
         * genau mit ihr aufhoert. Der bereits gemeldete Beginn der
         * laufenden bleibt gueltig; ihre wahre Laenge liefert
         * uft_rev_laengen_aus_grenzen().
         *
         * Hier stand zuerst zusaetzlich eine Pruefung `gelaufen < soll`.
         * Die MUTATIONSPROBE hat gezeigt, dass sie nichts tut: die
         * Schleife darueber endet nur, wenn `i >= sample_count` ODER
         * `gelaufen >= soll` — der erste Fall ist also der einzige, in
         * dem sie greifen koennte, und den faengt die Zeile hier. Zwei
         * Bedingungen fuer denselben Fall sind eine zu viel. */
        if (i >= sample_count) break;

        versaetze[n++] = i;
        gelaufen -= soll;       /* Ueberhang zaehlt zur naechsten Umdrehung */
    }

    /* Ein einzelner Versatz ist keine Grenze, sondern nur der Anfang.
     * Wer damit vergleichen wollte, haette eine Umdrehung — und das ist
     * genau der Fall, den uft_fuse_revolutions() ablehnt (MF-949). */
    return n;
}

void uft_rev_laengen_aus_grenzen(const size_t *versaetze, size_t anzahl,
                                 size_t sample_count, size_t *laengen)
{
    if (!versaetze || !laengen || anzahl == 0) return;

    for (size_t k = 0; k + 1 < anzahl; k++)
        laengen[k] = versaetze[k + 1] - versaetze[k];

    laengen[anzahl - 1] = (sample_count > versaetze[anzahl - 1])
                        ? sample_count - versaetze[anzahl - 1]
                        : 0;
}
