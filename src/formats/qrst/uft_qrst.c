/**
 * @file uft_qrst.c
 * @brief QRST (Compaq Quick Release Sector Transfer)
 *
 * ── Referenz ────────────────────────────────────────────────────────
 *
 * `tools/uft-scout/work/libdsk/doc/qrst.html` — die Formatbeschreibung
 * von John Elliott, und dieselbe, die libdsks `lib/drvqrst.c` umsetzt
 * (**LGPL-2+**, John Elliott). **Nur gelesen** — Kanal *Spec* nach
 * MF-695; keine Zeile Quelltext uebernommen. Der Aufbau steht
 * vollstaendig im Kopf von `include/uft/formats/uft_qrst.h`.
 *
 * ── MF-1028: der ganze Aufbau war erfunden ──────────────────────────
 *
 * Bis MF-1028 konnte UFT **keine** echte QRST-Datei lesen, und sein
 * Schreiber erzeugte ein Format, das es nicht gibt. Sieben Abweichungen,
 * jede gegen die Beschreibung gemessen:
 *
 *   1. `QRST_HEADER_SIZE` war **22**; der Kopf ist **796** Byte.
 *   2. Die Kennung galt als vier Byte `"QRST"`; sie ist **fuenf**
 *      (`'QRST',0`). Gemessen: die Sonde nahm `"QRSTX"` mit Konfidenz
 *      **95** an.
 *   3. Der Kopf wurde als `version`/`cylinders`/`heads`/`sectors`/
 *      `sector_size` (u16 ab Versatz 4) gelesen. Dort stehen in
 *      Wirklichkeit drei unbenutzte Bytes, die **Pruefsumme** (0x08)
 *      und der **Kapazitaetskode** (0x0C).
 *   4. Die **Geometrie steht nicht im Kopf** — sie folgt aus dem
 *      Kapazitaetskode ueber eine Tafel von sieben Standardformaten.
 *      Der Kode wurde nie gelesen; gemessen wurde auch ein Kode 8
 *      angenommen.
 *   5. Der Spursatz galt als 8 Byte (`cyl` u16, `head`, `compressed`,
 *      `data_size` u32). Er ist **3** Byte, und das dritte ist der
 *      **Typ**.
 *   6. Es gab zwei Spurarten (keine/RLE); es gibt **drei** — roh,
 *      **leer mit Fuellbyte** und gepackt. Die leere Spur fehlte ganz.
 *   7. Die **Pruefsumme fehlte vollstaendig**, obwohl sie in der Datei
 *      steht: Summe `byte*(1+Versatz)` ueber die ganze Diskette. Das
 *      ist ein Beleg AM OBJEKT und damit die staerkste Abnahme, die
 *      dieses Format hergibt (dieselbe Art wie MF-869 und MF-1013).
 *
 * **Und die Packung ist woertlich die Klasse aus MF-1009.** Dort war es
 * `apridisk` mit „einem Byte-Strom statt drei Byte Laenge plus
 * Fuellbyte, dessen Rundlauftest gruen war, weil Packer und Entpacker
 * Spiegelbilder waren". Hier stand ein Strom, in dem `0x00` ein
 * Wiederhol-Tripel einleitete; wirklich wechseln sich ein Literal-Lauf
 * (`<len>` + `len` Bytes) und ein Wiederhol-Lauf (`<len>` + ein Byte)
 * ab, beginnend mit dem Literal-Lauf. Und wie dort war der
 * Rundlauftest gruen: `tests/test_libdsk_formats.c::qrst_rle_compression`
 * prueft `compress` gegen `decompress` — zwei Spiegel derselben
 * Erfindung.
 *
 * Der alte Dateikopf nannte `libdsk drvqrst.c (LGPL-2.0-or-later;
 * Fassung 1.5.12 geprueft)`. Eine benannte Referenz, deren Verhalten
 * der Code nicht umsetzte — dieselbe Gestalt wie MF-1026, wo
 * `victor9k` MAME nannte und in fuenf Punkten abwich.
 *
 * ── Abnahme ─────────────────────────────────────────────────────────
 *
 * `tests/corpus_free/qrst_spec_160k.qrst` ist nach dieser Beschreibung
 * gebaut und von **fremder Hand nachgewiesen**: libdsks aus dem Klon
 * gebaute Werkzeuge lesen sie (`dskid` meldet 40/1/8/512 samt
 * Beschreibung und Etikett), und `dsktrans -itype qrst -otype raw`
 * stellt die Diskette **163840 von 163840 Byte identisch** wieder her.
 * Alle drei Spursatz-Arten gehen damit durch einen fremden Entpacker —
 * das ist der Unterschied zu MF-1009.
 *
 * Regressionsschutz: `tests/test_qrst_gegen_libdsk.c`.
 */

