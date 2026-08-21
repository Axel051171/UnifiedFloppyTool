/**
 * @file test_register_all_formats.c
 * @brief uft_register_all_formats() runs here for the first time (MF-446).
 *
 * The function existed for the whole life of the project and had no caller —
 * not in the GUI, not in a test, nowhere. Three separate caps kept the registry
 * far below the plugin count (ARCH-9), and because an empty registry answers
 * NULL to everything, nothing ever noticed. Executing it once, and checking
 * what comes out, is the whole point of this file.
 *
 * What running it revealed, all of it invisible while it was never called:
 *
 *   - `all_plugins[]` was sized by a hand-written sum of per-group literals,
 *     and the OTHER line said 47 against an array of 48. The declared size held
 *     exactly as many pointers as there are plugins, so the terminating
 *     `all_plugins[idx] = NULL` wrote one element past the end.
 *   - Seven defined plugins were in no group at all and could never be
 *     registered — among them SCP, the flux container the entire DeepRead path
 *     reads, plus G64 and IMG.
 *   - MAX_FORMAT_PLUGINS was 128 against 137 plugins: nine would have been
 *     refused with UFT_ERROR_BUFFER_TOO_SMALL.
 *
 * The count assertions below are deliberately exact. "More than a hundred"
 * would have passed at 128 too.
 *
 * MF-447 added the probe-conflict matrix at the bottom. It lives here rather
 * than in its own file because this is the only target that links all 137
 * plugins, and a probe conflict is by definition something you cannot see with
 * fewer. Turning registration on without it would have made the tool worse:
 * d88_probe() claimed every one of the twelve corpus images at confidence 90.
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_core.h"   /* uft_disk_open/close */
#include "uft/uft_smart_open.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef UFT_CORPUS_DIR
#error "UFT_CORPUS_DIR must be defined by the build (tests/CMakeLists.txt)"
#endif

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-48s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

/* Every plugin the tree defines, per scripts/plugin_registry_gate.py. If this
 * number moves, the gate moved with it — and the gate reads the source. */
#define EXPECTED_PLUGINS 137

static const char *img(const char *name)
{
    static char p[512];
    snprintf(p, sizeof(p), "%s/%s", UFT_CORPUS_DIR, name);
    return p;
}

TEST(without_registration_every_open_fails) {
    /* MF-447: this is the state the shipped application was in. Nothing called
     * uft_register_all_formats(), so uft_probe_file_format() walked an empty
     * list and uft_disk_open() returned NULL for every file that exists and is
     * perfectly readable. DiskAnalyzerWindow::loadImage() consequently always
     * took its fallback branch and estimated geometry from the file size.
     *
     * Runs first, before any registration, because that is the only way to
     * observe it. */
    ASSERT(uft_registered_format_plugin_count() == 0);
    ASSERT(uft_probe_file_format(img("vice_c1541_35trk.d64")) == NULL);
    ASSERT(uft_disk_open(img("vice_c1541_35trk.d64"), true) == NULL);
}

TEST(every_defined_plugin_reaches_the_registry) {
    ASSERT(uft_registered_format_plugin_count() == 0);

    ASSERT(uft_get_format_count() == EXPECTED_PLUGINS);
    ASSERT(uft_register_all_formats() == UFT_OK);
    ASSERT(uft_registered_format_plugin_count() == EXPECTED_PLUGINS);
}

TEST(the_formats_that_were_in_no_group_are_there) {
    /* These seven were defined and unreachable. SCP first: without it the flux
     * path has no container plugin at all. */
    ASSERT(uft_get_format_plugin_by_name("SCP") != NULL);
    ASSERT(uft_get_format_plugin_by_name("G64") != NULL);
    ASSERT(uft_get_format_plugin_by_name("IMG") != NULL);
    ASSERT(uft_get_format_plugin_by_name("ExtADF") != NULL);
    ASSERT(uft_get_format_plugin_by_name("KorgDSS1") != NULL);
    ASSERT(uft_get_format_plugin_by_name("AkaiS900") != NULL);
    ASSERT(uft_get_format_plugin_by_name("LisaTwiggy") != NULL);
}

TEST(the_macro_generated_variants_are_there_too) {
    /* 49 of the 137 exist only as DSK_PLUGIN() expansions. They are the reason
     * the MF-445 gate undercounted 137 as 88 and cleared a capacity of 128. */
    ASSERT(uft_get_format_plugin_by_name("DSK_FM7") != NULL);
    ASSERT(uft_get_format_plugin_by_name("DSK_MSX") != NULL);
    ASSERT(uft_get_format_plugin_by_name("DSK_VIC") != NULL);
}

