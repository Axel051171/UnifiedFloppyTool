// SPDX-License-Identifier: MIT
/*
 * woz2.c - WOZ 2.0 Disk Image Format Implementation
 * 
 * @version 2.8.4
 * @date 2024-12-26
 */

#include "uft/uft_error.h"
#include "woz2.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/*============================================================================*
 * CRC32 TABLE
 *============================================================================*/

static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA,
    0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
    0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
    0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
    0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE,
    0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC,
    0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
    0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
    0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
    0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940,
    0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116,
    0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
    0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
    0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
    0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A,
    0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818,
    0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
    0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
    0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
    0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C,
    0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2,
    0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
    0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
    0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
    0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086,
    0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4,
    0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
    0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
    0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
    0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8,
    0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE,
    0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
    0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
    0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
    0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252,
    0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60,
    0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
    0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
    0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
    0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04,
    0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A,
    0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
    0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
    0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
    0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E,
    0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C,
    0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
    0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
    0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
    0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0,
    0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6,
    0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
    0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
    0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

/*============================================================================*
 * CRC32 IMPLEMENTATION
 *============================================================================*/

uint32_t woz2_crc32(const uint8_t *data, size_t size)
{
    uint32_t crc = 0xFFFFFFFF;
    
    for (size_t i = 0; i < size; i++) {
        uint8_t index = (crc ^ data[i]) & 0xFF;
        crc = (crc >> 8) ^ crc32_table[index];
    }
    
    return ~crc;
}

/*============================================================================*
 * HELPER FUNCTIONS
 *============================================================================*/

static inline uint32_t read_le32(const uint8_t *buf)
{
    return buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
}

static inline void write_le32(uint8_t *buf, uint32_t val)
{
    buf[0] = val & 0xFF;
    buf[1] = (val >> 8) & 0xFF;
    buf[2] = (val >> 16) & 0xFF;
    buf[3] = (val >> 24) & 0xFF;
}

static inline uint16_t read_le16(const uint8_t *buf)
{
    return buf[0] | (buf[1] << 8);
}

static inline void write_le16(uint8_t *buf, uint16_t val)
{
    buf[0] = val & 0xFF;
    buf[1] = (val >> 8) & 0xFF;
}

/*============================================================================*
 * INITIALIZATION
 *============================================================================*/

bool woz2_init(woz2_image_t *image, uint8_t disk_type)
{
    if (!image) return false;
    
    memset(image, 0, sizeof(woz2_image_t));
    
    /* Initialize header */
    memcpy(image->header.magic, WOZ2_MAGIC, 4);
    image->header.ff = 0xFF;
    image->header.lf_cr[0] = 0x0A;
    image->header.lf_cr[1] = 0x0D;
    image->header.reserved = 0x00;
    image->header.crc32 = 0;  /* Will be calculated on write */
    
    /* Initialize INFO */
    image->info.version = WOZ2_INFO_VERSION;
    image->info.disk_type = disk_type;
    image->info.write_protected = WOZ2_WRITE_PROTECTED_NO;
    image->info.synchronized = WOZ2_SYNCHRONIZED_NO;
    image->info.cleaned = WOZ2_CLEANED_NO;
    strncpy(image->info.creator, WOZ2_CREATOR_UFT, 31);
    image->info.creator[31] = '\0';
    image->info.disk_sides = 1;
    image->info.boot_sector_format = 0;
    image->info.optimal_bit_timing = 32;  /* 4μs / 125ns = 32 */
    image->info.compatible_hardware = 0;
    image->info.required_ram = 0;
    image->info.largest_track = 0;
    image->info.flux_block = 0;
    image->info.largest_flux_track = 0;
    
    /* Initialize TMAP */
    memset(image->tmap.map, WOZ2_TRACK_EMPTY, WOZ2_TRACK_MAP_SIZE);
    
    /* Initialize tracks */
    image->num_tracks = 0;
    
    return true;
}

/*============================================================================*
 * MEMORY MANAGEMENT
 *============================================================================*/

void woz2_free(woz2_image_t *image)
{
    if (!image) return;
    
    if (image->track_data) {
        free(image->track_data);
        image->track_data = NULL;
    }
    
    if (image->meta) {
        free(image->meta);
        image->meta = NULL;
    }
    
    if (image->writ_data) {
        free(image->writ_data);
        image->writ_data = NULL;
    }
    
    if (image->filename) {
        free(image->filename);
        image->filename = NULL;
    }
}

/*============================================================================*
 * TRACK OPERATIONS
 *============================================================================*/

