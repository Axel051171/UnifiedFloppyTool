/**
 * =============================================================================
 * MFM Detect - CLI Tool & Tests
 * =============================================================================
 */

#include "mfm_detect.h"
#include <assert.h>

/** Local big-endian 32-bit helper for tests */
static inline uint32_t be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  %-50s ", name); tests_run++; } while(0)
#define PASS()     do { printf("✓\n"); tests_passed++; } while(0)
#define FAIL(m)    do { printf("✗ (%s)\n", m); tests_failed++; return; } while(0)


/* ==========================================================================
 * Test: FAT12 BPB Parsing
 * ========================================================================== */
static void test_fat12_bpb(void) {
    TEST("FAT12 BPB Parsing (1.44M)");

    /* Simulierter Boot-Sektor einer 1.44M FAT12-Diskette */
    uint8_t boot[512];
    memset(boot, 0, sizeof(boot));

    /* Jump */
    boot[0] = 0xEB; boot[1] = 0x3C; boot[2] = 0x90;

    /* OEM */
    memcpy(boot + 3, "MSDOS5.0", 8);

    /* BPB */
    boot[0x0B] = 0x00; boot[0x0C] = 0x02;  /* 512 bytes/sector */
    boot[0x0D] = 0x01;                       /* 1 sector/cluster */
    boot[0x0E] = 0x01; boot[0x0F] = 0x00;  /* 1 reserved */
    boot[0x10] = 0x02;                       /* 2 FATs */
    boot[0x11] = 0xE0; boot[0x12] = 0x00;  /* 224 root entries */
    boot[0x13] = 0x40; boot[0x14] = 0x0B;  /* 2880 total sectors */
    boot[0x15] = 0xF0;                       /* media descriptor */
    boot[0x16] = 0x09; boot[0x17] = 0x00;  /* 9 sectors/FAT */
    boot[0x18] = 0x12; boot[0x19] = 0x00;  /* 18 sectors/track */
    boot[0x1A] = 0x02; boot[0x1B] = 0x00;  /* 2 heads */

    /* EBPB */
    boot[0x26] = 0x29;                       /* Extended signature */
    boot[0x27] = 0x34; boot[0x28] = 0x12;  /* Serial number */
    memcpy(boot + 0x2B, "NO NAME    ", 11);
    memcpy(boot + 0x36, "FAT12   ", 8);

    /* Boot-Signatur */
    boot[0x1FE] = 0x55; boot[0x1FF] = 0xAA;

    fat_bpb_t bpb;
    mfm_error_t err = mfm_parse_fat_bpb(boot, 512, &bpb);
    if (err != MFM_OK) FAIL("parse failed");
    if (!bpb.has_valid_bpb) FAIL("BPB ungültig");
    if (bpb.bytes_per_sector != 512) FAIL("bytes_per_sector");
    if (bpb.total_sectors_16 != 2880) FAIL("total_sectors");
    if (bpb.sectors_per_track != 18) FAIL("sectors_per_track");
    if (bpb.media_descriptor != 0xF0) FAIL("media_descriptor");
    if (!bpb.has_ebpb) FAIL("kein EBPB");
    if (!bpb.has_boot_sig) FAIL("kein 0xAA55");
    if (strstr(bpb.fs_type, "FAT12") == NULL) FAIL("kein FAT12");

    PASS();
}


/* ==========================================================================
 * Test: FAT BPB Validierung
 * ========================================================================== */
