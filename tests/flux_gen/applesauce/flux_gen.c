/**
 * @file tests/flux_gen/applesauce/flux_gen.c
 * @brief Synthetic Apple-domain GCR flux generator implementation.
 *
 * Encoding references (all public, documented):
 *   - 6-and-2 nibble table + prenibbilize + XOR-chain checksum:
 *     "Beneath Apple DOS" (Worth/Lechner), ch. 3 — the canonical
 *     public documentation of the DOS 3.3 disk format.
 *   - 4-and-4 address-field encoding: same source.
 *   - Self-sync 10-bit FF: same source (FF + two trailing zero cells).
 *   - Mac 3.5" zone geometry: Sony OA-D34V drive documentation as
 *     reflected in the WOZ/A2R community specs (12/11/10/9/8 sectors,
 *     five 16-track zones).
 *
 * Every emitted byte is traceable: nibble stream → bit stream → flux
 * tick deltas → LE32 serialisation. No fabricated data — payload bytes
 * come from the seeded RNG and are recoverable via the 6-and-2 decode.
 */

#include "flux_gen.h"

#include <stdlib.h>
#include <string.h>

/* ─── Deterministic RNG (xorshift64*) — identical to SCP/GW gens ────── */

static uint64_t rng_next(uint64_t *s)
{
    uint64_t x = *s;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    *s = x;
    return x * 0x2545F4914F6CDD1Dull;
}

static void rng_seed(uint64_t *s, uint64_t seed)
{
    *s = (seed == 0) ? 1 : seed;
}

/* ─── 6-and-2 write-translate table (Beneath Apple DOS) ─────────────── */

static const uint8_t GCR62[64] = {
    0x96, 0x97, 0x9A, 0x9B, 0x9D, 0x9E, 0x9F, 0xA6,
    0xA7, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB2, 0xB3,
    0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC,
    0xBD, 0xBE, 0xBF, 0xCB, 0xCD, 0xCE, 0xCF, 0xD3,
    0xD6, 0xD7, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE,
    0xDF, 0xE5, 0xE6, 0xE7, 0xE9, 0xEA, 0xEB, 0xEC,
    0xED, 0xEE, 0xEF, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6,
    0xF7, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};

uint8_t uft_as_flux_gcr62_nibble(uint8_t six)
{
    if (six > 63) return 0;
    return GCR62[six];
}

int uft_as_flux_gcr62_detrans(uint8_t nibble)
{
    for (int i = 0; i < 64; i++) {
        if (GCR62[i] == nibble) return i;
    }
    return -1;
}

/* ─── Mac 3.5" speed-zone table ─────────────────────────────────────── */

static const double MAC_ZONE_RPM[5] = {
    393.38, 429.17, 472.14, 524.57, 590.11
};
static const int MAC_ZONE_SECTORS[5] = { 12, 11, 10, 9, 8 };

int uft_as_flux_mac_zone_for_track(int track)
{
    if (track < 0 || track > UFT_AS_FLUX_GEN_MAX_TRACK_35) return -1;
    return track / 16;
}

int uft_as_flux_mac_sectors_for_track(int track)
{
    int z = uft_as_flux_mac_zone_for_track(track);
    return (z < 0) ? -1 : MAC_ZONE_SECTORS[z];
}

double uft_as_flux_mac_rpm_for_zone(int zone)
{
    if (zone < 0 || zone > 4) return 0.0;
    return MAC_ZONE_RPM[zone];
}

/* ─── Bit-stream → flux-tick emitter ────────────────────────────────
 *
 * The emitter models the write head: a '1' bit is a transition at the
 * end of its cell; '0' bits extend the current interval. Weak-bit
 * jitter (if armed) perturbs each interval by <=10%. Every interval is
 * medium-safety-checked before serialisation. */

