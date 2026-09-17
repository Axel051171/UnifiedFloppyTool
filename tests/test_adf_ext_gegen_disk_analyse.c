/**
 * @file test_adf_ext_gegen_disk_analyse.c
 * @brief `adf_ext` gegen ein Extended ADF von FREMDER Hand (MF-1222)
 *
 * ERZEUGER (Kanal *Oracle* nach MF-695 — ausgefuehrt, nicht portiert):
 *   `disk-analyse` aus `keirf/disk-utilities`, gebaut mit
 *   `mingw32-make SHARED_LIB=n` (gcc 13.1.0, erster Versuch rc 0).
 *   Lizenz: **Unlicense / public domain** — `COPYING` im mitgelieferten
 *   `neue-ideen/disk-utilities-master.zip` gemessen, nicht nur am Ursprung
 *   nachgelesen. Keine Klausel schraenkt Ausfuehrung oder Weitergabe ein.
 *
 * ABSTAMMUNG — WARUM DAS KEIN GESCHLOSSENER KREIS IST:
 *   `test_adf_ext_plugin` baut seine Extended ADF SELBST; Schreiber und
 *   Leser sind dieselbe Hand, und genau daran sind `apridisk` (MF-1009)
 *   und `qrst` (MF-1028) gruen durch einen erfundenen Aufbau gelaufen.
 *   Hier kodiert eine unabhaengige Umsetzung die AmigaDOS-Spuren nach MFM
 *   und UFT dekodiert sie mit seinem EIGENEN Dekoder zurueck. Die beiden
 *   Haende sind belegt verschieden: UFTs Leser ist gegen **WinUAEs**
 *   `disk.cpp read_header_ext2` abgenommen (siehe `uft_adf_ext.c`-Kopf),
 *   der Erzeuger ist Keir Frasers eigene Fassung in
 *   `libdisk/container/eadf.c`. Beim `dms` war genau das die Falle
 *   (MF-1135): UFTs Leser und hxcfes Leser stammten beide aus xDMS.
 *
 * DIE EINGABE BENENNT SICH SELBST (MF-1020/MF-1021):
 *   Jeder 512-Byte-Sektor der Quell-ADF traegt alle 17 Byte die Marke
 *   `UFT-K Ccc Hh Sss `. Ein Leseergebnis sagt damit nicht nur, DASS
 *   etwas kam, sondern ob die RICHTIGE Stelle getroffen wurde — ohne das
 *   waere eine Pruefdatei aus Fuellbytes wertlos (MF-1021, `v9t9`).
 *   512 = 30 x 17 + 2, die Phase wandert also innerhalb des Sektors;
 *   deshalb wird die Marke am SEKTORANFANG geprueft, statt ab Versatz 0
 *   durchzuvergleichen (Lehre aus MF-1149, wo eine 30-Byte-Marke gegen
 *   512 genau die Haelfte traf).
 *
 * WAS `disk-analyse` SCHREIBT, GEMESSEN AN DER DATEI SELBST:
 *   `eadf_close()` setzt `thdr.type = htobe16(1)` EINMAL vor der Schleife
 *   — jede Spur ist eine ROHE MFM-Spur, nie eine AmigaDOS-Spur. Der Weg
 *   durch UFT ist also `raw_data`, nicht der Sektorzweig.
 *   Kopf + Tafel + Nutzlast = 2 005 044 = Dateigroesse (stimmig),
 *   166 Spuren (83 x 2), davon 160 volle mit je 100 150 Bit und
 *   6 unformatierte (T80.0-82.1, `len` 0).
 *
 * REPRODUZIERBAR: `tests/corpus_manifest/gen_adf_ext_corpus.py`
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"
#include "uft/flux/uft_flux_decoder.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_adf_ext;

#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR "tests/corpus_free"
#endif

#define BILD            "disk_analyse_extadf_raw160.adf"

/* Alle Zahlen an der Datei gemessen, nicht angenommen. */
#define SPUREN          166u    /* 83 Zylinder x 2 Koepfe */
#define ZYLINDER        83u
#define VOLLE_ZYL       80u     /* T0.0-79.1, von disk-analyse als AmigaDOS benannt */
#define BITS_JE_SPUR    100150u
#define BYTES_JE_SPUR   12519u  /* (100150 + 7) / 8 */
#define SEK_JE_SPUR     11u
#define SEK_GESAMT      (VOLLE_ZYL * 2u * SEK_JE_SPUR)   /* 1760 */
#define MARKE_LEN       17u

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *detail)
{
    if (ok) { gruen++; printf("  [OK]   %s\n", was); }
    else    { rot++;   printf("  [ROT]  %s\n         -> %s\n", was, detail); }
}

