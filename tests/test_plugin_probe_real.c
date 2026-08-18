/**
 * @file test_plugin_probe_real.c
 * @brief Probes the REAL plugins — wave 1 (MF-385).
 *
 * Replaces the "replica" pattern. Those tests copied the probe logic into the
 * test file and asserted against the copy, so production could drift away
 * without a single test turning red. The audit found 32 of them; this file
 * starts converting them into tests that call the shipped code.
 *
 * The drift was not hypothetical: test_d64_plugin.c knew four accepted D64
 * sizes, while d64_plugin_probe() accepts eight (the 41/42-track VICE/Schepers
 * variants were added to production and never reached the replica).
 *
 * Every expectation below was read out of the plugin source, not assumed:
 *   d64  8 sizes incl. 200960/201745/205312/206114 (uft_d64_plugin.c:38)
 *   d71  349696 / 351062                            (uft_d71.c:13)
 *   d81  819200 / 822400                            (uft_d81.c:12)
 *   g64  signature "GCR-1541", >= 12 bytes          (uft_g64.c:39,45)
 *   hfe  "HXCPICFE" or "HXCHFEV3"                   (uft_hfe.c:39,40)
 */

#include "uft/uft_format_plugin.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_d64;
extern const uft_format_plugin_t uft_format_plugin_d71;
extern const uft_format_plugin_t uft_format_plugin_d81;
extern const uft_format_plugin_t uft_format_plugin_g64;
extern const uft_format_plugin_t uft_format_plugin_hfe;
extern const uft_format_plugin_t uft_format_plugin_atr;
extern const uft_format_plugin_t uft_format_plugin_d88;
extern const uft_format_plugin_t uft_format_plugin_dc42;
extern const uft_format_plugin_t uft_format_plugin_imd;
extern const uft_format_plugin_t uft_format_plugin_atx;
extern const uft_format_plugin_t uft_format_plugin_cqm;
extern const uft_format_plugin_t uft_format_plugin_jv1;
extern const uft_format_plugin_t uft_format_plugin_xfd;
extern const uft_format_plugin_t uft_format_plugin_edsk;
extern const uft_format_plugin_t uft_format_plugin_msa;
extern const uft_format_plugin_t uft_format_plugin_nib;
extern const uft_format_plugin_t uft_format_plugin_woz;
extern const uft_format_plugin_t uft_format_plugin_do;
extern const uft_format_plugin_t uft_format_plugin_po;
extern const uft_format_plugin_t uft_format_plugin_img;
extern const uft_format_plugin_t uft_format_plugin_td0;
extern const uft_format_plugin_t uft_format_plugin_fdi;
extern const uft_format_plugin_t uft_format_plugin_ipf;
extern const uft_format_plugin_t uft_format_plugin_scp;
extern const uft_format_plugin_t uft_format_plugin_stx;
extern const uft_format_plugin_t uft_format_plugin_2img;
extern const uft_format_plugin_t uft_format_plugin_dsk_cpc;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-40s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

/** Call a real probe with a header buffer and a claimed file size. */
static bool probe_sized(const uft_format_plugin_t *p, const uint8_t *hdr,
                        size_t hdr_len, size_t file_size, int *conf)
{
    int c = -1;
    bool ok = p->probe(hdr, hdr_len, file_size, &c);
    if (conf) *conf = c;
    return ok;
}

TEST(d64_accepts_all_eight_production_sizes) {
    /* The replica knew four; production accepts eight. */
    static const size_t accepted[] = {
        174848, 175531, 196608, 197376,     /* 35 / 40 tracks, +/- error map */
        200960, 201745, 205312, 206114      /* 41 / 42 tracks (VICE/Schepers) */
    };
    uint8_t hdr[64];
    memset(hdr, 0, sizeof(hdr));

    for (size_t i = 0; i < sizeof(accepted) / sizeof(accepted[0]); i++) {
        int conf = -1;
        ASSERT(probe_sized(&uft_format_plugin_d64, hdr, sizeof(hdr),
                           accepted[i], &conf));
        ASSERT(conf > 0);
    }
}

TEST(d64_rejects_neighbouring_sizes) {
    uint8_t hdr[64];
    memset(hdr, 0, sizeof(hdr));
    int conf = -1;
    ASSERT(!probe_sized(&uft_format_plugin_d64, hdr, sizeof(hdr), 174847, &conf));
    ASSERT(!probe_sized(&uft_format_plugin_d64, hdr, sizeof(hdr), 174849, &conf));
    ASSERT(!probe_sized(&uft_format_plugin_d64, hdr, sizeof(hdr), 0, &conf));
}

TEST(d64_confidence_rises_with_a_real_bam_link) {
    /* Production raises confidence from 75 to 92 when the BAM at 0x16500
     * carries the 18/1 directory link. Exercising that needs a full image. */
    size_t sz = 174848;
    uint8_t *img = calloc(1, sz);
    ASSERT(img != NULL);

    int plain = -1;
    ASSERT(probe_sized(&uft_format_plugin_d64, img, sz, sz, &plain));

    img[0x16500] = 18;
    img[0x16501] = 1;
    int with_bam = -1;
    ASSERT(probe_sized(&uft_format_plugin_d64, img, sz, sz, &with_bam));
    ASSERT(with_bam > plain);

    free(img);
}

