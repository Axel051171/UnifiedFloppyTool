/**
 * @file test_cfi_gegen_samdisk.c
 * @brief CFI Feld fuer Feld gegen `src/samdisk/cfi.cpp` (MF-1004).
 *
 * ── Was hier passiert und warum ─────────────────────────────────────
 *
 * `cfi` stand auf **T3** — ungeprueft. Das Orakel liegt im **eigenen
 * Baum**: `src/samdisk/cfi.cpp`, Simon Owens `ReadCFI()`. Das ist
 * dieselbe Lage wie bei `opus` in MF-905, wo `src/samdisk/opd.cpp` die
 * Hebung von T3 auf T2 getragen hat.
 *
 * **T2 und nicht T1b**, weil ein gelesener fremder Quelltext die
 * STRUKTUR belegt, nicht die Wirklichkeit — dafuer braeuchte es ein von
 * fremder Hand erzeugtes CFI-Abbild, und im Korpus liegt keines.
 *
 * ── Der Feldabgleich ────────────────────────────────────────────────
 *
 * Sechs Abweichungen, alle in derselben Richtung: das Orakel bricht ab,
 * UFT kuerzte still.
 *
 *   D1  Spurblock mit Laenge 0   samdisk: weiter    UFT: break, Rest weg
 *   D2  Teilblock mit Laenge 0   samdisk: weiter    UFT: break
 *   D3  Ausgabepuffer voll       samdisk: Abbruch   UFT: klemmt still
 *   D4  Datei zu kurz            samdisk: Abbruch   UFT: break, UFT_OK
 *   D5  Spurlaenge > Datei       samdisk: Abbruch   UFT: break, UFT_OK
 *   D6  Teilbloecke != track_end samdisk: Abbruch   UFT: keine Pruefung
 *
 * Die Zeilen im Orakel, auf die sich das stuetzt:
 *
 *   `throw util::exception("short file in CFI track block ", track)`
 *   `throw util::exception("expanded CFI image is too big")`
 *   `throw util::exception("short file reading CFI track block")`
 *   `throw util::exception("track data overflows CFI track block")`
 *
 * Kein fremder Quelltext uebernommen; `ReadCFI()` ist gelesen worden.
 *
 * ── Die Pruefdateien ────────────────────────────────────────────────
 *
 * Von Hand gebaut, jede Zahl steht hier. CFI ist eine Folge von
 * Spurbloecken:
 *
 *   Spurblock  = [2 Byte LE Laenge L][L Byte Inhalt]
 *   Inhalt     = Folge von Teilbloecken
 *   Teilblock  = [2 Byte LE: lo, hi&0x7F = Laenge, Bit 15 = RLE]
 *                RLE -> ein Fuellbyte; sonst -> Laenge Rohbytes
 *
 * Die Pruefdiskette ist bewusst winzig, damit die erwarteten Bytes
 * genau aufgehen:
 *
 *   BPB: 512 Byte/Sektor, 9 Sektoren/Spur, 2 Koepfe, 18 Sektoren gesamt
 *        -> 18 / (9 * 2) = **1 Zylinder**
 *   Gesamt: 1 * 2 * 9 * 512 = **9216 Byte** = zwei Spuren zu 4608
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "uft/formats/uft_cfi.h"
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

/* ── Baukasten ───────────────────────────────────────────────────── */

#define SEKTORGROESSE 512
#define SPURGROESSE   4608          /* 9 * 512 */
#define MARKE_KOPF1   0x5A          /* Fuellbyte der zweiten Spur   */

typedef struct { uint8_t *p; size_t n, kap; } puffer_t;

static void schreib(puffer_t *b, const void *q, size_t n)
{
    if (b->n + n > b->kap) {
        b->kap = (b->n + n) * 2 + 64;
        b->p = (uint8_t *)realloc(b->p, b->kap);
    }
    memcpy(b->p + b->n, q, n);
    b->n += n;
}

static void le16(puffer_t *b, uint16_t v)
{
    uint8_t z[2] = { (uint8_t)(v & 0xFF), (uint8_t)(v >> 8) };
    schreib(b, z, 2);
}

/** Teilblock: RLE, `len` Byte des Werts `fuell`. */
static void teilblock_rle(puffer_t *b, uint16_t len, uint8_t fuell)
{
    uint8_t z[3] = { (uint8_t)(len & 0xFF),
                     (uint8_t)(((len >> 8) & 0x7F) | 0x80), fuell };
    schreib(b, z, 3);
}

/** Teilblock: unkomprimiert, `len` Byte aus `daten`. */
static void teilblock_roh(puffer_t *b, const uint8_t *daten, uint16_t len)
{
    le16(b, (uint16_t)(len & 0x7FFF));
    schreib(b, daten, len);
}

