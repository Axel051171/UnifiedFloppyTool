/**
 * @file test_apple_do_po_bounds.c
 * @brief DO/PO: no invented sectors, no unbounded cylinder (MF-463).
 *
 * Links the real DO and PO plugins (src/formats/do/uft_do.c,
 * src/formats/po/uft_po.c).
 *
 * Both are headerless 143,360-byte dumps, and both used to read like this:
 *
 * ```c
 * if (fread(buf, 1, DO_SS, p->file) != DO_SS) { memset(buf, 0xE5, DO_SS); }
 * uft_format_add_sector(track, s, buf, DO_SS, cyl, 0);
 * ```
 *
 * A sector that is not in the file came back as 256 bytes of 0xE5, added with
 * `crc_ok = true` like any other. Nothing in the result said the bytes had
 * never been read. `open()` does not check the file size either, so this was
 * not a corner case: any short or truncated file produced a complete-looking
 * 35-track disk made of fill.
 *
 * Neither read_track nor write_track bounded the cylinder, so a caller asking
 * for track 5000 got an offset far past the end — a fabricated track on read,
 * and on write a file silently extended to make room for it.
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_do;
extern const uft_format_plugin_t uft_format_plugin_po;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-38s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define TRACKS 35
#define SPT    16
#define SS     256u
#define FULL   (TRACKS * SPT * SS)   /* 143,360 */

static void get_temp_path(char *path, size_t size, const char *tag) {
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(path, size, "%s/uft_apple_%s_%d.dsk", dir, tag, rand() % 100000);
}

/** Image of @p bytes length; every 256-byte block starts with its own index. */
static int build_image(const char *path, size_t bytes) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    uint8_t sector[SS];
    size_t written = 0;
    for (uint32_t idx = 0; written < bytes; idx++) {
        memset(sector, 0x11, sizeof(sector));
        sector[0] = (uint8_t)(idx & 0xFF);
        sector[1] = (uint8_t)((idx >> 8) & 0xFF);
        size_t chunk = (bytes - written < SS) ? (bytes - written) : SS;
        fwrite(sector, 1, chunk, f);
        written += chunk;
    }
    fclose(f);
    return 1;
}

static void free_track(uft_track_t *tr) {
    for (size_t i = 0; i < tr->sector_count; i++) free(tr->sectors[i].data);
    free(tr->sectors);
    tr->sectors = NULL; tr->sector_count = 0;
}

static void check_plugin(const uft_format_plugin_t *pl, const char *tag) {
    char path[512];
    uft_disk_t disk;
    uft_track_t tr;

    /* --- complete image: all 16 sectors, data from the right offsets --- */
    get_temp_path(path, sizeof(path), tag);
    if (!build_image(path, FULL)) { printf("FAIL: build\n"); _fail++; return; }

    memset(&disk, 0, sizeof(disk));
    if (pl->open(&disk, path, true) != UFT_OK) { printf("FAIL: open\n"); _fail++; return; }

    memset(&tr, 0, sizeof(tr));
    if (pl->read_track(&disk, TRACKS - 1, 0, &tr) != UFT_OK) { printf("FAIL: read\n"); _fail++; return; }
    if (tr.sector_count != SPT) { printf("FAIL: %zu sectors\n", tr.sector_count); _fail++; return; }
    uint32_t idx = (uint32_t)(TRACKS - 1) * SPT;
    if (tr.sectors[0].data[0] != (uint8_t)(idx & 0xFF) ||
        tr.sectors[0].data[1] != (uint8_t)((idx >> 8) & 0xFF)) {
        printf("FAIL: wrong offset\n"); _fail++; return;
    }
    free_track(&tr);

    /* --- cylinder past the end is refused, not fabricated --- */
    memset(&tr, 0, sizeof(tr));
    if (pl->read_track(&disk, TRACKS, 0, &tr) == UFT_OK) {
        printf("FAIL: track %d accepted\n", TRACKS); _fail++; return;
    }
    if (pl->read_track(&disk, -1, 0, &tr) == UFT_OK) {
        printf("FAIL: track -1 accepted\n"); _fail++; return;
    }
    pl->close(&disk);
    remove(path);

    /* --- truncated image: only what exists is handed over --- */
    /* 10 whole tracks plus 5 sectors of track 10 — the rest of the disk is
     * simply not in the file, which is the point. */
    const size_t SHORT = (size_t)(10 * SPT + 5) * SS;
    get_temp_path(path, sizeof(path), tag);
    if (!build_image(path, SHORT)) { printf("FAIL: build short\n"); _fail++; return; }

    memset(&disk, 0, sizeof(disk));
    if (pl->open(&disk, path, true) != UFT_OK) { printf("FAIL: open short\n"); _fail++; return; }

    memset(&tr, 0, sizeof(tr));
    if (pl->read_track(&disk, 10, 0, &tr) != UFT_OK) { printf("FAIL: read short\n"); _fail++; return; }
    if (tr.sector_count != 5) {
        printf("FAIL: %zu sectors instead of 5\n", tr.sector_count); _fail++; return;
    }
    /* and what came back is real data, not fill */
    idx = 10u * SPT;
    if (tr.sectors[0].data[0] != (uint8_t)(idx & 0xFF)) {
        printf("FAIL: short-track data\n"); _fail++; return;
    }
    free_track(&tr);

    /* a track entirely past the end yields nothing at all */
    memset(&tr, 0, sizeof(tr));
    if (pl->read_track(&disk, TRACKS - 1, 0, &tr) == UFT_OK && tr.sector_count != 0) {
        printf("FAIL: %zu invented sectors\n", tr.sector_count); _fail++; return;
    }
    free_track(&tr);

    pl->close(&disk);
    remove(path);
}

TEST(do_reports_only_sectors_that_exist)  { check_plugin(&uft_format_plugin_do, "do"); }
TEST(po_reports_only_sectors_that_exist)  { check_plugin(&uft_format_plugin_po, "po"); }

int main(void) {
    printf("=== Apple DO/PO: keine erfundenen Sektoren (MF-463) ===\n");
    RUN(do_reports_only_sectors_that_exist);
    RUN(po_reports_only_sectors_that_exist);
    printf("=== %d passed, %d failed ===\n", _pass, _fail);
    return _fail ? 1 : 0;
}
