/**
 * @file test_atx_layout.c
 * @brief ATX track/chunk layout against the a8rawconv oracle (MF-467).
 *
 * Links the real ATX plugin (src/formats/atx/uft_atx.c).
 *
 * Authority: `src/a8rawconv/diskatx.cpp` by Avery Lee (GPL-2-or-later,
 * reference oracle only — see src/a8rawconv/README.md). Its writer
 * (`write_atx`, :216-416) pins the layout byte for byte, and its reader
 * (`read_atx`, :64-190) confirms how it is consumed:
 *
 *   file header, 48 bytes                    "AT8X", density at 0x12
 *   track records, one after another         starting right after the header
 *     track header, 32 bytes
 *       0x00 size   0x08 track number   0x0A sector count
 *       0x10 flags  0x14 data offset (= 32)
 *     chunks, from track+data_offset, each 8 bytes of header:
 *       0x00 size   0x04 type   0x05 num   0x06 data
 *       type 0x01 sector list   n x 8-byte sector headers
 *       type 0x00 sector data   n x 128 bytes
 *       type 0x10 weak bits     num = sector index, data = first weak byte
 *       type 0x11 ext sector    num = sector index, data = size code
 *       size 0    end of chunk list
 *     sector header, 8 bytes
 *       0x00 index  0x01 FDC status  0x02 timing  0x04 data offset
 *
 * There is **no track-offset table** anywhere in the format, and no track
 * count in the file header. The reader used to assume both.
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_atx;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-42s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define ATX_SEC   128
#define ATX_TRACKS  3
#define ATX_SECS    4        /* sectors per track in the fixture */

/* FDC status bits, from write_atx (:333-364) */
#define FDC_DRQ_LOST   0x06  /* long sector: lost data + DRQ */
#define FDC_DATA_CRC   0x08
#define FDC_MISSING    0x10
#define FDC_DELETED    0x20
#define FDC_WEAK       0x40

static void get_temp_path(char *path, size_t size, const char *tag)
{
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(path, size, "%s/uft_atx_%s_%d.atx", dir, tag, rand() % 100000);
}

static void put_le16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put_le32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

/* fdc[] gives the FDC status of each sector; weak_at[] the first weak byte
 * (or -1). Sector i of track t carries the byte 0x10*t + i in every position,
 * so a wrong offset cannot look plausible. */
