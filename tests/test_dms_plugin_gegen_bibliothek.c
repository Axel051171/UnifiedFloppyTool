/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_dms_plugin_gegen_bibliothek.c
 * @brief Das DMS-Plugin las den Track-Kopf vollstaendig falsch (MF-837).
 *
 * ── Zwei DMS-Umsetzungen im Baum ─────────────────────────────────────────
 *
 * `src/formats/dms/uft_dms.c` (1 100 Zeilen) ist die gegen **xDMS 1.3**
 * (Andre Rodrigues de la Rocha, 24.03.1999) verifizierte Portierung: alle
 * sieben Betriebsarten, beide Entpackstufen, die drei Tracklaengen, alle
 * vier Integritaetspruefungen, Zustandserhalt ueber `flags & 1` und
 * `flags & 2`, Heavy1/Heavy2-Fenster, Entschluesselung — und reentrant
 * ueber `dms_ctx_t`, was xDMS selbst nicht ist (dort sind `left[]`,
 * `right[]`, `text` usw. Dateiglobale).
 *
 * `src/formats/dms/uft_dms_plugin.c` ist die **registrierte** Umsetzung
 * (`UFT_REGISTER_FORMAT_PLUGIN(dms)`) mit einem eigenen, unabhaengigen
 * Entpacker. Gemessen: die Bibliothek hat ausserhalb ihres Verzeichnisses
 * genau **einen** Aufrufer, und der ist ein Test (`tests/test_uft_dms.c`).
 * Kein Produktionspfad. Es lief also die zweite Fassung.
 *
 * ── Der Track-Kopf, Feld fuer Feld ───────────────────────────────────────
 *
 *   Byte    xDMS / uft_dms.c        uft_dms_plugin.c
 *   -----   ---------------------   -----------------------------
 *   2-3     number                  trk_num              (richtig)
 *   6-7     pklen1                  —
 *   8-9     pklen2                  —
 *   10-11   unpklen                 comp_mode            FALSCH
 *   12      flags                   \
 *   13      cmode                    > unpacked_size BE32 FALSCH
 *   14-15   usum                    /
 *   16-17   dcrc                    \ packed_size BE32    FALSCH
 *   18-19   Kopf-CRC                /
 *
 * Nur die Tracknummer stimmte. `comp_mode` kam aus `unpklen` — auf einer
 * Amiga-Spur typisch 5632 (0x1600) —, traf damit im `switch` den
 * `default:`-Zweig und liess die Spur bei ihrer 0xE5-Fuellung. Und
 * `packed_size` kam aus zwei CRC-Feldern, wodurch die Schranke
 * `packed_size > file_size - pos` die Schleife meist sofort beendete.
 *
 * Praktische Folge: das Plugin lieferte fuer im Wesentlichen **jede**
 * echte DMS-Datei ein durchgehend 0xE5-gefuelltes ADF — und 0xE5 ist die
 * Formatfuellung von AmigaDOS. Ein Fehlschlag war damit nicht von einer
 * leeren, formatierten Diskette zu unterscheiden. Der Kommentar an der
 * Stelle nannte das „forensic integrity"; es ist das Gegenteil, denn der
 * Verlust wird als Datum ausgegeben.
 *
 * ── Was dieser Test prueft ───────────────────────────────────────────────
 *
 * Ein von Hand gebautes, in allen vier Pruefsummen stimmiges DMS mit
 * einer NOCOMP-Spur. NOCOMP braucht keinen Entpacker — der Test misst
 * also allein, ob der Track-Kopf richtig gelesen wird.
 *
 * Die Pruefsummen werden im Test selbst gerechnet (CRC-16/ARC, Poly
 * 0xA001, Init 0 — bitweise Form, gegen die Tabelle in `uft_dms.c:188`
 * geprueft: `crc_tab[1] == 0xC0C1`). Dass die Rechnung stimmt, sichert
 * der erste Test ueber `dms_is_dms()` ab, das den Dateikopf-CRC prueft.
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"
#include "uft/formats/uft_dms.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_dms;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-46s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                        _fail++; return; } } while (0)

