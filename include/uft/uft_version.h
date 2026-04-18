/**
 * @file uft_version.h
 * @brief UnifiedFloppyTool Version Information
 *
 * Canonical version source: UFT_VERSION.txt at the repository root
 * (read directly by CMake). Keep the values below in sync — the
 * consistency-auditor + must-fix-hunter agents check that on every
 * run and block mismatches.
 */
#ifndef UFT_VERSION_H
#define UFT_VERSION_H

#define UFT_VERSION_MAJOR 4
#define UFT_VERSION_MINOR 1
#define UFT_VERSION_PATCH 3

#define UFT_VERSION_STRING "4.1.3"
#define UFT_VERSION_FULL "UnifiedFloppyTool v4.1.3"

#ifndef UFT_BUILD_DATE
#define UFT_BUILD_DATE __DATE__
#endif

#ifndef UFT_BUILD_TIME
#define UFT_BUILD_TIME __TIME__
#endif

#ifndef UFT_GIT_HASH
#define UFT_GIT_HASH "unknown"
#endif

#endif /* UFT_VERSION_H */
