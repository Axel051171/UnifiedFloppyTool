/**
 * @file test_convert_flippy_reverse.c
 * @brief Die Rueckseite einer Flippy-Diskette (MF-484).
 *
 * Punkt 3.4 der a8rawconv-Gap-Analyse. Eine Flippy-Diskette wurde
 * beschrieben, indem man sie im EINSEITIGEN Laufwerk umdrehte. Liest man sie
 * spaeter im zweiseitigen Laufwerk vom zweiten Kopf, laeuft dieselbe Spur
 * rueckwaerts am Kopf vorbei: der Datenstrom ist zeitlich gespiegelt, und
 * kein Sync-Muster passt mehr. Fuer Bestaende mit C64-, Atari- und
 * Apple-Disketten ist das der Regelfall, nicht die Ausnahme.
 *
 * a8rawconv loest das mit `-r` (`reverse_track`, disk.cpp:63-89, Referenz-
 * Orakel, wird nicht gebaut):
 *
 *     max_time = max(letzte Indexzeit, letzte Uebergangszeit)
 *     t  ->  max_time - t        fuer Uebergaenge UND Indexmarken
 *     danach beide Folgen umdrehen
 *
 * Die Reihenfolge ist der Punkt: erst spiegeln, dann umdrehen. Das haelt
 * beide Folgen aufsteigend — worauf der Messpfad seit MF-471 aufbaut
 * (`uft_media_rev_ns_from_index` weist eine nicht aufsteigende Indexreihe
 * ausdruecklich zurueck).
 *
 * ── Was der Test aufbaut ─────────────────────────────────────────────────
 *
 * Eine echte AmigaDOS-Spur (tests/flux_gen/amigados) wird ZEITLICH
 * GESPIEGELT in eine SCP-Datei geschrieben — also genau das, was ein
 * zweiseitiges Laufwerk von der Rueckseite einer Flippy-Diskette sieht.
 * Ohne `-r` darf davon nichts dekodieren; mit `-r` muss die Diskette
 * byteidentisch zurueckkommen.
 */

#include "uft/uft_format_convert.h"
#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/formats/uft_scp_writer.h"
#include "uft/flux/uft_flux_decoder.h"
#include "flux_gen.h"                 /* tests/flux_gen/amigados */

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-52s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                                    _fail++; return; } } while (0)

#define NTRACKS      2
#define ALL_SECTORS  (NTRACKS * UFT_AMIGADOS_SPT)
#define CELLS        UFT_AMIGADOS_CELLS_PER_REV

static void get_temp_path(char *path, size_t size, const char *tag)
{
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(path, size, "%s/uft_flip_%s_%d", dir, tag, rand() % 100000);
}

static uint8_t *make_source_adf(void)
{
    uint8_t *adf = (uint8_t *)calloc(1, UFT_AMIGADOS_ADF_SIZE);
    if (!adf) return NULL;
    uft_amigados_fill_pattern(adf, (size_t)ALL_SECTORS * UFT_AMIGADOS_SECSZ);
    return adf;
}

/**
 * Eine SCP-Datei schreiben; @p mirrored kehrt die Zeitachse um.
 *
 * Gespiegelt wird auf der Intervall-Ebene: die Folge der Abstaende
 * rueckwaerts gelesen IST der gespiegelte Datenstrom. Das ist dieselbe
 * Aussage wie a8rawconvs `max_time - t` auf kumulierten Zeiten, nur eine
 * Darstellung frueher — und damit unabhaengig von der Funktion, die geprueft
 * werden soll. Ein Test, der zum Erzeugen dasselbe benutzt wie zum Pruefen,
 * prueft nichts.
 */
