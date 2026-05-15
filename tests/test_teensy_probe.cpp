/**
 * @file test_teensy_probe.cpp
 * @brief Unit tests for the ADF-Copy <-> Applesauce probe classifier
 *        (audit ARCH-7-C, MF-213).
 *
 * Covers the PURE decision core of teensy_probe.h —
 * teensy_probe_is_plausible_reply() and classify_teensy_probe(). These
 * are header-inline and Qt-free, so this test links no Qt and joins the
 * header-only C++ test loop.
 *
 * The QSerialPort I/O wrapper probe_teensy_serial() is NOT exercised
 * here — it needs a Qt build with QtSerialPort and a real device. The
 * forensically critical part is the classifier: it must never guess
 * (both / neither plausible -> Unknown), which is exactly what these
 * synthetic vectors pin down.
 *
 * Uses a real CHECK() macro, not assert(): the suite builds with
 * -DNDEBUG, under which assert() is a no-op.
 */
#include "hardware_providers/teensy_probe.h"

#include <cstdio>
#include <string>
#include <string_view>

using ::uft::hal::TeensyDevice;
using ::uft::hal::classify_teensy_probe;
using ::uft::hal::teensy_probe_is_plausible_reply;

static int g_failures = 0;

#define CHECK(cond, msg)                                                   \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("  FAIL: %s  (%s:%d)\n", (msg), __FILE__, __LINE__);     \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

/* ── teensy_probe_is_plausible_reply ──────────────────────────────── */
static void test_plausible_reply(void)
{
    /* Genuine short text replies. */
    CHECK(teensy_probe_is_plausible_reply("v1.0"), "plain ascii text rejected");
    CHECK(teensy_probe_is_plausible_reply("ADF-Copy 1.4\n"),
          "newline-terminated text rejected");
    CHECK(teensy_probe_is_plausible_reply("Applesauce v2.1\r\n"),
          "CRLF-terminated text rejected");
    CHECK(teensy_probe_is_plausible_reply("ok"), "2-byte minimum rejected");

    /* Silence / noise / nonsense. */
    CHECK(!teensy_probe_is_plausible_reply(""), "empty accepted");
    CHECK(!teensy_probe_is_plausible_reply("x"), "1 byte accepted");
    {
        /* a non-printable, non-whitespace byte -> line noise, not a reply */
        const char noise[] = { 'v', '1', '\x01', '\x80', 0 };
        CHECK(!teensy_probe_is_plausible_reply(std::string_view(noise, 4)),
              "binary noise accepted as text");
    }
    {
        /* embedded NUL -> not a clean text line */
        const char withnul[] = { 'v', '\0', '1', 0 };
        CHECK(!teensy_probe_is_plausible_reply(std::string_view(withnul, 3)),
              "embedded NUL accepted");
    }
    {
        /* over-long blob -> not an identify line */
        std::string huge(600, 'A');
        CHECK(!teensy_probe_is_plausible_reply(huge), "600-byte blob accepted");
    }
    printf("  ok: plausible_reply\n");
}

/* ── classify_teensy_probe — the no-guess decision core ───────────── */
static void test_classify(void)
{
    /* Exactly the "?vers" reply plausible -> Applesauce. */
    CHECK(classify_teensy_probe("Applesauce v2.1\r\n", "") ==
              TeensyDevice::Applesauce,
          "vers-only plausible must classify Applesauce");

    /* Exactly the 0x00 (PING) reply plausible -> ADF-Copy. */
    CHECK(classify_teensy_probe("", "ADF-Copy 1.4\n") ==
              TeensyDevice::AdfCopy,
          "ping-only plausible must classify AdfCopy");

    /* Both plausible (a device answered the wrong command too, e.g. an
     * error line) -> Unknown. Never a coin-flip. */
    CHECK(classify_teensy_probe("some text", "other text") ==
              TeensyDevice::Unknown,
          "both plausible must be Unknown, never a guess");

    /* Neither plausible (silence on both) -> Unknown. */
    CHECK(classify_teensy_probe("", "") == TeensyDevice::Unknown,
          "no response on either command must be Unknown");

    /* Noise on one, clean on the other -> the clean one wins. */
    {
        const char noise[] = { '\x00', '\xff', '\x02', 0 };
        CHECK(classify_teensy_probe(std::string_view(noise, 3),
                                    "ADF-Copy v1.4\n") == TeensyDevice::AdfCopy,
              "noise on ?vers + clean ping must classify AdfCopy");
        CHECK(classify_teensy_probe("Applesauce 2.0\n",
                                    std::string_view(noise, 3)) ==
                  TeensyDevice::Applesauce,
              "clean vers + noise on ping must classify Applesauce");
    }

    /* A bare echo / 1-byte twitch is not a reply -> Unknown. */
    CHECK(classify_teensy_probe("?", "") == TeensyDevice::Unknown,
          "1-byte vers echo must not classify");

    printf("  ok: classify\n");
}

int main(void)
{
    printf("test_teensy_probe: ADF-Copy/Applesauce probe classifier "
           "(ARCH-7-C / MF-213)\n");
    test_plausible_reply();
    test_classify();

    if (g_failures != 0) {
        printf("test_teensy_probe: %d CHECK(s) FAILED\n", g_failures);
        return 1;
    }
    printf("test_teensy_probe: all passed\n");
    return 0;
}
