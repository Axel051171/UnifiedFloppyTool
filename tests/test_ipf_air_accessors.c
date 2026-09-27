/*
 * test_ipf_air_accessors.c — verify the opaque-handle accessor API exposed
 * by include/uft/formats/ipf/uft_ipf_air.h works on a synthetic IPF buffer.
 *
 * Wired into CTest via tests/CMakeLists.txt (target_sources pulls in
 * src/formats/ipf/uft_ipf_air.c). Builds standalone too:
 *   gcc -I include/ -o test tests/test_ipf_air_accessors.c \
 *       src/formats/ipf/uft_ipf_air.c
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/formats/ipf/uft_ipf_air.h"
#include "uft/formats/uft_air_crc32.h"

static void put_be32(uint8_t *p, uint32_t v) {
    p[0] = (v >> 24) & 0xFF;
    p[1] = (v >> 16) & 0xFF;
    p[2] = (v >>  8) & 0xFF;
    p[3] = (v      ) & 0xFF;
}

int main(void) {
    /* Build minimal valid IPF: CAPS + INFO records. INFO sets max_track=3
     * (so 4 cylinders) and max_side=1 (so 2 sides). Platform[0]=Atari_ST.
     * Layout: 12 (CAPS) + 96 (INFO total) — `length` field is total
     * record size including the 12-byte header. */
    uint8_t buf[12 + 96] = {0};

    /* CAPS record (12 bytes total = header only). */
    memcpy(buf, "CAPS", 4);
    put_be32(buf + 4, 12);
    put_be32(buf + 8, air_crc32_header(buf, 0, 12));

    /* INFO record: 12-byte header + 84-byte payload = 96 total. */
    uint8_t *p = buf + 12;
    memcpy(p, "INFO", 4);
    put_be32(p + 4, 96);
    /* Field offsets within payload (BE32):
     *   0  media_type   4  encoder_type    8  encoder_rev   12 file_key
     *  16  file_rev    20  origin         24  min_track    28  max_track
     *  32  min_side    36  max_side       40  creation_date 44 creation_time
     *  48  platforms[0] (then [1..3])     64 disk_number   68 creator_id
     */
    put_be32(p + 12 +  4, 2);   /* encoder_type = SPS */
    put_be32(p + 12 + 28, 3);   /* max_track = 3 → 4 cyls */
    put_be32(p + 12 + 36, 1);   /* max_side = 1  → 2 sides */
    put_be32(p + 12 + 48, 2);   /* platforms[0] = Atari_ST */
    put_be32(p + 8, air_crc32_header(p, 0, 96));

    /* === Test 1: alloc + parse ===================================== */
    printf("Test 1: alloc + parse... ");
    ipf_air_disk_t *disk = ipf_air_alloc();
    assert(disk != NULL);
    ipf_air_status_t st = ipf_air_parse(buf, sizeof(buf), disk);
    assert(st == IPF_AIR_OK);
    printf("OK\n");

    /* === Test 2: ipf_air_get_geometry ============================== */
    printf("Test 2: ipf_air_get_geometry... ");
    int cyls = -1, sides = -1;
    uint32_t platform = 0xFFFFFFFFu;
    int rc = ipf_air_get_geometry(disk, &cyls, &sides, &platform);
    assert(rc == 0);
    assert(cyls == 4);
    assert(sides == 2);
    assert(platform == 2);  /* Atari_ST */
    printf("OK (cyls=%d sides=%d platform=%u)\n", cyls, sides, platform);

    /* === Test 3: track_present on absent (cyl, head) =============== */
    printf("Test 3: track_present false for unmapped track... ");
    assert(ipf_air_track_present(disk, 0, 0) == false);
    assert(ipf_air_track_present(disk, 99, 0) == false);
    assert(ipf_air_track_present(disk, 0, 5) == false);
    printf("OK\n");

    /* === Test 4: get_track_meta returns -1 for absent ============== */
    printf("Test 4: get_track_meta -1 for absent... ");
    uint32_t tb = 0xFFu, dens = 0xFFu, flags = 0xFFu;
    bool fuzzy = true;
    rc = ipf_air_get_track_meta(disk, 0, 0, &tb, &dens, &flags, &fuzzy);
    assert(rc == -1);
    printf("OK\n");

    /* === Test 5: get_track_raw -1 for absent ======================= */
    printf("Test 5: get_track_raw -1 for absent... ");
    uint8_t *bufp = (uint8_t *)0xDEADBEEF;
    uint32_t bits = 0xCAFEBABEu;
    rc = ipf_air_get_track_raw(disk, 0, 0, &bufp, &bits);
    assert(rc == -1);
    assert(bufp == NULL);
    assert(bits == 0);
    printf("OK\n");

    /* === Test 6: get_track_raw NULL-arg guards ===================== */
    printf("Test 6: get_track_raw NULL arg guards... ");
    rc = ipf_air_get_track_raw(disk, 0, 0, NULL, &bits);
    assert(rc == -1);
    rc = ipf_air_get_track_raw(disk, 0, 0, &bufp, NULL);
    assert(rc == -1);
    printf("OK\n");

    /* === Test 7: cleanup =========================================== */
    printf("Test 7: ipf_air_free + free... ");
    ipf_air_free(disk);
    free(disk);
    printf("OK\n");

    /* === Test 7b: ein Blockverlust muss sich melden (MF-830/MF-1372) =
     *
     * MF-830 hielt hier die feste Klemme IPF_MAX_BLOCKS (16) fest: mehr
     * angekuendigte Bloecke fielen bis dahin STILL weg. Seit MF-1372
     * (P3-361 Weg b) gibt es keine Klemme mehr — gehalten wird, was der
     * DATA-Satz an Blockbeschreibungen TRAEGT. Ein Verlust steht also
     * genau dann fest, wenn die Nutzlast weniger Beschreibungen (je 32
     * Byte) enthaelt, als der Spurkopf ansagt.
     *
     * Drei Spuren, damit keine Zusicherung leer ist:
     *   Spur 1: 20 angesagt, 20 getragen  -> 20 gehalten, kein Verlust
     *           (vor MF-1372: 16 gehalten, Verlust — die alte Klemme)
     *   Spur 2: 20 angesagt,  8 getragen  -> 8 gehalten, Verlust
     *   Spur 3:  8 angesagt, kein DATA    -> 0 gehalten, KEIN Verlust
     *           (fehlender DATA-Satz ist eine andere Ursache, siehe
     *           Kopfkommentar von ipf_air_get_track_loss) */
    printf("Test 7b: Blockverlust meldet sich (MF-830/MF-1372)... ");
    {
        static uint8_t b2[12 + 96 + 3 * 80 + 2 * (28 + 20 * 32)];
        memset(b2, 0, sizeof b2);
        size_t w = 0;
        memcpy(b2, "CAPS", 4);
        put_be32(b2 + 4, 12);
        put_be32(b2 + 8, air_crc32_header(b2, 0, 12));
        w = 12;

        uint8_t *q = b2 + w;
        memcpy(q, "INFO", 4);
        put_be32(q + 4, 96);
        put_be32(q + 12 +  4, 2);   /* encoder_type = SPS */
        put_be32(q + 12 + 28, 3);   /* max_track = 3 */
        put_be32(q + 12 + 36, 1);   /* max_side  = 1 */
        put_be32(q + 12 + 48, 2);   /* platforms[0] = Atari_ST */
        put_be32(q + 8, air_crc32_header(q, 0, 96));
        w += 96;

        /* IMGE-Nutzlast (BE32 ab dem 12-Byte-Kopf):
         *  0 track   4 side  8 density 12 signal 16 track_bytes
         * 20 start_byte 24 start_bit 28 data_bits 32 gap_bits
         * 36 track_bits 40 BLOCK_COUNT 44 encoder 48 flags 52 data_key
         * 56..64 reserved[3]  = 68 Byte Nutzlast, 80 Byte Satz */
        struct { uint32_t track, blocks, key; } tr[3] = {
            {1, 20, 1}, {2, 20, 2}, {3, 8, 3} };
        for (int k = 0; k < 3; k++) {
            uint8_t *m = b2 + w;
            memcpy(m, "IMGE", 4);
            put_be32(m + 4, 80);
            put_be32(m + 12 +  0, tr[k].track);
            put_be32(m + 12 +  4, 0);              /* side */
            put_be32(m + 12 + 40, tr[k].blocks);   /* block_count */
            put_be32(m + 12 + 52, tr[k].key);      /* data_key */
            put_be32(m + 8, air_crc32_header(m, 0, 80));
            w += 80;
        }
        /* DATA fuer Spur 1 (20 Beschreibungen) und Spur 2 (8). */
        uint32_t getragen[2] = { 20, 8 };
        for (int k = 0; k < 2; k++) {
            uint8_t *dr = b2 + w;
            const uint32_t len = getragen[k] * 32u;
            memcpy(dr, "DATA", 4);
            put_be32(dr + 4, 28);
            put_be32(dr + 12, len);                 /* Nutzlastgroesse */
            put_be32(dr + 16, len * 8u);
            put_be32(dr + 24, tr[k].key);
            /* Beschreibungen: alles 0 — kein Block traegt Daten. */
            put_be32(dr + 20, air_crc32_buffer(dr + 28, 0, len));
            put_be32(dr + 8, air_crc32_header(dr, 0, 28));
            w += 28 + len;
        }

        ipf_air_disk_t *d2 = ipf_air_alloc();
        assert(d2 != NULL);
        assert(ipf_air_parse(b2, w, d2) == IPF_AIR_OK);

        uint32_t decl = 0, stored = 0xFFFFu;
        bool cut = true;

        /* Spur 1: 20 angesagt und 20 getragen — alle gehalten. */
        assert(ipf_air_get_track_loss(d2, 1, 0, &decl, &stored, &cut) == 0);
        assert(decl == 20);
        assert(stored == 20);
        assert(cut == false);

        /* Spur 2: 20 angesagt, 8 getragen — der Verlust MUSS stehen. */
        cut = false;
        assert(ipf_air_get_track_loss(d2, 2, 0, &decl, &stored, &cut) == 0);
        assert(decl == 20);
        assert(stored == 8);
        assert(cut == true);

        /* Spur 3: kein DATA-Satz — nichts gelesen, aber keine Kappung. */
        cut = true;
        assert(ipf_air_get_track_loss(d2, 3, 0, &decl, &stored, &cut) == 0);
        assert(decl == 8);
        assert(stored == 0);
        assert(cut == false);

        /* Abwesende Spur und NULL-Griff. */
        assert(ipf_air_get_track_loss(d2, 9, 0, &decl, &stored, &cut) == -1);
        assert(ipf_air_get_track_loss(NULL, 1, 0, &decl, &stored, &cut) == -1);
        assert(ipf_air_get_track_loss(d2, 1, 0, NULL, NULL, NULL) == 0);

        ipf_air_free(d2);
        free(d2);
    }
    printf("OK\n");

    /* === Test 8: NULL-disk handling ================================ */
    printf("Test 8: NULL disk handling... ");
    rc = ipf_air_get_geometry(NULL, &cyls, &sides, &platform);
    assert(rc == -1);
    assert(ipf_air_track_present(NULL, 0, 0) == false);
    rc = ipf_air_get_track_meta(NULL, 0, 0, &tb, &dens, &flags, &fuzzy);
    assert(rc == -1);
    bufp = (uint8_t *)0x1; bits = 1;
    rc = ipf_air_get_track_raw(NULL, 0, 0, &bufp, &bits);
    assert(rc == -1);
    assert(bufp == NULL && bits == 0);
    printf("OK\n");

    printf("\n=== All IPF AIR accessor smoke-tests passed ===\n");
    return 0;
}