static void test_fat_validation(void) {
    TEST("FAT BPB Validierung");

    fat_bpb_t bpb = {0};

    /* Ungültig: Alles Null */
    if (mfm_validate_fat_bpb(&bpb)) FAIL("soll ungültig sein");

    /* Gültig: Minimal */
    bpb.bytes_per_sector = 512;
    bpb.sectors_per_cluster = 2;
    bpb.reserved_sectors = 1;
    bpb.num_fats = 2;
    bpb.root_entries = 112;
    bpb.total_sectors_16 = 720;
    bpb.media_descriptor = 0xFD;
    bpb.sectors_per_fat = 2;
    bpb.sectors_per_track = 9;
    bpb.num_heads = 2;

    if (!mfm_validate_fat_bpb(&bpb)) FAIL("soll gültig sein");

    /* Ungültig: Sektorgröße 300 */
    bpb.bytes_per_sector = 300;
    if (mfm_validate_fat_bpb(&bpb)) FAIL("300 B/S ungültig");
    bpb.bytes_per_sector = 512;

    /* Ungültig: Cluster-Größe 3 */
    bpb.sectors_per_cluster = 3;
    if (mfm_validate_fat_bpb(&bpb)) FAIL("3 S/C ungültig");

    PASS();
}


/* ==========================================================================
 * Test: Amiga Bootblock
 * ========================================================================== */
static void test_amiga_bootblock(void) {
    TEST("Amiga Bootblock Erkennung");

    uint8_t boot[1024];
    memset(boot, 0, sizeof(boot));

    /* DOS\0 (OFS) */
    boot[0] = 'D'; boot[1] = 'O'; boot[2] = 'S'; boot[3] = 0x00;

    /* Rootblock-Zeiger */
    boot[8] = 0x00; boot[9] = 0x00; boot[10] = 0x03; boot[11] = 0x70; /* 880 */

    /* Prüfsumme berechnen (alle Wörter summiert = 0) */
    /* Für den Test setzen wir die Checksum manuell */
    uint32_t sum = 0;
    boot[4] = boot[5] = boot[6] = boot[7] = 0;
    for (int i = 0; i < 1024; i += 4) {
        if (i != 4) sum += be32(boot + i);
    }
    uint32_t cksum = (uint32_t)(-(int32_t)sum);
    boot[4] = (cksum >> 24) & 0xFF;
    boot[5] = (cksum >> 16) & 0xFF;
    boot[6] = (cksum >> 8) & 0xFF;
    boot[7] = cksum & 0xFF;

    amiga_info_t info;
    mfm_error_t err = mfm_parse_amiga_bootblock(boot, 1024, &info);
    if (err != MFM_OK) FAIL("parse failed");
    if (info.flags != 0x00) FAIL("flags != OFS");
    if (info.rootblock != 880) FAIL("rootblock != 880");
    if (!info.checksum_valid) FAIL("checksum ungültig");

    /* FFS-Variante */
    boot[3] = 0x01;
    /* Checksum neu berechnen */
    boot[4] = boot[5] = boot[6] = boot[7] = 0;
    sum = 0;
    for (int i = 0; i < 1024; i += 4) {
        if (i != 4) sum += be32(boot + i);
    }
    cksum = (uint32_t)(-(int32_t)sum);
    boot[4] = (cksum >> 24); boot[5] = (cksum >> 16);
    boot[6] = (cksum >> 8); boot[7] = cksum;

    err = mfm_parse_amiga_bootblock(boot, 1024, &info);
    if (err != MFM_OK) FAIL("FFS parse failed");
    if (!(info.flags & 0x01)) FAIL("FFS flag fehlt");

    PASS();
}


/* ==========================================================================
 * Test: Atari ST Erkennung
 * ========================================================================== */
