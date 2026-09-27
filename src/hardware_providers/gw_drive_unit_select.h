/**
 * @file gw_drive_unit_select.h
 * @brief #43 D1 (MF-XXXX): the Hardware tab's drive combo -> Greaseweazle
 *        bus unit, in ONE place.
 *
 * Until #43 nothing read `comboDriveSelect`; GreaseweazleProviderV2 ran
 * unit 0 whatever the operator chose. HardwareTab now reads the combo on
 * connect and on every change while a Greaseweazle is connected — through
 * this function, so that both paths refuse the same things:
 *
 *   * a combo item without integer user data — `QVariant().toInt()` is 0,
 *     which would silently be unit 0 again, i.e. the old defect;
 *   * a value outside 0..1 — the same combo carries Commodore device
 *     numbers 8..11 when the XUM1541 is selected, and the provider only
 *     addresses units 0 and 1 (ensure_drive_selected());
 *   * any controller other than the Greaseweazle — deliberately not part
 *     of #43 (owner decision 2026-09-26, "Nur Greaseweazle"), and none of
 *     them honours the combo today: KryoFlux (`dtc -d0`) and FluxEngine
 *     (`drive:0`) hard-wire drive 0, the XUM1541 provider keeps its
 *     default device 8 while the combo offers 8..11, and the SCP direct
 *     HAL sends no drive-select command (SELA/SELB) at all.
 *
 * Qt-free on purpose: the caller unpacks the QVariant, so the mapping is
 * testable without a widget (tests/test_gw_drive_unit_select.cpp).
 */
#ifndef UFT_GW_DRIVE_UNIT_SELECT_H
#define UFT_GW_DRIVE_UNIT_SELECT_H

#include <optional>
#include <string_view>

namespace uft::hal {

/**
 * @param controller_key  comboController's item data ("greaseweazle", ...)
 * @param data_is_int     true iff comboDriveSelect's current item carries
 *                        user data that converts to int
 * @param data            that integer (ignored when @p data_is_int is false)
 * @return the bus unit (0 = drive A, 1 = drive B on the IBM-PC bus), or
 *         std::nullopt when the combo does not describe a Greaseweazle unit.
 */
[[nodiscard]] constexpr std::optional<int>
gw_drive_unit_from_combo(std::string_view controller_key, bool data_is_int,
                         int data) noexcept
{
    if (controller_key != "greaseweazle") return std::nullopt;
    if (!data_is_int) return std::nullopt;
    if (data != 0 && data != 1) return std::nullopt;
    return data;
}

}  // namespace uft::hal

#endif  // UFT_GW_DRIVE_UNIT_SELECT_H
