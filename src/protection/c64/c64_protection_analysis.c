/**
 * @file c64_protection_analysis.c
 * @brief C64 Protection Scheme Detection
 * @version 4.1.5
 *
 * Individual protection scheme detectors for V-MAX!, RapidLok,
 * Datasoft, SSI RapidDOS, EA Interlock, Novaload, and Speedlock.
 */

#include "c64_protection_internal.h"

/* ============================================================================
 * V-MAX! Version Detection
 * Based on Pete Rittwage and Lord Crass documentation
 * ============================================================================ */

const char *c64_vmax_version_string(c64_vmax_version_t version) {
    switch (version) {
        case VMAX_VERSION_0: return "V-Max! v0 (Star Rank Boxing - CBM DOS, checksums)";
        case VMAX_VERSION_1: return "V-Max! v1 (Activision - CBM DOS, byte counting)";
        case VMAX_VERSION_2A: return "V-Max! v2a (Cinemaware - single EOR, CBM DOS)";
        case VMAX_VERSION_2B: return "V-Max! v2b (Cinemaware - dual EOR, custom sectors)";
        case VMAX_VERSION_3A: return "V-Max! v3a (Taito - variable sectors, normal syncs)";
        case VMAX_VERSION_3B: return "V-Max! v3b (Taito - variable sectors, short syncs)";
        case VMAX_VERSION_4: return "V-Max! v4 (4 marker bytes variant)";
        default: return "V-Max! (unknown version)";
    }
}

c64_vmax_version_t c64_detect_vmax_version(const uint8_t *data, size_t size,
                                            c64_protection_analysis_t *result) {
    if (!data || size < 12) return VMAX_VERSION_UNKNOWN;
    
    /* Check for G64 signature */
    if (memcmp(data, "GCR-1541", 8) != 0) {
        /* Not G64 - try D64 directory check */
        if (c64_check_vmax_directory(data, size)) {
            result->protection_flags |= C64_PROT_V_MAX;
            result->vmax_version = VMAX_VERSION_2B; /* Likely v2 based on directory */
            return VMAX_VERSION_2B;
        }
        return VMAX_VERSION_UNKNOWN;
    }
    
    /* G64 analysis for V-MAX detection */
    uint8_t num_tracks = data[9];
    
    /* Track offset table starts at byte 12 */
    const uint8_t *track_offsets = data + 12;
    
    /* Check track 20 (V-MAX loader track) */
    uint32_t track20_offset = 0;
    int track20_idx = 19 * 2; /* Track 20, full track (not half) */
    
    if (track20_idx < num_tracks) {
        track20_offset = track_offsets[track20_idx * 4] |
                        (track_offsets[track20_idx * 4 + 1] << 8) |
                        (track_offsets[track20_idx * 4 + 2] << 16) |
                        (track_offsets[track20_idx * 4 + 3] << 24);
    }
    
    if (track20_offset == 0 || track20_offset >= size) {
        return VMAX_VERSION_UNKNOWN;
    }
    
    /* Analyze track 20 for V-MAX signatures */
    uint16_t track20_size = data[track20_offset] | (data[track20_offset + 1] << 8);
    const uint8_t *track20_data = data + track20_offset + 2;
    
    if (track20_size < 100) return VMAX_VERSION_UNKNOWN;
    
    /* Look for V-MAX signatures */
    int sync_count = 0;
    int eor_sections = 0;
    int marker_49_count = 0;
    int marker_5a_count = 0;
    bool found_ee_marker = false;
    
    for (uint16_t i = 0; i < track20_size - 10 && (track20_offset + 2 + i + 10) < size; i++) {
        /* Count sync marks */
        if (track20_data[i] == 0xFF) {
            int sync_len = 0;
            while (i + sync_len < track20_size && track20_data[i + sync_len] == 0xFF) {
                sync_len++;
            }
            if (sync_len >= 5) sync_count++;
            i += sync_len - 1;
        }
        
        /* Count $5A runs (V-MAX v2 signature) */
        if (track20_data[i] == 0x5A) {
            marker_5a_count++;
        }
        
        /* Count $49 markers (V-MAX v3/v4 signature) */
        if (track20_data[i] == 0x49 && i + 1 < track20_size && 
            track20_data[i + 1] == 0x49) {
            marker_49_count++;
        }
        
        /* Check for $EE end-of-header marker (v3) */
        if (track20_data[i] == 0xEE) {
            found_ee_marker = true;
        }
    }
    
    /* Determine V-MAX version based on signatures */
    c64_vmax_version_t detected = VMAX_VERSION_UNKNOWN;
    
    if (marker_49_count > 5 && found_ee_marker) {
        /* V-MAX v3/v4 - check for short syncs */
        int short_sync_count = 0;
        for (uint16_t i = 0; i < track20_size - 5; i++) {
            if (track20_data[i] == 0xFF && track20_data[i + 1] != 0xFF &&
                (i == 0 || track20_data[i - 1] != 0xFF)) {
                short_sync_count++;
            }
        }
        
        if (short_sync_count > 10) {
            detected = VMAX_VERSION_3B;  /* Short syncs = v3b */
        } else {
            detected = VMAX_VERSION_3A;  /* Normal syncs = v3a */
        }
    } else if (marker_5a_count > 20) {
        /* V-MAX v2 - check for EOR sections */
        detected = VMAX_VERSION_2B;  /* Assume v2b (custom sectors) */
    } else if (sync_count > 0 && track20_size > 5000) {
        /* Could be v0 or v1 */
        detected = VMAX_VERSION_1;
    }
    
    if (detected != VMAX_VERSION_UNKNOWN && result) {
        result->protection_flags |= C64_PROT_V_MAX;
        result->vmax_version = detected;
        result->vmax_loader_blocks = (detected >= VMAX_VERSION_3A) ? 8 : 7;
        snprintf(result->protection_name, sizeof(result->protection_name),
                 "%s", c64_vmax_version_string(detected));
    }
    
    return detected;
}

