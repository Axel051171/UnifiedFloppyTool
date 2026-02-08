/**
 * uft_mfm_detect_bridge.c — Bridge between MFM Detect Module and UFT
 * ====================================================================
 */

#include "mfm_detect.h"
#include "cpm_fs.h"
#include "uft_mfm_detect_bridge.h"

#include <stdlib.h>
#include <string.h>

#define BRIDGE_VERSION "1.0.0"

/* ================================================================== */
/*  Internal: image-backed sector reader                               */
/* ================================================================== */

typedef struct {
    const uint8_t *data;
    size_t         size;
    uint16_t       sector_size;
    uint8_t        sectors_per_track;
    uint8_t        heads;
    uint8_t        first_sector;
} image_reader_ctx_t;

static mfm_error_t image_reader(void *ctx, uint16_t cyl, uint8_t head,
                                 uint8_t sector, uint8_t *buf,
                                 uint16_t *bytes_read) {
    image_reader_ctx_t *r = (image_reader_ctx_t *)ctx;
    if (!r || !buf) return MFM_ERR_NULL_PARAM;
    uint8_t sec_idx = (uint8_t)(sector - r->first_sector);
    uint32_t lba = (uint32_t)((cyl * r->heads + head) *
                    r->sectors_per_track + sec_idx);
    size_t offset = (size_t)lba * r->sector_size;
    if (offset + r->sector_size > r->size)
        return MFM_ERR_READ_FAILED;
    memcpy(buf, r->data + offset, r->sector_size);
    if (bytes_read) *bytes_read = r->sector_size;
    return MFM_OK;
}

/* ================================================================== */
/*  Internal: callback-backed sector reader                            */
/* ================================================================== */

typedef struct {
    uft_mfmd_read_fn  user_fn;
    void             *user_ctx;
} live_reader_ctx_t;

static mfm_error_t live_reader(void *ctx, uint16_t cyl, uint8_t head,
                                uint8_t sector, uint8_t *buf,
                                uint16_t *bytes_read) {
    live_reader_ctx_t *lr = (live_reader_ctx_t *)ctx;
    if (!lr || !lr->user_fn) return MFM_ERR_NULL_PARAM;
    int rc = lr->user_fn(lr->user_ctx, cyl, head, sector, buf, bytes_read);
    return (rc == 0) ? MFM_OK : MFM_ERR_READ_FAILED;
}

/* ================================================================== */
/*  Internal: populate info from detection result                      */
/* ================================================================== */

static void populate_info(uft_mfm_detect_info_t *info,
                          mfm_detect_result_t *result) {
    memset(info, 0, sizeof(*info));
    info->detail = (mfm_detect_handle_t)result;

    info->sector_size       = result->physical.sector_size;
    info->sectors_per_track = result->physical.sectors_per_track;
    info->heads             = result->physical.heads;
    info->cylinders         = result->physical.cylinders;
    info->disk_size         = result->physical.disk_size;
    info->geometry_name     = mfm_geometry_str(result->physical.geometry);
    info->encoding_name     = mfm_encoding_str(result->physical.encoding);
    info->has_boot_sector   = result->has_boot_sector;
    info->num_candidates    = result->num_candidates;

    if (result->num_candidates > 0) {
        const format_candidate_t *best = &result->candidates[0];
        info->fs_name      = best->description;
        info->system_name  = best->system_name;
        info->confidence   = best->confidence;

        mfm_fs_type_t fs = best->fs_type;
        info->is_fat   = (fs >= MFM_FS_FAT12_DOS && fs <= MFM_FS_FAT16);
        info->is_amiga = (fs >= MFM_FS_AMIGA_OFS && fs <= MFM_FS_AMIGA_PFS);
        info->is_cpm   = (fs >= MFM_FS_CPM_22 && fs <= MFM_FS_CPM_GENERIC);
    } else {
        info->fs_name     = "Unknown";
        info->system_name = "Unknown";
        info->confidence  = 0;
    }
}

/* ================================================================== */
/*  Public: Image detection                                            */
/* ================================================================== */

