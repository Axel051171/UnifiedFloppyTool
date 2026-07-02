#pragma once
/*
 * uft_c64_prg.h — C64 PRG container parsing + BASIC v2 listing (C11)
 *
 * What it gives you (usable for UFT):
 * - PRG load address + payload view
 * - quick classification: BASIC-ish vs. machine code-ish
 * - BASIC v2 token decoder (enough for tooling / inspection)
 *
 * This is NOT a decompiler. It is a forensic-friendly extractor/decoder layer.
 */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct uft_c64_prg_view {
    uint16_t load_addr;
    const uint8_t *payload; /* points to bytes after the 2-byte load address */
    size_t payload_size;
} uft_c64_prg_view_t;

typedef enum uft_c64_prg_kind {
    UFT_C64_PRG_UNKNOWN = 0,
    UFT_C64_PRG_BASIC  = 1,
    UFT_C64_PRG_MACHINE= 2
} uft_c64_prg_kind_t;




/**
 * @brief Get token name for BASIC v2 token
 * @param token Token value (0x80-0xCB)
 * @return Token name or NULL if not a valid token
 */
const char *uft_c64_basic_token_name(uint8_t token);



/**
 * @brief Extended PRG analysis info
 */
typedef struct uft_c64_prg_info {
    uft_c64_prg_view_t view;
    uft_c64_prg_kind_t kind;
    uint8_t sha1[20];
    
    /* Address info */
    uint16_t end_addr;        /* End address (load_addr + size - 1) */
    uint16_t entry_point;     /* Entry point (SYS address or load address) */
    
    /* BASIC info (if kind == BASIC) */
    int basic_line_count;
    uint16_t first_line_num;
    uint16_t last_line_num;
    int has_sys_call;
    uint16_t sys_address;
} uft_c64_prg_info_t;



/**
 * @brief Get human-readable name for PRG kind
 */
const char *uft_c64_prg_kind_name(uft_c64_prg_kind_t kind);

#ifdef __cplusplus
}
#endif