bool woz2_add_track(woz2_image_t *image, uint8_t track_num,
                    uint8_t quarter, const uint8_t *data,
                    uint32_t bit_count)
{
    if (!image || !data || bit_count == 0) return false;
    if (track_num >= 40) return false;  /* Max 40 tracks for 5.25" */
    if (quarter >= 4) return false;
    
    /* Calculate TMAP index */
    uint8_t tmap_index = track_num * 4 + quarter;
    if (tmap_index >= WOZ2_TRACK_MAP_SIZE) return false;
    
    /* Calculate byte count (round up) */
    uint32_t byte_count = (bit_count + 7) / 8;
    uint32_t block_count = (byte_count + WOZ2_TRACK_BLOCK_SIZE - 1) / 
                           WOZ2_TRACK_BLOCK_SIZE;
    
    /* Reallocate track data */
    size_t new_size = image->track_data_size + (block_count * WOZ2_TRACK_BLOCK_SIZE);
    uint8_t *new_data = realloc(image->track_data, new_size);
    if (!new_data) return false;
    
    /* Zero the new blocks */
    memset(new_data + image->track_data_size, 0, 
           block_count * WOZ2_TRACK_BLOCK_SIZE);
    
    /* Copy track data */
    memcpy(new_data + image->track_data_size, data, byte_count);
    
    image->track_data = new_data;
    
    /* Update TRK entry */
    uint8_t trk_index = image->num_tracks;
    image->tracks[trk_index].starting_block = image->track_data_size / WOZ2_TRACK_BLOCK_SIZE;
    image->tracks[trk_index].block_count = block_count;
    image->tracks[trk_index].bit_count = bit_count;
    
    /* Update TMAP */
    image->tmap.map[tmap_index] = trk_index;
    
    /* Update sizes */
    image->track_data_size = new_size;
    image->num_tracks++;
    
    /* Update largest_track in INFO */
    if (block_count > image->info.largest_track) {
        image->info.largest_track = block_count;
    }
    
    return true;
}

bool woz2_get_track(const woz2_image_t *image, uint8_t track_num,
                    uint8_t quarter, uint8_t **data, uint32_t *bit_count)
{
    if (!image || !data || !bit_count) return false;
    if (track_num >= 40) return false;
    if (quarter >= 4) return false;
    
    /* Get TMAP index */
    uint8_t tmap_index = track_num * 4 + quarter;
    if (tmap_index >= WOZ2_TRACK_MAP_SIZE) return false;
    
    /* Get TRK index */
    uint8_t trk_index = image->tmap.map[tmap_index];
    if (trk_index == WOZ2_TRACK_EMPTY) {
        *data = NULL;
        *bit_count = 0;
        return false;
    }
    
    /* Get track info */
    const woz2_trk_t *trk = &image->tracks[trk_index];
    
    /* Calculate offset */
    size_t offset = trk->starting_block * WOZ2_TRACK_BLOCK_SIZE;
    
    if (offset >= image->track_data_size) return false;
    
    *data = image->track_data + offset;
    *bit_count = trk->bit_count;
    
    return true;
}

/*============================================================================*
 * FILE I/O - READ
 *============================================================================*/