TEST(d71_accepts_both_variants_and_rejects_others) {
    uint8_t hdr[64];
    memset(hdr, 0, sizeof(hdr));
    int conf = -1;
    ASSERT(probe_sized(&uft_format_plugin_d71, hdr, sizeof(hdr), 349696, &conf));
    ASSERT(probe_sized(&uft_format_plugin_d71, hdr, sizeof(hdr), 351062, &conf));
    ASSERT(!probe_sized(&uft_format_plugin_d71, hdr, sizeof(hdr), 349695, &conf));
    /* a D64 must not be claimed by the D71 plugin */
    ASSERT(!probe_sized(&uft_format_plugin_d71, hdr, sizeof(hdr), 174848, &conf));
}

TEST(d81_accepts_both_variants_and_rejects_others) {
    uint8_t hdr[64];
    memset(hdr, 0, sizeof(hdr));
    int conf = -1;
    ASSERT(probe_sized(&uft_format_plugin_d81, hdr, sizeof(hdr), 819200, &conf));
    ASSERT(probe_sized(&uft_format_plugin_d81, hdr, sizeof(hdr), 822400, &conf));
    ASSERT(!probe_sized(&uft_format_plugin_d81, hdr, sizeof(hdr), 819199, &conf));
    ASSERT(!probe_sized(&uft_format_plugin_d81, hdr, sizeof(hdr), 349696, &conf));
}

TEST(g64_requires_its_signature) {
    uint8_t hdr[32];
    memset(hdr, 0, sizeof(hdr));
    int conf = -1;

    ASSERT(!probe_sized(&uft_format_plugin_g64, hdr, sizeof(hdr), sizeof(hdr), &conf));

    memcpy(hdr, "GCR-1541", 8);
    ASSERT(probe_sized(&uft_format_plugin_g64, hdr, sizeof(hdr), sizeof(hdr), &conf));
    ASSERT(conf > 0);

    /* one byte off is not a G64 */
    hdr[7] = '0';
    ASSERT(!probe_sized(&uft_format_plugin_g64, hdr, sizeof(hdr), sizeof(hdr), &conf));

    /* too short to hold a header at all */
    memcpy(hdr, "GCR-1541", 8);
    ASSERT(!probe_sized(&uft_format_plugin_g64, hdr, 8, 8, &conf));
}

TEST(hfe_accepts_v1_and_v3_signatures) {
    /* hfe_header_t is padded to 512 bytes and the probe refuses anything
     * shorter — found by this test on its first run, when a 64-byte buffer
     * was rejected for BOTH signatures. */
    uint8_t hdr[512];
    int conf = -1;

    memset(hdr, 0, sizeof(hdr));
    memcpy(hdr, "HXCPICFE", 8);
    ASSERT(probe_sized(&uft_format_plugin_hfe, hdr, sizeof(hdr), sizeof(hdr), &conf));
    ASSERT(conf >= 95);

    memset(hdr, 0, sizeof(hdr));
    memcpy(hdr, "HXCHFEV3", 8);
    ASSERT(probe_sized(&uft_format_plugin_hfe, hdr, sizeof(hdr), sizeof(hdr), &conf));
    ASSERT(conf >= 95);

    memset(hdr, 0, sizeof(hdr));
    memcpy(hdr, "HXCPICFF", 8);
    ASSERT(!probe_sized(&uft_format_plugin_hfe, hdr, sizeof(hdr), sizeof(hdr), &conf));

    /* a truncated header is not enough, even with a valid signature */
    memcpy(hdr, "HXCPICFE", 8);
    ASSERT(!probe_sized(&uft_format_plugin_hfe, hdr, 64, 64, &conf));
}

TEST(plugins_do_not_claim_each_others_headers) {
    /* Cross-check: a G64 header must not be accepted by HFE and vice versa.
     * A replica test cannot do this at all — it only ever sees its own copy. */
    uint8_t g64_hdr[512], hfe_hdr[512];
    memset(g64_hdr, 0, sizeof(g64_hdr));
    memset(hfe_hdr, 0, sizeof(hfe_hdr));
    memcpy(g64_hdr, "GCR-1541", 8);
    memcpy(hfe_hdr, "HXCPICFE", 8);
    int conf = -1;

    ASSERT(!probe_sized(&uft_format_plugin_hfe, g64_hdr, sizeof(g64_hdr),
                        sizeof(g64_hdr), &conf));
    ASSERT(!probe_sized(&uft_format_plugin_g64, hfe_hdr, sizeof(hfe_hdr),
                        sizeof(hfe_hdr), &conf));
}

