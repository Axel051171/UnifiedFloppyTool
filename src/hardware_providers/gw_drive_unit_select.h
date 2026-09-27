/**
 * @file gw_drive_unit_select.h
 * @brief #43 D1 (MF-1365) and its follow-up (MF-1376): the Hardware tab's
 *        drive combo -> Greaseweazle BUS and unit, in ONE place.
 *
 * Until #43 nothing read `comboDriveSelect`; GreaseweazleProviderV2 ran
 * unit 0 whatever the operator chose. HardwareTab now reads the combo on
 * connect and on every change while a Greaseweazle is connected — through
 * this function, so that both paths refuse the same things:
 *
 *   * a combo item without integer user data — `QVariant().toInt()` is 0,
 *     which would silently be unit 0 again, i.e. the old defect;
 *   * a value that names no Greaseweazle drive — the same combo carries
 *     Commodore device numbers 8..11 when the XUM1541 is selected;
 *   * any controller other than the Greaseweazle — deliberately not part
 *     of #43 (owner decision 2026-09-26, "Nur Greaseweazle"), and none of
 *     them honours the combo today: KryoFlux (`dtc -d0`) and FluxEngine
 *     (`drive:0`) hard-wire drive 0, the XUM1541 provider keeps its
 *     default device 8 while the combo offers 8..11, and the SCP direct
 *     HAL sends no drive-select command (SELA/SELB) at all.
 *
 * MF-1376 — TWO BUSES, not two names (petrkr on #43, 2026-09-27: "Drive
 * 0/1/2 is NOT same as Drive A/B"). The first version offered "A: (Drive
 * 0)" / "B: (Drive 1)" and set no bus, so the HAL chose IBM-PC and a
 * Shugart drive was unreachable. Reference: gw's own drive argument,
 * greaseweazle `src/greaseweazle/tools/util.py:127-141` (Unlicense, read
 * at 26690f8):
 *
 *     'A' -> (BusType.IBMPC, 0)     '0' -> (BusType.Shugart, 0)
 *     'B' -> (BusType.IBMPC, 1)     '1'..'3' -> (BusType.Shugart, 1..3)
 *
 * GwBus carries gw's BusType numbers (IBMPC = 1, Shugart = 2, usb.py:125),
 * which are also uft_gw_bus_type_t's.
 *
 * Combo item data: IBM-PC A/B keep the values 0/1 they had since MF-1365;
 * Shugart unit u is 16 + u, so no two drives share a value.
 *
 * Qt-free on purpose: the caller unpacks the QVariant, so the mapping is
 * testable without a widget (tests/test_gw_drive_unit_select.cpp).
 */
#ifndef UFT_GW_DRIVE_UNIT_SELECT_H
#define UFT_GW_DRIVE_UNIT_SELECT_H

#include <cstdint>
#include <optional>
#include <string_view>

namespace uft::hal {

/** Greaseweazle drive bus; values as gw's BusType and uft_gw_bus_type_t. */
enum class GwBus : std::uint8_t { IbmPc = 1, Shugart = 2 };

/** One Greaseweazle drive: a bus and a unit on that bus. */
struct GwDrive {
    GwBus bus;
    int   unit;
    friend constexpr bool operator==(const GwDrive &, const GwDrive &) = default;
};

/** Highest unit per bus: IBM-PC A/B, Shugart 0..3 (gw tools/util.py). */
[[nodiscard]] constexpr int gw_bus_max_unit(GwBus bus) noexcept
{
    return bus == GwBus::IbmPc ? 1 : 3;
}

/** The combo item data for one drive (see file head). */
[[nodiscard]] constexpr int gw_drive_combo_code(GwDrive d) noexcept
{
    return d.bus == GwBus::IbmPc ? d.unit : 16 + d.unit;
}

/**
 * @param controller_key  comboController's item data ("greaseweazle", ...)
 * @param data_is_int     true iff comboDriveSelect's current item carries
 *                        user data that converts to int
 * @param data            that integer (ignored when @p data_is_int is false)
 * @return the drive (bus + unit), or std::nullopt when the combo does not
 *         describe a Greaseweazle drive.
 */
[[nodiscard]] constexpr std::optional<GwDrive>
gw_drive_from_combo(std::string_view controller_key, bool data_is_int,
                    int data) noexcept
{
    if (controller_key != "greaseweazle") return std::nullopt;
    if (!data_is_int) return std::nullopt;
    if (data == 0 || data == 1) return GwDrive{GwBus::IbmPc, data};
    if (data >= 16 && data <= 16 + gw_bus_max_unit(GwBus::Shugart))
        return GwDrive{GwBus::Shugart, data - 16};
    return std::nullopt;
}

}  // namespace uft::hal

#endif  // UFT_GW_DRIVE_UNIT_SELECT_H
