/**
 * @file test_gw_port_kurzname.cpp
 * @brief #42 (MF-XXXX): GreaseweazleProviderV2::open() must accept the
 *        SHORT port name that the Hardware tab hands it.
 *
 * ── The defect ─────────────────────────────────────────────────────────
 *
 * hardwaretab.cpp stores `QSerialPortInfo::portName()` as the port combo's
 * item data and passes it verbatim to `GreaseweazleProviderV2::open()`.
 * On Linux that is "ttyACM0", not "/dev/ttyACM0". The protected C HAL's
 * POSIX `serial_open()` calls a raw `open(port, ...)`, and a raw open()
 * resolves a relative name against the process's CURRENT DIRECTORY. The
 * reporter of GitHub issue #42 saw exactly that:
 *
 *     [GW] open(ttyACM0): No such file or directory (errno=2)
 *
 * The fix sits in the provider (the C HAL is a protected path): outside
 * Windows, a name that does not start with '/', "./" or "../" gets "/dev/"
 * in front — the rule of Qt's own
 * `QSerialPortInfoPrivate::portNameToSystemLocation()`
 * (qtserialport, src/serialport/qserialportinfo_unix.cpp).
 *
 * ── How this test reaches the defect without hardware ──────────────────
 *
 * A pseudo-terminal stands in for the USB CDC-ACM port, and the
 * Greaseweazle firmware automaton (tests/emulators/greaseweazle/) answers
 * on its master side through the wire bridge (MF-848). A pump thread
 * copies bytes between the pty master and `gw_wire_t`. The provider under
 * test opens the SLAVE side exactly like hardwaretab.cpp does: through
 * `GreaseweazleProviderV2::open(name, &err)` — which, since P3-551
 * (MF-1356), runs the full GET_INFO handshake. The automaton answers it,
 * so a successful open() here means the real handshake ran over the
 * short name, not merely that a file descriptor was obtained.
 *
 * The short name is `ptsname()` without its "/dev/" prefix ("pts/N" on
 * Linux). The test chdir()s into a fresh mkdtemp() directory first, so a
 * relative lookup finds nothing there — the same situation as the
 * reporter's working directory.
 *
 * ── Why Linux only ──────────────────────────────────────────────────────
 *
 *   * Windows: the HAL prepends "\\.\" itself (serial_open, Windows
 *     branch) and a COM port has no pty; returns 77 (named skip).
 *   * macOS: NOT measured in this run (no macOS host). Its pty and termios
 *     behaviour differs from Linux in ways this rig depends on (poll() on
 *     device files, TIOCMBIS on a pty), and an unmeasured rig in the
 *     gating CI matrix would be a guess. Returns 77 with the reason; one
 *     measured macOS run can lift this. That run would also cover the
 *     slash-free form of the report ("ttys00N"), which Linux ptys cannot
 *     produce without root ("pts/N" always contains a slash).
 *
 * ── A warning for whoever changes the GUI side ─────────────────────────
 *
 * Do not "fix" this by storing `QSerialPortInfo::systemLocation()` in the
 * combo instead: on Windows that is "\\.\COMn", and the HAL would build
 * "\\.\\\.\COMn" from it — every Windows user would break.
 */

#include <cstdio>

#if defined(_WIN32)

int main()
{
    std::puts("SKIP: #42 is the POSIX raw-open() path; Windows prepends "
              "the device prefix inside the HAL and has no pty.");
    return 77;
}

#elif defined(__APPLE__)

int main()
{
    std::puts("SKIP: pty rig not measured on macOS (poll()/termios on a "
              "pty differ from Linux); needs one measured macOS run.");
    return 77;
}

#else /* Linux / other POSIX */

#include <atomic>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>

#include <fcntl.h>
#include <sys/select.h>
#include <unistd.h>

extern "C" {
#include "emulators/greaseweazle/gw_wire_bridge.h"
}
#include "hardware_providers/greaseweazle_provider_v2.h"

static gw_fw_t           g_fw;
static gw_wire_t         g_draht;
static std::mutex        g_draht_mx;
static int               g_master = -1;
static std::atomic<bool> g_stop{false};

static int g_fehl = 0;

#define ZUSAGE(nr, bedingung, text)                                        \
    do {                                                                   \
        const bool _ok = (bedingung);                                      \
        std::printf("  [%s] Zusage %d: %s\n", _ok ? "GRUEN" : "ROT", (nr), \
                    (text));                                               \
        if (!_ok) ++g_fehl;                                                \
    } while (0)

/* Copies bytes between the pty master and the firmware automaton. select()
 * and not poll(): the HAL itself uses select(), and poll() on device files
 * is the part that is not portable. */
static void pumpe()
{
    uint8_t buf[512];
    while (!g_stop.load()) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(g_master, &rfds);
        struct timeval tv = {0, 20000};
        const int r = select(g_master + 1, &rfds, nullptr, nullptr, &tv);
        if (r <= 0) continue;
        const ssize_t n = read(g_master, buf, sizeof buf);
        if (n <= 0) {
            /* EIO while no slave is open (between two open() attempts). */
            usleep(5000);
            continue;
        }
        std::lock_guard<std::mutex> lk(g_draht_mx);
        g_draht.ops.write(g_draht.ops.user, buf, static_cast<size_t>(n));
        for (;;) {
            size_t got = 0;
            g_draht.ops.read_available(g_draht.ops.user, buf, sizeof buf,
                                       &got, 0);
            if (got == 0) break;
            size_t off = 0;
            while (off < got) {
                const ssize_t w = write(g_master, buf + off, got - off);
                if (w <= 0) break;
                off += static_cast<size_t>(w);
            }
        }
    }
}

