/**
 * @file uft_rx50.c
 * @brief DEC RX50 Disk Format Support
 * @version 4.1.2
 * 
 * DEC RX50 - 5.25" floppy format for DEC Rainbow and Professional
 * 80 tracks, 10 sectors per track, 1 or 2 sides
 * 512 bytes per sector
 */

#include "uft/formats/uft_rx50.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define RX50_SECTOR_SIZE        512
#define RX50_TRACKS             80
#define RX50_SECTORS            10
#define RX50_SS_SIZE            (RX50_TRACKS * RX50_SECTORS * 1 * RX50_SECTOR_SIZE)
#define RX50_DS_SIZE            (RX50_TRACKS * RX50_SECTORS * 2 * RX50_SECTOR_SIZE)

static const int rx50_interleave[10] = { 1, 3, 5, 7, 9, 2, 4, 6, 8, 10 };

int uft_rx50_probe(const uint8_t *data, size_t size, int *out_heads, 
                   rx50_fs_type_t *out_fs) {
    if (!data) return 0;
    
    int confidence = 0;
    int heads = 0;
    rx50_fs_type_t fs = RX50_FS_UNKNOWN;
    
    if (size == RX50_SS_SIZE) {
        heads = 1;
        confidence = 30;
    } else if (size == RX50_DS_SIZE) {
        heads = 2;
        confidence = 30;
    } else {
        return 0;
    }
    
    /* Check boot sector signatures */
    if (data[0] == 0xEB || data[0] == 0xE9) {
        confidence += 20;
        /* Check for MS-DOS BPB */
        if (data[11] == 0x00 && data[12] == 0x02) {
            fs = RX50_FS_MSDOS;
            confidence += 15;
        } else {
            fs = RX50_FS_CPM86;
        }
    } else {
        fs = RX50_FS_RT11;
    }
    
    /* Check data pattern */
    int non_zero = 0, non_ff = 0;
    for (int i = 0; i < 512 && i < (int)size; i++) {
        if (data[i] != 0x00) non_zero++;
        if (data[i] != 0xFF) non_ff++;
    }
    if (non_zero > 10 && non_ff > 10) confidence += 15;
    
    if (confidence > 90) confidence = 90;
    
    if (out_heads) *out_heads = heads;
    if (out_fs) *out_fs = fs;
    
    return confidence;
}

int uft_rx50_read(const char *path, uft_rx50_image_t **image) {
    if (!path || !image) return UFT_ERR_INVALID_PARAM;
    
    FILE *f = fopen(path, "rb");
    if (!f) return UFT_ERR_IO;
    
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz <= 0) { fclose(f); return UFT_ERR_IO; }
    size_t size = (size_t)sz;
    fseek(f, 0, SEEK_SET);
    
    if (size != RX50_SS_SIZE && size != RX50_DS_SIZE) {
        fclose(f);
        return UFT_ERR_UNKNOWN_FORMAT;
    }
    
    uint8_t *data = malloc(size);
    if (!data) { fclose(f); return UFT_ERR_MEMORY; }
    
    if (fread(data, 1, size, f) != size) {
        free(data); fclose(f); return UFT_ERR_IO;
    }
    fclose(f);
    
    uft_rx50_image_t *img = calloc(1, sizeof(uft_rx50_image_t));
    if (!img) { free(data); return UFT_ERR_MEMORY; }
    
    img->data = data;
    img->size = size;
    img->tracks = RX50_TRACKS;
    img->sectors = RX50_SECTORS;
    img->heads = (size == RX50_DS_SIZE) ? 2 : 1;
    img->sector_size = RX50_SECTOR_SIZE;
    
    int heads;
    rx50_fs_type_t fs;
    uft_rx50_probe(data, size, &heads, &fs);
    img->fs_type = fs;
    
    *image = img;
    return UFT_OK;
}

void uft_rx50_free(uft_rx50_image_t *image) {
    if (image) {
        free(image->data);
        free(image);
    }
}

int uft_rx50_read_sector(uft_rx50_image_t *image, int track, int head, int sector,
                         uint8_t *buffer) {
    if (!image || !buffer) return UFT_ERR_INVALID_PARAM;
    
    if (track < 0 || track >= image->tracks ||
        head < 0 || head >= image->heads ||
        sector < 1 || sector > image->sectors) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    size_t offset = ((size_t)track * (size_t)image->heads * (size_t)image->sectors +
                     (size_t)head * (size_t)image->sectors +
                     (size_t)(sector - 1)) * (size_t)image->sector_size;
    
    if (offset + (size_t)image->sector_size > image->size) {
        return UFT_ERR_INCOMPLETE;
    }
    
    memcpy(buffer, image->data + offset, (size_t)image->sector_size);
    return UFT_OK;
}