static void test_atari_st(void) {
    TEST("Atari ST Erkennung");

    uint8_t boot[512];
    memset(boot, 0, sizeof(boot));

    /* 68000 BRA.S */
    boot[0] = 0x60; boot[1] = 0x1C;

    /* BPB (gültig für Atari ST DD) */
    boot[0x0B] = 0x00; boot[0x0C] = 0x02;  /* 512 */
    boot[0x0D] = 0x02;                       /* 2 sect/cluster */
    boot[0x0E] = 0x01; boot[0x0F] = 0x00;  /* 1 reserved */
    boot[0x10] = 0x02;                       /* 2 FATs */
    boot[0x11] = 0x70; boot[0x12] = 0x00;  /* 112 root entries */
    boot[0x13] = 0xA0; boot[0x14] = 0x05;  /* 1440 sectors */
    boot[0x15] = 0xF9;                       /* media */
    boot[0x16] = 0x05; boot[0x17] = 0x00;  /* 5 sect/FAT */
    boot[0x18] = 0x09; boot[0x19] = 0x00;  /* 9 sect/track */
    boot[0x1A] = 0x02; boot[0x1B] = 0x00;  /* 2 heads */

    if (!mfm_detect_atari_st(boot, 512)) FAIL("Atari ST nicht erkannt");

    /* Prüfsumme 0x1234 setzen (16-Bit Big-Endian Summe) */
    uint16_t current = mfm_atari_st_checksum(boot);
    uint16_t needed = 0x1234 - current;
    /* In die letzten 2 Bytes schreiben (BE) */
    boot[510] = (needed >> 8) & 0xFF;
    boot[511] = needed & 0xFF;

    if (mfm_atari_st_checksum(boot) != 0x1234) FAIL("Checksum != 0x1234");

    PASS();
}


/* ==========================================================================
 * Test: Burst-Query Parsing
 * ========================================================================== */
static void test_burst_query(void) {
    TEST("Burst-Query Parsing");

    mfm_detect_result_t *r = mfm_detect_create();
    if (!r) FAIL("alloc");

    /* Simulierte Burst-Antwort: MFM, 10 Sektoren, Min=0, Max=9, Interleave=1 */
    uint8_t burst[] = {0x02, 0x00, 10, 0, 0, 9, 1};
    mfm_error_t err = mfm_detect_from_burst(r, burst, 7);
    if (err != MFM_OK) { mfm_detect_free(r); FAIL("burst parse"); }

    if (!r->burst.is_mfm) { mfm_detect_free(r); FAIL("not MFM"); }
    if (r->burst.sectors_per_track != 10) { mfm_detect_free(r); FAIL("spt"); }
    if (r->burst.cpm_interleave != 1) { mfm_detect_free(r); FAIL("interlv"); }
    if (r->physical.sector_size != 512) { mfm_detect_free(r); FAIL("secsize"); }

    /* CBM 1581 Geometrie (10×512×2×80 = 800K) */
    if (r->physical.geometry != MFM_GEOM_CBM_1581) {
        mfm_detect_free(r); FAIL("geometry");
    }

    mfm_detect_free(r);

    /* GCR-Disk: Status < 2 */
    r = mfm_detect_create();
    uint8_t gcr_burst[] = {0x01};
    err = mfm_detect_from_burst(r, gcr_burst, 1);
    if (err != MFM_ERR_NOT_MFM) { mfm_detect_free(r); FAIL("GCR"); }

    mfm_detect_free(r);
    PASS();
}


/* ==========================================================================
 * Test: Geometrie-Erkennung
 * ========================================================================== */
static void test_geometry(void) {
    TEST("Geometrie-Erkennung");

    if (mfm_identify_geometry(512, 18, 2, 80) != MFM_GEOM_35_DSHD_80)
        FAIL("1.44M");
    if (mfm_identify_geometry(512, 9, 2, 80) != MFM_GEOM_35_DSDD_80)
        FAIL("720K");
    if (mfm_identify_geometry(512, 9, 2, 40) != MFM_GEOM_525_DSDD_40)
        FAIL("360K");
    if (mfm_identify_geometry(512, 11, 2, 80) != MFM_GEOM_AMIGA_DD)
        FAIL("Amiga DD");
    if (mfm_identify_geometry(512, 22, 2, 80) != MFM_GEOM_AMIGA_HD)
        FAIL("Amiga HD");
    if (mfm_identify_geometry(512, 10, 2, 80) != MFM_GEOM_CBM_1581)
        FAIL("CBM 1581");
    if (mfm_identify_geometry(128, 26, 1, 77) != MFM_GEOM_8_SSSD)
        FAIL("8\" SSSD");
    if (mfm_identify_geometry(512, 15, 2, 80) != MFM_GEOM_525_DSHD_80)
        FAIL("1.2M");

    PASS();
}


