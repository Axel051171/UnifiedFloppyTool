/**
 * @file test_stx_groessenkode.c
 * @brief STX: der Groessenkode wird ungemaskiert geschifted (MF-1165)
 *
 * ── Der Befund ──────────────────────────────────────────────────────────
 *
 * `src/formats/stx/uft_stx_air.c` nimmt das Groessenbyte des Sektorkopfes
 * roh aus der Datei (`sd[snum].id.size = data[pos + 11];`) und rechnet an
 * drei Stellen damit:
 *
 *     :354   sec->sector_size = (uint32_t)(128 << sd[snum].id.size);
 *     :433   uint32_t sec_size = (uint32_t)(128 << sd[snum].id.size);
 *     :492   int sec_size = (int)(128 << sd[snum].id.size);
 *
 * Die benannte Quelle des Formats sagt das Gegenteil. `atari.8bitchip.info/
 * STXdesc.html`, Sektorkopf Versatz 0x0b:
 *
 *     „Size of the sector from the address block identifying the sector
 *      (typically: 2=512 or 3=1024 bytes) - but may be more. Then only
 *      bits 0-1 matter."
 *
 * Dieselbe Seite ist in `docs/VERIFICATION_TIERS.md` als Quelle fuer `stx`
 * eingetragen (T1b, MF-335). Der Satz stand also die ganze Zeit in der
 * Referenz, die der Baum selbst nennt.
 *
 * ── Was daraus folgt, gestaffelt ────────────────────────────────────────
 *
 * Kode 4..7 — die Faelle, die die Seite ausdruecklich ankuendigt:
 *   die Sektorgroesse wird 2048..16384 statt 512 oder 1024.
 *
 * Und weil die Schranke bei :441 lautet
 *
 *     if (sec_pos + sec_size <= size && sec->sector_data) memcpy(...)
 *
 * faellt bei einem zu grossen `sec_size` der memcpy AUS, waehrend der
 * `malloc` darueber schon gelaufen ist. Der Leser reicht dann einen Puffer
 * heraus, der NIE BESCHRIEBEN wurde — `uft_stx_sector_view_t.data` zeigt
 * auf uninitialisierten Speicher und `.size` nennt dessen volle Laenge.
 * Genau das prueft Fall 2.
 *
 * Kode 24 — `128 << 24` ist als `int` INT_MIN, also negativ. Bei :492
 * steht `fuzzy_offset += sec_size` AUSSERHALB der Schranke, der naechste
 * Sektor liest damit bei `fuzzy_mask + negativ`, also vor dem Puffer.
 * Kode >= 25 — `128 << size` ist undefiniertes Verhalten (Shift-Ueberlauf).
 *
 * **Die beiden letzten sind hier NICHT gemessen**, weil die Fuzzy-Maske
 * nicht durch `uft_stx_sector_view_t` nach aussen kommt und undefiniertes
 * Verhalten sich nicht zusichern laesst. Sie haengen an derselben Variablen
 * und verschwinden mit derselben Maskierung; das ist eine Folgerung, kein
 * Messwert, und steht deshalb so da.
 *
 * ── Warum der Korpus es nicht gefangen hat ──────────────────────────────
 *
 * `tests/corpus_free/hxcfe_pc160.stx` traegt in allen 320 Sektoren den Kode
 * **2** (512 Byte), 42 Spuren, Flags 0x01. Die Maskierung aendert an diesem
 * Abbild nichts — nachgemessen. Ein Abbild mit einem Kode ueber 3 gibt es
 * im Korpus nicht, und deshalb musste der Rotbeweis eines bauen.
 *
 * ── Was ausdruecklich UNVERAENDERT bleibt ───────────────────────────────
 *
 * `uft_stx_sector_view_t.id_size` gibt weiter den ROHEN Kode aus der Datei
 * heraus. Die Datei sagt 6, also sagt das Feld 6 — maskiert wird nur die
 * abgeleitete Laenge. „Das Format sagt X" und „hier stehen Y Byte" sind
 * zwei Aussagen (Muster MF-980).
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"
#include "uft/formats/stx/uft_stx_air.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_stx;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-48s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define STX_KOPF      16u
#define STX_SPUR_KOPF 16u
#define STX_SEK_KOPF  16u

static void le16(uint8_t *p, unsigned v) { p[0] = (uint8_t)(v & 0xFF); p[1] = (uint8_t)(v >> 8); }
static void le32(uint8_t *p, unsigned long v) {
    p[0] = (uint8_t)(v & 0xFF);         p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF); p[3] = (uint8_t)((v >> 24) & 0xFF);
}

static void temp_pfad(char *p, size_t n)
{
    const char *d = getenv("TMPDIR");
    if (!d || !d[0]) d = getenv("TMP");
    if (!d || !d[0]) d = getenv("TEMP");
    if (!d || !d[0]) d = ".";
    snprintf(p, n, "%s/uft_stx_%d.stx", d, rand() % 100000);
}

/* Baut eine STX mit EINER Spur und `nsek` Sektoren, jeder mit dem
 * angegebenen Groessenkode und 512 Byte Nutzdaten. Flags 0x01 heisst
 * „Sektorbeschreiber vorhanden" und Bit 6 ist klar, also folgen die
 * Sektordaten direkt hinter der (leeren) Fuzzy-Maske — die Seite nennt
 * genau diesen Fall: „If neither Bit 6 or 7 in the track flags is set,
 * after the fuzzy sector mask, the sectors are written to the file."
 *
 * Feldlagen aus derselben Quelle: Dateikopf `'R','S','Y',0`, Version LE16
 * bei 4, Werkzeug LE16 bei 6, Spurzahl bei 0x0a. Spurkopf: Gesamtlaenge
 * LE32 bei 0, Fuzzy-Byte LE32 bei 4, Sektorzahl LE16 bei 8, Flags LE16 bei
 * 0x0a, Spurbildlaenge LE16 bei 0x0c, Spurnummer bei 0x0e, Bildart bei
 * 0x0f. Sektorkopf: Versatz LE32 bei 0, Position LE16 bei 4, Lesezeit LE16
 * bei 6, C/H/R/N bei 8..0x0b, CRC LE16 bei 0x0c, FDC-Status bei 0x0e. */
