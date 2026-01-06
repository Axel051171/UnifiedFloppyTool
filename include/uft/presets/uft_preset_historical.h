/*
 * uft_preset_historical.h - Historical/Exotic Computer Format Presets
 *
 * Part of UnifiedFloppyTool (UFT) v3.3.0
 * Based on MAME format implementations (BSD-3-Clause)
 *
 * Covers rare and historical computer systems:
 * Victor 9000, Oric, DEC Rainbow/PDP, HP, Sharp, etc.
 */

#ifndef UFT_PRESET_HISTORICAL_H
#define UFT_PRESET_HISTORICAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Format IDs
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum uft_historical_format_id {
    /* Victor 9000 (GCR, variable speed zones) */
    UFT_VICTOR9K_SS = 0,
    UFT_VICTOR9K_DS,
    
    /* Oric */
    UFT_ORIC_DSK,
    
    /* DEC Rainbow / RX50 */
    UFT_DEC_RX50,               /* 400K SSQD */
    UFT_DEC_RX01,               /* 256K 8" */
    UFT_DEC_RX02,               /* 512K 8" */
    
    /* HP */
    UFT_HP_MFI,                 /* HP LIF format */
    UFT_HP_300,                 /* HP 9000/300 */
    
    /* Sharp */
    UFT_SHARP_X1,               /* Sharp X1 */
    UFT_SHARP_X68K,             /* Sharp X68000 */
    UFT_SHARP_MZ,               /* Sharp MZ series */
    
    /* Sord M5 */
    UFT_SORD_M5,
    
    /* Tiki-100 */
    UFT_TIKI100,
    
    /* Epson QX-10 */
    UFT_EPSON_QX10,
    
    /* Kaypro */
    UFT_KAYPRO_2,               /* Kaypro II/4 */
    UFT_KAYPRO_10,              /* Kaypro 10 */
    
    /* Osborne */
    UFT_OSBORNE_1,              /* Osborne 1 */
    UFT_OSBORNE_DD,             /* Osborne DD */
    
    UFT_HISTORICAL_FORMAT_COUNT
} uft_historical_format_id_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Victor 9000 Zone Table
 * GCR encoding with variable speed zones (like Commodore but different)
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_VICTOR9K_ZONE_COUNT     9

typedef struct uft_victor9k_zone {
    uint8_t  start_track_h0;    /* Start track, head 0 */
    uint8_t  end_track_h0;      /* End track, head 0 */
    uint8_t  start_track_h1;    /* Start track, head 1 (if DS) */
    uint8_t  end_track_h1;      /* End track, head 1 */
    uint8_t  sectors;           /* Sectors per track */
    uint16_t rpm;               /* Rotational speed */
    float    period_ms;         /* Rotational period in ms */
} uft_victor9k_zone_t;

