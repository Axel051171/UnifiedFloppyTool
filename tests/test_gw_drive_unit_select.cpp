/**
 * @file test_gw_drive_unit_select.cpp
 * @brief #43 D1 (MF-XXXX): the Hardware tab's drive combo becomes a
 *        Greaseweazle bus unit through ONE mapping, and every value that is
 *        not a Greaseweazle unit is refused instead of becoming unit 0.
 *
 * Before #43 nothing read `ui->comboDriveSelect`; the provider always ran
 * unit 0 whatever the operator chose. The fix reads the combo in
 * HardwareTab::onConnect() and on every combo change while a Greaseweazle
 * is connected, through `uft::hal::gw_drive_unit_from_combo()`.
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

using uft::hal::gw_drive_unit_from_combo;

static int g_fehl = 0;
static int g_zusagen = 0;

#define ZUSAGE(bedingung, text)                                            \
    do {                                                                   \
        const bool _ok = (bedingung);                                      \
        ++g_zusagen;                                                       \
        std::printf("  [%s] %s\n", _ok ? "GRUEN" : "ROT", (text));         \
        if (!_ok) ++g_fehl;                                                \
    } while (0)

int main()
{
    std::puts("test_gw_drive_unit_select (#43 D1): drive combo -> GW unit");

    ZUSAGE(gw_drive_unit_from_combo("greaseweazle", true, 0) == 0,
           "A: (Drive 0) -> unit 0");
    ZUSAGE(gw_drive_unit_from_combo("greaseweazle", true, 1) == 1,
           "B: (Drive 1) -> unit 1");

    ZUSAGE(!gw_drive_unit_from_combo("greaseweazle", false, 0).has_value(),
           "missing user data is refused, not taken as unit 0");
    ZUSAGE(!gw_drive_unit_from_combo("greaseweazle", true, 8).has_value(),
           "Commodore device 8 is not a Greaseweazle unit");
    ZUSAGE(!gw_drive_unit_from_combo("greaseweazle", true, 2).has_value() &&
           !gw_drive_unit_from_combo("greaseweazle", true, -1).has_value(),
           "units outside 0..1 are refused");
    ZUSAGE(!gw_drive_unit_from_combo("xum1541", true, 8).has_value(),
           "XUM1541 device numbers never reach the GW path");
    ZUSAGE(!gw_drive_unit_from_combo("kryoflux", true, 1).has_value() &&
           !gw_drive_unit_from_combo("fluxengine", true, 1).has_value() &&
           !gw_drive_unit_from_combo("scp", true, 1).has_value(),
           "only the Greaseweazle reads the combo (every other controller: P3 note)");

    if (g_fehl) {
        std::printf("FAILED: %d of %d promises red\n", g_fehl, g_zusagen);
        return 1;
    }
    std::printf("OK: %d of %d promises green\n", g_zusagen, g_zusagen);
    return 0;
}