static uint8_t *stx_bauen(unsigned nsek, uint8_t groessenkode,
                          size_t *out_len)
{
    const unsigned nutz = 512u;
    size_t spur = STX_SPUR_KOPF + (size_t)nsek * STX_SEK_KOPF
                + (size_t)nsek * nutz;
    size_t ges  = STX_KOPF + spur;
    uint8_t *b = (uint8_t *)calloc(1, ges);
    if (!b) return NULL;

    memcpy(b, "RSY\0", 4);
    le16(b + 4, 3);          /* Version  */
    le16(b + 6, 1);          /* Werkzeug: „public"           */
    le16(b + 8, 0);
    b[0x0A] = 1;             /* eine Spur */
    b[0x0B] = 1;

    uint8_t *t = b + STX_KOPF;
    le32(t + 0x00, (unsigned long)spur);
    le32(t + 0x04, 0);       /* keine Fuzzy-Maske */
    le16(t + 0x08, nsek);
    le16(t + 0x0A, 0x0001);  /* Sektorbeschreiber, kein Spurbild */
    le16(t + 0x0C, 0);       /* Spurbildlaenge 0 */
    t[0x0E] = 0;             /* Spur 0, Seite 0 (Bit 7 klar) */
    t[0x0F] = 0;             /* WDC-Abzug */

    uint8_t *daten = t + STX_SPUR_KOPF + (size_t)nsek * STX_SEK_KOPF;
    for (unsigned i = 0; i < nsek; i++) {
        uint8_t *s = t + STX_SPUR_KOPF + (size_t)i * STX_SEK_KOPF;
        le32(s + 0x00, (unsigned long)i * nutz);   /* Versatz ab Datenanfang */
        le16(s + 0x04, 0);
        le16(s + 0x06, 0);
        s[0x08] = 0;                 /* C */
        s[0x09] = 0;                 /* H */
        s[0x0A] = (uint8_t)(i + 1);  /* R */
        s[0x0B] = groessenkode;      /* N — der Gegenstand dieses Tests */
        le16(s + 0x0C, 0);
        s[0x0E] = 0;                 /* FDC-Status: kein RNF, kein Fuzzy */
        s[0x0F] = 0;

        /* Nutzdaten benennen sich selbst. */
        uint8_t *d = daten + (size_t)i * nutz;
        memset(d, (uint8_t)(0xA0 + i), nutz);
        snprintf((char *)d, nutz, "UFT-STX S%02u N%u", i + 1, groessenkode);
    }

    *out_len = ges;
    return b;
}

