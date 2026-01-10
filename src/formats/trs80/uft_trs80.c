/**
 * @file uft_trs80.c
 * @brief TRS-80 Disk Format Implementation
 * @version 3.7.0
 * 
 * SPDX-License-Identifier: MIT
 */

#include "uft/uft_safe_io.h"
#include "uft/formats/uft_trs80.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*============================================================================
 * Geometry Tables
 *============================================================================*/

static const uft_trs80_geometry_t g_trs80_geometries[] = {
    [UFT_TRS80_GEOM_UNKNOWN] = {
        .type = UFT_TRS80_GEOM_UNKNOWN, .model = UFT_TRS80_MODEL_UNKNOWN,
        .tracks = 0, .heads = 0, .sectors_per_track = 0, .sector_size = 0,
        .total_bytes = 0, .density = UFT_TRS80_DENSITY_UNKNOWN, .name = "Unknown"
    },
    [UFT_TRS80_GEOM_M1_SSSD] = {
        .type = UFT_TRS80_GEOM_M1_SSSD, .model = UFT_TRS80_MODEL_I,
        .tracks = 35, .heads = 1, .sectors_per_track = 10, .sector_size = 256,
        .total_bytes = 89600, .density = UFT_TRS80_DENSITY_FM,
        .name = "Model I SSSD (35×1×10×256)"
    },
    [UFT_TRS80_GEOM_M1_DSSD] = {
        .type = UFT_TRS80_GEOM_M1_DSSD, .model = UFT_TRS80_MODEL_I,
        .tracks = 35, .heads = 2, .sectors_per_track = 10, .sector_size = 256,
        .total_bytes = 179200, .density = UFT_TRS80_DENSITY_FM,
        .name = "Model I DSSD (35×2×10×256)"
    },
    [UFT_TRS80_GEOM_M1_SSDD] = {
        .type = UFT_TRS80_GEOM_M1_SSDD, .model = UFT_TRS80_MODEL_I,
        .tracks = 35, .heads = 1, .sectors_per_track = 18, .sector_size = 256,
        .total_bytes = 161280, .density = UFT_TRS80_DENSITY_MFM,
        .name = "Model I SSDD (35×1×18×256)"
    },
    [UFT_TRS80_GEOM_M1_DSDD] = {
        .type = UFT_TRS80_GEOM_M1_DSDD, .model = UFT_TRS80_MODEL_I,
        .tracks = 35, .heads = 2, .sectors_per_track = 18, .sector_size = 256,
        .total_bytes = 322560, .density = UFT_TRS80_DENSITY_MFM,
        .name = "Model I DSDD (35×2×18×256)"
    },
    [UFT_TRS80_GEOM_M3_SSDD] = {
        .type = UFT_TRS80_GEOM_M3_SSDD, .model = UFT_TRS80_MODEL_III,
        .tracks = 40, .heads = 1, .sectors_per_track = 18, .sector_size = 256,
        .total_bytes = 184320, .density = UFT_TRS80_DENSITY_MFM,
        .name = "Model III SSDD (40×1×18×256)"
    },
    [UFT_TRS80_GEOM_M3_DSDD] = {
        .type = UFT_TRS80_GEOM_M3_DSDD, .model = UFT_TRS80_MODEL_III,
        .tracks = 40, .heads = 2, .sectors_per_track = 18, .sector_size = 256,
        .total_bytes = 368640, .density = UFT_TRS80_DENSITY_MFM,
        .name = "Model III DSDD (40×2×18×256)"
    },
    [UFT_TRS80_GEOM_M4_SSDD] = {
        .type = UFT_TRS80_GEOM_M4_SSDD, .model = UFT_TRS80_MODEL_4,
        .tracks = 40, .heads = 1, .sectors_per_track = 18, .sector_size = 256,
        .total_bytes = 184320, .density = UFT_TRS80_DENSITY_MFM,
        .name = "Model 4 SSDD (40×1×18×256)"
    },
    [UFT_TRS80_GEOM_M4_DSDD] = {
        .type = UFT_TRS80_GEOM_M4_DSDD, .model = UFT_TRS80_MODEL_4,
        .tracks = 40, .heads = 2, .sectors_per_track = 18, .sector_size = 256,
        .total_bytes = 368640, .density = UFT_TRS80_DENSITY_MFM,
        .name = "Model 4 DSDD (40×2×18×256)"
    },
    [UFT_TRS80_GEOM_M4_DS80] = {
        .type = UFT_TRS80_GEOM_M4_DS80, .model = UFT_TRS80_MODEL_4,
        .tracks = 80, .heads = 2, .sectors_per_track = 18, .sector_size = 256,
        .total_bytes = 737280, .density = UFT_TRS80_DENSITY_MFM,
        .name = "Model 4 DS80 (80×2×18×256)"
    },
    [UFT_TRS80_GEOM_M4_DS80_HD] = {
        .type = UFT_TRS80_GEOM_M4_DS80_HD, .model = UFT_TRS80_MODEL_4,
        .tracks = 80, .heads = 2, .sectors_per_track = 36, .sector_size = 256,
        .total_bytes = 1474560, .density = UFT_TRS80_DENSITY_MFM,
        .name = "Model 4 DS80-HD (80×2×36×256)"
    },
    [UFT_TRS80_GEOM_COCO_SSSD] = {
        .type = UFT_TRS80_GEOM_COCO_SSSD, .model = UFT_TRS80_MODEL_COCO,
        .tracks = 35, .heads = 1, .sectors_per_track = 18, .sector_size = 256,
        .total_bytes = 161280, .density = UFT_TRS80_DENSITY_MFM,
        .name = "CoCo SSSD (35×1×18×256)"
    },
    [UFT_TRS80_GEOM_COCO_DSDD] = {
        .type = UFT_TRS80_GEOM_COCO_DSDD, .model = UFT_TRS80_MODEL_COCO,
        .tracks = 40, .heads = 2, .sectors_per_track = 18, .sector_size = 256,
        .total_bytes = 368640, .density = UFT_TRS80_DENSITY_MFM,
        .name = "CoCo DSDD (40×2×18×256)"
    },
    [UFT_TRS80_GEOM_COCO_80T] = {
        .type = UFT_TRS80_GEOM_COCO_80T, .model = UFT_TRS80_MODEL_COCO,
        .tracks = 80, .heads = 2, .sectors_per_track = 18, .sector_size = 256,
        .total_bytes = 737280, .density = UFT_TRS80_DENSITY_MFM,
        .name = "CoCo 80T (80×2×18×256)"
    }
};

