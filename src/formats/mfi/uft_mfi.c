/**
 * @file uft_mfi.c
 * @brief MAME Floppy Image (MFI) Plugin
 *
 * MFI is MAME's native floppy disk format designed for cycle-accurate
 * floppy emulation. It stores flux-level timing data per track.
 *
 * File layout — MF-614: das hier stand vorher, war erfunden, und der Parser
 * folgte ihm:
 *
 *     0x00  8   Magic "MAMEFLOP"          <- die Kennung ist 16 Byte lang
 *     0x08  4   Form factor               <- liegt IN der Kennung
 *     0x0C  4   Reserved
 *     0x10  N*16 Track entries            <- dort liegen cyl/head_count
 *
 * Die Kennung ist ein PRAEFIX der echten. Der Leser wies echte MAME-Dateien
 * deshalb nicht ab, sondern nahm sie AN und las sie falsch. Ein Parser, der
 * abweist, ist aergerlich; einer, der annimmt und still falsch auslegt, ist
 * die Klasse, an der dieses Projekt fuenfmal verbrannt ist (FMT-2/3/10/11/12).
 *
 * Richtig, nach [1] und [2]:
 *
 *   struct header (32 Byte, = sizeof(header) = Beginn der Eintraege)
 *     0x00   16  sign[16]      "MAMEFLOPPYIMAGE\0" oder "MESSFLOPPYIMAGE\0"
 *     0x10    4  cyl_count     untere 30 Bit Zylinder, obere 2 Bit Aufloesung
 *     0x14    4  head_count
 *     0x18    4  form_factor
 *     0x1C    4  variant
 *
 *   struct entry (16 Byte), Anzahl = (cyl_count << resolution) * head_count,
 *   ab 0x20, zylinder-dur, kopf-innen:
 *     0x00    4  offset
 *     0x04    4  compressed_size
 *     0x08    4  uncompressed_size
 *     0x0C    4  write_splice
 *
 * Track entry (16 bytes):
 *   0x00    4     Offset in file (LE32)
 *   0x04    4     Compressed size (LE32)
 *   0x08    4     Uncompressed size (LE32)
 *   0x0C    4     Write splice position (LE32)
 *
 * Track data is zlib-compressed. Each flux cell is 4 bytes (LE32):
 *   Bits 0-27:  Cell size in attoseconds / 2^32 (relative to 200ms rotation)
 *   Bits 28-31: Cell type (0=normal, 1=weak)
 *
 * Since MFI stores flux data, we present it as raw track data.
 * Full flux decoding requires PLL — this plugin provides the container.
 *
 * Referenz (benannt, wie die Einfrier-Regel es verlangt):
 *   [1] MAME, src/lib/formats/mfi_dsk.h — struct header / struct entry,
 *       RESOLUTION_SHIFT = 30, CYLINDER_MASK = 0x3fffffff
 *   [2] MAME, src/lib/formats/mfi_dsk.cpp:81-82 — sign/sign_old,
 *       identify() vergleicht alle 16 Byte, load() liest die
 *       Eintraege ab sizeof(header).
 * Beide Dateien BSD-3-Clause (SPDX je Datei). Kein MAME-Code kopiert.
 * Abgesichert durch tests/test_mfi_layout.c (Rotprobe: beide
 * Zusicherungen fallen auf der alten Fassung).
 */

#include "uft/uft_format_common.h"
#include "uft/formats/mfi.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

/* MF-614: 16 Byte einschliesslich der abschliessenden Null, beide Varianten
 * (mfi_dsk.cpp:81-82). Der alte Wert 8 traf nur ein Praefix. */
/* MF-863: die Kennung steht jetzt in include/uft/formats/mfi.h —
 * eine Definition, zwei Leser. */
#define MFI_MAGIC           UFT_MFI_MAGIC
#define MFI_MAGIC_OLD       UFT_MFI_MAGIC_OLD
#define MFI_MAGIC_LEN       UFT_MFI_MAGIC_LEN
#define MFI_HEADER_SIZE     32
#define MFI_RESOLUTION_SHIFT 30
#define MFI_CYLINDER_MASK   0x3FFFFFFFu
#define MFI_TRACK_ENTRY     16
#define MFI_MAX_TRACKS      168     /* 84 cyl * 2 heads */

/* Form factors */
#define MFI_FF_35           1       /* 3.5" */
#define MFI_FF_525          2       /* 5.25" */
#define MFI_FF_8            3       /* 8" */

/* ============================================================================
 * Plugin data
 * ============================================================================ */

typedef struct {
    uint32_t    offset;
    uint32_t    compressed_size;
    uint32_t    uncompressed_size;
    uint32_t    write_splice;
} mfi_track_entry_t;

typedef struct {
    FILE*               file;
    uint32_t            form_factor;
    uint8_t             track_count;    /* actual tracks found */
    uint8_t             cylinders;
    uint8_t             heads;
    mfi_track_entry_t   tracks[MFI_MAX_TRACKS];
} mfi_data_t;

/* ============================================================================
 * probe
 * ============================================================================ */

