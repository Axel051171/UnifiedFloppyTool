/**
 * @file uft_format_converters.c
 * @brief Three Kat-A closures:
 *
 *   uft_imd_write    — Serialize an in-memory uft_imd_image_t to an IMD file.
 *   uft_imd_from_raw — Synthesize an IMD image from a flat raw byte buffer.
 *   uft_td0_to_imd   — Convert an already-decoded TD0 image to IMD.
 *
 * These three were declared in include/uft/formats/uft_imd.h and
 * include/uft/formats/uft_td0.h but had no implementation anywhere in
 * the tree. Callers (src/formats/uft_format_convert_*) relied on
 * broken ABI-mismatched stubs in uft_core_stubs.c that silently did
 * nothing.
 *
 * Scope: minimal but correct.
 *   - imd_write produces a file that can round-trip through imd_read_mem.
 *   - imd_from_raw uses the caller-supplied track_header as the template
 *     for every track (same mode, same nsectors, same sector_size).
 *   - td0_to_imd maps TD0 sector flags onto the 1..8 IMD stype codes
 *     per the Teledisk / IMD specs. Sectors without data become stype=0
 *     (unavailable).
 */

#include "uft/formats/uft_imd.h"
#include "uft/formats/uft_td0.h"
#include "uft/uft_error.h"     /* UFT_OK */
/* MF-1287: der Wandler liest jetzt die KANONISCHEN Sektorfelder
 * (`uft_track_t`, `uft_sector_t`, `uft_track_cleanup`) statt der
 * TD0-Flaggen — und braucht dafuer die Plugin-Typen. */
#include "uft/uft_format_plugin.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* MF-1287: hier standen NEUN eigene `IMD_STYPE_*`-Konstanten, mit dem
 * Kommentar „Not symbolically named in the header so define locally".
 * Der Satz stimmte nicht: `include/uft/formats/uft_imd.h:169-180` fuehrt
 * dieselben neun Werte als `uft_imd_sectype_t` — `UFT_IMD_SEC_UNAVAIL`
 * bis `UFT_IMD_SEC_DEL_ERR_COMP`, Wert fuer Wert deckungsgleich.
 *
 * Neun Zahlen, zwei Namen, zwei Orte: genau die Bauform, gegen die
 * §MF-1177 geschrieben ist. Die zweite Kopie ist weg; benutzt wird die
 * Aufzaehlung des Formatheaders. */

/* TD0 sector flag bits (per Teledisk). */

/* Map TD0 flags + data-presence to IMD stype. `compressed` means the
 * whole sector is a single fill byte. */
/* MF-1287: `td0_flags_to_imd_stype()` ist weg. Sie deutete die
 * TD0-Flaggen ein ZWEITES Mal — neben dem Plugin —, und sie tat es
 * falsch: `TD0_FLAG_NO_DATA 0x08` ist ein Bit, das TD0 nicht
 * benutzt, also war `UFT_IMD_SEC_UNAVAIL` unerreichbar (P3-523).
 * Die Deutung steht jetzt an einer Stelle, und der Wandler liest
 * die kanonischen Sektorfelder. */

/* ==========================================================================
 * uft_imd_write
 * ========================================================================== */

