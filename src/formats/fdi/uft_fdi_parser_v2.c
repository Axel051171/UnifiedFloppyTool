/**
 * @file uft_fdi_parser_v2.c
 * @brief GOD MODE FDI Parser v2 - Formatted Disk Image
 * 
 * Supports:
 * - FDI format (Russian TR-DOS, various)
 * - UDI format (Ultra Disk Image)
 * - Multiple sector sizes
 * - CRC error flags
 * - Track metadata
 * - Head/cylinder mapping
 * 
 * FDI is used for ZX Spectrum clones, Russian computers (Vector-06C, etc.)
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

#define FDI_HEADER_SIZE        14
#define FDI_TRACK_HEADER_SIZE  7
#define FDI_SECTOR_HEADER_SIZE 7
#define FDI_MAX_TRACKS         256
#define FDI_MAX_HEADS          2
#define FDI_MAX_SECTORS        64

/* Magic signatures */
static const char FDI_SIGNATURE[] = "FDI";
static const char UDI_SIGNATURE[] = "UDI!";

/* ═══════════════════════════════════════════════════════════════════════════
 * DATA STRUCTURES
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief FDI format type
 */
typedef enum {
    FDI_FORMAT_UNKNOWN = 0,
    FDI_FORMAT_FDI,          /* Standard FDI */
    FDI_FORMAT_UDI           /* Ultra Disk Image */
} fdi_format_type_t;

/**
 * @brief Sector flags
 */
typedef enum {
    FDI_FLAG_NONE        = 0x00,
    FDI_FLAG_NO_DATA     = 0x01,  /* Sector has no data */
    FDI_FLAG_DELETED     = 0x02,  /* Deleted data mark */
    FDI_FLAG_CRC_ERROR   = 0x04,  /* CRC error */
    FDI_FLAG_NO_ID       = 0x08,  /* No ID field */
    FDI_FLAG_NO_DAM      = 0x10,  /* No data address mark */
    FDI_FLAG_WEAK        = 0x20   /* Weak/fuzzy bits */
} fdi_sector_flags_t;

/**
 * @brief Recording mode
 */
typedef enum {
    FDI_MODE_FM = 0,    /* Single density */
    FDI_MODE_MFM = 1    /* Double density */
} fdi_recording_mode_t;

/**
 * @brief Disk header
 */
typedef struct {
    char           signature[4];
    uint8_t        write_protect;
    uint16_t       num_cylinders;
    uint16_t       num_heads;
    uint16_t       description_offset;
    uint16_t       data_offset;
    uint16_t       extra_info_size;
} fdi_disk_header_t;

/**
 * @brief Track header (7 bytes)
 */
typedef struct {
    uint32_t       offset;            /* Offset to track data */
    uint16_t       reserved;
    uint8_t        num_sectors;
} fdi_track_header_t;

/**
 * @brief Sector header (7 bytes)
 */
typedef struct {
    uint8_t        cylinder;          /* C - Physical cylinder */
    uint8_t        head;              /* H - Physical head */
    uint8_t        sector_id;         /* R - Sector ID */
    uint8_t        size_code;         /* N - Size code */
    uint8_t        flags;             /* Status flags */
    uint16_t       data_offset;       /* Offset within track */
} fdi_sector_header_t;

/**
 * @brief Parsed sector
 */
typedef struct {
    fdi_sector_header_t header;
    uint8_t*       data;              /* Sector data (not owned) */
    size_t         data_size;
    bool           has_crc_error;
    bool           is_deleted;
    bool           is_weak;
} fdi_parsed_sector_t;

/**
 * @brief Parsed track
 */
typedef struct {
    uint8_t        cylinder;
    uint8_t        head;
    uint8_t        num_sectors;
    fdi_recording_mode_t mode;
    fdi_parsed_sector_t sectors[FDI_MAX_SECTORS];
} fdi_parsed_track_t;

/**
 * @brief Full FDI image
 */
typedef struct {
    fdi_format_type_t format;
    uint16_t       num_cylinders;
    uint16_t       num_heads;
    bool           write_protected;
    char           description[256];
    fdi_parsed_track_t tracks[FDI_MAX_TRACKS][FDI_MAX_HEADS];
    
    /* Statistics */
    size_t         total_sectors;
    size_t         error_sectors;
    bool           has_errors;
    bool           has_deleted;
    bool           has_weak;
} fdi_image_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * UTILITY FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read 16-bit little-endian value
 */
static uint16_t fdi_read_le16(const uint8_t* p) {
    return p[0] | (p[1] << 8);
}