uft_mfmd_error_t uft_mfmd_detect_image(const uint8_t *data, size_t size,
                                         uft_mfm_detect_info_t *info) {
    if (!data || !info) return UFT_MFMD_ERR_NULL;
    if (size < 512) return UFT_MFMD_ERR_TOO_SMALL;

    mfm_detect_result_t *result = mfm_detect_create();
    if (!result) return UFT_MFMD_ERR_NOMEM;

    typedef struct { size_t sz; uint16_t ss; uint8_t spt; uint8_t h;
                     uint16_t c; uint8_t fs; } sz_map_t;
    static const sz_map_t sizes[] = {
        {  163840, 512,  8, 1, 40, 1 },
        {  184320, 512,  9, 1, 40, 1 },
        {  327680, 512,  8, 2, 40, 1 },
        {  368640, 512,  9, 2, 40, 1 },
        {  737280, 512,  9, 2, 80, 1 },
        {  819200, 512, 10, 2, 80, 1 },
        {  901120, 512, 11, 2, 80, 0 },
        { 1228800, 512, 15, 2, 80, 1 },
        { 1474560, 512, 18, 2, 80, 1 },
        { 1802240, 512, 22, 2, 80, 0 },
        { 2949120, 512, 36, 2, 80, 1 },
    };
    int found = 0;
    for (size_t i = 0; i < sizeof(sizes)/sizeof(sizes[0]); i++) {
        if (size == sizes[i].sz) {
            mfm_detect_set_physical(result, sizes[i].ss, sizes[i].spt,
                                     sizes[i].h, sizes[i].c, sizes[i].fs);
            found = 1;
            break;
        }
    }
    if (!found) {
        uint32_t ts = (uint32_t)(size / 512);
        if (ts > 0 && size % 512 == 0) {
            uint8_t spt = (uint8_t)(ts / 160);
            if (spt >= 5 && spt <= 36) {
                mfm_detect_set_physical(result, 512, spt, 2, 80, 1);
                found = 1;
            }
        }
        if (!found)
            mfm_detect_set_physical(result, 512, 9, 2, 80, 1);
    }

    image_reader_ctx_t rctx = {
        .data = data, .size = size,
        .sector_size = result->physical.sector_size,
        .sectors_per_track = result->physical.sectors_per_track,
        .heads = result->physical.heads,
        .first_sector = result->physical.min_sector_id
    };
    mfm_detect_set_reader(result, image_reader, &rctx);

    mfm_detect_analyze_boot(result);
    mfm_detect_analyze_filesystem(result);
    mfm_sort_candidates(result);
    populate_info(info, result);

    return UFT_MFMD_OK;
}

/* ================================================================== */
/*  Public: Live detection via callback                                */
/* ================================================================== */

uft_mfmd_error_t uft_mfmd_detect_live(uft_mfmd_read_fn reader, void *ctx,
                                        uint16_t sector_size,
                                        uint8_t sectors_per_track,
                                        uint8_t heads, uint16_t cylinders,
                                        uft_mfm_detect_info_t *info) {
    if (!reader || !info) return UFT_MFMD_ERR_NULL;

    mfm_detect_result_t *result = mfm_detect_create();
    if (!result) return UFT_MFMD_ERR_NOMEM;

    mfm_detect_set_physical(result, sector_size, sectors_per_track,
                             heads, cylinders, 1);
    live_reader_ctx_t lctx = { reader, ctx };
    mfm_detect_set_reader(result, live_reader, &lctx);

    mfm_detect_full(result);
    mfm_sort_candidates(result);
    populate_info(info, result);
    return UFT_MFMD_OK;
}

/* ================================================================== */
/*  Public: Boot sector only (quick mode)                              */
/* ================================================================== */

uft_mfmd_error_t uft_mfmd_detect_boot(const uint8_t *boot_sector,
                                        uint16_t size,
                                        uint16_t sector_size,
                                        uint8_t sectors_per_track,
                                        uint8_t heads, uint16_t cylinders,
                                        uft_mfm_detect_info_t *info) {
    if (!boot_sector || !info) return UFT_MFMD_ERR_NULL;
    if (size < 256) return UFT_MFMD_ERR_TOO_SMALL;

    mfm_detect_result_t *result = mfm_detect_create();
    if (!result) return UFT_MFMD_ERR_NOMEM;

    mfm_detect_set_physical(result, sector_size, sectors_per_track,
                             heads, cylinders, 1);
    mfm_detect_analyze_boot_data(result, boot_sector, size);
    mfm_sort_candidates(result);
    populate_info(info, result);
    return UFT_MFMD_OK;
}

