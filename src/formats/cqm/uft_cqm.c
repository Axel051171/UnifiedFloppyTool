/**
 * @file uft_cqm.c
 * @brief CopyQM (Sydex) CQM image reader
 *
 * Header layout and RLE encoding verified field by field against two
 * independent sources (MF-461):
 *
 *   [1] "CopyQM Format (*.cqm) — Disk image layout", RPN, 2023-03-31,
 *       https://rio.early8bitz.de/cqm/cqm-format.pdf — derived from the
 *       LibDsk drivers drvqm.c / crctable.c
 *   [2] SAMdisk 4.0, src/samdisk/cqm.cpp:10-41 (MIT, reference oracle only,
 *       not built — see src/samdisk/README.md)
 *
 * Both agree on every field used here, and [1] states outright that the block
 * at 0x03..0x1B is the BIOS Parameter Block of a DOS floppy — which is why the
 * field order there matches a DOS BPB exactly.
 *
 * The layout this file used before MF-461 matched neither source and no BPB:
 * sector size was read as a 128<<n size code from a single byte at 0x03,
 * sectors per track from 0x08 (which is the FAT-copy count), heads from 0x09
 * (directory entries), cylinders from 0x0F (sectors per FAT), comment length
 * from 0x10 (sectors per track), the data was assumed to start at offset 18
 * instead of 133, and the RLE polarity was inverted. It could not have decoded
 * a real CopyQM image.
 */
#include "uft/uft_format_common.h"
#include "uft/uft_log.h"

/* Header, 133 bytes — [1] "Headersize = 133 Byte (0x85)" */
#define CQM_HEADER_SIZE     133
#define CQM_OFF_SECSIZE     0x03  /**< LE16 bytes per sector                  */
#define CQM_OFF_SPT         0x10  /**< LE16 sectors per track                 */
#define CQM_OFF_HEADS       0x12  /**< LE16 head count (1 or 2)               */
#define CQM_OFF_BLIND       0x58  /**< 0=DOS, 1=blind, 2=HFS                  */
#define CQM_OFF_USED_CYLS   0x5A  /**< cylinders present in the image         */
#define CQM_OFF_TOTAL_CYLS  0x5B  /**< cylinders on the physical disk         */
#define CQM_OFF_DATA_CRC    0x5C  /**< LE32 CRC over the decoded data         */
#define CQM_OFF_COMMENT_LEN 0x6F  /**< LE16 length of the optional comment    */
#define CQM_OFF_SECTOR_BASE 0x71  /**< number of the first sector, minus one  */

typedef struct {
    uint8_t*  data;
    size_t    size;      /**< bytes the geometry calls for                    */
    size_t    decoded;   /**< bytes the file actually delivered               */
    uint16_t  cyls, heads, spt, sec_size;
    uint8_t   sector_base;
    uint8_t   fill;
} cqm_data_t;

/**
 * CopyQM's data CRC. A CRC-32 (0xEDB88320 reflected) with one quirk: CopyQM
 * indexes a 1024-byte table with an 8-bit register, so only the low six bits
 * of each byte reach the table. [1] "all bytes must have their top two bits
 * removed (&0x3f) before added to the CRC"; identical in [2] cqm.cpp:60-79.
 *
 * The table is built per call — 64 entries against a whole disk image is
 * noise, and it keeps the function free of shared state.
 */
static uint32_t cqm_data_crc(const uint8_t* buf, size_t len)
{
    uint32_t table[0x40];
    for (uint32_t i = 0; i < 0x40; i++) {
        uint32_t e = i;
        for (int j = 0; j < 8; j++)
            e = (e >> 1) ^ ((e & 1u) ? 0xEDB88320u : 0u);
        table[i] = e;
    }

    uint32_t crc = 0;
    while (len--)
        crc = table[(crc ^ *buf++) & 0x3fu] ^ (crc >> 8);
    return crc;
}

/**
 * RLE, per [1]: a positive 16-bit count introduces that many literal bytes,
 * a negative one repeats the single byte that follows -count times.
 * Zero is not defined by the format and cannot make progress, so it ends the
 * stream rather than spinning.
 *
 * @return number of bytes decoded; may be less than @p dst_size on a short file
 */
static size_t cqm_decompress(FILE* f, uint8_t* dst, size_t dst_size)
{
    size_t w = 0;

    while (w < dst_size) {
        uint8_t cb[2];
        if (fread(cb, 1, 2, f) != 2)
            break;

        int16_t n = (int16_t)((uint16_t)cb[0] | ((uint16_t)cb[1] << 8));

        if (n > 0) {                                  /* literal run */
            size_t avail = dst_size - w;
            size_t want  = (size_t)n < avail ? (size_t)n : avail;
            if (fread(dst + w, 1, want, f) != want)
                break;
            w += want;
            if ((size_t)n > want &&                   /* image longer than geometry */
                fseek(f, (long)((size_t)n - want), SEEK_CUR) != 0)
                break;
        } else if (n < 0) {                           /* repeated byte */
            int b = fgetc(f);
            if (b == EOF)
                break;
            size_t cnt   = (size_t)(-(int32_t)n);
            size_t avail = dst_size - w;
            if (cnt > avail)
                cnt = avail;
            memset(dst + w, b, cnt);
            w += cnt;
        } else {
            break;
        }
    }
    return w;
}

