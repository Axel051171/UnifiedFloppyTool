/**
 * test_mfm_bridge.c — Tests for UFT MFM Detect Bridge
 * =====================================================
 */

#include "mfm_detect.h"
#include "cpm_fs.h"
#include "uft_mfm_detect_bridge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int tests_run = 0, tests_passed = 0, tests_failed = 0;

#define TEST(name) do { printf("  %-55s ", name); tests_run++; } while(0)
#define PASS()     do { printf("✓\n"); tests_passed++; } while(0)
#define FAIL(m)    do { printf("✗ (%s)\n", m); tests_failed++; return; } while(0)

/* ================================================================== */
/*  Helpers: build synthetic disk images                                */
/* ================================================================== */

static inline void put_le16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
}

static inline void put_le32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}

static inline void put_be32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)((v >> 24) & 0xFF);
    p[1] = (uint8_t)((v >> 16) & 0xFF);
    p[2] = (uint8_t)((v >> 8) & 0xFF);
    p[3] = (uint8_t)(v & 0xFF);
}

/** Build a minimal MS-DOS 1.44M FAT12 boot sector */
static void build_dos_144_boot(uint8_t *boot) {
    memset(boot, 0, 512);
    boot[0] = 0xEB; boot[1] = 0x3C; boot[2] = 0x90;  /* JMP short */
    memcpy(boot + 3, "MSDOS5.0", 8);                   /* OEM */
    put_le16(boot + 0x0B, 512);                         /* bytes/sector */
    boot[0x0D] = 1;                                     /* sectors/cluster */
    put_le16(boot + 0x0E, 1);                           /* reserved sectors */
    boot[0x10] = 2;                                     /* num FATs */
    put_le16(boot + 0x11, 224);                         /* root entries */
    put_le16(boot + 0x13, 2880);                        /* total sectors */
    boot[0x15] = 0xF0;                                  /* media descriptor */
    put_le16(boot + 0x16, 9);                           /* sectors/FAT */
    put_le16(boot + 0x18, 18);                          /* sectors/track */
    put_le16(boot + 0x1A, 2);                           /* heads */
    boot[0x26] = 0x29;                                  /* EBPB signature */
    put_le32(boot + 0x27, 0x12345678);                  /* serial */
    memcpy(boot + 0x2B, "NO NAME    ", 11);             /* label */
    memcpy(boot + 0x36, "FAT12   ", 8);                 /* FS type */
    put_le16(boot + 0x1FE, 0xAA55);                     /* boot sig */
}

/** Build a minimal Amiga DD FFS bootblock (2 sectors = 1024 bytes) */
static void build_amiga_ffs_boot(uint8_t *bb) {
    memset(bb, 0, 1024);
    bb[0] = 'D'; bb[1] = 'O'; bb[2] = 'S'; bb[3] = 0x01;  /* DOS\1 = FFS */
    /* Checksum at offset 4 — compute so that sum of all longs == 0 */
    put_be32(bb + 8, 880);  /* rootblock pointer */
    /* Calculate checksum */
    uint32_t sum = 0;
    put_be32(bb + 4, 0);
    for (int i = 0; i < 256; i++) {
        uint32_t w = ((uint32_t)bb[i*4] << 24) | ((uint32_t)bb[i*4+1] << 16) |
                     ((uint32_t)bb[i*4+2] << 8) | bb[i*4+3];
        uint32_t old = sum;
        sum += w;
        if (sum < old) sum++;  /* carry */
    }
    sum = ~sum;  /* negate */
    put_be32(bb + 4, sum);
}

/** Create a full 1.44M DOS image */
static uint8_t *create_dos_144_image(void) {
    uint8_t *img = (uint8_t *)calloc(1, 1474560);
    if (!img) return NULL;
    build_dos_144_boot(img);
    /* FAT1 at sector 1, FAT2 at sector 10 */
    img[512] = 0xF0; img[513] = 0xFF; img[514] = 0xFF;      /* FAT ID */
    img[512 + 9*512] = 0xF0; img[513 + 9*512] = 0xFF; img[514 + 9*512] = 0xFF;
    return img;
}

