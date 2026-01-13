/**
 * @file uft_format_extensions.c
 * @brief Extended Format Support Implementation
 * 
 * P3-007: Format Extensions
 */

#include "uft/uft_format_extensions.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * MSA Format (Atari ST)
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_msa_decompress(
    const uint8_t *msa_data, size_t msa_size,
    uint8_t *raw_output, size_t *raw_size)
{
    if (!msa_data || msa_size < 10 || !raw_output || !raw_size) {
        return -1;
    }
    
    /* Check magic */
    uint16_t magic = (msa_data[0] << 8) | msa_data[1];
    if (magic != 0x0E0F) {
        return -1;
    }
    
    /* Parse header (big-endian) */
    int spt = (msa_data[2] << 8) | msa_data[3];
    int sides = ((msa_data[4] << 8) | msa_data[5]) + 1;
    int start_track = (msa_data[6] << 8) | msa_data[7];
    int end_track = (msa_data[8] << 8) | msa_data[9];
    
    int tracks = end_track - start_track + 1;
    size_t track_size = spt * 512;
    size_t total_size = tracks * sides * track_size;
    
    const uint8_t *src = msa_data + 10;
    uint8_t *dst = raw_output;
    size_t remaining = msa_size - 10;
    
    for (int t = 0; t < tracks * sides; t++) {
        if (remaining < 2) break;
        
        uint16_t data_len = (src[0] << 8) | src[1];
        src += 2;
        remaining -= 2;
        
        if (data_len == track_size) {
            /* Uncompressed track */
            if (remaining < data_len) break;
            memcpy(dst, src, data_len);
            src += data_len;
            remaining -= data_len;
        } else {
            /* RLE compressed */
            size_t written = 0;
            size_t read = 0;
            
            while (written < track_size && read < data_len) {
                uint8_t byte = src[read++];
                
                if (byte == 0xE5 && read + 2 <= data_len) {
                    /* RLE: 0xE5 <byte> <count_hi> <count_lo> */
                    uint8_t rle_byte = src[read++];
                    uint16_t count = (src[read] << 8) | src[read + 1];
                    read += 2;
                    
                    while (count-- && written < track_size) {
                        dst[written++] = rle_byte;
                    }
                } else {
                    dst[written++] = byte;
                }
            }
            
            src += data_len;
            remaining -= data_len;
        }
        
        dst += track_size;
    }
    
    *raw_size = dst - raw_output;
    return 0;
}

