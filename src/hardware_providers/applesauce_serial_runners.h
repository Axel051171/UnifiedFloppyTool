/**
 * @file applesauce_serial_runners.h
 * @brief Runner-factories that bind an IApplesauceTransport to the 7
 *        Applesauce V2 provider callable slots (MF-250).
 *
 * Each factory returns a `std::function<...>` that captures a shared
 * transport and implements the corresponding Applesauce text-protocol
 * exchange. Pure protocol layer — no hardware-specific knowledge
 * beyond the published Applesauce command set.
 *
 * Defensive contract: every runner returns a fully-populated *Result
 * struct, sets `transport_unavailable = true` if the transport pointer
 * is null, and sets `device_error = true` + populates `error_message`
 * on any unexpected response. Never throws.
 *
 * Protocol reference: src/hal/uft_applesauce.c reverse-engineered
 * notes + the published Applesauce SDK (cbmstuff.com/applesauce).
 * Sample clock 8 MHz / 125 ns per tick.
 *
 * Hardware verification status: SAME as
 * docs/M3_APPLESAUCE_TRANSPORT.md §5 — compiles + unit-testable, but
 * has NOT yet been run against a real Applesauce. HardwareTab keeps
 * the orange "Disconnect (Preview)" styling until that bench-test
 * runs green.
 */
#pragma once

#include "applesauce_provider_v2.h"

#include <memory>

namespace uft::hal {

/* The transport is owned by the caller; we hold a shared_ptr so that
 * all 7 runners share the same QSerialPort instance — they each send
 * commands and receive responses on the SAME wire, so they need the
 * SAME transport object. */
using ApplesauceTransportPtr = std::shared_ptr<IApplesauceTransport>;

/** Build a read-runner that talks the `sync:on / disk:readx N /
 *  data:?size / data:<N / sync:off` Applesauce flux-capture protocol. */
ApplesauceProviderV2::ApplesauceReadRunner
make_applesauce_read_runner(ApplesauceTransportPtr tx);

/** Build a write-runner that talks the `disk:?write / data:clear /
 *  data:>N / disk:write` Applesauce flux-write protocol. */
ApplesauceProviderV2::ApplesauceWriteRunner
make_applesauce_write_runner(ApplesauceTransportPtr tx);

/** Build a motor-runner that talks `psu:on / motor:on` (or `motor:off`). */
ApplesauceProviderV2::ApplesauceMotorRunner
make_applesauce_motor_runner(ApplesauceTransportPtr tx);

/** Build a seek-runner that talks `head:track N / head:side X`. */
ApplesauceProviderV2::ApplesauceSeekRunner
make_applesauce_seek_runner(ApplesauceTransportPtr tx);

/** Build a recalibrate-runner that talks `head:zero`. */
ApplesauceProviderV2::ApplesauceRecalRunner
make_applesauce_recal_runner(ApplesauceTransportPtr tx);

/** Build an RPM-runner that talks `sync:?speed`. */
ApplesauceProviderV2::ApplesauceRpmRunner
make_applesauce_rpm_runner(ApplesauceTransportPtr tx);

/** Build a detect-runner that talks `?kind / data:?max / ?vers / ?pcb`. */
ApplesauceProviderV2::ApplesauceDetectRunner
make_applesauce_detect_runner(ApplesauceTransportPtr tx);

} // namespace uft::hal
