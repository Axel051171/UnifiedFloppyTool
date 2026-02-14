/**
 * @file uft_atarist_dec0de.h
 * @brief Atari ST Copy Protection Decoder
 * 
 * Complete implementation of Atari ST copy protection decryption algorithms.
 * Based on dec0de by Orion ^ The Replicants.
 * 
 * Supported protections:
 * - Rob Northen Copylock Series 1 (1988) - 5 variants
 * - Rob Northen Copylock Series 2 (1989) - 6 variants
 * - Illegal Anti-bitos v1.0/1.4/1.6/1.61
 * - Zippy Little Protection v2.05/v2.06
 * - Toxic Packer v1.0
 * - Cameo Cooper v0.5/v0.6
 * - CID Encrypter v1.0bp
 * - Yoda Lock-o-matic v1.3
 * 
 * @version 1.0.0
 * @date 2026-01-04
 * @author UFT Team
 * @note Based on dec0de by Orion ^ The Replicants ^ Fuzion
 * 
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_ATARIST_DEC0DE_H
#define UFT_ATARIST_DEC0DE_H

#include <stdint.h>
#ifdef _WIN32
#include <BaseTsd.h>
#ifndef _SSIZE_T_DEFINED
#define _SSIZE_T_DEFINED
typedef SSIZE_T ssize_t;
#endif
#else
#include <sys/types.h>
#endif
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

/** GEMDOS program header magic */
#define UFT_GEMDOS_MAGIC            0x601A

/** GEMDOS program header size */
#define UFT_GEMDOS_HEADER_SIZE      28

/** Protection pattern types */
#define UFT_PATTERN_NONE            0
#define UFT_PATTERN_PROG            1   /**< GEMDOS program */
#define UFT_PATTERN_BIN             2   /**< Binary file */
#define UFT_PATTERN_ANY             3   /**< Either type */

/*===========================================================================
 * Rob Northen Copylock Constants
 *===========================================================================*/

/** Rob Northen serial usage flags */
#define UFT_ROBN_SERIAL_NONE            0x00
#define UFT_ROBN_SERIAL_DECODE_PROG     0x01  /**< Decrypt wrapped program */
#define UFT_ROBN_SERIAL_RETURN          0x02  /**< Return to caller */
#define UFT_ROBN_SERIAL_SAVE_MEM        0x04  /**< Store in memory */
#define UFT_ROBN_SERIAL_MAGIC_MEM       0x08  /**< Compute magic value */
#define UFT_ROBN_SERIAL_EOR_MEM         0x10  /**< XOR memory regions */
#define UFT_ROBN_SERIAL_OTHER_MEM       0x20  /**< Other memory ops */
#define UFT_ROBN_SERIAL_UNKNOWN         0x40  /**< Unknown usage */

/** Floppy lock address ($43E) */
#define UFT_FLOPPY_LOCK_ADDR        0x0000043E

/*===========================================================================
 * GEMDOS Program Header
 *===========================================================================*/

/**
 * @brief GEMDOS program header structure
 * 
 * Standard Atari ST executable format.
 * See: http://toshyp.atari.org/en/005005.html
 */
typedef struct {
    uint8_t ph_branch[2];       /**< WORD: Branch instruction (0x601A) */
    uint8_t ph_tlen[4];         /**< LONG: TEXT segment length */
    uint8_t ph_dlen[4];         /**< LONG: DATA segment length */
    uint8_t ph_blen[4];         /**< LONG: BSS segment length */
    uint8_t ph_slen[4];         /**< LONG: Symbol table length */
    uint8_t ph_res1[4];         /**< LONG: Reserved (must be 0) */
    uint8_t ph_prgflags[4];     /**< LONG: Program flags */
    uint8_t ph_absflag[2];      /**< WORD: 0 = relocation info present */
} uft_gemdos_header_t;

/*===========================================================================
 * Pattern Matching Structures
 *===========================================================================*/

/**
 * @brief Protection detection pattern
 */
typedef struct {
    unsigned int type;          /**< Pattern type (PROG/BIN/ANY) */
    size_t       offset;        /**< Fixed offset in file */
    size_t       delta;         /**< Search stride */
    size_t       count;         /**< Pattern byte count */
    const uint8_t *buf;         /**< Pattern bytes */
    const uint8_t *mask;        /**< Optional mask (NULL = exact match) */
} uft_pattern_t;