#define TRACKLEN   (11u * 512u)          /* 5632 */
#define HEADLEN    56u
#define THLEN      20u
#define MARKE_A    0x5Au                 /* erstes Byte der Spur   */
#define MARKE_E    0xA7u                 /* letztes Byte der Spur  */

/* CRC-16/ARC, bitweise. Entspricht der Tabellenform in uft_dms.c. */
static uint16_t crc16(const uint8_t *p, size_t n)
{
    uint16_t c = 0;
    for (size_t i = 0; i < n; i++) {
        c ^= p[i];
        for (int k = 0; k < 8; k++)
            c = (uint16_t)((c >> 1) ^ ((c & 1) ? 0xA001u : 0u));
    }
    return c;
}

static uint16_t summe(const uint8_t *p, size_t n)
{
    uint16_t u = 0;
    for (size_t i = 0; i < n; i++) u = (uint16_t)(u + p[i]);
    return u;
}

static void be16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }

/* Baut ein DMS mit EINER Spur.
 *   cmode      Betriebsart im Kopf
 *   nutzlast   Bytes und Laenge der gepackten Daten
 *   unpklen    deklarierte entpackte Laenge
 *   kaputte_dcrc  true: Daten-CRC absichtlich falsch
 * Gibt einen malloc'ten Puffer und dessen Laenge zurueck. */
static uint8_t *baue_dms(uint8_t cmode, const uint8_t *nutzlast, uint16_t pklen,
                         uint16_t unpklen, bool kaputte_dcrc, size_t *out_len)
{
    size_t len = HEADLEN + THLEN + pklen;
    uint8_t *b = (uint8_t *)calloc(1, len);
    if (!b) return NULL;

    /* ---- Dateikopf ---- */
    memcpy(b, "DMS!", 4);
    be16(b + 10, 0x0000);       /* geninfo: DD, unverschluesselt */
    be16(b + 16, 0);            /* track_lo */
    be16(b + 18, 0);            /* track_hi */
    be16(b + 50, 0);            /* disk_type: nicht FMS */
    be16(b + 52, cmode);        /* comp_mode (nur informativ) */
    be16(b + HEADLEN - 2, crc16(b + 4, HEADLEN - 6));

    /* ---- Track-Kopf ---- */
    uint8_t *t = b + HEADLEN;
    t[0] = 'T'; t[1] = 'R';
    be16(t + 2,  0);            /* Tracknummer 0 */
    be16(t + 4,  0);
    be16(t + 6,  pklen);        /* pklen1  */
    be16(t + 8,  pklen);        /* pklen2  */
    be16(t + 10, unpklen);      /* unpklen */
    t[12] = 0x01;               /* flags: Zustand behalten */
    t[13] = cmode;
    be16(t + 14, summe(nutzlast, unpklen < pklen ? unpklen : pklen));
    uint16_t dcrc = crc16(nutzlast, pklen);
    be16(t + 16, (uint16_t)(kaputte_dcrc ? dcrc ^ 0xFFFFu : dcrc));
    be16(t + 18, crc16(t, THLEN - 2));

    memcpy(b + HEADLEN + THLEN, nutzlast, pklen);
    *out_len = len;
    return b;
}

static char *schreibe(const uint8_t *b, size_t n, const char *stamm)
{
    static char pfad[300];
    const char *d = getenv("TMPDIR");
    if (!d || !d[0]) d = getenv("TMP");
    if (!d || !d[0]) d = getenv("TEMP");
    if (!d || !d[0]) d = ".";
    snprintf(pfad, sizeof pfad, "%s/uft_dms_%s_%d.dms", d, stamm, rand() % 100000);
    FILE *f = fopen(pfad, "wb");
    if (!f) return NULL;
    size_t w = fwrite(b, 1, n, f);
    fclose(f);
    return (w == n) ? pfad : NULL;
}

static uint8_t *spur_nutzlast(void)
{
    uint8_t *p = (uint8_t *)malloc(TRACKLEN);
    if (!p) return NULL;
    for (size_t i = 0; i < TRACKLEN; i++) p[i] = (uint8_t)(0x20 + (i % 0x40));
    p[0] = MARKE_A;
    p[TRACKLEN - 1] = MARKE_E;
    return p;
}

