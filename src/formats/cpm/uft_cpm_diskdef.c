/**
 * @file uft_cpm_diskdef.c
 * @brief CP/M Disk Definition implementation
 * @version 3.9.0
 * 
 * Comprehensive CP/M disk definition support compatible with cpmtools.
 * Reference: libdsk diskdefs, cpmtools by Michael Haardt
 */

#include "uft/formats/uft_cpm_diskdef.h"
#include "uft/uft_format_common.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "uft/uft_compat.h"

/* ============================================================================
 * Predefined CP/M Disk Definitions
 * ============================================================================ */

/* IBM 8" Single-Sided Single-Density (CP/M 1.4/2.2 standard) */
const cpm_diskdef_t cpm_diskdef_ibm_8ss = {
    .name = "ibm-8ss",
    .description = "IBM 8\" SS SD (250K)",
    .cylinders = 77,
    .heads = 1,
    .sectors = 26,
    .sector_size = 128,
    .first_sector = 1,
    .skew = 6,
    .has_skew_table = false,
    .boot_type = CPM_BOOT_CPM22,
    .system_tracks = 2,
    .dpb = {
        .spt = 26,      /* 26 sectors * 128 bytes = 3328 bytes/track */
        .bsh = 3,       /* 1K blocks */
        .blm = 7,
        .exm = 0,
        .dsm = 242,     /* 243 blocks = 243K data */
        .drm = 63,      /* 64 directory entries */
        .al0 = 0xC0,
        .al1 = 0x00,
        .cks = 16,
        .off = 2,
    },
    .encoding = UFT_ENC_FM,
    .uppercase_only = true,
    .extent_bytes = 8,
};

/* IBM 8" Double-Sided Single-Density */
const cpm_diskdef_t cpm_diskdef_ibm_8ds = {
    .name = "ibm-8ds",
    .description = "IBM 8\" DS SD (500K)",
    .cylinders = 77,
    .heads = 2,
    .sectors = 26,
    .sector_size = 128,
    .first_sector = 1,
    .skew = 6,
    .has_skew_table = false,
    .boot_type = CPM_BOOT_CPM22,
    .system_tracks = 2,
    .dpb = {
        .spt = 52,
        .bsh = 4,       /* 2K blocks */
        .blm = 15,
        .exm = 1,
        .dsm = 242,
        .drm = 127,
        .al0 = 0xC0,
        .al1 = 0x00,
        .cks = 32,
        .off = 2,
    },
    .encoding = UFT_ENC_FM,
    .uppercase_only = true,
    .extent_bytes = 8,
};

/* Kaypro II (single-sided) */
const cpm_diskdef_t cpm_diskdef_kaypro2 = {
    .name = "kaypro2",
    .description = "Kaypro II 5.25\" SS DD (191K)",
    .cylinders = 40,
    .heads = 1,
    .sectors = 10,
    .sector_size = 512,
    .first_sector = 0,
    .skew = 0,
    .has_skew_table = false,
    .boot_type = CPM_BOOT_CPM22,
    .system_tracks = 1,
    .dpb = {
        .spt = 40,
        .bsh = 3,
        .blm = 7,
        .exm = 0,
        .dsm = 194,
        .drm = 63,
        .al0 = 0xF0,
        .al1 = 0x00,
        .cks = 16,
        .off = 1,
    },
    .encoding = UFT_ENC_MFM,
    .uppercase_only = true,
    .extent_bytes = 8,
};

/* Kaypro 4/10 (double-sided) */
const cpm_diskdef_t cpm_diskdef_kaypro4 = {
    .name = "kaypro4",
    .description = "Kaypro 4 5.25\" DS DD (390K)",
    .cylinders = 40,
    .heads = 2,
    .sectors = 10,
    .sector_size = 512,
    .first_sector = 0,
    .skew = 0,
    .has_skew_table = false,
    .boot_type = CPM_BOOT_CPM22,
    .system_tracks = 1,
    .dpb = {
        .spt = 40,
        .bsh = 4,
        .blm = 15,
        .exm = 1,
        .dsm = 196,
        .drm = 63,
        .al0 = 0xC0,
        .al1 = 0x00,
        .cks = 16,
        .off = 1,
    },
    .encoding = UFT_ENC_MFM,
    .uppercase_only = true,
    .extent_bytes = 8,
};

