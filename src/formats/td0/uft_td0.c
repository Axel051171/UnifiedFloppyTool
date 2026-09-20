/**
 * @file uft_td0.c
 * @brief Teledisk (TD0) Format Plugin - API-konform
 */

#include "uft/uft_format_common.h"
#include "uft/uft_format_probe.h"   /* MF-1231: uft_format_variant_t */
/* MF-1284: `uft_td0_lzss_init()` / `uft_td0_lzss_getbyte()` und ihr
 * Zustandstyp. Den Typ hier nachzubilden waere die zweite Kopie aus
 * §MF-1177 — er ist 4 KB Schiebefenster plus Huffman-Baeume. */
#include "uft/formats/uft_td0.h"

/* Teledisk signature, read with uft_read_le16() (p[0] | p[1] << 8):
 *   normal / RLE      on-disk bytes 'T','D'  ->  0x4454
 *   advanced / Huffman on-disk bytes 't','d' ->  0x6474
 *
 * This constant used to be 0x5444, which is the byte-swapped value and
 * therefore matches a file starting with "DT" — a thing that does not exist.
 * Uncompressed TD0 images were consequently never recognised by this plugin.
 * Authority: src/samdisk/td0.cpp:10 compares the signature as the byte string
 * "TD" via memcmp (no endianness involved); the repo's own second TD0 reader
 * (src/formats/td0/uft_td0_parser_v2.c:37) and include/uft/uft_formats_extended.h:120
 * both already had it right. See docs/KNOWN_ISSUES.md FMT-13 (MF-389). */
#define TD0_MAGIC_NORMAL    0x4454
#define TD0_MAGIC_ADVANCED  0x6474
#define TD0_HEADER_SIZE     12

/* MF-1284: hier standen `FILE* file` und `long data_start`, und gelesen
 * wurde mit blankem `fread`.
 *
 * `compressed` wurde gesetzt und NIRGENDS benutzt. Bei Magie `td` hat
 * das Plugin damit den Huffman-Strom selbst als Spur- und Sektorkoepfe
 * gedeutet. Gemessen am Vorzustand meldete `Transylvania.td0`
 * **188 Zylinder x 192 Sektoren** und lieferte ueber alle 376
 * „gelesenen" Spuren **null** Sektoren — mit `UFT_OK`.
 *
 * Der Strom liegt jetzt entpackt im Speicher. Das ist keine Bequemlich-
 * keit, sondern notwendig: ein LZH-Strom ist ZUSTANDSBEHAFTET, ein
 * `fseek` zurueck an den Datenanfang gibt es darin nicht — und genau
 * das tat `td0_read_track()` bei jedem Aufruf. */
/* MF-1285: hier stand `td0_data_t`. Was es trug — Strom, Laenge,
 * Datenanfang, Kopffelder — heisst jetzt `uft_td0_strom_t` und steht im
 * Header, weil der Speicher-Wandler DIESELBE Sache braucht und kein
 * `uft_disk_t` hat (ARCH-6). `disk->plugin_data` zeigt auf einen.
 *
 * Der Grund fuer den Umzug ist nicht Ordnung, sondern eine Messung:
 * `uft_td0_read_mem()` und dieses Plugin waren zwei Leser fuer ein
 * Format, und jeder konnte etwas, das dem anderen fehlte. */

/* Obergrenze des entpackten Stroms. Die groesste Geometrie der eigenen
 * Formattafel ist `myz80` mit 64 x 1 x 128 x 1024 = 8,4 MB; 64 MB ist
 * also reichlich. Die Grenze KAPPT nicht — sie sagt ab (Dauerregel D5,
 * und die Klasse MF-1040, wo ein Block auf 65 535 gekuerzt und als
 * guter Sektor gemeldet wurde). */
#define TD0_STROM_MAX (64u * 1024u * 1024u)

/**
 * @brief Entpackt den LZH-Strom einer `td`-Datei vollstaendig.
 *
 * Eigene Funktion, damit jeder ihrer Zweige pruefbar ist, ohne den
 * Leser zu mutieren (Bauform MF-1283).
 */
