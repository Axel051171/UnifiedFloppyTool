/**
 * @file tests/flux_gen/kryoflux/flux_gen.c
 * @brief Implementation of the synthetic KryoFlux RAW stream generator.
 *
 * See flux_gen.h. The output is designed to be decoded by the production
 * parser uft_kf_decode() — every opcode and OOB block matches that
 * decoder's expectations, so a defect flag here produces a specific
 * uft_kf_status_t there.
 */

#include "flux_gen.h"

#include <stdlib.h>
#include <string.h>

/* ─── deterministic RNG (xorshift64*) ───────────────────────────────── */

static uint64_t rng_next(uint64_t *s) {
    uint64_t x = *s;
    x ^= x >> 12; x ^= x << 25; x ^= x >> 27;
    *s = x;
    return x * 0x2545F4914F6CDD1DULL;
}

/* ─── opcode constants (mirror the decoder) ─────────────────────────── */
#define OP_OVL16   0x0Bu
#define OP_FLUX3   0x0Cu
#define OP_OOB     0x0Du
#define OOB_STREAM_INFO  0x01u
#define OOB_INDEX        0x02u
#define OOB_STREAM_END   0x03u
#define OOB_EOF          0x0Du
#define KF_RESULT_OK        0x00u
#define KF_RESULT_BUFFERING 0x01u
#define KF_RESULT_NO_INDEX  0x02u

/* ─── little-endian writers ─────────────────────────────────────────── */

static void put_u16(uint8_t *b, uint16_t v) {
    b[0] = (uint8_t)(v & 0xFF);
    b[1] = (uint8_t)((v >> 8) & 0xFF);
}
static void put_u32(uint8_t *b, uint32_t v) {
    b[0] = (uint8_t)(v & 0xFF);
    b[1] = (uint8_t)((v >> 8) & 0xFF);
    b[2] = (uint8_t)((v >> 16) & 0xFF);
    b[3] = (uint8_t)((v >> 24) & 0xFF);
}

/* ─── flux value → opcode stream ────────────────────────────────────── */

size_t uft_kf_gen_encode_flux(uint8_t *buf, size_t cap, size_t *pos,
                              uint32_t value) {
    size_t start = *pos;
    /* Emit as many Ovl16 as needed to bring the residual below 0x10000.
     * The parser accumulates these and adds to the NEXT flux value. */
    while (value >= 0x10000u) {
        if (*pos + 1 > cap) return 0;
        buf[(*pos)++] = OP_OVL16;
        value -= 0x10000u;
    }
    if (value >= 0x0Eu && value <= 0xFFu) {
        if (*pos + 1 > cap) return 0;
        buf[(*pos)++] = (uint8_t)value;              /* Flux1 */
    } else if (value <= 0x07FFu) {
        if (*pos + 2 > cap) return 0;
        buf[(*pos)++] = (uint8_t)((value >> 8) & 0x07u);  /* Flux2 hi (0..7) */
        buf[(*pos)++] = (uint8_t)(value & 0xFF);
    } else {
        if (*pos + 3 > cap) return 0;
        buf[(*pos)++] = OP_FLUX3;
        buf[(*pos)++] = (uint8_t)((value >> 8) & 0xFF);
        buf[(*pos)++] = (uint8_t)(value & 0xFF);
    }
    return *pos - start;
}

/* Emit an OOB block: 0x0D, type, LE16 size, payload. */
static bool emit_oob(uint8_t *buf, size_t cap, size_t *pos,
                     uint8_t type, const uint8_t *payload, uint16_t size) {
    if (*pos + 4u + size > cap) return false;
    buf[(*pos)++] = OP_OOB;
    buf[(*pos)++] = type;
    put_u16(&buf[*pos], size); *pos += 2;
    if (size && payload) { memcpy(&buf[*pos], payload, size); *pos += size; }
    return true;
}

/* ─── ns <-> ticks ──────────────────────────────────────────────────── */

static uint32_t ns_to_ticks(uint32_t ns) {
    /* ticks = ns * 24.027428e6 / 1e9 */
    return (uint32_t)((double)ns * UFT_KF_GEN_SAMPLE_HZ / 1e9 + 0.5);
}