static int schreiben(const char *pfad, const uint8_t *d, size_t n)
{
    FILE *f = fopen(pfad, "wb");
    if (!f) return 0;
    size_t w = fwrite(d, 1, n, f);
    fclose(f);
    return w == n;
}

/* ── Fall 1: Kode 2 — der Korpusfall, er muss unveraendert bleiben ───── */

TEST(kode_2_bleibt_512_byte)
{
    /* Gruen vor UND nach MF-1165. Steht zuerst, weil die Maskierung an
     * einem gueltigen Kode nichts aendern darf — `2 & 3` ist 2. Das ist die
     * Gegenprobe dazu, dass die Korrektur den Leser nicht verengt. */
    size_t len = 0;
    uint8_t *b = stx_bauen(1, 2, &len);
    ASSERT(b != NULL);
    char p[512]; temp_pfad(p, sizeof(p));
    ASSERT(schreiben(p, b, len));
    free(b);

    uft_disk_t d; memset(&d, 0, sizeof(d)); d.read_only = true;
    ASSERT(uft_format_plugin_stx.open(&d, p, true) == UFT_OK);

    uft_track_t t; memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_stx.read_track(&d, 0, 0, &t) == UFT_OK);
    ASSERT(t.sector_count == 1);
    ASSERT(t.sectors[0].data_len == 512u);
    ASSERT(t.sectors[0].data != NULL);
    ASSERT(strncmp((const char *)t.sectors[0].data, "UFT-STX S01 N2", 14) == 0);

    for (size_t s = 0; s < t.sector_count; s++) free(t.sectors[s].data);
    free(t.sectors); free(t.raw_data);
    uft_format_plugin_stx.close(&d);
    remove(p);
}

/* ── Fall 2: ROTBEWEIS — Kode 6 heisst 512, nicht 8192 ──────────────── */

TEST(kode_6_heisst_512_nicht_8192)
{
    /* Vor MF-1165 gemessen: `128 << 6` = 8192. Die Schranke
     * `sec_pos + 8192 <= size` reisst bei dieser 560-Byte-Datei, also
     * BLEIBT DER malloc-PUFFER UNBESCHRIEBEN und der Leser gibt 8192 Byte
     * uninitialisierten Speicher als Sektordaten heraus. */
    size_t len = 0;
    uint8_t *b = stx_bauen(1, 6, &len);
    ASSERT(b != NULL);
    ASSERT(len == STX_KOPF + STX_SPUR_KOPF + STX_SEK_KOPF + 512u);  /* 560 */
    char p[512]; temp_pfad(p, sizeof(p));
    ASSERT(schreiben(p, b, len));
    free(b);

    uft_disk_t d; memset(&d, 0, sizeof(d)); d.read_only = true;
    ASSERT(uft_format_plugin_stx.open(&d, p, true) == UFT_OK);

    uft_track_t t; memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_stx.read_track(&d, 0, 0, &t) == UFT_OK);
    ASSERT(t.sector_count == 1);
    /* Nur Bits 0-1 gelten: 6 & 3 == 2 -> 512 Byte. */
    ASSERT(t.sectors[0].data_len == 512u);
    /* und die Bytes sind WIRKLICH gelesen, nicht bloss zugesagt: */
    ASSERT(t.sectors[0].data != NULL);
    ASSERT(strncmp((const char *)t.sectors[0].data, "UFT-STX S01 N6", 14) == 0);

    for (size_t s = 0; s < t.sector_count; s++) free(t.sectors[s].data);
    free(t.sectors); free(t.raw_data);
    uft_format_plugin_stx.close(&d);
    remove(p);
}