static uft_error_t td0_strom_entpacken(const uint8_t *quelle, size_t quell_len,
                                       uint8_t **out, size_t *out_len)
{
    uft_td0_lzss_state_t lzss;
    uft_td0_lzss_init(&lzss, quelle, quell_len);

    size_t kap = quell_len * 4u + 4096u;
    if (kap > TD0_STROM_MAX) kap = TD0_STROM_MAX;
    uint8_t *buf = malloc(kap);
    if (!buf) return UFT_ERR_MEMORY;

    size_t n = 0;
    int c;
    while ((c = uft_td0_lzss_getbyte(&lzss)) >= 0) {
        if (n == kap) {
            if (kap >= TD0_STROM_MAX) { free(buf); return UFT_ERR_FORMAT_INVALID; }
            size_t neu = (kap > TD0_STROM_MAX / 2u) ? TD0_STROM_MAX : kap * 2u;
            uint8_t *groesser = realloc(buf, neu);
            if (!groesser) { free(buf); return UFT_ERR_MEMORY; }
            buf = groesser;
            kap = neu;
        }
        buf[n++] = (uint8_t)c;
    }
    if (n == 0) { free(buf); return UFT_ERR_FORMAT_INVALID; }

    *out = buf;
    *out_len = n;
    return UFT_OK;
}

static const uint16_t td0_sector_sizes[8] = { 128, 256, 512, 1024, 2048, 4096, 8192, 16384 };

bool td0_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    if (size < 2) return false;
    uint16_t magic = uft_read_le16(data);
    if (magic == TD0_MAGIC_NORMAL || magic == TD0_MAGIC_ADVANCED) {
        *confidence = 95;
        return true;
    }
    return false;
}