/*============================================================================
 * Name Lookup Tables
 *============================================================================*/

static const char* g_model_names[] = {
    "Unknown", "Model I", "Model II", "Model III", "Model 4",
    "Model 4P", "Model 4D", "Model 12", "Model 16", "Color Computer", "MC-10"
};

static const char* g_dos_names[] = {
    "Unknown", "TRSDOS 2.3", "TRSDOS 1.3", "TRSDOS 6.x/LS-DOS",
    "NewDOS/80", "LDOS", "DOS+", "MultiDOS", "DoubleDOS",
    "CP/M", "FLEX", "OS-9", "RS-DOS"
};

static const char* g_format_names[] = {
    "Unknown", "JV1", "JV3", "JVC", "DMK", "VDK", "DSK", "HFE", "IMD"
};

/*============================================================================
 * Geometry API
 *============================================================================*/

const uft_trs80_geometry_t* uft_trs80_get_geometry(uft_trs80_geometry_type_t type) {
    if (type < 0 || type >= UFT_TRS80_GEOM_COUNT) {
        return &g_trs80_geometries[UFT_TRS80_GEOM_UNKNOWN];
    }
    return &g_trs80_geometries[type];
}

uft_trs80_geometry_type_t uft_trs80_detect_geometry_by_size(uint64_t file_size,
                                                            uint8_t* confidence) {
    uint8_t conf = 0;
    uft_trs80_geometry_type_t result = UFT_TRS80_GEOM_UNKNOWN;
    
    /* Check for JV1 standard size first */
    if (file_size == UFT_JV1_FILE_SIZE) {
        result = UFT_TRS80_GEOM_M1_SSSD;
        conf = 95;
    }
    /* Check other exact matches */
    else {
        for (int i = 1; i < UFT_TRS80_GEOM_COUNT; i++) {
            if (file_size == g_trs80_geometries[i].total_bytes) {
                result = (uft_trs80_geometry_type_t)i;
                conf = 85;
                break;
            }
        }
    }
    
    /* Approximate matches for JV3/JVC with headers */
    if (result == UFT_TRS80_GEOM_UNKNOWN) {
        /* JVC header can add 0-5 bytes */
        for (int i = 1; i < UFT_TRS80_GEOM_COUNT; i++) {
            int64_t diff = (int64_t)file_size - (int64_t)g_trs80_geometries[i].total_bytes;
            if (diff >= 0 && diff <= 5) {
                result = (uft_trs80_geometry_type_t)i;
                conf = 70;
                break;
            }
        }
    }
    
    if (confidence) *confidence = conf;
    return result;
}

const char* uft_trs80_model_name(uft_trs80_model_t model) {
    if (model < 0 || model > UFT_TRS80_MODEL_MC10) return "Unknown";
    return g_model_names[model];
}

const char* uft_trs80_dos_name(uft_trs80_dos_t dos) {
    if (dos < 0 || dos > UFT_TRS80_DOS_RSDOS) return "Unknown";
    return g_dos_names[dos];
}

const char* uft_trs80_format_name(uft_trs80_format_t format) {
    if (format < 0 || format > UFT_TRS80_FMT_IMD) return "Unknown";
    return g_format_names[format];
}

/*============================================================================
 * JV1 Format Operations
 *============================================================================*/

bool uft_jv1_detect(uint64_t file_size, const uint8_t* data, size_t data_size,
                    uint8_t* confidence) {
    uint8_t conf = 0;
    
    /* JV1 is exactly 89600 bytes (35×10×256) */
    if (file_size == UFT_JV1_FILE_SIZE) {
        conf = 60;
        
        /* Check for TRSDOS-like boot sector */
        if (data && data_size >= 256) {
            /* TRSDOS 2.3 typically has specific patterns */
            if (data[0] == 0x00 || data[0] == 0xFE || data[0] == 0xC3) {
                conf += 20;
            }
            /* Check for directory track pattern */
            if (data_size >= 4608) { /* Track 17 start */
                /* Directory sectors have specific format */
                conf += 10;
            }
        }
    }
    
    if (confidence) *confidence = conf;
    return conf >= 60;
}

uft_trs80_rc_t uft_jv1_read_sector(uft_trs80_ctx_t* ctx, uint8_t track,
                                    uint8_t sector, uint8_t* buffer, size_t size) {
    if (!ctx || !ctx->path || !buffer) return UFT_TRS80_ERR_ARG;
    if (size < UFT_JV1_SECTOR_SIZE) return UFT_TRS80_ERR_ARG;
    if (track >= UFT_JV1_TRACKS || sector >= UFT_JV1_SECTORS) {
        return UFT_TRS80_ERR_RANGE;
    }
    
    FILE* fp = fopen(ctx->path, "rb");
    if (!fp) return UFT_TRS80_ERR_IO;
    
    uint32_t offset = (track * UFT_JV1_SECTORS + sector) * UFT_JV1_SECTOR_SIZE;
    
    if (fseek(fp, offset, SEEK_SET) != 0) {
        fclose(fp);
        return UFT_TRS80_ERR_IO;
    }
    
    if (fread(buffer, UFT_JV1_SECTOR_SIZE, 1, fp) != 1) {
        fclose(fp);
        return UFT_TRS80_ERR_IO;
    }
    
    fclose(fp);
    return UFT_TRS80_SUCCESS;
}

