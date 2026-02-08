/**
 * @file uft_fat_boot.h
 * @brief FAT Boot Sector Templates
 * @version 1.0.0
 * 
 * Bootable FAT image creation with various boot codes:
 * - MS-DOS 6.22 boot sector
 * - FreeDOS boot sector
 * - Windows 9x boot sector
 * - Generic "Not bootable" message
 * - Custom boot code injection
 * 
 * @note Part of UnifiedFloppyTool preservation suite
 */

#ifndef UFT_FAT_BOOT_H
#define UFT_FAT_BOOT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "uft/fs/uft_fat12.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Boot Template Types
 *===========================================================================*/

/**
 * @brief Available boot code templates
 */
typedef enum {
    UFT_BOOT_NONE = 0,              /**< No boot code (zeroed) */
    UFT_BOOT_NOT_BOOTABLE,          /**< "Not a bootable disk" message */
    UFT_BOOT_MSDOS_622,             /**< MS-DOS 6.22 compatible */
    UFT_BOOT_MSDOS_70,              /**< MS-DOS 7.0 (Windows 95) */
    UFT_BOOT_FREEDOS,               /**< FreeDOS boot sector */
    UFT_BOOT_FREEDOS_FAT32,         /**< FreeDOS FAT32 boot sector */
    UFT_BOOT_WIN98,                 /**< Windows 98 boot sector */
    UFT_BOOT_SYSLINUX,              /**< SYSLINUX compatible */
    UFT_BOOT_GRUB,                  /**< GRUB stage 1 */
    UFT_BOOT_CUSTOM,                /**< User-provided boot code */
    UFT_BOOT_COUNT
} uft_boot_template_t;

/**
 * @brief Boot code information
 */
typedef struct {
    uft_boot_template_t type;       /**< Template type */
    const char *name;               /**< Human-readable name */
    const char *description;        /**< Description */
    uft_fat_type_t fat_type;        /**< Supported FAT type (0=all) */
    size_t code_size;               /**< Boot code size */
    size_t code_offset;             /**< Offset in boot sector */
    const uint8_t *boot_code;       /**< Boot code data */
    const char *required_files;     /**< Required system files (comma-sep) */
} uft_boot_info_t;

/*===========================================================================
 * Boot Messages
 *===========================================================================*/

/** Standard "not bootable" message */
#define UFT_BOOT_MSG_NOT_BOOTABLE \
    "This is not a bootable disk. Please insert a bootable floppy and\r\n" \
    "press any key to try again...\r\n"

/** DOS boot error message */
#define UFT_BOOT_MSG_DOS_ERROR \
    "Non-System disk or disk error\r\n" \
    "Replace and press any key when ready\r\n"

/** FreeDOS boot message */
#define UFT_BOOT_MSG_FREEDOS \
    "FreeDOS\r\n"

/*===========================================================================
 * Boot Code Constants
 *===========================================================================*/

/** Boot sector jump instruction: JMP SHORT + NOP */
#define UFT_BOOT_JMP_SHORT          0xEB
#define UFT_BOOT_JMP_NEAR           0xE9
#define UFT_BOOT_NOP                0x90

/** Boot code area in FAT12/16 boot sector */
#define UFT_BOOT_CODE_OFFSET_FAT16  0x3E
#define UFT_BOOT_CODE_SIZE_FAT16    448

/** Boot code area in FAT32 boot sector */
#define UFT_BOOT_CODE_OFFSET_FAT32  0x5A
#define UFT_BOOT_CODE_SIZE_FAT32    420

/** OEM names for different systems */
#define UFT_OEM_MSDOS               "MSDOS5.0"
#define UFT_OEM_MSWIN               "MSWIN4.1"
#define UFT_OEM_FREEDOS             "FRDOS7.1"
#define UFT_OEM_MKDOSFS             "mkdosfs "
#define UFT_OEM_UFT                 "UFT 3.8 "

/*===========================================================================
 * API - Template Access
 *===========================================================================*/

/**
 * @brief Get boot template information
 * @param type Template type
 * @return Pointer to info or NULL
 */
const uft_boot_info_t *uft_boot_get_info(uft_boot_template_t type);

/**
 * @brief Get list of available templates
 * @param count Output: number of templates
 * @return Array of template infos
 */
const uft_boot_info_t *uft_boot_list_templates(size_t *count);

/**
 * @brief Find template by name
 * @param name Template name
 * @return Template type or UFT_BOOT_NONE
 */
uft_boot_template_t uft_boot_find_by_name(const char *name);

