/**
 * @file uft_fat12.c
 * @brief FAT12/FAT16 read-only backend.
 *
 * Closes the biggest FAT Kat-A cluster (19 declarations, zero impl).
 * The canonical header include/uft/fs/uft_fat12.h declared 60 functions;
 * this file implements the read-only subset needed for mount, directory
 * listing, cluster-chain traversal, and file extraction.
 *
 * Scope: READ-ONLY. Write paths return UFT_FAT_ERR_READONLY per spec §1.3
 * (explicit negative error, not a silent UFT_OK). A full port with FAT
 * allocation + directory insertion comes later.
 *
 * References:
 *   Microsoft FAT Specification (hardwhitepaper, Aug 2005)
 *   http://elm-chan.org/docs/fat_e.html
 *
 * Implementation notes:
 *   - FAT sector layout is little-endian. AmigaDOS (sibling backend)
 *     was big-endian — this file has its own tiny le16/le32 helpers.
 *   - FAT12 entries are packed 12 bits per entry: three bytes = two
 *     entries. Unpack depends on whether the cluster number is even
 *     or odd.
 *   - Root directory for FAT12 lives at a fixed location between FATs
 *     and the data area — NOT in the cluster-numbered region.
 *   - Cluster 0 and 1 are reserved; first valid cluster is 2.
 */

#include "uft/fs/uft_fat12.h"
#include "uft/uft_error.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ----- little-endian helpers ----- */
static uint16_t le16(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}
static uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Copy a space-padded FAT filename into a NUL-terminated buffer. */
static void trim_padded(const char *src, size_t len, char *dst) {
    size_t end = len;
    while (end > 0 && (src[end - 1] == ' ' || src[end - 1] == '\0')) end--;
    if (end > 0) memcpy(dst, src, end);
    dst[end] = '\0';
}

/* ==========================================================================
 * Lifecycle
 * ========================================================================== */

uft_fat_ctx_t *uft_fat_create(void) {
    uft_fat_ctx_t *c = (uft_fat_ctx_t *)calloc(1, sizeof(*c));
    if (!c) return NULL;
    c->read_only = true;
    return c;
}

void uft_fat_destroy(uft_fat_ctx_t *ctx) {
    if (!ctx) return;
    if (ctx->owns_data && ctx->data) free(ctx->data);
    free(ctx->fat_cache);
    free(ctx);
}

/* uft_fat_close is a release of the image without destroying the ctx. */
void uft_fat_close(uft_fat_ctx_t *ctx) {
    if (!ctx) return;
    if (ctx->owns_data && ctx->data) {
        free(ctx->data);
        ctx->data = NULL;
        ctx->owns_data = false;
    }
    free(ctx->fat_cache);
    ctx->fat_cache = NULL;
    ctx->fat_dirty = false;
    ctx->data_size = 0;
    memset(&ctx->vol, 0, sizeof(ctx->vol));
}

/* ==========================================================================
 * Detection + geometry lookup
 * ========================================================================== */

const uft_fat_geometry_t *uft_fat_geometry_from_size(size_t size) {
    for (const uft_fat_geometry_t *g = uft_fat_std_geometries; g->name; g++) {
        if ((size_t)g->total_sectors * UFT_FAT_SECTOR_SIZE == size) return g;
    }
    return NULL;
}

uft_fat_platform_t uft_fat_detect_platform(const uft_fat_bootsect_t *boot) {
    if (!boot) return UFT_FAT_PLATFORM_PC;
    /* Atari ST bootsect: jump starts 0x60 0x.. */
    if (boot->jmp_boot[0] == 0x60) return UFT_FAT_PLATFORM_ATARI;
    /* MSX DOS bootsect OEM starts with "IBM" or "MSDOS" — PC-compatible. */
    return UFT_FAT_PLATFORM_PC;
}