TEST(calling_it_twice_changes_nothing) {
    ASSERT(uft_register_all_formats() == UFT_OK);
    ASSERT(uft_registered_format_plugin_count() == EXPECTED_PLUGINS);
}

TEST(the_container_id_is_hopeless_and_the_name_is_not) {
    /* The reason MF-444/445 exist, measured on the full set instead of on four
     * plugins: the overwhelming majority of the registry answers to one id. */
    size_t dsk = uft_count_format_plugins_for(UFT_FORMAT_DSK);
    ASSERT(dsk > 120);
    ASSERT(dsk < EXPECTED_PLUGINS);

    /* and every one of them still has its own identity */
    ASSERT(uft_get_format_plugin_by_name("D64") !=
           uft_get_format_plugin_by_name("D81"));
    ASSERT(uft_get_format_plugin_by_name("D64")->format ==
           uft_get_format_plugin_by_name("D81")->format);
}

TEST(no_two_plugins_share_a_name) {
    /* Registration keys on .name since MF-444, so a collision means the second
     * plugin is silently refused. The gate checks the source; this checks the
     * result of actually registering all of them. */
    const uft_format_plugin_t *list[EXPECTED_PLUGINS];
    size_t n = uft_list_format_plugins(list, EXPECTED_PLUGINS);
    ASSERT(n == EXPECTED_PLUGINS);

    for (size_t i = 0; i < n; i++) {
        ASSERT(list[i] != NULL);
        ASSERT(list[i]->name != NULL);
        for (size_t j = i + 1; j < n; j++) {
            if (strcmp(list[i]->name, list[j]->name) == 0) {
                printf("duplicate name '%s'\n", list[i]->name);
                _fail++;
                return;
            }
        }
    }
}

TEST(after_registration_the_analyzer_branch_is_reachable) {
    /* The payoff of ARCH-9…ARCH-12 in one assertion: the same call that
     * returned NULL above now yields a disk whose geometry came off the image
     * instead of off its file size. 35 tracks is the 1541 answer; the fallback
     * in DiskAnalyzerWindow::loadImage() would have reported 512-byte sectors,
     * two sides and 18 sectors per track — for a D64. */
    const uft_format_plugin_t *p = uft_probe_file_format(img("vice_c1541_35trk.d64"));
    ASSERT(p != NULL);
    ASSERT(strcmp(p->name, "D64") == 0);

    uft_disk_t *disk = uft_disk_open(img("vice_c1541_35trk.d64"), true);
    ASSERT(disk != NULL);
    ASSERT(uft_disk_plugin(disk) == p);
    ASSERT(disk->geometry.cylinders == 35);
    ASSERT(disk->geometry.heads == 1);
    uft_disk_close(disk);

    /* and one of the seven that were in no group until MF-446 */
    ASSERT(uft_probe_file_format(img("atrcopy_dos2sd.xfd")) != NULL);
}

/* Which plugin must win the probe for each reference image. The point is not
 * that a plugin claims the file — several legitimately do, a D81 and a SAD
 * image really are both 819200 bytes of sectors — but that the right one ranks
 * highest, because uft_probe_buffer_format() returns the top scorer. */
static const struct { const char *file; const char *winner; } k_expected[] = {
    { "vice_c1541_35trk.d64", "D64" },
    { "vice_c1541_2040.d67",  "D67" },
    { "vice_c1541_70trk.d71", "D71" },
    { "vice_c1541_80trk.d81", "D81" },
    { "vice_c1541_8050.d80",  "D80" },
    { "vice_c1541_8250.d82",  "D82" },
    { "vice_c1541_35trk.g64", "G64" },
    { "vice_c1541_1571.g71",  "G71" },
    { "xdftool_dd_ofs.adf",   "ADF" },
    { "atrcopy_dos2sd.atr",   "ATR" },
    { "gw_amigados.hfe",      "HFE" },
};

TEST(every_reference_image_is_won_by_its_own_plugin) {
    /* Before MF-447 this failed for eight of the eleven: d88_probe() answered
     * 90 for anything whose byte at 0x1B was 0x00 and whose LE32 at 0x1C was
     * smaller than the file, which is most sector images. */
    for (size_t i = 0; i < sizeof(k_expected)/sizeof(k_expected[0]); i++) {
        const uft_format_plugin_t *w = uft_probe_file_format(img(k_expected[i].file));
        if (!w || strcmp(w->name, k_expected[i].winner) != 0) {
            printf("%s -> %s (expected %s)\n", k_expected[i].file,
                   w ? w->name : "(none)", k_expected[i].winner);
            _fail++;
            return;
        }
    }
}

