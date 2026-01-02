/**
 * @file uft_variant_detect.c
 * @brief Unified Format Variant Detection Implementation
 * 
 * HAFTUNGSMODUS: Implementiert Erkennung für 47 Format-Varianten
 * 
 * @author Superman QA
 * @date 2026
 */

#include "uft/formats/variants/uft_variant_detect.h"
#include <string.h>
#include <stdio.h>

// ============================================================================
// Internal Helpers
// ============================================================================

static inline uint16_t read_le16(const uint8_t* p) {
    return p[0] | (p[1] << 8);
}

static inline uint32_t read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

static inline uint32_t read_be32(const uint8_t* p) {
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

// ============================================================================
// D64 Detection
// ============================================================================

static int detect_d64(const uint8_t* data, size_t size, uft_variant_info_t* info) {
    info->format_id = UFT_FMT_D64;
    strcpy(info->format_name, "D64");
    info->geometry.sector_size = 256;
    info->geometry.heads = 1;
    info->encoding.type = ENC_GCR;
    info->encoding.rpm = 300.0;
    
    // Size-based variant detection
    switch (size) {
        case 174848:
            info->variant_flags = VAR_D64_35_TRACK;
            info->geometry.tracks = 35;
            info->geometry.total_size = 174848;
            strcpy(info->variant_name, "35-Track");
            info->confidence = 95;
            break;
            
        case 175531:
            info->variant_flags = VAR_D64_35_TRACK | VAR_D64_ERROR_INFO;
            info->geometry.tracks = 35;
            info->features.has_error_info = true;
            strcpy(info->variant_name, "35-Track+Errors");
            info->confidence = 98;
            break;
            
        case 196608:
            info->variant_flags = VAR_D64_40_TRACK;
            info->geometry.tracks = 40;
            strcpy(info->variant_name, "40-Track");
            info->confidence = 95;
            break;
            
        case 197376:
            info->variant_flags = VAR_D64_40_TRACK | VAR_D64_ERROR_INFO;
            info->geometry.tracks = 40;
            info->features.has_error_info = true;
            strcpy(info->variant_name, "40-Track+Errors");
            info->confidence = 98;
            break;
            
        case 205312:
            info->variant_flags = VAR_D64_42_TRACK;
            info->geometry.tracks = 42;
            strcpy(info->variant_name, "42-Track");
            info->confidence = 90;
            break;
            
        case 206114:
            info->variant_flags = VAR_D64_42_TRACK | VAR_D64_ERROR_INFO;
            info->geometry.tracks = 42;
            info->features.has_error_info = true;
            strcpy(info->variant_name, "42-Track+Errors");
            info->confidence = 93;
            break;
            
        default:
            return -1;
    }
    
    info->detection.size_matched = true;
    
    // GEOS Detection: Check directory for GEOS file types
    // Track 18 starts at offset 0x16500 for 35-track
    size_t dir_start = 0x16500;
    if (size > dir_start + 512 && info->geometry.tracks >= 35) {
        // Scan first few directory entries
        for (int entry = 0; entry < 8; entry++) {
            size_t offset = dir_start + 256 + (entry * 32);
            if (offset + 32 > size) break;
            
            uint8_t file_type = data[offset + 2];
            // GEOS files have high bit set
            if ((file_type & 0x80) && (file_type != 0x80)) {
                info->variant_flags |= VAR_D64_GEOS;
                strcat(info->variant_name, "/GEOS");
                info->confidence = 97;
                info->detection.content_matched = true;
                break;
            }
        }
    }
    
    // SpeedDOS Detection: Check BAM pointer
    if (size > dir_start + 2) {
        uint8_t bam_track = data[dir_start];
        uint8_t bam_sector = data[dir_start + 1];
        
        // Non-standard BAM location indicates SpeedDOS or similar
        if (bam_track == 18 && bam_sector != 1 && bam_sector != 0) {
            info->variant_flags |= VAR_D64_SPEEDDOS;
            strcat(info->variant_name, "/SpeedDOS");
        }
    }
    
    snprintf(info->full_description, sizeof(info->full_description),
             "Commodore 64 D64 %s (%d tracks, %zu bytes)",
             info->variant_name, info->geometry.tracks, size);
    
    snprintf(info->detection.evidence_summary, sizeof(info->detection.evidence_summary),
             "Size=%zu matches %d-track variant, error_info=%s",
             size, info->geometry.tracks, 
             info->features.has_error_info ? "yes" : "no");
    
    return 0;
}

// ============================================================================
// G64 Detection
// ============================================================================

static int detect_g64(const uint8_t* data, size_t size, uft_variant_info_t* info) {
    if (size < 12) return -1;
    if (memcmp(data, "GCR-1541", 8) != 0) return -1;
    
    info->format_id = UFT_FMT_G64;
    strcpy(info->format_name, "G64");
    info->detection.magic_matched = true;
    info->features.is_flux_level = false;  // GCR, not raw flux
    info->encoding.type = ENC_GCR;
    
    uint8_t version = data[8];
    uint8_t num_tracks = data[9];
    uint16_t max_track_size = read_le16(data + 10);
    
    info->geometry.tracks = num_tracks / 2;  // Half-tracks
    info->geometry.heads = 1;
    
    if (version == 0) {
        info->variant_flags = VAR_G64_V0;
        strcpy(info->variant_name, "v0 Standard");
        info->version.major = 0;
        info->confidence = 100;
    } else if (version == 1) {
        info->variant_flags = VAR_G64_V1;
        strcpy(info->variant_name, "v1 Extended");
        info->version.major = 1;
        info->confidence = 100;
        info->features.has_metadata = true;  // Per-track speed zones
    } else {
        info->variant_flags = VAR_G64_V0;
        snprintf(info->variant_name, sizeof(info->variant_name), 
                 "v%d Unknown", version);
        info->confidence = 70;
    }
    
    // Check for nibtools signature
    if (size > 0x300 && memcmp(data + 0x2F8, "NIBTOOLS", 8) == 0) {
        info->variant_flags |= VAR_G64_NIBTOOLS;
        strcat(info->variant_name, "+Nibtools");
    }
    
    snprintf(info->full_description, sizeof(info->full_description),
             "Commodore GCR G64 %s (%d half-tracks, max %d bytes/track)",
             info->variant_name, num_tracks, max_track_size);
    
    return 0;
}

// ============================================================================
// ADF Detection
// ============================================================================

static int detect_adf(const uint8_t* data, size_t size, uft_variant_info_t* info) {
    info->format_id = UFT_FMT_ADF;
    strcpy(info->format_name, "ADF");
    info->geometry.sector_size = 512;
    info->geometry.heads = 2;
    info->encoding.type = ENC_MFM;
    info->encoding.rpm = 300.0;
    
    bool is_hd = false;
    
    // Size detection
    if (size == 901120) {
        is_hd = false;
        info->geometry.tracks = 80;
        info->geometry.sectors_per_track = 11;
        info->variant_flags = VAR_ADF_DD;
        info->encoding.bitrate_kbps = 250;
        info->detection.size_matched = true;
    } else if (size == 1802240) {
        is_hd = true;
        info->geometry.tracks = 80;
        info->geometry.sectors_per_track = 22;
        info->variant_flags = VAR_ADF_HD;
        info->encoding.bitrate_kbps = 500;
        info->detection.size_matched = true;
    } else {
        return -1;
    }
    
    info->geometry.total_size = size;
    
    // Check for PC-formatted (CrossDOS)
    if (data[510] == 0x55 && data[511] == 0xAA) {
        if (data[0] == 0xEB || data[0] == 0xE9) {
            info->variant_flags |= VAR_ADF_PC_FAT;
            strcpy(info->variant_name, "PC-FAT");
            info->features.is_bootable = true;
            info->confidence = 95;
            info->detection.content_matched = true;
            
            snprintf(info->full_description, sizeof(info->full_description),
                     "Amiga ADF with PC/FAT filesystem (%s, %zu bytes)",
                     is_hd ? "HD" : "DD", size);
            return 0;
        }
    }
    
    // Amiga bootblock detection
    if (size >= 4 && memcmp(data, "DOS", 3) == 0) {
        uint8_t fs_type = data[3];
        info->detection.magic_matched = true;
        info->features.is_bootable = true;
        
        switch (fs_type) {
            case 0:
                info->variant_flags |= VAR_ADF_OFS;
                strcpy(info->variant_name, "OFS");
                break;
            case 1:
                info->variant_flags |= VAR_ADF_FFS;
                strcpy(info->variant_name, "FFS");
                break;
            case 2:
                info->variant_flags |= VAR_ADF_OFS_INTL;
                strcpy(info->variant_name, "OFS-INTL");
                break;
            case 3:
                info->variant_flags |= VAR_ADF_FFS_INTL;
                strcpy(info->variant_name, "FFS-INTL");
                break;
            case 4:
                info->variant_flags |= VAR_ADF_OFS_DC;
                strcpy(info->variant_name, "OFS-DC");
                info->features.has_metadata = true;  // DirCache
                break;
            case 5:
                info->variant_flags |= VAR_ADF_FFS_DC;
                strcpy(info->variant_name, "FFS-DC");
                info->features.has_metadata = true;
                break;
            default:
                snprintf(info->variant_name, sizeof(info->variant_name),
                         "DOS%d-Unknown", fs_type);
        }
        info->confidence = 98;
    } else if (size >= 4 && memcmp(data, "NDOS", 4) == 0) {
        info->variant_flags |= VAR_ADF_NDOS;
        strcpy(info->variant_name, "NDOS");
        info->confidence = 95;
    } else if (size >= 4 && memcmp(data, "KICK", 4) == 0) {
        strcpy(info->variant_name, "Kickstart");
        info->features.is_bootable = true;
        info->confidence = 95;
    } else {
        strcpy(info->variant_name, "Unknown");
        info->confidence = 60;
    }
    
    if (is_hd) {
        char tmp[64];
        snprintf(tmp, sizeof(tmp), "%s-HD", info->variant_name);
        strcpy(info->variant_name, tmp);
    }
    
    snprintf(info->full_description, sizeof(info->full_description),
             "Amiga Disk File %s (%s, %zu bytes)",
             info->variant_name, is_hd ? "HD" : "DD", size);
    
    return 0;
}

// ============================================================================
// WOZ Detection
// ============================================================================

static int detect_woz(const uint8_t* data, size_t size, uft_variant_info_t* info) {
    if (size < 12) return -1;
    
    uint32_t magic = read_le32(data);
    uint32_t tail = read_le32(data + 4);
    
    if (tail != 0x0A0D0AFF) return -1;
    
    info->format_id = UFT_FMT_WOZ;
    strcpy(info->format_name, "WOZ");
    info->detection.magic_matched = true;
    info->features.is_flux_level = true;
    info->encoding.type = ENC_GCR;
    
    if (magic == 0x315A4F57) {  // "WOZ1"
        info->variant_flags = VAR_WOZ_V1;
        strcpy(info->variant_name, "v1.0");
        info->version.major = 1;
        info->confidence = 100;
    } else if (magic == 0x325A4F57) {  // "WOZ2"
        // Parse INFO chunk to determine sub-version
        size_t pos = 12;
        bool found_info = false;
        
        while (pos + 8 <= size) {
            uint32_t chunk_id = read_le32(data + pos);
            uint32_t chunk_size = read_le32(data + pos + 4);
            
            if (chunk_id == 0x4F464E49) {  // "INFO"
                if (pos + 8 + chunk_size <= size && chunk_size >= 1) {
                    uint8_t info_version = data[pos + 8];
                    
                    if (info_version >= 3) {
                        info->variant_flags = VAR_WOZ_V21 | VAR_WOZ_FLUX_TIMING;
                        strcpy(info->variant_name, "v2.1");
                        info->version.major = 2;
                        info->version.minor = 1;
                        info->features.has_metadata = true;
                        
                        // Check for optimal bit timing (byte 39)
                        if (chunk_size >= 40) {
                            uint8_t bit_timing = data[pos + 8 + 39];
                            if (bit_timing > 0) {
                                info->variant_flags |= VAR_WOZ_FLUX_TIMING;
                            }
                        }
                    } else {
                        info->variant_flags = VAR_WOZ_V2;
                        strcpy(info->variant_name, "v2.0");
                        info->version.major = 2;
                    }
                    found_info = true;
                }
                break;
            }
            
            pos += 8 + chunk_size;
            // Align to 4 bytes
            if (pos % 4) pos += 4 - (pos % 4);
        }
        
        if (!found_info) {
            info->variant_flags = VAR_WOZ_V2;
            strcpy(info->variant_name, "v2.0");
            info->version.major = 2;
        }
        info->confidence = 100;
    } else {
        return -1;
    }
    
    snprintf(info->full_description, sizeof(info->full_description),
             "Apple WOZ Image %s (%zu bytes)",
             info->variant_name, size);
    
    return 0;
}

// ============================================================================
// NIB Detection
// ============================================================================

static int detect_nib(const uint8_t* data, size_t size, uft_variant_info_t* info) {
    info->format_id = UFT_FMT_NIB;
    strcpy(info->format_name, "NIB");
    info->encoding.type = ENC_GCR;
    info->geometry.heads = 1;
    
    // NIB has fixed track size of 6656 bytes
    const size_t TRACK_SIZE = 6656;
    
    switch (size) {
        case 232960:  // 35 * 6656
            info->variant_flags = VAR_NIB_35_TRACK;
            info->geometry.tracks = 35;
            strcpy(info->variant_name, "35-Track");
            info->confidence = 95;
            break;
            
        case 266240:  // 40 * 6656
            info->variant_flags = VAR_NIB_40_TRACK;
            info->geometry.tracks = 40;
            strcpy(info->variant_name, "40-Track");
            info->confidence = 95;
            break;
            
        case 465920:  // 70 * 6656 (half-tracks)
            info->variant_flags = VAR_NIB_35_TRACK | VAR_NIB_HALF_TRACK;
            info->geometry.tracks = 70;
            strcpy(info->variant_name, "35-Track Half");
            info->features.has_copy_protection = true;
            info->confidence = 90;
            break;
            
        case 532480:  // 80 * 6656 (half-tracks)
            info->variant_flags = VAR_NIB_40_TRACK | VAR_NIB_HALF_TRACK;
            info->geometry.tracks = 80;
            strcpy(info->variant_name, "40-Track Half");
            info->features.has_copy_protection = true;
            info->confidence = 90;
            break;
            
        default:
            // Check if divisible by track size
            if (size % TRACK_SIZE == 0) {
                info->geometry.tracks = size / TRACK_SIZE;
                snprintf(info->variant_name, sizeof(info->variant_name),
                         "%d-Track Custom", info->geometry.tracks);
                info->confidence = 70;
            } else {
                return -1;
            }
    }
    
    info->geometry.total_size = size;
    info->detection.size_matched = true;
    
    snprintf(info->full_description, sizeof(info->full_description),
             "Apple NIB Image %s (%d tracks, %zu bytes)",
             info->variant_name, info->geometry.tracks, size);
    
    return 0;
}

// ============================================================================
// SCP Detection
// ============================================================================

static int detect_scp(const uint8_t* data, size_t size, uft_variant_info_t* info) {
    if (size < 16) return -1;
    if (memcmp(data, "SCP", 3) != 0) return -1;
    
    info->format_id = UFT_FMT_SCP;
    strcpy(info->format_name, "SCP");
    info->detection.magic_matched = true;
    info->features.is_flux_level = true;
    
    uint8_t version = data[3];
    uint8_t disk_type = data[4];
    uint8_t num_revs = data[5];
    uint8_t start_track = data[6];
    uint8_t end_track = data[7];
    uint8_t flags = data[8];
    
    info->geometry.tracks = end_track - start_track + 1;
    
    // Version detection
    if (version < 0x10) {
        info->variant_flags = VAR_SCP_V1;
        strcpy(info->variant_name, "v1.x");
        info->version.major = 1;
    } else if (version < 0x25) {
        info->variant_flags = VAR_SCP_V2;
        strcpy(info->variant_name, "v2.x");
        info->version.major = 2;
    } else {
        info->variant_flags = VAR_SCP_V25;
        strcpy(info->variant_name, "v2.5+");
        info->version.major = 2;
        info->version.minor = 5;
        info->features.has_metadata = true;
    }
    
    // Flag-based features
    if (flags & 0x01) {
        info->variant_flags |= VAR_SCP_INDEX;
        info->features.has_metadata = true;
    }
    if (flags & 0x02) {
        info->variant_flags |= VAR_SCP_SPLICE;
    }
    if (flags & 0x40) {
        info->variant_flags |= VAR_SCP_FOOTER;
    }
    
    // Disk type to geometry
    uint8_t platform = disk_type & 0xF0;
    switch (platform) {
        case 0x00: info->encoding.rpm = 300.0; break;  // C64
        case 0x04: info->encoding.rpm = 300.0; break;  // Amiga
        case 0x08: info->encoding.rpm = 300.0; break;  // Atari ST
        case 0x10: info->encoding.rpm = 300.0; break;  // Apple II
        case 0x20: info->encoding.rpm = 300.0; break;  // PC
        default:   info->encoding.rpm = 300.0;
    }
    
    info->confidence = 100;
    
    snprintf(info->full_description, sizeof(info->full_description),
             "SuperCard Pro %s (tracks %d-%d, %d revs, type 0x%02X)",
             info->variant_name, start_track, end_track, num_revs, disk_type);
    
    return 0;
}

// ============================================================================
// HFE Detection
// ============================================================================

static int detect_hfe(const uint8_t* data, size_t size, uft_variant_info_t* info) {
    if (size < 16) return -1;
    
    info->format_id = UFT_FMT_HFE;
    strcpy(info->format_name, "HFE");
    info->features.is_flux_level = true;
    
    // HFE v3 has different signature
    if (memcmp(data, "HXCHFE3", 7) == 0) {
        info->variant_flags = VAR_HFE_V3;
        strcpy(info->variant_name, "v3 Stream");
        info->version.major = 3;
        info->detection.magic_matched = true;
        info->confidence = 100;
        
        snprintf(info->full_description, sizeof(info->full_description),
                 "HxC Floppy Emulator v3 Stream Format (%zu bytes)", size);
        
        snprintf(info->detection.evidence_summary, sizeof(info->detection.evidence_summary),
                 "HXCHFE3 signature detected - STREAM FORMAT requires special parsing");
        
        return 0;
    }
    
    // Standard HFE v1/v2
    if (memcmp(data, "HXCPICFE", 8) != 0) return -1;
    
    info->detection.magic_matched = true;
    
    uint8_t revision = data[8];
    uint8_t num_tracks = data[9];
    uint8_t num_sides = data[10];
    uint8_t encoding = data[11];
    uint16_t bitrate = read_le16(data + 12);
    
    info->geometry.tracks = num_tracks;
    info->geometry.heads = num_sides;
    info->encoding.bitrate_kbps = bitrate;
    
    if (revision == 0) {
        info->variant_flags = VAR_HFE_V1;
        strcpy(info->variant_name, "v1");
        info->version.major = 1;
    } else {
        info->variant_flags = VAR_HFE_V2;
        strcpy(info->variant_name, "v2");
        info->version.major = 2;
    }
    
    // Encoding type
    switch (encoding) {
        case 0: info->encoding.type = ENC_MFM; break;
        case 1: info->encoding.type = ENC_MFM; break;  // Amiga MFM
        case 2: info->encoding.type = ENC_MFM; break;  // Atari ST MFM
        case 3: info->encoding.type = ENC_MFM; break;  // CPC MFM
        case 4: info->encoding.type = ENC_FM; break;
        default: info->encoding.type = ENC_MFM;
    }
    
    info->confidence = 100;
    
    snprintf(info->full_description, sizeof(info->full_description),
             "HxC Floppy Emulator %s (%d tracks, %d sides, %d kbps)",
             info->variant_name, num_tracks, num_sides, bitrate);
    
    return 0;
}

// ============================================================================
// IPF Detection
// ============================================================================

static int detect_ipf(const uint8_t* data, size_t size, uft_variant_info_t* info) {
    if (size < 12) return -1;
    if (memcmp(data, "CAPS", 4) != 0) return -1;
    
    info->format_id = UFT_FMT_IPF;
    strcpy(info->format_name, "IPF");
    info->detection.magic_matched = true;
    info->features.is_flux_level = true;
    
    // Scan for record types
    size_t pos = 0;
    bool found_ctraw = false;
    bool found_data = false;
    
    while (pos + 12 <= size) {
        char record_type[5] = {0};
        memcpy(record_type, data + pos, 4);
        uint32_t record_len = read_be32(data + pos + 4);
        
        if (memcmp(record_type, "CTRA", 4) == 0) {
            found_ctraw = true;
        }
        if (memcmp(record_type, "DATA", 4) == 0) {
            found_data = true;
        }
        
        pos += 12;
        if (record_len > 12) pos += record_len - 12;
    }
    
    if (found_ctraw) {
        info->variant_flags = VAR_IPF_CTRAW;
        strcpy(info->variant_name, "CTRaw");
        info->confidence = 100;
        
        snprintf(info->detection.evidence_summary, sizeof(info->detection.evidence_summary),
                 "CTRAW record found - raw flux capture format");
    } else {
        info->variant_flags = VAR_IPF_V2;
        strcpy(info->variant_name, "Standard");
        info->version.major = 2;
        info->confidence = 100;
    }
    
    snprintf(info->full_description, sizeof(info->full_description),
             "Interchangeable Preservation Format %s (%zu bytes)",
             info->variant_name, size);
    
    return 0;
}

// ============================================================================
// IMG Detection
// ============================================================================

static int detect_img(const uint8_t* data, size_t size, uft_variant_info_t* info) {
    info->format_id = UFT_FMT_IMG;
    strcpy(info->format_name, "IMG");
    info->geometry.sector_size = 512;
    info->encoding.type = ENC_MFM;
    
    // Size-based detection
    struct {
        size_t size;
        uint32_t variant;
        const char* name;
        int tracks;
        int heads;
        int spt;
    } img_sizes[] = {
        {163840,  VAR_IMG_160K,  "160K",  40, 1, 8},
        {184320,  VAR_IMG_180K,  "180K",  40, 1, 9},
        {327680,  VAR_IMG_320K,  "320K",  40, 2, 8},
        {368640,  VAR_IMG_360K,  "360K",  40, 2, 9},
        {737280,  VAR_IMG_720K,  "720K",  80, 2, 9},
        {1228800, VAR_IMG_1200K, "1.2M",  80, 2, 15},
        {1474560, VAR_IMG_1440K, "1.44M", 80, 2, 18},
        {1720320, VAR_IMG_DMF,   "DMF",   80, 2, 21},
        {1763328, VAR_IMG_DMF,   "DMF",   80, 2, 21},
        {2949120, VAR_IMG_2880K, "2.88M", 80, 2, 36},
        {0, 0, NULL, 0, 0, 0}
    };
    
    for (int i = 0; img_sizes[i].name; i++) {
        if (size == img_sizes[i].size) {
            info->variant_flags = img_sizes[i].variant;
            strcpy(info->variant_name, img_sizes[i].name);
            info->geometry.tracks = img_sizes[i].tracks;
            info->geometry.heads = img_sizes[i].heads;
            info->geometry.sectors_per_track = img_sizes[i].spt;
            info->geometry.total_size = size;
            info->detection.size_matched = true;
            info->confidence = 85;
            break;
        }
    }
    
    if (!info->detection.size_matched) {
        // Try to detect from BPB
        if (size >= 512 && (data[0] == 0xEB || data[0] == 0xE9)) {
            uint16_t bps = read_le16(data + 11);
            uint16_t spt = read_le16(data + 24);
            uint16_t heads = read_le16(data + 26);
            
            if (bps == 512 && spt > 0 && spt <= 36 && heads > 0 && heads <= 2) {
                info->geometry.sectors_per_track = spt;
                info->geometry.heads = heads;
                info->geometry.sector_size = bps;
                info->geometry.tracks = size / (spt * heads * bps);
                strcpy(info->variant_name, "Custom");
                info->detection.structure_matched = true;
                info->confidence = 75;
            }
        }
    }
    
    if (info->variant_name[0] == '\0') {
        strcpy(info->variant_name, "Unknown");
        info->confidence = 40;
    }
    
    // Check for boot sector
    if (size >= 512 && data[510] == 0x55 && data[511] == 0xAA) {
        info->features.is_bootable = true;
    }
    
    // DMF detection via OEM name
    if (size >= 11 && memcmp(data + 3, "MSDMF", 5) == 0) {
        info->variant_flags = VAR_IMG_DMF;
        strcpy(info->variant_name, "DMF");
        info->confidence = 98;
    }
    
    snprintf(info->full_description, sizeof(info->full_description),
             "Raw Sector Image %s (%d tracks, %d heads, %d spt)",
             info->variant_name, info->geometry.tracks, 
             info->geometry.heads, info->geometry.sectors_per_track);
    
    return 0;
}

// ============================================================================
// ATR Detection
// ============================================================================

static int detect_atr(const uint8_t* data, size_t size, uft_variant_info_t* info) {
    if (size < 16) return -1;
    
    // ATR magic: 0x96 0x02
    if (data[0] != 0x96 || data[1] != 0x02) return -1;
    
    info->format_id = UFT_FMT_ATR;
    strcpy(info->format_name, "ATR");
    info->detection.magic_matched = true;
    info->geometry.heads = 1;
    
    // Parse header
    uint16_t paragraphs = read_le16(data + 2);
    uint16_t sector_size = read_le16(data + 4);
    uint8_t paragraphs_hi = data[6];
    uint8_t flags = data[15];
    
    uint32_t total_paragraphs = paragraphs | (paragraphs_hi << 16);
    size_t data_size = total_paragraphs * 16;
    
    info->geometry.sector_size = sector_size;
    
    // Determine density
    if (sector_size == 128) {
        if (data_size <= 92160) {
            info->variant_flags = VAR_ATR_SD;
            strcpy(info->variant_name, "Single Density");
        } else {
            info->variant_flags = VAR_ATR_ED;
            strcpy(info->variant_name, "Enhanced Density");
        }
        info->encoding.type = ENC_FM;
    } else if (sector_size == 256) {
        if (data_size <= 184320) {
            info->variant_flags = VAR_ATR_DD;
            strcpy(info->variant_name, "Double Density");
        } else {
            info->variant_flags = VAR_ATR_QD;
            strcpy(info->variant_name, "Quad Density");
        }
        info->encoding.type = ENC_MFM;
    } else {
        strcpy(info->variant_name, "Custom");
    }
    
    // Extended header check
    if (flags & 0x80) {
        info->variant_flags |= VAR_ATR_EXT_HDR;
        strcat(info->variant_name, "+ExtHdr");
        info->features.has_metadata = true;
    }
    
    // Write protect
    if (flags & 0x01) {
        info->features.is_write_protected = true;
    }
    
    info->confidence = 100;
    
    snprintf(info->full_description, sizeof(info->full_description),
             "Atari ATR %s (%d bytes/sector, %zu bytes data)",
             info->variant_name, sector_size, data_size);
    
    return 0;
}

// ============================================================================
// DMK Detection
// ============================================================================

static int detect_dmk(const uint8_t* data, size_t size, uft_variant_info_t* info) {
    if (size < 16) return -1;
    
    // DMK has no magic, detect by structure
    uint8_t write_protect = data[0];
    uint8_t num_tracks = data[1];
    uint16_t track_length = read_le16(data + 2);
    uint8_t options = data[4];
    
    // Sanity checks
    if (num_tracks == 0 || num_tracks > 96) return -1;
    if (track_length < 128 || track_length > 0x4000) return -1;
    
    // Verify size matches
    size_t expected = 16 + (num_tracks * track_length);
    if (options & 0x10) expected *= 2;  // Double-sided
    
    if (size < expected - track_length || size > expected + track_length) {
        return -1;
    }
    
    info->format_id = UFT_FMT_DMK;
    strcpy(info->format_name, "DMK");
    info->detection.structure_matched = true;
    
    info->geometry.tracks = num_tracks;
    info->geometry.heads = (options & 0x10) ? 2 : 1;
    
    if (options & 0x40) {
        info->variant_flags = VAR_DMK_FM;
        strcpy(info->variant_name, "FM");
        info->encoding.type = ENC_FM;
    } else {
        info->variant_flags = VAR_DMK_MFM;
        strcpy(info->variant_name, "MFM");
        info->encoding.type = ENC_MFM;
    }
    
    if (write_protect) {
        info->features.is_write_protected = true;
    }
    
    info->confidence = 80;  // Lower because no magic
    
    snprintf(info->full_description, sizeof(info->full_description),
             "TRS-80/CoCo DMK %s (%d tracks, %d sides, %d bytes/track)",
             info->variant_name, num_tracks, info->geometry.heads, track_length);
    
    return 0;
}

// ============================================================================
// Main Detection Function
// ============================================================================

int uft_variant_detect(const uint8_t* data, size_t size,
                       uft_variant_info_t* info) {
    if (!data || !info || size < 2) {
        return -1;
    }
    
    memset(info, 0, sizeof(*info));
    
    // Magic-based detection (high confidence)
    
    // SCP
    if (size >= 16 && memcmp(data, "SCP", 3) == 0) {
        return detect_scp(data, size, info);
    }
    
    // HFE
    if (size >= 16 && (memcmp(data, "HXCPICFE", 8) == 0 || 
                       memcmp(data, "HXCHFE3", 7) == 0)) {
        return detect_hfe(data, size, info);
    }
    
    // WOZ
    if (size >= 8) {
        uint32_t magic = read_le32(data);
        if (magic == 0x315A4F57 || magic == 0x325A4F57) {
            return detect_woz(data, size, info);
        }
    }
    
    // G64
    if (size >= 12 && memcmp(data, "GCR-1541", 8) == 0) {
        return detect_g64(data, size, info);
    }
    
    // IPF
    if (size >= 12 && memcmp(data, "CAPS", 4) == 0) {
        return detect_ipf(data, size, info);
    }
    
    // ATR
    if (size >= 16 && data[0] == 0x96 && data[1] == 0x02) {
        return detect_atr(data, size, info);
    }
    
    // Size-based detection
    
    // ADF (exact sizes)
    if (size == 901120 || size == 1802240) {
        return detect_adf(data, size, info);
    }
    
    // D64 (range of sizes)
    if (size >= 174848 && size <= 206114) {
        int result = detect_d64(data, size, info);
        if (result == 0) return 0;
    }
    
    // NIB (multiples of 6656)
    if (size % 6656 == 0 && size >= 232960 && size <= 532480) {
        return detect_nib(data, size, info);
    }
    
    // DMK (structure-based)
    if (detect_dmk(data, size, info) == 0) {
        return 0;
    }
    
    // IMG (fallback for common sizes)
    return detect_img(data, size, info);
}

// ============================================================================
// Utility Functions
// ============================================================================

const char* uft_variant_get_description(const uft_variant_info_t* info) {
    return info->full_description;
}

bool uft_variant_is_supported(const uft_variant_info_t* info) {
    // List unsupported variants
    if (info->format_id == UFT_FMT_HFE && (info->variant_flags & VAR_HFE_V3)) {
        return false;  // HFE v3 stream format not fully supported
    }
    if (info->format_id == UFT_FMT_IPF && (info->variant_flags & VAR_IPF_CTRAW)) {
        return false;  // CTRaw not fully supported
    }
    if (info->format_id == UFT_FMT_WOZ && (info->variant_flags & VAR_WOZ_V21)) {
        return false;  // WOZ 2.1 flux timing not fully supported
    }
    if (info->format_id == UFT_FMT_NIB && (info->variant_flags & VAR_NIB_HALF_TRACK)) {
        return false;  // Half-track NIB not fully supported
    }
    
    return true;
}

const char* uft_variant_get_limitations(const uft_variant_info_t* info) {
    static char buffer[256];
    buffer[0] = '\0';
    
    if (info->format_id == UFT_FMT_HFE && (info->variant_flags & VAR_HFE_V3)) {
        strcpy(buffer, "HFE v3 stream format requires special parser");
    } else if (info->format_id == UFT_FMT_IPF && (info->variant_flags & VAR_IPF_CTRAW)) {
        strcpy(buffer, "IPF CTRaw format not fully decoded");
    } else if (info->format_id == UFT_FMT_WOZ && (info->variant_flags & VAR_WOZ_V21)) {
        strcpy(buffer, "WOZ 2.1 optimal bit timing ignored");
    } else if (info->format_id == UFT_FMT_NIB && (info->variant_flags & VAR_NIB_HALF_TRACK)) {
        strcpy(buffer, "Half-track NIB may lose protection data");
    } else if (info->format_id == UFT_FMT_ADF && (info->variant_flags & (VAR_ADF_OFS_DC | VAR_ADF_FFS_DC))) {
        strcpy(buffer, "DirCache blocks not fully parsed");
    }
    
    return buffer;
}
