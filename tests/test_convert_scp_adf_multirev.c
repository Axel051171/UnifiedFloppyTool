/**
 * @file test_convert_scp_adf_multirev.c
 * @brief SCP -> ADF: alle Umdrehungen zaehlen, und was sie ueber den Sektor
 *        aussagen (MF-473).
 *
 * Eine SCP-Aufnahme enthaelt bis zu fuenf Umdrehungen derselben Spur. Der
 * Wandler benutzte davon die erste; das SCP-Plugin sagt es in seinem eigenen
 * Kommentar ("Fuer bessere Ergebnisse koennte man alle Revolutions
 * kombinieren"). Alles nach der ersten Umdrehung war damit gemessene,
 * gespeicherte Information ohne jede Wirkung.
 *
 * Parallel dazu lag seit MF-473 eine Klassifikation im Baum
 * (`multiread_class_t` in src/recovery/uft_multiread_pipeline.c) — mit Test,
 * ohne einen einzigen Aufrufer in src/. Dieser Test prueft beides in einem
 * Zug, weil es dieselbe Sache ist: mehrere Lesungen desselben Sektors
 * auszuwerten heisst, sagen zu koennen WIE sie sich zueinander verhalten.
 *
 * ── Warum synthetischer Flux und kein Korpus ─────────────────────────────
 *
 * tests/test_convert_scp_adf.c deckt denselben Pfad gegen eine echte
 * Greaseweazle-Aufnahme ab — und SKIPt in CI, weil die Datei 32 MB gross und
 * gitignored ist. Ein Test, der nur lokal laeuft, schuetzt die Verdrahtung
 * nicht. Also wird hier eine AmigaDOS-Spur erzeugt.
 *
 * Der Kodierer unten ist die exakte Umkehrung von `decode_amiga_sector()`
 * (src/flux/uft_flux_decoder.c:1204) und pruefte sich selbst: laeuft
 * `the_encoder_produces_a_track_the_decoder_reads` gruen, stimmt er — der
 * Dekoder ist gegen die echte Aufnahme belegt (MF-438). Ein Kodierer, den der
 * belegte Dekoder liest, ist damit kein zweites unbelegtes Stueck Code.
 *
 * ── Die Rotproben ────────────────────────────────────────────────────────
 *
 *   1. Umdrehung 0 traegt einen kaputten Sektor, 1 und 2 nicht. Nur wer alle
 *      drei liest, bekommt die Diskette vollstaendig. Mit der alten Fassung
 *      (nur Umdrehung 0) fehlt genau dieser eine Sektor.
 *   2. Derselbe Sektor ist in ALLEN Umdrehungen gleich falsch. Das ist kein
 *      Schaden, sondern ein absichtlich falsch aufgezeichneter Kopierschutz;
 *      er darf nicht als weak gemeldet werden und nochmal lesen hilft nicht.
 *   3. Der Sektor liest sich jedes Mal anders — das ist weak.
 */

#include "uft/uft_format_convert.h"
#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/formats/uft_scp_writer.h"
#include "uft/flux/uft_flux_decoder.h"
#include "flux_gen.h"   /* tests/flux_gen/amigados */

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

/* Der AmigaDOS-Kodierer liegt seit MF-480 unter tests/flux_gen/amigados/,
 * weil test_convert_cell_adjust.c ihn ebenfalls braucht. Der Selbsttest
 * `the_encoder_produces_a_track_the_decoder_reads` unten ist weiterhin die
 * einzige Begruendung, warum man ihm glauben darf. */

#define ADF_SPT        UFT_AMIGADOS_SPT
#define ADF_SECSZ      UFT_AMIGADOS_SECSZ
#define ADF_SIZE       UFT_AMIGADOS_ADF_SIZE
#define AMIGA_CELL_NS  UFT_AMIGADOS_CELL_NS
#define AMIGA_CELLS_PER_REV  UFT_AMIGADOS_CELLS_PER_REV
#define AMIGA_REV_NS   UFT_AMIGADOS_REV_NS

