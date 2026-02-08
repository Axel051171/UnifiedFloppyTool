/**
 * @file uft_cart7_3ds.c
 * @brief Nintendo 3DS HAL Provider for Cart8
 */

#include "uft/cart7/cart7_3ds_protocol.h"
#include "uft/cart7/uft_cart7_3ds.h"
#include "uft/cart7/uft_cart7.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

uft_cart7_error_t uft_cart7_3ds_select(uft_cart7_device_t *device) {
    if (!device) return CART7_ERROR;
    return uft_cart7_select_slot(device, SLOT_3DS);
}

uft_cart7_error_t uft_cart7_3ds_read_ncsd(uft_cart7_device_t *device, ctr_ncsd_header_t *header) {
    if (!device || !header) return CART7_ERROR;
    int64_t n = uft_cart7_read_rom(device, 0, header, sizeof(ctr_ncsd_header_t));
    if (n != sizeof(ctr_ncsd_header_t)) return CART7_ERROR_READ;
    return ctr_ncsd_is_valid(header) ? CART7_OK : CART7_ERROR;
}

uft_cart7_error_t uft_cart7_3ds_read_ncch(uft_cart7_device_t *device, int partition, ctr_ncch_header_t *header) {
    if (!device || !header || partition < 0 || partition >= 8) return CART7_ERROR;
    
    ctr_ncsd_header_t ncsd;
    uft_cart7_error_t err = uft_cart7_3ds_read_ncsd(device, &ncsd);
    if (err != CART7_OK) return err;
    if (ncsd.partitions[partition].size == 0) return CART7_ERROR;
    
    uint64_t offset = ctr_media_to_bytes(ncsd.partitions[partition].offset);
    int64_t n = uft_cart7_read_rom(device, offset, header, sizeof(ctr_ncch_header_t));
    if (n != sizeof(ctr_ncch_header_t)) return CART7_ERROR_READ;
    return ctr_ncch_is_valid(header) ? CART7_OK : CART7_ERROR;
}

uft_cart7_error_t uft_cart7_3ds_read_exefs_header(uft_cart7_device_t *device, int partition, ctr_exefs_header_t *header) {
    if (!device || !header) return CART7_ERROR;
    
    ctr_ncch_header_t ncch;
    uft_cart7_error_t err = uft_cart7_3ds_read_ncch(device, partition, &ncch);
    if (err != CART7_OK) return err;
    if (ncch.exefs_size == 0) return CART7_ERROR;
    
    ctr_ncsd_header_t ncsd;
    err = uft_cart7_3ds_read_ncsd(device, &ncsd);
    if (err != CART7_OK) return err;
    
    uint64_t part_offset = ctr_media_to_bytes(ncsd.partitions[partition].offset);
    uint64_t exefs_offset = part_offset + ctr_media_to_bytes(ncch.exefs_offset);
    
    int64_t n = uft_cart7_read_rom(device, exefs_offset, header, sizeof(ctr_exefs_header_t));
    return (n == sizeof(ctr_exefs_header_t)) ? CART7_OK : CART7_ERROR_READ;
}

uft_cart7_error_t uft_cart7_3ds_read_smdh(uft_cart7_device_t *device, int partition, ctr_smdh_t *smdh) {
    if (!device || !smdh) return CART7_ERROR;
    
    ctr_exefs_header_t exefs;
    uft_cart7_error_t err = uft_cart7_3ds_read_exefs_header(device, partition, &exefs);
    if (err != CART7_OK) return err;
    
    int icon_idx = -1;
    for (int i = 0; i < 10; i++) {
        if (strncmp(exefs.files[i].name, "icon", 8) == 0) { icon_idx = i; break; }
    }
    if (icon_idx < 0) return CART7_ERROR;
    
    ctr_ncsd_header_t ncsd;
    ctr_ncch_header_t ncch;
    uft_cart7_3ds_read_ncsd(device, &ncsd);
    uft_cart7_3ds_read_ncch(device, partition, &ncch);
    
    uint64_t part_offset = ctr_media_to_bytes(ncsd.partitions[partition].offset);
    uint64_t exefs_offset = part_offset + ctr_media_to_bytes(ncch.exefs_offset);
    uint64_t icon_offset = exefs_offset + CTR_EXEFS_HEADER_SIZE + exefs.files[icon_idx].offset;
    
    uint32_t read_size = exefs.files[icon_idx].size;
    if (read_size > sizeof(ctr_smdh_t)) read_size = sizeof(ctr_smdh_t);
    
    int64_t n = uft_cart7_read_rom(device, icon_offset, smdh, read_size);
    if (n != (int64_t)read_size) return CART7_ERROR_READ;
    return (memcmp(smdh->magic, CTR_SMDH_MAGIC, 4) == 0) ? CART7_OK : CART7_ERROR;
}