#include "uft/formats/uft_qrst.h"
#include "uft/uft_format_common.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Kleine Helfer
 * ==========================================================================*/

/* MF-594: die Klammern trugen bis dahin keinen Cast — `p[3] << 24`
 * befoerdert `uint8_t` zu `int`, und ab 128 passt das Ergebnis nicht mehr
 * hinein. In C ist das undefiniert, nicht bloss haesslich; UBSan meldet es,
 * der Uebersetzer nicht. Gefunden vom Fuzzer in uft_apridisk.c:22. */
static uint32_t qrst_le32(const uint8_t *p) {
    return (uint32_t)p[0]         | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t qrst_le16(const uint8_t *p) {
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static void qrst_put_le16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
}

static void qrst_put_le32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}

/* ============================================================================
 * Die Kapazitaetstafel
 *
 * `doc/qrst.html` nennt bei 0x0C sieben Kodes; die Geometrien sind
 * libdsks Standardformate (`include/libdsk.h:134-141`: 160k = 8
 * Sektoren 1 Seite, 180k = 9 Sektoren 1 Seite, 320k = 8 Sektoren
 * 2 Seiten, 360k = 9 Sektoren 2 Seiten).
 * ==========================================================================*/

typedef struct {
    uint8_t  capacity;
    uint8_t  cylinders;
    uint8_t  heads;
    uint8_t  sectors;
    uint16_t sector_size;
} qrst_kapazitaet_t;

static const qrst_kapazitaet_t qrst_tafel[] = {
    { QRST_CAP_360K,  40, 2,  9, 512 },
    { QRST_CAP_1200K, 80, 2, 15, 512 },
    { QRST_CAP_720K,  80, 2,  9, 512 },
    { QRST_CAP_1440K, 80, 2, 18, 512 },
    { QRST_CAP_160K,  40, 1,  8, 512 },
    { QRST_CAP_180K,  40, 1,  9, 512 },
    { QRST_CAP_320K,  40, 2,  8, 512 },
};

bool uft_qrst_geometry(uint8_t capacity, uint8_t *cyl, uint8_t *heads,
                       uint8_t *spt, uint16_t *sector_size) {
    size_t i;
    for (i = 0; i < sizeof(qrst_tafel) / sizeof(qrst_tafel[0]); i++) {
        if (qrst_tafel[i].capacity != capacity) continue;
        if (cyl)         *cyl         = qrst_tafel[i].cylinders;
        if (heads)       *heads       = qrst_tafel[i].heads;
        if (spt)         *spt         = qrst_tafel[i].sectors;
        if (sector_size) *sector_size = qrst_tafel[i].sector_size;
        return true;
    }
    return false;
}

bool uft_qrst_validate_header(const qrst_header_t *header) {
    if (!header) return false;
    return uft_qrst_geometry(header->capacity, NULL, NULL, NULL, NULL);
}

/* ============================================================================
 * Pruefsumme
 *
 * doc/qrst.html: „The checksum is the sum of all bytes on the disc,
 * each byte multiplied by (1 + its offset on the disc)."
 * ==========================================================================*/

uint32_t uft_qrst_checksum(const uint8_t *disc, size_t size) {
    uint32_t summe = 0;
    size_t i;
    if (!disc) return 0;
    for (i = 0; i < size; i++)
        summe += (uint32_t)disc[i] * (uint32_t)(i + 1);
    return summe;
}

/* ============================================================================
 * Packung: abwechselnd Literal-Lauf und Wiederhol-Lauf
 *
 * doc/qrst.html:
 *
 *     The compressed data consists of alternating runs of literal
 *     bytes:   <count> <byte1..byten>
 *     and repeat bytes: <count> <byte_to_repeat>
 *
 * Der Wechsel beginnt beim Literal-Lauf. Ein Lauf der Laenge 0 ist
 * zulaessig und noetig, wenn der Block mit einem Wiederhol-Lauf
 * anfaengt.
 *
 * Die Namen `qrst_rle_*` sind historisch und bleiben, weil
 * `tests/test_libdsk_formats.c` sie ruft; die Regel dahinter ist keine
 * gewoehnliche RLE.
 * ==========================================================================*/

