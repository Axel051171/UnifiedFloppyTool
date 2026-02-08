/**
 * =============================================================================
 * Polyglot Boot-Sektor Test-Suite & CLI-Tool
 * =============================================================================
 */

#include "polyglot_boot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ═══════════════════════════════════════════════════════════════════════════
 *  Test-Infrastruktur
 * ═══════════════════════════════════════════════════════════════════════════ */

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name)  do { \
    printf("  %-50s ", name); \
    fflush(stdout); \
} while(0)

#define PASS() do { \
    printf("✓\n"); \
    tests_passed++; \
} while(0)

#define FAIL(msg) do { \
    printf("✗ (%s)\n", msg); \
    tests_failed++; \
} while(0)

#define ASSERT_EQ(a, b, msg) do { \
    if ((a) != (b)) { FAIL(msg); return; } \
} while(0)

#define ASSERT_TRUE(cond, msg) do { \
    if (!(cond)) { FAIL(msg); return; } \
} while(0)

/* ═══════════════════════════════════════════════════════════════════════════
 *  Synthetische Boot-Sektoren erstellen
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Standard FAT12 BPB in einen Sektor schreiben.
 * Konfiguriert für 720K DD (9×512, DS, 80 Zylinder).
 */
static void write_bpb_720k(uint8_t *sector)
{
    /* OEM Name */
    memcpy(&sector[0x03], "TEST    ", 8);

    /* Bytes per Sector = 512 */
    sector[0x0B] = 0x00; sector[0x0C] = 0x02;

    /* Sectors per Cluster = 2 */
    sector[0x0D] = 0x02;

    /* Reserved Sectors = 1 */
    sector[0x0E] = 0x01; sector[0x0F] = 0x00;

    /* Number of FATs = 2 */
    sector[0x10] = 0x02;

    /* Root Directory Entries = 112 */
    sector[0x11] = 0x70; sector[0x12] = 0x00;

    /* Total Sectors = 1440 (9×2×80) */
    sector[0x13] = 0xA0; sector[0x14] = 0x05;

    /* Media Descriptor = 0xF9 (720K) */
    sector[0x15] = 0xF9;

    /* Sectors per FAT = 3 */
    sector[0x16] = 0x03; sector[0x17] = 0x00;

    /* Sectors per Track = 9 */
    sector[0x18] = 0x09; sector[0x19] = 0x00;

    /* Heads = 2 */
    sector[0x1A] = 0x02; sector[0x1B] = 0x00;

    /* Hidden Sectors = 0 */
    memset(&sector[0x1C], 0, 4);

    /* Total Sectors 32 = 0 (16-Bit reicht) */
    memset(&sector[0x20], 0, 4);
}

/**
 * Standard FAT12 BPB für 800K Atari ST (10×512, DS, 80 Zylinder).
 */
static void write_bpb_800k(uint8_t *sector)
{
    write_bpb_720k(sector);

    /* Sectors per Track = 10 */
    sector[0x18] = 0x0A;

    /* Total Sectors = 1600 (10×2×80) */
    sector[0x13] = 0x40; sector[0x14] = 0x06;

    /* Sectors per FAT = 3 */
    sector[0x16] = 0x03;

    /* Media Descriptor = 0xF8 */
    sector[0x15] = 0xF8;
}

/** PC Boot-Sektor erstellen (Short JMP + NOP + BPB + 0x55AA) */
static void create_pc_boot(uint8_t *sector)
{
    memset(sector, 0, 512);

    /* Short JMP to offset 0x3E, NOP */
    sector[0] = 0xEB;
    sector[1] = 0x3C;
    sector[2] = 0x90;

    write_bpb_720k(sector);

    /* Boot-Signatur */
    sector[0x1FE] = 0x55;
    sector[0x1FF] = 0xAA;
}