bool woz2_read(const char *filename, woz2_image_t *image)
{
    if (!filename || !image) return false;
    
    FILE *f = fopen(filename, "rb");
    if (!f) return false;
    
    /* Get file size */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (file_size < WOZ2_HEADER_SIZE) {
        fclose(f);
        return false;
    }
    
    /* Read entire file */
    uint8_t *file_data = malloc(file_size);
    if (!file_data) {
        fclose(f);
        return false;
    }
    
    if (fread(file_data, 1, file_size, f) != (size_t)file_size) {
        free(file_data);
        fclose(f);
        return false;
    }
    
    fclose(f);
    
    /* Parse header */
    memcpy(&image->header, file_data, WOZ2_HEADER_SIZE);
    
    /* Verify magic */
    if (memcmp(image->header.magic, WOZ2_MAGIC, 4) != 0) {
        free(file_data);
        return false;
    }
    
    /* Verify CRC32 */
    uint32_t calculated_crc = woz2_crc32(file_data + WOZ2_HEADER_SIZE,
                                         file_size - WOZ2_HEADER_SIZE);
    if (calculated_crc != image->header.crc32) {
        /* CRC mismatch - could be intentional or corruption */
        fprintf(stderr, "WOZ2: CRC mismatch (calc: %08X, file: %08X)\n",
                calculated_crc, image->header.crc32);
    }
    
    /* Parse chunks */
    size_t offset = WOZ2_HEADER_SIZE;
    bool has_info = false, has_tmap = false, has_trks = false;
    
    while (offset + 8 <= (size_t)file_size) {
        woz2_chunk_header_t chunk;
        memcpy(&chunk, file_data + offset, 8);
        offset += 8;
        
        switch (chunk.id) {
            case WOZ2_CHUNK_INFO:
                if (chunk.size >= sizeof(woz2_info_t)) {
                    memcpy(&image->info, file_data + offset, sizeof(woz2_info_t));
                    has_info = true;
                }
                break;
                
            case WOZ2_CHUNK_TMAP:
                if (chunk.size >= sizeof(woz2_tmap_t)) {
                    memcpy(&image->tmap, file_data + offset, sizeof(woz2_tmap_t));
                    has_tmap = true;
                }
                break;
                
            case WOZ2_CHUNK_TRKS:
                has_trks = true;
                /* Parse TRKS */
                {
                    size_t trks_offset = 0;
                    image->num_tracks = 0;
                    
                    /* Read TRK entries (8 bytes each) */
                    while (trks_offset + 8 <= chunk.size && image->num_tracks < 160) {
                        memcpy(&image->tracks[image->num_tracks],
                               file_data + offset + trks_offset, 8);
                        
                        /* Only count non-empty tracks */
                        if (image->tracks[image->num_tracks].starting_block != 0 ||
                            image->tracks[image->num_tracks].block_count != 0) {
                            image->num_tracks++;
                        }
                        
                        trks_offset += 8;
                    }
                    
                    /* Calculate track data size */
                    if (image->num_tracks > 0) {
                        /* Find the highest block */
                        uint32_t max_block = 0;
                        for (int i = 0; i < image->num_tracks; i++) {
                            uint32_t end_block = image->tracks[i].starting_block +
                                               image->tracks[i].block_count;
                            if (end_block > max_block) {
                                max_block = end_block;
                            }
                        }
                        
                        image->track_data_size = max_block * WOZ2_TRACK_BLOCK_SIZE;
                        
                        if (image->track_data_size > 0) {
                            image->track_data = malloc(image->track_data_size);
                            if (image->track_data) {
                                /* Track data starts after TRK entries */
                                size_t data_offset = ((chunk.size > 1280) ? 1280 : chunk.size);
                                size_t available = chunk.size - data_offset;
                                size_t to_copy = (available < image->track_data_size) ?
                                               available : image->track_data_size;
                                
                                memcpy(image->track_data,
                                       file_data + offset + data_offset,
                                       to_copy);
                            }
                        }
                    }
                }
                break;
                
            case WOZ2_CHUNK_META:
                if (chunk.size > 0) {
                    image->meta = malloc(chunk.size + 1);
                    if (image->meta) {
                        memcpy(image->meta, file_data + offset, chunk.size);
                        image->meta[chunk.size] = '\0';
                        image->meta_size = chunk.size;
                    }
                }
                break;
                
            case WOZ2_CHUNK_WRIT:
                if (chunk.size > 0) {
                    image->writ_data = malloc(chunk.size);
                    if (image->writ_data) {
                        memcpy(image->writ_data, file_data + offset, chunk.size);
                        image->writ_size = chunk.size;
                        image->has_writ = true;
                    }
                }
                break;
                
            default:
                /* Unknown chunk - skip */
                break;
        }
        
        offset += chunk.size;
    }
    
    free(file_data);
    
    /* Verify required chunks */
    if (!has_info || !has_tmap || !has_trks) {
        woz2_free(image);
        return false;
    }
    
    /* Save filename */
    image->filename = strdup(filename);
    
    return true;
}

/*============================================================================*
 * FILE I/O - WRITE
 *============================================================================*/

