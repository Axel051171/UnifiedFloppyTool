/**
 * @file uft_hardware_guide.h
 * @brief Hardware Controller Setup Guides
 * 
 * P3-004: Documentation for all supported hardware
 * 
 * Supported Controllers:
 * - FC5025
 * - XUM1541
 */

#ifndef UFT_HARDWARE_GUIDE_H
#define UFT_HARDWARE_GUIDE_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * GREASEWEAZLE
 * ============================================================================ */

/**
 * @{
 * 
 * @section uft_gw_overview Overview
 * 
 * - PC 5.25" and 3.5" drives
 * - Shugart and IBM PC interfaces
 * - Flux-level reading and writing
 * 
 * | Feature        | Support |
 * |----------------|---------|
 * | Read Flux      | ✅      |
 * | Write Flux     | ✅      |
 * | Sample Rate    | 72 MHz  |
 * | Index Detect   | ✅      |
 * | Write Protect  | ✅      |
 * 
 * @section uft_gw_setup Setup
 * 
 * 2. Connect floppy drive via 34-pin cable
 * 3. Install drivers (automatic on most OS)
 * 4. Run: `gw info` to verify
 * 
 * Linux udev rule (optional):
 * @code
 * # /etc/udev/rules.d/60-greaseweazle.rules
 * SUBSYSTEM=="usb", ATTR{idVendor}=="1209", ATTR{idProduct}=="4d69", MODE="0666"
 * @endcode
 * 
 * @section uft_gw_uft UFT Configuration
 * 
 * @code{.c}
 * uft_hal_config_t cfg = {
 *     .controller = UFT_CTRL_GREASEWEAZLE,
 *     .device = "/dev/ttyACM0",  // Linux
 *     // .device = "COM3",       // Windows
 *     .drive_select = 0,
 * };
 * @endcode
 * 
 * @section uft_gw_pins Pinout
 * 
 * | Pin | Name       | Function |
 * |-----|------------|----------|
 * | 2   | Density    | HD select |
 * | 8   | Index      | Index pulse |
 * | 10  | Motor A    | Drive 0 motor |
 * | 12  | Drive A    | Drive 0 select |
 * | 14  | Drive B    | Drive 1 select |
 * | 16  | Motor B    | Drive 1 motor |
 * | 18  | Direction  | Step direction |
 * | 20  | Step       | Step pulse |
 * | 22  | Write Data | Write flux |
 * | 24  | Write Gate | Enable write |
 * | 26  | Track 0    | Track 0 sensor |
 * | 28  | Write Prot | Write protect |
 * | 30  | Read Data  | Read flux |
 * | 32  | Side       | Head select |
 * | 34  | Disk Chg   | Disk change |
 */

/** @} */

/* ============================================================================
 * FLUXENGINE
 * ============================================================================ */

/**
 * @{
 * 
 * @section fe_overview Overview
 * 
 * 
 * | Feature        | Support |
 * |----------------|---------|
 * | Read Flux      | ✅      |
 * | Write Flux     | ✅      |
 * | Sample Rate    | 12 MHz  |
 * | Index Detect   | ✅      |
 * 
 * @section fe_setup Setup
 * 
 * 1. Flash firmware to CY8CKIT-059 board
 * 2. Connect floppy cable adapter
 * 3. Install USB drivers
 * 
 * @section fe_uft UFT Configuration
 * 
 * @code{.c}
 * uft_hal_config_t cfg = {
 *     .controller = UFT_CTRL_FLUXENGINE,
 *     .device = NULL,  // Auto-detect
 * };
 * @endcode
 */

/** @} */

/* ============================================================================
 * KRYOFLUX
 * ============================================================================ */

/**
 * @{
 * 
 * @section uft_kf_overview Overview
 * 
 * high-precision flux capture.
 * 
 * | Feature        | Support |
 * |----------------|---------|
 * | Read Flux      | ✅      |
 * | Write Flux     | ✅      |
 * | Sample Rate    | 24.027428 MHz |
 * | Index Detect   | ✅      |
 * | Calibration    | ✅      |
 * 
 * @section uft_kf_setup Setup
 * 
 * 2. Connect board and drive
 * 4. Test with DTC: `dtc -i0`
 * 
 * @section uft_kf_uft UFT Configuration
 * 
 * @code{.c}
 * uft_hal_config_t cfg = {
 *     .controller = UFT_CTRL_KRYOFLUX,
 *     .device = NULL,  // Auto-detect
 *     .options = {
 *         .sample_rate = 0,  // Use hardware default
 *     },
 * };
 * @endcode
 * 
 * @section uft_kf_stream Stream Format
 * 
 * - Flux timing samples
 * - Index pulse positions
 * - OOB (out-of-band) markers
 * 
 * Sample timing: 41.6 ns per tick
 */

