/**
 * @file uft_atx.c
 * @brief ATX (Atari 8-bit Protected / VAPI) Plugin.
 *
 * ATX stores Atari 8-bit disk data with FDC status, per-sector timing,
 * weak-bit regions and extended-sector info — all of it copy-protection
 * evidence, which is why the format exists at all.
 *
 * **Authority:** `src/a8rawconv/diskatx.cpp` by Avery Lee (GPL-2-or-later,
 * reference oracle only, not built — see `src/a8rawconv/README.md`). Its
 * writer (`write_atx`, :216-416) pins the layout byte for byte and its
 * reader (`read_atx`, :64-190) confirms how it is consumed:
 *
 *   file header, 48 bytes
 *     0x00 "AT8X"   0x04 version major   0x06 version minor
 *     0x12 density (0 = single/FM, 1 = enhanced/MFM)
 *     0x1C track data offset — where the track records begin
 *   track records, ONE AFTER ANOTHER (there is no offset table):
 *     track header, 32 bytes
 *       0x00 record size    0x04 type (0 = track)   0x08 track number
 *       0x0A sector count   0x10 flags              0x14 chunk offset (= 32)
 *     chunks, each with an 8-byte header
 *       0x00 size   0x04 type   0x05 num   0x06 data
 *       0x01 sector list  — n x 8-byte sector headers
 *       0x00 sector data  — n x 128 bytes
 *       0x10 weak bits    — num = index in the list, data = first weak byte
 *       0x11 ext sector   — num = index in the list, data = size code
 *       size 0            — end of the chunk list
 *     sector header, 8 bytes
 *       0x00 id   0x01 FDC status   0x02 timing   0x04 data offset
 *       (data offset is relative to the START OF THE TRACK RECORD)
 *
 * What this file assumed before MF-467, and what is really there:
 *
 *   | assumed                          | reality                        |
 *   |----------------------------------|--------------------------------|
 *   | density at 0x13                  | 0x12 (0x13 is padding)         |
 *   | track count at 0x14              | that is `mImageId`; the format |
 *   |                                  | has no track count             |
 *   | a table of LE32 track offsets    | track records, walked in order |
 *   | track flags at 0x14 of the header| 0x10                           |
 *   | chunk offset at 0x18             | 0x14                           |
 *
 * The last two are why it never returned a single sector: the chunk offset
 * came out of padding, i.e. 0, so the scan started on the track header
 * itself, found no sector list, and returned an empty track. Every ATX file
 * read like an empty disk, silently.
 *
 * Part of M2 TA3 (MASTER_PLAN.md). See docs/A8RAWCONV_INTEGRATION_TODO.md.
 */
#include "uft/uft_format_common.h"
#include "uft/uft_log.h"
#include "uft/formats/atx.h"   /* uft_atx_write() (MF-474) */

/*
 * ATX signature as read by uft_read_le32 on the bytes 'A','T','8','X'
 * (0x41, 0x54, 0x38, 0x58 in file order). A little-endian read yields
 * 0x58385441, NOT 0x41543858 — see KNOWN_ISSUES.md §M.-1 (closed).
 */
#define ATX_SIGNATURE          0x58385441u  /* "AT8X" as LE32 */
#define ATX_FILE_HEADER_SIZE   48
#define ATX_TRACK_HEADER_SIZE  32
#define ATX_SECTOR_HEADER_SIZE 8
#define ATX_CHUNK_HEADER_SIZE  8
#define ATX_MAX_TRACKS         48          /* a8rawconv warns above 40 */
#define ATX_MAX_SECTORS        32          /* per track; ED uses 26 */

/* Atari SD and ED both use 128-byte sectors; ATX stores 128 per sector even
 * for a long one (write_atx:381 writes 128 unconditionally). The extended
 * chunk describes the PHYSICAL size, not extra payload. */
#define ATX_SECTOR_SIZE        128

/* Winkelposition: ATX zaehlt eine Umdrehung in 26042 Schritten
 * (diskatx.cpp:170 `sec.mPosition = (float)shdr.mTimingOffset / 26042.0f`,
 * write_atx:376 rechnet mit demselben Faktor zurueck). */
