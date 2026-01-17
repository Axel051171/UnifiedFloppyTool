/**
 * @file uft_fat_bootsector.c
 * @brief FAT Boot Sector Analysis Implementation
 * @version 4.1.6
 * 
 * Based on OpenGate.at article "Bootsektor von Disketten" and MS-DOS specs.
 * https://www.opengate.at/blog/2024/01/bootsector/
 */

#include "uft/formats/fat/uft_fat_bootsector.h"
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Standard Disk Geometry Definitions (from OpenGate article)
 * ============================================================================ */

const fat_disk_geometry_t fat_geometry_160k = {
    .name = "5.25\" 160KB SS/DD",
    .media_byte = 0xFE,
    .total_sectors = 320,       /* 40 tracks × 1 head × 8 sectors */
    .bytes_per_sector = 512,
    .sectors_per_cluster = 1,
    .reserved_sectors = 1,
    .fat_count = 2,
    .root_entries = 64,
    .sectors_per_fat = 1,
    .sectors_per_track = 8,
    .heads = 1,
    .tracks = 40
};

const fat_disk_geometry_t fat_geometry_180k = {
    .name = "5.25\" 180KB SS/DD",
    .media_byte = 0xFC,
    .total_sectors = 360,       /* 40 tracks × 1 head × 9 sectors */
    .bytes_per_sector = 512,
    .sectors_per_cluster = 1,
    .reserved_sectors = 1,
    .fat_count = 2,
    .root_entries = 64,
    .sectors_per_fat = 2,
    .sectors_per_track = 9,
    .heads = 1,
    .tracks = 40
};

const fat_disk_geometry_t fat_geometry_320k = {
    .name = "5.25\" 320KB DS/DD",
    .media_byte = 0xFF,
    .total_sectors = 640,       /* 40 tracks × 2 heads × 8 sectors */
    .bytes_per_sector = 512,
    .sectors_per_cluster = 2,
    .reserved_sectors = 1,
    .fat_count = 2,
    .root_entries = 112,
    .sectors_per_fat = 1,
    .sectors_per_track = 8,
    .heads = 2,
    .tracks = 40
};

const fat_disk_geometry_t fat_geometry_360k = {
    .name = "5.25\" 360KB DS/DD",
    .media_byte = 0xFD,
    .total_sectors = 720,       /* 40 tracks × 2 heads × 9 sectors */
    .bytes_per_sector = 512,
    .sectors_per_cluster = 2,
    .reserved_sectors = 1,
    .fat_count = 2,
    .root_entries = 112,
    .sectors_per_fat = 2,
    .sectors_per_track = 9,
    .heads = 2,
    .tracks = 40
};

const fat_disk_geometry_t fat_geometry_720k = {
    .name = "3.5\" 720KB DS/DD",
    .media_byte = 0xF9,
    .total_sectors = 1440,      /* 80 tracks × 2 heads × 9 sectors */
    .bytes_per_sector = 512,
    .sectors_per_cluster = 2,
    .reserved_sectors = 1,
    .fat_count = 2,
    .root_entries = 112,
    .sectors_per_fat = 3,
    .sectors_per_track = 9,
    .heads = 2,
    .tracks = 80
};

const fat_disk_geometry_t fat_geometry_1200k = {
    .name = "5.25\" 1.2MB DS/HD",
    .media_byte = 0xF9,
    .total_sectors = 2400,      /* 80 tracks × 2 heads × 15 sectors */
    .bytes_per_sector = 512,
    .sectors_per_cluster = 1,
    .reserved_sectors = 1,
    .fat_count = 2,
    .root_entries = 224,
    .sectors_per_fat = 7,
    .sectors_per_track = 15,
    .heads = 2,
    .tracks = 80
};

const fat_disk_geometry_t fat_geometry_1440k = {
    .name = "3.5\" 1.44MB DS/HD",
    .media_byte = 0xF0,
    .total_sectors = 2880,      /* 80 tracks × 2 heads × 18 sectors */
    .bytes_per_sector = 512,
    .sectors_per_cluster = 1,
    .reserved_sectors = 1,
    .fat_count = 2,
    .root_entries = 224,
    .sectors_per_fat = 9,
    .sectors_per_track = 18,
    .heads = 2,
    .tracks = 80
};

