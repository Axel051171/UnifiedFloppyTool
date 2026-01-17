/**
 * @file uft_m2i.h
 * @brief M2I Tape Image Format Interface
 * @version 4.1.1
 */

#ifndef UFT_M2I_H
#define UFT_M2I_H

#include "uft/core/uft_unified_types.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* File types */
#define M2I_TYPE_DEL  0x00
#define M2I_TYPE_SEQ  0x01
#define M2I_TYPE_PRG  0x02
#define M2I_TYPE_USR  0x03
#define M2I_TYPE_REL  0x04

/* File entry in M2I image */
typedef struct {
    char filename[17];       /* Filename (ASCII) */
    uint8_t file_type;       /* File type */
    bool locked;             /* Write-protected */
    uint16_t start_address;  /* Load address */
    uint32_t file_size;      /* Size in bytes */
    uint32_t data_offset;    /* Offset in image */
    uint8_t *data;           /* File data (when loaded) */
} m2i_file_entry_t;

/* M2I image structure */
typedef struct {
    uint8_t version;
    int entry_count;
    m2i_file_entry_t *entries;
} m2i_image_t;

/* Probe */
bool uft_m2i_probe(const uint8_t *data, size_t size, int *confidence);

/* Read */
int uft_m2i_read(const char *path, m2i_image_t **out);
int uft_m2i_extract_file(const char *m2i_path, int index, const char *output_path);

/* Write */
int uft_m2i_create(m2i_image_t **out);
int uft_m2i_add_file(m2i_image_t *img, const char *filename,
                      uint8_t file_type, uint16_t start_addr,
                      const uint8_t *data, size_t data_size);
int uft_m2i_write(const char *path, const m2i_image_t *img);

/* Utility */
void uft_m2i_free(m2i_image_t *img);
int uft_m2i_get_info(const char *path, char *buf, size_t buf_size);

/* Conversion */
int uft_m2i_to_t64(const char *m2i_path, const char *t64_path);

#ifdef __cplusplus
}
#endif

#endif /* UFT_M2I_H */
