/**
 * @file uft_bootblock_scanner.c
 * @brief Amiga bootblock scanner implementation (M2 T2).
 *
 * Depends on the virus-catalog SSOT from M2 T1 via
 * uft_amiga_get_virus_db(). Read-only — no modification or
 * disinfection (see docs/XCOPY_INTEGRATION_TODO.md § Anti-Features).
 */

#include "uft/fs/uft_bootblock_scanner.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define DOS_TYPE_BASE  0x444F5300u  /* 'D''O''S''\0' */

/* Read 32 bits big-endian from an unaligned byte pointer. */
static inline uint32_t rd_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] <<  8) |  (uint32_t)p[3];
}

/* Classify a DOS type code (raw BE32) into an enum. */
static uft_bb_dos_type_t classify_dos_type(uint32_t raw) {
    if ((raw & 0xFFFFFF00u) != DOS_TYPE_BASE) return UFT_BB_DOS_UNKNOWN;
    uint8_t variant = (uint8_t)(raw & 0xFFu);
    switch (variant) {
    case 0: return UFT_BB_DOS_OFS;
    case 1: return UFT_BB_DOS_FFS;
    case 2: return UFT_BB_DOS_OFS_INTL;
    case 3: return UFT_BB_DOS_FFS_INTL;
    case 4: return UFT_BB_DOS_OFS_DC;
    case 5: return UFT_BB_DOS_FFS_DC;
    default: return UFT_BB_DOS_UNKNOWN;
    }
}

uint32_t uft_bb_compute_checksum(const uint8_t *bootblock) {
    if (bootblock == NULL) return 0;

    uint32_t sum = 0;
    for (size_t i = 0; i < UFT_BOOTBLOCK_SIZE; i += 4) {
        uint32_t word = rd_be32(bootblock + i);
        /* Stored checksum field at bytes 4..7 is treated as zero. */
        if (i == 4) word = 0;

        uint32_t prev = sum;
        sum += word;
        if (sum < prev) sum++;  /* add-with-carry-round */
    }
    return ~sum;
}

/* Does any non-zero byte appear in the buffer? */
static bool has_any_nonzero(const uint8_t *p, size_t len) {
    for (size_t i = 0; i < len; i++) if (p[i]) return true;
    return false;
}

/*
 * Cheap heuristic: bootblocks with real boot code usually contain
 * m68k opcodes (branches, moves, jumps) rather than ASCII or zeros.
 * Look for BRA.S (0x60xx), BSR.S (0x61xx), JMP (0x4EFx), RTS (0x4E75),
 * or LEA (0x4x79/0x4xF9) in the first 64 bytes. This is a soft hint,
 * NOT a safety check.
 */
static bool looks_like_m68k_code(const uint8_t *bootblock) {
    const size_t scan = 64;
    for (size_t i = 0; i + 1 < scan; i++) {
        uint8_t hi = bootblock[i];
        uint8_t lo = bootblock[i + 1];
        if (hi == 0x60 || hi == 0x61) return true;                     /* BRA/BSR */
        if (hi == 0x4E && (lo == 0x75 || (lo & 0xF0) == 0xF0))         /* RTS/JMP */
            return true;
        if ((hi & 0xF0) == 0x40 && (lo == 0x79 || lo == 0xF9))         /* LEA abs */
            return true;
    }
    return false;
}

/*
 * Match one signature entry against the bootblock.
 * Returns true + fills match_offset if a match is found.
 */
static bool match_virus_sig(const uint8_t *bootblock,
                             const uft_amiga_virus_sig_t *sig,
                             uint32_t *match_offset)
{
    if (sig->pattern_len == 0) return false;  /* PENDING — skip */
    if (sig->pattern == NULL || sig->pattern_mask == NULL) return false;
    if (sig->pattern_len > UFT_BOOTBLOCK_SIZE) return false;

    const size_t limit = UFT_BOOTBLOCK_SIZE - sig->pattern_len;
    for (size_t off = 0; off <= limit; off++) {
        bool all_match = true;
        for (size_t k = 0; k < sig->pattern_len; k++) {
            uint8_t want = sig->pattern[k];
            uint8_t mask = sig->pattern_mask[k];
            if (((bootblock[off + k] ^ want) & mask) != 0) {
                all_match = false;
                break;
            }
        }
        if (all_match) {
            if (match_offset) *match_offset = (uint32_t)off;
            return true;
        }
    }
    return false;
}

uft_error_t uft_bb_scan(const uint8_t *bootblock,
                         size_t len,
                         uft_bb_scan_result_t *result)
{
    if (bootblock == NULL || result == NULL) return UFT_ERR_INVALID_ARG;
    if (len != UFT_BOOTBLOCK_SIZE) return UFT_ERR_INVALID_ARG;

    memset(result, 0, sizeof(*result));

    /* DOS-type classification. */
    result->dos_type_raw = rd_be32(bootblock + 0);
    result->dos_type     = classify_dos_type(result->dos_type_raw);

    /* Checksum. */
    result->stored_checksum   = rd_be32(bootblock + 4);
    result->computed_checksum = uft_bb_compute_checksum(bootblock);
    result->checksum_ok       =
        (result->stored_checksum == result->computed_checksum);

    /* Root-block pointer (BE32). */
    result->root_block = rd_be32(bootblock + 8);

    /* Coarse structural signals. */
    result->all_zeros       = !has_any_nonzero(bootblock, UFT_BOOTBLOCK_SIZE);
    result->looks_like_code = looks_like_m68k_code(bootblock);

    /* Virus-DB matching. */
    size_t db_count = 0;
    const uft_amiga_virus_sig_t *db = uft_amiga_get_virus_db(&db_count);
    if (db == NULL) return UFT_OK;  /* DB unavailable — result still valid */

    for (size_t i = 0; i < db_count; i++) {
        if (result->match_count >= UFT_BB_MAX_MATCHES) break;
        uint32_t off = 0;
        if (!match_virus_sig(bootblock, &db[i], &off)) continue;

        uft_bb_match_t *m = &result->matches[result->match_count++];
        m->virus_id     = db[i].id;
        m->virus_name   = db[i].name;
        m->category     = db[i].category;
        m->match_offset = off;
    }

    return UFT_OK;
}
