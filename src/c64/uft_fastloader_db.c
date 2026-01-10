/**
 * @file uft_fastloader_db.c
 * @brief C64/CBM Fastloader & Copy Protection Signature Database
 * 
 * Contains known signatures for fastloaders, copy programs, and protection.
 * Patterns derived from analysis of original software.
 * 
 * @author UFT Team
 * @version 1.0.0
 */

#include "uft/c64/uft_fastloader_db.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* ============================================================================
 * Signature Database
 * 
 * Each entry contains:
 * - Name and version
 * - Category
 * - Byte patterns with optional masks
 * - Load address and size hints
 * - Additional metadata
 * ============================================================================ */

/* Helper macros for pattern definition */
#define PAT(off, ...) { .offset = off, .bytes = { __VA_ARGS__ }, \
                        .mask = {0}, .length = 0 }
#define PATM(off, len, byt, msk) { .offset = off, .bytes = byt, \
                                   .mask = msk, .length = len }

static const uft_sig_entry_t SIGNATURE_DB[] = {
    /* ========================================================================
     * COMMERCIAL FASTLOADERS
     * ======================================================================== */
    
    {
        .name = "Epyx FastLoad",
        .version = "",
        .category = UFT_SIG_CAT_FASTLOADER,
        .patterns = {
            { .offset = 0, .bytes = {0x78, 0xA2, 0x00, 0xBD}, .length = 4 },
        },
        .pattern_count = 1,
        .patterns_required = 1,
        .load_addr_min = 0xC000,
        .load_addr_max = 0xCFFF,
        .description = "Epyx FastLoad cartridge loader",
        .publisher = "Epyx",
        .year = 1984,
        .score_weight = 10
    },
    
    {
        .name = "Novaload",
        .version = "",
        .category = UFT_SIG_CAT_FASTLOADER,
        .patterns = {
            { .offset = 0xFFFF, .bytes = {0x4E, 0x4F, 0x56, 0x41}, .length = 4 }, /* "NOVA" */
        },
        .pattern_count = 1,
        .patterns_required = 1,
        .description = "Novaload tape/disk turbo",
        .publisher = "Visiload",
        .year = 1985,
        .score_weight = 8
    },
    
    {
        .name = "Pavloda",
        .version = "",
        .category = UFT_SIG_CAT_FASTLOADER,
        .patterns = {
            { .offset = 0xFFFF, .bytes = {0x50, 0x41, 0x56, 0x4C}, .length = 4 }, /* "PAVL" */
        },
        .pattern_count = 1,
        .patterns_required = 1,
        .description = "Pavloda loader system",
        .year = 1986,
        .score_weight = 8
    },
    
    {
        .name = "Cyberload",
        .version = "",
        .category = UFT_SIG_CAT_FASTLOADER,
        .patterns = {
            { .offset = 0xFFFF, .bytes = {0x43, 0x59, 0x42, 0x45, 0x52}, .length = 5 }, /* "CYBER" */
        },
        .pattern_count = 1,
        .patterns_required = 1,
        .description = "Cyberload tape turbo",
        .year = 1987,
        .score_weight = 8
    },
    
    {
        .name = "Burner",
        .version = "",
        .category = UFT_SIG_CAT_FASTLOADER,
        .patterns = {
            { .offset = 0xFFFF, .bytes = {0x42, 0x55, 0x52, 0x4E, 0x45, 0x52}, .length = 6 },
        },
        .pattern_count = 1,
        .patterns_required = 1,
        .description = "Burner fast loader",
        .score_weight = 7
    },
    
    {
        .name = "Freeload",
        .version = "",
        .category = UFT_SIG_CAT_FASTLOADER,
        .patterns = {
            { .offset = 0xFFFF, .bytes = {0x46, 0x52, 0x45, 0x45, 0x4C, 0x4F, 0x41, 0x44}, .length = 8 },
        },
        .pattern_count = 1,
        .patterns_required = 1,
        .description = "Freeload fast loader",
        .score_weight = 7
    },
    
    /* ========================================================================
     * NIBBLER / BIT COPIERS
     * ======================================================================== */
    
    {
        .name = "Turbo Nibbler",
        .version = "1.0",
        .category = UFT_SIG_CAT_NIBBLER,
        .patterns = {
            { .offset = 0xFFFF, .bytes = {0x54, 0x55, 0x52, 0x42, 0x4F, 0x20, 0x4E, 0x49, 
                                          0x42, 0x42, 0x4C, 0x45, 0x52, 0x20, 0x56, 0x31}, .length = 16 },
        },
        .pattern_count = 1,
        .patterns_required = 1,
        .description = "Turbo Nibbler V1.0 - Halftrack copier",
        .year = 1987,
        .score_weight = 15
    },
    
    {
        .name = "Turbo Nibbler",
        .version = "2.x",
        .category = UFT_SIG_CAT_NIBBLER,
        .patterns = {
            { .offset = 0xFFFF, .bytes = {0x54, 0x55, 0x52, 0x42, 0x4F, 0x20, 0x4E, 0x49,
                                          0x42, 0x42, 0x4C, 0x45, 0x52, 0x20, 0x56, 0x32}, .length = 16 },
        },
        .pattern_count = 1,
        .patterns_required = 1,
        .description = "Turbo Nibbler V2.x - Enhanced halftrack copier",
        .year = 1988,
        .score_weight = 15
    },
    
    {
        .name = "Turbo Nibbler",
        .version = "4.0",
        .category = UFT_SIG_CAT_NIBBLER,
        .patterns = {
            { .offset = 0xFFFF, .bytes = {0x52, 0x42, 0x4F, 0x20, 0x4E, 0x49, 0x42, 0x42,
                                          0x4C, 0x45, 0x52, 0x20, 0x56, 0x34}, .length = 14 },
        },
        .pattern_count = 1,
        .patterns_required = 1,
        .description = "Turbo Nibbler V4.0 - Professional nibbler",
        .year = 1990,
        .score_weight = 15
    },
    
    {
        .name = "Burst Nibbler",
        .version = "",
        .category = UFT_SIG_CAT_NIBBLER,
        .patterns = {
            { .offset = 0xFFFF, .bytes = {0x42, 0x55, 0x52, 0x53, 0x54, 0x4E, 0x49, 0x42}, .length = 8 },
        },
        .pattern_count = 1,
        .patterns_required = 1,
        .description = "Burst Nibbler - 1571 burst mode copier",
        .score_weight = 15
    },
    
    {
        .name = "Super Nibbler",
        .version = "",
        .category = UFT_SIG_CAT_NIBBLER,
        .patterns = {
            { .offset = 0xFFFF, .bytes = {0x53, 0x55, 0x50, 0x45, 0x52, 0x20, 0x4E, 0x49,
                                          0x42, 0x42, 0x4C, 0x45, 0x52}, .length = 13 },
        },
        .pattern_count = 1,
        .patterns_required = 1,
        .description = "Super Nibbler bit copier",
        .score_weight = 14
    },
    
    {
        .name = "Maverick",
        .version = "",
        .category = UFT_SIG_CAT_NIBBLER,
        .patterns = {
            { .offset = 0xFFFF, .bytes = {0x4D, 0x41, 0x56, 0x45, 0x52, 0x49, 0x43, 0x4B}, .length = 8 },
        },
        .pattern_count = 1,
        .patterns_required = 1,
        .description = "Maverick copy system",
        .publisher = "Kracker Jax",
        .score_weight = 14
    },
    
    /* ========================================================================
     * DISK COPIERS
     * ======================================================================== */
    
    {
        .name = "Turbo Copy",
        .version = "",
        .category = UFT_SIG_CAT_COPIER,
        .patterns = {
            { .offset = 0xFFFF, .bytes = {0x54, 0x55, 0x52, 0x42, 0x4F, 0x43, 0x4F, 0x50, 0x59}, .length = 9 },
        },
        .pattern_count = 1,
        .patterns_required = 1,
        .description = "Turbo Copy fast disk copier",
        .score_weight = 10
    },
    
    {
        .name = "FCopy",
        .version = "",
        .category = UFT_SIG_CAT_COPIER,
        .patterns = {
            { .offset = 0xFFFF, .bytes = {0x46, 0x43, 0x4F, 0x50, 0x59}, .length = 5 },
        },
        .pattern_count = 1,
        .patterns_required = 1,
        .description = "FCopy fast copier series",
        .score_weight = 8
    },
    
    {
        .name = "Fast Hack'em",
        .version = "",
        .category = UFT_SIG_CAT_COPIER,
        .patterns = {
            { .offset = 0xFFFF, .bytes = {0x46, 0x41, 0x53, 0x54, 0x20, 0x48, 0x41, 0x43,
                                          0x4B, 0x27, 0x45, 0x4D}, .length = 12 },
        },
        .pattern_count = 1,
        .patterns_required = 1,
        .description = "Fast Hack'em copy utility",
        .publisher = "Basement Boys",
        .score_weight = 10
    },
    
    {
        .name = "21 Second Copy",
        .version = "",
        .category = UFT_SIG_CAT_COPIER,
        .patterns = {
            { .offset = 0xFFFF, .bytes = {0x32, 0x31, 0x20, 0x53, 0x45, 0x43}, .length = 6 },
        },
        .pattern_count = 1,
        .patterns_required = 1,
        .description = "21 Second Copy - RAM expander copier",
        .score_weight = 9
    },
    
    {
        .name = "Dual Copy",
        .version = "",
        .category = UFT_SIG_CAT_COPIER,
        .patterns = {
            { .offset = 0xFFFF, .bytes = {0x44, 0x55, 0x41, 0x4C, 0x20, 0x43, 0x4F, 0x50, 0x59}, .length = 9 },
        },
        .pattern_count = 1,
        .patterns_required = 1,
        .description = "Dual Copy - dual drive copier",
        .score_weight = 8
    },
    
    {
        .name = "Speed Copy",
        .version = "",
        .category = UFT_SIG_CAT_COPIER,
        .patterns = {
            { .offset = 0xFFFF, .bytes = {0x53, 0x50, 0x45, 0x45, 0x44, 0x43, 0x4F, 0x50, 0x59}, .length = 9 },
        },
        .pattern_count = 1,
        .patterns_required = 1,
        .description = "Speed Copy disk copier",
        .score_weight = 8
    },
    
    /* ========================================================================
     * COPY PROTECTION SCHEMES
     * ======================================================================== */
    
    {
        .name = "V-Max",
        .version = "",
        .category = UFT_SIG_CAT_PROTECTION,
        .patterns = {
            { .offset = 0xFFFF, .bytes = {0x56, 0x2D, 0x4D, 0x41, 0x58}, .length = 5 },
        },
        .pattern_count = 1,
        .patterns_required = 1,
        .description = "V-Max copy protection",
        .publisher = "Vorpal",
        .year = 1985,
        .score_weight = 12
    },
    
    {
        .name = "Rapidlok",
        .version = "",
        .category = UFT_SIG_CAT_PROTECTION,
        .patterns = {
            { .offset = 0xFFFF, .bytes = {0x52, 0x41, 0x50, 0x49, 0x44, 0x4C, 0x4F, 0x4B}, .length = 8 },
        },
        .pattern_count = 1,
        .patterns_required = 1,
        .description = "Rapidlok protection system",
        .year = 1986,
        .score_weight = 12
    },
    
    {
        .name = "Rob Northen Copylock",
        .version = "",
        .category = UFT_SIG_CAT_PROTECTION,
        .patterns = {
            /* M-W drive code signature */
            { .offset = 0xFFFF, .bytes = {0x4D, 0x2D, 0x57}, .length = 3 },
        },
        .pattern_count = 1,
        .patterns_required = 1,
        .description = "Rob Northen Copylock protection",
        .score_weight = 10
    },
    
    /* ========================================================================
     * DOS EXTENSIONS / SPEEDERS
     * ======================================================================== */
    
    {
        .name = "JiffyDOS",
        .version = "",
        .category = UFT_SIG_CAT_DOS,
        .patterns = {
            { .offset = 0xFFFF, .bytes = {0x4A, 0x49, 0x46, 0x46, 0x59, 0x44, 0x4F, 0x53}, .length = 8 },
        },
        .pattern_count = 1,
        .patterns_required = 1,
        .description = "JiffyDOS ROM replacement",
        .score_weight = 10
    },
    
    {
        .name = "Dolphin DOS",
        .version = "",
        .category = UFT_SIG_CAT_DOS,
        .patterns = {
            { .offset = 0xFFFF, .bytes = {0x44, 0x4F, 0x4C, 0x50, 0x48, 0x49, 0x4E}, .length = 7 },
        },
        .pattern_count = 1,
        .patterns_required = 1,
        .description = "Dolphin DOS speeder",
        .score_weight = 10
    },
    
    {
        .name = "SpeedDOS",
        .version = "",
        .category = UFT_SIG_CAT_DOS,
        .patterns = {
            { .offset = 0xFFFF, .bytes = {0x53, 0x50, 0x45, 0x45, 0x44, 0x44, 0x4F, 0x53}, .length = 8 },
        },
        .pattern_count = 1,
        .patterns_required = 1,
        .description = "SpeedDOS Plus",
        .score_weight = 10
    },
    
    {
        .name = "Professional DOS",
        .version = "",
        .category = UFT_SIG_CAT_DOS,
        .patterns = {
            { .offset = 0xFFFF, .bytes = {0x50, 0x52, 0x4F, 0x46, 0x49, 0x2D, 0x44, 0x4F, 0x53}, .length = 9 },
        },
        .pattern_count = 1,
        .patterns_required = 1,
        .description = "Professional DOS ROM",
        .score_weight = 10
    },
    
    /* ========================================================================
     * DRIVE SPEEDER ROUTINES (code signatures)
     * ======================================================================== */
    
    {
        .name = "GCR Burst Read",
        .version = "",
        .category = UFT_SIG_CAT_SPEEDER,
        .patterns = {
            /* Common burst read setup: LDA #$00, STA $1800 */
            { .offset = 0xFFFF, .bytes = {0xA9, 0x00, 0x8D, 0x00, 0x18}, .length = 5 },
        },
        .pattern_count = 1,
        .patterns_required = 1,
        .description = "GCR burst read routine",
        .score_weight = 5
    },
    
    {
        .name = "Fast Serial",
        .version = "",
        .category = UFT_SIG_CAT_SPEEDER,
        .patterns = {
            /* Serial port fast transfer */
            { .offset = 0xFFFF, .bytes = {0xAD, 0x00, 0xDD, 0x29, 0x40}, .length = 5 },
        },
        .pattern_count = 1,
        .patterns_required = 1,
        .description = "Fast serial transfer routine",
        .score_weight = 5
    },
    
    /* Sentinel */
    { .name = "", .category = UFT_SIG_CAT_UNKNOWN }
};