bool woz2_write(const char *filename, const woz2_image_t *image)
{
    if (!filename || !image) return false;
    
    FILE *f = fopen(filename, "wb");
    if (!f) return false;
    
    /* Calculate total size for CRC */
    size_t total_size = 0;
    
    /* INFO chunk */
    total_size += 8 + sizeof(woz2_info_t);
    
    /* TMAP chunk */
    total_size += 8 + sizeof(woz2_tmap_t);
    
    /* TRKS chunk */
    size_t trks_size = 1280 + image->track_data_size;  /* 160 * 8 bytes + data */
    total_size += 8 + trks_size;
    
    /* META chunk (optional) */
    if (image->meta && image->meta_size > 0) {
        total_size += 8 + image->meta_size;
    }
    
    /* WRIT chunk (optional) */
    if (image->has_writ && image->writ_data && image->writ_size > 0) {
        total_size += 8 + image->writ_size;
    }
    
    /* Allocate buffer for CRC calculation */
    uint8_t *buffer = malloc(total_size);
    if (!buffer) {
        fclose(f);
        return false;
    }
    
    size_t buf_offset = 0;
    
    /* Build INFO chunk */
    write_le32(buffer + buf_offset, WOZ2_CHUNK_INFO);
    buf_offset += 4;
    write_le32(buffer + buf_offset, sizeof(woz2_info_t));
    buf_offset += 4;
    memcpy(buffer + buf_offset, &image->info, sizeof(woz2_info_t));
    buf_offset += sizeof(woz2_info_t);
    
    /* Build TMAP chunk */
    write_le32(buffer + buf_offset, WOZ2_CHUNK_TMAP);
    buf_offset += 4;
    write_le32(buffer + buf_offset, sizeof(woz2_tmap_t));
    buf_offset += 4;
    memcpy(buffer + buf_offset, &image->tmap, sizeof(woz2_tmap_t));
    buf_offset += sizeof(woz2_tmap_t);
    
    /* Build TRKS chunk */
    write_le32(buffer + buf_offset, WOZ2_CHUNK_TRKS);
    buf_offset += 4;
    write_le32(buffer + buf_offset, trks_size);
    buf_offset += 4;
    
    /* Write TRK entries */
    for (int i = 0; i < 160; i++) {
        if (i < image->num_tracks) {
            memcpy(buffer + buf_offset, &image->tracks[i], 8);
        } else {
            memset(buffer + buf_offset, 0, 8);
        }
        buf_offset += 8;
    }
    
    /* Write track data */
    if (image->track_data && image->track_data_size > 0) {
        memcpy(buffer + buf_offset, image->track_data, image->track_data_size);
        buf_offset += image->track_data_size;
    }
    
    /* Build META chunk (optional) */
    if (image->meta && image->meta_size > 0) {
        write_le32(buffer + buf_offset, WOZ2_CHUNK_META);
        buf_offset += 4;
        write_le32(buffer + buf_offset, image->meta_size);
        buf_offset += 4;
        memcpy(buffer + buf_offset, image->meta, image->meta_size);
        buf_offset += image->meta_size;
    }
    
    /* Build WRIT chunk (optional) */
    if (image->has_writ && image->writ_data && image->writ_size > 0) {
        write_le32(buffer + buf_offset, WOZ2_CHUNK_WRIT);
        buf_offset += 4;
        write_le32(buffer + buf_offset, image->writ_size);
        buf_offset += 4;
        memcpy(buffer + buf_offset, image->writ_data, image->writ_size);
        buf_offset += image->writ_size;
    }
    
    /* Calculate CRC32 */
    uint32_t crc = woz2_crc32(buffer, buf_offset);
    
    /* Write header */
    woz2_header_t header = image->header;
    header.crc32 = crc;
    
    if (fwrite(&header, 1, WOZ2_HEADER_SIZE, f) != WOZ2_HEADER_SIZE) {
        free(buffer);
        fclose(f);
        return false;
    }
    
    /* Write chunks */
    if (fwrite(buffer, 1, buf_offset, f) != buf_offset) {
        free(buffer);
        fclose(f);
        return false;
    }
    
    free(buffer);
    fclose(f);
    
    return true;
}

/*============================================================================*
 * VALIDATION
 *============================================================================*/

bool woz2_validate(const woz2_image_t *image, char *errors, size_t errors_size)
{
    if (!image) return false;
    
    bool valid = true;
    char msg[256];
    
    if (errors) errors[0] = '\0';
    
    /* Check magic */
    if (memcmp(image->header.magic, WOZ2_MAGIC, 4) != 0) {
        valid = false;
        snprintf(msg, sizeof(msg), "Invalid magic (not WOZ2)\n");
        if (errors) strncat(errors, msg, errors_size - strlen(errors) - 1);
    }
    
    /* Check INFO version */
    if (image->info.version != WOZ2_INFO_VERSION) {
        valid = false;
        snprintf(msg, sizeof(msg), "Invalid version (expected 2, got %d)\n",
                image->info.version);
        if (errors) strncat(errors, msg, errors_size - strlen(errors) - 1);
    }
    
    /* Check disk type */
    if (image->info.disk_type != WOZ2_DISK_TYPE_5_25 &&
        image->info.disk_type != WOZ2_DISK_TYPE_3_5) {
        valid = false;
        snprintf(msg, sizeof(msg), "Invalid disk type (%d)\n",
                image->info.disk_type);
        if (errors) strncat(errors, msg, errors_size - strlen(errors) - 1);
    }
    
    /* Check track data */
    if (image->num_tracks > 0 && !image->track_data) {
        valid = false;
        snprintf(msg, sizeof(msg), "Missing track data\n");
        if (errors) strncat(errors, msg, errors_size - strlen(errors) - 1);
    }
    
    return valid;
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