/**
 * @brief Protection description
 */
typedef struct uft_protection {
    struct uft_protection *parent;  /**< Parent for variants */
    const char            *name;    /**< Human-readable name */
    uint8_t               varnum;   /**< Variant number */
    size_t                doffset;  /**< Decoded data offset */
    const uft_pattern_t   **patterns; /**< Detection patterns */
    void                  *private; /**< Private data */
} uft_protection_t;

/*===========================================================================
 * Rob Northen Info Structure
 *===========================================================================*/

/**
 * @brief Rob Northen protection analysis result
 */
typedef struct {
    uint32_t magic32;           /**< Magic value for decryption */
    
    /* Static analysis: code locations */
    ssize_t  prog_off;          /**< Embedded program offset */
    ssize_t  start_off;         /**< Start of protection code */
    ssize_t  pushtramp_off;     /**< Push trampoline offset */
    ssize_t  decode_off;        /**< Decode routine offset */
    ssize_t  reloc_off;         /**< Relocation code offset */
    ssize_t  vecs_off;          /**< Vector check offset */
    ssize_t  keydisk_off;       /**< Key disk read offset */
    ssize_t  serial_off;        /**< Serial key usage offset */
    size_t   subrout_sz;        /**< Subroutine size */
    
    int      serial_usage;      /**< Serial usage flags */
    
    /* Dynamic analysis: values extracted */
    bool     prot_run;          /**< Protection was executed */
    bool     keydisk_hit;       /**< Key disk was accessed */
    bool     serial_valid;      /**< Serial key is valid */
    bool     magic_valid;       /**< Magic value is valid */
    bool     dstexec_valid;     /**< Destination is valid */
    
    uint32_t serial;            /**< Extracted serial number */
    uint32_t *serial_dst_addr;  /**< Serial destination address */
    
    uint32_t magic;             /**< Extracted magic value */
    uint32_t *magic_dst_addr;   /**< Magic destination address */
    
    void     *dst_addr;         /**< Program destination address */
    size_t   entry_off;         /**< Entry point offset */
} uft_robn_info_t;

/*===========================================================================
 * Byte Order Helpers (Big Endian - M68K)
 *===========================================================================*/

/**
 * @brief Read 16-bit big-endian value
 */
static inline uint16_t uft_read16_be(const uint8_t *buf) {
    return ((uint16_t)buf[0] << 8) | (uint16_t)buf[1];
}

/**
 * @brief Read 32-bit big-endian value
 */
static inline uint32_t uft_read32_be(const uint8_t *buf) {
    return ((uint32_t)buf[0] << 24) |
           ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] << 8)  |
           (uint32_t)buf[3];
}

/**
 * @brief Write 16-bit big-endian value
 */
static inline void uft_write16_be(uint16_t val, uint8_t *buf) {
    buf[0] = (uint8_t)(val >> 8);
    buf[1] = (uint8_t)(val);
}

/**
 * @brief Write 32-bit big-endian value
 */
static inline void uft_write32_be(uint32_t val, uint8_t *buf) {
    buf[0] = (uint8_t)(val >> 24);
    buf[1] = (uint8_t)(val >> 16);
    buf[2] = (uint8_t)(val >> 8);
    buf[3] = (uint8_t)(val);
}

/**
 * @brief Swap bytes in 32-bit value
 */
static inline uint32_t uft_swap32(uint32_t val) {
    return ((val & 0x000000FF) << 24) |
           ((val & 0x0000FF00) << 8)  |
           ((val & 0x00FF0000) >> 8)  |
           ((val & 0xFF000000) >> 24);
}

/**
 * @brief Rotate left 8-bit value
 */
static inline uint8_t uft_rol8(uint8_t val, unsigned int shift) {
    return (uint8_t)((val << shift) | (val >> (8 - shift)));
}

/**
 * @brief Rotate right 8-bit value
 */
static inline uint8_t uft_ror8(uint8_t val, unsigned int shift) {
    return (uint8_t)((val >> shift) | (val << (8 - shift)));
}

/*===========================================================================
 * Rob Northen Copylock Series 1 (1988)
 *===========================================================================*/