static const uft_victor9k_zone_t UFT_VICTOR9K_ZONES[UFT_VICTOR9K_ZONE_COUNT] = {
    { 0,  3,   0,  0, 19, 252, 237.9f },  /* Zone 0: unused on head 1 */
    { 4, 15,   0,  7, 18, 267, 224.5f },
    {16, 26,   8, 18, 17, 283, 212.2f },
    {27, 37,  19, 29, 16, 300, 199.9f },
    {38, 47,  30, 39, 15, 320, 187.6f },
    {48, 59,  40, 51, 14, 342, 175.3f },
    {60, 70,  52, 62, 13, 368, 163.0f },
    {71, 79,  63, 74, 12, 401, 149.6f },
    { 0,  0,  75, 79, 11, 417, 144.0f },  /* Zone 8: unused on head 0 */
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Preset Structure
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct uft_historical_preset {
    uft_historical_format_id_t id;
    const char *name;
    const char *description;
    const char *system;         /* Computer system name */
    
    /* Geometry */
    uint8_t  form_factor;       /* 5 = 5.25", 3 = 3.5", 8 = 8" */
    uint8_t  cyls;
    uint8_t  heads;
    uint8_t  secs;              /* 0 = variable */
    uint16_t bps;
    
    /* Encoding */
    uint8_t  encoding;          /* 0=FM, 1=MFM, 2=GCR */
    uint8_t  variable_speed;    /* 1 = uses speed zones */
    
    /* Extensions */
    const char *extensions;
} uft_historical_preset_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Preset Table
 * ═══════════════════════════════════════════════════════════════════════════ */

static const uft_historical_preset_t UFT_HISTORICAL_PRESETS[] = {
    /* Victor 9000 */
    {
        .id = UFT_VICTOR9K_SS,
        .name = "Victor 9000 SS",
        .description = "600K Victor 9000 single sided",
        .system = "Victor 9000/Sirius 1",
        .form_factor = 5, .cyls = 80, .heads = 1, .secs = 0, .bps = 512,
        .encoding = 2, .variable_speed = 1,
        .extensions = ".img"
    },
    {
        .id = UFT_VICTOR9K_DS,
        .name = "Victor 9000 DS",
        .description = "1.2M Victor 9000 double sided",
        .system = "Victor 9000/Sirius 1",
        .form_factor = 5, .cyls = 80, .heads = 2, .secs = 0, .bps = 512,
        .encoding = 2, .variable_speed = 1,
        .extensions = ".img"
    },
    
    /* Oric */
    {
        .id = UFT_ORIC_DSK,
        .name = "Oric DSK",
        .description = "Oric Atmos/Telestrat disk",
        .system = "Oric Atmos",
        .form_factor = 3, .cyls = 80, .heads = 2, .secs = 17, .bps = 256,
        .encoding = 1, .variable_speed = 0,
        .extensions = ".dsk"
    },
    
    /* DEC */
    {
        .id = UFT_DEC_RX50,
        .name = "DEC RX50",
        .description = "400K DEC Rainbow/Pro RX50",
        .system = "DEC Rainbow 100",
        .form_factor = 5, .cyls = 80, .heads = 1, .secs = 10, .bps = 512,
        .encoding = 1, .variable_speed = 0,
        .extensions = ".img"
    },
    {
        .id = UFT_DEC_RX01,
        .name = "DEC RX01",
        .description = "256K DEC RX01 8-inch",
        .system = "DEC PDP-11",
        .form_factor = 8, .cyls = 77, .heads = 1, .secs = 26, .bps = 128,
        .encoding = 0, .variable_speed = 0,
        .extensions = ".img"
    },
    {
        .id = UFT_DEC_RX02,
        .name = "DEC RX02",
        .description = "512K DEC RX02 8-inch",
        .system = "DEC PDP-11",
        .form_factor = 8, .cyls = 77, .heads = 1, .secs = 26, .bps = 256,
        .encoding = 1, .variable_speed = 0,
        .extensions = ".img"
    },
    
    /* HP */
    {
        .id = UFT_HP_MFI,
        .name = "HP LIF",
        .description = "HP Logical Interchange Format",
        .system = "HP 9000",
        .form_factor = 5, .cyls = 77, .heads = 2, .secs = 16, .bps = 256,
        .encoding = 1, .variable_speed = 0,
        .extensions = ".lif"
    },
    {
        .id = UFT_HP_300,
        .name = "HP 9000/300",
        .description = "HP 9000/300 series",
        .system = "HP 9000/300",
        .form_factor = 3, .cyls = 80, .heads = 2, .secs = 18, .bps = 512,
        .encoding = 1, .variable_speed = 0,
        .extensions = ".img"
    },
    
    /* Sharp */
    {
        .id = UFT_SHARP_X1,
        .name = "Sharp X1",
        .description = "Sharp X1 series",
        .system = "Sharp X1",
        .form_factor = 5, .cyls = 40, .heads = 2, .secs = 16, .bps = 256,
        .encoding = 1, .variable_speed = 0,
        .extensions = ".2d"
    },
    {
        .id = UFT_SHARP_X68K,
        .name = "Sharp X68000",
        .description = "Sharp X68000 (PC-98 compatible)",
        .system = "Sharp X68000",
        .form_factor = 5, .cyls = 77, .heads = 2, .secs = 8, .bps = 1024,
        .encoding = 1, .variable_speed = 0,
        .extensions = ".xdf;.hdm"
    },
    {
        .id = UFT_SHARP_MZ,
        .name = "Sharp MZ",
        .description = "Sharp MZ-80/MZ-700 series",
        .system = "Sharp MZ",
        .form_factor = 5, .cyls = 35, .heads = 1, .secs = 16, .bps = 256,
        .encoding = 1, .variable_speed = 0,
        .extensions = ".mzf"
    },
    
    /* Sord M5 */
    {
        .id = UFT_SORD_M5,
        .name = "Sord M5",
        .description = "Sord M5 computer",
        .system = "Sord M5",
        .form_factor = 5, .cyls = 40, .heads = 1, .secs = 18, .bps = 256,
        .encoding = 1, .variable_speed = 0,
        .extensions = ".dsk"
    },
    
    /* Tiki-100 */
    {
        .id = UFT_TIKI100,
        .name = "Tiki-100",
        .description = "Norwegian Tiki-100",
        .system = "Tiki-100",
        .form_factor = 5, .cyls = 40, .heads = 2, .secs = 10, .bps = 512,
        .encoding = 1, .variable_speed = 0,
        .extensions = ".dsk"
    },
    
    /* Epson QX-10 */
    {
        .id = UFT_EPSON_QX10,
        .name = "Epson QX-10",
        .description = "Epson QX-10 CP/M",
        .system = "Epson QX-10",
        .form_factor = 5, .cyls = 40, .heads = 2, .secs = 9, .bps = 512,
        .encoding = 1, .variable_speed = 0,
        .extensions = ".img"
    },
    
    /* Kaypro */
    {
        .id = UFT_KAYPRO_2,
        .name = "Kaypro II/4",
        .description = "Kaypro II/4 SSDD",
        .system = "Kaypro",
        .form_factor = 5, .cyls = 40, .heads = 1, .secs = 10, .bps = 512,
        .encoding = 1, .variable_speed = 0,
        .extensions = ".img"
    },
    {
        .id = UFT_KAYPRO_10,
        .name = "Kaypro 10",
        .description = "Kaypro 10 DSDD",
        .system = "Kaypro",
        .form_factor = 5, .cyls = 40, .heads = 2, .secs = 10, .bps = 512,
        .encoding = 1, .variable_speed = 0,
        .extensions = ".img"
    },
    
    /* Osborne */
    {
        .id = UFT_OSBORNE_1,
        .name = "Osborne 1",
        .description = "Osborne 1 SSSD",
        .system = "Osborne 1",
        .form_factor = 5, .cyls = 40, .heads = 1, .secs = 10, .bps = 256,
        .encoding = 0, .variable_speed = 0,
        .extensions = ".img"
    },
    {
        .id = UFT_OSBORNE_DD,
        .name = "Osborne DD",
        .description = "Osborne Executive DSDD",
        .system = "Osborne Executive",
        .form_factor = 5, .cyls = 40, .heads = 2, .secs = 5, .bps = 1024,
        .encoding = 1, .variable_speed = 0,
        .extensions = ".img"
    },
};

/* ═══════════════════════════════════════════════════════════════════════════
 * API Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

static inline const uft_historical_preset_t *uft_historical_get_preset(uft_historical_format_id_t id) {
    if (id >= UFT_HISTORICAL_FORMAT_COUNT) return NULL;
    return &UFT_HISTORICAL_PRESETS[id];
}

/**
 * Get Victor 9000 zone for a given track/head
 */
static inline const uft_victor9k_zone_t *uft_victor9k_get_zone(uint8_t track, uint8_t head) {
    for (int i = 0; i < UFT_VICTOR9K_ZONE_COUNT; i++) {
        const uft_victor9k_zone_t *z = &UFT_VICTOR9K_ZONES[i];
        if (head == 0) {
            if (track >= z->start_track_h0 && track <= z->end_track_h0) return z;
        } else {
            if (track >= z->start_track_h1 && track <= z->end_track_h1) return z;
        }
    }
    return NULL;
}

/**
 * Get sectors per track for Victor 9000
 */
static inline uint8_t uft_victor9k_sectors(uint8_t track, uint8_t head) {
    const uft_victor9k_zone_t *z = uft_victor9k_get_zone(track, head);
    return z ? z->sectors : 0;
}

#ifdef __cplusplus
}
#endif
#endif /* UFT_PRESET_HISTORICAL_H */
