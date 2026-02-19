/**
 * @file uft_atarist_copylock.h
 * @brief Atari ST CopyLock Protection Detection & Analysis
 * 
 * Based on dec0de by Orion ^ The Replicants (Nov 2017)
 * https://github.com/orionfuzion/dec0de
 * 
 * Supports:
 * - CopyLock Series 1 (1988) - 5 variants (a-e)
 * - CopyLock Series 2 (1989) - 6 variants (a-f)
 * - TVD (Trace Vector Decoding) analysis
 * 
 * @version 1.0.0
 * @date 2026-01-04
 * @author UFT Team
 * 
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_ATARIST_COPYLOCK_H
#define UFT_ATARIST_COPYLOCK_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

/** CopyLock series identifiers */
typedef enum {
    UFT_COPYLOCK_SERIES_UNKNOWN = 0,
    UFT_COPYLOCK_SERIES_1_1988,      /**< Original 1988 series */
    UFT_COPYLOCK_SERIES_2_1989       /**< Enhanced 1989 series */
} uft_copylock_series_t;

/** CopyLock variant within series */
typedef enum {
    UFT_COPYLOCK_VARIANT_UNKNOWN = 0,
    UFT_COPYLOCK_VARIANT_A,
    UFT_COPYLOCK_VARIANT_B,
    UFT_COPYLOCK_VARIANT_C,
    UFT_COPYLOCK_VARIANT_D,
    UFT_COPYLOCK_VARIANT_E,
    UFT_COPYLOCK_VARIANT_F     /**< Series 2 only */
} uft_copylock_variant_t;

/** Protection type */
typedef enum {
    UFT_COPYLOCK_TYPE_UNKNOWN = 0,
    UFT_COPYLOCK_TYPE_INTERNAL,      /**< Encryption returns serial to caller */
    UFT_COPYLOCK_TYPE_WRAPPER        /**< Decrypts wrapped program using serial */
} uft_copylock_type_t;

/** Serial key usage flags */
#define UFT_SERIAL_USAGE_NONE          0x00
#define UFT_SERIAL_USAGE_DECODE_PROG   0x01  /**< Used to decrypt program */
#define UFT_SERIAL_USAGE_RETURN        0x02  /**< Returned to caller */
#define UFT_SERIAL_USAGE_SAVE_MEM      0x04  /**< Saved in memory */
#define UFT_SERIAL_USAGE_MAGIC_MEM     0x08  /**< Converted to magic value */
#define UFT_SERIAL_USAGE_EOR_MEM       0x10  /**< XOR-ed with memory */
#define UFT_SERIAL_USAGE_OTHER_MEM     0x20  /**< External memory decoding */
#define UFT_SERIAL_USAGE_UNKNOWN       0x40

/*===========================================================================
 * Series 1 (1988) Detection Patterns
 *===========================================================================*/

/**
 * @brief Series 1 encryption scheme
 * 
 * Internal type: each instruction is XOR-ed with ~SWAP32(previous_instruction)
 * 
 * key32 = read32(buf - 4)
 * key32 = ~key32
 * key32 = SWAP32(key32)  // swap high/low words
 * decoded = read32(buf) ^ key32
 */
static inline uint32_t uft_copylock88_decode_instr(const uint8_t *buf) {
    uint32_t key32, w32;
    
    /* Read previous instruction (big-endian) */
    key32 = ((uint32_t)buf[-4] << 24) | ((uint32_t)buf[-3] << 16) |
            ((uint32_t)buf[-2] << 8)  | ((uint32_t)buf[-1]);
    
    /* Invert and swap words */
    key32 = ~key32;
    key32 = ((key32 & 0xFFFF0000u) >> 16) | ((key32 & 0x0000FFFFu) << 16);
    
    /* Read and decrypt current instruction */
    w32 = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
          ((uint32_t)buf[2] << 8)  | ((uint32_t)buf[3]);
    
    return w32 ^ key32;
}