const fat_disk_geometry_t fat_geometry_2880k = {
    .name = "3.5\" 2.88MB DS/ED",
    .media_byte = 0xF0,
    .total_sectors = 5760,      /* 80 tracks × 2 heads × 36 sectors */
    .bytes_per_sector = 512,
    .sectors_per_cluster = 2,
    .reserved_sectors = 1,
    .fat_count = 2,
    .root_entries = 240,
    .sectors_per_fat = 9,
    .sectors_per_track = 36,
    .heads = 2,
    .tracks = 80
};

/* Array of all standard geometries for lookup */
static const fat_disk_geometry_t *g_standard_geometries[] = {
    &fat_geometry_160k,
    &fat_geometry_180k,
    &fat_geometry_320k,
    &fat_geometry_360k,
    &fat_geometry_720k,
    &fat_geometry_1200k,
    &fat_geometry_1440k,
    &fat_geometry_2880k,
    NULL
};

/* ============================================================================
 * Media Descriptor Descriptions (from OpenGate article)
 * ============================================================================ */

typedef struct {
    uint8_t media_byte;
    const char *description;
} media_desc_entry_t;

static const media_desc_entry_t g_media_descriptions[] = {
    /* From OpenGate article */
    {0xF0, "3.5\" 1.44MB HD or 2.88MB ED"},
    {0xF8, "Hard disk / Fixed disk"},
    {0xF9, "3.5\" 720KB DD or 5.25\" 1.2MB HD"},
    {0xFA, "RAM disk"},
    {0xFB, "3.5\" 640KB (Japanese PC-98)"},
    {0xFC, "5.25\" 180KB SS/DD (9 sectors)"},
    {0xFD, "5.25\" 360KB DS/DD or 8\" 500KB"},
    {0xFE, "5.25\" 160KB SS/DD (8 sectors) or 8\" 250KB/1.2MB"},
    {0xFF, "5.25\" 320KB DS/DD (8 sectors)"},
    {0x00, NULL}
};

/* ============================================================================
 * API Implementation
 * ============================================================================ */

bool fat_check_boot_signature(const uint8_t *data, size_t size) {
    if (!data || size < FAT_SECTOR_SIZE) {
        return false;
    }
    
    /* Check for 0x55 0xAA at offset 510 (little-endian 0xAA55) */
    return (data[510] == 0x55 && data[511] == 0xAA);
}

bool fat_check_jump_instruction(const uint8_t *data) {
    if (!data) return false;
    
    /* Valid jump instructions:
     * EB xx 90 - JMP SHORT xx, NOP
     * E9 xx xx - JMP NEAR xxxx
     */
    if (data[0] == 0xEB && data[2] == 0x90) {
        return true;
    }
    if (data[0] == 0xE9) {
        return true;
    }
    
    /* Some very old disks might not have a proper jump */
    return false;
}

const char *fat_media_description(uint8_t media_byte) {
    for (int i = 0; g_media_descriptions[i].description != NULL; i++) {
        if (g_media_descriptions[i].media_byte == media_byte) {
            return g_media_descriptions[i].description;
        }
    }
    return "Unknown media type";
}

fat_type_t fat_determine_type(uint32_t cluster_count) {
    /* MS FAT specification thresholds */
    if (cluster_count < 4085) {
        return FAT_TYPE_FAT12;
    } else if (cluster_count < 65525) {
        return FAT_TYPE_FAT16;
    } else {
        return FAT_TYPE_FAT32;
    }
}

const char *fat_type_string(fat_type_t type) {
    switch (type) {
        case FAT_TYPE_FAT12: return "FAT12";
        case FAT_TYPE_FAT16: return "FAT16";
        case FAT_TYPE_FAT32: return "FAT32";
        case FAT_TYPE_EXFAT: return "exFAT";
        default: return "Unknown";
    }
}

