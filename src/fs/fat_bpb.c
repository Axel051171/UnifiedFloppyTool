#include "uft/fs/fat_bpb.h"
#include <string.h>

static uint16_t rd_le16(const uint8_t *p){ return (uint16_t)(p[0] | (p[1]<<8)); }
static uint32_t rd_le32(const uint8_t *p){ return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24); }

static int is_pow2_u16(uint16_t x){ return x && ((x & (x-1))==0); }
static int is_pow2_u8(uint8_t x){ return x && ((x & (x-1))==0); }

uft_rc_t uft_fat_bpb_parse(const uint8_t *s, size_t n, uft_fat_bpb_t *out, uft_diag_t *diag)
{
    if (!s || !out) { uft_diag_set(diag, "fat: invalid args"); return UFT_EINVAL; }
    memset(out, 0, sizeof(*out));
    if (n < 512) { uft_diag_set(diag, "fat: need >=512 bytes"); return UFT_EINVAL; }

    /* Minimal FAT boot sector signature is 0x55AA at bytes 510-511. */
    if (!(s[510] == 0x55 && s[511] == 0xAA)) {
        uft_diag_set(diag, "fat: missing 0x55AA signature");
        return UFT_EFORMAT;
    }

    out->bytes_per_sector = rd_le16(s + 11);
    out->sectors_per_cluster = s[13];
    out->reserved_sectors = rd_le16(s + 14);
    out->fats = s[16];
    out->root_entries = rd_le16(s + 17);
    uint16_t ts16 = rd_le16(s + 19);
    uint32_t ts32 = rd_le32(s + 32);
    out->total_sectors = ts16 ? (uint32_t)ts16 : ts32;

    uint16_t spf16 = rd_le16(s + 22);
    uint32_t spf32 = rd_le32(s + 36);
    out->sectors_per_fat = spf16 ? (uint32_t)spf16 : spf32;

    out->sectors_per_track = rd_le16(s + 24);
    out->heads = rd_le16(s + 26);
    out->hidden_sectors = rd_le32(s + 28);
    out->root_cluster = spf16 ? 0 : rd_le32(s + 44);

    out->fat_type = UFT_FAT_UNKNOWN;
    out->confidence = 30; /* base if signature present */
    uft_diag_set(diag, "fat: bpb parsed");
    return UFT_OK;
}

uft_rc_t uft_fat_bpb_validate_and_layout(const uft_fat_bpb_t *b, uft_fat_layout_t *lay, uft_diag_t *diag)
{
    if (!b || !lay) { uft_diag_set(diag, "fat: invalid args"); return UFT_EINVAL; }
    memset(lay, 0, sizeof(*lay));

    if (!is_pow2_u16(b->bytes_per_sector) || b->bytes_per_sector < 512 || b->bytes_per_sector > 4096) {
        uft_diag_set(diag, "fat: bytes_per_sector invalid");
        return UFT_EFORMAT;
    }
    if (!is_pow2_u8(b->sectors_per_cluster) || b->sectors_per_cluster == 0) {
        uft_diag_set(diag, "fat: sectors_per_cluster invalid");
        return UFT_EFORMAT;
    }
    if (b->reserved_sectors == 0) { uft_diag_set(diag, "fat: reserved_sectors invalid"); return UFT_EFORMAT; }
    if (b->fats == 0 || b->fats > 4) { uft_diag_set(diag, "fat: fats count invalid"); return UFT_EFORMAT; }
    if (b->total_sectors < 16) { uft_diag_set(diag, "fat: total_sectors too small"); return UFT_EFORMAT; }
    if (b->sectors_per_fat == 0) { uft_diag_set(diag, "fat: sectors_per_fat invalid"); return UFT_EFORMAT; }

    /* Root dir sectors (FAT12/16). For FAT32, root_entries is 0 and root is cluster chain. */
    uint32_t root_sectors = (uint32_t)(((uint32_t)b->root_entries * 32u) + (b->bytes_per_sector - 1u)) / b->bytes_per_sector;
    uint32_t fat_sectors = (uint32_t)b->fats * b->sectors_per_fat;

    uint32_t lba_fat0 = (uint32_t)b->reserved_sectors;
    uint32_t lba_root = lba_fat0 + fat_sectors;
    uint32_t lba_data = lba_root + root_sectors;

    if (lba_data >= b->total_sectors) { uft_diag_set(diag, "fat: layout exceeds total sectors"); return UFT_EFORMAT; }

    uint32_t data_sectors = b->total_sectors - lba_data;
    uint32_t cluster_count = data_sectors / b->sectors_per_cluster;

    /* FAT type threshold per Microsoft (common rule): */
    uft_fat_type_t ft = UFT_FAT_UNKNOWN;
    if (cluster_count < 4085) ft = UFT_FAT12;
    else if (cluster_count < 65525) ft = UFT_FAT16;
    else ft = UFT_FAT32;

    /* FAT32 sanity: root_entries typically 0 and root_cluster >=2 */
    if (ft == UFT_FAT32) {
        if (b->root_cluster < 2) { uft_diag_set(diag, "fat: FAT32 but root_cluster invalid"); return UFT_EFORMAT; }
    } else {
        if (b->root_entries == 0) { uft_diag_set(diag, "fat: FAT12/16 but root_entries=0"); return UFT_EFORMAT; }
    }

    lay->lba_fat0 = lba_fat0;
    lay->lba_root = lba_root;
    lay->lba_data = lba_data;
    lay->fat_sectors = fat_sectors;
    lay->root_sectors = root_sectors;
    lay->data_sectors = data_sectors;
    lay->cluster_count = cluster_count;

    /* confidence scoring: geometry hints improve, common floppy sizes improve */
    uft_diag_set(diag, "fat: layout ok");
    return UFT_OK;
}

uft_rc_t uft_fat_probe_image(const uint8_t *img, size_t len, uft_fat_bpb_t *bpb, uft_fat_layout_t *layout, uft_diag_t *diag)
{
    if (!img || !bpb || !layout) { uft_diag_set(diag, "fat: invalid args"); return UFT_EINVAL; }
    if (len < 512) { uft_diag_set(diag, "fat: image too small"); return UFT_EFORMAT; }

    uft_rc_t rc = uft_fat_bpb_parse(img, len, bpb, diag);
    if (rc != UFT_OK) return rc;
    rc = uft_fat_bpb_validate_and_layout(bpb, layout, diag);
    if (rc != UFT_OK) return rc;

    /* set fat_type based on computed cluster count */
    uint32_t clusters = layout->cluster_count;
    if (clusters < 4085) bpb->fat_type = UFT_FAT12;
    else if (clusters < 65525) bpb->fat_type = UFT_FAT16;
    else bpb->fat_type = UFT_FAT32;

    int conf = 60;
    if (bpb->sectors_per_track && bpb->heads) conf += 5;
    /* common floppy total sectors */
    if (bpb->total_sectors == 1440 || bpb->total_sectors == 2880 || bpb->total_sectors == 2400 || bpb->total_sectors == 720 || bpb->total_sectors == 5760)
        conf += 10;
    if (conf > 100) conf = 100;
    bpb->confidence = conf;

    uft_diag_set(diag, "fat: probed");
    return UFT_OK;
}