/* ==========================================================================
 * Test: CP/M Directory-Analyse
 * ========================================================================== */
static void test_cpm_directory(void) {
    TEST("CP/M Directory-Analyse");

    /* Simuliertes CP/M-Directory (4 Einträge) */
    uint8_t dir[256];
    memset(dir, 0xE5, sizeof(dir));  /* Alle gelöscht */

    /* Eintrag 0: User 0, TEST    .COM */
    uint8_t *e0 = dir;
    e0[0] = 0x00;  /* User 0 */
    memcpy(e0 + 1, "TEST    COM", 11);
    e0[12] = 0;    /* EX = 0 */
    e0[13] = 0;    /* S1 = 0 */
    e0[14] = 0;    /* S2 = 0 */
    e0[15] = 16;   /* RC = 16 */
    e0[16] = 2;    /* Block 2 */
    e0[17] = 3;    /* Block 3 */

    /* Eintrag 1: User 0, HELLO   .BAS */
    uint8_t *e1 = dir + 32;
    e1[0] = 0x00;
    memcpy(e1 + 1, "HELLO   BAS", 11);
    e1[12] = 0;
    e1[15] = 8;
    e1[16] = 4;

    /* Eintrag 2: Gelöscht */
    dir[64] = 0xE5;
    memcpy(dir + 65, "OLD     TXT", 11);

    /* Eintrag 3: Leer (alle Null) */
    memset(dir + 96, 0, 32);

    mfm_cpm_analysis_t analysis;
    mfm_error_t err = mfm_analyze_cpm_directory(dir, sizeof(dir), 512, &analysis);
    if (err != MFM_OK) FAIL("analyse failed");
    if (analysis.num_files < 2) FAIL("zu wenige Dateien");
    if (analysis.confidence < 40) FAIL("Konfidenz zu niedrig");

    PASS();
}


/* ==========================================================================
 * Test: CP/M DPB Berechnung
 * ========================================================================== */
static void test_cpm_dpb(void) {
    TEST("CP/M DPB Berechnung (Kaypro II)");

    disk_physical_t phys = {
        .sector_size = 512,
        .sectors_per_track = 10,
        .heads = 1,
        .cylinders = 40,
    };

    mfm_cpm_dpb_t dpb;
    mfm_error_t err = mfm_calc_cpm_dpb(&phys, 1, 1024, 64, &dpb);
    if (err != MFM_OK) FAIL("calc failed");

    /* Kaypro II: SPT=40 (10×512/128), BSH=3, BLM=7, OFF=1 */
    if (dpb.spt != 40) FAIL("SPT");
    if (dpb.bsh != 3) FAIL("BSH");
    if (dpb.blm != 7) FAIL("BLM");
    if (dpb.off != 1) FAIL("OFF");
    if (dpb.drm != 63) FAIL("DRM");
    if (!dpb.is_valid) FAIL("ungültig");

    /* Kapazität: (40-1 tracks) × 10 × 512 / 1024 = 195 Blöcke × 1K */
    /* DSM sollte > 0 sein */
    if (dpb.dsm == 0) FAIL("DSM=0");

    PASS();
}


/* ==========================================================================
 * Test: Boot-Sektor-Analyse (DOS)
 * ========================================================================== */
