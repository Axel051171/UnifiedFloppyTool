/**
 * @file uft_version.h
 * @brief UnifiedFloppyTool Version Information
 *
 * GENERATED FILE — do not edit by hand. Regenerate via:
 *   python3 scripts/generate_version_header.py
 * Source of truth: VERSION.txt at the repository root.
 *
 * Every macro is #ifndef-guarded so a build-system override
 * (-DUFT_VERSION_STRING="...") wins over the checked-in value.
 */
#ifndef UFT_VERSION_H
#define UFT_VERSION_H

#ifndef UFT_VERSION_MAJOR
#define UFT_VERSION_MAJOR 4
#endif
#ifndef UFT_VERSION_MINOR
#define UFT_VERSION_MINOR 1
#endif
#ifndef UFT_VERSION_PATCH
#define UFT_VERSION_PATCH 3
#endif

#ifndef UFT_VERSION_STRING
#define UFT_VERSION_STRING "4.1.3"
#endif
#ifndef UFT_VERSION_FULL
#define UFT_VERSION_FULL "UnifiedFloppyTool v4.1.3"
#endif

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