uft_trs80_rc_t uft_jv1_write_sector(uft_trs80_ctx_t* ctx, uint8_t track,
                                     uint8_t sector, const uint8_t* data, size_t size) {
    if (!ctx || !ctx->path || !data) return UFT_TRS80_ERR_ARG;
    if (!ctx->writable) return UFT_TRS80_ERR_READONLY;
    if (size < UFT_JV1_SECTOR_SIZE) return UFT_TRS80_ERR_ARG;
    if (track >= UFT_JV1_TRACKS || sector >= UFT_JV1_SECTORS) {
        return UFT_TRS80_ERR_RANGE;
    }
    
    FILE* fp = fopen(ctx->path, "r+b");
    if (!fp) return UFT_TRS80_ERR_IO;
    
    uint32_t offset = (track * UFT_JV1_SECTORS + sector) * UFT_JV1_SECTOR_SIZE;
    
    if (fseek(fp, offset, SEEK_SET) != 0) {
        fclose(fp);
        return UFT_TRS80_ERR_IO;
    }
    
    if (fwrite(data, UFT_JV1_SECTOR_SIZE, 1, fp) != 1) {
        fclose(fp);
        return UFT_TRS80_ERR_IO;
    }
    
    fclose(fp);
    return UFT_TRS80_SUCCESS;
}

uft_trs80_rc_t uft_jv1_create_blank(const char* path) {
    if (!path) return UFT_TRS80_ERR_ARG;
    
    FILE* fp = fopen(path, "wb");
    if (!fp) return UFT_TRS80_ERR_IO;
    
    uint8_t zeros[UFT_JV1_SECTOR_SIZE] = {0};
    
    /* Write all sectors */
    for (int i = 0; i < UFT_JV1_TRACKS * UFT_JV1_SECTORS; i++) {
        if (fwrite(zeros, UFT_JV1_SECTOR_SIZE, 1, fp) != 1) {
            fclose(fp);
            return UFT_TRS80_ERR_IO;
        }
    }
    
    fclose(fp);
    return UFT_TRS80_SUCCESS;
}

/*============================================================================
 * JV3 Format Operations
 *============================================================================*/

uint16_t uft_jv3_sector_size(uint8_t flags) {
    switch (flags & UFT_JV3_FLAG_SIZE_MASK) {
        case 0: return 256;
        case 1: return 128;
        case 2: return 1024;
        case 3: return 512;
        default: return 256;
    }
}

bool uft_jv3_detect(const uint8_t* data, size_t data_size, uint8_t* confidence) {
    if (!data || data_size < UFT_JV3_HEADER_SIZE) {
        if (confidence) *confidence = 0;
        return false;
    }
    
    uint8_t conf = 0;
    
    /* JV3 header is 2901 × 3 bytes + 1 byte write protect flag */
    const uft_jv3_sector_header_t* sectors = (const uft_jv3_sector_header_t*)data;
    
    int valid_sectors = 0;
    int free_markers = 0;
    bool has_track_0 = false;
    
    for (int i = 0; i < UFT_JV3_SECTORS_MAX; i++) {
        if (sectors[i].track == UFT_JV3_FREE && 
            sectors[i].sector == UFT_JV3_FREE &&
            sectors[i].flags == UFT_JV3_FREE) {
            free_markers++;
        } else {
            /* Valid sector entry */
            if (sectors[i].track < 85 && sectors[i].sector < 40) {
                valid_sectors++;
                if (sectors[i].track == 0) has_track_0 = true;
            }
        }
    }
    
    /* JV3 should have at least some valid sectors and some free markers */
    if (valid_sectors > 0 && valid_sectors < UFT_JV3_SECTORS_MAX) {
        conf += 40;
    }
    if (has_track_0) conf += 20;
    if (free_markers > 100) conf += 20;  /* Many unused entries */
    
    /* Check write protect flag */
    uint8_t wp_flag = data[UFT_JV3_SECTORS_MAX * 3];
    if (wp_flag == 0x00 || wp_flag == 0xFF) {
        conf += 10;
    }
    
    if (confidence) *confidence = conf;
    return conf >= 50;
}

uft_trs80_rc_t uft_jv3_read_header(uft_trs80_ctx_t* ctx) {
    if (!ctx || !ctx->path) return UFT_TRS80_ERR_ARG;
    
    FILE* fp = fopen(ctx->path, "rb");
    if (!fp) return UFT_TRS80_ERR_IO;
    
    /* Read sector header table */
    if (fread(ctx->jv3_sectors, sizeof(ctx->jv3_sectors), 1, fp) != 1) {
        fclose(fp);
        return UFT_TRS80_ERR_IO;
    }
    
    /* Read write protect flag */
    if (fread(&ctx->jv3_write_protected, 1, 1, fp) != 1) {
        fclose(fp);
        return UFT_TRS80_ERR_IO;
    }
    
    fclose(fp);
    
    /* Count valid sectors */
    ctx->jv3_sector_count = 0;
    for (int i = 0; i < UFT_JV3_SECTORS_MAX; i++) {
        if (ctx->jv3_sectors[i].track != UFT_JV3_FREE) {
            ctx->jv3_sector_count++;
        } else {
            break;  /* Free entries mark end of used sectors */
        }
    }
    
    return UFT_TRS80_SUCCESS;
}

