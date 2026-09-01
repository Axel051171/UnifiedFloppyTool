/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_flux_nosync_split.c
 * @brief „Kein Sync" war vier Medienzustaende in einem Wort (MF-764).
 *
 * ── Der Befund ───────────────────────────────────────────────────────────
 *
 * Gemessen mit drei synthetischen Spuren durch den ECHTEN Dekoder
 * (`flux_decode_mfm`, nicht durch einen Nachbau des Pfads):
 *
 *     leer (geloescht)                FLUX_ERR_NO_SYNC  0 Sektoren
 *     gleichfoermig + 3 Bruchstellen  FLUX_ERR_NO_SYNC  0 Sektoren
 *     verrauscht                      FLUX_ERR_NO_SYNC  0 Sektoren
 *
 * Drei physisch voellig verschiedene Medienzustaende, ein Verdikt, und
 * kein Zaehler unterschied sie.
 *
 * Fuer ein Preservation-Werkzeug ist das die falsche Auskunft. X-Copys
 * Handbuch von 1991 (Fassung 3.4, Abschnitt „7.2) ERRORS") liest „keine
 * Lesemarkierungen gefunden" ausdruecklich als „wahrscheinlich ein
 * Kopierschutz ODER FREMDFORMAT" — also gerade nicht als leer und nicht
 * als defekt. Eine leere Diskette und eine geschuetzte sehen fuer den
 * Archivar gleich aus, wenn das Werkzeug beide „kein Sync" nennt.
 *
 * ── Woran unterschieden wird ─────────────────────────────────────────────
 *
 * Am Intervall-Histogramm, das dieser Baum seit MF-488 hat und dessen
 * Aussage bisher niemand las. Kein neues Verfahren, keine Heuristik:
 *
 *     leer            0 Berge
 *     gleichfoermig   1 Berg      ein Takt, aber keine Marke
 *     verrauscht     >1 Berg, nicht `confident`
 *
 * ── Die Gegenprobe ist der wichtigere Teil ───────────────────────────────
 *
 * Fall 4 ist ein ECHTER MFM-Strom (2T/3T/4T) ohne Sync-Marke. Er MUSS
 * `FLUX_ERR_NO_SYNC` bleiben. Wuerde er zu NOISE, haette die Verfeinerung
 * eine gueltige Spur als unlesbar gemeldet — schlimmer als die
 * Zusammenfassung, die sie behebt.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "uft/flux/uft_flux_decoder.h"

static int fehler = 0;

static const char *name_of(flux_status_t s) {
    switch (s) {
    case FLUX_OK:              return "FLUX_OK";
    case FLUX_ERR_NO_SYNC:     return "FLUX_ERR_NO_SYNC";
    case FLUX_ERR_UNFORMATTED: return "FLUX_ERR_UNFORMATTED";
    case FLUX_ERR_NOISE:       return "FLUX_ERR_NOISE";
    default:                   return "anderer Status";
    }
}

/* Fester Startwert: ein Test, der bei jedem Lauf etwas anderes misst,
 * ist keiner. */
static uint32_t rnd_state = 12345u;
static uint32_t rnd(void) {
    rnd_state ^= rnd_state << 13;
    rnd_state ^= rnd_state >> 17;
    rnd_state ^= rnd_state << 5;
    return rnd_state;
}

static void pruefe(const char *titel, const uint32_t *iv, size_t n,
                   flux_status_t erwartet)
{
    flux_raw_data_t raw;
    memset(&raw, 0, sizeof(raw));
    if (flux_raw_from_ns_intervals(iv, n, &raw) != FLUX_OK) {
        printf("  FAIL %-32s Aufbau der Fixture fehlgeschlagen\n", titel);
        fehler++;
        return;
    }

    flux_decoded_track_t trk;
    flux_decoder_options_t opts;
    memset(&trk, 0, sizeof(trk));
    memset(&opts, 0, sizeof(opts));

    flux_status_t s = flux_decode_mfm(&raw, &trk, &opts);
    int ok = (s == erwartet);
    printf("  %s %-32s %-22s (erwartet %s)\n",
           ok ? "ok  " : "FAIL", titel, name_of(s), name_of(erwartet));
    if (!ok) fehler++;

    free(raw.transitions);
    free(raw.index_times);
}

enum { ZELLE_NS = 2000, N = 24000 };

int main(void)
{
    uint32_t *iv = (uint32_t *)malloc(N * sizeof(uint32_t));
    if (!iv) return 2;

    printf("test_flux_nosync_split (MF-764)\n");

    /* 1 — LEER: eine geloeschte Spur traegt praktisch keine Wechsel. */
    for (size_t i = 0; i < 40; i++) iv[i] = 40000 + (rnd() % 20000);
    pruefe("leer (geloescht)", iv, 40, FLUX_ERR_UNFORMATTED);

    /* 2 — GLEICHFOERMIG mit wenigen Bruchstellen: das Nahtstellen-Muster.
     *     Ein Takt ist da, eine bekannte Marke nicht. */
    for (size_t i = 0; i < N; i++) iv[i] = 2 * ZELLE_NS;
    iv[4000]  = 3 * ZELLE_NS;
    iv[9000]  = 3 * ZELLE_NS;
    iv[15000] = 4 * ZELLE_NS;
    pruefe("gleichfoermig + 3 Bruchstellen", iv, N, FLUX_ERR_NO_SYNC);

    /* 3 — VERRAUSCHT: Abstaende ohne gemeinsamen Takt. */
    for (size_t i = 0; i < N; i++) iv[i] = 1500 + (rnd() % 7000);
    pruefe("verrauscht", iv, N, FLUX_ERR_NOISE);

    /* 4 — GEGENPROBE: echter MFM-Strom ohne Sync-Marke. Bleibt NO_SYNC. */
    for (size_t i = 0; i < N; i++) iv[i] = (2 + (rnd() % 3)) * ZELLE_NS;
    pruefe("echtes MFM ohne Sync-Marke", iv, N, FLUX_ERR_NO_SYNC);

    free(iv);

    /* Die vier duerfen nicht auf denselben Wert fallen — das war der
     * Ausgangsbefund, und ein Test, der das nicht prueft, koennte ihn
     * nicht wiederfinden. */
    if (fehler == 0)
        printf("  ok   vier Zustaende, drei verschiedene Verdikte\n");

    printf("%s (%d Fehler)\n", fehler ? "FAIL" : "PASS", fehler);
    return fehler ? 1 : 0;
}
