/**
 * @file test_gw_rpm_indexzeiten.cpp
 * @brief #43 (MF-XXXX): GreaseweazleProviderV2::measure_rpm() computes the
 *        speed from the index pulses of the flux it just read — and the
 *        provider really addresses the bus unit it was given.
 *
 * ── The defect (D2 of issue #43) ────────────────────────────────────────
 *
 * do_measure_rpm() read a flux capture and then asked the device for the
 * index times with CMD_GET_INDEX_TIMES (0x0A). That command left the
 * Greaseweazle firmware in v0.22 (measured: keirf/greaseweazle v0.21
 * inc/cdc_acm_protocol.h defines `CMD_GET_INDEX_TIMES 10`, v0.22 and v0.31
 * jump from 9 to 11), and UFT refuses firmware older than v0.31. So on
 * EVERY firmware UFT accepts, the answer was an error, the index list came
 * back empty and the tab said
 *
 *     Error: RPM measurement produced no index intervals
 *
 * for drive 0 and drive 1 alike — the reporter's screenshot.
 *
 * The index times were there all along: uft_gw_read_flux() decodes them
 * from the flux stream into `flux->index_times[]` (durations since the
 * previous index; entry 0 is the sync zero or the partial revolution from
 * capture start to the first pulse, see MF-957 in the provider).
 *
 * ── How the promises reach it without hardware ───────────────────────────
 *
 * The provider drives the Greaseweazle firmware automaton through the wire
 * bridge (MF-848) over the production byte seam (uft_gw_open_stream, which
 * since P3-551 runs the real GET_INFO handshake). The bridge counts every
 * command it does not know (`unbekannt`) — CMD 0x0A is one of them, exactly
 * as for the real firmware.
 *
 * The streams are built here by hand from the documented flux opcodes
 * (0xFF 0x01 = index, 0xFF 0x02 = space, N28 values). What the promises
 * check is not the byte layout but the ARITHMETIC over the durations the
 * production decoder extracts from them.
 *
 * ── The realistic case (promise 4) ──────────────────────────────────────
 *
 * UFT bounds the capture to 1.2 s (ticks = revs x sample_freq/5 x 2 in
 * uft_gw_read_flux()). At the reporter's 150 rpm one revolution lasts
 * 400 ms: a real drive delivers a PARTIAL revolution (capture start to the
 * first pulse) and then only two full ones before the window closes —
 * three index entries, not four. A computation that averages over all
 * entries, or that insists on revs+1 entries, passes the idealised stream
 * and fails at the reporter's desk. Promise 4 is that desk.
 *
 * ── Added after review (#43, second round) ──────────────────────────────
 *
 *   * Promise 7 runs the automaton at 84 MHz (F7 Plus): a formula with a
 *     hard-wired 72 MHz passed promises 1-6 and reported 257.14 rpm here.
 *   * Promises 8/9 pin jitter_pct, which nothing asserted before.
 *   * Promise 10: one index pulse proves the disk turns, so its refusal
 *     must not open with "Motor on?" as the zero-pulse refusal does.
 *
 * ── Emulator deviations this test works around (DIVERGENCES.md) ─────────
 *
 *   * The automaton ignores the `ticks` bound of READ_FLUX
 *     (firmware_state_machine.c, gw_fw_cmd_read_flux: `(void)ticks`), so
 *     the stream alone decides how many index entries arrive.
 *   * It refuses READ_FLUX without a running motor (BAD_COMMAND) and
 *     answers NO_INDEX at once when no pulses "fire". Real firmware accepts
 *     READ_FLUX with a stopped motor and ends the 1.2 s window with ACK_OK
 *     and NO index entry. The test therefore switches the motor on and
 *     models "motor off" as a stream without index opcodes (promise 6).
 */

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <variant>
#include <vector>

extern "C" {
#include "emulators/greaseweazle/gw_wire_bridge.h"
}
#include "hardware_providers/greaseweazle_provider_v2.h"

using namespace uft::hal;

static int g_fehl = 0;
static int g_zusagen = 0;

#define ZUSAGE(nr, bedingung, text)                                        \
    do {                                                                   \
        const bool _ok = (bedingung);                                      \
        ++g_zusagen;                                                       \
        std::printf("  [%s] Zusage %s: %s\n", _ok ? "GRUEN" : "ROT", (nr), \
                    (text));                                               \
        if (!_ok) ++g_fehl;                                                \
    } while (0)

/* ── Stream construction (Greaseweazle flux opcodes) ─────────────────── */

