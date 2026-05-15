/*
 * uft_flux_decode.c — UFT-side decoder for the P3.2 differential
 * conformance harness (task #109).
 *
 * NOT a product CLI. This is a TEST FIXTURE: it links the UFT flux
 * decode engine directly and is invoked by tests/differential/
 * test_gw_parity.py. UFT itself is GUI-only — this helper is the
 * bridge between the Python differential orchestrator and the C
 * engine (see memory feedback_no_cli: tests target the core library).
 *
 *   usage: uft_flux_decode <in.scp> <encoding> <out.img>
 *                          <heads> <spt> <secsize> <first_sec> [bitcell_ns]
 *
 *   encoding   : mfm        uniform-geometry IBM-style MFM (IBM-DD/HD, AtariST)
 *                gcr_c64    Commodore 1541 GCR (zone geometry, 40-track .d64)
 *                gcr_apple  Apple II 5.25" GCR (35-track .do, DOS 3.3 skew)
 *                amiga      AmigaDOS trackdisk MFM (80-cyl 2-head 11-sec .adf)
 *   heads      : 1 or 2          (mfm only; fixed-geometry encodings ignore it)
 *   spt        : sectors/track   (mfm only)
 *   secsize    : bytes/sector    (mfm only)
 *   first_sec  : first sector no (mfm: IBM/AtariST = 1)
 *   bitcell_ns : bit-cell time; 0 = decoder default. mfm HD ~1000.
 *
 * Pipeline: SCP file -> uft_scp_parser (flux intervals, ns) ->
 * flux_raw_data_t (cumulative ticks @ 1 GHz, so 1 tick = 1 ns) ->
 * flux_decode_{mfm,gcr_c64,gcr_apple,amiga}() -> decoded sectors ->
 * flat sector image in the layout gw also uses.
 *
 *   mfm:       offset = ((cyl*heads + head)*spt + (sec-first_sec)) * secsize
 *   gcr_c64:   offset = d64_track_offset(cyl) + sec*256   (40-track .d64)
 *   gcr_apple: offset = (cyl*16 + dos33_skew[sec]) * 256  (35-track .do)
 *   amiga:     offset = ((cyl*2 + head)*11 + sec) * 512   (80-cyl .adf)
 *
 * SCOPE: all four flux-encoding families the differential corpus
 * exercises are wired and reach byte-exact parity with gw 1.23. The
 * uniform-geometry MFM family is geometry-parameterised (heads/spt/...);
 * the three fixed-geometry encodings hardcode their disk class.
 *
 * Exit codes: 0 ok | 1 usage | 2 scp/decode error
 */
#include "uft/flux/uft_scp_parser.h"
#include "uft/flux/uft_flux_decoder.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXIT_OK            0
#define EXIT_USAGE         1
#define EXIT_SCP_ERROR     2

typedef enum { ENC_MFM, ENC_GCR_C64, ENC_GCR_APPLE, ENC_AMIGA } helper_enc_t;

/* ── Commodore 1541 .d64 geometry ────────────────────────────────────
 * gw's `commodore.1541` diskdef is the 40-track .d64 (768 sectors,
 * 196608 bytes). Zone sector counts: 21/19/18/17. The engine carries
 * the same table internally (uft_flux_decoder.c c64_sectors_per_track);
 * this is the test-fixture copy used only for image placement. */
static const int d64_spt[40] = {
    21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,  /* tracks  1-17 */
    19,19,19,19,19,19,19,                                /* tracks 18-24 */
    18,18,18,18,18,18,                                   /* tracks 25-30 */
    17,17,17,17,17,17,17,17,17,17,                       /* tracks 31-40 */
};
#define D64_TRACKS      40
#define D64_SECSIZE     256
#define D64_IMAGE_SIZE  196608  /* sum(d64_spt) * 256 */

/* Byte offset of (0-based track) cyl in the .d64 image. */
static size_t d64_track_offset(int cyl)
{
    size_t off = 0;
    for (int t = 0; t < cyl && t < D64_TRACKS; ++t)
        off += (size_t)d64_spt[t] * D64_SECSIZE;
    return off;
}