int uft_td0_strom_aus_bytes(const uint8_t *daten, size_t len,
                            uft_td0_strom_t *aus)
{
    if (!daten || !aus) return UFT_ERR_NULL_POINTER;
    memset(aus, 0, sizeof(*aus));
    if (len < TD0_HEADER_SIZE) return UFT_ERR_FORMAT_INVALID;

    const uint8_t *header = daten;
    uint16_t magic = uft_read_le16(header);
    if (magic != TD0_MAGIC_NORMAL && magic != TD0_MAGIC_ADVANCED)
        return UFT_ERR_FORMAT_INVALID;

    aus->version   = header[4];
    aus->data_rate = header[5];
    aus->sides     = header[9];
    aus->gepackt   = (magic == TD0_MAGIC_ADVANCED);

    /* MF-1284: den Rest der Datei holen und — bei `td` — entpacken.
     * Danach gibt es kein `FILE*` mehr; alles Weitere laeuft ueber den
     * Puffer, und `open` und `read_track` benutzen damit ZWINGEND
     * dieselbe Quelle. Vorher waren es zwei Laeufe mit zwei
     * Ueberspring-Regeln: `open` sprang `fseek(f, len, ...)`,
     * `read_track` nur `if (data_len > 1)` — bei einem Datensatz von
     * genau einem Byte drifteten die beiden auseinander (§MF-1177). */
    size_t rest_len = len - TD0_HEADER_SIZE;
    if (rest_len == 0) return UFT_ERR_FORMAT_INVALID;

    if (aus->gepackt) {
        uft_error_t e = td0_strom_entpacken(daten + TD0_HEADER_SIZE, rest_len,
                                            &aus->strom, &aus->strom_len);
        if (e != UFT_OK) return (int)e;
    } else {
        /* Auch der unkomprimierte Fall bekommt eine eigene Kopie: der
         * Aufrufer darf `daten` nach der Rueckkehr freigeben, und
         * `anmerkung.text` zeigt in den Strom. */
        aus->strom = malloc(rest_len);
        if (!aus->strom) return UFT_ERR_MEMORY;
        memcpy(aus->strom, daten + TD0_HEADER_SIZE, rest_len);
        aus->strom_len = rest_len;
    }

    /* MF-971: hier stand `if (pdata->version >= 0x10)`.
     *
     * Der Kommentarblock haengt am FLAG, nicht an der Version. Byte 7
     * des Kopfes (`bTrackDensity`) traegt in Bit 7 die Angabe, ob einer
     * folgt. Vier voneinander unabhaengige Quellen sagen dasselbe:
     *
     *   src/samdisk/td0.cpp:28   "Optional comment block, present if
     *                             bit 7 is set in bTrackDensity above"
     *                            :208  if (th.bTrackDensity & 0x80)
     *   src/formats/td0/uft_td0_lzss.c:469
     *                            if (img->header.stepping & 0x80)
     *   libdisk/teledisk.c:341 (KCemu 0.5.1)
     *                            if ((h.track_density & 0x80) != 0)
     *   Die Kopfbelegung selbst — die Dichtewerte 0/1/2 belegen nur die
     *   unteren Bits.
     *
     * Die ERSTE davon ist die Referenz, mit der `VERIFICATION_TIERS.md`
     * TD0 auf T2 fuehrt. Sie liegt im eigenen Baum, und das Plugin
     * widersprach ihr.
     *
     * Gemessen an gebauten Pruefdateien (2 Spuren, 9 Sektoren):
     *
     *   Version 0x15, Flag GELOESCHT -> gelesen 1 Zylinder / 16 Sektoren
     *   Version 0x09, Flag GESETZT   -> gelesen 33 Zylinder / 84 Sektoren
     *
     * Beide Male mit `UFT_OK`. Im ersten Fall wurden 10 Byte Spurdaten
     * als Kommentarkopf gelesen und danach eine Laenge uebersprungen,
     * die aus Spurdaten stammte; im zweiten begann die Spursuche im
     * Kommentartext. `header[7]` wurde bis hierher gar nicht gelesen. */
    size_t pos = 0;
    if (header[7] & 0x80) {
        if (aus->strom_len < 10) {
            uft_td0_strom_frei(aus); return UFT_ERR_FORMAT_INVALID;
        }
        uint16_t com_len = uft_read_le16(aus->strom + 2);
        if ((size_t)10u + com_len > aus->strom_len) {
            /* Der Kommentar reicht ueber das Stromende — die Laenge ist
             * gelogen oder die Datei ist abgeschnitten. Absagen, nicht
             * kappen (D5). */
            uft_td0_strom_frei(aus); return UFT_ERR_FORMAT_INVALID;
        }
        /* MF-1285: hier wurde der Block bisher nur UEBERSPRUNGEN.
         * `uft_td0_read_mem()` las ihn, und `uft_td0_to_imd()` holt
         * daraus Zeitstempel und Kommentartext — wer `read_mem` loescht,
         * ohne das hier zu behalten, verliert still das METADATA-Merkmal,
         * das `uft_format_traegt()` fuer TD0 UND IMD als getragen fuehrt
         * (MF-1283). */
        aus->anmerkung.vorhanden = true;
        aus->anmerkung.crc      = uft_read_le16(aus->strom + 0);
        aus->anmerkung.jahr     = aus->strom[4];
        aus->anmerkung.monat    = aus->strom[5];
        aus->anmerkung.tag      = aus->strom[6];
        aus->anmerkung.stunde   = aus->strom[7];
        aus->anmerkung.minute   = aus->strom[8];
        aus->anmerkung.sekunde  = aus->strom[9];
        aus->anmerkung.text     = (const char *)(aus->strom + 10);
        aus->anmerkung.text_len = com_len;
        pos = (size_t)10u + com_len;
    }
    aus->daten_start = pos;

    // Scan for geometry
    uint8_t max_cyl = 0, max_sec = 0;
    while (pos + 4u <= aus->strom_len) {
        const uint8_t *trk_hdr = aus->strom + pos;
        if (trk_hdr[0] == 0xFF) break;

        uint8_t num_sec = trk_hdr[0], cyl = trk_hdr[1];
        pos += 4u;
        if (cyl > max_cyl) max_cyl = cyl;
        if (num_sec > max_sec) max_sec = num_sec;

        for (int s = 0; s < num_sec; s++) {
            if (pos + 6u > aus->strom_len) { pos = aus->strom_len; break; }
            uint8_t sec_flags = aus->strom[pos + 4];
            pos += 6u;
            if (!(sec_flags & 0x30)) {
                if (pos + 2u > aus->strom_len) { pos = aus->strom_len; break; }
                uint16_t len = uft_read_le16(aus->strom + pos);
                pos += 2u;
                /* The data record after the length word is exactly `len` bytes
                 * (byte 0 = encoding method, rest = encoded data) — read_track
                 * consumes `len` bytes here, so the geometry scan must skip the
                 * same `len`. The previous `len - 1` drifted one byte per data
                 * sector and mis-scanned the geometry of any multi-sector TD0. */
                if (pos + len > aus->strom_len) { pos = aus->strom_len; break; }
                pos += len;
            }
        }
    }

    aus->zylinder = (unsigned)max_cyl + 1u;
    aus->sektoren = max_sec;
    return UFT_OK;
}

