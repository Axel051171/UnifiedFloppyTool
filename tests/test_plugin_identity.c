/**
 * @file test_plugin_identity.c
 * @brief A disk must be handled by the plugin that opened it (MF-445, ARCH-10).
 *
 * `struct uft_disk` stored only `format`, and every consumer recovered the
 * plugin with `uft_get_format_plugin(disk->format)`. That returns the FIRST
 * registered plugin carrying the id — and 82 of the 88 plugins in the tree
 * declare `.format = UFT_FORMAT_DSK`, because the enum names a container class
 * ("a sector image"), not a format.
 *
 * So: register D64 first, open a D81, and `uft_disk_close()` called the D64
 * plugin's close() on `plugin_data` the D81 plugin had allocated. Free of
 * foreign memory. It never surfaced only because the registry was empty in
 * every process (ARCH-9) and the lookup answered NULL.
 *
 * The first test here is the one that matters: it registers D64 before D81 on
 * purpose, so a first-match lookup returns the wrong plugin, and then checks
 * that the disk still knows its own. Against the old code it fails; the second
 * assertion in it is exactly the heap corruption written down as a comparison.
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_disk_convert.h"
#include "uft/uft_core.h"   /* uft_disk_open/close */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef UFT_CORPUS_DIR
#error "UFT_CORPUS_DIR must be defined by the build (tests/CMakeLists.txt)"
#endif

extern const uft_format_plugin_t uft_format_plugin_d64;
extern const uft_format_plugin_t uft_format_plugin_d67;
extern const uft_format_plugin_t uft_format_plugin_d81;
extern const uft_format_plugin_t uft_format_plugin_xfd;
/* MF-450: two of the 49 DSK_PLUGIN() variants. They share UFT_FORMAT_DSK
 * legitimately - generic sector images with no dedicated enum value - so
 * they are what keeps the ambiguity cases in this file real now that D64,
 * D81 and XFD have ids of their own. */
extern const uft_format_plugin_t uft_format_plugin_dsk_fm7;
extern const uft_format_plugin_t uft_format_plugin_dsk_msx;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-50s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

static const char *img(const char *name)
{
    static char p[512];
    snprintf(p, sizeof(p), "%s/%s", UFT_CORPUS_DIR, name);
    return p;
}

TEST(the_disk_knows_which_plugin_opened_it) {
    /* Registration order used to be the trap: D64 first, so a lookup keyed on
     * UFT_FORMAT_DSK answered D64 for every sector image ever opened. MF-450
     * removed the cause — D64, D81 and XFD each declare their own uft_format_t
     * now — so the two answers agree here, and the agreement is the assertion.
     * The disagreement case moved to two_plugins_may_share_a_container_id,
     * where sharing an id is legitimate. */
    ASSERT(uft_register_format_plugin(&uft_format_plugin_d64) == UFT_OK);
    ASSERT(uft_register_format_plugin(&uft_format_plugin_d67) == UFT_OK);
    ASSERT(uft_register_format_plugin(&uft_format_plugin_d81) == UFT_OK);
    ASSERT(uft_register_format_plugin(&uft_format_plugin_xfd) == UFT_OK);
    ASSERT(uft_register_format_plugin(&uft_format_plugin_dsk_fm7) == UFT_OK);
    ASSERT(uft_register_format_plugin(&uft_format_plugin_dsk_msx) == UFT_OK);

    uft_disk_t *disk = uft_disk_open(img("vice_c1541_80trk.d81"), true);
    ASSERT(disk != NULL);

    /* what the disk reports */
    ASSERT(uft_disk_plugin(disk) == &uft_format_plugin_d81);

    /* and the id-keyed lookup, which now has an id that means D81 */
    ASSERT(disk->format == UFT_FORMAT_D81);
    ASSERT(uft_get_format_plugin(disk->format) == &uft_format_plugin_d81);
    ASSERT(uft_count_format_plugins_for(UFT_FORMAT_D81) == 1);

    /* close() must be the D81 one: it frees what D81's open() allocated */
    uft_disk_close(disk);
}

TEST(two_plugins_may_share_a_container_id) {
    /* DSK_FM7 and DSK_MSX are two of the 49 DSK_PLUGIN() variants: generic
     * sector images with no enum value of their own, both UFT_FORMAT_DSK.
     * That is correct, not a defect — and it is why uft_get_format_plugin()
     * can never be the way to identify a plugin. D67 shares it for the same
     * reason: the enum has no UFT_FORMAT_D67. */
    ASSERT(uft_format_plugin_dsk_fm7.format == UFT_FORMAT_DSK);
    ASSERT(uft_format_plugin_dsk_msx.format == UFT_FORMAT_DSK);
    ASSERT(uft_format_plugin_d67.format == UFT_FORMAT_DSK);
    ASSERT(uft_count_format_plugins_for(UFT_FORMAT_DSK) == 3);

    /* first match, and which of the three it is depends on registration
     * order — exactly why nothing may depend on it */
    ASSERT(uft_get_format_plugin(UFT_FORMAT_DSK) == &uft_format_plugin_d67);

    /* the name always answers */
    ASSERT(uft_get_format_plugin_by_name("DSK_MSX") == &uft_format_plugin_dsk_msx);
}