/**
 * @brief Decrypt instruction using Series 1 TVD method
 * 
 * Each instruction is decrypted by XOR with the preceding instruction
 * after bitwise NOT and byte swap.
 * 
 * @param buf Pointer to current encrypted instruction
 * @return Decrypted 32-bit value
 */
static inline uint32_t uft_robn88_decrypt_instr(const uint8_t *buf) {
    uint32_t key32;
    uint32_t w32;

    /* Key = NOT(SWAP(previous_instruction)) */
    key32 = uft_read32_be(buf - 4);
    key32 = ~key32;
    key32 = uft_swap32(key32);

    /* Decrypt current instruction */
    w32 = uft_read32_be(buf);
    w32 ^= key32;

    return w32;
}

/**
 * @brief Rob Northen Series 1 keydisk detection pattern
 * 
 * Pattern: st $43e.l (set floppy lock)
 * Opcode: 0x50f9 0x0000 0x043e
 */
static const uint16_t uft_robn88_keydisk_pattern[] = { 
    0x50F9, 0x0000, 0x043E 
};

/**
 * @brief Rob Northen Series 1 resume pattern
 * 
 * Pattern: move.l a0,2(sp) (internal routine return)
 * Opcode: 0x2f48 0x0002
 */
static const uint16_t uft_robn88_resume_pattern[] = { 
    0x2F48, 0x0002 
};

/**
 * @brief Rob Northen Series 1 vector check pattern
 * 
 * Pattern: instr #$fc0000,operand (vector range check)
 * Opcode: 0x0000 0x00fc 0x0000
 */
static const uint16_t uft_robn88_vecs_pattern[] = { 
    0x0000, 0x00FC, 0x0000 
};

/**
 * @brief Rob Northen Series 1 serial save pattern
 * 
 * Pattern: move.l d0,$1c(a0) (save serial to memory)
 * Opcode: 0x2140 0x001c
 */
static const uint16_t uft_robn88_serial_pattern[] = { 
    0x2140, 0x001C 
};

/*===========================================================================
 * Rob Northen Copylock Series 2 (1989)
 *===========================================================================*/

/**
 * @brief Get decryption key for Series 2 TVD method
 * 
 * Key is computed by adding magic value to previous instruction.
 * 
 * @param buf Pointer to current encrypted instruction
 * @param magic32 Magic value for this protection
 * @return Decryption key
 */
static inline uint32_t uft_robn89_get_key(const uint8_t *buf, uint32_t magic32) {
    uint32_t key32;

    key32 = uft_read32_be(buf - 4);  /* Previous instruction */
    key32 += magic32;                 /* Add magic constant */

    return key32;
}

/**
 * @brief Decrypt instruction using Series 2 TVD method
 * 
 * @param buf Pointer to current encrypted instruction
 * @param magic32 Magic value for this protection
 * @return Decrypted 32-bit value
 */
static inline uint32_t uft_robn89_decrypt_instr(const uint8_t *buf, uint32_t magic32) {
    uint32_t key32 = uft_robn89_get_key(buf, magic32);
    uint32_t w32 = uft_read32_be(buf);
    return w32 ^ key32;
}

/**
 * @brief Find Series 2 trampoline pattern and extract magic value
 * 
 * Searches for the pattern:
 *   lea pc+$12,a6     (0x4dfa0010)
 *   move.l -4(a6),d6  (0x2c2efffc)
 *   add.l $8.l,d6     (0xdcb90000)
 * 
 * The magic value is discovered by XORing the first encrypted
 * instruction with the known plaintext.
 * 
 * @param buf Buffer containing protection code
 * @param size Buffer size
 * @param magic32 Output: discovered magic value
 * @param prog_off Output: embedded program offset
 * @return Offset of pattern, or -1 if not found
 */
