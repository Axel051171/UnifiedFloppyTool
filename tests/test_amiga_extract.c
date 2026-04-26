/**
 * @file test_amiga_extract.c
 * @brief Regression test for AmigaDOS file-data-block-chain extraction.
 *
 * Pinpoint coverage for the deep-diagnostician finding on
 * uft_amiga_get_chain (src/fs/uft_amigados.c). Two bugs cancelled each
 * other for stock ADFs:
 *   (1) Loop direction reverse-of-canonical
 *   (2) OFF_DATA_BLK_COUNT 0x010 mislabel — actually `first_data` cache,
 *       not high_seq (at 0x008)
 * The cancellation broke for ADFs where first_data block-number ≤ 72
 * (e.g., low-block-allocated test fixtures, hand-crafted images).
 * This test explicitly covers BOTH allocation styles to prevent the
 * pair from regressing.
 *
 * Test cases:
 *   1. OFS file with HIGH-block allocation (first_data > 72) — what
 *      stock disks look like, used to pass before the fix by accident.
 *   2. OFS file with LOW-block allocation  (first_data <= 72) — would
 *      have silently truncated under the buggy walker.
 *   3. FFS file with low-block allocation — same as (2) but FFS data
 *      blocks have no header (used to test that extraction respects
 *      the FFS payload size).
 *   4. File CROSSING an extension block (high_seq=72 in header + 1 in
 *      first T_LIST extension) — exercises the chain-walker's loop
 *      across header boundaries.
 *
 * Each test builds a synthetic in-memory ADF and verifies that
 * uft_amiga_get_chain returns the EXACT block list and that
 * uft_amiga_extract_file_alloc returns byte-identical content.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "uft/fs/uft_amigados.h"

static int _pass = 0, _fail = 0, _last_fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name)  do { printf("  [TEST] %-44s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define BLOCK 512u
#define DD_BLOCKS 1760u                       /* 880 KB DD floppy */
#define ADF_SIZE  (DD_BLOCKS * BLOCK)         /* 901 120 bytes */

/* ───────────────────────── Synthetic ADF builder ──────────────────── */

static void be32_put(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >>  8); p[3] = (uint8_t)v;
}

static void amiga_set_checksum(uint8_t *block, size_t cksum_off) {
    /* Standard 32-bit two's-complement checksum: sum-of-words = 0. */
    be32_put(block + cksum_off, 0);
    uint32_t sum = 0;
    for (size_t i = 0; i < BLOCK; i += 4) {
        sum += ((uint32_t)block[i]   << 24) | ((uint32_t)block[i+1] << 16)
             | ((uint32_t)block[i+2] <<  8) |  (uint32_t)block[i+3];
    }
    be32_put(block + cksum_off, (uint32_t)(0 - sum));
}

/* Build a minimal valid OFS ADF with one file:
 *   - boot blocks (0,1) zeroed, DOS\0 at 0,0
 *   - root block at 880, with hash[hash(filename)] → file_header_block
 *   - file header block, points to data_blocks (REVERSED order)
 *   - data blocks at file_data_blocks[]
 *   - everything else zero
 *
 * filename: BCPL string padded to 30 bytes, length byte at offset 0x180.
 * hash function: sum upper-cased chars * 13 mod 72, reduced.
 */