/* ============================================================================
 * Database Access Functions
 * ============================================================================ */

size_t uft_sig_db_size(void)
{
    size_t count = 0;
    while (SIGNATURE_DB[count].name[0] != '\0') {
        count++;
    }
    return count;
}

const uft_sig_entry_t *uft_sig_db_get(size_t index)
{
    if (index >= uft_sig_db_size()) return NULL;
    return &SIGNATURE_DB[index];
}

size_t uft_sig_db_find_category(uft_sig_category_t cat,
                                 const uft_sig_entry_t **entries,
                                 size_t max_entries)
{
    if (!entries || max_entries == 0) return 0;
    
    size_t found = 0;
    for (size_t i = 0; i < uft_sig_db_size() && found < max_entries; i++) {
        if (SIGNATURE_DB[i].category == cat) {
            entries[found++] = &SIGNATURE_DB[i];
        }
    }
    return found;
}

const uft_sig_entry_t *uft_sig_db_find_name(const char *name)
{
    if (!name) return NULL;
    
    for (size_t i = 0; i < uft_sig_db_size(); i++) {
        if (strcasecmp(SIGNATURE_DB[i].name, name) == 0) {
            return &SIGNATURE_DB[i];
        }
    }
    return NULL;
}

/* ============================================================================
 * Pattern Matching
 * ============================================================================ */

