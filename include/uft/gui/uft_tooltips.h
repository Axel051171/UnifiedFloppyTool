/**
 * @file uft_tooltips.h
 * @brief GUI Tooltip Strings
 * 
 * P3-005: Centralized tooltip text for all GUI elements
 * 
 * Usage in Qt:
 *   widget->setToolTip(tr(UFT_TIP_PLATFORM));
 */

#ifndef UFT_TOOLTIPS_H
#define UFT_TOOLTIPS_H

/* ============================================================================
 * Simple Tab
 * ============================================================================ */

#define UFT_TIP_PLATFORM \
    "Select the computer platform for the disk.\n" \
    "This determines available presets and default settings."

#define UFT_TIP_PRESET \
    "Select a predefined configuration for common disk types.\n" \
    "Presets automatically configure track count, encoding, and format."

#define UFT_TIP_SOURCE \
    "Source disk image file or hardware device.\n" \
    "Drag and drop files here or click Browse."

#define UFT_TIP_TARGET \
    "Destination file path for the output.\n" \
    "Leave empty to auto-generate based on source name."

#define UFT_TIP_OUTPUT_FORMAT \
    "Output file format.\n" \
    "Available formats depend on the source format capabilities."

#define UFT_TIP_START \
    "Start the conversion process.\n" \
    "Progress will be shown below."

/* ============================================================================
 * Advanced Tab
 * ============================================================================ */

#define UFT_TIP_TRACK_START \
    "First track to process (0-based).\n" \
    "Set to 0 for complete disk."

#define UFT_TIP_TRACK_END \
    "Last track to process (inclusive).\n" \
    "Set to max (e.g., 79) for complete disk."

#define UFT_TIP_HEADS \
    "Number of disk sides to process.\n" \
    "1 = Single-sided, 2 = Double-sided"

#define UFT_TIP_ENCODING \
    "Disk encoding type:\n" \
    "• MFM: IBM PC, Amiga, Atari ST\n" \
    "• FM: Older 8\" and some 5.25\" disks\n" \
    "• GCR: Commodore, Apple II"

#define UFT_TIP_BITRATE \
    "Data bit rate in bits per second.\n" \
    "Common values:\n" \
    "• 250,000: DD disks\n" \
    "• 500,000: HD disks\n" \
    "• 1,000,000: ED disks"

#define UFT_TIP_RPM \
    "Disk rotation speed.\n" \
    "• 300 RPM: Most 3.5\" and 5.25\" DD drives\n" \
    "• 360 RPM: 5.25\" HD drives"

#define UFT_TIP_RETRY \
    "Enable automatic retry on read errors.\n" \
    "Increases success rate for damaged disks."

#define UFT_TIP_RETRY_COUNT \
    "Number of retry attempts per sector.\n" \
    "Higher values increase read time but improve recovery."

#define UFT_TIP_VERIFY \
    "Verify data after writing.\n" \
    "Recommended for important data."

#define UFT_TIP_PRESERVE_ERRORS \
    "Keep error information in output.\n" \
    "Useful for preservation and analysis."

/* ============================================================================
 * Flux Tab
 * ============================================================================ */

#define UFT_TIP_SAMPLE_RATE \
    "Flux sampling rate in Hz.\n" \
    "Higher rates capture more detail but increase file size.\n" \
    "Typical: 24-80 MHz"

#define UFT_TIP_REVOLUTIONS \
    "Number of disk revolutions to capture.\n" \
    "More revolutions help recover weak data.\n" \
    "Typical: 3-5 for protected disks"

#define UFT_TIP_USE_INDEX \
    "Use index pulse for rotation alignment.\n" \
    "Disable only if index sensor is faulty."

#define UFT_TIP_PLL_TYPE \
    "Phase-Locked Loop algorithm:\n" \
    "• Simple: Fast, good for clean disks\n" \
    "• Kalman: Better for noisy data\n" \
    "• Viterbi: Best quality, slowest"

#define UFT_TIP_PLL_BANDWIDTH \
    "PLL tracking bandwidth.\n" \
    "Lower = more stable, slower adaptation\n" \
    "Higher = faster adaptation, more noise"

#define UFT_TIP_WEAK_DETECT \
    "Detect weak/unstable bits.\n" \
    "Important for copy-protected disks."

