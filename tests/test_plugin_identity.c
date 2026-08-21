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
    /* Registration order is the trap: D64 is first, so a lookup keyed on
     * UFT_FORMAT_DSK answers D64 for every sector image ever opened. */
    ASSERT(uft_register_format_plugin(&uft_format_plugin_d64) == UFT_OK);
    ASSERT(uft_register_format_plugin(&uft_format_plugin_d67) == UFT_OK);
    ASSERT(uft_register_format_plugin(&uft_format_plugin_d81) == UFT_OK);
    ASSERT(uft_register_format_plugin(&uft_format_plugin_xfd) == UFT_OK);

    uft_disk_t *disk = uft_disk_open(img("vice_c1541_80trk.d81"), true);
    ASSERT(disk != NULL);

    /* what the disk reports */
    ASSERT(uft_disk_plugin(disk) == &uft_format_plugin_d81);

    /* what the id-keyed lookup reports for the very same disk — the bug,
     * written as a comparison. If these two are ever the same object again,
     * either the registration order changed or the field was dropped. */
    ASSERT(uft_get_format_plugin(disk->format) == &uft_format_plugin_d64);
    ASSERT(uft_disk_plugin(disk) != uft_get_format_plugin(disk->format));

    /* close() must be the D81 one: it frees what D81's open() allocated */
    uft_disk_close(disk);
}

TEST(a_hand_built_disk_gets_no_guess) {
    /* Disks assembled by hand (GUI, legacy paths) have no plugin recorded. The
     * container id is used only when exactly one plugin carries it — otherwise
     * NULL, because the answer would be handed plugin_data to free. */
    uft_disk_t d;
    memset(&d, 0, sizeof(d));
    d.format = UFT_FORMAT_DSK;

    ASSERT(uft_count_format_plugins_for(UFT_FORMAT_DSK) == 4);
    ASSERT(uft_disk_plugin(&d) == NULL);

    /* An id only one plugin carries is not ambiguous and does resolve. None of
     * the four registered here is such a case, so this checks the other side:
     * an id no plugin carries is NULL too, not a fallback to something else. */
    d.format = UFT_FORMAT_SCP;
    ASSERT(uft_disk_plugin(&d) == NULL);
}

TEST(an_ambiguous_target_is_refused_not_guessed) {
    /* uft_resolve_format_plugin(): id first, then the extension the caller
     * chose, then nothing. Never a first match. */
    size_t candidates = 0;

    /* no hint, ambiguous id -> refused, and the count says why */
    ASSERT(uft_resolve_format_plugin(UFT_FORMAT_DSK, NULL, &candidates) == NULL);
    ASSERT(candidates == 4);

    /* the caller named the output: that is intent, and it decides */
    ASSERT(uft_resolve_format_plugin(UFT_FORMAT_DSK, "out.d81", NULL)
           == &uft_format_plugin_d81);
    ASSERT(uft_resolve_format_plugin(UFT_FORMAT_DSK, "out.xfd", NULL)
           == &uft_format_plugin_xfd);

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
    RUN(a_hand_built_disk_gets_no_guess);
    RUN(an_ambiguous_target_is_refused_not_guessed);
    RUN(the_extension_matcher_does_not_use_strtok);
    RUN(convert_by_name_needs_nothing_resolved);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