/*---------------------------------------------------------------------------
 * Wave 2 (MF-386) — atr, d88, dc42, imd
 * Constants again read from the plugin sources:
 *   atr   LE16 magic 0x0296, header 16 bytes        (uft_atr.c:8,9)
 *   d88   header 0x2B0, media byte at 0x1B in {0x00,0x10,0x20},
 *         declared size at 0x1C must fit the file  (uft_d88.c:8,13)
 *   dc42  BE16 magic 0x0100 at offset 82, header 84,
 *         name length <= 63, data size != 0        (uft_dc42.c:31,32,34)
 *   imd   ASCII signature "IMD "                    (uft_imd_plugin.c:14)
 *--------------------------------------------------------------------------*/

TEST(atr_requires_its_little_endian_magic) {
    uint8_t hdr[32];
    int conf = -1;

    memset(hdr, 0, sizeof(hdr));
    hdr[0] = 0x96; hdr[1] = 0x02;            /* LE 0x0296 */
    ASSERT(probe_sized(&uft_format_plugin_atr, hdr, sizeof(hdr), sizeof(hdr), &conf));
    ASSERT(conf >= 95);

    /* byte-swapped magic must NOT be accepted — a real byte-order regression
     * of exactly this kind was found in the ATX probe (MASTER_PLAN M1). */
    memset(hdr, 0, sizeof(hdr));
    hdr[0] = 0x02; hdr[1] = 0x96;
    ASSERT(!probe_sized(&uft_format_plugin_atr, hdr, sizeof(hdr), sizeof(hdr), &conf));

    /* header shorter than 16 bytes is rejected regardless of magic */
    memset(hdr, 0, sizeof(hdr));
    hdr[0] = 0x96; hdr[1] = 0x02;
    ASSERT(!probe_sized(&uft_format_plugin_atr, hdr, 8, 8, &conf));
}

TEST(d88_checks_media_byte_and_declared_size) {
    const size_t N = 0x400;
    uint8_t *hdr = calloc(1, N);
    ASSERT(hdr != NULL);
    int conf = -1;

    /* declared size 0x300 fits into a claimed file size of 0x400 */
    hdr[0x1B] = 0x00;                        /* media: 2D */
    hdr[0x1C] = 0x00; hdr[0x1D] = 0x03;      /* LE32 0x00000300 */
    ASSERT(probe_sized(&uft_format_plugin_d88, hdr, N, N, &conf));

    /* an unknown media byte is not a D88 */
    hdr[0x1B] = 0x77;
    ASSERT(!probe_sized(&uft_format_plugin_d88, hdr, N, N, &conf));

    /* a declared size larger than the file is not a D88 either */
    hdr[0x1B] = 0x20;
    hdr[0x1C] = 0x00; hdr[0x1D] = 0x00; hdr[0x1E] = 0x10;  /* 0x00100000 */
    ASSERT(!probe_sized(&uft_format_plugin_d88, hdr, N, N, &conf));

    free(hdr);
}

TEST(dc42_requires_big_endian_magic_at_offset_82) {
    uint8_t hdr[128];
    int conf = -1;

    memset(hdr, 0, sizeof(hdr));
    hdr[82] = 0x01; hdr[83] = 0x00;          /* BE 0x0100 */
    hdr[0]  = 10;                            /* sane name length */
    hdr[0x43] = 0x10;                        /* non-zero BE32 data size */
    ASSERT(probe_sized(&uft_format_plugin_dc42, hdr, sizeof(hdr), sizeof(hdr), &conf));

    /* swapped magic must be rejected */
    hdr[82] = 0x00; hdr[83] = 0x01;
    ASSERT(!probe_sized(&uft_format_plugin_dc42, hdr, sizeof(hdr), sizeof(hdr), &conf));

    /* implausible name length is rejected even with correct magic */
    hdr[82] = 0x01; hdr[83] = 0x00;
    hdr[0] = 200;
    ASSERT(!probe_sized(&uft_format_plugin_dc42, hdr, sizeof(hdr), sizeof(hdr), &conf));

    /* zero data size is rejected */
    hdr[0] = 10;
    hdr[0x43] = 0x00;
    ASSERT(!probe_sized(&uft_format_plugin_dc42, hdr, sizeof(hdr), sizeof(hdr), &conf));
}

TEST(imd_requires_its_ascii_signature) {
    uint8_t hdr[32];
    int conf = -1;

    memset(hdr, 0, sizeof(hdr));
    memcpy(hdr, "IMD ", 4);
    ASSERT(probe_sized(&uft_format_plugin_imd, hdr, sizeof(hdr), sizeof(hdr), &conf));
    ASSERT(conf >= 95);

    /* the trailing space is part of the signature */
    memcpy(hdr, "IMD1", 4);
    ASSERT(!probe_sized(&uft_format_plugin_imd, hdr, sizeof(hdr), sizeof(hdr), &conf));

    memcpy(hdr, "IMD ", 4);
    ASSERT(!probe_sized(&uft_format_plugin_imd, hdr, 3, 3, &conf));
}