/**
 * @brief Series 1 detection pattern: BRA.S instruction
 * Pattern: 0x60 0x?? (BRA.S offset)
 */
static const uint8_t uft_copylock88_bra_pattern[] = { 0x60, 0x72 };
static const uint8_t uft_copylock88_bra_mask[]    = { 0xFF, 0x00 };

/**
 * @brief Series 1 keydisk pattern: ST $43E.L
 * Pattern: 0x50F9 0x0000 0x043E
 */
static const uint8_t uft_copylock88_keydisk_pattern[] = {
    0x50, 0xF9, 0x00, 0x00, 0x04, 0x3E
};

/**
 * @brief Series 1 serial save pattern: MOVE.L D0,$1C(A0)
 * Pattern: 0x2140 0x001C
 */
static const uint8_t uft_copylock88_serial_pattern[] = {
    0x21, 0x40, 0x00, 0x1C
};

/*===========================================================================
 * Series 2 (1989) Detection Patterns
 *===========================================================================*/

/**
 * @brief Series 2 encryption scheme
 * 
 * Uses two different TVD (Trace Vector Decoding) routines:
 * 1. Complex method (start of protection) - anti-debugger
 * 2. Simple method (key disk access) - ADD-based
 * 
 * Simple method:
 * key32 = read32(buf - 4) + magic32
 * decoded = read32(buf) ^ key32
 */
static inline uint32_t uft_copylock89_decode_instr(const uint8_t *buf, 
                                                    uint32_t magic32) {
    uint32_t key32, w32;
    
    /* Read previous instruction (big-endian) */
    key32 = ((uint32_t)buf[-4] << 24) | ((uint32_t)buf[-3] << 16) |
            ((uint32_t)buf[-2] << 8)  | ((uint32_t)buf[-1]);
    
    /* Add magic value */
    key32 += magic32;
    
    /* Read and decrypt current instruction */
    w32 = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
          ((uint32_t)buf[2] << 8)  | ((uint32_t)buf[3]);
    
    return w32 ^ key32;
}

/**
 * @brief Series 2 trampoline search pattern
 * 
 * The end of protection installs a trampoline at $24:
 * LEA PC+$12,A6     (0x4DFA0010)
 * MOVE.L -4(A6),D6  (0x2C2EFFFC)
 * ADD.L $8.L,D6     (0xDCB90000 0x0008)
 */
static const uint32_t uft_copylock89_trampoline_lea   = 0x4DFA0010u;
static const uint32_t uft_copylock89_trampoline_move  = 0x2C2EFFFCu;
static const uint32_t uft_copylock89_trampoline_add   = 0xDCB90000u;

/**
 * @brief Series 2 init pattern variant 1
 * MOVEM.L D0-A7,-(A7)  PEA PC+$1C  MOVE.L (A7)+,$10 ...
 */
static const uint8_t uft_copylock89_init1_pattern[] = {
    0x48, 0xE7, 0xFF, 0xFF,  /* movem.l d0-a7,-(a7) */
    0x48, 0x7A, 0x00, 0x1A,  /* pea pc+$1c */
    0x23, 0xDF, 0x00, 0x00, 0x00, 0x10  /* move.l (a7)+,$10 */
};

/**
 * @brief Series 2 init pattern variant 2
 * MOVEM.L D0-A7,-(A7)  PEA PC+$1A  MOVE.L (A7)+,$10.W ...
 */
static const uint8_t uft_copylock89_init2_pattern[] = {
    0x48, 0xE7, 0xFF, 0xFF,  /* movem.l d0-a7,-(a7) */
    0x48, 0x7A, 0x00, 0x18,  /* pea pc+$1a */
    0x21, 0xDF, 0x00, 0x10   /* move.l (a7)+,$10.w */
};

/*===========================================================================
 * Detection Result
 *===========================================================================*/

/**
 * @brief CopyLock detection result
 */