int uft_imd_write(const char *filename, const uft_imd_image_t *img)
{
    if (!filename || !img) return -1;
    FILE *f = fopen(filename, "wb");
    if (!f) return -1;

    /* ASCII header: "IMD V.R: DD/MM/YYYY HH:MM:SS\r\n"
     * Version defaults to 1.19 if unset. */
    uint8_t vmaj = img->header.version_major ? img->header.version_major : 1;
    uint8_t vmin = img->header.version_minor ? img->header.version_minor : 19;
    fprintf(f, "IMD %u.%02u: %02u/%02u/%04u %02u:%02u:%02u\r\n",
            vmaj, vmin,
            img->header.day, img->header.month, img->header.year,
            img->header.hour, img->header.minute, img->header.second);

    /* Comment block, terminated by 0x1A. */
    if (img->comment && img->comment_len > 0)
        fwrite(img->comment, 1, img->comment_len, f);
    fputc(UFT_IMD_COMMENT_END, f);

    /* Track records. */
    for (unsigned t = 0; t < img->num_tracks; t++) {
        const uft_imd_track_t *tr = &img->tracks[t];
        const uft_imd_track_header_t *hdr = &tr->header;

        /* Track header: 5 bytes. Head byte carries optional cylmap/headmap flags. */
        uint8_t head_byte = hdr->head;
        if (tr->has_cylmap)  head_byte |= UFT_IMD_HEAD_CYLMAP;
        if (tr->has_headmap) head_byte |= UFT_IMD_HEAD_HEADMAP;

        uint8_t hdr_out[5] = {
            hdr->mode, hdr->cylinder, head_byte, hdr->nsectors, hdr->sector_size
        };
        if (fwrite(hdr_out, 1, 5, f) != 5) { fclose(f); return -1; }

        /* Sector numbering map (always present). */
        fwrite(tr->smap, 1, hdr->nsectors, f);
        if (tr->has_cylmap)  fwrite(tr->cmap, 1, hdr->nsectors, f);
        if (tr->has_headmap) fwrite(tr->hmap, 1, hdr->nsectors, f);

        /* Sector data records. */
        uint16_t ssize = uft_imd_ssize_to_bytes(hdr->sector_size);
        for (unsigned s = 0; s < hdr->nsectors; s++) {
            uint8_t stype = tr->stype[s];
            fputc(stype, f);
            if (stype == UFT_IMD_SEC_UNAVAIL) continue;

            bool compressed = (stype == UFT_IMD_SEC_COMPRESSED ||
                               stype == UFT_IMD_SEC_DEL_COMP ||
                               stype == UFT_IMD_SEC_ERR_COMP ||
                               stype == UFT_IMD_SEC_DEL_ERR_COMP);

            uint16_t this_size = tr->has_varsizes ? tr->ssize[s] : ssize;
            size_t offset = tr->sector_offsets[s];
            if (!tr->data || offset + (compressed ? 1 : this_size) > tr->data_size) {
                fclose(f); return -1;
            }
            if (compressed) fputc(tr->data[offset], f);
            else            fwrite(tr->data + offset, 1, this_size, f);
        }
    }

    if (ferror(f)) { fclose(f); return -1; }
    fclose(f);
    return UFT_OK;
}

/* ==========================================================================
 * uft_imd_from_raw
 * ========================================================================== */

int uft_imd_from_raw(const uint8_t *data, size_t size,
                      const uft_imd_track_header_t *params,
                      uft_imd_image_t *img)
{
    if (!data || !params || !img) return -1;
    if (params->nsectors == 0) return -1;

    uint16_t ssize = uft_imd_ssize_to_bytes(params->sector_size);
    if (ssize == 0) return -1;

    size_t track_bytes = (size_t)params->nsectors * ssize;
    if (track_bytes == 0 || size % track_bytes != 0) return -1;

    size_t ntracks = size / track_bytes;
    if (ntracks == 0) return -1;

    if (uft_imd_init(img) != UFT_OK) return -1;
    img->tracks = (uft_imd_track_t *)calloc(ntracks, sizeof(uft_imd_track_t));
    if (!img->tracks) return -1;
    img->num_tracks    = (uint16_t)ntracks;
    img->num_cylinders = (uint16_t)((ntracks + 1) / 2);   /* assume 2 heads */
    img->num_heads     = 2;
    if (img->num_cylinders * 2 != ntracks) {
        img->num_cylinders = (uint16_t)ntracks;           /* single-sided */
        img->num_heads     = 1;
    }

    for (size_t t = 0; t < ntracks; t++) {
        uft_imd_track_t *tr = &img->tracks[t];
        tr->header = *params;
        tr->header.cylinder = (uint8_t)(t / img->num_heads);
        tr->header.head     = (uint8_t)(t % img->num_heads);

        /* Default ascending sector map 1..N. */
        for (uint8_t s = 0; s < params->nsectors; s++) {
            tr->smap[s]  = (uint8_t)(s + 1);
            tr->stype[s] = UFT_IMD_SEC_NORMAL;
            tr->sector_offsets[s] = (size_t)s * ssize;
        }

        tr->data_size = track_bytes;
        tr->data = (uint8_t *)malloc(track_bytes);
        if (!tr->data) { uft_imd_free(img); return -1; }
        memcpy(tr->data, data + t * track_bytes, track_bytes);

        img->total_sectors += params->nsectors;
    }
    return UFT_OK;
}

