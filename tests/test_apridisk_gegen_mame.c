/**
 * @file test_apridisk_gegen_mame.c
 * @brief APRIDISK gegen MAMEs `apridisk.cpp` (MF-1009).
 *
 * ── Der Befund ──────────────────────────────────────────────────────
 *
 * `apridisk` stand auf **T3**. Der Feldabgleich gegen MAMEs
 * `src/lib/formats/apridisk.cpp` (BSD-3-Clause, Dirk Best) ergibt:
 * **UFTs Leser kann keine echte APRIDISK-Datei lesen**, und zwar aus
 * drei unabhaengigen Gruenden.
 *
 * **(1) Die Satztypen haben die obere Haelfte verloren — und Sektor
 * und Kommentar sind vertauscht.**
 *
 *     Satz          MAME (Orakel)   UFT (vorher)
 *     geloescht     0xE31D0000      0x00000000
 *     SEKTOR        0xE31D0001      0x00000002   <- vertauscht
 *     Kommentar     0xE31D0002      0x00000001   <- vertauscht
 *     Erzeuger      0xE31D0003      0x00000003
 *
 * **(2) `compression` und `header_size` sind 16 Bit, nicht 32.**
 * MAME liest `get_u16le(&sector_header[4])` und
 * `get_u16le(&sector_header[6])`; UFTs `apridisk_record_desc_t` hatte
 * vier `uint32_t`. Damit liegt ALLES nach dem Typfeld zwei Byte
 * falsch — und die Kompressionswerte selbst waren ebenfalls erfunden
 * (`0x9E90` roh / `0x3E5A` gepackt gegen `0` / `1`).
 *
 * **(3) Die Sektorfelder liegen INNERHALB des 16-Byte-Kopfs, nicht
 * dahinter.**
 *
 *     MAME:  [12] head   [13] sector (1-basiert)   [14..15] u16le track
 *     UFT:   separater 8-Byte-Deskriptor mit
 *            cylinder, head, sector, size_code
 *
 * Andere Stelle, andere Reihenfolge, anderes Feld (`size_code` gibt es
 * im Format nicht — die Sektorgroesse ist mit 512 fest).
 *
 * Das ist die Klasse aus **MF-961** (`86f` probte auf „86BX", ein
 * Magic, das in keiner echten Datei steht), nur schwerer: dort war eine
 * Kennung erfunden, hier der ganze Satzaufbau.
 *
 * ── Die Pruefdatei ──────────────────────────────────────────────────
 *
 * Von Hand nach MAMEs `load()` gebaut, jede Zahl steht hier:
 *
 *     Kopf     128 Byte, Magic "ACT Apricot disk image\x1A\x04"
 *     Satz     16 Byte: [0..3] u32le Typ, [4..5] u16le Kompression,
 *              [6..7] u16le Kopfgroesse, [8..11] u32le Datengroesse,
 *              [12] Kopf, [13] Sektor, [14..15] u16le Spur
 *     roh      Kompression 0x9E90, Datengroesse 512, dann 512 Byte
 *     gepackt  Kompression 0x3E5A, Datengroesse 3:
 *              [0..1] u16le Laenge (MUSS 512 sein), [2] Fuellbyte
 *
 * Die Diskette: **2 Spuren, 1 Kopf, 9 Sektoren** zu 512 Byte. Dazu ein
 * ERZEUGER-Satz (0xE31D0003) mitten hinein, damit belegt ist, dass
 * unbekannte Saetze uebersprungen werden statt den Lauf zu beenden.
 *
 * Spur 1 / Sektor 5 ist **gepackt** — der einzige Sektor, der nicht
 * roh vorliegt.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "uft/formats/uft_apridisk.h"
#include "uft/uft_types.h"

static int gruen = 0;
static int rot = 0;

static void pruefe(const char *name, int bedingung, const char *hinweis)
{
    if (bedingung) {
        printf("  [OK]   %s\n", name);
        gruen++;
    } else {
        printf("  [ROT]  %s%s%s\n", name, hinweis ? "  -- " : "",
               hinweis ? hinweis : "");
        rot++;
    }
}

/* Werte aus dem Orakel, hier bewusst noch einmal ausgeschrieben statt
 * aus dem UFT-Header genommen: der Test darf nicht dieselbe Konstante
 * pruefen, die er belegt (MF-913). */
