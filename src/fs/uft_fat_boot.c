/**
 * @file uft_fat_boot.c
 * @brief FAT Boot Sector Templates Implementation
 */

#include "uft/fs/uft_fat_boot.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*===========================================================================
 * Boot Code: "Not Bootable" Message
 *===========================================================================*/

/* Minimal boot code that displays a message and waits for keypress */
static const uint8_t boot_code_not_bootable[] = {
    /* Entry point at 0x3E for FAT12/16 */
    0xFA,                   /* CLI */
    0x31, 0xC0,             /* XOR AX, AX */
    0x8E, 0xD8,             /* MOV DS, AX */
    0x8E, 0xC0,             /* MOV ES, AX */
    0x8E, 0xD0,             /* MOV SS, AX */
    0xBC, 0x00, 0x7C,       /* MOV SP, 0x7C00 */
    0xFB,                   /* STI */
    
    /* Display message */
    0xBE, 0x5E, 0x7C,       /* MOV SI, message (offset 0x5E) */
    
    /* Print loop */
    0xAC,                   /* LODSB */
    0x08, 0xC0,             /* OR AL, AL */
    0x74, 0x09,             /* JZ wait_key */
    0xB4, 0x0E,             /* MOV AH, 0x0E (teletype) */
    0xBB, 0x07, 0x00,       /* MOV BX, 0x0007 */
    0xCD, 0x10,             /* INT 0x10 */
    0xEB, 0xF2,             /* JMP print_loop */
    
    /* Wait for keypress */
    0x31, 0xC0,             /* XOR AX, AX */
    0xCD, 0x16,             /* INT 0x16 (wait key) */
    
    /* Reboot */
    0xCD, 0x19,             /* INT 0x19 (reboot) */
    
    /* Message (null-terminated) */
    'T', 'h', 'i', 's', ' ', 'i', 's', ' ', 'n', 'o', 't', ' ',
    'a', ' ', 'b', 'o', 'o', 't', 'a', 'b', 'l', 'e', ' ', 'd', 'i', 's', 'k', '.', 
    '\r', '\n',
    'P', 'l', 'e', 'a', 's', 'e', ' ', 'i', 'n', 's', 'e', 'r', 't', ' ', 
    'a', ' ', 'b', 'o', 'o', 't', 'a', 'b', 'l', 'e', ' ', 'f', 'l', 'o', 'p', 'p', 'y',
    '\r', '\n',
    'a', 'n', 'd', ' ', 'p', 'r', 'e', 's', 's', ' ', 'a', 'n', 'y', ' ', 'k', 'e', 'y',
    '.', '.', '.', '\r', '\n', 0
};

/*===========================================================================
 * Boot Code: MS-DOS 6.22 Stub
 *===========================================================================*/

/* DOS-compatible boot code stub that loads IO.SYS */
static const uint8_t boot_code_msdos622[] = {
    /* Standard DOS bootstrap */
    0xFA,                   /* CLI */
    0x33, 0xC0,             /* XOR AX, AX */
    0x8E, 0xD0,             /* MOV SS, AX */
    0xBC, 0x00, 0x7C,       /* MOV SP, 0x7C00 */
    0x8E, 0xD8,             /* MOV DS, AX */
    0x8E, 0xC0,             /* MOV ES, AX */
    0xFB,                   /* STI */
    0xFC,                   /* CLD */
    
    /* Error - no system files */
    0xBE, 0x4A, 0x7C,       /* MOV SI, msg */
    0xAC,                   /* LODSB */
    0x0A, 0xC0,             /* OR AL, AL */
    0x74, 0x09,             /* JZ wait */
    0xB4, 0x0E,             /* MOV AH, 0x0E */
    0xBB, 0x07, 0x00,       /* MOV BX, 7 */
    0xCD, 0x10,             /* INT 10h */
    0xEB, 0xF2,             /* JMP loop */
    
    /* Wait and reboot */
    0x33, 0xC0,             /* XOR AX, AX */
    0xCD, 0x16,             /* INT 16h */
    0xCD, 0x19,             /* INT 19h */
    
    /* Message */
    'N', 'o', 'n', '-', 'S', 'y', 's', 't', 'e', 'm', ' ', 'd', 'i', 's', 'k', '\r', '\n',
    'R', 'e', 'p', 'l', 'a', 'c', 'e', ' ', 'a', 'n', 'd', ' ', 'p', 'r', 'e', 's', 's',
    ' ', 'a', 'n', 'y', ' ', 'k', 'e', 'y', '\r', '\n', 0
};