/*===========================================================================
 * API - Boot Code Application
 *===========================================================================*/

/**
 * @brief Apply boot template to boot sector
 * @param boot Boot sector (modified)
 * @param type Template type
 * @param fat_type FAT type (12, 16, 32)
 * @return 0 on success
 * 
 * @note Preserves BPB, only modifies boot code area
 */
int uft_boot_apply_template(void *boot, uft_boot_template_t type,
                            uft_fat_type_t fat_type);

/**
 * @brief Apply custom boot code
 * @param boot Boot sector (modified)
 * @param code Boot code data
 * @param code_size Size of boot code
 * @param fat_type FAT type
 * @return 0 on success
 */
int uft_boot_apply_custom(void *boot, const uint8_t *code, size_t code_size,
                          uft_fat_type_t fat_type);

/**
 * @brief Apply boot code from file
 * @param boot Boot sector (modified)
 * @param filename Path to boot code file
 * @param fat_type FAT type
 * @return 0 on success
 */
int uft_boot_apply_from_file(void *boot, const char *filename,
                             uft_fat_type_t fat_type);

/**
 * @brief Set custom boot message
 * @param boot Boot sector (modified)
 * @param message Message to display (null-terminated)
 * @param fat_type FAT type
 * @return 0 on success
 * 
 * @note Creates minimal boot code that displays message
 */
int uft_boot_set_message(void *boot, const char *message, uft_fat_type_t fat_type);

/*===========================================================================
 * API - Boot Sector Validation
 *===========================================================================*/

/**
 * @brief Check if boot sector is bootable
 * @param boot Boot sector
 * @param fat_type FAT type
 * @return true if contains valid boot code
 */
bool uft_boot_is_bootable(const void *boot, uft_fat_type_t fat_type);

/**
 * @brief Identify boot code type
 * @param boot Boot sector
 * @param fat_type FAT type
 * @return Detected template type
 */
uft_boot_template_t uft_boot_identify(const void *boot, uft_fat_type_t fat_type);

/**
 * @brief Extract boot code to buffer
 * @param boot Boot sector
 * @param code Output buffer
 * @param max_size Buffer size
 * @param fat_type FAT type
 * @return Actual code size extracted
 */
size_t uft_boot_extract_code(const void *boot, uint8_t *code, size_t max_size,
                             uft_fat_type_t fat_type);

/*===========================================================================
 * API - OEM Name Handling
 *===========================================================================*/

/**
 * @brief Set OEM name in boot sector
 * @param boot Boot sector (modified)
 * @param oem_name OEM name (max 8 chars, space-padded)
 * @return 0 on success
 */
int uft_boot_set_oem(void *boot, const char *oem_name);

/**
 * @brief Get OEM name from boot sector
 * @param boot Boot sector
 * @param oem_name Output buffer (at least 9 bytes)
 */
void uft_boot_get_oem(const void *boot, char *oem_name);

/**
 * @brief Set OEM name based on template
 * @param boot Boot sector (modified)
 * @param type Template type
 * @return 0 on success
 */
int uft_boot_set_oem_for_template(void *boot, uft_boot_template_t type);

/*===========================================================================
 * API - System File Installation
 *===========================================================================*/

/**
 * @brief Get required system files for template
 * @param type Template type
 * @return Comma-separated list of files or NULL
 */
const char *uft_boot_required_files(uft_boot_template_t type);

/**
 * @brief Check if image has required system files
 * @param ctx FAT context
 * @param type Boot template
 * @return true if all required files present
 */
bool uft_boot_check_system_files(const uft_fat_ctx_t *ctx, 
                                 uft_boot_template_t type);

/*===========================================================================
 * Boot Code Templates (Embedded)
 *===========================================================================*/

/**
 * @brief Get "Not Bootable" boot code
 * @param size Output: code size
 * @return Boot code data
 */
const uint8_t *uft_boot_code_not_bootable(size_t *size);

/**
 * @brief Get MS-DOS 6.22 boot code stub
 * @param size Output: code size  
 * @return Boot code data
 */
const uint8_t *uft_boot_code_msdos622(size_t *size);

/**
 * @brief Get FreeDOS boot code stub
 * @param size Output: code size
 * @return Boot code data
 */
const uint8_t *uft_boot_code_freedos(size_t *size);

/**
 * @brief Get FreeDOS FAT32 boot code
 * @param size Output: code size
 * @return Boot code data
 */
const uint8_t *uft_boot_code_freedos_fat32(size_t *size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FAT_BOOT_H */
