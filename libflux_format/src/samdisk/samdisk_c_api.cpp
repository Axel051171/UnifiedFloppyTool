// SPDX-License-Identifier: MIT
/*
 * samdisk_c_api.cpp - SAMdisk C API Export
 * 
 * C API wrapper for SAMdisk engine.
 * 
 * @version 2.8.0
 * @date 2024-12-26
 */

#include "samdisk_wrapper.cpp"  /* Include Part 1 */
#include <cstdlib>
#include <cstring>

/*============================================================================*
 * C API STRUCTURES
 *============================================================================*/

extern "C" {

/* Opaque handle */
typedef struct samdisk_engine_t {
    SAMdiskEngine* engine;
} samdisk_engine_t;

/* Disk handle */
typedef struct samdisk_disk_t {
    Disk* disk;
} samdisk_disk_t;

/* Format info */
typedef struct samdisk_format_info_t {
    const char* name;
    const char* extension;
    const char* description;
    int type;  /* 0=unknown, 1=flux, 2=sector, 3=track */
    int can_read;
    int can_write;
} samdisk_format_info_t;

/*============================================================================*
 * ENGINE MANAGEMENT
 *============================================================================*/

/**
 * @brief Initialize SAMdisk engine
 */
int samdisk_init(samdisk_engine_t** engine_out)
{
    if (!engine_out) {
        return -1;
    }
    
    samdisk_engine_t* handle = (samdisk_engine_t*)malloc(sizeof(samdisk_engine_t));
    if (!handle) {
        return -1;
    }
    
    try {
        handle->engine = new SAMdiskEngine();
    } catch (...) {
        free(handle);
        return -1;
    }
    
    *engine_out = handle;
    return 0;
}

/**
 * @brief Close SAMdisk engine
 */
void samdisk_close(samdisk_engine_t* engine)
{
    if (!engine) {
        return;
    }
    
    delete engine->engine;
    free(engine);
}

/*============================================================================*
 * FORMAT DETECTION
 *============================================================================*/

/**
 * @brief Auto-detect format from data
 */
int samdisk_detect_format(
    samdisk_engine_t* engine,
    const uint8_t* data,
    size_t len,
    char** format_name_out
) {
    if (!engine || !data || !format_name_out) {
        return -1;
    }
    
    std::string format;
    if (!engine->engine->detectFormat(data, len, format)) {
        return -1;
    }
    
    *format_name_out = strdup(format.c_str());
    return 0;
}

/**
 * @brief Auto-detect format from file
 */
int samdisk_detect_format_file(
    samdisk_engine_t* engine,
    const char* filename,
    char** format_name_out
) {
    if (!engine || !filename || !format_name_out) {
        return -1;
    }
    
    FILE* f = fopen(filename, "rb");
    if (!f) {
        return -1;
    }
    
    /* Read first 8KB for detection */
    uint8_t buffer[8192];
    size_t len = fread(buffer, 1, sizeof(buffer), f);
    fclose(f);
    
    return samdisk_detect_format(engine, buffer, len, format_name_out);
}

/*============================================================================*
 * DISK IMAGE I/O
 *============================================================================*/

/**
 * @brief Read disk image from file
 */
int samdisk_read_image(
    samdisk_engine_t* engine,
    const char* filename,
    const char* format,
    samdisk_disk_t** disk_out
) {
    if (!engine || !filename || !disk_out) {
        return -1;
    }
    
    samdisk_disk_t* disk_handle = (samdisk_disk_t*)malloc(sizeof(samdisk_disk_t));
    if (!disk_handle) {
        return -1;
    }
    
    try {
        disk_handle->disk = new Disk();
        
        std::string fmt = format ? std::string(format) : "";
        
        if (fmt.empty()) {
            /* Auto-detect */
            char* detected = nullptr;
            if (samdisk_detect_format_file(engine, filename, &detected) == 0) {
                fmt = detected;
                free(detected);
            }
        }
        
        if (!engine->engine->readImage(filename, fmt, *disk_handle->disk)) {
            delete disk_handle->disk;
            free(disk_handle);
            return -1;
        }
        
        *disk_out = disk_handle;
        return 0;
        
    } catch (...) {
        if (disk_handle->disk) {
            delete disk_handle->disk;
        }
        free(disk_handle);
        return -1;
    }
}

/**
 * @brief Write disk image to file
 */
int samdisk_write_image(
    samdisk_engine_t* engine,
    samdisk_disk_t* disk,
    const char* filename,
    const char* format
) {
    if (!engine || !disk || !filename || !format) {
        return -1;
    }
    
    try {
        return engine->engine->writeImage(filename, format, *disk->disk) ? 0 : -1;
    } catch (...) {
        return -1;
    }
}

/**
 * @brief Free disk handle
 */
void samdisk_free_disk(samdisk_disk_t* disk)
{
    if (!disk) {
        return;
    }
    
    delete disk->disk;
    free(disk);
}

/*============================================================================*
 * CONVERSION
 *============================================================================*/

/**
 * @brief Convert between formats
 */
int samdisk_convert(
    samdisk_engine_t* engine,
    const char* input_file,
    const char* input_format,
    const char* output_file,
    const char* output_format
) {
    if (!engine || !input_file || !output_file || !output_format) {
        return -1;
    }
    
    try {
        std::string in_fmt = input_format ? std::string(input_format) : "";
        
        if (in_fmt.empty()) {
            /* Auto-detect input format */
            char* detected = nullptr;
            if (samdisk_detect_format_file(engine, input_file, &detected) == 0) {
                in_fmt = detected;
                free(detected);
            } else {
                return -1;
            }
        }
        
        return engine->engine->convert(input_file, in_fmt, 
                                       output_file, output_format) ? 0 : -1;
        
    } catch (...) {
        return -1;
    }
}

/*============================================================================*
 * FORMAT INFORMATION
 *============================================================================*/

/**
 * @brief List all supported formats
 */
int samdisk_list_formats(
    samdisk_engine_t* engine,
    samdisk_format_info_t** formats_out,
    int* count_out
) {
    if (!engine || !formats_out || !count_out) {
        return -1;
    }
    
    /* Count formats */
    int count = 0;
    while (SAMDISK_FORMATS[count].name != nullptr) {
        count++;
    }
    
    /* Allocate array */
    samdisk_format_info_t* formats = (samdisk_format_info_t*)calloc(
        count, sizeof(samdisk_format_info_t)
    );
    if (!formats) {
        return -1;
    }
    
    /* Copy format info */
    for (int i = 0; i < count; i++) {
        formats[i].name = SAMDISK_FORMATS[i].name;
        formats[i].extension = SAMDISK_FORMATS[i].extension;
        formats[i].description = SAMDISK_FORMATS[i].description;
        formats[i].type = (int)SAMDISK_FORMATS[i].type;
        formats[i].can_read = SAMDISK_FORMATS[i].can_read ? 1 : 0;
        formats[i].can_write = SAMDISK_FORMATS[i].can_write ? 1 : 0;
    }
    
    *formats_out = formats;
    *count_out = count;
    
    return 0;
}

/**
 * @brief Free format list
 */
void samdisk_free_formats(samdisk_format_info_t* formats)
{
    free(formats);
}

/**
 * @brief Get format info by name
 */
int samdisk_get_format_info(
    samdisk_engine_t* engine,
    const char* format_name,
    samdisk_format_info_t* info_out
) {
    if (!engine || !format_name || !info_out) {
        return -1;
    }
    
    for (int i = 0; SAMDISK_FORMATS[i].name != nullptr; i++) {
        if (strcmp(SAMDISK_FORMATS[i].name, format_name) == 0) {
            info_out->name = SAMDISK_FORMATS[i].name;
            info_out->extension = SAMDISK_FORMATS[i].extension;
            info_out->description = SAMDISK_FORMATS[i].description;
            info_out->type = (int)SAMDISK_FORMATS[i].type;
            info_out->can_read = SAMDISK_FORMATS[i].can_read ? 1 : 0;
            info_out->can_write = SAMDISK_FORMATS[i].can_write ? 1 : 0;
            return 0;
        }
    }
    
    return -1;
}

/*============================================================================*
 * DISK INFO
 *============================================================================*/

/**
 * @brief Get disk geometry
 */
int samdisk_get_geometry(
    samdisk_disk_t* disk,
    int* tracks_out,
    int* sides_out
) {
    if (!disk || !tracks_out || !sides_out) {
        return -1;
    }
    
    *tracks_out = disk->disk->tracks;
    *sides_out = disk->disk->sides;
    
    return 0;
}

/**
 * @brief Get disk format name
 */
const char* samdisk_get_format_name(samdisk_disk_t* disk)
{
    if (!disk) {
        return nullptr;
    }
    
    return disk->disk->format_name.c_str();
}

/**
 * @brief Read sector data
 */
int samdisk_read_sector(
    samdisk_disk_t* disk,
    int track,
    int side,
    int sector_id,
    uint8_t* buffer,
    size_t buffer_size,
    size_t* bytes_read_out
) {
    if (!disk || !buffer || !bytes_read_out) {
        return -1;
    }
    
    int key = track * 2 + side;
    auto it = disk->disk->track_map.find(key);
    if (it == disk->disk->track_map.end()) {
        return -1;
    }
    
    const Track& t = it->second;
    
    for (const Sector& s : t.sectors) {
        if (s.sector_id == sector_id) {
            size_t to_copy = (s.data.size() < buffer_size) ? 
                           s.data.size() : buffer_size;
            memcpy(buffer, s.data.data(), to_copy);
            *bytes_read_out = to_copy;
            return 0;
        }
    }
    
    return -1;
}

/**
 * @brief Write sector data
 */
int samdisk_write_sector(
    samdisk_disk_t* disk,
    int track,
    int side,
    int sector_id,
    const uint8_t* data,
    size_t data_size
) {
    if (!disk || !data) {
        return -1;
    }
    
    int key = track * 2 + side;
    auto it = disk->disk->track_map.find(key);
    if (it == disk->disk->track_map.end()) {
        /* Create new track */
        Track t;
        t.track_num = track;
        t.side = side;
        disk->disk->track_map[key] = t;
        it = disk->disk->track_map.find(key);
    }
    
    Track& t = it->second;
    
    /* Find existing sector */
    for (Sector& s : t.sectors) {
        if (s.sector_id == sector_id) {
            s.data.resize(data_size);
            memcpy(s.data.data(), data, data_size);
            return 0;
        }
    }
    
    /* Create new sector */
    Sector s;
    s.track = track;
    s.side = side;
    s.sector_id = sector_id;
    s.size = data_size;
    s.data.resize(data_size);
    memcpy(s.data.data(), data, data_size);
    s.deleted = false;
    s.crc_error = false;
    
    t.sectors.push_back(s);
    
    return 0;
}

} /* extern "C" */

/*
 * Local variables:
 * mode: C++
 * c-file-style: "Linux"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
