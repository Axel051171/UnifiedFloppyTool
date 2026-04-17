/**
 * @file uft_d88.c
 * @brief NEC PC-88/98 D88 format core
 * @version 3.8.0
 */
#include "uft/uft_format_common.h"

#define D88_HEADER 0x2B0

typedef struct { FILE* file; uint8_t media; uint32_t track_off[164]; } d88_data_t;

bool d88_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    if (size < D88_HEADER) return false;
    uint32_t dsz = uft_read_le32(data + 0x1C);
    uint8_t media = data[0x1B];
    if (dsz <= file_size && (media == 0 || media == 0x10 || media == 0x20)) {
        *confidence = 90; return true;
    }
    return false;
}

static uft_error_t d88_open(uft_disk_t* disk, const char* path, bool read_only) {
    FILE* f = fopen(path, read_only ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    
    uint8_t hdr[D88_HEADER];
    if (fread(hdr, 1, D88_HEADER, f) != D88_HEADER) { fclose(f); return UFT_ERROR_IO; }
    d88_data_t* p = calloc(1, sizeof(d88_data_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f;
    p->media = hdr[0x1B];
    for (int i = 0; i < 164; i++) p->track_off[i] = uft_read_le32(&hdr[0x20 + i*4]);
    
    disk->plugin_data = p;
    disk->geometry.cylinders = (p->media == 0x20) ? 77 : 80;
    disk->geometry.heads = 2;
    disk->geometry.sectors = (p->media == 0x20) ? 8 : 16;
    disk->geometry.sector_size = (p->media == 0x20) ? 1024 : 256;
    disk->geometry.total_sectors = (uint32_t)disk->geometry.cylinders * disk->geometry.heads * disk->geometry.sectors;

    /* Read actual geometry from first track's sector headers rather than
     * relying solely on the media type byte (which is often wrong for
     * non-standard or custom-formatted disks). */
    (void)fseek(f, 0, SEEK_END);  /* best-effort geometry detection */
    long file_size = ftell(f);
    if (p->track_off[0] > 0 && p->track_off[0] < (uint32_t)file_size - 16) {
        uint8_t trk_hdr[16];
        if (fseek(f, p->track_off[0], SEEK_SET) == 0 &&
            fread(trk_hdr, 1, 16, f) == 16) {
            uint16_t sec_size  = trk_hdr[14] | ((uint16_t)trk_hdr[15] << 8);
            uint16_t sec_count = trk_hdr[4]  | ((uint16_t)trk_hdr[5] << 8);
            if (sec_size > 0 && sec_size <= 8192 && sec_count > 0 && sec_count <= 64) {
                disk->geometry.sector_size = sec_size;
                disk->geometry.sectors = sec_count;
            }
        }
    }

    return UFT_OK;
}

static void d88_close(uft_disk_t* disk) {
    d88_data_t* p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

static uft_error_t d88_read_track(uft_disk_t* disk, int cyl, int head, uft_track_t* track) {
    d88_data_t* p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    
    int idx = cyl * 2 + head;
    if (idx >= 164 || p->track_off[idx] == 0) return UFT_ERROR_INVALID_ARG;
    
    uft_track_init(track, cyl, head);
    if (fseek(p->file, p->track_off[idx], SEEK_SET) != 0) { return UFT_ERROR_INVALID_ARG; }
    uint8_t sec_hdr[16];
    for (int s = 0; s < disk->geometry.sectors; s++) {
        if (fread(sec_hdr, 1, 16, p->file) != 16) break;
        uint16_t dsize = uft_read_le16(&sec_hdr[14]);
        if (dsize == 0 || dsize > 8192) break;
        
        uint8_t* buf = malloc(dsize);
        if (!buf) break;
        if (fread(buf, 1, dsize, p->file) != dsize) { free(buf); break; }
        uft_format_add_sector(track, sec_hdr[2] - 1, buf, dsize, cyl, head);
        /* D88 sector status byte: sec_hdr[13]
         * 0x00=normal, 0x10=deleted, 0xA0=ID CRC error,
         * 0xB0=data CRC error, 0xE0=no address mark, 0xF0=no data */
        if (track->sector_count > 0) {
            uint8_t st = sec_hdr[13];
            if (st == 0xA0 || st == 0xB0)
                track->sectors[track->sector_count - 1].crc_ok = false;
            if (st == 0x10)
                track->sectors[track->sector_count - 1].deleted = true;
        }
        free(buf);
    }
    return UFT_OK;
}

static uft_error_t d88_write_track(uft_disk_t* disk, int cyl, int head,
                                    const uft_track_t* track) {
    d88_data_t* p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;

    int idx = cyl * 2 + head;
    if (idx >= 164 || p->track_off[idx] == 0) return UFT_ERROR_INVALID_ARG;

    if (fseek(p->file, p->track_off[idx], SEEK_SET) != 0) return UFT_ERROR_IO;

    uint8_t sec_hdr[16];
    for (int s = 0; s < disk->geometry.sectors; s++) {
        long hdr_pos = ftell(p->file);
        if (hdr_pos < 0) return UFT_ERROR_IO;
        if (fread(sec_hdr, 1, 16, p->file) != 16) break;
        uint16_t dsize = uft_read_le16(&sec_hdr[14]);
        if (dsize == 0 || dsize > 8192) break;

        /* Write sector data if we have a matching sector in track */
        if ((size_t)s < track->sector_count) {
            const uint8_t *data = track->sectors[s].data;
            uint8_t *pad = NULL;
            if (!data || track->sectors[s].data_len == 0) {
                pad = malloc(dsize);
                if (!pad) return UFT_ERROR_NO_MEMORY;
                memset(pad, 0xE5, dsize);
                data = pad;
            }
            if (fwrite(data, 1, dsize, p->file) != dsize) { free(pad); return UFT_ERROR_IO; }
            free(pad);
        } else {
            /* Skip past this sector's data */
            if (fseek(p->file, (long)dsize, SEEK_CUR) != 0) return UFT_ERROR_IO;
        }
    }
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_d88 = {
    .name = "D88", .description = "PC-88/PC-98", .extensions = "d88;88d;d98",
    .format = UFT_FORMAT_DSK, .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe = d88_probe, .open = d88_open, .close = d88_close,
    .read_track = d88_read_track, .write_track = d88_write_track,
    .verify_track = uft_generic_verify_track,
};
UFT_REGISTER_FORMAT_PLUGIN(d88)