bool c64_check_vmax_directory(const uint8_t *data, size_t size) {
    /* Check for V-MAX v2 "!" only directory signature */
    size_t dir_offset = c64_d64_get_sector_offset(18, 1); /* Directory starts at 18/1 */
    if (dir_offset == (size_t)-1 || dir_offset + 256 > size) return false;
    
    const uint8_t *dir = data + dir_offset;
    
    /* Check first directory entry */
    /* Entry starts at offset 2, filename at offset 5 */
    if (dir[2] == 0x82 && dir[5] == '!' && dir[6] == 0xA0) {
        /* File type PRG, name is "!" padded with shifted space */
        return true;
    }
    
    return false;
}

/* ============================================================================
 * RapidLok Version Detection
 * Based on Pete Rittwage, Banguibob, and Kracker Jax documentation
 * ============================================================================ */

const char *c64_rapidlok_version_string(c64_rapidlok_version_t version) {
    switch (version) {
        case RAPIDLOK_VERSION_1: return "RapidLok v1 (patch keycheck works)";
        case RAPIDLOK_VERSION_2: return "RapidLok v2 (patch keycheck works)";
        case RAPIDLOK_VERSION_3: return "RapidLok v3 (patch keycheck works)";
        case RAPIDLOK_VERSION_4: return "RapidLok v4 (patch keycheck works)";
        case RAPIDLOK_VERSION_5: return "RapidLok v5 (intermittent in VICE)";
        case RAPIDLOK_VERSION_6: return "RapidLok v6 (intermittent in VICE)";
        case RAPIDLOK_VERSION_7: return "RapidLok v7 (requires additional crack)";
        default: return "RapidLok (unknown version)";
    }
}

