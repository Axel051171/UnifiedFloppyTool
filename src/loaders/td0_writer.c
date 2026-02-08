/**
 * @file td0_writer.c
 * @brief TD0 (Teledisk) Image Writer
 * 
 * TD0 is a compressed disk image format from the DOS era.
 * Supports LZSS compression and various sector sizes.
 * 
 * @version 5.29.0
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* TD0 Signatures */
#define TD0_SIG_NORMAL      0x4454  /* "TD" - uncompressed */
#define TD0_SIG_ADVANCED    0x6474  /* "td" - LZSS compressed */

/* TD0 Header */
#pragma pack(push, 1)
typedef struct {
    uint16_t signature;     /* "TD" or "td" */
    uint8_t  sequence;      /* Volume sequence (0 for single) */
    uint8_t  check_sig;     /* Check signature */
    uint8_t  version;       /* Version number */
    uint8_t  data_rate;     /* Source data rate */
    uint8_t  drive_type;    /* Source drive type */
    uint8_t  stepping;      /* Track stepping */
    uint8_t  dos_alloc;     /* DOS allocation flag */
    uint8_t  sides;         /* Number of sides */
    uint16_t crc;           /* CRC of header */
} td0_header_t;

typedef struct {
    uint16_t crc;           /* CRC of comment */
    uint16_t length;        /* Comment length */
    uint8_t  year;          /* Year - 1900 */
    uint8_t  month;         /* Month (1-12) */
    uint8_t  day;           /* Day (1-31) */
    uint8_t  hour;          /* Hour (0-23) */
    uint8_t  minute;        /* Minute (0-59) */
    uint8_t  second;        /* Second (0-59) */
} td0_comment_t;

typedef struct {
    uint8_t  sectors;       /* Sectors on track */
    uint8_t  cylinder;      /* Cylinder number */
    uint8_t  head;          /* Head number */
    uint8_t  crc;           /* CRC of track header */
} td0_track_t;

typedef struct {
    uint8_t  cylinder;      /* Cylinder */
    uint8_t  head;          /* Head */
    uint8_t  sector;        /* Sector number */
    uint8_t  size;          /* Size code (0=128, 1=256, etc.) */
    uint8_t  flags;         /* Flags */
    uint8_t  crc;           /* CRC */
} td0_sector_t;
#pragma pack(pop)

/* LZSS Constants */
#define LZSS_N          4096    /* Ring buffer size */
#define LZSS_F          60      /* Max match length */
#define LZSS_THRESHOLD  2       /* Min match for encoding */

typedef struct {
    uint8_t *buffer;
    size_t   size;
    size_t   capacity;
} td0_buffer_t;

typedef struct {
    td0_header_t header;
    char        *comment;
    
    struct {
        int sector_count;
        struct {
            uint8_t sector_num;
            uint8_t size_code;
            uint8_t flags;
            uint8_t *data;
            int      data_size;
        } sectors[64];
    } tracks[160];  /* 80 cyls × 2 heads */
    
    int num_tracks;
    int num_sides;
    int num_cylinders;
    bool use_compression;
} td0_image_t;

/* ============================================================================
 * CRC Calculation
 * ============================================================================ */

static uint16_t td0_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0;
    
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    
    return crc;
}

/* ============================================================================
 * LZSS Compression
 * ============================================================================ */

typedef struct {
    uint8_t ring[LZSS_N + LZSS_F - 1];
    int     match_pos;
    int     match_len;
    int     r;
} lzss_state_t;

/**
 * @brief Initialize LZSS state
 */
static void lzss_init(lzss_state_t *state)
{
    memset(state->ring, ' ', LZSS_N - 1);
    state->r = LZSS_N - LZSS_F;
    state->match_pos = 0;
    state->match_len = 0;
}

/**
 * @brief Find longest match in ring buffer
 */
static void lzss_find_match(lzss_state_t *state, int pos, int len)
{
    state->match_len = 0;
    state->match_pos = 0;
    
    for (int i = pos - LZSS_N + 1; i < pos; i++) {
        if (i < 0) continue;
        
        int match = 0;
        while (match < len && match < LZSS_F &&
               state->ring[(i + match) % LZSS_N] == 
               state->ring[(pos + match) % LZSS_N]) {
            match++;
        }
        
        if (match > state->match_len) {
            state->match_len = match;
            state->match_pos = i;
        }
    }
}

/**
 * @brief Compress data using LZSS
 */