typedef uft_amigados_defect_t sector_defect_t;
typedef uft_amigados_cells_t  cellbuf_t;

static void build_track_cells(cellbuf_t *c, const uint8_t *adf, uint8_t track,
                              const sector_defect_t *defect)
{
    uft_amigados_build_track(c, adf, track, defect);
}

static size_t cells_to_intervals(const cellbuf_t *c, uint32_t *out, size_t cap)
{
    return uft_amigados_cells_to_intervals(c, AMIGA_CELL_NS, out, cap);
}

/* ──────────────────────────────────────────────────────────────────────────
 * Testgeruest
 * ────────────────────────────────────────────────────────────────────────*/

static void get_temp_path(char *path, size_t size, const char *tag)
{
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(path, size, "%s/uft_mrev_%s_%d", dir, tag, rand() % 100000);
}

/* Zwei Spuren (C0H0, C0H1) mit erkennbarem, nicht konstantem Inhalt. */
#define NTRACKS 2
static uint8_t *make_source_adf(void)
{
    uint8_t *adf = (uint8_t *)calloc(1, ADF_SIZE);
    if (!adf) return NULL;
    uft_amigados_fill_pattern(adf, (size_t)NTRACKS * ADF_SPT * ADF_SECSZ);
    return adf;
}

#define MAX_INTERVALS  AMIGA_CELLS_PER_REV

/**
 * Eine SCP-Datei mit @p nrevs Umdrehungen je Spur schreiben.
 *
 * @param defect_per_rev  Defekt je Umdrehung (NULL-Eintrag = saubere
 *                        Umdrehung). Nur Spur 0 bekommt Defekte; Spur 1
 *                        bleibt sauber und dient als Gegenprobe.
 */
static int write_scp_warped(const char *path, const uint8_t *adf, int nrevs,
                            const sector_defect_t *const *defect_per_rev,
                            double warp_factor, double warp_fraction);

static int write_scp(const char *path, const uint8_t *adf, int nrevs,
                     const sector_defect_t *const *defect_per_rev)
{
    return write_scp_warped(path, adf, nrevs, defect_per_rev, 1.0, 0.0);
}

/**
 * Wie @ref write_scp, aber die Spur laeuft ungleich schnell.
 *
 * @param warp_factor    Faktor, um den der Anfang gedehnt wird (1,0 = kein
 *                       Gleichlauffehler)
 * @param warp_fraction  Anteil der Spur, der gedehnt wird
 *
 * Gebraucht, um die Sperre zu pruefen, die eine zu ungenaue Winkelangabe
 * gar nicht erst einordnet (MF-502). Ohne eine solche Spur waere dieser
 * Zweig ungeprueft — und ein ungeprueftes Sicherheitsnetz ist keines.
 */
static int write_scp_warped(const char *path, const uint8_t *adf, int nrevs,
                            const sector_defect_t *const *defect_per_rev,
                            double warp_factor, double warp_fraction)
{
    uint8_t *cellbits = (uint8_t *)calloc((AMIGA_CELLS_PER_REV + 7) / 8 + 1, 1);
    uint32_t *iv = (uint32_t *)malloc(MAX_INTERVALS * sizeof(uint32_t));
    if (!cellbits || !iv) { free(cellbits); free(iv); return -1; }

    scp_writer_t *w = scp_writer_create(SCP_TYPE_AMIGA, (uint8_t)nrevs);
    if (!w) { free(cellbits); free(iv); return -1; }

    int rc = 0;
    for (int track = 0; track < NTRACKS && rc == 0; track++) {
        for (int r = 0; r < nrevs && rc == 0; r++) {
            cellbuf_t c = { cellbits, AMIGA_CELLS_PER_REV, 0, 0 };
            const sector_defect_t *d =
                (track == 0 && defect_per_rev) ? defect_per_rev[r] : NULL;
            build_track_cells(&c, adf, (uint8_t)track, d);

            size_t n = cells_to_intervals(&c, iv, MAX_INTERVALS);
            if (warp_factor > 1.0 && warp_fraction > 0.0) {
                size_t lim = (size_t)((double)n * warp_fraction);
                for (size_t i = 0; i < lim; i++)
                    iv[i] = (uint32_t)((double)iv[i] * warp_factor);
            }
            rc = scp_writer_add_track(w, track / 2, track % 2, iv, n,
                                      AMIGA_REV_NS, r);
        }
    }
    if (rc == 0) rc = scp_writer_save(w, path);

    scp_writer_free(w);
    free(cellbits);
    free(iv);
    return rc;
}