static int build_atx(const char *path, const uint8_t *fdc, const int *weak_at)
{
    FILE *f = fopen(path, "wb");
    if (!f) return 0;

    uint8_t hdr[48];
    memset(hdr, 0, sizeof(hdr));
    memcpy(hdr, "AT8X", 4);
    put_le16(&hdr[0x04], 1);          /* version major */
    put_le16(&hdr[0x06], 1);          /* version minor */
    put_le16(&hdr[0x08], 0x5241);     /* creator 'AR' */
    hdr[0x12] = 0;                    /* density: 0 = single */
    put_le32(&hdr[0x1C], 48);         /* track data offset */
    fwrite(hdr, 1, sizeof(hdr), f);

    for (int t = 0; t < ATX_TRACKS; t++) {
        int weak_count = 0;
        for (int s = 0; s < ATX_SECS; s++)
            if (weak_at[s] >= 0) weak_count++;

        uint32_t rec_size = 32u                    /* track header      */
                          + 8 + 8 * ATX_SECS       /* sector list chunk */
                          + 8 + ATX_SEC * ATX_SECS /* sector data chunk */
                          + 8u * (uint32_t)weak_count
                          + 8u;                    /* terminator        */

        uint8_t th[32];
        memset(th, 0, sizeof(th));
        put_le32(&th[0x00], rec_size);
        put_le16(&th[0x04], 0);            /* type 0 = track record */
        th[0x08] = (uint8_t)t;             /* track number */
        put_le16(&th[0x0A], ATX_SECS);     /* sector count */
        put_le32(&th[0x10], 0);            /* flags: FM */
        put_le32(&th[0x14], 32);           /* chunks start after the header */
        fwrite(th, 1, sizeof(th), f);

        /* sector list chunk */
        uint8_t ch[8];
        memset(ch, 0, sizeof(ch));
        put_le32(&ch[0], 8 + 8 * ATX_SECS);
        ch[4] = 0x01;
        fwrite(ch, 1, sizeof(ch), f);

        uint32_t data_base = 32u + 8u + 8u * ATX_SECS + 8u;
        for (int s = 0; s < ATX_SECS; s++) {
            uint8_t sh[8];
            memset(sh, 0, sizeof(sh));
            sh[0] = (uint8_t)(s + 1);                   /* 1-based sector id */
            sh[1] = fdc[s];
            put_le16(&sh[2], (uint16_t)(s * 1000));     /* timing */
            put_le32(&sh[4], data_base + (uint32_t)s * ATX_SEC);
            fwrite(sh, 1, sizeof(sh), f);
        }

        /* sector data chunk */
        memset(ch, 0, sizeof(ch));
        put_le32(&ch[0], 8 + ATX_SEC * ATX_SECS);
        ch[4] = 0x00;
        fwrite(ch, 1, sizeof(ch), f);

        for (int s = 0; s < ATX_SECS; s++) {
            uint8_t payload[ATX_SEC];
            memset(payload, (uint8_t)(0x10 * t + s), sizeof(payload));
            fwrite(payload, 1, sizeof(payload), f);
        }

        /* weak chunks */
        for (int s = 0; s < ATX_SECS; s++) {
            if (weak_at[s] < 0) continue;
            memset(ch, 0, sizeof(ch));
            put_le32(&ch[0], 8);
            ch[4] = 0x10;
            ch[5] = (uint8_t)s;
            put_le16(&ch[6], (uint16_t)weak_at[s]);
            fwrite(ch, 1, sizeof(ch), f);
        }

        /* end of chunk list */
        memset(ch, 0, sizeof(ch));
        fwrite(ch, 1, sizeof(ch), f);
    }

    fclose(f);
    return 1;
}

static void free_track(uft_track_t *tr)
{
    for (size_t i = 0; i < tr->sector_count; i++) {
        free(tr->sectors[i].data);
        free(tr->sectors[i].weak_mask);
    }
    free(tr->sectors);
    tr->sectors = NULL; tr->sector_count = 0;
}

TEST(track_records_are_walked_and_sectors_delivered)
{
    char path[512];
    const uint8_t fdc[ATX_SECS]  = { 0, 0, 0, 0 };
    const int weak_at[ATX_SECS]  = { -1, -1, -1, -1 };

    get_temp_path(path, sizeof(path), "plain");
    ASSERT(build_atx(path, fdc, weak_at));

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    ASSERT(uft_format_plugin_atx.open(&disk, path, true) == UFT_OK);
    ASSERT(disk.geometry.cylinders == ATX_TRACKS);
    ASSERT(disk.geometry.heads == 1);
    ASSERT(disk.geometry.sector_size == ATX_SEC);

    /* The last track is the interesting one: every offset error upstream
     * lands somewhere else by the time we get here. */
    uft_track_t tr;
    memset(&tr, 0, sizeof(tr));
    ASSERT(uft_format_plugin_atx.read_track(&disk, ATX_TRACKS - 1, 0, &tr) == UFT_OK);
    ASSERT(tr.sector_count == ATX_SECS);

    for (size_t s = 0; s < tr.sector_count; s++) {
        ASSERT(tr.sectors[s].id.sector == (uint8_t)(s + 1));  /* Atari: 1..18 */
        ASSERT(tr.sectors[s].data != NULL);
        ASSERT(tr.sectors[s].data_len == ATX_SEC);
        ASSERT(tr.sectors[s].data[0] == (uint8_t)(0x10 * (ATX_TRACKS - 1) + (int)s));
        ASSERT(tr.sectors[s].data[ATX_SEC - 1] == tr.sectors[s].data[0]);
        ASSERT(tr.sectors[s].crc_ok);
    }
    free_track(&tr);

    /* track 0 must not be a copy of track 2 */
    memset(&tr, 0, sizeof(tr));
    ASSERT(uft_format_plugin_atx.read_track(&disk, 0, 0, &tr) == UFT_OK);
    ASSERT(tr.sector_count == ATX_SECS);
    ASSERT(tr.sectors[0].data[0] == 0x00);
    free_track(&tr);

    uft_format_plugin_atx.close(&disk);
    remove(path);
}

