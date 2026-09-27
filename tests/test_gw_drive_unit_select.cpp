/**
 * @file test_gw_drive_unit_select.cpp
 * @brief #43 D1 (MF-XXXX): the Hardware tab's drive combo becomes a
 *        Greaseweazle bus unit through ONE mapping, and every value that is
 *        not a Greaseweazle unit is refused instead of becoming unit 0.
 *
 * Before #43 nothing read `ui->comboDriveSelect`; the provider always ran
 * unit 0 whatever the operator chose. The fix reads the combo in
 * HardwareTab::onConnect() and on every combo change while a Greaseweazle
 * is connected, through `uft::hal::gw_drive_from_combo()`.
 *
 * The three refusals are the point, because each is today's defect in a
 * new shape:
 *   * missing user data — `QVariant().toInt()` is 0, i.e. unit 0 again;
 *   * Commodore device numbers 8..11 — the SAME combo carries them when
 *     the XUM1541 is selected (HardwareTab::onControllerChanged);
 *   * another controller — NOT part of this fix (owner decision "#43: Nur
 *     Greaseweazle"); none of them honours the combo today (KryoFlux and
 *     FluxEngine hard-wire drive 0, XUM1541 keeps device 8, SCP selects
 *     no drive) — see gw_drive_unit_select.h.
 *
 * What this test cannot reach: the GUI slots themselves. Test targets are
 * built without UFT_HAS_HAL, so HardwareTab's Greaseweazle connect takes
 * the simulated branch, and a failed open() raises a modal QMessageBox.
 * tests/test_hardware_tab_gui.cpp covers the combo's labels and item data
 * and that setFluxJobRunning() disables it; the connect and change path
 * (open(port, unit), onDriveSelectChanged(), motor-off, set_drive_unit())
 * is NOT exercised by any test.
 */

#include <cstdio>

#include "hardware_providers/gw_drive_unit_select.h"

using uft::hal::gw_drive_from_combo;
using uft::hal::gw_drive_combo_code;
using uft::hal::GwBus;
using uft::hal::GwDrive;

static int g_fehl = 0;
static int g_zusagen = 0;

#define ZUSAGE(bedingung, text)                                            \
    do {                                                                   \
        const bool _ok = (bedingung);                                      \
        ++g_zusagen;                                                       \
        std::printf("  [%s] %s\n", _ok ? "GRUEN" : "ROT", (text));         \
        if (!_ok) ++g_fehl;                                                \
    } while (0)

/* One GW drive from a combo code — outside the macro, whose comma
 * splitting a brace initialiser would break. */
static bool ist(int code, GwBus bus, int unit)
{
    const auto d = gw_drive_from_combo("greaseweazle", true, code);
    return d && d->bus == bus && d->unit == unit;
}

int main()
{
    std::puts("test_gw_drive_unit_select (#43 D1, MF-1376): drive combo -> GW bus + unit");

    /* gw tools/util.py:127-141: A/B -> IBM-PC 0/1, 0..3 -> Shugart 0..3 */
    ZUSAGE(ist(0, GwBus::IbmPc, 0),
           "A: -> IBM-PC bus, unit 0");
    ZUSAGE(ist(1, GwBus::IbmPc, 1),
           "B: -> IBM-PC bus, unit 1");
    ZUSAGE(ist(16, GwBus::Shugart, 0) &&
           ist(18, GwBus::Shugart, 2) &&
           ist(19, GwBus::Shugart, 3),
           "Shugart 0/2/3 -> Shugart bus, same unit (petrkr: not the same as A/B)");
    bool rund = true;
    for (GwBus bus : {GwBus::IbmPc, GwBus::Shugart})
        for (int u = 0; u <= uft::hal::gw_bus_max_unit(bus); ++u)
            rund = rund && gw_drive_from_combo("greaseweazle", true,
                                               gw_drive_combo_code({bus, u})) ==
                               GwDrive{bus, u};
    ZUSAGE(rund, "every drive round-trips through its combo code");

    ZUSAGE(!gw_drive_from_combo("greaseweazle", false, 0).has_value(),
           "missing user data is refused, not taken as unit 0");
    ZUSAGE(!gw_drive_from_combo("greaseweazle", true, 8).has_value(),
           "Commodore device 8 is not a Greaseweazle drive");
    ZUSAGE(!gw_drive_from_combo("greaseweazle", true, 2).has_value() &&
           !gw_drive_from_combo("greaseweazle", true, -1).has_value() &&
           !gw_drive_from_combo("greaseweazle", true, 20).has_value() &&
           !gw_drive_from_combo("greaseweazle", true, 15).has_value(),
           "IBM-PC unit 2, Shugart unit 4 and gaps are refused");
    ZUSAGE(!gw_drive_from_combo("xum1541", true, 8).has_value(),
           "XUM1541 device numbers never reach the GW path");
    ZUSAGE(!gw_drive_from_combo("kryoflux", true, 1).has_value() &&
           !gw_drive_from_combo("fluxengine", true, 16).has_value() &&
           !gw_drive_from_combo("scp", true, 1).has_value(),
           "only the Greaseweazle reads the combo (every other controller: P3 note)");

    if (g_fehl) {
        std::printf("FAILED: %d of %d promises red\n", g_fehl, g_zusagen);
        return 1;
    }
    std::printf("OK: %d of %d promises green\n", g_zusagen, g_zusagen);
    return 0;
}
