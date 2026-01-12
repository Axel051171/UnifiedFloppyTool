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

/* Parse raw PRG bytes into a view. */
int uft_c64_prg_parse(const uint8_t *buf, size_t len, uft_c64_prg_view_t *out);

/* Heuristic: does payload look like BASIC line chain at load address? */
uft_c64_prg_kind_t uft_c64_prg_classify(const uft_c64_prg_view_t *prg);

/* BASIC v2 token listing:
 * - decodes tokenized BASIC from PRG payload into ASCII text.
 * - returns bytes written (excluding terminating NUL); output is always NUL-terminated if cap>0.
 */
size_t uft_c64_basic_list(const uft_c64_prg_view_t *prg, char *out, size_t out_cap);

/**
 * @brief Get token name for BASIC v2 token
 * @param token Token value (0x80-0xCB)
 * @return Token name or NULL if not a valid token
 */
const char *uft_c64_basic_token_name(uint8_t token);

/* Utility: compute SHA-1 (small implementation) for forensics/reproducibility. */
void uft_sha1(const uint8_t *data, size_t len, uint8_t out20[20]);

/* Utility: format SHA-1 hash as hex string */
size_t uft_sha1_format(const uint8_t hash[20], char *out, size_t out_cap);

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
 * @brief Find SYS call in BASIC program
 * @param prg PRG view
 * @param sys_addr Output SYS address
 * @return 1 if found, 0 otherwise
 */
int uft_c64_prg_find_sys(const uft_c64_prg_view_t *prg, uint16_t *sys_addr);

/**
 * @brief Analyze PRG file
 * @param buf Raw PRG data (with 2-byte load address)
 * @param len Length of data
 * @param out Output info structure
 * @return 0 on success, -1 on error
 */
int uft_c64_prg_analyze(const uint8_t *buf, size_t len, uft_c64_prg_info_t *out);

/**
 * @brief Get human-readable name for PRG kind
 */
const char *uft_c64_prg_kind_name(uft_c64_prg_kind_t kind);

#ifdef __cplusplus
}
#endif