static int pattern_match(const uint8_t *data, size_t size,
                         const uft_sig_pattern_t *pat, uint32_t *match_off)
{
    if (pat->length == 0) return 0;
    
    /* Search through data for pattern */
    size_t search_end = (size > pat->length) ? size - pat->length : 0;
    
    for (size_t i = 0; i <= search_end; i++) {
        int match = 1;
        
        for (uint8_t j = 0; j < pat->length && match; j++) {
            uint8_t mask = (pat->mask[j] != 0) ? pat->mask[j] : 0xFF;
            if ((data[i + j] & mask) != (pat->bytes[j] & mask)) {
                match = 0;
            }
        }
        
        if (match) {
            if (match_off) *match_off = (uint32_t)i;
            return 1;
        }
    }
    
    return 0;
}

int uft_sig_match_entry(const uint8_t *data, size_t size, uint16_t load_addr,
                        const uft_sig_entry_t *entry, uft_sig_match_t *match)
{
    if (!data || !entry || !match) return 0;
    
    memset(match, 0, sizeof(*match));
    match->entry = entry;
    
    /* Check load address range */
    if (entry->load_addr_min != 0 || entry->load_addr_max != 0) {
        if (load_addr < entry->load_addr_min || load_addr > entry->load_addr_max) {
            return 0;
        }
    }
    
    /* Check size range */
    if (entry->size_min != 0 || entry->size_max != 0) {
        if (size < entry->size_min || size > entry->size_max) {
            return 0;
        }
    }
    
    /* Match patterns */
    int patterns_matched = 0;
    uint32_t first_match = 0;
    
    for (uint8_t i = 0; i < entry->pattern_count; i++) {
        const uft_sig_pattern_t *pat = &entry->patterns[i];
        uint32_t off = 0;
        
        if (pattern_match(data, size, pat, &off)) {
            patterns_matched++;
            if (patterns_matched == 1) {
                first_match = off;
            }
        }
    }
    
    /* Check if enough patterns matched */
    int required = (entry->patterns_required > 0) ? 
                   entry->patterns_required : entry->pattern_count;
    
    if (patterns_matched >= required) {
        match->patterns_matched = (uint8_t)patterns_matched;
        match->match_offset = first_match;
        match->score = entry->score_weight * patterns_matched;
        
        /* Set confidence */
        if (patterns_matched == entry->pattern_count) {
            match->confidence = UFT_SIG_CONF_EXACT;
        } else if (patterns_matched >= required) {
            match->confidence = UFT_SIG_CONF_HIGH;
        } else {
            match->confidence = UFT_SIG_CONF_MEDIUM;
        }
        
        return 1;
    }
    
    return 0;
}

