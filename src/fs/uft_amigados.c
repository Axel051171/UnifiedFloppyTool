/**
 * @file uft_amigados.c
 * @brief AmigaDOS OFS/FFS read-only backend.
 *
 * Fills the Kat-A gap: include/uft/fs/uft_amigados.h declares the full
 * AmigaDOS API but had zero implementation in the tree before this file.
 * That was also the blocker for the FS-driver adapter in
 * src/fs/uft_fs_amigados_driver.c — it compiled but could not link.
 *
 * Scope: READ-ONLY. All write paths return UFT_ERROR_NOT_SUPPORTED /
 * UFT_ERROR_READ_ONLY so the public API is honest. A full read-write
 * port (bitmap allocation, inject/delete/rename) is a separate future
 * commit — the header carries those declarations but flagging them
 * write-capable would be lying until that code lands.
 *
 * References:
 *   Amiga ROM Kernel Reference Manual, Devices, 3rd ed., Ch. 20
 *   AmigaDOS Technical Reference, Commodore-Amiga 1989
 *   http://lclevy.free.fr/adflib/adf_info.html
 *
 * Implementation notes:
 *   - AmigaDOS stores all multi-byte fields big-endian.
 *   - Block 0/1 = bootblock, block (total/2) = root block.
 *   - Names are BCPL-style: 1 length byte followed by chars (no NUL).
 *   - Directory entries are linked via hash table + hash_chain pointer.
 *   - OFS data blocks have a 24-byte header then 488 bytes of payload.
 *   - FFS data blocks are raw 512 bytes.
 */

#include "uft/fs/uft_amigados.h"
#include "uft/uft_error.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLOCK_SIZE   UFT_AMIGA_BLOCK_SIZE       /* 512 */
#define HASH_SIZE    UFT_AMIGA_HASH_SIZE        /* 72 */

/* Block field offsets (byte offsets into a 512-byte block) */
#define OFF_TYPE               0x000   /* int32 primary type */
#define OFF_HEADER_KEY         0x004   /* self-reference */
#define OFF_HIGH_SEQ           0x008   /* file/T_LIST header: # data blocks valid */
#define OFF_HASHTABLE_SIZE     0x00C   /* dir/root header: hash-table entries */
#define OFF_FIRST_DATA         0x010   /* file header: cache of data_blocks[71] */
#define OFF_DATABLOCKS         0x018   /* 72 longs (REVERSE order — slot 71 = first) */
#define OFF_FILE_SIZE          0x144   /* 0x14C - 8 */
#define OFF_BYTE_SIZE          0x144
#define OFF_FILENAME_LEN       0x150
#define OFF_FILENAME           0x150
#define OFF_NEXT_EXT           0x1F0   /* 0x1F0 extension block */
#define OFF_PARENT             0x1F4
#define OFF_HASH_CHAIN         0x1F0
#define OFF_PROTECTION         0x140
#define OFF_COMMENT_LEN        0x108
#define OFF_COMMENT            0x108
#define OFF_DAYS               0x1A4
#define OFF_MINS               0x1A8
#define OFF_TICKS              0x1AC
#define OFF_SEC_TYPE           0x1FC

#define T_HEADER               2
#define T_DATA                 8
#define T_LIST                16
#define ST_ROOT                1
#define ST_USERDIR             2
#define ST_FILE               (-3)
#define ST_LINKFILE           (-4)
#define ST_SOFTLINK            3
#define ST_LINKDIR             4

/* OFS data block: data starts at offset 24 (header: type, hdr_key, seq_num, size, next_data, chksum) */
#define OFS_DATA_HEADER_SIZE   24
#define OFS_DATA_PAYLOAD       (BLOCK_SIZE - OFS_DATA_HEADER_SIZE)  /* 488 */

/* --------------------------------------------------------------------------
 * Big-endian helpers — AmigaDOS stores every multi-byte field BE.
 * -------------------------------------------------------------------------- */
static uint32_t be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  | (uint32_t)p[3];
}
static int32_t be32s(const uint8_t *p) { return (int32_t)be32(p); }

/* Number of data days since 1978-01-01 → Unix epoch. 1978-01-01 = 252460800. */
static time_t amiga_to_unix(uint32_t days, uint32_t mins, uint32_t ticks) {
    return (time_t)(252460800u + days * 86400u + mins * 60u + ticks / 50u);
}