/* Fresh firmware state for every open(): v1.6 is above UFT's floor (0.31)
 * and distinct from the automaton's default (1.23), so the version string
 * proves that THIS handshake answered. */
static void automat_neu()
{
    std::lock_guard<std::mutex> lk(g_draht_mx);
    gw_fw_reset(&g_fw);
    gw_fw_power_on_defaults(&g_fw);
    gw_fw_set_firmware_version(&g_fw, 1, 6);
    gw_wire_init(&g_draht, &g_fw);
}

struct Versuch {
    bool        ok = false;
    std::string err;
    std::string fw;
};

static Versuch oeffne(const char *name)
{
    automat_neu();
    Versuch v;
    ::uft::hal::GreaseweazleProviderV2 gw;
    v.ok = gw.open(name, &v.err);
    v.fw = gw.firmware_version();
    std::printf("    open(\"%s\") = %s%s%s  fw=\"%s\"\n", name,
                v.ok ? "true" : "false", v.ok ? "" : ", err=",
                v.ok ? "" : v.err.c_str(), v.fw.c_str());
    return v;   /* provider destructor closes the port */
}

int main()
{
    std::puts("test_gw_port_kurzname (#42): short port name reaches the "
              "Greaseweazle provider");

    g_master = posix_openpt(O_RDWR | O_NOCTTY);
    if (g_master < 0 || grantpt(g_master) != 0 || unlockpt(g_master) != 0) {
        std::printf("SKIP: no pseudo-terminal available (%s)\n",
                    std::strerror(errno));
        return 77;
    }
    const char *pn = ptsname(g_master);
    if (!pn || std::strncmp(pn, "/dev/", 5) != 0) {
        std::printf("SKIP: ptsname() is not under /dev (%s)\n",
                    pn ? pn : "(null)");
        return 77;
    }
    const std::string voll = pn;             /* "/dev/pts/N"  */
    const std::string kurz = voll.substr(5); /* "pts/N"       */

    char tmpl[] = "/tmp/uft_gw42.XXXXXX";
    if (!mkdtemp(tmpl) || chdir(tmpl) != 0) {
        std::printf("Pruefstand kaputt: mkdtemp/chdir (%s)\n",
                    std::strerror(errno));
        return 1;
    }
    /* An explicitly relative name must stay relative (Qt rule: "./" is
     * not forced under /dev). */
    const char *link_name = "./tty42";
    if (symlink(voll.c_str(), "tty42") != 0) {
        std::printf("Pruefstand kaputt: symlink (%s)\n", std::strerror(errno));
        return 1;
    }

    std::thread pump(pumpe);

    /* 1. CONTROL FIRST: the full path must open. If it does not, the rig is
     *    broken and every red below would be red for the wrong reason. */
    const Versuch k = oeffne(voll.c_str());
    if (!k.ok || k.fw != "v1.6") {
        std::puts("Pruefstand kaputt: the full pty path does not open or no "
                  "handshake ran - nothing below would be a statement.");
        g_stop = true;
        pump.join();
        unlink("tty42");
        if (chdir("/") != 0) { /* nothing to undo */ }
        rmdir(tmpl);
        close(g_master);
        return 1;
    }
    ZUSAGE(1, k.ok, "control: the full path (/dev/pts/N) opens");

    /* 2./3. The defect: the name as QSerialPortInfo::portName() gives it. */
    const Versuch s = oeffne(kurz.c_str());
    ZUSAGE(2, s.ok && s.err.empty(),
           "the short name (pts/N, as QSerialPortInfo::portName()) opens");
    ZUSAGE(3, s.fw == "v1.6",
           "over the short name the GET_INFO handshake ran (firmware v1.6)");

    /* 4./5. Qt's rule has a second half: "./" and "../" stay relative.
     * Each prefix is its own clause of the rule, so each gets its own
     * promise — dropping only the "../" clause would make the second name
     * "/dev/../uft_gw42.XXXXXX/tty42", which does not exist. */
    const Versuch r = oeffne(link_name);
    ZUSAGE(4, r.ok, "\"./tty42\" (symlink in the cwd) opens relative, "
                    "not forced under /dev");
    const std::string hoch = std::string("../") +
                             (std::strrchr(tmpl, '/') + 1) + "/tty42";
    const Versuch h = oeffne(hoch.c_str());
    ZUSAGE(5, h.ok, "\"../<tmpdir>/tty42\" opens relative, not forced "
                    "under /dev");

    /* 6. The prefix must not turn a missing device into a success. */
    const Versuch n = oeffne("uft-kein-geraet-42");
    ZUSAGE(6, !n.ok && !n.err.empty(),
           "a name that exists nowhere is refused with a message");

    g_stop = true;
    pump.join();
    unlink("tty42");
    if (chdir("/") != 0) { /* nothing to undo */ }
    rmdir(tmpl);
    close(g_master);

    if (g_fehl) {
        std::printf("FAILED: %d of 6 promises red\n", g_fehl);
        return 1;
    }
    std::puts("OK: 6 of 6 promises green");
    return 0;
}

#endif