static void marke_bauen(char *aus, size_t n, unsigned c, unsigned h, unsigned s)
{
    snprintf(aus, n, "UFT-K C%02u H%u S%02u ", c, h, s);
}

static void spur_freigeben(uft_track_t *tr)
{
    for (size_t i = 0; i < tr->sector_count; i++) free(tr->sectors[i].data);
    free(tr->sectors); tr->sectors = NULL; tr->sector_count = 0;
    free(tr->raw_data); tr->raw_data = NULL;
    tr->raw_size = tr->raw_len = 0; tr->raw_bits = 0;
}

/* ------------------------------------------------------------------ */
/* 1. Die Kennung und die Geometrie, die die Datei selbst ansagt.      */
/* ------------------------------------------------------------------ */
static void t_kennung_und_geometrie(uft_disk_t *disk, const char *pfad)
{
    uint8_t kopf[12];
    FILE *f = fopen(pfad, "rb");
    if (!f) { pruefe("Korpusdatei vorhanden", 0, pfad); return; }
    size_t gelesen = fread(kopf, 1, sizeof(kopf), f);
    fclose(f);
    pruefe("Korpusdatei liefert 12 Kopfbytes", gelesen == sizeof(kopf), pfad);
    if (gelesen != sizeof(kopf)) return;

    int konf = 0;
    pruefe("Sonde nimmt das FREMDE Abbild an",
           uft_format_plugin_adf_ext.probe(kopf, sizeof(kopf), 0, &konf) == true,
           "probe() sagte nein zu einem gueltigen UAE-1ADF");
    pruefe("Konfidenz 95 (Kennung getroffen, Band nach MF-729)",
           konf == 95, "Konfidenz nicht 95");

    pruefe("open() nimmt das fremde Abbild an",
           uft_format_plugin_adf_ext.open(disk, pfad, true) == UFT_OK,
           "open() hat abgesagt");
    pruefe("Zylinder 83 (166 Spuren / 2 Koepfe)",
           disk->geometry.cylinders == (int)ZYLINDER, "Zylinderzahl weicht ab");
    pruefe("Koepfe 2", disk->geometry.heads == 2, "Kopfzahl weicht ab");
    pruefe("11 Sektoren (DD, nicht die 22 der HD-Heuristik)",
           disk->geometry.sectors == (int)SEK_JE_SPUR, "Sektorzahl weicht ab");
}

/* ------------------------------------------------------------------ */
/* 2. ROTBEWEIS: die Spurlaenge in BIT steht in der Datei und wurde    */
/*    verworfen. `uft_track_t` hat das Feld `raw_bits` dafuer.         */
/* ------------------------------------------------------------------ */
static void t_bitlaenge_ueberlebt(uft_disk_t *disk)
{
    unsigned falsch_bits = 0, falsch_bytes = 0, geprueft = 0;
    for (unsigned c = 0; c < VOLLE_ZYL; c++) {
        for (unsigned h = 0; h < 2; h++) {
            uft_track_t t; memset(&t, 0, sizeof(t));
            if (uft_format_plugin_adf_ext.read_track(disk, (int)c, (int)h, &t)
                != UFT_OK) { falsch_bytes++; continue; }
            geprueft++;
            if (t.raw_size != BYTES_JE_SPUR) falsch_bytes++;
            if (t.raw_bits != BITS_JE_SPUR)  falsch_bits++;
            spur_freigeben(&t);
        }
    }
    char d[160];
    snprintf(d, sizeof(d), "%u von %u Spuren ohne die Bitlaenge der Datei",
             falsch_bits, geprueft);
    pruefe("jede Spur traegt ihre Bitlaenge (raw_bits == 100150)",
           falsch_bits == 0 && geprueft == VOLLE_ZYL * 2u, d);
    snprintf(d, sizeof(d), "%u von %u Spuren mit anderer Bytezahl",
             falsch_bytes, geprueft);
    pruefe("jede Spur traegt 12519 Byte", falsch_bytes == 0, d);

    /* Gegen die Tautologie: die Bytezahl KANN die Bitzahl nicht liefern.
     * 12519 x 8 = 100152, die Datei sagt 100150 — zwei Bitstellen hinter
     * dem Spurende. Wer `raw_size * 8` rechnet, liest sie mit. */
    pruefe("Bytezahl x 8 ist NICHT die Bitzahl (sonst waere die Zusage leer)",
           BYTES_JE_SPUR * 8u != BITS_JE_SPUR, "100152 == 100150?");
}