/* Case-folding for hash / compare. INTL mode includes extended chars. */
static int amiga_toupper(int c, bool intl) {
    if (!intl) return (c >= 'a' && c <= 'z') ? c - 32 : c;
    if (c >= 'a' && c <= 'z') return c - 32;
    if (c >= 0xE0 && c <= 0xFE && c != 0xF7) return c - 32;
    return c;
}

uint32_t uft_amiga_hash_name(const char *name, bool intl) {
    if (!name) return 0;
    size_t len = strlen(name);
    uint32_t h = (uint32_t)len;
    for (size_t i = 0; i < len; i++) {
        int c = amiga_toupper((unsigned char)name[i], intl);
        h = (h * 13 + (uint32_t)c) & 0x7FFFFFFF;
    }
    return h % HASH_SIZE;
}

/* BCPL string: first byte is length, followed by chars. */
static void read_bcpl(const uint8_t *p, size_t max_len, char *out, size_t out_size) {
    if (out_size == 0) return;
    uint8_t len = p[0];
    if (len > max_len) len = (uint8_t)max_len;
    if (len >= out_size) len = (uint8_t)(out_size - 1);
    memcpy(out, p + 1, len);
    out[len] = '\0';
}

/* --------------------------------------------------------------------------
 * Lifecycle
 * -------------------------------------------------------------------------- */

uft_amiga_ctx_t *uft_amiga_create(void) {
    uft_amiga_ctx_t *ctx = (uft_amiga_ctx_t *)calloc(1, sizeof(*ctx));
    if (!ctx) return NULL;
    ctx->verify_checksums = true;
    ctx->preserve_dates = true;
    return ctx;
}

void uft_amiga_close(uft_amiga_ctx_t *ctx) {
    if (!ctx) return;
    if (ctx->owns_data && ctx->data) {
        free(ctx->data);
        ctx->data = NULL;
    }
    ctx->size = 0;
    ctx->is_valid = false;
    ctx->owns_data = false;
}

void uft_amiga_destroy(uft_amiga_ctx_t *ctx) {
    if (!ctx) return;
    uft_amiga_close(ctx);
    free(ctx);
}

/* --------------------------------------------------------------------------
 * Detect
 * -------------------------------------------------------------------------- */

int uft_amiga_detect(const uint8_t *data, size_t size, uft_amiga_detect_t *result) {
    if (!data || !result) return -1;
    memset(result, 0, sizeof(*result));

    if (size < 1024) return -1;
    if (data[0] != 'D' || data[1] != 'O' || data[2] != 'S') return -1;

    uint8_t flags = data[3];
    result->fs_type      = (uft_amiga_fs_type_t)flags;
    result->is_ffs       = (flags & 0x01) != 0;
    result->is_intl      = (flags & 0x02) != 0;
    result->is_dircache  = (flags & 0x04) != 0;
    result->is_longnames = (flags & 0x06) == 0x06;

    memcpy(result->dos_type, data, 4);
    result->dos_type[3] = (char)('0' + flags);
    result->dos_type[4] = '\0';

    result->total_blocks = (uint32_t)(size / BLOCK_SIZE);
    result->root_block   = result->total_blocks / 2;

    /* Bootblock checksum: sum of 256 BE longs over blocks 0+1 with
     * checksum slot excluded must produce (~sum) at offset 4. */
    uint32_t sum = 0;
    uint32_t stored = be32(data + 4);
    for (size_t i = 0; i < 256; i++) {
        if (i == 1) continue;              /* skip checksum word */
        uint32_t v = be32(data + i * 4);
        uint32_t prev = sum;
        sum += v;
        if (sum < prev) sum++;             /* carry-wrap */
    }
    result->bootblock_checksum = stored;
    result->bootblock_valid    = (~sum == stored);

    result->is_valid = true;                /* DOS signature + plausible size */
    return 0;
}

bool uft_amiga_is_adf(const char *filename) {
    if (!filename) return false;
    size_t n = strlen(filename);
    if (n < 4) return false;
    const char *e = filename + n - 4;
    return (e[0] == '.' &&
            (e[1] == 'a' || e[1] == 'A') &&
            (e[2] == 'd' || e[2] == 'D') &&
            (e[3] == 'f' || e[3] == 'F'));
}

