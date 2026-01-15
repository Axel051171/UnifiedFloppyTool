/**
 * @file uft_tc.c
 * @brief Transcopy TC Format Implementation
 * 
 * @version 1.0.0
 * @date 2026-01-15
 */

#include "uft/formats/uft_tc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*===========================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

typedef struct {
    uft_tc_track_header_t header;
    uint8_t *data;
    size_t data_size;
} tc_track_t;

struct uft_tc_context {
    FILE *file;
    char *path;
    uft_tc_header_t header;
    tc_track_t **tracks;        /**< [track * sides + side] */
    int total_tracks;
    bool modified;
};

/*===========================================================================
 * HELPERS
 *===========================================================================*/

static int track_index(uft_tc_t *tc, int track, int side) {
    if (!tc || track < 0 || track >= tc->header.num_tracks ||
        side < 0 || side >= tc->header.num_sides) return -1;
    return track * tc->header.num_sides + side;
}

/*===========================================================================
 * LIFECYCLE
 *===========================================================================*/

bool uft_tc_probe(const char *path) {
    if (!path) return false;
    
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    
    uint8_t sig;
    bool valid = (fread(&sig, 1, 1, f) == 1 && sig == UFT_TC_SIGNATURE);
    fclose(f);
    
    return valid;
}

uft_tc_t* uft_tc_open(const char *path) {
    if (!path) return NULL;
    
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    
    /* Read header */
    uft_tc_header_t header;
    if (fread(&header, sizeof(header), 1, f) != 1) {
        fclose(f);
        return NULL;
    }
    
    /* Verify signature */
    if (header.signature != UFT_TC_SIGNATURE) {
        fclose(f);
        return NULL;
    }
    
    /* Allocate context */
    uft_tc_t *tc = (uft_tc_t *)calloc(1, sizeof(*tc));
    if (!tc) {
        fclose(f);
        return NULL;
    }
    
    tc->file = f;
    tc->path = strdup(path);
    tc->header = header;
    tc->total_tracks = header.num_tracks * header.num_sides;
    
    /* Allocate track array */
    tc->tracks = (tc_track_t **)calloc(tc->total_tracks, sizeof(tc_track_t *));
    if (!tc->tracks) {
        free(tc->path);
        free(tc);
        fclose(f);
        return NULL;
    }
    
    /* Read all tracks */
    for (int t = 0; t < header.num_tracks; t++) {
        for (int s = 0; s < header.num_sides; s++) {
            int idx = t * header.num_sides + s;
            
            tc->tracks[idx] = (tc_track_t *)calloc(1, sizeof(tc_track_t));
            if (!tc->tracks[idx]) continue;
            
            /* Read track header */
            if (fread(&tc->tracks[idx]->header, sizeof(uft_tc_track_header_t), 
                      1, f) != 1) {
                free(tc->tracks[idx]);
                tc->tracks[idx] = NULL;
                continue;
            }
            
            /* Read track data */
            uint16_t data_len = tc->tracks[idx]->header.data_length;
            if (data_len == 0) data_len = header.track_size;
            
            tc->tracks[idx]->data = (uint8_t *)malloc(data_len);
            if (tc->tracks[idx]->data) {
                tc->tracks[idx]->data_size = fread(tc->tracks[idx]->data, 
                                                    1, data_len, f);
            }
        }
    }
    
    return tc;
}

uft_tc_t* uft_tc_create(const char *path, int tracks, int sides, int density) {
    if (!path || tracks <= 0 || tracks > UFT_TC_MAX_TRACKS ||
        sides <= 0 || sides > UFT_TC_MAX_SIDES) return NULL;
    
    uft_tc_t *tc = (uft_tc_t *)calloc(1, sizeof(*tc));
    if (!tc) return NULL;
    
    tc->path = strdup(path);
    tc->total_tracks = tracks * sides;
    tc->modified = true;
    
    /* Initialize header */
    tc->header.signature = UFT_TC_SIGNATURE;
    tc->header.version = 1;
    tc->header.num_tracks = tracks;
    tc->header.num_sides = sides;
    tc->header.track_size = (density == UFT_TC_DENSITY_HD) ? 12500 : 6250;
    tc->header.density = density;
    
    /* Allocate track array */
    tc->tracks = (tc_track_t **)calloc(tc->total_tracks, sizeof(tc_track_t *));
    
    return tc;
}

void uft_tc_close(uft_tc_t *tc) {
    if (!tc) return;
    
    /* TODO: Save if modified */
    
    /* Free tracks */
    if (tc->tracks) {
        for (int i = 0; i < tc->total_tracks; i++) {
            if (tc->tracks[i]) {
                free(tc->tracks[i]->data);
                free(tc->tracks[i]);
            }
        }
        free(tc->tracks);
    }
    
    if (tc->file) fclose(tc->file);
    free(tc->path);
    free(tc);
}

/*===========================================================================
 * INFORMATION
 *===========================================================================*/

const uft_tc_header_t* uft_tc_get_header(uft_tc_t *tc) {
    return tc ? &tc->header : NULL;
}

int uft_tc_get_tracks(uft_tc_t *tc) {
    return tc ? tc->header.num_tracks : 0;
}

