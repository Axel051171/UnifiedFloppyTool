/**
 * @file test_imd_track_record.c
 * @brief IMD track-record layout: what the head byte means (MF-430).
 *
 * The IMD track record is
 *
 *   [Mode][Cylinder][Head][SectorCount][SectorSizeCode]
 *   [sector numbering map]                      <- ALWAYS present
 *   [sector cylinder map]                       <- only if Head & 0x80
 *   [sector head map]                           <- only if Head & 0x40
 *   [sector data records]
 *
 * Two independent implementations agree on this and were used as the source:
 *   MAME  src/lib/formats/imd_dsk.cpp — "offs += 5 + sector_num" for the
 *         numbering map, then "if(header[2] & 0x80)" / "& 0x40" for the two
 *         optional maps.
 *   hharte/libimd src/libimd.h — IMD_HFLAG_CMAP_PRES 0x80,
 *         IMD_HFLAG_HMAP_PRES 0x40; libimd.c reads smap before testing a flag.
 *
 * uft_imd_adapter.c had it wrong three ways: 0x80 was read as "sector map
 * present", 0x40 as "cylinder map", and a 0x20 "head map" bit was invented.
 * IMD has no third map flag. For an ordinary track — head byte 0x00 — the
 * mandatory numbering map was therefore never consumed and every offset after
 * it was short by sector_count bytes.
 *
 * The second defect these vectors pin down: sector_count comes from the file
 * as a single byte, so it can say 255, while the destination arrays held 64
 * entries and no destination bound was checked before memcpy.
 *
 * These are unit vectors, not a corpus entry: they assert the LAYOUT, which is
 * what was misread. Tier stays where it is — see docs/VERIFICATION_PLAN.md.
 */

#include "uft/formats/uft_imd_adapter.h"
#include "uft/uft_error.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-46s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define MODE_MFM_250 3
#define SIZE_CODE_512 2

TEST(plain_track_still_consumes_the_mandatory_numbering_map) {
    /* head = 0x00: no optional maps, but the numbering map is there anyway.
     * This is the case that used to leave `pos` at 5. */
    const uint8_t nsec = 9;
    uint8_t buf[5 + 9 + 16];
    memset(buf, 0xEE, sizeof(buf));            /* poison past the record */
    buf[0] = MODE_MFM_250; buf[1] = 40; buf[2] = 0x00;
    buf[3] = nsec;         buf[4] = SIZE_CODE_512;
    /* interleaved numbering, so a synthesised 1..N would not match */
    static const uint8_t ids[9] = { 1, 4, 7, 2, 5, 8, 3, 6, 9 };
    memcpy(buf + 5, ids, nsec);

    uft_imd_track_header_t t;
    size_t consumed = 0;
    ASSERT(uft_imd_adapter_parse_track_header(buf, sizeof(buf), &t, &consumed) == UFT_OK);
    ASSERT(consumed == 5u + nsec);             /* was 5 before MF-430 */
    ASSERT(t.sector_count == nsec);
    ASSERT(memcmp(t.sector_map, ids, nsec) == 0);
    /* no maps in the file, so both are filled from the track header */
    for (int i = 0; i < nsec; i++) {
        ASSERT(t.cylinder_map[i] == 40);
        ASSERT(t.head_map[i] == 0);
    }
}

TEST(bit_0x80_is_the_cylinder_map_and_0x40_the_head_map) {
    const uint8_t nsec = 4;
    uint8_t buf[5 + 4 * 3];
    buf[0] = MODE_MFM_250; buf[1] = 10; buf[2] = 0x80 | 0x40 | 1;
    buf[3] = nsec;         buf[4] = SIZE_CODE_512;
    const uint8_t ids[4]  = { 11, 12, 13, 14 };
    const uint8_t cyls[4] = { 20, 21, 22, 23 };   /* differ from header cyl 10 */
    const uint8_t heads[4]= {  0,  1,  0,  1 };   /* differ from header head 1 */
    memcpy(buf + 5,            ids,   nsec);
    memcpy(buf + 5 + nsec,     cyls,  nsec);
    memcpy(buf + 5 + nsec * 2, heads, nsec);

    uft_imd_track_header_t t;
    size_t consumed = 0;
    ASSERT(uft_imd_adapter_parse_track_header(buf, sizeof(buf), &t, &consumed) == UFT_OK);
    ASSERT(consumed == 5u + (size_t)nsec * 3u);
    ASSERT(memcmp(t.sector_map,   ids,   nsec) == 0);
    /* The order is what the old code got backwards: with 0x40 read as
     * "cylinder map", these two arrays came out swapped. */
    ASSERT(memcmp(t.cylinder_map, cyls,  nsec) == 0);
    ASSERT(memcmp(t.head_map,     heads, nsec) == 0);
    ASSERT(t.head == 1);                       /* flags stripped, head kept */
}

