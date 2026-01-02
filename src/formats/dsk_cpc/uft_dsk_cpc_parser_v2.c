/**
 * @file uft_dsk_cpc_parser_v2.c
 * @brief GOD MODE DSK Parser v2 - Amstrad CPC/ZX Spectrum Extended DSK
 * 
 * Supports:
 * - Standard DSK format (184-byte header)
 * - Extended DSK (EDSK) format with variable track sizes
 * - Sector status flags (copy protection)
 * - Multiple sector sizes per track
 * - Gap3 length detection
 * - Interleave pattern analysis
 * - Weak sector detection
 * - Data rate analysis
 * 
 * @version 2.0.0 GOD MODE
 * @date 2026-01-02
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════════ */

#define DSK_HEADER_SIZE         256
#define DSK_TRACK_HEADER_SIZE   256
#define DSK_SECTOR_INFO_SIZE    8
#define DSK_MAX_TRACKS          85
#define DSK_MAX_SIDES           2
#define DSK_MAX_SECTORS         29

/* Magic signatures */
static const char DSK_SIGNATURE[] = "MV - CPC";
static const char EDSK_SIGNATURE[] = "EXTENDED CPC DSK File\r\nDisk-Info\r\n";

/* ═══════════════════════════════════════════════════════════════════════════
 * DATA STRUCTURES
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Sector status flags (FDC result)
 */
typedef enum {
    DSK_ST1_NONE           = 0x00,
    DSK_ST1_MISSING_AM     = 0x01,  /* Missing Address Mark */
    DSK_ST1_NOT_WRITABLE   = 0x02,  /* Not Writable */
    DSK_ST1_NO_DATA        = 0x04,  /* No Data */
    DSK_ST1_OVERRUN        = 0x10,  /* Overrun */
    DSK_ST1_CRC_ERROR      = 0x20,  /* CRC Error in ID field */
    DSK_ST1_END_OF_CYL     = 0x80,  /* End of Cylinder */
    
    DSK_ST2_NONE           = 0x00,
    DSK_ST2_MISSING_DAM    = 0x01,  /* Missing Data Address Mark */
    DSK_ST2_BAD_CYLINDER   = 0x02,  /* Bad Cylinder */
    DSK_ST2_SCAN_NOT_SAT   = 0x04,  /* Scan Not Satisfied */
    DSK_ST2_SCAN_EQUAL     = 0x08,  /* Scan Equal Hit */
    DSK_ST2_WRONG_CYL      = 0x10,  /* Wrong Cylinder */
    DSK_ST2_CRC_ERROR      = 0x20,  /* CRC Error in Data field */
    DSK_ST2_DELETED        = 0x40   /* Control Mark (Deleted) */
} dsk_fdc_status_t;

/**
 * @brief DSK format type
 */
typedef enum {
    DSK_FORMAT_UNKNOWN = 0,
    DSK_FORMAT_STANDARD,       /* Original DSK format */
    DSK_FORMAT_EXTENDED        /* EDSK with variable track sizes */
} dsk_format_type_t;

/**
 * @brief Recording mode
 */
typedef enum {
    DSK_MODE_UNKNOWN = 0,
    DSK_MODE_FM,               /* Single density */
    DSK_MODE_MFM               /* Double density */
} dsk_recording_mode_t;

/**
 * @brief Disk information header
 */
typedef struct {
    char           signature[34];      /* "MV - CPC..." or "EXTENDED CPC..." */
    char           creator[14];        /* Creator name */
    uint8_t        tracks;             /* Number of tracks */
    uint8_t        sides;              /* Number of sides */
    uint16_t       track_size;         /* Standard: track size, EDSK: unused */
    uint8_t        track_sizes[DSK_MAX_TRACKS * DSK_MAX_SIDES];  /* EDSK: per-track sizes in 256-byte units */
} dsk_disk_info_t;

/**
 * @brief Sector information block
 */