/* Osborne 1 */
const cpm_diskdef_t cpm_diskdef_osborne1 = {
    .name = "osborne1",
    .description = "Osborne 1 5.25\" SS SD (92K)",
    .cylinders = 40,
    .heads = 1,
    .sectors = 10,
    .sector_size = 256,
    .first_sector = 1,
    .skew = 2,
    .has_skew_table = false,
    .boot_type = CPM_BOOT_CPM22,
    .system_tracks = 3,
    .dpb = {
        .spt = 20,
        .bsh = 3,
        .blm = 7,
        .exm = 0,
        .dsm = 45,
        .drm = 63,
        .al0 = 0x80,
        .al1 = 0x00,
        .cks = 16,
        .off = 3,
    },
    .encoding = UFT_ENC_FM,
    .uppercase_only = true,
    .extent_bytes = 8,
};

/* Morrow MD2 */
const cpm_diskdef_t cpm_diskdef_morrow_md2 = {
    .name = "morrow-md2",
    .description = "Morrow MD2 5.25\" SS DD (384K)",
    .cylinders = 40,
    .heads = 1,
    .sectors = 17,
    .sector_size = 512,
    .first_sector = 1,
    .skew = 0,
    .has_skew_table = false,
    .boot_type = CPM_BOOT_CPM22,
    .system_tracks = 2,
    .dpb = {
        .spt = 68,
        .bsh = 4,
        .blm = 15,
        .exm = 0,
        .dsm = 149,
        .drm = 127,
        .al0 = 0xC0,
        .al1 = 0x00,
        .cks = 32,
        .off = 2,
    },
    .encoding = UFT_ENC_MFM,
    .uppercase_only = true,
    .extent_bytes = 8,
};

/* Morrow MD3 */
const cpm_diskdef_t cpm_diskdef_morrow_md3 = {
    .name = "morrow-md3",
    .description = "Morrow MD3 5.25\" DS DD (768K)",
    .cylinders = 40,
    .heads = 2,
    .sectors = 17,
    .sector_size = 512,
    .first_sector = 1,
    .skew = 0,
    .has_skew_table = false,
    .boot_type = CPM_BOOT_CPM22,
    .system_tracks = 2,
    .dpb = {
        .spt = 68,
        .bsh = 4,
        .blm = 15,
        .exm = 0,
        .dsm = 314,
        .drm = 127,
        .al0 = 0xC0,
        .al1 = 0x00,
        .cks = 32,
        .off = 2,
    },
    .encoding = UFT_ENC_MFM,
    .uppercase_only = true,
    .extent_bytes = 8,
};

/* Epson QX-10 */
const cpm_diskdef_t cpm_diskdef_epson_qx10 = {
    .name = "epson-qx10",
    .description = "Epson QX-10 5.25\" DS DD",
    .cylinders = 40,
    .heads = 2,
    .sectors = 16,
    .sector_size = 256,
    .first_sector = 1,
    .skew = 0,
    .has_skew_table = false,
    .boot_type = CPM_BOOT_CPM22,
    .system_tracks = 2,
    .dpb = {
        .spt = 64,
        .bsh = 3,
        .blm = 7,
        .exm = 0,
        .dsm = 299,
        .drm = 127,
        .al0 = 0xF0,
        .al1 = 0x00,
        .cks = 32,
        .off = 2,
    },
    .encoding = UFT_ENC_MFM,
    .uppercase_only = true,
    .extent_bytes = 8,
};

/* Cromemco */
const cpm_diskdef_t cpm_diskdef_cromemco = {
    .name = "cromemco",
    .description = "Cromemco 5.25\" DS DD",
    .cylinders = 40,
    .heads = 2,
    .sectors = 18,
    .sector_size = 256,
    .first_sector = 1,
    .skew = 5,
    .has_skew_table = false,
    .boot_type = CPM_BOOT_CPM22,
    .system_tracks = 2,
    .dpb = {
        .spt = 72,
        .bsh = 4,
        .blm = 15,
        .exm = 0,
        .dsm = 176,
        .drm = 127,
        .al0 = 0xC0,
        .al1 = 0x00,
        .cks = 32,
        .off = 2,
    },
    .encoding = UFT_ENC_MFM,
    .uppercase_only = true,
    .extent_bytes = 8,
};

