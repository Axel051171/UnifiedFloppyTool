/* SPDX-License-Identifier: MIT */
/**
 * @file test_deepread_messung.c
 * @brief Was die DeepRead-Forensikmodule wirklich messen (MF-1430)
 *
 * ── WORUM ES GEHT ────────────────────────────────────────────────────
 *
 * Die fuenf Module in `src/analysis/deepread/` hatten bis MF-1430 KEINEN
 * Aufrufer (MF-627/MF-767) und keinen einzigen Test. Bevor eines davon
 * in die Oberflaeche kommt, steht hier, was ihre Rechnungen gegen
 * geschlossene Loesungen ergeben. Referenz ist jeweils die Mathematik
 * selbst (Regressionsgerade, Korrelationskoeffizient nach Pearson) und
 * die Speicherordnung von `otdr_disk_create()` — keine fremde Quelle,
 * weil es fuer diese Kenngroessen keine gibt.
 *
 * ── DIE BEFUNDE (alle gemessen, Rotbeweis zuerst) ────────────────────
 *
 *   A1  `uft_deepread_aging_analyze()` teilte die SNR-Summe durch ALLE
 *       Spurplaetze, auch die leeren, und rechnete den SNR-Gradienten
 *       ueber leere Plaetze mit SNR 0. An der gw-Aufnahme
 *       `gw_fm_acorn_3trk.scp` (3 Spuren in 6 Plaetzen) ergab das
 *       gemessen **21,01 dB statt 42,01 dB** und einen Gradienten von
 *       -3,6 dB/Spur auf einer Diskette, deren drei Spuren gleich sind.
 *
 *   C1  `uft_deepread_crosstrack_analyze()` verglich Spur t mit Spur
 *       t+1. `otdr_disk_create()` legt Spuren als `zyl * koepfe + kopf`
 *       ab — auf einer zweiseitigen Diskette sind t und t+1 also IMMER
 *       die beiden Oberflaechen, nie radial benachbarte Spuren derselben
 *       Oberflaeche. Eine radiale Beschaedigung (Kratzer) war damit auf
 *       zweiseitigen Disketten grundsaetzlich nicht sichtbar.
 *
 *   S1  `uft_deepread_compute_llr()` liest `quality_profile` als
 *       positiven Rauschabstand („higher = better"). `otdr_quality_to_db()`
 *       liefert aber einen VERLUST: 0 dB ist perfekt, alles andere ist
 *       negativ. `quality_norm = q/20` wird damit immer auf 0,1
 *       geklemmt, und JEDE LLR ist genau +0,1 oder -0,1 — die harte
 *       Entscheidung, ohne jede weiche Information. Das ist hier
 *       FESTGEHALTEN, nicht behoben: ein richtiges Weichmodell braucht
 *       eine Quelle, und eine erfundene Formel waere dieselbe
 *       Fabrikation (P3-630). Wer das Modul repariert, macht diesen Test
 *       rot und muss die Doku nachziehen.
 *
 *   F1  `uft_deepread_fingerprint()` bildet CRC32 ueber die Float-Bytes
 *       des normierten Histogramms. Ein einziges Intervall, das um
 *       20 ns ueber eine 100-ns-Bingrenze rutscht, aendert den Hash —
 *       der Kosinus bleibt > 0,9999. Der Hash ist also KEINE Kennung
 *       des Datentraegers (zwei Lesungen derselben Diskette haben
 *       Jitter), nur der Kosinusvergleich traegt. Festgehalten.
 *
 * ── WAS NICHT BELEGT IST ─────────────────────────────────────────────
 *
 * Die Klassen (Pristine/Mild/…; Radial/Magnetic/…) haengen an Schwellen
 * ohne Quelle (0,01/0,005/0,001; 0,7/0,3/0,6). Es gibt im freien Korpus
 * **keine echte Flussaufnahme mit Jitter** — `gw_fm_acorn_3trk.scp` und
 * `hxcfe_kfx_t*.raw` sind synthetisch (gemessen: beide Umdrehungen der
 * gw-Datei sind bitgleich, nur 3950 und 7950 ns). Die Schwellen sind
 * damit an keiner Diskette geeicht; die Oberflaeche zeigt sie als
 * Heuristik (P3-630).
 */