typedef struct {
    /* Detection status */
    bool                    detected;
    uft_copylock_series_t   series;
    uft_copylock_variant_t  variant;
    uft_copylock_type_t     type;
    
    /* Encryption parameters */
    uint32_t                magic32;     /**< Series 2 magic value */
    
    /* Offsets in protection code */
    int32_t                 start_off;   /**< Start of encrypted section */
    int32_t                 keydisk_off; /**< Key disk access offset */
    int32_t                 serial_off;  /**< Serial key handling offset */
    int32_t                 decode_off;  /**< Program decryption offset */
    int32_t                 prog_off;    /**< Wrapped program offset */
    int32_t                 vecs_off;    /**< Vectors check offset */
    
    /* Serial key info */
    int                     serial_usage;
    bool                    serial_valid;
    uint32_t                serial;
    uint32_t                serial_dst_addr;
    
    /* Wrapped program info (wrapper type) */
    bool                    has_wrapped_prog;
    void                   *dst_addr;
    size_t                  entry_off;
    size_t                  prog_len;
    size_t                  zeroes_len;
    
    /* Diagnostics */
    char                    name[64];
    char                    info[256];
} uft_copylock_st_result_t;

/*===========================================================================
 * Detection Functions
 *===========================================================================*/

/**
 * @brief Initialize detection result
 */
static inline void uft_copylock_st_init_result(uft_copylock_st_result_t *result) {
    memset(result, 0, sizeof(*result));
    result->start_off = -1;
    result->keydisk_off = -1;
    result->serial_off = -1;
    result->decode_off = -1;
    result->prog_off = -1;
    result->vecs_off = -1;
    result->serial_usage = UFT_SERIAL_USAGE_NONE;
}

/**
 * @brief Search for byte pattern with mask
 * 
 * @param data Data buffer to search
 * @param data_len Length of data buffer
 * @param pattern Pattern to search for
 * @param mask Mask for pattern (NULL for exact match)
 * @param pattern_len Length of pattern
 * @param start_offset Starting offset in buffer
 * @return Offset if found, -1 if not found
 */
static inline int32_t uft_copylock_find_pattern(
    const uint8_t *data, size_t data_len,
    const uint8_t *pattern, const uint8_t *mask,
    size_t pattern_len, size_t start_offset)
{
    if (data_len < pattern_len + start_offset) {
        return -1;
    }
    
    for (size_t i = start_offset; i <= data_len - pattern_len; i++) {
        bool match = true;
        for (size_t j = 0; j < pattern_len && match; j++) {
            uint8_t m = mask ? mask[j] : 0xFF;
            if ((data[i + j] & m) != (pattern[j] & m)) {
                match = false;
            }
        }
        if (match) {
            return (int32_t)i;
        }
    }
    return -1;
}

/**
 * @brief Detect Series 2 magic value from trampoline pattern
 * 
 * @param data Data buffer (protection code)
 * @param data_len Length of buffer
 * @param[out] magic32 Discovered magic value
 * @param[out] prog_offset Offset of wrapped program
 * @return Start offset if found, -1 if not found
 */
