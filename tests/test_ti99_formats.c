/**
 * @file test_ti99_formats.c
 * @brief TIFILES and FIAD format tests (MF-393).
 *
 * This file used to be a demo program registered as a test: it called twelve
 * real production functions, printed their results, announced "SUCCESS" for
 * every block and returned 0 unconditionally. It could not fail — a broken
 * TIFILES writer would have printed "=> FAILED: ..." and still gone green.
 * It even had a branch that printed "YES (BUG!)" without failing.
 *
 * The calls were fine; only the checking was missing. Every expectation below
 * is pinned against the implementation, not assumed:
 *   filename rules  leading space rejected, max 10 chars, TI charset only
 *                                             (uft_fiad.c:448, uft_fiad.h:46)
 *   header sizes    TIFILES 128, FIAD 128     (uft_tifiles.h:52, uft_fiad.h:44)
 *
 * It also wrote to a hard-coded /tmp, which is silently useless on the Windows
 * CI runner — the same trap that broke test_otdr_bridge in MF-376. Now uses
 * the TMPDIR/TMP/TEMP chain and removes what it writes.
 */

#include "uft/formats/uft_tifiles.h"
#include "uft/formats/uft_fiad.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-38s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

/** Portable scratch path — never hard-code /tmp, the Windows runner has none. */
static const char *scratch(const char *leaf) {
    static char p[512];
    const char *base = getenv("TMPDIR");
    if (!base) base = getenv("TMP");
    if (!base) base = getenv("TEMP");
    if (!base) base = ".";
    snprintf(p, sizeof(p), "%s/%s", base, leaf);
    return p;
}

TEST(tifiles_dis_var80_roundtrips_its_text) {
    uft_tifiles_file_t tf;
    const char *text = "10 REM TI-99/4A BASIC\n20 PRINT \"HELLO\"\n30 END\n";

    ASSERT(uft_tifiles_create_dis_var80(&tf, "HELLO", text) == UFT_TIFILES_OK);

    uft_tifiles_info_t info;
    memset(&info, 0, sizeof(info));
    uft_tifiles_get_info((const uint8_t *)&tf.header,
                         UFT_TIFILES_HEADER_SIZE + tf.data_size, &info);
    ASSERT(strncmp(info.filename, "HELLO", 5) == 0);
    ASSERT(info.total_sectors > 0);
    ASSERT(info.num_records == 3);          /* three newline-terminated lines */

    /* The text must survive the round trip through the record format. */
    char extracted[1024] = {0};
    ASSERT(uft_tifiles_extract_text(&tf, extracted, sizeof(extracted)) == UFT_TIFILES_OK);
    ASSERT(strstr(extracted, "10 REM TI-99/4A BASIC") != NULL);
    ASSERT(strstr(extracted, "30 END") != NULL);

    uft_tifiles_free(&tf);
}

TEST(tifiles_program_file_reports_its_type) {
    uft_tifiles_file_t tf;
    uint8_t program_data[] = {
        0x00, 0x00, 0x00, 0x10,
        0x10, 0xFE, 0x00, 0x0A,
        0x83, 0xE9, 0x00, 0x00
    };

    ASSERT(uft_tifiles_create_program(&tf, "MYPROGRAM", program_data,
                                      sizeof(program_data)) == UFT_TIFILES_OK);

    uft_tifiles_info_t info;
    memset(&info, 0, sizeof(info));
    uft_tifiles_get_info((const uint8_t *)&tf.header,
                         UFT_TIFILES_HEADER_SIZE + tf.data_size, &info);
    ASSERT(strncmp(info.filename, "MYPROGRAM", 9) == 0);
    ASSERT(info.total_sectors > 0);
    ASSERT(uft_tifiles_type_str(info.type) != NULL);

    uft_tifiles_free(&tf);
}

TEST(tifiles_signature_check_rejects_non_tifiles) {
    /* The old version printed "YES (BUG!)" here and still passed. */
    uint8_t valid[] = {0x07, 'T', 'I', 'F', 'I', 'L', 'E', 'S',
                       0, 1, 0x80, 3, 10, 80, 3, 0,
                       'T', 'E', 'S', 'T', ' ', ' ', ' ', ' ', ' ', ' '};
    uint8_t full[UFT_TIFILES_HEADER_SIZE + 256];
    memset(full, 0, sizeof(full));
    memcpy(full, valid, sizeof(valid));
    ASSERT(uft_tifiles_is_valid(full, sizeof(full)));

    uint8_t invalid[] = {0x00, 'N', 'O', 'T', 'V', 'A', 'L', 'I', 'D'};
    ASSERT(!uft_tifiles_is_valid(invalid, sizeof(invalid)));

    /* a truncated but otherwise valid header must not be accepted either */
    ASSERT(!uft_tifiles_is_valid(full, 8));
}

TEST(fiad_dis_var80_reports_plausible_geometry) {
    uft_fiad_file_t fiad;
    const char *text = "THIS IS A TEST FILE\nSECOND LINE\nTHIRD LINE\n";

    ASSERT(uft_fiad_create_dis_var80(&fiad, "TESTFILE", text) == UFT_FIAD_OK);

    uft_fiad_info_t info;
    memset(&info, 0, sizeof(info));
    uft_fiad_get_info((const uint8_t *)&fiad.header,
                      UFT_FIAD_HEADER_SIZE + fiad.data_size, &info);
    ASSERT(strncmp(info.filename, "TESTFILE", 8) == 0);
    ASSERT(info.total_sectors > 0);
    ASSERT(info.num_records == 3);
    ASSERT(uft_fiad_type_str(info.type) != NULL);

    uft_fiad_free(&fiad);
}

TEST(filename_validation_follows_the_ti_rules) {
    /* pinned against uft_fiad.c:448 and UFT_FIAD_FILENAME_LEN = 10 */
    ASSERT(uft_fiad_validate_filename("HELLO"));
    ASSERT(uft_fiad_validate_filename("TEST123"));
    ASSERT(uft_fiad_validate_filename("EXACTLY10C"));    /* 10 chars: allowed */

    ASSERT(!uft_fiad_validate_filename(" SPACE"));       /* leading space */
    ASSERT(!uft_fiad_validate_filename("TOOLONGNAME"));  /* 11 chars */
    ASSERT(!uft_fiad_validate_filename(""));
    ASSERT(!uft_fiad_validate_filename(NULL));
}

TEST(saved_files_land_on_disk_with_content) {
    uft_tifiles_file_t tf;
    ASSERT(uft_tifiles_create_dis_var80(&tf, "HELLO", "LINE\n") == UFT_TIFILES_OK);

    const char *path = scratch("uft_ti99_hello.tfi");
    ASSERT(uft_tifiles_save_file(&tf, path) == UFT_TIFILES_OK);
    uft_tifiles_free(&tf);

    FILE *f = fopen(path, "rb");
    ASSERT(f != NULL);
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fclose(f);
    remove(path);

    /* at minimum the 128-byte header plus one sector must have been written */
    ASSERT(sz >= UFT_TIFILES_HEADER_SIZE);
}

int main(void) {
    printf("=== TI-99/4A TIFILES + FIAD (MF-393) ===\n");
    RUN(tifiles_dis_var80_roundtrips_its_text);
    RUN(tifiles_program_file_reports_its_type);
    RUN(tifiles_signature_check_rejects_non_tifiles);
    RUN(fiad_dis_var80_reports_plausible_geometry);
    RUN(filename_validation_follows_the_ti_rules);
    RUN(saved_files_land_on_disk_with_content);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
