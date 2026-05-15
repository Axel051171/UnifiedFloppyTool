/*
 * test_protection_detection.c — improvement test: copy-protection
 * detection (#110b, P3.3).
 *
 * Strategy: docs/TESTER_STRATEGY.md §3 (improvement/copy_protection/).
 *
 * WHAT THIS DEFENDS
 * -----------------
 * Greaseweazle captures flux faithfully but does not *analyse* it — it
 * has no notion of "this track carries a copy-protection scheme" and
 * never names one. UFT's mission ("Keine erfundenen Daten" — but also:
 * surface what is really there) includes classifying historical
 * protection. This is an improvement test: gw cannot pass it because gw
 * has no protection-analysis stage at all.
 *
 * The test feeds each detector a synthetic track carrying a KNOWN
 * protection signature — built to that scheme's documented parameters
 * (sync word, length, fill pattern, signature block) — and asserts UFT
 * names the scheme. The fixtures are synthesised here, deterministically,
 * from the published scheme definitions in uft_longtrack.h /
 * uft_fuzzy_bits.h; they are not captured disks and never claim to be.
 *
 * Scope vs. the original TESTER_STRATEGY plan: "long-track detection"
 * and "Dungeon Master fuzzy-bit detection" are delivered here. The
 * planned "Lenslok" case is dropped — Lenslok is a physical lens-overlay
 * protection, not a disk-flux scheme, and UFT has no Lenslok detector to
 * exercise (there is nothing on the disk to detect). See the category
 * README.
 *
 * Self-contained: synthesises every fixture in-process.
 */
#include "uft/protection/uft_longtrack.h"
#include "uft/protection/uft_fuzzy_bits.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ── Linker-satisfying stubs ─────────────────────────────────────────
 * uft_fuzzy_bits.c also contains ctx-based functions
 * (uft_analyze_fuzzy_sector, uft_capture_fuzzy_flux, ...) that call into
 * the HAL. This test exercises ONLY the pure timing / CRC / serial /
 * pattern functions and never reaches a ctx path, so these symbols are
 * never invoked — they exist purely to resolve the link. */
int uft_hal_read_flux(void);
int uft_hal_write_flux(void);
int uft_hal_read_flux(void)  { return -1; }
int uft_hal_write_flux(void) { return -1; }

/* ── check harness ───────────────────────────────────────────────────*/
static int g_errors = 0;
#define CHECK(cond, msg)                                                 \
    do {                                                                 \
        if (!(cond)) {                                                   \
            ++g_errors;                                                  \
            fprintf(stderr, "[protection] FAIL %s:%d  %s\n",             \
                    __FILE__, __LINE__, (msg));                          \
        }                                                                \
    } while (0)

/* ════════════════════════════════════════════════════════════════════
 *  Longtrack synthesis + detection
 *
 *  uft_longtrack_find_sync() searches byte-aligned, MSB-first: a 16-bit
 *  sync 0xWXYZ lives at track_data[k]=0xWX, [k+1]=0xYZ; a 32-bit sync
 *  spans four bytes. So a fixture is just: a buffer of `track_bits/8`
 *  bytes, filled with the scheme's pattern byte, with the sync word's
 *  bytes (and any signature) written at a known offset.
 * ════════════════════════════════════════════════════════════════════ */

/* Allocate a track buffer of `track_bits` bits, pattern-filled. Caller
 * frees. A few guard bytes past the end cover get_word_at_bit()'s
 * one-byte look-ahead. */
static uint8_t *make_track(uint32_t track_bits, uint8_t fill, size_t *bytes_out)
{
    size_t bytes = track_bits / 8;
    uint8_t *buf = malloc(bytes + 8);
    if (!buf) { perror("malloc"); exit(2); }
    memset(buf, fill, bytes + 8);
    *bytes_out = bytes;
    return buf;
}

static void test_longtrack_protec(void)
{
    /* PROTEC: 16-bit sync 0x4454, min 107200 bits, 0x33 fill. */
    size_t bytes;
    uint32_t track_bits = 108000;                  /* > min, > normal+500 */
    uint8_t *t = make_track(track_bits, 0x33, &bytes);
    t[200] = 0x44; t[201] = 0x54;                   /* sync 0x4454 */

    uft_longtrack_result_t r;
    int rc = uft_longtrack_detect(t, track_bits, 5, 0, &r);
    CHECK(rc == 0, "PROTEC: detect returned non-zero");
    CHECK(r.detected, "PROTEC: not detected");
    CHECK(r.primary.type == UFT_LONGTRACK_PROTEC, "PROTEC: wrong primary type");
    CHECK(strcmp(uft_longtrack_type_name(r.primary.type), "PROTEC") == 0,
          "PROTEC: type name not 'PROTEC'");
    CHECK(r.primary.sync_word == 0x4454, "PROTEC: sync word not echoed");
    CHECK(r.primary.sync_offset == 200 * 8, "PROTEC: wrong sync offset");
    free(t);
}