TEST(only_one_optional_map_present) {
    /* 0x80 alone: cylinder map follows, head map does not. If the flags were
     * still swapped this would consume the same number of bytes but populate
     * the wrong array — hence the value checks. */
    const uint8_t nsec = 3;
    uint8_t buf[5 + 3 * 2];
    buf[0] = MODE_MFM_250; buf[1] = 7; buf[2] = 0x80;
    buf[3] = nsec;         buf[4] = SIZE_CODE_512;
    const uint8_t ids[3]  = { 1, 2, 3 };
    const uint8_t cyls[3] = { 77, 78, 79 };
    memcpy(buf + 5,        ids,  nsec);
    memcpy(buf + 5 + nsec, cyls, nsec);

    uft_imd_track_header_t t;
    size_t consumed = 0;
    ASSERT(uft_imd_adapter_parse_track_header(buf, sizeof(buf), &t, &consumed) == UFT_OK);
    ASSERT(consumed == 5u + (size_t)nsec * 2u);
    ASSERT(memcmp(t.cylinder_map, cyls, nsec) == 0);
    for (int i = 0; i < nsec; i++) ASSERT(t.head_map[i] == 0);
}

TEST(a_255_sector_track_does_not_overrun_the_maps) {
    /* sector_count is one byte, so 255 is legal to write down. The arrays used
     * to hold 64 entries and nothing checked the destination before memcpy —
     * 191 bytes past the end, three times. Under ASan this test is the proof;
     * without it, the length check below still catches the regression. */
    const size_t nsec = 255;
    uint8_t *buf = (uint8_t *)malloc(5 + nsec * 3);
    ASSERT(buf != NULL);
    buf[0] = MODE_MFM_250; buf[1] = 0; buf[2] = 0x80 | 0x40;
    buf[3] = (uint8_t)nsec; buf[4] = SIZE_CODE_512;
    for (size_t i = 0; i < nsec * 3; i++) buf[5 + i] = (uint8_t)(i & 0xFF);

    uft_imd_track_header_t t;
    size_t consumed = 0;
    ASSERT(uft_imd_adapter_parse_track_header(buf, 5 + nsec * 3, &t, &consumed) == UFT_OK);
    ASSERT(consumed == 5u + nsec * 3u);
    ASSERT(t.sector_count == 255);
    ASSERT(t.sector_map[254] == (uint8_t)254);
    free(buf);
}

TEST(a_truncated_map_is_reported_not_read) {
    /* The record claims 200 sectors but the buffer holds only part of the
     * numbering map. Reading past the source is the other half of the same
     * bug class. */
    uint8_t buf[5 + 50];
    buf[0] = MODE_MFM_250; buf[1] = 0; buf[2] = 0x00;
    buf[3] = 200;          buf[4] = SIZE_CODE_512;
    memset(buf + 5, 0x5A, 50);

    uft_imd_track_header_t t;
    size_t consumed = 0;
    uft_error_t e = uft_imd_adapter_parse_track_header(buf, sizeof(buf), &t, &consumed);
    ASSERT(e != UFT_OK);
}

int main(void)
{
    printf("=== IMD track record layout (MF-430) ===\n");
    RUN(plain_track_still_consumes_the_mandatory_numbering_map);
    RUN(bit_0x80_is_the_cylinder_map_and_0x40_the_head_map);
    RUN(only_one_optional_map_present);
    RUN(a_255_sector_track_does_not_overrun_the_maps);
    RUN(a_truncated_map_is_reported_not_read);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