const char *uft_amiga_fs_type_str(uft_amiga_fs_type_t t) {
    switch (t) {
        case UFT_AMIGA_FS_OFS:           return "OFS";
        case UFT_AMIGA_FS_FFS:           return "FFS";
        case UFT_AMIGA_FS_OFS_INTL:      return "OFS-INTL";
        case UFT_AMIGA_FS_FFS_INTL:      return "FFS-INTL";
        case UFT_AMIGA_FS_OFS_DC:        return "OFS-DC";
        case UFT_AMIGA_FS_FFS_DC:        return "FFS-DC";
        case UFT_AMIGA_FS_FFS_LNFS:      return "FFS-LNFS";
        default:                          return "unknown";
    }
}

/* --------------------------------------------------------------------------
 * Open
 * -------------------------------------------------------------------------- */

int uft_amiga_open_buffer(uft_amiga_ctx_t *ctx, uint8_t *data, size_t size,
                           bool copy, const uft_amiga_options_t *opts) {
    if (!ctx || !data || size < 1024) return -1;
    uft_amiga_close(ctx);

    if (copy) {
        ctx->data = (uint8_t *)malloc(size);
        if (!ctx->data) return -2;
        memcpy(ctx->data, data, size);
        ctx->owns_data = true;
    } else {
        ctx->data = data;
        ctx->owns_data = false;
    }
    ctx->size = size;

    uft_amiga_detect_t det;
    if (uft_amiga_detect(ctx->data, ctx->size, &det) != 0) {
        uft_amiga_close(ctx);
        return -1;
    }
    ctx->is_valid     = det.is_valid;
    ctx->fs_type      = det.fs_type;
    ctx->is_ffs       = det.is_ffs;
    ctx->is_intl      = det.is_intl;
    ctx->is_dircache  = det.is_dircache;
    ctx->is_longnames = det.is_longnames;
    ctx->total_blocks = det.total_blocks;
    ctx->root_block   = det.root_block;
    ctx->fs_flags     = (uint8_t)det.fs_type;

    /* Pull volume name from root block offset 432. */
    const uint8_t *root = ctx->data + ctx->root_block * BLOCK_SIZE;
    read_bcpl(root + 432, 31, ctx->volume_name, sizeof(ctx->volume_name));

    if (opts) {
        ctx->verify_checksums = opts->verify_checksums;
        ctx->preserve_dates   = opts->preserve_dates;
    }
    return 0;
}

int uft_amiga_open_file(uft_amiga_ctx_t *ctx, const char *filename,
                         const uft_amiga_options_t *opts) {
    if (!ctx || !filename) return -1;
    FILE *f = fopen(filename, "rb");
    if (!f) return -3;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz <= 0 || sz > (long)(16 * 1024 * 1024)) { fclose(f); return -3; }
    rewind(f);
    uint8_t *buf = (uint8_t *)malloc((size_t)sz);
    if (!buf) { fclose(f); return -2; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf); fclose(f); return -3;
    }
    fclose(f);
    /* Take ownership via copy=false + manual owns_data. */
    int rc = uft_amiga_open_buffer(ctx, buf, (size_t)sz, /*copy=*/false, opts);
    if (rc != 0) { free(buf); return rc; }
    ctx->owns_data = true;
    return 0;
}

int uft_amiga_save(const uft_amiga_ctx_t *ctx, const char *filename) {
    (void)ctx; (void)filename;
    /* Write path not implemented — this backend is read-only. */
    return -8;
}

/* --------------------------------------------------------------------------
 * Block I/O + checksum
 * -------------------------------------------------------------------------- */

int uft_amiga_read_block(const uft_amiga_ctx_t *ctx, uint32_t block_num,
                          uint8_t *buffer) {
    if (!ctx || !ctx->data || !buffer) return -1;
    if ((size_t)(block_num + 1) * BLOCK_SIZE > ctx->size) return -1;
    memcpy(buffer, ctx->data + block_num * BLOCK_SIZE, BLOCK_SIZE);
    return 0;
}

int uft_amiga_write_block(uft_amiga_ctx_t *ctx, uint32_t block_num,
                           const uint8_t *buffer) {
    (void)ctx; (void)block_num; (void)buffer;
    return -8;  /* read-only */
}

