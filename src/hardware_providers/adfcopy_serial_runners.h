/**
 * @file adfcopy_serial_runners.h
 * @brief Runner-factories for the 5 ADFCopy V2 provider callable slots (MF-252).
 *
 * Each factory returns a `std::function<...>` that captures a shared
 * IADFCopyTransport and drives the documented ADFCopy binary protocol.
 *
 * Protocol summary:
 *   CMD_INIT       (0x01)  → wait 'O'
 *   CMD_SEEK       (0x02) + track_byte → wait 'O'
 *   CMD_READ_FLUX  (0x06) + track + revs → 3-byte header + N flux bytes
 *   CMD_GET_STATUS (0x0B) → 1-byte status bitmask
 *
 * Status bitmask flags:
 *   bit 0 = DISK_PRESENT
 *   bit 1 = WRITE_PROT
 *   bit 2 = MOTOR_ON
 *   bit 3 = FLUX_CAPABLE
 *
 * Sample clock: 40 MHz / 25 ns per tick (matches SCP resolution).
 */
#pragma once

#include "adfcopy_provider_v2.h"
#include <memory>

namespace uft::hal {

using ADFCopyTransportPtr = std::shared_ptr<IADFCopyTransport>;

/* Protocol byte constants — single source-of-truth here so the runners
 * and any future test/firmware reference share the same values. */
constexpr uint8_t kAdfcCmdInit       = 0x01;
constexpr uint8_t kAdfcCmdSeek       = 0x02;
constexpr uint8_t kAdfcCmdReadFlux   = 0x06;
constexpr uint8_t kAdfcCmdGetStatus  = 0x0B;
constexpr uint8_t kAdfcRspOk         = 'O';   /* 0x4F */
constexpr uint8_t kAdfcRspError      = 'E';   /* 0x45 */
constexpr uint8_t kAdfcRspNoDisk     = 'D';   /* 0x44 */

constexpr uint8_t kAdfcStatusDiskPresent  = 0x01;
constexpr uint8_t kAdfcStatusWriteProt    = 0x02;
constexpr uint8_t kAdfcStatusMotorOn      = 0x04;
constexpr uint8_t kAdfcStatusFluxCapable  = 0x08;

ADFCopyProviderV2::ADFCopyReadRunner
make_adfcopy_read_runner(ADFCopyTransportPtr tx);

ADFCopyProviderV2::ADFCopyMotorRunner
make_adfcopy_motor_runner(ADFCopyTransportPtr tx);

ADFCopyProviderV2::ADFCopySeekRunner
make_adfcopy_seek_runner(ADFCopyTransportPtr tx);

ADFCopyProviderV2::ADFCopyRecalRunner
make_adfcopy_recal_runner(ADFCopyTransportPtr tx);

ADFCopyProviderV2::ADFCopyDetectRunner
make_adfcopy_detect_runner(ADFCopyTransportPtr tx);

} // namespace uft::hal