c64_rapidlok_version_t c64_detect_rapidlok_version(const uint8_t *data, size_t size,
                                                    c64_protection_analysis_t *result) {
    if (!data || size < 12) return RAPIDLOK_VERSION_UNKNOWN;
    
    /* Check for G64 signature */
    if (memcmp(data, "GCR-1541", 8) != 0) {
        return RAPIDLOK_VERSION_UNKNOWN;
    }
    
    uint8_t num_tracks = data[9];
    const uint8_t *track_offsets = data + 12;
    
    /* Check for track 36 (RapidLok key track) */
    int track36_idx = 35 * 2; /* Track 36, full track */
    if (track36_idx >= num_tracks) {
        return RAPIDLOK_VERSION_UNKNOWN;
    }
    
    uint32_t track36_offset = track_offsets[track36_idx * 4] |
                              (track_offsets[track36_idx * 4 + 1] << 8) |
                              (track_offsets[track36_idx * 4 + 2] << 16) |
                              (track_offsets[track36_idx * 4 + 3] << 24);
    
    if (track36_offset == 0 || track36_offset >= size) {
        return RAPIDLOK_VERSION_UNKNOWN;
    }
    
    /* Analyze track 36 for RapidLok key sector */
    uint16_t track36_size = data[track36_offset] | (data[track36_offset + 1] << 8);
    const uint8_t *track36_data = data + track36_offset + 2;
    
    if (track36_size < 100) return RAPIDLOK_VERSION_UNKNOWN;
    
    /* Look for RapidLok signatures */
    bool found_long_sync = false;
    bool found_key_data = false;
    bool found_bad_gcr = false;
    int sync_bits = 0;
    
    /* Count initial sync length */
    for (uint16_t i = 0; i < track36_size && track36_data[i] == 0xFF; i++) {
        sync_bits += 8;
    }
    
    if (sync_bits >= 40) {
        found_long_sync = true;
    }
    
    /* Look for key data pattern and bad GCR ($00) */
    for (uint16_t i = 0; i < track36_size - 10 && (track36_offset + 2 + i + 10) < size; i++) {
        if (track36_data[i] == 0x00) {
            found_bad_gcr = true;
        }
        
        /* Look for encoded key bytes after sync */
        if (track36_data[i] != 0xFF && track36_data[i] != 0x00 &&
            i > 5 && track36_data[i - 5] == 0xFF) {
            found_key_data = true;
        }
    }
    
    if (!found_long_sync || !found_key_data) {
        return RAPIDLOK_VERSION_UNKNOWN;
    }
    
    /* Now check other tracks for RapidLok signatures */
    int rapidlok_track_count = 0;
    int total_7b_count = 0;
    
    for (int track = 1; track <= 35; track++) {
        int track_idx = (track - 1) * 2;
        if (track_idx >= num_tracks) continue;
        
        uint32_t offset = track_offsets[track_idx * 4] |
                         (track_offsets[track_idx * 4 + 1] << 8) |
                         (track_offsets[track_idx * 4 + 2] << 16) |
                         (track_offsets[track_idx * 4 + 3] << 24);
        
        if (offset == 0 || offset >= size) continue;
        
        uint16_t track_size = data[offset] | (data[offset + 1] << 8);
        const uint8_t *track_data = data + offset + 2;
        
        /* Count $7B bytes (extra sector signature) */
        int count_7b = 0;
        for (uint16_t i = 0; i < track_size && (offset + 2 + i) < size; i++) {
            if (track_data[i] == RAPIDLOK_EXTRA_SECTOR) {
                count_7b++;
            }
        }
        
        if (count_7b > 10) {
            rapidlok_track_count++;
            total_7b_count += count_7b;
            if (result && track <= 35) {
                result->rapidlok_7b_counts[track] = count_7b;
            }
        }
        
        /* Look for $75 (sector header) and $6B (data block) markers */
        bool found_75 = false;
        bool found_6b = false;
        for (uint16_t i = 0; i < track_size - 1 && (offset + 2 + i + 1) < size; i++) {
            if (track_data[i] == RAPIDLOK_SECTOR_HEADER) found_75 = true;
            if (track_data[i] == RAPIDLOK_DATA_BLOCK) found_6b = true;
        }
        
        if (found_75 && found_6b) {
            rapidlok_track_count++;
        }
    }
    
    if (rapidlok_track_count < 5) {
        return RAPIDLOK_VERSION_UNKNOWN;
    }
    
    /* Determine RapidLok version based on characteristics */
    c64_rapidlok_version_t detected = RAPIDLOK_VERSION_UNKNOWN;
    
    /* Version detection heuristics based on sync lengths and structure */
    if (total_7b_count > 500) {
        /* Later versions have more complex $7B patterns */
        detected = RAPIDLOK_VERSION_6;  /* Common in MicroProse games like Pirates! */
    } else if (total_7b_count > 300) {
        detected = RAPIDLOK_VERSION_5;
    } else if (rapidlok_track_count > 20) {
        detected = RAPIDLOK_VERSION_4;
    } else {
        detected = RAPIDLOK_VERSION_3;
    }
    
    if (result) {
        result->protection_flags |= C64_PROT_RAPIDLOK | C64_PROT_EXTRA_TRACKS;
        result->rapidlok_version = detected;
        result->rapidlok_key_valid = found_key_data;
        result->rapidlok_sync_track_start = sync_bits;
        snprintf(result->protection_name, sizeof(result->protection_name),
                 "%s", c64_rapidlok_version_string(detected));
    }
    
    return detected;
}

bool c64_extract_rapidlok_key(const uint8_t *data, size_t size, uint8_t *key_table) {
    if (!data || !key_table || size < 12) return false;
    
    /* Check for G64 signature */
    if (memcmp(data, "GCR-1541", 8) != 0) return false;
    
    uint8_t num_tracks = data[9];
    const uint8_t *track_offsets = data + 12;
    
    /* Get track 36 offset */
    int track36_idx = 35 * 2;
    if (track36_idx >= num_tracks) return false;
    
    uint32_t track36_offset = track_offsets[track36_idx * 4] |
                              (track_offsets[track36_idx * 4 + 1] << 8) |
                              (track_offsets[track36_idx * 4 + 2] << 16) |
                              (track_offsets[track36_idx * 4 + 3] << 24);
    
    if (track36_offset == 0 || track36_offset >= size) return false;
    
    uint16_t track36_size = data[track36_offset] | (data[track36_offset + 1] << 8);
    const uint8_t *track36_data = data + track36_offset + 2;
    
    /* Find key data after sync mark */
    int key_start = 0;
    for (uint16_t i = 0; i < track36_size && track36_data[i] == 0xFF; i++) {
        key_start = i + 1;
    }
    
    if (key_start == 0 || key_start + 35 > track36_size) return false;
    
    /* Extract 35 key values (one per track) */
    /* Note: In real RapidLok, this is encrypted/encoded */
    for (int i = 0; i < 35 && (key_start + i) < track36_size; i++) {
        key_table[i] = track36_data[key_start + i];
    }
    
    return true;
}

