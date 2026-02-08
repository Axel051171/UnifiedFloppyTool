/**
 * @file uft_format_advisor.h
 * @brief Format Advisor - Intelligent Format Recommendations
 * 
 * LAYER SEPARATION:
 * - Diese Logik war fälschlicherweise in uft_hardware.c
 * - Jetzt korrekt in Core angesiedelt
 */

#ifndef UFT_FORMAT_ADVISOR_H
#define UFT_FORMAT_ADVISOR_H

#include "uft_types.h"
#include "uft_error.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration
typedef struct uft_unified_image uft_unified_image_t;

// ============================================================================
// Format Advice
// ============================================================================

typedef struct uft_format_advice {
    uft_format_t    recommended_format;
    int             confidence;         // 0-100
    const char*     reason;
    
    uft_format_t    alternatives[8];
    int             alternative_count;
} uft_format_advice_t;

typedef struct uft_conversion_info {
    bool            possible;
    bool            lossy;
    char            warning[256];
} uft_conversion_info_t;

// ============================================================================
// API
// ============================================================================

/**
 * @brief Empfiehlt das beste Format für ein Image
 */
uft_error_t uft_get_format_advice(const uft_unified_image_t* img,
                                   uft_format_advice_t* advice);

/**
 * @brief Prüft ob Konvertierung möglich ist
 */
bool uft_format_can_convert(uft_format_t src, uft_format_t dst,
                             uft_conversion_info_t* info);

/**
 * @brief Format-Informationen
 */
const char* uft_format_get_name(uft_format_t format);
const char* uft_format_get_extension(uft_format_t format);
bool uft_format_supports_flux(uft_format_t format);

#ifdef __cplusplus
}
#endif

#endif // UFT_FORMAT_ADVISOR_H
