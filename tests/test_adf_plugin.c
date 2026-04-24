/**
 * @file test_adf_plugin.c
 * @brief ADF (Amiga Disk Format) Plugin — probe tests.
 *
 * Self-contained per template from test_d64_plugin.c.
 * Mirrors src/formats/adf/uft_adf_parser_v2.c size-based detection plus
 * the DOS bootblock magic check used by the v2 parser.
 *
 * Canonical sizes:
 *   901120  bytes = DD 880k  (80 tracks × 2 sides × 11 sectors × 512 bytes)
 *   1802240 bytes = HD 1760k (80 tracks × 2 sides × 22 sectors × 512 bytes)
 *
 * Bootblock at offset 0 contains a 32-bit "DOS\x" magic (0x444F5300 is
 * the base, with low byte 0..7 for filesystem variant: OFS, FFS,
 * International, DirCache, etc.).
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name)  do { printf("  [TEST] %-28s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define ADF_SIZE_DD   901120u
#define ADF_SIZE_HD   1802240u
#define ADF_DOS_BASE  0x444F5300u   /* "DOS\x00" */

typedef enum { ADF_DENS_NONE = 0, ADF_DENS_DD = 1, ADF_DENS_HD = 2 } adf_density_t;

static adf_density_t adf_size_class(size_t file_size) {
    if (file_size == ADF_SIZE_DD) return ADF_DENS_DD;
    if (file_size == ADF_SIZE_HD) return ADF_DENS_HD;
    return ADF_DENS_NONE;
}

static bool adf_bootblock_magic_ok(const uint8_t *data, size_t size) {
    if (size < 4) return false;
    uint32_t dos_type = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
                        ((uint32_t)data[2] << 8)  |  (uint32_t)data[3];
    return (dos_type & 0xFFFFFF00u) == ADF_DOS_BASE;
}

TEST(size_class_dd) {
    ASSERT(adf_size_class(ADF_SIZE_DD) == ADF_DENS_DD);
}

TEST(size_class_hd) {
    ASSERT(adf_size_class(ADF_SIZE_HD) == ADF_DENS_HD);
}

TEST(size_class_rejects_near_dd) {
    ASSERT(adf_size_class(ADF_SIZE_DD - 1) == ADF_DENS_NONE);
    ASSERT(adf_size_class(ADF_SIZE_DD + 1) == ADF_DENS_NONE);
}

TEST(size_class_rejects_zero) {
    ASSERT(adf_size_class(0) == ADF_DENS_NONE);
}

TEST(bootblock_ofs_ok) {
    uint8_t boot[4] = { 'D', 'O', 'S', 0x00 };   /* OFS */
    ASSERT(adf_bootblock_magic_ok(boot, sizeof(boot)));
}

TEST(bootblock_ffs_ok) {
    uint8_t boot[4] = { 'D', 'O', 'S', 0x01 };   /* FFS */
    ASSERT(adf_bootblock_magic_ok(boot, sizeof(boot)));
}

TEST(bootblock_intl_ok) {
    uint8_t boot[4] = { 'D', 'O', 'S', 0x04 };   /* International */
    ASSERT(adf_bootblock_magic_ok(boot, sizeof(boot)));
}

TEST(bootblock_pfs_rejected) {
    /* PFS/AFS use "PFS\x00" — not "DOS\x" so adf probe rejects.
     * (They would be picked up by a separate PFS plugin.) */
    uint8_t boot[4] = { 'P', 'F', 'S', 0x00 };
    ASSERT(!adf_bootblock_magic_ok(boot, sizeof(boot)));
}

TEST(bootblock_truncated) {
    uint8_t boot[3] = { 'D', 'O', 'S' };
    ASSERT(!adf_bootblock_magic_ok(boot, sizeof(boot)));
}

int main(void) {
    printf("=== ADF plugin probe tests ===\n");
    RUN(size_class_dd);
    RUN(size_class_hd);
    RUN(size_class_rejects_near_dd);
    RUN(size_class_rejects_zero);
    RUN(bootblock_ofs_ok);
    RUN(bootblock_ffs_ok);
    RUN(bootblock_intl_ok);
    RUN(bootblock_pfs_rejected);
    RUN(bootblock_truncated);
    printf("Results: %d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