/** Bootsektor mit der Mini-BPB von oben. */
static void bootsektor(uint8_t *s)
{
    memset(s, 0x00, SEKTORGROESSE);
    s[0] = 0xEB;
    s[11] = 0x00; s[12] = 0x02;   /* 512 Byte je Sektor  */
    s[19] = 0x12; s[20] = 0x00;   /* 18 Sektoren gesamt  */
    s[24] = 0x09; s[25] = 0x00;   /* 9 Sektoren je Spur  */
    s[26] = 0x02; s[27] = 0x00;   /* 2 Koepfe            */
}

/** Spur 0: Bootsektor roh + Rest als RLE. Ergibt 4608 Byte. */
static void spur0(puffer_t *datei)
{
    uint8_t boot[SEKTORGROESSE];
    bootsektor(boot);

    puffer_t inhalt = { NULL, 0, 0 };
    teilblock_roh(&inhalt, boot, SEKTORGROESSE);
    teilblock_rle(&inhalt, SPURGROESSE - SEKTORGROESSE, 0xAA);

    le16(datei, (uint16_t)inhalt.n);
    schreib(datei, inhalt.p, inhalt.n);
    free(inhalt.p);
}

/** Spur 1: 4608 Byte RLE mit der Marke. */
static void spur1(puffer_t *datei)
{
    puffer_t inhalt = { NULL, 0, 0 };
    teilblock_rle(&inhalt, SPURGROESSE, MARKE_KOPF1);
    le16(datei, (uint16_t)inhalt.n);
    schreib(datei, inhalt.p, inhalt.n);
    free(inhalt.p);
}

/* ── Faelle ──────────────────────────────────────────────────────── */

static uft_disk_image_t *oeffne(const puffer_t *b, uft_error_t *rc)
{
    uft_disk_image_t *disk = NULL;
    cfi_read_result_t erg;
    *rc = uft_cfi_read_mem(b->p, b->n, &disk, &erg);
    return disk;
}

static uft_sector_t *sektor(uft_disk_image_t *disk, int kopf, int idx)
{
    if (!disk || !disk->track_data) return NULL;
    uft_track_t *t = disk->track_data[kopf];      /* cyl 0, heads 2 */
    if (!t || t->sector_count <= idx) return NULL;
    return &t->sectors[idx];
}