static void n28(std::vector<uint8_t> &v, uint32_t x)
{
    v.push_back(static_cast<uint8_t>(((x      ) & 0x7Fu) << 1 | 1u));
    v.push_back(static_cast<uint8_t>(((x >>  7) & 0x7Fu) << 1 | 1u));
    v.push_back(static_cast<uint8_t>(((x >> 14) & 0x7Fu) << 1 | 1u));
    v.push_back(static_cast<uint8_t>(((x >> 21) & 0x7Fu) << 1 | 1u));
}

static void index_puls(std::vector<uint8_t> &s)
{
    s.push_back(0xFF); s.push_back(0x01); n28(s, 0);
}

/* One stretch of `ticks` ending in a flux transition, then an index. */
static void umdrehung(std::vector<uint8_t> &s, uint32_t ticks)
{
    s.push_back(0xFF); s.push_back(0x02); n28(s, ticks - 100u); /* space */
    s.push_back(100);                                           /* flux  */
    index_puls(s);
}

/* Idealised stream as the automaton's own tests build it: an index at the
 * very start (sync zero), then `n` full revolutions. */
static std::vector<uint8_t> strom_sync(uint32_t t, int n)
{
    std::vector<uint8_t> s;
    index_puls(s);
    for (int i = 0; i < n; ++i) umdrehung(s, t);
    s.push_back(0x00);
    return s;
}

/* Real-drive stream: capture starts mid-revolution, the first index comes
 * after `teil` ticks, then `n` full revolutions of `t`. */
static std::vector<uint8_t> strom_teil(uint32_t teil, uint32_t t, int n)
{
    std::vector<uint8_t> s;
    umdrehung(s, teil);
    for (int i = 0; i < n; ++i) umdrehung(s, t);
    s.push_back(0x00);
    return s;
}

/* Real-drive stream with full revolutions of DIFFERENT lengths: partial
 * revolution `teil`, then one revolution per entry of `umlaeufe`. */
static std::vector<uint8_t> strom_folge(uint32_t teil,
                                        const std::vector<uint32_t> &umlaeufe)
{
    std::vector<uint8_t> s;
    umdrehung(s, teil);
    for (const uint32_t t : umlaeufe) umdrehung(s, t);
    s.push_back(0x00);
    return s;
}

/* Flux without a single index pulse — what a stopped motor produces. */
static std::vector<uint8_t> strom_ohne_index()
{
    std::vector<uint8_t> s;
    s.push_back(0xFF); s.push_back(0x02); n28(s, 5000000u);
    s.push_back(100);
    s.push_back(0x00);
    return s;
}

/* ── Rig ──────────────────────────────────────────────────────────────── */

struct Stand {
    gw_fw_t          fw;
    gw_wire_t        w;
    uft_gw_device_t *dev = nullptr;
};

/* `takt` is the sample clock the automaton reports in GET_INFO: 72 MHz
 * (F7, the default) or 84 MHz (F7 Plus). The HAL copies it into every
 * flux capture (flux->sample_freq), and the provider must compute with it. */
static bool stand_auf(Stand &s, uint32_t takt = 72000000u)
{
    std::memset(&s.fw, 0, sizeof s.fw);
    gw_fw_reset(&s.fw);
    gw_fw_power_on_defaults(&s.fw);
    gw_fw_set_firmware_version(&s.fw, 1, 23);
    gw_fw_set_sample_freq(&s.fw, takt);
    gw_wire_init(&s.w, &s.fw);
    return uft_gw_open_stream(&s.w.ops, &s.dev) == UFT_GW_OK && s.dev;
}

struct Messung {
    bool        gemessen   = false;
    double      rpm        = 0.0;
    double      jitter     = -1.0;
    int         umdrehungen = 0;
    bool        fehler     = false;
    std::string text;              /* what + why + fix of a ProviderError */
    std::string why;               /* the why part alone                  */
    std::string fix;               /* the fix part alone                  */
    unsigned    unbekannt  = 0;    /* commands the bridge did not know    */
    uint8_t     unit_fw    = 0xFF; /* unit the firmware had selected      */
    bool        stand_ok   = false;
};