/* ================================================================== */
/*  Public: Result access                                              */
/* ================================================================== */

bool uft_mfmd_get_candidate(const uft_mfm_detect_info_t *info, uint8_t index,
                              const char **fs_name, const char **system_name,
                              uint8_t *confidence) {
    if (!info || !info->detail || index >= info->num_candidates)
        return false;

    mfm_detect_result_t *r = (mfm_detect_result_t *)info->detail;
    const format_candidate_t *c = &r->candidates[index];
    if (fs_name)     *fs_name     = c->description;
    if (system_name) *system_name = c->system_name;
    if (confidence)  *confidence  = c->confidence;
    return true;
}

void uft_mfmd_print_report(const uft_mfm_detect_info_t *info,
                             void *file_handle) {
    if (!info || !info->detail || !file_handle) return;
    mfm_detect_print_report((const mfm_detect_result_t *)info->detail,
                             (FILE *)file_handle);
}

void uft_mfmd_free(uft_mfm_detect_info_t *info) {
    if (!info) return;
    if (info->detail) {
        mfm_detect_free((mfm_detect_result_t *)info->detail);
        info->detail = NULL;
    }
    memset(info, 0, sizeof(*info));
}

/* ================================================================== */
/*  Internal: CP/M image I/O                                           */
/* ================================================================== */

typedef struct {
    uint8_t  *data;
    size_t    size;
    uint16_t  sector_size;
    uint8_t   spt;
    uint8_t   heads;
    uint8_t   first_sector;
} cpm_img_ctx_t;

static cpm_error_t cpm_img_read(void *ctx, uint16_t cyl, uint8_t head,
                                 uint8_t sector, uint8_t *buf,
                                 uint16_t *bytes_read) {
    cpm_img_ctx_t *c = (cpm_img_ctx_t *)ctx;
    uint8_t si = (uint8_t)(sector - c->first_sector);
    size_t off = (size_t)((cyl * c->heads + head) * c->spt + si)
                 * c->sector_size;
    if (off + c->sector_size > c->size) return CPM_ERR_IO;
    memcpy(buf, c->data + off, c->sector_size);
    if (bytes_read) *bytes_read = c->sector_size;
    return CPM_OK;
}

static cpm_error_t cpm_img_write(void *ctx, uint16_t cyl, uint8_t head,
                                  uint8_t sector, const uint8_t *buf,
                                  uint16_t sz) {
    cpm_img_ctx_t *c = (cpm_img_ctx_t *)ctx;
    uint8_t si = (uint8_t)(sector - c->first_sector);
    size_t off = (size_t)((cyl * c->heads + head) * c->spt + si)
                 * c->sector_size;
    if (off + sz > c->size) return CPM_ERR_IO;
    memcpy(c->data + off, buf, sz);
    return CPM_OK;
}

/* ================================================================== */
/*  Public: CP/M filesystem access                                     */
/* ================================================================== */