/* ------------------------------------------------------------------ */
/* 3. Der Beleg: fremde MFM-Kodierung, eigener Dekoder, jeder Sektor   */
/*    an seiner eigenen Ortsmarke.                                     */
/* ------------------------------------------------------------------ */
static void t_sektoren_an_ihrer_ortsmarke(uft_disk_t *disk)
{
    unsigned getroffen = 0, abweichend = 0, crc_rot = 0, spuren_ohne_11 = 0;
    for (unsigned c = 0; c < VOLLE_ZYL; c++) {
        for (unsigned h = 0; h < 2; h++) {
            uft_track_t t; memset(&t, 0, sizeof(t));
            if (uft_format_plugin_adf_ext.read_track(disk, (int)c, (int)h, &t)
                != UFT_OK || !t.raw_data) { spuren_ohne_11++; continue; }

            size_t bits = t.raw_bits ? t.raw_bits : t.raw_size * 8u;
            flux_decoded_track_t dt; memset(&dt, 0, sizeof(dt));
            if (flux_decode_amiga_bits(t.raw_data, bits, &dt, NULL) != FLUX_OK) {
                spuren_ohne_11++; spur_freigeben(&t); continue;
            }
            if (dt.sector_count != SEK_JE_SPUR) spuren_ohne_11++;

            for (size_t i = 0; i < dt.sector_count; i++) {
                const flux_decoded_sector_t *s = &dt.sectors[i];
                if (!s->data_crc_ok) crc_rot++;
                char marke[MARKE_LEN + 1];
                marke_bauen(marke, sizeof(marke), c, h, s->sector);
                if (s->data && s->data_size == 512 &&
                    memcmp(s->data, marke, MARKE_LEN) == 0)
                    getroffen++;
                else
                    abweichend++;
            }
            flux_decoded_track_free(&dt);
            spur_freigeben(&t);
        }
    }
    char d[200];
    snprintf(d, sizeof(d), "%u getroffen, %u abweichend (erwartet %u / 0)",
             getroffen, abweichend, (unsigned)SEK_GESAMT);
    pruefe("1760 von 1760 Sektoren an ihrer eigenen Ortsmarke",
           getroffen == SEK_GESAMT && abweichend == 0, d);
    snprintf(d, sizeof(d), "%u Sektoren mit falscher Datenpruefsumme", crc_rot);
    pruefe("keine Datenpruefsumme faellt", crc_rot == 0, d);
    snprintf(d, sizeof(d), "%u Spuren ohne 11 Sektoren", spuren_ohne_11);
    pruefe("jede der 160 Spuren liefert 11 Sektoren", spuren_ohne_11 == 0, d);
}

