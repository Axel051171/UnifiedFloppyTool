/**
 * @file test_scp_readers_agree.c
 * @brief The SCP readers must prove they agree (MF-439, MF-440).
 *
 * MF-438 found that `uft_scp_plugin.c` read the revolution entry's `length`
 * field as a byte count and derived `length / 2` flux values from it, so every
 * SCP track came back as exactly half a revolution. Silently: sectors still
 * decoded, five of eleven instead of eleven.
 *
 * That raised the obvious question — how many of the other four make the same
 * mistake? All four were read:
 *
 *     src/flux/uft_scp_parser.c:376,518        length = flux values   correct
 *     src/formats/scp/uft_scp_multirev.c:670   length = flux values   correct
 *     src/formats/scp/uft_scp_parser_v3.c:1205 length * 2 bytes       correct
 *     src/formats/scp/uft_scp_reader_v2.c:806  length * 2 bytes       correct
 *     src/formats/scp/uft_scp_plugin.c:116     length / 2             WRONG
 *
 * One of five. MF-440 then removed two of them — uft_scp_multirev.c and
 * uft_scp_reader_v2.c had no callers at all, 1850 lines between them — so
 * three remain: the canonical parser, the v3 parser reached through
 * uft_v3_bridge for protection detection, and the plugin.
 *
 * Reading is not proof, and three readers of one format is still a standing
 * invitation for the next one to drift. This turns the reading into an
 * assertion: two independent readers, the same file, the same track — and
 * every flux interval identical.
 *
 * The check is deliberately not "both return something plausible". Two
 * readers that both truncate at half a revolution would satisfy that. It is
 * value-for-value equality, plus an anchor that neither can fake: the
 * intervals must sum to the revolution duration the track itself declares.
 *
 * SKIPS with exit 77 without the 32 MB gitignored corpus SCP.
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"
#include "uft/flux/uft_scp_parser.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef UFT_CORPUS_RESTRICTED_DIR
#error "UFT_CORPUS_RESTRICTED_DIR must be defined by the build"
#endif

extern const uft_format_plugin_t uft_format_plugin_scp;

#define SKIP_EXIT 77
#define SCP_IMAGE "gw_amigados.scp"

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-46s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

static const char *scp_path(void)
{
    static char p[512];
    snprintf(p, sizeof(p), "%s/%s", UFT_CORPUS_RESTRICTED_DIR, SCP_IMAGE);
    return p;
}

/* Tracks sampled across the disk rather than only track 0: a length bug that
 * happened to be harmless on the first track would still be caught. */
static const int SAMPLE_TRACKS[] = { 0, 1, 40, 79, 158, 159 };
#define N_SAMPLES ((int)(sizeof(SAMPLE_TRACKS) / sizeof(SAMPLE_TRACKS[0])))

TEST(plugin_and_parser_return_identical_flux) {
    uft_scp_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ASSERT(uft_scp_open(&ctx, scp_path()) == UFT_SCP_OK);

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = true;
    ASSERT(uft_format_plugin_scp.open(&disk, scp_path(), true) == UFT_OK);

    int compared = 0;
    for (int i = 0; i < N_SAMPLES; i++) {
        int scp_track = SAMPLE_TRACKS[i];
        int cyl = scp_track / 2, head = scp_track % 2;

        uft_scp_track_data_t td;
        memset(&td, 0, sizeof(td));
        if (uft_scp_read_track(&ctx, scp_track, &td) != UFT_SCP_OK) continue;
        if (td.revolution_count == 0) { uft_scp_free_track(&td); continue; }

        uft_track_t pt;
        memset(&pt, 0, sizeof(pt));
        if (uft_format_plugin_scp.read_track(&disk, cyl, head, &pt) != UFT_OK) {
            uft_scp_free_track(&td);
            uft_track_release(&pt);
            continue;
        }

        /* The plugin exposes revolution 0; compare against the same one. */
        const uft_scp_rev_data_t *rev = &td.revolutions[0];
        ASSERT(pt.flux != NULL);
        ASSERT(rev->flux_data != NULL);

        /* Equal counts first: this is where MF-438 showed up as a factor
         * of two. */
        ASSERT(pt.flux_count == rev->flux_count);

        /* And equal values — a reader could get the count right and still
         * mis-assemble the big-endian words or the overflow handling. */
        size_t differing = 0;
        for (size_t k = 0; k < pt.flux_count; k++)
            if (pt.flux[k] != rev->flux_data[k]) differing++;
        if (differing) {
            printf("\n        track %d: %zu of %zu intervals differ\n",
                   scp_track, differing, pt.flux_count);
        }
        ASSERT(differing == 0);

        compared++;
        uft_scp_free_track(&td);
        uft_track_release(&pt);
    }

    ASSERT(compared == N_SAMPLES);   /* every sampled track really compared */

    uft_format_plugin_scp.close(&disk);
    uft_scp_close(&ctx);
}

