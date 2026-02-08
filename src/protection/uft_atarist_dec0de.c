/**
 * @file uft_atarist_dec0de.c
 * @brief Atari ST Copy Protection Decoder Implementation
 * 
 * Based on dec0de by Orion ^ The Replicants.
 * 
 * @version 2.0.0
 * @date 2025-01-08
 */

#include "uft/protection/uft_atarist_dec0de.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*===========================================================================
 * Local Types
 *===========================================================================*/

/** Protection types detected by dec0de */
typedef enum {
    UFT_DEC0DE_PROT_NONE = 0,
    UFT_DEC0DE_PROT_ROBN88,         /**< Rob Northen 1988 */
    UFT_DEC0DE_PROT_ROBN89,         /**< Rob Northen 1989+ */
    UFT_DEC0DE_PROT_ANTIBITOS,      /**< Illegal Anti-bitos */
    UFT_DEC0DE_PROT_TOXIC,          /**< NTM/Cameo Toxic Packer */
    UFT_DEC0DE_PROT_COOPER,         /**< Cameo Cooper */
    UFT_DEC0DE_PROT_ZIPPY,          /**< Zippy Little Protection */
    UFT_DEC0DE_PROT_LOCKOMATIC,     /**< Yoda Lock-o-matic */
    UFT_DEC0DE_PROT_CID,            /**< CID Encrypter */
    UFT_DEC0DE_PROT_RAL,            /**< R.AL Protection */
    UFT_DEC0DE_PROT_SLY,            /**< Orion Sly Packer */
} uft_dec0de_prot_type_t;

/** Dec0de detection result */
typedef struct {
    bool detected;
    uft_dec0de_prot_type_t prot_type;
    size_t offset;
    char name[64];
    char variant;
    double confidence;
} uft_dec0de_result_t;

/*===========================================================================
 * Helper Functions
 *===========================================================================*/

static inline uint16_t read16_be(const uint8_t *p) {
    return ((uint16_t)p[0] << 8) | p[1];
}

