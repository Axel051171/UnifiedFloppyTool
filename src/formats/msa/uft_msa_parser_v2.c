/**
 * @file uft_msa_parser_v2.c
 * @brief GOD MODE MSA Parser v2 - Magic Shadow Archiver (Atari ST)
 * 
 * Supports:
 * - MSA compressed disk images
 * - RLE compression/decompression
 * - Track-by-track storage
 * - Geometry detection (SS/DS, SD/DD/ED)
 * - Raw ST image conversion
 * 
 * MSA is the standard archival format for Atari ST floppy disks.
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

#define MSA_MAGIC           0x0E0F
#define MSA_HEADER_SIZE     10
#define MSA_MAX_TRACKS      86
#define MSA_MAX_SIDES       2
#define MSA_SECTOR_SIZE     512
#define MSA_RLE_MARKER      0xE5

/* ═══════════════════════════════════════════════════════════════════════════
 * DATA STRUCTURES
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief MSA disk type
 */
typedef enum {
    MSA_TYPE_UNKNOWN = 0,
    MSA_TYPE_SS_SD,     /* Single side, single density (360KB) */
    MSA_TYPE_SS_DD,     /* Single side, double density (400KB) */
    MSA_TYPE_DS_SD,     /* Double side, single density (720KB) */
    MSA_TYPE_DS_DD,     /* Double side, double density (800KB) */
    MSA_TYPE_DS_ED,     /* Double side, extended density (1.44MB) */
    MSA_TYPE_DS_HD      /* Double side, high density (1.44MB) */
} msa_disk_type_t;

/**
 * @brief MSA header (10 bytes, big-endian)
 */
typedef struct {
    uint16_t magic;         /* 0x0E0F */
    uint16_t sectors_per_track;
    uint16_t sides;         /* 0 = SS, 1 = DS */
    uint16_t start_track;
    uint16_t end_track;
} msa_header_t;

/**
 * @brief Track data descriptor
 */
typedef struct {
    uint16_t compressed_size;
    uint16_t uncompressed_size;
    bool     is_compressed;
    size_t   data_offset;
} msa_track_desc_t;

/**
 * @brief Parsed MSA image
 */
typedef struct {
    msa_header_t header;
    msa_disk_type_t type;
    
    uint16_t num_tracks;
    uint16_t num_sides;
    uint16_t sectors_per_track;
    size_t   track_size;
    size_t   total_size;      /* Uncompressed size */
    
    msa_track_desc_t tracks[MSA_MAX_TRACKS][MSA_MAX_SIDES];
    
    /* Compression stats */
    size_t   compressed_tracks;
    size_t   uncompressed_tracks;
    double   compression_ratio;
} msa_image_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * UTILITY FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read 16-bit big-endian value
 */
static uint16_t msa_read_be16(const uint8_t* p) {
    return (p[0] << 8) | p[1];
}

/**
 * @brief Write 16-bit big-endian value
 */
static void msa_write_be16(uint8_t* p, uint16_t value) {
    p[0] = (value >> 8) & 0xFF;
    p[1] = value & 0xFF;
}

/**
 * @brief Get disk type name
 */
const char* msa_disk_type_name(msa_disk_type_t type) {
    switch (type) {
        case MSA_TYPE_SS_SD: return "SS/SD (360KB)";
        case MSA_TYPE_SS_DD: return "SS/DD (400KB)";
        case MSA_TYPE_DS_SD: return "DS/SD (720KB)";
        case MSA_TYPE_DS_DD: return "DS/DD (800KB)";
        case MSA_TYPE_DS_ED: return "DS/ED (1.44MB)";
        case MSA_TYPE_DS_HD: return "DS/HD (1.44MB)";
        default:             return "Unknown";
    }
}

/**
 * @brief Detect disk type from geometry
 */
msa_disk_type_t msa_detect_type(uint16_t sides, uint16_t sectors_per_track,
                                 uint16_t tracks) {
    size_t total_sectors = (sides + 1) * sectors_per_track * tracks;
    size_t total_size = total_sectors * MSA_SECTOR_SIZE;
    
    /* Single sided */
    if (sides == 0) {
        if (sectors_per_track == 9) return MSA_TYPE_SS_SD;
        if (sectors_per_track == 10) return MSA_TYPE_SS_DD;
    }
    
    /* Double sided */
    if (sides == 1) {
        if (total_size >= 1400000) return MSA_TYPE_DS_HD;
        if (sectors_per_track >= 10) return MSA_TYPE_DS_DD;
        return MSA_TYPE_DS_SD;
    }
    
    return MSA_TYPE_UNKNOWN;
}

/**
 * @brief Get expected track size
 */