/* ============================================================================
 * Datasoft Long Track Protection Detection
 * Technical: Uses tracks with more data than normal (6680 bytes vs 6500)
 * Titles: Bruce Lee, Mr. Do!, Dig Dug, Pac-Man, Conan, etc.
 * ============================================================================ */

bool c64_detect_datasoft(const uint8_t *data, size_t size,
                          c64_protection_analysis_t *result) {
    if (!data || size < 12) return false;
    
    /* Check for G64 signature */
    if (memcmp(data, "GCR-1541", 8) != 0) {
        /* For D64, check directory for Datasoft titles */
        return c64_detect_datasoft_d64(data, size, result);
    }
    
    /* G64 analysis - look for long tracks */
    uint8_t num_tracks = data[9];
    const uint8_t *track_offsets = data + 12;
    
    int long_track_count = 0;
    int max_track_bytes = 0;
    
    for (int track = 1; track <= 35; track++) {
        int track_idx = (track - 1) * 2;
        if (track_idx >= num_tracks) continue;
        
        uint32_t offset = track_offsets[track_idx * 4] |
                         (track_offsets[track_idx * 4 + 1] << 8) |
                         (track_offsets[track_idx * 4 + 2] << 16) |
                         (track_offsets[track_idx * 4 + 3] << 24);
        
        if (offset == 0 || offset >= size) continue;
        
        uint16_t track_size = data[offset] | (data[offset + 1] << 8);
        
        /* Standard track sizes by zone:
         * Zone 1 (tracks 1-17):  ~7692 bytes
         * Zone 2 (tracks 18-24): ~7142 bytes
         * Zone 3 (tracks 25-30): ~6666 bytes
         * Zone 4 (tracks 31-35): ~6250 bytes
         */
        int expected_max;
        if (track <= 17) expected_max = 7800;
        else if (track <= 24) expected_max = 7250;
        else if (track <= 30) expected_max = 6750;
        else expected_max = 6350;
        
        if (track_size > expected_max) {
            long_track_count++;
            if (track_size > max_track_bytes) {
                max_track_bytes = track_size;
            }
        }
    }
    
    /* Datasoft protection uses tracks with > 6680 bytes (vs ~6500 normal) */
    if (long_track_count >= 3 && max_track_bytes > DATASOFT_LONG_TRACK_BYTES - 200) {
        if (result) {
            result->protection_flags |= C64_PROT_DATASOFT | C64_PROT_GCR_LONG_TRACK;
            result->confidence = 75 + (long_track_count * 2);
            if (result->confidence > 95) result->confidence = 95;
            snprintf(result->protection_name, sizeof(result->protection_name),
                     "Datasoft Long Track (%d tracks, max %d bytes)", 
                     long_track_count, max_track_bytes);
        }
        return true;
    }
    
    return false;
}

bool c64_detect_datasoft_d64(const uint8_t *data, size_t size,
                              c64_protection_analysis_t *result) {
    /* For D64 images, we can't detect long tracks directly
     * Instead check directory for known Datasoft titles and patterns */
    
    if (size < D64_35_TRACKS) return false;
    
    /* Check directory track 18 for Datasoft patterns */
    size_t dir_offset = c64_d64_get_sector_offset(18, 1);
    if (dir_offset == (size_t)-1 || dir_offset + 256 > size) return false;
    
    /* Check BAM for Datasoft signature patterns */
    size_t bam_offset = c64_d64_get_sector_offset(18, 0);
    if (bam_offset == (size_t)-1 || bam_offset + 256 > size) return false;
    
    const uint8_t *bam = data + bam_offset;
    
    /* Datasoft often has specific BAM patterns */
    /* Check disk name offset 0x90 for "DATASOFT" or known titles */
    char disk_name[17];
    memcpy(disk_name, bam + 0x90, 16);
    disk_name[16] = '\0';
    
    /* Check for known Datasoft disk names */
    const char *datasoft_names[] = {
        "BRUCE LEE", "MR. DO", "DIG DUG", "PAC-MAN", "CONAN",
        "POLE POSITION", "ZAXXON", "POOYAN", "AZTEC", "GOONIES",
        "DALLAS", "ALTERNATE", NULL
    };
    
    for (int i = 0; datasoft_names[i] != NULL; i++) {
        if (strstr(disk_name, datasoft_names[i]) != NULL) {
            if (result) {
                result->protection_flags |= C64_PROT_DATASOFT;
                result->publisher = C64_PUB_DATASOFT;
                result->confidence = 80;
                snprintf(result->protection_name, sizeof(result->protection_name),
                         "Datasoft (detected from disk name)");
            }
            return true;
        }
    }
    
    return false;
}

