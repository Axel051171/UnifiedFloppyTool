/**
 * @file test_udi_gegen_die_spezifikation.c
 * @brief UDI gegen die Spezifikation seines Urhebers (MF-1015).
 *
 * ── Woher die Frage kommt ────────────────────────────────────────────
 *
 * `udi` stand auf **T3** und hatte keinen eigenen Test. Gemessen am
 * Quelltext lagen drei Befunde offen, und der erste ist der schwerste.
 *
 * **Befund 1 — der Kopf war um vier Byte verschoben.** Das
 * Kopfkommentar des Plugins beschrieb einen Dateikopf **ohne** das
 * 4-Byte-Groessenfeld; `udi_open()` las Zylinder und Kopfzahl deshalb
 * aus `data[5]` und `data[6]` — den Bytes 1 und 2 der little-endian
 * **Dateigroesse**. Gerechnet:
 *
 *   TR-DOS 80x2, 6250 B/Spur (1125620 B) -> data[5]= 44, data[6]=17
 *   TR-DOS 40x1, 6250 B/Spur ( 281420 B) -> data[5]= 75, data[6]= 4
 *   TR-DOS 80x2, 6400 B/Spur (1152500 B) -> data[5]=149, data[6]=17
 *
 * Und `max_head > 1` wies ab. **Jede realistisch grosse UDI wurde
 * abgelehnt.** Klasse MF-961 (`86f`).
 *
 * **Befund 2 — die Taktmarken folgen JEDEM zerlegbaren Spurtyp**, nicht
 * nur MFM. `udi_find_track()` sprang nur bei `ttype == 0x00` darueber;
 * hinter einer FM-Spur lag jede weitere Spur um CLEN Byte daneben (bei
 * 6250 Byte je Spur: 782). Klasse MF-794 (`sad`).
 *
 * **Befund 3 — drei Pruefsummen, und die richtige stand woanders.** Der
 * Urheber gibt den Code woertlich an; er invertiert **je Byte**. Das
 * Plugin rechnete das gewoehnliche CRC-32 („standard PKZIP") und
 * schrieb damit den Abschluss jeder von UFT geschriebenen UDI.
 * `src/formats/udi/uft_udi.c` hatte die richtige Fassung — in der Datei,
 * die niemand ruft.
 *
 * ── Die Referenz ─────────────────────────────────────────────────────
 *
 * **Alex Makeev, „UDI file format Version 1.0 final", 24.03.2002**,
 * wiedergegeben im Sinclair Wiki „UDI format"
 * (https://sinclair.wiki.zxnet.co.uk/wiki/UDI_format), abgerufen
 * 2026-09-10. Kopf, Spursatz, Typtafel und Pruefsummencode stehen dort
 * woertlich.
 *
 * **Das ist die Spezifikation des URHEBERS, und sie schlaegt das
 * Orakel.** `src/samdisk/udi.cpp` rechnet die Pruefsumme mit `int32_t`
 * — sein `crc >> 1` ist ein arithmetischer Shift, und gegen den
 * Referenzcode gehalten faellt es. Ein Orakel ist eine Referenz, kein
 * Beweis (dieselbe Lage wie MF-961, wo die Spezifikation des Urhebers
 * gegen einen erfundenen Magic entschied).
 *
 * Damit ist das hier **T2**: kein UDI von fremder Hand im Korpus, aber
 * die Umsetzung steht gegen eine autoritative Quelle, und der Test
 * traegt seine **eigene** Umschrift des Referenzcodes — faellt die
 * Fassung im Plugin auf eine andere zurueck, faellt dieser Test.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"

extern const uft_format_plugin_t uft_format_plugin_udi;
int uft_udi_track_clk(const uft_disk_t *disk, int cyl, int head,
                      const uint8_t **clk, size_t *clk_len);
int uft_udi_pruefsumme(const uft_disk_t *disk, uint32_t *soll, uint32_t *ist);

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

/* ── Der Referenzcode des Urhebers, hier unabhaengig umgeschrieben ────
 *
 *     CRC ^= -1 ^ *(((unsigned char*)buf)+i);
 *     for( BYTE k = 8; k--; ) {
 *         temp = -(CRC & 1); CRC >>= 1; CRC ^= 0xedb88320 & temp; }
 *     CRC ^= -1;
 */
