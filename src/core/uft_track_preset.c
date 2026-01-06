/**
 * @file uft_track_preset.c
 * @brief Track-Level Copy Preset System Implementation
 * 
 * Implements DC-BC-EDIT style copy profiles for per-track
 * copy mode configuration.
 * 
 * @version 1.0.0
 */

#include "uft/uft_track_preset.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/*============================================================================
 * Profile Management
 *============================================================================*/

int uft_copy_profile_init(uft_copy_profile_t *profile,
                          const char *name,
                          uint8_t tracks,
                          uint8_t sides)
{
    if (!profile || tracks == 0 || sides == 0 || sides > 2) {
        return -1;
    }
    
    memset(profile, 0, sizeof(*profile));
    
    /* Set name */
    if (name) {
        strncpy(profile->name, name, UFT_PROFILE_NAME_LEN - 1);
        profile->name[UFT_PROFILE_NAME_LEN - 1] = '\0';
    }
    
    profile->track_count = tracks;
    profile->side_count = sides;
    profile->default_config = uft_track_config_default();
    profile->version = 1;
    
    /* Allocate per-track configs */
    size_t total = (size_t)tracks * (size_t)sides;
    profile->tracks = (uft_track_config_t *)calloc(total, sizeof(uft_track_config_t));
    if (!profile->tracks) {
        return -1;
    }
    profile->tracks_count = total;
    
    /* Initialize all tracks with default */
    for (size_t i = 0; i < total; i++) {
        profile->tracks[i] = profile->default_config;
    }
    
    return 0;
}

void uft_copy_profile_free(uft_copy_profile_t *profile)
{
    if (!profile) return;
    
    if (profile->tracks) {
        free(profile->tracks);
        profile->tracks = NULL;
    }
    profile->tracks_count = 0;
}

static size_t track_index(const uft_copy_profile_t *profile, 
                          uint8_t track, 
                          uint8_t side)
{
    /* Layout: track[0].side[0], track[0].side[1], track[1].side[0], ... */
    return (size_t)track * (size_t)profile->side_count + (size_t)side;
}

int uft_copy_profile_set_track(uft_copy_profile_t *profile,
                               uint8_t track,
                               uint8_t side,
                               const uft_track_config_t *config)
{
    if (!profile || !profile->tracks || !config) {
        return -1;
    }
    
    if (track >= profile->track_count || side >= profile->side_count) {
        return -1;
    }
    
    size_t idx = track_index(profile, track, side);
    if (idx >= profile->tracks_count) {
        return -1;
    }
    
    profile->tracks[idx] = *config;
    return 0;
}

int uft_copy_profile_set_range(uft_copy_profile_t *profile,
                               uint8_t track_start,
                               uint8_t track_end,
                               int8_t side,
                               const uft_track_config_t *config)
{
    if (!profile || !config) {
        return -1;
    }
    
    if (track_start > track_end || track_end >= profile->track_count) {
        return -1;
    }
    
    uint8_t side_start = (side < 0) ? 0 : (uint8_t)side;
    uint8_t side_end = (side < 0) ? profile->side_count - 1 : (uint8_t)side;
    
    if (side_end >= profile->side_count) {
        return -1;
    }
    
    for (uint8_t t = track_start; t <= track_end; t++) {
        for (uint8_t s = side_start; s <= side_end; s++) {
            if (uft_copy_profile_set_track(profile, t, s, config) != 0) {
                return -1;
            }
        }
    }
    
    return 0;
}

uft_track_config_t uft_copy_profile_get_track(const uft_copy_profile_t *profile,
                                              uint8_t track,
                                              uint8_t side)
{
    if (!profile || track >= profile->track_count || side >= profile->side_count) {
        return uft_track_config_default();
    }
    
    if (!profile->tracks) {
        return profile->default_config;
    }
    
    size_t idx = track_index(profile, track, side);
    if (idx >= profile->tracks_count) {
        return profile->default_config;
    }
    
    return profile->tracks[idx];
}

/*============================================================================
 * DC-BC-EDIT Style Parser
 *============================================================================*/

typedef struct {
    const char *text;
    size_t pos;
    size_t len;
    int line;
    char error[256];
} parser_state_t;

