/**
 * @file uft_dmk.c
 * @brief DMK Disk Image Format Implementation for UFT
 * 
 * @copyright UFT Project
 */

#include "uft_dmk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*============================================================================
 * CRC-16 for DMK (reversed bit order)
 *============================================================================*/

static uint16_t crc16_table[256];
static bool crc16_table_init = false;

static void init_crc16_table(void)
{
    if (crc16_table_init) return;
    
    for (int i = 0; i < 256; i++) {
        uint16_t crc = i << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
        crc16_table[i] = crc;
    }
    crc16_table_init = true;
}

uint16_t uft_dmk_crc16(const uint8_t* data, size_t length, uint16_t crc)
{
    init_crc16_table();
    
    while (length--) {
        crc = (crc << 8) ^ crc16_table[((crc >> 8) ^ *data++) & 0xFF];
    }
    return crc;
}

/*============================================================================
 * DMK Initialization and Cleanup
 *============================================================================*/

int uft_dmk_init(uft_dmk_image_t* img)
{
    if (!img) return -1;
    memset(img, 0, sizeof(*img));
    return 0;
}

void uft_dmk_free(uft_dmk_image_t* img)
{
    if (!img) return;
    
    if (img->tracks) {
        for (uint8_t t = 0; t < img->num_tracks; t++) {
            uft_dmk_track_t* track = &img->tracks[t];
            if (track->sectors) {
                for (uint8_t s = 0; s < track->num_sectors; s++) {
                    if (track->sectors[s].data) {
                        free(track->sectors[s].data);
                    }
                }
                free(track->sectors);
            }
            if (track->raw_data) {
                free(track->raw_data);
            }
        }
        free(img->tracks);
        img->tracks = NULL;
    }
    
    img->num_tracks = 0;
}

/*============================================================================
 * DMK Detection
 *============================================================================*/

bool uft_dmk_detect(const uint8_t* data, size_t size)
{
    if (!data || size < UFT_DMK_HEADER_SIZE) return false;
    
    const uft_dmk_header_t* hdr = (const uft_dmk_header_t*)data;
    
    /* Check for reasonable values */
    if (hdr->tracks == 0 || hdr->tracks > UFT_DMK_MAX_TRACKS) return false;
    if (hdr->track_length < 128 || hdr->track_length > 0x4000) return false;
    
    /* Check write protect byte */
    if (hdr->write_protect != 0x00 && hdr->write_protect != 0xFF) return false;
    
    /* Verify file size is reasonable */
    uint8_t sides = (hdr->flags & UFT_DMK_FLAG_SS) ? 1 : 2;
    size_t expected_min = UFT_DMK_HEADER_SIZE + 
                          (size_t)hdr->tracks * sides * hdr->track_length;
    
    if (size < expected_min) return false;
    
    return true;
}

/*============================================================================
 * IDAM Parsing
 *============================================================================*/

int uft_dmk_parse_idams(uft_dmk_track_t* track)
{
    if (!track || !track->raw_data) return -1;
    
    track->num_idams = 0;
    
    /* IDAM table is at the start of raw track data */
    const uint8_t* idam_table = track->raw_data;
    
    for (int i = 0; i < UFT_DMK_MAX_IDAMS; i++) {
        uint16_t ptr = idam_table[i * 2] | (idam_table[i * 2 + 1] << 8);
        
        if (ptr == 0) break;  /* End of IDAM list */
        
        uft_dmk_idam_t* idam = &track->idams[track->num_idams];
        idam->single_density = (ptr & UFT_DMK_IDAM_SD_FLAG) != 0;
        idam->offset = ptr & UFT_DMK_IDAM_MASK;
        idam->valid = true;
        
        /* Validate offset is within track */
        if (idam->offset >= UFT_DMK_IDAM_TABLE_SIZE && 
            idam->offset < track->track_length) {
            track->num_idams++;
        }
    }
    
    return track->num_idams;
}

/*============================================================================
 * MFM Sync Detection
 *============================================================================*/