#define ATX_TIMING_UNITS_PER_REV 26042

/* File header offsets */
#define ATX_OFF_VERSION_MAJOR  0x04
#define ATX_OFF_VERSION_MINOR  0x06
#define ATX_OFF_DENSITY        0x12
#define ATX_OFF_TRACK_DATA     0x1C

/* Track header offsets */
#define ATX_TRK_OFF_SIZE       0x00
#define ATX_TRK_OFF_TYPE       0x04
#define ATX_TRK_OFF_NUMBER     0x08
#define ATX_TRK_OFF_NUMSECS    0x0A
#define ATX_TRK_OFF_FLAGS      0x10
#define ATX_TRK_OFF_CHUNKS     0x14

/* Chunk types (byte at offset 4 of each chunk header). */
#define ATX_CHUNK_SECTOR_DATA     0x00
#define ATX_CHUNK_SECTOR_LIST     0x01
#define ATX_CHUNK_WEAK_BITS       0x10
#define ATX_CHUNK_EXT_SECTOR_HDR  0x11

/* Track flags (uint32_t at 0x10 of the track header). */
#define ATX_TRK_FLAG_MFM          0x00000002
#define ATX_TRK_FLAG_NO_SKEW      0x00000100

/* FDC status bits (sector header byte 1, not inverted).
 * Semantics from write_atx:333-364 and read_atx:170-186. */
#define ATX_FDC_DRQ               0x02      /* long sector, with LOST_DATA  */
#define ATX_FDC_LOST_DATA         0x04      /* long sector                  */
#define ATX_FDC_CRC_ERROR         0x08      /* data-field CRC              */
#define ATX_FDC_MISSING_DATA      0x10      /* no data field at all        */
#define ATX_FDC_DELETED           0x20      /* deleted-data mark (0xF8)    */
#define ATX_FDC_WEAK              0x40      /* sector has weak bits        */
/* CRC + MISSING together mean the ADDRESS field's CRC failed, not the
 * data field's (read_atx:184). */
#define ATX_FDC_ADDR_CRC_MASK     0x18

typedef struct {
    uint8_t  *file_data;
    size_t    file_size;
    uint16_t  version_major;
    uint16_t  version_minor;
    uint8_t   density;
    uint8_t   track_count;                     /* highest track number + 1 */
    uint8_t   max_sectors;                     /* largest sector count seen */
    uint32_t  track_offsets[ATX_MAX_TRACKS];   /* file offset, 0 = absent  */
} atx_data_t;

/* ───────────────────────── Probe ──────────────────────────────────── */

static bool atx_plugin_probe(const uint8_t *data, size_t size,
                              size_t file_size, int *confidence) {
    (void)file_size;
    if (size < ATX_FILE_HEADER_SIZE) return false;
    if (uft_read_le32(data) != ATX_SIGNATURE) return false;
    *confidence = 98;
    return true;
}

/* ───────────────────────── Open ───────────────────────────────────── */