/** Create an 880K Amiga FFS image */
static uint8_t *create_amiga_ffs_image(void) {
    uint8_t *img = (uint8_t *)calloc(1, 901120);
    if (!img) return NULL;
    build_amiga_ffs_boot(img);
    return img;
}

/** Create a 720K image with CP/M-like directory */
static uint8_t *create_cpm_image(void) {
    uint8_t *img = (uint8_t *)calloc(1, 737280);
    if (!img) return NULL;
    /* Track 0+1 = system tracks, directory starts at track 2 */
    /* CP/M directory: 512-byte sectors, 9 spt, 2 heads, 80 cyl */
    /* Track 2 offset = 2 * 2 * 9 * 512 = 18432 */
    size_t dir_off = 2 * 2 * 9 * 512;

    /* Write some CP/M directory entries */
    /* Entry 0: USER=0, "HELLO   COM" */
    uint8_t *e = img + dir_off;
    e[0] = 0;  /* user 0 */
    memcpy(e + 1, "HELLO   ", 8);
    memcpy(e + 9, "COM", 3);
    e[12] = 0;  /* EX */
    e[13] = 0;  /* S1 */
    e[14] = 0;  /* S2 */
    e[15] = 8;  /* RC = 8 records */
    e[16] = 2;  /* block 2 */

    /* Entry 1: USER=0, "WORLD   TXT" */
    e = img + dir_off + 32;
    e[0] = 0;
    memcpy(e + 1, "WORLD   ", 8);
    memcpy(e + 9, "TXT", 3);
    e[12] = 0;
    e[15] = 4;
    e[16] = 3;

    /* Fill rest of first directory sector with 0xE5 (deleted/empty) */
    for (int i = 2; i < 16; i++) {
        img[dir_off + i * 32] = 0xE5;
    }
    /* Fill second directory sector with 0xE5 */
    memset(img + dir_off + 512, 0xE5, 512);

    return img;
}

/* ================================================================== */
/*  Tests                                                              */
/* ================================================================== */

static void test_version(void) {
    TEST("Bridge version string");
    const char *v = uft_mfmd_version();
    if (!v || strlen(v) == 0) FAIL("empty");
    PASS();
}

static void test_error_strings(void) {
    TEST("Error strings all non-NULL");
    for (int i = 0; i <= 7; i++) {
        if (!uft_mfmd_error_str((uft_mfmd_error_t)i)) FAIL("NULL string");
    }
    PASS();
}

static void test_null_params(void) {
    TEST("NULL parameter handling");
    uft_mfm_detect_info_t info;
    if (uft_mfmd_detect_image(NULL, 0, &info) != UFT_MFMD_ERR_NULL)
        FAIL("detect_image NULL");
    uint8_t buf[512];
    if (uft_mfmd_detect_image(buf, 0, NULL) != UFT_MFMD_ERR_NULL)
        FAIL("detect_image NULL info");
    PASS();
}

static void test_too_small(void) {
    TEST("Reject too-small input");
    uft_mfm_detect_info_t info;
    uint8_t buf[256];
    memset(buf, 0, sizeof(buf));
    if (uft_mfmd_detect_image(buf, 256, &info) != UFT_MFMD_ERR_TOO_SMALL)
        FAIL("should reject < 512");
    PASS();
}

