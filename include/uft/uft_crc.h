/**
 * @file uft_crc.h
 * @brief CRC helpers for floppy formats.
 */

#ifndef UFT_CRC_H
#define UFT_CRC_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief CRC type enumeration
 */
#ifndef UFT_CRC_TYPE_DEFINED
#define UFT_CRC_TYPE_DEFINED
#ifndef UFT_CRC_TYPE_T_DEFINED
#define UFT_CRC_TYPE_T_DEFINED
typedef enum {
    CRC_TYPE_NONE = 0,
    CRC_TYPE_CRC16_CCITT,
    CRC_TYPE_CRC16_IBM,
    CRC_TYPE_CRC32,
    CRC_TYPE_CHECKSUM
} uft_crc_type_t;
#endif /* UFT_CRC_TYPE_T_DEFINED */
#endif /* UFT_CRC_TYPE_DEFINED */

/*
 * REMOVED in MF-397: `int uft_crc_correct(uft_crc_type_t, uint8_t*, size_t,
 * int, uft_crc_result_t*)` together with UFT_CRC_MAX_ERRORS and
 * uft_crc_result_t, which existed only to serve it.
 *
 * No such function was ever defined. The only definition in the tree is
 *   bool uft_crc_correct(uint8_t *data, size_t size, int crc_type,
 *                        uft_crc_correction_t *result)
 * in src/algorithms/advanced/uft_god_mode_api.c:288, declared correctly in
 * include/uft/uft_god_mode.h:216 — a DIFFERENT signature under the SAME
 * symbol name.
 *
 * In C that is silent: a translation unit including this header could call
 * the five-argument form, link against the four-argument definition and pass
 * arguments in the wrong registers, with no compiler and no linker
 * diagnostic. Nothing called it yet, so this was a loaded gun rather than a
 * live defect — see docs/KNOWN_ISSUES.md ABI-1.
 *
 * uft_crc_type_t below stays: include/uft/uft_crc_polys.h uses it.
 * Do not reintroduce a declaration here without a matching definition.
 */






#ifdef __cplusplus
}
#endif

#endif /* UFT_CRC_H */