uft_trs80_rc_t uft_jv3_find_sector(uft_trs80_ctx_t* ctx, uint8_t track,
                                    uint8_t side, uint8_t sector,
                                    uint16_t* index, uint16_t* size) {
    if (!ctx) return UFT_TRS80_ERR_ARG;
    
    for (uint16_t i = 0; i < ctx->jv3_sector_count; i++) {
        const uft_jv3_sector_header_t* sh = &ctx->jv3_sectors[i];
        
        /* Check track and sector match */
        if (sh->track != track || sh->sector != sector) continue;
        
        /* Check side */
        uint8_t sector_side = (sh->flags & UFT_JV3_FLAG_SIDES) ? 1 : 0;
        if (sector_side != side) continue;
        
        /* Found it */
        if (index) *index = i;
        if (size) *size = uft_jv3_sector_size(sh->flags);
        return UFT_TRS80_SUCCESS;
    }
    
    return UFT_TRS80_ERR_NOTFOUND;
}

uft_trs80_rc_t uft_jv3_read_sector(uft_trs80_ctx_t* ctx, uint8_t track,
                                    uint8_t side, uint8_t sector,
                                    uint8_t* buffer, size_t buffer_size) {
    if (!ctx || !ctx->path || !buffer) return UFT_TRS80_ERR_ARG;
    
    uint16_t index, size;
    uft_trs80_rc_t rc = uft_jv3_find_sector(ctx, track, side, sector, &index, &size);
    if (rc != UFT_TRS80_SUCCESS) return rc;
    
    if (buffer_size < size) return UFT_TRS80_ERR_ARG;
    
    /* Calculate data offset */
    uint32_t data_offset = UFT_JV3_HEADER_SIZE;
    for (uint16_t i = 0; i < index; i++) {
        data_offset += uft_jv3_sector_size(ctx->jv3_sectors[i].flags);
    }
    
    FILE* fp = fopen(ctx->path, "rb");
    if (!fp) return UFT_TRS80_ERR_IO;
    
    if (fseek(fp, data_offset, SEEK_SET) != 0) {
        fclose(fp);
        return UFT_TRS80_ERR_IO;
    }
    
    if (fread(buffer, size, 1, fp) != 1) {
        fclose(fp);
        return UFT_TRS80_ERR_IO;
    }
    
    fclose(fp);
    return UFT_TRS80_SUCCESS;
}

/*============================================================================
 * JVC Format Operations
 *============================================================================*/

bool uft_jvc_detect(uint64_t file_size, const uint8_t* data, size_t data_size,
                    uft_jvc_header_t* header, uint8_t* confidence) {
    if (!header) return false;
    
    memset(header, 0, sizeof(*header));
    uint8_t conf = 0;
    
    /* JVC is JV1 with optional 0-5 byte header */
    /* Common sizes: 89600 (no header), 89601-89605 (with header) */
    
    uint8_t header_size = 0;
    
    for (int h = 0; h <= 5; h++) {
        uint64_t expected = UFT_JV1_FILE_SIZE + h;
        if (file_size == expected) {
            header_size = h;
            conf = 70;
            break;
        }
        
        /* Also check for other standard sizes with header */
        for (int g = 1; g < UFT_TRS80_GEOM_COUNT; g++) {
            if (file_size == g_trs80_geometries[g].total_bytes + h) {
                header_size = h;
                conf = 60;
                break;
            }
        }
    }
    
    if (conf > 0) {
        header->present = (header_size > 0);
        header->header_size = header_size;
        
        if (header_size > 0 && data && data_size >= (size_t)header_size) {
            if (header_size >= 1) header->sectors_per_track = data[0];
            if (header_size >= 2) header->side_count = data[1];
            if (header_size >= 3) header->sector_size_code = data[2];
            if (header_size >= 4) header->first_sector = data[3];
            if (header_size >= 5) header->sector_attr_flag = data[4];
            
            /* Validate header values */
            if (header->sectors_per_track > 0 && header->sectors_per_track <= 36) {
                conf += 10;
            }
            if (header->side_count >= 1 && header->side_count <= 2) {
                conf += 10;
            }
        }
    }
    
    if (confidence) *confidence = conf;
    return conf >= 60;
}

uft_trs80_rc_t uft_jvc_read_sector(uft_trs80_ctx_t* ctx, uint8_t track,
                                    uint8_t side, uint8_t sector,
                                    uint8_t* buffer, size_t size) {
    if (!ctx || !ctx->path || !buffer) return UFT_TRS80_ERR_ARG;
    
    /* Get geometry from JVC header or defaults */
    uint8_t sectors = ctx->jvc_header.sectors_per_track;
    if (sectors == 0) sectors = UFT_JV1_SECTORS;
    
    uint8_t sides = ctx->jvc_header.side_count;
    if (sides == 0) sides = 1;
    
    uint16_t sector_size = 256;  /* Default */
    if (ctx->jvc_header.sector_size_code == 1) sector_size = 128;
    else if (ctx->jvc_header.sector_size_code == 2) sector_size = 512;
    else if (ctx->jvc_header.sector_size_code == 3) sector_size = 1024;
    
    if (size < sector_size) return UFT_TRS80_ERR_ARG;
    if (side >= sides || sector >= sectors) return UFT_TRS80_ERR_RANGE;
    
    uint32_t data_offset = ctx->jvc_header.header_size;
    uint32_t linear_sector = (track * sides + side) * sectors + sector;
    data_offset += linear_sector * sector_size;
    
    FILE* fp = fopen(ctx->path, "rb");
    if (!fp) return UFT_TRS80_ERR_IO;
    
    if (fseek(fp, data_offset, SEEK_SET) != 0) {
        fclose(fp);
        return UFT_TRS80_ERR_IO;
    }
    
    if (fread(buffer, sector_size, 1, fp) != 1) {
        fclose(fp);
        return UFT_TRS80_ERR_IO;
    }
    
    fclose(fp);
    return UFT_TRS80_SUCCESS;
}