/* ============================================================================
 * SSI RapidDOS Protection Detection
 * Technical: Custom DOS with track 36 key, 10 sectors per track
 * Titles: Pool of Radiance, Curse of Azure Bonds, War games
 * ============================================================================ */

bool c64_detect_ssi_rdos(const uint8_t *data, size_t size,
                          c64_protection_analysis_t *result) {
    if (!data || size < 12) return false;
    
    bool is_g64 = (memcmp(data, "GCR-1541", 8) == 0);
    
    if (is_g64) {
        return c64_detect_ssi_rdos_g64(data, size, result);
    } else {
        return c64_detect_ssi_rdos_d64(data, size, result);
    }
}

bool c64_detect_ssi_rdos_g64(const uint8_t *data, size_t size,
                              c64_protection_analysis_t *result) {
    /* G64 analysis for SSI RapidDOS */
    uint8_t num_tracks = data[9];
    const uint8_t *track_offsets = data + 12;
    
    /* Check for track 36 (SSI key track) */
    int track36_idx = 35 * 2;
    if (track36_idx >= num_tracks) return false;
    
    uint32_t track36_offset = track_offsets[track36_idx * 4] |
                              (track_offsets[track36_idx * 4 + 1] << 8) |
                              (track_offsets[track36_idx * 4 + 2] << 16) |
                              (track_offsets[track36_idx * 4 + 3] << 24);
    
    if (track36_offset == 0 || track36_offset >= size) return false;
    
    uint16_t track36_size = data[track36_offset] | (data[track36_offset + 1] << 8);
    const uint8_t *track36_data = data + track36_offset + 2;
    
    /* Look for SSI RapidDOS signatures */
    /* SSI uses custom header marker $4B instead of standard $08 */
    int ssi_header_count = 0;
    bool found_key_pattern = false;
    
    for (uint16_t i = 0; i < track36_size - 10 && (track36_offset + 2 + i + 10) < size; i++) {
        /* Look for SSI custom header marker */
        if (track36_data[i] == SSI_RDOS_HEADER_MARKER) {
            ssi_header_count++;
        }
        
        /* SSI key pattern: specific byte sequence after sync */
        if (i > 5 && track36_data[i - 1] == 0xFF && 
            track36_data[i] != 0xFF && track36_data[i] != 0x00) {
            /* Check for 10-sector structure */
            int sector_count = 0;
            for (uint16_t j = i; j < track36_size - 1 && j < i + 3000; j++) {
                if (track36_data[j] == 0x08 || track36_data[j] == SSI_RDOS_HEADER_MARKER) {
                    sector_count++;
                }
            }
            if (sector_count >= 8 && sector_count <= 12) {
                found_key_pattern = true;
            }
        }
    }
    
    /* Check other tracks for 10-sector structure (instead of standard 17-21) */
    int tracks_with_10_sectors = 0;
    
    for (int track = 1; track <= 35; track++) {
        int track_idx = (track - 1) * 2;
        if (track_idx >= num_tracks) continue;
        
        uint32_t offset = track_offsets[track_idx * 4] |
                         (track_offsets[track_idx * 4 + 1] << 8) |
                         (track_offsets[track_idx * 4 + 2] << 16) |
                         (track_offsets[track_idx * 4 + 3] << 24);
        
        if (offset == 0 || offset >= size) continue;
        
        uint16_t track_size = data[offset] | (data[offset + 1] << 8);
        const uint8_t *track_data = data + offset + 2;
        
        /* Count sector headers (standard $08 or custom) */
        int sector_headers = 0;
        for (uint16_t i = 0; i < track_size - 5 && (offset + 2 + i + 5) < size; i++) {
            /* Standard sector header after sync: FF FF FF FF FF 08 */
            if (track_data[i] == 0x08 && i >= 5 && track_data[i - 1] == 0xFF) {
                sector_headers++;
            }
        }
        
        /* SSI RapidDOS uses 10 sectors per track instead of standard */
        if (sector_headers >= 9 && sector_headers <= 11) {
            tracks_with_10_sectors++;
        }
    }
    
    if ((found_key_pattern && tracks_with_10_sectors >= 5) || ssi_header_count >= 5) {
        if (result) {
            result->protection_flags |= C64_PROT_SSI_RDOS | C64_PROT_EXTRA_TRACKS;
            result->publisher = C64_PUB_SSI;
            result->confidence = 80 + (tracks_with_10_sectors / 2);
            if (result->confidence > 95) result->confidence = 95;
            snprintf(result->protection_name, sizeof(result->protection_name),
                     "SSI RapidDOS (10 sectors/track, track 36 key)");
        }
        return true;
    }
    
    return false;
}