/* ── Apple II DOS 3.3 5.25" geometry ─────────────────────────────────
 * gw's `apple2.appledos.140` is the 35-track, 16-sector, 256-byte .do
 * image (143360 B). The engine's flux_decode_gcr_apple() returns the
 * PHYSICAL sector number from the address field; the .do on-disk order
 * is DOS 3.3 LOGICAL. The physical->logical de-interleave below is the
 * standard DOS 3.3 soft-skew table — it is verified against gw's actual
 * `.do` output by tests/differential/test_gw_parity.py (a wrong table
 * fails the byte-exact differential, it is not taken on faith). */
static const int apple_dos33_phys_to_log[16] = {
    0x0, 0x7, 0xE, 0x6, 0xD, 0x5, 0xC, 0x4,
    0xB, 0x3, 0xA, 0x2, 0x9, 0x1, 0x8, 0xF,
};
#define APPLE_TRACKS      35
#define APPLE_SPT         16
#define APPLE_SECSIZE     256
#define APPLE_IMAGE_SIZE  143360  /* 35 * 16 * 256 */

/* ── Amiga AmigaDOS DD 880K geometry ─────────────────────────────────
 * gw's `amiga.amigados` is the 80-cyl, 2-head, 11-sector, 512-byte
 * .adf (901120 B). flux_decode_amiga() returns the AmigaDOS track
 * number in cylinder/head already; .adf order is (cyl, head, sector). */
#define AMIGA_CYLS        80
#define AMIGA_HEADS       2
#define AMIGA_SPT         11
#define AMIGA_SECSIZE     512
#define AMIGA_IMAGE_SIZE  901120  /* 80 * 2 * 11 * 512 */

/* ── SCP track -> flux_raw_data_t ────────────────────────────────────
 * SCP flux_data is ns intervals (uft_scp_parser converts cells -> ns,
 * with 0 entries as overflow placeholders). flux_decode_* wants
 * cumulative transition TIMES in sample ticks. A 1 GHz sample rate
 * makes 1 tick == 1 ns; the 0 placeholders are not real transitions
 * and are skipped. Caller owns *trans_out and must free() it. */
static int scp_track_to_flux(uft_scp_ctx_t *scp, int t,
                             flux_raw_data_t *flux, uint32_t **trans_out)
{
    *trans_out = NULL;
    if (!uft_scp_has_track(scp, t)) return 0;

    uft_scp_track_data_t td;
    memset(&td, 0, sizeof(td));
    if (uft_scp_read_track(scp, t, &td) != UFT_SCP_OK) return 0;
    if (td.revolution_count == 0 || td.revolutions[0].flux_count == 0) {
        uft_scp_free_track(&td);
        return 0;
    }

    const uft_scp_rev_data_t *rev = &td.revolutions[0];
    uint32_t *trans = (uint32_t *)malloc(rev->flux_count * sizeof(uint32_t));
    if (!trans) { uft_scp_free_track(&td); return 0; }

    size_t   n   = 0;
    uint64_t cum = 0;
    for (uint32_t i = 0; i < rev->flux_count; ++i) {
        uint32_t iv = rev->flux_data ? rev->flux_data[i] : 0u;
        if (iv == 0) continue;            /* SCP overflow placeholder */
        cum += iv;
        trans[n++] = (uint32_t)cum;
    }

    uft_scp_free_track(&td);

    memset(flux, 0, sizeof(*flux));
    flux->transitions      = trans;
    flux->transition_count = n;
    flux->sample_rate      = 1000000000u;  /* 1 GHz -> 1 tick = 1 ns */
    *trans_out = trans;
    return 1;
}