/*============================================================================
 * DOS Detection
 *============================================================================*/

uft_trs80_dos_t uft_trs80_detect_dos(const uint8_t* boot_sector, size_t size) {
    if (!boot_sector || size < 256) return UFT_TRS80_DOS_UNKNOWN;
    
    /* Check for TRSDOS 2.3 signature */
    if (boot_sector[0] == 0x00 && boot_sector[1] == 0xFE) {
        return UFT_TRS80_DOS_TRSDOS_23;
    }
    
    /* Check for LDOS signature */
    if (memcmp(boot_sector + 0xF4, "LDOS", 4) == 0) {
        return UFT_TRS80_DOS_LDOS;
    }
    
    /* Check for NewDOS/80 */
    if (memcmp(boot_sector + 0xD0, "NEWDOS", 6) == 0) {
        return UFT_TRS80_DOS_NEWDOS80;
    }
    
    /* Check for TRSDOS 6 / LS-DOS */
    if (boot_sector[0] == 0xFE || 
        (size >= 512 && memcmp(boot_sector + 0x100, "TRSDOS", 6) == 0)) {
        return UFT_TRS80_DOS_TRSDOS_6;
    }
    
    /* Check for RS-DOS (CoCo) */
    if (boot_sector[0] == 0x00 && boot_sector[1] == 0x4F) {
        return UFT_TRS80_DOS_RSDOS;
    }
    
    /* Check for OS-9 */
    if (boot_sector[0] == 0x00 && boot_sector[1] == 0x00 &&
        boot_sector[2] == 0x03) {
        return UFT_TRS80_DOS_OS9;
    }
    
    return UFT_TRS80_DOS_UNKNOWN;
}

/*============================================================================
 * Disk Context Operations
 *============================================================================*/

uft_trs80_rc_t uft_trs80_open(uft_trs80_ctx_t* ctx, const char* path, bool writable) {
    if (!ctx || !path) return UFT_TRS80_ERR_ARG;
    
    memset(ctx, 0, sizeof(*ctx));
    
    FILE* fp = fopen(path, "rb");
    if (!fp) return UFT_TRS80_ERR_IO;
    
    /* Get file size */
    if (fseek(fp, 0, SEEK_END) != 0) { /* I/O error */ }
    ctx->file_size = (uint64_t)ftell(fp);
    if (fseek(fp, 0, SEEK_SET) != 0) { /* I/O error */ }
    
    /* Read header/first sectors for detection */
    uint8_t header[8704];  /* Enough for JV3 header */
    size_t header_read = fread(header, 1, sizeof(header), fp);
    fclose(fp);
    
    ctx->path = strdup(path);
    if (!ctx->path) return UFT_TRS80_ERR_NOMEM;
    
    ctx->writable = writable;
    
    /* Try JV1 detection */
    uint8_t jv1_conf;
    if (uft_jv1_detect(ctx->file_size, header, header_read, &jv1_conf) && jv1_conf >= 60) {
        ctx->format = UFT_TRS80_FMT_JV1;
        ctx->format_confidence = jv1_conf;
        ctx->geometry = *uft_trs80_get_geometry(UFT_TRS80_GEOM_M1_SSSD);
    }
    /* Try JV3 detection */
    else {
        uint8_t jv3_conf;
        if (uft_jv3_detect(header, header_read, &jv3_conf) && jv3_conf >= 50) {
            ctx->format = UFT_TRS80_FMT_JV3;
            ctx->format_confidence = jv3_conf;
            uft_jv3_read_header(ctx);
            
            /* Determine geometry from sector map */
            uint8_t max_track = 0, max_side = 0;
            for (uint16_t i = 0; i < ctx->jv3_sector_count; i++) {
                if (ctx->jv3_sectors[i].track > max_track) {
                    max_track = ctx->jv3_sectors[i].track;
                }
                if (ctx->jv3_sectors[i].flags & UFT_JV3_FLAG_SIDES) {
                    max_side = 1;
                }
            }
            
            ctx->geometry.tracks = max_track + 1;
            ctx->geometry.heads = max_side + 1;
        }
        /* Try JVC detection */
        else {
            uint8_t jvc_conf;
            if (uft_jvc_detect(ctx->file_size, header, header_read, 
                              &ctx->jvc_header, &jvc_conf) && jvc_conf >= 60) {
                ctx->format = UFT_TRS80_FMT_JVC;
                ctx->format_confidence = jvc_conf;
                
                /* Build geometry from JVC header */
                ctx->geometry.sectors_per_track = ctx->jvc_header.sectors_per_track;
                if (ctx->geometry.sectors_per_track == 0) {
                    ctx->geometry.sectors_per_track = UFT_JV1_SECTORS;
                }
                ctx->geometry.heads = ctx->jvc_header.side_count;
                if (ctx->geometry.heads == 0) ctx->geometry.heads = 1;
                ctx->geometry.sector_size = 256;
            }
            /* Fall back to size-based detection */
            else {
                uint8_t size_conf;
                uft_trs80_geometry_type_t geom_type = 
                    uft_trs80_detect_geometry_by_size(ctx->file_size, &size_conf);
                if (geom_type != UFT_TRS80_GEOM_UNKNOWN) {
                    ctx->format = UFT_TRS80_FMT_DSK;
                    ctx->format_confidence = size_conf;
                    ctx->geometry = *uft_trs80_get_geometry(geom_type);
                }
            }
        }
    }
    
    /* Detect DOS type */
    if (ctx->format == UFT_TRS80_FMT_JV1 || ctx->format == UFT_TRS80_FMT_JVC) {
        ctx->dos_type = uft_trs80_detect_dos(header + ctx->jvc_header.header_size, 256);
    } else if (ctx->format == UFT_TRS80_FMT_JV3) {
        uint8_t boot[256];
        if (uft_jv3_read_sector(ctx, 0, 0, 0, boot, sizeof(boot)) == UFT_TRS80_SUCCESS) {
            ctx->dos_type = uft_trs80_detect_dos(boot, sizeof(boot));
        }
    }
    
    /* Set model from DOS/geometry */
    if (ctx->geometry.model != UFT_TRS80_MODEL_UNKNOWN) {
        ctx->model = ctx->geometry.model;
    } else if (ctx->dos_type == UFT_TRS80_DOS_RSDOS) {
        ctx->model = UFT_TRS80_MODEL_COCO;
    } else if (ctx->dos_type == UFT_TRS80_DOS_TRSDOS_23) {
        ctx->model = UFT_TRS80_MODEL_I;
    } else if (ctx->dos_type == UFT_TRS80_DOS_TRSDOS_13) {
        ctx->model = UFT_TRS80_MODEL_III;
    }
    
    return UFT_TRS80_SUCCESS;
}