static void skip_whitespace(parser_state_t *ps)
{
    while (ps->pos < ps->len) {
        char c = ps->text[ps->pos];
        if (c == '\n') {
            ps->line++;
            ps->pos++;
        } else if (isspace((unsigned char)c)) {
            ps->pos++;
        } else if (c == '#') {
            /* Skip comment to end of line */
            while (ps->pos < ps->len && ps->text[ps->pos] != '\n') {
                ps->pos++;
            }
        } else {
            break;
        }
    }
}

static int parse_number(parser_state_t *ps)
{
    skip_whitespace(ps);
    
    int sign = 1;
    if (ps->pos < ps->len && ps->text[ps->pos] == '-') {
        sign = -1;
        ps->pos++;
    }
    
    if (ps->pos >= ps->len || !isdigit((unsigned char)ps->text[ps->pos])) {
        return 0;
    }
    
    int value = 0;
    while (ps->pos < ps->len && isdigit((unsigned char)ps->text[ps->pos])) {
        value = value * 10 + (ps->text[ps->pos] - '0');
        ps->pos++;
    }
    
    return sign * value;
}

static bool expect_char(parser_state_t *ps, char c)
{
    skip_whitespace(ps);
    if (ps->pos < ps->len && ps->text[ps->pos] == c) {
        ps->pos++;
        return true;
    }
    return false;
}

static bool peek_char(parser_state_t *ps, char c)
{
    skip_whitespace(ps);
    return ps->pos < ps->len && ps->text[ps->pos] == c;
}

static char peek_next(parser_state_t *ps)
{
    skip_whitespace(ps);
    if (ps->pos < ps->len) {
        return ps->text[ps->pos];
    }
    return '\0';
}

