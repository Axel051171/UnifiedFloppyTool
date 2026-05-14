#pragma once
/**
 * @file teensy_probe.h
 * @brief ADF-Copy <-> Applesauce disambiguation probe (audit ARCH-7-C).
 *
 * ADF-Copy and Applesauce are both Teensy-based USB-CDC controllers and
 * BOTH ship the stock, unmodified PJRC Teensy USB identity 0x16C0:0x0483
 * with the stock string descriptors (verified by lsusb / Device Manager
 * readout — see audit/ARCH-7_VID_PID.md sub-finding C). They are
 * therefore *genuinely indistinguishable* at the USB layer — VID/PID and
 * string descriptors alike. The only authoritative signal is a protocol
 * probe: the two devices have distinct, non-overlapping identify
 * handshakes.
 *
 *   ADF-Copy    : ADFC_CMD_PING = byte 0x00  -> a text version line ending '\n'
 *                 (adfcopy_provider_v2.h:128)
 *   Applesauce  : the ASCII command "?vers"  -> a firmware version string
 *                 (applesauce_provider_v2.h:299)
 *
 * An ADF-Copy does not understand "?vers"; an Applesauce does not treat a
 * bare 0x00 as a version query. Probing both and seeing which device
 * answers its OWN command with plausible text disambiguates them even
 * with byte-identical USB descriptors.
 *
 * Forensic contract (proposal docs/proposals/ARCH7C_TEENSY_ID_DISAMBIGUATION.md):
 *   - Non-destructive: only the two pure identify queries are ever sent;
 *     never a motor / seek / flux / write command.
 *   - Never guess: if BOTH or NEITHER response is plausible, the result is
 *     Unknown. Picking wrong would send ADF-Copy bytes to an Applesauce
 *     (or vice versa) — exactly the "stille Veränderung" the project
 *     forbids.
 *
 * Split design: `classify_teensy_probe()` is the PURE decision core
 * (no Qt — header-inline, exhaustively unit-tested by
 * tests/test_teensy_probe.cpp). `probe_teensy_serial()` is the thin
 * QSerialPort I/O wrapper that runs the handshake and calls the
 * classifier; its real body needs a Qt build WITH QtSerialPort.
 */
#include <cstddef>
#include <string_view>

/* Forward-declare only — the QString parameter of probe_teensy_serial()
 * must not drag a Qt include into the pure classifier's translation
 * units (the unit test includes this header and links no Qt). */
class QString;

namespace uft::hal {

/** Probe outcome. Unknown also covers "port could not be opened" and
 *  "response ambiguous" — it is never a fallback guess. */
enum class TeensyDevice {
    Unknown = 0,
    AdfCopy,
    Applesauce,
};

/**
 * @brief Is @p resp a plausible text identify-reply (not noise / silence)?
 *
 * A genuine reply is a short run of printable ASCII (plus the usual
 * CR/LF/TAB whitespace). Empty input, a single stray byte, an
 * over-long blob, or anything containing a non-printable, non-whitespace
 * byte is rejected — the device either did not answer this command or
 * answered with line noise. Strict on purpose: a false "plausible" is a
 * mis-identification, a false "implausible" only costs an Unknown.
 */
inline bool teensy_probe_is_plausible_reply(std::string_view resp) noexcept
{
    if (resp.size() < 2 || resp.size() > 512) {
        return false;
    }
    for (unsigned char c : resp) {
        if (c == '\r' || c == '\n' || c == '\t') {
            continue;
        }
        if (c < 0x20 || c > 0x7E) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Classify a Teensy serial device from its two identify responses.
 *
 * @param vers_response  bytes received after sending "?vers"
 * @param ping_response  bytes received after sending 0x00 (ADFC_CMD_PING)
 *
 * Pure, deterministic, no I/O. Conservative:
 *   - exactly the "?vers" reply plausible  -> Applesauce
 *   - exactly the 0x00 reply plausible     -> AdfCopy
 *   - both plausible (a device answered the wrong command too, e.g. with
 *     an error line) OR neither plausible  -> Unknown
 *
 * "Both plausible -> Unknown" is deliberate: when the signal is
 * contradictory the honest answer is "cannot tell", never a coin-flip.
 */
inline TeensyDevice classify_teensy_probe(std::string_view vers_response,
                                          std::string_view ping_response) noexcept
{
    const bool vers_ok = teensy_probe_is_plausible_reply(vers_response);
    const bool ping_ok = teensy_probe_is_plausible_reply(ping_response);

    if (vers_ok && !ping_ok) {
        return TeensyDevice::Applesauce;
    }
    if (ping_ok && !vers_ok) {
        return TeensyDevice::AdfCopy;
    }
    return TeensyDevice::Unknown;
}

/**
 * @brief Authoritative ARCH-7-C probe: open @p portName, run the
 *        non-destructive identify handshake, classify.
 *
 * Opens its OWN QSerialPort (115200 8N1), sends "?vers" then 0x00 with
 * short bounded timeouts, closes the port, and returns
 * classify_teensy_probe() of the two responses. Returns Unknown if the
 * port cannot be opened — never a guess.
 *
 * Defined in teensy_probe.cpp. The real body requires a Qt build with
 * the QtSerialPort module; without it the function still links but
 * always returns Unknown (UFT_HAS_SERIALPORT is undefined).
 */
TeensyDevice probe_teensy_serial(const QString &portName);

}  // namespace uft::hal