bool mfi_probe(const uint8_t *data, size_t size, size_t file_size,
               int *confidence)
{
    (void)file_size;
    if (size < MFI_HEADER_SIZE) return false;

    /* MF-614: beide Kennungen, volle 16 Byte. identify() in mfi_dsk.cpp
     * vergleicht ebenso alle 16 — ein Praefix-Vergleich nimmt Dateien an,
     * die er nicht lesen kann. */
    if (memcmp(data, MFI_MAGIC, MFI_MAGIC_LEN) != 0 &&
        memcmp(data, MFI_MAGIC_OLD, MFI_MAGIC_LEN) != 0)
        return false;

    /* Dieselben Schranken wie identify(): Zylinder <= 84, Aufloesung < 3,
     * Koepfe <= 2. Ohne sie waere jede Datei mit passender Kennung gut. */
    uint32_t cyl_raw = uft_read_le32(data + 0x10);
    if ((cyl_raw & MFI_CYLINDER_MASK) > 84) return false;
    if ((cyl_raw >> MFI_RESOLUTION_SHIFT) >= 3) return false;
    if (uft_read_le32(data + 0x14) > 2) return false;

    uint32_t ff = uft_read_le32(data + 0x18);
    if (ff >= MFI_FF_35 && ff <= MFI_FF_8) {
        *confidence = 95;
    } else {
        *confidence = 70;
    }
    return true;
}

/* ============================================================================
 * open
 * ============================================================================ */

static uft_error_t mfi_open(uft_disk_t *disk, const char *path,
                             bool read_only)
{
    (void)read_only;
    FILE *f = fopen(path, "rb");
    if (!f) return UFT_ERROR_FILE_OPEN;

    /* Get file size */
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_IO; }
    long fs = ftell(f);
    if (fs < 0) { fclose(f); return UFT_ERROR_IO; }
    size_t file_size = (size_t)fs;
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_IO; }

    /* Read header */
    uint8_t hdr[MFI_HEADER_SIZE];
    if (fread(hdr, 1, MFI_HEADER_SIZE, f) != MFI_HEADER_SIZE) {
        fclose(f);
        return UFT_ERROR_IO;
    }

    if (memcmp(hdr, MFI_MAGIC, MFI_MAGIC_LEN) != 0) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }

    mfi_data_t *pdata = calloc(1, sizeof(mfi_data_t));
    if (!pdata) { fclose(f); return UFT_ERROR_NO_MEMORY; }

    pdata->file = f;
    /* MF-614: stand auf `hdr + 8` — mitten in der Kennung. */
    pdata->form_factor = uft_read_le32(hdr + 0x18);

    /* Geometrie kommt aus dem Kopf, nicht aus der Dateigroesse. Die
     * Eintragszahl ist (cyl_count << resolution) * head_count
     * (mfi_dsk.cpp load()). */
    uint32_t cyl_raw    = uft_read_le32(hdr + 0x10);
    uint32_t resolution = cyl_raw >> MFI_RESOLUTION_SHIFT;
    uint32_t hdr_cyls   = cyl_raw & MFI_CYLINDER_MASK;
    uint32_t hdr_heads  = uft_read_le32(hdr + 0x14);
    if (hdr_cyls > 84 || hdr_heads > 2 || resolution >= 3) {
        free(pdata); fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }
    size_t want = (size_t)(hdr_cyls << resolution) * hdr_heads;
    if (want > MFI_MAX_TRACKS) want = MFI_MAX_TRACKS;

    /* Read track entries until EOF or max */
    size_t remaining = file_size - MFI_HEADER_SIZE;
    uint8_t count = 0;
    uint8_t max_cyl = 0;
    uint8_t max_head = 0;

    /* MF-614: lief bis EOF und leitete cyl/head aus dem Laufindex ab.
     * Jetzt genau so viele Eintraege, wie der Kopf ansagt. */
    while (remaining >= MFI_TRACK_ENTRY && count < want) {
        uint8_t entry[MFI_TRACK_ENTRY];
        if (fread(entry, 1, MFI_TRACK_ENTRY, f) != MFI_TRACK_ENTRY)
            break;

        pdata->tracks[count].offset = uft_read_le32(entry);
        pdata->tracks[count].compressed_size = uft_read_le32(entry + 4);
        pdata->tracks[count].uncompressed_size = uft_read_le32(entry + 8);
        pdata->tracks[count].write_splice = uft_read_le32(entry + 12);

        /* End marker: offset 0 and size 0 */
        if (pdata->tracks[count].offset == 0 &&
            pdata->tracks[count].compressed_size == 0)
            break;

        /* Track index → cyl/head */
        uint8_t c = count / 2;
        uint8_t h = count % 2;
        if (c > max_cyl) max_cyl = c;
        if (h > max_head) max_head = h;

        count++;
        remaining -= MFI_TRACK_ENTRY;
    }

    pdata->track_count = count;
    pdata->cylinders = max_cyl + 1;
    pdata->heads = max_head + 1;

    if (count == 0) {
        free(pdata);
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }

    disk->plugin_data = pdata;
    disk->geometry.cylinders = pdata->cylinders;
    disk->geometry.heads = pdata->heads;
    disk->geometry.sectors = 1;     /* flux = 1 raw track */
    disk->geometry.sector_size = 0; /* variable */
    disk->geometry.total_sectors = count;

    return UFT_OK;
}

