/**
 * @file uft_chd.c
 * @brief MAME Compressed Hunks of Data (CHD) Read-Only Parser
 *
 * Parses CHD v3, v4, and v5 headers and metadata. Supports direct
 * sector access for uncompressed CHD files. Compressed CHDs are
 * detected but require external libchdr for decompression.
 *
 * All CHD header fields are stored big-endian on disk.
 *
 * @author UFT Project
 * @date 2026-04-10
 */

#include "uft/formats/mame/uft_chd.h"
#include "uft/uft_error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Big-Endian Readers
 * ============================================================================ */

static uint32_t read_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  | (uint32_t)p[3];
}

static uint64_t read_be64(const uint8_t *p)
{
    return ((uint64_t)read_be32(p) << 32) | (uint64_t)read_be32(p + 4);
}

/* ============================================================================
 * Metadata Parsing
 * ============================================================================ */

/**
 * @brief Parse CHS geometry from metadata string
 *
 * Expected format: "CYLS:80,HEADS:2,SECS:18,BPS:512"
 * Also handles legacy: "80,2,18,512"
 */
static int parse_geometry_string(const char *str, uft_chd_info_t *info)
{
    if (!str || !info) return -1;

    uint32_t c = 0, h = 0, s = 0, bps = 512;

    /* Try MAME verbose format first */
    if (sscanf(str, "CYLS:%u,HEADS:%u,SECS:%u,BPS:%u", &c, &h, &s, &bps) == 4) {
        info->cylinders = c;
        info->heads = h;
        info->sectors = s;
        info->sector_size = bps;
        return 0;
    }

    /* Try legacy comma-separated format */
    if (sscanf(str, "%u,%u,%u,%u", &c, &h, &s, &bps) >= 3) {
        info->cylinders = c;
        info->heads = h;
        info->sectors = s;
        info->sector_size = (bps > 0) ? bps : 512;
        return 0;
    }

    return -1;
}

/**
 * @brief Search for metadata entries in CHD file
 *
 * CHD metadata entries are stored as a linked list starting at meta_offset.
 * Each entry: 4-byte tag, 4-byte flags+length, 8-byte next offset, then data.
 */
static int chd_read_metadata_entry(FILE *fp, uint64_t meta_offset,
                                    const char *tag, char *value,
                                    size_t valuelen)
{
    if (!fp || !tag || !value || valuelen == 0) return -1;

    uint64_t offset = meta_offset;
    uint8_t meta_hdr[16];
    uint32_t tag_val = ((uint32_t)tag[0] << 24) | ((uint32_t)tag[1] << 16) |
                       ((uint32_t)tag[2] << 8) | (uint32_t)tag[3];

    /* Walk the metadata chain (limit iterations to prevent infinite loops) */
    for (int i = 0; i < 256 && offset != 0; i++) {
        if (fseek(fp, (long)offset, SEEK_SET) != 0) return _UFTEC_IO;
        if (fread(meta_hdr, 1, 16, fp) != 16) return _UFTEC_FREAD;

        uint32_t entry_tag = read_be32(meta_hdr);
        uint32_t flags_len = read_be32(meta_hdr + 4);
        uint64_t next_offset = read_be64(meta_hdr + 8);

        uint32_t data_len = flags_len & 0x00FFFFFF;

        if (entry_tag == tag_val) {
            /* Found matching tag; read the data */
            if (data_len == 0) {
                value[0] = '\0';
                return 0;
            }

            size_t to_read = (data_len < valuelen - 1) ? data_len : valuelen - 1;
            if (fread(value, 1, to_read, fp) != to_read) return _UFTEC_FREAD;
            value[to_read] = '\0';
            return 0;
        }

        offset = next_offset;
    }

    return _UFTEC_FNOTFOUND; /* Tag not found */
}

/* ============================================================================
 * Header Parsing (v3, v4, v5)
 * ============================================================================ */