static int lzss_compress(const uint8_t *in, size_t in_len,
                         uint8_t *out, size_t out_capacity)
{
    lzss_state_t state;
    lzss_init(&state);
    
    size_t out_pos = 0;
    size_t in_pos = 0;
    
    /* Copy input to ring buffer */
    for (size_t i = 0; i < in_len && i < LZSS_F; i++) {
        state.ring[state.r + i] = in[i];
    }
    
    while (in_pos < in_len) {
        /* Find best match */
        lzss_find_match(&state, state.r, 
                        (in_len - in_pos < LZSS_F) ? in_len - in_pos : LZSS_F);
        
        if (state.match_len <= LZSS_THRESHOLD) {
            /* Output literal */
            if (out_pos >= out_capacity) return -1;
            out[out_pos++] = in[in_pos];
            
            state.ring[state.r] = in[in_pos];
            state.r = (state.r + 1) % LZSS_N;
            in_pos++;
        } else {
            /* Output match (position, length) */
            if (out_pos + 2 >= out_capacity) return -1;
            
            out[out_pos++] = state.match_pos & 0xFF;
            out[out_pos++] = ((state.match_pos >> 4) & 0xF0) | 
                             (state.match_len - LZSS_THRESHOLD - 1);
            
            for (int i = 0; i < state.match_len; i++) {
                state.ring[state.r] = in[in_pos];
                state.r = (state.r + 1) % LZSS_N;
                in_pos++;
            }
        }
    }
    
    return out_pos;
}

/* ============================================================================
 * TD0 Writer
 * ============================================================================ */

/**
 * @brief Create TD0 image
 */
int td0_create(td0_image_t *img, int cylinders, int heads, bool compress)
{
    if (!img) return -1;
    
    memset(img, 0, sizeof(*img));
    
    img->header.signature = compress ? TD0_SIG_ADVANCED : TD0_SIG_NORMAL;
    img->header.sequence = 0;
    img->header.check_sig = 0;
    img->header.version = 21;  /* Version 2.1 */
    img->header.data_rate = 2; /* 250 kbps */
    img->header.drive_type = 2; /* 3.5" HD */
    img->header.stepping = 0;
    img->header.dos_alloc = 0;
    img->header.sides = heads;
    
    img->num_cylinders = cylinders;
    img->num_sides = heads;
    img->num_tracks = 0;
    img->use_compression = compress;
    
    return 0;
}

/**
 * @brief Add sector to track
 */
int td0_add_sector(td0_image_t *img, int cylinder, int head, int sector,
                   int size_code, const uint8_t *data)
{
    if (!img || !data) return -1;
    if (cylinder < 0 || cylinder >= 80) return -1;
    if (head < 0 || head >= 2) return -1;
    
    int track_idx = cylinder * 2 + head;
    int sect_idx = img->tracks[track_idx].sector_count;
    
    if (sect_idx >= 64) return -1;
    
    int sector_size = 128 << size_code;
    
    img->tracks[track_idx].sectors[sect_idx].sector_num = sector;
    img->tracks[track_idx].sectors[sect_idx].size_code = size_code;
    img->tracks[track_idx].sectors[sect_idx].flags = 0;
    img->tracks[track_idx].sectors[sect_idx].data = malloc(sector_size);
    img->tracks[track_idx].sectors[sect_idx].data_size = sector_size;
    
    if (!img->tracks[track_idx].sectors[sect_idx].data) return -1;
    
    memcpy(img->tracks[track_idx].sectors[sect_idx].data, data, sector_size);
    img->tracks[track_idx].sector_count++;
    
    if (track_idx >= img->num_tracks) {
        img->num_tracks = track_idx + 1;
    }
    
    return 0;
}

/**
 * @brief Set comment
 */
int td0_set_comment(td0_image_t *img, const char *comment)
{
    if (!img) return -1;
    
    free(img->comment);
    img->comment = comment ? strdup(comment) : NULL;
    
    return 0;
}

/**
 * @brief Write sector data block
 */
static int write_sector_data(FILE *fp, const uint8_t *data, int size, 
                             bool compress)
{
    if (compress && size > 4) {
        /* Try RLE first */
        bool all_same = true;
        for (int i = 1; i < size; i++) {
            if (data[i] != data[0]) {
                all_same = false;
                break;
            }
        }
        
        if (all_same) {
            /* RLE: repeat single byte */
            uint16_t block_size = 4;
            uint8_t encoding = 1;  /* RLE */
            if (fwrite(&block_size, 2, 1, fp) != 1) { /* I/O error */ }
            if (fwrite(&encoding, 1, 1, fp) != 1) { /* I/O error */ }
            uint16_t count = size;
            if (fwrite(&count, 2, 1, fp) != 1) { /* I/O error */ }
            if (fwrite(&data[0], 1, 1, fp) != 1) { /* I/O error */ }
            return 0;
        }
    }
    
    /* Write uncompressed */
    uint16_t block_size = size + 1;
    uint8_t encoding = 0;  /* Raw */
    
    if (fwrite(&block_size, 2, 1, fp) != 1) { /* I/O error */ }
    if (fwrite(&encoding, 1, 1, fp) != 1) { /* I/O error */ }
    if (fwrite(data, 1, size, fp) != size) { /* I/O error */ }
    return 0;
}

