/**
 * @file uft_trsdos_file.c
 * @brief TRSDOS/LDOS/NewDOS Filesystem - File Write Operations
 * 
 * File injection, deletion, rename, image creation
 */

#include "uft/fs/uft_trsdos.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/*===========================================================================
 * File Injection - RS-DOS
 *===========================================================================*/

/**
 * @brief Find free directory entry for RS-DOS
 */
static int find_free_rsdos_entry(uft_trsdos_ctx_t *ctx) {
    uint8_t sector[256];
    
    for (int sec = 2; sec < 11; sec++) {
        if (uft_trsdos_read_sector(ctx, 17, 0, sec, sector, sizeof(sector)) != UFT_TRSDOS_OK) {
            continue;
        }
        
        for (int i = 0; i < 8; i++) {
            uint8_t first = sector[i * 32];
            if (first == 0x00 || first == 0xFF) {
                return (sec - 2) * 8 + i;
            }
        }
    }
    
    return -1; /* Directory full */
}

/**
 * @brief Allocate granules for RS-DOS file
 */
static int allocate_rsdos_chain(uft_trsdos_ctx_t *ctx, size_t size, 
                                 uint8_t *first_granule, uint8_t *last_sectors) {
    /* Calculate granules needed */
    size_t granules_needed = (size + (9 * 256) - 1) / (9 * 256);
    if (granules_needed == 0) granules_needed = 1;
    
    /* Calculate last sector count */
    size_t full_granules = (granules_needed > 1) ? granules_needed - 1 : 0;
    size_t remaining = size - (full_granules * 9 * 256);
    *last_sectors = (remaining + 255) / 256;
    if (*last_sectors == 0) *last_sectors = 1;
    if (*last_sectors > 9) *last_sectors = 9;
    
    /* Read FAT */
    uint8_t fat[256];
    if (uft_trsdos_read_sector(ctx, 17, 0, 1, fat, sizeof(fat)) != UFT_TRSDOS_OK) {
        return -1;
    }
    
    /* Allocate chain */
    uint8_t prev = 0xFF;
    uint8_t first = 0xFF;
    
    for (size_t g = 0; g < granules_needed; g++) {
        /* Find free granule */
        int free_gran = -1;
        for (int i = 0; i < 68; i++) {
            if (fat[i] == 0xFF || fat[i] == 0x00) {
                free_gran = i;
                break;
            }
        }
        
        if (free_gran < 0) {
            return -1; /* Disk full */
        }
        
        if (first == 0xFF) {
            first = free_gran;
        }
        
        if (prev != 0xFF) {
            fat[prev] = free_gran;
        }
        
        if (g == granules_needed - 1) {
            /* Last granule - encode sector count */
            fat[free_gran] = 0xC0 + *last_sectors;
        } else {
            fat[free_gran] = 0xFE; /* Temporary marker */
        }
        
        prev = free_gran;
    }
    
    /* Write FAT back */
    if (uft_trsdos_write_sector(ctx, 17, 0, 1, fat, sizeof(fat)) != UFT_TRSDOS_OK) {
        return -1;
    }
    
    *first_granule = first;
    return (int)granules_needed;
}

/**
 * @brief Write data to RS-DOS granule chain
 */
static uft_trsdos_err_t write_rsdos_data(uft_trsdos_ctx_t *ctx,
                                          uint8_t first_granule,
                                          const uint8_t *data, size_t size) {
    uint8_t fat[256];
    if (uft_trsdos_read_sector(ctx, 17, 0, 1, fat, sizeof(fat)) != UFT_TRSDOS_OK) {
        return UFT_TRSDOS_ERR_IO;
    }
    
    uint8_t granule = first_granule;
    size_t offset = 0;
    uint8_t sector_buf[256];
    
    while (granule < 68 && offset < size) {
        uint8_t next = fat[granule];
        int sectors_in_gran = 9;
        
        if (next >= 0xC0 && next <= 0xC9) {
            sectors_in_gran = next - 0xC0;
        }
        
        /* Calculate track and sector */
        uint8_t track = granule / 2;
        if (track >= 17) track++;
        uint8_t sector = (granule % 2) * 9;
        
        /* Write sectors */
        for (int s = 0; s < sectors_in_gran && offset < size; s++) {
            size_t to_write = size - offset;
            if (to_write > 256) to_write = 256;
            
            memset(sector_buf, 0, sizeof(sector_buf));
            memcpy(sector_buf, data + offset, to_write);
            
            uft_trsdos_err_t err = uft_trsdos_write_sector(ctx, track, 0, 
                sector + s, sector_buf, 256);
            if (err != UFT_TRSDOS_OK) return err;
            
            offset += 256;
        }
        
        /* Next granule */
        if (next >= 0xC0) break;
        granule = next;
    }
    
    return UFT_TRSDOS_OK;
}