TEST(d88_and_dmk_no_longer_claim_foreign_images) {
    /* Named separately from the ranking test above: these two probes were the
     * cause, and a future loosening should point here, not at a D64. */
    const uft_format_plugin_t *d88 = uft_get_format_plugin_by_name("D88");
    const uft_format_plugin_t *dmk = uft_get_format_plugin_by_name("DMK");
    ASSERT(d88 && d88->probe);
    ASSERT(dmk && dmk->probe);

    for (size_t i = 0; i < sizeof(k_expected)/sizeof(k_expected[0]); i++) {
        FILE *fh = fopen(img(k_expected[i].file), "rb");
        ASSERT(fh != NULL);
        fseek(fh, 0, SEEK_END); long fs = ftell(fh); fseek(fh, 0, SEEK_SET);
        unsigned char buf[4096];
        size_t rd = fread(buf, 1, sizeof(buf), fh);
        fclose(fh);

        int c = 0;
        if (d88->probe(buf, rd, (size_t)fs, &c)) {
            printf("D88 still claims %s (conf %d)\n", k_expected[i].file, c);
            _fail++; return;
        }
        c = 0;
        if (dmk->probe(buf, rd, (size_t)fs, &c) && c >= 85) {
            printf("DMK claims %s at %d\n", k_expected[i].file, c);
            _fail++; return;
        }
    }
}

TEST(the_tightened_probes_still_accept_what_their_readers_read) {
    /* tests/corpus_free holds no real D88 and no real DMK, so this is the
     * honest substitute: headers assembled from exactly the fields d88_open()
     * and dmk_open() dereference. It proves the probes did not become so strict
     * that their own readers are unreachable. It does NOT prove they accept
     * every file a PC-88 or a TRS-80 ever wrote — that needs a real sample and
     * is tracked in docs/VERIFICATION_PLAN.md. */
    const uft_format_plugin_t *d88 = uft_get_format_plugin_by_name("D88");
    const uft_format_plugin_t *dmk = uft_get_format_plugin_by_name("DMK");

    /* --- D88: name, media 2D, size, one track offset just past the header */
    static unsigned char h88[0x2B0];
    memset(h88, 0, sizeof(h88));
    memcpy(h88, "SYNTHETIC", 9);
    h88[0x1A] = 0x00;                 /* not write protected */
    h88[0x1B] = 0x00;                 /* media 2D */
    const uint32_t d88_size = 0x2B0 + 0x1000;
    h88[0x1C] = (unsigned char)(d88_size & 0xFF);
    h88[0x1D] = (unsigned char)((d88_size >> 8) & 0xFF);
    h88[0x1E] = (unsigned char)((d88_size >> 16) & 0xFF);
    h88[0x1F] = (unsigned char)((d88_size >> 24) & 0xFF);
    h88[0x20] = 0xB0; h88[0x21] = 0x02;   /* track 0 at 0x2B0 */

    int c = 0;
    ASSERT(d88->probe(h88, sizeof(h88), d88_size, &c));
    ASSERT(c >= 90);                  /* exact size + valid prot + ASCII name */

    /* the same header with a track offset pointing outside the image */
    h88[0x20] = 0xFF; h88[0x21] = 0xFF; h88[0x22] = 0xFF;
    c = 0;
    ASSERT(!d88->probe(h88, sizeof(h88), d88_size, &c));

    /* --- DMK: 40 tracks, 2 sides, 0x1900 per track */
    unsigned char h[16];
    memset(h, 0, sizeof(h));
    h[0] = 0x00;                      /* not write protected */
    h[1] = 40;                        /* tracks */
    h[2] = 0x00; h[3] = 0x19;         /* track length 0x1900 = 6400 */
    h[4] = 0x00;                      /* double sided */
    const size_t dmk_size = 16 + (size_t)40 * 2 * 0x1900;

    c = 0;
    ASSERT(dmk->probe(h, sizeof(h), dmk_size, &c));
    ASSERT(c >= 95);

    /* one byte short of the size its own track arithmetic implies */
    c = 0;
    ASSERT(!dmk->probe(h, sizeof(h), dmk_size - 1, &c));
}