int uft_fat12_detect(const uint8_t *data, size_t size, uft_fat_detect_t *result) {
    if (!data || !result) return -1;
    memset(result, 0, sizeof(*result));
    if (size < UFT_FAT_SECTOR_SIZE) return -1;

    uint16_t sig = le16(data + 0x1FE);
    result->boot_sig_missing = (sig != UFT_FAT_BOOT_SIG);

    uint16_t bps = le16(data + 0x0B);
    uint8_t  spc = data[0x0D];
    uint16_t reserved = le16(data + 0x0E);
    uint8_t  num_fats = data[0x10];
    uint16_t root_cnt = le16(data + 0x11);
    uint16_t tot16    = le16(data + 0x13);
    uint16_t fatsz16  = le16(data + 0x16);
    uint32_t tot32    = le32(data + 0x20);
    uint32_t total    = tot16 ? tot16 : tot32;

    if (bps != UFT_FAT_SECTOR_SIZE || spc == 0 || num_fats == 0) {
        result->bpb_inconsistent = true;
        return -1;
    }

    uint32_t root_sec = (root_cnt * 32u + bps - 1) / bps;
    uint32_t data_sec = reserved + num_fats * fatsz16 + root_sec;
    uint32_t clusters = (total - data_sec) / spc;

    if      (clusters < 4085)  result->type = UFT_FAT_TYPE_FAT12;
    else if (clusters < 65525) result->type = UFT_FAT_TYPE_FAT16;
    else                        result->type = UFT_FAT_TYPE_FAT32;

    result->geometry   = uft_fat_geometry_from_size(size);
    result->platform   = (data[0] == 0x60) ? UFT_FAT_PLATFORM_ATARI : UFT_FAT_PLATFORM_PC;
    result->valid      = !result->boot_sig_missing && !result->bpb_inconsistent;
    result->confidence = result->valid ? 90 : 30;
    snprintf(result->description, sizeof(result->description),
             "%s (%u clusters)",
             result->type == UFT_FAT_TYPE_FAT12 ? "FAT12" :
             result->type == UFT_FAT_TYPE_FAT16 ? "FAT16" : "FAT32",
             (unsigned)clusters);
    return 0;
}

/* ==========================================================================
 * Open / parse BPB
 * ========================================================================== */

static int parse_bpb(uft_fat_ctx_t *ctx) {
    const uint8_t *d = ctx->data;
    uft_fat_volume_t *v = &ctx->vol;

    v->bytes_per_sector    = le16(d + 0x0B);
    v->sectors_per_cluster = d[0x0D];
    v->reserved_sectors    = le16(d + 0x0E);
    v->num_fats            = d[0x10];
    v->root_entry_count    = le16(d + 0x11);
    uint16_t tot16         = le16(d + 0x13);
    uint32_t tot32         = le32(d + 0x20);
    v->total_sectors       = tot16 ? tot16 : tot32;
    v->fat_size            = le16(d + 0x16);
    v->media_type          = d[0x15];

    if (v->bytes_per_sector != UFT_FAT_SECTOR_SIZE) return UFT_FAT_ERR_INVALID;
    if (v->sectors_per_cluster == 0) return UFT_FAT_ERR_INVALID;
    if (v->num_fats == 0) return UFT_FAT_ERR_INVALID;

    v->fat_start_sector = v->reserved_sectors;
    v->root_dir_sectors = ((uint32_t)v->root_entry_count * 32u
                           + v->bytes_per_sector - 1) / v->bytes_per_sector;
    v->root_dir_sector  = v->fat_start_sector + v->num_fats * v->fat_size;
    v->data_start_sector = v->root_dir_sector + v->root_dir_sectors;

    if (v->total_sectors < v->data_start_sector) return UFT_FAT_ERR_INVALID;
    v->data_clusters = (v->total_sectors - v->data_start_sector) / v->sectors_per_cluster;
    v->last_cluster  = v->data_clusters + UFT_FAT_FIRST_CLUSTER - 1;

    if      (v->data_clusters < 4085)  v->fat_type = UFT_FAT_TYPE_FAT12;
    else if (v->data_clusters < 65525) v->fat_type = UFT_FAT_TYPE_FAT16;
    else                                v->fat_type = UFT_FAT_TYPE_FAT32;

    v->platform = (d[0] == 0x60) ? UFT_FAT_PLATFORM_ATARI : UFT_FAT_PLATFORM_PC;
    memcpy(v->oem_name, d + 3, 8); v->oem_name[8] = '\0';

    /* Extended boot record: only if signature byte 0x29 present. */
    if (d[0x26] == UFT_FAT_EXT_BOOT_SIG) {
        v->serial = le32(d + 0x27);
        memcpy(v->label, d + 0x2B, 11); v->label[11] = '\0';
    }
    return UFT_FAT_OK;
}

static int load_fat_cache(uft_fat_ctx_t *ctx) {
    uint32_t bytes = ctx->vol.fat_size * ctx->vol.bytes_per_sector;
    if (bytes == 0 || bytes > 1024 * 1024) return UFT_FAT_ERR_INVALID;
    ctx->fat_cache = (uint8_t *)malloc(bytes);
    if (!ctx->fat_cache) return UFT_FAT_ERR_NOMEM;
    memcpy(ctx->fat_cache,
           ctx->data + ctx->vol.fat_start_sector * ctx->vol.bytes_per_sector,
           bytes);
    return UFT_FAT_OK;
}