/*---------------------------------------------------------------------------
 * Wave 3 (MF-387) — atx, cqm, jv1, xfd
 * These four had NO test touching production code at all before this wave.
 *   atx  LE32 signature 0x58385441 "AT8X", header 48   (uft_atx.c:32,33)
 *   cqm  'C','Q',0x14                                   (uft_cqm.c)
 *   jv1  size-only: multiple of 10*256, <=40 tracks 1 head,
 *        <=80 and even 2 heads                          (uft_jv1.c:22,23)
 *   xfd  four canonical sizes, plus a permissive fallback
 *                                                       (uft_xfd.c)
 *--------------------------------------------------------------------------*/

TEST(atx_signature_is_byte_order_correct) {
    uint8_t hdr[64];
    int conf = -1;

    /* 0x58385441 read as LE32 means the bytes are 'A','T','8','X'. */
    memset(hdr, 0, sizeof(hdr));
    memcpy(hdr, "AT8X", 4);
    ASSERT(probe_sized(&uft_format_plugin_atx, hdr, sizeof(hdr), sizeof(hdr), &conf));
    ASSERT(conf >= 95);

    /* The reversed spelling must NOT pass. A byte-order slip in exactly this
     * signature once made the ATX plugin return no sector data at all. */
    memset(hdr, 0, sizeof(hdr));
    memcpy(hdr, "X8TA", 4);
    ASSERT(!probe_sized(&uft_format_plugin_atx, hdr, sizeof(hdr), sizeof(hdr), &conf));

    /* below the 48-byte file header the probe refuses regardless */
    memcpy(hdr, "AT8X", 4);
    ASSERT(!probe_sized(&uft_format_plugin_atx, hdr, 16, 16, &conf));
}

TEST(cqm_requires_its_three_byte_marker) {
    uint8_t hdr[32];
    int conf = -1;

    memset(hdr, 0, sizeof(hdr));
    hdr[0] = 'C'; hdr[1] = 'Q'; hdr[2] = 0x14;
    ASSERT(probe_sized(&uft_format_plugin_cqm, hdr, sizeof(hdr), sizeof(hdr), &conf));

    /* the third byte is part of the marker, "CQ" alone is not enough */
    hdr[2] = 0x00;
    ASSERT(!probe_sized(&uft_format_plugin_cqm, hdr, sizeof(hdr), sizeof(hdr), &conf));

    hdr[2] = 0x14;
    ASSERT(!probe_sized(&uft_format_plugin_cqm, hdr, 2, 2, &conf));
}

TEST(jv1_accepts_only_whole_track_multiples) {
    uint8_t hdr[32];
    memset(hdr, 0, sizeof(hdr));
    int conf = -1;
    const size_t TRACK = 10u * 256u;         /* 10 sectors x 256 bytes */

    ASSERT(probe_sized(&uft_format_plugin_jv1, hdr, sizeof(hdr), TRACK, &conf));
    ASSERT(probe_sized(&uft_format_plugin_jv1, hdr, sizeof(hdr), 35 * TRACK, &conf));
    ASSERT(probe_sized(&uft_format_plugin_jv1, hdr, sizeof(hdr), 80 * TRACK, &conf));

    /* not a whole number of tracks */
    ASSERT(!probe_sized(&uft_format_plugin_jv1, hdr, sizeof(hdr), TRACK + 1, &conf));
    ASSERT(!probe_sized(&uft_format_plugin_jv1, hdr, sizeof(hdr), 0, &conf));
    /* 41..79 tracks are only valid when even (two heads) */
    ASSERT(!probe_sized(&uft_format_plugin_jv1, hdr, sizeof(hdr), 41 * TRACK, &conf));
    /* beyond 80 tracks there is no JV1 geometry */
    ASSERT(!probe_sized(&uft_format_plugin_jv1, hdr, sizeof(hdr), 82 * TRACK, &conf));

    /* size-only detection, so the plugin says so with a low confidence */
    ASSERT(probe_sized(&uft_format_plugin_jv1, hdr, sizeof(hdr), 35 * TRACK, &conf));
    ASSERT(conf < 50);
}

TEST(xfd_is_deliberately_permissive_outside_its_canonical_sizes) {
    /* Pins CURRENT behaviour. The canonical Atari sizes score high, but the
     * fallback accepts ANY file that is a multiple of 128 or 256 and not
     * larger than 266240 — with confidence 25. That is very broad for a
     * format without a magic number; it only stays harmless because format
     * selection compares confidences. Worth knowing, hence pinned. */
    uint8_t hdr[64];
    memset(hdr, 0, sizeof(hdr));
    int conf = -1;

    /* canonical single density, with a plausible Atari boot sector */
    hdr[0] = 0x00; hdr[3] = 0x07;
    ASSERT(probe_sized(&uft_format_plugin_xfd, hdr, sizeof(hdr), 92160, &conf));
    ASSERT(conf >= 80);

    /* an arbitrary 512-byte file is still claimed, but weakly */
    memset(hdr, 0xEE, sizeof(hdr));
    ASSERT(probe_sized(&uft_format_plugin_xfd, hdr, sizeof(hdr), 512, &conf));
    ASSERT(conf <= 30);

    /* odd sizes and oversized files are rejected outright */
    ASSERT(!probe_sized(&uft_format_plugin_xfd, hdr, sizeof(hdr), 513, &conf));
    ASSERT(!probe_sized(&uft_format_plugin_xfd, hdr, sizeof(hdr), 1024 * 1024, &conf));
    ASSERT(!probe_sized(&uft_format_plugin_xfd, hdr, sizeof(hdr), 0, &conf));
}

