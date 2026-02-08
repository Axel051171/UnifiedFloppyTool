/**
 * @file uft_sega_cd.c
 * @brief Sega Saturn / Dreamcast CD-ROM Implementation
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#include "uft/formats/sega/uft_sega_cd.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Detection
 * ============================================================================ */

sega_cd_platform_t sega_cd_detect_platform(const uint8_t *data, size_t size)
{
    if (!data || size < 256) return SEGA_CD_UNKNOWN;
    
    /* Check for Saturn header */
    if (memcmp(data, "SEGA SEGASATURN ", 16) == 0) {
        return SEGA_CD_SATURN;
    }
    
    /* Check for Dreamcast header */
    if (memcmp(data, "SEGA SEGAKATANA ", 16) == 0) {
        return SEGA_CD_DREAMCAST;
    }
    
    /* Check at offset for 2352-byte sectors */
    if (size > 16 + 16) {
        if (memcmp(data + 16, "SEGA SEGASATURN ", 16) == 0) {
            return SEGA_CD_SATURN;
        }
        if (memcmp(data + 16, "SEGA SEGAKATANA ", 16) == 0) {
            return SEGA_CD_DREAMCAST;
        }
    }
    
    return SEGA_CD_UNKNOWN;
}

sega_cd_format_t sega_cd_detect_format(const uint8_t *data, size_t size)
{
    if (!data || size < 16) return SEGA_CD_FORMAT_UNKNOWN;
    
    /* Check for CDI magic */
    /* CDI files have specific header patterns */
    
    /* Check if it's a standard ISO (2048 byte sectors) */
    if (size >= 32768 + 6) {
        /* ISO-9660 identifier at sector 16 */
        if (memcmp(data + 32768 + 1, "CD001", 5) == 0) {
            return SEGA_CD_FORMAT_ISO;
        }
    }
    
    /* Check for raw 2352 byte sectors */
    if (size >= 2352 * 16 + 16) {
        /* Sync pattern at start of raw sector */
        static const uint8_t sync[12] = {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
                                          0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00};
        if (memcmp(data, sync, 12) == 0) {
            return SEGA_CD_FORMAT_BIN_CUE;
        }
    }
    
    /* Default to ISO if we can detect a valid header */
    if (sega_cd_detect_platform(data, size) != SEGA_CD_UNKNOWN) {
        return SEGA_CD_FORMAT_ISO;
    }
    
    return SEGA_CD_FORMAT_UNKNOWN;
}

const char *sega_cd_platform_name(sega_cd_platform_t platform)
{
    switch (platform) {
        case SEGA_CD_SATURN:    return "Sega Saturn";
        case SEGA_CD_DREAMCAST: return "Sega Dreamcast";
        default:                return "Unknown";
    }
}

const char *sega_cd_format_name(sega_cd_format_t format)
{
    switch (format) {
        case SEGA_CD_FORMAT_ISO:     return "ISO-9660";
        case SEGA_CD_FORMAT_BIN_CUE: return "BIN/CUE";
        case SEGA_CD_FORMAT_GDI:     return "GD-ROM (GDI)";
        case SEGA_CD_FORMAT_CDI:     return "DiscJuggler (CDI)";
        default:                     return "Unknown";
    }
}

/* ============================================================================
 * CD Operations
 * ============================================================================ */

static void parse_saturn_ip(const uint8_t *data, saturn_ip_t *ip)
{
    memcpy(ip->hardware_id, data, 16);
    memcpy(ip->maker_id, data + 16, 16);
    memcpy(ip->product_number, data + 32, 10);
    memcpy(ip->version, data + 42, 6);
    memcpy(ip->release_date, data + 48, 8);
    memcpy(ip->device_info, data + 56, 8);
    memcpy(ip->area_symbols, data + 64, 10);
    memcpy(ip->peripheral, data + 80, 16);
    memcpy(ip->title, data + 96, 112);
}

static void parse_dreamcast_ip(const uint8_t *data, dreamcast_ip_t *ip)
{
    memcpy(ip->hardware_id, data, 16);
    memcpy(ip->maker_id, data + 16, 16);
    memcpy(ip->device_info, data + 32, 16);
    memcpy(ip->area_symbols, data + 48, 8);
    memcpy(ip->peripherals, data + 56, 8);
    memcpy(ip->product_number, data + 64, 10);
    memcpy(ip->version, data + 74, 6);
    memcpy(ip->release_date, data + 80, 16);
    memcpy(ip->boot_filename, data + 96, 16);
    memcpy(ip->software_maker, data + 112, 16);
    memcpy(ip->title, data + 128, 128);
}