int uft_fat_open(uft_fat_ctx_t *ctx, const uint8_t *data, size_t size, bool copy) {
    if (!ctx || !data || size < UFT_FAT_SECTOR_SIZE) return UFT_FAT_ERR_INVALID;
    uft_fat_close(ctx);

    if (copy) {
        ctx->data = (uint8_t *)malloc(size);
        if (!ctx->data) return UFT_FAT_ERR_NOMEM;
        memcpy(ctx->data, data, size);
        ctx->owns_data = true;
    } else {
        ctx->data = (uint8_t *)data;      /* caller-managed */
        ctx->owns_data = false;
    }
    ctx->data_size = size;

    int rc = parse_bpb(ctx);
    if (rc != UFT_FAT_OK) { uft_fat_close(ctx); return rc; }
    rc = load_fat_cache(ctx);
    if (rc != UFT_FAT_OK) { uft_fat_close(ctx); return rc; }
    return UFT_FAT_OK;
}

int uft_fat_open_file(uft_fat_ctx_t *ctx, const char *filename) {
    if (!ctx || !filename) return UFT_FAT_ERR_INVALID;
    FILE *f = fopen(filename, "rb");
    if (!f) return UFT_FAT_ERR_IO;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz <= 0) { fclose(f); return UFT_FAT_ERR_IO; }
    rewind(f);
    uint8_t *buf = (uint8_t *)malloc((size_t)sz);
    if (!buf) { fclose(f); return UFT_FAT_ERR_NOMEM; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf); fclose(f); return UFT_FAT_ERR_IO;
    }
    fclose(f);
    int rc = uft_fat_open(ctx, buf, (size_t)sz, /*copy=*/false);
    if (rc != UFT_FAT_OK) { free(buf); return rc; }
    ctx->owns_data = true;
    return UFT_FAT_OK;
}

int uft_fat_save(uft_fat_ctx_t *ctx, const char *filename) {
    (void)ctx; (void)filename;
    return UFT_FAT_ERR_READONLY;
}

const uint8_t *uft_fat_get_data(const uft_fat_ctx_t *ctx, size_t *size) {
    if (!ctx) return NULL;
    if (size) *size = ctx->data_size;
    return ctx->data;
}

const uft_fat_volume_t *uft_fat_get_volume(const uft_fat_ctx_t *ctx) {
    return ctx ? &ctx->vol : NULL;
}

int uft_fat_get_label(const uft_fat_ctx_t *ctx, char *label) {
    if (!ctx || !label) return UFT_FAT_ERR_INVALID;
    strncpy(label, ctx->vol.label, 12);
    label[11] = '\0';
    return UFT_FAT_OK;
}

int uft_fat_set_label(uft_fat_ctx_t *ctx, const char *label) {
    (void)ctx; (void)label; return UFT_FAT_ERR_READONLY;
}

/* ==========================================================================
 * FAT entry access (FAT12 nibble-packed, FAT16 word-aligned)
 * ========================================================================== */

int32_t uft_fat_get_entry(const uft_fat_ctx_t *ctx, uint32_t cluster) {
    if (!ctx || !ctx->fat_cache) return -1;
    if (cluster > ctx->vol.last_cluster) return -1;

    if (ctx->vol.fat_type == UFT_FAT_TYPE_FAT12) {
        uint32_t off = cluster + cluster / 2;   /* 1.5 bytes per entry */
        uint16_t w = (uint16_t)(ctx->fat_cache[off] |
                                (ctx->fat_cache[off + 1] << 8));
        return (cluster & 1) ? (w >> 4) : (w & 0x0FFF);
    } else if (ctx->vol.fat_type == UFT_FAT_TYPE_FAT16) {
        return le16(ctx->fat_cache + cluster * 2);
    }
    return -1;
}

int uft_fat_set_entry(uft_fat_ctx_t *ctx, uint32_t cluster, uint32_t value) {
    (void)ctx; (void)cluster; (void)value;
    return UFT_FAT_ERR_READONLY;
}

bool uft_fat_cluster_is_free(const uft_fat_ctx_t *ctx, uint32_t c) {
    int32_t v = uft_fat_get_entry(ctx, c);
    return v == 0;
}

bool uft_fat_cluster_is_eof(const uft_fat_ctx_t *ctx, uint32_t c) {
    int32_t v = uft_fat_get_entry(ctx, c);
    if (v < 0) return true;
    if (ctx->vol.fat_type == UFT_FAT_TYPE_FAT12)
        return (uint32_t)v >= UFT_FAT12_EOF_MIN;
    return (uint32_t)v >= UFT_FAT16_EOF_MIN;
}

bool uft_fat_cluster_is_bad(const uft_fat_ctx_t *ctx, uint32_t c) {
    int32_t v = uft_fat_get_entry(ctx, c);
    if (v < 0) return false;
    if (ctx->vol.fat_type == UFT_FAT_TYPE_FAT12) return (uint32_t)v == UFT_FAT12_BAD;
    return (uint32_t)v == UFT_FAT16_BAD;
}