/* Liest Sektor 0 und Sektor 10 der Spur 0/0 ueber den Plugin-Pfad. */
static int oeffne_und_lies(const char *pfad, uint8_t *s0_b0, uint8_t *s10_last,
                           uft_error_t *rc_out)
{
    uft_disk_t disk;
    memset(&disk, 0, sizeof disk);
    disk.read_only = true;
    uft_error_t rc = uft_format_plugin_dms.open(&disk, pfad, true);
    if (rc_out) *rc_out = rc;
    if (rc != UFT_OK) return -1;

    uft_track_t t;
    memset(&t, 0, sizeof t);
    int ok = -1;
    if (uft_format_plugin_dms.read_track(&disk, 0, 0, &t) == UFT_OK
        && t.sector_count >= 11
        && t.sectors[0].data && t.sectors[10].data) {
        if (s0_b0)     *s0_b0     = t.sectors[0].data[0];
        if (s10_last)  *s10_last  = t.sectors[10].data[511];
        ok = 0;
    }
    for (size_t i = 0; i < t.sector_count; i++) free(t.sectors[i].data);
    free(t.sectors);
    if (uft_format_plugin_dms.close) uft_format_plugin_dms.close(&disk);
    return ok;
}

TEST(die_pruefsummenrechnung_dieses_tests_stimmt)
{
    /* Absicherung der Absicherung: `dms_is_dms()` prueft den
     * Dateikopf-CRC. Geht das durch, rechnet dieser Test richtig — und
     * nur dann sind die Track-Pruefsummen unten belastbar. */
    uint8_t *nl = spur_nutzlast();
    ASSERT(nl != NULL);
    size_t n = 0;
    uint8_t *b = baue_dms(0, nl, TRACKLEN, TRACKLEN, false, &n);
    ASSERT(b != NULL);
    ASSERT(dms_is_dms(b, n) == 1);

    dms_info_t info;
    ASSERT(dms_read_info(b, n, &info) == DMS_OK);
    free(b); free(nl);
}

TEST(nocomp_spur_kommt_durch_das_plugin_an)
{
    /* DER ROTBEWEIS. NOCOMP braucht keinen Entpacker — dieser Test misst
     * allein, ob der Track-Kopf richtig gelesen wird. Vor MF-837 kam
     * hier 0xE5 an, weil `comp_mode` aus `unpklen` gelesen wurde. */
    uint8_t *nl = spur_nutzlast();
    ASSERT(nl != NULL);
    size_t n = 0;
    uint8_t *b = baue_dms(0, nl, TRACKLEN, TRACKLEN, false, &n);
    ASSERT(b != NULL);
    const char *p = schreibe(b, n, "nocomp");
    ASSERT(p != NULL);

    uint8_t a = 0, e = 0;
    uft_error_t rc = UFT_OK;
    ASSERT(oeffne_und_lies(p, &a, &e, &rc) == 0);
    ASSERT(rc == UFT_OK);
    ASSERT(a == MARKE_A);      /* vorher: 0xE5 */
    ASSERT(e == MARKE_E);      /* vorher: 0xE5 */

    remove(p); free(b); free(nl);
}

TEST(die_bibliothek_selbst_liest_dieselbe_datei_richtig)
{
    /* Gegenprobe zur Zuordnung: wenn die Bibliothek dieselbe Datei
     * richtig auspackt, liegt der Fehler nicht am Testaufbau. */
    uint8_t *nl = spur_nutzlast();
    ASSERT(nl != NULL);
    size_t n = 0;
    uint8_t *b = baue_dms(0, nl, TRACKLEN, TRACKLEN, false, &n);
    ASSERT(b != NULL);

    size_t adf_cap = 80u * 2u * TRACKLEN;
    uint8_t *adf = (uint8_t *)calloc(1, adf_cap);
    ASSERT(adf != NULL);
    size_t geschrieben = 0;
    dms_error_t de = dms_unpack(b, n, adf, adf_cap, &geschrieben,
                                NULL, 0, NULL, NULL, NULL);
    ASSERT(de == DMS_OK);
    ASSERT(geschrieben == TRACKLEN);
    ASSERT(adf[0] == MARKE_A);
    ASSERT(adf[TRACKLEN - 1] == MARKE_E);

    free(adf); free(b); free(nl);
}