int qrst_rle_decompress(const uint8_t *input, size_t input_size,
                        uint8_t *output, size_t output_size) {
    size_t in_pos = 0, out_pos = 0;
    bool literal = true;

    if (!input || !output) return -1;

    while (in_pos < input_size) {
        uint8_t count = input[in_pos++];
        if (literal) {
            if (in_pos + count > input_size) return -1;
            if (out_pos + count > output_size) return -1;
            memcpy(output + out_pos, input + in_pos, count);
            in_pos  += count;
            out_pos += count;
        } else {
            uint8_t wert;
            if (in_pos >= input_size) return -1;
            wert = input[in_pos++];
            if (out_pos + count > output_size) return -1;
            memset(output + out_pos, wert, count);
            out_pos += count;
        }
        literal = !literal;
    }
    return (int)out_pos;
}

int qrst_rle_compress(const uint8_t *input, size_t input_size,
                      uint8_t *output, size_t output_capacity) {
    size_t in_pos = 0, out_pos = 0;
    bool literal = true;

    if (!input || !output) return -1;

    while (in_pos < input_size) {
        if (literal) {
            /* Literale sammeln, bis ein Lauf von mindestens drei
             * gleichen Bytes beginnt — darunter kostet der
             * Wiederhol-Lauf mehr, als er spart. */
            size_t j = in_pos;
            size_t n;
            while (j < input_size && (j - in_pos) < 255) {
                if (j + 2 < input_size && input[j] == input[j + 1]
                        && input[j] == input[j + 2])
                    break;
                j++;
            }
            n = j - in_pos;
            if (out_pos + 1 + n > output_capacity) return -1;
            output[out_pos++] = (uint8_t)n;
            memcpy(output + out_pos, input + in_pos, n);
            out_pos += n;
            in_pos = j;
        } else {
            uint8_t wert = input[in_pos];
            size_t n = 0;
            while (in_pos + n < input_size && input[in_pos + n] == wert
                   && n < 255)
                n++;
            if (out_pos + 2 > output_capacity) return -1;
            output[out_pos++] = (uint8_t)n;
            output[out_pos++] = wert;
            in_pos += n;
        }
        literal = !literal;
    }
    /* Endet der Block auf einem Literal-Lauf, folgt kein weiterer
     * Satz — der Entpacker hoert auf, wenn die gepackte Laenge
     * erschoepft ist. */
    return (int)out_pos;
}

/* ============================================================================
 * Sonde
 * ==========================================================================*/

bool uft_qrst_probe(const uint8_t *data, size_t size, int *confidence) {
    uint8_t kode;
    if (!data || size < QRST_OFF_CAPACITY + 1) return false;

    /* MF-1028: FUENF Byte. Das Nullbyte gehoert zur Kennung; ohne es
     * nahm die Sonde `"QRSTX"` mit Konfidenz 95 an. */
    if (memcmp(data, "QRST\0", QRST_SIGNATURE_LEN) != 0) return false;

    /* Und der Kapazitaetskode muss einer von sieben sein — er traegt
     * die Geometrie, also ist ein unbekannter Kode keine lesbare
     * Datei, sondern eine, deren Geometrie UFT erfinden muesste. */
    kode = data[QRST_OFF_CAPACITY];
    if (!uft_qrst_geometry(kode, NULL, NULL, NULL, NULL)) return false;

    /* MF-729: 80..100 heisst „Merkmal getroffen". Eine 5-Byte-Kennung
     * plus ein gueltiger Kode ist genau das. */
    if (confidence) *confidence = 90;
    return true;
}

/* ============================================================================
 * Lesen
 * ==========================================================================*/

static void qrst_ergebnis_init(qrst_read_result_t *r) {
    if (!r) return;
    memset(r, 0, sizeof(*r));
}