/* ============================================================================
 * close
 * ============================================================================ */

static void mfi_close(uft_disk_t *disk)
{
    mfi_data_t *pdata = disk->plugin_data;
    if (pdata) {
        if (pdata->file) fclose(pdata->file);
        free(pdata);
        disk->plugin_data = NULL;
    }
}

/* ============================================================================
 * read_track — reads compressed flux data as raw sector
 * ============================================================================ */

static uft_error_t mfi_read_track(uft_disk_t *disk, int cyl, int head,
                                   uft_track_t *track)
{
    mfi_data_t *pdata = disk->plugin_data;
    if (!pdata || !pdata->file) return UFT_ERROR_INVALID_STATE;

    /* MF-560: negative Koordinaten abweisen, BEVOR daraus ein Index wird.
     *
     * Hier stand nur die obere Schranke. Ein negatives `cyl` oder `head`
     * ergibt einen negativen Index, kommt an `>=` vorbei und liest vor dem
     * Feld — gefunden von ASan in der CI (heap-buffer-overflow in
     * mfi_read_track), nicht lokal: MinGW hat keinen Sanitizer.
     *
     * MF-516/522 hat dieselbe Klasse in 54 Dateien behoben. Diese hier
     * rechnet den Index VOR der Schranke aus und ist dabei durchgerutscht. */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    int idx = cyl * pdata->heads + head;
    if (idx >= pdata->track_count) return UFT_ERROR_INVALID_STATE;

    mfi_track_entry_t *te = &pdata->tracks[idx];
    if (te->compressed_size == 0) return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    /* Read compressed track data */
    if (fseek(pdata->file, (long)te->offset, SEEK_SET) != 0)
        return UFT_ERROR_IO;

    uint8_t *buf = malloc(te->compressed_size);
    if (!buf) return UFT_ERROR_NO_MEMORY;

    if (fread(buf, 1, te->compressed_size, pdata->file) !=
        te->compressed_size) {
        free(buf);
        return UFT_ERROR_IO;
    }

    /* Store as single raw sector (compressed flux data)
     * Full decompression would require zlib — deferred to analysis layer */
    uint16_t chunk = (te->compressed_size > 65535) ?
                     65535 : (uint16_t)te->compressed_size;
    uft_format_add_sector(track, 0, buf, chunk, (uint8_t)cyl, (uint8_t)head);
    free(buf);

    return UFT_OK;
}

/* ============================================================================
 * write_track — DOCUMENTED NOT_IMPLEMENTED per spec §1.3 Option 1.
 *
 * MFI (MAME Floppy Image) stores per-track zlib-deflated streams of
 * 32-bit flux cells (lower 28 bits = position, upper 4 bits = type:
 * neutral, MG_0, MG_1, MG_N). Writing needs MFM cell synthesis plus
 * zlib compression plus file-offset-table recomputation.
 *
 * Implementation steps:
 *   1. Encode sectors → MFM bits (same shared encoder KFX needs).
 *   2. Map MFM cells → MFI 32-bit cell format (position in 200e6/2
 *      units = 100 MHz, type in upper nibble).
 *   3. zlib-deflate the cell array.
 *   4. Update the file's track offset table and refresh each track
 *      entry's compressed + uncompressed size.
 *   5. Rewrite the 4096-byte header with total track count and the
 *      variant string.
 *
 * Estimated effort: ~250 lines (above the shared MFM encoder).
 * Blocker: shared MFM encoder not in the tree + zlib not linked.
 * Workaround: use HFE/SCP for interchange; MAME reads HFE natively
 * since 0.238.
 * ============================================================================ */

static uft_error_t mfi_write_track(uft_disk_t *disk, int cyl, int head,
                                    const uft_track_t *track)
{
    (void)disk; (void)cyl; (void)head; (void)track;
    return UFT_ERROR_NOT_SUPPORTED;
}

/* ============================================================================
 * Plugin registration
 * ============================================================================ */

static const uft_plugin_feature_t uft_format_plugin_mfi_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_SUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_mfi = {
    .name         = "MFI",
    .description  = "MAME Floppy Image",
    .extensions   = "mfi",
    .version      = 0x00010000,
    .format       = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_FLUX | UFT_FORMAT_CAP_VERIFY,
    .probe        = mfi_probe,
    .open         = mfi_open,
    .close        = mfi_close,
    .read_track   = mfi_read_track,
    .write_track  = mfi_write_track,
    .verify_track = uft_flux_verify_track,
    .spec_status = UFT_SPEC_OFFICIAL_PARTIAL,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_mfi_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_mfi_features) / sizeof(uft_format_plugin_mfi_features[0]),
};

UFT_REGISTER_FORMAT_PLUGIN(mfi)