bool uft_dmk_is_mfm_sync(const uft_dmk_track_t* track, uint16_t offset)
{
    if (!track || !track->raw_data) return false;
    if (offset + 3 >= track->track_length) return false;
    
    const uint8_t* p = track->raw_data + offset;
    
    /* Look for 3x 0xA1 sync bytes */
    return (p[0] == UFT_DMK_MFM_SYNC && 
            p[1] == UFT_DMK_MFM_SYNC && 
            p[2] == UFT_DMK_MFM_SYNC);
}

/*============================================================================
 * Address Mark Finding
 *============================================================================*/

int uft_dmk_find_mark(const uft_dmk_track_t* track, uint16_t start,
                      uint8_t mark, bool fm)
{
    if (!track || !track->raw_data) return -1;
    
    for (uint16_t i = start; i < track->track_length; i++) {
        if (fm) {
            /* FM: Just look for the mark byte */
            if (track->raw_data[i] == mark) {
                return i;
            }
        } else {
            /* MFM: Look for A1 A1 A1 + mark */
            if (i + 3 < track->track_length &&
                track->raw_data[i] == UFT_DMK_MFM_SYNC &&
                track->raw_data[i+1] == UFT_DMK_MFM_SYNC &&
                track->raw_data[i+2] == UFT_DMK_MFM_SYNC &&
                track->raw_data[i+3] == mark) {
                return i + 3;
            }
        }
    }
    
    return -1;
}

/*============================================================================
 * Sector Extraction
 *============================================================================*/

int uft_dmk_extract_sectors(uft_dmk_track_t* track)
{
    if (!track || !track->raw_data) return -1;
    
    /* Parse IDAMs if not already done */
    if (track->num_idams == 0) {
        uft_dmk_parse_idams(track);
    }
    
    if (track->num_idams == 0) return 0;
    
    /* Allocate sectors array */
    track->sectors = calloc(track->num_idams, sizeof(uft_dmk_sector_t));
    if (!track->sectors) return -1;
    
    track->num_sectors = 0;
    
    for (uint8_t i = 0; i < track->num_idams; i++) {
        uft_dmk_idam_t* idam = &track->idams[i];
        if (!idam->valid) continue;
        
        uft_dmk_sector_t* sector = &track->sectors[track->num_sectors];
        bool fm = idam->single_density;
        
        /* Read ID field */
        uint16_t id_offset = idam->offset;
        if (!fm) {
            /* Skip past sync bytes to IDAM */
            id_offset++;  /* Point to IDAM byte */
        }
        
        if (id_offset + 6 >= track->track_length) continue;
        
        const uint8_t* id_data = track->raw_data + id_offset;
        
        /* Verify IDAM */
        if (id_data[0] != UFT_DMK_MFM_IDAM && id_data[0] != UFT_DMK_FM_IDAM) {
            continue;
        }
        
        /* Extract ID */
        sector->id.cylinder = id_data[1];
        sector->id.head = id_data[2];
        sector->id.sector = id_data[3];
        sector->id.size_code = id_data[4];
        sector->id.crc = (id_data[5] << 8) | id_data[6];
        
        sector->fm_encoding = fm;
        sector->data_size = 128U << sector->id.size_code;
        
        /* Verify ID CRC */
        uint16_t calc_crc;
        if (fm) {
            calc_crc = uft_dmk_crc16(id_data, 5, 0xFFFF);
        } else {
            /* MFM: CRC includes the A1 A1 A1 sync */
            calc_crc = uft_dmk_crc16(id_data - 3, 8, 0xFFFF);
        }
        
        if (calc_crc != sector->id.crc) {
            sector->crc_error = true;
        }
        
        /* Find Data Address Mark */
        uint16_t dam_start = id_offset + 7;  /* After ID + CRC */
        int dam_offset = uft_dmk_find_mark(track, dam_start, 
                                            UFT_DMK_MFM_DAM, fm);
        
        if (dam_offset < 0) {
            /* Try deleted DAM */
            dam_offset = uft_dmk_find_mark(track, dam_start,
                                            UFT_DMK_MFM_DDAM, fm);
            if (dam_offset >= 0) {
                sector->deleted = true;
            }
        }
        
        if (dam_offset < 0) continue;  /* No data field */
        
        sector->data_offset = dam_offset + 1;  /* After DAM byte */
        
        /* Check if data fits in track */
        if (sector->data_offset + sector->data_size + 2 > track->track_length) {
            continue;
        }
        
        /* Allocate and copy sector data */
        sector->data = malloc(sector->data_size);
        if (sector->data) {
            memcpy(sector->data, 
                   track->raw_data + sector->data_offset,
                   sector->data_size);
            
            /* Verify data CRC */
            const uint8_t* dam_ptr = track->raw_data + dam_offset - (fm ? 0 : 3);
            size_t crc_len = (fm ? 1 : 4) + sector->data_size;
            uint16_t data_crc = uft_dmk_crc16(dam_ptr, crc_len, 
                                              fm ? 0xFFFF : UFT_DMK_CRC_A1A1A1);
            
            uint16_t stored_crc = 
                (track->raw_data[sector->data_offset + sector->data_size] << 8) |
                track->raw_data[sector->data_offset + sector->data_size + 1];
            
            if (data_crc != stored_crc) {
                sector->crc_error = true;
            }
        }
        
        track->num_sectors++;
    }
    
    return track->num_sectors;
}

