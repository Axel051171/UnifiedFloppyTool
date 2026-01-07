#ifndef UFT_D64_VIEW_H
#define UFT_D64_VIEW_H
/*
  uft_d64_view.h — Blob-basierter D64 Reader für GUI Preview
  
  Fokus: Forensik/Recovery/Analyse - In-Memory ohne File-I/O
  - Strikte 35-Track-Geometrie (174,848 Bytes) + optional Error-Bytes (+683)
  - Directory Iteration (18/1 .. linked list)
  - Chain Reader (T/S-Link, Anti-Loop)
  
  Hinweis:
  - Das ist Container-Parsing (D64), NICHT On-Disk MFM/GCR-Decoding.
  - Perfekt für GUI-Preview ohne Temp-Files
*/
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum uft_d64_status {
    UFT_D64_OK = 0,
    UFT_D64_E_INVALID = 1,
    UFT_D64_E_TRUNC = 2,
    UFT_D64_E_GEOM = 3,
    UFT_D64_E_DIR = 4,
    UFT_D64_E_CHAIN = 5
} uft_d64_status_t;

typedef struct uft_d64_geom {
    uint16_t tracks;
    uint32_t total_sectors;
    uint32_t image_bytes;
    uint8_t  has_error_bytes;
} uft_d64_geom_t;

typedef struct uft_d64_view {
    const uint8_t *blob;
    size_t blob_len;
    uft_d64_geom_t geom;
} uft_d64_view_t;

typedef enum uft_d64_filetype {
    UFT_D64_FT_DEL = 0,
    UFT_D64_FT_SEQ = 1,
    UFT_D64_FT_PRG = 2,
    UFT_D64_FT_USR = 3,
    UFT_D64_FT_REL = 4
} uft_d64_filetype_t;

typedef struct uft_d64_dirent {
    uint8_t  raw_type;
    uint8_t  closed;
    uft_d64_filetype_t type;
    uint8_t  start_track;
    uint8_t  start_sector;
    char     name_ascii[17]; /* 16 + NUL */
    uint16_t blocks;
} uft_d64_dirent_t;

typedef struct uft_d64_dir_iter {
    uint8_t track;
    uint8_t sector;
    uint8_t entry_index;
    uint8_t done;
} uft_d64_dir_iter_t;

uft_d64_status_t uft_d64_open(const uint8_t *blob, size_t blob_len, uft_d64_view_t *out);
uft_d64_status_t uft_d64_sector_ptr(const uft_d64_view_t *d64, uint8_t track, uint8_t sector, const uint8_t **out_ptr);
uft_d64_status_t uft_d64_dir_begin(uft_d64_dir_iter_t *it);
uft_d64_status_t uft_d64_dir_next(const uft_d64_view_t *d64, uft_d64_dir_iter_t *it, uft_d64_dirent_t *out_ent);
uft_d64_status_t uft_d64_read_chain(const uft_d64_view_t *d64, uint8_t start_track, uint8_t start_sector,
                                   uint8_t *out_buf, size_t out_cap,
                                   size_t *out_len, size_t *out_blocks, uint8_t *out_chain_ok);

#ifdef __cplusplus
}
#endif

#endif /* UFT_D64_VIEW_H */