/*===========================================================================
 * Boot Code: FreeDOS
 *===========================================================================*/

static const uint8_t boot_code_freedos[] = {
    /* FreeDOS bootstrap stub */
    0xFA,                   /* CLI */
    0x31, 0xC0,             /* XOR AX, AX */
    0x8E, 0xD8,             /* MOV DS, AX */
    0x8E, 0xD0,             /* MOV SS, AX */
    0xBC, 0x00, 0x7C,       /* MOV SP, 0x7C00 */
    0xFB,                   /* STI */
    
    /* Print FreeDOS message */
    0xBE, 0x42, 0x7C,       /* MOV SI, msg */
    0xAC,                   /* LODSB */
    0x08, 0xC0,             /* OR AL, AL */
    0x74, 0x09,             /* JZ halt */
    0xB4, 0x0E,             /* MOV AH, 0x0E */
    0xBB, 0x07, 0x00,       /* MOV BX, 7 */
    0xCD, 0x10,             /* INT 10h */
    0xEB, 0xF2,             /* JMP loop */
    
    /* Halt */
    0xF4,                   /* HLT */
    0xEB, 0xFD,             /* JMP halt */
    
    /* Message */
    'F', 'r', 'e', 'e', 'D', 'O', 'S', '\r', '\n',
    'N', 'o', ' ', 'K', 'E', 'R', 'N', 'E', 'L', '.', 'S', 'Y', 'S', '\r', '\n', 0
};

/*===========================================================================
 * Boot Code: FreeDOS FAT32
 *===========================================================================*/

static const uint8_t boot_code_freedos_fat32[] = {
    /* FAT32 FreeDOS bootstrap stub (entry at 0x5A) */
    0xFA,                   /* CLI */
    0x31, 0xC0,             /* XOR AX, AX */
    0x8E, 0xD8,             /* MOV DS, AX */
    0x8E, 0xD0,             /* MOV SS, AX */
    0xBC, 0x00, 0x7C,       /* MOV SP, 0x7C00 */
    0xFB,                   /* STI */
    
    /* Print message */
    0xBE, 0x74, 0x7C,       /* MOV SI, msg (0x5A + offset) */
    0xAC,                   /* LODSB */
    0x08, 0xC0,             /* OR AL, AL */
    0x74, 0x09,             /* JZ halt */
    0xB4, 0x0E,             /* MOV AH, 0x0E */
    0xBB, 0x07, 0x00,       /* MOV BX, 7 */
    0xCD, 0x10,             /* INT 10h */
    0xEB, 0xF2,             /* JMP loop */
    
    0xF4, 0xEB, 0xFD,       /* HLT; JMP -3 */
    
    'F', 'r', 'e', 'e', 'D', 'O', 'S', ' ', 'F', 'A', 'T', '3', '2', '\r', '\n', 0
};

/*===========================================================================
 * Template Information Table
 *===========================================================================*/