static uft_error_t atx_open(uft_disk_t *disk, const char *path, bool ro) {
    (void)ro;
    uft_error_t err = UFT_ERROR_NO_MEMORY;
    uint8_t *raw = NULL;
    atx_data_t *p = NULL;

    size_t raw_size = 0;
    raw = uft_read_file(path, &raw_size);
    if (!raw) return UFT_ERROR_FILE_OPEN;
    if (raw_size < ATX_FILE_HEADER_SIZE) {
        err = UFT_ERROR_FORMAT_INVALID; goto fail;
    }
    if (uft_read_le32(raw) != ATX_SIGNATURE) {
        err = UFT_ERROR_FORMAT_INVALID; goto fail;
    }

    p = calloc(1, sizeof(atx_data_t));
    if (!p) goto fail;

    p->file_data     = raw;
    p->file_size     = raw_size;
    p->version_major = uft_read_le16(raw + ATX_OFF_VERSION_MAJOR);
    p->version_minor = uft_read_le16(raw + ATX_OFF_VERSION_MINOR);
    p->density       = raw[ATX_OFF_DENSITY];

    /* a8rawconv starts reading records directly after the 48-byte header and
     * ignores this field; its own writer sets it to 48. Honouring it when it
     * points somewhere sane costs nothing and reads files that do not. */
    uint32_t pos = uft_read_le32(raw + ATX_OFF_TRACK_DATA);
    if (pos < ATX_FILE_HEADER_SIZE || pos >= raw_size)
        pos = ATX_FILE_HEADER_SIZE;

    /* Walk the record chain. Each record says how long it is; a record that
     * claims zero or overruns the file ends the walk rather than looping. */
    while ((size_t)pos + ATX_TRACK_HEADER_SIZE <= raw_size) {
        const uint8_t *th = raw + pos;
        uint32_t rec_size = uft_read_le32(th + ATX_TRK_OFF_SIZE);
        uint16_t rec_type = uft_read_le16(th + ATX_TRK_OFF_TYPE);

        if (rec_size < ATX_TRACK_HEADER_SIZE ||
            (size_t)pos + rec_size > raw_size)
            break;

        if (rec_type == 0) {
            uint8_t  tnum = th[ATX_TRK_OFF_NUMBER];
            uint16_t nsec = uft_read_le16(th + ATX_TRK_OFF_NUMSECS);

            if (tnum < ATX_MAX_TRACKS) {
                if (p->track_offsets[tnum] != 0)
                    UFT_WARN("ATX: Spur %u kommt mehrfach vor — die spaetere "
                             "Aufzeichnung gewinnt (%s)", tnum, path);
                p->track_offsets[tnum] = pos;
                if (tnum + 1 > p->track_count)
                    p->track_count = (uint8_t)(tnum + 1);
                if (nsec <= ATX_MAX_SECTORS && nsec > p->max_sectors)
                    p->max_sectors = (uint8_t)nsec;
            } else {
                UFT_WARN("ATX: Spurnummer %u ausserhalb des Bereichs, "
                         "uebersprungen (%s)", tnum, path);
            }
        }

        pos += rec_size;
    }

    if (p->track_count == 0) {
        UFT_WARN("ATX: keine Spuraufzeichnung gefunden (%s)", path);
        err = UFT_ERROR_FORMAT_INVALID;
        goto fail;
    }

    disk->plugin_data          = p;
    disk->geometry.cylinders   = p->track_count;
    disk->geometry.heads       = 1;
    disk->geometry.sectors     = p->max_sectors ? p->max_sectors : 18;
    disk->geometry.sector_size = ATX_SECTOR_SIZE;
    disk->geometry.total_sectors =
        (uint32_t)p->track_count * disk->geometry.sectors;
    return UFT_OK;

fail:
    free(p);
    free(raw);
    return err;
}

static void atx_close(uft_disk_t *disk) {
    atx_data_t *p = disk->plugin_data;
    if (p) { free(p->file_data); free(p); disk->plugin_data = NULL; }
}

/* ───────────────────────── Read track ─────────────────────────────── */

typedef struct {
    uint8_t  list_index;        /* index into the sector list */
    uint16_t weak_offset;       /* first weak byte within the sector */
} atx_weak_info_t;