/* ==========================================================================
 * uft_td0_to_imd
 * ========================================================================== */

/* Map TD0 mode-byte-equivalent fields onto IMD mode. TD0 doesn't carry a
 * direct mode byte on each track in the same way IMD does; the choice
 * here follows the common case for PC floppies — MFM at the effective
 * rate matching the sector count. Callers who need exact mode mapping
 * should set it themselves after conversion. */
static uint8_t guess_imd_mode_mfm(uint8_t nsectors, uint8_t size_code) {
    uint16_t ssize = uft_imd_ssize_to_bytes(size_code);
    if (ssize == 0) ssize = 512;
    uint32_t track_bytes = (uint32_t)nsectors * ssize;
    if (track_bytes >= 12000) return UFT_IMD_MODE_500K_MFM;  /* HD */
    if (track_bytes >= 5000)  return UFT_IMD_MODE_250K_MFM;  /* DD */
    return UFT_IMD_MODE_300K_MFM;                             /* QD / slow */
}

/* ==========================================================================
 * uft_imd_to_raw — flatten an IMD image into a raw byte buffer
 *
 * Replaces the ABI-broken stub in uft_core_stubs.c (const void*). The new
 * signature matches include/uft/formats/uft_imd.h exactly.
 * ==========================================================================*/

int uft_imd_to_raw(const uft_imd_image_t *img, uint8_t **data_out,
                    size_t *size_out, uint8_t fill)
{
    if (!img || !data_out || !size_out) return -1;
    *data_out = NULL; *size_out = 0;
    if (img->num_tracks == 0 || !img->tracks) return -1;

    /* Sum all tracks' nominal sizes. Variable-size tracks contribute
     * sum-of-ssize[], fixed-size tracks contribute nsectors * ssize. */
    size_t total = 0;
    for (unsigned t = 0; t < img->num_tracks; t++) {
        const uft_imd_track_t *tr = &img->tracks[t];
        uint16_t nominal = uft_imd_ssize_to_bytes(tr->header.sector_size);
        if (tr->has_varsizes) {
            for (unsigned s = 0; s < tr->header.nsectors; s++)
                total += tr->ssize[s];
        } else {
            total += (size_t)tr->header.nsectors * nominal;
        }
    }
    if (total == 0) return -1;

    uint8_t *out = (uint8_t *)malloc(total);
    if (!out) return -1;
    memset(out, fill, total);

    size_t pos = 0;
    for (unsigned t = 0; t < img->num_tracks; t++) {
        const uft_imd_track_t *tr = &img->tracks[t];
        uint16_t nominal = uft_imd_ssize_to_bytes(tr->header.sector_size);

        /* Copy sectors in logical (smap) order. smap[i] is the sector ID;
         * to_raw emits them in the logical 1..N ordering matched by smap. */
        for (unsigned s = 0; s < tr->header.nsectors; s++) {
            uint16_t sz = tr->has_varsizes ? tr->ssize[s] : nominal;
            uint8_t stype = tr->stype[s];
            if (stype == 0 /*UNAVAILABLE*/ || !tr->data) {
                /* leave fill-byte in place */
                pos += sz;
                continue;
            }
            bool compressed = (stype == 2 || stype == 4 ||
                                stype == 6 || stype == 8);
            size_t off = tr->sector_offsets[s];
            if (compressed) {
                if (off >= tr->data_size) { free(out); return -1; }
                memset(out + pos, tr->data[off], sz);
            } else {
                if (off + sz > tr->data_size) { free(out); return -1; }
                memcpy(out + pos, tr->data + off, sz);
            }
            pos += sz;
        }
    }

    *data_out = out;
    *size_out = total;
    return UFT_OK;
}

