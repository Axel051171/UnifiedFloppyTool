/**
 * @file test_st_geometry.c
 * @brief Atari ST geometry: BPB first, then a size scan that covers the
 *        extended formats (MF-462).
 *
 * Links the real ST plugin (src/formats/st/uft_st.c).
 *
 * Three things are pinned here.
 *
 * 1. **The ambiguous size is resolved by the BPB.** 368,640 bytes is both
 *    80x1x9 and 40x2x9. The plugin's own header comment listed both while the
 *    code silently picked the first one. TOS writes a DOS-compatible BPB at
 *    offset 0x0B of the boot sector; when it says two heads and 40 cylinders,
 *    that is the answer.
 *
 * 2. **Extended ST formats are read at all.** 82 or 83 tracks with 10 or 11
 *    sectors was an everyday ST format. The old size table knew 80 cylinders
 *    and nothing else, so every one of those images was rejected outright.
 *    Range and order follow SAMdisk 4.0 (src/samdisk/st.cpp:47-67).
 *
 * 3. **The six sizes the old table listed still resolve identically.**
 *    Widening the scan must not silently re-interpret images that already
 *    read correctly — that would be the worst possible outcome of a fix.
 *
 * Plus the one positive identification the format offers: a bootable ST disk's
 * boot sector sums to 0x1234 over its 256 big-endian words (samdisk/st.cpp:6).
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_st;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-40s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define SEC 512u

static void get_temp_path(char *path, size_t size, const char *tag) {
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(path, size, "%s/uft_st_%s_%d.st", dir, tag, rand() % 100000);
}

static void put_le16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }

/**
 * Write an ST image of @p cyl x @p heads x @p spt sectors.
 * @param with_bpb   fill the BPB in the boot sector with that same geometry
 * @param bootable   adjust the boot sector so its big-endian words sum to 0x1234
 */
static int build_st(const char *path, int cyl, int heads, int spt,
                    bool with_bpb, bool bootable) {
    uint32_t total = (uint32_t)cyl * heads * spt;
    uint8_t boot[SEC];
    memset(boot, 0, sizeof(boot));

    boot[0] = 0x60; boot[1] = 0x1C;               /* 68000 BRA.S */
    memcpy(&boot[2], "UFTTEST ", 8);
    if (with_bpb) {
        put_le16(&boot[0x0B], (uint16_t)SEC);     /* bytes per sector  */
        boot[0x0D] = 2;                           /* sectors/cluster   */
        put_le16(&boot[0x0E], 1);                 /* reserved sectors  */
        boot[0x10] = 2;                           /* FAT copies        */
        put_le16(&boot[0x11], 112);               /* root dir entries  */
        put_le16(&boot[0x13], (uint16_t)total);   /* total sectors     */
        boot[0x15] = 0xF9;                        /* media descriptor  */
        put_le16(&boot[0x16], 5);                 /* sectors per FAT   */
        put_le16(&boot[0x18], (uint16_t)spt);     /* sectors per track */
        put_le16(&boot[0x1A], (uint16_t)heads);   /* heads             */
    }

    if (bootable) {
        /* Word 255 absorbs whatever is needed to reach 0x1234. */
        uint16_t sum = 0;
        for (size_t i = 0; i < SEC - 2; i += 2)
            sum = (uint16_t)(sum + (((uint16_t)boot[i] << 8) | boot[i + 1]));
        uint16_t last = (uint16_t)(0x1234u - sum);
        boot[SEC - 2] = (uint8_t)(last >> 8);
        boot[SEC - 1] = (uint8_t)last;
    }

    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    fwrite(boot, 1, SEC, f);

    /* Every remaining sector carries its own LBA, so a wrong stride shows up. */
    uint8_t sector[SEC];
    for (uint32_t lba = 1; lba < total; lba++) {
        memset(sector, 0, sizeof(sector));
        sector[0] = (uint8_t)(lba & 0xFF);
        sector[1] = (uint8_t)((lba >> 8) & 0xFF);
        fwrite(sector, 1, SEC, f);
    }
    fclose(f);
    return 1;
}

static int open_geometry(const char *path, uft_disk_t *disk) {
    memset(disk, 0, sizeof(*disk));
    return uft_format_plugin_st.open(disk, path, true) == UFT_OK;
}

TEST(bpb_resolves_the_ambiguous_360k_size) {
    char path[512];

    /* 40 x 2 x 9 = 720 sectors = 368,640 bytes — the very same size as
     * 80 x 1 x 9. Only the BPB can tell them apart. */
    get_temp_path(path, sizeof(path), "bpb40");
    ASSERT(build_st(path, 40, 2, 9, true, false));

    uft_disk_t disk;
    ASSERT(open_geometry(path, &disk));
    ASSERT(disk.geometry.cylinders == 40);
    ASSERT(disk.geometry.heads     == 2);
    ASSERT(disk.geometry.sectors   == 9);
    uft_format_plugin_st.close(&disk);
    remove(path);

    /* Same size, no BPB: the size scan decides, and its documented order
     * gives the single-sided 80-cylinder reading — as before MF-462. */
    get_temp_path(path, sizeof(path), "nobpb40");
    ASSERT(build_st(path, 80, 1, 9, false, false));
    ASSERT(open_geometry(path, &disk));
    ASSERT(disk.geometry.cylinders == 80);
    ASSERT(disk.geometry.heads     == 1);
    ASSERT(disk.geometry.sectors   == 9);
    uft_format_plugin_st.close(&disk);
    remove(path);
}