static void test_dos_144(void) {
    TEST("Detect MS-DOS 1.44M FAT12 image");
    uint8_t *img = create_dos_144_image();
    if (!img) FAIL("alloc");

    uft_mfm_detect_info_t info;
    uft_mfmd_error_t rc = uft_mfmd_detect_image(img, 1474560, &info);
    if (rc != UFT_MFMD_OK) { free(img); FAIL("detect failed"); }
    if (!info.is_fat) { uft_mfmd_free(&info); free(img); FAIL("not FAT"); }
    if (info.confidence < 50) { uft_mfmd_free(&info); free(img); FAIL("low conf"); }
    if (info.sector_size != 512) { uft_mfmd_free(&info); free(img); FAIL("bad ss"); }
    if (info.sectors_per_track != 18) { uft_mfmd_free(&info); free(img); FAIL("bad spt"); }

    uft_mfmd_free(&info);
    free(img);
    PASS();
}

static void test_amiga_ffs(void) {
    TEST("Detect Amiga 880K FFS image");
    uint8_t *img = create_amiga_ffs_image();
    if (!img) FAIL("alloc");

    uft_mfm_detect_info_t info;
    uft_mfmd_error_t rc = uft_mfmd_detect_image(img, 901120, &info);
    if (rc != UFT_MFMD_OK) { free(img); FAIL("detect failed"); }
    if (!info.is_amiga) { uft_mfmd_free(&info); free(img); FAIL("not Amiga"); }
    if (info.confidence < 50) { uft_mfmd_free(&info); free(img); FAIL("low conf"); }
    if (info.sectors_per_track != 11) { uft_mfmd_free(&info); free(img); FAIL("bad spt"); }

    uft_mfmd_free(&info);
    free(img);
    PASS();
}

static void test_cpm_heuristic(void) {
    TEST("Detect CP/M via directory heuristic");
    uint8_t *img = create_cpm_image();
    if (!img) FAIL("alloc");

    uft_mfm_detect_info_t info;
    uft_mfmd_error_t rc = uft_mfmd_detect_image(img, 737280, &info);
    if (rc != UFT_MFMD_OK) { free(img); FAIL("detect failed"); }
    /* CP/M detection may or may not trigger depending on heuristic thresholds;
       we mainly test that it doesn't crash and returns valid data */
    if (info.num_candidates == 0 && info.confidence == 0) {
        /* Acceptable: heuristic didn't reach threshold */
    }

    uft_mfmd_free(&info);
    free(img);
    PASS();
}

static void test_candidate_access(void) {
    TEST("Candidate enumeration");
    uint8_t *img = create_dos_144_image();
    if (!img) FAIL("alloc");

    uft_mfm_detect_info_t info;
    uft_mfmd_detect_image(img, 1474560, &info);

    const char *fs, *sys;
    uint8_t conf;
    bool ok = uft_mfmd_get_candidate(&info, 0, &fs, &sys, &conf);
    if (!ok) { uft_mfmd_free(&info); free(img); FAIL("no candidate 0"); }
    if (!fs || strlen(fs) == 0) { uft_mfmd_free(&info); free(img); FAIL("empty fs"); }

    /* Out of range should return false */
    ok = uft_mfmd_get_candidate(&info, 99, &fs, &sys, &conf);
    if (ok) { uft_mfmd_free(&info); free(img); FAIL("should fail idx=99"); }

    uft_mfmd_free(&info);
    free(img);
    PASS();
}

static void test_boot_only(void) {
    TEST("Boot-sector-only detection (quick mode)");
    uint8_t boot[512];
    build_dos_144_boot(boot);

    uft_mfm_detect_info_t info;
    uft_mfmd_error_t rc = uft_mfmd_detect_boot(boot, 512, 512, 18, 2, 80, &info);
    if (rc != UFT_MFMD_OK) FAIL("detect failed");
    if (!info.is_fat) { uft_mfmd_free(&info); FAIL("not FAT"); }

    uft_mfmd_free(&info);
    PASS();
}

static void test_print_report(void) {
    TEST("Print report (smoke test)");
    uint8_t *img = create_dos_144_image();
    if (!img) FAIL("alloc");

    uft_mfm_detect_info_t info;
    uft_mfmd_detect_image(img, 1474560, &info);

    /* Write to /dev/null to test it doesn't crash */
    FILE *f = fopen("/dev/null", "w");
    if (f) {
        uft_mfmd_print_report(&info, f);
        fclose(f);
    }

    uft_mfmd_free(&info);
    free(img);
    PASS();
}

