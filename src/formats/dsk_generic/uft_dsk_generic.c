/**
 * @file uft_dsk_generic.c
 * @brief Generic Raw DSK Plugin — serves 50+ computer-specific DSK variants
 *
 * Many vintage computers used headerless raw sector dump images with
 * the .dsk extension but different geometries. Instead of 50 separate
 * parsers, this single plugin handles all of them via a geometry table.
 *
 * The probe function uses file size to match known geometries.
 * Each variant is registered as a separate format plugin.
 */

#include "uft/uft_format_common.h"

/* ============================================================================
 * Geometry table — one entry per computer/variant
 * ============================================================================ */

typedef struct {
    const char *name;           /* Plugin name */
    const char *description;    /* Human-readable description */
    const char *extensions;     /* File extensions */
    uint8_t     cylinders;
    uint8_t     heads;
    uint8_t     spt;            /* Sectors per track */
    uint16_t    sector_size;
    uint32_t    expected_size;  /* Expected file size (0 = calculate) */
} dsk_geometry_t;

static const dsk_geometry_t dsk_geometries[] = {
    /* Computer-specific DSK variants */
    {"DSK_FM7",   "Fujitsu FM-7",          "dsk;d77",   40, 2, 16, 256, 327680},
    {"DSK_MSX",   "MSX",                   "dsk",       80, 2, 9,  512, 737280},
    {"DSK_PCW",   "Amstrad PCW",           "dsk",       80, 1, 9,  512, 368640},
    {"DSK_CG",    "SHARP MZ (CG)",         "dsk",       40, 2, 16, 256, 327680},
    {"DSK_BK",    "Elektronika BK-0010",   "dsk;bkd",   80, 2, 10, 512, 819200},
    {"DSK_KC",    "Robotron KC85",         "dsk",       80, 2, 5,  1024, 819200},
    {"DSK_MTX",   "Memotech MTX",          "dsk",       40, 2, 16, 256, 327680},
    {"DSK_NAS",   "Nascom",                "dsk",       77, 1, 16, 256, 315392},
    {"DSK_SC3",   "SHARP SC-3000",         "dsk;sc",    40, 1, 16, 256, 163840},
    {"DSK_SV",    "Spectravideo SVI",      "dsk",       40, 1, 18, 128, 92160},
    {"DSK_VEC",   "Vectrex (BIOS disk)",   "dsk",       40, 1, 18, 128, 92160},
    {"DSK_NB",    "Bondwell (NB)",         "dsk",       40, 2, 9,  512, 368640},
    {"DSK_SMC",   "SMC-777",               "dsk",       70, 2, 16, 256, 573440},
    {"DSK_UNI",   "Universal DSK",         "dsk",       80, 2, 9,  512, 737280},
    {"DSK_AQ",    "Mattel Aquarius",       "dsk",       40, 1, 16, 256, 163840},
    {"DSK_EMU",   "Emulator Generic",      "dsk;img",   80, 2, 9,  512, 737280},
    {"DSK_EQX",   "Equinox",              "dsk",       80, 2, 10, 512, 819200},
    {"DSK_KRG",   "Kreigsmarine",          "dsk",       40, 2, 9,  512, 368640},
    {"DSK_PX",    "Epson PX-8",            "dsk",       40, 1, 8,  512, 163840},
    {"DSK_ALN",   "Alphatronic PC",        "dsk",       40, 2, 16, 256, 327680},
    {"DSK_M5",    "Sord M5",               "dsk",       40, 1, 16, 256, 163840},
    {"DSK_LYN",   "Camputers Lynx",        "dsk",       40, 1, 10, 512, 204800},
    {"DSK_TOK",   "Toshiba T100",          "dsk",       40, 2, 9,  512, 368640},
    {"DSK_RLD",   "Roland DSK",            "dsk",       77, 2, 8,  1024, 1261568},
    {"DSK_FP",    "Epson FP-1100",         "dsk",       40, 2, 16, 256, 327680},
    {"DSK_AK",    "AKI Keyboard",          "dsk",       40, 1, 16, 256, 163840},
    {"DSK_EIN",   "Einstein TC-01",        "dsk",       40, 1, 10, 512, 204800},
    {"DSK_AGT",   "AGT",                   "dsk",       40, 2, 9,  512, 368640},
    {"DSK_NEC",   "NEC PC-6001",           "dsk",       40, 2, 16, 256, 327680},
    {"DSK_RC",    "RC702/Piccoline",       "dsk",       77, 2, 16, 256, 634880},
    {"DSK_SAN",   "Sanyo MBC-55x",         "dsk",       40, 2, 8,  512, 327680},
    {"DSK_X820",  "Xerox 820",             "dsk",       77, 1, 26, 128, 256256},
    {"DSK_BW",    "Bondwell 12/14",        "dsk",       40, 1, 9,  512, 184320},
    {"DSK_XM",    "Sharp X1 (XM)",         "dsk;2d",    40, 2, 16, 256, 327680},
    {"DSK_VT",    "DEC VT180",             "dsk",       40, 1, 10, 512, 204800},
    {"DSK_MZ",    "SHARP MZ-800",          "dsk",       80, 2, 16, 256, 655360},
    {"DSK_OLI",   "Olivetti M20",          "dsk",       35, 2, 16, 256, 286720},
    {"DSK_CRO",   "Cromemco",              "dsk",       77, 1, 18, 128, 177408},
    {"DSK_WNG",   "Wang",                  "dsk",       77, 2, 26, 128, 512512},
    {"DSK_HK",    "Hong Kong Computer",    "dsk",       40, 2, 16, 256, 327680},
    {"DSK_HP",    "HP LIF",                "dsk",       77, 2, 16, 256, 634880},
    {"DSK_ACE",   "Jupiter Ace",           "dsk;ace",   40, 1, 10, 512, 204800},
    {"DSK_FLEX",  "Flex OS",               "dsk",       80, 1, 36, 256, 737280},
    {"DSK_NS",    "North Star",            "dsk",       35, 1, 10, 512, 179200},
    {"DSK_VIC",   "Commodore VIC-1540",    "dsk",       35, 1, 17, 256, 152320},
    {"DSK_OS9",   "OS-9",                  "dsk",       80, 2, 18, 256, 737280},
    {"DSK_ORC",   "Oric Microdisc",        "dsk;ort",   80, 2, 17, 256, 696320},
    {"DSK_DC42",  "DiskCopy variant",      "dsk",       80, 2, 9,  512, 737280},
    {"DSK_P3",    "Spectrum +3",           "dsk",       40, 1, 9,  512, 184320},
    {NULL, NULL, NULL, 0, 0, 0, 0, 0}
};