static int chd_parse_v5(FILE *fp, const uint8_t *raw_hdr, uft_chd_info_t *info)
{
    /* v5 header layout (124 bytes total):
     *   0..7:   magic
     *   8..11:  header_size (32)
     *  12..15:  version (32)
     *  16..31:  compressors[4] (4 x 32)
     *  32..39:  logical_bytes (64)
     *  40..47:  map_offset (64)
     *  48..55:  meta_offset (64)
     *  56..59:  hunk_bytes (32)
     *  60..63:  unit_bytes (32)
     *  64..83:  raw_sha1 (20)
     *  84..103: sha1 (20)
     *  104..123: parent_sha1 (20)
     */
    (void)fp;

    memcpy(info->header.magic, raw_hdr, 8);
    info->header.header_size = read_be32(raw_hdr + 8);
    info->header.version = read_be32(raw_hdr + 12);

    for (int i = 0; i < 4; i++) {
        info->header.compressors[i] = read_be32(raw_hdr + 16 + i * 4);
    }

    info->header.logical_bytes = read_be64(raw_hdr + 32);
    info->header.map_offset    = read_be64(raw_hdr + 40);
    info->header.meta_offset   = read_be64(raw_hdr + 48);
    info->header.hunk_bytes    = read_be32(raw_hdr + 56);
    info->header.unit_bytes    = read_be32(raw_hdr + 60);
    memcpy(info->header.raw_sha1, raw_hdr + 64, 20);
    memcpy(info->header.sha1, raw_hdr + 84, 20);
    memcpy(info->header.parent_sha1, raw_hdr + 104, 20);

    /* Check compression */
    info->is_compressed = false;
    for (int i = 0; i < 4; i++) {
        if (info->header.compressors[i] != CHD_COMP_NONE &&
            info->header.compressors[i] != 0) {
            info->is_compressed = true;
            break;
        }
    }

    /* Derive hunk count */
    if (info->header.hunk_bytes > 0) {
        info->hunk_count = (uint32_t)(
            (info->header.logical_bytes + info->header.hunk_bytes - 1) /
            info->header.hunk_bytes);
    }

    /* Default sector size from unit_bytes */
    info->sector_size = info->header.unit_bytes;
    if (info->sector_size == 0) info->sector_size = 512;

    return 0;
}

static int chd_parse_v4(FILE *fp, const uint8_t *raw_hdr, uft_chd_info_t *info)
{
    /* v4 header layout (108 bytes):
     *   0..7:   magic
     *   8..11:  header_size
     *  12..15:  version
     *  16..19:  flags
     *  20..23:  compression
     *  24..27:  total_hunks
     *  28..35:  logical_bytes (64)
     *  36..43:  meta_offset (64)
     *  44..47:  hunk_bytes
     *  48..67:  sha1
     *  68..87:  parent_sha1
     *  88..107: raw_sha1
     */
    (void)fp;

    memcpy(info->header.magic, raw_hdr, 8);
    info->header.header_size = read_be32(raw_hdr + 8);
    info->header.version = read_be32(raw_hdr + 12);

    uint32_t compression = read_be32(raw_hdr + 20);
    info->header.compressors[0] = compression;
    info->header.compressors[1] = 0;
    info->header.compressors[2] = 0;
    info->header.compressors[3] = 0;

    info->hunk_count = read_be32(raw_hdr + 24);
    info->header.logical_bytes = read_be64(raw_hdr + 28);
    info->header.meta_offset = read_be64(raw_hdr + 36);
    info->header.hunk_bytes = read_be32(raw_hdr + 44);

    memcpy(info->header.sha1, raw_hdr + 48, 20);
    memcpy(info->header.parent_sha1, raw_hdr + 68, 20);
    memcpy(info->header.raw_sha1, raw_hdr + 88, 20);

    info->is_compressed = (compression != CHD_COMP_NONE);
    info->header.unit_bytes = 512; /* v4 default */
    info->sector_size = 512;

    return 0;
}