typedef struct {
    uint8_t  *buf;             /* LE32 output */
    size_t    cap;             /* bytes */
    size_t    len;             /* bytes used */
    uint32_t  cell_ticks;      /* ticks per bit cell */
    uint32_t  zero_run;        /* pending 0-cells since last transition */
    uint64_t  total_ticks;     /* running revolution length */
    uint64_t *rng;             /* NULL => no jitter */
    uint8_t   jitter_pct;      /* 0..10 */
    int       oos;             /* out-of-spec flag (sticky) */
} bit_emitter_t;

static void emit_le32(bit_emitter_t *e, uint32_t v)
{
    if (e->len + 4 > e->cap) { e->oos = 1; return; }
    e->buf[e->len + 0] = (uint8_t)(v);
    e->buf[e->len + 1] = (uint8_t)(v >> 8);
    e->buf[e->len + 2] = (uint8_t)(v >> 16);
    e->buf[e->len + 3] = (uint8_t)(v >> 24);
    e->len += 4;
}

static void emit_bit(bit_emitter_t *e, int bit)
{
    if (e->oos) return;
    if (!bit) {
        e->zero_run++;
        return;
    }
    uint32_t ticks = (e->zero_run + 1u) * e->cell_ticks;
    e->zero_run = 0;

    if (e->rng && e->jitter_pct) {
        uint64_t r = rng_next(e->rng);
        int32_t jit_max = (int32_t)((uint64_t)ticks * e->jitter_pct / 100u);
        int32_t jit = (int32_t)(r % (uint64_t)(2 * jit_max + 1)) - jit_max;
        int64_t jt = (int64_t)ticks + jit;
        if (jt < 1) jt = 1;
        ticks = (uint32_t)jt;
    }

    /* Forensic-medium-safety: refuse out-of-spec intervals. */
    uint64_t ns = (uint64_t)ticks * 1000000000ull / UFT_AS_FLUX_GEN_SAMPLE_HZ;
    if (ns < UFT_AS_FLUX_GEN_MIN_NS || ns > UFT_AS_FLUX_GEN_MAX_NS) {
        e->oos = 1;
        return;
    }
    e->total_ticks += ticks;
    emit_le32(e, ticks);
}

/** Emit one 8-bit disk nibble, MSB first (all valid nibbles have MSB=1). */
static void emit_nibble(bit_emitter_t *e, uint8_t nib)
{
    for (int b = 7; b >= 0; b--) {
        emit_bit(e, (nib >> b) & 1);
    }
}

/** Emit one 10-bit self-sync FF (8 ones + 2 zero cells). */
static void emit_selfsync(bit_emitter_t *e)
{
    emit_nibble(e, 0xFF);
    emit_bit(e, 0);
    emit_bit(e, 0);
}

/* ─── 4-and-4 address-field byte (two nibbles) ──────────────────────── */

static void emit_44(bit_emitter_t *e, uint8_t b)
{
    emit_nibble(e, (uint8_t)(((b >> 1) & 0x55) | 0xAA));
    emit_nibble(e, (uint8_t)((b & 0x55) | 0xAA));
}

/* ─── 6-and-2 prenibbilize (256 bytes → 86 aux + 256 primary) ───────── */

static uint8_t swap2(uint8_t x)
{
    return (uint8_t)(((x & 1u) << 1) | ((x >> 1) & 1u));
}

static void prenib_62(const uint8_t data[256], uint8_t six[342])
{
    /* aux buffer: low-2-bits of bytes i, i+86, i+172 packed reversed. */
    for (int i = 0; i < 86; i++) {
        uint8_t v = swap2(data[i]);
        v = (uint8_t)(v | (swap2(data[i + 86]) << 2));
        if (i + 172 < 256) {
            v = (uint8_t)(v | (swap2(data[i + 172]) << 4));
        }
        six[85 - i] = v;              /* written high-index-first on disk */
    }
    for (int i = 0; i < 256; i++) {
        six[86 + i] = (uint8_t)(data[i] >> 2);
    }
}

