/**
 * @file uft_amiga_bootblock.c
 * @brief Amiga Bootblock Analysis, Virus Detection and Recovery
 * 
 * Based on XVS Library, XCopy and DiskSalv algorithms.
 */

#include "uft/uft_amiga_bootblock.h"
#include <string.h>
#include <stdio.h>

/*============================================================================
 * HELPER FUNCTIONS
 *============================================================================*/

static inline uint32_t read_be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

static inline void write_be32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

/*============================================================================
 * STANDARD BOOTBLOCKS
 *============================================================================*/

/* Standard Kickstart 1.3 bootblock */
const uint8_t UFT_AMIGA_BOOTBLOCK_13[UFT_AMIGA_BOOTBLOCK_SIZE] = {
    'D', 'O', 'S', 0x00,                /* DOS\0 = OFS */
    0x00, 0x00, 0x00, 0x00,             /* Checksum (filled later) */
    0x00, 0x00, 0x03, 0x70,             /* Root block = 880 */
    /* Boot code - opens dos.library and calls DOS's boot routine */
    0x43, 0xFA, 0x00, 0x18,             /* LEA dosname(PC),A1 */
    0x4E, 0xAE, 0xFE, 0x68,             /* JSR _LVOFindResident(A6) */
    0x4A, 0x80,                         /* TST.L D0 */
    0x67, 0x0A,                         /* BEQ.S error */
    0x20, 0x40,                         /* MOVE.L D0,A0 */
    0x20, 0x68, 0x00, 0x16,             /* MOVE.L RT_INIT(A0),A0 */
    0x70, 0x00,                         /* MOVEQ #0,D0 */
    0x4E, 0x75,                         /* RTS */
    0x70, 0xFF,                         /* MOVEQ #-1,D0 (error) */
    0x4E, 0x75,                         /* RTS */
    'd', 'o', 's', '.', 'l', 'i', 'b', 'r', 'a', 'r', 'y', 0x00,
    /* Rest is zeros */
};

/* Standard Kickstart 2.0+ bootblock */
const uint8_t UFT_AMIGA_BOOTBLOCK_20[UFT_AMIGA_BOOTBLOCK_SIZE] = {
    'D', 'O', 'S', 0x00,                /* DOS\0 = OFS */
    0x00, 0x00, 0x00, 0x00,             /* Checksum (filled later) */
    0x00, 0x00, 0x03, 0x70,             /* Root block = 880 */
    /* Boot code - 2.0 style */
    0x43, 0xFA, 0x00, 0x3E,             /* LEA dosname(PC),A1 */
    0x70, 0x25,                         /* MOVEQ #37,D0 (version 37+) */
    0x4E, 0xAE, 0xFD, 0xD8,             /* JSR _LVOOpenLibrary(A6) */
    0x4A, 0x80,                         /* TST.L D0 */
    0x67, 0x0C,                         /* BEQ.S error */
    0x22, 0x40,                         /* MOVE.L D0,A1 */
    0x08, 0xE9, 0x00, 0x06, 0x00, 0x22, /* BSET #6,34(A1) */
    0x4E, 0xAE, 0xFE, 0x62,             /* JSR _LVOCloseLibrary(A6) */
    0x43, 0xFA, 0x00, 0x18,             /* LEA dosname2(PC),A1 */
    0x4E, 0xAE, 0xFE, 0x68,             /* JSR _LVOFindResident(A6) */
    0x4A, 0x80,                         /* TST.L D0 */
    0x67, 0x0A,                         /* BEQ.S error */
    0x20, 0x40,                         /* MOVE.L D0,A0 */
    0x20, 0x68, 0x00, 0x16,             /* MOVE.L RT_INIT(A0),A0 */
    0x70, 0x00,                         /* MOVEQ #0,D0 */
    0x4E, 0x75,                         /* RTS */
    0x70, 0xFF,                         /* MOVEQ #-1,D0 (error) */
    0x4E, 0x75,                         /* RTS */
    'd', 'o', 's', '.', 'l', 'i', 'b', 'r', 'a', 'r', 'y', 0x00,
    /* Rest is zeros */
};