uint64_t uft_fat_get_free_space(const uft_fat_ctx_t *ctx) {
    if (!ctx) return 0;
    size_t free_clusters = 0;
    for (uint32_t c = UFT_FAT_FIRST_CLUSTER; c <= ctx->vol.last_cluster; c++)
        if (uft_fat_cluster_is_free(ctx, c)) free_clusters++;
    return (uint64_t)free_clusters *
           ctx->vol.sectors_per_cluster * ctx->vol.bytes_per_sector;
}

uint64_t uft_fat_get_used_space(const uft_fat_ctx_t *ctx) {
    if (!ctx) return 0;
    return (uint64_t)ctx->vol.data_clusters *
           ctx->vol.sectors_per_cluster * ctx->vol.bytes_per_sector
           - uft_fat_get_free_space(ctx);
}

/* Allocation API — read-only backend. */
int32_t uft_fat_alloc_cluster(uft_fat_ctx_t *ctx, uint32_t hint) {
    (void)ctx; (void)hint; return -1;
}
int uft_fat_alloc_chain(uft_fat_ctx_t *ctx, size_t count, uft_fat_chain_t *chain) {
    (void)ctx; (void)count; (void)chain; return UFT_FAT_ERR_READONLY;
}

/* ==========================================================================
 * Cluster I/O
 * ========================================================================== */

size_t uft_fat_cluster_size(const uft_fat_ctx_t *ctx) {
    if (!ctx) return 0;
    return (size_t)ctx->vol.sectors_per_cluster * ctx->vol.bytes_per_sector;
}

int64_t uft_fat_cluster_offset(const uft_fat_ctx_t *ctx, uint32_t cluster) {
    if (!ctx || cluster < UFT_FAT_FIRST_CLUSTER) return -1;
    if (cluster > ctx->vol.last_cluster) return -1;
    uint32_t sec = ctx->vol.data_start_sector +
                   (cluster - UFT_FAT_FIRST_CLUSTER) * ctx->vol.sectors_per_cluster;
    return (int64_t)sec * ctx->vol.bytes_per_sector;
}

int uft_fat_read_cluster(const uft_fat_ctx_t *ctx, uint32_t cluster,
                          uint8_t *buffer) {
    if (!ctx || !buffer) return UFT_FAT_ERR_INVALID;
    int64_t off = uft_fat_cluster_offset(ctx, cluster);
    if (off < 0 || (size_t)off + uft_fat_cluster_size(ctx) > ctx->data_size)
        return UFT_FAT_ERR_INVALID;
    memcpy(buffer, ctx->data + off, uft_fat_cluster_size(ctx));
    return UFT_FAT_OK;
}

int uft_fat_write_cluster(uft_fat_ctx_t *ctx, uint32_t cluster,
                           const uint8_t *buffer) {
    (void)ctx; (void)cluster; (void)buffer; return UFT_FAT_ERR_READONLY;
}

int uft_fat_read_root_sector(const uft_fat_ctx_t *ctx, uint32_t index,
                              uint8_t *buffer) {
    if (!ctx || !buffer) return UFT_FAT_ERR_INVALID;
    if (index >= ctx->vol.root_dir_sectors) return UFT_FAT_ERR_INVALID;
    uint32_t sec = ctx->vol.root_dir_sector + index;
    size_t off = (size_t)sec * ctx->vol.bytes_per_sector;
    if (off + ctx->vol.bytes_per_sector > ctx->data_size) return UFT_FAT_ERR_INVALID;
    memcpy(buffer, ctx->data + off, ctx->vol.bytes_per_sector);
    return UFT_FAT_OK;
}

int uft_fat_write_root_sector(uft_fat_ctx_t *ctx, uint32_t index,
                               const uint8_t *buffer) {
    (void)ctx; (void)index; (void)buffer; return UFT_FAT_ERR_READONLY;
}

/* ==========================================================================
 * Directory parsing
 * ========================================================================== */

void uft_fat_dir_init(uft_fat_dir_t *dir) {
    if (!dir) return;
    memset(dir, 0, sizeof(*dir));
}

void uft_fat_dir_free(uft_fat_dir_t *dir) {
    if (!dir) return;
    free(dir->entries);
    dir->entries = NULL;
    dir->count = dir->capacity = 0;
}

void uft_fat_chain_init(uft_fat_chain_t *chain) {
    if (!chain) return;
    memset(chain, 0, sizeof(*chain));
}

