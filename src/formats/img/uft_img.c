/**
 * @file uft_img.c
 * @brief UnifiedFloppyTool - IMG/IMA Format Plugin
 * 
 * Generic PC disk image format:
 * - Flat file mit Sektoren in CHS-Reihenfolge
 * - Unterstützt 160KB bis 2.88MB
 * - Sector interleave: S0-S8 (oder S0-S17 etc.) pro Track
 * 
 * @author UFT Team
 * @date 2025
 */

#include "uft/uft_format_plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// IMG Constants
// ============================================================================

#define IMG_SECTOR_SIZE  512

// Bekannte Größen
typedef struct {
    size_t size;
    uint16_t cylinders;
    uint16_t heads;
    uint16_t sectors;
    const char* name;
} img_geometry_entry_t;

static const img_geometry_entry_t known_geometries[] = {
    { 163840,   40, 1,  8, "160KB 5.25\" SS/DD" },
    { 184320,   40, 1,  9, "180KB 5.25\" SS/DD" },
    { 327680,   40, 2,  8, "320KB 5.25\" DS/DD" },
    { 368640,   40, 2,  9, "360KB 5.25\" DS/DD" },
    { 737280,   80, 2,  9, "720KB 3.5\" DS/DD" },
    { 1228800,  80, 2, 15, "1.2MB 5.25\" DS/HD" },
    { 1474560,  80, 2, 18, "1.44MB 3.5\" DS/HD" },
    { 1720320,  80, 2, 21, "1.68MB 3.5\" DMF" },
    { 2949120,  80, 2, 36, "2.88MB 3.5\" DS/ED" },
    { 0, 0, 0, 0, NULL }
};

// ============================================================================
// Plugin Data
// ============================================================================

typedef struct {
    FILE*    file;
    size_t   file_size;
} img_data_t;

// ============================================================================
// Geometry Detection
// ============================================================================

static bool img_detect_geometry(size_t file_size, uft_geometry_t* geo) {
    // Exakte Matches
    for (int i = 0; known_geometries[i].size != 0; i++) {
        if (file_size == known_geometries[i].size) {
            geo->cylinders = known_geometries[i].cylinders;
            geo->heads = known_geometries[i].heads;
            geo->sectors = known_geometries[i].sectors;
            geo->sector_size = IMG_SECTOR_SIZE;
            geo->total_sectors = geo->cylinders * geo->heads * geo->sectors;
            geo->double_step = false;
            return true;
        }
    }
    
    // Generisch: Versuche Geometrie zu erraten
    if (file_size % IMG_SECTOR_SIZE != 0) {
        return false;  // Muss durch Sektorgröße teilbar sein
    }
    
    size_t total_sectors = file_size / IMG_SECTOR_SIZE;
    
    // Typische Werte probieren
    static const int sectors_options[] = { 18, 9, 15, 36, 21, 8, 10 };
    static const int heads_options[] = { 2, 1 };
    
    for (int h = 0; h < 2; h++) {
        for (int s = 0; s < 7; s++) {
            int heads = heads_options[h];
            int sectors = sectors_options[s];
            
            if (total_sectors % (heads * sectors) == 0) {
                int cylinders = total_sectors / (heads * sectors);
                if (cylinders >= 35 && cylinders <= 84) {
                    geo->cylinders = cylinders;
                    geo->heads = heads;
                    geo->sectors = sectors;
                    geo->sector_size = IMG_SECTOR_SIZE;
                    geo->total_sectors = total_sectors;
                    geo->double_step = (cylinders == 40);
                    return true;
                }
            }
        }
    }
    
    return false;
}

// ============================================================================
// Probe
// ============================================================================