/*============================================================================
 * VIRUS SIGNATURE DATABASE
 *============================================================================*/

/* Common Amiga bootblock viruses */
static const uint8_t SIG_SCA[] = { 
    0x00, 0x00, 0x03, 0xF3, 'S', 'C', 'A' 
};
static const uint8_t SIG_BYTE_BANDIT[] = { 
    'T', 'h', 'e', ' ', 'B', 'y', 't', 'e', ' ', 'B', 'a', 'n', 'd', 'i', 't' 
};
static const uint8_t SIG_LAMER[] = { 
    'L', 'A', 'M', 'E', 'R', ' ', 'E', 'x', 't' 
};
static const uint8_t SIG_SADDAM[] = { 
    'S', 'A', 'D', 'D', 'A', 'M', ' ', 'H' 
};
static const uint8_t SIG_REVENGE[] = { 
    'R', 'E', 'V', 'E', 'N', 'G', 'E' 
};
static const uint8_t SIG_OBELISK[] = { 
    'O', 'b', 'e', 'l', 'i', 's', 'k' 
};
static const uint8_t SIG_BUTCHER[] = {
    'B', 'U', 'T', 'C', 'H', 'E', 'R'
};
static const uint8_t SIG_BSSBSS[] = {
    0x60, 0x00, 0x00, 0x58, 0x00, 0x00, 0x00, 0x00
};

static const uft_amiga_virus_sig_t VIRUS_DB[] = {
    { "SCA", 12, 7, SIG_SCA, 
      "SCA Virus - First widespread Amiga virus", true },
    { "Byte Bandit", 0x30, 15, SIG_BYTE_BANDIT,
      "Byte Bandit - Displays message after infection count", true },
    { "Lamer Exterminator", 0x20, 9, SIG_LAMER,
      "Lamer Exterminator - Destroys data on bad disks", true },
    { "Saddam Hussein", 0x40, 8, SIG_SADDAM,
      "Saddam Hussein virus", true },
    { "Revenge Bootblock", 0x20, 7, SIG_REVENGE,
      "Revenge virus family", true },
    { "Obelisk", 0x30, 7, SIG_OBELISK,
      "Obelisk bootblock virus", false },
    { "Butcher", 0x20, 7, SIG_BUTCHER,
      "Butcher virus - Corrupts disk data", true },
    { "BSS/BSS", 12, 8, SIG_BSSBSS,
      "Generic virus pattern", false },
};

#define VIRUS_DB_COUNT (sizeof(VIRUS_DB) / sizeof(VIRUS_DB[0]))

const uft_amiga_virus_sig_t* uft_amiga_get_virus_db(size_t* count)
{
    if (count) *count = VIRUS_DB_COUNT;
    return VIRUS_DB;
}

/*============================================================================
 * BOOTBLOCK ANALYSIS
 *============================================================================*/

uint32_t uft_amiga_calc_bootblock_checksum(const uint8_t* bootblock)
{
    uint32_t sum = 0;
    
    for (int i = 0; i < UFT_AMIGA_BOOTBLOCK_WORDS; i++) {
        uint32_t word = read_be32(bootblock + i * 4);
        
        /* Skip checksum field at offset 4 */
        if (i == 1) word = 0;
        
        /* Add with overflow carry */
        uint32_t old_sum = sum;
        sum += word;
        if (sum < old_sum) sum++;  /* Carry */
    }
    
    return ~sum;
}

void uft_amiga_fix_bootblock_checksum(uint8_t* bootblock)
{
    uint32_t checksum = uft_amiga_calc_bootblock_checksum(bootblock);
    write_be32(bootblock + 4, checksum);
}

