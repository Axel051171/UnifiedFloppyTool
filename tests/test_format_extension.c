/*
 * test_format_extension.c — improvement test: formats outside gw's set
 * (#110c, P3.3).
 *
 * Strategy: docs/TESTER_STRATEGY.md §3 (improvement/format_extension/).
 *
 * WHAT THIS DEFENDS
 * -----------------
 * Greaseweazle reads and writes floppy flux. It has no notion of an
 * Amiga *hard disk* image, let alone the Rigid Disk Block (RDB)
 * partition table that lives on one — that format is entirely outside
 * gw's world. UFT carries a real RDB/HDF parser. This is an improvement
 * test: gw cannot pass it because the format is not in its set at all;
 * there is no gw side to diff against, which is the whole point — the
 * assertion is purely on UFT's decode of a known-good fixture.
 *
 * The fixture is a synthetic HDF built in-process, byte-exact to the
 * documented RDB / PART block layout (big-endian, the Amiga
 * convention). It is not a captured disk image and never claims to be.
 *
 * Three layers:
 *   - uft_hdf_parse_rdb()       — buffer-level RDB block decode
 *   - uft_hdf_parse_partition() — buffer-level PART block decode
 *   - uft_hdf_parse()           — full-file decode: RDB + partition list
 *
 * Scope vs. the original TESTER_STRATEGY plan: that list named IPF /
 * STX / KryoFlux-stream / Japanese formats. HDF/RDB is delivered here
 * because it has a real, self-contained, buffer-based parser that can
 * be exercised honestly from a synthetic fixture. The others remain
 * open — see the category README.
 */
#include "uft_hdf_parser.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── check harness ───────────────────────────────────────────────────*/
static int g_errors = 0;
#define CHECK(cond, msg)                                                 \
    do {                                                                 \
        if (!(cond)) {                                                   \
            ++g_errors;                                                  \
            fprintf(stderr, "[format_ext] FAIL %s:%d  %s\n",             \
                    __FILE__, __LINE__, (msg));                          \
        }                                                                \
    } while (0)

/* RDB / PART blocks are big-endian (the Amiga convention). */
static void put_be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

/* ── synthetic-block builders ────────────────────────────────────────
 * Both build a 512-byte block byte-exact to the layout uft_hdf_parser.c
 * reads. Field offsets mirror the parser, not a guess. */

/* RDSK block. partition_list is the BLOCK index of the first PART. */
static void build_rdb(uint8_t blk[512], uint32_t partition_list)
{
    memset(blk, 0, 512);
    put_be32(blk + 0,  UFT_RDB_MAGIC);    /* "RDSK"                     */
    put_be32(blk + 4,  64);               /* size, in 32-bit words      */
    put_be32(blk + 8,  0);                /* checksum (parser ignores)  */
    put_be32(blk + 12, 0x554E4954);       /* host_id                    */
    put_be32(blk + 16, 512);              /* block_bytes                */
    put_be32(blk + 20, 0);                /* flags                      */
    put_be32(blk + 24, 0xFFFFFFFF);       /* bad_block_list (none)      */
    put_be32(blk + 28, partition_list);   /* partition_list             */
    put_be32(blk + 32, 0xFFFFFFFF);       /* fs_header_list (none)     */
    put_be32(blk + 64, 4);                /* cylinders                  */
    put_be32(blk + 68, 32);               /* sectors                    */
    put_be32(blk + 72, 2);                /* heads                      */
    memcpy(blk + 160, "UFTDISK\0", 8);    /* disk_vendor                */
    memcpy(blk + 168, "SYNTH-HDF-IMAGE\0", 16); /* disk_product         */
}

/* PART block. `next` is the BLOCK index of the next PART, or
 * 0xFFFFFFFF to end the list. */