static inline int32_t uft_copylock89_find_trampoline(
    const uint8_t *data, size_t data_len,
    uint32_t *magic32, int32_t *prog_offset)
{
    uint32_t key32, w32;
    
    for (size_t i = 0; i + 40 <= data_len; i += 2) {
        /* Read potential LEA instruction */
        w32 = ((uint32_t)data[i] << 24) | ((uint32_t)data[i+1] << 16) |
              ((uint32_t)data[i+2] << 8) | ((uint32_t)data[i+3]);
        
        /* XOR with expected LEA to get magic candidate */
        uint32_t magic_candidate = w32 ^ uft_copylock89_trampoline_lea;
        
        /* Get key for previous instruction */
        if (i < 4) continue;
        key32 = ((uint32_t)data[i-4] << 24) | ((uint32_t)data[i-3] << 16) |
                ((uint32_t)data[i-2] << 8) | ((uint32_t)data[i-1]);
        magic_candidate -= key32;
        
        /* Verify second instruction (MOVE.L -4(A6),D6) */
        size_t j = i + 4;
        key32 = ((uint32_t)data[j-4] << 24) | ((uint32_t)data[j-3] << 16) |
                ((uint32_t)data[j-2] << 8) | ((uint32_t)data[j-1]);
        key32 += magic_candidate;
        
        w32 = ((uint32_t)data[j] << 24) | ((uint32_t)data[j+1] << 16) |
              ((uint32_t)data[j+2] << 8) | ((uint32_t)data[j+3]);
        
        if ((w32 ^ key32) != uft_copylock89_trampoline_move) {
            continue;
        }
        
        /* Verify third instruction (ADD.L $8.L,D6) */
        j += 4;
        key32 = ((uint32_t)data[j-4] << 24) | ((uint32_t)data[j-3] << 16) |
                ((uint32_t)data[j-2] << 8) | ((uint32_t)data[j-1]);
        key32 += magic_candidate;
        
        w32 = ((uint32_t)data[j] << 24) | ((uint32_t)data[j+1] << 16) |
              ((uint32_t)data[j+2] << 8) | ((uint32_t)data[j+3]);
        
        if ((w32 ^ key32) != uft_copylock89_trampoline_add) {
            continue;
        }
        
        /* Pattern found! */
        *magic32 = magic_candidate;
        
        /* Find MOVE.L A7,$24.L to determine program offset */
        for (j = i + 16; j + 6 <= data_len && j < i + 256; j += 2) {
            w32 = ((uint32_t)data[j] << 24) | ((uint32_t)data[j+1] << 16) |
                  ((uint32_t)data[j+2] << 8) | ((uint32_t)data[j+3]);
            key32 = w32 ^ 0x23CF0000u;  /* MOVE.L A7,<addr>.L */
            
            uint16_t w16 = ((uint16_t)data[j+4] << 8) | data[j+5];
            w16 ^= (uint16_t)(key32 >> 16);
            
            if (w16 == 0x0024) {  /* <addr> == $24 */
                *prog_offset = (int32_t)(j + 6);
                return (int32_t)i;
            }
        }
        
        *prog_offset = -1;
        return (int32_t)i;
    }
    
    *prog_offset = -1;
    return -1;
}

/**
 * @brief Detect CopyLock protection on Atari ST program
 * 
 * @param data Program data (GEMDOS format or raw binary)
 * @param data_len Length of data
 * @param result Output: detection result
 * @return true if CopyLock detected
 */