/** Kopf byteweise auslesen — keine Struktur ueber die Datei legen. */
static bool qrst_kopf_lesen(const uint8_t *data, size_t size,
                            qrst_header_t *h) {
    size_t i;
    if (size < QRST_HEADER_SIZE) return false;
    if (memcmp(data, "QRST\0", QRST_SIGNATURE_LEN) != 0) return false;

    memset(h, 0, sizeof(*h));
    h->checksum = qrst_le32(data + QRST_OFF_CHECKSUM);
    h->capacity = data[QRST_OFF_CAPACITY];
    h->volume   = data[QRST_OFF_VOLUME];
    h->volumes  = data[QRST_OFF_VOLUMES];

    for (i = 0; i < QRST_DESCRIPTION_MAX; i++) {
        char c = (char)data[QRST_OFF_DESCRIPTION + i];
        h->description[i] = c;
        if (c == '\0') break;
    }
    h->description[QRST_DESCRIPTION_MAX] = '\0';
    for (i = 0; i < QRST_LABEL_MAX; i++) {
        char c = (char)data[QRST_OFF_LABEL + i];
        h->label[i] = c;
        if (c == '\0') break;
    }
    h->label[QRST_LABEL_MAX] = '\0';

    return uft_qrst_geometry(h->capacity, &h->cylinders, &h->heads,
                             &h->sectors, &h->sector_size);
}