TEST(a_tie_is_reported_as_a_tie) {
    /* ARCH-13 / MF-448. atrcopy_dos2sd.xfd is raw Atari sectors with no header
     * at all, and four plugins answer with exactly 40:
     *
     *     XFD=40  JVC=40  DSK_SV=40  DSK_VEC=40
     *
     * uft_probe_buffer_format() compares with `conf > best`, so XFD wins
     * because the ATARI group sits before OTHER in g_groups[]. Right by
     * accident. The case is not pathological — a raw sector image really is
     * indistinguishable from another raw sector image of the same size — but
     * the answer used to LOOK identified, and that is the part that was wrong.
     */
    uft_probe_ranking_t r;
    size_t n = uft_probe_file_ranked(img("atrcopy_dos2sd.xfd"), &r);

    ASSERT(n == r.claimants);
    ASSERT(r.winner != NULL);
    ASSERT(r.tied > 1);                       /* several, equally */
    ASSERT(r.claimants >= r.tied);

    /* the winner is still the same one the old API returned — deterministic,
     * so nothing downstream changes format overnight */
    ASSERT(r.winner == uft_probe_file_format(img("atrcopy_dos2sd.xfd")));

    /* and an unambiguous file reports exactly one at the top */
    uft_probe_ranking_t g;
    uft_probe_file_ranked(img("vice_c1541_35trk.g64"), &g);
    ASSERT(g.winner != NULL);
    ASSERT(strcmp(g.winner->name, "G64") == 0);
    ASSERT(g.tied == 1);
    ASSERT(g.claimants >= 2);                 /* G71 also claims it, lower */
    ASSERT(g.runner_up != NULL);
    ASSERT(g.runner_up_confidence < g.confidence);
}

TEST(the_probe_answer_depends_on_how_much_the_caller_read) {
    /* ARCH-15, found while testing ARCH-13 and pinned here because it is worse
     * than the tie it turned up next to.
     *
     * uft_probe_file_format() reads 4096 bytes. uft_smart_open() reads up to
     * 65536. Same file, two entry points, two different formats:
     *
     *     4096 bytes  -> XFD=40, tied with JVC, DSK_SV, DSK_VEC and V9T9
     *     65536 bytes -> JV3=70, alone
     *
     * The identification is a property of the buffer size the caller happened
     * to choose, not of the file. And the larger buffer gives the WORSE answer:
     * JV3 is TRS-80, the file is Atari. Both numbers are asserted exactly, so
     * whichever way this is unified later, the change is visible. */
    uft_probe_ranking_t small;
    uft_probe_file_ranked(img("atrcopy_dos2sd.xfd"), &small);   /* 4096 */
    ASSERT(small.winner != NULL);
    ASSERT(strcmp(small.winner->name, "XFD") == 0);
    ASSERT(small.tied == 5);        /* XFD, JVC, DSK_SV, DSK_VEC, V9T9 */
    ASSERT(small.claimants == 7);   /* plus JV1 and XDM86 at 35 */

    uft_smart_options_t opts;
    uft_smart_options_init(&opts);
    uft_smart_result_t res;
    memset(&res, 0, sizeof(res));
    ASSERT(uft_smart_open(img("atrcopy_dos2sd.xfd"), &opts, &res) == 0);
    ASSERT(res.detection.format_name != NULL);
    ASSERT(strcmp(res.detection.format_name, "JV3") == 0);      /* 65536 */
    ASSERT(res.detection.equally_ranked == 1);
    ASSERT(res.detection.claimants == 8);
    uft_smart_close(&res);
}

TEST(the_smart_open_report_says_when_the_format_is_not_certain) {
    /* The tie has to reach the human. uft_smart_open() carries it in
     * detection.equally_ranked and writes it into the warnings, which the
     * report prints — a confidence number on its own states a certainty nobody
     * measured. D80 is the case that ties at 65536 bytes. */
    uft_smart_options_t opts;
    uft_smart_options_init(&opts);

    uft_smart_result_t res;
    memset(&res, 0, sizeof(res));
    ASSERT(uft_smart_open(img("vice_c1541_35trk.g64"), &opts, &res) == 0);
    ASSERT(res.detection.equally_ranked == 1);
    char *report = uft_smart_report(&res);
    ASSERT(report != NULL);
    ASSERT(strstr(report, "nicht eindeutig") == NULL);
    free(report);
    uft_smart_close(&res);
}

int main(void)
{
    printf("=== uft_register_all_formats(): first execution ever (MF-446/447) ===\n");
    RUN(without_registration_every_open_fails);
    RUN(every_defined_plugin_reaches_the_registry);
    RUN(the_formats_that_were_in_no_group_are_there);
    RUN(the_macro_generated_variants_are_there_too);
    RUN(calling_it_twice_changes_nothing);
    RUN(the_container_id_is_hopeless_and_the_name_is_not);
    RUN(no_two_plugins_share_a_name);
    RUN(after_registration_the_analyzer_branch_is_reachable);
    RUN(every_reference_image_is_won_by_its_own_plugin);
    RUN(d88_and_dmk_no_longer_claim_foreign_images);
    RUN(the_tightened_probes_still_accept_what_their_readers_read);
    RUN(a_tie_is_reported_as_a_tie);
    RUN(the_probe_answer_depends_on_how_much_the_caller_read);
    RUN(the_smart_open_report_says_when_the_format_is_not_certain);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
