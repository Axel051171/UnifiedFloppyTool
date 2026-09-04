/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_a2r_layout.c
 * @brief Der A2R-Leser gegen die veroeffentlichte Struktur (MF-868)
 *
 * ── Der Befund ───────────────────────────────────────────────────────
 *
 * Der Leser meldete `A2R_OK` und lieferte NICHTS. An einer echten Datei
 * gemessen (Applesauce v1.88.4, A2R3, `tests/corpus/kor_a/…a2r`):
 * `a2r_is_valid_file()` sagte ja, `a2r_get_file_version()` lieferte
 * richtig 3, aber `a2r_get_info()` gab `OK` mit lauter Nullen zurueck —
 * Version 0, leerer Erzeuger, Disk-Typ „Unknown" — und
 * `a2r_read_track()` fand fuer die Spuren 0..11 keine einzige Aufnahme.
 *
 * ── Zwei unabhaengige Haende, gleiche Zahlen ─────────────────────────
 *
 * Die Struktur unten stammt aus der **veroeffentlichten Referenz**
 * „A2R 3.x Disk Image Reference" (applesaucefdc.com/a2r/) und ist
 * byteweise an der echten Datei nachgemessen. Beide stimmen ueberein:
 *
 *   INFO, 37 Byte (der Leser verlangte 60):
 *     +0   1   INFO Version         (echte Datei: 1)
 *     +1  32   Creator, mit 0x20 gefuellt ("Applesauce v1.88.4")
 *     +33  1   Drive Type           (echte Datei: 6)
 *     +34  1   Write Protected      (1)
 *     +35  1   Synchronized         (1)
 *     +36  1   Hard Sector Count    (0)
 *
 *   RWCP-Kopf, 16 Byte (der Leser uebersprang ihn GAR NICHT):
 *     +0   1   RWCP Version
 *     +1   4   Resolution in Pikosekunden je Tick
 *     +5  11   Reserve
 *
 *   je Aufnahme:
 *     +0   1   Mark  0x43 'C' = Aufnahme, 0x58 'X' = Ende
 *     +1   1   Capture Type (1 timing, 2 bits, 3 xtiming)
 *     +2   2   Location (uint16!)
 *     +4   1   Zahl der Index-Signale
 *     +5   4*N Index-Signale
 *     ..   4   Groesse der Aufnahmedaten
 *     ..   N   Aufnahmedaten
 *
 * Der Leser nahm stattdessen an: `location` in Byte 0 (dort steht die
 * MARK), `side` in Byte 2 (dort steht das untere Byte der Location),
 * `data_len` in Byte 6 (dort steht das Index-Array) und eine feste
 * Eintragsgroesse von 10 Byte. Und den Erzeuger las er ab Byte 0, wo
 * die INFO-Version steht — jedes Feld um eins verschoben.
 *
 * ── Warum die Aufloesung aus der DATEI kommen muss ───────────────────
 *
 * `A2R_TICK_NS` ist im Baum die feste Zahl 125. Die Referenz fuehrt die
 * Aufloesung als Feld im RWCP-Kopf. Eine feste Konstante rechnet jede
 * Aufnahme mit einer anderen Abtastrate falsch um — und Dauer und
 * Drehzahl haengen daran.
 *
 * ── Was dieser Test NICHT belegt ─────────────────────────────────────
 *
 * Er laeuft gegen eine ERZEUGTE Datei. Die Messung an der echten
 * 28-MB-Aufnahme steht im Commit-Text von MF-868; sie kann hier nicht
 * stehen, weil `tests/corpus/` ungetrackt ist (Rechtslage ungeklaert,
 * Zone PRUEFEN).
 */
#include "uft/parsers/uft_a2r_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last = 0;
#define RUN(n)  do { printf("  [TEST] %-46s ... ", #n); test_##n(); \
                     if (_last == _fail) { printf("OK\n"); _pass++; } \
                     _last = _fail; } while (0)
#define TEST(n) static void test_##n(void)
#define ASSERT(c) do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                       _fail++; return; } } while (0)

#define ERZEUGER   "UFT Testfixture"
#define DRIVE_TYPE 6u
#define AUFLOESUNG 125000u        /* ps je Tick == 125 ns */

static const char *TMP = "test_a2r_layout.tmp.a2r";

static void put_u16(uint8_t *b, uint16_t v)
{ b[0] = (uint8_t)(v & 0xFF); b[1] = (uint8_t)(v >> 8); }

static void put_u32(uint8_t *b, uint32_t v)
{ b[0]=(uint8_t)(v); b[1]=(uint8_t)(v>>8); b[2]=(uint8_t)(v>>16); b[3]=(uint8_t)(v>>24); }

/**
 * Baut eine A2R3-Datei nach der veroeffentlichten Struktur:
 * zwei Aufnahmen auf Location 0, eine auf Location 4.
 */