/*============================================================================
 * DMK Reading
 *============================================================================*/

int uft_dmk_read_mem(const uint8_t* data, size_t size, uft_dmk_image_t* img)
{
    if (!data || !img) return -1;
    if (!uft_dmk_detect(data, size)) return -1;
    
    uft_dmk_init(img);
    
    /* Copy header */
    memcpy(&img->header, data, sizeof(uft_dmk_header_t));
    
    /* Set flags */
    img->write_protected = (img->header.write_protect == 0xFF);
    img->single_sided = (img->header.flags & UFT_DMK_FLAG_SS) != 0;
    img->single_density = (img->header.flags & UFT_DMK_FLAG_SD) != 0;
    img->native_mode = (img->header.native_flag == UFT_DMK_NATIVE_SIG);
    
    img->num_heads = img->single_sided ? 1 : 2;
    img->num_cylinders = img->header.tracks;
    img->num_tracks = img->num_cylinders * img->num_heads;
    
    /* Allocate tracks */
    img->tracks = calloc(img->num_tracks, sizeof(uft_dmk_track_t));
    if (!img->tracks) return -1;
    
    /* Read tracks */
    size_t pos = UFT_DMK_HEADER_SIZE;
    uint16_t track_len = img->header.track_length;
    
    for (uint8_t cyl = 0; cyl < img->num_cylinders; cyl++) {
        for (uint8_t head = 0; head < img->num_heads; head++) {
            uint8_t track_idx = cyl * img->num_heads + head;
            uft_dmk_track_t* track = &img->tracks[track_idx];
            
            track->cylinder = cyl;
            track->head = head;
            track->track_length = track_len;
            
            /* Check bounds */
            if (pos + track_len > size) {
                return -1;
            }
            
            /* Allocate and copy raw track data */
            track->raw_data = malloc(track_len);
            if (!track->raw_data) return -1;
            
            memcpy(track->raw_data, data + pos, track_len);
            pos += track_len;
            
            /* Parse track */
            uft_dmk_parse_idams(track);
            uft_dmk_extract_sectors(track);
        }
    }
    
    return 0;
}

int uft_dmk_read(const char* filename, uft_dmk_image_t* img)
{
    FILE* fp = fopen(filename, "rb");
    if (!fp) return -1;
    
    if (fseek(fp, 0, SEEK_END) != 0) { /* seek error */ }
    long size = ftell(fp);
    if (fseek(fp, 0, SEEK_SET) != 0) { /* seek error */ }
    if (size <= 0 || size > 64*1024*1024) {
        fclose(fp);
        return -1;
    }
    
    uint8_t* data = malloc(size);
    if (!data) {
        fclose(fp);
        return -1;
    }
    
    if (fread(data, 1, size, fp) != (size_t)size) {
        free(data);
        fclose(fp);
        return -1;
    }
    fclose(fp);
    
    int result = uft_dmk_read_mem(data, size, img);
    free(data);
    
    return result;
}