/* Amstrad PCW 3" CF2 */
const cpm_diskdef_t cpm_diskdef_amstrad_pcw = {
    .name = "amstrad-pcw",
    .description = "Amstrad PCW 3\" CF2 (173K)",
    .cylinders = 40,
    .heads = 1,
    .sectors = 9,
    .sector_size = 512,
    .first_sector = 1,
    .skew = 0,
    .has_skew_table = false,
    .boot_type = CPM_BOOT_CPM3,
    .system_tracks = 1,
    .dpb = {
        .spt = 36,
        .bsh = 3,
        .blm = 7,
        .exm = 0,
        .dsm = 174,
        .drm = 63,
        .al0 = 0xC0,
        .al1 = 0x00,
        .cks = 16,
        .off = 1,
    },
    .encoding = UFT_ENC_MFM,
    .uppercase_only = false,
    .extent_bytes = 16,
};

/* Amstrad CPC */
const cpm_diskdef_t cpm_diskdef_amstrad_cpc = {
    .name = "amstrad-cpc",
    .description = "Amstrad CPC 3\" (178K)",
    .cylinders = 40,
    .heads = 1,
    .sectors = 9,
    .sector_size = 512,
    .first_sector = 0xC1,  /* CPC uses 0xC1-0xC9 */
    .skew = 0,
    .has_skew_table = false,
    .boot_type = CPM_BOOT_NONE,
    .system_tracks = 2,
    .dpb = {
        .spt = 36,
        .bsh = 3,
        .blm = 7,
        .exm = 0,
        .dsm = 170,
        .drm = 63,
        .al0 = 0xC0,
        .al1 = 0x00,
        .cks = 16,
        .off = 2,
    },
    .encoding = UFT_ENC_MFM,
    .uppercase_only = true,
    .extent_bytes = 16,
};

/* Spectrum +3 */
const cpm_diskdef_t cpm_diskdef_spectrum_p3 = {
    .name = "spectrum-p3",
    .description = "Spectrum +3 3\" (173K)",
    .cylinders = 40,
    .heads = 1,
    .sectors = 9,
    .sector_size = 512,
    .first_sector = 1,
    .skew = 0,
    .has_skew_table = false,
    .boot_type = CPM_BOOT_CPM3,
    .system_tracks = 1,
    .dpb = {
        .spt = 36,
        .bsh = 3,
        .blm = 7,
        .exm = 0,
        .dsm = 174,
        .drm = 63,
        .al0 = 0xC0,
        .al1 = 0x00,
        .cks = 16,
        .off = 1,
    },
    .encoding = UFT_ENC_MFM,
    .uppercase_only = false,
    .extent_bytes = 16,
};

/* Amstrad PCW 720K */
const cpm_diskdef_t cpm_diskdef_pcw_720 = {
    .name = "pcw-720",
    .description = "Amstrad PCW 3.5\" 720K",
    .cylinders = 80,
    .heads = 2,
    .sectors = 9,
    .sector_size = 512,
    .first_sector = 1,
    .skew = 0,
    .has_skew_table = false,
    .boot_type = CPM_BOOT_CPM3,
    .system_tracks = 1,
    .dpb = {
        .spt = 36,
        .bsh = 4,
        .blm = 15,
        .exm = 0,
        .dsm = 357,
        .drm = 255,
        .al0 = 0xF0,
        .al1 = 0x00,
        .cks = 64,
        .off = 1,
    },
    .encoding = UFT_ENC_MFM,
    .uppercase_only = false,
    .extent_bytes = 16,
};

/* RC2014 CF format */
const cpm_diskdef_t cpm_diskdef_rc2014 = {
    .name = "rc2014",
    .description = "RC2014 CF Card (8MB)",
    .cylinders = 512,
    .heads = 2,
    .sectors = 32,
    .sector_size = 512,
    .first_sector = 0,
    .skew = 0,
    .has_skew_table = false,
    .boot_type = CPM_BOOT_NONE,
    .system_tracks = 6,
    .dpb = {
        .spt = 128,
        .bsh = 5,
        .blm = 31,
        .exm = 1,
        .dsm = 2039,
        .drm = 511,
        .al0 = 0xF0,
        .al1 = 0x00,
        .cks = 0,
        .off = 6,
    },
    .encoding = UFT_ENC_MFM,
    .uppercase_only = true,
    .extent_bytes = 16,
};