static Messung miss(int unit, const std::vector<uint8_t> &strom,
                    uint32_t takt = 72000000u)
{
    Messung m;
    Stand s;
    if (!stand_auf(s, takt)) return m;
    m.stand_ok = true;
    gw_fw_load_read_stream(&s.fw, strom.data(), strom.size());
    {
        GreaseweazleProviderV2 p(s.dev, unit);   /* takes ownership */
        (void)p.set_motor(true);
        const unsigned vorher = s.w.unbekannt;
        const RpmOutcome r = p.measure_rpm();
        m.unbekannt = s.w.unbekannt - vorher;
        m.unit_fw   = s.fw.selected_unit;
        if (const auto *ok = std::get_if<RpmMeasured>(&r)) {
            m.gemessen    = true;
            m.rpm         = ok->rpm;
            m.jitter      = ok->jitter_pct;
            m.umdrehungen = ok->revolutions_sampled;
        } else if (const auto *e = std::get_if<ProviderError>(&r)) {
            m.fehler = true;
            m.text   = e->what + " | " + e->why + " | " + e->fix;
            m.why    = e->why;
            m.fix    = e->fix;
        }
    }   /* provider destructor closes the device */
    std::printf("    unit=%d clock=%u: %s rpm=%.3f jitter=%.4f%% revs=%d "
                "unknown_cmds=%u fw_unit=%d%s%s\n",
                unit, takt,
                m.gemessen ? "RpmMeasured" : (m.fehler ? "ProviderError" : "other"),
                m.rpm, m.jitter, m.umdrehungen, m.unbekannt,
                static_cast<int>(m.unit_fw),
                m.text.empty() ? "" : " text=", m.text.c_str());
    return m;
}

static bool nennt_motor(const std::string &t)
{
    return t.find("motor") != std::string::npos ||
           t.find("Motor") != std::string::npos;
}

static bool beginnt_mit(const std::string &t, const char *anfang)
{
    return t.rfind(anfang, 0) == 0;
}