/** Atari ST Boot-Sektor erstellen (BRA.S + BPB + Checksum) */
static void create_st_boot(uint8_t *sector, bool bootable)
{
    memset(sector, 0, 512);

    /* 68000 BRA.S to offset 0x3A (0x60 + displacement 0x38) */
    sector[0] = 0x60;
    sector[1] = 0x38;

    /* OEM: "Loader" (typisch für ST) */
    memcpy(&sector[0x03], "Loader  ", 8);

    write_bpb_720k(sector);

    /* Seriennummer (Offset 0x08, überschreibt Teil von OEM) */
    sector[0x08] = 0x12;
    sector[0x09] = 0x34;
    sector[0x0A] = 0x56;

    if (bootable) {
        /* Checksum auf 0x1234 setzen.
         * Berechne aktuelle Summe, setze letztes Word so dass Summe = 0x1234 */
        sector[0x1FE] = 0;
        sector[0x1FF] = 0;

        uint32_t sum = 0;
        for (int i = 0; i < 510; i += 2) {
            sum += ((uint16_t)sector[i] << 8) | sector[i+1];
        }
        uint16_t needed = 0x1234 - (uint16_t)(sum & 0xFFFF);
        sector[0x1FE] = (uint8_t)(needed >> 8);
        sector[0x1FF] = (uint8_t)(needed & 0xFF);
    }
}

/** Amiga OFS Bootblock erstellen (2 Sektoren) */
static void create_amiga_boot(uint8_t *sector0, uint8_t *sector1, uint8_t fs_type)
{
    memset(sector0, 0, 512);
    memset(sector1, 0, 512);

    /* Magic: "DOS\x" */
    sector0[0] = 'D';
    sector0[1] = 'O';
    sector0[2] = 'S';
    sector0[3] = fs_type;

    /* Rootblock = 880 (Mitte der 1760 Sektoren) */
    sector0[8] = 0x00;
    sector0[9] = 0x00;
    sector0[10] = 0x03;
    sector0[11] = 0x70; /* 880 */

    /* Checksum berechnen (Feld bei Offset 4 = 0 für Berechnung) */
    sector0[4] = sector0[5] = sector0[6] = sector0[7] = 0;

    uint32_t sum = 0;
    for (int i = 0; i < 512; i += 4) {
        uint32_t old = sum;
        sum += ((uint32_t)sector0[i] << 24) | ((uint32_t)sector0[i+1] << 16) |
               ((uint32_t)sector0[i+2] << 8) | sector0[i+3];
        if (sum < old) sum++;
    }
    for (int i = 0; i < 512; i += 4) {
        uint32_t old = sum;
        sum += ((uint32_t)sector1[i] << 24) | ((uint32_t)sector1[i+1] << 16) |
               ((uint32_t)sector1[i+2] << 8) | sector1[i+3];
        if (sum < old) sum++;
    }

    /* Checksum = -sum (damit Gesamt = 0) */
    uint32_t cksum = (uint32_t)(-(int32_t)sum);
    sector0[4] = (uint8_t)(cksum >> 24);
    sector0[5] = (uint8_t)(cksum >> 16);
    sector0[6] = (uint8_t)(cksum >> 8);
    sector0[7] = (uint8_t)(cksum);
}

/** Polyglot PC+ST Boot-Sektor: PC JMP + gültiger BPB, ST-kompatibel */
static void create_pc_st_dual(uint8_t *sector)
{
    memset(sector, 0, 512);

    /* PC Short JMP */
    sector[0] = 0xEB;
    sector[1] = 0x3C;
    sector[2] = 0x90;

    write_bpb_720k(sector);

    /* PC Signatur */
    sector[0x1FE] = 0x55;
    sector[0x1FF] = 0xAA;
}

/** Polyglot ST+Amiga Dual-Format Boot-Sektor:
 *  Track 0 = ST BRA.S + FAT12 BPB (für ST/PC)
 *  Amiga-Daten auf separaten Tracks.
 *  Hier: Der Boot-Sektor von Track 0 (Standard-MFM) */
static void create_st_amiga_dual(uint8_t *sector, bool bootable)
{
    /* ST-Format Boot-Sektor mit BRA.S + BPB */
    create_st_boot(sector, bootable);

    /* Aber: Reduzierte Sektorzahl im BPB, da nicht alle Tracks FAT12 sind.
     * Auf einer Dual-Disk nutzt FAT12 z.B. nur 40 der 80 Zylinder. */
    uint16_t fat_sectors = 720; /* Nur ~50% der Disk für FAT12 */
    sector[0x13] = (uint8_t)(fat_sectors & 0xFF);
    sector[0x14] = (uint8_t)(fat_sectors >> 8);

    /* Root Dir kleiner */
    sector[0x11] = 64;
    sector[0x12] = 0;

    /* Sectors per FAT anpassen */
    sector[0x16] = 0x02;
    sector[0x17] = 0x00;

    if (bootable) {
        /* Checksum korrigieren */
        sector[0x1FE] = 0;
        sector[0x1FF] = 0;
        uint32_t sum = 0;
        for (int i = 0; i < 510; i += 2) {
            sum += ((uint16_t)sector[i] << 8) | sector[i+1];
        }
        uint16_t needed = 0x1234 - (uint16_t)(sum & 0xFFFF);
        sector[0x1FE] = (uint8_t)(needed >> 8);
        sector[0x1FF] = (uint8_t)(needed & 0xFF);
    }
}