/* RCBus */
const cpm_diskdef_t cpm_diskdef_rcbus = {
    .name = "rcbus",
    .description = "RCBus CF (4MB)",
    .cylinders = 256,
    .heads = 2,
    .sectors = 32,
    .sector_size = 512,
    .first_sector = 0,
    .skew = 0,
    .has_skew_table = false,
    .boot_type = CPM_BOOT_NONE,
    .system_tracks = 2,
    .dpb = {
        .spt = 128,
        .bsh = 5,
        .blm = 31,
        .exm = 1,
        .dsm = 1019,
        .drm = 511,
        .al0 = 0xF0,
        .al1 = 0x00,
        .cks = 0,
        .off = 2,
    },
    .encoding = UFT_ENC_MFM,
    .uppercase_only = true,
    .extent_bytes = 16,
};

/* NEC PC-8001 */
const cpm_diskdef_t cpm_diskdef_nec_pc8001 = {
    .name = "nec-pc8001",
    .description = "NEC PC-8001 5.25\" (143K)",
    .cylinders = 35,
    .heads = 1,
    .sectors = 16,
    .sector_size = 256,
    .first_sector = 1,
    .skew = 0,
    .has_skew_table = false,
    .boot_type = CPM_BOOT_CPM22,
    .system_tracks = 3,
    .dpb = {
        .spt = 32,
        .bsh = 3,
        .blm = 7,
        .exm = 0,
        .dsm = 127,
        .drm = 63,
        .al0 = 0xC0,
        .al1 = 0x00,
        .cks = 16,
        .off = 3,
    },
    .encoding = UFT_ENC_FM,
    .uppercase_only = true,
    .extent_bytes = 8,
};

/* Sharp MZ-80 */
const cpm_diskdef_t cpm_diskdef_sharp_mz80 = {
    .name = "sharp-mz80",
    .description = "Sharp MZ-80 5.25\" (140K)",
    .cylinders = 35,
    .heads = 1,
    .sectors = 16,
    .sector_size = 256,
    .first_sector = 1,
    .skew = 0,
    .has_skew_table = false,
    .boot_type = CPM_BOOT_CPM22,
    .system_tracks = 2,
    .dpb = {
        .spt = 32,
        .bsh = 3,
        .blm = 7,
        .exm = 0,
        .dsm = 131,
        .drm = 63,
        .al0 = 0xC0,
        .al1 = 0x00,
        .cks = 16,
        .off = 2,
    },
    .encoding = UFT_ENC_FM,
    .uppercase_only = true,
    .extent_bytes = 8,
};

/* Array of all definitions */
const cpm_diskdef_t *cpm_diskdefs[] = {
    &cpm_diskdef_ibm_8ss,
    &cpm_diskdef_ibm_8ds,
    &cpm_diskdef_kaypro2,
    &cpm_diskdef_kaypro4,
    &cpm_diskdef_osborne1,
    &cpm_diskdef_morrow_md2,
    &cpm_diskdef_morrow_md3,
    &cpm_diskdef_epson_qx10,
    &cpm_diskdef_cromemco,
    &cpm_diskdef_amstrad_pcw,
    &cpm_diskdef_amstrad_cpc,
    &cpm_diskdef_spectrum_p3,
    &cpm_diskdef_pcw_720,
    &cpm_diskdef_rc2014,
    &cpm_diskdef_rcbus,
    &cpm_diskdef_nec_pc8001,
    &cpm_diskdef_sharp_mz80,
    NULL
};

const size_t cpm_diskdef_count = sizeof(cpm_diskdefs) / sizeof(cpm_diskdefs[0]) - 1;

/* ============================================================================
 * Disk Definition Functions
 * ============================================================================ */

const cpm_diskdef_t* uft_cpm_find_diskdef(const char *name) {
    if (!name) return NULL;
    
    for (size_t i = 0; i < cpm_diskdef_count; i++) {
        if (strcasecmp(cpm_diskdefs[i]->name, name) == 0) {
            return cpm_diskdefs[i];
        }
    }
    
    return NULL;
}

