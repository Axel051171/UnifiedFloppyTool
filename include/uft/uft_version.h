/**
 * @file uft_version.h
 * @brief UnifiedFloppyTool Version Information
 * 
 * This file is auto-generated and should match CMakeLists.txt VERSION.
 * CI will verify consistency.
 */
#ifndef UFT_VERSION_H
#define UFT_VERSION_H

#define UFT_VERSION_MAJOR 3
#define UFT_VERSION_MINOR 7
#define UFT_VERSION_PATCH 0

#define UFT_VERSION_STRING "3.7.3"
#define UFT_VERSION_FULL "UnifiedFloppyTool v3.7.3"

/* Build info - set by CI or build system */
#ifndef UFT_BUILD_DATE
#define UFT_BUILD_DATE __DATE__
#endif

#ifndef UFT_BUILD_TIME
#define UFT_BUILD_TIME __TIME__
#endif

/* Git info - set by CI */
#ifndef UFT_GIT_HASH
#define UFT_GIT_HASH "unknown"
#endif

#ifndef UFT_GIT_BRANCH
#define UFT_GIT_BRANCH "unknown"
#endif

/* Platform detection for version string */
#if defined(_WIN32)
#define UFT_PLATFORM_NAME "Windows"
#elif defined(__APPLE__)
#define UFT_PLATFORM_NAME "macOS"
#elif defined(__linux__)
#define UFT_PLATFORM_NAME "Linux"
#else
#define UFT_PLATFORM_NAME "Unknown"
#endif

/* Architecture detection */
#if defined(__x86_64__) || defined(_M_X64)
#define UFT_ARCH_NAME "x64"
#elif defined(__i386__) || defined(_M_IX86)
#define UFT_ARCH_NAME "x86"
#elif defined(__aarch64__) || defined(_M_ARM64)
#define UFT_ARCH_NAME "ARM64"
#elif defined(__arm__) || defined(_M_ARM)
#define UFT_ARCH_NAME "ARM"
#else
#define UFT_ARCH_NAME "unknown"
#endif

/**
 * @brief Get full version string with platform info
 * @return Static string like "UnifiedFloppyTool v3.7.3 (Linux x64)"
 */
static inline const char* uft_version_full(void) {
    return UFT_VERSION_FULL " (" UFT_PLATFORM_NAME " " UFT_ARCH_NAME ")";
}

/**
 * @brief Get version as integer for comparisons
 * @return Version as (MAJOR * 10000 + MINOR * 100 + PATCH)
 */
static inline int uft_version_int(void) {
    return UFT_VERSION_MAJOR * 10000 + UFT_VERSION_MINOR * 100 + UFT_VERSION_PATCH;
}

#endif /* UFT_VERSION_H */