int uft_copy_profile_parse(const char *text,
                           uft_copy_profile_t *profile,
                           char *error_out,
                           size_t error_size)
{
    if (!text || !profile) {
        if (error_out) snprintf(error_out, error_size, "NULL input");
        return -1;
    }
    
    parser_state_t ps = {
        .text = text,
        .pos = 0,
        .len = strlen(text),
        .line = 1,
        .error = {0}
    };
    
    /* Parse header: e.g. "SS 80 TRKS ..." or "DS 80 TRKS ..." */
    uint8_t sides = 1;
    uint8_t tracks = 80;
    
    skip_whitespace(&ps);
    
    /* Look for SS/DS indicator */
    if (ps.pos + 2 <= ps.len) {
        if (strncasecmp(ps.text + ps.pos, "SS", 2) == 0) {
            sides = 1;
            ps.pos += 2;
        } else if (strncasecmp(ps.text + ps.pos, "DS", 2) == 0) {
            sides = 2;
            ps.pos += 2;
        }
    }
    
    /* Look for track count */
    skip_whitespace(&ps);
    int t = parse_number(&ps);
    if (t > 0 && t <= UFT_MAX_TRACKS_PER_SIDE) {
        tracks = (uint8_t)t;
    }
    
    /* Skip rest of header line(s) until we hit a command */
    while (ps.pos < ps.len) {
        char c = peek_next(&ps);
        if (c == '!' || c == 'S' || c == ')' || c == ']' || 
            isdigit((unsigned char)c)) {
            break;
        }
        /* Skip to next line */
        while (ps.pos < ps.len && ps.text[ps.pos] != '\n') {
            ps.pos++;
        }
        skip_whitespace(&ps);
    }
    
    /* Initialize profile */
    if (uft_copy_profile_init(profile, "Imported", tracks, sides) != 0) {
        if (error_out) snprintf(error_out, error_size, "Failed to init profile");
        return -1;
    }
    
    /* Parse commands */
    int current_side = -1;  /* -1 = not in side block */
    int last_track = -1;
    uft_track_config_t last_config = uft_track_config_default();
    
    while (ps.pos < ps.len) {
        char cmd = peek_next(&ps);
        
        if (cmd == '\0') break;
        
        if (cmd == '!') {
            /* Start side 0 */
            ps.pos++;
            current_side = 0;
            last_track = -1;
        }
        else if (cmd == 'S') {
            /* Start side 1 */
            ps.pos++;
            current_side = 1;
            last_track = -1;
        }
        else if (cmd == ')') {
            /* End side */
            ps.pos++;
            current_side = -1;
        }
        else if (cmd == ']') {
            /* End profile */
            ps.pos++;
            break;
        }
        else if (isdigit((unsigned char)cmd)) {
            /* Track command: "N : CMD ..." */
            int track_num = parse_number(&ps);
            
            if (!expect_char(&ps, ':')) {
                snprintf(ps.error, sizeof(ps.error), 
                         "Line %d: expected ':' after track number", ps.line);
                if (error_out) strncpy(error_out, ps.error, error_size);
                uft_copy_profile_free(profile);
                return -1;
            }
            
            skip_whitespace(&ps);
            char track_cmd = '\0';
            if (ps.pos < ps.len) {
                track_cmd = toupper((unsigned char)ps.text[ps.pos]);
                ps.pos++;
            }
            
            uft_track_config_t cfg = uft_track_config_default();
            
            if (track_cmd == 'W') {
                /* Write Flux: W offset flags size revs */
                cfg.mode = UFT_TRACK_FLUX;
                cfg.flux_offset = (uint32_t)parse_number(&ps);
                int flags = parse_number(&ps);
                cfg.flags = (uint16_t)flags;
                cfg.flux_size = (uint32_t)parse_number(&ps);
                int revs = parse_number(&ps);
                cfg.revolutions = (revs > 0) ? (uint8_t)revs : 1;
            }
            else if (track_cmd == 'U') {
                /* Use Index: U mode flags */
                cfg.mode = UFT_TRACK_INDEX;
                int mode = parse_number(&ps);
                int flags = parse_number(&ps);
                cfg.flags = (uint16_t)flags;
                (void)mode;
            }
            else if (track_cmd == 'R') {
                /* Raw copy */
                cfg.mode = UFT_TRACK_RAW;
            }
            else if (track_cmd == 'S') {
                /* Sector copy */
                cfg.mode = UFT_TRACK_SECTOR;
            }
            else if (track_cmd == 'X') {
                /* Skip track */
                cfg.mode = UFT_TRACK_SKIP;
            }
            
            /* Apply to track */
            if (current_side >= 0 && current_side < profile->side_count &&
                track_num >= 0 && track_num < profile->track_count) {
                uft_copy_profile_set_track(profile, 
                                           (uint8_t)track_num, 
                                           (uint8_t)current_side, 
                                           &cfg);
            }
            
            last_track = track_num;
            last_config = cfg;
        }
        else if (cmd == 'R' || cmd == 'r') {
            /* Repeat: R : count */
            ps.pos++;
            
            if (!expect_char(&ps, ':')) {
                snprintf(ps.error, sizeof(ps.error), 
                         "Line %d: expected ':' after R", ps.line);
                if (error_out) strncpy(error_out, ps.error, error_size);
                uft_copy_profile_free(profile);
                return -1;
            }
            
            int end_track = parse_number(&ps);
            
            /* Apply last_config from last_track+1 to end_track */
            if (last_track >= 0 && current_side >= 0) {
                for (int t = last_track + 1; t <= end_track && t < profile->track_count; t++) {
                    uft_copy_profile_set_track(profile,
                                               (uint8_t)t,
                                               (uint8_t)current_side,
                                               &last_config);
                }
                last_track = end_track;
            }
        }
        else {
            /* Unknown character, skip line */
            while (ps.pos < ps.len && ps.text[ps.pos] != '\n') {
                ps.pos++;
            }
        }
        
        skip_whitespace(&ps);
    }
    
    return 0;
}

/*============================================================================
 * Profile Export
 *============================================================================*/