size_t msa_track_size(uint16_t sectors_per_track) {
    return sectors_per_track * MSA_SECTOR_SIZE;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * RLE COMPRESSION
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Decompress RLE track data
 * 
 * MSA RLE format:
 * - 0xE5 <count_hi> <count_lo> <byte> = repeat <byte> <count> times
 * - Other bytes are literal
 */
size_t msa_decompress_track(const uint8_t* input, size_t input_size,
                            uint8_t* output, size_t output_size) {
    size_t in_pos = 0;
    size_t out_pos = 0;
    
    while (in_pos < input_size && out_pos < output_size) {
        uint8_t byte = input[in_pos++];
        
        if (byte == MSA_RLE_MARKER && in_pos + 2 < input_size) {
            /* RLE sequence */
            uint16_t count = msa_read_be16(input + in_pos);
            in_pos += 2;
            uint8_t value = input[in_pos++];
            
            while (count > 0 && out_pos < output_size) {
                output[out_pos++] = value;
                count--;
            }
        } else {
            /* Literal byte */
            output[out_pos++] = byte;
        }
    }
    
    return out_pos;
}

/**
 * @brief Compress track data using RLE
 */
size_t msa_compress_track(const uint8_t* input, size_t input_size,
                          uint8_t* output, size_t output_size) {
    size_t in_pos = 0;
    size_t out_pos = 0;
    
    while (in_pos < input_size) {
        uint8_t byte = input[in_pos];
        
        /* Count consecutive bytes */
        size_t run_length = 1;
        while (in_pos + run_length < input_size &&
               input[in_pos + run_length] == byte &&
               run_length < 65535) {
            run_length++;
        }
        
        /* Decide: RLE vs literal */
        /* RLE costs 4 bytes, so only use for runs >= 5 */
        /* Or runs of 0xE5 of any length (to avoid ambiguity) */
        if ((run_length >= 5) || (byte == MSA_RLE_MARKER && run_length >= 1)) {
            if (out_pos + 4 > output_size) break;
            
            output[out_pos++] = MSA_RLE_MARKER;
            msa_write_be16(output + out_pos, (uint16_t)run_length);
            out_pos += 2;
            output[out_pos++] = byte;
        } else {
            /* Literal */
            for (size_t i = 0; i < run_length && out_pos < output_size; i++) {
                output[out_pos++] = byte;
            }
        }
        
        in_pos += run_length;
    }
    
    return out_pos;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * DETECTION
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Check for MSA signature
 */
bool msa_is_msa(const uint8_t* data, size_t size) {
    if (size < MSA_HEADER_SIZE) return false;
    return msa_read_be16(data) == MSA_MAGIC;
}

/**
 * @brief Probe confidence (0-100)
 */
int msa_probe_confidence(const uint8_t* data, size_t size) {
    if (!msa_is_msa(data, size)) return 0;
    
    int score = 90;
    
    /* Validate header fields */
    uint16_t sectors = msa_read_be16(data + 2);
    uint16_t sides = msa_read_be16(data + 4);
    uint16_t start_track = msa_read_be16(data + 6);
    uint16_t end_track = msa_read_be16(data + 8);
    
    /* Sanity checks */
    if (sectors == 0 || sectors > 21) return 0;
    if (sides > 1) return 0;
    if (end_track < start_track) return 0;
    if (end_track >= MSA_MAX_TRACKS) return 0;
    
    /* Common geometries boost score */
    if ((sectors == 9 || sectors == 10 || sectors == 18) &&
        (start_track == 0) &&
        (end_track == 79 || end_track == 80)) {
        score += 10;
    }
    
    return (score > 100) ? 100 : score;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PARSING
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Parse MSA header
 */
static bool msa_parse_header(const uint8_t* data, size_t size,
                             msa_header_t* header) {
    if (size < MSA_HEADER_SIZE) return false;
    if (msa_read_be16(data) != MSA_MAGIC) return false;
    
    header->magic = MSA_MAGIC;
    header->sectors_per_track = msa_read_be16(data + 2);
    header->sides = msa_read_be16(data + 4);
    header->start_track = msa_read_be16(data + 6);
    header->end_track = msa_read_be16(data + 8);
    
    return true;
}

/**
 * @brief Parse full MSA image
 */
bool msa_parse_image(const uint8_t* data, size_t size, msa_image_t* image) {
    memset(image, 0, sizeof(*image));
    
    if (!msa_parse_header(data, size, &image->header)) {
        return false;
    }
    
    image->sectors_per_track = image->header.sectors_per_track;
    image->num_sides = image->header.sides + 1;  /* 0 = 1 side, 1 = 2 sides */
    image->num_tracks = image->header.end_track - image->header.start_track + 1;
    image->track_size = msa_track_size(image->sectors_per_track);
    
    image->type = msa_detect_type(image->header.sides, 
                                   image->sectors_per_track,
                                   image->num_tracks);
    
    /* Parse track table */
    size_t offset = MSA_HEADER_SIZE;
    size_t total_uncompressed = 0;
    size_t total_compressed = 0;
    
    for (int t = 0; t < image->num_tracks; t++) {
        for (int s = 0; s < image->num_sides; s++) {
            if (offset + 2 > size) {
                return false;  /* Truncated */
            }
            
            msa_track_desc_t* track = &image->tracks[t][s];
            
            /* Read track length word */
            uint16_t track_len = msa_read_be16(data + offset);
            offset += 2;
            
            track->compressed_size = track_len;
            track->uncompressed_size = image->track_size;
            track->data_offset = offset;
            track->is_compressed = (track_len != image->track_size);
            
            if (track->is_compressed) {
                image->compressed_tracks++;
            } else {
                image->uncompressed_tracks++;
            }
            
            total_compressed += track_len;
            total_uncompressed += image->track_size;
            
            offset += track_len;
        }
    }
    
    image->total_size = total_uncompressed;
    
    if (total_uncompressed > 0) {
        image->compression_ratio = (double)total_compressed / total_uncompressed;
    }
    
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * CONVERSION
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Convert MSA to raw ST image
 */
size_t msa_to_st(const uint8_t* msa_data, size_t msa_size,
                 uint8_t* output, size_t output_size) {
    
    msa_image_t image;
    if (!msa_parse_image(msa_data, msa_size, &image)) {
        return 0;
    }
    
    if (output_size < image.total_size) {
        return 0;
    }
    
    size_t out_offset = 0;
    
    for (int t = 0; t < image.num_tracks; t++) {
        for (int s = 0; s < image.num_sides; s++) {
            msa_track_desc_t* track = &image.tracks[t][s];
            
            if (track->is_compressed) {
                /* Decompress */
                size_t decompressed = msa_decompress_track(
                    msa_data + track->data_offset,
                    track->compressed_size,
                    output + out_offset,
                    image.track_size
                );
                
                /* Pad if needed */
                if (decompressed < image.track_size) {
                    memset(output + out_offset + decompressed, 0,
                           image.track_size - decompressed);
                }
            } else {
                /* Direct copy */
                if (track->data_offset + image.track_size <= msa_size) {
                    memcpy(output + out_offset,
                           msa_data + track->data_offset,
                           image.track_size);
                }
            }
            
            out_offset += image.track_size;
        }
    }
    
    return out_offset;
}

/**
 * @brief Create MSA from raw ST image
 */
size_t msa_from_st(const uint8_t* st_data, size_t st_size,
                   uint8_t* output, size_t output_size,
                   uint16_t sectors_per_track, uint16_t sides,
                   uint16_t start_track, uint16_t end_track) {
    
    if (output_size < MSA_HEADER_SIZE) return 0;
    
    /* Write header */
    msa_write_be16(output + 0, MSA_MAGIC);
    msa_write_be16(output + 2, sectors_per_track);
    msa_write_be16(output + 4, sides - 1);  /* 0 = 1 side */
    msa_write_be16(output + 6, start_track);
    msa_write_be16(output + 8, end_track);
    
    size_t track_size = sectors_per_track * MSA_SECTOR_SIZE;
    size_t out_offset = MSA_HEADER_SIZE;
    size_t in_offset = 0;
    
    /* Temporary buffer for compression */
    uint8_t* compress_buf = malloc(track_size + 256);
    if (!compress_buf) return 0;
    
    int num_tracks = end_track - start_track + 1;
    
    for (int t = 0; t < num_tracks; t++) {
        for (int s = 0; s < sides; s++) {
            if (in_offset + track_size > st_size) {
                /* Pad missing tracks */
                if (out_offset + 2 + track_size > output_size) {
                    free(compress_buf);
                    return out_offset;
                }
                
                msa_write_be16(output + out_offset, track_size);
                out_offset += 2;
                memset(output + out_offset, 0, track_size);
                out_offset += track_size;
            } else {
                /* Try compression */
                size_t compressed_size = msa_compress_track(
                    st_data + in_offset, track_size,
                    compress_buf, track_size + 256
                );
                
                /* Use compressed only if smaller */
                if (compressed_size < track_size && compressed_size > 0) {
                    if (out_offset + 2 + compressed_size > output_size) {
                        free(compress_buf);
                        return out_offset;
                    }
                    
                    msa_write_be16(output + out_offset, compressed_size);
                    out_offset += 2;
                    memcpy(output + out_offset, compress_buf, compressed_size);
                    out_offset += compressed_size;
                } else {
                    /* Store uncompressed */
                    if (out_offset + 2 + track_size > output_size) {
                        free(compress_buf);
                        return out_offset;
                    }
                    
                    msa_write_be16(output + out_offset, track_size);
                    out_offset += 2;
                    memcpy(output + out_offset, st_data + in_offset, track_size);
                    out_offset += track_size;
                }
            }
            
            in_offset += track_size;
        }
    }
    
    free(compress_buf);
    return out_offset;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * COMMON GEOMETRIES
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    const char* name;
    uint16_t sides;
    uint16_t sectors;
    uint16_t tracks;
    size_t total_size;
} msa_geometry_t;

static const msa_geometry_t msa_known_geometries[] = {
    { "Atari ST SS/SD",  1, 9,  80, 360*1024 },
    { "Atari ST SS/DD",  1, 10, 80, 400*1024 },
    { "Atari ST DS/SD",  2, 9,  80, 720*1024 },
    { "Atari ST DS/DD",  2, 10, 80, 800*1024 },
    { "Atari ST DS/ED",  2, 11, 80, 880*1024 },
    { "Atari HD",        2, 18, 80, 1440*1024 },
    { NULL, 0, 0, 0, 0 }
};

/**
 * @brief Detect geometry from size
 */
const msa_geometry_t* msa_detect_geometry(size_t size) {
    for (int i = 0; msa_known_geometries[i].name; i++) {
        if (msa_known_geometries[i].total_size == size) {
            return &msa_known_geometries[i];
        }
    }
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TEST SUITE
 * ═══════════════════════════════════════════════════════════════════════════ */

#ifdef MSA_TEST

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

TEST(magic) {
    uint8_t valid[10] = { 0x0E, 0x0F, 0x00, 0x09, 0x00, 0x01, 0x00, 0x00, 0x00, 0x4F };
    uint8_t invalid[10] = { 0x00, 0x00, 0x00, 0x09, 0x00, 0x01, 0x00, 0x00, 0x00, 0x4F };
    
    assert(msa_is_msa(valid, 10));
    assert(!msa_is_msa(invalid, 10));
}

TEST(disk_types) {
    assert(strstr(msa_disk_type_name(MSA_TYPE_SS_SD), "SS") != NULL);
    assert(strstr(msa_disk_type_name(MSA_TYPE_DS_DD), "DS") != NULL);
    assert(strstr(msa_disk_type_name(MSA_TYPE_DS_HD), "HD") != NULL);
    
    /* Type detection */
    assert(msa_detect_type(0, 9, 80) == MSA_TYPE_SS_SD);
    assert(msa_detect_type(0, 10, 80) == MSA_TYPE_SS_DD);
    assert(msa_detect_type(1, 9, 80) == MSA_TYPE_DS_SD);
    assert(msa_detect_type(1, 10, 80) == MSA_TYPE_DS_DD);
}

TEST(rle_roundtrip) {
    /* Test data with runs */
    uint8_t original[100];
    memset(original, 0, 100);
    memset(original, 0xAA, 20);
    memset(original + 40, 0xBB, 30);
    
    uint8_t compressed[150];
    uint8_t decompressed[100];
    
    size_t comp_size = msa_compress_track(original, 100, compressed, 150);
    assert(comp_size > 0);
    assert(comp_size < 100);  /* Should compress */
    
    size_t decomp_size = msa_decompress_track(compressed, comp_size, 
                                               decompressed, 100);
    assert(decomp_size == 100);
    assert(memcmp(original, decompressed, 100) == 0);
}

TEST(track_size) {
    assert(msa_track_size(9) == 4608);   /* 9 * 512 */
    assert(msa_track_size(10) == 5120);  /* 10 * 512 */
    assert(msa_track_size(18) == 9216);  /* 18 * 512 */
}

TEST(geometry) {
    const msa_geometry_t* geom;
    
    geom = msa_detect_geometry(720*1024);
    assert(geom != NULL);
    assert(geom->sides == 2);
    assert(geom->sectors == 9);
    
    geom = msa_detect_geometry(800*1024);
    assert(geom != NULL);
    assert(geom->sectors == 10);
}

int main(void) {
    printf("=== MSA Parser v2 Tests ===\n");
    
    RUN_TEST(magic);
    RUN_TEST(disk_types);
    RUN_TEST(rle_roundtrip);
    RUN_TEST(track_size);
    RUN_TEST(geometry);
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: %d, Failed: %d\n", tests_passed, tests_failed);
    
    return tests_failed;
}

#endif /* MSA_TEST */