#include "uft/analysis/floppy_otdr.h"
#include "uft/analysis/uft_deepread_aging.h"
#include "uft/analysis/uft_deepread_crosstrack.h"
#include "uft/analysis/uft_deepread_fingerprint.h"
#include "uft/analysis/uft_deepread_soft_decode.h"
#include "uft/flux/uft_scp_parser.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef UFT_CORPUS_FREE_DIR
#define UFT_CORPUS_FREE_DIR "tests/corpus_free"
#endif
#define GW_SCP UFT_CORPUS_FREE_DIR "/gw_fm_acorn_3trk.scp"

static int fehler = 0;
static int geprueft = 0;

#define PRUEFE(bed, ...) do {                                   \
        geprueft++;                                             \
        if (!(bed)) {                                           \
            fehler++;                                           \
            printf("  FEHLER %s:%d: ", __FILE__, __LINE__);     \
            printf(__VA_ARGS__);                                \
            printf("\n");                                       \
        }                                                       \
    } while (0)

/* Setzt ein Qualitaetsprofil von Hand — die Module lesen nur das Feld. */
static void setze_profil(otdr_track_t *t, const float *werte, uint32_t n)
{
    free(t->quality_profile);
    t->quality_profile = (float *)malloc(n * sizeof(float));
    memcpy(t->quality_profile, werte, n * sizeof(float));
    t->bitcell_count = n;
}

/* ── A1 ───────────────────────────────────────────────────────────── */

static void t_aging_leere_plaetze_zaehlen_nicht(void)
{
    otdr_disk_t *d = otdr_disk_create(3, 1);
    float p[64];
    for (int i = 0; i < 64; i++) p[i] = -1.0f - 0.01f * (float)(i % 7);
    setze_profil(&d->tracks[0], p, 64);
    setze_profil(&d->tracks[1], p, 64);
    d->tracks[0].stats.snr_estimate = 20.0f;
    d->tracks[1].stats.snr_estimate = 20.0f;
    /* tracks[2] bleibt leer: kein Fluss, kein Profil */

    uft_aging_result_t r;
    PRUEFE(uft_deepread_aging_analyze(d, &r) == 0, "aging rc");
    PRUEFE(fabsf(r.mean_snr_db - 20.0f) < 1e-4f,
           "mittlerer SNR ueber 2 gemessene Spuren = 20, war %.4f", r.mean_snr_db);
    PRUEFE(fabsf(r.snr_gradient) < 1e-4f,
           "zwei gleiche Spuren haben Gradient 0, war %.4f", r.snr_gradient);
    otdr_disk_free(d);
}

static void t_aging_regression_geschlossen(void)
{
    /* y = 0,5 x - 3: Steigung 0,5, R^2 = 1, Rest 0 */
    otdr_track_t t;
    memset(&t, 0, sizeof t);
    float p[100];
    for (int i = 0; i < 100; i++) p[i] = 0.5f * (float)i - 3.0f;
    setze_profil(&t, p, 100);
    float s = 0, r2 = 0, rm = 0;
    PRUEFE(uft_deepread_aging_track(&t, &s, &r2, &rm) == 0, "aging_track rc");
    PRUEFE(fabsf(s - 0.5f) < 1e-5f, "Steigung 0,5, war %g", s);
    PRUEFE(fabsf(r2 - 1.0f) < 1e-5f, "R^2 1, war %g", r2);
    PRUEFE(rm < 1e-3f, "Rest 0, war %g", rm);
    free(t.quality_profile);
}

/* ── C1 ───────────────────────────────────────────────────────────── */

