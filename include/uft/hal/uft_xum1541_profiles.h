/**
 * @file uft_xum1541_profiles.h
 * @brief XUM1541 Hardware Variant Profiles Interface
 * @version 4.1.1
 */

#ifndef UFT_XUM1541_PROFILES_H
#define UFT_XUM1541_PROFILES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Hardware variant enumeration */
typedef enum {
    XUM1541_VARIANT_UNKNOWN = 0,
    XUM1541_VARIANT_ZOOMFLOPPY,      /**< Commercial ZoomFloppy */
    XUM1541_VARIANT_ZOOMFLOPPY_IEEE, /**< ZoomFloppy with IEEE-488 */
    XUM1541_VARIANT_XUM1541_II,      /**< tebl XUM1541-II with 7406 */
    XUM1541_VARIANT_XUM1541_ORIG,    /**< Original XUM1541 (Pro Micro) */
    XUM1541_VARIANT_AT90USBKEY,      /**< AT90USBKEY dev board */
    XUM1541_VARIANT_BUMBLEB,         /**< Bumble-B board */
} xum1541_variant_t;

/* Hardware profile structure */
typedef struct {
    xum1541_variant_t variant;
    const char *name;
    const char *description;
    uint16_t vendor_id;
    uint16_t product_id;
    uint16_t revision_min;
    uint16_t revision_max;
    
    bool has_parallel;
    bool has_ieee488;
    bool has_7406_inverter;
    bool has_srq_nibbling;
    bool has_activity_led;
    
    int iec_delay_us;
    int parallel_delay_us;
    
    const char *firmware_file;
    const char *firmware_url;
} xum1541_profile_t;

/**
 * @brief Get profile by firmware revision
 */
const xum1541_profile_t* xum1541_get_profile_by_revision(uint16_t revision);

/**
 * @brief Get profile by name
 */
const xum1541_profile_t* xum1541_get_profile_by_name(const char *name);

/**
 * @brief List all profiles to buffer
 */
int xum1541_list_profiles(char *buffer, size_t buffer_size);

/**
 * @brief Print profile to stdout
 */
void xum1541_print_profile(const xum1541_profile_t *profile);

#ifdef __cplusplus
}
#endif

#endif /* UFT_XUM1541_PROFILES_H */