bool img_probe(const uint8_t* data, size_t size, size_t file_size, 
                      int* confidence) {
    *confidence = 0;
    
    uft_geometry_t geo;
    if (!img_detect_geometry(file_size, &geo)) {
        return false;
    }
    
    *confidence = 40;  // Größe passt
    
    // FAT12 Bootsektor prüfen
    if (size >= 512) {
        // Jump instruction
        if (data[0] == 0xEB || data[0] == 0xE9) {
            *confidence = 60;
        }
        
        // Boot signature
        if (data[510] == 0x55 && data[511] == 0xAA) {
            *confidence = 80;
        }
        
        // OEM Name prüfen (Bytes 3-10)
        bool has_oem = true;
        for (int i = 3; i < 11; i++) {
            if (data[i] < 0x20 || data[i] > 0x7E) {
                has_oem = false;
                break;
            }
        }
        if (has_oem) {
            *confidence = 85;
        }
        
        // Bytes per sector (sollte 512 sein)
        uint16_t bps = data[11] | (data[12] << 8);
        if (bps == 512) {
            *confidence = 90;
        }
    }
    
    return *confidence > 0;
}

// ============================================================================
// Open
// ============================================================================

static uft_error_t img_open(uft_disk_t* disk, const char* path, bool read_only) {
    FILE* f = fopen(path, read_only ? "rb" : "r+b");
    if (!f) {
        return UFT_ERROR_FILE_OPEN;
    }
    
    // Größe ermitteln - H1 FIX: ftell() Fehlerprüfung
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return UFT_ERROR_FILE_SEEK;
    }
    long pos = ftell(f);
    if (pos < 0) {
        fclose(f);
        return UFT_ERROR_FILE_SEEK;
    }
    size_t file_size = (size_t)pos;
    
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return UFT_ERROR_FILE_SEEK;
    }
    
    // Geometrie ermitteln
    if (!img_detect_geometry(file_size, &disk->geometry)) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }
    
    // Plugin-Daten
    img_data_t* pdata = calloc(1, sizeof(img_data_t));
    if (!pdata) {
        fclose(f);
        return UFT_ERROR_NO_MEMORY;
    }
    
    pdata->file = f;
    pdata->file_size = file_size;
    disk->plugin_data = pdata;
    
    return UFT_OK;
}

// ============================================================================
// Close
// ============================================================================

static void img_close(uft_disk_t* disk) {
    if (!disk || !disk->plugin_data) return;
    
    img_data_t* pdata = disk->plugin_data;
    if (pdata->file) {
        fclose(pdata->file);
    }
    
    free(pdata);
    disk->plugin_data = NULL;
}

// ============================================================================
// Create
// ============================================================================

static uft_error_t img_create(uft_disk_t* disk, const char* path,
                               const uft_geometry_t* geometry) {
    if (geometry->sector_size != 512) {
        return UFT_ERROR_INVALID_ARG;
    }
    
    FILE* f = fopen(path, "wb");
    if (!f) {
        return UFT_ERROR_FILE_OPEN;
    }
    
    // Mit Nullen füllen
    size_t total_size = (size_t)geometry->cylinders * geometry->heads * 
                        geometry->sectors * geometry->sector_size;
    
    uint8_t zero[512] = {0};
    size_t sectors = total_size / 512;
    
    for (size_t i = 0; i < sectors; i++) {
        if (fwrite(zero, 512, 1, f) != 1) {
            fclose(f);
            return UFT_ERROR_FILE_WRITE;
        }
    }
    
    fclose(f);
    
    return img_open(disk, path, false);
}

// ============================================================================
// Flush
// ============================================================================

static uft_error_t img_flush(uft_disk_t* disk) {
    if (!disk || !disk->plugin_data) return UFT_ERROR_NULL_POINTER;
    
    img_data_t* pdata = disk->plugin_data;
    if (pdata->file) {
        fflush(pdata->file);
    }
    
    return UFT_OK;
}

// ============================================================================
// Read Track
// ============================================================================

