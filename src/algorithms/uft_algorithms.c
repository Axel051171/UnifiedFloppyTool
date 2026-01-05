/**
 * @file uft_algorithms.c
 * @brief Specialized Algorithms Implementation
 * @version 4.2.0
 */

#include "uft/uft_algorithms.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * RABIN-KARP PATTERN MATCHING
 * ═══════════════════════════════════════════════════════════════════════════════ */

void uft_rk_init(uft_rk_context_t *ctx,
                 const uint8_t *pattern,
                 size_t pattern_len)
{
    uft_rk_init_custom(ctx, pattern, pattern_len, UFT_RK_PRIME);
}

void uft_rk_init_custom(uft_rk_context_t *ctx,
                        const uint8_t *pattern,
                        size_t pattern_len,
                        uint64_t prime)
{
    if (!ctx || !pattern || pattern_len == 0) return;
    
    ctx->pattern = pattern;
    ctx->pattern_len = pattern_len;
    ctx->prime = prime;
    
    /* Calculate pattern hash */
    ctx->pattern_hash = 0;
    for (size_t i = 0; i < pattern_len; i++) {
        ctx->pattern_hash = (ctx->pattern_hash * UFT_RK_BASE + pattern[i]) % prime;
    }
    
    /* Calculate high_pow = base^(m-1) mod prime */
    ctx->high_pow = 1;
    for (size_t i = 0; i < pattern_len - 1; i++) {
        ctx->high_pow = (ctx->high_pow * UFT_RK_BASE) % prime;
    }
}

uint64_t uft_rk_roll(const uft_rk_context_t *ctx,
                     uint64_t old_hash,
                     uint8_t old_byte,
                     uint8_t new_byte)
{
    uint64_t hash = old_hash;
    
    /* Remove old byte contribution */
    hash = (hash + ctx->prime - (old_byte * ctx->high_pow) % ctx->prime) % ctx->prime;
    
    /* Shift and add new byte */
    hash = (hash * UFT_RK_BASE + new_byte) % ctx->prime;
    
    return hash;
}

int uft_rk_search(const uft_rk_context_t *ctx,
                  const uint8_t *data,
                  size_t data_len,
                  size_t *matches,
                  int max_matches)
{
    if (!ctx || !data || !matches || max_matches <= 0) return 0;
    if (data_len < ctx->pattern_len) return 0;
    
    int match_count = 0;
    
    /* Calculate initial hash for first window */
    uint64_t hash = 0;
    for (size_t i = 0; i < ctx->pattern_len; i++) {
        hash = (hash * UFT_RK_BASE + data[i]) % ctx->prime;
    }
    
    /* Check first position */
    if (hash == ctx->pattern_hash) {
        if (memcmp(data, ctx->pattern, ctx->pattern_len) == 0) {
            matches[match_count++] = 0;
            if (match_count >= max_matches) return match_count;
        }
    }
    
    /* Roll through the data */
    for (size_t i = 1; i <= data_len - ctx->pattern_len; i++) {
        hash = uft_rk_roll(ctx, hash, data[i - 1], data[i + ctx->pattern_len - 1]);
        
        if (hash == ctx->pattern_hash) {
            /* Hash match - verify with memcmp */
            if (memcmp(data + i, ctx->pattern, ctx->pattern_len) == 0) {
                matches[match_count++] = i;
                if (match_count >= max_matches) return match_count;
            }
        }
    }
    
    return match_count;
}