#define O_TYP_GELOESCHT  0xE31D0000u
#define O_TYP_SEKTOR     0xE31D0001u
#define O_TYP_KOMMENTAR  0xE31D0002u
#define O_TYP_ERZEUGER   0xE31D0003u
#define O_ROH            0x9E90u
#define O_GEPACKT        0x3E5Au
#define O_KOPFGROESSE    128
#define O_SEKTORGROESSE  512

#define SPUREN     2
#define SEKTOREN   9
#define FUELL      0x7E     /* Fuellbyte des gepackten Sektors */
#define G_SPUR     1
#define G_SEKTOR   5

typedef struct { uint8_t *p; size_t n, kap; } puffer_t;

static void schreib(puffer_t *b, const void *q, size_t n)
{
    if (b->n + n > b->kap) {
        b->kap = (b->n + n) * 2 + 256;
        b->p = (uint8_t *)realloc(b->p, b->kap);
    }
    memcpy(b->p + b->n, q, n);
    b->n += n;
}

static void le16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFF); p[1] = (uint8_t)(v >> 8);
}

static void le32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFF);  p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF); p[3] = (uint8_t)((v >> 24) & 0xFF);
}

/** Kennbyte je Sektor, damit eine Verwechslung sicher auffaellt. */
static uint8_t marke(int spur, int sektor)
{
    return (uint8_t)(0x40 + spur * 16 + sektor);
}

static void satzkopf(puffer_t *b, uint32_t typ, uint16_t komp,
                     uint32_t datengroesse, uint8_t kopf, uint8_t sektor,
                     uint16_t spur)
{
    uint8_t h[16];
    memset(h, 0, sizeof(h));
    le32(h + 0, typ);
    le16(h + 4, komp);
    le16(h + 6, 16);                  /* Kopfgroesse = Schrittweite */
    le32(h + 8, datengroesse);
    h[12] = kopf;
    h[13] = sektor;
    le16(h + 14, spur);
    schreib(b, h, sizeof(h));
}

static uint8_t *baue_apridisk(size_t *out_size)
{
    puffer_t b = { NULL, 0, 0 };

    uint8_t kopf[O_KOPFGROESSE];
    memset(kopf, 0, sizeof(kopf));
    memcpy(kopf, "ACT Apricot disk image\032\004", 24);
    schreib(&b, kopf, sizeof(kopf));

    for (int spur = 0; spur < SPUREN; spur++) {
        for (int s = 1; s <= SEKTOREN; s++) {
            if (spur == G_SPUR && s == G_SEKTOR) {
                /* gepackt: Laenge 512 + Fuellbyte, Datengroesse 3 */
                satzkopf(&b, O_TYP_SEKTOR, O_GEPACKT, 3,
                         0, (uint8_t)s, (uint16_t)spur);
                uint8_t c[3];
                le16(c, O_SEKTORGROESSE);
                c[2] = FUELL;
                schreib(&b, c, sizeof(c));
            } else {
                satzkopf(&b, O_TYP_SEKTOR, O_ROH, O_SEKTORGROESSE,
                         0, (uint8_t)s, (uint16_t)spur);
                uint8_t *d = (uint8_t *)malloc(O_SEKTORGROESSE);
                memset(d, marke(spur, s), O_SEKTORGROESSE);
                schreib(&b, d, O_SEKTORGROESSE);
                free(d);
            }
            /* Nach dem dritten Sektor der Spur 0 ein ERZEUGER-Satz:
             * ein unbekannter Typ darf den Lauf nicht beenden. */
            if (spur == 0 && s == 3) {
                const char *wer = "UFT-Pruefstand";
                satzkopf(&b, O_TYP_ERZEUGER, O_ROH,
                         (uint32_t)strlen(wer), 0, 0, 0);
                schreib(&b, wer, strlen(wer));
            }
        }
    }

    *out_size = b.n;
    return b.p;
}