const cpm_diskdef_t* uft_cpm_find_diskdef_by_geometry(
    uint16_t cylinders, uint8_t heads, uint8_t sectors, uint16_t sector_size) {
    
    for (size_t i = 0; i < cpm_diskdef_count; i++) {
        const cpm_diskdef_t *def = cpm_diskdefs[i];
        if (def->cylinders == cylinders &&
            def->heads == heads &&
            def->sectors == sectors &&
            def->sector_size == sector_size) {
            return def;
        }
    }
    
    return NULL;
}

const cpm_diskdef_t* uft_cpm_detect_diskdef(const uint8_t *data, size_t size) {
    if (!data || size == 0) return NULL;
    
    /* Try each definition by size first */
    for (size_t i = 0; i < cpm_diskdef_count; i++) {
        const cpm_diskdef_t *def = cpm_diskdefs[i];
        size_t expected = (size_t)def->cylinders * def->heads * 
                          def->sectors * def->sector_size;
        
        if (size == expected) {
            /* Verify by checking for 0xE5 in directory area */
            size_t dir_offset = (size_t)def->system_tracks * def->heads *
                               def->sectors * def->sector_size;
            
            if (dir_offset < size) {
                /* Check first directory entry */
                if (data[dir_offset] == 0xE5 || data[dir_offset] <= 15) {
                    return def;
                }
            }
        }
    }
    
    return NULL;
}

size_t uft_cpm_list_diskdefs(const cpm_diskdef_t **defs, size_t max) {
    if (!defs || max == 0) return 0;
    
    size_t count = (cpm_diskdef_count < max) ? cpm_diskdef_count : max;
    
    for (size_t i = 0; i < count; i++) {
        defs[i] = cpm_diskdefs[i];
    }
    
    return count;
}

/* ============================================================================
 * CP/M Directory Operations
 * ============================================================================ */

/* Calculate logical sector number */
static size_t cpm_logical_sector(const cpm_diskdef_t *def,
                                  uint16_t track, uint16_t sector) {
    return (size_t)track * def->dpb.spt + sector;
}

/* Read sector from disk image */
static uft_error_t cpm_read_sector(const uft_disk_image_t *disk,
                                    const cpm_diskdef_t *def,
                                    uint16_t log_sector,
                                    uint8_t *buffer) {
    /* Convert logical sector to physical */
    uint16_t phys_track = log_sector / (def->sectors * (def->heads > 1 ? 2 : 1));
    uint16_t rem = log_sector % (def->sectors * (def->heads > 1 ? 2 : 1));
    uint8_t head = (def->heads > 1) ? (rem / def->sectors) : 0;
    uint8_t sector = rem % def->sectors;
    
    /* Apply skew if present */
    if (def->has_skew_table && sector < CPM_MAX_SKEW_TABLE) {
        sector = def->skew_table[sector];
    } else if (def->skew > 0) {
        sector = (sector * def->skew) % def->sectors;
    }
    
    /* Get track from disk */
    if (phys_track >= disk->tracks || head >= disk->heads) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    size_t idx = phys_track * disk->heads + head;
    uft_track_t *track = disk->track_data[idx];
    
    if (!track || sector >= track->sector_count) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Find sector by ID */
    for (uint8_t s = 0; s < track->sector_count; s++) {
        if (track->sectors[s].id.sector == sector + def->first_sector) {
            if (track->sectors[s].data) {
                memcpy(buffer, track->sectors[s].data, def->sector_size);
                return UFT_OK;
            }
        }
    }
    
    /* Fallback: use index */
    if (track->sectors[sector].data) {
        memcpy(buffer, track->sectors[sector].data, def->sector_size);
        return UFT_OK;
    }
    
    return UFT_ERR_NOT_FOUND;
}