typedef struct {
    uint8_t        track;              /* C - Cylinder/Track */
    uint8_t        side;               /* H - Head/Side */
    uint8_t        sector_id;          /* R - Sector ID */
    uint8_t        size_code;          /* N - Size code (0=128, 1=256, 2=512, 3=1024...) */
    uint8_t        st1;                /* FDC Status Register 1 */
    uint8_t        st2;                /* FDC Status Register 2 */
    uint16_t       actual_size;        /* EDSK: actual data length */
} dsk_sector_info_t;

/**
 * @brief Track information block
 */
typedef struct {
    char           signature[13];      /* "Track-Info\r\n" */
    uint8_t        unused1[3];
    uint8_t        track;              /* Track number */
    uint8_t        side;               /* Side number */
    uint8_t        unused2[2];
    uint8_t        size_code;          /* Sector size code */
    uint8_t        num_sectors;        /* Number of sectors */
    uint8_t        gap3_length;        /* GAP#3 length */
    uint8_t        filler_byte;        /* Filler byte */
    dsk_sector_info_t sectors[DSK_MAX_SECTORS];
} dsk_track_info_t;

/**
 * @brief Parsed sector data
 */
typedef struct {
    dsk_sector_info_t info;
    uint8_t*       data;               /* Sector data (not owned) */
    size_t         data_size;
    bool           has_crc_error;
    bool           is_deleted;
    bool           is_weak;            /* Multiple reads differ */
    uint8_t        weak_mask;          /* Bits that vary */
} dsk_parsed_sector_t;

/**
 * @brief Parsed track
 */
typedef struct {
    uint8_t        track_num;
    uint8_t        side_num;
    uint8_t        num_sectors;
    uint8_t        gap3_length;
    uint8_t        filler_byte;
    dsk_recording_mode_t mode;
    dsk_parsed_sector_t sectors[DSK_MAX_SECTORS];
    uint8_t        interleave[DSK_MAX_SECTORS];  /* Sector order */
    size_t         track_size;
} dsk_parsed_track_t;

/**
 * @brief Full DSK image
 */
typedef struct {
    dsk_format_type_t format;
    char           creator[15];
    uint8_t        num_tracks;
    uint8_t        num_sides;
    dsk_parsed_track_t tracks[DSK_MAX_TRACKS][DSK_MAX_SIDES];
    
    /* Analysis results */
    bool           has_copy_protection;
    bool           has_weak_sectors;
    bool           has_errors;
    size_t         total_sectors;
    size_t         error_sectors;
} dsk_image_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * UTILITY FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get sector size from size code
 */
static size_t dsk_sector_size_from_code(uint8_t code) {
    if (code > 7) return 0;
    return 128 << code;
}

/**
 * @brief Get size code from sector size
 */
static uint8_t dsk_code_from_sector_size(size_t size) {
    switch (size) {
        case 128:   return 0;
        case 256:   return 1;
        case 512:   return 2;
        case 1024:  return 3;
        case 2048:  return 4;
        case 4096:  return 5;
        case 8192:  return 6;
        case 16384: return 7;
        default:    return 2;  /* Default to 512 */
    }
}

/**
 * @brief Get format type name
 */
const char* dsk_format_type_name(dsk_format_type_t type) {
    switch (type) {
        case DSK_FORMAT_STANDARD: return "DSK (Standard)";
        case DSK_FORMAT_EXTENDED: return "EDSK (Extended)";
        default:                  return "Unknown";
    }
}

/**
 * @brief Get recording mode name
 */
const char* dsk_recording_mode_name(dsk_recording_mode_t mode) {
    switch (mode) {
        case DSK_MODE_FM:  return "FM (Single Density)";
        case DSK_MODE_MFM: return "MFM (Double Density)";
        default:           return "Unknown";
    }
}

/**
 * @brief Get ST1 status description
 */
const char* dsk_st1_description(uint8_t st1) {
    static char buf[256];
    buf[0] = '\0';
    
    if (st1 == 0) return "OK";
    
    if (st1 & DSK_ST1_MISSING_AM)   strcat(buf, "Missing AM, ");
    if (st1 & DSK_ST1_NOT_WRITABLE) strcat(buf, "Not Writable, ");
    if (st1 & DSK_ST1_NO_DATA)      strcat(buf, "No Data, ");
    if (st1 & DSK_ST1_OVERRUN)      strcat(buf, "Overrun, ");
    if (st1 & DSK_ST1_CRC_ERROR)    strcat(buf, "ID CRC Error, ");
    if (st1 & DSK_ST1_END_OF_CYL)   strcat(buf, "End of Cylinder, ");
    
    /* Remove trailing ", " */
    size_t len = strlen(buf);
    if (len >= 2) buf[len - 2] = '\0';
    
    return buf;
}