int uft_copy_profile_export(const uft_copy_profile_t *profile,
                            char *buffer,
                            size_t buffer_size)
{
    if (!profile || !buffer || buffer_size == 0) {
        return -1;
    }
    
    size_t pos = 0;
    
    /* Header */
    int n = snprintf(buffer + pos, buffer_size - pos,
                     "%s %d TRKS\n",
                     profile->side_count == 1 ? "SS" : "DS",
                     profile->track_count);
    if (n < 0 || (size_t)n >= buffer_size - pos) return -1;
    pos += (size_t)n;
    
    /* Per-side blocks */
    for (uint8_t side = 0; side < profile->side_count; side++) {
        /* Side start marker */
        n = snprintf(buffer + pos, buffer_size - pos,
                     "%s\n", side == 0 ? "!" : "S");
        if (n < 0 || (size_t)n >= buffer_size - pos) return -1;
        pos += (size_t)n;
        
        /* Track commands */
        uint8_t last_t = 0;
        uft_track_config_t last_cfg = {0};
        bool in_repeat = false;
        
        for (uint8_t t = 0; t < profile->track_count; t++) {
            uft_track_config_t cfg = uft_copy_profile_get_track(profile, t, side);
            
            /* Check if same as last */
            bool same = (t > 0) && 
                        (cfg.mode == last_cfg.mode) &&
                        (cfg.flags == last_cfg.flags) &&
                        (cfg.revolutions == last_cfg.revolutions);
            
            if (same && !in_repeat) {
                /* Start repeat */
                in_repeat = true;
            } else if (!same && in_repeat) {
                /* End repeat */
                n = snprintf(buffer + pos, buffer_size - pos,
                             "R : %d\n", t - 1);
                if (n < 0 || (size_t)n >= buffer_size - pos) return -1;
                pos += (size_t)n;
                in_repeat = false;
            }
            
            if (!same || t == 0) {
                /* Write track command */
                const char *mode_char = "U";
                switch (cfg.mode) {
                    case UFT_TRACK_FLUX: mode_char = "W"; break;
                    case UFT_TRACK_INDEX: mode_char = "U"; break;
                    case UFT_TRACK_SECTOR: mode_char = "S"; break;
                    case UFT_TRACK_RAW: mode_char = "R"; break;
                    case UFT_TRACK_SKIP: mode_char = "X"; break;
                    default: mode_char = "U"; break;
                }
                
                if (cfg.mode == UFT_TRACK_FLUX) {
                    n = snprintf(buffer + pos, buffer_size - pos,
                                 "%d : %s %u %u %u %u\n",
                                 t, mode_char,
                                 cfg.flux_offset, cfg.flags,
                                 cfg.flux_size, cfg.revolutions);
                } else {
                    n = snprintf(buffer + pos, buffer_size - pos,
                                 "%d : %s %d %d\n",
                                 t, mode_char, 0, cfg.flags);
                }
                if (n < 0 || (size_t)n >= buffer_size - pos) return -1;
                pos += (size_t)n;
                
                last_t = t;
            }
            
            last_cfg = cfg;
        }
        
        /* Close repeat if active */
        if (in_repeat) {
            n = snprintf(buffer + pos, buffer_size - pos,
                         "R : %d\n", profile->track_count - 1);
            if (n < 0 || (size_t)n >= buffer_size - pos) return -1;
            pos += (size_t)n;
        }
        
        /* Side end marker */
        n = snprintf(buffer + pos, buffer_size - pos, ")\n");
        if (n < 0 || (size_t)n >= buffer_size - pos) return -1;
        pos += (size_t)n;
    }
    
    /* Profile end marker */
    n = snprintf(buffer + pos, buffer_size - pos, "]\n");
    if (n < 0 || (size_t)n >= buffer_size - pos) return -1;
    pos += (size_t)n;
    
    return (int)pos;
}

/*============================================================================
 * Predefined Profiles
 *============================================================================*/

static uft_copy_profile_t s_profile_amiga_dd;
static uft_copy_profile_t s_profile_amiga_copyprot;
static uft_copy_profile_t s_profile_c64_standard;
static uft_copy_profile_t s_profile_c64_copyprot;
static uft_copy_profile_t s_profile_pc_dd;
static uft_copy_profile_t s_profile_pc_hd;
static uft_copy_profile_t s_profile_atari_st;
static bool s_profiles_initialized = false;

