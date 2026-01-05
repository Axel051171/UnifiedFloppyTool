/**
 * @file uft_atarist_protection.c
 * @brief Atari ST Protection Detection Implementation
 * 
 * Generic Atari ST copy protection detection framework.
 * 
 * @version 1.0.0
 * @date 2026-01-04
 * @author UFT Team
 * 
 * SPDX-License-Identifier: MIT
 */

#include "uft/protection/uft_atarist_protection.h"
#include "uft/protection/uft_atarist_copylock.h"
#include "uft/protection/uft_atarist_dec0de.h"
#include <stdio.h>
#include <stdlib.h>

/*===========================================================================
 * Detection Framework
 *===========================================================================*/

/**
 * @brief Initialize Atari ST protection result
 */
void uft_atarist_prot_init(uft_atarist_prot_result_t *result)
{
    if (!result) return;
    memset(result, 0, sizeof(*result));
    result->type = UFT_ATARIST_PROT_NONE;
}

/**
 * @brief Detect any Atari ST protection
 */
int uft_atarist_prot_detect(const uint8_t *data, size_t data_len,
                             uft_atarist_prot_result_t *result)
{
    if (!data || !result) {
        return -1;
    }
    
    uft_atarist_prot_init(result);
    
    /* Try CopyLock first (most common) */
    uft_copylock_st_result_t cl_result;
    if (uft_copylock_st_detect(data, data_len, &cl_result)) {
        result->detected = true;
        result->type = UFT_ATARIST_PROT_COPYLOCK;
        result->series = (cl_result.series == UFT_COPYLOCK_SERIES_1_1988) ? 1 : 2;
        result->variant = cl_result.variant;
        snprintf(result->name, sizeof(result->name), "%s", cl_result.name);
        return 0;
    }
    
    /* Try other protections via dec0de */
    uft_dec0de_result_t dec_result;
    if (uft_dec0de_detect(data, data_len, &dec_result) == 0 && dec_result.detected) {
        result->detected = true;
        switch (dec_result.prot_type) {
            case UFT_DEC0DE_PROT_ROBN88:
            case UFT_DEC0DE_PROT_ROBN89:
                result->type = UFT_ATARIST_PROT_COPYLOCK;
                break;
            case UFT_DEC0DE_PROT_ANTIBITOS:
                result->type = UFT_ATARIST_PROT_ANTIBITOS;
                break;
            case UFT_DEC0DE_PROT_TOXIC:
                result->type = UFT_ATARIST_PROT_TOXIC;
                break;
            case UFT_DEC0DE_PROT_COOPER:
                result->type = UFT_ATARIST_PROT_COOPER;
                break;
            case UFT_DEC0DE_PROT_ZIPPY:
                result->type = UFT_ATARIST_PROT_ZIPPY;
                break;
            case UFT_DEC0DE_PROT_LOCKOMATIC:
                result->type = UFT_ATARIST_PROT_LOCKOMATIC;
                break;
            case UFT_DEC0DE_PROT_CID:
                result->type = UFT_ATARIST_PROT_CID;
                break;
            default:
                result->type = UFT_ATARIST_PROT_UNKNOWN;
                break;
        }
        result->variant = dec_result.variant;
        snprintf(result->name, sizeof(result->name), "%s", dec_result.name);
        return 0;
    }
    
    return 1;  /* No protection detected */
}

/**
 * @brief Get protection type name
 */
const char* uft_atarist_prot_type_name(uft_atarist_prot_type_t type)
{
    switch (type) {
        case UFT_ATARIST_PROT_NONE:       return "None";
        case UFT_ATARIST_PROT_COPYLOCK:   return "Rob Northen CopyLock";
        case UFT_ATARIST_PROT_ANTIBITOS:  return "Illegal Anti-bitos";
        case UFT_ATARIST_PROT_TOXIC:      return "NTM/Cameo Toxic Packer";
        case UFT_ATARIST_PROT_COOPER:     return "Cameo Cooper";
        case UFT_ATARIST_PROT_ZIPPY:      return "Zippy Little Protection";
        case UFT_ATARIST_PROT_LOCKOMATIC: return "Yoda Lock-o-matic";
        case UFT_ATARIST_PROT_CID:        return "CID Encrypter";
        case UFT_ATARIST_PROT_RAL:        return "R.AL Protection";
        case UFT_ATARIST_PROT_SLY:        return "Orion Sly Packer";
        case UFT_ATARIST_PROT_MACRODOS:   return "MacroDOS";
        default:                          return "Unknown";
    }
}

/**
 * @brief Print detection result
 */
void uft_atarist_prot_print(FILE *out, const uft_atarist_prot_result_t *result)
{
    if (!out || !result) return;
    
    fprintf(out, "=== Atari ST Protection Detection ===\n");
    fprintf(out, "Detected:   %s\n", result->detected ? "YES" : "NO");
    
    if (!result->detected) return;
    
    fprintf(out, "Type:       %s\n", uft_atarist_prot_type_name(result->type));
    fprintf(out, "Name:       %s\n", result->name);
    
    if (result->series) {
        fprintf(out, "Series:     %d\n", result->series);
    }
    if (result->variant) {
        fprintf(out, "Variant:    %c\n", result->variant);
    }
}