static int chd_parse_v3(FILE *fp, const uint8_t *raw_hdr, uft_chd_info_t *info)
{
    /* v3 header layout (120 bytes):
     *   0..7:   magic
     *   8..11:  header_size
     *  12..15:  version
     *  16..19:  flags
     *  20..23:  compression
     *  24..27:  total_hunks
     *  28..35:  logical_bytes (64)
     *  36..43:  meta_offset (64)
     *  44..63:  md5
     *  64..83:  parent_md5
     *  84..87:  hunk_bytes
     *  88..107: sha1
     *  108..127: parent_sha1 (only 12 bytes used in some versions)
     */
    (void)fp;

    memcpy(info->header.magic, raw_hdr, 8);
    info->header.header_size = read_be32(raw_hdr + 8);
    info->header.version = read_be32(raw_hdr + 12);

    uint32_t compression = read_be32(raw_hdr + 20);
    info->header.compressors[0] = compression;

    info->hunk_count = read_be32(raw_hdr + 24);
    info->header.logical_bytes = read_be64(raw_hdr + 28);
    info->header.meta_offset = read_be64(raw_hdr + 36);
    info->header.hunk_bytes = read_be32(raw_hdr + 84);

    memcpy(info->header.sha1, raw_hdr + 88, 20);

    info->is_compressed = (compression != CHD_COMP_NONE);
    info->header.unit_bytes = 512;
    info->sector_size = 512;

    return 0;
}

/* ============================================================================
 * API Implementation
 * ============================================================================ */

int uft_chd_open(const char *path, uft_chd_info_t *info)
{
    if (!path || !info) return _UFTEC_NULLPTR;
    memset(info, 0, sizeof(uft_chd_info_t));

    FILE *fp = fopen(path, "rb");
    if (!fp) return _UFTEC_FOPEN;

    /* Read enough for the largest header version */
    uint8_t raw_hdr[128];
    memset(raw_hdr, 0, sizeof(raw_hdr));

    if (fread(raw_hdr, 1, 16, fp) != 16) {
        fclose(fp);
        return _UFTEC_FREAD;
    }

    /* Verify magic */
    if (memcmp(raw_hdr, CHD_MAGIC, CHD_MAGIC_SIZE) != 0) {
        fclose(fp);
        return _UFTEC_CORRUPT;
    }

    uint32_t header_size = read_be32(raw_hdr + 8);
    uint32_t version = read_be32(raw_hdr + 12);
    info->version = version;

    /* Read rest of header */
    size_t remaining = 0;
    if (version == 5 && header_size >= CHD_V5_HEADER) {
        remaining = CHD_V5_HEADER - 16;
    } else if (version == 4 && header_size >= CHD_V4_HEADER) {
        remaining = CHD_V4_HEADER - 16;
    } else if (version == 3 && header_size >= CHD_V3_HEADER) {
        remaining = CHD_V3_HEADER - 16;
    } else {
        fclose(fp);
        snprintf(info->description, sizeof(info->description),
                 "Unsupported CHD version %u (header_size=%u)", version, header_size);
        return _UFTEC_UNSUP;
    }

    if (remaining > sizeof(raw_hdr) - 16) remaining = sizeof(raw_hdr) - 16;
    if (fread(raw_hdr + 16, 1, remaining, fp) != remaining) {
        fclose(fp);
        return _UFTEC_FREAD;
    }

    /* Parse version-specific header */
    int rc;
    switch (version) {
        case 5:  rc = chd_parse_v5(fp, raw_hdr, info); break;
        case 4:  rc = chd_parse_v4(fp, raw_hdr, info); break;
        case 3:  rc = chd_parse_v3(fp, raw_hdr, info); break;
        default: rc = _UFTEC_UNSUP; break;
    }

    if (rc != 0) {
        fclose(fp);
        return rc;
    }

    /* Determine if this is a floppy image */
    info->is_floppy = (info->header.logical_bytes > 0 &&
                       info->header.logical_bytes <= CHD_FLOPPY_MAX_SIZE);

    /* Try to read geometry from GDDD metadata */
    if (info->header.meta_offset > 0) {
        char geo_str[256];
        memset(geo_str, 0, sizeof(geo_str));

        if (chd_read_metadata_entry(fp, info->header.meta_offset,
                                     "GDDD", geo_str, sizeof(geo_str)) == 0) {
            parse_geometry_string(geo_str, info);
        }
    }

    /* Fallback geometry estimation for floppies without metadata */
    if (info->cylinders == 0 && info->is_floppy && info->sector_size > 0) {
        uint64_t total_sectors = info->header.logical_bytes / info->sector_size;

        /* Common floppy geometries */
        if (total_sectors == 2880) {
            /* 1.44 MB: 80 cyl, 2 heads, 18 spt */
            info->cylinders = 80;
            info->heads = 2;
            info->sectors = 18;
        } else if (total_sectors == 1440) {
            /* 720 KB: 80 cyl, 2 heads, 9 spt */
            info->cylinders = 80;
            info->heads = 2;
            info->sectors = 9;
        } else if (total_sectors == 720) {
            /* 360 KB: 40 cyl, 2 heads, 9 spt */
            info->cylinders = 40;
            info->heads = 2;
            info->sectors = 9;
        } else if (total_sectors == 1600) {
            /* Amiga DD: 80 cyl, 2 heads, 11 spt (880 KB w/ 512-byte sectors) */
            info->cylinders = 80;
            info->heads = 2;
            info->sectors = 10;
        }
    }

    /* Build description string */
    snprintf(info->description, sizeof(info->description),
             "CHD v%u, %s, %llu bytes, %u hunk%s%s%s",
             version,
             info->is_compressed ? "compressed" : "uncompressed",
             (unsigned long long)info->header.logical_bytes,
             info->hunk_count,
             info->hunk_count != 1 ? "s" : "",
             info->is_floppy ? ", floppy" : "",
             (info->cylinders > 0) ? "" : ", no geometry");

    fclose(fp);
    return 0;
}