static void build_part(uint8_t blk[512], const char *name, uint32_t next,
                       uint32_t low_cyl, uint32_t high_cyl, uint32_t dos_type)
{
    memset(blk, 0, 512);
    put_be32(blk + 0,  UFT_PART_MAGIC);   /* "PART"                     */
    put_be32(blk + 16, next);             /* pb_Next                    */

    /* Drive name: BCPL string (length byte + chars) at offset 36. */
    size_t n = strlen(name);
    if (n > 30) n = 30;
    blk[36] = (uint8_t)n;
    memcpy(blk + 37, name, n);

    /* Environment vector at offset 128. */
    uint8_t *env = blk + 128;
    put_be32(env + 4,  128);              /* SizeBlock -> block_size*4  */
    put_be32(env + 24, low_cyl);          /* LowCyl                     */
    put_be32(env + 28, high_cyl);         /* HighCyl                    */
    put_be32(env + 48, 0);                /* BootPri (>=0 -> bootable)  */
    put_be32(env + 52, dos_type);         /* DosType                    */
}

/* ════════════════════════════════════════════════════════════════════
 *  Buffer-level RDB decode
 * ════════════════════════════════════════════════════════════════════ */
static void test_rdb_buffer_parse(void)
{
    uint8_t blk[512];
    build_rdb(blk, 1);

    uft_rdb_info_t rdb;
    int rc = uft_hdf_parse_rdb(blk, sizeof(blk), &rdb);
    CHECK(rc == 0, "RDB: parse returned non-zero");
    CHECK(rdb.valid, "RDB: not marked valid");
    CHECK(rdb.block_bytes == 512, "RDB: block_bytes wrong");
    CHECK(rdb.cylinders == 4, "RDB: cylinders wrong");
    CHECK(rdb.sectors == 32, "RDB: sectors wrong");
    CHECK(rdb.heads == 2, "RDB: heads wrong");
    CHECK(rdb.partition_list == 1, "RDB: partition_list wrong");
    CHECK(memcmp(rdb.disk_vendor, "UFTDISK", 7) == 0, "RDB: disk_vendor wrong");
    CHECK(memcmp(rdb.disk_product, "SYNTH-HDF-IMAGE", 15) == 0,
          "RDB: disk_product wrong");

    /* A buffer without the RDSK magic must be rejected — not guessed at. */
    uint8_t bad[512];
    memset(bad, 0xAA, sizeof(bad));
    uft_rdb_info_t r2;
    CHECK(uft_hdf_parse_rdb(bad, sizeof(bad), &r2) == -1,
          "RDB: non-RDSK buffer wrongly accepted");

    /* Too short to even hold a header -> reject. */
    CHECK(uft_hdf_parse_rdb(blk, 64, &r2) == -1,
          "RDB: undersized buffer wrongly accepted");
}

/* ════════════════════════════════════════════════════════════════════
 *  Buffer-level PART decode
 * ════════════════════════════════════════════════════════════════════ */
static void test_partition_buffer_parse(void)
{
    uint8_t blk[512];
    build_part(blk, "DH0", 0xFFFFFFFF, 2, 3, UFT_DOS_FFS_I);

    uft_hdf_partition_t part;
    int rc = uft_hdf_parse_partition(blk, sizeof(blk), &part);
    CHECK(rc == 0, "PART: parse returned non-zero");
    CHECK(strcmp(part.name, "DH0") == 0, "PART: name wrong");
    CHECK(part.block_size == 512, "PART: block_size wrong (SizeBlock*4)");
    CHECK(part.start_cylinder == 2, "PART: start_cylinder wrong");
    CHECK(part.end_cylinder == 3, "PART: end_cylinder wrong");
    CHECK(part.dos_type == UFT_DOS_FFS_I, "PART: dos_type wrong");
    CHECK(part.fs_type_name != NULL &&
          strcmp(part.fs_type_name, "FFS-INTL") == 0,
          "PART: fs_type_name not 'FFS-INTL'");
    CHECK(part.bootable, "PART: BootPri 0 should be bootable");

    /* Non-PART buffer -> reject. */
    uint8_t bad[512];
    memset(bad, 0, sizeof(bad));
    uft_hdf_partition_t p2;
    CHECK(uft_hdf_parse_partition(bad, sizeof(bad), &p2) == -1,
          "PART: non-PART buffer wrongly accepted");
}