bool c64_detect_ssi_rdos_d64(const uint8_t *data, size_t size,
                              c64_protection_analysis_t *result) {
    /* For D64, check for SSI titles in directory or known patterns */
    if (size < D64_35_TRACKS) return false;
    
    /* Check BAM for SSI disk name patterns */
    size_t bam_offset = c64_d64_get_sector_offset(18, 0);
    if (bam_offset == (size_t)-1 || bam_offset + 256 > size) return false;
    
    const uint8_t *bam = data + bam_offset;
    char disk_name[17];
    memcpy(disk_name, bam + 0x90, 16);
    disk_name[16] = '\0';
    
    /* Check for known SSI disk names */
    const char *ssi_names[] = {
        "POOL OF RAD", "CURSE", "AZURE", "CHAMPIONS", "KRYNN",
        "DEATH KNIGHT", "GATEWAY", "SAVAGE", "QUESTRON", 
        "PANZER", "KAMPFGRUPPE", "CARRIER", "ROADWAR",
        "GETTYSBURG", "ANTIETAM", "SHILOH", "NAM", NULL
    };
    
    for (int i = 0; ssi_names[i] != NULL; i++) {
        if (strstr(disk_name, ssi_names[i]) != NULL) {
            if (result) {
                result->protection_flags |= C64_PROT_SSI_RDOS;
                result->publisher = C64_PUB_SSI;
                result->confidence = 75;
                snprintf(result->protection_name, sizeof(result->protection_name),
                         "SSI RapidDOS (detected from disk name)");
            }
            return true;
        }
    }
    
    /* Check for 40-track D64 (SSI often uses extended tracks) */
    if (size >= D64_40_TRACKS) {
        /* Has extended tracks - more likely to be SSI */
        /* Additional heuristic: check if tracks 36-40 have data */
        bool has_track_36_data = false;
        for (int track = 36; track <= 40; track++) {
            size_t track_offset = c64_d64_get_sector_offset(track, 0);
            if (track_offset != (size_t)-1 && track_offset + 256 <= size) {
                const uint8_t *sector = data + track_offset;
                /* Check if sector has non-zero data */
                for (int i = 0; i < 256; i++) {
                    if (sector[i] != 0x00 && sector[i] != 0x01) {
                        has_track_36_data = true;
                        break;
                    }
                }
            }
        }
        
        if (has_track_36_data) {
            if (result) {
                result->protection_flags |= C64_PROT_SSI_RDOS | C64_PROT_EXTRA_TRACKS;
                result->publisher = C64_PUB_SSI;
                result->confidence = 60;
                snprintf(result->protection_name, sizeof(result->protection_name),
                         "SSI RapidDOS (40-track image with extended data)");
            }
            return true;
        }
    }
    
    return false;
}

/* ============================================================================
 * EA Interlock Protection Detection
 * Technical: Custom DOS with interleave and specific boot sequence
 * Titles: Bard's Tale, Archon, Seven Cities of Gold, etc.
 * ============================================================================ */