uft_error_t uft_qrst_read_mem(const uint8_t *data, size_t size,
                              uft_disk_image_t **out_disk,
                              qrst_read_result_t *result) {
    qrst_header_t h;
    uft_disk_image_t *image = NULL;
    uint8_t *disc = NULL;
    size_t tracklen, disc_size, pos;
    uint32_t summe;
    int c, hd, s;

    qrst_ergebnis_init(result);
    if (!data || !out_disk) return UFT_ERR_INVALID_PARAM;
    *out_disk = NULL;

    if (!qrst_kopf_lesen(data, size, &h)) return UFT_ERROR_FORMAT_INVALID;

    tracklen  = (size_t)h.sectors * h.sector_size;
    disc_size = (size_t)h.cylinders * h.heads * tracklen;
    disc = (uint8_t *)calloc(1, disc_size);
    if (!disc) return UFT_ERROR_NO_MEMORY;

    /* ── die Spursaetze abgehen ─────────────────────────────────────
     *
     * Die Saetze stehen in der Reihenfolge der Datei und nennen ihre
     * Lage selbst (Zylinder, Kopf). Es wird NICHT angenommen, dass sie
     * vollstaendig oder geordnet sind; was fehlt, bleibt genullt und
     * wird unten als fehlend gekennzeichnet. */
    pos = QRST_HEADER_SIZE;
    {
        uint8_t *gesehen = (uint8_t *)calloc(1, (size_t)h.cylinders * h.heads);
        if (!gesehen) { free(disc); return UFT_ERROR_NO_MEMORY; }

        while (pos + 3 <= size) {
            const uint8_t tcyl = data[pos];
            const uint8_t thead = data[pos + 1];
            const uint8_t ttyp = data[pos + 2];
            size_t idx;
            pos += 3;

            if (tcyl >= h.cylinders || thead >= h.heads) {
                /* Ein Satz, der eine Spur nennt, die es in dieser
                 * Geometrie nicht gibt. Abweisen statt ueberspringen:
                 * entweder ist der Kapazitaetskode falsch oder die
                 * Datei ist beschaedigt, und beides heisst, dass jede
                 * weitere Auslegung geraten waere. */
                free(gesehen);
                free(disc);
                return UFT_ERROR_FORMAT_INVALID;
            }
            idx = (size_t)tcyl * h.heads + thead;

            if (ttyp == QRST_TRACK_RAW) {
                if (pos + tracklen > size) {
                    free(gesehen); free(disc);
                    return UFT_ERROR_FORMAT_INVALID;
                }
                memcpy(disc + idx * tracklen, data + pos, tracklen);
                pos += tracklen;
                if (result) result->raw_tracks++;
            } else if (ttyp == QRST_TRACK_BLANK) {
                if (pos + 1 > size) {
                    free(gesehen); free(disc);
                    return UFT_ERROR_FORMAT_INVALID;
                }
                memset(disc + idx * tracklen, data[pos], tracklen);
                pos += 1;
                if (result) result->blank_tracks++;
            } else if (ttyp == QRST_TRACK_PACKED) {
                uint16_t clen;
                int n;
                if (pos + 2 > size) {
                    free(gesehen); free(disc);
                    return UFT_ERROR_FORMAT_INVALID;
                }
                clen = qrst_le16(data + pos);
                pos += 2;
                if (pos + clen > size) {
                    free(gesehen); free(disc);
                    return UFT_ERROR_FORMAT_INVALID;
                }
                n = qrst_rle_decompress(data + pos, clen,
                                        disc + idx * tracklen, tracklen);
                if (n < 0 || (size_t)n != tracklen) {
                    free(gesehen); free(disc);
                    return UFT_ERROR_FORMAT_INVALID;
                }
                pos += clen;
                if (result) result->packed_tracks++;
            } else {
                /* MF-1028: eine Satzart, die es nicht gibt, wird
                 * ABGEWIESEN. Sie stillschweigend zu ueberspringen
                 * waere unmoeglich — die Satzlaenge haengt am Typ,
                 * also weiss niemand, wo der naechste Satz beginnt. */
                free(gesehen); free(disc);
                return UFT_ERROR_FORMAT_INVALID;
            }
            gesehen[idx] = 1;
        }

        /* ── das Abbild aufbauen ────────────────────────────────── */
        image = uft_disk_alloc(h.cylinders, h.heads);
        if (!image) { free(gesehen); free(disc); return UFT_ERROR_NO_MEMORY; }
        image->format = UFT_FORMAT_DSK;
        snprintf(image->format_name, sizeof(image->format_name), "QRST");
        image->sectors_per_track = h.sectors;
        image->bytes_per_sector  = h.sector_size;

        for (c = 0; c < h.cylinders; c++) {
            for (hd = 0; hd < h.heads; hd++) {
                const size_t idx = (size_t)c * h.heads + hd;
                uft_track_t *tr = (uft_track_t *)calloc(1, sizeof(uft_track_t));
                if (!tr) { free(gesehen); free(disc); uft_disk_free(image);
                           return UFT_ERROR_NO_MEMORY; }
                uft_track_init(tr, c, hd);
                for (s = 0; s < h.sectors; s++) {
                    const uint8_t *sek = disc + idx * tracklen
                                         + (size_t)s * h.sector_size;
                    /* Sektor-IDs sind 1-basiert: libdsks `.libdskrc`
                     * fuehrt fuer alle ibm/pcw-Formate `SecBase=1`, und
                     * `uft_format_add_sector()` addiert laut eigenem
                     * Kopf 1 auf den 0-basierten Laufindex. Geprueft
                     * und deshalb NICHT auf `_with_id` umgestellt
                     * (anders als bei `jv1`/`victor9k`). */
                    uft_format_add_sector(tr, (uint8_t)s, sek,
                                          h.sector_size, (uint8_t)c,
                                          (uint8_t)hd);
                    if (!gesehen[idx]) {
                        /* MF-980: eine Spur, fuer die kein Satz in der
                         * Datei stand, ist nicht „lauter Nullen" —
                         * sie ist nicht da. Ohne diese Kennzeichnung
                         * waeren die genullten Bytes von echten Daten
                         * nicht zu unterscheiden. */
                        uft_format_mark_last_missing(tr);
                    }
                }
                image->track_data[idx] = tr;
            }
        }
        free(gesehen);
    }

    summe = uft_qrst_checksum(disc, disc_size);
    if (result) {
        result->success = true;
        result->error = UFT_OK;
        result->cylinders   = h.cylinders;
        result->heads       = h.heads;
        result->sectors     = h.sectors;
        result->sector_size = h.sector_size;
        result->total_tracks = (uint32_t)h.cylinders * h.heads;
        result->checksum_header   = h.checksum;
        result->checksum_computed = summe;
        result->checksum_ok = (summe == h.checksum);
        result->original_size   = disc_size;
        result->compressed_size = size;
    }

    if (h.description[0]) {
        image->comment = (char *)malloc(strlen(h.description) + 1);
        if (image->comment) {
            strcpy(image->comment, h.description);
            image->comment_len = strlen(h.description);
        }
    }

    free(disc);
    *out_disk = image;
    return UFT_OK;
}

uft_error_t uft_qrst_read(const char *path,
                          uft_disk_image_t **out_disk,
                          qrst_read_result_t *result) {
    FILE *f;
    uint8_t *buf;
    long len;
    uft_error_t rc;

    qrst_ergebnis_init(result);
    if (!path || !out_disk) return UFT_ERR_INVALID_PARAM;

    f = fopen(path, "rb");
    if (!f) return UFT_ERROR_FILE_OPEN;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_IO; }
    len = ftell(f);
    if (len <= 0) { fclose(f); return UFT_ERROR_IO; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_IO; }

    buf = (uint8_t *)malloc((size_t)len);
    if (!buf) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    if (fread(buf, 1, (size_t)len, f) != (size_t)len) {
        free(buf); fclose(f); return UFT_ERROR_IO;
    }
    fclose(f);

    rc = uft_qrst_read_mem(buf, (size_t)len, out_disk, result);
    free(buf);
    return rc;
}