/* ════════════════════════════════════════════════════════════════════
 *  Filesystem-type name table
 * ════════════════════════════════════════════════════════════════════ */
static void test_fs_type_names(void)
{
    CHECK(strcmp(uft_hdf_fs_type_name(UFT_DOS_OFS),   "OFS") == 0,
          "fs_type_name: OFS");
    CHECK(strcmp(uft_hdf_fs_type_name(UFT_DOS_FFS),   "FFS") == 0,
          "fs_type_name: FFS");
    CHECK(strcmp(uft_hdf_fs_type_name(UFT_DOS_FFS_I), "FFS-INTL") == 0,
          "fs_type_name: FFS-INTL");
    CHECK(strcmp(uft_hdf_fs_type_name(0x12345678), "Unknown") == 0,
          "fs_type_name: unknown dos_type should be 'Unknown'");
}

/* ════════════════════════════════════════════════════════════════════
 *  Full-file decode: RDB + partition list walked from disk
 * ════════════════════════════════════════════════════════════════════ */
static void test_hdf_file_end_to_end(void)
{
    /* Synthetic 2-block HDF: block 0 = RDSK (partition_list -> block 1),
     * block 1 = PART (pb_Next = end-of-list). */
    uint8_t img[1024];
    build_rdb(img,            1);
    build_part(img + 512, "DH0", 0xFFFFFFFF, 0, 3, UFT_DOS_FFS_I);

    const char *path = "test_format_extension_synth.hdf";
    FILE *f = fopen(path, "wb");
    if (!f) { perror("fopen"); ++g_errors; return; }
    size_t wrote = fwrite(img, 1, sizeof(img), f);
    fclose(f);
    if (wrote != sizeof(img)) {
        fprintf(stderr, "[format_ext] FAIL: short write of synthetic HDF\n");
        ++g_errors;
        remove(path);
        return;
    }

    uft_hdf_info_t info;
    int rc = uft_hdf_parse(path, &info);
    remove(path);

    CHECK(rc == 0, "HDF file: parse returned non-zero");
    CHECK(info.has_rdb, "HDF file: RDB not detected");
    CHECK(info.geometry.cylinders == 4, "HDF file: geometry cylinders wrong");
    CHECK(info.geometry.heads == 2, "HDF file: geometry heads wrong");
    CHECK(info.geometry.sectors == 32, "HDF file: geometry sectors wrong");
    CHECK(info.num_partitions == 1, "HDF file: expected exactly 1 partition");
    if (info.num_partitions == 1) {
        const uft_hdf_partition_t *p = &info.partitions[0];
        CHECK(strcmp(p->name, "DH0") == 0, "HDF file: partition name wrong");
        CHECK(p->dos_type == UFT_DOS_FFS_I, "HDF file: partition dos_type wrong");
        CHECK(p->start_cylinder == 0, "HDF file: partition start_cylinder wrong");
        CHECK(p->end_cylinder == 3, "HDF file: partition end_cylinder wrong");
        /* num_blocks = cyls * heads * sectors = 4 * 2 * 32 = 256. */
        CHECK(p->num_blocks == 256, "HDF file: partition num_blocks wrong");
    }
}

int main(void)
{
    test_rdb_buffer_parse();
    test_partition_buffer_parse();
    test_fs_type_names();
    test_hdf_file_end_to_end();

    if (g_errors) {
        fprintf(stderr, "[format_ext] %d check(s) failed\n", g_errors);
        return 1;
    }
    printf("[format_ext] all checks passed\n");
    return 0;
}