/*---------------------------------------------------------------------------
 * Wave 4 (MF-388) — edsk, msa, nib, woz
 *   edsk  "EXTENDED CPC DSK" (95) or "MV - CPC" (90), >= 34 bytes
 *   msa   BE16 magic 0x0E0F, >= 10 bytes            (uft_msa_plugin.c:14)
 *   nib   exact size 232960, confidence rises with Apple GCR content
 *                                                    (uft_nib.c:10)
 *   woz   LE32 'WOZ1' 0x315A4F57 or 'WOZ2' 0x325A4F57 (uft_woz.h:47,48)
 *--------------------------------------------------------------------------*/

TEST(edsk_accepts_both_amstrad_headers) {
    uint8_t hdr[64];
    int conf = -1;

    memset(hdr, 0, sizeof(hdr));
    memcpy(hdr, "EXTENDED CPC DSK", 16);
    ASSERT(probe_sized(&uft_format_plugin_edsk, hdr, sizeof(hdr), sizeof(hdr), &conf));
    ASSERT(conf >= 95);

    /* the plain (non-extended) CPC header is accepted with lower confidence */
    memset(hdr, 0, sizeof(hdr));
    memcpy(hdr, "MV - CPC", 8);
    int plain = -1;
    ASSERT(probe_sized(&uft_format_plugin_edsk, hdr, sizeof(hdr), sizeof(hdr), &plain));
    ASSERT(plain < conf);

    memset(hdr, 0, sizeof(hdr));
    memcpy(hdr, "EXTENDED CPC DSC", 16);      /* one letter off */
    ASSERT(!probe_sized(&uft_format_plugin_edsk, hdr, sizeof(hdr), sizeof(hdr), &conf));
}

TEST(msa_magic_is_big_endian) {
    uint8_t hdr[32];
    int conf = -1;

    memset(hdr, 0, sizeof(hdr));
    hdr[0] = 0x0E; hdr[1] = 0x0F;             /* BE 0x0E0F */
    ASSERT(probe_sized(&uft_format_plugin_msa, hdr, sizeof(hdr), sizeof(hdr), &conf));
    ASSERT(conf >= 95);

    hdr[0] = 0x0F; hdr[1] = 0x0E;             /* swapped */
    ASSERT(!probe_sized(&uft_format_plugin_msa, hdr, sizeof(hdr), sizeof(hdr), &conf));

    hdr[0] = 0x0E; hdr[1] = 0x0F;
    ASSERT(!probe_sized(&uft_format_plugin_msa, hdr, 4, 4, &conf));
}

TEST(nib_requires_exact_size_and_grades_gcr_content) {
    const size_t NIB = 232960;
    uint8_t *img = calloc(1, NIB);
    ASSERT(img != NULL);
    int empty_conf = -1, gcr_conf = -1;

    /* size alone is enough to claim the format */
    ASSERT(probe_sized(&uft_format_plugin_nib, img, NIB, NIB, &empty_conf));
    ASSERT(empty_conf > 0);

    /* a track that actually looks like Apple II GCR scores higher: sync runs
     * of 0xFF plus D5 AA 96 address prologues */
    memset(img, 0xFF, 4000);
    for (int i = 0; i < 20; i++) {
        size_t off = 4000 + (size_t)i * 100;
        img[off] = 0xD5; img[off + 1] = 0xAA; img[off + 2] = 0x96;
    }
    ASSERT(probe_sized(&uft_format_plugin_nib, img, NIB, NIB, &gcr_conf));
    ASSERT(gcr_conf > empty_conf);

    /* one byte short is not a NIB */
    ASSERT(!probe_sized(&uft_format_plugin_nib, img, NIB, NIB - 1, &empty_conf));

    free(img);
}

TEST(woz_accepts_v1_and_v2_signatures) {
    uint8_t hdr[32];
    int conf = -1;

    memset(hdr, 0, sizeof(hdr));
    memcpy(hdr, "WOZ1", 4);
    ASSERT(probe_sized(&uft_format_plugin_woz, hdr, sizeof(hdr), sizeof(hdr), &conf));
    ASSERT(conf >= 95);

    memcpy(hdr, "WOZ2", 4);
    ASSERT(probe_sized(&uft_format_plugin_woz, hdr, sizeof(hdr), sizeof(hdr), &conf));

    /* WOZ3 does not exist and must not be accepted preemptively */
    memcpy(hdr, "WOZ3", 4);
    ASSERT(!probe_sized(&uft_format_plugin_woz, hdr, sizeof(hdr), sizeof(hdr), &conf));

    memcpy(hdr, "WOZ2", 4);
    ASSERT(!probe_sized(&uft_format_plugin_woz, hdr, 4, 4, &conf));
}