static int datei_bauen(const char *pfad)
{
    uint8_t buf[4096];
    size_t p = 0;

    /* Dateikopf: 'A2R3' FF 0A 0D 0A */
    memcpy(&buf[p], "A2R3", 4); p += 4;
    buf[p++] = 0xFF; buf[p++] = 0x0A; buf[p++] = 0x0D; buf[p++] = 0x0A;

    /* INFO-Chunk: 37 Byte Nutzlast */
    memcpy(&buf[p], "INFO", 4); p += 4;
    put_u32(&buf[p], 37); p += 4;
    size_t info = p;
    buf[p++] = 1;                                  /* INFO Version      */
    memset(&buf[p], 0x20, 32);                     /* Creator, 0x20     */
    memcpy(&buf[p], ERZEUGER, strlen(ERZEUGER));
    p += 32;
    buf[p++] = DRIVE_TYPE;                         /* Drive Type        */
    buf[p++] = 1;                                  /* Write Protected   */
    buf[p++] = 1;                                  /* Synchronized      */
    buf[p++] = 0;                                  /* Hard Sector Count */
    if (p - info != 37) return -1;

    /* RWCP-Chunk */
    memcpy(&buf[p], "RWCP", 4); p += 4;
    size_t rwcp_len_at = p; p += 4;                /* Laenge, spaeter   */
    size_t rwcp = p;

    buf[p++] = 1;                                  /* RWCP Version      */
    put_u32(&buf[p], AUFLOESUNG); p += 4;          /* ps je Tick        */
    memset(&buf[p], 0, 11); p += 11;               /* Reserve           */

    /* Drei Aufnahmen. Die Flussbytes sind bewusst klein und gleich,
     * damit die erwartete Dauer nachrechenbar bleibt. */
    const struct { uint16_t location; uint8_t typ; uint8_t idx; uint32_t n; }
        AUFN[3] = { {0, 1, 1, 8}, {0, 1, 1, 8}, {4, 1, 2, 16} };

    for (int a = 0; a < 3; a++) {
        buf[p++] = 0x43;                           /* Mark 'C'          */
        buf[p++] = AUFN[a].typ;                    /* Capture Type      */
        put_u16(&buf[p], AUFN[a].location); p += 2;
        buf[p++] = AUFN[a].idx;                    /* Index-Signale     */
        for (uint8_t k = 0; k < AUFN[a].idx; k++) {
            put_u32(&buf[p], 1000u * (k + 1)); p += 4;
        }
        put_u32(&buf[p], AUFN[a].n); p += 4;       /* Datengroesse      */
        for (uint32_t k = 0; k < AUFN[a].n; k++)
            buf[p++] = 0x20;                       /* 32 Ticks je Wechsel */
    }
    buf[p++] = 0x58;                               /* Mark 'X' = Ende   */

    put_u32(&buf[rwcp_len_at], (uint32_t)(p - rwcp));

    FILE *f = fopen(pfad, "wb");
    if (!f) return -1;
    size_t n = fwrite(buf, 1, p, f);
    fclose(f);
    return (n == p) ? 0 : -1;
}

TEST(die_datei_wird_als_a2r3_erkannt)
{
    ASSERT(datei_bauen(TMP) == 0);
    ASSERT(a2r_is_valid_file(TMP));
    ASSERT(a2r_get_file_version(TMP) == 3);
}

TEST(der_kopf_wird_richtig_gelesen)
{
    /* DER ROTBEWEIS, Teil 1. Vor MF-868 verlangte `parse_info_chunk()`
     * 60 Byte und las jedes Feld um eins verschoben — heraus kam ein
     * genullter Datensatz mit `A2R_OK` daran. */
    ASSERT(datei_bauen(TMP) == 0);
    a2r_context_t *ctx = a2r_open(TMP);
    ASSERT(ctx != NULL);

    a2r_info_t info;
    memset(&info, 0, sizeof info);
    a2r_error_t rc = a2r_get_info(ctx, &info);
    if (rc != A2R_OK) {
        printf("\n      a2r_get_info: rc=%d (%s)\n      ",
               rc, a2r_error_string(rc));
        _fail++;
        a2r_close(ctx);
        return;
    }

    if (info.version != 3 || info.disk_type != DRIVE_TYPE ||
        !info.write_protected || !info.synchronized ||
        strncmp(info.creator, ERZEUGER, strlen(ERZEUGER)) != 0) {
        printf("\n      Version %u, Typ %u, wp=%d sync=%d, Erzeuger \"%s\"\n"
               "      erwartet 3 / %u / 1 / 1 / \"%s\"\n      ",
               info.version, info.disk_type, (int)info.write_protected,
               (int)info.synchronized, info.creator,
               DRIVE_TYPE, ERZEUGER);
        _fail++;
    }
    a2r_close(ctx);
    remove(TMP);
}