const fat_disk_geometry_t *fat_find_geometry(uint32_t total_sectors, uint8_t media_byte) {
    for (int i = 0; g_standard_geometries[i] != NULL; i++) {
        const fat_disk_geometry_t *geom = g_standard_geometries[i];
        if (geom->total_sectors == total_sectors && geom->media_byte == media_byte) {
            return geom;
        }
    }
    
    /* Try matching by total sectors only */
    for (int i = 0; g_standard_geometries[i] != NULL; i++) {
        const fat_disk_geometry_t *geom = g_standard_geometries[i];
        if (geom->total_sectors == total_sectors) {
            return geom;
        }
    }
    
    return NULL;
}

uint64_t fat_calculate_disk_size(const fat_bpb_t *bpb) {
    if (!bpb) return 0;
    
    uint32_t total_sectors = bpb->total_sectors_16;
    if (total_sectors == 0) {
        total_sectors = bpb->total_sectors_32;
    }
    
    return (uint64_t)total_sectors * bpb->bytes_per_sector;
}

void fat_format_serial(uint32_t serial, char *buffer) {
    if (!buffer) return;
    snprintf(buffer, 10, "%04X-%04X", 
             (serial >> 16) & 0xFFFF, 
             serial & 0xFFFF);
}

int fat_analyze_boot_sector(const uint8_t *data, size_t size, 
                             fat_analysis_result_t *result) {
    if (!data || !result) {
        return FAT_ERR_NULL_POINTER;
    }
    if (size < FAT_SECTOR_SIZE) {
        return FAT_ERR_BUFFER_TOO_SMALL;
    }
    
    memset(result, 0, sizeof(*result));
    
    const fat_boot_sector_t *boot = (const fat_boot_sector_t *)data;
    const fat_bpb_t *bpb = &boot->bpb;
    
    /* Check boot signature */
    result->has_boot_signature = fat_check_boot_signature(data, size);
    
    /* Check jump instruction */
    result->has_jump_instruction = fat_check_jump_instruction(data);
    
    /* Extract OEM name (null-terminate) */
    memcpy(result->oem_name, bpb->oem_name, 8);
    result->oem_name[8] = '\0';
    
    /* Extract BPB values */
    result->bytes_per_sector = bpb->bytes_per_sector;
    result->sectors_per_cluster = bpb->sectors_per_cluster;
    result->reserved_sectors = bpb->reserved_sectors;
    result->fat_count = bpb->fat_count;
    result->root_entry_count = bpb->root_entry_count;
    result->media_type = bpb->media_type;
    result->sectors_per_fat = bpb->sectors_per_fat_16;
    result->sectors_per_track = bpb->sectors_per_track;
    result->head_count = bpb->head_count;
    result->hidden_sectors = bpb->hidden_sectors;
    
    /* Calculate total sectors */
    result->total_sectors = bpb->total_sectors_16;
    if (result->total_sectors == 0) {
        result->total_sectors = bpb->total_sectors_32;
    }
    
    /* Validate BPB */
    result->has_valid_bpb = true;
    
    if (result->bytes_per_sector == 0 || 
        (result->bytes_per_sector & (result->bytes_per_sector - 1)) != 0 ||
        result->bytes_per_sector > 4096) {
        result->has_valid_bpb = false;
    }
    
    if (result->sectors_per_cluster == 0 ||
        (result->sectors_per_cluster & (result->sectors_per_cluster - 1)) != 0) {
        result->has_valid_bpb = false;
    }
    
    if (result->fat_count == 0 || result->fat_count > 4) {
        result->has_valid_bpb = false;
    }
    
    if (result->media_type < 0xF0) {
        result->has_valid_bpb = false;
    }
    
    /* Check for extended BPB */
    const fat_ebpb_t *ebpb = &boot->ebpb.fat16;
    if (ebpb->boot_signature == FAT_EXTENDED_BPB_MARKER ||
        ebpb->boot_signature == FAT_EXTENDED_BPB_MARKER_OLD) {
        result->has_extended_bpb = true;
        result->volume_serial = ebpb->volume_serial;
        
        /* Extract volume label */
        memcpy(result->volume_label, ebpb->volume_label, 11);
        result->volume_label[11] = '\0';
        
        /* Extract FS type string */
        memcpy(result->fs_type_string, ebpb->fs_type, 8);
        result->fs_type_string[8] = '\0';
    }
    
    /* Calculate derived values */
    if (result->has_valid_bpb && result->bytes_per_sector > 0) {
        /* Root directory sectors */
        result->root_dir_sectors = ((result->root_entry_count * 32) + 
                                    (result->bytes_per_sector - 1)) / 
                                    result->bytes_per_sector;
        
        /* First data sector */
        result->first_data_sector = result->reserved_sectors +
                                    (result->fat_count * result->sectors_per_fat) +
                                    result->root_dir_sectors;
        
        /* Data sectors */
        if (result->total_sectors > result->first_data_sector) {
            result->data_sectors = result->total_sectors - result->first_data_sector;
        }
        
        /* Cluster count */
        if (result->sectors_per_cluster > 0) {
            result->cluster_count = result->data_sectors / result->sectors_per_cluster;
        }
        
        /* Total bytes */
        result->total_bytes = (uint64_t)result->total_sectors * result->bytes_per_sector;
        
        /* Determine FAT type */
        result->fat_type = fat_determine_type(result->cluster_count);
    }
    
    /* Get media description */
    result->media_description = fat_media_description(result->media_type);
    
    /* Find matching standard geometry */
    result->geometry = fat_find_geometry(result->total_sectors, result->media_type);
    
    /* Set overall validity */
    result->valid = result->has_boot_signature && 
                    result->has_valid_bpb &&
                    (result->has_jump_instruction || result->total_sectors < 10000);
    
    return FAT_OK;
}