static inline bool uft_copylock_st_detect(
    const uint8_t *data, size_t data_len,
    uft_copylock_st_result_t *result)
{
    int32_t off;
    
    uft_copylock_st_init_result(result);
    
    if (data_len < 100) {
        return false;
    }
    
    /* Try Series 2 (1989) first - more complex pattern */
    off = uft_copylock_find_pattern(data, data_len,
                                     uft_copylock89_init1_pattern, NULL,
                                     sizeof(uft_copylock89_init1_pattern), 0);
    if (off >= 0) {
        result->detected = true;
        result->series = UFT_COPYLOCK_SERIES_2_1989;
        result->variant = UFT_COPYLOCK_VARIANT_A;
        
        /* Find magic value */
        int32_t prog_off;
        result->start_off = uft_copylock89_find_trampoline(
            data, data_len, &result->magic32, &prog_off);
        
        if (result->start_off >= 0) {
            result->prog_off = prog_off;
        }
        
        snprintf(result->name, sizeof(result->name),
                 "Copylock Protection System series 2 (1989) by Rob Northen");
        return true;
    }
    
    /* Try Series 2 variant 2 */
    off = uft_copylock_find_pattern(data, data_len,
                                     uft_copylock89_init2_pattern, NULL,
                                     sizeof(uft_copylock89_init2_pattern), 0);
    if (off >= 0) {
        result->detected = true;
        result->series = UFT_COPYLOCK_SERIES_2_1989;
        result->variant = UFT_COPYLOCK_VARIANT_B;
        
        int32_t prog_off;
        result->start_off = uft_copylock89_find_trampoline(
            data, data_len, &result->magic32, &prog_off);
        
        if (result->start_off >= 0) {
            result->prog_off = prog_off;
        }
        
        snprintf(result->name, sizeof(result->name),
                 "Copylock Protection System series 2 (1989) variant B");
        return true;
    }
    
    /* Try Series 1 (1988) */
    off = uft_copylock_find_pattern(data, data_len,
                                     uft_copylock88_bra_pattern,
                                     uft_copylock88_bra_mask,
                                     sizeof(uft_copylock88_bra_pattern), 0);
    if (off >= 0) {
        /* Verify with keydisk pattern */
        int32_t keydisk_off = uft_copylock_find_pattern(
            data, data_len,
            uft_copylock88_keydisk_pattern, NULL,
            sizeof(uft_copylock88_keydisk_pattern), (size_t)off);
        
        if (keydisk_off >= 0) {
            result->detected = true;
            result->series = UFT_COPYLOCK_SERIES_1_1988;
            result->variant = UFT_COPYLOCK_VARIANT_A;
            result->keydisk_off = keydisk_off;
            result->type = UFT_COPYLOCK_TYPE_INTERNAL;
            
            /* Find serial pattern */
            result->serial_off = uft_copylock_find_pattern(
                data, data_len,
                uft_copylock88_serial_pattern, NULL,
                sizeof(uft_copylock88_serial_pattern), (size_t)off);
            
            if (result->serial_off >= 0) {
                result->serial_usage = UFT_SERIAL_USAGE_RETURN | 
                                       UFT_SERIAL_USAGE_SAVE_MEM;
                result->serial_dst_addr = 0x24;  /* Usually saved at $24 */
            }
            
            snprintf(result->name, sizeof(result->name),
                     "Copylock Protection System series 1 (1988) by Rob Northen");
            return true;
        }
    }
    
    return false;
}

/**
 * @brief Get human-readable description of serial usage
 */
static inline const char* uft_copylock_serial_usage_str(int usage) {
    static char buf[256];
    buf[0] = '\0';
    
    if (usage == UFT_SERIAL_USAGE_NONE) {
        return "None";
    }
    
    if (usage & UFT_SERIAL_USAGE_DECODE_PROG) strncat(buf, "Program decoding, ", sizeof(buf) - strlen(buf) - 1);
    if (usage & UFT_SERIAL_USAGE_RETURN) strncat(buf, "Returned to caller, ", sizeof(buf) - strlen(buf) - 1);
    if (usage & UFT_SERIAL_USAGE_SAVE_MEM) strncat(buf, "Saved in memory, ", sizeof(buf) - strlen(buf) - 1);
    if (usage & UFT_SERIAL_USAGE_MAGIC_MEM) strncat(buf, "Magic value, ", sizeof(buf) - strlen(buf) - 1);
    if (usage & UFT_SERIAL_USAGE_EOR_MEM) strncat(buf, "XOR-ed in memory, ", sizeof(buf) - strlen(buf) - 1);
    if (usage & UFT_SERIAL_USAGE_OTHER_MEM) strncat(buf, "External decode, ", sizeof(buf) - strlen(buf) - 1);
    if (usage & UFT_SERIAL_USAGE_UNKNOWN) strncat(buf, "Unknown, ", sizeof(buf) - strlen(buf) - 1);
    
    /* Remove trailing ", " */
    size_t len = strlen(buf);
    if (len >= 2) buf[len - 2] = '\0';
    
    return buf;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_ATARIST_COPYLOCK_H */
