// SPDX-License-Identifier: MIT
/*
 * msa.c - MSA (Magic Shadow Archiver) Format Implementation
 * 
 * @version 2.8.6
 * @date 2024-12-26
 */

#include "uft/uft_error.h"
#include "msa.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*============================================================================*
 * HELPERS
 *============================================================================*/

static inline uint16_t read_be16(const uint8_t *buf) {
    return (buf[0] << 8) | buf[1];
}

static inline void write_be16(uint8_t *buf, uint16_t val) {
    buf[0] = (val >> 8) & 0xFF;
    buf[1] = val & 0xFF;
}

size_t msa_track_size(uint16_t sectors_per_track) {
    return sectors_per_track * MSA_SECTOR_SIZE;
}

/*============================================================================*
 * RLE COMPRESSION/DECOMPRESSION
 *============================================================================*/

bool msa_decompress_track(const uint8_t *compressed, size_t comp_size,
                          uint8_t *decompressed, size_t decomp_size) {
    if (!compressed || !decompressed) return false;
    
    size_t in = 0, out = 0;
    
    while (in < comp_size && out < decomp_size) {
        uint8_t byte = compressed[in++];
        
        if (byte == 0xE5 && in + 2 < comp_size) {
            uint8_t data = compressed[in++];
            uint16_t count = read_be16(&compressed[in]);
            in += 2;
            
            if (count == 0) {
                decompressed[out++] = 0xE5;
            } else {
                for (uint16_t i = 0; i < count && out < decomp_size; i++) {
                    decompressed[out++] = data;
                }
            }
        } else {
            decompressed[out++] = byte;
        }
    }
    
    return (out == decomp_size);
}

bool msa_compress_track(const uint8_t *data, size_t size,
                        uint8_t *compressed, size_t *comp_size) {
    if (!data || !compressed || !comp_size) return false;
    
    size_t in = 0, out = 0, max_out = *comp_size;
    
    while (in < size) {
        uint8_t byte = data[in];
        size_t run = 1;
        
        while (in + run < size && data[in + run] == byte && run < 0xFFFF) {
            run++;
        }
        
        if (run >= 4 || byte == 0xE5) {
            if (out + 4 > max_out) return false;
            compressed[out++] = 0xE5;
            compressed[out++] = byte;
            write_be16(&compressed[out], run);
            out += 2;
            in += run;
        } else {
            if (out >= max_out) return false;
            compressed[out++] = byte;
            in++;
        }
    }
    
    *comp_size = out;
    return true;
}

/*============================================================================*
 * INIT/FREE
 *============================================================================*/

bool msa_init(msa_image_t *image, uint16_t sectors_per_track,
              uint16_t sides, uint16_t tracks) {
    if (!image || tracks == 0) return false;
    if (sides != 1 && sides != 2) return false;
    
    memset(image, 0, sizeof(msa_image_t));
    
    image->header.magic = MSA_MAGIC;
    image->header.sectors_per_track = sectors_per_track;
    image->header.sides = sides - 1;
    image->header.starting_track = 0;
    image->header.ending_track = tracks - 1;
    
    image->num_tracks = tracks;
    image->num_sides = sides;
    
    size_t total = tracks * sides;
    image->tracks = calloc(total, sizeof(uint8_t *));
    image->track_sizes = calloc(total, sizeof(size_t));
    
    if (!image->tracks || !image->track_sizes) {
        msa_free(image);
        return false;
    }
    
    size_t track_size = msa_track_size(sectors_per_track);
    for (size_t i = 0; i < total; i++) {
        image->tracks[i] = calloc(1, track_size);
        if (!image->tracks[i]) {
            msa_free(image);
            return false;
        }
        image->track_sizes[i] = track_size;
    }
    
    return true;
}

void msa_free(msa_image_t *image) {
    if (!image) return;
    
    if (image->tracks) {
        size_t total = image->num_tracks * image->num_sides;
        for (size_t i = 0; i < total; i++) {
            if (image->tracks[i]) free(image->tracks[i]);
        }
        free(image->tracks);
    }
    
    if (image->track_sizes) free(image->track_sizes);
    if (image->filename) free(image->filename);
}

/*============================================================================*
 * FILE I/O
 *============================================================================*/