static uft_error_t atx_read_track(uft_disk_t *disk, int cyl, int head,
                                   uft_track_t *track) {
    atx_data_t *p = disk->plugin_data;
    if (!p || !p->file_data) return UFT_ERROR_INVALID_STATE;
    if (head != 0 || cyl < 0 || cyl >= p->track_count)
        return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    uint32_t trk_off = p->track_offsets[cyl];
    if (trk_off == 0)
        return UFT_OK;  /* track absent from the image — not an error */

    const uint8_t *th = p->file_data + trk_off;
    uint32_t trk_size    = uft_read_le32(th + ATX_TRK_OFF_SIZE);
    uint16_t num_sectors = uft_read_le16(th + ATX_TRK_OFF_NUMSECS);
    uint32_t trk_flags   = uft_read_le32(th + ATX_TRK_OFF_FLAGS);
    uint32_t chunk_off   = uft_read_le32(th + ATX_TRK_OFF_CHUNKS);

    if (trk_size < ATX_TRACK_HEADER_SIZE ||
        (size_t)trk_off + trk_size > p->file_size) return UFT_OK;
    if (num_sectors == 0 || num_sectors > ATX_MAX_SECTORS) return UFT_OK;
    if (chunk_off < ATX_TRACK_HEADER_SIZE || chunk_off >= trk_size) return UFT_OK;

    track->encoding = (trk_flags & ATX_TRK_FLAG_MFM) ? UFT_ENCODING_MFM
                                                     : UFT_ENCODING_FM;

    /* Scan the chunk list. Offsets are relative to the track record. */
    uint16_t ext_size_code[ATX_MAX_SECTORS] = {0};
    atx_weak_info_t weak[ATX_MAX_SECTORS];
    int weak_count = 0;
    uint32_t sec_list_off = 0;

    uint32_t tcpos = chunk_off;
    while (tcpos + ATX_CHUNK_HEADER_SIZE <= trk_size) {
        const uint8_t *ch = th + tcpos;
        uint32_t c_size = uft_read_le32(ch + 0);
        uint8_t  c_type = ch[4];
        uint8_t  c_num  = ch[5];
        uint16_t c_data = uft_read_le16(ch + 6);

        if (c_size == 0) break;                       /* end of chunk list */
        if (c_size < ATX_CHUNK_HEADER_SIZE) break;
        if (tcpos + c_size > trk_size) break;

        if (c_type == ATX_CHUNK_SECTOR_LIST) {
            if (c_size >= ATX_CHUNK_HEADER_SIZE +
                          (uint32_t)num_sectors * ATX_SECTOR_HEADER_SIZE)
                sec_list_off = tcpos + ATX_CHUNK_HEADER_SIZE;
        } else if (c_type == ATX_CHUNK_EXT_SECTOR_HDR) {
            if (c_num < ATX_MAX_SECTORS) ext_size_code[c_num] = c_data;
        } else if (c_type == ATX_CHUNK_WEAK_BITS) {
            if (weak_count < ATX_MAX_SECTORS) {
                weak[weak_count].list_index  = c_num;
                weak[weak_count].weak_offset = c_data;
                weak_count++;
            }
        }

        tcpos += c_size;
    }

    if (sec_list_off == 0) {
        UFT_WARN("ATX: Spur %d hat %u Sektoren im Kopf, aber keine "
                 "Sektorliste", cyl, num_sectors);
        return UFT_OK;
    }

    /* list index -> index of the sector this produced, or -1 if none was
     * created. The weak chunks address the LIST, so the mapping has to be
     * kept explicitly instead of assuming the two run in step. */
    int produced[ATX_MAX_SECTORS];
    for (int i = 0; i < ATX_MAX_SECTORS; i++) produced[i] = -1;

    uint8_t seen_ids[ATX_MAX_SECTORS];
    int seen_count = 0;

    for (uint16_t s = 0; s < num_sectors && s < ATX_MAX_SECTORS; s++) {
        size_t sh_off = (size_t)trk_off + sec_list_off +
                        (size_t)s * ATX_SECTOR_HEADER_SIZE;
        if (sh_off + ATX_SECTOR_HEADER_SIZE > p->file_size) break;

        const uint8_t *sh = p->file_data + sh_off;
        uint8_t  sec_id       = sh[0];
        uint8_t  fdc          = sh[1];
        uint16_t timing       = uft_read_le16(sh + 2);
        uint32_t sec_data_off = uft_read_le32(sh + 4);

        bool missing = (fdc & ATX_FDC_MISSING_DATA) != 0;
        bool addr_crc_bad = (fdc & ATX_FDC_ADDR_CRC_MASK) == ATX_FDC_ADDR_CRC_MASK;
        bool data_crc_bad = !addr_crc_bad && (fdc & ATX_FDC_CRC_ERROR) != 0;
        bool long_sector  = (fdc & ATX_FDC_LOST_DATA) != 0;

        /* The stored payload is always 128 bytes. A long sector's extended
         * chunk states the PHYSICAL size; the extra bytes are not in the
         * file, so they are not invented here either. */
        uint8_t payload[ATX_SECTOR_SIZE];
        const uint8_t *src;

        if (missing) {
            /* The sector header exists, the data field does not. The buffer
             * below is a placeholder so the sector can be reported at all —
             * it is marked MISSING and crc_ok = false, and no caller may
             * read it as recovered content. */
            memset(payload, 0, sizeof(payload));
            src = payload;
        } else {
            size_t abs = (size_t)trk_off + sec_data_off;
            if (sec_data_off == 0 || abs + ATX_SECTOR_SIZE > p->file_size) {
                UFT_WARN("ATX: Spur %d Sektor %u zeigt auf Offset %u ausserhalb "
                         "der Spur — uebersprungen", cyl, sec_id, sec_data_off);
                continue;
            }
            src = p->file_data + abs;
        }

        if (uft_format_add_sector_with_id(track, sec_id, src,
                                          ATX_SECTOR_SIZE,
                                          (uint8_t)cyl, 0) != UFT_OK)
            continue;

        produced[s] = (int)track->sector_count - 1;
        uft_sector_t *sec = &track->sectors[track->sector_count - 1];

        /* Winkelposition erhalten (MF-474). ATX zaehlt in 1/26042
         * Umdrehung (diskatx.cpp:170). Sie ist hier forensisch tragend:
         * zwei Sektoren mit derselben Nummer unterscheiden sich NUR durch
         * ihre Position, und genau das ist der Phantomsektor-Kopierschutz.
         * Bis MF-474 wurde der Wert gelesen und weggeworfen. */
        sec->angular_position = (double)timing / (double)ATX_TIMING_UNITS_PER_REV;
        sec->has_angular_position = true;

        uft_sector_set_crc(sec, !(data_crc_bad || missing));
        uft_sector_set_id_crc(sec, !addr_crc_bad);
        sec->deleted   = (fdc & ATX_FDC_DELETED) != 0;
        sec->data_mark = sec->deleted ? 0xF8u : 0xFBu;

        sec->status = UFT_SECTOR_OK;
        if (data_crc_bad) sec->status |= UFT_SECTOR_CRC_ERROR;
        if (addr_crc_bad) sec->status |= UFT_SECTOR_ID_CRC_ERROR;
        if (missing)      sec->status |= UFT_SECTOR_MISSING;
        if (sec->deleted) sec->status |= UFT_SECTOR_DELETED;
        if (fdc & ATX_FDC_WEAK) sec->status |= UFT_SECTOR_WEAK;

        if (long_sector) {
            sec->status |= UFT_SECTOR_EXTRA;
            unsigned code = ext_size_code[s] & 3u;
            if (code < 1) code = 1;
            UFT_INFO("ATX: Spur %d Sektor %u ist ein langer Sektor "
                     "(physisch %u Byte, im Abbild stehen %u)",
                     cyl, sec_id, 128u << code, (unsigned)ATX_SECTOR_SIZE);
        }

        /* A sector number that appears twice on one track is a phantom
         * sector — one of the oldest Atari protections, and the reason the
         * format keeps a list instead of an array (write_atx:279-284). */
        for (int k = 0; k < seen_count; k++) {
            if (seen_ids[k] == sec_id) {
                sec->status |= UFT_SECTOR_DUPLICATE;
                if (produced[k] >= 0)
                    track->sectors[produced[k]].status |= UFT_SECTOR_DUPLICATE;
                break;
            }
        }
        if (seen_count < ATX_MAX_SECTORS) seen_ids[seen_count++] = sec_id;
    }

    /* Weak-bit chunks, applied after the sectors exist.
     *
     * The chunk carries the index into the SECTOR LIST plus the offset of the
     * first weak byte; everything from there to the end of the sector is
     * weak. The per-byte mask lets multi-read voting and forensic export keep
     * that byte-exact. uft_track_free() releases weak_mask. */
    for (int w = 0; w < weak_count; w++) {
        uint8_t li = weak[w].list_index;
        if (li >= ATX_MAX_SECTORS || produced[li] < 0) continue;

        uft_sector_t *sec = &track->sectors[produced[li]];
        sec->weak = true;
        sec->status |= UFT_SECTOR_WEAK;

        size_t sec_size = sec->data_len ? sec->data_len : sec->data_size;
        if (sec_size == 0) continue;

        uint16_t first = weak[w].weak_offset;
        if (first >= sec_size) continue;   /* nothing in this sector is weak */

        if (!sec->weak_mask) {
            sec->weak_mask = calloc(sec_size, 1);
            if (!sec->weak_mask) continue;   /* flag stays, mask omitted */
        }
        memset(sec->weak_mask + first, 0xFF, sec_size - first);
    }

    return UFT_OK;
}