static uint32_t crc_urheber(const uint8_t *buf, size_t n)
{
    uint32_t CRC = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        CRC ^= 0xFFFFFFFFu ^ buf[i];
        for (int k = 8; k--; ) {
            uint32_t temp = (uint32_t)-(int32_t)(CRC & 1u);
            CRC >>= 1;
            CRC ^= 0xEDB88320u & temp;
        }
        CRC ^= 0xFFFFFFFFu;
    }
    return CRC;
}

/* Das gewoehnliche CRC-32, das hier NICHT gemeint ist — mitgefuehrt,
 * damit die Unterscheidung bewacht ist und niemand sie „vereinfacht". */
static uint32_t crc_pkzip(const uint8_t *d, size_t n)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        crc ^= d[i];
        for (int b = 0; b < 8; b++)
            crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)(-(int32_t)(crc & 1)));
    }
    return crc ^ 0xFFFFFFFFu;
}

/* CLEN nach der Spezifikation */
static size_t clen_spec(size_t tlen)
{
    return tlen / 8 + (tlen % 8 + 7) / 8;
}

typedef struct { uint8_t typ; size_t tlen; uint8_t muster; } spur_t;

/* Baut eine UDI. `version`, `max_cyl`, `max_head` werden woertlich in
 * den Kopf geschrieben; `spuren` beschreibt die Saetze in Reihenfolge. */
static size_t baue_udi(uint8_t *b, size_t kap, uint8_t version,
                       uint8_t max_cyl, uint8_t max_head,
                       const spur_t *spuren, int n_spuren,
                       int summe_verfaelschen)
{
    size_t n = 0;
    memcpy(b + n, "UDI!", 4); n += 4;
    size_t gr_pos = n; n += 4;
    b[n++] = version;
    b[n++] = max_cyl;
    b[n++] = max_head;
    b[n++] = 0x00;
    b[n++] = 0; b[n++] = 0; b[n++] = 0; b[n++] = 0;   /* EXTHDL = 0 */

    for (int i = 0; i < n_spuren; i++) {
        size_t tlen = spuren[i].tlen;
        size_t clen = clen_spec(tlen);
        if (n + 3 + tlen + clen + 4 > kap) return 0;
        b[n++] = spuren[i].typ;
        b[n++] = (uint8_t)(tlen & 0xFF);
        b[n++] = (uint8_t)((tlen >> 8) & 0xFF);
        for (size_t k = 0; k < tlen; k++)
            b[n++] = (uint8_t)(spuren[i].muster ^ (k & 0xFF));
        /* Taktmarken: erkennbares Muster, damit der Zugang pruefbar ist */
        for (size_t k = 0; k < clen; k++)
            b[n++] = (uint8_t)(spuren[i].muster + (uint8_t)k);
    }

    uint32_t gr = (uint32_t)n;              /* Dateigroesse minus 4 */
    b[gr_pos + 0] = (uint8_t)(gr & 0xFF);
    b[gr_pos + 1] = (uint8_t)((gr >> 8) & 0xFF);
    b[gr_pos + 2] = (uint8_t)((gr >> 16) & 0xFF);
    b[gr_pos + 3] = (uint8_t)((gr >> 24) & 0xFF);

    uint32_t crc = crc_urheber(b, n);
    if (summe_verfaelschen) crc ^= 0x00000001u;
    b[n++] = (uint8_t)(crc & 0xFF);
    b[n++] = (uint8_t)((crc >> 8) & 0xFF);
    b[n++] = (uint8_t)((crc >> 16) & 0xFF);
    b[n++] = (uint8_t)((crc >> 24) & 0xFF);
    return n;
}

static int schreibe(const char *pfad, const uint8_t *b, size_t n)
{
    FILE *f = fopen(pfad, "wb");
    if (!f) return 0;
    int ok = (fwrite(b, 1, n, f) == n);
    fclose(f);
    return ok;
}

static void frei(uft_track_t *t)
{
    if (!t) return;
    free(t->raw_data);
    free(t->sectors);
    memset(t, 0, sizeof(*t));
}

#define TLEN_ECHT 6250      /* eine echte TR-DOS-Spur */

