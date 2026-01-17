/**
 * @file uft_woz.c
 * @brief WOZ Disk Image Format Implementation
 * @version 4.1.5
 */

#include "uft/formats/apple/uft_woz.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * CRC32 Table (Gary S. Brown, 1986)
 * ============================================================================ */

static const uint32_t crc32_tab[] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
    0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
    0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
    0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
    0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
    0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
    0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
    0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
    0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
    0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
    0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
    0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
    0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
    0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
    0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
    0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
    0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
    0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
    0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
    0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
    0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

uint32_t woz_crc32(uint32_t crc, const uint8_t *data, size_t size)
{
    crc = crc ^ ~0U;
    while (size--) {
        crc = crc32_tab[(crc ^ *data++) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ ~0U;
}

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static uint16_t read_u16_le(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_u32_le(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void parse_metadata_value(const char *start, const char *end, char *dest, size_t max)
{
    size_t len = (size_t)(end - start);
    if (len >= max) len = max - 1;
    memcpy(dest, start, len);
    dest[len] = '\0';
}

static int parse_metadata(const char *data, size_t size, woz_metadata_t *meta)
{
    memset(meta, 0, sizeof(*meta));
    
    const char *p = data;
    const char *end = data + size;
    
    while (p < end) {
        /* Find key */
        const char *key_start = p;
        while (p < end && *p != '\t' && *p != '\n') p++;
        if (p >= end) break;
        
        size_t key_len = (size_t)(p - key_start);
        
        /* Skip tab */
        if (*p == '\t') p++;
        
        /* Find value */
        const char *val_start = p;
        while (p < end && *p != '\n') p++;
        
        const char *val_end = p;
        
        /* Skip newline */
        if (p < end && *p == '\n') p++;
        
        /* Match known keys */
        if (key_len == 5 && memcmp(key_start, "title", 5) == 0) {
            parse_metadata_value(val_start, val_end, meta->title, sizeof(meta->title));
        } else if (key_len == 8 && memcmp(key_start, "subtitle", 8) == 0) {
            parse_metadata_value(val_start, val_end, meta->subtitle, sizeof(meta->subtitle));
        } else if (key_len == 9 && memcmp(key_start, "publisher", 9) == 0) {
            parse_metadata_value(val_start, val_end, meta->publisher, sizeof(meta->publisher));
        } else if (key_len == 9 && memcmp(key_start, "developer", 9) == 0) {
            parse_metadata_value(val_start, val_end, meta->developer, sizeof(meta->developer));
        } else if (key_len == 9 && memcmp(key_start, "copyright", 9) == 0) {
            parse_metadata_value(val_start, val_end, meta->copyright, sizeof(meta->copyright));
        } else if (key_len == 7 && memcmp(key_start, "version", 7) == 0) {
            parse_metadata_value(val_start, val_end, meta->version, sizeof(meta->version));
        } else if (key_len == 8 && memcmp(key_start, "language", 8) == 0) {
            parse_metadata_value(val_start, val_end, meta->language, sizeof(meta->language));
        } else if (key_len == 12 && memcmp(key_start, "requires_ram", 12) == 0) {
            parse_metadata_value(val_start, val_end, meta->requires_ram, sizeof(meta->requires_ram));
        } else if (key_len == 16 && memcmp(key_start, "requires_machine", 16) == 0) {
            parse_metadata_value(val_start, val_end, meta->requires_machine, sizeof(meta->requires_machine));
        } else if (key_len == 5 && memcmp(key_start, "notes", 5) == 0) {
            parse_metadata_value(val_start, val_end, meta->notes, sizeof(meta->notes));
        } else if (key_len == 4 && memcmp(key_start, "side", 4) == 0) {
            parse_metadata_value(val_start, val_end, meta->side, sizeof(meta->side));
        } else if (key_len == 9 && memcmp(key_start, "side_name", 9) == 0) {
            parse_metadata_value(val_start, val_end, meta->side_name, sizeof(meta->side_name));
        } else if (key_len == 11 && memcmp(key_start, "contributor", 11) == 0) {
            parse_metadata_value(val_start, val_end, meta->contributor, sizeof(meta->contributor));
        } else if (key_len == 10 && memcmp(key_start, "image_date", 10) == 0) {
            parse_metadata_value(val_start, val_end, meta->image_date, sizeof(meta->image_date));
        }
    }
    
    return 0;
}

/* ============================================================================
 * Loading Functions
 * ============================================================================ */

int woz_load_from_memory(const uint8_t *data, size_t size, woz_image_t **image)
{
    if (!data || !image || size < 12) {
        return WOZ_ERR_INVALID_HEADER;
    }
    
    /* Check signature */
    uint32_t sig = read_u32_le(data);
    if (sig != WOZ_SIGNATURE_V1 && sig != WOZ_SIGNATURE_V2) {
        return WOZ_ERR_INVALID_HEADER;
    }
    
    /* Verify high bit and LF CR LF */
    if (data[4] != 0xFF || data[5] != 0x0A || data[6] != 0x0D || data[7] != 0x0A) {
        return WOZ_ERR_INVALID_HEADER;
    }
    
    /* Allocate image */
    woz_image_t *img = calloc(1, sizeof(woz_image_t));
    if (!img) {
        return WOZ_ERR_OUT_OF_MEMORY;
    }
    
    img->version = (sig == WOZ_SIGNATURE_V1) ? 1 : 2;
    img->file_crc = read_u32_le(data + 8);
    
    /* Verify CRC if present */
    if (img->file_crc != 0) {
        uint32_t calc_crc = woz_crc32(0, data + 12, size - 12);
        img->crc_valid = (calc_crc == img->file_crc);
    } else {
        img->crc_valid = true;  /* No CRC present */
    }
    
    /* Initialize TMAP and FLUX to empty */
    memset(img->tmap, WOZ_TMAP_EMPTY, sizeof(img->tmap));
    memset(img->flux_map, WOZ_TMAP_EMPTY, sizeof(img->flux_map));
    
    /* Parse chunks */
    size_t pos = 12;
    bool has_info = false;
    bool has_tmap = false;
    bool has_trks = false;
    
    while (pos + 8 <= size) {
        uint32_t chunk_id = read_u32_le(data + pos);
        uint32_t chunk_size = read_u32_le(data + pos + 4);
        pos += 8;
        
        if (pos + chunk_size > size) {
            break;  /* Incomplete chunk */
        }
        
        switch (chunk_id) {
            case WOZ_CHUNK_INFO:
                if (chunk_size >= 60) {
                    memcpy(&img->info, data + pos, 60);
                    has_info = true;
                    img->is_525 = (img->info.disk_type == WOZ_DISK_525);
                }
                break;
                
            case WOZ_CHUNK_TMAP:
                if (chunk_size >= WOZ_TMAP_SIZE) {
                    memcpy(img->tmap, data + pos, WOZ_TMAP_SIZE);
                    has_tmap = true;
                }
                break;
                
            case WOZ_CHUNK_TRKS:
                /* Read TRK entries */
                if (img->version == 1) {
                    /* WOZ 1.0: Fixed 6656-byte tracks */
                    int num_tracks = (int)(chunk_size / 6656);
                    img->track_data_size = chunk_size;
                    img->track_data = malloc(chunk_size);
                    if (img->track_data) {
                        memcpy(img->track_data, data + pos, chunk_size);
                        /* Create TRK entries for v1 format */
                        for (int i = 0; i < num_tracks && i < WOZ_MAX_TRACKS; i++) {
                            /* In v1, track data is inline at known offsets */
                            /* We'll handle this differently */
                        }
                    }
                } else {
                    /* WOZ 2.0: TRK array followed by variable track data */
                    /* First 1280 bytes = 160 TRK entries (8 bytes each) */
                    if (chunk_size >= 1280) {
                        for (int i = 0; i < WOZ_MAX_TRACKS; i++) {
                            size_t trk_off = (size_t)i * 8;
                            img->trks[i].starting_block = read_u16_le(data + pos + trk_off);
                            img->trks[i].block_count = read_u16_le(data + pos + trk_off + 2);
                            img->trks[i].bit_count = read_u32_le(data + pos + trk_off + 4);
                        }
                        
                        /* Track data starts at block 3 (byte 1536) relative to file */
                        /* Calculate total track data size */
                        uint16_t max_block = 3;
                        for (int i = 0; i < WOZ_MAX_TRACKS; i++) {
                            if (img->trks[i].starting_block > 0 && img->trks[i].block_count > 0) {
                                uint16_t end_block = img->trks[i].starting_block + img->trks[i].block_count;
                                if (end_block > max_block) max_block = end_block;
                            }
                        }
                        
                        /* Copy all track data from file */
                        size_t track_data_start = 1536;  /* Block 3 */
                        if (track_data_start < size) {
                            img->track_data_size = size - track_data_start;
                            img->track_data = malloc(img->track_data_size);
                            if (img->track_data) {
                                memcpy(img->track_data, data + track_data_start, img->track_data_size);
                            }
                        }
                    }
                }
                has_trks = true;
                break;
                
            case WOZ_CHUNK_META:
                if (chunk_size > 0) {
                    parse_metadata((const char *)(data + pos), chunk_size, &img->metadata);
                    img->has_metadata = true;
                }
                break;
                
            case WOZ_CHUNK_FLUX:
                if (chunk_size >= WOZ_TMAP_SIZE) {
                    memcpy(img->flux_map, data + pos, WOZ_TMAP_SIZE);
                    img->has_flux = true;
                }
                break;
                
            case WOZ_CHUNK_WRIT:
                /* TODO: Parse write hints */
                img->has_write_hints = true;
                break;
        }
        
        pos += chunk_size;
    }
    
    if (!has_info) {
        woz_free(img);
        return WOZ_ERR_MISSING_INFO;
    }
    if (!has_tmap) {
        woz_free(img);
        return WOZ_ERR_MISSING_TMAP;
    }
    if (!has_trks) {
        woz_free(img);
        return WOZ_ERR_MISSING_TRKS;
    }
    
    /* Count tracks */
    img->total_tracks = 0;
    img->quarter_tracks = 0;
    for (int i = 0; i < WOZ_TMAP_SIZE; i++) {
        if (img->tmap[i] != WOZ_TMAP_EMPTY) {
            img->quarter_tracks++;
            if (img->tmap[i] >= img->total_tracks) {
                img->total_tracks = img->tmap[i] + 1;
            }
        }
    }
    
    *image = img;
    return WOZ_OK;
}

int woz_load(const char *filename, woz_image_t **image)
{
    if (!filename || !image) {
        return WOZ_ERR_FILE_NOT_FOUND;
    }
    
    FILE *f = fopen(filename, "rb");
    if (!f) {
        return WOZ_ERR_FILE_NOT_FOUND;
    }
    
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (file_size <= 0 || file_size > 100 * 1024 * 1024) {  /* Max 100MB */
        fclose(f);
        return WOZ_ERR_INVALID_HEADER;
    }
    
    uint8_t *data = malloc((size_t)file_size);
    if (!data) {
        fclose(f);
        return WOZ_ERR_OUT_OF_MEMORY;
    }
    
    if (fread(data, 1, (size_t)file_size, f) != (size_t)file_size) {
        free(data);
        fclose(f);
        return WOZ_ERR_CORRUPT_DATA;
    }
    
    fclose(f);
    
    int result = woz_load_from_memory(data, (size_t)file_size, image);
    free(data);
    return result;
}

void woz_free(woz_image_t *image)
{
    if (!image) return;
    
    free(image->track_data);
    free(image->write_hints);
    free(image);
}

/* ============================================================================
 * Track Access Functions
 * ============================================================================ */

int woz_get_track_525(const woz_image_t *image, int quarter_track,
                       const uint8_t **data, uint32_t *bit_count)
{
    if (!image || !data || !bit_count) return -1;
    if (quarter_track < 0 || quarter_track >= WOZ_TMAP_SIZE) return -1;
    
    uint8_t trk_idx = image->tmap[quarter_track];
    if (trk_idx == WOZ_TMAP_EMPTY) {
        return -1;  /* Empty track */
    }
    
    if (trk_idx >= WOZ_MAX_TRACKS) return -1;
    
    const woz_trk_t *trk = &image->trks[trk_idx];
    if (trk->starting_block == 0 || trk->block_count == 0) {
        return -1;
    }
    
    /* Calculate offset into track_data */
    /* starting_block is relative to file start, track_data starts at block 3 */
    size_t offset = ((size_t)trk->starting_block - 3) * WOZ_BLOCK_SIZE;
    if (offset >= image->track_data_size) {
        return -1;
    }
    
    *data = image->track_data + offset;
    *bit_count = trk->bit_count;
    return 0;
}

int woz_get_track_35(const woz_image_t *image, int track, int side,
                      const uint8_t **data, uint32_t *bit_count)
{
    if (!image || !data || !bit_count) return -1;
    if (track < 0 || track >= 80 || side < 0 || side > 1) return -1;
    
    int map_idx = (track << 1) + side;
    return woz_get_track_525(image, map_idx, data, bit_count);  /* Same mechanism */
}

int woz_get_flux(const woz_image_t *image, int quarter_track,
                  const uint8_t **data, uint32_t *byte_count)
{
    if (!image || !image->has_flux) return -1;
    
    uint8_t trk_idx = image->flux_map[quarter_track];
    if (trk_idx == WOZ_TMAP_EMPTY) {
        return -1;
    }
    
    const woz_trk_t *trk = &image->trks[trk_idx];
    size_t offset = ((size_t)trk->starting_block - 3) * WOZ_BLOCK_SIZE;
    
    *data = image->track_data + offset;
    *byte_count = trk->bit_count;  /* For flux, bit_count is actually byte_count */
    return 0;
}

/* ============================================================================
 * String Functions
 * ============================================================================ */

const char *woz_version_string(uint32_t version)
{
    switch (version) {
        case 1: return "WOZ 1.0";
        case 2: return "WOZ 2.0/2.1";
        default: return "Unknown";
    }
}

const char *woz_disk_type_string(uint8_t disk_type)
{
    switch (disk_type) {
        case WOZ_DISK_525: return "5.25\"";
        case WOZ_DISK_35: return "3.5\"";
        default: return "Unknown";
    }
}

int woz_hardware_string(uint16_t hw_flags, char *buffer, size_t size)
{
    if (!buffer || size == 0) return 0;
    
    buffer[0] = '\0';
    int written = 0;
    
    struct { uint16_t flag; const char *name; } hw_names[] = {
        { WOZ_HW_APPLE_II, "Apple ][" },
        { WOZ_HW_APPLE_II_PLUS, "Apple ][+" },
        { WOZ_HW_APPLE_IIE, "Apple //e" },
        { WOZ_HW_APPLE_IIC, "Apple //c" },
        { WOZ_HW_APPLE_IIE_ENH, "Apple //e Enhanced" },
        { WOZ_HW_APPLE_IIGS, "Apple IIgs" },
        { WOZ_HW_APPLE_IIC_PLUS, "Apple //c+" },
        { WOZ_HW_APPLE_III, "Apple ///" },
        { WOZ_HW_APPLE_III_PLUS, "Apple ///+" },
        { 0, NULL }
    };
    
    for (int i = 0; hw_names[i].name; i++) {
        if (hw_flags & hw_names[i].flag) {
            if (written > 0) {
                written += snprintf(buffer + written, size - (size_t)written, ", ");
            }
            written += snprintf(buffer + written, size - (size_t)written, "%s", hw_names[i].name);
        }
    }
    
    if (written == 0) {
        written = snprintf(buffer, size, "Unknown");
    }
    
    return written;
}

int woz_info_string(const woz_image_t *image, char *buffer, size_t size)
{
    if (!image || !buffer || size == 0) return 0;
    
    char hw_str[256];
    woz_hardware_string(image->info.compatible_hw, hw_str, sizeof(hw_str));
    
    int written = snprintf(buffer, size,
        "WOZ Disk Image\n"
        "══════════════════════════════════════\n"
        "Format Version:   %s\n"
        "Disk Type:        %s\n"
        "Disk Sides:       %d\n"
        "Write Protected:  %s\n"
        "Synchronized:     %s\n"
        "Cleaned:          %s\n"
        "Creator:          %.32s\n"
        "Boot Format:      %s\n"
        "Bit Timing:       %d (%.2f µs)\n"
        "Compatible HW:    %s\n"
        "Required RAM:     %d KB\n"
        "Total Tracks:     %d\n"
        "Quarter Tracks:   %d\n"
        "CRC Valid:        %s\n",
        woz_version_string(image->version),
        woz_disk_type_string(image->info.disk_type),
        image->info.disk_sides ? image->info.disk_sides : 1,
        image->info.write_protected ? "Yes" : "No",
        image->info.synchronized ? "Yes" : "No",
        image->info.cleaned ? "Yes" : "No",
        image->info.creator,
        image->info.boot_sector_fmt == WOZ_BOOT_16_SECTOR ? "16-sector (DOS 3.3)" :
        image->info.boot_sector_fmt == WOZ_BOOT_13_SECTOR ? "13-sector (DOS 3.2)" :
        image->info.boot_sector_fmt == WOZ_BOOT_BOTH ? "Both" : "Unknown",
        image->info.optimal_bit_timing,
        (double)image->info.optimal_bit_timing * WOZ_TICK_NS / 1000.0,
        hw_str,
        image->info.required_ram,
        image->total_tracks,
        image->quarter_tracks,
        image->crc_valid ? "Yes" : "No"
    );
    
    /* Add metadata if present */
    if (image->has_metadata) {
        written += snprintf(buffer + written, size - (size_t)written,
            "\nMetadata:\n"
            "──────────────────────────────────────\n");
        
        if (image->metadata.title[0]) {
            written += snprintf(buffer + written, size - (size_t)written,
                "Title:            %s\n", image->metadata.title);
        }
        if (image->metadata.publisher[0]) {
            written += snprintf(buffer + written, size - (size_t)written,
                "Publisher:        %s\n", image->metadata.publisher);
        }
        if (image->metadata.developer[0]) {
            written += snprintf(buffer + written, size - (size_t)written,
                "Developer:        %s\n", image->metadata.developer);
        }
        if (image->metadata.copyright[0]) {
            written += snprintf(buffer + written, size - (size_t)written,
                "Copyright:        %s\n", image->metadata.copyright);
        }
        if (image->metadata.language[0]) {
            written += snprintf(buffer + written, size - (size_t)written,
                "Language:         %s\n", image->metadata.language);
        }
        if (image->metadata.side[0]) {
            written += snprintf(buffer + written, size - (size_t)written,
                "Side:             %s\n", image->metadata.side);
        }
        if (image->metadata.contributor[0]) {
            written += snprintf(buffer + written, size - (size_t)written,
                "Contributor:      %s\n", image->metadata.contributor);
        }
        if (image->metadata.image_date[0]) {
            written += snprintf(buffer + written, size - (size_t)written,
                "Image Date:       %s\n", image->metadata.image_date);
        }
    }
    
    return written;
}

/* ============================================================================
 * Copy Protection Detection
 * ============================================================================ */

bool woz_detect_spiradisc(const woz_image_t *image)
{
    if (!image || !image->is_525) return false;
    
    /* Spiradisc uses quarter-track stepping
     * Look for data on quarter-tracks (0.25, 0.50, 0.75, etc.)
     */
    int quarter_track_count = 0;
    
    for (int i = 0; i < WOZ_TMAP_SIZE; i++) {
        if (image->tmap[i] != WOZ_TMAP_EMPTY) {
            /* Check if this is a quarter-track (not whole track) */
            int quarter = i % 4;
            if (quarter == 1 || quarter == 2 || quarter == 3) {
                quarter_track_count++;
            }
        }
    }
    
    /* If we have significant quarter-track data, likely Spiradisc */
    /* Spiradisc typically has data on every quarter-track through the disk */
    return (quarter_track_count >= 20);
}

bool woz_has_sync_tracks(const woz_image_t *image)
{
    if (!image) return false;
    return (image->info.synchronized != 0);
}

/* ============================================================================
 * MC3470 Fake Bit Simulation
 * ============================================================================ */

/* Random bit buffer for fake bit generation */
static uint8_t fake_bit_buffer[32];
static int fake_bit_pos = 0;
static bool fake_bit_initialized = false;

static void init_fake_bits(void)
{
    if (!fake_bit_initialized) {
        /* Fill with ~30% ones */
        for (int i = 0; i < 32; i++) {
            fake_bit_buffer[i] = (uint8_t)(((i * 37) % 256) < 77 ? 0xFF : 0x00);
        }
        fake_bit_initialized = true;
    }
}

static uint8_t get_fake_bit(void)
{
    init_fake_bits();
    uint8_t bit = (fake_bit_buffer[fake_bit_pos / 8] >> (7 - (fake_bit_pos % 8))) & 1;
    fake_bit_pos = (fake_bit_pos + 1) % 256;
    return bit;
}

uint8_t woz_read_nibble_mc3470(const uint8_t *bit_stream, uint32_t bit_count,
                                 uint32_t *position)
{
    if (!bit_stream || !position) return 0;
    
    /* 4-bit head window for MC3470 simulation */
    uint8_t head_window = 0;
    uint8_t nibble = 0;
    int bits_collected = 0;
    
    while (bits_collected < 8) {
        /* Get next bit from stream */
        uint32_t byte_pos = *position / 8;
        uint32_t bit_pos = 7 - (*position % 8);
        
        uint8_t woz_bit = 0;
        if (byte_pos < (bit_count + 7) / 8) {
            woz_bit = (bit_stream[byte_pos] >> bit_pos) & 1;
        }
        
        /* Update head window */
        head_window = (uint8_t)((head_window << 1) | woz_bit);
        
        /* Check for fake bit condition (4+ consecutive zeros) */
        uint8_t output_bit;
        if ((head_window & 0x0F) != 0x00) {
            output_bit = (head_window >> 1) & 1;
        } else {
            output_bit = get_fake_bit();  /* Random fake bit */
        }
        
        /* Accumulate into nibble */
        nibble = (uint8_t)((nibble << 1) | output_bit);
        if (output_bit) {
            bits_collected++;
        }
        
        /* Advance position with wrap-around */
        (*position)++;
        if (*position >= bit_count) {
            *position = 0;
        }
    }
    
    return nibble;
}

bool woz_verify_crc(const woz_image_t *image)
{
    if (!image) return false;
    return image->crc_valid;
}