static uft_error_t img_read_track(uft_disk_t* disk, int cylinder, int head,
                                   uft_track_t* track) {
    if (!disk || !track) return UFT_ERROR_NULL_POINTER;
    
    img_data_t* pdata = disk->plugin_data;
    if (!pdata || !pdata->file) return UFT_ERROR_FILE_READ;
    
    uft_track_init(track, cylinder, head);
    
    // Bounds-Check (H7 FIX)
    if (cylinder < 0 || cylinder >= (int)disk->geometry.cylinders ||
        head < 0 || head >= (int)disk->geometry.heads) {
        return UFT_ERROR_OUT_OF_RANGE;
    }
    
    // PC Layout: CHS in Reihenfolge (C0H0S1-Sn, C0H1S1-Sn, C1H0S1-Sn, ...)
    // Sektoren sind 1-basiert in der ID!
    // K2 FIX: size_t casts für Overflow-Schutz
    size_t track_offset = ((size_t)cylinder * (size_t)disk->geometry.heads + (size_t)head) * 
                          (size_t)disk->geometry.sectors * (size_t)IMG_SECTOR_SIZE;
    
    if (fseek(pdata->file, track_offset, SEEK_SET) != 0) {
        return UFT_ERROR_FILE_SEEK;
    }
    
    for (int s = 0; s < disk->geometry.sectors; s++) {
        uft_sector_t sector = {0};
        
        // PC-Sektoren sind 1-basiert!
        sector.id.cylinder = cylinder;
        sector.id.head = head;
        sector.id.sector = s + 1;  // 1-basiert
        sector.id.size_code = 2;   // 512 Bytes
        sector.id.crc_ok = true;
        
        sector.data = malloc(IMG_SECTOR_SIZE);
        if (!sector.data) {
            return UFT_ERROR_NO_MEMORY;
        }
        
        if (fread(sector.data, IMG_SECTOR_SIZE, 1, pdata->file) != 1) {
            free(sector.data);
            return UFT_ERROR_FILE_READ;
        }
        
        sector.data_size = IMG_SECTOR_SIZE;
        sector.status = UFT_SECTOR_OK;
        
        uft_error_t err = uft_track_add_sector(track, &sector);
        free(sector.data);
        
        if (UFT_FAILED(err)) {
            return err;
        }
    }
    
    track->status = UFT_TRACK_OK;
    
    return UFT_OK;
}

// ============================================================================
// Write Track
// ============================================================================

static uft_error_t img_write_track(uft_disk_t* disk, int cylinder, int head,
                                    const uft_track_t* track) {
    if (!disk || !track) return UFT_ERROR_NULL_POINTER;
    if (disk->read_only) return UFT_ERROR_DISK_PROTECTED;
    
    img_data_t* pdata = disk->plugin_data;
    if (!pdata || !pdata->file) return UFT_ERROR_FILE_WRITE;
    
    // Bounds-Check (H7 FIX)
    if (cylinder < 0 || cylinder >= (int)disk->geometry.cylinders ||
        head < 0 || head >= (int)disk->geometry.heads) {
        return UFT_ERROR_OUT_OF_RANGE;
    }
    
    // K2 FIX: size_t casts
    size_t track_offset = ((size_t)cylinder * (size_t)disk->geometry.heads + (size_t)head) * 
                          (size_t)disk->geometry.sectors * (size_t)IMG_SECTOR_SIZE;
    
    if (fseek(pdata->file, track_offset, SEEK_SET) != 0) {
        return UFT_ERROR_FILE_SEEK;
    }
    
    // Sektoren in Reihenfolge schreiben (1-basiert!)
    // M3 FIX: Static buffer für Null-Sektoren
    static const uint8_t zeros[IMG_SECTOR_SIZE] = {0};
    
    for (int s = 1; s <= disk->geometry.sectors; s++) {
        const uft_sector_t* sector = uft_track_find_sector(track, s);
        
        if (sector && sector->data && sector->data_size >= IMG_SECTOR_SIZE) {
            if (fwrite(sector->data, IMG_SECTOR_SIZE, 1, pdata->file) != 1) {
                return UFT_ERROR_FILE_WRITE;
            }
        } else {
            // M3 FIX: Verwende static buffer
            if (fwrite(zeros, IMG_SECTOR_SIZE, 1, pdata->file) != 1) {
                return UFT_ERROR_FILE_WRITE;
            }
        }
    }
    
    return UFT_OK;
}