static void test_boot_analysis_dos(void) {
    TEST("Boot-Analyse: MS-DOS 720K");

    mfm_detect_result_t *r = mfm_detect_create();
    if (!r) FAIL("alloc");

    mfm_detect_set_physical(r, 512, 9, 2, 80, 1);

    /* Boot-Sektor erstellen */
    uint8_t boot[512];
    memset(boot, 0, sizeof(boot));
    boot[0] = 0xEB; boot[1] = 0x3C; boot[2] = 0x90;
    memcpy(boot + 3, "MSDOS5.0", 8);
    boot[0x0B] = 0x00; boot[0x0C] = 0x02;
    boot[0x0D] = 0x02;
    boot[0x0E] = 0x01; boot[0x0F] = 0x00;
    boot[0x10] = 0x02;
    boot[0x11] = 0x70; boot[0x12] = 0x00;
    boot[0x13] = 0xA0; boot[0x14] = 0x05;  /* 1440 */
    boot[0x15] = 0xF9;
    boot[0x16] = 0x03; boot[0x17] = 0x00;
    boot[0x18] = 0x09; boot[0x19] = 0x00;
    boot[0x1A] = 0x02; boot[0x1B] = 0x00;
    boot[0x26] = 0x29;
    memcpy(boot + 0x36, "FAT12   ", 8);
    boot[0x1FE] = 0x55; boot[0x1FF] = 0xAA;

    mfm_detect_analyze_boot_data(r, boot, 512);

    if (r->num_candidates == 0) { mfm_detect_free(r); FAIL("keine Kandidaten"); }

    /* Besten Kandidaten prüfen */
    mfm_sort_candidates(r);
    bool found_dos = false;
    for (int i = 0; i < r->num_candidates; i++) {
        if (r->candidates[i].fs_type == MFM_FS_FAT12_DOS) {
            found_dos = true;
            break;
        }
    }
    if (!found_dos) { mfm_detect_free(r); FAIL("DOS nicht erkannt"); }

    mfm_detect_free(r);
    PASS();
}


/* ==========================================================================
 * Test: Boot-Analyse (Amiga)
 * ========================================================================== */
static void test_boot_analysis_amiga(void) {
    TEST("Boot-Analyse: Amiga FFS");

    mfm_detect_result_t *r = mfm_detect_create();
    if (!r) FAIL("alloc");

    mfm_detect_set_physical(r, 512, 11, 2, 80, 0);

    uint8_t boot[1024];
    memset(boot, 0, sizeof(boot));
    boot[0] = 'D'; boot[1] = 'O'; boot[2] = 'S'; boot[3] = 0x01; /* FFS */
    boot[8] = 0; boot[9] = 0; boot[10] = 0x03; boot[11] = 0x70; /* root=880 */

    /* Checksum */
    boot[4] = boot[5] = boot[6] = boot[7] = 0;
    uint32_t sum = 0;
    for (int i = 0; i < 1024; i += 4) {
        if (i != 4) sum += be32(boot + i);
    }
    uint32_t ck = (uint32_t)(-(int32_t)sum);
    boot[4] = (ck >> 24); boot[5] = (ck >> 16);
    boot[6] = (ck >> 8); boot[7] = ck;

    mfm_detect_analyze_boot_data(r, boot, 1024);

    if (r->num_candidates == 0) { mfm_detect_free(r); FAIL("keine Kandidaten"); }

    mfm_sort_candidates(r);
    if (r->best_fs != MFM_FS_AMIGA_FFS) {
        mfm_detect_free(r);
        FAIL("kein Amiga FFS");
    }
    if (r->best_confidence < 90) { mfm_detect_free(r); FAIL("Konfidenz zu niedrig"); }

    mfm_detect_free(r);
    PASS();
}


/* ==========================================================================
 * Test: Bekannte CP/M-Formate
 * ========================================================================== */