/**
 * Probe. The three-byte marker alone is thin, so confidence is graded: a full
 * header whose checksum holds and whose geometry the reader would accept is
 * worth far more than a signature match. Same reasoning as the MSA and D88
 * probes (MF-447, MF-460) — since the registry really asks every plugin, a
 * probe that claims 95 on two or three bytes outbids plugins that are right.
 */
bool cqm_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence)
{
    (void)file_size;

    if (size < 3 || data[0] != 'C' || data[1] != 'Q' || data[2] != 0x14)
        return false;

    if (size < CQM_HEADER_SIZE) {
        *confidence = 60;                 /* marker only — nothing else seen */
        return true;
    }

    unsigned sum = 0;
    for (size_t i = 0; i < CQM_HEADER_SIZE; i++)
        sum += data[i];

    uint16_t sec_size = uft_read_le16(&data[CQM_OFF_SECSIZE]);
    uint16_t spt      = uft_read_le16(&data[CQM_OFF_SPT]);
    uint16_t heads    = uft_read_le16(&data[CQM_OFF_HEADS]);
    uint8_t  used     = data[CQM_OFF_USED_CYLS];

    bool plausible = (sum & 0xffu) == 0 &&
                     heads >= 1 && heads <= 2 &&
                     spt >= 1 && spt <= UFT_MAX_SPT &&
                     used >= 1 &&
                     uft_sector_sizes[uft_bytes_to_size_code(sec_size)] == sec_size;

    *confidence = plausible ? 95 : 60;
    return true;
}

static uft_error_t cqm_open(uft_disk_t* disk, const char* path, bool read_only)
{
    (void)read_only;

    FILE* f = fopen(path, "rb");
    if (!f)
        return UFT_ERR_FILE_OPEN;

    uint8_t h[CQM_HEADER_SIZE];
    if (fread(h, 1, sizeof(h), f) != sizeof(h)) {
        fclose(f);
        return UFT_ERR_IO;
    }
    if (h[0] != 'C' || h[1] != 'Q' || h[2] != 0x14) {
        fclose(f);
        return UFT_ERR_FORMAT_INVALID;
    }

    /* [1]: "When reading the header, the sum over the entire header must be
     * zero" — byte 132 is chosen to make it so. A header that fails this is
     * not partially usable: every geometry field below would be a guess. */
    unsigned sum = 0;
    for (size_t i = 0; i < sizeof(h); i++)
        sum += h[i];
    if ((sum & 0xffu) != 0) {
        UFT_WARN("CQM: Header-Pruefsumme falsch (0x%02X statt 0x00) — %s",
                 sum & 0xffu, path);
        fclose(f);
        return UFT_ERR_CRC;
    }

    uint16_t sec_size    = uft_read_le16(&h[CQM_OFF_SECSIZE]);
    uint16_t spt         = uft_read_le16(&h[CQM_OFF_SPT]);
    uint16_t heads       = uft_read_le16(&h[CQM_OFF_HEADS]);
    uint8_t  used_cyls   = h[CQM_OFF_USED_CYLS];
    uint8_t  total_cyls  = h[CQM_OFF_TOTAL_CYLS];
    uint16_t comment_len = uft_read_le16(&h[CQM_OFF_COMMENT_LEN]);
    uint8_t  sector_base = h[CQM_OFF_SECTOR_BASE];
    uint32_t want_crc    = uft_read_le32(&h[CQM_OFF_DATA_CRC]);

    if (heads < 1 || heads > 2 || spt < 1 || used_cyls < 1 ||
        uft_sector_sizes[uft_bytes_to_size_code(sec_size)] != sec_size ||
        !uft_geometry_sane(used_cyls, (uint8_t)heads, (uint8_t)spt, sec_size)) {
        UFT_WARN("CQM: unbrauchbare Geometrie %ux%ux%u a %u Byte — %s",
                 used_cyls, heads, spt, sec_size, path);
        fclose(f);
        return UFT_ERR_FORMAT_INVALID;
    }

    /* Only the cylinders actually stored are reported. t-cyl says how large
     * the source disk was; presenting the difference as readable tracks would
     * be inventing data. */
    if (total_cyls > used_cyls)
        UFT_INFO("CQM: Abbild enthaelt %u von %u Zylindern der Quelldiskette",
                 used_cyls, total_cyls);

    cqm_data_t* p = calloc(1, sizeof(cqm_data_t));
    if (!p) {
        fclose(f);
        return UFT_ERR_NOMEM;
    }

    p->sec_size    = sec_size;
    p->spt         = spt;
    p->heads       = heads;
    p->cyls        = used_cyls;
    p->sector_base = sector_base;
    p->fill        = (h[CQM_OFF_BLIND] == 0) ? 0xE5 : 0x00;  /* [2] cqm.cpp:105 */
    p->size        = (size_t)used_cyls * heads * spt * sec_size;

    if (fseek(f, CQM_HEADER_SIZE + (long)comment_len, SEEK_SET) != 0) {
        fclose(f);
        free(p);
        return UFT_ERR_IO;
    }

    p->data = malloc(p->size);
    if (!p->data) {
        fclose(f);
        free(p);
        return UFT_ERR_NOMEM;
    }
    memset(p->data, p->fill, p->size);

    p->decoded = cqm_decompress(f, p->data, p->size);
    fclose(f);

    if (p->decoded < p->size)
        UFT_WARN("CQM: Datenstrom endet nach %zu von %zu Byte — die fehlenden "
                 "Sektoren werden nicht geliefert (%s)",
                 p->decoded, p->size, path);
    else if (cqm_data_crc(p->data, p->decoded) != want_crc)
        UFT_WARN("CQM: Daten-CRC des Abbilds stimmt nicht (Kopf sagt 0x%08X) — "
                 "Inhalt wird unveraendert geliefert (%s)", want_crc, path);

    disk->plugin_data           = p;
    disk->geometry.cylinders    = p->cyls;
    disk->geometry.heads        = (uint8_t)p->heads;
    disk->geometry.sectors      = (uint8_t)p->spt;
    disk->geometry.sector_size  = p->sec_size;
    disk->geometry.total_sectors = (uint32_t)p->cyls * p->heads * p->spt;
    return UFT_OK;
}