int uft_sig_scan(const uint8_t *data, size_t size, uint16_t load_addr,
                 uft_sig_result_t *result)
{
    if (!data || !result) return 0;
    
    memset(result, 0, sizeof(*result));
    
    size_t db_size = uft_sig_db_size();
    
    for (size_t i = 0; i < db_size && result->match_count < 16; i++) {
        uft_sig_match_t match;
        
        if (uft_sig_match_entry(data, size, load_addr, &SIGNATURE_DB[i], &match)) {
            result->matches[result->match_count++] = match;
            result->total_score += match.score;
            
            /* Track primary category (highest scoring) */
            if (result->match_count == 1 || 
                match.score > result->matches[0].score) {
                result->primary_category = match.entry->category;
            }
        }
    }
    
    return result->match_count;
}

int uft_sig_scan_category(const uint8_t *data, size_t size, uint16_t load_addr,
                          uft_sig_category_t category, uft_sig_result_t *result)
{
    if (!data || !result) return 0;
    
    memset(result, 0, sizeof(*result));
    result->primary_category = category;
    
    size_t db_size = uft_sig_db_size();
    
    for (size_t i = 0; i < db_size && result->match_count < 16; i++) {
        if (SIGNATURE_DB[i].category != category) continue;
        
        uft_sig_match_t match;
        if (uft_sig_match_entry(data, size, load_addr, &SIGNATURE_DB[i], &match)) {
            result->matches[result->match_count++] = match;
            result->total_score += match.score;
        }
    }
    
    return result->match_count;
}