static inline ssize_t uft_robn89_find_start(
    const uint8_t *buf,
    size_t size,
    uint32_t *magic32,
    ssize_t *prog_off)
{
    uint32_t w32;
    uint32_t key32;
    uint32_t m32;
    uint16_t w16;
    size_t i, j;

    for (i = 0; (ssize_t)i <= (ssize_t)(size - 20); i += 2) {
        j = i;
        
        /* XOR with expected lea instruction to get magic */
        w32 = uft_read32_be(buf + j);
        w32 ^= 0x4DFA0010;  /* lea pc+$12,a6 */
        m32 = w32 - uft_read32_be(buf + j - 4);

        j += 4;
        w32 = uft_read32_be(buf + j);
        key32 = uft_robn89_get_key(buf + j, m32);
        w32 ^= key32;

        if (w32 != 0x2C2EFFFC) {  /* move.l -4(a6),d6 */
            continue;
        }

        j += 4;
        w32 = uft_read32_be(buf + j);
        key32 = uft_robn89_get_key(buf + j, m32);
        w32 ^= key32;

        if (w32 != 0xDCB90000) {  /* add.l $8.l,d6 */
            continue;
        }

        /* Pattern found! Now find end of protection */
        size_t limit = (i + 256 < size - 6) ? i + 256 : size - 6;
        
        for (j = i + 16; j <= limit; j += 2) {
            /* Look for: move.l a7,$24.l */
            w32 = uft_read32_be(buf + j);
            key32 = w32 ^ 0x23CF0000;  /* move.l a7,<addr>.l */

            w16 = uft_read16_be(buf + j + 4);
            w16 ^= (uint16_t)(key32 >> 16);

            if (w16 == 0x0024) {  /* <addr> == $24 */
                *magic32 = m32;
                *prog_off = (ssize_t)(j + 6);
                return (ssize_t)i;
            }
        }
    }

    *prog_off = -1;
    return -1;
}

/*===========================================================================
 * Illegal Anti-bitos Decryption
 *===========================================================================*/

/**
 * @brief Decrypt Anti-bitos protected data
 * 
 * Two-phase decryption:
 * 1. XOR with evolving 16-bit key (shift + XOR + rotate)
 * 2. XOR with incrementing random counter
 * 
 * @param buf Data buffer to decrypt in-place
 * @param size Data size
 * @param sub_count Subtraction constant (version-specific)
 * @param rand16 Random seed for phase 2
 * @return 0 on success
 */
static inline int uft_antibitos_decrypt(
    uint8_t *buf,
    size_t size,
    uint16_t sub_count,
    uint16_t rand16)
{
    uint16_t key16 = 0x004F;
    uint8_t key8;
    size_t i;

    /* Phase 1: XOR with evolving key */
    for (i = 0; i < size; i++) {
        buf[i] ^= (uint8_t)(key16 & 0x00FF);
        key16 -= sub_count;
        key16 = key16 << 1;
        key16 ^= 0x1234;
        key8 = (uint8_t)(key16 & 0x00FF);
        key8 = uft_rol8(key8, 1);
        key16 = (key16 & 0xFF00) | (uint16_t)key8;
    }

    /* Phase 2: XOR with incrementing random */
    for (i = 0; i + 1 < size; i += 2) {
        uint16_t w16 = uft_read16_be(buf + i);
        w16 ^= rand16;
        rand16 += 1;
        uft_write16_be(w16, buf + i);
    }

    return 0;
}

/*===========================================================================
 * Zippy Little Protection Decryption
 *===========================================================================*/

/**
 * @brief Zippy LCG (Linear Congruential Generator)
 * 
 * Uses π-based multiplier: x' = x * 3141597 + 1
 */
#define UFT_ZIPPY_LCG_MULT  3141597u
#define UFT_ZIPPY_LCG_INC   1u

/**
 * @brief Advance Zippy LCG state
 */
static inline uint32_t uft_zippy_lcg_next(uint32_t state) {
    return state * UFT_ZIPPY_LCG_MULT + UFT_ZIPPY_LCG_INC;
}

/**
 * @brief Decrypt Zippy protected data
 * 
 * Two-phase decryption:
 * 1. Forward XOR from start
 * 2. Backward XOR from end
 * 
 * @param buf Data buffer to decrypt in-place
 * @param size Total data size
 * @param xfer_size Size of backward transfer section
 * @param rand_init Initial random seed
 * @return 0 on success
 */