TEST(a_hand_built_disk_gets_no_guess) {
    /* Disks assembled by hand (GUI, legacy paths) have no plugin recorded. The
     * container id is used only when exactly one plugin carries it — otherwise
     * NULL, because the answer would be handed plugin_data to free. */
    uft_disk_t d;
    memset(&d, 0, sizeof(d));
    d.format = UFT_FORMAT_DSK;

    ASSERT(uft_count_format_plugins_for(UFT_FORMAT_DSK) == 3);
    ASSERT(uft_disk_plugin(&d) == NULL);     /* three carry it: no guess */

    /* An id exactly one plugin carries is not ambiguous and does resolve —
     * possible since MF-450, when 30 plugins stopped declaring DSK for a
     * format the enum already had a value for. */
    d.format = UFT_FORMAT_D81;
    ASSERT(uft_disk_plugin(&d) == &uft_format_plugin_d81);

    /* an id no registered plugin carries stays NULL, not a fallback */
    d.format = UFT_FORMAT_SCP;
    ASSERT(uft_disk_plugin(&d) == NULL);
}

TEST(an_ambiguous_target_is_refused_not_guessed) {
    /* uft_resolve_format_plugin(): id first, then the extension the caller
     * chose, then nothing. Never a first match. */
    size_t candidates = 0;

    /* no hint, ambiguous id -> refused, and the count says why */
    ASSERT(uft_resolve_format_plugin(UFT_FORMAT_DSK, NULL, &candidates) == NULL);
    ASSERT(candidates == 3);

    /* an unambiguous id needs no hint at all (MF-450) */
    ASSERT(uft_resolve_format_plugin(UFT_FORMAT_D81, NULL, &candidates)
           == &uft_format_plugin_d81);
    ASSERT(candidates == 1);

    /* the caller named the output: that is intent, and it decides among the
     * three that do share the container id */
    ASSERT(uft_resolve_format_plugin(UFT_FORMAT_DSK, "out.d67", NULL)
           == &uft_format_plugin_d67);

    /* an extension none of them claims stays unresolved */
    ASSERT(uft_resolve_format_plugin(UFT_FORMAT_DSK, "out.wibble", NULL) == NULL);
    /* and so does a name with no extension at all */
    ASSERT(uft_resolve_format_plugin(UFT_FORMAT_DSK, "outfile", NULL) == NULL);
}

TEST(the_extension_matcher_does_not_use_strtok) {
    /* MF-445 replaced a strtok() loop over a 256-byte stack copy. The copy
     * truncated long extension lists and the cursor is static, so two threads
     * walking the list interleaved. Behaviour is checked here; the reentrancy
     * is checked by reading the code. Case must not matter, and a leading dot
     * must be tolerated on the query side. */
    ASSERT(uft_find_format_plugin_by_extension("d81") == &uft_format_plugin_d81);
    ASSERT(uft_find_format_plugin_by_extension("D81") == &uft_format_plugin_d81);
    ASSERT(uft_find_format_plugin_by_extension(".d81") == &uft_format_plugin_d81);
    ASSERT(uft_find_format_plugin_by_extension("d8") == NULL);   /* no prefix hit */
    ASSERT(uft_find_format_plugin_by_extension("d811") == NULL);
    ASSERT(uft_find_format_plugin_by_extension("") == NULL);
}

TEST(convert_by_name_needs_nothing_resolved) {
    /* uft_disk_convert() takes a uft_format_t and may have to refuse.
     * uft_disk_convert_as() takes the plugin name and never can. */
    uft_convert_mode_t mode;
    bool lossy = true;

    /* the enum path cannot answer for an ambiguous pair */
    ASSERT(uft_disk_convert_check(UFT_FORMAT_DSK, UFT_FORMAT_DSK, &mode, &lossy)
           == UFT_ERROR_NOT_SUPPORTED);

    /* but it can for two ids that each mean one plugin — what MF-450 bought:
     * 36 of the 37 ids now identify exactly one plugin */
    ASSERT(uft_disk_convert_check(UFT_FORMAT_D64, UFT_FORMAT_D81, &mode, &lossy)
           == UFT_OK);

    /* the name path can */
    ASSERT(uft_disk_convert_check_by_name("D64", "D81", &mode, &lossy) == UFT_OK);
    ASSERT(mode == UFT_CONVERT_SECTOR);

    /* an unknown name is refused, not resolved to something nearby */
    ASSERT(uft_disk_convert_check_by_name("D64", "NOSUCHPLUGIN", &mode, &lossy)
           != UFT_OK);
}

int main(void)
{
    printf("=== plugin identity: the disk knows its own (MF-445) ===\n");
    RUN(the_disk_knows_which_plugin_opened_it);
    RUN(two_plugins_may_share_a_container_id);
    RUN(a_hand_built_disk_gets_no_guess);
    RUN(an_ambiguous_target_is_refused_not_guessed);
    RUN(the_extension_matcher_does_not_use_strtok);
    RUN(convert_by_name_needs_nothing_resolved);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