TEST(the_flux_spans_a_whole_revolution_on_every_sampled_track) {
    /* The anchor neither reader can fake. A track carries its own
     * index-to-index duration, so "did we get all of it" needs no external
     * reference — the intervals must add up to the time the disk took to turn
     * once. Half a revolution, the MF-438 symptom, fails here immediately.
     *
     * Checked per track rather than once, because a truncation that depends
     * on track length would otherwise slip through on the shortest one. */
    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = true;
    ASSERT(uft_format_plugin_scp.open(&disk, scp_path(), true) == UFT_OK);

    int checked = 0;
    for (int i = 0; i < N_SAMPLES; i++) {
        int cyl = SAMPLE_TRACKS[i] / 2, head = SAMPLE_TRACKS[i] % 2;
        uft_track_t pt;
        memset(&pt, 0, sizeof(pt));
        if (uft_format_plugin_scp.read_track(&disk, cyl, head, &pt) != UFT_OK ||
            !pt.flux) { uft_track_release(&pt); continue; }

        uint64_t sum = 0;
        for (size_t k = 0; k < pt.flux_count; k++) sum += pt.flux[k];

        double turn = pt.metrics.index_time_ns;
        ASSERT(turn > 190e6 && turn < 210e6);          /* ~300 rpm */
        double ratio = (double)sum / turn;
        if (ratio < 0.99 || ratio > 1.01) {
            printf("\n        track %d: flux spans %.2f ms of %.2f ms (%.0f %%)\n",
                   SAMPLE_TRACKS[i], sum / 1e6, turn / 1e6, ratio * 100.0);
        }
        ASSERT(ratio > 0.99 && ratio < 1.01);

        checked++;
        uft_track_release(&pt);
    }
    ASSERT(checked == N_SAMPLES);

    uft_format_plugin_scp.close(&disk);
}

TEST(the_memory_path_agrees_with_the_file_path) {
    /* uft_scp_parser.c has two independent implementations of the same read —
     * one over a FILE*, one over a memory buffer (uft_scp_read_track_memory).
     * Both had the length field right, so this is a guard rather than a
     * discovery: two code paths, one format, one answer. */
    FILE *f = fopen(scp_path(), "rb");
    ASSERT(f != NULL);
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    ASSERT(n > 0);
    uint8_t *buf = (uint8_t *)malloc((size_t)n);
    ASSERT(buf != NULL);
    ASSERT(fread(buf, 1, (size_t)n, f) == (size_t)n);
    fclose(f);

    uft_scp_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ASSERT(uft_scp_open(&ctx, scp_path()) == UFT_SCP_OK);

    int compared = 0;
    for (int i = 0; i < N_SAMPLES; i++) {
        int trk = SAMPLE_TRACKS[i];

        uft_scp_track_data_t a, b;
        memset(&a, 0, sizeof(a));
        memset(&b, 0, sizeof(b));
        if (uft_scp_read_track(&ctx, trk, &a) != UFT_SCP_OK) continue;
        if (uft_scp_read_track_memory(buf, (size_t)n, trk, &b) != UFT_SCP_OK) {
            uft_scp_free_track(&a);
            continue;
        }
        ASSERT(a.revolution_count == b.revolution_count);
        ASSERT(a.revolutions[0].flux_count == b.revolutions[0].flux_count);
        ASSERT(memcmp(a.revolutions[0].flux_data, b.revolutions[0].flux_data,
                      a.revolutions[0].flux_count * sizeof(uint32_t)) == 0);
        compared++;
        uft_scp_free_track(&a);
        uft_scp_free_track(&b);
    }
    ASSERT(compared == N_SAMPLES);

    uft_scp_close(&ctx);
    free(buf);
}

int main(void)
{
    printf("=== Five SCP readers, one answer (MF-439) ===\n");

    FILE *probe = fopen(scp_path(), "rb");
    if (!probe) {
        printf("SKIP: local corpus image absent (%s)\n"
               "      32 MB, gitignored; reproducible from the tracked ADF,\n"
               "      see tests/corpus_manifest/manifest.json\n", scp_path());
        return SKIP_EXIT;
    }
    fclose(probe);

    RUN(plugin_and_parser_return_identical_flux);
    RUN(the_flux_spans_a_whole_revolution_on_every_sampled_track);
    RUN(the_memory_path_agrees_with_the_file_path);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