static int write_scp(const char *path, const uint8_t *adf, bool mirrored)
{
    uint8_t  *bits = (uint8_t *)calloc((CELLS + 7) / 8 + 1, 1);
    uint32_t *iv   = (uint32_t *)malloc(CELLS * sizeof(uint32_t));
    uint32_t *tmp  = (uint32_t *)malloc(CELLS * sizeof(uint32_t));
    if (!bits || !iv || !tmp) { free(bits); free(iv); free(tmp); return -1; }

    scp_writer_t *w = scp_writer_create(SCP_TYPE_AMIGA, 1);
    if (!w) { free(bits); free(iv); free(tmp); return -1; }

    int rc = 0;
    for (int track = 0; track < NTRACKS && rc == 0; track++) {
        uft_amigados_cells_t c = { bits, CELLS, 0, 0 };
        uft_amigados_build_track(&c, adf, (uint8_t)track, NULL);
        size_t n = uft_amigados_cells_to_intervals(&c, UFT_AMIGADOS_CELL_NS,
                                                   iv, CELLS);
        if (mirrored) {
            for (size_t i = 0; i < n; i++) tmp[i] = iv[n - 1 - i];
            memcpy(iv, tmp, n * sizeof(uint32_t));
        }
        rc = scp_writer_add_track(w, track / 2, track % 2, iv, n,
                                  UFT_AMIGADOS_REV_NS, 0);
    }
    if (rc == 0) rc = scp_writer_save(w, path);

    scp_writer_free(w);
    free(bits); free(iv); free(tmp);
    return rc;
}

static uft_error_t convert(const char *scp, const char *out, bool reverse,
                           uft_convert_result_t *res)
{
    uft_convert_options_t o = uft_convert_default_options();
    o.accept_data_loss = true;
    o.reverse_decode = reverse;
    memset(res, 0, sizeof(*res));
    return uft_convert_file(scp, out, UFT_FORMAT_ADF, &o, res);
}

static bool adf_matches(const char *path, const uint8_t *want)
{
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    uint8_t *got = (uint8_t *)malloc(UFT_AMIGADOS_ADF_SIZE);
    if (!got) { fclose(f); return false; }
    size_t n = fread(got, 1, UFT_AMIGADOS_ADF_SIZE, f);
    fclose(f);
    bool ok = (n == UFT_AMIGADOS_ADF_SIZE) &&
              (memcmp(got, want, (size_t)ALL_SECTORS * UFT_AMIGADOS_SECSZ) == 0);
    free(got);
    return ok;
}

/* ────────────────────────────────────────────────────────────────────── */

TEST(reversing_twice_is_the_identity)
{
    /* Die Eigenschaft, die die Funktion ueberhaupt zu einer Spiegelung
     * macht. Faellt sie, ist es irgendeine Umsortierung. */
    uint32_t iv[6] = { 1000, 2000, 1500, 3000, 1000, 2500 };
    flux_raw_data_t raw;
    ASSERT(flux_raw_from_ns_intervals_indexed(iv, 6, 11000, &raw) == FLUX_OK);
    ASSERT(raw.index_count == 2);

    uint32_t before[8];
    memcpy(before, raw.transitions, 6 * sizeof(uint32_t));
    uint32_t idx_before[2] = { raw.index_times[0], raw.index_times[1] };

    ASSERT(flux_raw_reverse(&raw) == FLUX_OK);
    ASSERT(flux_raw_reverse(&raw) == FLUX_OK);

    ASSERT(memcmp(before, raw.transitions, 6 * sizeof(uint32_t)) == 0);
    ASSERT(raw.index_times[0] == idx_before[0]);
    ASSERT(raw.index_times[1] == idx_before[1]);

    flux_raw_free(&raw);
}

TEST(both_series_stay_ascending_and_the_revolution_survives)
{
    /* Worauf der ganze Messpfad seit MF-471 aufbaut: eine nicht aufsteigende
     * Indexreihe weist uft_media_rev_ns_from_index ausdruecklich zurueck.
     * Eine Spiegelung, die das verletzt, wuerde die gemessene Umdrehung
     * still verlieren — und damit die Zellendauer-Wahl auf ihren Nennwert
     * zurueckfallen lassen, ohne dass es jemand merkt. */
    uint32_t iv[5] = { 4000, 6000, 4000, 8000, 4000 };
    flux_raw_data_t raw;
    ASSERT(flux_raw_from_ns_intervals_indexed(iv, 5, 26000, &raw) == FLUX_OK);
    ASSERT(raw.index_count == 2);
    uint32_t rev_before = raw.index_times[1] - raw.index_times[0];

    ASSERT(flux_raw_reverse(&raw) == FLUX_OK);

    for (size_t i = 1; i < raw.transition_count; i++)
        ASSERT(raw.transitions[i] > raw.transitions[i - 1]);
    ASSERT(raw.index_times[1] > raw.index_times[0]);

    /* Der ABSTAND der Marken ist die Umdrehungsdauer — er muss die
     * Spiegelung unveraendert ueberstehen. */
    ASSERT(raw.index_times[1] - raw.index_times[0] == rev_before);

    flux_raw_free(&raw);
}