/* ------------------------------------------------------------------ */
/* 4. Die sechs unformatierten Spuren, die disk-analyse selbst benennt */
/*    (T80.0-82.1) — `len` 0 heisst KEIN Bitstrom, nicht ein leerer.   */
/* ------------------------------------------------------------------ */
static void t_unformatierte_spuren_sind_leer(uft_disk_t *disk)
{
    unsigned falsch = 0, geprueft = 0;
    for (unsigned c = VOLLE_ZYL; c < ZYLINDER; c++) {
        for (unsigned h = 0; h < 2; h++) {
            uft_track_t t; memset(&t, 0, sizeof(t));
            if (uft_format_plugin_adf_ext.read_track(disk, (int)c, (int)h, &t)
                != UFT_OK) { falsch++; continue; }
            geprueft++;
            if (t.raw_data != NULL || t.sector_count != 0) falsch++;
            spur_freigeben(&t);
        }
    }
    char d[160];
    snprintf(d, sizeof(d), "%u von %u unformatierten Spuren mit Daten",
             falsch, geprueft);
    pruefe("die 6 unformatierten Spuren liefern nichts statt Nullen",
           falsch == 0 && geprueft == (ZYLINDER - VOLLE_ZYL) * 2u, d);
}

/* ------------------------------------------------------------------ */
/* 5. Anti-Tautologie: ein gekippter Bitstrom MUSS auffallen. Ohne     */
/*    diese Probe koennte 3. gruen sein, weil der Vergleich nichts     */
/*    unterscheidet (Klasse MF-1014/MF-1026: gruen aus falschem Grund).*/
/* ------------------------------------------------------------------ */
static void t_gekippter_bitstrom_faellt_auf(uft_disk_t *disk)
{
    uft_track_t t; memset(&t, 0, sizeof(t));
    if (uft_format_plugin_adf_ext.read_track(disk, 0, 0, &t) != UFT_OK
        || !t.raw_data || t.raw_size < 4096) {
        pruefe("Spur 0/0 fuer die Gegenprobe lesbar", 0, "read_track");
        return;
    }
    size_t bits = t.raw_bits ? t.raw_bits : t.raw_size * 8u;

    flux_decoded_track_t heil; memset(&heil, 0, sizeof(heil));
    flux_decode_amiga_bits(t.raw_data, bits, &heil, NULL);
    size_t gut_heil = 0;
    for (size_t i = 0; i < heil.sector_count; i++)
        if (heil.sectors[i].data_crc_ok) gut_heil++;
    flux_decoded_track_free(&heil);

    t.raw_data[t.raw_size / 2] ^= 0xFF;   /* mitten in den Nutzdaten */
    flux_decoded_track_t kaputt; memset(&kaputt, 0, sizeof(kaputt));
    flux_decode_amiga_bits(t.raw_data, bits, &kaputt, NULL);
    size_t gut_kaputt = 0;
    for (size_t i = 0; i < kaputt.sector_count; i++)
        if (kaputt.sectors[i].data_crc_ok) gut_kaputt++;
    flux_decoded_track_free(&kaputt);

    char d[160];
    snprintf(d, sizeof(d), "heil %zu gute Sektoren, gekippt %zu — kein Unterschied",
             gut_heil, gut_kaputt);
    pruefe("ein gekipptes Byte im Bitstrom senkt die guten Sektoren",
           gut_heil == SEK_JE_SPUR && gut_kaputt < gut_heil, d);
    spur_freigeben(&t);
}

int main(void)
{
    char pfad[512];
    snprintf(pfad, sizeof(pfad), "%s/%s", UFT_CORPUS_DIR, BILD);
    printf("== adf_ext gegen disk-analyse (keirf/disk-utilities, public domain)\n");
    printf("   Abbild: %s\n", pfad);

    uft_disk_t disk; memset(&disk, 0, sizeof(disk));
    disk.read_only = true;

    t_kennung_und_geometrie(&disk, pfad);
    if (disk.plugin_data) {
        t_bitlaenge_ueberlebt(&disk);
        t_sektoren_an_ihrer_ortsmarke(&disk);
        t_unformatierte_spuren_sind_leer(&disk);
        t_gekippter_bitstrom_faellt_auf(&disk);
        if (uft_format_plugin_adf_ext.close)
            uft_format_plugin_adf_ext.close(&disk);
    }

    printf("\n  %d gruen, %d rot\n", gruen, rot);
    return rot == 0 ? 0 : 1;
}