bool c64_detect_ea_interlock(const uint8_t *data, size_t size,
                              c64_protection_analysis_t *result) {
    if (!data || size < D64_35_TRACKS) return false;
    
    /* Check BAM for EA disk patterns */
    size_t bam_offset = c64_d64_get_sector_offset(18, 0);
    if (bam_offset == (size_t)-1 || bam_offset + 256 > size) return false;
    
    const uint8_t *bam = data + bam_offset;
    
    /* EA Interlock characteristics:
     * 1. Specific disk ID patterns
     * 2. Custom sector interleave
     * 3. Boot sector at track 1, sector 0 with EA signature
     */
    
    /* Check boot sector for EA signature */
    size_t boot_offset = c64_d64_get_sector_offset(1, 0);
    if (boot_offset == (size_t)-1 || boot_offset + 256 > size) return false;
    
    const uint8_t *boot = data + boot_offset;
    
    /* EA boot sectors often have specific patterns */
    bool found_ea_boot = false;
    
    /* Look for "EA" or Electronic Arts signatures in boot sector */
    for (int i = 0; i < 200; i++) {
        if (boot[i] == 'E' && boot[i + 1] == 'A' && boot[i + 2] == ' ') {
            found_ea_boot = true;
            break;
        }
        /* Also check for common EA loader patterns */
        if (boot[i] == 0x4C && (boot[i + 2] == 0x08 || boot[i + 2] == 0x03)) {
            /* JMP instruction to common EA loader address */
            found_ea_boot = true;
        }
    }
    
    /* Check disk name for EA titles */
    char disk_name[17];
    memcpy(disk_name, bam + 0x90, 16);
    disk_name[16] = '\0';
    
    const char *ea_names[] = {
        "ARCHON", "BARD", "SEVEN CITIES", "STARFLIGHT", "SKYFOX",
        "MULE", "MAIL ORDER", "RACING DEST", "MOVIE MAKER",
        "MARBLE", "WASTELAND", "ULTIMA", "CHUCK YEAGER",
        "MADDEN", "EARL WEAVER", "DELUXE", NULL
    };
    
    bool found_ea_name = false;
    for (int i = 0; ea_names[i] != NULL; i++) {
        if (strstr(disk_name, ea_names[i]) != NULL) {
            found_ea_name = true;
            break;
        }
    }
    
    /* Check sector interleave pattern (EA uses non-standard interleave) */
    int interleave_anomalies = 0;
    size_t dir_offset = c64_d64_get_sector_offset(18, 1);
    if (dir_offset != (size_t)-1 && dir_offset + 256 <= size) {
        const uint8_t *dir = data + dir_offset;
        
        /* Check directory chain for non-standard sector links */
        int prev_sector = 1;
        int curr_track = 18;
        int curr_sector = dir[0] == 18 ? dir[1] : -1;
        
        while (curr_sector > 0 && curr_sector < 19) {
            int expected_next = (prev_sector + 10) % 19; /* Standard interleave 10 */
            if (curr_sector != expected_next && curr_sector != (prev_sector + 1) % 19) {
                interleave_anomalies++;
            }
            
            size_t next_offset = c64_d64_get_sector_offset(curr_track, curr_sector);
            if (next_offset == (size_t)-1 || next_offset + 256 > size) break;
            
            prev_sector = curr_sector;
            const uint8_t *next_sector = data + next_offset;
            curr_track = next_sector[0];
            curr_sector = next_sector[1];
            
            if (curr_track != 18) break;
        }
    }
    
    if (found_ea_boot || found_ea_name || interleave_anomalies >= 3) {
        if (result) {
            result->protection_flags |= C64_PROT_EA_INTERLOCK;
            result->publisher = C64_PUB_ELECTRONIC_ARTS;
            result->confidence = 60;
            if (found_ea_boot) result->confidence += 15;
            if (found_ea_name) result->confidence += 15;
            if (interleave_anomalies >= 3) result->confidence += 10;
            if (result->confidence > 95) result->confidence = 95;
            snprintf(result->protection_name, sizeof(result->protection_name),
                     "EA Interlock");
        }
        return true;
    }
    
    return false;
}

/* ============================================================================
 * Novaload Protection Detection (Tape, but also disk variants)
 * Technical: Fast loader with anti-tampering, stack manipulation
 * Titles: Combat School, Target Renegade, Gryzor, etc. (Ocean/Imagine)
 * ============================================================================ */

bool c64_detect_novaload(const uint8_t *data, size_t size,
                          c64_protection_analysis_t *result) {
    if (!data || size < D64_35_TRACKS) return false;
    
    /* Novaload is primarily a tape protection, but disk versions exist
     * Key characteristics:
     * 1. Specific loader signature in boot sector
     * 2. Stack manipulation code
     * 3. Self-wiping on tampering
     */
    
    /* Check boot sector for Novaload loader */
    size_t boot_offset = c64_d64_get_sector_offset(1, 0);
    if (boot_offset == (size_t)-1 || boot_offset + 256 > size) return false;
    
    const uint8_t *boot = data + boot_offset;
    
    bool found_novaload = false;
    int novaload_score = 0;
    
    /* Look for Novaload signature patterns */
    /* Novaload often uses these opcodes for its fast loader:
     * LDA #$xx, STA $01 - switch memory config
     * SEI - disable interrupts
     * LDA $DD00, AND #$03, ORA #$04, STA $DD00 - set VIC bank
     */
    
    for (int i = 0; i < 200; i++) {
        /* SEI instruction (disable interrupts) - common in Novaload */
        if (boot[i] == 0x78) novaload_score++;
        
        /* STA $01 (memory config) after LDA #xx */
        if (boot[i] == 0x85 && boot[i + 1] == 0x01 && i > 0 && boot[i - 2] == 0xA9) {
            novaload_score += 2;
        }
        
        /* LDA $DD00 (VIC bank register) */
        if (boot[i] == 0xAD && boot[i + 1] == 0x00 && boot[i + 2] == 0xDD) {
            novaload_score += 2;
        }
        
        /* Stack pointer manipulation: TXS after LDX #$xx */
        if (boot[i] == 0x9A && i > 0 && boot[i - 2] == 0xA2) {
            novaload_score += 3;  /* Stack manipulation is key Novaload feature */
        }
        
        /* Look for "NOVALOAD" or "NOVA" string */
        if (i < 196 && boot[i] == 'N' && boot[i + 1] == 'O' && 
            boot[i + 2] == 'V' && boot[i + 3] == 'A') {
            found_novaload = true;
            novaload_score += 10;
        }
    }
    
    /* Check disk name for known Novaload/Ocean titles */
    size_t bam_offset = c64_d64_get_sector_offset(18, 0);
    if (bam_offset != (size_t)-1 && bam_offset + 256 <= size) {
        const uint8_t *bam = data + bam_offset;
        char disk_name[17];
        memcpy(disk_name, bam + 0x90, 16);
        disk_name[16] = '\0';
        
        const char *novaload_names[] = {
            "COMBAT SCHOOL", "TARGET RENE", "GRYZOR", "HEAD OVER",
            "GREEN BERET", "YIE AR", "IMAGINE", "OCEAN", NULL
        };
        
        for (int i = 0; novaload_names[i] != NULL; i++) {
            if (strstr(disk_name, novaload_names[i]) != NULL) {
                novaload_score += 5;
                break;
            }
        }
    }
    
    /* Detection threshold */
    if (found_novaload || novaload_score >= 8) {
        if (result) {
            result->protection_flags |= C64_PROT_NOVALOAD;
            result->publisher = C64_PUB_OCEAN;
            result->confidence = 50 + novaload_score * 3;
            if (result->confidence > 95) result->confidence = 95;
            snprintf(result->protection_name, sizeof(result->protection_name),
                     "Novaload (fast loader with anti-tampering)");
        }
        return true;
    }
    
    return false;
}