static void test_longtrack_silmarils(void)
{
    /* Silmarils: 16-bit sync 0xA144, min 104128 bits, "ROD0" signature
     * required within 256 bytes of the sync. */
    size_t bytes;
    uint32_t track_bits = 106000;
    uint8_t *t = make_track(track_bits, 0x00, &bytes);
    t[300] = 0xA1; t[301] = 0x44;                   /* sync 0xA144 */
    memcpy(&t[320], "ROD0", 4);                     /* signature */

    uft_longtrack_result_t r;
    int rc = uft_longtrack_detect(t, track_bits, 12, 1, &r);
    CHECK(rc == 0, "Silmarils: detect returned non-zero");
    CHECK(r.detected, "Silmarils: not detected");
    CHECK(r.primary.type == UFT_LONGTRACK_SILMARILS,
          "Silmarils: wrong primary type");
    CHECK(strcmp(uft_longtrack_type_name(r.primary.type), "Silmarils") == 0,
          "Silmarils: type name not 'Silmarils'");
    CHECK(r.primary.signature_found, "Silmarils: ROD0 signature not found");
    /* Signature present -> the detector must report full confidence. */
    CHECK(r.confidence == UFT_LONGTRACK_CONF_CERTAIN,
          "Silmarils: signature should yield CERTAIN confidence");
    free(t);
}

static void test_longtrack_protoscan(void)
{
    /* Protoscan: 32-bit sync 0x41244124, min 102400 bits, 0x00 fill.
     * track_bits is chosen above the Tiertex max (103680) — so the
     * earlier-priority Tiertex detector cleanly declines — and above the
     * top-level longtrack gate (AmigaDOS-normal + 500 = 105500), so the
     * per-scheme detectors actually run. */
    size_t bytes;
    uint32_t track_bits = 106000;
    uint8_t *t = make_track(track_bits, 0x00, &bytes);
    t[150] = 0x41; t[151] = 0x24; t[152] = 0x41; t[153] = 0x24;

    uft_longtrack_result_t r;
    int rc = uft_longtrack_detect(t, track_bits, 7, 0, &r);
    CHECK(rc == 0, "Protoscan: detect returned non-zero");
    CHECK(r.detected, "Protoscan: not detected");
    CHECK(r.primary.type == UFT_LONGTRACK_PROTOSCAN,
          "Protoscan: wrong primary type");
    CHECK(strcmp(uft_longtrack_type_name(r.primary.type), "Protoscan") == 0,
          "Protoscan: type name not 'Protoscan'");
    CHECK(r.primary.sync_word == 0x41244124, "Protoscan: sync word not echoed");
    free(t);
}

static void test_longtrack_negative(void)
{
    /* A normal-length track must NOT be flagged as a longtrack — a false
     * positive here would be UFT inventing protection that isn't there,
     * the exact opposite of the forensic mandate. 105200 bits is past
     * the AmigaDOS-normal floor but below the longtrack threshold
     * (normal + 500). */
    size_t bytes;
    uint32_t track_bits = 105200;
    uint8_t *t = make_track(track_bits, 0x33, &bytes);
    /* Even with a stray 0x4454 in it, a normal-length track is rejected
     * before any per-scheme detector runs. */
    t[200] = 0x44; t[201] = 0x54;

    uft_longtrack_result_t r;
    int rc = uft_longtrack_detect(t, track_bits, 0, 0, &r);
    CHECK(rc == 0, "negative: detect returned non-zero");
    CHECK(!r.detected, "negative: normal-length track wrongly flagged");
    free(t);
}

/* ════════════════════════════════════════════════════════════════════
 *  Dungeon Master fuzzy-bit detection
 * ════════════════════════════════════════════════════════════════════ */

static void test_dm_fuzzy_pattern_positive(void)
{
    /* DM's documented signature: timings sit in the ambiguous ~5µs zone,
     * and consecutive pairs compensate to sum ~10µs. Alternating
     * 4.5 / 5.5 µs reproduces both properties — every timing is in the
     * fuzzy band (4.3 < t < 5.7) and every pair sums to 10.0. */
    enum { N = 64 };
    uft_flux_timing_t tm[N];
    for (int i = 0; i < N; i++) {
        tm[i].timing_us   = (i & 1) ? 5.5 : 4.5;
        tm[i].position_us = i * 5.0;
        tm[i].is_ambiguous = false;            /* let the detector judge */
    }
    CHECK(uft_detect_dm_fuzzy_pattern(tm, N),
          "DM fuzzy: documented compensating-pair pattern not detected");
}

