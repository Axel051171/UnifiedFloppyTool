/**
 * @file uft_version.h
 * @brief UnifiedFloppyTool Version Information
 */
#ifndef UFT_VERSION_H
#define UFT_VERSION_H

#define UFT_VERSION_MAJOR 4
#define UFT_VERSION_MINOR 1
#define UFT_VERSION_PATCH 0

#define UFT_VERSION_STRING "4.1.0"
#define UFT_VERSION_FULL "UnifiedFloppyTool v4.1.0"

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