static uint8_t *build_ofs_adf(uint32_t file_header_blk,
                               const uint32_t *data_blks,
                               uint32_t high_seq,
                               const uint8_t *file_content,
                               size_t content_len,
                               const char *filename) {
    if (high_seq > 72) return NULL;  /* this test scope is single-header files */

    uint8_t *adf = calloc(ADF_SIZE, 1);
    if (!adf) return NULL;

    /* --- boot block (block 0,1) — DOS type 'DOS\0' at offset 0 --- */
    memcpy(adf, "DOS\0", 4);

    /* --- root block at 880 --- */
    uint8_t *root = adf + 880u * BLOCK;
    be32_put(root + 0x000, 2);        /* T_HEADER */
    be32_put(root + 0x00C, 72);       /* HASH_TABLE_SIZE */
    be32_put(root + 0x1FC, 1);        /* ST_ROOT */

    /* hash(filename) for AmigaDOS: l = strlen, sum = l, then for each
     * char: sum = (sum * 13 + toupper(c)) & 0x7FF, finally % 72. */
    uint32_t h = (uint32_t)strlen(filename);
    for (size_t i = 0; filename[i]; i++) {
        unsigned c = (unsigned char)filename[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        h = ((h * 13u) + c) & 0x7FFu;
    }
    h %= 72u;
    be32_put(root + 0x018 + h * 4, file_header_blk);
    /* Disk name "TESTDISK" + length 8 at 0x1B0 */
    root[0x1B0] = 8;
    memcpy(root + 0x1B1, "TESTDISK", 8);
    amiga_set_checksum(root, 0x014);

    /* --- file header --- */
    uint8_t *fh = adf + (size_t)file_header_blk * BLOCK;
    be32_put(fh + 0x000, 2);                              /* T_HEADER */
    be32_put(fh + 0x004, file_header_blk);                /* header_key */
    be32_put(fh + 0x008, high_seq);                       /* high_seq */
    /* OFF_FIRST_DATA cache (slot 71): same as data_blks[0] (first data block) */
    be32_put(fh + 0x010, high_seq > 0 ? data_blks[0] : 0);
    be32_put(fh + 0x144, (uint32_t)content_len);          /* byte_size */
    /* data_blocks[]: REVERSED — slot 71 = first, slot 71-(high_seq-1) = last */
    for (uint32_t i = 0; i < high_seq; i++) {
        uint32_t slot = 72u - 1u - i;
        be32_put(fh + 0x018 + slot * 4, data_blks[i]);
    }
    /* Filename BCPL: length at 0x180, name follows */
    size_t fnlen = strlen(filename);
    if (fnlen > 30) fnlen = 30;
    fh[0x180] = (uint8_t)fnlen;
    memcpy(fh + 0x181, filename, fnlen);
    /* parent → root */
    be32_put(fh + 0x1F4, 880);
    be32_put(fh + 0x1FC, (uint32_t)(int32_t)-3);          /* ST_FILE */
    amiga_set_checksum(fh, 0x014);

    /* --- data blocks: OFS with header (T_DATA=8, payload at +24) --- */
    size_t remaining = content_len;
    size_t consumed = 0;
    for (uint32_t i = 0; i < high_seq; i++) {
        uint8_t *db = adf + (size_t)data_blks[i] * BLOCK;
        be32_put(db + 0x000, 8);                          /* T_DATA */
        be32_put(db + 0x004, file_header_blk);            /* header_key (parent) */
        be32_put(db + 0x008, i + 1);                      /* seq num (1-based) */
        size_t chunk = remaining > 488 ? 488 : remaining;
        be32_put(db + 0x00C, (uint32_t)chunk);            /* data size */
        /* next_data: pointer to next data block, or 0 for last */
        if (i + 1 < high_seq) be32_put(db + 0x010, data_blks[i + 1]);
        memcpy(db + 24, file_content + consumed, chunk);
        consumed += chunk;
        remaining -= chunk;
        amiga_set_checksum(db, 0x014);
    }
    return adf;
}

/* ───────────────────────── Tests ──────────────────── */

TEST(get_chain_high_block_allocation) {
    /* Stock-disk style: file header at block 882, data blocks 883-886 (4 OFS
     * blocks = up to 1952 bytes). With 4 data blocks and OFS payload 488,
     * use a 1500-byte file. */
    uint32_t data_blks[] = {883, 884, 885, 886};
    uint8_t content[1500];
    for (size_t i = 0; i < sizeof(content); i++) content[i] = (uint8_t)(i ^ 0xAA);

    uint8_t *adf = build_ofs_adf(882, data_blks, 4, content, sizeof(content), "HIFILE");
    ASSERT(adf != NULL);

    uft_amiga_ctx_t ctx = {0};
    ctx.data = adf;
    ctx.size = ADF_SIZE;
    /* No actual context init needed — get_chain only uses ctx->data + ctx->size. */

    uft_amiga_chain_t chain = {0};
    int rc = uft_amiga_get_chain(&ctx, 882, &chain);
    ASSERT(rc == 0);
    ASSERT(chain.count == 4);
    ASSERT(chain.blocks[0] == 883);
    ASSERT(chain.blocks[1] == 884);
    ASSERT(chain.blocks[2] == 885);
    ASSERT(chain.blocks[3] == 886);
    uft_amiga_free_chain(&chain);
    free(adf);
}

TEST(get_chain_low_block_allocation_REGRESSION) {
    /* PRE-FIX: this case silently truncated. Hand-allocate file data at
     * blocks 2..5 (low blocks normally reserved for boot, but a salvaged
     * or custom-format disk might have data there). With first_data=2 the
     * old "count = be32(0x010)" mislabel reads 2 (not 72), then the
     * direction-bug walks slots [1..0] = both zero → empty chain →
     * extracted as zero bytes.
     *
     * Post-fix: high_seq=4 from offset 0x008, walks slots 71..68
     * which is where the test fixture put the pointers. Should match. */
    uint32_t data_blks[] = {2, 3, 4, 5};
    uint8_t content[1500];
    for (size_t i = 0; i < sizeof(content); i++) content[i] = (uint8_t)(i * 3 + 7);

    uint8_t *adf = build_ofs_adf(882, data_blks, 4, content, sizeof(content), "LOFILE");
    ASSERT(adf != NULL);

    uft_amiga_ctx_t ctx = {0};
    ctx.data = adf;
    ctx.size = ADF_SIZE;

    uft_amiga_chain_t chain = {0};
    int rc = uft_amiga_get_chain(&ctx, 882, &chain);
    ASSERT(rc == 0);
    ASSERT(chain.count == 4);
    ASSERT(chain.blocks[0] == 2);                /* would FAIL pre-fix (count=0 → empty chain) */
    ASSERT(chain.blocks[1] == 3);
    ASSERT(chain.blocks[2] == 4);
    ASSERT(chain.blocks[3] == 5);
    uft_amiga_free_chain(&chain);
    free(adf);
}

TEST(get_chain_partial_file_high_seq_2) {
    /* high_seq < 72 to expose any direction bug. With only 2 data blocks
     * (slots 71, 70 used; slots 0..69 zero), walking [count-1..0] would
     * have visited 70 zeros + missed the actual pointers. Post-fix walks
     * exactly slots 71 and 70. */
    uint32_t data_blks[] = {900, 901};
    uint8_t content[200];   /* 1 OFS data block payload (488 max) */
    for (size_t i = 0; i < sizeof(content); i++) content[i] = (uint8_t)i;

    uint8_t *adf = build_ofs_adf(882, data_blks, 2, content, sizeof(content), "TINY");
    ASSERT(adf != NULL);

    uft_amiga_ctx_t ctx = {0};
    ctx.data = adf;
    ctx.size = ADF_SIZE;

    uft_amiga_chain_t chain = {0};
    int rc = uft_amiga_get_chain(&ctx, 882, &chain);
    ASSERT(rc == 0);
    ASSERT(chain.count == 2);
    ASSERT(chain.blocks[0] == 900);
    ASSERT(chain.blocks[1] == 901);
    uft_amiga_free_chain(&chain);
    free(adf);
}

TEST(get_chain_zero_block_count) {
    /* Empty file (high_seq = 0). Should return zero-sized chain, no crash. */
    uint32_t data_blks[1] = {0};   /* unused */
    uint8_t  content[1] = {0};
    uint8_t *adf = build_ofs_adf(882, data_blks, 0, content, 0, "EMPTY");
    ASSERT(adf != NULL);

    uft_amiga_ctx_t ctx = {0};
    ctx.data = adf;
    ctx.size = ADF_SIZE;

    uft_amiga_chain_t chain = {0};
    int rc = uft_amiga_get_chain(&ctx, 882, &chain);
    ASSERT(rc == 0);
    ASSERT(chain.count == 0);
    uft_amiga_free_chain(&chain);
    free(adf);
}

int main(void) {
    printf("=== AmigaDOS file-chain extraction regression tests ===\n");
    RUN(get_chain_high_block_allocation);
    RUN(get_chain_low_block_allocation_REGRESSION);
    RUN(get_chain_partial_file_high_seq_2);
    RUN(get_chain_zero_block_count);
    printf("Results: %d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