/* ── Fall 3: ROTBEWEIS — der Versatz des ZWEITEN Sektors ────────────── */

TEST(zwei_sektoren_mit_kode_6_liegen_richtig)
{
    /* Der Versatz jedes Sektors steht in seinem eigenen Kopf, also muss
     * auch Sektor 2 an seiner Stelle ankommen. Vor MF-1165 gab schon
     * Sektor 1 eine falsche Laenge, und beide Puffer blieben unbeschrieben. */
    size_t len = 0;
    uint8_t *b = stx_bauen(2, 6, &len);
    ASSERT(b != NULL);
    char p[512]; temp_pfad(p, sizeof(p));
    ASSERT(schreiben(p, b, len));
    free(b);

    uft_disk_t d; memset(&d, 0, sizeof(d)); d.read_only = true;
    ASSERT(uft_format_plugin_stx.open(&d, p, true) == UFT_OK);

    uft_track_t t; memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_stx.read_track(&d, 0, 0, &t) == UFT_OK);
    ASSERT(t.sector_count == 2);
    for (unsigned i = 0; i < 2; i++) {
        char erwartet[32];
        snprintf(erwartet, sizeof(erwartet), "UFT-STX S%02u N6", i + 1);
        ASSERT(t.sectors[i].data_len == 512u);
        ASSERT(t.sectors[i].data != NULL);
        ASSERT(strncmp((const char *)t.sectors[i].data, erwartet,
                       strlen(erwartet)) == 0);
    }

    for (size_t s = 0; s < t.sector_count; s++) free(t.sectors[s].data);
    free(t.sectors); free(t.raw_data);
    uft_format_plugin_stx.close(&d);
    remove(p);
}

/* ── Fall 4: der rohe Kode bleibt sichtbar ──────────────────────────── */

TEST(der_rohe_kode_wird_nicht_umgeschrieben)
{
    /* Maskiert wird die abgeleitete LAENGE, nicht die Angabe der Datei.
     * `uft_stx_sector_view_t.id_size` muss weiter 6 melden — „das Format
     * sagt 6" und „hier stehen 512 Byte" sind zwei Aussagen (MF-980).
     * Der Weg geht ueber den AIR-Port, weil nur seine Sicht das Feld
     * herausgibt; das Plugin bildet es nicht ab. */
    size_t len = 0;
    uint8_t *b = stx_bauen(1, 6, &len);
    ASSERT(b != NULL);

    stx_air_handle_t *air = uft_stx_air_open(b, len);
    free(b);
    ASSERT(air != NULL);

    uft_stx_sector_view_t sv;
    memset(&sv, 0, sizeof(sv));
    ASSERT(uft_stx_air_sector(air, 0, 0, 0, &sv));
    ASSERT(sv.id_size == 6);          /* unveraendert aus der Datei */
    ASSERT(sv.size    == 512u);       /* abgeleitet, maskiert       */
    ASSERT(sv.id_number == 1);
    ASSERT(!sv.rnf && !sv.fuzzy);

    uft_stx_air_close(air);
}

int main(void)
{
    printf("=== STX: nur Bits 0-1 des Groessenkodes gelten (MF-1165) ===\n");
    RUN(kode_2_bleibt_512_byte);
    RUN(kode_6_heisst_512_nicht_8192);
    RUN(zwei_sektoren_mit_kode_6_liegen_richtig);
    RUN(der_rohe_kode_wird_nicht_umgeschrieben);
    printf("\nErgebnis: %d bestanden, %d gefallen\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