/* SCP -> ADF ueber die oeffentliche Wandler-API. */
static uft_error_t convert(const char *scp, const char *adf_out,
                           bool use_multiple_revs, uft_convert_result_t *res)
{
    uft_convert_options_t o = uft_convert_default_options();
    o.use_multiple_revs = use_multiple_revs;
    /* Flux -> Sektor verliert Timing und Weak-Bits; der Preflight verlangt
     * dafuer ausdrueckliche Zustimmung (UFT-A05). Hier ist sie gegeben. */
    o.accept_data_loss = true;
    memset(res, 0, sizeof(*res));
    uft_error_t rc = uft_convert_file(scp, adf_out, UFT_FORMAT_ADF, &o, res);
    if (rc != UFT_OK) {
        printf("\n        convert rc=%d error=%d\n", (int)rc, (int)res->error);
        for (int i = 0; i < res->warning_count; i++)
            printf("        warn: %s\n", res->warnings[i]);
    }
    return rc;
}

static uint8_t *slurp(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return NULL; }
    uint8_t *b = (uint8_t *)malloc((size_t)n);
    if (!b) { fclose(f); return NULL; }
    *len = fread(b, 1, (size_t)n, f);
    fclose(f);
    return b;
}

static bool warned_about(const uft_convert_result_t *r, const char *needle)
{
    for (int i = 0; i < r->warning_count; i++)
        if (strstr(r->warnings[i], needle)) return true;
    return false;
}

static void dump_warnings(const uft_convert_result_t *r)
{
    for (int i = 0; i < r->warning_count; i++)
        printf("\n        warn: %s", r->warnings[i]);
    printf("\n");
}

/* Wie viele der NTRACKS x 11 Sektoren stimmen mit der Quelle ueberein? */
static int sectors_matching(const uint8_t *want, const uint8_t *got)
{
    int ok = 0;
    for (int i = 0; i < NTRACKS * ADF_SPT; i++) {
        size_t off = (size_t)i * ADF_SECSZ;
        if (memcmp(want + off, got + off, ADF_SECSZ) == 0) ok++;
    }
    return ok;
}

/* ──────────────────────────────────────────────────────────────────────────
 * Tests
 * ────────────────────────────────────────────────────────────────────────*/