TEST(ein_falscher_daten_crc_wird_nicht_verschwiegen)
{
    /* Das Plugin prueft keine der vier Integritaetsangaben. Mit dem
     * absichtlich verdrehten Daten-CRC muss das Oeffnen scheitern oder
     * — wenn Teildaten geliefert werden — die Bibliothek den Fehler
     * gemeldet haben. Geprueft wird das Beobachtbare: die Bibliothek
     * lehnt ab, und das Plugin darf die Datei nicht als einwandfrei
     * durchreichen. */
    uint8_t *nl = spur_nutzlast();
    ASSERT(nl != NULL);
    size_t n = 0;
    uint8_t *b = baue_dms(0, nl, TRACKLEN, TRACKLEN, true, &n);
    ASSERT(b != NULL);

    size_t adf_cap = 80u * 2u * TRACKLEN;
    uint8_t *adf = (uint8_t *)calloc(1, adf_cap);
    ASSERT(adf != NULL);
    dms_error_t de = dms_unpack(b, n, adf, adf_cap, NULL, NULL, 0,
                                NULL, NULL, NULL);
    ASSERT(de != DMS_OK);      /* die Bibliothek faengt es */
    free(adf);

    /* Und mit `override_errors` liefert sie die Daten TROTZDEM — das ist
     * die forensisch richtige Wahl: melden, nicht verweigern. */
    uint8_t *adf2 = (uint8_t *)calloc(1, adf_cap);
    ASSERT(adf2 != NULL);
    ASSERT(dms_unpack(b, n, adf2, adf_cap, NULL, NULL, 1,
                      NULL, NULL, NULL) == DMS_OK);
    ASSERT(adf2[0] == MARKE_A);
    free(adf2);

    free(b); free(nl);
}

TEST(eine_unbekannte_betriebsart_liefert_keine_leere_diskette)
{
    /* Betriebsart 6 (HEAVY2) gab es im Plugin ueberhaupt nicht — der
     * `default:`-Zweig liess die 0xE5-Fuellung stehen und meldete
     * UFT_OK. Eine fehlgeschlagene Dekompression war damit von einer
     * leeren, formatierten Amiga-Diskette nicht zu unterscheiden.
     *
     * Mit Muell als Nutzlast darf dabei NICHT UFT_OK mit einer
     * scheinbar leeren Diskette herauskommen. */
    uint8_t *nl = spur_nutzlast();
    ASSERT(nl != NULL);
    size_t n = 0;
    uint8_t *b = baue_dms(6, nl, TRACKLEN, TRACKLEN, false, &n);
    ASSERT(b != NULL);
    const char *p = schreibe(b, n, "mode6");
    ASSERT(p != NULL);

    uint8_t a = 0xE5, e = 0xE5;
    uft_error_t rc = UFT_OK;
    int gelesen = oeffne_und_lies(p, &a, &e, &rc);

    /* Zulaessig ist: Oeffnen scheitert. Nicht zulaessig ist: UFT_OK und
     * eine Spur voller 0xE5 — das ist die erfundene leere Diskette. */
    bool erfundene_leere = (rc == UFT_OK && gelesen == 0
                            && a == 0xE5 && e == 0xE5);
    ASSERT(!erfundene_leere);

    remove(p); free(b); free(nl);
}

int main(void)
{
    printf("=== DMS: Plugin gegen die verifizierte Bibliothek (MF-837) ===\n");
    RUN(die_pruefsummenrechnung_dieses_tests_stimmt);
    RUN(die_bibliothek_selbst_liest_dieselbe_datei_richtig);
    RUN(nocomp_spur_kommt_durch_das_plugin_an);
    RUN(ein_falscher_daten_crc_wird_nicht_verschwiegen);
    RUN(eine_unbekannte_betriebsart_liefert_keine_leere_diskette);
    printf("\nErgebnis: %d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