TEST(fdc_status_becomes_sector_status)
{
    char path[512];
    /* one clean, one data-CRC error, one deleted, one missing data field */
    const uint8_t fdc[ATX_SECS] = { 0, FDC_DATA_CRC, FDC_DELETED, FDC_MISSING };
    const int weak_at[ATX_SECS] = { -1, -1, -1, -1 };

    get_temp_path(path, sizeof(path), "fdc");
    ASSERT(build_atx(path, fdc, weak_at));

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    ASSERT(uft_format_plugin_atx.open(&disk, path, true) == UFT_OK);

    uft_track_t tr;
    memset(&tr, 0, sizeof(tr));
    ASSERT(uft_format_plugin_atx.read_track(&disk, 1, 0, &tr) == UFT_OK);
    ASSERT(tr.sector_count == ATX_SECS);

    ASSERT(tr.sectors[0].crc_ok  == true);
    ASSERT(tr.sectors[0].deleted == false);

    ASSERT(tr.sectors[1].crc_ok  == false);            /* 0x08: data CRC */
    ASSERT((tr.sectors[1].status & UFT_SECTOR_CRC_ERROR) != 0);

    ASSERT(tr.sectors[2].deleted == true);             /* 0x20 */
    ASSERT(tr.sectors[2].data_mark == 0xF8);
    ASSERT((tr.sectors[2].status & UFT_SECTOR_DELETED) != 0);

    /* 0x10: the sector header exists, the data field does not. Zeros must
     * not be handed over as if they had been read. */
    ASSERT((tr.sectors[3].status & UFT_SECTOR_MISSING) != 0);

    free_track(&tr);
    uft_format_plugin_atx.close(&disk);
    remove(path);
}

TEST(weak_chunk_marks_from_its_offset_to_the_end)
{
    char path[512];
    const uint8_t fdc[ATX_SECS] = { 0, FDC_WEAK, 0, 0 };
    const int weak_at[ATX_SECS] = { -1, 100, -1, -1 };   /* sector index 1 */

    get_temp_path(path, sizeof(path), "weak");
    ASSERT(build_atx(path, fdc, weak_at));

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    ASSERT(uft_format_plugin_atx.open(&disk, path, true) == UFT_OK);

    uft_track_t tr;
    memset(&tr, 0, sizeof(tr));
    ASSERT(uft_format_plugin_atx.read_track(&disk, 0, 0, &tr) == UFT_OK);
    ASSERT(tr.sector_count == ATX_SECS);

    ASSERT(tr.sectors[1].weak == true);
    ASSERT(tr.sectors[1].weak_mask != NULL);
    ASSERT(tr.sectors[1].weak_mask[99]  == 0);    /* solid before the offset */
    ASSERT(tr.sectors[1].weak_mask[100] != 0);    /* weak from there on      */
    ASSERT(tr.sectors[1].weak_mask[ATX_SEC - 1] != 0);
    ASSERT(tr.sectors[0].weak == false);
    ASSERT(tr.sectors[0].weak_mask == NULL);

    free_track(&tr);
    uft_format_plugin_atx.close(&disk);
    remove(path);
}

int main(void)
{
    printf("=== ATX-Layout gegen das a8rawconv-Orakel (MF-467) ===\n");
    RUN(track_records_are_walked_and_sectors_delivered);
    RUN(fdc_status_becomes_sector_status);
    RUN(weak_chunk_marks_from_its_offset_to_the_end);
    printf("=== %d passed, %d failed ===\n", _pass, _fail);
    return _fail ? 1 : 0;
}