int uft_rk_search_multi(const uint8_t **patterns,
                        const size_t *pattern_lens,
                        int pattern_count,
                        const uint8_t *data,
                        size_t data_len,
                        size_t *matches,
                        int *pattern_ids,
                        int max_matches)
{
    if (!patterns || !pattern_lens || !data || !matches || !pattern_ids) return 0;
    
    int total_matches = 0;
    
    /* Simple approach: search for each pattern */
    for (int p = 0; p < pattern_count && total_matches < max_matches; p++) {
        uft_rk_context_t ctx;
        uft_rk_init(&ctx, patterns[p], pattern_lens[p]);
        
        size_t temp_matches[256];
        int count = uft_rk_search(&ctx, data, data_len, temp_matches, 
                                  max_matches - total_matches > 256 ? 256 : max_matches - total_matches);
        
        for (int i = 0; i < count && total_matches < max_matches; i++) {
            matches[total_matches] = temp_matches[i];
            pattern_ids[total_matches] = p;
            total_matches++;
        }
    }
    
    return total_matches;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * HUMAN68K FAT - Sharp X68000
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_human68k_detect(const uint8_t *data, size_t size)
{
    if (!data || size < 512) return 0;
    
    int score = 0;
    
    /* Check for Human68K OEM name patterns */
    if (memcmp(data + 2, "Hudson soft", 11) == 0) score += 50;
    else if (memcmp(data + 2, "X68K", 4) == 0) score += 40;
    else if (memcmp(data + 2, "Human", 5) == 0) score += 40;
    
    /* Check for valid BPB */
    uint16_t bytes_per_sector = data[17] | (data[18] << 8);
    if (bytes_per_sector == 256 || bytes_per_sector == 512 || 
        bytes_per_sector == 1024) {
        score += 20;
    }
    
    /* Check media type */
    uint8_t media = data[21];
    if (media == 0xFE || media == 0xF9 || media == 0xF8) {
        score += 15;
    }
    
    /* Check FAT count */
    if (data[16] == 2) score += 10;
    
    return score > 100 ? 100 : score;
}

int uft_human68k_mount(const uint8_t *data, size_t size,
                       uft_human68k_volume_t *volume)
{
    if (!data || !volume || size < 1024) return -1;
    
    memset(volume, 0, sizeof(*volume));
    
    /* Copy boot sector */
    memcpy(&volume->boot, data, sizeof(uft_human68k_boot_t));
    
    /* Calculate parameters */
    uint16_t bps = volume->boot.bytes_per_sector;
    if (bps == 0) bps = 1024;
    
    uint16_t spc = volume->boot.sectors_per_cluster;
    if (spc == 0) spc = 1;
    
    volume->cluster_size = bps * spc;
    
    uint16_t reserved = volume->boot.reserved_sectors;
    if (reserved == 0) reserved = 1;
    
    uint16_t fat_size = volume->boot.sectors_per_fat;
    uint8_t fat_count = volume->boot.fat_count;
    if (fat_count == 0) fat_count = 2;
    
    /* Copy FAT */
    size_t fat_offset = reserved * bps;
    volume->fat_size = fat_size * bps;
    
    if (fat_offset + volume->fat_size > size) return -1;
    
    volume->fat = (uint8_t*)malloc(volume->fat_size);
    if (!volume->fat) return -1;
    memcpy(volume->fat, data + fat_offset, volume->fat_size);
    
    /* Root directory */
    size_t root_offset = fat_offset + (fat_count * fat_size * bps);
    volume->root_entries = volume->boot.root_entries;
    if (volume->root_entries == 0) volume->root_entries = 224;
    
    size_t root_size = volume->root_entries * 32;
    if (root_offset + root_size > size) {
        free(volume->fat);
        return -1;
    }
    
    volume->root = (uft_human68k_dirent_t*)malloc(root_size);
    if (!volume->root) {
        free(volume->fat);
        return -1;
    }
    memcpy(volume->root, data + root_offset, root_size);
    
    /* Data area */
    volume->data_start_sector = reserved + (fat_count * fat_size) + 
                                ((root_size + bps - 1) / bps);
    
    /* Determine FAT type */
    uint16_t total = volume->boot.total_sectors_16;
    if (total == 0) total = volume->boot.total_sectors_32 & 0xFFFF;
    
    uint16_t data_sectors = total - volume->data_start_sector;
    uint16_t clusters = data_sectors / spc;
    
    volume->fat_type = (clusters < 4085) ? 12 : 16;
    
    /* Store raw data reference */
    volume->data = (uint8_t*)malloc(size);
    if (volume->data) {
        memcpy(volume->data, data, size);
        volume->data_size = size;
    }
    
    return 0;
}

int uft_human68k_list_root(const uft_human68k_volume_t *volume,
                           uft_human68k_dirent_t *entries,
                           int max_entries)
{
    if (!volume || !volume->root || !entries) return 0;
    
    int count = 0;
    for (int i = 0; i < volume->root_entries && count < max_entries; i++) {
        uint8_t first = volume->root[i].filename[0];
        
        if (first == 0x00) break;  /* End of directory */
        if (first == 0xE5) continue;  /* Deleted */
        if (volume->root[i].attributes == 0x0F) continue;  /* LFN */
        
        entries[count++] = volume->root[i];
    }
    
    return count;
}

void uft_human68k_free(uft_human68k_volume_t *volume)
{
    if (!volume) return;
    
    free(volume->fat);
    free(volume->root);
    free(volume->data);
    memset(volume, 0, sizeof(*volume));
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * TARBELL CP/M FORMAT
 * ═══════════════════════════════════════════════════════════════════════════════ */

const uft_tarbell_geometry_t UFT_TARBELL_SSSD_8 = {
    .tracks = 77,
    .sectors_per_track = 26,
    .sector_size = 128,
    .block_size = 1024,
    .dir_blocks = 2,
    .reserved_tracks = 2,
    .single_sided = true,
    .skew = 6
};

const uft_tarbell_geometry_t UFT_TARBELL_DSDD_8 = {
    .tracks = 77,
    .sectors_per_track = 26,
    .sector_size = 256,
    .block_size = 2048,
    .dir_blocks = 2,
    .reserved_tracks = 2,
    .single_sided = false,
    .skew = 6
};

const uft_tarbell_geometry_t UFT_TARBELL_SSDD_5 = {
    .tracks = 40,
    .sectors_per_track = 18,
    .sector_size = 256,
    .block_size = 2048,
    .dir_blocks = 2,
    .reserved_tracks = 2,
    .single_sided = true,
    .skew = 4
};

int uft_tarbell_detect(const uint8_t *data, size_t size)
{
    if (!data) return 0;
    
    /* Check for standard CP/M sizes */
    if (size == 256256) return 80;  /* 8" SSSD */
    if (size == 512512) return 70;  /* 8" DSSD */
    if (size == 184320) return 60;  /* 5.25" SSDD */
    
    /* Check directory for CP/M signatures */
    /* Directory typically starts at track 2 */
    size_t dir_offset = 2 * 26 * 128;  /* SSSD assumed */
    if (dir_offset + 32 > size) return 0;
    
    int valid_entries = 0;
    for (int i = 0; i < 16 && dir_offset + (i + 1) * 32 <= size; i++) {
        const uft_cpm_dirent_t *entry = (const uft_cpm_dirent_t*)(data + dir_offset + i * 32);
        
        if (entry->user_number <= 15 || entry->user_number == 0xE5) {
            /* Check for valid filename characters */
            bool valid = true;
            for (int j = 0; j < 8 && valid; j++) {
                char c = entry->filename[j] & 0x7F;
                if (c < 0x20 && c != 0) valid = false;
            }
            if (valid) valid_entries++;
        }
    }
    
    return valid_entries > 2 ? 50 + valid_entries * 3 : 0;
}

int uft_tarbell_open(const uint8_t *data, size_t size,
                     const uft_tarbell_geometry_t *geometry,
                     uft_tarbell_disk_t *disk)
{
    if (!data || !geometry || !disk) return -1;
    
    memset(disk, 0, sizeof(*disk));
    disk->geometry = *geometry;
    
    /* Verify size */
    size_t expected = geometry->tracks * geometry->sectors_per_track * 
                      geometry->sector_size;
    if (geometry->single_sided) {
        if (size < expected) return -1;
    }
    
    /* Copy data */
    disk->data = (uint8_t*)malloc(size);
    if (!disk->data) return -1;
    memcpy(disk->data, data, size);
    disk->data_size = size;
    
    /* Parse directory */
    size_t dir_offset = geometry->reserved_tracks * 
                        geometry->sectors_per_track * 
                        geometry->sector_size;
    
    int dir_size = (geometry->dir_blocks * geometry->block_size) / 32;
    disk->directory = (uft_cpm_dirent_t*)calloc(dir_size, sizeof(uft_cpm_dirent_t));
    if (!disk->directory) {
        free(disk->data);
        return -1;
    }
    
    disk->dir_entries = 0;
    for (int i = 0; i < dir_size && dir_offset + 32 <= size; i++) {
        memcpy(&disk->directory[i], data + dir_offset, 32);
        if (disk->directory[i].user_number <= 15) {
            disk->dir_entries++;
        }
        dir_offset += 32;
    }
    
    /* Calculate total blocks */
    int data_sectors = (geometry->tracks - geometry->reserved_tracks) *
                       geometry->sectors_per_track;
    if (!geometry->single_sided) data_sectors *= 2;
    
    disk->total_blocks = (data_sectors * geometry->sector_size) / 
                         geometry->block_size;
    
    return 0;
}

int uft_tarbell_list_files(const uft_tarbell_disk_t *disk,
                           uft_cpm_dirent_t *entries,
                           int max_entries)
{
    if (!disk || !disk->directory || !entries) return 0;
    
    int count = 0;
    
    /* Only return first extent of each file */
    for (int i = 0; i < disk->dir_entries && count < max_entries; i++) {
        const uft_cpm_dirent_t *e = &disk->directory[i];
        
        if (e->user_number > 15) continue;
        if (e->extent_low != 0 || e->extent_high != 0) continue;  /* Not first extent */
        
        entries[count++] = *e;
    }
    
    return count;
}

void uft_tarbell_free(uft_tarbell_disk_t *disk)
{
    if (!disk) return;
    
    free(disk->data);
    free(disk->directory);
    free(disk->allocation_map);
    memset(disk, 0, sizeof(*disk));
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * NINTENDO GAMECUBE DISK FORMAT
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_gcm_detect(const uint8_t *data, size_t size)
{
    if (!data || size < 0x500) return 0;
    
    /* Check GameCube magic at offset 0x1C */
    uint32_t magic = (data[0x1C] << 24) | (data[0x1D] << 16) | 
                     (data[0x1E] << 8) | data[0x1F];
    
    if (magic == UFT_GCM_MAGIC) return 95;
    
    /* Check for 'G' console ID */
    if (data[0] == 'G') return 30;
    
    return 0;
}

int uft_gcm_open(const uint8_t *data, size_t size, uft_gcm_disc_t *disc)
{
    if (!data || !disc || size < 0x500) return -1;
    
    memset(disc, 0, sizeof(*disc));
    
    /* Copy header */
    memcpy(&disc->header, data, sizeof(uft_gcm_header_t));
    
    /* Verify magic */
    if (disc->header.gc_magic != UFT_GCM_MAGIC) {
        return -1;
    }
    
    /* Copy disc info */
    memcpy(&disc->disc_info, data + 0x420, sizeof(uft_gcm_disc_info_t));
    
    /* Read FST */
    uint32_t fst_offset = disc->disc_info.fst_offset;
    uint32_t fst_size = disc->disc_info.fst_size;
    
    if (fst_offset == 0 || fst_offset + fst_size > size) {
        return -1;
    }
    
    /* First entry tells us entry count */
    const uft_gcm_fst_entry_t *first = (const uft_gcm_fst_entry_t*)(data + fst_offset);
    disc->fst_entries = first->size_or_next;
    
    if (disc->fst_entries == 0 || disc->fst_entries > 10000) {
        return -1;
    }
    
    /* Copy FST */
    size_t fst_table_size = disc->fst_entries * sizeof(uft_gcm_fst_entry_t);
    disc->fst = (uft_gcm_fst_entry_t*)malloc(fst_table_size);
    if (!disc->fst) return -1;
    memcpy(disc->fst, data + fst_offset, fst_table_size);
    
    /* String table follows FST */
    size_t string_table_offset = fst_offset + fst_table_size;
    size_t string_table_size = fst_size - fst_table_size;
    
    disc->string_table = (char*)malloc(string_table_size + 1);
    if (!disc->string_table) {
        free(disc->fst);
        return -1;
    }
    memcpy(disc->string_table, data + string_table_offset, string_table_size);
    disc->string_table[string_table_size] = '\0';
    
    /* Build file list */
    disc->files = (uft_gcm_file_t*)calloc(disc->fst_entries, sizeof(uft_gcm_file_t));
    if (!disc->files) {
        free(disc->fst);
        free(disc->string_table);
        return -1;
    }
    
    disc->file_count = 0;
    for (int i = 0; i < disc->fst_entries; i++) {
        uft_gcm_fst_entry_t *e = &disc->fst[i];
        uft_gcm_file_t *f = &disc->files[disc->file_count];
        
        /* Get name offset (24-bit) */
        uint32_t name_off = (e->name_offset[0] << 16) | 
                            (e->name_offset[1] << 8) | 
                            e->name_offset[2];
        
        if (name_off < string_table_size) {
            strncpy(f->name, disc->string_table + name_off, 255);
            f->name[255] = '\0';
        }
        
        f->is_directory = (e->flags & 1) != 0;
        
        if (f->is_directory) {
            f->parent = e->offset_or_parent;
            f->size = e->size_or_next;  /* Next entry index */
        } else {
            f->offset = e->offset_or_parent;
            f->size = e->size_or_next;
        }
        
        disc->file_count++;
    }
    
    /* Store data reference */
    disc->data = (uint8_t*)malloc(size);
    if (disc->data) {
        memcpy(disc->data, data, size);
        disc->data_size = size;
    }
    
    return 0;
}

void uft_gcm_info(const uft_gcm_disc_t *disc)
{
    if (!disc) return;
    
    printf("GameCube Disc Info:\n");
    printf("  Game Code: %.4s\n", (char*)&disc->header.console_id);
    printf("  Region: %c\n", disc->header.region_code);
    printf("  Title: %.64s\n", disc->header.game_name);
    printf("  Files: %d\n", disc->file_count);
    printf("  DOL Offset: 0x%08X\n", disc->disc_info.dol_offset);
    printf("  FST Offset: 0x%08X\n", disc->disc_info.fst_offset);
}

int uft_gcm_list_files(const uft_gcm_disc_t *disc,
                       uft_gcm_file_t *files, int max_files)
{
    if (!disc || !files) return 0;
    
    int count = 0;
    for (int i = 0; i < disc->file_count && count < max_files; i++) {
        files[count++] = disc->files[i];
    }
    
    return count;
}

int uft_gcm_extract_file(const uft_gcm_disc_t *disc,
                         const char *path,
                         uint8_t **data, size_t *size)
{
    if (!disc || !path || !data || !size) return -1;
    
    /* Find file */
    for (int i = 0; i < disc->file_count; i++) {
        if (disc->files[i].is_directory) continue;
        
        if (strcmp(disc->files[i].name, path) == 0) {
            uint32_t offset = disc->files[i].offset;
            uint32_t file_size = disc->files[i].size;
            
            if (offset + file_size > disc->data_size) return -1;
            
            *data = (uint8_t*)malloc(file_size);
            if (!*data) return -1;
            
            memcpy(*data, disc->data + offset, file_size);
            *size = file_size;
            
            return 0;
        }
    }
    
    return -1;  /* Not found */
}

void uft_gcm_free(uft_gcm_disc_t *disc)
{
    if (!disc) return;
    
    free(disc->fst);
    free(disc->string_table);
    free(disc->files);
    free(disc->data);
    memset(disc, 0, sizeof(*disc));
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * ADDITIONAL ALGORITHMS
 * ═══════════════════════════════════════════════════════════════════════════════ */

double uft_entropy(const uint8_t *data, size_t length)
{
    if (!data || length == 0) return 0.0;
    
    /* Count byte frequencies */
    size_t freq[256] = {0};
    for (size_t i = 0; i < length; i++) {
        freq[data[i]]++;
    }
    
    /* Calculate entropy */
    double entropy = 0.0;
    for (int i = 0; i < 256; i++) {
        if (freq[i] > 0) {
            double p = (double)freq[i] / length;
            entropy -= p * log2(p);
        }
    }
    
    return entropy;
}

uft_compress_type_t uft_detect_compression(const uint8_t *data, size_t length)
{
    if (!data || length < 4) return UFT_COMPRESS_NONE;
    
    /* Check for common compression signatures */
    
    /* DEFLATE/ZLIB/GZIP */
    if ((data[0] == 0x78 && (data[1] == 0x01 || data[1] == 0x9C || data[1] == 0xDA)) ||
        (data[0] == 0x1F && data[1] == 0x8B)) {
        return UFT_COMPRESS_DEFLATE;
    }
    
    /* LZ4 */
    if (data[0] == 0x04 && data[1] == 0x22 && data[2] == 0x4D && data[3] == 0x18) {
        return UFT_COMPRESS_LZ;
    }
    
    /* Calculate entropy to detect compression */
    double ent = uft_entropy(data, length > 4096 ? 4096 : length);
    
    if (ent > 7.9) return UFT_COMPRESS_UNKNOWN;  /* Very high entropy = encrypted or compressed */
    if (ent > 7.0) return UFT_COMPRESS_LZ;       /* High entropy = likely compressed */
    
    return UFT_COMPRESS_NONE;
}

int uft_find_repeats(const uint8_t *data, size_t data_len,
                     size_t min_length,
                     uft_repeat_t *repeats, int max_repeats)
{
    if (!data || !repeats || data_len < min_length * 2) return 0;
    
    int count = 0;
    
    /* Simple O(n²) algorithm for finding repeats */
    for (size_t i = 0; i < data_len - min_length && count < max_repeats; i++) {
        /* Check if this position starts a repeat we haven't found yet */
        bool already_found = false;
        for (int j = 0; j < count; j++) {
            if (i >= repeats[j].offset && 
                i < repeats[j].offset + repeats[j].length) {
                already_found = true;
                break;
            }
        }
        if (already_found) continue;
        
        /* Look for this sequence elsewhere */
        int repeat_count = 1;
        size_t best_length = min_length;
        
        for (size_t j = i + min_length; j < data_len - min_length; j++) {
            size_t len = 0;
            while (i + len < data_len && j + len < data_len && 
                   data[i + len] == data[j + len]) {
                len++;
            }
            
            if (len >= min_length) {
                repeat_count++;
                if (len > best_length) best_length = len;
            }
        }
        
        if (repeat_count > 1) {
            repeats[count].offset = i;
            repeats[count].length = best_length;
            repeats[count].count = repeat_count;
            count++;
        }
    }
    
    return count;
}
