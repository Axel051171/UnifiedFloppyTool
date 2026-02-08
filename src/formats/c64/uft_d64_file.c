/**
 * @file uft_d64_file.c
 * @brief D64 File Extraction and Insertion Implementation
 * 
 * @author UFT Project
 * @date 2026-01-16
 */

#include "uft/formats/c64/uft_d64_file.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ============================================================================
 * Static Data
 * ============================================================================ */

/** Sectors per track (1-42) */
static const int sectors_per_track[43] = {
    0,
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
    21, 21, 21, 21, 21, 21, 21, 19, 19, 19,
    19, 19, 19, 19, 18, 18, 18, 18, 18, 18,
    17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
    17, 17
};

/** Track offsets (cumulative sectors) */
static int track_offsets[43] = {0};
static bool offsets_init = false;

/** File type extensions */
static const char *type_extensions[] = {
    "del", "seq", "prg", "usr", "rel"
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Initialize track offset table
 */
static void init_offsets(void)
{
    if (offsets_init) return;
    
    track_offsets[0] = 0;
    for (int t = 1; t <= 42; t++) {
        track_offsets[t] = track_offsets[t - 1] + sectors_per_track[t - 1];
    }
    offsets_init = true;
}

/**
 * @brief Get sector offset in D64
 */
static int sector_offset(int track, int sector)
{
    if (track < 1 || track > 42) return -1;
    if (sector < 0 || sector >= sectors_per_track[track]) return -1;
    
    init_offsets();
    return (track_offsets[track] + sector) * 256;
}

/**
 * @brief Convert PETSCII to ASCII
 */
static void petscii_to_ascii(const uint8_t *petscii, char *ascii, int len)
{
    for (int i = 0; i < len; i++) {
        uint8_t c = petscii[i];
        if (c == 0xA0) {
            ascii[i] = '\0';  /* End on shifted space */
            return;
        } else if (c >= 0x41 && c <= 0x5A) {
            ascii[i] = c;  /* A-Z */
        } else if (c >= 0xC1 && c <= 0xDA) {
            ascii[i] = c - 0x80;  /* Shifted letters */
        } else if (c >= 0x20 && c <= 0x7E) {
            ascii[i] = c;
        } else {
            ascii[i] = '_';
        }
    }
    ascii[len] = '\0';
}

/**
 * @brief Convert ASCII to PETSCII
 */
static void ascii_to_petscii(const char *ascii, uint8_t *petscii, int len)
{
    int i;
    for (i = 0; i < len && ascii[i]; i++) {
        char c = ascii[i];
        if (c >= 'a' && c <= 'z') {
            petscii[i] = c - 32;  /* Uppercase */
        } else if (c >= 'A' && c <= 'Z') {
            petscii[i] = c;
        } else {
            petscii[i] = c;
        }
    }
    /* Pad with 0xA0 */
    for (; i < len; i++) {
        petscii[i] = 0xA0;
    }
}

/**
 * @brief Find file in directory
 */
static int find_file_entry(const uint8_t *d64_data, size_t d64_size,
                           const char *filename, int *track, int *sector,
                           int *entry_offset)
{
    (void)d64_size;
    
    /* Convert filename to PETSCII for comparison */
    uint8_t petscii_name[16];
    ascii_to_petscii(filename, petscii_name, 16);
    
    /* Directory starts at track 18, sector 1 */
    int dir_track = 18;
    int dir_sector = 1;
    int file_index = 0;
    
    while (dir_track != 0) {
        int offset = sector_offset(dir_track, dir_sector);
        if (offset < 0) return -1;
        
        const uint8_t *dir_data = d64_data + offset;
        
        /* 8 entries per sector */
        for (int e = 0; e < 8; e++) {
            const uint8_t *entry = dir_data + (e * 32);
            uint8_t file_type = entry[2] & 0x07;
            
            if (file_type != D64_FILE_DEL) {
                /* Compare filename */
                if (memcmp(entry + 5, petscii_name, 16) == 0) {
                    if (track) *track = dir_track;
                    if (sector) *sector = dir_sector;
                    if (entry_offset) *entry_offset = e * 32;
                    return file_index;
                }
                file_index++;
            }
        }
        
        /* Next directory sector */
        dir_track = dir_data[0];
        dir_sector = dir_data[1];
    }
    
    return -1;  /* Not found */
}

/* ============================================================================
 * File Extraction
 * ============================================================================ */

/**
 * @brief Extract file by name
 */
int d64_extract_file(const uint8_t *d64_data, size_t d64_size,
                     const char *filename, d64_file_t *file)
{
    if (!d64_data || !filename || !file) return -1;
    
    int dir_track, dir_sector, entry_offset;
    if (find_file_entry(d64_data, d64_size, filename, 
                        &dir_track, &dir_sector, &entry_offset) < 0) {
        return -2;  /* File not found */
    }
    
    int offset = sector_offset(dir_track, dir_sector);
    const uint8_t *entry = d64_data + offset + entry_offset;
    
    memset(file, 0, sizeof(d64_file_t));
    
    /* Get file info from directory entry */
    file->file_type = entry[2] & 0x07;
    petscii_to_ascii(entry + 5, file->filename, 16);
    file->block_count = entry[30] | (entry[31] << 8);
    
    int first_track = entry[3];
    int first_sector = entry[4];
    
    /* Follow file chain and collect data */
    size_t total_size = 0;
    size_t capacity = file->block_count * 254 + 256;
    file->data = malloc(capacity);
    if (!file->data) return -3;
    
    int track = first_track;
    int sector = first_sector;
    int blocks = 0;
    
    while (track != 0 && blocks < 1000) {  /* Safety limit */
        int sec_offset = sector_offset(track, sector);
        if (sec_offset < 0 || (size_t)(sec_offset + 256) > d64_size) {
            break;
        }
        
        const uint8_t *sec_data = d64_data + sec_offset;
        int next_track = sec_data[0];
        int next_sector = sec_data[1];
        
        /* Calculate bytes in this sector */
        int bytes;
        if (next_track == 0) {
            /* Last sector: next_sector = bytes used + 1 */
            bytes = next_sector - 1;
            if (bytes < 0) bytes = 0;
            if (bytes > 254) bytes = 254;
        } else {
            bytes = 254;
        }
        
        /* Copy data */
        if (total_size + bytes > capacity) {
            capacity = total_size + bytes + 1024;
            file->data = realloc(file->data, capacity);
            if (!file->data) return -4;
        }
        
        memcpy(file->data + total_size, sec_data + 2, bytes);
        total_size += bytes;
        
        track = next_track;
        sector = next_sector;
        blocks++;
    }
    
    file->data_size = total_size;
    
    /* Extract load address for PRG files */
    if (file->file_type == D64_FILE_PRG && total_size >= 2) {
        file->load_address = file->data[0] | (file->data[1] << 8);
        file->has_load_address = true;
    }
    
    return 0;
}

/**
 * @brief Extract file by index
 */
int d64_extract_by_index(const uint8_t *d64_data, size_t d64_size,
                         int index, d64_file_t *file)
{
    if (!d64_data || !file || index < 0) return -1;
    
    int dir_track = 18;
    int dir_sector = 1;
    int file_index = 0;
    
    while (dir_track != 0) {
        int offset = sector_offset(dir_track, dir_sector);
        if (offset < 0) return -2;
        
        const uint8_t *dir_data = d64_data + offset;
        
        for (int e = 0; e < 8; e++) {
            const uint8_t *entry = dir_data + (e * 32);
            uint8_t file_type = entry[2] & 0x07;
            
            if (file_type != D64_FILE_DEL) {
                if (file_index == index) {
                    /* Found the file, extract filename and call extract_file */
                    char filename[17];
                    petscii_to_ascii(entry + 5, filename, 16);
                    return d64_extract_file(d64_data, d64_size, filename, file);
                }
                file_index++;
            }
        }
        
        dir_track = dir_data[0];
        dir_sector = dir_data[1];
    }
    
    return -3;  /* Index out of range */
}

/**
 * @brief Extract all files
 */
int d64_extract_all(const uint8_t *d64_data, size_t d64_size,
                    d64_file_t *files, int max_files)
{
    if (!d64_data || !files || max_files <= 0) return 0;
    
    int count = 0;
    
    for (int i = 0; i < max_files && count < 144; i++) {
        if (d64_extract_by_index(d64_data, d64_size, i, &files[count]) == 0) {
            count++;
        } else {
            break;  /* No more files */
        }
    }
    
    return count;
}

/**
 * @brief Save file to disk
 */
int d64_save_file(const d64_file_t *file, const char *path,
                  const d64_extract_opts_t *opts)
{
    if (!file || !file->data) return -1;
    
    char filepath[512];
    
    if (path) {
        strncpy(filepath, path, sizeof(filepath) - 1);
    } else {
        /* Generate filename */
        const char *dir = (opts && opts->output_dir) ? opts->output_dir : ".";
        const char *ext = d64_file_extension(file->file_type);
        snprintf(filepath, sizeof(filepath), "%s/%s.%s", dir, file->filename, ext);
    }
    
    FILE *fp = fopen(filepath, "wb");
    if (!fp) return -2;
    
    size_t write_size = file->data_size;
    const uint8_t *write_data = file->data;
    
    /* Optionally skip load address for PRG */
    if (opts && !opts->include_load_addr && 
        file->file_type == D64_FILE_PRG && file->data_size >= 2) {
        write_data += 2;
        write_size -= 2;
    }
    
    size_t written = fwrite(write_data, 1, write_size, fp);
    fclose(fp);
    
    return (written == write_size) ? 0 : -3;
}

/**
 * @brief Free file data
 */
void d64_free_file(d64_file_t *file)
{
    if (file) {
        free(file->data);
        file->data = NULL;
        file->data_size = 0;
    }
}

/* ============================================================================
 * File Insertion
 * ============================================================================ */

/**
 * @brief Insert file into D64
 */
int d64_insert_file(uint8_t *d64_data, size_t d64_size,
                    const char *filename, const uint8_t *data,
                    size_t data_size, const d64_insert_opts_t *opts)
{
    if (!d64_data || !filename || !data || data_size == 0) return -1;
    
    d64_insert_opts_t default_opts;
    if (!opts) {
        d64_get_insert_defaults(&default_opts);
        opts = &default_opts;
    }
    
    /* Check if file exists */
    if (!opts->overwrite) {
        int found = find_file_entry(d64_data, d64_size, filename, NULL, NULL, NULL);
        if (found >= 0) return -2;  /* File exists */
    }
    
    /* Calculate blocks needed */
    int blocks_needed = d64_calc_blocks(data_size);
    
    /* Find free directory entry */
    int dir_track, dir_sector, entry_offset;
    if (d64_find_free_dir_entry(d64_data, d64_size, 
                                &dir_track, &dir_sector, &entry_offset) != 0) {
        return -3;  /* Directory full */
    }
    
    /* Allocate sectors for file */
    d64_chain_t chain;
    memset(&chain, 0, sizeof(chain));
    chain.entries = malloc(blocks_needed * sizeof(d64_chain_entry_t));
    if (!chain.entries) return -4;
    chain.capacity = blocks_needed;
    
    /* Find free sectors (simple allocation from track 17 down, then 19 up) */
    int allocated = 0;
    
    /* BAM is at track 18, sector 0 */
    int bam_offset = sector_offset(18, 0);
    uint8_t *bam = d64_data + bam_offset;
    
    /* Allocate from inner tracks outward */
    for (int delta = 1; allocated < blocks_needed && delta <= 35; delta++) {
        /* Try track below directory */
        int t = 18 - delta;
        if (t >= 1) {
            for (int s = 0; s < sectors_per_track[t] && allocated < blocks_needed; s++) {
                int byte_idx = s / 8;
                int bit_idx = s % 8;
                uint8_t *track_bam = bam + 4 + (t - 1) * 4;
                
                if (track_bam[1 + byte_idx] & (1 << bit_idx)) {
                    /* Sector is free, allocate it */
                    track_bam[1 + byte_idx] &= ~(1 << bit_idx);
                    track_bam[0]--;  /* Decrease free count */
                    
                    chain.entries[allocated].track = t;
                    chain.entries[allocated].sector = s;
                    allocated++;
                }
            }
        }
        
        /* Try track above directory (skip 18) */
        t = 18 + delta;
        if (t <= 35) {
            for (int s = 0; s < sectors_per_track[t] && allocated < blocks_needed; s++) {
                int byte_idx = s / 8;
                int bit_idx = s % 8;
                uint8_t *track_bam = bam + 4 + (t - 1) * 4;
                
                if (track_bam[1 + byte_idx] & (1 << bit_idx)) {
                    track_bam[1 + byte_idx] &= ~(1 << bit_idx);
                    track_bam[0]--;
                    
                    chain.entries[allocated].track = t;
                    chain.entries[allocated].sector = s;
                    allocated++;
                }
            }
        }
    }
    
    if (allocated < blocks_needed) {
        free(chain.entries);
        return -5;  /* Disk full */
    }
    
    chain.num_entries = allocated;
    
    /* Write file data to allocated sectors */
    size_t bytes_written = 0;
    
    for (int i = 0; i < chain.num_entries; i++) {
        int offset = sector_offset(chain.entries[i].track, chain.entries[i].sector);
        uint8_t *sec_data = d64_data + offset;
        
        /* Set chain pointer */
        if (i < chain.num_entries - 1) {
            sec_data[0] = chain.entries[i + 1].track;
            sec_data[1] = chain.entries[i + 1].sector;
        } else {
            /* Last sector */
            sec_data[0] = 0;
            size_t remaining = data_size - bytes_written;
            sec_data[1] = (remaining > 254) ? 255 : remaining + 1;
        }
        
        /* Copy data */
        size_t to_copy = data_size - bytes_written;
        if (to_copy > 254) to_copy = 254;
        
        memcpy(sec_data + 2, data + bytes_written, to_copy);
        bytes_written += to_copy;
        
        /* Clear remaining bytes in last sector */
        if (to_copy < 254) {
            memset(sec_data + 2 + to_copy, 0, 254 - to_copy);
        }
    }
    
    /* Create directory entry */
    d64_create_dir_entry(d64_data, d64_size, filename, opts->file_type,
                         chain.entries[0].track, chain.entries[0].sector,
                         blocks_needed);
    
    free(chain.entries);
    return 0;
}

/**
 * @brief Insert PRG file
 */
int d64_insert_prg(uint8_t *d64_data, size_t d64_size,
                   const char *filename, const uint8_t *data,
                   size_t data_size, uint16_t load_address)
{
    if (!d64_data || !filename || !data) return -1;
    
    uint8_t *prg_data;
    size_t prg_size;
    
    if (load_address != 0) {
        /* Add load address header */
        prg_size = data_size + 2;
        prg_data = malloc(prg_size);
        if (!prg_data) return -2;
        
        prg_data[0] = load_address & 0xFF;
        prg_data[1] = (load_address >> 8) & 0xFF;
        memcpy(prg_data + 2, data, data_size);
    } else {
        /* Use data as-is (should already have load address) */
        prg_data = (uint8_t *)data;
        prg_size = data_size;
    }
    
    d64_insert_opts_t opts;
    d64_get_insert_defaults(&opts);
    opts.file_type = D64_FILE_PRG;
    
    int result = d64_insert_file(d64_data, d64_size, filename, prg_data, prg_size, &opts);
    
    if (load_address != 0) {
        free(prg_data);
    }
    
    return result;
}

/**
 * @brief Insert from disk file
 */
int d64_insert_from_file(uint8_t *d64_data, size_t d64_size,
                         const char *path, const char *c64_name,
                         const d64_insert_opts_t *opts)
{
    if (!d64_data || !path) return -1;
    
    FILE *fp = fopen(path, "rb");
    if (!fp) return -2;
    
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    if (size == 0 || size > D64_MAX_FILE_SIZE) {
        fclose(fp);
        return -3;
    }
    
    uint8_t *data = malloc(size);
    if (!data) {
        fclose(fp);
        return -4;
    }
    
    if (fread(data, 1, size, fp) != size) {
        free(data);
        fclose(fp);
        return -5;
    }
    fclose(fp);
    
    char filename[17];
    if (c64_name) {
        strncpy(filename, c64_name, 16);
        filename[16] = '\0';
    } else {
        d64_make_filename(path, filename);
    }
    
    int result = d64_insert_file(d64_data, d64_size, filename, data, size, opts);
    free(data);
    
    return result;
}

/* ============================================================================
 * File Chain
 * ============================================================================ */

/**
 * @brief Get file chain
 */
int d64_get_chain(const uint8_t *d64_data, size_t d64_size,
                  int first_track, int first_sector, d64_chain_t *chain)
{
    if (!d64_data || !chain) return -1;
    
    memset(chain, 0, sizeof(d64_chain_t));
    chain->capacity = 64;
    chain->entries = malloc(chain->capacity * sizeof(d64_chain_entry_t));
    if (!chain->entries) return -2;
    
    int track = first_track;
    int sector = first_sector;
    
    while (track != 0 && chain->num_entries < 1000) {
        if (chain->num_entries >= chain->capacity) {
            chain->capacity *= 2;
            chain->entries = realloc(chain->entries, 
                                     chain->capacity * sizeof(d64_chain_entry_t));
            if (!chain->entries) return -3;
        }
        
        int offset = sector_offset(track, sector);
        if (offset < 0 || (size_t)(offset + 256) > d64_size) break;
        
        const uint8_t *sec_data = d64_data + offset;
        
        chain->entries[chain->num_entries].track = track;
        chain->entries[chain->num_entries].sector = sector;
        
        if (sec_data[0] == 0) {
            /* Last sector */
            chain->entries[chain->num_entries].bytes_used = sec_data[1] - 1;
        } else {
            chain->entries[chain->num_entries].bytes_used = 254;
        }
        
        chain->num_entries++;
        
        track = sec_data[0];
        sector = sec_data[1];
    }
    
    return 0;
}

/**
 * @brief Free chain
 */
void d64_free_chain(d64_chain_t *chain)
{
    if (chain) {
        free(chain->entries);
        chain->entries = NULL;
        chain->num_entries = 0;
    }
}

/**
 * @brief Validate chain
 */
bool d64_validate_chain(const uint8_t *d64_data, size_t d64_size,
                        const d64_chain_t *chain, int *errors)
{
    (void)d64_data;
    (void)d64_size;
    
    if (!chain) return false;
    
    int err_count = 0;
    
    for (int i = 0; i < chain->num_entries; i++) {
        int t = chain->entries[i].track;
        int s = chain->entries[i].sector;
        
        if (t < 1 || t > 42 || s < 0 || s >= sectors_per_track[t]) {
            err_count++;
        }
    }
    
    if (errors) *errors = err_count;
    return (err_count == 0);
}

/* ============================================================================
 * Directory Operations
 * ============================================================================ */

/**
 * @brief Find free directory entry
 */
int d64_find_free_dir_entry(const uint8_t *d64_data, size_t d64_size,
                            int *track, int *sector, int *entry_offset)
{
    (void)d64_size;
    
    int dir_track = 18;
    int dir_sector = 1;
    
    while (dir_track != 0) {
        int offset = sector_offset(dir_track, dir_sector);
        if (offset < 0) return -1;
        
        const uint8_t *dir_data = d64_data + offset;
        
        for (int e = 0; e < 8; e++) {
            const uint8_t *entry = dir_data + (e * 32);
            
            if ((entry[2] & 0x07) == D64_FILE_DEL) {
                if (track) *track = dir_track;
                if (sector) *sector = dir_sector;
                if (entry_offset) *entry_offset = e * 32;
                return 0;
            }
        }
        
        dir_track = dir_data[0];
        dir_sector = dir_data[1];
    }
    
    return -1;  /* Directory full */
}

/**
 * @brief Create directory entry
 */
int d64_create_dir_entry(uint8_t *d64_data, size_t d64_size,
                         const char *filename, uint8_t file_type,
                         int first_track, int first_sector, int block_count)
{
    int dir_track, dir_sector, entry_offset;
    
    if (d64_find_free_dir_entry(d64_data, d64_size,
                                &dir_track, &dir_sector, &entry_offset) != 0) {
        return -1;
    }
    
    int offset = sector_offset(dir_track, dir_sector);
    uint8_t *entry = d64_data + offset + entry_offset;
    
    /* Clear entry */
    memset(entry, 0, 32);
    
    /* Set file type (with closed flag) */
    entry[2] = file_type | 0x80;
    
    /* Set first track/sector */
    entry[3] = first_track;
    entry[4] = first_sector;
    
    /* Set filename */
    ascii_to_petscii(filename, entry + 5, 16);
    
    /* Set block count */
    entry[30] = block_count & 0xFF;
    entry[31] = (block_count >> 8) & 0xFF;
    
    return 0;
}

/* ============================================================================
 * Utilities
 * ============================================================================ */

/**
 * @brief Get default extraction options
 */
void d64_get_extract_defaults(d64_extract_opts_t *opts)
{
    if (!opts) return;
    
    opts->include_load_addr = true;
    opts->convert_petscii = true;
    opts->preserve_padding = false;
    opts->output_dir = NULL;
}

/**
 * @brief Get default insertion options
 */
void d64_get_insert_defaults(d64_insert_opts_t *opts)
{
    if (!opts) return;
    
    opts->file_type = D64_FILE_PRG;
    opts->load_address = 0;
    opts->overwrite = false;
    opts->lock_file = false;
    opts->interleave = 0;
}

/**
 * @brief Calculate blocks needed
 */
int d64_calc_blocks(size_t data_size)
{
    if (data_size == 0) return 0;
    return (data_size + 253) / 254;
}

/**
 * @brief Get file extension
 */
const char *d64_file_extension(uint8_t file_type)
{
    if (file_type < 5) {
        return type_extensions[file_type];
    }
    return "???";
}

/**
 * @brief Parse extension
 */
uint8_t d64_parse_extension(const char *extension)
{
    if (!extension) return D64_FILE_PRG;
    
    for (int i = 0; i < 5; i++) {
        if (strcasecmp(extension, type_extensions[i]) == 0) {
            return i;
        }
    }
    
    return D64_FILE_PRG;
}

/**
 * @brief Create C64 filename from path
 */
void d64_make_filename(const char *path, char *c64_name)
{
    if (!path || !c64_name) return;
    
    /* Find last path separator */
    const char *name = path;
    const char *p = strrchr(path, '/');
    if (p) name = p + 1;
    p = strrchr(name, '\\');
    if (p) name = p + 1;
    
    /* Copy up to extension */
    int i = 0;
    while (name[i] && name[i] != '.' && i < 16) {
        char c = name[i];
        if (c >= 'a' && c <= 'z') c -= 32;  /* Uppercase */
        c64_name[i] = c;
        i++;
    }
    
    /* Pad with spaces */
    while (i < 16) {
        c64_name[i++] = ' ';
    }
    c64_name[16] = '\0';
}