/* ─── main generator ────────────────────────────────────────────────── */

uft_kf_gen_err_t uft_kf_gen_stream(const uft_kf_gen_params_t *params,
                                   uft_kf_gen_stream_t *out) {
    if (!params || !out) return UFT_KF_GEN_ERR_NULL;
    memset(out, 0, sizeof(*out));

    if (params->track < 0 || params->track > UFT_KF_GEN_MAX_TRACK ||
        params->side < 0 || params->side > 1 ||
        params->revolutions < UFT_KF_GEN_MIN_REVS ||
        params->revolutions > UFT_KF_GEN_MAX_REVS)
        return UFT_KF_GEN_ERR_OUT_OF_SPEC;

    uint32_t cell_ns = params->cell_ns ? params->cell_ns : UFT_KF_GEN_CELL_DD_NS;
    if (cell_ns * 1u < UFT_KF_GEN_MIN_NS || cell_ns > UFT_KF_GEN_MAX_NS)
        return UFT_KF_GEN_ERR_OUT_OF_SPEC;

    uint8_t jitter = 0;
    if (params->defects & UFT_KF_DEFECT_WEAK_BITS) {
        jitter = params->weak_jitter_pct;
        if (jitter < 1 || jitter > UFT_KF_GEN_MAX_JITTER)
            return UFT_KF_GEN_ERR_OUT_OF_SPEC;
    }

    /* One 300-RPM revolution = 200 ms. Cells are emitted until the
     * accumulated tick budget reaches one revolution, so the decoded
     * index-to-index time is ~200 ms regardless of the 4/6/8 µs cell
     * mix — that keeps the derived RPM at ~300. */
    const uint32_t rev_ns    = 200000000u;             /* 200 ms */
    const uint32_t rev_ticks = ns_to_ticks(rev_ns);
    /* Worst case: every cell the shortest (4 µs ≈ 96 ticks), 1 byte each,
     * plus OOB blocks. Budget generously. */
    size_t max_cells = (size_t)(rev_ticks / ns_to_ticks(cell_ns)) + 16u;
    size_t cap = max_cells * (size_t)params->revolutions * 3u + 4096u;
    uint8_t *buf = (uint8_t *)malloc(cap);
    if (!buf) return UFT_KF_GEN_ERR_NOMEM;
    size_t pos = 0;

    /* MF-867: die STREAM-POSITION ist nicht die Pufferposition.
     *
     * Der KryoFlux-Strom zaehlt nur Zellenstrom-Bytes; OOB-Bloecke
     * zaehlen NICHT mit (der Baum hat dafuer einen eigenen Testfall:
     * "OOB bytes do NOT advance stream pos"). Dieser Erzeuger schrieb
     * ueberall `pos` — die rohe Pufferposition, in der jeder Index- und
     * StreamInfo-Block mitzaehlt. An einer Stelle stand sogar
     * ausdruecklich "our stream_pos proxy".
     *
     * Aufgefallen ist es, als MF-867 den Positionsabgleich im Leser
     * einbaute: der Emulator erzeugte prompt genau die Fehlerklasse, die
     * der Abgleich fangen soll — "OOB-Bytes wurden faelschlich in die
     * Position eingerechnet". Das ist der Fall, fuer den er gebaut ist,
     * und der Prueftisch selbst hat ihn geliefert.
     *
     * Es ist derselbe Befund wie MF-825 ein paar Zeilen weiter unten:
     * ein Emulator, der die Hardware falsch nachbildet, kann die
     * Fehlerklasse nicht fangen, fuer die er gebaut wurde. */
    size_t zell_pos = 0;

    uint64_t rng = params->seed ? params->seed : 0x1234567890ABCDEFULL;

    /* StreamInfo header (position 0, transfer time 0 — placeholder). */
    uint8_t si[8]; put_u32(&si[0], 0); put_u32(&si[4], 0);
    emit_oob(buf, cap, &pos, OOB_STREAM_INFO, si, sizeof(si));

    uint32_t flux_encoded = 0;
    uint32_t idx_emitted  = 0;
    uint32_t sample_ctr   = 0;   /* running KF-tick counter for Index blocks */
    bool want_index = !(params->defects & UFT_KF_DEFECT_NO_INDEX_BLK);

    int long_cell_rev = -1;
    if (params->defects & UFT_KF_DEFECT_LONG_CELL)
        long_cell_rev = 0;   /* inject one damaged gap on the first rev */

    for (int rev = 0; rev < params->revolutions; rev++) {
        /* Index OOB at the START of each revolution: stream_pos = current
         * cell-stream byte position, sample_counter = running ticks. */
        if (want_index) {
            /* MF-825: die beiden Zaehler standen VERTAUSCHT.
             *
             * Hier stand `sample_counter = sample_ctr` (ein laufender
             * Tickzaehler) und `index_counter = rev` (die blosse
             * Umdrehungsnummer). Die Spezifikation sagt das Gegenteil:
             *
             *   Sample Counter — „This counter is reset after each Flux
             *     reversal recording."  Also ein Versatz INNERHALB einer
             *     Zelle, klein (bei DD-MFM 96..192 Ticks).
             *   Index Counter  — „a free running counter (not reset)",
             *     getaktet mit ick = sck/8.
             *
             * Der Emulator bildete also einen freilaufenden Zaehler im
             * falschen Feld ab — und der Parser rechnete die
             * Umdrehungszeit aus genau diesem falschen Feld. Beide
             * Fehler hoben sich auf, und `decoded_rpm_near_300` war
             * gruen, WEIL beide da waren. Erst der berichtigte Parser
             * (MF-825) hat den Prueftisch freigelegt.
             *
             * Ein Emulator, der die Hardware falsch nachbildet, kann die
             * Fehlerklasse nicht fangen, fuer die er gebaut wurde. */
            uint8_t ix[12];
            put_u32(&ix[0], (uint32_t)zell_pos);   /* MF-867: echt, kein Proxy */
            /* Versatz in der Zelle: eine plausible, feste Groesse. Der
             * genaue Wert ist fuer die Umdrehungszeit ohne Belang — das
             * ist ja der Punkt. */
            put_u32(&ix[4], 144u);
            /* Freilaufender Index Counter in ick-Ticks (ick = sck/8). */
            put_u32(&ix[8], sample_ctr / 8u);
            if (emit_oob(buf, cap, &pos, OOB_INDEX, ix, sizeof(ix)))
                idx_emitted++;
        }

        uint32_t rev_accum = 0;      /* ticks emitted this revolution */
        uint32_t c = 0;
        bool injected_long = false;
        while (rev_accum < rev_ticks) {
            /* Cell family: mostly nominal, some 1.5x/2x (MFM 01/000). */
            uint32_t mult_num = 1, mult_den = 1;
            uint64_t r = rng_next(&rng) % 100;
            if (r < 50)      { mult_num = 1; mult_den = 1; }   /* 4 µs */
            else if (r < 80) { mult_num = 3; mult_den = 2; }   /* 6 µs */
            else             { mult_num = 2; mult_den = 1; }   /* 8 µs */
            uint32_t this_ns = cell_ns * mult_num / mult_den;

            if (jitter) {
                /* Symmetric jitter within ±jitter%. */
                int32_t span = (int32_t)(this_ns * jitter / 100);
                int32_t d = (int32_t)(rng_next(&rng) % (uint32_t)(2 * span + 1)) - span;
                this_ns = (uint32_t)((int32_t)this_ns + d);
            }
            if (long_cell_rev == rev && !injected_long &&
                rev_accum >= rev_ticks / 2) {
                this_ns = 150000u;   /* 150 µs damaged gap (still < MAX) */
                injected_long = true;
            }

            uint32_t ticks = ns_to_ticks(this_ns);
            if (ticks == 0) ticks = 1;
            if (pos + 6 > cap) { free(buf); return UFT_KF_GEN_ERR_NOMEM; }
            size_t geschrieben = uft_kf_gen_encode_flux(buf, cap, &pos,
                                                        ticks);
            if (geschrieben == 0) {
                free(buf); return UFT_KF_GEN_ERR_NOMEM;
            }
            zell_pos += geschrieben;      /* MF-867: nur Zellenstrom */
            flux_encoded++;
            sample_ctr += ticks;
            rev_accum  += ticks;
            c++;
        }
        (void)c;
    }

    /* Truncation defect: cut the buffer mid-stream BEFORE the terminators
     * so the parser hits MISSING_DATA / MISSING_END. */
    if (params->defects & UFT_KF_DEFECT_TRUNCATED) {
        /* Chop the last 1 byte off a Flux2/Flux3 tail if possible, else
         * just drop the terminators. Simplest deterministic cut: remove
         * the final byte, guaranteeing an incomplete trailing opcode OR
         * (if it was Flux1) simply no StreamEnd. Either way != OK. */
        if (pos > 1) pos -= 1;
        out->bytes = buf;
        out->bytes_len = pos;
        out->flux_count = flux_encoded;
        out->index_count = idx_emitted;
        out->revolution_ticks = rev_ticks;
        out->rpm = 300.0;
        return UFT_KF_GEN_OK;
    }

    /* StreamEnd with a result code (defect-driven), then EOF. */
    if (!(params->defects & UFT_KF_DEFECT_MISSING_END)) {
        uint32_t result = KF_RESULT_OK;
        if (params->defects & UFT_KF_DEFECT_DEV_NO_INDEX) result = KF_RESULT_NO_INDEX;
        else if (params->defects & UFT_KF_DEFECT_DEV_BUFFER) result = KF_RESULT_BUFFERING;
        uint8_t se[8];
        put_u32(&se[0], (uint32_t)zell_pos);   /* MF-867 */
        put_u32(&se[4], result);
        emit_oob(buf, cap, &pos, OOB_STREAM_END, se, sizeof(se));
        /* EOF marker: type 0x0D, sentinel size 0x0D0D, no payload. */
        if (pos + 4 <= cap) {
            buf[pos++] = OP_OOB; buf[pos++] = OOB_EOF;
            put_u16(&buf[pos], 0x0D0Du); pos += 2;
        }
    }

    out->bytes = buf;
    out->bytes_len = pos;
    out->flux_count = flux_encoded;
    out->index_count = idx_emitted;
    out->revolution_ticks = rev_ticks;
    out->rpm = 300.0;
    return UFT_KF_GEN_OK;
}