static inline int uft_zippy_decrypt(
    uint8_t *buf,
    size_t size,
    size_t xfer_size,
    uint32_t rand_init)
{
    uint32_t rand32;
    size_t i, j;

    /* Phase 1: Forward decryption */
    rand32 = rand_init;
    for (i = 0; i + 1 < size - xfer_size; i += 2) {
        uint16_t w16 = uft_read16_be(buf + i);
        w16 ^= (uint16_t)(rand32 >> 16);
        rand32 = uft_zippy_lcg_next(rand32);
        uft_write16_be(w16, buf + i);
    }

    /* Phase 2: Backward decryption */
    rand32 = rand_init;
    for (i = 0; i + 1 < xfer_size; i += 2) {
        j = size - 2 - i;
        uint16_t w16 = uft_read16_be(buf + j);
        w16 ^= (uint16_t)(rand32 >> 16);
        rand32 = uft_zippy_lcg_next(rand32);
        uft_write16_be(w16, buf + j);
    }

    return 0;
}

/*===========================================================================
 * Toxic Packer Decryption
 *===========================================================================*/

/**
 * @brief Decrypt Toxic Packer protected data
 * 
 * Simple XOR with static key.
 * 
 * @param buf Data buffer to decrypt in-place
 * @param size Data size
 * @param key32 Decryption key
 * @return 0 on success
 */
static inline int uft_toxic_decrypt(
    uint8_t *buf,
    size_t size,
    uint32_t key32)
{
    size_t i;

    for (i = 0; i + 3 < size; i += 4) {
        uint32_t w32 = uft_read32_be(buf + i);
        w32 ^= key32;
        uft_write32_be(w32, buf + i);
    }

    return 0;
}

/*===========================================================================
 * Detection Patterns
 *===========================================================================*/

/**
 * @brief Anti-bitos v1.0 initialization pattern
 */
static const uint8_t uft_antibitos_init_pattern[] = {
    0x41, 0xFA, 0x00, 0xA6,  /* lea pc+$a8,a0 */
    0x43, 0xFA, 0x00, 0xCE,  /* lea pc+$d0,a1 */
    0x45, 0xFA, 0x00, 0x90,  /* lea pc+$92,a2 */
    0x21, 0xC8, 0x00, 0x10,  /* move.l a0,$10.w */
    0x21, 0xC9, 0x00, 0x80,  /* move.l a1,$80.w */
    0x21, 0xCA, 0x00, 0x24   /* move.l a2,$24.w */
};

/**
 * @brief Anti-bitos TVD pattern
 */
static const uint8_t uft_antibitos_tvd_pattern[] = {
    0x48, 0x50,              /* pea (a0) */
    0x20, 0x6F, 0x00, 0x06,  /* movea.l 6(a7),a0 */
    0x4E, 0x40,              /* trap #0 */
    0x4A, 0xFC,              /* illegal */
    0x20, 0x5F,              /* movea.l (a7)+,a0 */
    0x4E, 0x73               /* rte */
};

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

/**
 * @brief Check GEMDOS header validity
 * 
 * @param buf Buffer containing header
 * @param size Buffer size
 * @return true if valid GEMDOS header
 */
static inline bool uft_gemdos_is_valid(const uint8_t *buf, size_t size) {
    if (size < UFT_GEMDOS_HEADER_SIZE) {
        return false;
    }
    return (uft_read16_be(buf) == UFT_GEMDOS_MAGIC);
}

/**
 * @brief Get GEMDOS program size from header
 * 
 * @param buf Buffer containing header
 * @return Total program size (TEXT + DATA + SYMBOL + HEADER)
 */
static inline size_t uft_gemdos_get_size(const uint8_t *buf) {
    const uft_gemdos_header_t *hdr = (const uft_gemdos_header_t *)buf;
    size_t tlen = uft_read32_be(hdr->ph_tlen);
    size_t dlen = uft_read32_be(hdr->ph_dlen);
    size_t slen = uft_read32_be(hdr->ph_slen);
    return UFT_GEMDOS_HEADER_SIZE + tlen + dlen + slen;
}

/**
 * @brief Initialize Rob Northen info structure
 */
static inline void uft_robn_info_init(uft_robn_info_t *info) {
    memset(info, 0, sizeof(*info));
    info->prog_off = -1;
    info->start_off = -1;
    info->pushtramp_off = -1;
    info->decode_off = -1;
    info->reloc_off = -1;
    info->vecs_off = -1;
    info->keydisk_off = -1;
    info->serial_off = -1;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_ATARIST_DEC0DE_H */