#define UFT_TIP_WEAK_THRESHOLD \
    "Threshold for weak bit detection.\n" \
    "Number of differing bits across revolutions."

/* ============================================================================
 * Protection Tab
 * ============================================================================ */

#define UFT_TIP_DETECT_PROTECTION \
    "Scan disk for copy protection schemes.\n" \
    "Analyzes timing, layout, and data patterns."

#define UFT_TIP_COPY_MODE \
    "Copy mode for protected disks:\n" \
    "• Standard: Sector-level copy\n" \
    "• Flux: Full flux-level copy\n" \
    "• Smart: Auto-select based on protection"

#define UFT_TIP_PRESERVE_TIMING \
    "Preserve sector timing information.\n" \
    "Required for timing-based protection."

#define UFT_TIP_PRESERVE_WEAK_BITS \
    "Preserve weak bit regions.\n" \
    "Required for weak-bit protection."

#define UFT_TIP_MIN_REVOLUTIONS \
    "Minimum revolutions for protected copy.\n" \
    "More revolutions improve weak bit accuracy."

/* ============================================================================
 * Analysis Tab
 * ============================================================================ */

#define UFT_TIP_ANALYZE \
    "Perform full disk analysis.\n" \
    "Checks CRC, identifies format, detects errors."

#define UFT_TIP_TRACK_GRID \
    "Visual overview of all tracks.\n" \
    "Color indicates quality:\n" \
    "• Green: Good\n" \
    "• Yellow: Minor errors\n" \
    "• Red: Major errors\n" \
    "• Gray: Empty/Missing"

#define UFT_TIP_EXPORT_REPORT \
    "Save analysis report to file.\n" \
    "Formats: JSON, CSV, HTML, TXT"

/* ============================================================================
 * Hardware Tab
 * ============================================================================ */

#define UFT_TIP_CONTROLLER \
    "Select floppy controller hardware:\n" \
    "• Greaseweazle: Open-source USB\n" \
    "• FluxEngine: Cypress PSoC5\n" \
    "• KryoFlux: Professional USB\n" \
    "• FC5025: Device Side\n" \
    "• XUM1541: Commodore IEC"

#define UFT_TIP_REFRESH \
    "Scan for connected controllers.\n" \
    "Detects USB devices and serial ports."

#define UFT_TIP_DRIVE_PROFILE \
    "Drive mechanical profile:\n" \
    "• Step delay: Time between track steps\n" \
    "• Settle time: Wait after seeking\n" \
    "• RPM: Rotation speed"

#define UFT_TIP_TEST_DRIVE \
    "Test drive functionality.\n" \
    "Checks motor, index, track 0, and read."

#define UFT_TIP_STEP_DELAY \
    "Delay between step pulses in milliseconds.\n" \
    "Increase for older or sticky drives."

#define UFT_TIP_SETTLE_TIME \
    "Delay after seek before read/write.\n" \
    "Allows head vibration to settle."

/* ============================================================================
 * Batch Tab
 * ============================================================================ */

#define UFT_TIP_BATCH_FILES \
    "List of files to process.\n" \
    "Drag and drop files or use Add Files."

#define UFT_TIP_ADD_FILES \
    "Add files to batch queue.\n" \
    "Supports multiple selection."

#define UFT_TIP_REMOVE_SELECTED \
    "Remove selected files from queue."

#define UFT_TIP_CLEAR_ALL \
    "Remove all files from queue."

#define UFT_TIP_BATCH_ACTION \
    "Operation to perform on all files:\n" \
    "• Convert: Change format\n" \
    "• Analyze: Generate reports\n" \
    "• Verify: Check integrity\n" \
    "• Extract: Export files from images"

#define UFT_TIP_START_BATCH \
    "Start batch processing.\n" \
    "Progress shown per file and overall."

/* ============================================================================
 * Common
 * ============================================================================ */

#define UFT_TIP_CANCEL \
    "Cancel current operation.\n" \
    "In-progress data will be preserved if possible."

#define UFT_TIP_HELP \
    "Open documentation.\n" \
    "Press F1 for context-sensitive help."

#define UFT_TIP_SETTINGS \
    "Open application settings.\n" \
    "Configure defaults, paths, and appearance."

#endif /* UFT_TOOLTIPS_H */