static inline uint32_t read32_be(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

static inline void write16_be(uint8_t *p, uint16_t val) {
    p[0] = (val >> 8) & 0xFF;
    p[1] = val & 0xFF;
}

static inline void write32_be(uint8_t *p, uint32_t val) {
    p[0] = (val >> 24) & 0xFF;
    p[1] = (val >> 16) & 0xFF;
    p[2] = (val >> 8) & 0xFF;
    p[3] = val & 0xFF;
}

/*===========================================================================
 * GEMDOS Program Handling
 *===========================================================================*/

int uft_gemdos_parse_header(const uint8_t *data, size_t data_len,
                             uft_gemdos_header_t *hdr) {
    if (!data || !hdr || data_len < UFT_GEMDOS_HEADER_SIZE) {
        return -1;
    }
    
    /* Check magic: 0x601A or 0x601B */
    uint16_t magic = read16_be(data);
    if (magic != UFT_GEMDOS_MAGIC && magic != 0x601B) {
        return 1;  /* Not a GEMDOS program */
    }
    
    /* Copy header directly (already in correct format) */
    memcpy(hdr, data, UFT_GEMDOS_HEADER_SIZE);
    
    return 0;
}

/* uft_gemdos_is_valid is defined inline in header */

size_t uft_gemdos_total_size_from_header(const uft_gemdos_header_t *hdr) {
    if (!hdr) return 0;
    
    return UFT_GEMDOS_HEADER_SIZE +
           read32_be(hdr->ph_tlen) +
           read32_be(hdr->ph_dlen) +
           read32_be(hdr->ph_slen);
}

/*===========================================================================
 * Protection Patterns
 *===========================================================================*/

/* Rob Northen Copylock 1988 signature */
static const uint8_t ROBN88_PATTERN[] = {
    0x4E, 0x75,             /* RTS */
    0x41, 0xFA,             /* LEA (PC,d16),A0 */
};

/* Rob Northen Copylock 1989 signature */
static const uint8_t ROBN89_PATTERN[] = {
    0x4E, 0x71,             /* NOP */
    0x4E, 0x71,             /* NOP */
    0x41, 0xFA,             /* LEA (PC,d16),A0 */
};

/* Antibitos signature */
static const uint8_t ANTIBITOS_PATTERN[] = {
    0x48, 0xE7, 0xFF, 0xFE, /* MOVEM.L D0-D7/A0-A6,-(SP) */
    0x41, 0xF9,             /* LEA xxx,A0 */
};

/* NTM/Cameo Toxic Packer */
static const uint8_t TOXIC_PATTERN[] = {
    0x60, 0x00,             /* BRA.W */
    0x00, 0x14,             /* offset */
    'T', 'O', 'X', 'I', 'C',
};

/*===========================================================================
 * Pattern Matching
 *===========================================================================*/

static bool match_pattern(const uint8_t *data, size_t len,
                          const uint8_t *pattern, size_t plen,
                          size_t offset) {
    if (offset + plen > len) return false;
    return memcmp(data + offset, pattern, plen) == 0;
}

static bool search_pattern(const uint8_t *data, size_t len,
                           const uint8_t *pattern, size_t plen,
                           size_t *found_offset) {
    if (plen > len) return false;
    
    for (size_t i = 0; i + plen <= len; i++) {
        if (memcmp(data + i, pattern, plen) == 0) {
            if (found_offset) *found_offset = i;
            return true;
        }
    }
    return false;
}

/*===========================================================================
 * Detection Functions
 *===========================================================================*/

int uft_dec0de_detect(const uint8_t *data, size_t data_len,
                       uft_dec0de_result_t *result) {
    if (!data || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    
    /* Check for GEMDOS header */
    uft_gemdos_header_t hdr;
    bool is_gemdos = (uft_gemdos_parse_header(data, data_len, &hdr) == 0);
    
    size_t search_start = is_gemdos ? UFT_GEMDOS_HEADER_SIZE : 0;
    const uint8_t *search_data = data + search_start;
    size_t search_len = data_len - search_start;
    
    /* Try Rob Northen 1988 */
    size_t offset;
    if (search_pattern(search_data, search_len, ROBN88_PATTERN,
                       sizeof(ROBN88_PATTERN), &offset)) {
        result->detected = true;
        result->prot_type = UFT_DEC0DE_PROT_ROBN88;
        result->offset = search_start + offset;
        strncpy(result->name, "Rob Northen Copylock (1988)", sizeof(result->name) - 1);
        result->confidence = 0.9;
        return 0;
    }
    
    /* Try Rob Northen 1989 */
    if (search_pattern(search_data, search_len, ROBN89_PATTERN,
                       sizeof(ROBN89_PATTERN), &offset)) {
        result->detected = true;
        result->prot_type = UFT_DEC0DE_PROT_ROBN89;
        result->offset = search_start + offset;
        strncpy(result->name, "Rob Northen Copylock (1989+)", sizeof(result->name) - 1);
        result->confidence = 0.9;
        return 0;
    }
    
    /* Try Antibitos */
    if (search_pattern(search_data, search_len, ANTIBITOS_PATTERN,
                       sizeof(ANTIBITOS_PATTERN), &offset)) {
        result->detected = true;
        result->prot_type = UFT_DEC0DE_PROT_ANTIBITOS;
        result->offset = search_start + offset;
        strncpy(result->name, "Illegal Anti-bitos", sizeof(result->name) - 1);
        result->confidence = 0.85;
        return 0;
    }
    
    /* Try Toxic Packer */
    if (search_pattern(search_data, search_len, TOXIC_PATTERN,
                       sizeof(TOXIC_PATTERN), &offset)) {
        result->detected = true;
        result->prot_type = UFT_DEC0DE_PROT_TOXIC;
        result->offset = search_start + offset;
        strncpy(result->name, "NTM/Cameo Toxic Packer", sizeof(result->name) - 1);
        result->confidence = 0.95;
        return 0;
    }
    
    return 1;  /* No protection detected */
}

const char* uft_dec0de_type_name(uft_dec0de_prot_type_t type) {
    switch (type) {
        case UFT_DEC0DE_PROT_NONE:      return "None";
        case UFT_DEC0DE_PROT_ROBN88:    return "Rob Northen Copylock (1988)";
        case UFT_DEC0DE_PROT_ROBN89:    return "Rob Northen Copylock (1989+)";
        case UFT_DEC0DE_PROT_ANTIBITOS: return "Illegal Anti-bitos";
        case UFT_DEC0DE_PROT_TOXIC:     return "NTM/Cameo Toxic Packer";
        case UFT_DEC0DE_PROT_COOPER:    return "Cameo Cooper";
        case UFT_DEC0DE_PROT_ZIPPY:     return "Zippy Little Protection";
        case UFT_DEC0DE_PROT_LOCKOMATIC:return "Yoda Lock-o-matic";
        case UFT_DEC0DE_PROT_CID:       return "CID Encrypter";
        case UFT_DEC0DE_PROT_RAL:       return "R.AL Protection";
        case UFT_DEC0DE_PROT_SLY:       return "Orion Sly Packer";
        default:                        return "Unknown";
    }
}

void uft_dec0de_print_result(FILE *out, const uft_dec0de_result_t *result) {
    if (!out || !result) return;
    
    fprintf(out, "=== Dec0de Detection Result ===\n");
    fprintf(out, "Detected:   %s\n", result->detected ? "YES" : "NO");
    
    if (!result->detected) return;
    
    fprintf(out, "Protection: %s\n", result->name);
    fprintf(out, "Type:       %s\n", uft_dec0de_type_name(result->prot_type));
    fprintf(out, "Offset:     0x%08zX\n", result->offset);
    fprintf(out, "Confidence: %.0f%%\n", result->confidence * 100);
    
    if (result->variant) {
        fprintf(out, "Variant:    %c\n", result->variant);
    }
}