int fat_create_boot_sector(const fat_disk_geometry_t *geometry,
                            const char *oem_name,
                            const char *volume_label,
                            uint8_t *buffer) {
    if (!geometry || !buffer) {
        return FAT_ERR_NULL_POINTER;
    }
    
    memset(buffer, 0, FAT_SECTOR_SIZE);
    
    fat_boot_sector_t *boot = (fat_boot_sector_t *)buffer;
    
    /* Jump instruction: JMP SHORT boot_code, NOP */
    boot->bpb.jmp_boot[0] = 0xEB;
    boot->bpb.jmp_boot[1] = 0x3C;  /* Jump to offset 0x3E */
    boot->bpb.jmp_boot[2] = 0x90;  /* NOP */
    
    /* OEM name */
    const char *oem = oem_name ? oem_name : "MSDOS5.0";
    memset(boot->bpb.oem_name, ' ', 8);
    size_t oem_len = strlen(oem);
    if (oem_len > 8) oem_len = 8;
    memcpy(boot->bpb.oem_name, oem, oem_len);
    
    /* BPB */
    boot->bpb.bytes_per_sector = geometry->bytes_per_sector;
    boot->bpb.sectors_per_cluster = geometry->sectors_per_cluster;
    boot->bpb.reserved_sectors = geometry->reserved_sectors;
    boot->bpb.fat_count = geometry->fat_count;
    boot->bpb.root_entry_count = geometry->root_entries;
    boot->bpb.total_sectors_16 = geometry->total_sectors;
    boot->bpb.media_type = geometry->media_byte;
    boot->bpb.sectors_per_fat_16 = geometry->sectors_per_fat;
    boot->bpb.sectors_per_track = geometry->sectors_per_track;
    boot->bpb.head_count = geometry->heads;
    boot->bpb.hidden_sectors = 0;
    boot->bpb.total_sectors_32 = 0;
    
    /* Extended BPB */
    fat_ebpb_t *ebpb = &boot->ebpb.fat16;
    ebpb->drive_number = 0x00;  /* Floppy */
    ebpb->boot_signature = FAT_EXTENDED_BPB_MARKER;
    
    /* Generate volume serial (simple pseudo-random) */
    ebpb->volume_serial = 0x12345678;  /* Should be randomized in real use */
    
    /* Volume label */
    const char *label = volume_label ? volume_label : "NO NAME    ";
    memset(ebpb->volume_label, ' ', 11);
    size_t label_len = strlen(label);
    if (label_len > 11) label_len = 11;
    memcpy(ebpb->volume_label, label, label_len);
    
    /* FS type */
    memcpy(ebpb->fs_type, "FAT12   ", 8);
    
    /* Boot signature */
    buffer[510] = 0x55;
    buffer[511] = 0xAA;
    
    return FAT_OK;
}