void uft_td0_strom_frei(uft_td0_strom_t *s)
{
    if (!s) return;
    free(s->strom);
    memset(s, 0, sizeof(*s));
}

static uft_error_t td0_open(uft_disk_t* disk, const char* path, bool read_only) {
    (void)read_only;

    FILE* f = fopen(path, "rb");
    if (!f) return UFT_ERR_FILE_OPEN;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERR_IO; }
    long groesse = ftell(f);
    if (groesse < (long)TD0_HEADER_SIZE) { fclose(f); return UFT_ERR_FORMAT_INVALID; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERR_IO; }

    uint8_t *roh = malloc((size_t)groesse);
    if (!roh) { fclose(f); return UFT_ERR_MEMORY; }
    if (fread(roh, 1, (size_t)groesse, f) != (size_t)groesse) {
        free(roh); fclose(f); return UFT_ERR_IO;
    }
    fclose(f);

    uft_td0_strom_t *s = calloc(1, sizeof(*s));
    if (!s) { free(roh); return UFT_ERR_MEMORY; }

    int rc = uft_td0_strom_aus_bytes(roh, (size_t)groesse, s);
    free(roh);                      /* der Strom hat seine eigene Kopie */
    if (rc != UFT_OK) { free(s); return (uft_error_t)rc; }

    disk->plugin_data = s;
    disk->geometry.cylinders = s->zylinder;
    disk->geometry.heads = s->sides;
    disk->geometry.sectors = s->sektoren;
    disk->geometry.sector_size = 512;
    disk->geometry.total_sectors =
        (uint32_t)s->zylinder * s->sides * s->sektoren;

    return UFT_OK;
}

static void td0_close(uft_disk_t* disk) {
    uft_td0_strom_t* s = disk->plugin_data;
    if (s) {
        uft_td0_strom_frei(s);   /* MF-1284: vorher `fclose(pdata->file)` */
        free(s);
        disk->plugin_data = NULL;
    }
}

