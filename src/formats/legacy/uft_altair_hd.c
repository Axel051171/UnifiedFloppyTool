/**
 * @file uft_altair_hd.c
 * @brief Altair High-Density Floppy Format Implementation for UFT
 * 
 * @copyright UFT Project
 */

#include "uft_altair_hd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*============================================================================
 * Initialization and Cleanup
 *============================================================================*/

int uft_altair_init(uft_altair_image_t* img)
{
    if (!img) return -1;
    memset(img, 0, sizeof(*img));
    return 0;
}

void uft_altair_free(uft_altair_image_t* img)
{
    if (!img) return;
    
    if (img->data) {
        free(img->data);
        img->data = NULL;
    }
    
    img->size = 0;
}

/*============================================================================
 * Image Creation
 *============================================================================*/

int uft_altair_create(uft_altair_image_t* img, uint8_t fill)
{
    if (!img) return -1;
    
    uft_altair_init(img);
    
    img->size = UFT_ALTAIR_DISK_SIZE;
    img->data = malloc(img->size);
    if (!img->data) return -1;
    
    memset(img->data, fill, img->size);
    memset(img->track_status, UFT_ALTAIR_TRACK_OK, UFT_ALTAIR_NUM_TRACKS);
    
    return 0;
}

/*============================================================================
 * File I/O
 *============================================================================*/

int uft_altair_read_mem(const uint8_t* data, size_t size, uft_altair_image_t* img)
{
    if (!data || !img) return -1;
    
    uft_altair_init(img);
    
    /* Accept exact size or slightly larger (padding) */
    if (size < UFT_ALTAIR_DISK_SIZE) return -1;
    
    img->size = UFT_ALTAIR_DISK_SIZE;
    img->data = malloc(img->size);
    if (!img->data) return -1;
    
    memcpy(img->data, data, img->size);
    memset(img->track_status, UFT_ALTAIR_TRACK_OK, UFT_ALTAIR_NUM_TRACKS);
    
    return 0;
}

int uft_altair_read(const char* filename, uft_altair_image_t* img)
{
    FILE* fp = fopen(filename, "rb");
    if (!fp) return -1;
    
    if (fseek(fp, 0, SEEK_END) != 0) { /* seek error */ }
    long size = ftell(fp);
    if (fseek(fp, 0, SEEK_SET) != 0) { /* seek error */ }
    if (size < (long)UFT_ALTAIR_DISK_SIZE) {
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
    
    int result = uft_altair_read_mem(data, size, img);
    free(data);
    
    return result;
}

int uft_altair_write(const char* filename, const uft_altair_image_t* img)
{
    if (!filename || !img || !img->data) return -1;
    
    FILE* fp = fopen(filename, "wb");
    if (!fp) return -1;
    
    if (fwrite(img->data, 1, img->size, fp) != img->size) {
        fclose(fp);
        return -1;
    }
    
    fclose(fp);
    return 0;
}

/*============================================================================
 * Track Access
 *============================================================================*/

int uft_altair_read_track(const uft_altair_image_t* img, uint8_t track,
                          uint8_t* buffer)
{
    if (!img || !img->data || !buffer) return -1;
    if (track >= UFT_ALTAIR_NUM_TRACKS) return -1;
    
    int32_t offset = uft_altair_track_offset(track);
    if (offset < 0) return -1;
    
    memcpy(buffer, img->data + offset, UFT_ALTAIR_TRACK_LENGTH);
    return UFT_ALTAIR_TRACK_LENGTH;
}

int uft_altair_write_track(uft_altair_image_t* img, uint8_t track,
                           const uint8_t* buffer)
{
    if (!img || !img->data || !buffer) return -1;
    if (track >= UFT_ALTAIR_NUM_TRACKS) return -1;
    if (img->write_protected) return -1;
    
    int32_t offset = uft_altair_track_offset(track);
    if (offset < 0) return -1;
    
    memcpy(img->data + offset, buffer, UFT_ALTAIR_TRACK_LENGTH);
    img->track_status[track] = UFT_ALTAIR_TRACK_OK;
    
    return 0;
}

/*============================================================================
 * Information Display
 *============================================================================*/

void uft_altair_print_info(const uft_altair_image_t* img, bool verbose)
{
    if (!img) return;
    
    printf("Altair HD Floppy Image Information:\n");
    printf("  Format: Altair 8800 High-Density\n");
    printf("  Size: %zu bytes (%.2f KB)\n", img->size, img->size / 1024.0);
    printf("  Tracks: %d\n", UFT_ALTAIR_NUM_TRACKS);
    printf("  Sectors/track: %d\n", UFT_ALTAIR_SECTORS_PER_TRACK);
    printf("  Bytes/sector: %d\n", UFT_ALTAIR_SECTOR_LENGTH);
    printf("  Write protected: %s\n", img->write_protected ? "yes" : "no");
    
    if (verbose) {
        printf("\n  Track Layout:\n");
        printf("    Tracks 0-143: Cylinders 0-71, both sides (interleaved)\n");
        printf("    Tracks 144-148: Cylinders 72-76, bottom side only\n");
        
        /* Count tracks with errors */
        int errors = 0;
        int missing = 0;
        for (int i = 0; i < UFT_ALTAIR_NUM_TRACKS; i++) {
            if (img->track_status[i] & UFT_ALTAIR_TRACK_ERROR) errors++;
            if (img->track_status[i] & UFT_ALTAIR_TRACK_MISSING) missing++;
        }
        
        if (errors > 0 || missing > 0) {
            printf("\n  Track Status:\n");
            printf("    Errors: %d\n", errors);
            printf("    Missing: %d\n", missing);
        }
    }
}

/*============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * @brief Validate track layout against expected Altair format
 */
bool uft_altair_validate(const uft_altair_image_t* img)
{
    if (!img || !img->data) return false;
    if (img->size != UFT_ALTAIR_DISK_SIZE) return false;
    return true;
}

/**
 * @brief Get cylinder and side description for track
 */
void uft_altair_track_desc(uint8_t track, char* buffer, size_t size)
{
    if (!buffer || size < 32) return;
    
    uint8_t cyl = uft_altair_track_to_cylinder(track);
    uint8_t side = uft_altair_track_to_side(track);
    
    snprintf(buffer, size, "Track %3d: Cyl %2d, %s side",
             track, cyl, side ? "top" : "bottom");
}