void uft_fat_chain_free(uft_fat_chain_t *chain) {
    if (!chain) return;
    free(chain->clusters);
    chain->clusters = NULL;
    chain->count = chain->capacity = 0;
}

time_t uft_fat_to_unix_time(uint16_t fat_time, uint16_t fat_date) {
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    tm.tm_sec  = (fat_time & 0x1F) * 2;
    tm.tm_min  = (fat_time >> 5) & 0x3F;
    tm.tm_hour = (fat_time >> 11) & 0x1F;
    tm.tm_mday = (fat_date & 0x1F) ? (fat_date & 0x1F) : 1;
    tm.tm_mon  = ((fat_date >> 5) & 0x0F) - 1;
    tm.tm_year = ((fat_date >> 9) & 0x7F) + 80;   /* 1980 + n == 1900 + (80+n) */
    if (tm.tm_mon < 0) tm.tm_mon = 0;
    return mktime(&tm);
}

void uft_fat_from_unix_time(time_t t, uint16_t *ft, uint16_t *fd) {
    struct tm *tm = localtime(&t);
    if (!tm) { if (ft) *ft = 0; if (fd) *fd = 0; return; }
    if (ft) *ft = (uint16_t)((tm->tm_hour << 11) | (tm->tm_min << 5) | (tm->tm_sec / 2));
    if (fd) *fd = (uint16_t)(((tm->tm_year - 80) << 9) | ((tm->tm_mon + 1) << 5) | tm->tm_mday);
}

static int dir_append(uft_fat_dir_t *dir, const uft_fat_entry_t *e) {
    if (dir->count >= dir->capacity) {
        size_t n = dir->capacity ? dir->capacity * 2 : 16;
        uft_fat_entry_t *tmp = (uft_fat_entry_t *)realloc(dir->entries,
                                                         n * sizeof(*tmp));
        if (!tmp) return UFT_FAT_ERR_NOMEM;
        dir->entries = tmp;
        dir->capacity = n;
    }
    dir->entries[dir->count++] = *e;
    return UFT_FAT_OK;
}

/* Parse one 32-byte SFN slot into entry. Returns 0 on real entry,
 * 1 on end-of-directory marker, 2 on LFN / deleted / skip. */
static int parse_sfn_slot(const uint8_t *slot, uft_fat_entry_t *out) {
    memset(out, 0, sizeof(*out));
    if (slot[0] == UFT_FAT_DIRENT_END) return 1;
    if (slot[0] == UFT_FAT_DIRENT_FREE) { out->is_deleted = true; }
    if ((slot[0x0B] & UFT_FAT_ATTR_LFN_MASK) == UFT_FAT_ATTR_LFN) return 2;

    char base[9], ext[4];
    trim_padded((const char *)slot,    8, base);
    trim_padded((const char *)slot + 8, 3, ext);
    if (ext[0])
        snprintf(out->sfn, sizeof(out->sfn), "%s.%s", base, ext);
    else
        snprintf(out->sfn, sizeof(out->sfn), "%s", base);
    /* If first byte was 0x05 (Kanji protection), put 0xE5 back. */
    if (slot[0] == UFT_FAT_DIRENT_KANJI) out->sfn[0] = (char)0xE5;
    strncpy(out->lfn, out->sfn, sizeof(out->lfn) - 1);

    out->attributes = slot[0x0B];
    uint16_t hi = le16(slot + 0x14);
    uint16_t lo = le16(slot + 0x1A);
    out->cluster = ((uint32_t)hi << 16) | lo;
    out->size = le32(slot + 0x1C);

    uint16_t mtime = le16(slot + 0x16);
    uint16_t mdate = le16(slot + 0x18);
    uint16_t ctime = le16(slot + 0x0E);
    uint16_t cdate = le16(slot + 0x10);
    out->modify_time = uft_fat_to_unix_time(mtime, mdate);
    out->create_time = uft_fat_to_unix_time(ctime, cdate);
    out->access_time = out->modify_time;

    out->is_directory    = (out->attributes & UFT_FAT_ATTR_DIRECTORY) != 0;
    out->is_volume_label = (out->attributes & UFT_FAT_ATTR_VOLUME_ID) != 0;
    return 0;
}