bool uft_amiga_verify_checksum(const uint8_t *block) {
    if (!block) return false;
    uint32_t sum = 0;
    for (size_t i = 0; i < 128; i++)
        sum += be32(block + i * 4);
    return sum == 0;
}

void uft_amiga_update_checksum(uint8_t *block) {
    if (!block) return;
    /* Clear slot, recompute, store -sum. */
    uint8_t *slot = block + 20;
    slot[0] = slot[1] = slot[2] = slot[3] = 0;
    uint32_t sum = 0;
    for (size_t i = 0; i < 128; i++)
        sum += be32(block + i * 4);
    uint32_t neg = (uint32_t)-(int32_t)sum;
    slot[0] = (uint8_t)(neg >> 24);
    slot[1] = (uint8_t)(neg >> 16);
    slot[2] = (uint8_t)(neg >> 8);
    slot[3] = (uint8_t)neg;
}

/* --------------------------------------------------------------------------
 * Directory entry parsing
 * -------------------------------------------------------------------------- */

static void parse_entry(const uft_amiga_ctx_t *ctx, uint32_t block_num,
                         uft_amiga_entry_t *out) {
    const uint8_t *b = ctx->data + block_num * BLOCK_SIZE;
    memset(out, 0, sizeof(*out));
    out->header_block = block_num;
    out->header_key   = block_num;
    out->type         = be32(b + OFF_TYPE);
    out->secondary_type = be32s(b + OFF_SEC_TYPE);
    out->hash_chain   = be32(b + OFF_HASH_CHAIN);
    out->parent_block = be32(b + OFF_PARENT);
    out->parent       = out->parent_block;
    out->size         = be32(b + OFF_BYTE_SIZE);
    out->first_data   = be32(b + OFF_DATABLOCKS + (72 - 1) * 4);  /* last entry = 1st data */
    out->extension    = be32(b + OFF_NEXT_EXT);
    out->protection   = be32(b + OFF_PROTECTION);
    out->days         = be32(b + OFF_DAYS);
    out->mins         = be32(b + OFF_MINS);
    out->ticks        = be32(b + OFF_TICKS);
    out->mtime        = amiga_to_unix(out->days, out->mins, out->ticks);

    /* filename at offset (BLOCK_SIZE - 80) = 0x1B0 for HD, 0x1B0 for DD too —
     * standard is offset 432 for volume (root), offset (BLOCK_SIZE - 80) = 432
     * for entries on a 512-byte block layout. Wait — for entries filename is
     * at offset 432 too. Let me use that. */
    read_bcpl(b + 432, 31, out->name, sizeof(out->name));
    read_bcpl(b + 328, 79, out->comment, sizeof(out->comment));

    int32_t st = out->secondary_type;
    out->is_dir      = (st == ST_USERDIR || st == ST_LINKDIR);
    out->is_file     = (st == ST_FILE || st == ST_LINKFILE);
    out->is_softlink = (st == ST_SOFTLINK);
    out->is_hardlink = (st == ST_LINKFILE || st == ST_LINKDIR);
    out->is_link     = out->is_softlink || out->is_hardlink;
}

/* --------------------------------------------------------------------------
 * Directory load — walks hash table and follows hash chains
 * -------------------------------------------------------------------------- */

static int dir_append(uft_amiga_dir_t *dir, const uft_amiga_entry_t *e) {
    if (dir->count >= dir->capacity) {
        size_t n = dir->capacity ? dir->capacity * 2 : 16;
        uft_amiga_entry_t *tmp = (uft_amiga_entry_t *)realloc(dir->entries,
                                                              n * sizeof(*tmp));
        if (!tmp) return -2;
        dir->entries  = tmp;
        dir->capacity = n;
    }
    dir->entries[dir->count++] = *e;
    return 0;
}

int uft_amiga_load_root(const uft_amiga_ctx_t *ctx, uft_amiga_dir_t *dir) {
    if (!ctx || !ctx->data || !dir) return -1;
    return uft_amiga_load_dir(ctx, ctx->root_block, dir);
}