/* ============================================================================
 * Schreiben
 *
 * Der Schreiber ist spezifikationsgerecht, hat aber weiterhin **keinen
 * Aufrufer** aus dem Plugin-Pfad — `write_track` sagt ab und `close()`
 * schreibt nicht. Das ist P3-204 (MF-930/931) und wird hier NICHT
 * mitverdrahtet: die Verdrahtung ist je Format eine eigene Aufgabe mit
 * eigenem Rundlaufbeweis. Was MF-1028 aendert, ist, dass der Schreiber
 * nicht mehr ein erfundenes Format erzeugt.
 * ==========================================================================*/

void uft_qrst_write_options_init(qrst_write_options_t *opts) {
    if (!opts) return;
    opts->use_compression = true;
}

uft_error_t uft_qrst_write(const uft_disk_image_t *disk,
                           const char *path,
                           const qrst_write_options_t *opts) {
    qrst_write_options_t vorgabe;
    uint8_t kopf[QRST_HEADER_SIZE];
    uint8_t *disc = NULL, *gepackt = NULL;
    size_t tracklen, disc_size;
    uint8_t kode = 0;
    size_t i;
    int c, hd, s;
    FILE *f;

    if (!disk || !path) return UFT_ERR_INVALID_PARAM;
    if (!opts) { uft_qrst_write_options_init(&vorgabe); opts = &vorgabe; }

    /* Kapazitaetskode aus der Geometrie — QRST kann nur die sieben
     * Formate seiner Tafel tragen, und eine Diskette, die nicht
     * hineinpasst, wird ABGEWIESEN statt gerundet. */
    for (i = 0; i < sizeof(qrst_tafel) / sizeof(qrst_tafel[0]); i++) {
        if (qrst_tafel[i].cylinders == disk->tracks
            && qrst_tafel[i].heads == disk->heads
            && qrst_tafel[i].sectors == disk->sectors_per_track
            && qrst_tafel[i].sector_size == disk->bytes_per_sector) {
            kode = qrst_tafel[i].capacity;
            break;
        }
    }
    if (!kode) return UFT_ERROR_NOT_SUPPORTED;

    tracklen  = (size_t)disk->sectors_per_track * disk->bytes_per_sector;
    disc_size = (size_t)disk->tracks * disk->heads * tracklen;
    disc = (uint8_t *)calloc(1, disc_size);
    if (!disc) return UFT_ERROR_NO_MEMORY;

    for (c = 0; c < disk->tracks; c++) {
        for (hd = 0; hd < disk->heads; hd++) {
            const size_t idx = (size_t)c * disk->heads + hd;
            const uft_track_t *tr = disk->track_data
                                    ? disk->track_data[idx] : NULL;
            if (!tr) continue;
            for (s = 0; s < (int)tr->sector_count
                        && s < disk->sectors_per_track; s++) {
                const uft_sector_t *sek = &tr->sectors[s];
                size_t n = sek->data_len < disk->bytes_per_sector
                           ? sek->data_len : disk->bytes_per_sector;
                if (sek->data && n)
                    memcpy(disc + idx * tracklen
                           + (size_t)s * disk->bytes_per_sector,
                           sek->data, n);
            }
        }
    }

    memset(kopf, 0, sizeof(kopf));
    memcpy(kopf, "QRST\0", QRST_SIGNATURE_LEN);
    kopf[QRST_OFF_UNUSED]     = 0x00;
    kopf[QRST_OFF_UNUSED + 1] = 0x80;
    kopf[QRST_OFF_UNUSED + 2] = 0x3F;
    qrst_put_le32(kopf + QRST_OFF_CHECKSUM,
                  uft_qrst_checksum(disc, disc_size));
    kopf[QRST_OFF_CAPACITY] = kode;
    kopf[QRST_OFF_VOLUME]   = 1;
    kopf[QRST_OFF_VOLUMES]  = 1;
    if (disk->comment && disk->comment[0]) {
        size_t n = strlen(disk->comment);
        if (n > QRST_DESCRIPTION_MAX - 1) n = QRST_DESCRIPTION_MAX - 1;
        memcpy(kopf + QRST_OFF_DESCRIPTION, disk->comment, n);
    }

    f = fopen(path, "wb");
    if (!f) { free(disc); return UFT_ERROR_FILE_OPEN; }
    if (fwrite(kopf, 1, sizeof(kopf), f) != sizeof(kopf)) {
        fclose(f); free(disc); return UFT_ERROR_IO;
    }

    if (opts->use_compression) {
        gepackt = (uint8_t *)malloc(tracklen * 2 + 16);
        if (!gepackt) { fclose(f); free(disc); return UFT_ERROR_NO_MEMORY; }
    }

    for (c = 0; c < disk->tracks; c++) {
        for (hd = 0; hd < disk->heads; hd++) {
            const size_t idx = (size_t)c * disk->heads + hd;
            const uint8_t *spur = disc + idx * tracklen;
            uint8_t satz[5];
            int gleich = 1;
            size_t k;

            for (k = 1; k < tracklen; k++)
                if (spur[k] != spur[0]) { gleich = 0; break; }

            if (gleich) {
                satz[0] = (uint8_t)c;
                satz[1] = (uint8_t)hd;
                satz[2] = QRST_TRACK_BLANK;
                satz[3] = spur[0];
                if (fwrite(satz, 1, 4, f) != 4) goto fehler;
                continue;
            }
            if (opts->use_compression) {
                int n = qrst_rle_compress(spur, tracklen, gepackt,
                                          tracklen * 2 + 16);
                if (n > 0 && (size_t)n < tracklen) {
                    satz[0] = (uint8_t)c;
                    satz[1] = (uint8_t)hd;
                    satz[2] = QRST_TRACK_PACKED;
                    qrst_put_le16(satz + 3, (uint16_t)n);
                    if (fwrite(satz, 1, 5, f) != 5) goto fehler;
                    if (fwrite(gepackt, 1, (size_t)n, f) != (size_t)n)
                        goto fehler;
                    continue;
                }
            }
            satz[0] = (uint8_t)c;
            satz[1] = (uint8_t)hd;
            satz[2] = QRST_TRACK_RAW;
            if (fwrite(satz, 1, 3, f) != 3) goto fehler;
            if (fwrite(spur, 1, tracklen, f) != tracklen) goto fehler;
        }
    }

    free(gepackt);
    free(disc);
    if (fclose(f) != 0) return UFT_ERROR_IO;
    return UFT_OK;

fehler:
    free(gepackt);
    free(disc);
    fclose(f);
    return UFT_ERROR_IO;
}