TEST(die_aufnahmen_werden_gefunden)
{
    /* DER ROTBEWEIS, Teil 2. Der Leser nahm eine feste Eintragsgroesse
     * von 10 Byte an und las die Location aus der MARK — er kam nie an
     * die zweite Aufnahme. */
    ASSERT(datei_bauen(TMP) == 0);
    a2r_context_t *ctx = a2r_open(TMP);
    ASSERT(ctx != NULL);

    a2r_track_t tr;
    memset(&tr, 0, sizeof tr);
    a2r_error_t rc = a2r_read_track(ctx, 0, 0, &tr);
    if (rc != A2R_OK || tr.capture_count != 2) {
        printf("\n      Location 0: rc=%d, %u Aufnahmen, erwartet 2\n      ",
               rc, tr.capture_count);
        _fail++;
    } else {
        for (uint8_t c = 0; c < tr.capture_count; c++) {
            if (tr.captures[c].data_length != 8) {
                printf("\n      Aufnahme %u: %u Byte, erwartet 8\n      ",
                       c, tr.captures[c].data_length);
                _fail++;
                break;
            }
        }
    }
    a2r_free_track(&tr);

    memset(&tr, 0, sizeof tr);
    rc = a2r_read_track(ctx, 4, 0, &tr);
    if (rc != A2R_OK || tr.capture_count != 1 ||
        tr.captures[0].data_length != 16) {
        printf("\n      Location 4: rc=%d, %u Aufnahmen a %u Byte, "
               "erwartet 1 a 16\n      ", rc, tr.capture_count,
               tr.capture_count ? tr.captures[0].data_length : 0u);
        _fail++;
    }
    a2r_free_track(&tr);

    a2r_close(ctx);
    remove(TMP);
}

TEST(die_dauer_kommt_aus_der_aufloesung_der_datei)
{
    /* DER ROTBEWEIS, Teil 3. `A2R_TICK_NS` ist die feste Zahl 125; die
     * Referenz fuehrt die Aufloesung als Feld im RWCP-Kopf. Acht
     * Flussbytes zu je 32 Ticks bei 125 000 ps = 125 ns ergeben
     * 8 * 32 * 125 ns = 32 000 ns = 32 us. */
    ASSERT(datei_bauen(TMP) == 0);
    a2r_context_t *ctx = a2r_open(TMP);
    ASSERT(ctx != NULL);

    a2r_track_t tr;
    memset(&tr, 0, sizeof tr);
    ASSERT(a2r_read_track(ctx, 0, 0, &tr) == A2R_OK);
    ASSERT(tr.capture_count >= 1);

    double soll = 8.0 * 32.0 * 0.125;      /* us */
    double ist  = tr.captures[0].duration_us;
    if (ist < soll * 0.99 || ist > soll * 1.01) {
        printf("\n      Dauer %.3f us, erwartet %.3f us\n      ", ist, soll);
        _fail++;
    }
    a2r_free_track(&tr);
    a2r_close(ctx);
    remove(TMP);
}

TEST(eine_abgeschnittene_datei_wird_nicht_stillschweigend_angenommen)
{
    /* Gegenprobe: der urspruengliche Fehler war nicht, dass etwas nicht
     * ging — sondern dass `A2R_OK` daran stand. Eine Datei mit
     * abgeschnittenem INFO darf nicht als heil gelten. */
    ASSERT(datei_bauen(TMP) == 0);

    FILE *f = fopen(TMP, "rb");
    ASSERT(f != NULL);
    uint8_t buf[4096];
    size_t n = fread(buf, 1, sizeof buf, f);
    fclose(f);
    ASSERT(n > 30);

    FILE *g = fopen(TMP, "wb");
    ASSERT(g != NULL);
    fwrite(buf, 1, 24, g);          /* mitten im INFO abgeschnitten */
    fclose(g);

    a2r_context_t *ctx = a2r_open(TMP);
    if (ctx) {
        a2r_info_t info;
        memset(&info, 0, sizeof info);
        a2r_error_t rc = a2r_get_info(ctx, &info);
        if (rc == A2R_OK && info.creator[0] == '\0') {
            printf("\n      abgeschnittene Datei: A2R_OK mit leerem "
                   "Datensatz — genau der Ausgangsfehler\n      ");
            _fail++;
        }
        a2r_close(ctx);
    }
    remove(TMP);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== A2R: Struktur gegen die Referenz (MF-868) ===\n");
    RUN(die_datei_wird_als_a2r3_erkannt);
    RUN(der_kopf_wird_richtig_gelesen);
    RUN(die_aufnahmen_werden_gefunden);
    RUN(die_dauer_kommt_aus_der_aufloesung_der_datei);
    RUN(eine_abgeschnittene_datei_wird_nicht_stillschweigend_angenommen);
    printf("\nErgebnis: %d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
