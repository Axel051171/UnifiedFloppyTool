/**
 * @file uft_d64_bam.c
 * @brief D64 Block Allocation Map (BAM) Extended API Implementation
 * @version 1.0.0
 * 
 * Based on "The Little Black Book" forensic extraction techniques.
 * 
 * SPDX-License-Identifier: MIT
 */

#include <uft/cbm/uft_d64_bam.h>
#include <uft/cbm/uft_d64_layout.h>
#include <uft/cbm/uft_petscii.h>
#include <string.h>
#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal Helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get BAM sector pointer (read-only)
 */
static const uint8_t* get_bam_sector(const uft_d64_image_t* img)
{
    if (!img || !img->data || !img->layout) return NULL;
    return uft_d64_get_sector_const(img, UFT_D64_BAM_TRACK, UFT_D64_BAM_SECTOR);
}

/**
 * @brief Get BAM sector pointer (writable)
 */
static uint8_t* get_bam_sector_mut(uft_d64_image_t* img)
{
    if (!img || !img->data || !img->layout) return NULL;
    return uft_d64_get_sector(img, UFT_D64_BAM_TRACK, UFT_D64_BAM_SECTOR);
}

/**
 * @brief Sanitize PETSCII to ASCII
 * Conservative: map 0xA0 padding to space, keep printable ASCII, else '?'
 */
static void sanitize_petscii(char* out, size_t out_size, 
                             const uint8_t* petscii, size_t len)
{
    size_t w = 0;
    for (size_t i = 0; i < len && w + 1 < out_size; i++) {
        uint8_t b = petscii[i];
        if (b == 0xA0) {
            /* Shifted space / padding - treat as end or space */
            out[w++] = ' ';
            continue;
        }
        if (b >= 0x20 && b <= 0x7E) {
            out[w++] = (char)b;
        } else if (b >= 0x41 && b <= 0x5A) {
            /* PETSCII uppercase A-Z */
            out[w++] = (char)b;
        } else if (b >= 0xC1 && b <= 0xDA) {
            /* PETSCII shifted letters -> ASCII uppercase */
            out[w++] = (char)(b - 0x80);
        } else {
            out[w++] = '?';
        }
    }
    out[w] = '\0';
    
    /* Trim trailing spaces */
    while (w > 0 && out[w - 1] == ' ') {
        out[--w] = '\0';
    }
}

/**
 * @brief Convert ASCII to PETSCII for disk operations
 */
static void ascii_to_petscii_padded(uint8_t* out, size_t out_size,
                                    const char* ascii, uint8_t pad_char)
{
    size_t len = ascii ? strlen(ascii) : 0;
    for (size_t i = 0; i < out_size; i++) {
        if (i < len) {
            char c = ascii[i];
            if (c >= 'a' && c <= 'z') {
                out[i] = (uint8_t)(c - 'a' + 0xC1); /* PETSCII shifted */
            } else if (c >= 'A' && c <= 'Z') {
                out[i] = (uint8_t)c;
            } else if (c >= 0x20 && c <= 0x7E) {
                out[i] = (uint8_t)c;
            } else {
                out[i] = pad_char;
            }
        } else {
            out[i] = pad_char;
        }
    }
}

/**
 * @brief Calculate BAM entry offset for a track
 */
static size_t bam_entry_offset(uint8_t track)
{
    if (track < 1 || track > 42) return 0;
    return UFT_D64_BAM_OFF_ENTRIES + (size_t)(track - 1) * 4;
}

/**
 * @brief Set or clear a bit in BAM bitmap
 */
static int set_bam_bit(uint8_t* bam, uint8_t track, uint8_t sector, 
                       bool is_free, const uft_d64_layout_t* layout)
{
    if (!bam || !layout) return -1;
    if (track < 1 || track > layout->max_tracks) return -1;
    
    uint8_t spt = uft_d64_sectors_per_track(layout, track);
    if (sector >= spt) return -1;
    
    size_t base = bam_entry_offset(track);
    size_t byte_idx = base + 1 + (sector / 8);
    uint8_t bit = (uint8_t)(1u << (sector % 8));
    
    if (byte_idx >= 256) return -1;
    
    if (is_free) {
        bam[byte_idx] |= bit;
    } else {
        bam[byte_idx] &= (uint8_t)~bit;
    }
    
    return 0;
}

/**
 * @brief Recalculate free count for a single track
 */