uft_cart7_error_t uft_cart7_3ds_get_info(uft_cart7_device_t *device, cart7_3ds_info_t *info) {
    if (!device || !info) return CART7_ERROR;
    memset(info, 0, sizeof(cart7_3ds_info_t));
    
    ctr_ncsd_header_t ncsd;
    uft_cart7_error_t err = uft_cart7_3ds_read_ncsd(device, &ncsd);
    if (err != CART7_OK) return err;
    
    ctr_ncch_header_t ncch;
    err = uft_cart7_3ds_read_ncch(device, 0, &ncch);
    if (err != CART7_OK) return err;
    
    memcpy(info->product_code, ncch.product_code, 16);
    memcpy(info->maker_code, ncch.maker_code, 2);
    info->card_size = ctr_media_to_bytes(ncsd.size);
    info->crypto_type = ncch.flags[3];
    
    for (int i = 0; i < 8; i++) {
        if (ncsd.partitions[i].size > 0) {
            info->partitions[i].offset = ctr_media_to_bytes(ncsd.partitions[i].offset);
            info->partitions[i].size = ctr_media_to_bytes(ncsd.partitions[i].size);
            info->partitions[i].type = ncsd.partition_fs_type[i];
            info->partitions[i].encrypted = (ncsd.partition_crypt_type[i] != 0);
            info->partition_count++;
            if (i == CTR_PARTITION_MANUAL) info->has_manual = true;
            if (i == CTR_PARTITION_DLP_CHILD) info->has_dlp_child = true;
        }
    }
    
    ctr_smdh_t smdh;
    if (uft_cart7_3ds_read_smdh(device, 0, &smdh) == CART7_OK) {
        for (int i = 0; i < 64 && smdh.titles[1].short_desc[i]; i++) {
            uint16_t c = smdh.titles[1].short_desc[i];
            if (c < 128) info->title_short[i] = (char)c;
        }
        for (int i = 0; i < 128 && smdh.titles[1].long_desc[i]; i++) {
            uint16_t c = smdh.titles[1].long_desc[i];
            if (c < 128) info->title_long[i] = (char)c;
        }
        for (int i = 0; i < 64 && smdh.titles[1].publisher[i]; i++) {
            uint16_t c = smdh.titles[1].publisher[i];
            if (c < 128) info->publisher[i] = (char)c;
        }
        info->is_new3ds_exclusive = (smdh.flags & 0x10000000) != 0;
    }
    return CART7_OK;
}

uft_cart7_error_t uft_cart7_3ds_dump(uft_cart7_device_t *device, const char *filename, bool trimmed, uft_cart7_progress_cb progress, void *user_data) {
    if (!device || !filename) return CART7_ERROR;
    
    ctr_ncsd_header_t ncsd;
    uft_cart7_error_t err = uft_cart7_3ds_read_ncsd(device, &ncsd);
    if (err != CART7_OK) return err;
    
    uint64_t dump_size;
    if (trimmed) {
        dump_size = 0;
        for (int i = 0; i < 8; i++) {
            if (ncsd.partitions[i].size > 0) {
                uint64_t end = ctr_media_to_bytes(ncsd.partitions[i].offset + ncsd.partitions[i].size);
                if (end > dump_size) dump_size = end;
            }
        }
    } else {
        dump_size = ctr_media_to_bytes(ncsd.size);
    }
    
    FILE *f = fopen(filename, "wb");
    if (!f) return CART7_ERROR;
    
    const size_t chunk_size = 1024 * 1024;
    uint8_t *chunk = (uint8_t *)malloc(chunk_size);
    if (!chunk) { fclose(f); return CART7_ERROR; }
    
    uint64_t offset = 0;
    while (offset < dump_size) {
        size_t read_size = chunk_size;
        if (offset + read_size > dump_size) read_size = (size_t)(dump_size - offset);
        
        int64_t n = uft_cart7_read_rom(device, offset, chunk, read_size);
        if (n <= 0) { free(chunk); fclose(f); return CART7_ERROR_READ; }
        
        fwrite(chunk, 1, n, f);
        offset += n;
        
        if (progress && !progress(offset, dump_size, user_data)) {
            free(chunk); fclose(f); return CART7_ERROR_ABORTED;
        }
    }
    
    free(chunk);
    fclose(f);
    return CART7_OK;
}

uint32_t ctr_save_size_bytes(ctr_save_type_t type) {
    switch (type) {
        case CTR_SAVE_EEPROM_4K:  return 512;
        case CTR_SAVE_EEPROM_64K: return 8 * 1024;
        case CTR_SAVE_EEPROM_512K:return 64 * 1024;
        case CTR_SAVE_FLASH_512K: return 64 * 1024;
        case CTR_SAVE_FLASH_1M:   return 128 * 1024;
        case CTR_SAVE_FLASH_2M:   return 256 * 1024;
        case CTR_SAVE_FLASH_4M:   return 512 * 1024;
        case CTR_SAVE_FLASH_8M:   return 1024 * 1024;
        default: return 0;
    }
}

void uft_cart7_3ds_print_info(const cart7_3ds_info_t *info) {
    if (!info) return;
    printf("=== 3DS Cartridge Info ===\n");
    printf("Product Code: %s\n", info->product_code);
    printf("Card Size:    %u MB\n", info->card_size / (1024 * 1024));
    printf("Title:        %s\n", info->title_short);
    printf("Publisher:    %s\n", info->publisher);
    printf("Partitions:   %u\n", info->partition_count);
}