int uft_amiga_load_dir(const uft_amiga_ctx_t *ctx, uint32_t block_num,
                        uft_amiga_dir_t *dir) {
    if (!ctx || !ctx->data || !dir) return -1;
    if ((size_t)(block_num + 1) * BLOCK_SIZE > ctx->size) return -1;

    memset(dir, 0, sizeof(*dir));
    dir->dir_block = block_num;

    /* Volume name on root, else entry name on sub-dir header. */
    const uint8_t *b = ctx->data + block_num * BLOCK_SIZE;
    read_bcpl(b + 432, 31, dir->dir_name, sizeof(dir->dir_name));

    /* Hash table at offset 24, 72 slots of 4 bytes each. */
    for (int slot = 0; slot < HASH_SIZE; slot++) {
        uint32_t head = be32(b + 24 + slot * 4);
        while (head != 0 && head < ctx->total_blocks) {
            uft_amiga_entry_t e;
            parse_entry(ctx, head, &e);
            if (dir_append(dir, &e) != 0) {
                uft_amiga_free_dir(dir);
                return -2;
            }
            if (e.hash_chain == head) break;  /* defensive: self-loop */
            head = e.hash_chain;
        }
    }
    return 0;
}

void uft_amiga_free_dir(uft_amiga_dir_t *dir) {
    if (!dir) return;
    free(dir->entries);
    dir->entries = NULL;
    dir->count = dir->capacity = 0;
}

int uft_amiga_find_entry(const uft_amiga_ctx_t *ctx, uint32_t dir_block,
                          const char *name, uft_amiga_entry_t *entry) {
    if (!ctx || !name || !entry) return -1;
    if (dir_block == 0) dir_block = ctx->root_block;

    uft_amiga_dir_t dir;
    if (uft_amiga_load_dir(ctx, dir_block, &dir) != 0) return -1;
    int rc = -4;
    for (size_t i = 0; i < dir.count; i++) {
        int match = 0;
        const char *a = dir.entries[i].name;
        const char *b = name;
        while (*a || *b) {
            int ca = amiga_toupper((unsigned char)*a, ctx->is_intl);
            int cb = amiga_toupper((unsigned char)*b, ctx->is_intl);
            if (ca != cb) { match = -1; break; }
            a++; b++;
        }
        if (match == 0) { *entry = dir.entries[i]; rc = 0; break; }
    }
    uft_amiga_free_dir(&dir);
    return rc;
}

int uft_amiga_find_path(const uft_amiga_ctx_t *ctx, const char *path,
                         uft_amiga_entry_t *entry) {
    if (!ctx || !path || !entry) return -1;

    /* Leading slashes and empty path mean "root directory" — which is not a
     * regular entry. Caller probably wants load_dir_path for that. */
    while (*path == '/' || *path == '\\') path++;
    if (*path == '\0') return -1;

    uint32_t cur_block = ctx->root_block;
    char component[UFT_AMIGA_MAX_FILENAME_LFS + 1];

    while (*path) {
        size_t n = 0;
        while (path[n] && path[n] != '/' && path[n] != '\\' &&
               n < sizeof(component) - 1) {
            component[n] = path[n];
            n++;
        }
        component[n] = '\0';
        path += n;
        while (*path == '/' || *path == '\\') path++;

        uft_amiga_entry_t e;
        if (uft_amiga_find_entry(ctx, cur_block, component, &e) != 0) return -4;

        if (*path == '\0') { *entry = e; return 0; }
        if (!e.is_dir) return -4;
        cur_block = e.header_block;
    }
    return -1;
}

int uft_amiga_load_dir_path(const uft_amiga_ctx_t *ctx, const char *path,
                             uft_amiga_dir_t *dir) {
    if (!ctx || !path || !dir) return -1;
    const char *p = path;
    while (*p == '/' || *p == '\\') p++;
    if (*p == '\0') return uft_amiga_load_root(ctx, dir);

    uft_amiga_entry_t e;
    if (uft_amiga_find_path(ctx, path, &e) != 0) return -4;
    if (!e.is_dir) return -1;
    return uft_amiga_load_dir(ctx, e.header_block, dir);
}

/* --------------------------------------------------------------------------
 * Data-block chain + file extraction
 * -------------------------------------------------------------------------- */