static const uft_boot_info_t boot_templates[] = {
    {
        .type = UFT_BOOT_NONE,
        .name = "none",
        .description = "No boot code (zeroed)",
        .fat_type = 0,
        .code_size = 0,
        .code_offset = 0,
        .boot_code = NULL,
        .required_files = NULL
    },
    {
        .type = UFT_BOOT_NOT_BOOTABLE,
        .name = "not-bootable",
        .description = "Displays 'not bootable' message",
        .fat_type = 0,
        .code_size = sizeof(boot_code_not_bootable),
        .code_offset = UFT_BOOT_CODE_OFFSET_FAT16,
        .boot_code = boot_code_not_bootable,
        .required_files = NULL
    },
    {
        .type = UFT_BOOT_MSDOS_622,
        .name = "msdos622",
        .description = "MS-DOS 6.22 compatible boot sector",
        .fat_type = UFT_FAT_TYPE_FAT12,
        .code_size = sizeof(boot_code_msdos622),
        .code_offset = UFT_BOOT_CODE_OFFSET_FAT16,
        .boot_code = boot_code_msdos622,
        .required_files = "IO.SYS,MSDOS.SYS,COMMAND.COM"
    },
    {
        .type = UFT_BOOT_FREEDOS,
        .name = "freedos",
        .description = "FreeDOS boot sector",
        .fat_type = 0,
        .code_size = sizeof(boot_code_freedos),
        .code_offset = UFT_BOOT_CODE_OFFSET_FAT16,
        .boot_code = boot_code_freedos,
        .required_files = "KERNEL.SYS,COMMAND.COM"
    },
    {
        .type = UFT_BOOT_FREEDOS_FAT32,
        .name = "freedos-fat32",
        .description = "FreeDOS FAT32 boot sector",
        .fat_type = UFT_FAT_TYPE_FAT32,
        .code_size = sizeof(boot_code_freedos_fat32),
        .code_offset = UFT_BOOT_CODE_OFFSET_FAT32,
        .boot_code = boot_code_freedos_fat32,
        .required_files = "KERNEL.SYS,COMMAND.COM"
    }
};

#define BOOT_TEMPLATE_COUNT (sizeof(boot_templates) / sizeof(boot_templates[0]))

/*===========================================================================
 * API Implementation
 *===========================================================================*/

const uft_boot_info_t *uft_boot_get_info(uft_boot_template_t type)
{
    for (size_t i = 0; i < BOOT_TEMPLATE_COUNT; i++) {
        if (boot_templates[i].type == type) {
            return &boot_templates[i];
        }
    }
    return NULL;
}

const uft_boot_info_t *uft_boot_list_templates(size_t *count)
{
    if (count) *count = BOOT_TEMPLATE_COUNT;
    return boot_templates;
}

uft_boot_template_t uft_boot_find_by_name(const char *name)
{
    if (!name) return UFT_BOOT_NONE;
    
    for (size_t i = 0; i < BOOT_TEMPLATE_COUNT; i++) {
        if (strcmp(boot_templates[i].name, name) == 0) {
            return boot_templates[i].type;
        }
    }
    return UFT_BOOT_NONE;
}

int uft_boot_apply_template(void *boot, uft_boot_template_t type,
                            uft_fat_type_t fat_type)
{
    if (!boot) return -1;
    
    const uft_boot_info_t *info = uft_boot_get_info(type);
    if (!info) return -2;
    
    /* Check FAT type compatibility */
    if (info->fat_type != 0 && info->fat_type != fat_type) {
        return -3;  /* Incompatible FAT type */
    }
    
    /* Determine code offset based on FAT type */
    size_t offset = (fat_type == UFT_FAT_TYPE_FAT32) ? 
                    UFT_BOOT_CODE_OFFSET_FAT32 : UFT_BOOT_CODE_OFFSET_FAT16;
    size_t max_size = (fat_type == UFT_FAT_TYPE_FAT32) ?
                      UFT_BOOT_CODE_SIZE_FAT32 : UFT_BOOT_CODE_SIZE_FAT16;
    
    uint8_t *boot_bytes = (uint8_t *)boot;
    
    if (type == UFT_BOOT_NONE) {
        /* Clear boot code area */
        memset(boot_bytes + offset, 0, max_size);
    } else if (info->boot_code && info->code_size > 0) {
        /* Apply boot code */
        size_t copy_size = info->code_size < max_size ? info->code_size : max_size;
        memset(boot_bytes + offset, 0, max_size);
        memcpy(boot_bytes + offset, info->boot_code, copy_size);
    }
    
    /* Set jump instruction */
    boot_bytes[0] = UFT_BOOT_JMP_SHORT;
    boot_bytes[1] = (uint8_t)(offset - 2);
    boot_bytes[2] = UFT_BOOT_NOP;
    
    return 0;
}