/**
 * @brief Create RS-DOS directory entry
 */
static uft_trsdos_err_t create_rsdos_entry(uft_trsdos_ctx_t *ctx,
                                            int entry_index,
                                            const char *name, const char *ext,
                                            uint8_t file_type, bool ascii,
                                            uint8_t first_granule,
                                            uint16_t last_bytes) {
    int sector_num = (entry_index / 8) + 2;
    int entry_in_sector = entry_index % 8;
    
    uint8_t sector[256];
    uft_trsdos_err_t err = uft_trsdos_read_sector(ctx, 17, 0, sector_num, sector, sizeof(sector));
    if (err != UFT_TRSDOS_OK) return err;
    
    uft_rsdos_dir_entry_t *de = (uft_rsdos_dir_entry_t *)(sector + entry_in_sector * 32);
    
    /* Clear entry */
    memset(de, 0, 32);
    
    /* Set filename (space-padded) */
    memset(de->name, ' ', 8);
    memset(de->ext, ' ', 3);
    
    size_t name_len = strlen(name);
    if (name_len > 8) name_len = 8;
    for (size_t i = 0; i < name_len; i++) {
        de->name[i] = toupper((unsigned char)name[i]);
    }
    
    if (ext) {
        size_t ext_len = strlen(ext);
        if (ext_len > 3) ext_len = 3;
        for (size_t i = 0; i < ext_len; i++) {
            de->ext[i] = toupper((unsigned char)ext[i]);
        }
    }
    
    de->file_type = file_type;
    de->ascii_flag = ascii ? 0xFF : 0x00;
    de->first_granule = first_granule;
    de->last_sector_bytes = last_bytes;
    
    return uft_trsdos_write_sector(ctx, 17, 0, sector_num, sector, sizeof(sector));
}

/**
 * @brief Inject file into RS-DOS disk
 */
static uft_trsdos_err_t inject_rsdos(uft_trsdos_ctx_t *ctx,
                                      const char *name, const char *ext,
                                      const uint8_t *data, size_t size,
                                      const uft_trsdos_attrib_t *attrib) {
    /* Check if file exists */
    uft_trsdos_entry_t existing;
    if (uft_trsdos_find_file(ctx, name, ext, &existing) == UFT_TRSDOS_OK) {
        return UFT_TRSDOS_ERR_EXISTS;
    }
    
    /* Find free directory entry */
    int entry_idx = find_free_rsdos_entry(ctx);
    if (entry_idx < 0) {
        return UFT_TRSDOS_ERR_FULL;
    }
    
    /* Allocate granule chain */
    uint8_t first_granule;
    uint8_t last_sectors;
    int granules = allocate_rsdos_chain(ctx, size, &first_granule, &last_sectors);
    if (granules < 0) {
        return UFT_TRSDOS_ERR_FULL;
    }
    
    /* Write data */
    uft_trsdos_err_t err = write_rsdos_data(ctx, first_granule, data, size);
    if (err != UFT_TRSDOS_OK) {
        return err;
    }
    
    /* Calculate last sector bytes */
    uint16_t last_bytes = size % 256;
    if (last_bytes == 0 && size > 0) last_bytes = 256;
    
    /* Determine file type from extension */
    uint8_t file_type = 1; /* Data */
    bool ascii = false;
    
    if (ext) {
        if (strncasecmp(ext, "BAS", 3) == 0) file_type = 0;
        else if (strncasecmp(ext, "BIN", 3) == 0) file_type = 2;
        else if (strncasecmp(ext, "TXT", 3) == 0) {
            file_type = 3;
            ascii = true;
        }
    }
    
    /* Create directory entry */
    return create_rsdos_entry(ctx, entry_idx, name, ext, file_type, ascii,
                              first_granule, last_bytes);
}

/*===========================================================================
 * File Injection - TRSDOS
 *===========================================================================*/