void uft_trs80_close(uft_trs80_ctx_t* ctx) {
    if (ctx) {
        free(ctx->path);
        memset(ctx, 0, sizeof(*ctx));
    }
}

/*============================================================================
 * Copy Protection Detection
 *============================================================================*/

uft_trs80_rc_t uft_trs80_detect_protection(uft_trs80_ctx_t* ctx,
                                            uft_trs80_protection_result_t* result) {
    if (!ctx || !result) return UFT_TRS80_ERR_ARG;
    
    memset(result, 0, sizeof(*result));
    int indicators = 0;
    
    if (ctx->format == UFT_TRS80_FMT_JV3) {
        /* Check for CRC errors */
        for (uint16_t i = 0; i < ctx->jv3_sector_count; i++) {
            if (ctx->jv3_sectors[i].flags & UFT_JV3_FLAG_ERROR) {
                result->crc_error_count++;
            }
        }
        
        if (result->crc_error_count > 0) {
            result->flags |= UFT_TRS80_PROT_CRC_ERRORS;
            indicators++;
        }
        
        /* Check for mixed density tracks */
        uint8_t track_density[85] = {0};
        for (uint16_t i = 0; i < ctx->jv3_sector_count; i++) {
            uint8_t t = ctx->jv3_sectors[i].track;
            if (t < 85) {
                uint8_t is_dd = (ctx->jv3_sectors[i].flags & UFT_JV3_FLAG_DDEN) ? 2 : 1;
                if (track_density[t] == 0) {
                    track_density[t] = is_dd;
                } else if (track_density[t] != is_dd) {
                    track_density[t] = 3;  /* Mixed */
                }
            }
        }
        
        for (int t = 0; t < 85; t++) {
            if (track_density[t] == 3) {
                result->mixed_density_tracks++;
            }
        }
        
        if (result->mixed_density_tracks > 0) {
            result->flags |= UFT_TRS80_PROT_MIXED_DENSITY;
            indicators++;
        }
    }
    
    /* Calculate confidence */
    result->confidence = indicators * 40;
    if (result->confidence > 100) result->confidence = 100;
    
    /* Build description */
    if (result->flags != 0) {
        char* p = result->description;
        size_t remaining = sizeof(result->description);
        
        if (result->flags & UFT_TRS80_PROT_CRC_ERRORS) {
            int w = snprintf(p, remaining, "CRC errors: %d; ", result->crc_error_count);
            p += w;
            remaining -= w;
        }
        if (result->flags & UFT_TRS80_PROT_MIXED_DENSITY) {
            snprintf(p, remaining, "Mixed density tracks: %d; ", result->mixed_density_tracks);
        }
    } else {
        strncpy(result->description, "No protection detected", 255); result->description[255] = '\0';
    }
    
    return UFT_TRS80_SUCCESS;
}

/*============================================================================
 * Format Conversion
 *============================================================================*/