/* ─── Track builders ────────────────────────────────────────────────── */

static uft_as_flux_gen_err_t finish_capture(bit_emitter_t *e,
                                            uft_as_flux_capture_t *out,
                                            double rpm, int sectors)
{
    if (e->oos) {
        free(e->buf);
        return UFT_AS_FLUX_GEN_ERR_OUT_OF_SPEC;
    }
    out->bytes            = e->buf;
    out->bytes_len        = e->len;
    out->transition_count = (uint32_t)(e->len / 4);
    out->revolution_ticks = (uint32_t)e->total_ticks;
    out->rpm              = rpm;
    out->sector_count     = sectors;
    return UFT_AS_FLUX_GEN_OK;
}

static uft_as_flux_gen_err_t validate_common(const uft_as_flux_params_t *p,
                                             uft_as_flux_capture_t *out)
{
    if (!p || !out) return UFT_AS_FLUX_GEN_ERR_NULL;
    memset(out, 0, sizeof(*out));
    if ((p->defects & UFT_AS_DEFECT_WEAK_BITS) &&
        (p->weak_jitter_pct < 1 ||
         p->weak_jitter_pct > UFT_AS_FLUX_GEN_MAX_JITTER_PCT)) {
        /* >10% deviation would be medium-unsafe on write-back. */
        return UFT_AS_FLUX_GEN_ERR_OUT_OF_SPEC;
    }
    return UFT_AS_FLUX_GEN_OK;
}

static int emitter_init(bit_emitter_t *e, uint64_t target_rev_ticks,
                        uint32_t cell_ticks, uint64_t *rng,
                        const uft_as_flux_params_t *p)
{
    memset(e, 0, sizeof(*e));
    /* Worst case one transition per bit cell: 4 bytes per cell. The
     * slack covers the padding loop's worst case when negative weak-bit
     * jitter (up to −10%) shortens every interval — up to ~11% more
     * sync bits are then needed to reach the target revolution time. */
    uint64_t max_bits = target_rev_ticks / cell_ticks;
    max_bits += max_bits / 8u + 4096u;
    e->cap  = (size_t)max_bits * 4u;
    e->buf  = (uint8_t *)calloc(1, e->cap);
    if (!e->buf) return 0;
    e->cell_ticks = cell_ticks;
    if (p->defects & UFT_AS_DEFECT_WEAK_BITS) {
        e->rng        = rng;
        e->jitter_pct = p->weak_jitter_pct;
    }
    return 1;
}

