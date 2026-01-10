#include "uft/c64/uft_cbm_drive_scan.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* Keywords and their categories */
typedef struct {
    const char *keyword;
    int score;
    int flag;  /* 0=none, 1=dos_cmd, 2=gcr, 3=halftrack, 4=track, 5=drive */
} kw_entry_t;

enum {
    KW_NONE = 0,
    KW_DOS_CMD,
    KW_GCR,
    KW_HALFTRACK,
    KW_TRACK,
    KW_DRIVE
};

static const kw_entry_t KWS[] = {
    /* DOS commands */
    {"M-W", 6, KW_DOS_CMD},
    {"M-R", 6, KW_DOS_CMD},
    {"B-P", 5, KW_DOS_CMD},
    {"U1", 4, KW_DOS_CMD},
    {"U2", 4, KW_DOS_CMD},
    {"UJ", 4, KW_DOS_CMD},
    {"UI", 4, KW_DOS_CMD},
    
    /* GCR/Nibbler keywords */
    {"GCR", 5, KW_GCR},
    {"NIB", 5, KW_GCR},
    {"NIBBLE", 6, KW_GCR},
    {"NIBBLER", 8, KW_GCR},
    {"BURST", 5, KW_GCR},
    
    /* Halftrack */
    {"HALFTRACK", 8, KW_HALFTRACK},
    {"HALBSPUR", 8, KW_HALFTRACK},
    {"HALF TRACK", 8, KW_HALFTRACK},
    
    /* Track/Sector */
    {"TRACK", 2, KW_TRACK},
    {"SECTOR", 2, KW_TRACK},
    {"COPY", 2, KW_TRACK},
    {"DISK", 1, KW_TRACK},
    {"FORMAT", 2, KW_TRACK},
    {"BAM", 3, KW_TRACK},
    
    /* Drive models */
    {"1541", 4, KW_DRIVE},
    {"1571", 4, KW_DRIVE},
    {"1581", 4, KW_DRIVE},
    
    /* Generic */
    {"FAST", 2, KW_NONE},
    {"SPEED", 2, KW_NONE},
    {"TURBO", 3, KW_NONE},
    {"LOADER", 3, KW_NONE},
    {"ERROR", 1, KW_NONE},
    {"DIR", 1, KW_NONE},
};

static int contains_kw(const char *s, const char *kw) {
    return (strstr(s, kw) != NULL);
}

int uft_cbm_drive_scan_payload(const uint8_t *p, size_t n, uft_scan_result_t *out)
{
    if (!out) return -1;
    memset(out, 0, sizeof(*out));
    if (!p || n == 0) return -2;

    /* Extract ASCII runs len>=4 */
    size_t i = 0;
    while (i < n) {
        if (p[i] >= 32 && p[i] <= 126) {
            size_t start = i;
            char buf[128];
            size_t b = 0;
            while (i < n && p[i] >= 32 && p[i] <= 126 && b < sizeof(buf)-1) {
                char c = (char)p[i++];
                /* normalize to uppercase */
                buf[b++] = (char)toupper((unsigned char)c);
            }
            buf[b] = '\0';
            
            if (b >= 3) {  /* Minimum 3 chars for keywords like "GCR" */
                for (size_t k = 0; k < sizeof(KWS)/sizeof(KWS[0]); k++) {
                    const kw_entry_t *kw = &KWS[k];
                    if (contains_kw(buf, kw->keyword)) {
                        out->score += kw->score;
                        
                        /* Set feature flags */
                        switch (kw->flag) {
                            case KW_DOS_CMD:   out->has_dos_commands = 1; break;
                            case KW_GCR:       out->has_gcr_keywords = 1; break;
                            case KW_HALFTRACK: out->has_halftrack = 1; break;
                            case KW_TRACK:     out->has_track_refs = 1; break;
                            case KW_DRIVE:     out->has_drive_refs = 1; break;
                        }
                        
                        if (out->hit_count < 64) {
                            uft_scan_hit_t *h = &out->hits[out->hit_count++];
                            h->offset = (uint32_t)start;
                            strncpy(h->text, buf, sizeof(h->text)-1);
                            h->text[sizeof(h->text)-1] = '\0';
                            h->score = kw->score;
                            /* Set category based on flag */
                            switch (kw->flag) {
                                case KW_DOS_CMD:   h->category = "DOS Command"; break;
                                case KW_GCR:       h->category = "GCR/Nibbler"; break;
                                case KW_HALFTRACK: h->category = "Halftrack"; break;
                                case KW_TRACK:     h->category = "Track/Sector"; break;
                                case KW_DRIVE:     h->category = "Drive Model"; break;
                                default:           h->category = "General"; break;
                            }
                        }
                    }
                }
            }
        } else {
            i++;
        }
    }
    
    /* Classify based on score and flags */
    if (out->score >= UFT_SCAN_SCORE_NIBBLER || 
        (out->has_gcr_keywords && out->has_halftrack)) {
        out->is_nibbler = 1;
    }
    
    if (out->score >= UFT_SCAN_SCORE_FASTLOADER ||
        (out->has_dos_commands && out->has_drive_refs)) {
        out->is_fastloader = 1;
    }
    
    if (out->has_track_refs && !out->is_nibbler) {
        out->is_copier = 1;
    }
    
    return 0;
}