// ============================================================================
// Metadata
// ============================================================================

static uft_error_t img_read_metadata(uft_disk_t* disk, const char* key, 
                                      char* value, size_t max_len) {
    if (!disk || !key || !value) return UFT_ERROR_NULL_POINTER;
    
    img_data_t* pdata = disk->plugin_data;
    if (!pdata || !pdata->file) return UFT_ERROR_FILE_READ;
    
    if (strcmp(key, "volume_name") == 0) {
        // FAT Volume Label lesen (Offset 43 im Bootsektor, oder Root-Dir)
        uint8_t boot[512];
        
        if (fseek(pdata->file, 0, SEEK_SET) != 0) {
            return UFT_ERROR_FILE_SEEK;
        }
        
        if (fread(boot, 512, 1, pdata->file) != 1) {
            return UFT_ERROR_FILE_READ;
        }
        
        // Volume Label bei Offset 43 (11 Bytes, space-padded)
        if (boot[38] == 0x29) {  // Extended boot signature
            char label[12];
            memcpy(label, &boot[43], 11);
            label[11] = '\0';
            
            // Trailing spaces entfernen
            for (int i = 10; i >= 0 && label[i] == ' '; i--) {
                label[i] = '\0';
            }
            
            // K3 FIX: snprintf statt strncpy für garantierte Null-Terminierung
            snprintf(value, max_len, "%s", label);
            return UFT_OK;
        }
        
        snprintf(value, max_len, "NO NAME");
        return UFT_OK;
    }
    
    if (strcmp(key, "filesystem") == 0) {
        uint8_t boot[128];
        
        if (fseek(pdata->file, 0, SEEK_SET) != 0) {
            return UFT_ERROR_FILE_SEEK;
        }
        
        if (fread(boot, 128, 1, pdata->file) != 1) {
            return UFT_ERROR_FILE_READ;
        }
        
        // FAT12/16 Type bei Offset 54 oder 82
        // K3 FIX: snprintf statt strncpy
        if (memcmp(&boot[54], "FAT12", 5) == 0) {
            snprintf(value, max_len, "FAT12");
        } else if (memcmp(&boot[54], "FAT16", 5) == 0) {
            snprintf(value, max_len, "FAT16");
        } else if (memcmp(&boot[82], "FAT32", 5) == 0) {
            snprintf(value, max_len, "FAT32");
        } else {
            snprintf(value, max_len, "Unknown");
        }
        
        return UFT_OK;
    }
    
    if (strcmp(key, "oem_name") == 0) {
        uint8_t boot[16];
        
        if (fseek(pdata->file, 0, SEEK_SET) != 0) {
            return UFT_ERROR_FILE_SEEK;
        }
        
        if (fread(boot, 16, 1, pdata->file) != 1) {
            return UFT_ERROR_FILE_READ;
        }
        
        char oem[9];
        memcpy(oem, &boot[3], 8);
        oem[8] = '\0';
        // K3 FIX: snprintf
        snprintf(value, max_len, "%s", oem);
        
        return UFT_OK;
    }
    
    return UFT_ERROR_NOT_SUPPORTED;
}

// ============================================================================
// Plugin Definition
// ============================================================================

const uft_format_plugin_t uft_format_plugin_img = {
    .name = "IMG",
    .description = "Generic PC Disk Image",
    .extensions = "img;ima;dsk;vfd;flp",
    .version = 0x00010000,
    .format = UFT_FORMAT_IMG,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_CREATE,
    
    .probe = img_probe,
    .open = img_open,
    .close = img_close,
    .create = img_create,
    .flush = img_flush,
    .read_track = img_read_track,
    .write_track = img_write_track,
    .detect_geometry = NULL,
    .read_metadata = img_read_metadata,
    .write_metadata = NULL,
    
    .init = NULL,
    .shutdown = NULL,
    .private_data = NULL
};
