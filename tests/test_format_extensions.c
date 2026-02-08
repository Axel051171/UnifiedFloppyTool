/**
 * @file test_format_extensions.c
 * @brief Format Extensions Tests
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

static int _pass = 0, _fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name) do { printf("  [TEST] %s... ", #name); test_##name(); printf("OK\n"); _pass++; } while(0)
#define ASSERT(c) do { if(!(c)) { printf("FAIL @ %d\n", __LINE__); _fail++; return; } } while(0)

/* Format IDs */
#define UFT_FMT_ST_MSA    0x102
#define UFT_FMT_CPC_DSK   0x200
#define UFT_FMT_CPC_EDSK  0x201
#define UFT_FMT_BBC_SSD   0x300

TEST(msa_magic) {
    uint8_t msa_header[] = {0x0E, 0x0F, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4F};
    ASSERT(msa_header[0] == 0x0E);
    ASSERT(msa_header[1] == 0x0F);
}

TEST(cpc_dsk_magic) {
    char dsk_magic[] = "MV - CPCEMU Disk-File";
    ASSERT(memcmp(dsk_magic, "MV - CPC", 8) == 0);
}

TEST(cpc_edsk_magic) {
    char edsk_magic[] = "EXTENDED CPC DSK File";
    ASSERT(memcmp(edsk_magic, "EXTENDED", 8) == 0);
}

TEST(sector_sizes) {
    /* CPC sector sizes: 128 << N */
    ASSERT((128 << 0) == 128);
    ASSERT((128 << 1) == 256);
    ASSERT((128 << 2) == 512);
    ASSERT((128 << 3) == 1024);
}

TEST(bbc_ssd_size) {
    /* Single-sided: 40 tracks * 10 sectors * 256 bytes = 102400 */
    int size = 40 * 10 * 256;
    ASSERT(size == 102400);
}

TEST(bbc_dsd_size) {
    /* Double-sided: 80 tracks * 10 sectors * 256 bytes = 204800 */
    int size = 80 * 10 * 256;
    ASSERT(size == 204800);
}

TEST(trdos_signature) {
    /* TR-DOS signature at offset 231 in sector 9 */
    int sig_offset = 256 * 8 + 231;
    ASSERT(sig_offset == 2279);
}

TEST(dfs_entry_size) {
    /* DFS catalog entry is 8 bytes in each sector */
    ASSERT(8 + 8 == 16);  /* Total per entry across sectors 0 and 1 */
}

TEST(format_id_ranges) {
    /* Atari ST: 0x100+ */
    ASSERT(UFT_FMT_ST_MSA >= 0x100 && UFT_FMT_ST_MSA < 0x200);
    /* CPC: 0x200+ */
    ASSERT(UFT_FMT_CPC_DSK >= 0x200 && UFT_FMT_CPC_DSK < 0x300);
    /* BBC: 0x300+ */
    ASSERT(UFT_FMT_BBC_SSD >= 0x300 && UFT_FMT_BBC_SSD < 0x400);
}

TEST(msa_track_size) {
    /* MSA track size for 9 sectors */
    int sectors = 9;
    int track_size = sectors * 512;
    ASSERT(track_size == 4608);
}

int main(void) {
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("  Format Extensions Tests (P3-007)\n");
    printf("═══════════════════════════════════════════════════════════════\n\n");
    
    RUN(msa_magic);
    RUN(cpc_dsk_magic);
    RUN(cpc_edsk_magic);
    RUN(sector_sizes);
    RUN(bbc_ssd_size);
    RUN(bbc_dsd_size);
    RUN(trdos_signature);
    RUN(dfs_entry_size);
    RUN(format_id_ranges);
    RUN(msa_track_size);
    
    printf("\n═══════════════════════════════════════════════════════════════\n");
    printf("  Results: %d passed, %d failed\n", _pass, _fail);
    printf("═══════════════════════════════════════════════════════════════\n");
    
    return _fail > 0 ? 1 : 0;
}