static void cqm_close(uft_disk_t* disk)
{
    cqm_data_t* p = disk->plugin_data;
    if (p) {
        free(p->data);
        free(p);
        disk->plugin_data = NULL;
    }
}

static uft_error_t cqm_read_track(uft_disk_t* disk, int cyl, int head, uft_track_t* track)
{
    cqm_data_t* p = disk->plugin_data;
    if (!p || !p->data)
        return UFT_ERR_INVALID_STATE;
    if (cyl < 0 || head < 0 || cyl >= p->cyls || head >= p->heads)
        return UFT_ERR_INVALID_ARG;

    uft_track_init(track, cyl, head);

    size_t off = ((size_t)cyl * p->heads + (size_t)head) * p->spt * p->sec_size;
    for (uint16_t s = 0; s < p->spt; s++, off += p->sec_size) {
        /* Sectors past the end of the decoded stream were never in the file.
         * They are left out rather than handed over as fill bytes. */
        if (off + p->sec_size > p->decoded)
            break;
        uft_format_add_sector(track, (uint8_t)(p->sector_base + s),
                              &p->data[off], p->sec_size,
                              (uint8_t)cyl, (uint8_t)head);
    }
    return UFT_OK;
}

/* Write track: modifies the decompressed CQM buffer in memory.
 * The original CQM file is NOT modified (re-compression not supported).
 * This enables format conversion workflows (read CQM -> modify -> write as IMG). */
static uft_error_t cqm_write_track(uft_disk_t* disk, int cyl, int head,
                                   const uft_track_t* track)
{
    cqm_data_t* p = disk->plugin_data;
    if (!p || !p->data)
        return UFT_ERR_INVALID_STATE;
    if (disk->read_only)
        return UFT_ERR_NOT_SUPPORTED;
    if (cyl < 0 || head < 0 || cyl >= p->cyls || head >= p->heads)
        return UFT_ERR_INVALID_ARG;

    size_t trk_off = ((size_t)cyl * p->heads + (size_t)head) * p->spt * p->sec_size;
    for (size_t s = 0; s < track->sector_count && s < p->spt; s++) {
        size_t soff = trk_off + s * p->sec_size;
        if (soff + p->sec_size > p->size)
            break;

        const uint8_t* data = track->sectors[s].data;
        size_t len = track->sectors[s].data_len;
        if (!data || len == 0) {
            memset(p->data + soff, p->fill, p->sec_size);
            continue;
        }
        if (len > p->sec_size)
            len = p->sec_size;
        memcpy(p->data + soff, data, len);
        if (len < p->sec_size)
            memset(p->data + soff + len, p->fill, p->sec_size - len);
    }
    if (trk_off + (size_t)p->spt * p->sec_size > p->decoded)
        p->decoded = trk_off + (size_t)p->spt * p->sec_size;
    return UFT_OK;
}

static const uft_plugin_feature_t uft_format_plugin_cqm_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_SUPPORTED, NULL },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_cqm = {
    .name = "CQM", .description = "CopyQM Compressed", .extensions = "cqm",
    .format = UFT_FORMAT_CQM, .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe = cqm_probe, .open = cqm_open, .close = cqm_close,
    .read_track = cqm_read_track, .write_track = cqm_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_REVERSE_ENGINEERED,  /* CopyQM (Sydex) proprietary; layout RE'd by LibDsk/rio, cross-checked vs SAMdisk */
    .features = uft_format_plugin_cqm_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_cqm_features) / sizeof(uft_format_plugin_cqm_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(cqm)