/* ============================================================================
 * Additional API functions
 * ============================================================================ */

uft_cbm_tool_type_t uft_cbm_classify_tool(const uft_scan_result_t *result)
{
    if (!result) return UFT_CBM_TOOL_UNKNOWN;
    
    if (result->is_nibbler) return UFT_CBM_TOOL_NIBBLER;
    if (result->is_fastloader) return UFT_CBM_TOOL_FASTLOADER;
    if (result->is_copier) return UFT_CBM_TOOL_COPIER;
    if (result->has_dos_commands || result->has_drive_refs) return UFT_CBM_TOOL_DRIVE_CODE;
    
    return UFT_CBM_TOOL_UNKNOWN;
}

int uft_cbm_has_dos_command(const uint8_t *payload, size_t len, const char *cmd)
{
    if (!payload || len == 0 || !cmd) return 0;
    
    size_t cmd_len = strlen(cmd);
    if (cmd_len == 0 || cmd_len > len) return 0;
    
    /* Search for command in payload (case-insensitive) */
    for (size_t i = 0; i <= len - cmd_len; i++) {
        int match = 1;
        for (size_t j = 0; j < cmd_len && match; j++) {
            char c1 = (char)toupper((unsigned char)payload[i + j]);
            char c2 = (char)toupper((unsigned char)cmd[j]);
            if (c1 != c2) match = 0;
        }
        if (match) return 1;
    }
    return 0;
}

int uft_cbm_identify_tool(const uint8_t *payload, size_t len, char *name_out, size_t name_cap)
{
    if (!payload || len == 0 || !name_out || name_cap == 0) return 0;
    name_out[0] = '\0';
    
    uft_scan_result_t result;
    int ret = uft_cbm_drive_scan_payload(payload, len, &result);
    if (ret != 0) return 0;
    
    /* Look for tool name in hits */
    for (size_t i = 0; i < result.hit_count; i++) {
        const char *text = result.hits[i].text;
        /* Look for patterns like "TURBO NIBBLER", "FAST COPY", etc. */
        if (strstr(text, "NIBBLER") || strstr(text, "COPIER") || 
            strstr(text, "COPY") || strstr(text, "LOADER")) {
            strncpy(name_out, text, name_cap - 1);
            name_out[name_cap - 1] = '\0';
            return 1;
        }
    }
    
    /* Return type name if no specific name found */
    uft_cbm_tool_type_t type = uft_cbm_classify_tool(&result);
    if (type != UFT_CBM_TOOL_UNKNOWN) {
        strncpy(name_out, uft_cbm_tool_type_name(type), name_cap - 1);
        name_out[name_cap - 1] = '\0';
        return 1;
    }
    
    return 0;
}

int uft_cbm_extract_strings(const uint8_t *payload, size_t len, char **strings_out, size_t max_strings)
{
    if (!payload || len == 0 || !strings_out || max_strings == 0) return 0;
    
    size_t count = 0;
    size_t i = 0;
    
    while (i < len && count < max_strings) {
        /* Find start of ASCII string */
        if (payload[i] >= 32 && payload[i] <= 126) {
            size_t start = i;
            while (i < len && payload[i] >= 32 && payload[i] <= 126) {
                i++;
            }
            size_t str_len = i - start;
            if (str_len >= 4) {
                /* Allocate and copy string */
                strings_out[count] = malloc(str_len + 1);
                if (strings_out[count]) {
                    memcpy(strings_out[count], &payload[start], str_len);
                    strings_out[count][str_len] = '\0';
                    count++;
                }
            }
        } else {
            i++;
        }
    }
    
    return (int)count;
}

const char *uft_cbm_tool_type_name(uft_cbm_tool_type_t type)
{
    switch (type) {
        case UFT_CBM_TOOL_NIBBLER:    return "Nibbler/Copy Protection Tool";
        case UFT_CBM_TOOL_COPIER:     return "Disk Copier";
        case UFT_CBM_TOOL_FASTLOADER: return "Fast Loader";
        case UFT_CBM_TOOL_DRIVE_CODE: return "Drive Code";
        default:                      return "Unknown";
    }
}

int uft_cbm_drive_scan_prg(const uint8_t *prg, size_t prg_len, uft_scan_result_t *out)
{
    if (!prg || prg_len < 3 || !out) return -1;
    
    /* Skip 2-byte PRG load address header */
    return uft_cbm_drive_scan_payload(prg + 2, prg_len - 2, out);
}