int uft_boot_apply_custom(void *boot, const uint8_t *code, size_t code_size,
                          uft_fat_type_t fat_type)
{
    if (!boot || !code) return -1;
    
    size_t offset = (fat_type == UFT_FAT_TYPE_FAT32) ? 
                    UFT_BOOT_CODE_OFFSET_FAT32 : UFT_BOOT_CODE_OFFSET_FAT16;
    size_t max_size = (fat_type == UFT_FAT_TYPE_FAT32) ?
                      UFT_BOOT_CODE_SIZE_FAT32 : UFT_BOOT_CODE_SIZE_FAT16;
    
    if (code_size > max_size) code_size = max_size;
    
    uint8_t *boot_bytes = (uint8_t *)boot;
    memset(boot_bytes + offset, 0, max_size);
    memcpy(boot_bytes + offset, code, code_size);
    
    return 0;
}

int uft_boot_apply_from_file(void *boot, const char *filename,
                             uft_fat_type_t fat_type)
{
    if (!boot || !filename) return -1;
    
    FILE *fp = fopen(filename, "rb");
    if (!fp) return -2;
    
    size_t max_size = (fat_type == UFT_FAT_TYPE_FAT32) ?
                      UFT_BOOT_CODE_SIZE_FAT32 : UFT_BOOT_CODE_SIZE_FAT16;
    
    uint8_t *code = malloc(max_size);
    if (!code) {
        fclose(fp);
        return -3;
    }
    
    size_t code_size = fread(code, 1, max_size, fp);
    fclose(fp);
    
    int result = uft_boot_apply_custom(boot, code, code_size, fat_type);
    free(code);
    
    return result;
}

int uft_boot_set_message(void *boot, const char *message, uft_fat_type_t fat_type)
{
    if (!boot || !message) return -1;
    
    size_t offset = (fat_type == UFT_FAT_TYPE_FAT32) ? 
                    UFT_BOOT_CODE_OFFSET_FAT32 : UFT_BOOT_CODE_OFFSET_FAT16;
    size_t max_size = (fat_type == UFT_FAT_TYPE_FAT32) ?
                      UFT_BOOT_CODE_SIZE_FAT32 : UFT_BOOT_CODE_SIZE_FAT16;
    
    uint8_t *boot_bytes = (uint8_t *)boot;
    
    /* Build minimal boot code with message */
    static const uint8_t msg_code_prefix[] = {
        0xFA,               /* CLI */
        0x31, 0xC0,         /* XOR AX, AX */
        0x8E, 0xD8,         /* MOV DS, AX */
        0x8E, 0xD0,         /* MOV SS, AX */
        0xBC, 0x00, 0x7C,   /* MOV SP, 0x7C00 */
        0xFB,               /* STI */
        0xBE, 0x00, 0x00,   /* MOV SI, msg_offset (patched below) */
        0xAC,               /* LODSB */
        0x08, 0xC0,         /* OR AL, AL */
        0x74, 0x09,         /* JZ wait */
        0xB4, 0x0E,         /* MOV AH, 0x0E */
        0xBB, 0x07, 0x00,   /* MOV BX, 7 */
        0xCD, 0x10,         /* INT 10h */
        0xEB, 0xF2,         /* JMP loop */
        0x31, 0xC0,         /* XOR AX, AX */
        0xCD, 0x16,         /* INT 16h */
        0xCD, 0x19          /* INT 19h */
    };
    
    size_t prefix_len = sizeof(msg_code_prefix);
    size_t msg_len = strlen(message) + 1;
    
    if (prefix_len + msg_len > max_size) {
        msg_len = max_size - prefix_len;
    }
    
    memset(boot_bytes + offset, 0, max_size);
    memcpy(boot_bytes + offset, msg_code_prefix, prefix_len);
    memcpy(boot_bytes + offset + prefix_len, message, msg_len - 1);
    boot_bytes[offset + prefix_len + msg_len - 1] = 0;
    
    /* Patch message offset (0x7C00 + offset + prefix_len) */
    uint16_t msg_addr = 0x7C00 + offset + prefix_len;
    boot_bytes[offset + 12] = msg_addr & 0xFF;
    boot_bytes[offset + 13] = msg_addr >> 8;
    
    /* Set jump */
    boot_bytes[0] = UFT_BOOT_JMP_SHORT;
    boot_bytes[1] = (uint8_t)(offset - 2);
    boot_bytes[2] = UFT_BOOT_NOP;
    
    return 0;
}