int uft_chd_read_sector(const char *path, const uft_chd_info_t *info,
                         int cyl, int head, int sector,
                         uint8_t *buf, size_t buflen)
{
    if (!path || !info || !buf) return _UFTEC_NULLPTR;

    if (info->is_compressed) {
        return _UFTEC_UNSUP; /* Compressed CHD requires libchdr */
    }

    if (info->cylinders == 0 || info->heads == 0 || info->sectors == 0) {
        return _UFTEC_CORRUPT; /* No geometry available */
    }

    if (cyl < 0 || (uint32_t)cyl >= info->cylinders ||
        head < 0 || (uint32_t)head >= info->heads ||
        sector < 1 || (uint32_t)sector > info->sectors) {
        return _UFTEC_INVAL;
    }

    if (buflen < info->sector_size) return _UFTEC_INVAL;

    /* Calculate linear sector number (sector is 1-based) */
    uint64_t linear_sector = (uint64_t)cyl * info->heads * info->sectors +
                             (uint64_t)head * info->sectors +
                             (uint64_t)(sector - 1);

    uint64_t byte_offset = linear_sector * info->sector_size;

    /* For uncompressed v5 CHDs, data starts after the hunk map.
     * The hunk map is at map_offset, each entry is 4 bytes (v5 uncompressed).
     * Data for each hunk follows sequentially after the map.
     *
     * However, for truly uncompressed CHDs (compressor[0]==0), the hunk map
     * entries directly encode the file offset of each hunk. We use a simpler
     * approach: compute which hunk the sector is in, then read the offset
     * from the hunk map. */

    FILE *fp = fopen(path, "rb");
    if (!fp) return _UFTEC_FOPEN;

    uint32_t hunk_bytes = info->header.hunk_bytes;
    if (hunk_bytes == 0) {
        fclose(fp);
        return _UFTEC_CORRUPT;
    }

    uint32_t hunk_index = (uint32_t)(byte_offset / hunk_bytes);
    uint32_t offset_in_hunk = (uint32_t)(byte_offset % hunk_bytes);

    if (hunk_index >= info->hunk_count) {
        fclose(fp);
        return _UFTEC_INVAL;
    }

    if (info->version == 5) {
        /* v5 uncompressed: hunk map at map_offset, each entry is 4 bytes.
         * For uncompressed hunks, map entry = offset / unit_bytes (or direct). */
        /* Simplified: for 'none' compression in v5, hunks are stored sequentially
         * after the map. Each map entry is 4 bytes containing the hunk's offset
         * divided by the unit size. */
        uint8_t map_entry[4];
        uint64_t map_pos = info->header.map_offset + (uint64_t)hunk_index * 4;

        if (fseek(fp, (long)map_pos, SEEK_SET) != 0) {
            fclose(fp);
            return _UFTEC_IO;
        }
        if (fread(map_entry, 1, 4, fp) != 4) {
            fclose(fp);
            return _UFTEC_FREAD;
        }

        /* For uncompressed CHDs, the map entry stores the offset in units */
        uint64_t hunk_file_offset = (uint64_t)read_be32(map_entry) *
                                    info->header.unit_bytes;

        uint64_t sector_file_offset = hunk_file_offset + offset_in_hunk;

        if (fseek(fp, (long)sector_file_offset, SEEK_SET) != 0) {
            fclose(fp);
            return _UFTEC_IO;
        }
    } else {
        /* v3/v4: hunk map starts at fixed offset after header.
         * Each map entry is 8 bytes: 64-bit file offset of the hunk. */
        uint64_t map_start = (info->version == 4) ? CHD_V4_HEADER : CHD_V3_HEADER;
        uint64_t map_pos = map_start + (uint64_t)hunk_index * 8;

        uint8_t map_entry[8];
        if (fseek(fp, (long)map_pos, SEEK_SET) != 0) {
            fclose(fp);
            return _UFTEC_IO;
        }
        if (fread(map_entry, 1, 8, fp) != 8) {
            fclose(fp);
            return _UFTEC_FREAD;
        }

        uint64_t hunk_file_offset = read_be64(map_entry);
        uint64_t sector_file_offset = hunk_file_offset + offset_in_hunk;

        if (fseek(fp, (long)sector_file_offset, SEEK_SET) != 0) {
            fclose(fp);
            return _UFTEC_IO;
        }
    }

    if (fread(buf, 1, info->sector_size, fp) != info->sector_size) {
        fclose(fp);
        return _UFTEC_FREAD;
    }

    fclose(fp);
    return 0;
}