int uft_fat_read_dir(const uft_fat_ctx_t *ctx, uint32_t cluster,
                      uft_fat_dir_t *dir) {
    if (!ctx || !dir) return UFT_FAT_ERR_INVALID;
    uft_fat_dir_init(dir);
    dir->cluster = cluster;

    if (cluster == 0) {
        /* Root directory — fixed location, not in cluster space. */
        for (uint32_t s = 0; s < ctx->vol.root_dir_sectors; s++) {
            uint8_t sec[UFT_FAT_SECTOR_SIZE];
            if (uft_fat_read_root_sector(ctx, s, sec) != UFT_FAT_OK) break;
            for (size_t o = 0; o < UFT_FAT_SECTOR_SIZE; o += 32) {
                uft_fat_entry_t e;
                int rc = parse_sfn_slot(sec + o, &e);
                if (rc == 1) return UFT_FAT_OK;
                if (rc == 2) continue;
                if (dir_append(dir, &e) != UFT_FAT_OK) {
                    uft_fat_dir_free(dir); return UFT_FAT_ERR_NOMEM;
                }
            }
        }
        return UFT_FAT_OK;
    }

    /* Subdirectory — walk cluster chain. */
    size_t csize = uft_fat_cluster_size(ctx);
    uint8_t *buf = (uint8_t *)malloc(csize);
    if (!buf) return UFT_FAT_ERR_NOMEM;
    uint32_t c = cluster;
    int guard = 65536;
    while (guard-- > 0 && c >= UFT_FAT_FIRST_CLUSTER && c <= ctx->vol.last_cluster) {
        if (uft_fat_read_cluster(ctx, c, buf) != UFT_FAT_OK) break;
        for (size_t o = 0; o < csize; o += 32) {
            uft_fat_entry_t e;
            int rc = parse_sfn_slot(buf + o, &e);
            if (rc == 1) { free(buf); return UFT_FAT_OK; }
            if (rc == 2) continue;
            if (dir_append(dir, &e) != UFT_FAT_OK) {
                free(buf); uft_fat_dir_free(dir); return UFT_FAT_ERR_NOMEM;
            }
        }
        if (uft_fat_cluster_is_eof(ctx, c)) break;
        int32_t nxt = uft_fat_get_entry(ctx, c);
        if (nxt < 0) break;
        c = (uint32_t)nxt;
    }
    free(buf);
    return UFT_FAT_OK;
}

static void split_path_component(const char *path, char *comp, size_t comp_size,
                                   const char **rest) {
    while (*path == '/' || *path == '\\') path++;
    size_t n = 0;
    while (*path && *path != '/' && *path != '\\' && n < comp_size - 1) {
        comp[n++] = *path++;
    }
    comp[n] = '\0';
    *rest = path;
}