/* ───────────────────────── Write ──────────────────────────────────── */

/*
 * Bis MF-474 stand hier: "write_track is deliberately omitted — ATX encodes
 * per-sector timing anomalies, weak bits, duplicate sector IDs, and
 * FDC-status quirks that cannot be synthesised from sector payload alone."
 *
 * Das war richtig, solange der Leser genau diese Dinge wegwarf. Seit MF-467
 * traegt die Spur sie alle — Status je Sektor, Weak-Maske, doppelte
 * Sektornummern — und seit MF-474 auch die Winkelposition. Damit ist der
 * Rueckweg nicht mehr Erfindung, sondern Wiedergabe.
 *
 * Was bleibt: eine Spur, die NICHT aus einem ATX stammt, hat keine
 * Winkelpositionen. Der Schreiber verteilt dann gleichmaessig und SAGT das —
 * er tut nicht so, als haette er gemessen.
 */

/** Einen Spur-Datensatz anfuegen. */
static uft_error_t atx_write_track_record(FILE *f, int cyl,
                                          const uft_track_t *track,
                                          bool *out_synth_positions)
{
    uint16_t n = (uint16_t)track->sector_count;
    if (n > ATX_MAX_SECTORS) n = ATX_MAX_SECTORS;

    /* Weak-Chunks nur fuer Sektoren, die wirklich eine Maske tragen. */
    uint16_t weak_n = 0;
    for (uint16_t s = 0; s < n; s++)
        if (track->sectors[s].weak_mask) weak_n++;

    /* Groesse wie write_atx:301-306: Kopf + Sektorliste + Sektordaten +
     * Weak-Chunks + Abschluss. */
    uint32_t rec_size = ATX_TRACK_HEADER_SIZE
                      + ATX_CHUNK_HEADER_SIZE + 8u * n
                      + ATX_CHUNK_HEADER_SIZE + (uint32_t)ATX_SECTOR_SIZE * n
                      + ATX_CHUNK_HEADER_SIZE * (uint32_t)weak_n
                      + ATX_CHUNK_HEADER_SIZE;

    uint8_t th[ATX_TRACK_HEADER_SIZE];
    memset(th, 0, sizeof(th));
    uft_write_le32(th + ATX_TRK_OFF_SIZE, rec_size);
    uft_write_le16(th + ATX_TRK_OFF_TYPE, 0);
    th[ATX_TRK_OFF_NUMBER] = (uint8_t)cyl;
    uft_write_le16(th + ATX_TRK_OFF_NUMSECS, n);
    uft_write_le32(th + ATX_TRK_OFF_FLAGS,
                   (track->encoding == UFT_ENCODING_MFM) ? ATX_TRK_FLAG_MFM : 0);
    uft_write_le32(th + ATX_TRK_OFF_CHUNKS, ATX_TRACK_HEADER_SIZE);
    if (fwrite(th, 1, sizeof(th), f) != sizeof(th)) return UFT_ERROR_FILE_WRITE;

    /* Sektorliste */
    uint8_t ch[ATX_CHUNK_HEADER_SIZE];
    memset(ch, 0, sizeof(ch));
    uft_write_le32(ch, ATX_CHUNK_HEADER_SIZE + 8u * n);
    ch[4] = ATX_CHUNK_SECTOR_LIST;
    if (fwrite(ch, 1, sizeof(ch), f) != sizeof(ch)) return UFT_ERROR_FILE_WRITE;

    const uint32_t data_base = ATX_TRACK_HEADER_SIZE
                             + ATX_CHUNK_HEADER_SIZE + 8u * n
                             + ATX_CHUNK_HEADER_SIZE;

    for (uint16_t s = 0; s < n; s++) {
        const uft_sector_t *sec = &track->sectors[s];
        uint8_t sh[8];
        memset(sh, 0, sizeof(sh));

        sh[0] = sec->id.sector;

        /* Status zurueckuebersetzen — die Umkehrung von read_track oben.
         * Adressfeld-CRC setzt BEIDE Bits (write_atx:346-349). */
        uint8_t fdc = 0;
        if (sec->status & UFT_SECTOR_ID_CRC_ERROR) fdc |= ATX_FDC_ADDR_CRC_MASK;
        else if (!sec->crc_ok)                     fdc |= ATX_FDC_CRC_ERROR;
        if (sec->status & UFT_SECTOR_MISSING)      fdc |= ATX_FDC_MISSING_DATA;
        if (sec->deleted)                          fdc |= ATX_FDC_DELETED;
        if (sec->status & UFT_SECTOR_EXTRA)        fdc |= ATX_FDC_LOST_DATA | ATX_FDC_DRQ;
        if (sec->weak || sec->weak_mask)           fdc |= ATX_FDC_WEAK;
        sh[1] = fdc;

        /* Winkelposition. Liegt keine vor, gleichmaessig verteilen — und den
         * Aufrufer wissen lassen, dass diese Zahlen gerechnet und nicht
         * gemessen sind. */
        double pos;
        if (sec->has_angular_position) {
            pos = sec->angular_position;
        } else {
            pos = (n > 0) ? ((double)s / (double)n) : 0.0;
            *out_synth_positions = true;
        }
        if (pos < 0.0) pos = 0.0;
        long units = (long)(pos * ATX_TIMING_UNITS_PER_REV);
        units %= ATX_TIMING_UNITS_PER_REV;      /* write_atx:371 */
        uft_write_le16(sh + 2, (uint16_t)units);

        uft_write_le32(sh + 4, data_base + (uint32_t)s * ATX_SECTOR_SIZE);
        if (fwrite(sh, 1, sizeof(sh), f) != sizeof(sh)) return UFT_ERROR_FILE_WRITE;
    }

    /* Sektordaten */
    memset(ch, 0, sizeof(ch));
    uft_write_le32(ch, ATX_CHUNK_HEADER_SIZE + (uint32_t)ATX_SECTOR_SIZE * n);
    ch[4] = ATX_CHUNK_SECTOR_DATA;
    if (fwrite(ch, 1, sizeof(ch), f) != sizeof(ch)) return UFT_ERROR_FILE_WRITE;

    for (uint16_t s = 0; s < n; s++) {
        const uft_sector_t *sec = &track->sectors[s];
        uint8_t payload[ATX_SECTOR_SIZE];
        memset(payload, 0, sizeof(payload));
        if (sec->data) {
            size_t len = sec->data_len ? sec->data_len : sec->data_size;
            if (len > ATX_SECTOR_SIZE) len = ATX_SECTOR_SIZE;
            memcpy(payload, sec->data, len);
        }
        if (fwrite(payload, 1, sizeof(payload), f) != sizeof(payload))
            return UFT_ERROR_FILE_WRITE;
    }

    /* Weak-Chunks: Index in der SEKTORLISTE plus erstes weakes Byte. */
    for (uint16_t s = 0; s < n; s++) {
        const uft_sector_t *sec = &track->sectors[s];
        if (!sec->weak_mask) continue;

        size_t len = sec->data_len ? sec->data_len : sec->data_size;
        if (len > ATX_SECTOR_SIZE) len = ATX_SECTOR_SIZE;
        size_t first = len;
        for (size_t i = 0; i < len; i++) {
            if (sec->weak_mask[i]) { first = i; break; }
        }
        if (first >= len) continue;     /* Maske ohne gesetztes Byte */

        memset(ch, 0, sizeof(ch));
        uft_write_le32(ch, ATX_CHUNK_HEADER_SIZE);
        ch[4] = ATX_CHUNK_WEAK_BITS;
        ch[5] = (uint8_t)s;
        uft_write_le16(ch + 6, (uint16_t)first);
        if (fwrite(ch, 1, sizeof(ch), f) != sizeof(ch)) return UFT_ERROR_FILE_WRITE;
    }

    /* Abschluss der Chunk-Liste: acht Nullbytes (write_atx:415-416). */
    memset(ch, 0, sizeof(ch));
    if (fwrite(ch, 1, sizeof(ch), f) != sizeof(ch)) return UFT_ERROR_FILE_WRITE;

    return UFT_OK;
}