int uft_tc_get_sides(uft_tc_t *tc) {
    return tc ? tc->header.num_sides : 0;
}

bool uft_tc_is_hd(uft_tc_t *tc) {
    return tc ? (tc->header.density == UFT_TC_DENSITY_HD) : false;
}

/*===========================================================================
 * TRACK OPERATIONS
 *===========================================================================*/

int uft_tc_get_track_header(uft_tc_t *tc, int track, int side,
                             uft_tc_track_header_t *header) {
    if (!tc || !header) return -1;
    
    int idx = track_index(tc, track, side);
    if (idx < 0 || !tc->tracks[idx]) return -1;
    
    *header = tc->tracks[idx]->header;
    return 0;
}

int uft_tc_read_track(uft_tc_t *tc, int track, int side,
                       uint8_t *data, size_t max_size) {
    if (!tc || !data) return -1;
    
    int idx = track_index(tc, track, side);
    if (idx < 0 || !tc->tracks[idx] || !tc->tracks[idx]->data) return -1;
    
    size_t to_copy = tc->tracks[idx]->data_size;
    if (to_copy > max_size) to_copy = max_size;
    
    memcpy(data, tc->tracks[idx]->data, to_copy);
    return (int)to_copy;
}

int uft_tc_write_track(uft_tc_t *tc, int track, int side,
                        const uint8_t *data, size_t size) {
    if (!tc || !data) return -1;
    
    int idx = track_index(tc, track, side);
    if (idx < 0) return -1;
    
    /* Allocate track if needed */
    if (!tc->tracks[idx]) {
        tc->tracks[idx] = (tc_track_t *)calloc(1, sizeof(tc_track_t));
        if (!tc->tracks[idx]) return -1;
    }
    
    /* Free old data */
    free(tc->tracks[idx]->data);
    
    /* Allocate new */
    tc->tracks[idx]->data = (uint8_t *)malloc(size);
    if (!tc->tracks[idx]->data) return -1;
    
    memcpy(tc->tracks[idx]->data, data, size);
    tc->tracks[idx]->data_size = size;
    
    /* Update header */
    tc->tracks[idx]->header.track_num = track;
    tc->tracks[idx]->header.side = side;
    tc->tracks[idx]->header.data_length = (uint16_t)size;
    
    tc->modified = true;
    return 0;
}

size_t uft_tc_get_track_length(uft_tc_t *tc, int track, int side) {
    int idx = track_index(tc, track, side);
    if (idx < 0 || !tc->tracks[idx]) return 0;
    return tc->tracks[idx]->data_size;
}

/*===========================================================================
 * CONVERSION
 *===========================================================================*/

int uft_tc_to_img(const char *tc_path, const char *img_path) {
    uft_tc_t *tc = uft_tc_open(tc_path);
    if (!tc) return -1;
    
    FILE *f = fopen(img_path, "wb");
    if (!f) {
        uft_tc_close(tc);
        return -1;
    }
    
    /* Decode tracks to sectors */
    /* TC stores raw MFM data, need to decode */
    uint8_t sector[512];
    int spt = uft_tc_is_hd(tc) ? 18 : 9;
    
    for (int t = 0; t < uft_tc_get_tracks(tc); t++) {
        for (int s = 0; s < uft_tc_get_sides(tc); s++) {
            /* For each sector */
            for (int sec = 0; sec < spt; sec++) {
                memset(sector, 0, sizeof(sector));
                /* TODO: Actually decode MFM data from track */
                fwrite(sector, 1, sizeof(sector), f);
            }
        }
    }
    
    fclose(f);
    uft_tc_close(tc);
    return 0;
}

int uft_img_to_tc(const char *img_path, const char *tc_path) {
    FILE *f = fopen(img_path, "rb");
    if (!f) return -1;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    /* Determine geometry */
    int tracks = 80, sides = 2, density = UFT_TC_DENSITY_HD;
    if (size == 720 * 1024) { density = UFT_TC_DENSITY_DD; }
    else if (size == 360 * 1024) { tracks = 40; density = UFT_TC_DENSITY_DD; }
    
    uft_tc_t *tc = uft_tc_create(tc_path, tracks, sides, density);
    if (!tc) {
        fclose(f);
        return -1;
    }
    
    int spt = (density == UFT_TC_DENSITY_HD) ? 18 : 9;
    uint8_t sector[512];
    
    for (int t = 0; t < tracks; t++) {
        for (int s = 0; s < sides; s++) {
            /* Read all sectors for this track */
            size_t track_size = spt * 512;
            uint8_t *track_data = (uint8_t *)calloc(1, track_size);
            
            for (int sec = 0; sec < spt; sec++) {
                if (fread(sector, 1, 512, f) == 512) {
                    memcpy(track_data + sec * 512, sector, 512);
                }
            }
            
            /* TODO: Encode to MFM/raw TC format */
            uft_tc_write_track(tc, t, s, track_data, track_size);
            
            free(track_data);
        }
    }
    
    fclose(f);
    
    /* TODO: Save TC file */
    uft_tc_close(tc);
    return 0;
}