int uft_td0_strom_spur(const uft_td0_strom_t *p, int cyl, int head,
                       uft_track_t *track) {
    /* MF-519: negative Koordinaten abweisen, BEVOR mit ihnen
     * gerechnet oder indiziert wird. Eine Pruefung, die nur nach
     * oben schaut (`if (cyl >= tracks)`), laesst -1 durch — und
     * `track_data[-1]` ist ein Zugriff vor dem Feld. Gefunden an
     * opus_read_track() von tests/test_disk_open_fuzz.c. */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    if (!p || !p->strom || !track) return UFT_ERR_INVALID_ARG;

    uft_track_init(track, cyl, head);

    /* MF-1284: hier stand `fseek(p->file, p->data_start, SEEK_SET)`.
     * In einem LZH-Strom gibt es kein Zurueckspringen — der Entpacker
     * ist zustandsbehaftet. Der Strom liegt deshalb seit MF-1284
     * entpackt im Speicher, und der Anfang ist ein Index. */
    size_t pos = p->daten_start;

    while (pos + 4u <= p->strom_len) {
        const uint8_t *trk_hdr = p->strom + pos;
        if (trk_hdr[0] == 0xFF) break;  /* End marker */

        uint8_t num_sec = trk_hdr[0];
        uint8_t trk_cyl = trk_hdr[1];
        uint8_t trk_head = trk_hdr[2];
        /* trk_hdr[3] = CRC */
        pos += 4u;

        bool is_target = (trk_cyl == cyl && trk_head == head);

        for (int s = 0; s < num_sec; s++) {
            if (pos + 6u > p->strom_len) goto done;
            const uint8_t *sec_hdr = p->strom + pos;
            pos += 6u;

            uint8_t sec_cyl = sec_hdr[0];
            uint8_t sec_head = sec_hdr[1];
            uint8_t sec_num = sec_hdr[2];
            uint8_t sec_size_code = sec_hdr[3];
            uint8_t sec_flags = sec_hdr[4];

            uint16_t sec_size = (sec_size_code < 7) ? (128 << sec_size_code) : 512;

            if (sec_flags & 0x30) {
                /* No data for this sector */
                if (is_target) {
                    uft_format_add_empty_sector(track, sec_num > 0 ? sec_num - 1 : 0,
                                                 (uint16_t)sec_size, 0xE5,
                                                 (uint8_t)cyl, (uint8_t)head);
                    /* MF-981: die Zeile darueber sagt es selbst — „No data
                     * for this sector". `uft_format_add_empty_sector()`
                     * geht durch `uft_format_add_sector_with_id()`, und der
                     * setzt UFT_SECTOR_OK und beide CRC-Flags auf „gut".
                     * Die 0xE5 waeren damit von echten Daten nicht zu
                     * unterscheiden gewesen. Kein `fread`, kein `memset` —
                     * Tor 62 sieht diesen Weg nicht. */
                    uft_format_mark_last_missing(track);
                    if (track->sector_count > 0) {
                        if (sec_flags & 0x01)
                            uft_sector_set_crc(&track->sectors[track->sector_count - 1], false);
                        if (sec_flags & 0x04)
                            track->sectors[track->sector_count - 1].deleted = true;
                    }
                }
                continue;
            }

            /* Read data length */
            if (pos + 2u > p->strom_len) goto done;
            uint16_t data_len = uft_read_le16(p->strom + pos);
            pos += 2u;

            /* MF-1284: EIN Ueberspringen fuer beide Zweige.
             *
             * Vorher sprang der Nicht-Ziel-Zweig `if (data_len > 1)` —
             * ein Datensatz von genau einem Byte (nur das Verfahrens-
             * byte, keine Nutzlast) wurde also NICHT uebersprungen, und
             * der Lauf verschob sich um ein Byte. `td0_open()` sprang an
             * derselben Stelle unbedingt `len`. Zwei Kopien einer Regel,
             * zwei Bedeutungen — die Bauform aus §MF-1177. */
            if (pos + data_len > p->strom_len) goto done;
            const uint8_t *raw = p->strom + pos;
            pos += data_len;

            if (is_target && data_len > 0) {
                /* TD0 encoding: byte 0 = method (0=raw, 1=repeat, 2=pattern) */
                uint8_t *decoded = calloc(1, sec_size);
                /* MF-981: WIE VIELE Bytes der Dekoder wirklich erzeugt hat.
                 *
                 * `decoded` ist ein genulltes `calloc`, und weiter unten
                 * bekommt `uft_format_add_sector()` immer die volle
                 * `sec_size` uebergeben. Endete der Dekoder frueher, ging
                 * der genullte Rest als Sektorinhalt durch — bei einem
                 * UNBEKANNTEN Verfahrensbyte sogar der ganze Sektor. */
                size_t decoded_len = 0;
                if (decoded) {
                    if (raw[0] == 0 && data_len > 1) {
                        /* Raw data */
                        size_t cp = (data_len - 1 < sec_size) ? data_len - 1 : sec_size;
                        memcpy(decoded, raw + 1, cp);
                        decoded_len = cp;
                    } else if (raw[0] == 1 && data_len >= 5) {
                        /* Repeat: 2-byte count + 2-byte pattern */
                        uint16_t count = uft_read_le16(raw + 1);
                        uint8_t p0 = raw[3], p1 = raw[4];
                        for (uint16_t i = 0; i < count && i * 2 + 1 < sec_size; i++) {
                            decoded[i * 2] = p0;
                            decoded[i * 2 + 1] = p1;
                            decoded_len = (size_t)i * 2 + 2;
                        }
                    } else if (raw[0] == 2 && data_len > 1) {
                        /* Pattern blocks */
                        size_t sp = 1, dp = 0;
                        while (sp < data_len && dp < sec_size) {
                            if (sp + 1 >= data_len) break;
                            uint8_t type = raw[sp++];
                            uint8_t count = raw[sp++];
                            if (type == 0) {
                                /* Literal bytes */
                                for (int i = 0; i < count && sp < data_len && dp < sec_size; i++)
                                    decoded[dp++] = raw[sp++];
                            } else {
                                /* Wiederhol-Lauf.
                                 *
                                 * MF-1063: hier stand `sp += type` und
                                 * `j < type` — der Typ wurde als
                                 * MUSTERLAENGE genommen. Die Musterlaenge
                                 * ist `1 << type`.
                                 *
                                 * Belegt an libdsks `lib/drvtele.c`
                                 * (LGPL-2+, John Elliott; **nur gelesen**,
                                 * Kanal Spec nach MF-695 — keine Zeile
                                 * uebernommen):
                                 *
                                 *     err = tele_fread(self, pattern,
                                 *                      (1 << ptype));
                                 *     memcpy(secbuf + pos, pattern,
                                 *            1 << ptype);
                                 *     pos += (1 << ptype);
                                 *
                                 * Und unabhaengig davon nachgerechnet: der
                                 * erste Sektor eines libdsk-TD0 zerlegt
                                 * sich als `lit 27 | wdh 241x 0000 |
                                 * lit 3` = 27 + 482 + 3 = **genau 512**.
                                 * Mit `type` als Laenge kaeme 27 + 241 = 268
                                 * heraus, und die drei Schlussbytes — bei
                                 * einem PC-Bootsektor `00 55 AA` — fielen
                                 * weg.
                                 *
                                 * **Warum es so lange unbemerkt blieb:**
                                 * das haeufigste Muster ist `00 00`, und
                                 * `decoded` ist ein genulltes `calloc` —
                                 * die fehlenden Byte sahen aus wie die
                                 * richtigen. Erst ein Sektor mit Inhalt
                                 * HINTER dem Nullbereich hat es gezeigt
                                 * (MF-1063; das Fixture traegt deshalb
                                 * einen echten Bootsektor). Bei jedem
                                 * anderen Muster waere der halbe Sektor
                                 * falsch gewesen.
                                 *
                                 * Die MF-981-Kennzeichnung hat dabei
                                 * gehalten: `decoded_len` blieb unter
                                 * `sec_size`, der Sektor wurde als fehlend
                                 * markiert. Falsch war er trotzdem. */
                                const size_t musterlaenge = (size_t)1u << type;
                                size_t pat_start = sp;
                                if (pat_start + musterlaenge > data_len) break;
                                sp += musterlaenge;
                                for (int i = 0; i < count; i++)
                                    for (size_t j = 0; j < musterlaenge
                                                       && dp < sec_size; j++)
                                        decoded[dp++] = raw[pat_start + j];
                            }
                        }
                        decoded_len = dp;
                    }

                    uft_format_add_sector(track, sec_num > 0 ? sec_num - 1 : 0,
                                          decoded, (uint16_t)sec_size,
                                          (uint8_t)cyl, (uint8_t)head);
                    /* MF-981: weniger dekodiert als der Sektor gross ist —
                     * der Rest ist genullter `calloc`, kein Messwert. Bei
                     * einem unbekannten Verfahrensbyte ist `decoded_len`
                     * null und der ganze Sektor erfunden. */
                    if (decoded_len < sec_size)
                        uft_format_mark_last_missing(track);
                    /* Propagate TD0 sector flags */
                    if (track->sector_count > 0) {
                        if (sec_flags & 0x01)
                            uft_sector_set_crc(&track->sectors[track->sector_count - 1], false);
                        if (sec_flags & 0x04)
                            track->sectors[track->sector_count - 1].deleted = true;
                    }
                    free(decoded);
                }
            }
        }

        if (is_target) break;  /* Found our track, done */
    }
done:
    return UFT_OK;
}

