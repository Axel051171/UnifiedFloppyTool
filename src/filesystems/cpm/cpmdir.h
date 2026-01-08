/**
 * @file cpmdir.h
 * @brief CP/M Directory Structures
 */

#ifndef CPMDIR_H
#define CPMDIR_H

#include <stdint.h>

#define CPM_DIRENT_SIZE 32
#define CPM_NAME_LEN 8
#define CPM_EXT_LEN 3

typedef struct {
    uint8_t user;
    char name[CPM_NAME_LEN];
    char ext[CPM_EXT_LEN];
    uint8_t extent_low;
    uint8_t reserved1;
    uint8_t reserved2;
    uint8_t record_count;
    uint8_t allocation[16];
} cpm_dirent_t;

#endif /* CPMDIR_H */