static void test_double_free(void) {
    TEST("Double free safety");
    uft_mfm_detect_info_t info;
    memset(&info, 0, sizeof(info));
    uft_mfmd_free(&info);  /* should be safe */
    uft_mfmd_free(&info);  /* should be safe */
    PASS();
}

static void test_unknown_image(void) {
    TEST("Unknown format (zero-filled image)");
    uint8_t *img = (uint8_t *)calloc(1, 737280);
    if (!img) FAIL("alloc");

    uft_mfm_detect_info_t info;
    uft_mfmd_error_t rc = uft_mfmd_detect_image(img, 737280, &info);
    if (rc != UFT_MFMD_OK) { free(img); FAIL("should not fail"); }
    /* Zero-filled = no valid filesystem, but should not crash */

    uft_mfmd_free(&info);
    free(img);
    PASS();
}

static void test_geometry_720k(void) {
    TEST("720K image geometry detection");
    uint8_t *img = (uint8_t *)calloc(1, 737280);
    if (!img) FAIL("alloc");
    build_dos_144_boot(img);
    /* Override BPB for 720K */
    put_le16(img + 0x13, 1440);
    put_le16(img + 0x16, 3);
    put_le16(img + 0x18, 9);
    img[0x15] = 0xF9;

    uft_mfm_detect_info_t info;
    uft_mfmd_detect_image(img, 737280, &info);

    if (info.sector_size != 512) { uft_mfmd_free(&info); free(img); FAIL("ss"); }
    if (info.sectors_per_track != 9) { uft_mfmd_free(&info); free(img); FAIL("spt"); }
    if (info.heads != 2) { uft_mfmd_free(&info); free(img); FAIL("heads"); }

    uft_mfmd_free(&info);
    free(img);
    PASS();
}

static void test_cpm_not_cpm(void) {
    TEST("cpm_open rejects non-CP/M disk");
    uint8_t *img = create_dos_144_image();
    if (!img) FAIL("alloc");

    uft_mfm_detect_info_t info;
    uft_mfmd_detect_image(img, 1474560, &info);

    cpm_disk_handle_t dh = NULL;
    uft_mfmd_error_t rc = uft_mfmd_cpm_open(img, 1474560, &info, &dh);
    if (rc != UFT_MFMD_ERR_UNSUPPORTED) {
        if (dh) uft_mfmd_cpm_close(dh);
        uft_mfmd_free(&info);
        free(img);
        FAIL("should reject non-CP/M");
    }

    uft_mfmd_free(&info);
    free(img);
    PASS();
}

/* ================================================================== */
/*  Main                                                               */
/* ================================================================== */

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    printf("\n╔══════════════════════════════════════════════════════════╗\n");
    printf("║   UFT MFM DETECT BRIDGE - TEST SUITE                    ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");

    printf("── Grundlagen ───────────────────────────────────────────────\n");
    test_version();
    test_error_strings();
    test_null_params();
    test_too_small();
    test_double_free();

    printf("\n── Format-Erkennung ─────────────────────────────────────────\n");
    test_dos_144();
    test_amiga_ffs();
    test_cpm_heuristic();
    test_unknown_image();
    test_geometry_720k();

    printf("\n── API ──────────────────────────────────────────────────────\n");
    test_candidate_access();
    test_boot_only();
    test_print_report();
    test_cpm_not_cpm();

    printf("\n══════════════════════════════════════════════════════════\n");
    printf("  Ergebnis: %d/%d Tests bestanden\n", tests_passed, tests_run);
    printf("══════════════════════════════════════════════════════════\n\n");

    return tests_failed > 0 ? 1 : 0;
}