/* ==========================================================================
 * uft_td0_to_raw / uft_td0_to_imd — auf dem Strom-Kern (MF-1287)
 *
 * Vorher nahmen beide ein `uft_td0_image_t`, das `uft_td0_read_mem()`
 * gefuellt hatte. Dieser zweite Leser ist mit MF-1287 weg; die Quelle
 * ist jetzt `uft_td0_strom_t` und der Spurlauf des Plugins.
 *
 * **Der wichtigere Unterschied ist, WAS gelesen wird.** Vorher deutete
 * dieser Wandler die TD0-Flaggen selbst (`td0_flags_to_imd_stype`) —
 * eine zweite Kopie derselben Regel, und sie war falsch: `0x08` als
 * „keine Daten" ist ein Bit, das TD0 nicht benutzt, also war der Zweig
 * `UFT_IMD_SEC_UNAVAIL` unerreichbar und ein datenloser Sektor kam als
 * NORMALER in der IMD an (P3-523). Jetzt liest der Wandler die
 * KANONISCHEN Felder, die das Plugin gesetzt hat — `status`, `crc_ok`,
 * `deleted` —, und die Deutung der Flaggen steht nur noch an einer
 * Stelle (§MF-1177).
 * ==========================================================================*/

/** IMD-Sektortyp aus dem kanonischen Sektor. */
static uint8_t imd_stype_aus_sektor(const uft_sector_t *p, bool compressed)
{
    /* MF-1287: `UFT_SECTOR_MISSING` setzt das Plugin in ZWEI Faellen —
     * beim datenlosen Sektor (Flaggen 0x10/0x20) und bei einem Satz, aus
     * dem weniger dekodiert wurde als der Sektor gross ist (MF-981).
     * Von aussen sind sie nicht zu unterscheiden, und beide werden hier
     * zu `UNAVAILABLE`. Das ist die vorsichtige Richtung: lieber sagen
     * „nicht gelesen" als Fuellbytes als Daten ausgeben. */
    if (p->status & UFT_SECTOR_MISSING) return UFT_IMD_SEC_UNAVAIL;

    bool deleted = p->deleted;
    bool bad     = !p->crc_ok;

    if (deleted && bad && compressed)  return UFT_IMD_SEC_DEL_ERR_COMP;
    if (deleted && bad)                return UFT_IMD_SEC_DEL_ERROR;
    if (deleted && compressed)         return UFT_IMD_SEC_DEL_COMP;
    if (deleted)                       return UFT_IMD_SEC_DELETED;
    if (bad && compressed)             return UFT_IMD_SEC_ERR_COMP;
    if (bad)                           return UFT_IMD_SEC_ERROR;
    if (compressed)                    return UFT_IMD_SEC_COMPRESSED;
    return UFT_IMD_SEC_NORMAL;
}

/** Groessencode zu einer Byteanzahl: 128 << code. 0xFF = kein Code. */
static uint8_t imd_groessencode(size_t bytes)
{
    for (uint8_t code = 0; code <= 6; code++)
        if ((size_t)(128u << code) == bytes) return code;
    return 0xFF;
}