/** Triple-Format Boot-Sektor: PC JMP + ST-kompatibel + BPB
 *  (Amiga-Daten auf separaten Tracks) */
static void create_triple_format(uint8_t *sector)
{
    memset(sector, 0, 512);

    /* PC Short JMP (auch auf 68000 harmlos - BCLR Opcode) */
    sector[0] = 0xEB;
    sector[1] = 0x3C;
    sector[2] = 0x90;

    write_bpb_720k(sector);

    /* Reduzierte Sektorzahl für Dual/Triple */
    uint16_t fat_sectors = 720;
    sector[0x13] = (uint8_t)(fat_sectors & 0xFF);
    sector[0x14] = (uint8_t)(fat_sectors >> 8);

    /* PC Signatur */
    sector[0x1FE] = 0x55;
    sector[0x1FF] = 0xAA;
}

/** MSX-DOS Boot-Sektor */
static void create_msx_boot(uint8_t *sector)
{
    memset(sector, 0, 512);

    /* MSX-DOS verwendet auch PC-JMP-Format */
    sector[0] = 0xEB;
    sector[1] = 0xFE;
    sector[2] = 0x90;

    write_bpb_720k(sector);

    /* MSX OEM — NACH write_bpb_720k, da diese OEM überschreibt */
    memcpy(&sector[0x03], "MSX_DOS ", 8);

    /* MSX: 0x55AA optional */
    sector[0x1FE] = 0x55;
    sector[0x1FF] = 0xAA;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_bpb_parse(void)
{
    TEST("BPB Parse (720K)");
    uint8_t sector[512];
    create_pc_boot(sector);

    poly_bpb_t bpb;
    bool ok = poly_parse_bpb(sector, &bpb);

    ASSERT_TRUE(ok, "BPB ungültig");
    ASSERT_EQ(bpb.bytes_per_sector, 512, "BPS");
    ASSERT_EQ(bpb.sectors_per_cluster, 2, "SPC");
    ASSERT_EQ(bpb.num_fats, 2, "FATs");
    ASSERT_EQ(bpb.root_dir_entries, 112, "RDE");
    ASSERT_EQ(bpb.total_sectors_16, 1440, "Total");
    ASSERT_EQ(bpb.sectors_per_track, 9, "SPT");
    ASSERT_EQ(bpb.num_heads, 2, "Heads");
    ASSERT_EQ(bpb.media_descriptor, 0xF9, "Media");
    PASS();
}

static void test_bpb_invalid(void)
{
    TEST("BPB Parse (ungültig)");
    uint8_t sector[512];
    memset(sector, 0, 512);

    poly_bpb_t bpb;
    bool ok = poly_parse_bpb(sector, &bpb);
    ASSERT_TRUE(!ok, "Sollte ungültig sein");
    PASS();
}

static void test_atari_checksum(void)
{
    TEST("Atari ST Checksum (bootbar)");
    uint8_t sector[512];
    create_st_boot(sector, true);

    uint16_t cksum = poly_atari_checksum(sector);
    ASSERT_EQ(cksum, 0x1234, "Checksum != 0x1234");
    PASS();
}

static void test_atari_checksum_nonboot(void)
{
    TEST("Atari ST Checksum (nicht bootbar)");
    uint8_t sector[512];
    create_st_boot(sector, false);

    uint16_t cksum = poly_atari_checksum(sector);
    ASSERT_TRUE(cksum != 0x1234, "Sollte nicht 0x1234 sein");
    PASS();
}

static void test_pc_boot(void)
{
    TEST("PC Boot-Sektor Erkennung");
    uint8_t sector[512];
    create_pc_boot(sector);

    poly_result_t r;
    poly_analyze_boot_sector(sector, &r);

    ASSERT_TRUE(r.pc.valid, "PC nicht erkannt");
    ASSERT_TRUE(r.pc.has_jmp, "JMP fehlt");
    ASSERT_TRUE(r.pc.has_55aa, "0x55AA fehlt");
    ASSERT_TRUE(r.platforms & POLY_PLATFORM_PC, "PC-Flag fehlt");
    ASSERT_TRUE(r.bpb.valid, "BPB ungültig");
    PASS();
}

static void test_st_boot(void)
{
    TEST("Atari ST Boot-Sektor Erkennung");
    uint8_t sector[512];
    create_st_boot(sector, true);

    poly_result_t r;
    poly_analyze_boot_sector(sector, &r);

    ASSERT_TRUE(r.atari.valid, "ST nicht erkannt");
    ASSERT_EQ(r.atari.cksum_status, POLY_ST_CKSUM_BOOT, "Nicht bootbar");
    ASSERT_TRUE(r.platforms & POLY_PLATFORM_ATARI_ST, "ST-Flag fehlt");
    ASSERT_TRUE(r.bpb.valid, "BPB ungültig");
    ASSERT_EQ(r.boot_sector[0], 0x60, "BRA.S fehlt");
    PASS();
}

static void test_amiga_ofs(void)
{
    TEST("Amiga OFS Bootblock Erkennung");
    uint8_t s0[512], s1[512];
    create_amiga_boot(s0, s1, 0); /* OFS */

    poly_result_t r;
    poly_analyze_boot_extended(s0, s1, &r);

    ASSERT_TRUE(r.amiga.valid, "Amiga nicht erkannt");
    ASSERT_TRUE(!r.amiga.is_ffs, "Sollte OFS sein");
    ASSERT_TRUE(r.platforms & POLY_PLATFORM_AMIGA, "Amiga-Flag fehlt");
    ASSERT_EQ(r.amiga.root_block, 880, "Rootblock != 880");
    PASS();
}

static void test_amiga_ffs(void)
{
    TEST("Amiga FFS Bootblock Erkennung");
    uint8_t s0[512], s1[512];
    create_amiga_boot(s0, s1, 1); /* FFS */

    poly_result_t r;
    poly_analyze_boot_extended(s0, s1, &r);

    ASSERT_TRUE(r.amiga.valid, "Amiga nicht erkannt");
    ASSERT_TRUE(r.amiga.is_ffs, "Sollte FFS sein");
    ASSERT_EQ(r.boot_type, POLY_BOOT_AMIGA_FFS, "Falscher Boot-Typ");
    PASS();
}

static void test_amiga_intl_ffs(void)
{
    TEST("Amiga International FFS Erkennung");
    uint8_t s0[512], s1[512];
    create_amiga_boot(s0, s1, 3); /* International FFS */

    poly_result_t r;
    poly_analyze_boot_extended(s0, s1, &r);

    ASSERT_TRUE(r.amiga.valid, "Amiga nicht erkannt");
    ASSERT_TRUE(r.amiga.is_ffs, "Sollte FFS sein");
    ASSERT_TRUE(r.amiga.is_intl, "Sollte International sein");
    PASS();
}

static void test_dual_pc_st(void)
{
    TEST("Dual-Format PC + Atari ST");
    uint8_t sector[512];
    create_pc_st_dual(sector);

    poly_result_t r;
    poly_analyze_boot_sector(sector, &r);

    ASSERT_TRUE(r.pc.valid, "PC nicht erkannt");
    ASSERT_TRUE(r.atari.valid, "ST nicht erkannt");
    ASSERT_TRUE(r.platforms & POLY_PLATFORM_PC, "PC fehlt");
    ASSERT_TRUE(r.platforms & POLY_PLATFORM_ATARI_ST, "ST fehlt");
    ASSERT_EQ(r.layout, POLY_LAYOUT_DUAL, "Kein Dual-Layout");
    ASSERT_EQ(r.boot_type, POLY_BOOT_POLYGLOT, "Nicht polyglot");
    ASSERT_TRUE(r.platform_count == 2, "Nicht 2 Plattformen");
    PASS();
}

static void test_dual_st_amiga(void)
{
    TEST("Dual-Format Atari ST + Amiga (Track 0)");
    uint8_t sector[512];
    create_st_amiga_dual(sector, true);

    poly_result_t r;
    poly_analyze_boot_sector(sector, &r);

    /* Track 0 Sektor 0 ist ST-Format (BRA.S + BPB).
     * Amiga-Erkennung kommt hier nur über Track-Analyse.
     * Aber der ST-Teil sollte korrekt sein. */
    ASSERT_TRUE(r.atari.valid, "ST nicht erkannt");
    ASSERT_EQ(r.atari.cksum_status, POLY_ST_CKSUM_BOOT, "Nicht bootbar");
    ASSERT_TRUE(r.bpb.valid, "BPB ungültig");
    PASS();
}

static void test_triple_format(void)
{
    TEST("Triple-Format PC + ST + Amiga");
    uint8_t sector[512];
    create_triple_format(sector);

    poly_result_t r;
    poly_analyze_boot_sector(sector, &r);

    /* PC und ST sollten beide erkannt werden */
    ASSERT_TRUE(r.pc.valid, "PC nicht erkannt");
    ASSERT_TRUE(r.atari.valid, "ST nicht erkannt");
    ASSERT_TRUE(r.pc.has_55aa, "0x55AA fehlt");
    ASSERT_TRUE(r.platforms & POLY_PLATFORM_PC, "PC fehlt");
    ASSERT_TRUE(r.platforms & POLY_PLATFORM_ATARI_ST, "ST fehlt");
    /* Amiga wird nicht im Boot-Sektor erkannt (separate Tracks),
     * aber PC+ST = mindestens Dual */
    ASSERT_TRUE(r.layout >= POLY_LAYOUT_DUAL, "Mindestens Dual");
    PASS();
}

static void test_msx_dos(void)
{
    TEST("MSX-DOS Erkennung");
    uint8_t sector[512];
    create_msx_boot(sector);

    poly_result_t r;
    poly_analyze_boot_sector(sector, &r);

    ASSERT_TRUE(r.pc.valid, "PC nicht erkannt");
    ASSERT_TRUE(r.platforms & POLY_PLATFORM_MSX, "MSX fehlt");
    ASSERT_TRUE(r.bpb.valid, "BPB ungültig");
    PASS();
}

static void test_st_800k(void)
{
    TEST("Atari ST 800K (10 Sektoren/Spur)");
    uint8_t sector[512];
    memset(sector, 0, 512);

    sector[0] = 0x60;
    sector[1] = 0x38;
    memcpy(&sector[0x03], "Loader  ", 8);
    write_bpb_800k(sector);

    poly_result_t r;
    poly_analyze_boot_sector(sector, &r);

    ASSERT_TRUE(r.atari.valid, "ST nicht erkannt");
    ASSERT_TRUE(r.bpb.valid, "BPB ungültig");
    ASSERT_EQ(r.bpb.sectors_per_track, 10, "SPT != 10");
    ASSERT_EQ(r.geometry.cylinders, 80, "Cyl != 80");
    PASS();
}

static void test_amiga_track_detect(void)
{
    TEST("Amiga Track Sync-Word Erkennung");

    /* Simuliere MFM-Rohdaten mit Amiga Sync-Words (0x4489) */
    uint8_t track[12000];
    memset(track, 0xAA, sizeof(track)); /* MFM-Idle */

    /* 11 Sync-Words platzieren (wie bei echtem Amiga-Track) */
    for (int i = 0; i < 11; i++) {
        int offset = 200 + i * 1088; /* ~1088 Bytes pro Sektor */
        if (offset + 1 < (int)sizeof(track)) {
            track[offset] = 0x44;
            track[offset + 1] = 0x89;
        }
    }

    bool is_amiga = poly_check_amiga_track(track, sizeof(track));
    ASSERT_TRUE(is_amiga, "Amiga-Track nicht erkannt");
    PASS();
}

static void test_non_amiga_track(void)
{
    TEST("Standard-MFM Track (kein Amiga)");

    /* Normaler MFM-Track ohne Amiga-Syncs */
    uint8_t track[6400];
    memset(track, 0x4E, sizeof(track)); /* Gap-Bytes */

    /* A1 A1 A1 FE (Standard MFM IDAM) - kein 0x4489 */
    for (int i = 0; i < 9; i++) {
        int offset = 100 + i * 700;
        track[offset] = 0xA1;
        track[offset+1] = 0xA1;
        track[offset+2] = 0xA1;
        track[offset+3] = 0xFE;
    }

    bool is_amiga = poly_check_amiga_track(track, sizeof(track));
    ASSERT_TRUE(!is_amiga, "Fälschlich als Amiga erkannt");
    PASS();
}

static void test_geometry_720k(void)
{
    TEST("Geometrie-Ableitung (720K)");
    uint8_t sector[512];
    create_pc_boot(sector);

    poly_result_t r;
    poly_analyze_boot_sector(sector, &r);

    ASSERT_EQ(r.geometry.cylinders, 80, "Cyl != 80");
    ASSERT_EQ(r.geometry.heads, 2, "Heads != 2");
    ASSERT_EQ(r.geometry.spt, 9, "SPT != 9");
    ASSERT_EQ(r.geometry.total_bytes, 1440 * 512, "Size falsch");
    PASS();
}

static void test_platform_strings(void)
{
    TEST("Plattform-String Konvertierung");
    char buf[128];

    poly_platforms_str(POLY_PLATFORM_PC, buf, sizeof(buf));
    ASSERT_TRUE(strstr(buf, "PC") != NULL, "PC fehlt");

    poly_platforms_str(POLY_PLATFORM_PC | POLY_PLATFORM_ATARI_ST,
                       buf, sizeof(buf));
    ASSERT_TRUE(strstr(buf, "PC") != NULL, "PC fehlt");
    ASSERT_TRUE(strstr(buf, "Atari") != NULL, "Atari fehlt");
    ASSERT_TRUE(strstr(buf, "+") != NULL, "+ fehlt");

    poly_platforms_str(0, buf, sizeof(buf));
    ASSERT_TRUE(strlen(buf) > 0, "Leer");
    PASS();
}

static void test_confidence(void)
{
    TEST("Konfidenz-Bewertung");

    /* PC mit 0x55AA → höchste Konfidenz */
    uint8_t pc[512];
    create_pc_boot(pc);
    poly_result_t r1;
    poly_analyze_boot_sector(pc, &r1);
    ASSERT_TRUE(r1.confidence >= 90, "PC Konfidenz zu niedrig");

    /* Dual-Format → ebenfalls hoch */
    uint8_t dual[512];
    create_pc_st_dual(dual);
    poly_result_t r2;
    poly_analyze_boot_sector(dual, &r2);
    ASSERT_TRUE(r2.confidence >= 85, "Dual Konfidenz zu niedrig");

    PASS();
}

static void test_empty_sector(void)
{
    TEST("Leerer Sektor (alle Nullen)");
    uint8_t sector[512];
    memset(sector, 0, 512);

    poly_result_t r;
    poly_analyze_boot_sector(sector, &r);

    ASSERT_EQ(r.platforms, POLY_PLATFORM_NONE, "Sollte leer sein");
    ASSERT_EQ(r.confidence, 0, "Konfidenz sollte 0 sein");
    PASS();
}

static void test_e5_sector(void)
{
    TEST("Gelöschter Sektor (alle 0xE5)");
    uint8_t sector[512];
    memset(sector, 0xE5, 512);

    poly_result_t r;
    poly_analyze_boot_sector(sector, &r);

    ASSERT_EQ(r.platforms, POLY_PLATFORM_NONE, "Sollte leer sein");
    PASS();
}

static void test_report_output(void)
{
    TEST("Report-Ausgabe (Smoke-Test)");

    uint8_t sector[512];
    create_pc_st_dual(sector);

    poly_result_t r;
    poly_analyze_boot_sector(sector, &r);

    /* Report erzeugen → darf nicht crashen */
    FILE *devnull = fopen("/dev/null", "w");
    if (devnull) {
        poly_print_report(&r, devnull);
        fclose(devnull);
    }
    PASS();
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Test-Runner
 * ═══════════════════════════════════════════════════════════════════════════ */

static void run_tests(void)
{
    printf("\n══════════════════════════════════════════════════════\n");
    printf("  Polyglot Boot-Sektor Tests\n");
    printf("══════════════════════════════════════════════════════\n\n");

    printf("── BPB & Checksum ─────────────────────────────────────\n");
    test_bpb_parse();
    test_bpb_invalid();
    test_atari_checksum();
    test_atari_checksum_nonboot();

    printf("\n── Einzelne Plattformen ────────────────────────────────\n");
    test_pc_boot();
    test_st_boot();
    test_st_800k();
    test_amiga_ofs();
    test_amiga_ffs();
    test_amiga_intl_ffs();
    test_msx_dos();

    printf("\n── Multi-Plattform (Polyglot) ──────────────────────────\n");
    test_dual_pc_st();
    test_dual_st_amiga();
    test_triple_format();

    printf("\n── Track-Level Erkennung ───────────────────────────────\n");
    test_amiga_track_detect();
    test_non_amiga_track();

    printf("\n── Sonstige ───────────────────────────────────────────\n");
    test_geometry_720k();
    test_platform_strings();
    test_confidence();
    test_empty_sector();
    test_e5_sector();
    test_report_output();

    printf("\n══════════════════════════════════════════════════════\n");
    printf("  Ergebnis: %d/%d Tests bestanden\n",
           tests_passed, tests_passed + tests_failed);
    printf("══════════════════════════════════════════════════════\n\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  CLI-Tool
 * ═══════════════════════════════════════════════════════════════════════════ */

static void print_usage(const char *prog)
{
    printf("\nVerwendung: %s <Befehl> [Argumente]\n\n", prog);
    printf("Befehle:\n");
    printf("  test                  Tests ausführen\n");
    printf("  analyze <image>       Boot-Sektor aus Image analysieren\n");
    printf("  demo                  Alle Demo-Boot-Sektoren analysieren\n");
}

static int cmd_analyze(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Fehler: Kann '%s' nicht öffnen\n", path);
        return 1;
    }

    uint8_t sector0[512], sector1[512];

    if (fread(sector0, 1, 512, f) != 512) {
        fprintf(stderr, "Fehler: Kann Boot-Sektor nicht lesen\n");
        fclose(f);
        return 1;
    }

    /* Versuche zweiten Sektor zu lesen (für Amiga) */
    bool has_sector1 = (fread(sector1, 1, 512, f) == 512);
    fclose(f);

    poly_result_t r;
    if (has_sector1) {
        poly_analyze_boot_extended(sector0, sector1, &r);
    } else {
        poly_analyze_boot_sector(sector0, &r);
    }

    poly_print_report(&r, stdout);
    return 0;
}

static void cmd_demo(void)
{
    uint8_t sector[512], sector1[512];
    poly_result_t r;

    printf("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("  Demo: Verschiedene Boot-Sektor Typen\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

    /* 1. PC 720K */
    printf("\n▸ PC/DOS 720K Boot-Sektor:\n");
    create_pc_boot(sector);
    poly_analyze_boot_sector(sector, &r);
    poly_print_report(&r, stdout);

    /* 2. Atari ST bootbar */
    printf("\n▸ Atari ST Boot-Sektor (bootbar):\n");
    create_st_boot(sector, true);
    poly_analyze_boot_sector(sector, &r);
    poly_print_report(&r, stdout);

    /* 3. Amiga OFS */
    printf("\n▸ Amiga OFS Bootblock:\n");
    create_amiga_boot(sector, sector1, 0);
    poly_analyze_boot_extended(sector, sector1, &r);
    poly_print_report(&r, stdout);

    /* 4. Dual PC+ST */
    printf("\n▸ Dual-Format PC + Atari ST:\n");
    create_pc_st_dual(sector);
    poly_analyze_boot_sector(sector, &r);
    poly_print_report(&r, stdout);

    /* 5. Triple-Format */
    printf("\n▸ Triple-Format PC + ST + Amiga:\n");
    create_triple_format(sector);
    poly_analyze_boot_sector(sector, &r);
    poly_print_report(&r, stdout);

    /* 6. MSX-DOS */
    printf("\n▸ MSX-DOS Boot-Sektor:\n");
    create_msx_boot(sector);
    poly_analyze_boot_sector(sector, &r);
    poly_print_report(&r, stdout);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Main
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(int argc, char *argv[])
{
    if (argc < 2) {
        run_tests();
        return tests_failed > 0 ? 1 : 0;
    }

    if (strcmp(argv[1], "test") == 0) {
        run_tests();
        return tests_failed > 0 ? 1 : 0;
    }

    if (strcmp(argv[1], "analyze") == 0 && argc >= 3) {
        return cmd_analyze(argv[2]);
    }

    if (strcmp(argv[1], "demo") == 0) {
        cmd_demo();
        return 0;
    }

    print_usage(argv[0]);
    return 1;
}