/* Parse filename from directory entry */
static void cpm_parse_filename(const cpm_dirent_t *entry, char *filename) {
    int pos = 0;
    
    /* Copy name, trimming spaces */
    for (int i = 0; i < 8 && entry->name[i] != ' '; i++) {
        filename[pos++] = entry->name[i] & 0x7F;  /* Strip high bit (attributes) */
    }
    
    filename[pos++] = '.';
    
    /* Copy extension */
    for (int i = 0; i < 3 && entry->ext[i] != ' '; i++) {
        filename[pos++] = entry->ext[i] & 0x7F;
    }
    
    /* Remove trailing dot if no extension */
    if (filename[pos - 1] == '.') {
        pos--;
    }
    
    filename[pos] = '\0';
}

uft_error_t uft_cpm_read_directory(const uft_disk_image_t *disk,
                                   const cpm_diskdef_t *def,
                                   cpm_file_t *files,
                                   size_t max_files,
                                   size_t *file_count) {
    if (!disk || !def || !files || max_files == 0) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Calculate directory location */
    uint16_t dir_track = def->system_tracks;
    size_t entries_per_sector = def->sector_size / 32;
    size_t dir_entries = def->dpb.drm + 1;
    size_t dir_sectors = (dir_entries + entries_per_sector - 1) / entries_per_sector;
    
    /* Allocate sector buffer */
    uint8_t *sector_buf = malloc(def->sector_size);
    if (!sector_buf) {
        return UFT_ERR_MEMORY;
    }
    
    /* Clear file list */
    memset(files, 0, max_files * sizeof(cpm_file_t));
    size_t file_idx = 0;
    
    /* Read directory */
    size_t log_sector = cpm_logical_sector(def, dir_track, 0);
    
    for (size_t ds = 0; ds < dir_sectors && file_idx < max_files; ds++) {
        if (cpm_read_sector(disk, def, log_sector + ds, sector_buf) != UFT_OK) {
            continue;
        }
        
        for (size_t e = 0; e < entries_per_sector && file_idx < max_files; e++) {
            cpm_dirent_t *entry = (cpm_dirent_t *)(sector_buf + e * 32);
            
            /* Skip deleted entries */
            if (entry->user == 0xE5) continue;
            
            /* Skip invalid user numbers */
            if (entry->user > 15) continue;
            
            /* Parse filename */
            char filename[13];
            cpm_parse_filename(entry, filename);
            
            /* Check if file already in list (additional extent) */
            bool found = false;
            for (size_t f = 0; f < file_idx; f++) {
                if (files[f].user == entry->user &&
                    strcmp(files[f].filename, filename) == 0) {
                    /* Update existing entry */
                    files[f].extents++;
                    files[f].size += entry->record_count * 128;
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                /* New file */
                files[file_idx].user = entry->user;
                strncpy(files[file_idx].filename, filename, sizeof(files[file_idx].filename) - 1);
                files[file_idx].filename[sizeof(files[file_idx].filename) - 1] = '\0';
                files[file_idx].read_only = (entry->ext[0] & 0x80) != 0;
                files[file_idx].system = (entry->ext[1] & 0x80) != 0;
                files[file_idx].archived = (entry->ext[2] & 0x80) != 0;
                files[file_idx].size = entry->record_count * 128;
                files[file_idx].extents = 1;
                
                /* First block */
                if (def->dpb.dsm > 255) {
                    files[file_idx].first_block = entry->alloc[0] | (entry->alloc[1] << 8);
                } else {
                    files[file_idx].first_block = entry->alloc[0];
                }
                
                file_idx++;
            }
        }
    }
    
    free(sector_buf);
    
    if (file_count) {
        *file_count = file_idx;
    }
    
    return UFT_OK;
}

/* ============================================================================
 * Format Plugin Registration
 * ============================================================================ */

static bool cpm_probe_plugin(const uint8_t *data, size_t size,
                             size_t file_size, int *confidence) {
    (void)file_size;
    
    const cpm_diskdef_t *def = uft_cpm_detect_diskdef(data, size);
    if (def) {
        if (confidence) *confidence = 60;  /* Medium confidence */
        return true;
    }
    
    return false;
}

static uft_error_t cpm_open(uft_disk_t *disk, const char *path, bool read_only) {
    (void)read_only;
    
    /* Read file */
    FILE *fp = fopen(path, "rb");
    if (!fp) return UFT_ERR_IO;
    
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    uint8_t *data = malloc(size);
    if (!data) {
        fclose(fp);
        return UFT_ERR_MEMORY;
    }
    
    if (fread(data, 1, size, fp) != size) {
        free(data);
        fclose(fp);
        return UFT_ERR_IO;
    }
    fclose(fp);
    
    /* Detect format */
    const cpm_diskdef_t *def = uft_cpm_detect_diskdef(data, size);
    if (!def) {
        free(data);
        return UFT_ERR_FORMAT;
    }
    
    /* Create disk image */
    uft_disk_image_t *image = uft_disk_alloc(def->cylinders, def->heads);
    if (!image) {
        free(data);
        return UFT_ERR_MEMORY;
    }
    
    image->format = UFT_FMT_RAW;
    snprintf(image->format_name, sizeof(image->format_name), "CP/M (%s)", def->name);
    image->sectors_per_track = def->sectors;
    image->bytes_per_sector = def->sector_size;
    
    /* Read sectors */
    size_t data_pos = 0;
    uint8_t size_code = 2;  /* Assume 512 bytes, adjust if needed */
    if (def->sector_size == 128) size_code = 0;
    else if (def->sector_size == 256) size_code = 1;
    else if (def->sector_size == 1024) size_code = 3;
    
    for (uint16_t c = 0; c < def->cylinders; c++) {
        for (uint8_t h = 0; h < def->heads; h++) {
            size_t idx = c * def->heads + h;
            
            uft_track_t *track = uft_track_alloc(def->sectors, 0);
            if (!track) {
                uft_disk_free(image);
                free(data);
                return UFT_ERR_MEMORY;
            }
            
            track->cylinder = c;
            track->head = h;
            track->encoding = def->encoding;
            
            for (uint8_t s = 0; s < def->sectors; s++) {
                uft_sector_t *sect = &track->sectors[s];
                sect->id.cylinder = c;
                sect->id.head = h;
                sect->id.sector = s + def->first_sector;
                sect->id.size_code = size_code;
                sect->status = UFT_SECTOR_OK;
                
                sect->data = malloc(def->sector_size);
                sect->data_size = def->sector_size;
                
                if (sect->data && data_pos + def->sector_size <= size) {
                    memcpy(sect->data, data + data_pos, def->sector_size);
                }
                data_pos += def->sector_size;
                track->sector_count++;
            }
            
            image->track_data[idx] = track;
        }
    }
    
    free(data);
    
    disk->plugin_data = image;
    disk->geometry.cylinders = image->tracks;
    disk->geometry.heads = image->heads;
    disk->geometry.sectors = image->sectors_per_track;
    disk->geometry.sector_size = image->bytes_per_sector;
    
    return UFT_OK;
}

static void cpm_close(uft_disk_t *disk) {
    if (disk && disk->plugin_data) {
        uft_disk_free((uft_disk_image_t*)disk->plugin_data);
        disk->plugin_data = NULL;
    }
}

static uft_error_t cpm_read_track(uft_disk_t *disk, int cyl, int head,
                                   uft_track_t *track) {
    uft_disk_image_t *image = (uft_disk_image_t*)disk->plugin_data;
    if (!image || !track) return UFT_ERR_INVALID_PARAM;
    
    size_t idx = cyl * image->heads + head;
    if (idx >= (size_t)(image->tracks * image->heads)) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    uft_track_t *src = image->track_data[idx];
    if (!src) return UFT_ERR_INVALID_PARAM;
    
    track->cylinder = cyl;
    track->head = head;
    track->sector_count = src->sector_count;
    track->encoding = src->encoding;
    
    for (uint8_t s = 0; s < src->sector_count; s++) {
        track->sectors[s] = src->sectors[s];
        if (src->sectors[s].data) {
            track->sectors[s].data = malloc(src->sectors[s].data_size);
            if (track->sectors[s].data) {
                memcpy(track->sectors[s].data, src->sectors[s].data,
                       src->sectors[s].data_size);
            }
        }
    }
    
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_cpm = {
    .name = "CP/M",
    .description = "CP/M Disk Image",
    .extensions = "cpm,dsk",
    .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe = cpm_probe_plugin,
    .open = cpm_open,
    .close = cpm_close,
    .read_track = cpm_read_track,
};

UFT_REGISTER_FORMAT_PLUGIN(cpm)