TEST(a_mirrored_capture_decodes_to_nothing_without_the_flag)
{
    /* Die Ausgangslage, ohne die der naechste Test nichts bedeutet. */
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);

    char scp[512], out[512];
    get_temp_path(scp, sizeof(scp), "back.scp");
    get_temp_path(out, sizeof(out), "back.adf");
    ASSERT(write_scp(scp, adf, true) == 0);

    uft_convert_result_t r;
    /* MF-545: hier stand `== UFT_OK`.
     *
     * Der Aufruf wandelt in diesem Fall NICHTS — die Zaehler daneben sagen
     * es ja selbst. Bis MF-545 schrieb der Wandler trotzdem eine Datei
     * voller Groesse und meldete Erfolg; seither lehnt er ab, wenn keine
     * einzige Spur gewandelt wurde.
     *
     * Die Aussage dieses Tests haengt daran nicht: gemessen werden die
     * Zaehler, nicht der Rueckgabewert. Was sich geaendert hat, ist die
     * Ehrlichkeit des Aufrufs, nicht das Verhalten, das hier geprueft wird.
     *
     * Belegt, dass es keine Regression ist: im selben Test gibt der
     * GELUNGENE Aufruf weiterhin UFT_OK zurueck. */
    ASSERT(convert(scp, out, false, &r) != UFT_OK);
    if (r.sectors_converted != 0)
        printf("\n        %d Sektoren ohne -r\n", r.sectors_converted);
    ASSERT(r.sectors_converted == 0);

    free(adf); remove(scp); remove(out);
}

TEST(the_flag_recovers_the_back_side_completely)
{
    /* Die Verdrahtung. Dieselbe Datei, ein Schalter — und die Diskette ist
     * vollstaendig da, byteidentisch zur Quelle. */
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);

    char scp[512], out[512];
    get_temp_path(scp, sizeof(scp), "fix.scp");
    get_temp_path(out, sizeof(out), "fix.adf");
    ASSERT(write_scp(scp, adf, true) == 0);

    uft_convert_result_t r;
    ASSERT(convert(scp, out, true, &r) == UFT_OK);
    if (r.sectors_converted != ALL_SECTORS)
        printf("\n        %d von %d Sektoren mit -r\n",
               r.sectors_converted, ALL_SECTORS);
    ASSERT(r.sectors_converted == ALL_SECTORS);
    ASSERT(adf_matches(out, adf));

    free(adf); remove(scp); remove(out);
}

TEST(the_flag_breaks_a_normal_capture)
{
    /* Die Gegenrichtung, und der Grund, warum der Schalter keinen
     * Standardwert bekommt: auf einer normal gelesenen Vorderseite macht er
     * die Diskette unlesbar. */
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);

    char scp[512], out[512];
    get_temp_path(scp, sizeof(scp), "front.scp");
    get_temp_path(out, sizeof(out), "front.adf");
    ASSERT(write_scp(scp, adf, false) == 0);

    uft_convert_result_t ok, bad;
    ASSERT(convert(scp, out, false, &ok) == UFT_OK);
    ASSERT(ok.sectors_converted == ALL_SECTORS);

    ASSERT(convert(scp, out, true, &bad) != UFT_OK);
    ASSERT(bad.sectors_converted == 0);

    free(adf); remove(scp); remove(out);
}

int main(void)
{
    printf("=== Flippy-Rueckseite: rueckwaerts dekodieren (MF-484) ===\n");

    if (uft_register_all_formats() != UFT_OK) {
        printf("FAIL: uft_register_all_formats()\n");
        return 1;
    }

    RUN(reversing_twice_is_the_identity);
    RUN(both_series_stay_ascending_and_the_revolution_survives);
    RUN(a_mirrored_capture_decodes_to_nothing_without_the_flag);
    RUN(the_flag_recovers_the_back_side_completely);
    RUN(the_flag_breaks_a_normal_capture);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