/*---------------------------------------------------------------------------
 * Wave 5 (MF-389) — do, po, img, td0
 *   do/po  size-only, BOTH exactly 143360        (uft_do.c:10, uft_po.c:10)
 *   img    table of nine PC geometries + FAT12 boot-sector bonus
 *                                                (uft_img.c known_geometries)
 *   td0    LE16 magic 'TD' 0x5444 or 'td' 0x6474 (uft_td0.c:8,9)
 *--------------------------------------------------------------------------*/

TEST(do_and_po_are_ambiguous_by_design) {
    /* Both Apple formats are 143360 bytes and neither probe looks at content,
     * so BOTH claim the same file and only the confidence separates them.
     * A replica test cannot notice this — it never sees the other plugin. */
    uint8_t hdr[64];
    memset(hdr, 0, sizeof(hdr));
    int do_conf = -1, po_conf = -1;

    ASSERT(probe_sized(&uft_format_plugin_do, hdr, sizeof(hdr), 143360, &do_conf));
    ASSERT(probe_sized(&uft_format_plugin_po, hdr, sizeof(hdr), 143360, &po_conf));

    /* Both accept. Both report modest confidence, and DO ranks above PO —
     * that ordering is the only thing distinguishing them today. */
    ASSERT(do_conf > 0 && po_conf > 0);
    ASSERT(do_conf > po_conf);
    ASSERT(do_conf < 80 && po_conf < 80);

    ASSERT(!probe_sized(&uft_format_plugin_do, hdr, sizeof(hdr), 143359, &do_conf));
    ASSERT(!probe_sized(&uft_format_plugin_po, hdr, sizeof(hdr), 143361, &po_conf));
}

TEST(img_accepts_the_nine_known_pc_geometries) {
    static const size_t sizes[] = {
        163840, 184320, 327680, 368640, 737280,
        1228800, 1474560, 1720320, 2949120
    };
    uint8_t hdr[64];
    memset(hdr, 0, sizeof(hdr));

    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        int conf = -1;
        ASSERT(probe_sized(&uft_format_plugin_img, hdr, sizeof(hdr), sizes[i], &conf));
        ASSERT(conf > 0);
    }
}

TEST(img_confidence_rises_with_a_fat12_boot_sector) {
    /* Size alone yields the base confidence; a plausible FAT12 boot sector
     * must raise it. Needs a full 512-byte sector, not just a size argument. */
    uint8_t sec[512];
    memset(sec, 0, sizeof(sec));
    int plain = -1;
    ASSERT(probe_sized(&uft_format_plugin_img, sec, sizeof(sec), 1474560, &plain));

    sec[0] = 0xEB; sec[1] = 0x3C; sec[2] = 0x90;      /* JMP short + NOP */
    memcpy(sec + 3, "MSDOS5.0", 8);
    sec[510] = 0x55; sec[511] = 0xAA;                 /* boot signature */
    int booted = -1;
    ASSERT(probe_sized(&uft_format_plugin_img, sec, sizeof(sec), 1474560, &booted));
    ASSERT(booted >= plain);
}

TEST(td0_accepts_the_real_teledisk_signatures) {
    /* REGRESSION GUARD for the byte-order bug this test found (FMT-13):
     * the plugin's normal-magic constant was byte-swapped, so uncompressed
     * TD0 images — the common case — were never recognised. The old replica
     * test asserted the SAME wrong constant and even explained it in a
     * comment, which is why it stayed green for as long as it existed. */
    uint8_t hdr[32];
    int conf = -1;

    /* normal / RLE: file starts with the ASCII bytes 'T','D' */
    memset(hdr, 0, sizeof(hdr));
    hdr[0] = 'T'; hdr[1] = 'D';
    ASSERT(probe_sized(&uft_format_plugin_td0, hdr, sizeof(hdr), sizeof(hdr), &conf));
    ASSERT(conf >= 95);

    /* advanced / Huffman: 't','d' */
    hdr[0] = 't'; hdr[1] = 'd';
    ASSERT(probe_sized(&uft_format_plugin_td0, hdr, sizeof(hdr), sizeof(hdr), &conf));

    /* "DT" is the byte-swapped spelling and must NOT be accepted */
    hdr[0] = 'D'; hdr[1] = 'T';
    ASSERT(!probe_sized(&uft_format_plugin_td0, hdr, sizeof(hdr), sizeof(hdr), &conf));

    /* mixed case is not a TD0 either */
    hdr[0] = 'T'; hdr[1] = 'd';
    ASSERT(!probe_sized(&uft_format_plugin_td0, hdr, sizeof(hdr), sizeof(hdr), &conf));

    hdr[0] = 'T'; hdr[1] = 'D';
    ASSERT(!probe_sized(&uft_format_plugin_td0, hdr, 1, 1, &conf));
}

