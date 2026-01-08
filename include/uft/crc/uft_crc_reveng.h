/**
 * @file uft_crc_reveng.h
 * @brief CRC Reverse Engineering
 */
#ifndef UFT_CRC_REVENG_H
#define UFT_CRC_REVENG_H

#include <stdint.h>
#include <stddef.h>
#include "uft_crc_bfs.h"

int uft_crc_reverse(const uft_crc_sample_t *samples, size_t count,
                    uft_crc_result_t *result);

#endif /* UFT_CRC_REVENG_H */