void uft_kf_gen_free(uft_kf_gen_stream_t *s) {
    if (!s) return;
    free(s->bytes);
    s->bytes = NULL;
    s->bytes_len = 0;
}

/* ─── self-decode safety scan ───────────────────────────────────────── */

size_t uft_kf_gen_count_unsafe(const uft_kf_gen_stream_t *s) {
    if (!s || !s->bytes) return 0;
    size_t unsafe = 0;
    const uint8_t *d = s->bytes;
    size_t len = s->bytes_len, i = 0;
    uint32_t overflow = 0;
    const double min_t = (double)UFT_KF_GEN_MIN_NS * UFT_KF_GEN_SAMPLE_HZ / 1e9;
    const double max_t = (double)UFT_KF_GEN_MAX_NS * UFT_KF_GEN_SAMPLE_HZ / 1e9;

    while (i < len) {
        uint8_t op = d[i];
        uint32_t v = 0; bool have = false;
        if (op <= 0x07u) {                      /* Flux2 */
            if (i + 1 >= len) break;
            v = ((uint32_t)op << 8) | d[i + 1]; have = true; i += 2;
        } else if (op == 0x08u) { i += 1;
        } else if (op == 0x09u) { i += 2;
        } else if (op == 0x0Au) { i += 3;
        } else if (op == OP_OVL16) { overflow += 0x10000u; i += 1;
        } else if (op == OP_FLUX3) {
            if (i + 2 >= len) break;
            v = ((uint32_t)d[i + 1] << 8) | d[i + 2]; have = true; i += 3;
        } else if (op == OP_OOB) {
            if (i + 4 > len) break;
            if (d[i + 1] == OOB_EOF) { i += 4; break; }
            uint16_t sz = (uint16_t)(d[i + 2] | (d[i + 3] << 8));
            i += 4u + sz;
        } else {                                /* Flux1 */
            v = op; have = true; i += 1;
        }
        if (have) {
            double t = (double)(v + overflow);
            overflow = 0;
            if (t < min_t || t > max_t) unsafe++;
        }
    }
    return unsafe;
}