TEST(the_encoder_produces_a_track_the_decoder_reads) {
    /* Selbstpruefung des Kodierers: was er schreibt, muss der belegte
     * AmigaDOS-Dekoder als 11 saubere Sektoren zurueckgeben. Faellt dieser
     * Test, sagen alle folgenden nichts ueber den Wandler aus. */
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);

    uint8_t *cellbits = (uint8_t *)calloc((AMIGA_CELLS_PER_REV + 7) / 8 + 1, 1);
    uint32_t *iv = (uint32_t *)malloc(MAX_INTERVALS * sizeof(uint32_t));
    ASSERT(cellbits != NULL && iv != NULL);

    cellbuf_t c = { cellbits, AMIGA_CELLS_PER_REV, 0, 0 };
    build_track_cells(&c, adf, 0, NULL);
    ASSERT(c.n > 90000);                       /* eine volle Umdrehung */

    size_t n = cells_to_intervals(&c, iv, MAX_INTERVALS);
    ASSERT(n > 20000);

    flux_raw_data_t raw;
    ASSERT(flux_raw_from_ns_intervals_indexed(iv, n, AMIGA_REV_NS, &raw)
           == FLUX_OK);
    ASSERT(raw.index_count == 2);              /* die Messung kam an */

    flux_decoder_options_t o;
    flux_decoder_options_init(&o);
    o.media = UFT_MEDIA_AMIGA_DD;

    flux_decoded_track_t dt;
    memset(&dt, 0, sizeof(dt));
    ASSERT(flux_decode_amiga(&raw, &dt, &o) == FLUX_OK);
    if (dt.sector_count != ADF_SPT)
        printf("\n        %zu Sektoren, %zu mit falscher Datenpruefsumme\n",
               (size_t)dt.sector_count, (size_t)dt.bad_data_crc);
    ASSERT(dt.sector_count == ADF_SPT);
    ASSERT(dt.bad_data_crc == 0);
    ASSERT(dt.bad_id_crc == 0);

    for (size_t s = 0; s < dt.sector_count; s++) {
        const flux_decoded_sector_t *sec = &dt.sectors[s];
        ASSERT(sec->sector < ADF_SPT);
        ASSERT(sec->data_size == ADF_SECSZ);
        ASSERT(memcmp(sec->data, adf + (size_t)sec->sector * ADF_SECSZ,
                      ADF_SECSZ) == 0);
    }

    flux_decoded_track_free(&dt);
    flux_raw_free(&raw);
    free(cellbits); free(iv); free(adf);
}

TEST(a_clean_capture_converts_to_the_source_adf) {
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);

    char scp[512], out[512];
    get_temp_path(scp, sizeof(scp), "clean.scp");
    get_temp_path(out, sizeof(out), "clean.adf");
    ASSERT(write_scp(scp, adf, 3, NULL) == 0);

    uft_convert_result_t r;
    ASSERT(convert(scp, out, true, &r) == UFT_OK);

    size_t got_len = 0;
    uint8_t *got = slurp(out, &got_len);
    ASSERT(got != NULL);
    ASSERT(got_len == ADF_SIZE);
    ASSERT(sectors_matching(adf, got) == NTRACKS * ADF_SPT);

    free(got); free(adf);
    remove(scp); remove(out);
}

TEST(a_single_revolution_of_a_non_index_synced_medium_is_flagged) {
    /* MF-483. AmigaDOS bindet die Sektorlage nicht an die Indexmarke — die
     * Spur wird am Stueck geschrieben, wo der Kopf gerade steht. Ein
     * Ein-Umdrehungs-Abbild schneidet einen Sektor, der ueber den Index
     * laeuft, mitten durch, und zwar UNAUFFAELLIG: es fehlt einfach einer, so
     * wie bei einem Lesefehler.
     *
     * Der Wandler muss das sagen. a8rawconv tut es fuer Atari fest
     * verdrahtet (rawdiskscp.cpp:120-124); hier kommt die Eigenschaft aus dem
     * Medienprofil, damit sie fuer jedes Format stimmt und nicht fuer eines. */
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);

    char scp[512], out[512];
    get_temp_path(scp, sizeof(scp), "onerev.scp");
    get_temp_path(out, sizeof(out), "onerev.adf");
    ASSERT(write_scp(scp, adf, 1, NULL) == 0);

    uft_convert_result_t r;
    ASSERT(convert(scp, out, true, &r) == UFT_OK);

    if (!warned_about(&r, "nicht indexsynchron")) dump_warnings(&r);
    ASSERT(warned_about(&r, "nicht indexsynchron"));
    ASSERT(warned_about(&r, "koennen fehlen"));

    /* Die Warnung ersetzt die Daten nicht: was lesbar war, ist da. */
    size_t got_len = 0;
    uint8_t *got = slurp(out, &got_len);
    ASSERT(got != NULL && got_len == ADF_SIZE);
    ASSERT(sectors_matching(adf, got) == NTRACKS * ADF_SPT);
    free(got);

    free(adf);
    remove(scp); remove(out);
}