bool uft_amiga_check_bootblock_virus(const uint8_t* bootblock,
                                      const char** virus_name)
{
    for (size_t i = 0; i < VIRUS_DB_COUNT; i++) {
        const uft_amiga_virus_sig_t* sig = &VIRUS_DB[i];
        
        if (sig->offset + sig->length > UFT_AMIGA_BOOTBLOCK_SIZE) continue;
        
        if (memcmp(bootblock + sig->offset, sig->signature, sig->length) == 0) {
            if (virus_name) *virus_name = sig->name;
            return true;
        }
    }
    
    if (virus_name) *virus_name = NULL;
    return false;
}

const char* uft_amiga_identify_custom_bootblock(const uint8_t* bootblock)
{
    /* Check for known custom bootblocks (games, demos, etc.) */
    
    /* NoClick bootblock */
    if (memcmp(bootblock + 0x30, "NoClick", 7) == 0) {
        return "NoClick";
    }
    
    /* Rob Northen Copylock */
    if (bootblock[12] == 0x60 && bootblock[13] == 0x00 &&
        memcmp(bootblock + 0x3E, "RNC", 3) == 0) {
        return "Rob Northen Copylock";
    }
    
    /* NDOS - Non-DOS disk */
    if (bootblock[0] == 'N' && bootblock[1] == 'D' &&
        bootblock[2] == 'O' && bootblock[3] == 'S') {
        return "NDOS";
    }
    
    /* KickStart disk */
    if (bootblock[0] == 'K' && bootblock[1] == 'I' &&
        bootblock[2] == 'C' && bootblock[3] == 'K') {
        return "Kickstart Disk";
    }
    
    return NULL;
}

uft_amiga_bb_type_t uft_amiga_analyze_bootblock(const uint8_t* bootblock,
                                                 uft_amiga_bootblock_info_t* info)
{
    if (!bootblock || !info) return UFT_BB_UNKNOWN;
    
    memset(info, 0, sizeof(*info));
    memcpy(info->data, bootblock, UFT_AMIGA_BOOTBLOCK_SIZE);
    
    /* Check for DOS magic */
    if (bootblock[0] != 'D' || bootblock[1] != 'O' || bootblock[2] != 'S') {
        /* Check for other disk types */
        const char* custom = uft_amiga_identify_custom_bootblock(bootblock);
        if (custom) {
            info->type = UFT_BB_CUSTOM;
            info->custom_name = custom;
            info->dos_type = -1;
        } else {
            info->type = UFT_BB_NOT_DOS;
            info->dos_type = -1;
        }
        return info->type;
    }
    
    /* Parse DOS type */
    info->dos_type = bootblock[3];
    if (info->dos_type > 5) info->dos_type = -1;
    
    /* Verify checksum */
    info->checksum_stored = read_be32(bootblock + 4);
    info->checksum_computed = uft_amiga_calc_bootblock_checksum(bootblock);
    info->checksum_valid = (info->checksum_stored == info->checksum_computed);
    
    /* Check for virus */
    const char* virus_name = NULL;
    if (uft_amiga_check_bootblock_virus(bootblock, &virus_name)) {
        info->type = UFT_BB_VIRUS;
        info->virus_name = virus_name;
        
        /* Find virus details */
        for (size_t i = 0; i < VIRUS_DB_COUNT; i++) {
            if (VIRUS_DB[i].name == virus_name) {
                info->virus_description = VIRUS_DB[i].description;
                info->virus_dangerous = VIRUS_DB[i].is_dangerous;
                break;
            }
        }
        return info->type;
    }
    
    /* Check for custom bootblock */
    const char* custom = uft_amiga_identify_custom_bootblock(bootblock);
    if (custom) {
        info->type = UFT_BB_CUSTOM;
        info->custom_name = custom;
        return info->type;
    }
    
    /* Check for standard bootblock */
    /* Compare boot code signature (skip DOS type and checksum) */
    bool is_standard_13 = (memcmp(bootblock + 12, UFT_AMIGA_BOOTBLOCK_13 + 12, 40) == 0);
    bool is_standard_20 = (memcmp(bootblock + 12, UFT_AMIGA_BOOTBLOCK_20 + 12, 60) == 0);
    
    if (is_standard_13) {
        info->type = UFT_BB_STANDARD_13;
    } else if (is_standard_20) {
        info->type = UFT_BB_STANDARD_20;
    } else if (!info->checksum_valid) {
        info->type = UFT_BB_CORRUPT;
    } else {
        /* Unknown but valid bootblock - might be custom loader */
        info->type = UFT_BB_CUSTOM;
        info->custom_name = "Unknown Custom";
    }
    
    /* Analyze code characteristics */
    info->executable_offset = 12;  /* Boot code starts after header */
    
    /* Look for trackdisk.device string */
    for (int i = 12; i < UFT_AMIGA_BOOTBLOCK_SIZE - 16; i++) {
        if (memcmp(bootblock + i, "trackdisk.device", 16) == 0) {
            info->has_disk_io = true;
            break;
        }
    }
    
    /* Look for dos.library string */
    for (int i = 12; i < UFT_AMIGA_BOOTBLOCK_SIZE - 11; i++) {
        if (memcmp(bootblock + i, "dos.library", 11) == 0) {
            info->has_dos_calls = true;
            break;
        }
    }
    
    return info->type;
}