uft_trsdos_err_t uft_trsdos_inject(uft_trsdos_ctx_t *ctx,
                                    const char *name, const char *ext,
                                    const uint8_t *data, size_t size,
                                    const uft_trsdos_attrib_t *attrib) {
    if (!ctx || !name || !data) {
        return UFT_TRSDOS_ERR_NULL;
    }
    
    if (!ctx->writable) {
        return UFT_TRSDOS_ERR_READONLY;
    }
    
    switch (ctx->version) {
        case UFT_TRSDOS_VERSION_RSDOS:
            return inject_rsdos(ctx, name, ext, data, size, attrib);
        default:
            /* TRSDOS 2.3/6/LDOS injection - similar structure */
            return inject_rsdos(ctx, name, ext, data, size, attrib);
    }
}

uft_trsdos_err_t uft_trsdos_inject_from_file(uft_trsdos_ctx_t *ctx,
                                              const char *name, const char *ext,
                                              const char *input_path,
                                              const uft_trsdos_attrib_t *attrib) {
    if (!ctx || !input_path) {
        return UFT_TRSDOS_ERR_NULL;
    }
    
    /* Read input file */
    FILE *fp = fopen(input_path, "rb");
    if (!fp) {
        return UFT_TRSDOS_ERR_IO;
    }
    
    if (fseek(fp, 0, SEEK_END) != 0) { /* seek error */ }
    long file_size = ftell(fp);
    if (fseek(fp, 0, SEEK_SET) != 0) { /* seek error */ }
    if (file_size <= 0 || file_size > 1024 * 1024) { /* Max 1MB */
        fclose(fp);
        return UFT_TRSDOS_ERR_RANGE;
    }
    
    uint8_t *data = malloc(file_size);
    if (!data) {
        fclose(fp);
        return UFT_TRSDOS_ERR_NOMEM;
    }
    
    size_t read = fread(data, 1, file_size, fp);
    fclose(fp);
    
    if (read != (size_t)file_size) {
        free(data);
        return UFT_TRSDOS_ERR_IO;
    }
    
    /* Extract name from path if not provided */
    const char *use_name = name;
    const char *use_ext = ext;
    
    char parsed_name[16], parsed_ext[8];
    if (!name) {
        const char *base = strrchr(input_path, '/');
        if (!base) base = strrchr(input_path, '\\');
        if (!base) base = input_path - 1;
        base++;
        
        uft_trsdos_parse_filename(base, parsed_name, parsed_ext);
        use_name = parsed_name;
        use_ext = parsed_ext;
    }
    
    uft_trsdos_err_t err = uft_trsdos_inject(ctx, use_name, use_ext, data, read, attrib);
    free(data);
    
    return err;
}

/*===========================================================================
 * File Deletion
 *===========================================================================*/

/**
 * @brief Free RS-DOS granule chain
 */
static void free_rsdos_chain(uft_trsdos_ctx_t *ctx, uint8_t first_granule) {
    uint8_t fat[256];
    if (uft_trsdos_read_sector(ctx, 17, 0, 1, fat, sizeof(fat)) != UFT_TRSDOS_OK) {
        return;
    }
    
    uint8_t granule = first_granule;
    while (granule < 68) {
        uint8_t next = fat[granule];
        fat[granule] = 0xFF; /* Mark as free */
        
        if (next >= 0xC0) break;
        granule = next;
    }
    
    uft_trsdos_write_sector(ctx, 17, 0, 1, fat, sizeof(fat));
}

/**
 * @brief Delete RS-DOS file
 */
static uft_trsdos_err_t delete_rsdos(uft_trsdos_ctx_t *ctx,
                                      const char *name, const char *ext) {
    /* Find file */
    uft_trsdos_entry_t entry;
    uft_trsdos_err_t err = uft_trsdos_find_file(ctx, name, ext, &entry);
    if (err != UFT_TRSDOS_OK) return err;
    
    /* Free granule chain */
    free_rsdos_chain(ctx, entry.extents[0].start_granule);
    
    /* Delete directory entry */
    int sector_num = (entry.dir_entry_index / 8) + 2;
    int entry_in_sector = entry.dir_entry_index % 8;
    
    uint8_t sector[256];
    err = uft_trsdos_read_sector(ctx, 17, 0, sector_num, sector, sizeof(sector));
    if (err != UFT_TRSDOS_OK) return err;
    
    /* Mark entry as deleted (first byte = 0x00) */
    memset(sector + entry_in_sector * 32, 0, 32);
    
    return uft_trsdos_write_sector(ctx, 17, 0, sector_num, sector, sizeof(sector));
}