TEST(extended_formats_are_accepted) {
    /* 82x2x10, 83x2x11 and 81x2x9 — everyday extended ST formats, all
     * rejected outright by the 80-cylinder-only table before MF-462. */
    const int geo[][3] = { {82, 2, 10}, {83, 2, 11}, {81, 2, 9}, {84, 2, 10} };

    for (size_t i = 0; i < sizeof(geo) / sizeof(geo[0]); i++) {
        char path[512], tag[32];
        snprintf(tag, sizeof(tag), "ext%zu", i);
        get_temp_path(path, sizeof(path), tag);
        ASSERT(build_st(path, geo[i][0], geo[i][1], geo[i][2], false, false));

        uft_disk_t disk;
        ASSERT(open_geometry(path, &disk));
        ASSERT(disk.geometry.cylinders == geo[i][0]);
        ASSERT(disk.geometry.heads     == geo[i][1]);
        ASSERT(disk.geometry.sectors   == geo[i][2]);

        /* and the data really is where that geometry says it is */
        uft_track_t tr;
        memset(&tr, 0, sizeof(tr));
        ASSERT(uft_format_plugin_st.read_track(&disk, geo[i][0] - 1, 1, &tr) == UFT_OK);
        ASSERT(tr.sector_count == (size_t)geo[i][2]);
        uint32_t lba = ((uint32_t)(geo[i][0] - 1) * geo[i][1] + 1) * geo[i][2];
        ASSERT(tr.sectors[0].data[0] == (uint8_t)(lba & 0xFF));
        ASSERT(tr.sectors[0].data[1] == (uint8_t)((lba >> 8) & 0xFF));
        for (size_t s = 0; s < tr.sector_count; s++) free(tr.sectors[s].data);
        free(tr.sectors);

        uft_format_plugin_st.close(&disk);
        remove(path);
    }
}

TEST(the_six_legacy_sizes_resolve_unchanged) {
    /* Exactly what the old size table returned. A wider scan that quietly
     * re-reads these would be worse than the gap it closes. */
    const int geo[][3] = {
        {80, 1,  9},   /*   368,640 */
        {80, 1, 10},   /*   409,600 */
        {80, 2,  9},   /*   737,280 */
        {80, 2, 10},   /*   819,200 */
        {80, 2, 18},   /* 1,474,560 */
        {40, 1,  9},   /*   184,320 — the old 40-cylinder fallback */
    };

    for (size_t i = 0; i < sizeof(geo) / sizeof(geo[0]); i++) {
        char path[512], tag[32];
        snprintf(tag, sizeof(tag), "leg%zu", i);
        get_temp_path(path, sizeof(path), tag);
        ASSERT(build_st(path, geo[i][0], geo[i][1], geo[i][2], false, false));

        uft_disk_t disk;
        ASSERT(open_geometry(path, &disk));
        ASSERT(disk.geometry.cylinders == geo[i][0]);
        ASSERT(disk.geometry.heads     == geo[i][1]);
        ASSERT(disk.geometry.sectors   == geo[i][2]);
        uft_format_plugin_st.close(&disk);
        remove(path);
    }
}

TEST(boot_checksum_outranks_a_bare_size_match) {
    char path[512];
    int conf_plain = -1, conf_boot = -1;
    uint8_t head[SEC];

    /* same geometry twice, once with the TOS boot checksum set */
    get_temp_path(path, sizeof(path), "plain");
    ASSERT(build_st(path, 80, 2, 9, false, false));
    FILE *f = fopen(path, "rb");
    ASSERT(f && fread(head, 1, SEC, f) == SEC);
    fclose(f);
    ASSERT(uft_format_plugin_st.probe(head, SEC, 80u * 2 * 9 * SEC, &conf_plain));
    remove(path);

    get_temp_path(path, sizeof(path), "boot");
    ASSERT(build_st(path, 80, 2, 9, false, true));
    f = fopen(path, "rb");
    ASSERT(f && fread(head, 1, SEC, f) == SEC);
    fclose(f);
    ASSERT(uft_format_plugin_st.probe(head, SEC, 80u * 2 * 9 * SEC, &conf_boot));
    remove(path);

    ASSERT(conf_boot == 90);
    ASSERT(conf_boot > conf_plain);
}

int main(void) {
    printf("=== Atari ST geometry: BPB first, extended sizes (MF-462) ===\n");
    RUN(bpb_resolves_the_ambiguous_360k_size);
    RUN(extended_formats_are_accepted);
    RUN(the_six_legacy_sizes_resolve_unchanged);
    RUN(boot_checksum_outranks_a_bare_size_match);
    printf("=== %d passed, %d failed ===\n", _pass, _fail);
    return _fail ? 1 : 0;
}
