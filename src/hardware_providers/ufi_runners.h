/**
 * @file ufi_runners.h
 * @brief UFI-C-HAL-backed runners for USBFloppyProviderV2 (MF-258).
 *
 * Closes the last open slot in UFT-001 (USB-Floppy / UFI). Unlike the
 * other M3 transports this is NOT libusb or QSerialPort or a subprocess
 * — it's a direct SCSI-pass-through layer (SG_IO on Linux,
 * IOCTL_SCSI_PASS_THROUGH on Windows, IOKit on macOS). The
 * platform-specific transport is owned by src/hal/ufi_linux.c (+
 * future ufi_win.c / ufi_mac.c), registered with the high-level UFI
 * command layer via uft_ufi_backend_init().
 *
 * These runner-factories sit on top of that — they translate the V2
 * provider's typed requests into uft_ufi_inquiry / read_capacity /
 * read_sectors / write_sectors calls.
 *
 * On hosts without a registered backend (current Windows/macOS UFT
 * build) the C-HAL returns UFT_ERR_NOT_IMPLEMENTED; the runners
 * surface this as `backend_unavailable = true` in the result struct,
 * which the provider translates to a clear ProviderError. Forensically
 * honest — never fabricates sectors.
 */
#pragma once

#include "usbfloppy_provider_v2.h"

namespace uft::hal {

/** Ensure the platform UFI backend is registered (calls
 *  uft_ufi_backend_init() once). Returns true on success, false on
 *  platforms where no backend exists in this build (Windows/macOS
 *  pre-MF-259 — see ufi_backend.c). Idempotent. */
bool ufi_runners_ensure_backend();

/** Build a UsbFloppyReadRunner that wraps uft_ufi_read_sectors().
 *  Maps cylinder/head/sector + geometry hints to LBA, calls the
 *  C-HAL, populates UsbFloppyReadResult including sense diagnostics. */
USBFloppyProviderV2::UsbFloppyReadRunner
make_usbfloppy_read_runner();

/** Build a UsbFloppyWriteRunner that wraps uft_ufi_write_sectors()
 *  with optional verify-after-write via uft_ufi_verify_lba(). */
USBFloppyProviderV2::UsbFloppyWriteRunner
make_usbfloppy_write_runner();

/** Build a UsbFloppyDetectRunner that wraps uft_ufi_inquiry() +
 *  uft_ufi_test_unit_ready() + uft_ufi_read_capacity(). The device
 *  path is captured at construction time via the closure-bound
 *  `device_path` argument supplied by the caller. */
USBFloppyProviderV2::UsbFloppyDetectRunner
make_usbfloppy_detect_runner(std::string device_path);

} // namespace uft::hal