static void test_dm_fuzzy_pattern_negative(void)
{
    /* A clean MFM track: uniform 4µs flux, no ambiguous timings, pairs
     * sum to 8µs not 10. Must NOT be flagged as fuzzy-bit protection. */
    enum { N = 64 };
    uft_flux_timing_t tm[N];
    for (int i = 0; i < N; i++) {
        tm[i].timing_us   = 4.0;
        tm[i].position_us = i * 4.0;
        tm[i].is_ambiguous = false;
    }
    CHECK(!uft_detect_dm_fuzzy_pattern(tm, N),
          "DM fuzzy: clean 4us MFM track wrongly flagged as fuzzy");

    /* Too few samples -> the detector must decline, not guess. */
    CHECK(!uft_detect_dm_fuzzy_pattern(tm, 4),
          "DM fuzzy: should decline on < 10 timings");
    CHECK(!uft_detect_dm_fuzzy_pattern(NULL, 64),
          "DM fuzzy: should decline on NULL input");
}

static void test_fuzzy_timing_boundaries(void)
{
    /* The ambiguous band is the open interval (4.3, 5.7) µs. */
    CHECK(uft_is_fuzzy_timing(5.0),  "5.0us should be fuzzy");
    CHECK(uft_is_fuzzy_timing(4.4),  "4.4us should be fuzzy");
    CHECK(uft_is_fuzzy_timing(5.6),  "5.6us should be fuzzy");
    CHECK(!uft_is_fuzzy_timing(4.0), "4.0us (valid MFM) is not fuzzy");
    CHECK(!uft_is_fuzzy_timing(6.0), "6.0us (valid MFM) is not fuzzy");
    CHECK(!uft_is_fuzzy_timing(4.3), "4.3us (band edge) is not fuzzy");
    CHECK(!uft_is_fuzzy_timing(5.7), "5.7us (band edge) is not fuzzy");

    /* Valid MFM flux spacings, with the default 10% tolerance. */
    CHECK(uft_is_valid_mfm_timing(4.0, 0.0),  "4.0us is valid MFM");
    CHECK(uft_is_valid_mfm_timing(6.0, 0.0),  "6.0us is valid MFM");
    CHECK(uft_is_valid_mfm_timing(8.0, 0.0),  "8.0us is valid MFM");
    CHECK(!uft_is_valid_mfm_timing(5.0, 0.0), "5.0us is not valid MFM");
}

static void test_dm_serial_crc_roundtrip(void)
{
    /* Build a Dungeon Master sector-7 protection block: PACE/FB marker,
     * "Seri" marker, a 4-byte serial and its CRC-8. uft_extract_dm_serial
     * must recover the serial and confirm the CRC. */
    uint8_t sector[512];
    memset(sector, 0, sizeof(sector));

    const uint8_t pace_fb[] = { 0x07, 'P', 'A', 'C', 'E', '/', 'F', 'B' };
    const uint8_t seri[]    = { 0x09, 'S', 'e', 'r', 'i' };
    const uint8_t serial[4] = { 0xDE, 0xAD, 0xBE, 0xEF };

    memcpy(sector + 0x00, pace_fb, sizeof(pace_fb));
    memcpy(sector + 0x08, seri,    sizeof(seri));
    memcpy(sector + 0x0D, serial,  4);
    sector[0x11] = uft_calc_dm_serial_crc(serial);

    uft_dm_serial_t out;
    int ok = uft_extract_dm_serial(sector, &out);
    CHECK(ok, "DM serial: extraction failed on a well-formed block");
    CHECK(memcmp(out.bytes, serial, 4) == 0, "DM serial: serial bytes wrong");
    CHECK(out.crc_valid, "DM serial: CRC reported invalid for a correct CRC");

    /* Corrupt the CRC -> extraction still parses, but flags it invalid:
     * the data is surfaced, the integrity failure is reported, nothing
     * is silently 'fixed'. */
    sector[0x11] ^= 0xFF;
    ok = uft_extract_dm_serial(sector, &out);
    CHECK(ok, "DM serial: extraction should still parse a bad-CRC block");
    CHECK(!out.crc_valid, "DM serial: bad CRC must be reported invalid");

    /* No PACE/FB marker -> not a DM protection block at all. */
    memset(sector, 0, sizeof(sector));
    CHECK(!uft_extract_dm_serial(sector, &out),
          "DM serial: a blank sector must not parse as a serial block");
}

int main(void)
{
    test_longtrack_protec();
    test_longtrack_silmarils();
    test_longtrack_protoscan();
    test_longtrack_negative();

    test_dm_fuzzy_pattern_positive();
    test_dm_fuzzy_pattern_negative();
    test_fuzzy_timing_boundaries();
    test_dm_serial_crc_roundtrip();

    if (g_errors) {
        fprintf(stderr, "[protection] %d check(s) failed\n", g_errors);
        return 1;
    }
    printf("[protection] all checks passed\n");
    return 0;
}