/*============================================================================
 * BOOTBLOCK INSTALLATION
 *============================================================================*/

int uft_amiga_install_bootblock(uint8_t* bootblock, int dos_type, int kickstart_version)
{
    if (!bootblock) return -1;
    if (dos_type < 0 || dos_type > 5) return -2;
    
    /* Choose bootblock template */
    const uint8_t* template;
    if (kickstart_version >= 20) {
        template = UFT_AMIGA_BOOTBLOCK_20;
    } else {
        template = UFT_AMIGA_BOOTBLOCK_13;
    }
    
    /* Copy template */
    memcpy(bootblock, template, UFT_AMIGA_BOOTBLOCK_SIZE);
    
    /* Set DOS type */
    bootblock[3] = (uint8_t)dos_type;
    
    /* Fix checksum */
    uft_amiga_fix_bootblock_checksum(bootblock);
    
    return 0;
}

/*============================================================================
 * SECTOR SCANNING
 *============================================================================*/

uft_amiga_sector_status_t uft_amiga_check_sector(const uint8_t* sector_data,
                                                  uint32_t block_number,
                                                  uft_amiga_sector_info_t* info)
{
    if (!sector_data || !info) return UFT_SECTOR_UNKNOWN;
    
    memcpy(info->data, sector_data, 512);
    info->block_number = block_number;
    info->status = UFT_SECTOR_NORMAL;
    info->virus_name = NULL;
    
    /* Check for sector-based virus signatures */
    /* This is a simplified check - real implementation would have more signatures */
    
    /* Check for destroyed block (all zeros or all 0xFF) */
    bool all_zero = true;
    bool all_ff = true;
    for (int i = 0; i < 512; i++) {
        if (sector_data[i] != 0x00) all_zero = false;
        if (sector_data[i] != 0xFF) all_ff = false;
    }
    
    if ((all_zero || all_ff) && block_number != 0) {
        info->status = UFT_SECTOR_DESTROYED;
        info->virus_name = "Unknown (data destroyed)";
    }
    
    return info->status;
}

/*============================================================================
 * ADF SCANNING
 *============================================================================*/

