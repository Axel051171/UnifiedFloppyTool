/**
 * @file uft_crc_bfs.h
 * @brief CRC Brute-Force Search
 */
#ifndef UFT_CRC_BFS_H
#define UFT_CRC_BFS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    uint32_t poly;
    uint32_t init;
    uint32_t xorout;
    int width;
    bool refin;
    bool refout;
    float confidence;
} uft_crc_bfs_result_t;

typedef struct {
    const uint8_t *data;
    size_t data_len;
    uint32_t expected_crc;
    int width;
} uft_crc_sample_t;

typedef struct {
    uint32_t poly;
    uint32_t init;
    uint32_t xorout;
    int width;
    bool refin;
    bool refout;
    const char *name;
} uft_crc_model_t;

typedef struct {
    uft_crc_model_t model;
    float confidence;
    int matches;
} uft_crc_result_t;

int uft_crc_bfs_search(const uft_crc_sample_t *samples, size_t count,
                       uft_crc_bfs_result_t *results, size_t max_results);

#endif /* UFT_CRC_BFS_H */