uft_trs80_rc_t uft_trs80_jv1_to_jv3(const char* jv1_path, const char* jv3_path) {
    if (!jv1_path || !jv3_path) return UFT_TRS80_ERR_ARG;
    
    /* Open JV1 */
    uft_trs80_ctx_t jv1_ctx;
    uft_trs80_rc_t rc = uft_trs80_open(&jv1_ctx, jv1_path, false);
    if (rc != UFT_TRS80_SUCCESS) return rc;
    
    if (jv1_ctx.format != UFT_TRS80_FMT_JV1) {
        uft_trs80_close(&jv1_ctx);
        return UFT_TRS80_ERR_FORMAT;
    }
    
    FILE* out = fopen(jv3_path, "wb");
    if (!out) {
        uft_trs80_close(&jv1_ctx);
        return UFT_TRS80_ERR_IO;
    }
    
    /* Build JV3 sector map */
    uft_jv3_sector_header_t sectors[UFT_JV3_SECTORS_MAX];
    memset(sectors, 0xFF, sizeof(sectors));  /* Mark all free */
    
    int sector_idx = 0;
    for (uint8_t t = 0; t < UFT_JV1_TRACKS && sector_idx < UFT_JV3_SECTORS_MAX; t++) {
        for (uint8_t s = 0; s < UFT_JV1_SECTORS && sector_idx < UFT_JV3_SECTORS_MAX; s++) {
            sectors[sector_idx].track = t;
            sectors[sector_idx].sector = s;
            sectors[sector_idx].flags = 0;  /* SD, side 0, 256 bytes, no error */
            sector_idx++;
        }
    }
    
    /* Write header */
    if (fwrite(sectors, sizeof(sectors), 1, out) != 1) {
        fclose(out);
        uft_trs80_close(&jv1_ctx);
        return UFT_TRS80_ERR_IO;
    }
    
    /* Write protect flag */
    uint8_t wp = 0x00;
    if (fwrite(&wp, 1, 1, out) != 1) {
        fclose(out);
        uft_trs80_close(&jv1_ctx);
        return UFT_TRS80_ERR_IO;
    }
    
    /* Copy sector data */
    uint8_t buffer[UFT_JV1_SECTOR_SIZE];
    for (uint8_t t = 0; t < UFT_JV1_TRACKS; t++) {
        for (uint8_t s = 0; s < UFT_JV1_SECTORS; s++) {
            rc = uft_jv1_read_sector(&jv1_ctx, t, s, buffer, sizeof(buffer));
            if (rc != UFT_TRS80_SUCCESS) {
                fclose(out);
                uft_trs80_close(&jv1_ctx);
                return rc;
            }
            
            if (fwrite(buffer, UFT_JV1_SECTOR_SIZE, 1, out) != 1) {
                fclose(out);
                uft_trs80_close(&jv1_ctx);
                return UFT_TRS80_ERR_IO;
            }
        }
    }
    
    fclose(out);
    uft_trs80_close(&jv1_ctx);
    return UFT_TRS80_SUCCESS;
}

uft_trs80_rc_t uft_trs80_to_raw(uft_trs80_ctx_t* ctx, const char* output_path) {
    if (!ctx || !ctx->path || !output_path) return UFT_TRS80_ERR_ARG;
    
    FILE* out = fopen(output_path, "wb");
    if (!out) return UFT_TRS80_ERR_IO;
    
    uint8_t buffer[1024];
    
    if (ctx->format == UFT_TRS80_FMT_JV1 || ctx->format == UFT_TRS80_FMT_JVC) {
        /* Simple sector-by-sector copy */
        uint16_t sector_size = ctx->geometry.sector_size ? ctx->geometry.sector_size : 256;
        uint8_t sectors = ctx->geometry.sectors_per_track;
        if (sectors == 0) sectors = UFT_JV1_SECTORS;
        uint8_t heads = ctx->geometry.heads;
        if (heads == 0) heads = 1;
        uint8_t tracks = ctx->geometry.tracks;
        if (tracks == 0) tracks = UFT_JV1_TRACKS;
        
        for (uint8_t t = 0; t < tracks; t++) {
            for (uint8_t h = 0; h < heads; h++) {
                for (uint8_t s = 0; s < sectors; s++) {
                    uft_trs80_rc_t rc;
                    if (ctx->format == UFT_TRS80_FMT_JV1) {
                        rc = uft_jv1_read_sector(ctx, t, s, buffer, sizeof(buffer));
                    } else {
                        rc = uft_jvc_read_sector(ctx, t, h, s, buffer, sizeof(buffer));
                    }
                    
                    if (rc == UFT_TRS80_SUCCESS) {
                        if (fwrite(buffer, sector_size, 1, out) != 1) { /* I/O error */ }
                    } else {
                        /* Write zeros for missing sectors */
                        memset(buffer, 0, sector_size);
                        if (fwrite(buffer, sector_size, 1, out) != 1) { /* I/O error */ }
                    }
                }
            }
        }
    }
    else if (ctx->format == UFT_TRS80_FMT_JV3) {
        /* Export sectors in order */
        for (uint16_t i = 0; i < ctx->jv3_sector_count; i++) {
            uint16_t size = uft_jv3_sector_size(ctx->jv3_sectors[i].flags);
            
            uft_trs80_rc_t rc = uft_jv3_read_sector(ctx,
                ctx->jv3_sectors[i].track,
                (ctx->jv3_sectors[i].flags & UFT_JV3_FLAG_SIDES) ? 1 : 0,
                ctx->jv3_sectors[i].sector,
                buffer, sizeof(buffer));
            
            if (rc == UFT_TRS80_SUCCESS) {
                if (fwrite(buffer, size, 1, out) != 1) { /* I/O error */ }
            }
        }
    }
    
    fclose(out);
    return UFT_TRS80_SUCCESS;
}

/*============================================================================
 * Analysis and Reporting
 *============================================================================*/

uft_trs80_rc_t uft_trs80_analyze(const char* path, uft_trs80_report_t* report) {
    if (!path || !report) return UFT_TRS80_ERR_ARG;
    
    memset(report, 0, sizeof(*report));
    
    uft_trs80_ctx_t ctx;
    uft_trs80_rc_t rc = uft_trs80_open(&ctx, path, false);
    if (rc != UFT_TRS80_SUCCESS) return rc;
    
    report->format = ctx.format;
    report->geometry = ctx.geometry;
    report->dos_type = ctx.dos_type;
    report->model = ctx.model;
    
    /* Calculate sector counts */
    if (ctx.format == UFT_TRS80_FMT_JV3) {
        report->total_sectors = ctx.jv3_sector_count;
        for (uint16_t i = 0; i < ctx.jv3_sector_count; i++) {
            if (ctx.jv3_sectors[i].flags & UFT_JV3_FLAG_ERROR) {
                report->error_sectors++;
            }
        }
    } else {
        report->total_sectors = ctx.geometry.tracks * ctx.geometry.heads *
                                ctx.geometry.sectors_per_track;
    }
    
    report->used_sectors = report->total_sectors - report->error_sectors;
    
    /* Check bootability */
    uint8_t boot[256];
    if (ctx.format == UFT_TRS80_FMT_JV1) {
        rc = uft_jv1_read_sector(&ctx, 0, 0, boot, sizeof(boot));
    } else if (ctx.format == UFT_TRS80_FMT_JVC) {
        rc = uft_jvc_read_sector(&ctx, 0, 0, 0, boot, sizeof(boot));
    } else if (ctx.format == UFT_TRS80_FMT_JV3) {
        rc = uft_jv3_read_sector(&ctx, 0, 0, 0, boot, sizeof(boot));
    } else {
        rc = UFT_TRS80_ERR_FORMAT;
    }
    
    if (rc == UFT_TRS80_SUCCESS) {
        report->is_bootable = (boot[0] != 0x00 || boot[1] != 0x00);
    }
    
    /* Protection detection */
    uft_trs80_detect_protection(&ctx, &report->protection);
    
    uft_trs80_close(&ctx);
    return UFT_TRS80_SUCCESS;
}