TEST(enough_revolutions_says_nothing_about_index_sync) {
    /* Gegenprobe. Ohne sie wuerde eine Warnung, die IMMER erscheint, genauso
     * gruen leuchten wie eine, die nur im richtigen Fall kommt. */
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);

    char scp[512], out[512];
    get_temp_path(scp, sizeof(scp), "tworev.scp");
    get_temp_path(out, sizeof(out), "tworev.adf");
    ASSERT(write_scp(scp, adf, 2, NULL) == 0);

    uft_convert_result_t r;
    ASSERT(convert(scp, out, true, &r) == UFT_OK);
    if (warned_about(&r, "nicht indexsynchron")) dump_warnings(&r);
    ASSERT(!warned_about(&r, "nicht indexsynchron"));

    free(adf);
    remove(scp); remove(out);
}

TEST(a_sector_broken_in_revolution_zero_is_recovered_from_the_others) {
    /* DIE Rotprobe fuer die Verdrahtung. Umdrehung 0 traegt einen Sektor mit
     * falscher Datenpruefsumme und veraendertem Inhalt, die Umdrehungen 1 und
     * 2 sind sauber. Wer nur Umdrehung 0 liest, verliert diesen Sektor. */
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);

    sector_defect_t d = { 3, true, 0x5A };
    const sector_defect_t *per_rev[3] = { &d, NULL, NULL };

    char scp[512], out[512];
    get_temp_path(scp, sizeof(scp), "rev0.scp");
    get_temp_path(out, sizeof(out), "rev0.adf");
    ASSERT(write_scp(scp, adf, 3, per_rev) == 0);

    /* Mit allen Umdrehungen: vollstaendig. */
    uft_convert_result_t r;
    ASSERT(convert(scp, out, true, &r) == UFT_OK);
    size_t got_len = 0;
    uint8_t *got = slurp(out, &got_len);
    ASSERT(got != NULL && got_len == ADF_SIZE);
    int matched = sectors_matching(adf, got);
    if (matched != NTRACKS * ADF_SPT) {
        printf("\n        %d von %d Sektoren stimmen\n",
               matched, NTRACKS * ADF_SPT);
        dump_warnings(&r);
    }
    ASSERT(matched == NTRACKS * ADF_SPT);
    ASSERT(r.sectors_converted == NTRACKS * ADF_SPT);
    free(got);

    /* Mit `use_multiple_revs = false` ausdruecklich nur die erste: der
     * Sektor fehlt. Das ist zugleich der Beleg, dass der Unterschied wirklich
     * von den weiteren Umdrehungen kommt und nicht von etwas anderem. */
    uft_convert_result_t r1;
    ASSERT(convert(scp, out, false, &r1) == UFT_OK);
    got = slurp(out, &got_len);
    ASSERT(got != NULL && got_len == ADF_SIZE);
    ASSERT(sectors_matching(adf, got) == NTRACKS * ADF_SPT - 1);
    ASSERT(r1.sectors_converted == NTRACKS * ADF_SPT - 1);
    free(got);

    free(adf);
    remove(scp); remove(out);
}

TEST(a_stable_crc_error_is_reported_as_copy_protection) {
    /* Derselbe Sektor, in jeder Umdrehung mit derselben falschen
     * Pruefsumme — ein absichtlich falsch aufgezeichneter Sektor. Er darf
     * nicht ins ADF, er ist nicht weak, und die Meldung muss sagen, dass
     * weiteres Lesen nichts aendert. */
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);

    sector_defect_t d = { 5, true, 0 };
    const sector_defect_t *per_rev[3] = { &d, &d, &d };

    char scp[512], out[512];
    get_temp_path(scp, sizeof(scp), "stable.scp");
    get_temp_path(out, sizeof(out), "stable.adf");
    ASSERT(write_scp(scp, adf, 3, per_rev) == 0);

    uft_convert_result_t r;
    ASSERT(convert(scp, out, true, &r) == UFT_OK);

    size_t got_len = 0;
    uint8_t *got = slurp(out, &got_len);
    ASSERT(got != NULL && got_len == ADF_SIZE);

    /* Genau ein Sektor fehlt, und zwar Nummer 5 der Spur 0. */
    ASSERT(sectors_matching(adf, got) == NTRACKS * ADF_SPT - 1);
    ASSERT(memcmp(got + 5 * ADF_SECSZ, adf + 5 * ADF_SECSZ, ADF_SECSZ) != 0);
    ASSERT(r.sectors_converted == NTRACKS * ADF_SPT - 1);

    if (!warned_about(&r, "stabiler CRC-Fehler")) dump_warnings(&r);
    ASSERT(warned_about(&r, "stabiler CRC-Fehler"));
    ASSERT(warned_about(&r, "Kopierschutz"));
    ASSERT(!warned_about(&r, "Sektoren weak"));

    free(got); free(adf);
    remove(scp); remove(out);
}