int main(void)
{
    printf("=== UDI gegen die Spezifikation des Urhebers (MF-1015) ===\n");

    char tmpdir[400];
    const char *tmp = getenv("TEMP");
    if (!tmp) tmp = getenv("TMPDIR");
    if (!tmp) tmp = ".";
    snprintf(tmpdir, sizeof(tmpdir), "%s", tmp);

    char pfad[512];
    char h[260];

    /* ── Die Umschrift der Pruefsumme trifft, was das Plugin rechnet ─── */
    {
        const uint8_t probe[4] = { 'U', 'D', 'I', '!' };
        uint32_t u = crc_urheber(probe, 4);
        uint32_t z = crc_pkzip(probe, 4);
        snprintf(h, sizeof(h), "Urheber %08X, PKZIP %08X", u, z);
        pruefe("die Fassung des Urhebers ist NICHT das gewoehnliche CRC-32",
               u != z && u == 0x196DC161u, h);
    }

    /* ── Befund 1: eine Datei in echter Groesse ─────────────────────── */
    {
        /* 80 Zylinder x 2 Koepfe x 6250 Byte — genau die Datei, die der
         * alte Leser abgewiesen hat. */
        int n_spuren = 160;
        size_t kap = 32 + (size_t)n_spuren * (3 + TLEN_ECHT + clen_spec(TLEN_ECHT)) + 64;
        uint8_t *b = (uint8_t *)malloc(kap);
        spur_t *sp = (spur_t *)malloc(sizeof(spur_t) * (size_t)n_spuren);
        if (!b || !sp) { printf("  [ROT]  kein Speicher\n"); return 1; }
        for (int i = 0; i < n_spuren; i++) {
            sp[i].typ = 0x00;
            sp[i].tlen = TLEN_ECHT;
            sp[i].muster = (uint8_t)(0x10 + i);
        }
        size_t n = baue_udi(b, kap, 0x00, 79, 1, sp, n_spuren, 0);

        snprintf(pfad, sizeof(pfad), "%s/uft_mf1015_echt.udi", tmpdir);
        int ok = (n > 0) && schreibe(pfad, b, n);

        /* Was der ALTE Leser aus dieser Datei gelesen haette: */
        uint32_t gr = (uint32_t)(n - 4);
        unsigned alt_cyl = (gr >> 8) & 0xFF;
        unsigned alt_head = (gr >> 16) & 0xFF;
        snprintf(h, sizeof(h),
                 "%zu Byte; data[5]=%u, data[6]=%u -> der alte Leser wies ab",
                 n, alt_cyl, alt_head);
        pruefe("die Pruefdatei hat echte Groesse, und die alten Versaetze "
               "haetten sie abgewiesen",
               ok && n > 1000000 && alt_head > 1, h);

        uft_disk_t disk;
        memset(&disk, 0, sizeof(disk));
        uft_error_t rc = uft_format_plugin_udi.open(&disk, pfad, true);
        snprintf(h, sizeof(h), "open=%d, %u Zyl, %u Koepfe", (int)rc,
                 disk.geometry.cylinders, disk.geometry.heads);
        pruefe("sie laesst sich oeffnen, 80 Zylinder und 2 Koepfe",
               rc == UFT_OK && disk.geometry.cylinders == 80
               && disk.geometry.heads == 2, h);

        if (rc == UFT_OK) {
            /* Die letzte Spur ist der Beweis fuer den Versatzlauf ueber
             * 160 Saetze: sie liegt 159 * (3+6250+782) Byte hinter dem
             * Kopf, und ihr Muster ist 0x10 + 159. */
            uft_track_t t;
            memset(&t, 0, sizeof(t));
            uft_error_t r = uft_format_plugin_udi.read_track(&disk, 79, 1, &t);
            uint8_t erwartet = (uint8_t)(0x10 + 159);
            int gut = (r == UFT_OK && t.raw_size == TLEN_ECHT
                       && t.raw_data && t.raw_data[0] == erwartet
                       && t.raw_data[1] == (uint8_t)(erwartet ^ 1));
            snprintf(h, sizeof(h), "rc=%d, %zu Byte, Byte 0 = %02X "
                     "(erwartet %02X)", (int)r, (size_t)t.raw_size,
                     t.raw_data ? t.raw_data[0] : 0, erwartet);
            pruefe("und die 160. Spur steht byteweise da, wo sie hingehoert",
                   gut, h);
            frei(&t);

            uint32_t soll = 0, ist = 0;
            int prc = uft_udi_pruefsumme(&disk, &soll, &ist);
            snprintf(h, sizeof(h), "rc=%d, Datei %08X, nachgerechnet %08X",
                     prc, soll, ist);
            pruefe("die Pruefsumme der echten Datei geht auf",
                   prc == 0 && soll == ist && soll == crc_urheber(b, n - 4), h);

            uft_format_plugin_udi.close(&disk);
        }
        remove(pfad);
        free(b); free(sp);
    }

    /* ── Befund 2: eine FM-Spur vor einer MFM-Spur ──────────────────── */
    {
        static uint8_t b[4096];
        const spur_t sp[2] = {
            { 0x01, 64, 0x11 },     /* FM   */
            { 0x00, 64, 0xA0 },     /* MFM  */
        };
        size_t n = baue_udi(b, sizeof(b), 0x00, 1, 0, sp, 2, 0);
        snprintf(pfad, sizeof(pfad), "%s/uft_mf1015_fm.udi", tmpdir);
        schreibe(pfad, b, n);

        uft_disk_t disk;
        memset(&disk, 0, sizeof(disk));
        uft_error_t rc = uft_format_plugin_udi.open(&disk, pfad, true);
        pruefe("die FM-vor-MFM-Datei laesst sich oeffnen", rc == UFT_OK, NULL);

        if (rc == UFT_OK) {
            uft_track_t t0, t1;
            memset(&t0, 0, sizeof(t0));
            memset(&t1, 0, sizeof(t1));
            uft_error_t r0 = uft_format_plugin_udi.read_track(&disk, 0, 0, &t0);
            uft_error_t r1 = uft_format_plugin_udi.read_track(&disk, 1, 0, &t1);

            snprintf(h, sizeof(h), "Spur 0: rc=%d, Kodierung=%d, Byte 0=%02X",
                     (int)r0, (int)t0.encoding,
                     t0.raw_data ? t0.raw_data[0] : 0);
            pruefe("Spur 0 ist FM und liest 0x11",
                   r0 == UFT_OK && t0.encoding == UFT_ENC_FM
                   && t0.raw_size == 64 && t0.raw_data
                   && t0.raw_data[0] == 0x11, h);

            /* Der eigentliche Befund: ohne den CLK-Sprung liest der
             * Leser hier die Taktmarken der FM-Spur als Spurkopf. */
            snprintf(h, sizeof(h), "Spur 1: rc=%d, Kodierung=%d, %zu Byte, "
                     "Byte 0=%02X (erwartet MFM, 64, A0)", (int)r1,
                     (int)t1.encoding, (size_t)t1.raw_size,
                     t1.raw_data ? t1.raw_data[0] : 0);
            pruefe("Spur 1 hinter der FM-Spur liegt richtig — die "
                   "Taktmarken werden auch bei FM uebersprungen",
                   r1 == UFT_OK && t1.encoding == UFT_ENC_MFM
                   && t1.raw_size == 64 && t1.raw_data
                   && t1.raw_data[0] == 0xA0, h);
            frei(&t0); frei(&t1);

            /* ── Die Taktmarken sind erreichbar ────────────────────── */
            const uint8_t *clk = NULL;
            size_t clk_len = 0;
            int crc0 = uft_udi_track_clk(&disk, 0, 0, &clk, &clk_len);
            int gut = (crc0 == 0 && clk && clk_len == clen_spec(64)
                       && clk[0] == 0x11 && clk[1] == 0x12
                       && clk[7] == 0x18);
            snprintf(h, sizeof(h), "rc=%d, %zu Byte (erwartet %zu), "
                     "clk[0]=%02X clk[7]=%02X", crc0, clk_len,
                     clen_spec(64), clk ? clk[0] : 0, clk ? clk[7] : 0);
            pruefe("und die Taktmarken der FM-Spur sind erreichbar",
                   gut, h);

            uft_format_plugin_udi.close(&disk);
        }
        remove(pfad);
    }

    /* ── Gegenproben ────────────────────────────────────────────────── */
    {
        static uint8_t b[4096];
        const spur_t eine[1] = { { 0x00, 64, 0x55 } };

        struct { const char *was; uint8_t ver, mc, mh, typ; int summe; } f[] = {
            { "Version 1 statt 0 wird abgewiesen",            0x01, 0, 0, 0x00, 0 },
            { "eine Kopfzahl von 2 wird abgewiesen (reserviert)", 0x00, 0, 2, 0x00, 0 },
            { "Spurtyp 0x80 (schwach) wird benannt abgewiesen",  0x00, 0, 0, 0x80, 0 },
            { "Spurtyp 0xE0 (Microdrive) wird benannt abgewiesen", 0x00, 0, 0, 0xE0, 0 },
            { "Spurtyp 0xF0 (zlib) wird benannt abgewiesen",     0x00, 0, 0, 0xF0, 0 },
        };
        for (size_t i = 0; i < sizeof(f)/sizeof(f[0]); i++) {
            spur_t sp = eine[0];
            sp.typ = f[i].typ;
            size_t n = baue_udi(b, sizeof(b), f[i].ver, f[i].mc, f[i].mh,
                                &sp, 1, f[i].summe);
            snprintf(pfad, sizeof(pfad), "%s/uft_mf1015_g%zu.udi", tmpdir, i);
            schreibe(pfad, b, n);
            uft_disk_t d;
            memset(&d, 0, sizeof(d));
            uft_error_t r = uft_format_plugin_udi.open(&d, pfad, true);
            snprintf(h, sizeof(h), "open=%d", (int)r);
            pruefe(f[i].was, r != UFT_OK, h);
            if (r == UFT_OK) uft_format_plugin_udi.close(&d);
            remove(pfad);
        }
    }
    {
        /* Eine verfaelschte Summe wird GEMELDET, nicht zum Abbruch —
         * „Kein Bit verloren". */
        static uint8_t b[4096];
        const spur_t sp[1] = { { 0x00, 64, 0x33 } };
        size_t n = baue_udi(b, sizeof(b), 0x00, 0, 0, sp, 1, 1);
        snprintf(pfad, sizeof(pfad), "%s/uft_mf1015_summe.udi", tmpdir);
        schreibe(pfad, b, n);

        uft_disk_t d;
        memset(&d, 0, sizeof(d));
        uft_error_t r = uft_format_plugin_udi.open(&d, pfad, true);
        pruefe("eine Datei mit falscher Summe bleibt lesbar", r == UFT_OK, NULL);
        if (r == UFT_OK) {
            uint32_t soll = 0, ist = 0;
            int prc = uft_udi_pruefsumme(&d, &soll, &ist);
            snprintf(h, sizeof(h), "rc=%d, Datei %08X, nachgerechnet %08X",
                     prc, soll, ist);
            pruefe("und die falsche Summe wird gemeldet",
                   prc == 1 && soll != ist, h);
            uft_format_plugin_udi.close(&d);
        }
        remove(pfad);
    }
    {
        /* Eine Spur der Laenge 0 ist erlaubt (fehlende Spur). */
        static uint8_t b[4096];
        const spur_t sp[2] = { { 0x00, 0, 0x00 }, { 0x00, 64, 0x77 } };
        size_t n = baue_udi(b, sizeof(b), 0x00, 1, 0, sp, 2, 0);
        snprintf(pfad, sizeof(pfad), "%s/uft_mf1015_leer.udi", tmpdir);
        schreibe(pfad, b, n);

        uft_disk_t d;
        memset(&d, 0, sizeof(d));
        uft_error_t r = uft_format_plugin_udi.open(&d, pfad, true);
        pruefe("eine Spur der Laenge 0 ist erlaubt", r == UFT_OK, NULL);
        if (r == UFT_OK) {
            uft_track_t t;
            memset(&t, 0, sizeof(t));
            uft_error_t r0 = uft_format_plugin_udi.read_track(&d, 0, 0, &t);
            pruefe("sie liefert nichts statt zu raten",
                   r0 == UFT_OK && t.raw_size == 0, NULL);
            frei(&t);
            memset(&t, 0, sizeof(t));
            uft_error_t r1 = uft_format_plugin_udi.read_track(&d, 1, 0, &t);
            snprintf(h, sizeof(h), "rc=%d, Byte 0=%02X (erwartet 77)",
                     (int)r1, t.raw_data ? t.raw_data[0] : 0);
            pruefe("und die Spur dahinter liegt richtig",
                   r1 == UFT_OK && t.raw_size == 64 && t.raw_data
                   && t.raw_data[0] == 0x77, h);
            frei(&t);
            uft_format_plugin_udi.close(&d);
        }
        remove(pfad);
    }

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