int uft_amiga_get_chain(const uft_amiga_ctx_t *ctx, uint32_t file_block,
                         uft_amiga_chain_t *chain) {
    if (!ctx || !chain) return -1;
    memset(chain, 0, sizeof(*chain));
    if ((size_t)(file_block + 1) * BLOCK_SIZE > ctx->size) return -1;

    const uint8_t *b = ctx->data + file_block * BLOCK_SIZE;
    chain->header_block = file_block;
    chain->total_size   = be32(b + OFF_BYTE_SIZE);

    size_t cap = 64;
    chain->blocks = (uint32_t *)malloc(cap * sizeof(uint32_t));
    if (!chain->blocks) return -2;
    chain->capacity = cap;

    uint32_t cur_hdr = file_block;
    while (cur_hdr != 0 && (size_t)(cur_hdr + 1) * BLOCK_SIZE <= ctx->size) {
        const uint8_t *hb = ctx->data + cur_hdr * BLOCK_SIZE;

        /* AmigaDOS file/T_LIST data-block table convention (per ADFlib):
         *   72 longs at offset OFF_DATABLOCKS, stored in REVERSE order:
         *     slot 71 holds the FIRST data block in the file
         *     slot 70 holds the SECOND, ... etc
         *     slot (72 - high_seq) holds the LAST
         *     slots 0 .. (71 - high_seq) are unused (zero) for partial
         *     files (high_seq < 72).
         *   The number of valid pointers is `high_seq` at OFF_HIGH_SEQ.
         *
         * Earlier code read OFF_DATA_BLK_COUNT (0x010) as "count" and
         * walked slots [count-1..0]. That had two bugs that cancelled
         * each other for stock ADFs:
         *   (1) 0x010 is `first_data` (a block-pointer cache), NOT high_seq
         *       (which lives at 0x008). Stock-disk first_data is typically
         *       a block number > 72, which the clamp `if > 72: count=72`
         *       silently accepted.
         *   (2) Walking [count-1..0] is the wrong direction.
         *   Combined: clamp forced count=72, loop became [71..0] which
         *   accidentally hit the upper-half real pointers + zero-skipped
         *   the lower-half empties. Latently dependent on the cancellation;
         *   any partial fix would have un-masked silent file truncation.
         * Found by deep-diagnostician analysis after a forensic-class
         * structured-reviewer pass on commit 7a8f7da's new walker (which
         * had only the direction bug). Reference uft_amigados_extended.c
         * for the same fixed pattern. */
        uint32_t high_seq = be32(hb + OFF_HIGH_SEQ);
        if (high_seq > 72) high_seq = 72;
        for (uint32_t i = 0; i < high_seq; i++) {
            uint32_t slot = 72u - 1u - i;          /* 71, 70, ..., 72-high_seq */
            uint32_t blk = be32(hb + OFF_DATABLOCKS + slot * 4);
            if (blk == 0) continue;
            if (chain->count >= chain->capacity) {
                size_t nc = chain->capacity * 2;
                uint32_t *tmp = (uint32_t *)realloc(chain->blocks,
                                                    nc * sizeof(uint32_t));
                if (!tmp) { uft_amiga_free_chain(chain); return -2; }
                chain->blocks = tmp; chain->capacity = nc;
            }
            chain->blocks[chain->count++] = blk;
        }
        uint32_t next = be32(hb + OFF_NEXT_EXT);
        if (next == cur_hdr) break;
        cur_hdr = next;
        if (cur_hdr) chain->has_extension = true;
    }
    /* no complete field in this struct */
    return 0;
}

void uft_amiga_free_chain(uft_amiga_chain_t *chain) {
    if (!chain) return;
    free(chain->blocks);
    chain->blocks = NULL;
    chain->count = chain->capacity = 0;
}