/* Apple II 5.25" 16-sector DOS 3.3 track. */
uft_as_flux_gen_err_t uft_as_flux_gen_a2_525(
    const uft_as_flux_params_t *params,
    uft_as_flux_capture_t      *out)
{
    uft_as_flux_gen_err_t v = validate_common(params, out);
    if (v != UFT_AS_FLUX_GEN_OK) return v;
    if (params->track < 0 || params->track > UFT_AS_FLUX_GEN_MAX_TRACK_525 ||
        params->side != 0) {
        return UFT_AS_FLUX_GEN_ERR_OUT_OF_SPEC;   /* stepper safety */
    }

    uint64_t rng;
    rng_seed(&rng, params->seed);

    /* Target: one revolution at 300 RPM = 200 ms = 1.6e6 ticks. */
    const uint64_t target_ticks =
        (uint64_t)(60.0 / UFT_AS_FLUX_GEN_RPM_525 * 1e9) / 125u;
    /* 4000 ns / 125 ns-per-tick = 32 ticks per bit cell. (Divide by the
     * tick period, NOT cell_ns * freq — that product overflows u32.) */
    const uint32_t cell_ticks = UFT_AS_FLUX_GEN_CELL_525_NS / 125u;

    bit_emitter_t e;
    /* Separate RNG stream for payload bytes so WEAK_BITS jitter does
     * not change WHAT data is on the track, only its timing. */
    uint64_t payload_rng;
    rng_seed(&payload_rng, params->seed ^ 0x5A5A5A5A5A5A5A5Aull);
    if (!emitter_init(&e, target_ticks, cell_ticks, &rng, params)) {
        return UFT_AS_FLUX_GEN_ERR_NULL;
    }

    /* Gap 1: 24 self-sync nibbles. */
    for (int i = 0; i < 24; i++) emit_selfsync(&e);

    for (int sec = 0; sec < 16; sec++) {
        const int drop_addr =
            (params->defects & UFT_AS_DEFECT_MISSING_ADDRESS) && sec == 3;
        const int bad_csum =
            (params->defects & UFT_AS_DEFECT_CHECKSUM_ERROR) && sec == 5;
        const int kill_sync =
            (params->defects & UFT_AS_DEFECT_SYNC_ANOMALY) && sec == 7;

        /* Address field: D5 AA 96, vol/trk/sec/chk in 4-and-4, DE AA EB. */
        if (!drop_addr) {
            emit_nibble(&e, 0xD5); emit_nibble(&e, 0xAA); emit_nibble(&e, 0x96);
            uint8_t vol = params->volume;
            uint8_t trk = (uint8_t)params->track;
            uint8_t sc  = (uint8_t)sec;
            emit_44(&e, vol);
            emit_44(&e, trk);
            emit_44(&e, sc);
            emit_44(&e, (uint8_t)(vol ^ trk ^ sc));
            emit_nibble(&e, 0xDE); emit_nibble(&e, 0xAA); emit_nibble(&e, 0xEB);
        }

        /* Gap 2: 7 self-sync — or, with SYNC_ANOMALY, non-sync junk
         * nibbles that a state machine cannot re-sync on. */
        if (kill_sync) {
            for (int i = 0; i < 7; i++) emit_nibble(&e, 0x96);
        } else {
            for (int i = 0; i < 7; i++) emit_selfsync(&e);
        }

        /* Data field: D5 AA AD + 342 six-bit values (XOR chain) +
         * checksum + DE AA EB. Payload = seeded RNG bytes. */
        emit_nibble(&e, 0xD5); emit_nibble(&e, 0xAA); emit_nibble(&e, 0xAD);
        uint8_t data[256];
        for (int i = 0; i < 256; i++) {
            data[i] = (uint8_t)(rng_next(&payload_rng) & 0xFF);
        }
        uint8_t six[342];
        prenib_62(data, six);
        uint8_t prev = 0;
        for (int i = 0; i < 342; i++) {
            emit_nibble(&e, GCR62[six[i] ^ prev]);
            prev = six[i];
        }
        /* Checksum nibble; CHECKSUM_ERROR replaces it with a DIFFERENT
         * valid nibble so decode succeeds but verification fails. */
        uint8_t csum_six = bad_csum ? (uint8_t)((prev + 1) & 0x3F) : prev;
        emit_nibble(&e, GCR62[csum_six]);
        emit_nibble(&e, 0xDE); emit_nibble(&e, 0xAA); emit_nibble(&e, 0xEB);

        /* FAKE_BITS: after sector 9's data field, a protection-style
         * zero-run of 5 bit cells (20 µs, in-spec absolute timing —
         * real MC3470 AGC produces random bits there). */
        if ((params->defects & UFT_AS_DEFECT_FAKE_BITS) && sec == 9) {
            for (int i = 0; i < 4; i++) emit_bit(&e, 0);
            emit_bit(&e, 1);
        }

        /* Gap 3: 12 self-sync. */
        for (int i = 0; i < 12; i++) emit_selfsync(&e);
    }

    /* Pad with self-sync to one nominal revolution. */
    while (!e.oos && e.total_ticks + 10u * cell_ticks < target_ticks) {
        emit_selfsync(&e);
    }

    return finish_capture(&e, out, UFT_AS_FLUX_GEN_RPM_525, 16);
}