int main(void)
{
    printf("=== APRIDISK gegen MAME apridisk.cpp (MF-1009) ===\n");

    size_t n = 0;
    uint8_t *datei = baue_apridisk(&n);
    if (!datei) { printf("  [ROT]  kein Speicher\n"); return 1; }
    printf("  (Pruefdatei: %zu Byte, %d Spuren x 1 Kopf x %d Sektoren)\n",
           n, SPUREN, SEKTOREN);

    int konf = -1;
    pruefe("die Sonde erkennt die Datei",
           uft_apridisk_probe(datei, n, &konf) && konf >= 80,
           "das Magic steht am Anfang, es ist ein echtes APRIDISK");

    uft_disk_image_t *disk = NULL;
    apridisk_read_result_t erg;
    uft_error_t rc = uft_apridisk_read_mem(datei, n, &disk, &erg);

    char h[210];
    snprintf(h, sizeof(h),
             "rc=%d, detail=%s -- die Datei ist nach MAMEs load() gebaut",
             (int)rc, erg.error_detail ? erg.error_detail : "(keins)");
    pruefe("sie laesst sich oeffnen", rc == UFT_OK && disk != NULL, h);
    if (rc != UFT_OK || !disk) {
        free(datei);
        printf("\n%d gruen, %d rot\n", gruen, rot);
        return 1;
    }

    snprintf(h, sizeof(h), "gemessen %ux%ux%u, %u Byte/Sektor",
             disk->tracks, disk->heads, disk->sectors_per_track,
             disk->bytes_per_sector);
    pruefe("Geometrie: 2 Spuren, 1 Kopf, 9 Sektoren, 512 Byte",
           disk->tracks == SPUREN && disk->heads == 1
           && disk->sectors_per_track == SEKTOREN
           && disk->bytes_per_sector == O_SEKTORGROESSE, h);

    /* Jeder rohe Sektor traegt seine Marke. Der Erzeuger-Satz mitten
     * in Spur 0 darf daran nichts geaendert haben. */
    int roh_ok = 1;
    char wo[80] = "";
    for (int spur = 0; spur < SPUREN && roh_ok; spur++) {
        uft_track_t *tr = disk->track_data ? disk->track_data[spur] : NULL;
        if (!tr || tr->sector_count < SEKTOREN) {
            roh_ok = 0;
            snprintf(wo, sizeof(wo), "Spur %d hat %lu Sektoren", spur,
                     tr ? (unsigned long)tr->sector_count : 0UL);
            break;
        }
        for (int s = 1; s <= SEKTOREN; s++) {
            if (spur == G_SPUR && s == G_SEKTOR) continue;
            uint8_t *d = tr->sectors[s - 1].data;
            if (!d || d[0] != marke(spur, s)) {
                roh_ok = 0;
                snprintf(wo, sizeof(wo),
                         "Spur %d Sektor %d traegt 0x%02X statt 0x%02X",
                         spur, s, d ? (unsigned)d[0] : 0u,
                         (unsigned)marke(spur, s));
                break;
            }
        }
    }
    pruefe("jeder rohe Sektor traegt seine Marke", roh_ok, wo[0] ? wo : NULL);

    /* Der gepackte Sektor: 512 Byte des Fuellbytes. */
    {
        uft_track_t *tr = disk->track_data ? disk->track_data[G_SPUR] : NULL;
        uint8_t *d = (tr && tr->sector_count >= G_SEKTOR)
                     ? tr->sectors[G_SEKTOR - 1].data : NULL;
        int voll = (d != NULL);
        if (voll)
            for (int i = 0; i < O_SEKTORGROESSE; i++)
                if (d[i] != FUELL) { voll = 0; break; }
        snprintf(h, sizeof(h),
                 "Spur %d Sektor %d: erstes Byte 0x%02X, erwartet 0x%02X "
                 "ueber alle 512 -- Kompression 0x3E5A, Laenge 512, "
                 "Fuellbyte", G_SPUR, G_SEKTOR, d ? (unsigned)d[0] : 0u,
                 (unsigned)FUELL);
        pruefe("der gepackte Sektor ist auf 512 Byte entpackt", voll, h);
    }

    uft_disk_free(disk);
    free(datei);

    /* Gegenprobe: eine Datei OHNE das Magic darf nicht angenommen
     * werden — sonst waere die Sonde auch bei einem Leser gruen, der
     * alles nimmt. */
    {
        uint8_t fremd[256];
        for (size_t i = 0; i < sizeof(fremd); i++)
            fremd[i] = (uint8_t)(i * 31u + 7u);
        konf = -1;
        pruefe("eine Datei ohne das Magic wird abgewiesen",
               !uft_apridisk_probe(fremd, sizeof(fremd), &konf), NULL);
    }

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot == 0 ? 0 : 1;
}