static uint8_t calc_track_free_count(const uint8_t* bam, uint8_t track,
                                     const uft_d64_layout_t* layout)
{
    if (!bam || !layout || track < 1 || track > layout->max_tracks) return 0;
    
    uint8_t spt = uft_d64_sectors_per_track(layout, track);
    size_t base = bam_entry_offset(track);
    uint8_t count = 0;
    
    for (uint8_t s = 0; s < spt; s++) {
        size_t byte_idx = base + 1 + (s / 8);
        uint8_t bit = (uint8_t)(1u << (s % 8));
        if (bam[byte_idx] & bit) count++;
    }
    
    return count;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * BAM Information Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_d64_bam_read_info(const uft_d64_image_t* img, uft_d64_bam_info_t* info)
{
    if (!img || !info) return -1;
    
    const uint8_t* bam = get_bam_sector(img);
    if (!bam) return -1;
    
    memset(info, 0, sizeof(*info));
    
    info->dir_track = bam[UFT_D64_BAM_OFF_DIR_TRACK];
    info->dir_sector = bam[UFT_D64_BAM_OFF_DIR_SECTOR];
    info->dos_version = bam[UFT_D64_BAM_OFF_DOS_VERSION];
    
    /* Disk name: 16 bytes at offset 0x90 */
    sanitize_petscii(info->disk_name, sizeof(info->disk_name),
                     &bam[UFT_D64_BAM_OFF_DISK_NAME], 16);
    
    /* Disk ID: 2 bytes at offset 0xA2, followed by 0xA0, then DOS type */
    sanitize_petscii(info->disk_id, sizeof(info->disk_id),
                     &bam[UFT_D64_BAM_OFF_DISK_ID], 2);
    
    /* DOS type: 2 bytes at offset 0xA5 */
    sanitize_petscii(info->dos_type, sizeof(info->dos_type),
                     &bam[UFT_D64_BAM_OFF_DOS_TYPE], 2);
    
    /* Check write-protect: non-0x41 DOS version often indicates protection */
    info->is_write_protected = (info->dos_version != UFT_D64_DOS_VERSION_1541);
    
    /* Calculate free blocks */
    info->free_blocks = uft_d64_bam_get_free_blocks(img);
    
    return 0;
}

uint16_t uft_d64_bam_get_free_blocks(const uft_d64_image_t* img)
{
    if (!img || !img->layout) return 0;
    
    const uint8_t* bam = get_bam_sector(img);
    if (!bam) return 0;
    
    uint16_t total = 0;
    for (uint8_t t = 1; t <= img->layout->max_tracks; t++) {
        /* Skip directory track (18) in free count - standard 1541 behavior */
        if (t == UFT_D64_BAM_TRACK) continue;
        
        size_t base = bam_entry_offset(t);
        total += bam[base]; /* Free count byte */
    }
    
    return total;
}

uint8_t uft_d64_bam_get_track_free(const uft_d64_image_t* img, uint8_t track)
{
    if (!img || !img->layout) return 0;
    if (track < 1 || track > img->layout->max_tracks) return 0;
    
    const uint8_t* bam = get_bam_sector(img);
    if (!bam) return 0;
    
    size_t base = bam_entry_offset(track);
    return bam[base];
}

bool uft_d64_bam_is_allocated(const uft_d64_image_t* img, 
                               uint8_t track, uint8_t sector)
{
    if (!img || !img->layout) return false;
    if (track < 1 || track > img->layout->max_tracks) return false;
    
    uint8_t spt = uft_d64_sectors_per_track(img->layout, track);
    if (sector >= spt) return false;
    
    const uint8_t* bam = get_bam_sector(img);
    if (!bam) return false;
    
    size_t base = bam_entry_offset(track);
    size_t byte_idx = base + 1 + (sector / 8);
    uint8_t bit = (uint8_t)(1u << (sector % 8));
    
    /* Bit set = free, bit clear = allocated */
    return (bam[byte_idx] & bit) == 0;
}

int uft_d64_bam_read_entry(const uft_d64_image_t* img, uint8_t track,
                           uft_d64_bam_entry_t* entry)
{
    if (!img || !entry || !img->layout) return -1;
    if (track < 1 || track > img->layout->max_tracks) return -1;
    
    const uint8_t* bam = get_bam_sector(img);
    if (!bam) return -1;
    
    size_t base = bam_entry_offset(track);
    
    entry->track = track;
    entry->free_count = bam[base];
    entry->bitmap[0] = bam[base + 1];
    entry->bitmap[1] = bam[base + 2];
    entry->bitmap[2] = bam[base + 3];
    
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * BAM Modification Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_d64_bam_write_dos_version(uft_d64_image_t* img, uint8_t version)
{
    if (!img) return -1;
    
    uint8_t* bam = get_bam_sector_mut(img);
    if (!bam) return -1;
    
    bam[UFT_D64_BAM_OFF_DOS_VERSION] = version;
    img->modified = true;
    
    return 0;
}

int uft_d64_bam_allocate_sector(uft_d64_image_t* img, 
                                 uint8_t track, uint8_t sector)
{
    if (!img || !img->layout) return -1;
    
    uint8_t* bam = get_bam_sector_mut(img);
    if (!bam) return -1;
    
    int rc = set_bam_bit(bam, track, sector, false, img->layout);
    if (rc != 0) return rc;
    
    /* Update free count */
    size_t base = bam_entry_offset(track);
    bam[base] = calc_track_free_count(bam, track, img->layout);
    
    img->modified = true;
    return 0;
}

int uft_d64_bam_free_sector(uft_d64_image_t* img, 
                             uint8_t track, uint8_t sector)
{
    if (!img || !img->layout) return -1;
    
    uint8_t* bam = get_bam_sector_mut(img);
    if (!bam) return -1;
    
    int rc = set_bam_bit(bam, track, sector, true, img->layout);
    if (rc != 0) return rc;
    
    /* Update free count */
    size_t base = bam_entry_offset(track);
    bam[base] = calc_track_free_count(bam, track, img->layout);
    
    img->modified = true;
    return 0;
}

uft_d64_bam_options_t uft_d64_bam_default_options(void)
{
    uft_d64_bam_options_t opts = {
        .preserve_directory = true,
        .preserve_bam = true,
        .dry_run = false
    };
    return opts;
}

int uft_d64_bam_allocate_all(uft_d64_image_t* img, 
                              const uft_d64_bam_options_t* options)
{
    if (!img || !img->layout) return -1;
    
    uft_d64_bam_options_t opts = options ? *options : uft_d64_bam_default_options();
    
    if (opts.dry_run) return 0;
    
    uint8_t* bam = get_bam_sector_mut(img);
    if (!bam) return -1;
    
    /* Get directory track info for preservation */
    uint8_t dir_track = bam[UFT_D64_BAM_OFF_DIR_TRACK];
    uint8_t dir_sector = bam[UFT_D64_BAM_OFF_DIR_SECTOR];
    
    /* Allocate all sectors on all tracks */
    for (uint8_t t = 1; t <= img->layout->max_tracks; t++) {
        uint8_t spt = uft_d64_sectors_per_track(img->layout, t);
        
        for (uint8_t s = 0; s < spt; s++) {
            /* Check preservation rules */
            if (opts.preserve_bam && t == UFT_D64_BAM_TRACK && s == UFT_D64_BAM_SECTOR) {
                continue;
            }
            if (opts.preserve_directory && t == dir_track && s == dir_sector) {
                continue;
            }
            
            set_bam_bit(bam, t, s, false, img->layout);
        }
        
        /* Update free count for track */
        size_t base = bam_entry_offset(t);
        bam[base] = calc_track_free_count(bam, t, img->layout);
    }
    
    img->modified = true;
    return 0;
}

int uft_d64_bam_free_all(uft_d64_image_t* img, 
                          const uft_d64_bam_options_t* options)
{
    if (!img || !img->layout) return -1;
    
    uft_d64_bam_options_t opts = options ? *options : uft_d64_bam_default_options();
    
    if (opts.dry_run) return 0;
    
    uint8_t* bam = get_bam_sector_mut(img);
    if (!bam) return -1;
    
    uint8_t dir_track = bam[UFT_D64_BAM_OFF_DIR_TRACK];
    uint8_t dir_sector = bam[UFT_D64_BAM_OFF_DIR_SECTOR];
    
    /* Free all sectors on all tracks */
    for (uint8_t t = 1; t <= img->layout->max_tracks; t++) {
        uint8_t spt = uft_d64_sectors_per_track(img->layout, t);
        
        for (uint8_t s = 0; s < spt; s++) {
            if (opts.preserve_bam && t == UFT_D64_BAM_TRACK && s == UFT_D64_BAM_SECTOR) {
                set_bam_bit(bam, t, s, false, img->layout); /* Keep allocated */
                continue;
            }
            if (opts.preserve_directory && t == dir_track && s == dir_sector) {
                set_bam_bit(bam, t, s, false, img->layout); /* Keep allocated */
                continue;
            }
            
            set_bam_bit(bam, t, s, true, img->layout);
        }
        
        size_t base = bam_entry_offset(t);
        bam[base] = calc_track_free_count(bam, t, img->layout);
    }
    
    img->modified = true;
    return 0;
}

int uft_d64_bam_unwrite_protect(uft_d64_image_t* img)
{
    return uft_d64_bam_write_dos_version(img, UFT_D64_DOS_VERSION_1541);
}

int uft_d64_bam_set_disk_name(uft_d64_image_t* img, const char* name)
{
    if (!img || !name) return -1;
    
    uint8_t* bam = get_bam_sector_mut(img);
    if (!bam) return -1;
    
    ascii_to_petscii_padded(&bam[UFT_D64_BAM_OFF_DISK_NAME], 16, name, 0xA0);
    img->modified = true;
    
    return 0;
}

int uft_d64_bam_set_disk_id(uft_d64_image_t* img, const char* id)
{
    if (!img || !id) return -1;
    
    uint8_t* bam = get_bam_sector_mut(img);
    if (!bam) return -1;
    
    ascii_to_petscii_padded(&bam[UFT_D64_BAM_OFF_DISK_ID], 2, id, 0xA0);
    img->modified = true;
    
    return 0;
}

int uft_d64_bam_recalculate_free_counts(uft_d64_image_t* img)
{
    if (!img || !img->layout) return -1;
    
    uint8_t* bam = get_bam_sector_mut(img);
    if (!bam) return -1;
    
    for (uint8_t t = 1; t <= img->layout->max_tracks; t++) {
        size_t base = bam_entry_offset(t);
        bam[base] = calc_track_free_count(bam, t, img->layout);
    }
    
    img->modified = true;
    return 0;
}