/* Mac 3.5" zoned GCR track (structure real, data payload simplified —
 * DIVERGENCES.md D-8). */
uft_as_flux_gen_err_t uft_as_flux_gen_mac_35(
    const uft_as_flux_params_t *params,
    uft_as_flux_capture_t      *out)
{
    uft_as_flux_gen_err_t v = validate_common(params, out);
    if (v != UFT_AS_FLUX_GEN_OK) return v;
    if (params->track < 0 || params->track > UFT_AS_FLUX_GEN_MAX_TRACK_35 ||
        params->side < 0 || params->side > 1) {
        return UFT_AS_FLUX_GEN_ERR_OUT_OF_SPEC;   /* stepper safety */
    }

    const int zone    = uft_as_flux_mac_zone_for_track(params->track);
    const int sectors = MAC_ZONE_SECTORS[zone];
    double rpm        = MAC_ZONE_RPM[zone];
    if (params->defects & UFT_AS_DEFECT_CROSS_ZONE_SPEED) {
        /* Track mastered at the neighbouring zone's speed — classic
         * duplicator error / protection signature. */
        rpm = MAC_ZONE_RPM[(zone + 1) % 5];
    }

    uint64_t rng;
    rng_seed(&rng, params->seed);
    uint64_t payload_rng;
    rng_seed(&payload_rng, params->seed ^ 0x3C3C3C3C3C3C3C3Cull);

    const uint64_t target_ticks =
        (uint64_t)((60.0 / rpm) * 1e9 / 125.0);
    /* 2000 ns / 125 ns-per-tick = 16 ticks per bit cell. */
    const uint32_t cell_ticks = UFT_AS_FLUX_GEN_CELL_35_NS / 125u;

    bit_emitter_t e;
    if (!emitter_init(&e, target_ticks, cell_ticks, &rng, params)) {
        return UFT_AS_FLUX_GEN_ERR_NULL;
    }

    for (int i = 0; i < 32; i++) emit_selfsync(&e);

    for (int sec = 0; sec < sectors; sec++) {
        const int drop_addr =
            (params->defects & UFT_AS_DEFECT_MISSING_ADDRESS) && sec == 2;

        /* Address field: D5 AA 96 + trk/sec/side/format/csum as
         * 6-and-2 nibbles (low 6 bits each) + DE AA. */
        if (!drop_addr) {
            emit_nibble(&e, 0xD5); emit_nibble(&e, 0xAA); emit_nibble(&e, 0x96);
            uint8_t trk_low = (uint8_t)(params->track & 0x3F);
            uint8_t sc      = (uint8_t)sec;
            uint8_t sd      = (uint8_t)((params->side << 5) |
                                        ((params->track >> 6) & 0x01));
            uint8_t fmt     = 0x22;   /* standard Mac interleave/format byte */
            emit_nibble(&e, GCR62[trk_low]);
            emit_nibble(&e, GCR62[sc & 0x3F]);
            emit_nibble(&e, GCR62[sd & 0x3F]);
            emit_nibble(&e, GCR62[fmt & 0x3F]);
            emit_nibble(&e, GCR62[(trk_low ^ sc ^ sd ^ fmt) & 0x3F]);
            emit_nibble(&e, 0xDE); emit_nibble(&e, 0xAA);
        }

        for (int i = 0; i < 5; i++) emit_selfsync(&e);

        /* Data field: D5 AA AD + sector nibble + 175 payload nibbles
         * (valid GCR, simplified vs the real 524-byte 3-way 6&2 encode)
         * + DE AA. */
        emit_nibble(&e, 0xD5); emit_nibble(&e, 0xAA); emit_nibble(&e, 0xAD);
        emit_nibble(&e, GCR62[sec & 0x3F]);
        for (int i = 0; i < 175; i++) {
            emit_nibble(&e, GCR62[rng_next(&payload_rng) & 0x3F]);
        }
        emit_nibble(&e, 0xDE); emit_nibble(&e, 0xAA);

        if ((params->defects & UFT_AS_DEFECT_FAKE_BITS) && sec == 1) {
            for (int i = 0; i < 4; i++) emit_bit(&e, 0);
            emit_bit(&e, 1);
        }

        for (int i = 0; i < 8; i++) emit_selfsync(&e);
    }

    while (!e.oos && e.total_ticks + 10u * cell_ticks < target_ticks) {
        emit_selfsync(&e);
    }

    return finish_capture(&e, out, rpm, sectors);
}