int main(int argc, char **argv)
{
    if (argc < 8 || argc > 9) {
        fprintf(stderr,
            "usage: %s <in.scp> <encoding> <out.img> <heads> <spt> "
            "<secsize> <first_sec> [bitcell_ns]\n", argv[0]);
        return EXIT_USAGE;
    }
    const char  *scp_path = argv[1];
    const char  *encoding = argv[2];
    const char  *out_path = argv[3];
    const int    heads    = atoi(argv[4]);
    const int    spt      = atoi(argv[5]);
    const int    secsize  = atoi(argv[6]);
    const int    first_sec= atoi(argv[7]);
    const uint32_t bitcell_ns = (argc == 9) ? (uint32_t)strtoul(argv[8], NULL, 10) : 0u;

    helper_enc_t enc;
    if      (strcmp(encoding, "mfm")       == 0) enc = ENC_MFM;
    else if (strcmp(encoding, "gcr_c64")   == 0) enc = ENC_GCR_C64;
    else if (strcmp(encoding, "gcr_apple") == 0) enc = ENC_GCR_APPLE;
    else if (strcmp(encoding, "amiga")     == 0) enc = ENC_AMIGA;
    else {
        fprintf(stderr, "uft_flux_decode: unknown encoding '%s' "
                "(mfm, gcr_c64, gcr_apple, amiga) — see file header\n",
                encoding);
        return EXIT_USAGE;
    }

    if (enc == ENC_MFM &&
        (heads < 1 || heads > 2 || spt < 1 || secsize < 1 || first_sec < 0)) {
        fprintf(stderr, "uft_flux_decode: bad mfm geometry args\n");
        return EXIT_USAGE;
    }

    uft_scp_ctx_t *scp = uft_scp_create();
    if (!scp) {
        fprintf(stderr, "uft_flux_decode: uft_scp_create failed\n");
        return EXIT_SCP_ERROR;
    }
    if (uft_scp_open(scp, scp_path) != UFT_SCP_OK) {
        fprintf(stderr, "uft_flux_decode: cannot open SCP '%s'\n", scp_path);
        uft_scp_destroy(scp);
        return EXIT_SCP_ERROR;
    }

    /* Output image. mfm: sized generously, trimmed on write to the real
     * extent. gcr_c64: fixed 40-track .d64. gcr_apple: fixed 35-track .do. */
    size_t image_size;
    switch (enc) {
    case ENC_GCR_C64:   image_size = (size_t)D64_IMAGE_SIZE;   break;
    case ENC_GCR_APPLE: image_size = (size_t)APPLE_IMAGE_SIZE; break;
    case ENC_AMIGA:     image_size = (size_t)AMIGA_IMAGE_SIZE; break;
    default:            image_size = (size_t)UFT_SCP_MAX_TRACKS
                            * (size_t)heads * (size_t)spt * (size_t)secsize;
    }
    uint8_t *image = (uint8_t *)calloc(image_size, 1);
    if (!image) {
        fprintf(stderr, "uft_flux_decode: out of memory\n");
        uft_scp_destroy(scp);
        return EXIT_SCP_ERROR;
    }

    int    placed_sectors = 0;
    /* GCR images have a fixed extent — the whole image is the output even
     * if a track fails to decode (a hole reads as the calloc zero, and
     * the differential then shows exactly which track diverged). */
    size_t highest_offset = (enc == ENC_GCR_C64)   ? (size_t)D64_IMAGE_SIZE
                          : (enc == ENC_GCR_APPLE) ? (size_t)APPLE_IMAGE_SIZE
                          : (enc == ENC_AMIGA)     ? (size_t)AMIGA_IMAGE_SIZE
                          : 0;

    for (int t = 0; t < UFT_SCP_MAX_TRACKS; ++t) {
        flux_raw_data_t flux;
        uint32_t       *trans = NULL;
        if (!scp_track_to_flux(scp, t, &flux, &trans)) continue;

        flux_decoder_options_t opts;
        flux_decoder_options_init(&opts);
        opts.use_pll = true;

        flux_decoded_track_t dt;
        flux_decoded_track_init(&dt);
        flux_status_t st;

        opts.bitcell_ns = bitcell_ns;           /* 0 -> per-decoder default */
        switch (enc) {
        case ENC_MFM:
            opts.encoding = FLUX_ENC_MFM;
            st = flux_decode_mfm(&flux, &dt, &opts);
            break;
        case ENC_GCR_C64:
            opts.encoding = FLUX_ENC_GCR_C64;
            st = flux_decode_gcr_c64(&flux, &dt, &opts);
            break;
        case ENC_GCR_APPLE:
            opts.encoding = FLUX_ENC_GCR_APPLE;
            st = flux_decode_gcr_apple(&flux, &dt, &opts);
            break;
        default: /* ENC_AMIGA */
            opts.encoding = FLUX_ENC_AMIGA;
            st = flux_decode_amiga(&flux, &dt, &opts);
            break;
        }

        if (st == FLUX_OK) {
            for (size_t s = 0; s < dt.sector_count; ++s) {
                const flux_decoded_sector_t *sec = &dt.sectors[s];
                if (!sec->data || sec->data_size == 0) continue;

                size_t off, slot;
                if (enc == ENC_MFM) {
                    int rel = (int)sec->sector - first_sec;
                    if (sec->head >= heads || rel < 0 || rel >= spt) continue;
                    off  = (((size_t)sec->cylinder * heads + sec->head)
                            * spt + (size_t)rel) * (size_t)secsize;
                    slot = (size_t)secsize;
                } else if (enc == ENC_GCR_C64) {
                    if (sec->cylinder >= D64_TRACKS) continue;
                    if (sec->sector >= d64_spt[sec->cylinder]) continue;
                    off  = d64_track_offset((int)sec->cylinder)
                           + (size_t)sec->sector * D64_SECSIZE;
                    slot = D64_SECSIZE;
                } else if (enc == ENC_GCR_APPLE) {
                    if (sec->cylinder >= APPLE_TRACKS) continue;
                    if (sec->sector >= APPLE_SPT) continue;
                    int log = apple_dos33_phys_to_log[sec->sector];
                    off  = ((size_t)sec->cylinder * APPLE_SPT + (size_t)log)
                           * APPLE_SECSIZE;
                    slot = APPLE_SECSIZE;
                } else { /* ENC_AMIGA — .adf order is (cyl, head, sector) */
                    if (sec->cylinder >= AMIGA_CYLS) continue;
                    if (sec->head >= AMIGA_HEADS) continue;
                    if (sec->sector >= AMIGA_SPT) continue;
                    off  = (((size_t)sec->cylinder * AMIGA_HEADS
                             + sec->head) * AMIGA_SPT + sec->sector)
                           * AMIGA_SECSIZE;
                    slot = AMIGA_SECSIZE;
                }

                if (off + slot > image_size) continue;
                size_t len = sec->data_size < slot ? sec->data_size : slot;
                memcpy(image + off, sec->data, len);
                if (off + slot > highest_offset) highest_offset = off + slot;
                placed_sectors++;
            }
        }

        flux_decoded_track_free(&dt);
        free(trans);
    }

    uft_scp_destroy(scp);

    if (placed_sectors == 0) {
        fprintf(stderr, "uft_flux_decode: decoded 0 sectors from '%s'\n",
                scp_path);
        free(image);
        return EXIT_SCP_ERROR;
    }

    FILE *out = fopen(out_path, "wb");
    if (!out) {
        fprintf(stderr, "uft_flux_decode: cannot write '%s'\n", out_path);
        free(image);
        return EXIT_SCP_ERROR;
    }
    size_t wrote = fwrite(image, 1, highest_offset, out);
    fclose(out);
    free(image);

    if (wrote != highest_offset) {
        fprintf(stderr, "uft_flux_decode: short write to '%s'\n", out_path);
        return EXIT_SCP_ERROR;
    }

    fprintf(stderr, "uft_flux_decode: %d sectors -> %zu bytes -> %s\n",
            placed_sectors, highest_offset, out_path);
    return EXIT_OK;
}