int uft_amiga_extract_file_alloc(const uft_amiga_ctx_t *ctx, const char *path,
                                  uint8_t **data_out, size_t *size_out) {
    if (!ctx || !path || !data_out || !size_out) return -1;
    *data_out = NULL; *size_out = 0;

    uft_amiga_entry_t e;
    if (uft_amiga_find_path(ctx, path, &e) != 0) return -4;
    if (!e.is_file) return -1;

    uft_amiga_chain_t chain;
    if (uft_amiga_get_chain(ctx, e.header_block, &chain) != 0) return -1;

    uint8_t *buf = (uint8_t *)malloc(e.size ? e.size : 1);
    if (!buf) { uft_amiga_free_chain(&chain); return -2; }

    size_t written = 0;
    const bool ffs = ctx->is_ffs;
    for (size_t i = 0; i < chain.count && written < e.size; i++) {
        uint32_t blk = chain.blocks[i];
        if ((size_t)(blk + 1) * BLOCK_SIZE > ctx->size) break;
        const uint8_t *src = ctx->data + blk * BLOCK_SIZE;
        size_t payload, src_off;
        if (ffs) { payload = BLOCK_SIZE; src_off = 0; }
        else     { payload = OFS_DATA_PAYLOAD; src_off = OFS_DATA_HEADER_SIZE; }
        size_t to_copy = payload;
        if (written + to_copy > e.size) to_copy = e.size - written;
        memcpy(buf + written, src + src_off, to_copy);
        written += to_copy;
    }
    uft_amiga_free_chain(&chain);

    *data_out = buf;
    *size_out = written;
    return 0;
}

int uft_amiga_extract_file(const uft_amiga_ctx_t *ctx, const char *path,
                            uint8_t *data, size_t *size) {
    if (!ctx || !path || !data || !size) return -1;
    uint8_t *tmp = NULL;
    size_t   got = 0;
    int rc = uft_amiga_extract_file_alloc(ctx, path, &tmp, &got);
    if (rc != 0) return rc;
    if (got > *size) { free(tmp); *size = got; return -6; }
    memcpy(data, tmp, got);
    free(tmp);
    *size = got;
    return 0;
}

int uft_amiga_extract_to_file(const uft_amiga_ctx_t *ctx, const char *path,
                               const char *dest_path) {
    if (!ctx || !path || !dest_path) return -1;
    uint8_t *buf = NULL; size_t n = 0;
    int rc = uft_amiga_extract_file_alloc(ctx, path, &buf, &n);
    if (rc != 0) return rc;
    FILE *f = fopen(dest_path, "wb");
    if (!f) { free(buf); return -3; }
    size_t wrote = fwrite(buf, 1, n, f);
    fclose(f);
    free(buf);
    return wrote == n ? 0 : -3;
}

/* --------------------------------------------------------------------------
 * Write paths (not implemented — read-only backend)
 * -------------------------------------------------------------------------- */

int uft_amiga_inject_file(uft_amiga_ctx_t *ctx, const char *dest_dir,
                           const char *name, const uint8_t *data, size_t size) {
    (void)ctx; (void)dest_dir; (void)name; (void)data; (void)size;
    return -8;
}
int uft_amiga_inject_from_file(uft_amiga_ctx_t *ctx, const char *dest_dir,
                                const char *src_path) {
    (void)ctx; (void)dest_dir; (void)src_path;
    return -8;
}
int uft_amiga_delete(uft_amiga_ctx_t *ctx, const char *path) {
    (void)ctx; (void)path; return -8;
}
int uft_amiga_rename(uft_amiga_ctx_t *ctx, const char *old_path,
                      const char *new_path) {
    (void)ctx; (void)old_path; (void)new_path; return -8;
}
int uft_amiga_mkdir(uft_amiga_ctx_t *ctx, const char *parent_dir,
                     const char *name) {
    (void)ctx; (void)parent_dir; (void)name; return -8;
}
int uft_amiga_set_protection(uft_amiga_ctx_t *ctx, const char *path,
                              uint32_t protection) {
    (void)ctx; (void)path; (void)protection; return -8;
}
int uft_amiga_set_comment(uft_amiga_ctx_t *ctx, const char *path,
                           const char *comment) {
    (void)ctx; (void)path; (void)comment; return -8;
}

int uft_amiga_foreach_entry(const uft_amiga_ctx_t *ctx, uint32_t dir_block,
                             uft_amiga_dir_callback_t cb, void *user) {
    if (!ctx || !cb) return -1;
    if (dir_block == 0) dir_block = ctx->root_block;
    uft_amiga_dir_t d;
    if (uft_amiga_load_dir(ctx, dir_block, &d) != 0) return -1;
    int rc = 0;
    for (size_t i = 0; i < d.count; i++) {
        rc = cb(&d.entries[i], user);
        if (rc != 0) break;
    }
    uft_amiga_free_dir(&d);
    return rc;
}