int uft_rx50_write_sector(uft_rx50_image_t *image, int track, int head, int sector,
                          const uint8_t *buffer) {
    if (!image || !buffer) return UFT_ERR_INVALID_PARAM;
    
    if (track < 0 || track >= image->tracks ||
        head < 0 || head >= image->heads ||
        sector < 1 || sector > image->sectors) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    size_t offset = ((size_t)track * (size_t)image->heads * (size_t)image->sectors +
                     (size_t)head * (size_t)image->sectors +
                     (size_t)(sector - 1)) * (size_t)image->sector_size;
    
    if (offset + (size_t)image->sector_size > image->size) {
        return UFT_ERR_INCOMPLETE;
    }
    
    memcpy(image->data + offset, buffer, (size_t)image->sector_size);
    return UFT_OK;
}

int uft_rx50_get_info(uft_rx50_image_t *image, char *buf, size_t buf_size) {
    if (!image || !buf) return UFT_ERR_INVALID_PARAM;
    
    const char *fs_name;
    switch (image->fs_type) {
        case RX50_FS_RT11:  fs_name = "RT-11"; break;
        case RX50_FS_CPM86: fs_name = "CP/M-86"; break;
        case RX50_FS_MSDOS: fs_name = "MS-DOS"; break;
        default:           fs_name = "Unknown"; break;
    }
    
    snprintf(buf, buf_size,
             "DEC RX50 Disk Image\n"
             "Geometry: %d tracks x %d sectors x %d sides\n"
             "Sector Size: %d bytes\n"
             "Total Size: %zu bytes (%zu KB)\n"
             "Filesystem: %s\n",
             image->tracks, image->sectors, image->heads,
             image->sector_size,
             image->size, image->size / 1024,
             fs_name);
    
    return UFT_OK;
}

int uft_rx50_create(const char *path, int heads, rx50_fs_type_t fs_type) {
    if (!path) return UFT_ERR_INVALID_PARAM;
    
    if (heads != 1 && heads != 2) heads = 1;
    
    size_t size = (heads == 2) ? RX50_DS_SIZE : RX50_SS_SIZE;
    uint8_t *data = calloc(1, size);
    if (!data) return UFT_ERR_MEMORY;
    
    /* Initialize based on filesystem type */
    if (fs_type == RX50_FS_MSDOS) {
        data[0] = 0xEB; data[1] = 0x3C; data[2] = 0x90;
        memcpy(data + 3, "MSDOS5.0", 8);
        data[11] = 0x00; data[12] = 0x02;  /* 512 bytes/sector */
        data[13] = 1;    /* Sectors per cluster */
        data[14] = 1; data[15] = 0;
        data[16] = 2;
        data[17] = 112; data[18] = 0;
        data[19] = (uint8_t)((size / 512) & 0xFF);
        data[20] = (uint8_t)((size / 512) >> 8);
        data[21] = 0xFD;
        data[22] = 2; data[23] = 0;
        data[24] = 10; data[25] = 0;
        data[26] = (uint8_t)heads; data[27] = 0;
    }
    
    FILE *f = fopen(path, "wb");
    if (!f) { free(data); return UFT_ERR_IO; }
    fwrite(data, 1, size, f);
    fclose(f); free(data);
    return UFT_OK;
}

int uft_rx50_write(uft_rx50_image_t *image, const char *path) {
    if (!image || !path) return UFT_ERR_INVALID_PARAM;
    
    FILE *f = fopen(path, "wb");
    if (!f) return UFT_ERR_IO;
    
    size_t written = fwrite(image->data, 1, image->size, f);
    fclose(f);
    
    return (written == image->size) ? UFT_OK : UFT_ERR_IO;
}

int uft_rx50_logical_to_physical(int logical_sector) {
    if (logical_sector < 1 || logical_sector > 10) return -1;
    return rx50_interleave[logical_sector - 1];
}

int uft_rx50_physical_to_logical(int physical_sector) {
    for (int i = 0; i < 10; i++) {
        if (rx50_interleave[i] == physical_sector) return i + 1;
    }
    return -1;
}