bool uft_boot_is_bootable(const void *boot, uft_fat_type_t fat_type)
{
    if (!boot) return false;
    
    const uint8_t *boot_bytes = (const uint8_t *)boot;
    
    /* Check boot signature */
    if (boot_bytes[510] != 0x55 || boot_bytes[511] != 0xAA) return false;
    
    /* Check jump instruction */
    if (boot_bytes[0] != UFT_BOOT_JMP_SHORT && boot_bytes[0] != UFT_BOOT_JMP_NEAR) {
        return false;
    }
    
    /* Check if boot code area has content */
    size_t offset = (fat_type == UFT_FAT_TYPE_FAT32) ? 
                    UFT_BOOT_CODE_OFFSET_FAT32 : UFT_BOOT_CODE_OFFSET_FAT16;
    
    bool has_code = false;
    for (size_t i = 0; i < 32; i++) {
        if (boot_bytes[offset + i] != 0) {
            has_code = true;
            break;
        }
    }
    
    return has_code;
}

uft_boot_template_t uft_boot_identify(const void *boot, uft_fat_type_t fat_type)
{
    if (!boot) return UFT_BOOT_NONE;
    
    const uint8_t *boot_bytes = (const uint8_t *)boot;
    size_t offset = (fat_type == UFT_FAT_TYPE_FAT32) ? 
                    UFT_BOOT_CODE_OFFSET_FAT32 : UFT_BOOT_CODE_OFFSET_FAT16;
    
    /* Compare against known templates */
    for (size_t i = 1; i < BOOT_TEMPLATE_COUNT; i++) {
        if (boot_templates[i].boot_code && boot_templates[i].code_size > 0) {
            if (memcmp(boot_bytes + offset, boot_templates[i].boot_code,
                       boot_templates[i].code_size < 32 ? 
                       boot_templates[i].code_size : 32) == 0) {
                return boot_templates[i].type;
            }
        }
    }
    
    return uft_boot_is_bootable(boot, fat_type) ? UFT_BOOT_CUSTOM : UFT_BOOT_NONE;
}

size_t uft_boot_extract_code(const void *boot, uint8_t *code, size_t max_size,
                             uft_fat_type_t fat_type)
{
    if (!boot || !code) return 0;
    
    size_t offset = (fat_type == UFT_FAT_TYPE_FAT32) ? 
                    UFT_BOOT_CODE_OFFSET_FAT32 : UFT_BOOT_CODE_OFFSET_FAT16;
    size_t code_size = (fat_type == UFT_FAT_TYPE_FAT32) ?
                       UFT_BOOT_CODE_SIZE_FAT32 : UFT_BOOT_CODE_SIZE_FAT16;
    
    if (code_size > max_size) code_size = max_size;
    
    memcpy(code, (const uint8_t *)boot + offset, code_size);
    return code_size;
}

int uft_boot_set_oem(void *boot, const char *oem_name)
{
    if (!boot || !oem_name) return -1;
    
    uint8_t *boot_bytes = (uint8_t *)boot;
    
    /* OEM name is at offset 3, 8 bytes, space-padded */
    memset(boot_bytes + 3, ' ', 8);
    size_t len = strlen(oem_name);
    if (len > 8) len = 8;
    memcpy(boot_bytes + 3, oem_name, len);
    
    return 0;
}

void uft_boot_get_oem(const void *boot, char *oem_name)
{
    if (!boot || !oem_name) return;
    
    const uint8_t *boot_bytes = (const uint8_t *)boot;
    memcpy(oem_name, boot_bytes + 3, 8);
    oem_name[8] = '\0';
    
    /* Trim trailing spaces */
    for (int i = 7; i >= 0 && oem_name[i] == ' '; i--) {
        oem_name[i] = '\0';
    }
}