/* ============================================================================
 * Plugin
 * ==========================================================================*/

static bool qrst_probe_plugin(const uint8_t *data, size_t size,
                              size_t file_size, int *confidence) {
    (void)file_size;
    return uft_qrst_probe(data, size, confidence);
}

static uft_error_t qrst_open(uft_disk_t *disk, const char *path,
                             bool read_only) {
    (void)read_only;
    uft_disk_image_t *image = NULL;
    uft_error_t err = uft_qrst_read(path, &image, NULL);
    if (err == UFT_OK && image) {
        disk->plugin_data = image;
        disk->geometry.cylinders = image->tracks;
        disk->geometry.heads = image->heads;
        disk->geometry.sectors = image->sectors_per_track;
        disk->geometry.sector_size = image->bytes_per_sector;
        disk->geometry.total_sectors = (uint32_t)image->tracks * image->heads *
                                       image->sectors_per_track;
    }
    return err;
}

static void qrst_close(uft_disk_t *disk) {
    if (disk && disk->plugin_data) {
        uft_disk_free((uft_disk_image_t*)disk->plugin_data);
        disk->plugin_data = NULL;
    }
}

static uft_error_t qrst_read_track(uft_disk_t *disk, int cyl, int head,
                                    uft_track_t *track) {
    /* MF-519: negative Koordinaten abweisen, BEVOR mit ihnen
     * gerechnet oder indiziert wird. Eine Pruefung, die nur nach
     * oben schaut (`if (cyl >= tracks)`), laesst -1 durch — und
     * `track_data[-1]` ist ein Zugriff vor dem Feld. Gefunden an
     * opus_read_track() von tests/test_disk_open_fuzz.c. */
    if (cyl < 0 || head < 0) return UFT_ERR_INVALID_PARAM;

    uft_disk_image_t *image = (uft_disk_image_t*)disk->plugin_data;
    if (!image || !track) return UFT_ERR_INVALID_PARAM;

    if (cyl >= (int)image->tracks || head >= (int)image->heads)
        return UFT_ERR_INVALID_PARAM;

    size_t idx = (size_t)cyl * image->heads + head;
    uft_track_t *src = image->track_data[idx];
    if (!src) return UFT_ERR_INVALID_PARAM;

    track->cylinder = cyl;
    track->head = head;
    track->encoding = src->encoding;

    /* MF-516: hier stand `track->sectors[s] = src->sectors[s];`.
     *
     * `uft_track_t.sectors` ist ein DYNAMISCHER Zeiger, kein Feld, und
     * `uft_track_init()` legt ihn NICHT an. `track->sectors` war hier
     * also bei JEDEM erfolgreichen Lesen NULL, und die Schleife schrieb
     * hindurch. Derselbe Rumpf stand woertlich in 12 Plugins. */
    for (size_t s = 0; s < src->sector_count; s++) {
        uft_error_t add_err = uft_track_add_sector(track, &src->sectors[s]);
        if (add_err != UFT_OK) return add_err;
    }

    return UFT_OK;
}