uft_trsdos_err_t uft_trsdos_delete(uft_trsdos_ctx_t *ctx,
                                    const char *name, const char *ext) {
    if (!ctx || !name) {
        return UFT_TRSDOS_ERR_NULL;
    }
    
    if (!ctx->writable) {
        return UFT_TRSDOS_ERR_READONLY;
    }
    
    switch (ctx->version) {
        case UFT_TRSDOS_VERSION_RSDOS:
            return delete_rsdos(ctx, name, ext);
        default:
            return delete_rsdos(ctx, name, ext); /* Similar */
    }
}

/*===========================================================================
 * File Rename
 *===========================================================================*/

uft_trsdos_err_t uft_trsdos_rename(uft_trsdos_ctx_t *ctx,
                                    const char *old_name, const char *old_ext,
                                    const char *new_name, const char *new_ext) {
    if (!ctx || !old_name || !new_name) {
        return UFT_TRSDOS_ERR_NULL;
    }
    
    if (!ctx->writable) {
        return UFT_TRSDOS_ERR_READONLY;
    }
    
    /* Find old file */
    uft_trsdos_entry_t entry;
    uft_trsdos_err_t err = uft_trsdos_find_file(ctx, old_name, old_ext, &entry);
    if (err != UFT_TRSDOS_OK) return err;
    
    /* Check new name doesn't exist */
    uft_trsdos_entry_t check;
    if (uft_trsdos_find_file(ctx, new_name, new_ext, &check) == UFT_TRSDOS_OK) {
        return UFT_TRSDOS_ERR_EXISTS;
    }
    
    /* Update directory entry */
    int sector_num, entry_in_sector;
    
    if (ctx->version == UFT_TRSDOS_VERSION_RSDOS) {
        sector_num = (entry.dir_entry_index / 8) + 2;
        entry_in_sector = entry.dir_entry_index % 8;
    } else {
        sector_num = (entry.dir_entry_index / 5) + 1;
        entry_in_sector = entry.dir_entry_index % 5;
    }
    
    uint8_t sector[256];
    err = uft_trsdos_read_sector(ctx, 17, 0, sector_num, sector, sizeof(sector));
    if (err != UFT_TRSDOS_OK) return err;
    
    /* Calculate entry offset */
    size_t offset;
    if (ctx->version == UFT_TRSDOS_VERSION_RSDOS) {
        offset = entry_in_sector * 32;
    } else if (ctx->version == UFT_TRSDOS_VERSION_23) {
        offset = entry_in_sector * 48;
    } else {
        offset = entry_in_sector * 32;
    }
    
    /* Update name */
    char norm_name[8], norm_ext[3];
    memset(norm_name, ' ', 8);
    memset(norm_ext, ' ', 3);
    
    size_t name_len = strlen(new_name);
    if (name_len > 8) name_len = 8;
    for (size_t i = 0; i < name_len; i++) {
        norm_name[i] = toupper((unsigned char)new_name[i]);
    }
    
    if (new_ext) {
        size_t ext_len = strlen(new_ext);
        if (ext_len > 3) ext_len = 3;
        for (size_t i = 0; i < ext_len; i++) {
            norm_ext[i] = toupper((unsigned char)new_ext[i]);
        }
    }
    
    if (ctx->version == UFT_TRSDOS_VERSION_RSDOS) {
        memcpy(sector + offset, norm_name, 8);
        memcpy(sector + offset + 8, norm_ext, 3);
    } else if (ctx->version == UFT_TRSDOS_VERSION_23) {
        memcpy(sector + offset + 8, norm_name, 8);
        memcpy(sector + offset + 16, norm_ext, 3);
    } else {
        memcpy(sector + offset + 1, norm_name, 8);
        memcpy(sector + offset + 9, norm_ext, 3);
    }
    
    return uft_trsdos_write_sector(ctx, 17, 0, sector_num, sector, sizeof(sector));
}

/*===========================================================================
 * Attributes
 *===========================================================================*/