/**
 * @brief Save TD0 image
 */
int td0_save(const td0_image_t *img, const char *filename)
{
    if (!img || !filename) return -1;
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) return -1;
    
    /* Calculate header CRC */
    td0_header_t header = img->header;
    header.crc = td0_crc16((uint8_t*)&header, 10);
    
    if (fwrite(&header, sizeof(header), 1, fp) != 1) { /* I/O error */ }
    /* Write comment if present */
    if (img->comment && strlen(img->comment) > 0) {
        time_t now = time(NULL);
        struct tm *tm = localtime(&now);
        
        td0_comment_t comment;
        comment.length = strlen(img->comment);
        comment.year = tm->tm_year;
        comment.month = tm->tm_mon + 1;
        comment.day = tm->tm_mday;
        comment.hour = tm->tm_hour;
        comment.minute = tm->tm_min;
        comment.second = tm->tm_sec;
        comment.crc = td0_crc16((uint8_t*)img->comment, comment.length);
        
        if (fwrite(&comment, sizeof(comment), 1, fp) != 1) { /* I/O error */ }
        if (fwrite(img->comment, 1, comment.length, fp) != comment.length) { /* I/O error */ }
    }
    
    /* Write tracks */
    for (int c = 0; c < img->num_cylinders; c++) {
        for (int h = 0; h < img->num_sides; h++) {
            int track_idx = c * 2 + h;
            
            if (img->tracks[track_idx].sector_count == 0) continue;
            
            /* Track header */
            td0_track_t track;
            track.sectors = img->tracks[track_idx].sector_count;
            track.cylinder = c;
            track.head = h;
            track.crc = td0_crc16((uint8_t*)&track, 3);
            
            if (fwrite(&track, sizeof(track), 1, fp) != 1) { /* I/O error */ }
            /* Sectors */
            for (int s = 0; s < img->tracks[track_idx].sector_count; s++) {
                td0_sector_t sector;
                sector.cylinder = c;
                sector.head = h;
                sector.sector = img->tracks[track_idx].sectors[s].sector_num;
                sector.size = img->tracks[track_idx].sectors[s].size_code;
                sector.flags = img->tracks[track_idx].sectors[s].flags;
                sector.crc = td0_crc16((uint8_t*)&sector, 5);
                
                if (fwrite(&sector, sizeof(sector), 1, fp) != 1) { /* I/O error */ }
                /* Sector data */
                write_sector_data(fp, 
                    img->tracks[track_idx].sectors[s].data,
                    img->tracks[track_idx].sectors[s].data_size,
                    img->use_compression);
            }
        }
    }
    
    /* End marker */
    uint8_t end = 0xFF;
    if (fwrite(&end, 1, 1, fp) != 1) { /* I/O error */ }
    if (ferror(fp)) {
        fclose(fp);
        return UFT_ERR_IO;
    }
        fclose(fp);
        return 0;
}

/**
 * @brief Convert IMG to TD0
 */
int td0_from_img(const char *img_file, const char *td0_file,
                 int cylinders, int heads, int sectors, int sector_size)
{
    FILE *fp = fopen(img_file, "rb");
    if (!fp) return -1;
    
    td0_image_t td0;
    td0_create(&td0, cylinders, heads, true);
    td0_set_comment(&td0, "Created by UnifiedFloppyTool");
    
    int size_code = 0;
    while ((128 << size_code) < sector_size) size_code++;
    
    uint8_t *sector_data = malloc(sector_size);
    
    for (int c = 0; c < cylinders; c++) {
        for (int h = 0; h < heads; h++) {
            for (int s = 1; s <= sectors; s++) {
                if (fread(sector_data, 1, sector_size, fp) != (size_t)sector_size) {
                    break;
                }
                td0_add_sector(&td0, c, h, s, size_code, sector_data);
            }
        }
    }
    
    free(sector_data);
    fclose(fp);
    
    int result = td0_save(&td0, td0_file);
    td0_free(&td0);
    
    return result;
}

/**
 * @brief Free TD0 image
 */
void td0_free(td0_image_t *img)
{
    if (img) {
        free(img->comment);
        
        for (int t = 0; t < 160; t++) {
            for (int s = 0; s < img->tracks[t].sector_count; s++) {
                free(img->tracks[t].sectors[s].data);
            }
        }
        
        memset(img, 0, sizeof(*img));
    }
}