int uft_trs80_report_to_json(const uft_trs80_report_t* report, 
                              char* json_out, size_t capacity) {
    if (!report || !json_out || capacity < 512) return -1;
    
    return snprintf(json_out, capacity,
        "{\n"
        "  \"format\": \"%s\",\n"
        "  \"model\": \"%s\",\n"
        "  \"dos\": \"%s\",\n"
        "  \"geometry\": {\n"
        "    \"name\": \"%s\",\n"
        "    \"tracks\": %u,\n"
        "    \"heads\": %u,\n"
        "    \"sectors_per_track\": %u,\n"
        "    \"sector_size\": %u,\n"
        "    \"total_bytes\": %u\n"
        "  },\n"
        "  \"sectors\": {\n"
        "    \"total\": %u,\n"
        "    \"used\": %u,\n"
        "    \"errors\": %u\n"
        "  },\n"
        "  \"bootable\": %s,\n"
        "  \"protection\": {\n"
        "    \"detected\": %s,\n"
        "    \"confidence\": %u,\n"
        "    \"description\": \"%s\"\n"
        "  }\n"
        "}",
        uft_trs80_format_name(report->format),
        uft_trs80_model_name(report->model),
        uft_trs80_dos_name(report->dos_type),
        report->geometry.name ? report->geometry.name : "Unknown",
        report->geometry.tracks,
        report->geometry.heads,
        report->geometry.sectors_per_track,
        report->geometry.sector_size,
        report->geometry.total_bytes,
        report->total_sectors,
        report->used_sectors,
        report->error_sectors,
        report->is_bootable ? "true" : "false",
        report->protection.flags ? "true" : "false",
        report->protection.confidence,
        report->protection.description
    );
}

int uft_trs80_report_to_markdown(const uft_trs80_report_t* report,
                                  char* md_out, size_t capacity) {
    if (!report || !md_out || capacity < 1024) return -1;
    
    return snprintf(md_out, capacity,
        "# TRS-80 Disk Analysis Report\n\n"
        "## System Information\n"
        "- **Format**: %s\n"
        "- **Model**: %s\n"
        "- **DOS**: %s\n\n"
        "## Geometry\n"
        "- **Type**: %s\n"
        "- **Tracks**: %u\n"
        "- **Heads**: %u\n"
        "- **Sectors/Track**: %u\n"
        "- **Sector Size**: %u bytes\n"
        "- **Total Size**: %u bytes\n\n"
        "## Sector Statistics\n"
        "| Metric | Value |\n"
        "|--------|-------|\n"
        "| Total Sectors | %u |\n"
        "| Used | %u |\n"
        "| Errors | %u |\n\n"
        "## Boot Status\n"
        "- **Bootable**: %s\n\n"
        "## Copy Protection\n"
        "- **Detected**: %s\n"
        "- **Confidence**: %u%%\n"
        "- **Details**: %s\n",
        uft_trs80_format_name(report->format),
        uft_trs80_model_name(report->model),
        uft_trs80_dos_name(report->dos_type),
        report->geometry.name ? report->geometry.name : "Unknown",
        report->geometry.tracks,
        report->geometry.heads,
        report->geometry.sectors_per_track,
        report->geometry.sector_size,
        report->geometry.total_bytes,
        report->total_sectors,
        report->used_sectors,
        report->error_sectors,
        report->is_bootable ? "Yes" : "No",
        report->protection.flags ? "Yes" : "No",
        report->protection.confidence,
        report->protection.description
    );
}

/*============================================================================
 * Format Creation
 *============================================================================*/

uft_trs80_rc_t uft_trs80_create_blank(const char* path, uft_trs80_format_t format,
                                       uft_trs80_geometry_type_t geometry) {
    if (!path) return UFT_TRS80_ERR_ARG;
    
    if (format == UFT_TRS80_FMT_JV1) {
        return uft_jv1_create_blank(path);
    }
    
    /* For other formats, create based on geometry */
    const uft_trs80_geometry_t* geom = uft_trs80_get_geometry(geometry);
    if (geom->total_bytes == 0) return UFT_TRS80_ERR_GEOMETRY;
    
    FILE* fp = fopen(path, "wb");
    if (!fp) return UFT_TRS80_ERR_IO;
    
    /* Fill with zeros */
    uint8_t zeros[4096] = {0};
    size_t remaining = geom->total_bytes;
    
    while (remaining > 0) {
        size_t chunk = remaining < sizeof(zeros) ? remaining : sizeof(zeros);
        if (fwrite(zeros, 1, chunk, fp) != chunk) {
            fclose(fp);
            return UFT_TRS80_ERR_IO;
        }
        remaining -= chunk;
    }
    
    fclose(fp);
    return UFT_TRS80_SUCCESS;
}