int main(void)
{
    printf("=== CFI gegen src/samdisk/cfi.cpp (MF-1004) ===\n");

    /* ── Gegenprobe: die wohlgeformte Datei muss aufgehen ─────────── */
    {
        puffer_t b = { NULL, 0, 0 };
        spur0(&b);
        spur1(&b);

        uft_error_t rc;
        uft_disk_image_t *disk = oeffne(&b, &rc);
        pruefe("wohlgeformtes CFI oeffnet", rc == UFT_OK && disk != NULL,
               "ohne diesen Fall sagen die uebrigen nichts");
        uft_sector_t *s = sektor(disk, 1, 0);
        pruefe("  Kopf 1 traegt seine Daten",
               s && s->data && s->data[0] == MARKE_KOPF1,
               "die Marke der zweiten Spur fehlt");
        if (disk) uft_disk_free(disk);
        free(b.p);
    }

    /* ── D1: leerer Spurblock in der Mitte ────────────────────────── */
    {
        puffer_t b = { NULL, 0, 0 };
        spur0(&b);
        le16(&b, 0);              /* Spurblock der Laenge 0          */
        spur1(&b);

        uft_error_t rc;
        uft_disk_image_t *disk = oeffne(&b, &rc);
        uft_sector_t *s = sektor(disk, 1, 0);

        char h[180];
        snprintf(h, sizeof(h),
                 "rc=%d, Kopf-1-Sektor: %s -- ein Spurblock der Laenge 0 "
                 "beendete das Lesen, samdisk laeuft weiter", (int)rc,
                 !s ? "fehlt ganz"
                    : (s->status & UFT_SECTOR_MISSING) ? "als fehlend "
                                                         "gekennzeichnet"
                    : "vorhanden");
        pruefe("D1: leerer Spurblock verwirft nicht den Rest der Datei",
               rc == UFT_OK && s && s->data
               && s->data[0] == MARKE_KOPF1
               && !(s->status & UFT_SECTOR_MISSING), h);
        if (disk) uft_disk_free(disk);
        free(b.p);
    }

    /* ── D5: Spurlaenge zeigt ueber das Dateiende hinaus ──────────── */
    {
        puffer_t b = { NULL, 0, 0 };
        spur0(&b);
        le16(&b, 3);              /* verspricht 3 Byte ...           */
        uint8_t eins = 0x00;
        schreib(&b, &eins, 1);    /* ... liefert 1                   */

        uft_error_t rc;
        uft_disk_image_t *disk = oeffne(&b, &rc);
        char h[160];
        snprintf(h, sizeof(h),
                 "rc=%d -- die Datei bricht mitten im Spurblock ab, "
                 "samdisk wirft „short file in CFI track block\"", (int)rc);
        pruefe("D5: abgeschnittene Datei meldet KEINEN Erfolg",
               rc != UFT_OK, h);
        if (disk) uft_disk_free(disk);
        free(b.p);
    }

    /* ── D6: Teilbloecke treffen das Ende des Spurblocks nicht ─────── */
    {
        puffer_t b = { NULL, 0, 0 };
        spur0(&b);
        /* Spurblock verspricht 4 Byte, die Teilbloecke belegen 3 */
        le16(&b, 4);
        uint8_t inhalt[4] = { 0x00, 0x92, MARKE_KOPF1, 0x00 };
        schreib(&b, inhalt, 4);

        uft_error_t rc;
        uft_disk_image_t *disk = oeffne(&b, &rc);
        char h[170];
        snprintf(h, sizeof(h),
                 "rc=%d -- die Teilbloecke enden vor `track_end`, samdisk "
                 "wirft „track data overflows CFI track block\"", (int)rc);
        pruefe("D6: unstimmiger Spurblock meldet KEINEN Erfolg",
               rc != UFT_OK, h);
        if (disk) uft_disk_free(disk);
        free(b.p);
    }

    /* ── D3: die Entpackung sprengt den Ausgabepuffer ─────────────── */
    {
        /* 200 RLE-Bloecke zu je 15000 Byte = 3 000 000 Byte,
         * der Puffer fasst 80*2*36*512 = 2 949 120.
         *
         * Warum 200 kleine statt 91 grosser: die erste Fassung nahm 91
         * Bloecke zu 32767 und war **gruen aus dem falschen Grund** —
         * die Datei war damit nur 275 Byte lang, und
         * `uft_cfi_read_mem()` weist alles unter CFI_MIN_FILE_SIZE
         * (512) sofort ab. Der Fall wurde nie erreicht. Ein Beweis, der
         * nicht feuert, beweist nichts; 200 Bloecke ergeben 602 Byte. */
        puffer_t inhalt = { NULL, 0, 0 };
        for (int i = 0; i < 200; i++)
            teilblock_rle(&inhalt, 15000, 0xAA);

        puffer_t b = { NULL, 0, 0 };
        le16(&b, (uint16_t)inhalt.n);
        schreib(&b, inhalt.p, inhalt.n);
        free(inhalt.p);

        uft_error_t rc;
        uft_disk_image_t *disk = oeffne(&b, &rc);
        char h[200];
        snprintf(h, sizeof(h),
                 "rc=%d -- die Entpackung wurde still auf die "
                 "Restkapazitaet geklemmt und die Geometrie danach aus der "
                 "GEKLEMMTEN Groesse erfunden; samdisk wirft „expanded CFI "
                 "image is too big\"", (int)rc);
        pruefe("D3: uebergrosse Entpackung meldet KEINEN Erfolg",
               rc != UFT_OK, h);
        if (disk) uft_disk_free(disk);
        free(b.p);
    }

    /* ── D7: eine legitim WINZIGE CFI-Datei ───────────────────────── */
    {
        /* CFI komprimiert. Eine gleichfoermige Diskette schrumpft auf
         * wenige Dutzend Byte — hier 1 x 2 x 9 x 512 = 9216 Byte
         * Nutzlast in **26 Byte** Datei. `ReadCFI()` kennt keine
         * Untergrenze; UFT hatte `CFI_MIN_FILE_SIZE` = 512 und wies
         * seine EIGENE Schreiberausgabe ab.
         *
         * Gefunden hat das nicht dieser Feldabgleich, sondern der
         * Rundlauf auf der Schreibseite: ein Feldabgleich vergleicht
         * Verhalten, und eine Konstante, die das Orakel gar nicht hat,
         * faellt dabei nicht auf. */
        uint8_t boot[SEKTORGROESSE];
        bootsektor(boot);

        /* Spur 0 komprimiert: Bootsektor als wenige Laeufe + Rest RLE.
         * Von Hand so gebaut, dass die Datei sicher unter 512 bleibt. */
        puffer_t i0 = { NULL, 0, 0 };
        teilblock_roh(&i0, boot, 32);                    /* 2 + 32   */
        teilblock_rle(&i0, SEKTORGROESSE - 32, 0x00);    /* 3        */
        teilblock_rle(&i0, SPURGROESSE - SEKTORGROESSE, 0xAA); /* 3  */

        puffer_t b = { NULL, 0, 0 };
        le16(&b, (uint16_t)i0.n);
        schreib(&b, i0.p, i0.n);
        free(i0.p);
        spur1(&b);                                        /* 2 + 3   */

        uft_error_t rc;
        uft_disk_image_t *disk = oeffne(&b, &rc);
        uft_sector_t *s = sektor(disk, 1, 0);
        char h[190];
        snprintf(h, sizeof(h),
                 "Datei %zu Byte, rc=%d -- eine gut komprimierte CFI ist "
                 "legitim winzig; samdisk kennt keine Untergrenze, UFT "
                 "hatte CFI_MIN_FILE_SIZE = 512", b.n, (int)rc);
        pruefe("D7: eine winzige, aber gueltige CFI-Datei oeffnet",
               rc == UFT_OK && s && s->data && s->data[0] == MARKE_KOPF1, h);
        if (disk) uft_disk_free(disk);
        free(b.p);
    }

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot == 0 ? 0 : 1;
}