static void test_known_cpm(void) {
    TEST("Bekannte CP/M-Format-Datenbank");

    if (mfm_get_known_cpm_format_count() < 10) FAIL("zu wenige Formate");

    /* Kaypro II suchen */
    disk_physical_t phys = {
        .sector_size = 512,
        .sectors_per_track = 10,
        .heads = 1,
        .cylinders = 40,
    };

    const mfm_cpm_known_format_t *matches[8];
    uint8_t n = mfm_find_known_cpm_formats(&phys, matches, 8);
    if (n == 0) FAIL("Kaypro II nicht gefunden");

    bool found_kaypro = false;
    for (int i = 0; i < n; i++) {
        if (matches[i]->fs_type == MFM_FS_CPM_KAYPRO) {
            found_kaypro = true;
            break;
        }
    }
    if (!found_kaypro) FAIL("kein Kaypro-Match");

    PASS();
}


/* ==========================================================================
 * Test: String-Funktionen
 * ========================================================================== */
static void test_strings(void) {
    TEST("String-Funktionen");

    if (strlen(mfm_fs_type_str(MFM_FS_FAT12_DOS)) == 0) FAIL("FAT12");
    if (strlen(mfm_fs_type_str(MFM_FS_AMIGA_FFS)) == 0) FAIL("Amiga");
    if (strlen(mfm_fs_type_str(MFM_FS_CPM_22)) == 0) FAIL("CP/M");
    if (strlen(mfm_geometry_str(MFM_GEOM_35_DSHD_80)) == 0) FAIL("1.44M geom");
    if (strlen(mfm_encoding_str(ENCODING_MFM)) == 0) FAIL("MFM enc");
    if (strlen(mfm_error_str(MFM_OK)) == 0) FAIL("OK err");

    PASS();
}


/* ==========================================================================
 * Test: Report-Ausgabe (Smoke)
 * ========================================================================== */
static void test_report(void) {
    TEST("Report-Ausgabe (Smoke-Test)");

    mfm_detect_result_t *r = mfm_detect_create();
    mfm_detect_set_physical(r, 512, 9, 2, 80, 1);

    uint8_t boot[512];
    memset(boot, 0, sizeof(boot));
    boot[0] = 0xEB; boot[1] = 0x3C; boot[2] = 0x90;
    memcpy(boot + 3, "MSDOS5.0", 8);
    boot[0x0B] = 0x00; boot[0x0C] = 0x02;
    boot[0x0D] = 0x02;
    boot[0x0E] = 0x01; boot[0x0F] = 0x00;
    boot[0x10] = 0x02;
    boot[0x11] = 0x70; boot[0x12] = 0x00;
    boot[0x13] = 0xA0; boot[0x14] = 0x05;
    boot[0x15] = 0xF9;
    boot[0x16] = 0x03; boot[0x17] = 0x00;
    boot[0x18] = 0x09; boot[0x19] = 0x00;
    boot[0x1A] = 0x02; boot[0x1B] = 0x00;
    boot[0x1FE] = 0x55; boot[0x1FF] = 0xAA;

    mfm_detect_analyze_boot_data(r, boot, 512);
    mfm_sort_candidates(r);

    FILE *null_fp = fopen("/dev/null", "w");
    if (null_fp) {
        mfm_detect_print_report(r, null_fp);
        fclose(null_fp);
    }

    mfm_detect_free(r);
    PASS();
}


/* ==========================================================================
 * Main: Tests oder CLI
 * ========================================================================== */

static void run_tests(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║        MFM DETECT MODULE - TEST SUITE               ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    printf("── BPB / Boot-Sektor ──────────────────────────────────\n");
    test_fat12_bpb();
    test_fat_validation();
    test_amiga_bootblock();
    test_atari_st();

    printf("\n── Physikalisch ───────────────────────────────────────\n");
    test_burst_query();
    test_geometry();

    printf("\n── CP/M ───────────────────────────────────────────────\n");
    test_cpm_directory();
    test_cpm_dpb();
    test_known_cpm();

    printf("\n── Integration ────────────────────────────────────────\n");
    test_boot_analysis_dos();
    test_boot_analysis_amiga();
    test_strings();
    test_report();

    printf("\n══════════════════════════════════════════════════════\n");
    printf("  Ergebnis: %d/%d Tests bestanden", tests_passed, tests_run);
    if (tests_failed > 0) printf(", %d FEHLGESCHLAGEN", tests_failed);
    printf("\n══════════════════════════════════════════════════════\n\n");
}