/* ─── Free / decode helpers ─────────────────────────────────────────── */

void uft_as_flux_gen_free(uft_as_flux_capture_t *capture)
{
    if (!capture) return;
    free(capture->bytes);
    capture->bytes            = NULL;
    capture->bytes_len        = 0;
    capture->transition_count = 0;
}

size_t uft_as_flux_gen_decode_ticks(const uft_as_flux_capture_t *capture,
                                    uint32_t *out_ticks, size_t out_cap)
{
    if (!capture || !capture->bytes || !out_ticks) return 0;
    size_t n = capture->bytes_len / 4;
    if (n > out_cap) n = out_cap;
    for (size_t i = 0; i < n; i++) {
        const uint8_t *p = capture->bytes + i * 4;
        out_ticks[i] = (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                       ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    }
    return n;
}

size_t uft_as_flux_gen_count_unsafe(const uft_as_flux_capture_t *capture)
{
    if (!capture || !capture->bytes) return 0;
    size_t unsafe = 0;
    size_t n = capture->bytes_len / 4;
    for (size_t i = 0; i < n; i++) {
        const uint8_t *p = capture->bytes + i * 4;
        uint32_t ticks = (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                         ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
        uint64_t ns = (uint64_t)ticks * 1000000000ull /
                      UFT_AS_FLUX_GEN_SAMPLE_HZ;
        if (ns < UFT_AS_FLUX_GEN_MIN_NS || ns > UFT_AS_FLUX_GEN_MAX_NS) {
            unsafe++;
        }
    }
    return unsafe;
}

size_t uft_as_flux_gen_decode_nibbles(const uft_as_flux_capture_t *capture,
                                      uint32_t cell_ns,
                                      uint8_t *out, size_t out_cap)
{
    if (!capture || !capture->bytes || !out || cell_ns == 0) return 0;

    size_t count = 0;
    uint32_t sr = 0;          /* controller shift register */
    size_t n = capture->bytes_len / 4;

    for (size_t i = 0; i < n && count < out_cap; i++) {
        const uint8_t *p = capture->bytes + i * 4;
        uint32_t ticks = (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                         ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
        uint64_t ns = (uint64_t)ticks * 1000000000ull /
                      UFT_AS_FLUX_GEN_SAMPLE_HZ;
        /* Round to nearest whole bit-cell count (>=1). */
        uint32_t cells = (uint32_t)((ns + cell_ns / 2) / cell_ns);
        if (cells == 0) cells = 1;

        /* cells-1 zero bits, then a one bit. The latch fires when the
         * register's bit 7 becomes 1 — a zero bit can complete a
         * nibble too (valid GCR nibbles may end in 0). An empty
         * register (sr == 0) absorbs zero bits without effect, which
         * is exactly how the real controller hunts through self-sync
         * gaps. */
        for (uint32_t z = 1; z < cells && count < out_cap; z++) {
            sr <<= 1;
            if (sr & 0x80u) {
                out[count++] = (uint8_t)(sr & 0xFF);
                sr = 0;
            }
        }
        if (count >= out_cap) break;
        sr = (sr << 1) | 1u;
        if (sr & 0x80u) {
            out[count++] = (uint8_t)(sr & 0xFF);
            sr = 0;
        }
    }
    return count;
}