int uft_amiga_scan_adf(const uint8_t* adf_data, size_t adf_size,
                       uft_amiga_scan_result_t* result)
{
    if (!adf_data || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    int infections = 0;
    
    /* Determine disk type */
    bool is_hd = (adf_size == 1802240);
    int sectors_per_track = is_hd ? 22 : 11;
    int root_block = is_hd ? 1760 : 880;
    
    /* Check bootblock */
    uft_amiga_bootblock_info_t bb_info;
    uft_amiga_analyze_bootblock(adf_data, &bb_info);
    
    if (bb_info.type == UFT_BB_VIRUS) {
        result->bootblock_infected = true;
        result->bootblock_virus = bb_info.virus_name;
        infections++;
    }
    
    result->can_recover_bootblock = true;  /* Always possible */
    
    /* Check root block */
    size_t root_offset = root_block * 512;
    if (root_offset + 512 <= adf_size) {
        const uint8_t* root = adf_data + root_offset;
        
        /* Check root block type (should be 2 = T_HEADER) */
        uint32_t type = read_be32(root);
        uint32_t sec_type = read_be32(root + 508);
        
        result->root_block_ok = (type == 2 && sec_type == 1);
    }
    
    /* Check bitmap (simplified) */
    if (root_block * 512 + 512 <= adf_size) {
        const uint8_t* root = adf_data + root_block * 512;
        uint32_t bitmap_flag = read_be32(root + 312);
        result->bitmap_ok = (bitmap_flag == 0xFFFFFFFF);
    }
    
    /* Scan all sectors for virus signatures */
    int num_sectors = adf_size / 512;
    for (int i = 2; i < num_sectors; i++) {  /* Skip bootblock */
        uft_amiga_sector_info_t sec_info;
        uft_amiga_sector_status_t status = 
            uft_amiga_check_sector(adf_data + i * 512, i, &sec_info);
        
        if (status == UFT_SECTOR_INFECTED) {
            result->infected_sectors++;
            infections++;
        } else if (status == UFT_SECTOR_DESTROYED) {
            result->destroyed_sectors++;
        }
    }
    
    /* Determine recovery options */
    result->can_recover_filesystem = result->root_block_ok;
    
    if (infections > 0) {
        result->recovery_notes = "Virus detected. Install clean bootblock recommended.";
    } else if (result->destroyed_sectors > 0) {
        result->recovery_notes = "Damaged sectors found. Data may be unrecoverable.";
    } else if (!result->root_block_ok) {
        result->recovery_notes = "Root block damaged. Filesystem recovery needed.";
    }
    
    return infections;
}

/*============================================================================
 * RECOVERY
 *============================================================================*/

int uft_amiga_recover_adf(uint8_t* adf_data, size_t adf_size,
                          const uft_amiga_recovery_options_t* options,
                          uft_amiga_recovery_result_t* result)
{
    if (!adf_data || !options || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    snprintf(result->log, sizeof(result->log), "Recovery started\n");
    
    /* Analyze current state */
    uft_amiga_bootblock_info_t bb_info;
    uft_amiga_analyze_bootblock(adf_data, &bb_info);
    
    /* Repair bootblock if requested */
    if (options->repair_bootblock) {
        if (bb_info.type == UFT_BB_VIRUS || bb_info.type == UFT_BB_CORRUPT) {
            result->errors_found++;
            
            if (!options->strict_mode) {
                int dos_type = bb_info.dos_type >= 0 ? bb_info.dos_type : 0;
                uft_amiga_install_bootblock(adf_data, dos_type, 
                                           options->kickstart_version);
                result->errors_fixed++;
                strncat(result->log, "Bootblock replaced with standard\n", 
                       sizeof(result->log) - strlen(result->log) - 1);
            }
        }
    }
    
    /* Repair root block checksum if needed */
    if (options->repair_root_block) {
        bool is_hd = (adf_size == 1802240);
        int root_block = is_hd ? 1760 : 880;
        size_t root_offset = root_block * 512;
        
        if (root_offset + 512 <= adf_size) {
            uint8_t* root = adf_data + root_offset;
            
            /* Recalculate checksum */
            uint32_t sum = 0;
            for (int i = 0; i < 128; i++) {
                if (i != 5) {  /* Skip checksum field */
                    sum += read_be32(root + i * 4);
                }
            }
            uint32_t correct_checksum = (uint32_t)(-(int32_t)sum);
            uint32_t stored_checksum = read_be32(root + 20);
            
            if (correct_checksum != stored_checksum) {
                result->errors_found++;
                
                if (!options->strict_mode) {
                    write_be32(root + 20, correct_checksum);
                    result->errors_fixed++;
                    strncat(result->log, "Root block checksum fixed\n",
                           sizeof(result->log) - strlen(result->log) - 1);
                }
            }
        }
    }
    
    return 0;
}