/** @} */

/* ============================================================================
 * FC5025
 * ============================================================================ */

/**
 * @defgroup hw_fc5025 FC5025
 * @{
 * 
 * @section fc_overview Overview
 * 
 * Device Side FC5025 USB floppy controller for 5.25" drives.
 * 
 * | Feature        | Support |
 * |----------------|---------|
 * | Read Data      | ✅      |
 * | Write Data     | ✅      |
 * | FM/MFM         | ✅      |
 * | GCR            | ❌      |
 * 
 * @section fc_setup Setup
 * 
 * 1. Install libusb
 * 2. Connect FC5025 via USB
 * 3. Set udev rules (Linux)
 * 
 * @section fc_uft UFT Configuration
 * 
 * @code{.c}
 * uft_hal_config_t cfg = {
 *     .controller = UFT_CTRL_FC5025,
 *     .drive_select = 0,
 * };
 * @endcode
 */

/** @} */

/* ============================================================================
 * XUM1541
 * ============================================================================ */

/**
 * @defgroup hw_xum1541 XUM1541
 * @{
 * 
 * @section xu_overview Overview
 * 
 * XUM1541 is a USB adapter for Commodore IEC drives (1541, 1571, 1581).
 * 
 * | Feature        | Support |
 * |----------------|---------|
 * | IEC Protocol   | ✅      |
 * | Fast Loaders   | ✅      |
 * | Drive Reset    | ✅      |
 * | Parallel Cable | ✅      |
 * 
 * @section xu_setup Setup
 * 
 * 1. Flash XUM1541 firmware to AVR board
 * 2. Connect to drive via serial cable
 * 
 * @section xu_uft UFT Configuration
 * 
 * @code{.c}
 * uft_hal_config_t cfg = {
 *     .controller = UFT_CTRL_XUM1541,
 *     .device_number = 8,  // Drive number (8-15)
 * };
 * @endcode
 */

/** @} */

/* ============================================================================
 * DRIVE PROFILES
 * ============================================================================ */

/**
 * @defgroup hw_drives Drive Profiles
 * @{
 * 
 * | Drive Type     | RPM | Density | Step Delay | Settle |
 * |----------------|-----|---------|------------|--------|
 * | PC 5.25" DD    | 300 | DD      | 3 ms       | 15 ms  |
 * | PC 5.25" HD    | 360 | HD      | 3 ms       | 15 ms  |
 * | PC 3.5" DD     | 300 | DD      | 3 ms       | 15 ms  |
 * | PC 3.5" HD     | 300 | HD      | 3 ms       | 15 ms  |
 * | PC 3.5" ED     | 300 | ED      | 3 ms       | 15 ms  |
 * | C64 1541       | 300 | SD      | 12 ms      | 18 ms  |
 * | Amiga DD       | 300 | DD      | 3 ms       | 15 ms  |
 * | Amiga HD       | 150 | HD      | 3 ms       | 15 ms  |
 * | Apple II       | 300 | SD      | 4 ms       | 20 ms  |
 * 
 * @code{.c}
 * uft_drive_profile_t profile = uft_hal_get_drive_profile(UFT_DRIVE_PC_35_HD);
 * @endcode
 */

/** @} */

/* ============================================================================
 * TROUBLESHOOTING
 * ============================================================================ */

/**
 * @defgroup hw_troubleshoot Troubleshooting
 * @{
 * 
 * @section ts_common Common Issues
 * 
 * **No drive detected:**
 * - Check cable connections
 * - Verify drive is powered
 * - Try different drive select (0/1)
 * - Check USB connection
 * 
 * **Read errors:**
 * - Clean drive heads
 * - Try different RPM compensation
 * - Increase retry count
 * - Check disk for damage
 * 
 * **Write verification fails:**
 * - Check write protect tab
 * - Clean drive heads
 * - Try fresh disk
 * - Reduce write precompensation
 * 
 * **Index pulse not detected:**
 * - Check index sensor
 * - Verify cable wiring
 * - Try manual RPM setting
 * 
 * @section ts_debug Debug Commands
 * 
 * @code{.c}
 * // Enable debug output
 * uft_hal_set_debug_level(UFT_DEBUG_VERBOSE);
 * 
 * // Test drive
 * uft_hal_test_result_t result;
 * uft_hal_test_drive(&cfg, &result);
 * printf("RPM: %.1f, Index: %s\n", result.rpm, 
 *        result.index_detected ? "OK" : "FAIL");
 * @endcode
 */

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* UFT_HARDWARE_GUIDE_H */