/*---------------------------------------------------------------------------
 * Wave 6 (MF-390) — fdi, ipf, scp, stx
 *   fdi  "FDI" + >= 14 bytes; confidence depends on plausible geometry
 *                                                (uft_fdi_plugin.c:27,28)
 *   ipf  "CAPS" at offset 0, >= 12 bytes         (uft_ipf_plugin.c)
 *   scp  "SCP" + a full 16-byte header           (uft_scp_plugin.c)
 *   stx  "RSY\0" + >= 16 bytes                   (uft_stx_plugin.c)
 *--------------------------------------------------------------------------*/

TEST(fdi_grades_confidence_by_geometry_plausibility) {
    uint8_t hdr[64];
    int weak = -1, strong = -1;

    /* signature present but geometry zeroed -> lower confidence */
    memset(hdr, 0, sizeof(hdr));
    memcpy(hdr, "FDI", 3);
    ASSERT(probe_sized(&uft_format_plugin_fdi, hdr, sizeof(hdr), sizeof(hdr), &weak));

    /* plausible geometry (80 cylinders, 2 heads) -> higher confidence */
    hdr[4] = 80; hdr[5] = 0;        /* LE16 cylinders */
    hdr[6] = 2;  hdr[7] = 0;        /* LE16 heads     */
    ASSERT(probe_sized(&uft_format_plugin_fdi, hdr, sizeof(hdr), sizeof(hdr), &strong));
    ASSERT(strong > weak);

    /* an absurd cylinder count falls back to the lower grade, but the file is
     * still an FDI — the plugin does not reject it outright */
    hdr[4] = 0xFF; hdr[5] = 0x00;   /* 255 cylinders */
    int absurd = -1;
    ASSERT(probe_sized(&uft_format_plugin_fdi, hdr, sizeof(hdr), sizeof(hdr), &absurd));
    ASSERT(absurd < strong);

    memcpy(hdr, "FDI", 3);
    ASSERT(!probe_sized(&uft_format_plugin_fdi, hdr, 8, 8, &weak));
}

TEST(ipf_requires_the_caps_magic) {
    uint8_t hdr[64];
    int conf = -1;

    memset(hdr, 0, sizeof(hdr));
    memcpy(hdr, "CAPS", 4);
    ASSERT(probe_sized(&uft_format_plugin_ipf, hdr, sizeof(hdr), sizeof(hdr), &conf));
    ASSERT(conf >= 90);

    memset(hdr, 0, sizeof(hdr));
    memcpy(hdr, "CAPT", 4);
    ASSERT(!probe_sized(&uft_format_plugin_ipf, hdr, sizeof(hdr), sizeof(hdr), &conf));

    memcpy(hdr, "CAPS", 4);
    ASSERT(!probe_sized(&uft_format_plugin_ipf, hdr, 8, 8, &conf));
}

TEST(scp_requires_magic_and_a_full_header) {
    uint8_t hdr[64];
    int conf = -1;

    memset(hdr, 0, sizeof(hdr));
    memcpy(hdr, "SCP", 3);
    ASSERT(probe_sized(&uft_format_plugin_scp, hdr, sizeof(hdr), sizeof(hdr), &conf));
    ASSERT(conf > 0);

    /* the 16-byte header must be present in full */
    ASSERT(!probe_sized(&uft_format_plugin_scp, hdr, 8, 8, &conf));

    memset(hdr, 0, sizeof(hdr));
    memcpy(hdr, "SCQ", 3);
    ASSERT(!probe_sized(&uft_format_plugin_scp, hdr, sizeof(hdr), sizeof(hdr), &conf));
}

TEST(stx_requires_the_pasti_magic_including_its_nul) {
    uint8_t hdr[64];
    int conf = -1;

    /* Pasti signature is "RSY" followed by a NUL byte — the NUL is part of it */
    memset(hdr, 0, sizeof(hdr));
    hdr[0] = 'R'; hdr[1] = 'S'; hdr[2] = 'Y'; hdr[3] = 0x00;
    ASSERT(probe_sized(&uft_format_plugin_stx, hdr, sizeof(hdr), sizeof(hdr), &conf));
    ASSERT(conf >= 95);

    hdr[3] = '!';
    ASSERT(!probe_sized(&uft_format_plugin_stx, hdr, sizeof(hdr), sizeof(hdr), &conf));

    hdr[3] = 0x00;
    ASSERT(!probe_sized(&uft_format_plugin_stx, hdr, 8, 8, &conf));
}

/*---------------------------------------------------------------------------
 * Wave 7 (MF-391) — 2img, dsk_cpc
 * The last two replicas whose format actually has a registered plugin. Note
 * the symbol names differ from the old test file names: test_2mg_plugin.c and
 * test_dsk_plugin.c named formats that exist in the registry as `2img` and
 * `dsk_cpc`.
 *   2img     LE32 magic "2IMG" 0x474D4932, header 64  (uft_2img.c:28,29)
 *   dsk_cpc  "EXTENDED" or "MV - CPC", >= 8 bytes      (uft_dsk_cpc.c)
 *--------------------------------------------------------------------------*/