int uft_chd_get_metadata(const char *path, const char *tag,
                          char *value, size_t valuelen)
{
    if (!path || !tag || !value || valuelen == 0) return _UFTEC_NULLPTR;
    if (strlen(tag) != 4) return _UFTEC_INVAL;

    FILE *fp = fopen(path, "rb");
    if (!fp) return _UFTEC_FOPEN;

    /* Read and verify header to get meta_offset */
    uint8_t hdr[64];
    if (fread(hdr, 1, 56, fp) != 56) {
        fclose(fp);
        return _UFTEC_FREAD;
    }

    if (memcmp(hdr, CHD_MAGIC, CHD_MAGIC_SIZE) != 0) {
        fclose(fp);
        return _UFTEC_CORRUPT;
    }

    uint32_t version = read_be32(hdr + 12);
    uint64_t meta_offset = 0;

    if (version == 5) {
        meta_offset = read_be64(hdr + 48);
    } else if (version == 4 || version == 3) {
        meta_offset = read_be64(hdr + 36);
    } else {
        fclose(fp);
        return _UFTEC_UNSUP;
    }

    if (meta_offset == 0) {
        fclose(fp);
        return _UFTEC_FNOTFOUND;
    }

    int rc = chd_read_metadata_entry(fp, meta_offset, tag, value, valuelen);
    fclose(fp);
    return rc;
}