/*============================================================================
 * Track/Sector Access
 *============================================================================*/

uft_dmk_track_t* uft_dmk_get_track(uft_dmk_image_t* img,
                                    uint8_t cylinder, uint8_t head)
{
    if (!img || !img->tracks) return NULL;
    if (cylinder >= img->num_cylinders) return NULL;
    if (head >= img->num_heads) return NULL;
    
    uint8_t idx = cylinder * img->num_heads + head;
    return &img->tracks[idx];
}

int uft_dmk_read_sector(const uft_dmk_track_t* track, uint8_t sector_num,
                        uint8_t* buffer, size_t size)
{
    if (!track || !buffer || !track->sectors) return -1;
    
    /* Find sector */
    for (uint8_t i = 0; i < track->num_sectors; i++) {
        if (track->sectors[i].id.sector == sector_num) {
            uft_dmk_sector_t* sector = &track->sectors[i];
            
            if (!sector->data) return -1;
            
            size_t copy_size = sector->data_size;
            if (copy_size > size) copy_size = size;
            
            memcpy(buffer, sector->data, copy_size);
            return copy_size;
        }
    }
    
    return -1;  /* Sector not found */
}

/*============================================================================
 * Information Display
 *============================================================================*/

void uft_dmk_print_info(const uft_dmk_image_t* img, bool verbose)
{
    if (!img) return;
    
    printf("DMK Image Information:\n");
    printf("  Tracks: %d\n", img->header.tracks);
    printf("  Track length: %d bytes\n", img->header.track_length);
    printf("  Sides: %d\n", img->num_heads);
    printf("  Write protected: %s\n", img->write_protected ? "yes" : "no");
    printf("  Single density: %s\n", img->single_density ? "yes" : "no");
    printf("  Native mode: %s\n", img->native_mode ? "yes" : "no");
    
    if (verbose && img->tracks) {
        printf("\n  Track Details:\n");
        for (uint8_t i = 0; i < img->num_tracks; i++) {
            const uft_dmk_track_t* t = &img->tracks[i];
            printf("    C%02d/H%d: %d IDAMs, %d sectors\n",
                   t->cylinder, t->head, t->num_idams, t->num_sectors);
        }
    }
}

/*============================================================================
 * Raw Binary Conversion
 *============================================================================*/

int uft_dmk_to_raw(const uft_dmk_image_t* img, uint8_t** data,
                   size_t* size, uint8_t fill)
{
    if (!img || !data || !size) return -1;
    
    /* Determine geometry from first track */
    if (!img->tracks || img->num_tracks == 0) return -1;
    
    const uft_dmk_track_t* first_track = &img->tracks[0];
    if (first_track->num_sectors == 0) return -1;
    
    uint16_t sector_size = first_track->sectors[0].data_size;
    uint8_t sectors_per_track = first_track->num_sectors;
    
    /* Calculate total size */
    size_t total = (size_t)img->num_cylinders * img->num_heads * 
                   sectors_per_track * sector_size;
    
    *data = malloc(total);
    if (!*data) return -1;
    
    /* Fill with default value */
    memset(*data, fill, total);
    
    /* Copy sector data */
    size_t offset = 0;
    for (uint8_t cyl = 0; cyl < img->num_cylinders; cyl++) {
        for (uint8_t head = 0; head < img->num_heads; head++) {
            uft_dmk_track_t* track = uft_dmk_get_track((uft_dmk_image_t*)img, 
                                                        cyl, head);
            if (!track) continue;
            
            for (uint8_t s = 1; s <= sectors_per_track; s++) {
                uint8_t* dest = *data + offset;
                uft_dmk_read_sector(track, s, dest, sector_size);
                offset += sector_size;
            }
        }
    }
    
    *size = total;
    return 0;
}