TEST(img2_requires_its_four_byte_magic) {
    uint8_t hdr[128];
    int conf = -1;

    memset(hdr, 0, sizeof(hdr));
    memcpy(hdr, "2IMG", 4);
    ASSERT(probe_sized(&uft_format_plugin_2img, hdr, sizeof(hdr), sizeof(hdr), &conf));
    ASSERT(conf >= 95);

    memcpy(hdr, "GMI2", 4);                  /* reversed */
    ASSERT(!probe_sized(&uft_format_plugin_2img, hdr, sizeof(hdr), sizeof(hdr), &conf));

    /* the 64-byte header must be present */
    memcpy(hdr, "2IMG", 4);
    ASSERT(!probe_sized(&uft_format_plugin_2img, hdr, 32, 32, &conf));
}

TEST(dsk_cpc_and_edsk_both_claim_extended_dsk_files) {
    /* Cross-plugin overlap, pinned because it is easy to break by accident:
     * dsk_cpc matches on the first 8 bytes "EXTENDED", while edsk requires the
     * full 16-byte "EXTENDED CPC DSK". Any extended Amstrad image is therefore
     * claimed by BOTH plugins, and both report the same high confidence — so
     * the winner depends on registry order, not on evidence. */
    uint8_t hdr[64];
    int dsk_conf = -1, edsk_conf = -1;

    memset(hdr, 0, sizeof(hdr));
    memcpy(hdr, "EXTENDED CPC DSK", 16);
    ASSERT(probe_sized(&uft_format_plugin_dsk_cpc, hdr, sizeof(hdr), sizeof(hdr), &dsk_conf));
    ASSERT(probe_sized(&uft_format_plugin_edsk, hdr, sizeof(hdr), sizeof(hdr), &edsk_conf));
    ASSERT(dsk_conf == edsk_conf);

    /* the plain CPC header is claimed by both as well */
    memset(hdr, 0, sizeof(hdr));
    memcpy(hdr, "MV - CPC", 8);
    ASSERT(probe_sized(&uft_format_plugin_dsk_cpc, hdr, sizeof(hdr), sizeof(hdr), &dsk_conf));
    ASSERT(probe_sized(&uft_format_plugin_edsk, hdr, sizeof(hdr), sizeof(hdr), &edsk_conf));

    /* neither accepts something that only looks similar */
    memset(hdr, 0, sizeof(hdr));
    memcpy(hdr, "EXTENDEX", 8);
    ASSERT(!probe_sized(&uft_format_plugin_dsk_cpc, hdr, sizeof(hdr), sizeof(hdr), &dsk_conf));
    ASSERT(!probe_sized(&uft_format_plugin_edsk, hdr, sizeof(hdr), sizeof(hdr), &edsk_conf));
}

int main(void) {
    printf("=== Real plugin probes, wave 1 (MF-385) ===\n");
    RUN(d64_accepts_all_eight_production_sizes);
    RUN(d64_rejects_neighbouring_sizes);
    RUN(d64_confidence_rises_with_a_real_bam_link);
    RUN(d71_accepts_both_variants_and_rejects_others);
    RUN(d81_accepts_both_variants_and_rejects_others);
    RUN(g64_requires_its_signature);
    RUN(hfe_accepts_v1_and_v3_signatures);
    RUN(plugins_do_not_claim_each_others_headers);
    RUN(atr_requires_its_little_endian_magic);
    RUN(d88_checks_media_byte_and_declared_size);
    RUN(dc42_requires_big_endian_magic_at_offset_82);
    RUN(imd_requires_its_ascii_signature);
    RUN(atx_signature_is_byte_order_correct);
    RUN(cqm_requires_its_three_byte_marker);
    RUN(jv1_accepts_only_whole_track_multiples);
    RUN(xfd_is_deliberately_permissive_outside_its_canonical_sizes);
    RUN(edsk_accepts_both_amstrad_headers);
    RUN(msa_magic_is_big_endian);
    RUN(nib_requires_exact_size_and_grades_gcr_content);
    RUN(woz_accepts_v1_and_v2_signatures);
    RUN(do_and_po_are_ambiguous_by_design);
    RUN(img_accepts_the_nine_known_pc_geometries);
    RUN(img_confidence_rises_with_a_fat12_boot_sector);
    RUN(td0_accepts_the_real_teledisk_signatures);
    RUN(fdi_grades_confidence_by_geometry_plausibility);
    RUN(ipf_requires_the_caps_magic);
    RUN(scp_requires_magic_and_a_full_header);
    RUN(stx_requires_the_pasti_magic_including_its_nul);
    RUN(img2_requires_its_four_byte_magic);
    RUN(dsk_cpc_and_edsk_both_claim_extended_dsk_files);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