int uft_msa_compress(
    const uint8_t *raw_data, size_t raw_size,
    int tracks, int sides, int sectors,
    uint8_t *msa_output, size_t *msa_size)
{
    if (!raw_data || !msa_output || !msa_size) {
        return -1;
    }
    
    size_t track_size = sectors * 512;
    uint8_t *dst = msa_output;
    
    /* Write header (big-endian) */
    *dst++ = 0x0E;
    *dst++ = 0x0F;
    *dst++ = (sectors >> 8) & 0xFF;
    *dst++ = sectors & 0xFF;
    *dst++ = ((sides - 1) >> 8) & 0xFF;
    *dst++ = (sides - 1) & 0xFF;
    *dst++ = 0;
    *dst++ = 0;
    *dst++ = ((tracks - 1) >> 8) & 0xFF;
    *dst++ = (tracks - 1) & 0xFF;
    
    const uint8_t *src = raw_data;
    
    for (int t = 0; t < tracks * sides; t++) {
        /* Simple approach: store uncompressed */
        *dst++ = (track_size >> 8) & 0xFF;
        *dst++ = track_size & 0xFF;
        memcpy(dst, src, track_size);
        dst += track_size;
        src += track_size;
    }
    
    *msa_size = dst - msa_output;
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * CPC DSK Format
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_cpc_dsk_load(
    const uint8_t *dsk_data, size_t dsk_size,
    void (*sector_callback)(int track, int side, int sector,
                           const uint8_t *data, size_t size,
                           uint8_t st1, uint8_t st2, void *ctx),
    void *ctx)
{
    if (!dsk_data || dsk_size < 256) {
        return -1;
    }
    
    /* Check magic */
    bool extended = (memcmp(dsk_data, "EXTENDED", 8) == 0);
    bool standard = (memcmp(dsk_data, "MV - CPC", 8) == 0);
    
    if (!extended && !standard) {
        return -1;
    }
    
    const uft_cpc_dsk_header_t *hdr = (const uft_cpc_dsk_header_t *)dsk_data;
    int tracks = hdr->tracks;
    int sides = hdr->sides;
    
    const uint8_t *track_ptr = dsk_data + 256;
    
    for (int t = 0; t < tracks; t++) {
        for (int s = 0; s < sides; s++) {
            size_t track_size;
            
            if (extended) {
                int idx = t * sides + s;
                track_size = hdr->track_sizes[idx] * 256;
                if (track_size == 0) continue;
            } else {
                track_size = hdr->track_size;
            }
            
            if (track_ptr + track_size > dsk_data + dsk_size) {
                break;
            }
            
            /* Parse track info block */
            const uft_cpc_track_info_t *tinfo = 
                (const uft_cpc_track_info_t *)track_ptr;
            
            if (memcmp(tinfo->magic, "Track-Info", 10) != 0) {
                track_ptr += track_size;
                continue;
            }
            
            const uft_cpc_sector_info_t *sinfo = 
                (const uft_cpc_sector_info_t *)(track_ptr + 24);
            const uint8_t *sector_data = track_ptr + 256;
            
            for (int sec = 0; sec < tinfo->sector_count; sec++) {
                size_t sec_size;
                
                if (extended) {
                    sec_size = sinfo[sec].data_size;
                } else {
                    sec_size = 128 << tinfo->sector_size;
                }
                
                if (sector_callback) {
                    sector_callback(t, s, sinfo[sec].sector,
                                  sector_data, sec_size,
                                  sinfo[sec].status1, sinfo[sec].status2,
                                  ctx);
                }
                
                sector_data += sec_size;
            }
            
            track_ptr += track_size;
        }
    }
    
    return 0;
}

int uft_cpc_dsk_create(
    uint8_t *output, size_t *output_size,
    int tracks, int sides, int sectors, int sector_size,
    const uint8_t *sector_data)
{
    if (!output || !output_size || !sector_data) {
        return -1;
    }
    
    size_t sec_bytes = 128 << sector_size;
    size_t track_data_size = sectors * sec_bytes;
    size_t track_size = 256 + track_data_size;  /* Info + data */
    
    uint8_t *ptr = output;
    
    /* Disk header */
    memset(ptr, 0, 256);
    memcpy(ptr, "MV - CPCEMU Disk-File\r\nDisk-Info\r\n", 34);
    memcpy(ptr + 34, "UFT 3.8.7     ", 14);
    ptr[48] = tracks;
    ptr[49] = sides;
    ptr[50] = track_size & 0xFF;
    ptr[51] = (track_size >> 8) & 0xFF;
    ptr += 256;
    
    const uint8_t *src = sector_data;
    
    for (int t = 0; t < tracks; t++) {
        for (int s = 0; s < sides; s++) {
            /* Track info block */
            memset(ptr, 0, 256);
            memcpy(ptr, "Track-Info\r\n", 12);
            ptr[16] = t;
            ptr[17] = s;
            ptr[20] = sector_size;
            ptr[21] = sectors;
            ptr[22] = 0x4E;  /* Gap3 */
            ptr[23] = 0xE5;  /* Filler */
            
            /* Sector info */
            uint8_t *sinfo = ptr + 24;
            for (int sec = 0; sec < sectors; sec++) {
                sinfo[0] = t;           /* C */
                sinfo[1] = s;           /* H */
                sinfo[2] = sec + 1;     /* R (1-based) */
                sinfo[3] = sector_size; /* N */
                sinfo += 8;
            }
            
            ptr += 256;
            
            /* Sector data */
            memcpy(ptr, src, track_data_size);
            ptr += track_data_size;
            src += track_data_size;
        }
    }
    
    *output_size = ptr - output;
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * BBC DFS Format
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_bbc_dfs_parse(
    const uint8_t *ssd_data, size_t ssd_size,
    uft_bbc_dfs_entry_t *entries, int *entry_count,
    char *disk_title)
{
    if (!ssd_data || ssd_size < 512) {
        return -1;
    }
    
    /* Sector 0: First part of catalog */
    /* Sector 1: Second part of catalog */
    
    /* Disk title (bytes 0-7 of sector 0 + 0-3 of sector 1) */
    if (disk_title) {
        memcpy(disk_title, ssd_data, 8);
        memcpy(disk_title + 8, ssd_data + 256, 4);
        disk_title[12] = '\0';
        
        /* Trim trailing spaces */
        for (int i = 11; i >= 0 && disk_title[i] == ' '; i--) {
            disk_title[i] = '\0';
        }
    }
    
    /* Number of files */
    int file_count = ssd_data[256 + 5] / 8;
    if (file_count > 31) file_count = 31;
    
    if (entries && entry_count) {
        for (int i = 0; i < file_count; i++) {
            int offset0 = 8 + i * 8;  /* Sector 0 */
            int offset1 = 256 + 8 + i * 8;  /* Sector 1 */
            
            memcpy(entries[i].filename, ssd_data + offset0, 7);
            entries[i].filename[7] = '\0';
            entries[i].directory = ssd_data[offset0 + 7] & 0x7F;
            
            entries[i].load_addr = ssd_data[offset1] | 
                                  (ssd_data[offset1 + 1] << 8);
            entries[i].exec_addr = ssd_data[offset1 + 2] | 
                                  (ssd_data[offset1 + 3] << 8);
            entries[i].length = ssd_data[offset1 + 4] | 
                               (ssd_data[offset1 + 5] << 8);
            entries[i].start_sector = ssd_data[offset1 + 7];
        }
        *entry_count = file_count;
    }
    
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * TR-DOS Format
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_trdos_parse(
    const uint8_t *trd_data, size_t trd_size,
    uft_trdos_entry_t *entries, int *entry_count,
    char *disk_label)
{
    if (!trd_data || trd_size < 256 * 9) {
        return -1;
    }
    
    /* Sector 9 contains disk info */
    const uint8_t *info = trd_data + 256 * 8;
    
    if (disk_label) {
        memcpy(disk_label, info + 245, 8);
        disk_label[8] = '\0';
    }
    
    /* Catalog in sectors 1-8 (16 entries per sector) */
    if (entries && entry_count) {
        int count = 0;
        
        for (int sec = 0; sec < 8 && count < 128; sec++) {
            const uint8_t *cat = trd_data + sec * 256;
            
            for (int e = 0; e < 16 && count < 128; e++) {
                const uint8_t *entry = cat + e * 16;
                
                /* Check if entry is used */
                if (entry[0] == 0x00) break;  /* End of catalog */
                if (entry[0] == 0x01) continue;  /* Deleted */
                
                memcpy(entries[count].filename, entry, 8);
                entries[count].filename[8] = '\0';
                entries[count].extension = entry[8];
                entries[count].start = entry[9] | (entry[10] << 8);
                entries[count].length = entry[11] | (entry[12] << 8);
                entries[count].sectors = entry[13];
                entries[count].first_sector = entry[14];
                entries[count].first_track = entry[15];
                
                count++;
            }
        }
        *entry_count = count;
    }
    
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Format Detection
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_format_ext_t uft_detect_format_ext(
    const uint8_t *data, size_t size,
    int *confidence)
{
    if (!data || size < 16) {
        if (confidence) *confidence = 0;
        return 0;
    }
    
    int conf = 0;
    uft_format_ext_t fmt = 0;
    
    /* MSA (Atari ST) */
    if (size >= 10 && data[0] == 0x0E && data[1] == 0x0F) {
        fmt = UFT_FMT_ST_MSA;
        conf = 90;
    }
    /* CPC EDSK */
    else if (size >= 256 && memcmp(data, "EXTENDED", 8) == 0) {
        fmt = UFT_FMT_CPC_EDSK;
        conf = 95;
    }
    /* CPC DSK */
    else if (size >= 256 && memcmp(data, "MV - CPC", 8) == 0) {
        fmt = UFT_FMT_CPC_DSK;
        conf = 95;
    }
    /* TR-DOS (check signature at sector 9) */
    else if (size >= 256 * 9 && data[256 * 8 + 231] == 0x10) {
        fmt = UFT_FMT_SPEC_TRD;
        conf = 80;
    }
    /* BBC SSD/DSD by size */
    else if (size == 102400 || size == 204800) {
        fmt = (size == 102400) ? UFT_FMT_BBC_SSD : UFT_FMT_BBC_DSD;
        conf = 60;
    }
    
    if (confidence) *confidence = conf;
    return fmt;
}

const char* uft_format_ext_name(uft_format_ext_t format)
{
    switch (format) {
        case UFT_FMT_ST_RAW:   return "ST Raw";
        case UFT_FMT_ST_STX:   return "Pasti STX";
        case UFT_FMT_ST_MSA:   return "MSA";
        case UFT_FMT_CPC_DSK:  return "CPC DSK";
        case UFT_FMT_CPC_EDSK: return "CPC EDSK";
        case UFT_FMT_BBC_SSD:  return "BBC SSD";
        case UFT_FMT_BBC_DSD:  return "BBC DSD";
        case UFT_FMT_BBC_ADF:  return "BBC ADFS";
        case UFT_FMT_MSX_DSK:  return "MSX DSK";
        case UFT_FMT_MSX_DMK:  return "MSX DMK";
        case UFT_FMT_SAM_SAD:  return "Sam SAD";
        case UFT_FMT_SAM_MGT:  return "Sam MGT";
        case UFT_FMT_SPEC_DSK: return "Spectrum DSK";
        case UFT_FMT_SPEC_TRD: return "TR-DOS";
        case UFT_FMT_SPEC_SCL: return "SCL";
        default: return "Unknown";
    }
}

const char* uft_format_ext_description(uft_format_ext_t format)
{
    switch (format) {
        case UFT_FMT_ST_RAW:   return "Atari ST raw sector image";
        case UFT_FMT_ST_STX:   return "Pasti STX flux-level image";
        case UFT_FMT_ST_MSA:   return "Magic Shadow Archiver compressed";
        case UFT_FMT_CPC_DSK:  return "Amstrad CPC standard disk image";
        case UFT_FMT_CPC_EDSK: return "Amstrad CPC extended disk image";
        case UFT_FMT_BBC_SSD:  return "BBC Micro single-sided DFS";
        case UFT_FMT_BBC_DSD:  return "BBC Micro double-sided DFS";
        case UFT_FMT_BBC_ADF:  return "BBC Micro ADFS disk image";
        case UFT_FMT_MSX_DSK:  return "MSX-DOS disk image";
        case UFT_FMT_MSX_DMK:  return "MSX DMK raw track image";
        case UFT_FMT_SAM_SAD:  return "Sam Coupe SAD disk image";
        case UFT_FMT_SAM_MGT:  return "Sam Coupe MGT disk image";
        case UFT_FMT_SPEC_DSK: return "Spectrum +3 disk image";
        case UFT_FMT_SPEC_TRD: return "Spectrum TR-DOS image";
        case UFT_FMT_SPEC_SCL: return "Spectrum SCL archive";
        default: return "Unknown format";
    }
}