uft_trsdos_err_t uft_trsdos_set_attrib(uft_trsdos_ctx_t *ctx,
                                        const char *name, const char *ext,
                                        const uft_trsdos_attrib_t *attrib) {
    if (!ctx || !name || !attrib) {
        return UFT_TRSDOS_ERR_NULL;
    }
    
    if (!ctx->writable) {
        return UFT_TRSDOS_ERR_READONLY;
    }
    
    /* Find file */
    uft_trsdos_entry_t entry;
    uft_trsdos_err_t err = uft_trsdos_find_file(ctx, name, ext, &entry);
    if (err != UFT_TRSDOS_OK) return err;
    
    /* RS-DOS doesn't support attributes */
    if (ctx->version == UFT_TRSDOS_VERSION_RSDOS) {
        return UFT_TRSDOS_ERR_INVALID;
    }
    
    /* Update attribute byte in directory entry */
    int sector_num = (entry.dir_entry_index / 5) + 1;
    int entry_in_sector = entry.dir_entry_index % 5;
    
    uint8_t sector[256];
    err = uft_trsdos_read_sector(ctx, 17, 0, sector_num, sector, sizeof(sector));
    if (err != UFT_TRSDOS_OK) return err;
    
    size_t offset = entry_in_sector * (ctx->version == UFT_TRSDOS_VERSION_23 ? 48 : 32);
    
    /* Build attribute byte */
    uint8_t attr = 0;
    attr |= (attrib->visibility & 0x03) << 6;
    attr |= (attrib->protection & 0x07) << 3;
    if (attrib->is_system) attr |= 0x04;
    
    sector[offset] = attr;
    
    return uft_trsdos_write_sector(ctx, 17, 0, sector_num, sector, sizeof(sector));
}

uft_trsdos_err_t uft_trsdos_set_password(uft_trsdos_ctx_t *ctx,
                                          const char *name, const char *ext,
                                          const char *password) {
    if (!ctx || !name) {
        return UFT_TRSDOS_ERR_NULL;
    }
    
    if (!ctx->writable) {
        return UFT_TRSDOS_ERR_READONLY;
    }
    
    /* RS-DOS doesn't support passwords */
    if (ctx->version == UFT_TRSDOS_VERSION_RSDOS) {
        return UFT_TRSDOS_ERR_INVALID;
    }
    
    /* Find file */
    uft_trsdos_entry_t entry;
    uft_trsdos_err_t err = uft_trsdos_find_file(ctx, name, ext, &entry);
    if (err != UFT_TRSDOS_OK) return err;
    
    /* Calculate password hash */
    uint8_t hash[2] = {0, 0};
    if (password && *password) {
        uft_trsdos_hash_password(password, hash);
    }
    
    /* Update password in directory entry */
    int sector_num, entry_in_sector;
    size_t offset, pw_offset;
    
    if (ctx->version == UFT_TRSDOS_VERSION_23) {
        sector_num = (entry.dir_entry_index / 5) + 1;
        entry_in_sector = entry.dir_entry_index % 5;
        offset = entry_in_sector * 48;
        pw_offset = 6; /* Password at bytes 6-7 */
    } else {
        sector_num = (entry.dir_entry_index / 8) + 1;
        entry_in_sector = entry.dir_entry_index % 8;
        offset = entry_in_sector * 32;
        pw_offset = 12; /* Update password at bytes 12-13 */
    }
    
    uint8_t sector[256];
    err = uft_trsdos_read_sector(ctx, 17, 0, sector_num, sector, sizeof(sector));
    if (err != UFT_TRSDOS_OK) return err;
    
    sector[offset + pw_offset] = hash[0];
    sector[offset + pw_offset + 1] = hash[1];
    
    return uft_trsdos_write_sector(ctx, 17, 0, sector_num, sector, sizeof(sector));
}

/*===========================================================================
 * Image Creation
 *===========================================================================*/

/**
 * @brief Initialize RS-DOS FAT
 */
static void init_rsdos_fat(uint8_t *fat) {
    /* All granules free (0xFF) except for directory track */
    memset(fat, 0xFF, 256);
    
    /* Mark granules 34-35 (track 17) as used */
    fat[34] = 0xFE; /* Directory */
    fat[35] = 0xFE;
}

/**
 * @brief Initialize RS-DOS directory
 */
static void init_rsdos_dir(uint8_t *dir_sectors, int count) {
    /* All entries deleted (0xFF or 0x00) */
    for (int i = 0; i < count; i++) {
        memset(dir_sectors + i * 256, 0xFF, 256);
    }
}

/**
 * @brief Create RS-DOS blank image
 */