TEST(a_sector_that_reads_differently_each_time_is_reported_as_weak) {
    /* Drei Umdrehungen, drei verschiedene Inhalte, jedes Mal falsche
     * Pruefsumme. Kein Kopierschutz — ein wackelnder Sektor. */
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);

    sector_defect_t d0 = { 7, true, 0x11 };
    sector_defect_t d1 = { 7, true, 0x22 };
    sector_defect_t d2 = { 7, true, 0x33 };
    const sector_defect_t *per_rev[3] = { &d0, &d1, &d2 };

    char scp[512], out[512];
    get_temp_path(scp, sizeof(scp), "weak.scp");
    get_temp_path(out, sizeof(out), "weak.adf");
    ASSERT(write_scp(scp, adf, 3, per_rev) == 0);

    uft_convert_result_t r;
    ASSERT(convert(scp, out, true, &r) == UFT_OK);

    size_t got_len = 0;
    uint8_t *got = slurp(out, &got_len);
    ASSERT(got != NULL && got_len == ADF_SIZE);
    ASSERT(sectors_matching(adf, got) == NTRACKS * ADF_SPT - 1);

    if (!warned_about(&r, "Sektoren weak")) dump_warnings(&r);
    ASSERT(warned_about(&r, "Sektoren weak"));
    ASSERT(!warned_about(&r, "stabiler CRC-Fehler"));

    free(got); free(adf);
    remove(scp); remove(out);
}

/* ------------------------------------------------------------------ */
/* Wo der Schaden auf der Umdrehung sitzt (MF-501)                     */
/* ------------------------------------------------------------------ */

TEST(damage_is_reported_with_its_place_on_the_revolution)
{
    /* Der Decoder wusste seit jeher, WO ein Sektor lag (`id_position`),
     * und niemand las es: die einzige Funktion, die es auswertete
     * (`uft_otdr_adaptive_decode`), hat keinen Aufrufer. Seit MF-501 wird
     * daraus eine Winkellage — die Datenquelle, die der Polarkarte
     * fehlte.
     *
     * Hier ist Sektor 3 in ALLEN Umdrehungen kaputt, also nicht durch
     * Mehrfachlesung zu retten. Der Bericht muss den Schaden nennen UND
     * verorten. */
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);

    sector_defect_t d = { 3, true, 0x5A };
    const sector_defect_t *per_rev[3] = { &d, &d, &d };

    char scp[512], out[512];
    get_temp_path(scp, sizeof(scp), "angle.scp");
    get_temp_path(out, sizeof(out), "angle.adf");
    ASSERT(write_scp(scp, adf, 3, per_rev) == 0);

    uft_convert_result_t r;
    ASSERT(convert(scp, out, true, &r) == UFT_OK);

    if (!warned_about(&r, "Schadenslage")) {
        printf("\n        keine Schadenslage gemeldet:\n");
        dump_warnings(&r);
    }
    ASSERT(warned_about(&r, "Schadenslage"));
    /* Die Meldung muss die Umdrehung in Achtel teilen und die Zahl der
     * betroffenen Spuren nennen — sonst ist sie eine Behauptung ohne
     * Groessenordnung. */
    ASSERT(warned_about(&r, "Achtel"));

    remove(scp); remove(out); free(adf);
}