int fat_generate_report(const fat_analysis_result_t *result, 
                         char *buffer, size_t size) {
    if (!result || !buffer || size == 0) {
        return 0;
    }
    
    int written = 0;
    
    written += snprintf(buffer + written, size - written,
        "╔══════════════════════════════════════════════════════════════════╗\n"
        "║              FAT BOOT SECTOR ANALYSIS REPORT                    ║\n"
        "╚══════════════════════════════════════════════════════════════════╝\n\n");
    
    /* Validation Status */
    written += snprintf(buffer + written, size - written,
        "Validation Status:\n"
        "  Boot Signature (0x55AA): %s\n"
        "  Jump Instruction:        %s\n"
        "  BPB Valid:               %s\n"
        "  Extended BPB:            %s\n"
        "  Overall:                 %s\n\n",
        result->has_boot_signature ? "✓ Present" : "✗ Missing",
        result->has_jump_instruction ? "✓ Valid" : "✗ Invalid",
        result->has_valid_bpb ? "✓ Valid" : "✗ Invalid",
        result->has_extended_bpb ? "✓ Present" : "- Not present",
        result->valid ? "✓ VALID" : "✗ INVALID");
    
    /* Identification */
    written += snprintf(buffer + written, size - written,
        "Identification:\n"
        "  OEM Name:        \"%s\"\n"
        "  FAT Type:        %s\n"
        "  Media Type:      0x%02X (%s)\n",
        result->oem_name,
        fat_type_string(result->fat_type),
        result->media_type,
        result->media_description);
    
    if (result->has_extended_bpb) {
        char serial_str[10];
        fat_format_serial(result->volume_serial, serial_str);
        written += snprintf(buffer + written, size - written,
            "  Volume Label:    \"%s\"\n"
            "  Volume Serial:   %s\n"
            "  FS Type String:  \"%s\"\n",
            result->volume_label,
            serial_str,
            result->fs_type_string);
    }
    
    /* Geometry */
    written += snprintf(buffer + written, size - written,
        "\nDisk Geometry:\n"
        "  Bytes/Sector:    %u\n"
        "  Sectors/Cluster: %u\n"
        "  Reserved Sectors:%u\n"
        "  FAT Count:       %u\n"
        "  Root Entries:    %u\n"
        "  Total Sectors:   %u\n"
        "  Sectors/FAT:     %u\n"
        "  Sectors/Track:   %u\n"
        "  Heads:           %u\n"
        "  Hidden Sectors:  %u\n",
        result->bytes_per_sector,
        result->sectors_per_cluster,
        result->reserved_sectors,
        result->fat_count,
        result->root_entry_count,
        result->total_sectors,
        result->sectors_per_fat,
        result->sectors_per_track,
        result->head_count,
        result->hidden_sectors);
    
    /* Calculated Values */
    written += snprintf(buffer + written, size - written,
        "\nCalculated Values:\n"
        "  Root Dir Sectors:%u\n"
        "  First Data Sector:%u\n"
        "  Data Sectors:    %u\n"
        "  Cluster Count:   %u\n"
        "  Total Size:      %llu bytes (%.2f KB)\n",
        result->root_dir_sectors,
        result->first_data_sector,
        result->data_sectors,
        result->cluster_count,
        (unsigned long long)result->total_bytes,
        result->total_bytes / 1024.0);
    
    /* Standard Geometry Match */
    if (result->geometry) {
        written += snprintf(buffer + written, size - written,
            "\nStandard Format Match:\n"
            "  %s\n",
            result->geometry->name);
    }
    
    return written;
}