/* MF-1285: Der Plugin-Eintrag ist nur noch ein Aufsatz. Die Arbeit macht
 * `uft_td0_strom_spur()`, und sie macht sie fuer den Speicher-Wandler
 * genauso — der hat keinen Pfad und koennte `uft_disk_open()` gar nicht
 * rufen (ARCH-6). Ein Format, ein Leser. */
static uft_error_t td0_read_track(uft_disk_t *disk, int cyl, int head,
                                  uft_track_t *track) {
    if (!disk) return UFT_ERR_INVALID_ARG;
    return (uft_error_t)uft_td0_strom_spur(
        (const uft_td0_strom_t *)disk->plugin_data, cyl, head, track);
}

/* NOTE: write_track omitted by design — TD0 uses LZSS (Teledisk's own
 * LZ-Huffman variant) plus per-sector RLE. Writing a track requires
 * re-compressing the whole image as a stream, which cannot be done
 * track-by-track via the plugin interface. Use a dedicated TD0 writer
 * if round-trip is required. */
static const uft_plugin_feature_t uft_format_plugin_td0_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

/* ── Die zwei Packarten, die dieses Plugin unterscheidet (MF-1231) ───
 *
 * Neu ist hier keine Zahl: `TD0_MAGIC_NORMAL` und `TD0_MAGIC_ADVANCED`
 * stehen oben in dieser Datei, und `td0_probe` entscheidet an ihnen.
 * Die Oberflaeche nannte dieselben zwei als "Normal" und "Advanced"
 * (`m_formatInfo["TD0"]`) — die Namen stimmen, nur wusste sie nichts
 * von der Richtung.
 *
 * Und die ist hier der eigentliche Gewinn: TD0 fuehrt **kein**
 * `UFT_FORMAT_CAP_WRITE` und hat kein `write_track`. Beide Varianten
 * sind also nur lesbar, und `write_note` sagt es statt einer stummen
 * Absage im Speichern-Dialog. */