int main()
{
    std::puts("test_gw_rpm_indexzeiten (#43): RPM from the flux index "
              "times, unit really addressed");

    /* 72 MHz (the automaton's default): 14 400 000 ticks = 200 ms = 300 rpm,
     * 28 800 000 ticks = 400 ms = 150 rpm. */
    const uint32_t T300 = 14400000u;
    const uint32_t T150 = 28800000u;

    /* ── Guards: the provider addresses the unit it is given (D1, provider
     *    half). These held before the fix; they pin what the GUI fix
     *    relies on. */
    {
        Stand s;
        const bool ok = stand_auf(s);
        ZUSAGE("0", ok, "rig: uft_gw_open_stream() + handshake against the automaton");
        if (!ok) { std::puts("Pruefstand kaputt"); return 1; }
        GreaseweazleProviderV2 p(s.dev, 1);
        (void)p.set_motor(true);
        ZUSAGE("W1", s.fw.selected_unit == 1 && s.fw.bus_type == GW_FW_BUS_IBM_PC,
               "unit 1 -> firmware selects unit 1 on the IBM-PC bus");
        p.set_drive_unit(0);
        (void)p.set_motor(true);
        ZUSAGE("W2", s.fw.selected_unit == 0,
               "set_drive_unit(0) after an operation re-selects on the next one");

        /* #43 follow-up (petrkr, MF-1376): Shugart drives 0-3 and IBM-PC
         * A/B are two BUSES. Reference: gw tools/util.py:127-141 —
         * 'A'/'B' -> (IBMPC, 0/1), '0'..'3' -> (Shugart, 0..3). Before the
         * fix the provider never set a bus: the HAL chose IBM-PC, and
         * Shugart unit 2 was unreachable. */
        p.set_drive(GwBus::Shugart, 2);
        (void)p.set_motor(true);
        ZUSAGE("B1", s.fw.bus_type == GW_FW_BUS_SHUGART && s.fw.selected_unit == 2,
               "Shugart drive 2 -> firmware on the Shugart bus, unit 2");
        p.set_drive(GwBus::IbmPc, 1);
        (void)p.set_motor(true);
        ZUSAGE("B2", s.fw.bus_type == GW_FW_BUS_IBM_PC && s.fw.selected_unit == 1,
               "back to B: -> the bus is switched back to IBM-PC, unit 1");
        p.set_drive(GwBus::IbmPc, 2);
        const MotorOutcome m = p.set_motor(true);
        ZUSAGE("B3", std::holds_alternative<ProviderError>(m) &&
                     s.fw.bus_type == GW_FW_BUS_IBM_PC && s.fw.selected_unit == 1,
               "IBM-PC unit 2 does not exist: refused visibly, firmware untouched");
    }

    /* ── D2: the speed itself. */
    const Messung a = miss(1, strom_sync(T300, 3));
    ZUSAGE("1", a.stand_ok && a.gemessen && std::fabs(a.rpm - 300.0) < 0.01,
           "300 rpm stream on unit 1 -> RpmMeasured 300.00");
    ZUSAGE("2", a.stand_ok && a.unbekannt == 0,
           "no command the firmware does not know (no CMD 0x0A)");

    const Messung b = miss(1, strom_sync(T150, 3));
    ZUSAGE("3", b.stand_ok && b.gemessen && std::fabs(b.rpm - 150.0) < 0.01,
           "150 rpm stream on unit 1 -> RpmMeasured 150.00");

    /* The reporter's drive: partial revolution (5 000 000 ticks, ~69 ms)
     * then two full 150-rpm revolutions. A mean over all three entries
     * would be (5 000 000 + 2 x 28 800 000) / 3 = 20 866 667 ticks, i.e.
     * 207.0 rpm — this promise refuses exactly that. (In general such a
     * mean lands between 150 and 225 rpm, depending on where in the
     * revolution the capture starts.) */
    const Messung c = miss(1, strom_teil(5000000u, T150, 2));
    ZUSAGE("4", c.stand_ok && c.gemessen && std::fabs(c.rpm - 150.0) < 0.01,
           "partial + 2 full revolutions (1.2 s window at 150 rpm) -> 150.00");
    ZUSAGE("5", c.stand_ok && c.gemessen && c.umdrehungen == 2,
           "the partial revolution is not counted as a sampled revolution");

    /* Stopped motor on real hardware: ACK_OK, flux, zero index entries.
     * The FIX line must name the motor — it is what the operator acts on. */
    const Messung d = miss(1, strom_ohne_index());
    ZUSAGE("6", d.stand_ok && d.fehler && nennt_motor(d.fix) && d.unbekannt == 0,
           "no index pulse -> ProviderError whose fix points at the motor");

    /* The clock comes from the capture, not from a constant. An F7 Plus
     * samples at 84 MHz: 16 800 000 ticks are 200 ms = 300 rpm there,
     * while a hard-wired 72 MHz would report 257.14 rpm (P3-551). */
    const Messung f = miss(1, strom_sync(16800000u, 3), 84000000u);
    ZUSAGE("7", f.stand_ok && f.gemessen && std::fabs(f.rpm - 300.0) < 0.01,
           "84 MHz device, 16 800 000-tick revolutions -> 300.00, not 257.14");

    /* Jitter is (longest - shortest) / mean over the FULL revolutions:
     * 14 400 000 and 14 544 000 ticks (+1 %) give a mean of 14 472 000,
     * 298.507 rpm and 144 000 / 14 472 000 = 0.995 %. The partial
     * revolution in front must not enter it (with it: 84.35 %). Equal
     * revolutions give exactly 0 (stream of promise 1). */
    const Messung j = miss(1, strom_folge(5000000u, {T300, 14544000u}));
    ZUSAGE("8", j.stand_ok && j.gemessen &&
                std::fabs(j.rpm - 298.507) < 0.01 &&
                std::fabs(j.jitter - 0.995) < 0.001,
           "two unequal revolutions -> 298.507 rpm, jitter 0.995 %");
    ZUSAGE("9", a.stand_ok && a.gemessen && a.jitter == 0.0,
           "three equal revolutions -> jitter exactly 0");

    /* One index entry is the partial revolution only — not a speed. A
     * guard: the old code refused it too (for the wrong reason), the new
     * code must keep refusing it instead of reporting 864 rpm from it. */
    const Messung e = miss(1, strom_teil(5000000u, T150, 0));
    ZUSAGE("W3", e.stand_ok && e.fehler && !e.gemessen,
           "a single index entry (partial revolution only) is refused, not averaged");
    /* ...and with its own reason: one pulse proves the disk turns, so the
     * operator must not be told "Motor on?" (#43 review). */
    ZUSAGE("10", e.stand_ok && e.fehler &&
                 e.why.find("exactly one index pulse") != std::string::npos &&
                 !beginnt_mit(e.fix, "Motor on?"),
           "one pulse -> the refusal says so and does not blame the motor");

    /* Two index pulses with no time between them: durations 0, T, 0. A
     * mean over the full revolutions without the zero check would be T/2,
     * i.e. 300 rpm reported for a 150-rpm drive. */
    {
        std::vector<uint8_t> s;
        index_puls(s);
        umdrehung(s, T150);
        index_puls(s);
        s.push_back(0x00);
        const Messung z = miss(1, s);
        ZUSAGE("W4", z.stand_ok && z.fehler && !z.gemessen,
               "a zero-length revolution is refused instead of doubling the speed");
    }

    if (g_fehl) {
        std::printf("FAILED: %d of %d promises red\n", g_fehl, g_zusagen);
        return 1;
    }
    std::printf("OK: %d of %d promises green\n", g_zusagen, g_zusagen);
    return 0;
}