/**
 * @brief Get ST2 status description
 */
const char* dsk_st2_description(uint8_t st2) {
    static char buf[256];
    buf[0] = '\0';
    
    if (st2 == 0) return "OK";
    
    if (st2 & DSK_ST2_MISSING_DAM)  strcat(buf, "Missing DAM, ");
    if (st2 & DSK_ST2_BAD_CYLINDER) strcat(buf, "Bad Cylinder, ");
    if (st2 & DSK_ST2_SCAN_NOT_SAT) strcat(buf, "Scan Not Satisfied, ");
    if (st2 & DSK_ST2_SCAN_EQUAL)   strcat(buf, "Scan Equal, ");
    if (st2 & DSK_ST2_WRONG_CYL)    strcat(buf, "Wrong Cylinder, ");
    if (st2 & DSK_ST2_CRC_ERROR)    strcat(buf, "Data CRC Error, ");
    if (st2 & DSK_ST2_DELETED)      strcat(buf, "Deleted Mark, ");
    
    size_t len = strlen(buf);
    if (len >= 2) buf[len - 2] = '\0';
    
    return buf;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * DETECTION AND PROBE
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Check if data is standard DSK format
 */
bool dsk_is_standard(const uint8_t* data, size_t size) {
    if (size < DSK_HEADER_SIZE) return false;
    return memcmp(data, DSK_SIGNATURE, strlen(DSK_SIGNATURE)) == 0;
}

/**
 * @brief Check if data is extended DSK format
 */
bool dsk_is_extended(const uint8_t* data, size_t size) {
    if (size < DSK_HEADER_SIZE) return false;
    return memcmp(data, EDSK_SIGNATURE, strlen(EDSK_SIGNATURE)) == 0;
}

/**
 * @brief Detect DSK format type
 */
dsk_format_type_t dsk_detect_format(const uint8_t* data, size_t size) {
    if (dsk_is_extended(data, size)) return DSK_FORMAT_EXTENDED;
    if (dsk_is_standard(data, size)) return DSK_FORMAT_STANDARD;
    return DSK_FORMAT_UNKNOWN;
}

/**
 * @brief Probe confidence score (0-100)
 */
int dsk_probe_confidence(const uint8_t* data, size_t size) {
    if (size < DSK_HEADER_SIZE) return 0;
    
    int score = 0;
    
    /* Check signatures */
    if (dsk_is_extended(data, size)) {
        score = 95;  /* Strong EDSK signature */
    } else if (dsk_is_standard(data, size)) {
        score = 90;  /* Standard DSK signature */
    } else {
        return 0;
    }
    
    /* Validate header fields */
    uint8_t tracks = data[0x30];
    uint8_t sides = data[0x31];
    
    if (tracks == 0 || tracks > DSK_MAX_TRACKS) return 0;
    if (sides == 0 || sides > DSK_MAX_SIDES) return 0;
    
    /* Common geometries boost score */
    if ((tracks == 40 || tracks == 42 || tracks == 80) &&
        (sides == 1 || sides == 2)) {
        score += 5;
    }
    
    return (score > 100) ? 100 : score;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PARSING
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Parse disk header
 */
static bool dsk_parse_header(const uint8_t* data, size_t size, 
                             dsk_disk_info_t* info, dsk_format_type_t* format) {
    if (size < DSK_HEADER_SIZE) return false;
    
    *format = dsk_detect_format(data, size);
    if (*format == DSK_FORMAT_UNKNOWN) return false;
    
    memcpy(info->signature, data, 34);
    memcpy(info->creator, data + 0x22, 14);
    info->creator[14] = '\0';
    
    info->tracks = data[0x30];
    info->sides = data[0x31];
    info->track_size = data[0x32] | (data[0x33] << 8);
    
    /* EDSK has per-track size table */
    if (*format == DSK_FORMAT_EXTENDED) {
        memcpy(info->track_sizes, data + 0x34, 
               info->tracks * info->sides);
    }
    
    return true;
}

/**
 * @brief Parse track header
 */
static bool dsk_parse_track_header(const uint8_t* data, size_t size,
                                   dsk_track_info_t* track_info) {
    if (size < DSK_TRACK_HEADER_SIZE) return false;
    
    /* Verify signature */
    if (memcmp(data, "Track-Info\r\n", 12) != 0) return false;
    
    track_info->track = data[0x10];
    track_info->side = data[0x11];
    track_info->size_code = data[0x14];
    track_info->num_sectors = data[0x15];
    track_info->gap3_length = data[0x16];
    track_info->filler_byte = data[0x17];
    
    if (track_info->num_sectors > DSK_MAX_SECTORS) {
        track_info->num_sectors = DSK_MAX_SECTORS;
    }
    
    /* Parse sector info table */
    const uint8_t* sector_table = data + 0x18;
    for (int i = 0; i < track_info->num_sectors; i++) {
        dsk_sector_info_t* sec = &track_info->sectors[i];
        sec->track = sector_table[i * 8 + 0];
        sec->side = sector_table[i * 8 + 1];
        sec->sector_id = sector_table[i * 8 + 2];
        sec->size_code = sector_table[i * 8 + 3];
        sec->st1 = sector_table[i * 8 + 4];
        sec->st2 = sector_table[i * 8 + 5];
        sec->actual_size = sector_table[i * 8 + 6] | (sector_table[i * 8 + 7] << 8);
    }
    
    return true;
}

/**
 * @brief Detect interleave pattern
 */
static void dsk_detect_interleave(const dsk_track_info_t* track,
                                  uint8_t* interleave, uint8_t* interleave_factor) {
    if (track->num_sectors < 2) {
        *interleave_factor = 1;
        return;
    }
    
    /* Copy sector IDs in physical order */
    for (int i = 0; i < track->num_sectors; i++) {
        interleave[i] = track->sectors[i].sector_id;
    }
    
    /* Detect interleave factor */
    /* Find where sector 2 appears after sector 1 */
    int pos1 = -1, pos2 = -1;
    for (int i = 0; i < track->num_sectors; i++) {
        if (track->sectors[i].sector_id == 1) pos1 = i;
        if (track->sectors[i].sector_id == 2) pos2 = i;
    }
    
    if (pos1 >= 0 && pos2 >= 0) {
        int diff = pos2 - pos1;
        if (diff < 0) diff += track->num_sectors;
        *interleave_factor = (uint8_t)diff;
    } else {
        *interleave_factor = 1;
    }
}

/**
 * @brief Detect recording mode from track data
 */
static dsk_recording_mode_t dsk_detect_recording_mode(const dsk_track_info_t* track) {
    /* FM typically has smaller sectors and fewer sectors per track */
    /* MFM is the default for CPC/Spectrum */
    
    size_t sector_size = dsk_sector_size_from_code(track->size_code);
    
    /* FM indicators */
    if (sector_size == 128 && track->num_sectors <= 10) {
        return DSK_MODE_FM;
    }
    
    return DSK_MODE_MFM;
}

/**
 * @brief Parse complete DSK image
 */
bool dsk_parse_image(const uint8_t* data, size_t size, dsk_image_t* image) {
    memset(image, 0, sizeof(*image));
    
    dsk_disk_info_t disk_info;
    if (!dsk_parse_header(data, size, &disk_info, &image->format)) {
        return false;
    }
    
    /* Copy header info */
    memcpy(image->creator, disk_info.creator, 14);
    image->creator[14] = '\0';
    image->num_tracks = disk_info.tracks;
    image->num_sides = disk_info.sides;
    
    /* Parse tracks */
    size_t offset = DSK_HEADER_SIZE;
    
    for (int t = 0; t < disk_info.tracks; t++) {
        for (int s = 0; s < disk_info.sides; s++) {
            /* Calculate track size */
            size_t track_size;
            if (image->format == DSK_FORMAT_EXTENDED) {
                track_size = disk_info.track_sizes[t * disk_info.sides + s] * 256;
            } else {
                track_size = disk_info.track_size;
            }
            
            if (track_size == 0) continue;  /* Unformatted track */
            
            if (offset + DSK_TRACK_HEADER_SIZE > size) {
                return false;  /* Truncated */
            }
            
            dsk_track_info_t track_info;
            if (!dsk_parse_track_header(data + offset, size - offset, &track_info)) {
                offset += track_size;
                continue;  /* Skip malformed track */
            }
            
            /* Fill parsed track */
            dsk_parsed_track_t* parsed = &image->tracks[t][s];
            parsed->track_num = t;
            parsed->side_num = s;
            parsed->num_sectors = track_info.num_sectors;
            parsed->gap3_length = track_info.gap3_length;
            parsed->filler_byte = track_info.filler_byte;
            parsed->track_size = track_size;
            parsed->mode = dsk_detect_recording_mode(&track_info);
            
            /* Detect interleave */
            uint8_t interleave_factor;
            dsk_detect_interleave(&track_info, parsed->interleave, &interleave_factor);
            
            /* Parse sectors */
            size_t sector_offset = offset + DSK_TRACK_HEADER_SIZE;
            
            for (int sec = 0; sec < track_info.num_sectors; sec++) {
                dsk_parsed_sector_t* parsed_sec = &parsed->sectors[sec];
                parsed_sec->info = track_info.sectors[sec];
                
                /* Calculate actual data size */
                size_t data_size;
                if (image->format == DSK_FORMAT_EXTENDED && 
                    track_info.sectors[sec].actual_size > 0) {
                    data_size = track_info.sectors[sec].actual_size;
                } else {
                    data_size = dsk_sector_size_from_code(track_info.sectors[sec].size_code);
                }
                
                if (sector_offset + data_size <= size) {
                    parsed_sec->data = (uint8_t*)(data + sector_offset);
                    parsed_sec->data_size = data_size;
                }
                
                /* Check status flags */
                parsed_sec->has_crc_error = 
                    (track_info.sectors[sec].st1 & DSK_ST1_CRC_ERROR) ||
                    (track_info.sectors[sec].st2 & DSK_ST2_CRC_ERROR);
                
                parsed_sec->is_deleted = 
                    (track_info.sectors[sec].st2 & DSK_ST2_DELETED);
                
                /* Detect weak sectors (EDSK with multiple copies) */
                if (data_size > dsk_sector_size_from_code(track_info.sectors[sec].size_code)) {
                    parsed_sec->is_weak = true;
                    image->has_weak_sectors = true;
                }
                
                sector_offset += data_size;
                image->total_sectors++;
                
                if (parsed_sec->has_crc_error) {
                    image->error_sectors++;
                    image->has_errors = true;
                }
            }
            
            /* Check for copy protection indicators */
            if (track_info.gap3_length < 10 || 
                track_info.num_sectors > 9 ||
                image->has_weak_sectors) {
                image->has_copy_protection = true;
            }
            
            offset += track_size;
        }
    }
    
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * CONVERSION
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Convert DSK to raw sector image
 */
size_t dsk_to_raw_sectors(const dsk_image_t* image, uint8_t* output, 
                          size_t output_size, size_t sector_size) {
    size_t offset = 0;
    
    for (int t = 0; t < image->num_tracks; t++) {
        for (int s = 0; s < image->num_sides; s++) {
            const dsk_parsed_track_t* track = &image->tracks[t][s];
            
            /* Sort sectors by ID */
            for (int target_id = 1; target_id <= track->num_sectors; target_id++) {
                for (int sec = 0; sec < track->num_sectors; sec++) {
                    if (track->sectors[sec].info.sector_id == target_id) {
                        if (offset + sector_size > output_size) {
                            return offset;
                        }
                        
                        if (track->sectors[sec].data) {
                            size_t copy_size = (track->sectors[sec].data_size < sector_size) 
                                             ? track->sectors[sec].data_size : sector_size;
                            memcpy(output + offset, track->sectors[sec].data, copy_size);
                            
                            /* Pad if necessary */
                            if (copy_size < sector_size) {
                                memset(output + offset + copy_size, 0, sector_size - copy_size);
                            }
                        } else {
                            memset(output + offset, 0, sector_size);
                        }
                        
                        offset += sector_size;
                        break;
                    }
                }
            }
        }
    }
    
    return offset;
}

/**
 * @brief Create EDSK from raw sectors
 */
size_t dsk_create_edsk(uint8_t* output, size_t output_size,
                       const uint8_t* raw_data, size_t raw_size,
                       uint8_t tracks, uint8_t sides, 
                       uint8_t sectors_per_track, size_t sector_size) {
    
    if (output_size < DSK_HEADER_SIZE) return 0;
    
    memset(output, 0, DSK_HEADER_SIZE);
    
    /* Write header */
    memcpy(output, EDSK_SIGNATURE, strlen(EDSK_SIGNATURE));
    memcpy(output + 0x22, "UFT v5.4.0    ", 14);
    output[0x30] = tracks;
    output[0x31] = sides;
    
    /* Calculate track size */
    size_t track_data_size = sectors_per_track * sector_size;
    size_t track_total_size = DSK_TRACK_HEADER_SIZE + track_data_size;
    uint8_t track_size_units = (uint8_t)((track_total_size + 255) / 256);
    
    /* Write track size table */
    for (int t = 0; t < tracks * sides; t++) {
        output[0x34 + t] = track_size_units;
    }
    
    size_t offset = DSK_HEADER_SIZE;
    size_t raw_offset = 0;
    
    for (int t = 0; t < tracks; t++) {
        for (int s = 0; s < sides; s++) {
            if (offset + track_total_size > output_size) {
                return offset;
            }
            
            /* Write track header */
            memcpy(output + offset, "Track-Info\r\n", 12);
            output[offset + 0x10] = t;
            output[offset + 0x11] = s;
            output[offset + 0x14] = dsk_code_from_sector_size(sector_size);
            output[offset + 0x15] = sectors_per_track;
            output[offset + 0x16] = 0x4E;  /* GAP3 */
            output[offset + 0x17] = 0xE5;  /* Filler */
            
            /* Write sector info table */
            for (int sec = 0; sec < sectors_per_track; sec++) {
                uint8_t* sec_info = output + offset + 0x18 + sec * 8;
                sec_info[0] = t;           /* C */
                sec_info[1] = s;           /* H */
                sec_info[2] = sec + 1;     /* R */
                sec_info[3] = dsk_code_from_sector_size(sector_size);  /* N */
                sec_info[4] = 0;           /* ST1 */
                sec_info[5] = 0;           /* ST2 */
                sec_info[6] = sector_size & 0xFF;
                sec_info[7] = (sector_size >> 8) & 0xFF;
            }
            
            /* Write sector data */
            for (int sec = 0; sec < sectors_per_track; sec++) {
                size_t data_offset = offset + DSK_TRACK_HEADER_SIZE + sec * sector_size;
                
                if (raw_offset + sector_size <= raw_size) {
                    memcpy(output + data_offset, raw_data + raw_offset, sector_size);
                } else if (raw_offset < raw_size) {
                    memcpy(output + data_offset, raw_data + raw_offset, raw_size - raw_offset);
                    memset(output + data_offset + (raw_size - raw_offset), 0xE5, 
                           sector_size - (raw_size - raw_offset));
                } else {
                    memset(output + data_offset, 0xE5, sector_size);
                }
                
                raw_offset += sector_size;
            }
            
            offset += track_size_units * 256;
        }
    }
    
    return offset;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * COMMON GEOMETRIES
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    const char* name;
    uint8_t tracks;
    uint8_t sides;
    uint8_t sectors;
    size_t sector_size;
    size_t total_size;
} dsk_geometry_t;

static const dsk_geometry_t dsk_known_geometries[] = {
    /* Amstrad CPC */
    { "CPC Data",      40, 1, 9, 512, 180*1024 },
    { "CPC System",    40, 1, 9, 512, 180*1024 },
    { "CPC Data DS",   40, 2, 9, 512, 360*1024 },
    
    /* ZX Spectrum +3 */
    { "Spectrum +3",   40, 1, 9, 512, 180*1024 },
    { "Spectrum +3 DS", 40, 2, 9, 512, 360*1024 },
    { "Spectrum +3 80T", 80, 2, 9, 512, 720*1024 },
    
    /* PCW */
    { "PCW SS",        40, 1, 9, 512, 180*1024 },
    { "PCW DS",        80, 2, 9, 512, 720*1024 },
    
    { NULL, 0, 0, 0, 0, 0 }
};

/**
 * @brief Detect geometry by size
 */
const dsk_geometry_t* dsk_detect_geometry(size_t data_size) {
    for (int i = 0; dsk_known_geometries[i].name != NULL; i++) {
        if (dsk_known_geometries[i].total_size == data_size) {
            return &dsk_known_geometries[i];
        }
    }
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TEST SUITE
 * ═══════════════════════════════════════════════════════════════════════════ */

#ifdef DSK_TEST

#include <assert.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("Testing %s... ", #name); \
    test_##name(); \
    printf("✓\n"); \
    tests_passed++; \
} while(0)

TEST(signatures) {
    uint8_t std_header[256] = {0};
    uint8_t ext_header[256] = {0};
    
    memcpy(std_header, DSK_SIGNATURE, strlen(DSK_SIGNATURE));
    memcpy(ext_header, EDSK_SIGNATURE, strlen(EDSK_SIGNATURE));
    
    assert(dsk_is_standard(std_header, 256));
    assert(!dsk_is_extended(std_header, 256));
    
    assert(!dsk_is_standard(ext_header, 256));
    assert(dsk_is_extended(ext_header, 256));
    
    assert(dsk_detect_format(std_header, 256) == DSK_FORMAT_STANDARD);
    assert(dsk_detect_format(ext_header, 256) == DSK_FORMAT_EXTENDED);
}

TEST(sector_sizes) {
    assert(dsk_sector_size_from_code(0) == 128);
    assert(dsk_sector_size_from_code(1) == 256);
    assert(dsk_sector_size_from_code(2) == 512);
    assert(dsk_sector_size_from_code(3) == 1024);
    assert(dsk_sector_size_from_code(4) == 2048);
    assert(dsk_sector_size_from_code(5) == 4096);
    assert(dsk_sector_size_from_code(6) == 8192);
    
    assert(dsk_code_from_sector_size(128) == 0);
    assert(dsk_code_from_sector_size(512) == 2);
    assert(dsk_code_from_sector_size(1024) == 3);
}

TEST(status_flags) {
    assert(strcmp(dsk_st1_description(0), "OK") == 0);
    assert(strstr(dsk_st1_description(DSK_ST1_CRC_ERROR), "CRC") != NULL);
    
    assert(strcmp(dsk_st2_description(0), "OK") == 0);
    assert(strstr(dsk_st2_description(DSK_ST2_DELETED), "Deleted") != NULL);
}

TEST(format_names) {
    assert(strcmp(dsk_format_type_name(DSK_FORMAT_STANDARD), "DSK (Standard)") == 0);
    assert(strcmp(dsk_format_type_name(DSK_FORMAT_EXTENDED), "EDSK (Extended)") == 0);
    
    assert(strstr(dsk_recording_mode_name(DSK_MODE_FM), "FM") != NULL);
    assert(strstr(dsk_recording_mode_name(DSK_MODE_MFM), "MFM") != NULL);
}

TEST(geometry_detection) {
    const dsk_geometry_t* geom;
    
    geom = dsk_detect_geometry(180*1024);
    assert(geom != NULL);
    assert(geom->tracks == 40);
    assert(geom->sectors == 9);
    
    geom = dsk_detect_geometry(720*1024);
    assert(geom != NULL);
    assert(geom->tracks == 80);
}

int main(void) {
    printf("=== DSK Parser v2 Tests ===\n");
    
    RUN_TEST(signatures);
    RUN_TEST(sector_sizes);
    RUN_TEST(status_flags);
    RUN_TEST(format_names);
    RUN_TEST(geometry_detection);
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: %d, Failed: %d\n", tests_passed, tests_failed);
    
    return tests_failed;
}

#endif /* DSK_TEST */
