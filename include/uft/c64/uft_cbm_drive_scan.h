#pragma once
/*
 * uft_cbm_drive_scan.h — heuristics to find floppy/drive related artifacts in C64 PRGs (C11)
 *
 * What we look for:
 * - ASCII CBM DOS command strings (M-W, M-R, B-P, U1/U2, etc.)
 * - Keywords typical for nibblers/fastcopiers (GCR/NIB/BURST/HALFTRACK)
 *
 * Output is a deterministic score + hit list.
 */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct uft_scan_hit {
    uint32_t offset;      /* offset inside PRG payload */
    char     text[64];    /* matched snippet (nul-terminated) */
    int      score;       /* score contribution of this hit */
    const char *category; /* category name (static string) */
} uft_scan_hit_t;

/* Score thresholds */
#define UFT_SCAN_SCORE_NIBBLER    50
#define UFT_SCAN_SCORE_FASTLOADER 30
#define UFT_SCAN_SCORE_DRIVE_CODE 20

typedef struct uft_scan_result {
    int score;
    size_t hit_count;
    uft_scan_hit_t hits[64];
    
    /* Classification flags */
    int is_nibbler;        /* Detected as nibbler/copy tool */
    int is_fastloader;     /* Detected as fastloader */
    int is_copier;         /* Detected as general disk copier */
    
    /* Feature flags */
    int has_gcr_keywords;  /* Contains GCR-related code */
    int has_dos_commands;  /* Contains CBM DOS commands */
    int has_halftrack;     /* References halftrack operations */
    int has_track_refs;    /* References track/sector operations */
    int has_drive_refs;    /* References drive models (1541, etc.) */
} uft_scan_result_t;

/* Tool type enumeration */
typedef enum {
    UFT_CBM_TOOL_UNKNOWN = 0,
    UFT_CBM_TOOL_NIBBLER,
    UFT_CBM_TOOL_COPIER,
    UFT_CBM_TOOL_FASTLOADER,
    UFT_CBM_TOOL_DRIVE_CODE
} uft_cbm_tool_type_t;






/* Get name for tool type */
const char *uft_cbm_tool_type_name(uft_cbm_tool_type_t type);


#ifdef __cplusplus
}
#endif