int sega_cd_open(const uint8_t *data, size_t size, sega_cd_t *cd)
{
    if (!data || !cd || size < 256) return -1;
    
    memset(cd, 0, sizeof(sega_cd_t));
    
    cd->platform = sega_cd_detect_platform(data, size);
    cd->format = sega_cd_detect_format(data, size);
    
    if (cd->platform == SEGA_CD_UNKNOWN) return -2;
    
    /* Copy data */
    cd->data = malloc(size);
    if (!cd->data) return -3;
    
    memcpy(cd->data, data, size);
    cd->size = size;
    cd->owns_data = true;
    
    /* Find IP.BIN offset */
    size_t ip_offset = 0;
    if (cd->format == SEGA_CD_FORMAT_BIN_CUE) {
        ip_offset = 16;  /* Skip sync header */
    }
    
    /* Parse IP.BIN */
    if (cd->platform == SEGA_CD_SATURN) {
        parse_saturn_ip(cd->data + ip_offset, &cd->saturn_ip);
    } else {
        parse_dreamcast_ip(cd->data + ip_offset, &cd->dc_ip);
    }
    
    return 0;
}

int sega_cd_load(const char *filename, sega_cd_t *cd)
{
    if (!filename || !cd) return -1;
    
    FILE *fp = fopen(filename, "rb");
    if (!fp) return -2;
    
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    uint8_t *data = malloc(size);
    if (!data) {
        fclose(fp);
        return -3;
    }
    
    if (fread(data, 1, size, fp) != size) {
        free(data);
        fclose(fp);
        return -4;
    }
    fclose(fp);
    
    int result = sega_cd_open(data, size, cd);
    free(data);
    
    return result;
}

void sega_cd_close(sega_cd_t *cd)
{
    if (!cd) return;
    
    if (cd->owns_data) {
        free(cd->data);
    }
    free(cd->tracks);
    
    memset(cd, 0, sizeof(sega_cd_t));
}

int sega_cd_get_info(const sega_cd_t *cd, sega_cd_info_t *info)
{
    if (!cd || !info) return -1;
    
    memset(info, 0, sizeof(sega_cd_info_t));
    
    info->platform = cd->platform;
    info->platform_name = sega_cd_platform_name(cd->platform);
    info->format = cd->format;
    info->format_name = sega_cd_format_name(cd->format);
    
    if (cd->platform == SEGA_CD_SATURN) {
        /* Copy and trim title */
        memcpy(info->title, cd->saturn_ip.title, 112);
        info->title[112] = '\0';
        for (int i = 111; i >= 0 && info->title[i] == ' '; i--) {
            info->title[i] = '\0';
        }
        
        memcpy(info->product_number, cd->saturn_ip.product_number, 10);
        info->product_number[10] = '\0';
        
        memcpy(info->version, cd->saturn_ip.version, 6);
        info->version[6] = '\0';
        
        memcpy(info->maker_id, cd->saturn_ip.maker_id, 16);
        info->maker_id[16] = '\0';
        
        memcpy(info->release_date, cd->saturn_ip.release_date, 8);
        info->release_date[8] = '\0';
        
        /* Parse region */
        for (int i = 0; i < 10; i++) {
            char c = cd->saturn_ip.area_symbols[i];
            if (c == 'J') info->region_japan = true;
            if (c == 'U') info->region_usa = true;
            if (c == 'E') info->region_europe = true;
        }
        
    } else {
        /* Dreamcast */
        memcpy(info->title, cd->dc_ip.title, 128);
        info->title[128] = '\0';
        for (int i = 127; i >= 0 && info->title[i] == ' '; i--) {
            info->title[i] = '\0';
        }
        
        memcpy(info->product_number, cd->dc_ip.product_number, 10);
        info->product_number[10] = '\0';
        
        memcpy(info->version, cd->dc_ip.version, 6);
        info->version[6] = '\0';
        
        memcpy(info->maker_id, cd->dc_ip.maker_id, 16);
        info->maker_id[16] = '\0';
        
        memcpy(info->release_date, cd->dc_ip.release_date, 16);
        info->release_date[16] = '\0';
        
        /* Parse region */
        for (int i = 0; i < 8; i++) {
            char c = cd->dc_ip.area_symbols[i];
            if (c == 'J') info->region_japan = true;
            if (c == 'U') info->region_usa = true;
            if (c == 'E') info->region_europe = true;
        }
    }
    
    /* Set primary region */
    if (info->region_japan) info->region = SEGA_CD_REGION_JAPAN;
    else if (info->region_usa) info->region = SEGA_CD_REGION_USA;
    else if (info->region_europe) info->region = SEGA_CD_REGION_EUROPE;
    
    info->track_count = cd->track_count;
    
    return 0;
}

/* ============================================================================
 * Utilities
 * ============================================================================ */

void sega_cd_print_info(const sega_cd_t *cd, FILE *fp)
{
    if (!cd || !fp) return;
    
    sega_cd_info_t info;
    sega_cd_get_info(cd, &info);
    
    fprintf(fp, "%s Disc:\n", info.platform_name);
    fprintf(fp, "  Format: %s\n", info.format_name);
    fprintf(fp, "  Title: %s\n", info.title);
    fprintf(fp, "  Product: %s\n", info.product_number);
    fprintf(fp, "  Version: %s\n", info.version);
    fprintf(fp, "  Maker: %s\n", info.maker_id);
    fprintf(fp, "  Date: %s\n", info.release_date);
    fprintf(fp, "  Region: %s%s%s\n",
            info.region_japan ? "J" : "",
            info.region_usa ? "U" : "",
            info.region_europe ? "E" : "");
}