uft_mfmd_error_t uft_mfmd_cpm_open(const uint8_t *data, size_t size,
                                     const uft_mfm_detect_info_t *info,
                                     cpm_disk_handle_t *disk_out) {
    if (!data || !info || !disk_out) return UFT_MFMD_ERR_NULL;
    if (!info->is_cpm) return UFT_MFMD_ERR_UNSUPPORTED;
    if (!info->detail) return UFT_MFMD_ERR_NO_DATA;

    mfm_detect_result_t *r = (mfm_detect_result_t *)info->detail;

    /* Find CP/M candidate */
    const format_candidate_t *cpm_cand = NULL;
    for (uint8_t i = 0; i < r->num_candidates; i++) {
        mfm_fs_type_t fs = r->candidates[i].fs_type;
        if (fs >= MFM_FS_CPM_22 && fs <= MFM_FS_CPM_GENERIC) {
            cpm_cand = &r->candidates[i];
            break;
        }
    }
    if (!cpm_cand) return UFT_MFMD_ERR_DETECT_FAIL;

    /* Image copy + I/O context */
    cpm_img_ctx_t *ictx = (cpm_img_ctx_t *)malloc(sizeof(cpm_img_ctx_t));
    if (!ictx) return UFT_MFMD_ERR_NOMEM;
    ictx->data = (uint8_t *)malloc(size);
    if (!ictx->data) { free(ictx); return UFT_MFMD_ERR_NOMEM; }
    memcpy(ictx->data, data, size);
    ictx->size         = size;
    ictx->sector_size  = info->sector_size;
    ictx->spt          = info->sectors_per_track;
    ictx->heads        = info->heads;
    ictx->first_sector = r->physical.min_sector_id;

    /* Geometry */
    cpm_geometry_t geom;
    memset(&geom, 0, sizeof(geom));
    geom.sector_size       = info->sector_size;
    geom.sectors_per_track = info->sectors_per_track;
    geom.heads             = info->heads;
    geom.cylinders         = info->cylinders;
    geom.first_sector      = r->physical.min_sector_id;

    /* Convert mfm_cpm_dpb_t → cpm_dpb_t */
    const mfm_cpm_analysis_t *ca = &cpm_cand->detail.cpm;
    cpm_dpb_t fs_dpb;
    const cpm_dpb_t *dpb = NULL;
    if (ca->dpb.is_valid) {
        memset(&fs_dpb, 0, sizeof(fs_dpb));
        fs_dpb.spt          = ca->dpb.spt;
        fs_dpb.bsh          = ca->dpb.bsh;
        fs_dpb.blm          = ca->dpb.blm;
        fs_dpb.exm          = ca->dpb.exm;
        fs_dpb.dsm          = ca->dpb.dsm;
        fs_dpb.drm          = ca->dpb.drm;
        fs_dpb.al0          = ca->dpb.al0;
        fs_dpb.al1          = ca->dpb.al1;
        fs_dpb.cks          = ca->dpb.cks;
        fs_dpb.off          = ca->dpb.off;
        fs_dpb.block_size   = ca->dpb.block_size;
        fs_dpb.dir_entries  = ca->dpb.dir_entries;
        fs_dpb.dir_blocks   = ca->dpb.dir_blocks;
        fs_dpb.disk_capacity = ca->dpb.data_capacity;
        fs_dpb.use_16bit    = (ca->dpb.dsm > 255);
        fs_dpb.al_per_ext   = fs_dpb.use_16bit ? 8 : 16;
        dpb = &fs_dpb;
    }

    cpm_disk_t *disk = cpm_open(&geom, dpb, cpm_img_read, cpm_img_write, ictx);
    if (!disk) {
        free(ictx->data);
        free(ictx);
        return UFT_MFMD_ERR_DETECT_FAIL;
    }

    cpm_read_directory(disk);
    *disk_out = (cpm_disk_handle_t)disk;
    return UFT_MFMD_OK;
}

void uft_mfmd_cpm_close(cpm_disk_handle_t disk) {
    if (disk) cpm_close((cpm_disk_t *)disk);
}

/* ================================================================== */
/*  Public: Utility                                                    */
/* ================================================================== */

const char *uft_mfmd_error_str(uft_mfmd_error_t err) {
    switch (err) {
    case UFT_MFMD_OK:              return "OK";
    case UFT_MFMD_ERR_NULL:        return "NULL parameter";
    case UFT_MFMD_ERR_NOMEM:       return "Out of memory";
    case UFT_MFMD_ERR_NO_DATA:     return "No data available";
    case UFT_MFMD_ERR_TOO_SMALL:   return "Data too small";
    case UFT_MFMD_ERR_DETECT_FAIL: return "Detection failed";
    case UFT_MFMD_ERR_UNSUPPORTED: return "Not supported for this format";
    case UFT_MFMD_ERR_IO:          return "I/O error";
    default:                       return "Unknown error";
    }
}

const char *uft_mfmd_version(void) {
    return BRIDGE_VERSION;
}