TEST(a_clean_disk_gets_no_damage_location)
{
    /* Gegenprobe. Ein Werkzeug, das auch bei heilen Disketten eine
     * Schadenslage meldet, hat keine gemessen, sondern eine Zeile
     * ausgegeben. */
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);

    char scp[512], out[512];
    get_temp_path(scp, sizeof(scp), "noangle.scp");
    get_temp_path(out, sizeof(out), "noangle.adf");
    ASSERT(write_scp(scp, adf, 3, NULL) == 0);

    uft_convert_result_t r;
    ASSERT(convert(scp, out, true, &r) == UFT_OK);
    if (warned_about(&r, "Schadenslage")) {
        printf("\n        Schadenslage bei heiler Diskette:\n");
        dump_warnings(&r);
    }
    ASSERT(!warned_about(&r, "Schadenslage"));
    ASSERT(!warned_about(&r, "ihre Lage auf der Umdrehung ist unbekannt"));

    remove(scp); remove(out); free(adf);
}

TEST(a_track_that_ran_unevenly_gets_no_angle_it_cannot_support)
{
    /* Die Sperre aus MF-502. Der Winkel entsteht aus Bitindex mal
     * konstanter Zellendauer; wo die Spur ungleich schnell lief, stimmt
     * das nicht mehr. Gemessen gegen die wahre Lage: bei einer Spanne von
     * 1,153 liegt der Winkel schon 35 Grad daneben — ein Achtel misst 45.
     *
     * Verlangt wird deshalb NICHT, dass der Wandler die Lage angibt,
     * sondern dass er sie ausdruecklich verweigert und den Grund nennt.
     * Eine Verteilung aus falschen Faechern waere schlechter als keine. */
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);

    sector_defect_t d = { 3, true, 0x5A };
    const sector_defect_t *per_rev[3] = { &d, &d, &d };

    char scp[512], out[512];
    get_temp_path(scp, sizeof(scp), "warp.scp");
    get_temp_path(out, sizeof(out), "warp.adf");
    /* Erste Haelfte um 60 % gedehnt — weit ueber der Schwelle. */
    ASSERT(write_scp_warped(scp, adf, 3, per_rev, 1.6, 0.5) == 0);

    uft_convert_result_t r;
    ASSERT(convert(scp, out, true, &r) == UFT_OK);

    if (!warned_about(&r, "ungleich schnell")) {
        printf("\n        keine Verweigerung der Winkelangabe:\n");
        dump_warnings(&r);
    }
    ASSERT(warned_about(&r, "ungleich schnell"));
    /* Und ausdruecklich KEINE Verteilung, die es nicht tragen kann. */
    ASSERT(!warned_about(&r, "Schadenslage"));

    remove(scp); remove(out); free(adf);
}

int main(void)
{
    printf("=== SCP -> ADF: alle Umdrehungen, und was sie sagen (MF-473) ===\n");

    /* Ohne Registrierung ist die Plugin-Registry leer und uft_convert_file()
     * erkennt die SCP-Datei nicht (MF-447). */
    if (uft_register_all_formats() != UFT_OK) {
        printf("FAIL: uft_register_all_formats()\n");
        return 1;
    }

    RUN(the_encoder_produces_a_track_the_decoder_reads);
    RUN(a_clean_capture_converts_to_the_source_adf);
    RUN(a_single_revolution_of_a_non_index_synced_medium_is_flagged);
    RUN(enough_revolutions_says_nothing_about_index_sync);
    RUN(a_sector_broken_in_revolution_zero_is_recovered_from_the_others);
    RUN(a_stable_crc_error_is_reported_as_copy_protection);
    RUN(a_sector_that_reads_differently_each_time_is_reported_as_weak);

    /* MF-501: die Ortsangabe erreicht den Menschen */
    RUN(damage_is_reported_with_its_place_on_the_revolution);
    RUN(a_clean_disk_gets_no_damage_location);
    RUN(a_track_that_ran_unevenly_gets_no_angle_it_cannot_support);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