static void init_predefined_profiles(void)
{
    if (s_profiles_initialized) return;
    
    /* Amiga DD */
    uft_copy_profile_init(&s_profile_amiga_dd, "Amiga DD", 80, 2);
    strncpy(s_profile_amiga_dd.description, 
            "Standard Amiga 880K double-density", 
            UFT_PROFILE_DESC_LEN - 1);
    {
        uft_track_config_t cfg = uft_track_config_index();
        uft_copy_profile_set_range(&s_profile_amiga_dd, 0, 79, -1, &cfg);
    }
    
    /* Amiga with copy protection */
    uft_copy_profile_init(&s_profile_amiga_copyprot, "Amiga Copy-Protected", 80, 2);
    strncpy(s_profile_amiga_copyprot.description,
            "Amiga with copy protection (flux tracks 0, 79)",
            UFT_PROFILE_DESC_LEN - 1);
    {
        uft_track_config_t idx = uft_track_config_index();
        uft_track_config_t flux = uft_track_config_copyprot();
        
        /* Track 0 and 79: flux copy */
        uft_copy_profile_set_track(&s_profile_amiga_copyprot, 0, 0, &flux);
        uft_copy_profile_set_track(&s_profile_amiga_copyprot, 0, 1, &flux);
        uft_copy_profile_set_track(&s_profile_amiga_copyprot, 79, 0, &flux);
        uft_copy_profile_set_track(&s_profile_amiga_copyprot, 79, 1, &flux);
        
        /* Rest: index copy */
        uft_copy_profile_set_range(&s_profile_amiga_copyprot, 1, 78, -1, &idx);
    }
    
    /* C64 Standard */
    uft_copy_profile_init(&s_profile_c64_standard, "C64 1541", 35, 1);
    strncpy(s_profile_c64_standard.description,
            "Standard C64 1541 disk (35 tracks)",
            UFT_PROFILE_DESC_LEN - 1);
    {
        uft_track_config_t cfg = uft_track_config_index();
        uft_copy_profile_set_range(&s_profile_c64_standard, 0, 34, 0, &cfg);
    }
    
    /* C64 with copy protection */
    uft_copy_profile_init(&s_profile_c64_copyprot, "C64 Copy-Protected", 42, 1);
    strncpy(s_profile_c64_copyprot.description,
            "C64 with extended tracks and protection",
            UFT_PROFILE_DESC_LEN - 1);
    {
        uft_track_config_t idx = uft_track_config_index();
        uft_track_config_t flux = uft_track_config_copyprot();
        
        /* Standard tracks: index */
        uft_copy_profile_set_range(&s_profile_c64_copyprot, 0, 34, 0, &idx);
        
        /* Extended tracks (35-41): flux */
        uft_copy_profile_set_range(&s_profile_c64_copyprot, 35, 41, 0, &flux);
    }
    
    /* PC DD (720K) */
    uft_copy_profile_init(&s_profile_pc_dd, "PC DD 720K", 80, 2);
    strncpy(s_profile_pc_dd.description,
            "IBM PC 720K double-density",
            UFT_PROFILE_DESC_LEN - 1);
    {
        uft_track_config_t cfg = uft_track_config_index();
        uft_copy_profile_set_range(&s_profile_pc_dd, 0, 79, -1, &cfg);
    }
    
    /* PC HD (1.44M) */
    uft_copy_profile_init(&s_profile_pc_hd, "PC HD 1.44M", 80, 2);
    strncpy(s_profile_pc_hd.description,
            "IBM PC 1.44M high-density",
            UFT_PROFILE_DESC_LEN - 1);
    {
        uft_track_config_t cfg = uft_track_config_index();
        uft_copy_profile_set_range(&s_profile_pc_hd, 0, 79, -1, &cfg);
    }
    
    /* Atari ST */
    uft_copy_profile_init(&s_profile_atari_st, "Atari ST", 82, 2);
    strncpy(s_profile_atari_st.description,
            "Atari ST (up to 82 tracks)",
            UFT_PROFILE_DESC_LEN - 1);
    {
        uft_track_config_t cfg = uft_track_config_index();
        uft_copy_profile_set_range(&s_profile_atari_st, 0, 81, -1, &cfg);
    }
    
    s_profiles_initialized = true;
}

const uft_copy_profile_t *uft_profile_amiga_dd(void)
{
    init_predefined_profiles();
    return &s_profile_amiga_dd;
}

const uft_copy_profile_t *uft_profile_amiga_copyprot(void)
{
    init_predefined_profiles();
    return &s_profile_amiga_copyprot;
}

const uft_copy_profile_t *uft_profile_c64_standard(void)
{
    init_predefined_profiles();
    return &s_profile_c64_standard;
}

const uft_copy_profile_t *uft_profile_c64_copyprot(void)
{
    init_predefined_profiles();
    return &s_profile_c64_copyprot;
}

const uft_copy_profile_t *uft_profile_pc_dd(void)
{
    init_predefined_profiles();
    return &s_profile_pc_dd;
}

const uft_copy_profile_t *uft_profile_pc_hd(void)
{
    init_predefined_profiles();
    return &s_profile_pc_hd;
}

const uft_copy_profile_t *uft_profile_atari_st(void)
{
    init_predefined_profiles();
    return &s_profile_atari_st;
}
