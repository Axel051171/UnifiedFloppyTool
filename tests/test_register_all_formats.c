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
 */

#include "uft/uft_format_plugin.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* declared nowhere public — the registry's own entry point */
uft_error_t uft_register_all_formats(void);
size_t uft_get_format_count(void);

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-48s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

/* Every plugin the tree defines, per scripts/plugin_registry_gate.py. If this
 * number moves, the gate moved with it — and the gate reads the source. */
#define EXPECTED_PLUGINS 137

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

int main(void)
{
    printf("=== uft_register_all_formats(): first execution ever (MF-446) ===\n");
    RUN(every_defined_plugin_reaches_the_registry);
    RUN(the_formats_that_were_in_no_group_are_there);
    RUN(the_macro_generated_variants_are_there_too);
    RUN(calling_it_twice_changes_nothing);
    RUN(the_container_id_is_hopeless_and_the_name_is_not);
    RUN(no_two_plugins_share_a_name);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