int uft_td0_to_imd(const uft_td0_strom_t *s, struct uft_imd_image_t *imd_aus)
{
    uft_imd_image_t *imd = (uft_imd_image_t *)imd_aus;
    if (!s || !imd) return -1;
    if (uft_imd_init(imd) != UFT_OK) return -1;

    /* Zeitstempel aus dem Kommentarblock. Das Monatsfeld ist 0-BASIERT —
     * gemessen MF-1285 an libdsks Schreiber (`ptm->tm_mon`), an SAMdisks
     * Leser (`bMon + 1`) und am Objekt. Das `+1` hier ist deshalb
     * richtig; das Jahr steht mit `+1900` unter P3-522. */
    if (s->anmerkung.vorhanden) {
        imd->header.year   = (uint16_t)(s->anmerkung.jahr + 1900);
        imd->header.month  = (uint8_t)(s->anmerkung.monat + 1);
        imd->header.day    = s->anmerkung.tag;
        imd->header.hour   = s->anmerkung.stunde;
        imd->header.minute = s->anmerkung.minute;
        imd->header.second = s->anmerkung.sekunde;
    }
    imd->header.version_major = 1;
    imd->header.version_minor = 19;

    if (s->anmerkung.text && s->anmerkung.text_len > 0) {
        /* `anmerkung.text` ist NICHT NUL-terminiert — es zeigt in den
         * Strom. Die IMD-Seite will eine Zeichenkette, also kopieren und
         * selbst abschliessen. */
        imd->comment = (char *)malloc(s->anmerkung.text_len + 1);
        if (imd->comment) {
            memcpy(imd->comment, s->anmerkung.text, s->anmerkung.text_len);
            imd->comment[s->anmerkung.text_len] = '\0';
            imd->comment_len = s->anmerkung.text_len;
        }
    }

    const unsigned koepfe = s->sides ? s->sides : 1u;
    const size_t   platz  = (size_t)s->zylinder * koepfe;
    if (platz == 0) return UFT_OK;

    imd->tracks = (uft_imd_track_t *)calloc(platz, sizeof(uft_imd_track_t));
    if (!imd->tracks) return -1;
    imd->num_cylinders = s->zylinder;
    imd->num_heads     = koepfe;

    size_t n = 0;
    for (unsigned c = 0; c < s->zylinder; c++) {
        for (unsigned h = 0; h < koepfe; h++) {
            uft_track_t t;
            memset(&t, 0, sizeof(t));
            if (uft_td0_strom_spur(s, (int)c, (int)h, &t) != UFT_OK) continue;
            if (t.sector_count == 0) { uft_track_cleanup(&t); continue; }

            uft_imd_track_t *it = &imd->tracks[n];
            size_t nsec = t.sector_count;
            if (nsec > UFT_IMD_MAX_SECTORS) nsec = UFT_IMD_MAX_SECTORS;

            it->header.cylinder = (uint8_t)c;
            it->header.head     = (uint8_t)(h & 0x01u);
            it->header.nsectors = (uint8_t)nsec;

            /* Die Spur bekommt EINE Sektorgroesse — die des ersten.
             * TD0 laesst je Sektor eine eigene zu, IMD ebenfalls
             * (`has_varsizes`), und dieser Wandler nutzt das nicht:
             * gemischte Groessen werden auf die erste geebnet. Das ist
             * das Merkmal VAR_SECTOR_SZ aus `uft_format_traegt()` und
             * damit ein benannter Verlust, kein stiller — siehe
             * P3-524. */
            uint8_t code = imd_groessencode(t.sectors[0].data_len);
            if (code == 0xFF) code = 2;             /* 512, wie bisher */
            it->header.sector_size = code;
            it->header.mode = guess_imd_mode_mfm(it->header.nsectors, code);

            const uint16_t ssize = uft_imd_ssize_to_bytes(code);
            const size_t gesamt = nsec * (size_t)ssize;
            if (gesamt == 0) { uft_track_cleanup(&t); continue; }

            it->data = (uint8_t *)malloc(gesamt);
            if (!it->data) { uft_track_cleanup(&t); uft_imd_free(imd); return -1; }
            it->data_size = gesamt;

            size_t off = 0;
            for (size_t k = 0; k < nsec; k++) {
                const uft_sector_t *p = &t.sectors[k];
                it->smap[k] = (uint8_t)p->id.sector;
                it->sector_offsets[k] = off;

                bool compressed = false;
                if (p->data && p->data_len >= ssize) {
                    memcpy(it->data + off, p->data, ssize);
                    compressed = true;
                    for (uint16_t i = 1; i < ssize; i++)
                        if (p->data[i] != p->data[0]) { compressed = false; break; }
                } else if (p->data) {
                    size_t m = p->data_len < ssize ? p->data_len : ssize;
                    memcpy(it->data + off, p->data, m);
                    if (m < ssize) memset(it->data + off + m, 0, ssize - m);
                } else {
                    memset(it->data + off, 0, ssize);
                }

                it->stype[k] = imd_stype_aus_sektor(p, p->data ? compressed : false);
                off += ssize;

                imd->total_sectors++;
                if (it->stype[k] == UFT_IMD_SEC_UNAVAIL) imd->unavail_sectors++;
                if (compressed)                            imd->compressed_sectors++;
            }

            uft_track_cleanup(&t);
            n++;
        }
    }
    imd->num_tracks = n;
    return UFT_OK;
}

/* MF-1287: `uft_td0_to_raw()` ist weg. Sie tat nichts als
 * `uft_td0_to_imd()` + `uft_imd_to_raw()`, und ihr einziger Aufrufer
 * macht diese zwei Schritte jetzt selbst — weil er dann die SPURZAHL
 * aus der IMD nehmen kann statt sie aus der Geometrie zu raten. */