static void print_usage(const char *prog) {
    printf("Verwendung: %s [Befehl] [Argumente]\n\n", prog);
    printf("Befehle:\n");
    printf("  test              Tests ausführen\n");
    printf("  detect <image>    Image-Datei analysieren\n");
    printf("  burst <hex...>    Burst-Query Bytes analysieren\n");
    printf("  formats           Bekannte CP/M-Formate auflisten\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        run_tests();
        return tests_failed > 0 ? 1 : 0;
    }

    if (strcmp(argv[1], "test") == 0) {
        run_tests();
        return tests_failed > 0 ? 1 : 0;
    }

    if (strcmp(argv[1], "detect") == 0 && argc >= 3) {
        mfm_detect_result_t *r = mfm_detect_create();
        if (!r) { fprintf(stderr, "Fehler: Speicher\n"); return 1; }

        mfm_error_t err = mfm_detect_from_image(argv[2], r);
        if (err != MFM_OK) {
            fprintf(stderr, "Fehler: %s\n", mfm_error_str(err));
            mfm_detect_free(r);
            return 1;
        }

        mfm_detect_print_report(r, stdout);
        mfm_detect_free(r);
        return 0;
    }

    if (strcmp(argv[1], "burst") == 0 && argc >= 3) {
        uint8_t data[8] = {0};
        int len = 0;
        for (int i = 2; i < argc && len < 8; i++) {
            data[len++] = (uint8_t)strtol(argv[i], NULL, 16);
        }

        mfm_detect_result_t *r = mfm_detect_create();
        mfm_error_t err = mfm_detect_from_burst(r, data, len);
        if (err == MFM_ERR_NOT_MFM) {
            printf("GCR-Disk erkannt (kein MFM)\n");
            mfm_detect_free(r);
            return 0;
        }
        if (err != MFM_OK) {
            fprintf(stderr, "Fehler: %s\n", mfm_error_str(err));
            mfm_detect_free(r);
            return 1;
        }

        printf("Burst-Analyse:\n");
        mfm_print_physical(&r->physical, stdout);
        printf("  CP/M Interleave: %u\n", r->burst.cpm_interleave);

        /* Bekannte Formate suchen */
        const mfm_cpm_known_format_t *matches[8];
        uint8_t n = mfm_find_known_cpm_formats(&r->physical, matches, 8);
        if (n > 0) {
            printf("\nMögliche CP/M-Formate:\n");
            for (int i = 0; i < n; i++) {
                printf("  • %s (%s)\n", matches[i]->name, matches[i]->machine);
            }
        }

        mfm_detect_free(r);
        return 0;
    }

    if (strcmp(argv[1], "formats") == 0) {
        printf("\nBekannte CP/M-Formate:\n\n");
        printf("  %-20s %-16s %4s %3s %2s %3s %5s %4s %2s\n",
               "Name", "System", "SecS", "SPT", "H", "Cyl", "Block", "Dir", "BT");
        printf("  ─────────────────── ──────────────── ──── ─── ── ─── ───── ──── ──\n");

        uint16_t count = mfm_get_known_cpm_format_count();
        for (uint16_t i = 0; i < count; i++) {
            const mfm_cpm_known_format_t *f = mfm_get_known_cpm_format(i);
            if (!f) break;
            printf("  %-20s %-16s %4u %3u %2u %3u %5u %4u %2u\n",
                   f->name, f->machine,
                   f->sector_size, f->sectors_per_track,
                   f->heads, f->cylinders,
                   f->block_size, f->dir_entries, f->boot_tracks);
        }
        printf("\nGesamt: %u Formate\n\n", mfm_get_known_cpm_format_count());
        return 0;
    }

    print_usage(argv[0]);
    return 1;
}