bool msa_read(const char *filename, msa_image_t *image) {
    if (!filename || !image) return false;
    
    FILE *f = fopen(filename, "rb");
    if (!f) return false;
    
    uint8_t hdr[MSA_HEADER_SIZE];
    if (fread(hdr, 1, MSA_HEADER_SIZE, f) != MSA_HEADER_SIZE) {
        fclose(f);
        return false;
    }
    
    image->header.magic = read_be16(&hdr[0]);
    image->header.sectors_per_track = read_be16(&hdr[2]);
    image->header.sides = read_be16(&hdr[4]);
    image->header.starting_track = read_be16(&hdr[6]);
    image->header.ending_track = read_be16(&hdr[8]);
    
    if (image->header.magic != MSA_MAGIC) {
        fclose(f);
        return false;
    }
    
    image->num_tracks = image->header.ending_track - image->header.starting_track + 1;
    image->num_sides = image->header.sides + 1;
    
    size_t total = image->num_tracks * image->num_sides;
    size_t track_size = msa_track_size(image->header.sectors_per_track);
    
    image->tracks = calloc(total, sizeof(uint8_t *));
    image->track_sizes = calloc(total, sizeof(size_t));
    
    if (!image->tracks || !image->track_sizes) {
        fclose(f);
        msa_free(image);
        return false;
    }
    
    for (size_t i = 0; i < total; i++) {
        uint8_t th[2];
        if (fread(th, 1, 2, f) != 2) {
            fclose(f);
            msa_free(image);
            return false;
        }
        
        uint16_t len = read_be16(th);
        uint8_t *comp = malloc(len);
        if (!comp) {
            fclose(f);
            msa_free(image);
            return false;
        }
        
        if (fread(comp, 1, len, f) != len) {
            free(comp);
            fclose(f);
            msa_free(image);
            return false;
        }
        
        image->tracks[i] = malloc(track_size);
        if (!image->tracks[i]) {
            free(comp);
            fclose(f);
            msa_free(image);
            return false;
        }
        
        if (!msa_decompress_track(comp, len, image->tracks[i], track_size)) {
            free(comp);
            fclose(f);
            msa_free(image);
            return false;
        }
        
        image->track_sizes[i] = track_size;
        free(comp);
    }
    
    fclose(f);
    image->filename = strdup(filename);
    return true;
}

bool msa_write(const char *filename, const msa_image_t *image) {
    if (!filename || !image) return false;
    
    FILE *f = fopen(filename, "wb");
    if (!f) return false;
    
    uint8_t hdr[MSA_HEADER_SIZE];
    write_be16(&hdr[0], image->header.magic);
    write_be16(&hdr[2], image->header.sectors_per_track);
    write_be16(&hdr[4], image->header.sides);
    write_be16(&hdr[6], image->header.starting_track);
    write_be16(&hdr[8], image->header.ending_track);
    
    if (fwrite(hdr, 1, MSA_HEADER_SIZE, f) != MSA_HEADER_SIZE) {
        fclose(f);
        return false;
    }
    
    size_t total = image->num_tracks * image->num_sides;
    
    for (size_t i = 0; i < total; i++) {
        size_t max_comp = image->track_sizes[i] * 2;
        uint8_t *comp = malloc(max_comp);
        if (!comp) {
            fclose(f);
            return false;
        }
        
        size_t comp_size = max_comp;
        if (!msa_compress_track(image->tracks[i], image->track_sizes[i], comp, &comp_size)) {
            free(comp);
            fclose(f);
            return false;
        }
        
        uint8_t th[2];
        write_be16(th, comp_size);
        if (fwrite(th, 1, 2, f) != 2 || fwrite(comp, 1, comp_size, f) != comp_size) {
            free(comp);
            fclose(f);
            return false;
        }
        
        free(comp);
    }
    
    fclose(f);
    return true;
}

bool msa_get_track(const msa_image_t *image, uint16_t track, uint16_t side,
                   uint8_t **data, size_t *size) {
    if (!image || !data || !size) return false;
    if (track >= image->num_tracks || side >= image->num_sides) return false;
    
    size_t idx = track * image->num_sides + side;
    *data = image->tracks[idx];
    *size = image->track_sizes[idx];
    return true;
}

bool msa_validate(const msa_image_t *image, char *errors, size_t errors_size) {
    if (!image) return false;
    
    bool valid = true;
    if (errors) errors[0] = '\0';
    
    if (image->header.magic != MSA_MAGIC) {
        valid = false;
        if (errors) snprintf(errors, errors_size, "Invalid magic\n");
    }
    
    if (!image->tracks) {
        valid = false;
        if (errors) snprintf(errors + strlen(errors), 
                           errors_size - strlen(errors), "No tracks\n");
    }
    
    return valid;
}

bool msa_to_st(const char *msa_filename, const char *st_filename) {
    msa_image_t img;
    if (!msa_read(msa_filename, &img)) return false;
    
    FILE *f = fopen(st_filename, "wb");
    if (!f) {
        msa_free(&img);
        return false;
    }
    
    size_t total = img.num_tracks * img.num_sides;
    for (size_t i = 0; i < total; i++) {
        if (fwrite(img.tracks[i], 1, img.track_sizes[i], f) != img.track_sizes[i]) {
            fclose(f);
            msa_free(&img);
            return false;
        }
    }
    
    fclose(f);
    msa_free(&img);
    return true;
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "Linux"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