static uft_error_t qrst_write_track(uft_disk_t *disk, int cyl, int head,
                                     const uft_track_t *track) {
    /* MF-529: negative Koordinaten abweisen, BEVOR mit ihnen
     * gerechnet oder indiziert wird. Beim SCHREIBEN wiegt das schwerer
     * als beim Lesen: ein falscher Index liefert nicht nur falsche
     * Daten, er bestimmt, WOHIN geschrieben wird. */
    if (cyl < 0 || head < 0) return UFT_ERR_INVALID_PARAM;

    uft_disk_image_t *image = (uft_disk_image_t*)disk->plugin_data;
    if (!image || !track) return UFT_ERR_INVALID_PARAM;
    if (disk->read_only) return UFT_ERR_NOT_SUPPORTED;

    /* MF-930: Hier stand eine Speicher-Mutation, die `UFT_OK` meldete.
     *
     * Diese Datei HAT einen echten Dateischreiber — `uft_qrst_write()`,
     * mit `fwrite` und allem. Nur fuehrt kein Weg dorthin: die
     * Plugin-Tafel hat kein `.flush`, `close()` gibt den Puffer frei
     * ohne zu schreiben, und dieses `write_track` fasste nur den
     * Speicher an. Der Aufrufer bekam Erfolg gemeldet; kein Byte
     * erreichte die Platte.
     *
     * Die Verdrahtung ist je Format eine eigene Aufgabe mit eigenem
     * Rundlaufbeweis, verzeichnet als P3-204 (MF-930/931).
     *
     * **MF-1028 aendert daran nichts — aber es aendert, WAS dort
     * verdrahtet werden wuerde.** Bis MF-1028 haette ein verdrahteter
     * `uft_qrst_write()` ein Format geschrieben, das es nicht gibt:
     * 22-Byte-Kopf, 8-Byte-Spursaetze, ein erfundener Packstrom. Jetzt
     * schreibt er, was `doc/qrst.html` beschreibt, samt Pruefsumme —
     * die Verdrahtung ist damit erst sinnvoll geworden.
     *
     * `write_track` bleibt GESETZT statt NULL: ein Nullzeiger gaebe dem
     * Aufrufer keine Begruendung. */
    return UFT_ERROR_NOT_SUPPORTED;
}

static const uft_plugin_feature_t uft_format_plugin_qrst_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_UNSUPPORTED,
      "MF-930/P3-204: der spezifikationsgerechte uft_qrst_write() in derselben Datei hat keinen Aufrufer — kein flush, close() gibt frei" },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_qrst = {
    .name = "QRST",
    .description = "Compaq Quick Release Sector Transfer",
    .extensions = "qrst",
    .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_VERIFY,
    .probe = qrst_probe_plugin,
    .open = qrst_open,
    .close = qrst_close,
    .read_track = qrst_read_track,
    .write_track = qrst_write_track,
    .verify_track = uft_generic_verify_track,
    /* Geprueft und nicht angefasst: QRST hat keine Herstellerspec,
     * John Elliotts Beschreibung ist selbst eine RE-Referenz. Die
     * Verifikationsstufe traegt das Tier-System. */
    .spec_status = UFT_SPEC_REVERSE_ENGINEERED,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_qrst_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_qrst_features) / sizeof(uft_format_plugin_qrst_features[0]),
};

UFT_REGISTER_FORMAT_PLUGIN(qrst)