#define DSK_GEOM_COUNT (sizeof(dsk_geometries)/sizeof(dsk_geometries[0]) - 1)

/* ============================================================================
 * Plugin data
 * ============================================================================ */

typedef struct {
    FILE*                   file;
    const dsk_geometry_t*   geom;
} dsk_gen_data_t;

/* ============================================================================
 * Shared implementation
 * ============================================================================ */

static bool dsk_gen_probe_idx(int idx, const uint8_t *data, size_t size,
                               size_t file_size, int *confidence)
{
    (void)data; (void)size;
    const dsk_geometry_t *g = &dsk_geometries[idx];
    uint32_t expected = g->expected_size;
    if (expected == 0)
        expected = (uint32_t)g->cylinders * g->heads * g->spt * g->sector_size;

    if (file_size == expected) {
        *confidence = 40;  /* Size-only match, weak */
        return true;
    }
    return false;
}

static uft_error_t dsk_gen_open_idx(int idx, uft_disk_t *disk,
                                     const char *path, bool read_only)
{
    const dsk_geometry_t *g = &dsk_geometries[idx];
    FILE *f = fopen(path, read_only ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;

    dsk_gen_data_t *p = calloc(1, sizeof(dsk_gen_data_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }

    p->file = f;
    p->geom = g;

    disk->plugin_data = p;
    disk->geometry.cylinders = g->cylinders;
    disk->geometry.heads = g->heads;
    disk->geometry.sectors = g->spt;
    disk->geometry.sector_size = g->sector_size;
    disk->geometry.total_sectors = (uint32_t)g->cylinders * g->heads * g->spt;
    return UFT_OK;
}

static void dsk_gen_close(uft_disk_t *disk)
{
    dsk_gen_data_t *p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

static uft_error_t dsk_gen_read_track(uft_disk_t *disk, int cyl, int head,
                                       uft_track_t *track)
{
    dsk_gen_data_t *p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    const dsk_geometry_t *g = p->geom;
    if (cyl >= g->cylinders || head >= g->heads) return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    long offset = (long)(((uint32_t)cyl * g->heads + head) * g->spt * g->sector_size);
    if (fseek(p->file, offset, SEEK_SET) != 0) return UFT_ERROR_IO;

    uint8_t buf[1024];
    for (int s = 0; s < g->spt; s++) {
        if (fread(buf, 1, g->sector_size, p->file) != g->sector_size)
            return UFT_ERROR_IO;
        uft_format_add_sector(track, (uint8_t)s, buf, g->sector_size,
                              (uint8_t)cyl, (uint8_t)head);
    }
    return UFT_OK;
}

/* ============================================================================
 * Macro to generate plugin instances from the geometry table
 *
 * Each DSK variant gets its own probe/open that index into the table.
 * This avoids 50 nearly-identical source files.
 * ============================================================================ */

#define DSK_PLUGIN(INDEX, SUFFIX) \
    static bool dsk_probe_##SUFFIX(const uint8_t *d, size_t s, size_t fs, int *c) \
        { return dsk_gen_probe_idx(INDEX, d, s, fs, c); } \
    static uft_error_t dsk_open_##SUFFIX(uft_disk_t *disk, const char *path, bool ro) \
        { return dsk_gen_open_idx(INDEX, disk, path, ro); } \
    const uft_format_plugin_t uft_format_plugin_dsk_##SUFFIX = { \
        .name = dsk_geometries[INDEX].name, \
        .description = dsk_geometries[INDEX].description, \
        .extensions = dsk_geometries[INDEX].extensions, \
        .format = UFT_FORMAT_DSK, \
        .capabilities = UFT_FORMAT_CAP_READ, \
        .probe = dsk_probe_##SUFFIX, \
        .open = dsk_open_##SUFFIX, \
        .close = dsk_gen_close, \
        .read_track = dsk_gen_read_track, \
    };

/* Generate all 49 DSK variant plugins */
DSK_PLUGIN(0,  fm7)    DSK_PLUGIN(1,  msx)    DSK_PLUGIN(2,  pcw)
DSK_PLUGIN(3,  cg)     DSK_PLUGIN(4,  bk)     DSK_PLUGIN(5,  kc)
DSK_PLUGIN(6,  mtx)    DSK_PLUGIN(7,  nas)    DSK_PLUGIN(8,  sc3)
DSK_PLUGIN(9,  sv)     DSK_PLUGIN(10, vec)    DSK_PLUGIN(11, nb)
DSK_PLUGIN(12, smc)    DSK_PLUGIN(13, uni)    DSK_PLUGIN(14, aq)
DSK_PLUGIN(15, emu)    DSK_PLUGIN(16, eqx)    DSK_PLUGIN(17, krg)
DSK_PLUGIN(18, px)     DSK_PLUGIN(19, aln)    DSK_PLUGIN(20, m5)
DSK_PLUGIN(21, lyn)    DSK_PLUGIN(22, tok)    DSK_PLUGIN(23, rld)
DSK_PLUGIN(24, fp)     DSK_PLUGIN(25, ak)     DSK_PLUGIN(26, ein)
DSK_PLUGIN(27, agt)    DSK_PLUGIN(28, nec)    DSK_PLUGIN(29, rc)
DSK_PLUGIN(30, san)    DSK_PLUGIN(31, x820)   DSK_PLUGIN(32, bw)
DSK_PLUGIN(33, xm)     DSK_PLUGIN(34, vt)     DSK_PLUGIN(35, mz)
DSK_PLUGIN(36, oli)    DSK_PLUGIN(37, cro)    DSK_PLUGIN(38, wng)
DSK_PLUGIN(39, hk)     DSK_PLUGIN(40, hp)     DSK_PLUGIN(41, ace)
DSK_PLUGIN(42, flex)   DSK_PLUGIN(43, ns)     DSK_PLUGIN(44, vic)
DSK_PLUGIN(45, os9)    DSK_PLUGIN(46, orc)    DSK_PLUGIN(47, dc42v)
DSK_PLUGIN(48, p3)