uft_error_t uft_atx_write(const char *path, const uft_track_t *tracks,
                          size_t track_count, bool enhanced_density)
{
    if (!path || !tracks || track_count == 0) return UFT_ERROR_NULL_POINTER;
    if (track_count > ATX_MAX_TRACKS)         return UFT_ERROR_OUT_OF_RANGE;

    FILE *f = fopen(path, "wb");
    if (!f) return UFT_ERROR_FILE_OPEN;

    /* Dateikopf, Felder wie write_atx:242-259. Der Creator dort ist 0x5241
     * fuer a8rawconv; UFT setzt 0x4655. */
    uint8_t hdr[ATX_FILE_HEADER_SIZE];
    memset(hdr, 0, sizeof(hdr));
    memcpy(hdr, "AT8X", 4);
    uft_write_le16(hdr + ATX_OFF_VERSION_MAJOR, 1);
    uft_write_le16(hdr + ATX_OFF_VERSION_MINOR, 1);
    uft_write_le16(hdr + 0x08, 0x4655);            /* creator */
    hdr[ATX_OFF_DENSITY] = enhanced_density ? 1 : 0;
    uft_write_le32(hdr + ATX_OFF_TRACK_DATA, ATX_FILE_HEADER_SIZE);
    if (fwrite(hdr, 1, sizeof(hdr), f) != sizeof(hdr)) {
        fclose(f);
        return UFT_ERROR_FILE_WRITE;
    }

    bool synth_positions = false;
    for (size_t i = 0; i < track_count; i++) {
        uft_error_t rc = atx_write_track_record(f, (int)i, &tracks[i],
                                                &synth_positions);
        if (rc != UFT_OK) { fclose(f); return rc; }
    }

    fclose(f);

    if (synth_positions) {
        /* Prinzip 1: was gerechnet ist, darf nicht als gemessen durchgehen.
         * Winkelpositionen tragen bei ATX Kopierschutz-Information; ein
         * gleichmaessig verteiltes Layout sieht plausibel aus und ist es
         * nicht. */
        UFT_WARN("ATX: mindestens eine Spur brachte keine Winkelpositionen mit "
                 "— sie wurden gleichmaessig verteilt. Diese Positionen sind "
                 "GERECHNET, nicht gemessen (%s)", path);
    }
    return UFT_OK;
}

/* ───────────────────────── Plugin registration ────────────────────── */

/*
 * Die Plugin-Schnittstelle `write_track` bleibt bewusst leer: sie schreibt
 * EINE Spur in eine bestehende Datei, und ein ATX ist eine Kette von
 * Spur-Datensaetzen ohne Offset-Tabelle — eine Spur nachtraeglich zu
 * ersetzen hiesse, alles danach zu verschieben. Der Export geht deshalb
 * ueber uft_atx_write() ueber die ganze Diskette auf einmal (MF-474).
 */

static const uft_plugin_feature_t uft_format_plugin_atx_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_SUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_atx = {
    .name = "ATX", .description = "Atari 8-bit Protected (VAPI)",
    .extensions = "atx", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WEAK_BITS |
                    UFT_FORMAT_CAP_VERIFY,
    .probe = atx_plugin_probe, .open = atx_open,
    .close = atx_close, .read_track = atx_read_track,
    .verify_track = uft_weak_bit_verify_track,
    .spec_status = UFT_SPEC_REVERSE_ENGINEERED,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_atx_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_atx_features) / sizeof(uft_format_plugin_atx_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(atx)