static void t_crosstrack_vergleicht_dieselbe_oberflaeche(void)
{
    /* 2 Zylinder x 2 Koepfe. Kopf 0 traegt auf beiden Zylindern Muster A,
     * Kopf 1 auf beiden Muster B = -A. Radial benachbart (dieselbe
     * Oberflaeche) ist die Korrelation also +1, zwischen den Oberflaechen
     * -1. Geschlossene Loesung: Mittel ueber die 2 radialen Paare = +1. */
    otdr_disk_t *d = otdr_disk_create(2, 2);
    float a[128], b[128];
    for (int i = 0; i < 128; i++) {
        a[i] = -2.0f + sinf((float)i * 0.3f);
        b[i] = -2.0f - sinf((float)i * 0.3f);
    }
    for (int t = 0; t < 4; t++)
        setze_profil(&d->tracks[t], d->tracks[t].head == 0 ? a : b, 128);

    uft_crosstrack_result_t r;
    memset(&r, 0, sizeof r);
    PRUEFE(uft_deepread_crosstrack_analyze(d, &r) == 0, "crosstrack rc");
    PRUEFE(fabsf(r.mean_correlation - 1.0f) < 1e-4f,
           "radiale Paare derselben Oberflaeche: NCC 1, war %.4f", r.mean_correlation);
    PRUEFE(r.pair_count == 2, "2 Zylinder x 2 Koepfe = 2 radiale Paare, war %u", r.pair_count);
    uft_crosstrack_result_free(&r);
    otdr_disk_free(d);
}

static void t_crosstrack_ohne_paar_sagt_es(void)
{
    /* Nur Zylinder 0 und 2 belegt: kein radial benachbartes Paar. Der
     * Mittelwert 0 hiesse sonst „unkorreliert" — pair_count sagt, dass
     * gar nicht gemessen wurde. */
    otdr_disk_t *d = otdr_disk_create(3, 1);
    float a[32];
    for (int i = 0; i < 32; i++) a[i] = -1.0f - 0.1f * (float)(i % 5);
    setze_profil(&d->tracks[0], a, 32);
    setze_profil(&d->tracks[2], a, 32);
    uft_crosstrack_result_t r;
    memset(&r, 0, sizeof r);
    PRUEFE(uft_deepread_crosstrack_analyze(d, &r) == 0, "crosstrack rc");
    PRUEFE(r.pair_count == 0, "kein Nachbarpaar, war %u", r.pair_count);
    uft_crosstrack_result_free(&r);
    otdr_disk_free(d);
}

/* ── S1 ───────────────────────────────────────────────────────────── */

static void t_llr_ist_nur_die_harte_entscheidung(void)
{
    /* Der Wertebereich, den otdr_quality_to_db() liefern kann: <= 0. */
    for (float dev = 0.0f; dev <= 400.0f; dev += 0.5f)
        PRUEFE(otdr_quality_to_db(dev) <= 0.0f, "Verlust >0 bei %.1f %%", dev);

    /* Intervalle mit Jitter, Profil aus genau diesem Wertebereich. */
    uint32_t flux[200];
    for (int i = 0; i < 200; i++)
        flux[i] = (uint32_t)((i % 3 + 2) * 2000 + ((i * 37) % 301) - 150);
    float q[2000];
    for (int i = 0; i < 2000; i++) q[i] = otdr_quality_to_db((float)(i % 40));

    uft_soft_bits_t sb;
    memset(&sb, 0, sizeof sb);
    PRUEFE(uft_deepread_compute_llr(flux, 200, q, 2000, 2000, &sb) == 0, "llr rc");
    uint32_t anders = 0;
    for (size_t i = 0; i < sb.bit_count; i++)
        if (fabsf(fabsf(sb.llr[i]) - 0.1f) > 1e-6f) anders++;
    PRUEFE(sb.bit_count > 0, "keine Bits");
    PRUEFE(anders == 0,
           "S1 festgehalten: jede |LLR| ist 0,1 — %u von %zu weichen ab. Wer das "
           "Weichmodell repariert, zieht P3-630 und CAPABILITIES nach.",
           anders, sb.bit_count);
    uft_soft_bits_free(&sb);
}

/* ── F1 ───────────────────────────────────────────────────────────── */

static otdr_disk_t *eine_spur(const uint32_t *flux, uint32_t n)
{
    otdr_disk_t *d = otdr_disk_create(1, 1);
    otdr_track_load_flux(&d->tracks[0], flux, n, 0);
    otdr_track_histogram(&d->tracks[0]);
    return d;
}