int uft_sig_has_fastloader(const uint8_t *data, size_t size, uint16_t load_addr)
{
    uft_sig_result_t result;
    return uft_sig_scan_category(data, size, load_addr, 
                                  UFT_SIG_CAT_FASTLOADER, &result) > 0;
}

int uft_sig_has_protection(const uint8_t *data, size_t size, uint16_t load_addr)
{
    uft_sig_result_t result;
    return uft_sig_scan_category(data, size, load_addr,
                                  UFT_SIG_CAT_PROTECTION, &result) > 0;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char *uft_sig_category_name(uft_sig_category_t cat)
{
    switch (cat) {
        case UFT_SIG_CAT_FASTLOADER: return "Fastloader";
        case UFT_SIG_CAT_COPIER:     return "Disk Copier";
        case UFT_SIG_CAT_NIBBLER:    return "Nibbler";
        case UFT_SIG_CAT_PROTECTION: return "Copy Protection";
        case UFT_SIG_CAT_CRACKER:    return "Cracker Group";
        case UFT_SIG_CAT_DOS:        return "DOS Extension";
        case UFT_SIG_CAT_BOOT:       return "Boot Routine";
        case UFT_SIG_CAT_SPEEDER:    return "Drive Speeder";
        default:                     return "Unknown";
    }
}

const char *uft_sig_confidence_name(uft_sig_confidence_t conf)
{
    switch (conf) {
        case UFT_SIG_CONF_LOW:    return "Low";
        case UFT_SIG_CONF_MEDIUM: return "Medium";
        case UFT_SIG_CONF_HIGH:   return "High";
        case UFT_SIG_CONF_EXACT:  return "Exact";
        default:                  return "Unknown";
    }
}

size_t uft_sig_format_result(const uft_sig_result_t *result,
                              char *out, size_t out_cap)
{
    if (!result || !out || out_cap == 0) return 0;
    
    size_t pos = 0;
    
    #define APPEND(...) do { \
        int n = snprintf(out + pos, out_cap - pos, __VA_ARGS__); \
        if (n > 0 && (size_t)n < out_cap - pos) pos += (size_t)n; \
    } while(0)
    
    APPEND("Signatures found: %d (score: %d)\n", 
           result->match_count, result->total_score);
    
    for (int i = 0; i < result->match_count; i++) {
        const uft_sig_match_t *m = &result->matches[i];
        APPEND("  [%d] %s", i + 1, m->entry->name);
        if (m->entry->version[0]) {
            APPEND(" %s", m->entry->version);
        }
        APPEND(" (%s, %s confidence)\n",
               uft_sig_category_name(m->entry->category),
               uft_sig_confidence_name(m->confidence));
    }
    
    #undef APPEND
    
    return pos;
}