static uft_trsdos_err_t create_rsdos_image(uft_trsdos_geom_type_t geom,
                                            uint8_t **data, size_t *size) {
    const uft_trsdos_geometry_t *g = uft_trsdos_get_geometry(geom);
    
    size_t img_size = g->total_bytes;
    uint8_t *img = calloc(1, img_size);
    if (!img) return UFT_TRSDOS_ERR_NOMEM;
    
    /* Initialize FAT at track 17, sector 2 */
    uint8_t fat[256];
    init_rsdos_fat(fat);
    memcpy(img + 17 * 18 * 256 + 1 * 256, fat, 256);
    
    /* Initialize directory (track 17, sectors 3-11) */
    for (int sec = 2; sec < 11; sec++) {
        memset(img + 17 * 18 * 256 + sec * 256, 0xFF, 256);
    }
    
    *data = img;
    *size = img_size;
    
    return UFT_TRSDOS_OK;
}

/**
 * @brief Initialize TRSDOS 2.3 GAT
 */
static void init_trsdos23_gat(uint8_t *gat, int tracks) {
    memset(gat, 0, 256);
    
    /* Mark directory track as used */
    gat[17] = 0xFF; /* All granules on track 17 used */
}

/**
 * @brief Create TRSDOS 2.3 blank image
 */
static uft_trsdos_err_t create_trsdos23_image(uft_trsdos_geom_type_t geom,
                                               const char *disk_name,
                                               uint8_t **data, size_t *size) {
    const uft_trsdos_geometry_t *g = uft_trsdos_get_geometry(geom);
    
    size_t img_size = g->total_bytes;
    uint8_t *img = calloc(1, img_size);
    if (!img) return UFT_TRSDOS_ERR_NOMEM;
    
    /* Initialize GAT at track 17, sector 0 */
    uint8_t gat[256];
    init_trsdos23_gat(gat, g->tracks);
    memcpy(img + 17 * 10 * 256, gat, 256);
    
    /* Initialize directory (track 17, sectors 1-9) as empty */
    for (int sec = 1; sec < 10; sec++) {
        memset(img + 17 * 10 * 256 + sec * 256, 0, 256);
    }
    
    *data = img;
    *size = img_size;
    
    return UFT_TRSDOS_OK;
}

uft_trsdos_err_t uft_trsdos_create_image(uft_trsdos_version_t version,
                                          uft_trsdos_geom_type_t geom,
                                          const char *disk_name,
                                          uint8_t **data, size_t *size) {
    if (!data || !size) {
        return UFT_TRSDOS_ERR_NULL;
    }
    
    switch (version) {
        case UFT_TRSDOS_VERSION_RSDOS:
            return create_rsdos_image(geom, data, size);
        case UFT_TRSDOS_VERSION_23:
            return create_trsdos23_image(geom, disk_name, data, size);
        default:
            /* TRSDOS 6/LDOS - use similar to TRSDOS 2.3 */
            return create_trsdos23_image(geom, disk_name, data, size);
    }
}

uft_trsdos_err_t uft_trsdos_format(uft_trsdos_ctx_t *ctx, const char *disk_name) {
    if (!ctx || !ctx->data) {
        return UFT_TRSDOS_ERR_NULL;
    }
    
    if (!ctx->writable) {
        return UFT_TRSDOS_ERR_READONLY;
    }
    
    /* Clear all data */
    memset(ctx->data, 0, ctx->size);
    
    /* Re-initialize structures based on version */
    uint8_t *new_data = NULL;
    size_t new_size = 0;
    
    uft_trsdos_geom_type_t geom = UFT_TRSDOS_GEOM_M1_SSSD;
    if (ctx->geometry.total_bytes == 161280) {
        geom = ctx->version == UFT_TRSDOS_VERSION_RSDOS ? 
               UFT_TRSDOS_GEOM_COCO_SSSD : UFT_TRSDOS_GEOM_M1_SSDD;
    } else if (ctx->geometry.total_bytes == 368640) {
        geom = UFT_TRSDOS_GEOM_M3_DSDD;
    }
    
    uft_trsdos_err_t err = uft_trsdos_create_image(ctx->version, geom, disk_name,
                                                    &new_data, &new_size);
    if (err != UFT_TRSDOS_OK) return err;
    
    /* Copy initialized data */
    if (new_size <= ctx->size) {
        memcpy(ctx->data, new_data, new_size);
    }
    free(new_data);
    
    /* Re-read GAT */
    return uft_trsdos_read_gat(ctx);
}

/*===========================================================================
 * Validation
 *===========================================================================*/