static void t_fingerprint_hash_ist_keine_kennung(void)
{
    uint32_t f[1000];
    for (int i = 0; i < 1000; i++) f[i] = (i % 3) ? 3950u : 7950u;
    otdr_disk_t *d1 = eine_spur(f, 1000);
    f[500] = 4010u;                 /* 60 ns Jitter, ueber die Grenze 4000 */
    otdr_disk_t *d2 = eine_spur(f, 1000);

    uft_media_fingerprint_t a, b;
    PRUEFE(uft_deepread_fingerprint(d1, &a) == 0 && a.valid, "fp1");
    PRUEFE(uft_deepread_fingerprint(d2, &b) == 0 && b.valid, "fp2");
    float cos = uft_deepread_fingerprint_compare(&a, &b);
    PRUEFE(strcmp(a.hash_hex, b.hash_hex) != 0,
           "F1 festgehalten: ein Intervall ueber eine Bingrenze aendert den Hash");
    PRUEFE(cos > 0.9999f, "Kosinus bleibt nahe 1, war %.6f", cos);
    otdr_disk_free(d1);
    otdr_disk_free(d2);
}

/* ── A1 am Produktionspfad: gw-Aufnahme, wie das Panel sie laedt ──── */

static void t_aging_an_gw_aufnahme(void)
{
    uft_scp_ctx_t ctx;
    memset(&ctx, 0, sizeof ctx);
    if (uft_scp_open(&ctx, GW_SCP) != 0) {
        PRUEFE(0, "%s nicht lesbar (corpus_free ist versioniert)", GW_SCP);
        return;
    }
    int st = ctx.header.start_track, n = ctx.header.end_track - st + 1;
    /* Dieselbe Geometrie wie UftOtdrPanel::loadScpFile() */
    otdr_disk_t *d = otdr_disk_create((uint8_t)((n + 1) / 2), n > 1 ? 2 : 1);
    int belegt = 0;
    for (int t = 0; t < n && t < d->track_count; t++) {
        uft_scp_track_data_t td;
        memset(&td, 0, sizeof td);
        if (uft_scp_read_track(&ctx, st + t, &td) != 0) continue;
        for (int r = 0; r < (int)td.revolution_count && r < OTDR_MAX_REVOLUTIONS; r++)
            if (td.revolutions[r].flux_data && td.revolutions[r].flux_count)
                otdr_track_load_flux(&d->tracks[t], td.revolutions[r].flux_data,
                                     td.revolutions[r].flux_count, (uint8_t)r);
        uft_scp_free_track(&td);
        belegt++;
    }
    uft_scp_close(&ctx);
    otdr_config_t cfg;
    otdr_config_defaults(&cfg);
    for (int t = 0; t < d->track_count; t++)
        if (d->tracks[t].flux_count) otdr_track_analyze(&d->tracks[t], &cfg);

    PRUEFE(belegt == 3 && d->track_count == 6, "3 Spuren in 6 Plaetzen, %d/%u",
           belegt, d->track_count);
    uft_aging_result_t r;
    PRUEFE(uft_deepread_aging_analyze(d, &r) == 0, "aging rc");
    float snr0 = d->tracks[0].stats.snr_estimate;
    PRUEFE(fabsf(r.mean_snr_db - snr0) < 0.01f,
           "gw: drei gleiche Spuren, mittlerer SNR = Spur-SNR %.2f, war %.2f",
           snr0, r.mean_snr_db);
    PRUEFE(fabsf(r.snr_gradient) < 0.01f, "gw: Gradient 0, war %.3f", r.snr_gradient);
    otdr_disk_free(d);
}

int main(void)
{
    printf("DeepRead-Messung (MF-1430)\n");
    t_aging_leere_plaetze_zaehlen_nicht();
    t_aging_regression_geschlossen();
    t_crosstrack_vergleicht_dieselbe_oberflaeche();
    t_crosstrack_ohne_paar_sagt_es();
    t_llr_ist_nur_die_harte_entscheidung();
    t_fingerprint_hash_ist_keine_kennung();
    t_aging_an_gw_aufnahme();
    printf("%d Pruefungen, %d Fehler\n", geprueft, fehler);
    return fehler ? 1 : 0;
}