static int td0_variante_ist_normal(const uint8_t *d, size_t n)
{
    return (d && n >= 2 && uft_read_le16(d) == TD0_MAGIC_NORMAL) ? 1 : 0;
}

static int td0_variante_ist_advanced(const uint8_t *d, size_t n)
{
    return (d && n >= 2 && uft_read_le16(d) == TD0_MAGIC_ADVANCED) ? 1 : 0;
}

static const char TD0_KEIN_SCHREIBER[] =
    "UFT liest TD0, schreibt es aber nicht: das Plugin hat kein "
    "write_track und beansprucht kein UFT_FORMAT_CAP_WRITE.";

static const uft_format_variant_t td0_variants[] = {
    { .name = "Normal", .description = "Teledisk, RLE-gepackt ('TD')",
      .base_format = UFT_FORMAT_TD0,
      .validate = td0_variante_ist_normal,
      .can_read = true, .can_write = false,
      .write_note = TD0_KEIN_SCHREIBER,
      .is_write_default = false },
    { .name = "Advanced", .description = "Teledisk, Huffman-gepackt ('td')",
      .base_format = UFT_FORMAT_TD0,
      .validate = td0_variante_ist_advanced,
      .can_read = true, .can_write = false,
      .write_note = TD0_KEIN_SCHREIBER,
      .is_write_default = false },
};

const uft_format_plugin_t uft_format_plugin_td0 = {
    .name = "TD0",
    .description = "Teledisk Archive",
    .extensions = "td0",
    .version = 0x00010000,
    .format = UFT_FORMAT_TD0,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_VERIFY,
    .variants = td0_variants,
    .variant_count = sizeof(td0_variants) / sizeof(td0_variants[0]),
    .probe = td0_probe,
    .open = td0_open,
    .close = td0_close,
    .read_track = td0_read_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_REVERSE_ENGINEERED,  /* Sydex Teledisk was proprietary; RE'd by Dave Dunfield & wteledsk */
    .features = uft_format_plugin_td0_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_td0_features) / sizeof(uft_format_plugin_td0_features[0]),
};

UFT_REGISTER_FORMAT_PLUGIN(td0)