/**
 * @brief Read 32-bit little-endian value
 */
static uint32_t fdi_read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

/**
 * @brief Get sector size from code
 */
static size_t fdi_sector_size_from_code(uint8_t code) {
    if (code > 7) return 0;
    return 128 << code;
}

/**
 * @brief Get format type name
 */
const char* fdi_format_type_name(fdi_format_type_t type) {
    switch (type) {
        case FDI_FORMAT_FDI: return "FDI";
        case FDI_FORMAT_UDI: return "UDI (Ultra Disk Image)";
        default:             return "Unknown";
    }
}

/**
 * @brief Get recording mode name
 */
const char* fdi_recording_mode_name(fdi_recording_mode_t mode) {
    switch (mode) {
        case FDI_MODE_FM:  return "FM";
        case FDI_MODE_MFM: return "MFM";
        default:           return "Unknown";
    }
}

/**
 * @brief Get sector flag description
 */
const char* fdi_flags_description(uint8_t flags) {
    static char buf[256];
    buf[0] = '\0';
    
    if (flags == 0) return "OK";
    
    if (flags & FDI_FLAG_NO_DATA)   strncat(buf, "No Data, ", sizeof(buf) - strlen(buf) - 1);
    if (flags & FDI_FLAG_DELETED)   strncat(buf, "Deleted, ", sizeof(buf) - strlen(buf) - 1);
    if (flags & FDI_FLAG_CRC_ERROR) strncat(buf, "CRC Error, ", sizeof(buf) - strlen(buf) - 1);
    if (flags & FDI_FLAG_NO_ID)     strncat(buf, "No ID, ", sizeof(buf) - strlen(buf) - 1);
    if (flags & FDI_FLAG_NO_DAM)    strncat(buf, "No DAM, ", sizeof(buf) - strlen(buf) - 1);
    if (flags & FDI_FLAG_WEAK)      strncat(buf, "Weak, ", sizeof(buf) - strlen(buf) - 1);
    
    size_t len = strlen(buf);
    if (len >= 2) buf[len - 2] = '\0';
    
    return buf;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * DETECTION
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Check for FDI signature
 */
bool fdi_is_fdi(const uint8_t* data, size_t size) {
    if (size < FDI_HEADER_SIZE) return false;
    return memcmp(data, FDI_SIGNATURE, 3) == 0;
}

/**
 * @brief Check for UDI signature
 */
bool fdi_is_udi(const uint8_t* data, size_t size) {
    if (size < 16) return false;
    return memcmp(data, UDI_SIGNATURE, 4) == 0;
}

/**
 * @brief Detect format type
 */
fdi_format_type_t fdi_detect_format(const uint8_t* data, size_t size) {
    if (fdi_is_udi(data, size)) return FDI_FORMAT_UDI;
    if (fdi_is_fdi(data, size)) return FDI_FORMAT_FDI;
    return FDI_FORMAT_UNKNOWN;
}

/**
 * @brief Probe confidence (0-100)
 */
int fdi_probe_confidence(const uint8_t* data, size_t size) {
    if (size < FDI_HEADER_SIZE) return 0;
    
    fdi_format_type_t format = fdi_detect_format(data, size);
    if (format == FDI_FORMAT_UNKNOWN) return 0;
    
    int score = 90;
    
    /* Validate header fields */
    uint16_t cylinders = fdi_read_le16(data + 4);
    uint16_t heads = fdi_read_le16(data + 6);
    
    if (cylinders == 0 || cylinders > FDI_MAX_TRACKS) return 0;
    if (heads == 0 || heads > FDI_MAX_HEADS) return 0;
    
    /* Common geometries */
    if ((cylinders == 40 || cylinders == 80) && (heads == 1 || heads == 2)) {
        score += 5;
    }
    
    return (score > 100) ? 100 : score;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PARSING
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Parse FDI header
 */
static bool fdi_parse_header(const uint8_t* data, size_t size,
                             fdi_disk_header_t* header) {
    if (size < FDI_HEADER_SIZE) return false;
    
    memcpy(header->signature, data, 3);
    header->signature[3] = '\0';
    
    header->write_protect = data[3];
    header->num_cylinders = fdi_read_le16(data + 4);
    header->num_heads = fdi_read_le16(data + 6);
    header->description_offset = fdi_read_le16(data + 8);
    header->data_offset = fdi_read_le16(data + 10);
    header->extra_info_size = fdi_read_le16(data + 12);
    
    return true;
}

/**
 * @brief Parse track table
 */
static bool fdi_parse_track_table(const uint8_t* data, size_t size,
                                  size_t offset, int num_tracks,
                                  fdi_track_header_t* tracks) {
    for (int i = 0; i < num_tracks; i++) {
        size_t pos = offset + i * FDI_TRACK_HEADER_SIZE;
        if (pos + FDI_TRACK_HEADER_SIZE > size) return false;
        
        tracks[i].offset = fdi_read_le32(data + pos);
        tracks[i].reserved = fdi_read_le16(data + pos + 4);
        tracks[i].num_sectors = data[pos + 6];
    }
    
    return true;
}

/**
 * @brief Parse full FDI image
 */
bool fdi_parse_image(const uint8_t* data, size_t size, fdi_image_t* image) {
    memset(image, 0, sizeof(*image));
    
    image->format = fdi_detect_format(data, size);
    if (image->format == FDI_FORMAT_UNKNOWN) return false;
    
    fdi_disk_header_t header;
    if (!fdi_parse_header(data, size, &header)) return false;
    
    image->num_cylinders = header.num_cylinders;
    image->num_heads = header.num_heads;
    image->write_protected = (header.write_protect != 0);
    
    /* Read description if present */
    if (header.description_offset > 0 && 
        header.description_offset < size) {
        size_t desc_len = header.data_offset - header.description_offset;
        if (desc_len > sizeof(image->description) - 1) {
            desc_len = sizeof(image->description) - 1;
        }
        memcpy(image->description, data + header.description_offset, desc_len);
    }
    
    /* Track table starts after header */
    size_t track_table_offset = FDI_HEADER_SIZE;
    int total_tracks = image->num_cylinders * image->num_heads;
    
    fdi_track_header_t* track_headers = calloc(total_tracks, sizeof(fdi_track_header_t));
    if (!track_headers) return false;
    
    if (!fdi_parse_track_table(data, size, track_table_offset, 
                               total_tracks, track_headers)) {
        free(track_headers);
        return false;
    }
    
    /* Parse each track */
    for (int cyl = 0; cyl < image->num_cylinders; cyl++) {
        for (int head = 0; head < image->num_heads; head++) {
            int track_idx = cyl * image->num_heads + head;
            fdi_track_header_t* th = &track_headers[track_idx];
            
            if (th->offset == 0 || th->num_sectors == 0) continue;
            
            fdi_parsed_track_t* track = &image->tracks[cyl][head];
            track->cylinder = cyl;
            track->head = head;
            track->num_sectors = th->num_sectors;
            track->mode = FDI_MODE_MFM;  /* Default */
            
            /* Parse sector headers for this track */
            size_t sector_table_offset = header.data_offset + th->offset;
            
            for (int s = 0; s < th->num_sectors && s < FDI_MAX_SECTORS; s++) {
                size_t sec_offset = sector_table_offset + s * FDI_SECTOR_HEADER_SIZE;
                if (sec_offset + FDI_SECTOR_HEADER_SIZE > size) break;
                
                fdi_parsed_sector_t* sector = &track->sectors[s];
                sector->header.cylinder = data[sec_offset + 0];
                sector->header.head = data[sec_offset + 1];
                sector->header.sector_id = data[sec_offset + 2];
                sector->header.size_code = data[sec_offset + 3];
                sector->header.flags = data[sec_offset + 4];
                sector->header.data_offset = fdi_read_le16(data + sec_offset + 5);
                
                sector->data_size = fdi_sector_size_from_code(sector->header.size_code);
                
                /* Check flags */
                sector->has_crc_error = (sector->header.flags & FDI_FLAG_CRC_ERROR) != 0;
                sector->is_deleted = (sector->header.flags & FDI_FLAG_DELETED) != 0;
                sector->is_weak = (sector->header.flags & FDI_FLAG_WEAK) != 0;
                
                /* Point to data if available */
                if (!(sector->header.flags & FDI_FLAG_NO_DATA)) {
                    size_t data_pos = header.data_offset + th->offset + 
                                      th->num_sectors * FDI_SECTOR_HEADER_SIZE +
                                      sector->header.data_offset;
                    if (data_pos + sector->data_size <= size) {
                        sector->data = (uint8_t*)(data + data_pos);
                    }
                }
                
                /* Update statistics */
                image->total_sectors++;
                if (sector->has_crc_error) {
                    image->error_sectors++;
                    image->has_errors = true;
                }
                if (sector->is_deleted) image->has_deleted = true;
                if (sector->is_weak) image->has_weak = true;
            }
        }
    }
    
    free(track_headers);
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * CONVERSION
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Convert FDI to raw sector image
 */
size_t fdi_to_raw_sectors(const fdi_image_t* image, uint8_t* output,
                          size_t output_size, size_t sector_size) {
    size_t offset = 0;
    
    for (int cyl = 0; cyl < image->num_cylinders; cyl++) {
        for (int head = 0; head < image->num_heads; head++) {
            const fdi_parsed_track_t* track = &image->tracks[cyl][head];
            
            /* Sort by sector ID */
            for (int target_id = 1; target_id <= track->num_sectors; target_id++) {
                for (int s = 0; s < track->num_sectors; s++) {
                    if (track->sectors[s].header.sector_id == target_id) {
                        if (offset + sector_size > output_size) {
                            return offset;
                        }
                        
                        if (track->sectors[s].data) {
                            size_t copy_size = (track->sectors[s].data_size < sector_size)
                                             ? track->sectors[s].data_size : sector_size;
                            memcpy(output + offset, track->sectors[s].data, copy_size);
                            if (copy_size < sector_size) {
                                memset(output + offset + copy_size, 0, 
                                       sector_size - copy_size);
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
 * @brief Create FDI from raw sectors
 */
size_t fdi_create_from_raw(uint8_t* output, size_t output_size,
                           const uint8_t* raw_data, size_t raw_size,
                           uint16_t cylinders, uint16_t heads,
                           uint8_t sectors_per_track, size_t sector_size) {
    
    int total_tracks = cylinders * heads;
    size_t track_table_size = total_tracks * FDI_TRACK_HEADER_SIZE;
    size_t header_total = FDI_HEADER_SIZE + track_table_size;
    
    if (output_size < header_total) return 0;
    
    memset(output, 0, output_size);
    
    /* Write header */
    memcpy(output, FDI_SIGNATURE, 3);
    output[3] = 0;  /* Not write protected */
    output[4] = cylinders & 0xFF;
    output[5] = (cylinders >> 8) & 0xFF;
    output[6] = heads & 0xFF;
    output[7] = (heads >> 8) & 0xFF;
    output[8] = 0;  /* No description */
    output[9] = 0;
    output[10] = header_total & 0xFF;
    output[11] = (header_total >> 8) & 0xFF;
    output[12] = 0;  /* No extra info */
    output[13] = 0;
    
    /* Calculate track data size */
    size_t sector_table_size = sectors_per_track * FDI_SECTOR_HEADER_SIZE;
    size_t track_data_size = sectors_per_track * sector_size;
    
    size_t offset = header_total;
    size_t raw_offset = 0;
    
    /* Write track table */
    for (int t = 0; t < total_tracks; t++) {
        size_t table_pos = FDI_HEADER_SIZE + t * FDI_TRACK_HEADER_SIZE;
        uint32_t track_offset = offset - header_total;
        
        output[table_pos + 0] = track_offset & 0xFF;
        output[table_pos + 1] = (track_offset >> 8) & 0xFF;
        output[table_pos + 2] = (track_offset >> 16) & 0xFF;
        output[table_pos + 3] = (track_offset >> 24) & 0xFF;
        output[table_pos + 4] = 0;
        output[table_pos + 5] = 0;
        output[table_pos + 6] = sectors_per_track;
        
        /* Write sector headers */
        int cyl = t / heads;
        int head = t % heads;
        
        for (int s = 0; s < sectors_per_track; s++) {
            size_t sec_pos = offset + s * FDI_SECTOR_HEADER_SIZE;
            if (sec_pos + FDI_SECTOR_HEADER_SIZE > output_size) return offset;
            
            output[sec_pos + 0] = cyl;
            output[sec_pos + 1] = head;
            output[sec_pos + 2] = s + 1;  /* Sector ID */
            output[sec_pos + 3] = (sector_size == 512) ? 2 : 
                                  (sector_size == 256) ? 1 : 0;
            output[sec_pos + 4] = 0;  /* No flags */
            uint16_t data_off = s * sector_size;
            output[sec_pos + 5] = data_off & 0xFF;
            output[sec_pos + 6] = (data_off >> 8) & 0xFF;
        }
        
        /* Write sector data */
        size_t data_offset = offset + sector_table_size;
        for (int s = 0; s < sectors_per_track; s++) {
            if (data_offset + sector_size > output_size) return offset;
            
            if (raw_offset + sector_size <= raw_size) {
                memcpy(output + data_offset, raw_data + raw_offset, sector_size);
            } else {
                memset(output + data_offset, 0xE5, sector_size);
            }
            
            data_offset += sector_size;
            raw_offset += sector_size;
        }
        
        offset = data_offset;
    }
    
    return offset;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * COMMON GEOMETRIES
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    const char* name;
    uint16_t cylinders;
    uint16_t heads;
    uint8_t sectors;
    size_t sector_size;
    const char* system;
} fdi_geometry_t;

static const fdi_geometry_t fdi_known_geometries[] = {
    { "TR-DOS 40T SS", 40, 1, 16, 256, "ZX Spectrum" },
    { "TR-DOS 40T DS", 40, 2, 16, 256, "ZX Spectrum" },
    { "TR-DOS 80T SS", 80, 1, 16, 256, "ZX Spectrum" },
    { "TR-DOS 80T DS", 80, 2, 16, 256, "ZX Spectrum" },
    { "Vector-06C",    80, 2, 5,  1024, "Vector-06C" },
    { "Korvet PK",     80, 2, 9,  512, "Korvet" },
    { "MS-DOS",        80, 2, 9,  512, "PC" },
    { NULL, 0, 0, 0, 0, NULL }
};

/**
 * @brief Detect system by geometry
 */
const char* fdi_detect_system(uint16_t cylinders, uint16_t heads, 
                              uint8_t sectors, size_t sector_size) {
    for (int i = 0; fdi_known_geometries[i].name; i++) {
        if (fdi_known_geometries[i].cylinders == cylinders &&
            fdi_known_geometries[i].heads == heads &&
            fdi_known_geometries[i].sectors == sectors &&
            fdi_known_geometries[i].sector_size == sector_size) {
            return fdi_known_geometries[i].system;
        }
    }
    return "Unknown";
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TEST SUITE
 * ═══════════════════════════════════════════════════════════════════════════ */

#ifdef FDI_TEST

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
    uint8_t fdi_data[32] = {0};
    uint8_t udi_data[32] = {0};
    
    memcpy(fdi_data, FDI_SIGNATURE, 3);
    memcpy(udi_data, UDI_SIGNATURE, 4);
    
    assert(fdi_is_fdi(fdi_data, 32));
    assert(!fdi_is_udi(fdi_data, 32));
    
    assert(!fdi_is_fdi(udi_data, 32));
    assert(fdi_is_udi(udi_data, 32));
    
    assert(fdi_detect_format(fdi_data, 32) == FDI_FORMAT_FDI);
    assert(fdi_detect_format(udi_data, 32) == FDI_FORMAT_UDI);
}

TEST(sector_sizes) {
    assert(fdi_sector_size_from_code(0) == 128);
    assert(fdi_sector_size_from_code(1) == 256);
    assert(fdi_sector_size_from_code(2) == 512);
    assert(fdi_sector_size_from_code(3) == 1024);
    assert(fdi_sector_size_from_code(4) == 2048);
}

TEST(format_names) {
    assert(strcmp(fdi_format_type_name(FDI_FORMAT_FDI), "FDI") == 0);
    assert(strstr(fdi_format_type_name(FDI_FORMAT_UDI), "UDI") != NULL);
    
    assert(strcmp(fdi_recording_mode_name(FDI_MODE_FM), "FM") == 0);
    assert(strcmp(fdi_recording_mode_name(FDI_MODE_MFM), "MFM") == 0);
}

TEST(flags) {
    assert(strcmp(fdi_flags_description(0), "OK") == 0);
    assert(strstr(fdi_flags_description(FDI_FLAG_CRC_ERROR), "CRC") != NULL);
    assert(strstr(fdi_flags_description(FDI_FLAG_DELETED), "Deleted") != NULL);
    assert(strstr(fdi_flags_description(FDI_FLAG_WEAK), "Weak") != NULL);
}

TEST(system_detection) {
    assert(strcmp(fdi_detect_system(80, 2, 16, 256), "ZX Spectrum") == 0);
    assert(strcmp(fdi_detect_system(80, 2, 9, 512), "Korvet") == 0);  /* Korvet has priority for FDI */
    assert(strcmp(fdi_detect_system(80, 2, 5, 1024), "Vector-06C") == 0);
}

int main(void) {
    printf("=== FDI Parser v2 Tests ===\n");
    
    RUN_TEST(signatures);
    RUN_TEST(sector_sizes);
    RUN_TEST(format_names);
    RUN_TEST(flags);
    RUN_TEST(system_detection);
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: %d, Failed: %d\n", tests_passed, tests_failed);
    
    return tests_failed;
}

#endif /* FDI_TEST */