int uft_boot_set_oem_for_template(void *boot, uft_boot_template_t type)
{
    const char *oem = NULL;
    
    switch (type) {
        case UFT_BOOT_MSDOS_622:
        case UFT_BOOT_MSDOS_70:
            oem = UFT_OEM_MSDOS;
            break;
        case UFT_BOOT_WIN98:
            oem = UFT_OEM_MSWIN;
            break;
        case UFT_BOOT_FREEDOS:
        case UFT_BOOT_FREEDOS_FAT32:
            oem = UFT_OEM_FREEDOS;
            break;
        default:
            oem = UFT_OEM_UFT;
            break;
    }
    
    return uft_boot_set_oem(boot, oem);
}

const char *uft_boot_required_files(uft_boot_template_t type)
{
    const uft_boot_info_t *info = uft_boot_get_info(type);
    return info ? info->required_files : NULL;
}

bool uft_boot_check_system_files(const uft_fat_ctx_t *ctx, 
                                 uft_boot_template_t type)
{
    if (!ctx || !ctx->data) return false;
    
    /* Get required files list for this boot template */
    const uft_boot_info_t *info = uft_boot_get_info(type);
    if (!info || !info->required_files) return true;  /* No files required */
    
    /* Parse comma-separated required files */
    char files[256];
    strncpy(files, info->required_files, sizeof(files) - 1);
    files[sizeof(files) - 1] = '\0';
    
    /* Scan root directory for each required file */
    char *file = strtok(files, ",");
    while (file) {
        /* Trim whitespace */
        while (*file == ' ') file++;
        
        /* Convert filename to 8.3 FAT format (padded with spaces) */
        char fat_name[11];
        memset(fat_name, ' ', 11);
        const char *dot = strchr(file, '.');
        if (dot) {
            size_t base_len = (size_t)(dot - file);
            if (base_len > 8) base_len = 8;
            memcpy(fat_name, file, base_len);
            size_t ext_len = strlen(dot + 1);
            if (ext_len > 3) ext_len = 3;
            memcpy(fat_name + 8, dot + 1, ext_len);
        } else {
            size_t len = strlen(file);
            if (len > 8) len = 8;
            memcpy(fat_name, file, len);
        }
        
        /* Search root directory */
        bool found = false;
        size_t dir_offset = (size_t)ctx->vol.root_dir_sector * ctx->vol.bytes_per_sector;
        size_t dir_size = (size_t)ctx->vol.root_dir_sectors * ctx->vol.bytes_per_sector;
        
        for (size_t off = 0; off < dir_size && dir_offset + off + 32 <= ctx->data_size; off += 32) {
            const uint8_t *entry = ctx->data + dir_offset + off;
            if (entry[0] == 0x00) break;      /* End of directory */
            if (entry[0] == 0xE5) continue;    /* Deleted */
            if (entry[11] == 0x0F) continue;   /* LFN entry */
            if (entry[11] & 0x08) continue;    /* Volume label */
            
            if (memcmp(entry, fat_name, 11) == 0) {
                found = true;
                break;
            }
        }
        
        if (!found) return false;  /* Required file missing */
        file = strtok(NULL, ",");
    }
    
    return true;  /* All required files found */
}

const uint8_t *uft_boot_code_not_bootable(size_t *size)
{
    if (size) *size = sizeof(boot_code_not_bootable);
    return boot_code_not_bootable;
}

const uint8_t *uft_boot_code_msdos622(size_t *size)
{
    if (size) *size = sizeof(boot_code_msdos622);
    return boot_code_msdos622;
}

const uint8_t *uft_boot_code_freedos(size_t *size)
{
    if (size) *size = sizeof(boot_code_freedos);
    return boot_code_freedos;
}

const uint8_t *uft_boot_code_freedos_fat32(size_t *size)
{
    if (size) *size = sizeof(boot_code_freedos_fat32);
    return boot_code_freedos_fat32;
}