int uft_trsdos_validate(uft_trsdos_ctx_t *ctx, bool fix,
                        char *report, size_t report_size) {
    if (!ctx) return -1;
    
    int errors = 0;
    int pos = 0;
    
    if (report && report_size > 0) {
        pos += snprintf(report + pos, report_size - pos,
            "TRSDOS Disk Validation Report\n"
            "==============================\n"
            "Version: %s\n\n", uft_trsdos_version_name(ctx->version));
    }
    
    /* Read directory */
    uft_trsdos_dir_t dir;
    uft_trsdos_err_t err = uft_trsdos_read_dir(ctx, &dir);
    if (err != UFT_TRSDOS_OK) {
        if (report) {
            pos += snprintf(report + pos, report_size - pos,
                "ERROR: Cannot read directory\n");
        }
        return -1;
    }
    
    /* Check for cross-linked granules */
    uint8_t granule_usage[256] = {0};
    
    for (size_t i = 0; i < dir.count; i++) {
        uft_trsdos_entry_t *e = &dir.entries[i];
        
        if (ctx->version == UFT_TRSDOS_VERSION_RSDOS) {
            /* Follow chain */
            uint8_t fat[256];
            uft_trsdos_read_sector(ctx, 17, 0, 1, fat, sizeof(fat));
            
            uint8_t g = e->extents[0].start_granule;
            while (g < 68) {
                if (granule_usage[g]) {
                    if (report) {
                        char name[16];
                        uft_trsdos_format_filename(e->name, e->ext, name);
                        pos += snprintf(report + pos, report_size - pos,
                            "ERROR: Cross-link at granule %d (file: %s)\n", g, name);
                    }
                    errors++;
                }
                granule_usage[g] = 1;
                
                uint8_t next = fat[g];
                if (next >= 0xC0) break;
                if (next >= 68) {
                    if (report) {
                        pos += snprintf(report + pos, report_size - pos,
                            "ERROR: Invalid FAT pointer at granule %d\n", g);
                    }
                    errors++;
                    break;
                }
                g = next;
            }
        }
    }
    
    /* Summary */
    if (report) {
        pos += snprintf(report + pos, report_size - pos,
            "\nSummary: %d errors found\n", errors);
        if (fix && errors > 0) {
            pos += snprintf(report + pos, report_size - pos,
                "(Auto-fix not yet implemented)\n");
        }
    }
    
    uft_trsdos_free_dir(&dir);
    return errors;
}

int uft_trsdos_check_crosslinks(const uft_trsdos_ctx_t *ctx) {
    char report[1024];
    return uft_trsdos_validate((uft_trsdos_ctx_t *)ctx, false, report, sizeof(report));
}

uft_trsdos_err_t uft_trsdos_rebuild_gat(uft_trsdos_ctx_t *ctx) {
    if (!ctx || !ctx->writable) {
        return UFT_TRSDOS_ERR_NULL;
    }
    
    /* Clear GAT */
    memset(ctx->gat.raw, 0, sizeof(ctx->gat.raw));
    ctx->gat.free_granules = ctx->gat.total_granules;
    
    /* Mark directory track */
    if (ctx->version == UFT_TRSDOS_VERSION_RSDOS) {
        ctx->gat.raw[34] = 0xFE;
        ctx->gat.raw[35] = 0xFE;
        ctx->gat.free_granules -= 2;
    }
    
    /* Read directory and mark used granules */
    uft_trsdos_dir_t dir;
    uft_trsdos_err_t err = uft_trsdos_read_dir(ctx, &dir);
    if (err != UFT_TRSDOS_OK) return err;
    
    for (size_t i = 0; i < dir.count; i++) {
        uft_trsdos_entry_t *e = &dir.entries[i];
        
        if (ctx->version == UFT_TRSDOS_VERSION_RSDOS) {
            uint8_t fat[256];
            uft_trsdos_read_sector(ctx, 17, 0, 1, fat, sizeof(fat));
            
            uint8_t g = e->extents[0].start_granule;
            while (g < 68) {
                if (ctx->gat.raw[g] == 0) {
                    ctx->gat.raw[g] = 0xFF;
                    ctx->gat.free_granules--;
                }
                
                uint8_t next = fat[g];
                if (next >= 0xC0 || next >= 68) break;
                g = next;
            }
        }
    }
    
    uft_trsdos_free_dir(&dir);
    
    return uft_trsdos_write_gat(ctx);
}