/* ============================================================================
 * Speedlock Protection Detection
 * Technical: Custom loader with encrypted code, timing checks
 * Titles: Many Ocean, US Gold titles
 * ============================================================================ */

bool c64_detect_speedlock(const uint8_t *data, size_t size,
                           c64_protection_analysis_t *result) {
    if (!data || size < D64_35_TRACKS) return false;
    
    /* Check boot sector for Speedlock loader signatures */
    size_t boot_offset = c64_d64_get_sector_offset(1, 0);
    if (boot_offset == (size_t)-1 || boot_offset + 256 > size) return false;
    
    const uint8_t *boot = data + boot_offset;
    
    int speedlock_score = 0;
    
    /* Speedlock characteristics:
     * 1. Decryption loop at start
     * 2. Timing-based checks
     * 3. Specific byte patterns
     */
    
    for (int i = 0; i < 200; i++) {
        /* EOR (Exclusive OR) decryption - common in Speedlock */
        if (boot[i] == 0x49 || boot[i] == 0x45 || boot[i] == 0x55) {
            /* EOR #xx, EOR zp, EOR zp,X */
            speedlock_score++;
        }
        
        /* DEY/DEX in tight loop (decryption counter) */
        if ((boot[i] == 0x88 || boot[i] == 0xCA) && 
            i > 0 && (boot[i - 1] == 0xD0 || boot[i + 1] == 0xD0)) {
            speedlock_score += 2;
        }
        
        /* CIA timer access ($DC04-$DC05) for timing checks */
        if (boot[i] == 0xAD && boot[i + 2] == 0xDC &&
            (boot[i + 1] == 0x04 || boot[i + 1] == 0x05)) {
            speedlock_score += 3;
        }
    }
    
    /* Check disk name for known Speedlock/Ocean/US Gold titles */
    size_t bam_offset = c64_d64_get_sector_offset(18, 0);
    if (bam_offset != (size_t)-1 && bam_offset + 256 <= size) {
        const uint8_t *bam = data + bam_offset;
        char disk_name[17];
        memcpy(disk_name, bam + 0x90, 16);
        disk_name[16] = '\0';
        
        const char *speedlock_names[] = {
            "GAUNTLET", "ROAD RUNNER", "720", "INDIANA", "IKARI",
            "COMMANDO", "GHOSTS", "1942", "1943", "BIONIC",
            "WIZBALL", "TRANSFORMERS", "TERMINATOR", NULL
        };
        
        for (int i = 0; speedlock_names[i] != NULL; i++) {
            if (strstr(disk_name, speedlock_names[i]) != NULL) {
                speedlock_score += 5;
                break;
            }
        }
    }
    
    if (speedlock_score >= 7) {
        if (result) {
            result->protection_flags |= C64_PROT_SPEEDLOCK;
            result->publisher = C64_PUB_US_GOLD;
            result->confidence = 50 + speedlock_score * 3;
            if (result->confidence > 95) result->confidence = 95;
            snprintf(result->protection_name, sizeof(result->protection_name),
                     "Speedlock (encrypted loader with timing checks)");
        }
        return true;
    }
    
    return false;
}