static bool name_match(const char *a, const char *b) {
    while (*a && *b) {
        int ca = toupper((unsigned char)*a);
        int cb = toupper((unsigned char)*b);
        if (ca != cb) return false;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

int uft_fat_find_entry(const uft_fat_ctx_t *ctx, uint32_t cluster,
                        const char *name, uft_fat_entry_t *out) {
    if (!ctx || !name || !out) return UFT_FAT_ERR_INVALID;
    uft_fat_dir_t d;
    if (uft_fat_read_dir(ctx, cluster, &d) != UFT_FAT_OK) return UFT_FAT_ERR_IO;
    int rc = UFT_FAT_ERR_NOTFOUND;
    for (size_t i = 0; i < d.count; i++) {
        if (d.entries[i].is_deleted) continue;
        if (name_match(d.entries[i].sfn, name) ||
            name_match(d.entries[i].lfn, name)) {
            *out = d.entries[i];
            rc = UFT_FAT_OK;
            break;
        }
    }
    uft_fat_dir_free(&d);
    return rc;
}

int uft_fat_find_path(const uft_fat_ctx_t *ctx, const char *path,
                       uft_fat_entry_t *out) {
    if (!ctx || !path || !out) return UFT_FAT_ERR_INVALID;
    const char *p = path;
    while (*p == '/' || *p == '\\') p++;
    if (*p == '\0') return UFT_FAT_ERR_INVALID;

    uint32_t cur = 0;   /* root */
    char component[UFT_FAT_MAX_SFN + 1];

    while (*p) {
        const char *rest;
        split_path_component(p, component, sizeof(component), &rest);
        p = rest;
        while (*p == '/' || *p == '\\') p++;

        uft_fat_entry_t e;
        int rc = uft_fat_find_entry(ctx, cur, component, &e);
        if (rc != UFT_FAT_OK) return rc;

        if (*p == '\0') { *out = e; return UFT_FAT_OK; }
        if (!e.is_directory) return UFT_FAT_ERR_NOTFOUND;
        cur = e.cluster;
    }
    return UFT_FAT_ERR_INVALID;
}

int uft_fat_read_dir_path(const uft_fat_ctx_t *ctx, const char *path,
                           uft_fat_dir_t *dir) {
    if (!ctx || !path || !dir) return UFT_FAT_ERR_INVALID;
    const char *p = path;
    while (*p == '/' || *p == '\\') p++;
    if (*p == '\0') return uft_fat_read_dir(ctx, 0, dir);

    uft_fat_entry_t e;
    if (uft_fat_find_path(ctx, path, &e) != UFT_FAT_OK) return UFT_FAT_ERR_NOTFOUND;
    if (!e.is_directory) return UFT_FAT_ERR_INVALID;
    return uft_fat_read_dir(ctx, e.cluster, dir);
}

int uft_fat_foreach_entry(const uft_fat_ctx_t *ctx, uint32_t cluster,
                           uft_fat_dir_callback_t cb, void *user) {
    if (!ctx || !cb) return UFT_FAT_ERR_INVALID;
    uft_fat_dir_t d;
    if (uft_fat_read_dir(ctx, cluster, &d) != UFT_FAT_OK) return UFT_FAT_ERR_IO;
    int rc = UFT_FAT_OK;
    for (size_t i = 0; i < d.count; i++) {
        rc = cb(&d.entries[i], user);
        if (rc != 0) break;
    }
    uft_fat_dir_free(&d);
    return rc;
}

/* ==========================================================================
 * Cluster chain + file extraction
 * ==========================================================================*/

int uft_fat_get_chain(const uft_fat_ctx_t *ctx, uint32_t start,
                       uft_fat_chain_t *chain) {
    if (!ctx || !chain) return UFT_FAT_ERR_INVALID;
    uft_fat_chain_init(chain);
    if (start < UFT_FAT_FIRST_CLUSTER || start > ctx->vol.last_cluster)
        return UFT_FAT_ERR_BADCHAIN;

    size_t cap = 64;
    chain->clusters = (uint32_t *)malloc(cap * sizeof(uint32_t));
    if (!chain->clusters) return UFT_FAT_ERR_NOMEM;
    chain->capacity = cap;

    uint32_t c = start;
    int guard = 65536;
    while (guard-- > 0) {
        if (uft_fat_cluster_is_bad(ctx, c)) { chain->has_bad = true; break; }
        if (chain->count >= chain->capacity) {
            size_t n = chain->capacity * 2;
            uint32_t *tmp = (uint32_t *)realloc(chain->clusters,
                                                 n * sizeof(uint32_t));
            if (!tmp) { uft_fat_chain_free(chain); return UFT_FAT_ERR_NOMEM; }
            chain->clusters = tmp; chain->capacity = n;
        }
        chain->clusters[chain->count++] = c;
        if (uft_fat_cluster_is_eof(ctx, c)) { chain->complete = true; break; }
        int32_t n = uft_fat_get_entry(ctx, c);
        if (n < 0) break;
        if ((uint32_t)n < UFT_FAT_FIRST_CLUSTER || (uint32_t)n > ctx->vol.last_cluster)
            break;
        /* detect simple loops: cluster reused within first few entries */
        for (size_t i = 0; i < chain->count && i < 16; i++) {
            if (chain->clusters[i] == (uint32_t)n) { chain->has_loops = true; guard = 0; break; }
        }
        c = (uint32_t)n;
    }
    return UFT_FAT_OK;
}

int uft_fat_free_chain(uft_fat_ctx_t *ctx, uint32_t start) {
    (void)ctx; (void)start; return UFT_FAT_ERR_READONLY;
}

uint8_t *uft_fat_extract(const uft_fat_ctx_t *ctx, const uft_fat_entry_t *e,
                          uint8_t *buffer, size_t *size) {
    if (!ctx || !e || !size) return NULL;

    uft_fat_chain_t chain;
    if (uft_fat_get_chain(ctx, e->cluster, &chain) != UFT_FAT_OK) return NULL;

    size_t need = e->size;
    uint8_t *out = buffer;
    if (!out) { out = (uint8_t *)malloc(need ? need : 1); if (!out) {
        uft_fat_chain_free(&chain); return NULL;
    } } else if (*size < need) {
        *size = need;
        uft_fat_chain_free(&chain);
        return NULL;
    }

    size_t csize = uft_fat_cluster_size(ctx);
    size_t written = 0;
    for (size_t i = 0; i < chain.count && written < need; i++) {
        uint8_t tmp[8192];
        if (csize > sizeof(tmp)) break;
        if (uft_fat_read_cluster(ctx, chain.clusters[i], tmp) != UFT_FAT_OK) break;
        size_t chunk = csize;
        if (written + chunk > need) chunk = need - written;
        memcpy(out + written, tmp, chunk);
        written += chunk;
    }
    uft_fat_chain_free(&chain);
    *size = written;
    return out;
}

uint8_t *uft_fat_extract_path(const uft_fat_ctx_t *ctx, const char *path,
                               uint8_t *buffer, size_t *size) {
    if (!ctx || !path || !size) return NULL;
    uft_fat_entry_t e;
    if (uft_fat_find_path(ctx, path, &e) != UFT_FAT_OK) return NULL;
    return uft_fat_extract(ctx, &e, buffer, size);
}

int uft_fat_extract_to_file(const uft_fat_ctx_t *ctx, const char *path,
                             const char *dest_path) {
    if (!ctx || !path || !dest_path) return UFT_FAT_ERR_INVALID;
    size_t sz = 0;
    uint8_t *buf = uft_fat_extract_path(ctx, path, NULL, &sz);
    if (!buf) return UFT_FAT_ERR_NOTFOUND;
    FILE *f = fopen(dest_path, "wb");
    if (!f) { free(buf); return UFT_FAT_ERR_IO; }
    size_t w = fwrite(buf, 1, sz, f);
    fclose(f);
    free(buf);
    return (w == sz) ? UFT_FAT_OK : UFT_FAT_ERR_IO;
}

/* ==========================================================================
 * Write path — all return READONLY
 * ========================================================================== */

int uft_fat_inject(uft_fat_ctx_t *ctx, uint32_t dir_cluster, const char *name,
                    const uint8_t *data, size_t size) {
    (void)ctx; (void)dir_cluster; (void)name; (void)data; (void)size;
    return UFT_FAT_ERR_READONLY;
}
int uft_fat_inject_path(uft_fat_ctx_t *ctx, const char *path,
                         const uint8_t *data, size_t size) {
    (void)ctx; (void)path; (void)data; (void)size; return UFT_FAT_ERR_READONLY;
}
int uft_fat_inject_from_file(uft_fat_ctx_t *ctx, const char *path,
                              const char *src_path) {
    (void)ctx; (void)path; (void)src_path; return UFT_FAT_ERR_READONLY;
}
int uft_fat_delete(uft_fat_ctx_t *ctx, const char *path) {
    (void)ctx; (void)path; return UFT_FAT_ERR_READONLY;
}
int uft_fat_rename(uft_fat_ctx_t *ctx, const char *old_path, const char *new_path) {
    (void)ctx; (void)old_path; (void)new_path; return UFT_FAT_ERR_READONLY;
}
int uft_fat_mkdir(uft_fat_ctx_t *ctx, const char *path) {
    (void)ctx; (void)path; return UFT_FAT_ERR_READONLY;
}
int uft_fat_rmdir(uft_fat_ctx_t *ctx, const char *path) {
    (void)ctx; (void)path; return UFT_FAT_ERR_READONLY;
}
int uft_fat_set_attr(uft_fat_ctx_t *ctx, const char *path, uint8_t attr) {
    (void)ctx; (void)path; (void)attr; return UFT_FAT_ERR_READONLY;
}
int uft_fat_set_time(uft_fat_ctx_t *ctx, const char *path, time_t mtime) {
    (void)ctx; (void)path; (void)mtime; return UFT_FAT_ERR_READONLY;
}

/* ==========================================================================
 * Misc utilities
 * ========================================================================== */

const char *uft_fat_strerror(int err) {
    switch (err) {
        case UFT_FAT_OK:            return "OK";
        case UFT_FAT_ERR_INVALID:   return "invalid argument";
        case UFT_FAT_ERR_NOMEM:     return "out of memory";
        case UFT_FAT_ERR_IO:        return "I/O error";
        case UFT_FAT_ERR_NOTFOUND:  return "not found";
        case UFT_FAT_ERR_EXISTS:    return "exists";
        case UFT_FAT_ERR_FULL:      return "full";
        case UFT_FAT_ERR_NOTEMPTY:  return "not empty";
        case UFT_FAT_ERR_READONLY:  return "read-only";
        case UFT_FAT_ERR_BADCHAIN:  return "bad cluster chain";
        case UFT_FAT_ERR_TOOLONG:   return "name too long";
        case UFT_FAT_ERR_BADNAME:   return "invalid name";
        default:                    return "unknown error";
    }
}

char *uft_fat_attr_to_string(uint8_t attr, char *buffer) {
    if (!buffer) return buffer;
    buffer[0] = (attr & UFT_FAT_ATTR_READONLY)  ? 'R' : '-';
    buffer[1] = (attr & UFT_FAT_ATTR_HIDDEN)    ? 'H' : '-';
    buffer[2] = (attr & UFT_FAT_ATTR_SYSTEM)    ? 'S' : '-';
    buffer[3] = (attr & UFT_FAT_ATTR_VOLUME_ID) ? 'V' : '-';
    buffer[4] = (attr & UFT_FAT_ATTR_DIRECTORY) ? 'D' : '-';
    buffer[5] = (attr & UFT_FAT_ATTR_ARCHIVE)   ? 'A' : '-';
    buffer[6] = '\0';
    return buffer;
}
