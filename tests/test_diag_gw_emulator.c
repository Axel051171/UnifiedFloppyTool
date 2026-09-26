/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_diag_gw_emulator.c
 * @brief Oberflaechen-Scan ueber den Greaseweazle-Leseweg, gegen den
 *        Firmware-Automaten (Tuerstufe 4, Teil B).
 *
 * ── Kette ────────────────────────────────────────────────────────────
 *
 *   uft_diag_surface_scan()
 *     -> uft_diag_gw_read()                  (src/diag/uft_diag_gw.c)
 *       -> uft_gw_read_track()               (Produktionstreiber, geschuetzt)
 *         -> Drahtrahmen -> gw_wire_bridge.c -> Firmware-Automat
 *       -> flux_decode_track()               (Produktionsdekoder)
 *
 * Der Fluss, den der Automat ausliefert, entsteht aus UFTs IBM-MFM-
 * Encoder (`uft_mfm_encode_track`, MF-539 gegen den Dekoder abgenommen)
 * und dem Stromkodierer des Treibers selbst (`uft_gw_encode_flux_stream`).
 *
 * ── Grenze, ausdruecklich ────────────────────────────────────────────
 *
 * Encoder und Dekoder sind beide UFT-Code; dieser Test ist KEIN Beleg
 * fuer die Dekodierung, sondern fuer die VERDRAHTUNG: dass jede
 * Wiederholung eine neue Erfassung ist, dass Sektornummern, Seite und
 * Ortsmarke richtig ankommen, dass Pruefsummenfehler nicht als gelesen
 * gelten, und dass kein Schreibbefehl die Leitung erreicht. Geprueft wird
 * gegen ein MODELL, nicht gegen ein Geraet (MF-310); der Automat fuehrt
 * seine Abweichungen in tests/emulators/greaseweazle/DIVERGENCES.md.
 *
 * Nicht ausgeuebt: Indexmarken im Strom (der synthetische Strom traegt
 * keine), damit auch nicht das Aufsummieren der Indexzeiten im Adapter.
 */
#include "emulators/greaseweazle/gw_wire_bridge.h"
#include "uft/uft_types.h"
#include "uft/uft_mfm_encoder.h"
#include "uft/diag/uft_diag_gw.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *hinweis)
{
    if (ok) {
        printf("  [ok ] %s\n", was);
        gruen++;
    } else {
        printf("  [ROT] %s%s%s\n", was, hinweis ? " -- " : "", hinweis ? hinweis : "");
        rot++;
    }
}

#define SPT            9
#define SEKTOR         512
#define ZELLEN_CAP     32768
#define TAKT_HZ        72000000u
#define TICKS_JE_ZELLE 144u          /* MFM DD: 2 us je Zelle bei 72 MHz */

/* Die Diskette, die der Automat abspielt: 2 Zylinder x 2 Koepfe,
 * CPC-Datennummern 0xC1..0xC9. */
typedef struct {
    gw_fw_t          fw;
    gw_wire_t        draht;
    uft_gw_device_t *dev;

    int  kaputt_zyl, kaputt_kopf, kaputt_id;  /* Datenpruefsumme falsch ... */
    int  kaputt_erfassungen;                  /* ... in den ersten N Erfassungen */
    int  idcrc_zyl, idcrc_kopf, idcrc_id;     /* Pruefsumme des SEKTORKOPFS falsch */
    int  fremde_ortsmarke_zyl;                /* dieser Zylinder traegt C = 5 */
    bool kopf1_traegt_h0;                     /* Seite 1 schreibt H = 0 in den Kopf */
    int  erfassungen[2][2];                   /* je Stelle, gezaehlt im Haken */

    uint8_t  *strom;
    size_t    strom_len;
    uint32_t *abstaende;
} stand_t;

static void nutzlast(uint8_t *p, int zyl, int kopf, int id)
{
    for (int i = 0; i < SEKTOR; i++)
        p[i] = (uint8_t)(zyl * 31 + kopf * 17 + id + i * 7);
}

/* @p r_ersatz_id: der Sektorkopf dieses Sektors traegt R = id ^ 0x80
 * (nur als Spender fuer eine falsche Kopfpruefsumme, siehe lade_strom). */
static size_t kodiere(uint8_t *zellen, int zyl_im_id, int kopf_im_id, int kopf,
                      int zyl, int kaputter_id, int kippen, int r_ersatz_id)
{
    static uint8_t daten[SPT][SEKTOR];
    uft_sector_t secs[SPT];
    memset(secs, 0, sizeof secs);
    for (int k = 0; k < SPT; k++) {
        int id = 0xC1 + k;
        nutzlast(daten[k], zyl, kopf, id);
        if (kippen && id == kaputter_id) daten[k][100] ^= 0x5A;
        secs[k].id.cylinder  = (uint8_t)zyl_im_id;
        secs[k].id.head      = (uint8_t)kopf_im_id;
        secs[k].id.sector    = (uint8_t)(id == r_ersatz_id ? (id ^ 0x80) : id);
        secs[k].id.size_code = 2;
        secs[k].data         = daten[k];
        secs[k].data_len     = SEKTOR;
        secs[k].data_size    = SEKTOR;
    }
    uft_mfm_encode_params_t p = UFT_MFM_PARAMS_DEFAULT_DD;
    /* C und H des Sektorkopfs kommen aus den Argumenten, nicht aus
     * secs[].id (gelesen in uft_mfm_encoder.c: encode_sector). */
    return uft_mfm_encode_track(secs, SPT, (uint8_t)zyl_im_id, (uint8_t)kopf_im_id,
                                &p, zellen, ZELLEN_CAP);
}

static int zelle(const uint8_t *z, size_t i) { return (z[i >> 3] >> (7 - (i & 7))) & 1; }
static void setze(uint8_t *z, size_t i, int v)
{
    uint8_t m = (uint8_t)(1u << (7 - (i & 7)));
    if (v) z[i >> 3] |= m; else z[i >> 3] &= (uint8_t)~m;
}

/* Haken der Bruecke: vor JEDEM ReadFlux den Strom fuer die Stelle bauen,
 * auf der der Automat gerade steht. */
static void lade_strom(void *ud, unsigned lesung)
{
    (void)lesung;
    stand_t *s = (stand_t *)ud;
    int zyl = s->fw.current_cyl, kopf = s->fw.current_head;
    if (zyl < 0 || zyl > 1 || kopf > 1) { gw_fw_load_read_stream(&s->fw, NULL, 0); return; }
    int nr = s->erfassungen[zyl][kopf]++;

    static uint8_t zellen[ZELLEN_CAP], zellen2[ZELLEN_CAP];
    int zyl_im_id  = (zyl == s->fremde_ortsmarke_zyl) ? 5 : zyl;
    int kopf_im_id = (s->kopf1_traegt_h0 && kopf == 1) ? 0 : kopf;
    size_t n = kodiere(zellen, zyl_im_id, kopf_im_id, kopf, zyl, -1, 0, -1);

    if (zyl == s->kaputt_zyl && kopf == s->kaputt_kopf && nr < s->kaputt_erfassungen) {
        /* Ein Datenbyte anders kodieren, die Pruefsumme des ALTEN Inhalts
         * stehen lassen: nur die 16 Zellen des geaenderten Bytes werden
         * aus der zweiten Kodierung uebernommen. */
        size_t n2 = kodiere(zellen2, zyl_im_id, kopf_im_id, kopf, zyl, s->kaputt_id, 1, -1);
        size_t f = 0, gesamt = (n < n2 ? n : n2) * 8;
        while (f < gesamt && zelle(zellen, f) == zelle(zellen2, f)) f++;
        for (size_t i = f; i < f + 16 && i < gesamt; i++) setze(zellen, i, zelle(zellen2, i));
    }

    if (zyl == s->idcrc_zyl && kopf == s->idcrc_kopf) {
        /* Die PRUEFSUMME des Sektorkopfs falsch, der Kopf selbst richtig:
         * der Spender traegt R ^ 0x80, also eine andere Kopfpruefsumme.
         * Uebernommen werden nur ihre 32 Zellen (CRC1 CRC2) und die eine
         * Taktzelle danach, R und N bleiben. Jedes Byte sind 16 Zellen
         * und der Encoder beginnt auf einer Bytegrenze (uft_mfm_encoder.c:
         * emit_byte), also liegt R bei (erste Abweichung / 16) * 16. */
        size_t n2 = kodiere(zellen2, zyl_im_id, kopf_im_id, kopf, zyl, -1, 0, s->idcrc_id);
        size_t f = 0, gesamt = (n < n2 ? n : n2) * 8;
        while (f < gesamt && zelle(zellen, f) == zelle(zellen2, f)) f++;
        size_t crc_anfang = (f / 16) * 16 + 32;            /* hinter R und N */
        for (size_t i = crc_anfang; i < crc_anfang + 33 && i < gesamt; i++)
            setze(zellen, i, zelle(zellen2, i));
    }

    size_t m = 0, nullen = 0;
    for (size_t i = 0; i < n * 8; i++) {
        if (zelle(zellen, i)) { s->abstaende[m++] = (uint32_t)(nullen + 1) * TICKS_JE_ZELLE; nullen = 0; }
        else nullen++;
    }
    s->strom_len = uft_gw_encode_flux_stream(s->abstaende, (uint32_t)m, s->strom,
                                             (size_t)ZELLEN_CAP * 8 * 2 + 64, TAKT_HZ);
    gw_fw_load_read_stream(&s->fw, s->strom, s->strom_len);
}

static int stand_auf(stand_t *s)
{
    memset(s, 0, sizeof *s);
    s->kaputt_zyl = -1;
    s->idcrc_zyl = -1;
    s->fremde_ortsmarke_zyl = -1;
    s->abstaende = (uint32_t *)malloc((size_t)ZELLEN_CAP * 8 * sizeof(uint32_t));
    s->strom     = (uint8_t *)malloc((size_t)ZELLEN_CAP * 8 * 2 + 64);
    if (!s->abstaende || !s->strom) return 0;

    gw_fw_reset(&s->fw);
    gw_fw_power_on_defaults(&s->fw);
    gw_fw_set_firmware_version(&s->fw, 1, 23);
    gw_fw_set_sample_freq(&s->fw, TAKT_HZ);
    gw_wire_init(&s->draht, &s->fw);
    s->draht.trk0_folgt_zylinder = true;
    s->draht.vor_lesen    = lade_strom;
    s->draht.vor_lesen_ud = s;
    return uft_gw_open_stream(&s->draht.ops, &s->dev) == UFT_GW_OK && s->dev;
}

static void stand_ab(stand_t *s)
{
    if (s->dev) uft_gw_close(s->dev);
    free(s->abstaende);
    free(s->strom);
    memset(s, 0, sizeof *s);
}

static uft_diag_config_t geometrie(int mit_liste, int versuche)
{
    uft_diag_config_t c;
    memset(&c, 0, sizeof c);
    c.tracks = 2; c.sides = 2; c.sectors = SPT; c.sector_size = SEKTOR;
    c.retries = versuche;
    if (mit_liste) {
        for (int i = 0; i < SPT; i++) c.sector_ids[i] = (uint8_t)(0xC1 + i);
        c.sector_id_count = SPT;
    }
    return c;
}

/* Ein Scan ueber den Adapter. Liefert die Zahl der Erfassungen. */
static unsigned scanne(stand_t *s, uft_diag_ctx_t *ctx, const uft_diag_config_t *c)
{
    uft_diag_config_t k = *c;
    uft_diag_gw_t a;
    if (uft_diag_init(ctx, &k) != 0) { pruefe("uft_diag_init", 0, NULL); return 0; }
    if (uft_diag_gw_init(&a, (struct uft_gw_device *)s->dev, FLUX_ENC_MFM,
                         FLUX_MFM_DD_BITCELL_NS, 1) != 0) {
        pruefe("uft_diag_gw_init", 0, NULL); return 0;
    }
    int rc = uft_diag_surface_scan(ctx, uft_diag_gw_read, &a);
    if (rc != 0) pruefe("Scan laeuft", 0, NULL);
    unsigned r = a.reads;
    uft_diag_gw_free(&a);
    return r;
}

static void keine_schreibbefehle(const stand_t *s)
{
    char h[96];
    snprintf(h, sizeof h, "%u Schreibbefehle, %u unbekannte Befehle",
             s->draht.schreibbefehle, s->draht.unbekannt);
    pruefe("kein WriteFlux/EraseFlux auf der Leitung, jeder Befehl bekannt",
           s->draht.schreibbefehle == 0 && s->draht.unbekannt == 0, h);
}

static void t_cpc_diskette_alles_good(void)
{
    puts("1. CPC-Diskette 2x2x9, Nummern 0xC1..0xC9 -- alles GOOD, eine Erfassung je Spur");
    stand_t s; uft_diag_ctx_t ctx;
    if (!stand_auf(&s)) { pruefe("Pruefstand", 0, NULL); stand_ab(&s); return; }
    uft_diag_config_t c = geometrie(1, 2);
    unsigned r = scanne(&s, &ctx, &c);
    char h[128];
    snprintf(h, sizeof h, "GOOD=%d WEAK=%d BAD=%d", ctx.good_sectors, ctx.weak_sectors, ctx.bad_sectors);
    pruefe("36 von 36 GOOD", ctx.good_sectors == 36 && ctx.bad_sectors == 0 && ctx.weak_sectors == 0, h);
    snprintf(h, sizeof h, "%u Erfassungen, %u ReadFlux", r, s.draht.lesebefehle);
    pruefe("4 Erfassungen, 4 ReadFlux auf der Leitung", r == 4 && s.draht.lesebefehle == 4, h);
    keine_schreibbefehle(&s);
    uft_diag_free(&ctx);
    stand_ab(&s);
}

static void t_ohne_liste_alles_bad(void)
{
    puts("2. Dieselbe Diskette nach 1..9 gefragt -- alles BAD, jede Wiederholung neu erfasst");
    stand_t s; uft_diag_ctx_t ctx;
    if (!stand_auf(&s)) { pruefe("Pruefstand", 0, NULL); stand_ab(&s); return; }
    uft_diag_config_t c = geometrie(0, 2);
    unsigned r = scanne(&s, &ctx, &c);
    char h[128];
    snprintf(h, sizeof h, "GOOD=%d BAD=%d", ctx.good_sectors, ctx.bad_sectors);
    pruefe("36 von 36 BAD", ctx.bad_sectors == 36 && ctx.good_sectors == 0, h);
    /* je Spur: Versuch 1 von Sektor 1 erfasst, jeder Versuch 2 erfasst neu
     * und bedient Versuch 1 des naechsten -- 1 + 9 = 10 je Spur. */
    snprintf(h, sizeof h, "%u Erfassungen", r);
    pruefe("40 Erfassungen (10 je Spur und Seite)", r == 40, h);
    keine_schreibbefehle(&s);
    uft_diag_free(&ctx);
    stand_ab(&s);
}

static void t_wiederholung_ist_neue_erfassung(void)
{
    puts("3. 0xC5 auf Zyl 1 Kopf 1 nur in der ERSTEN Erfassung kaputt -- WEAK, nicht BAD");
    stand_t s; uft_diag_ctx_t ctx;
    if (!stand_auf(&s)) { pruefe("Pruefstand", 0, NULL); stand_ab(&s); return; }
    s.kaputt_zyl = 1; s.kaputt_kopf = 1; s.kaputt_id = 0xC5; s.kaputt_erfassungen = 1;
    uft_diag_config_t c = geometrie(1, 2);
    unsigned r = scanne(&s, &ctx, &c);
    char h[128];
    snprintf(h, sizeof h, "GOOD=%d WEAK=%d BAD=%d", ctx.good_sectors, ctx.weak_sectors, ctx.bad_sectors);
    pruefe("35 GOOD, 1 WEAK, 0 BAD", ctx.good_sectors == 35 && ctx.weak_sectors == 1 && ctx.bad_sectors == 0, h);
    pruefe("WEAK steht auf Zyl 1 Kopf 1 an Stelle 4 (0xC5)",
           ctx.track_results[1 * 2 + 1].sector_status[4] == UFT_DIAG_SECTOR_WEAK, NULL);
    snprintf(h, sizeof h, "%u Erfassungen, Stelle (1,1) %d-mal", r, s.erfassungen[1][1]);
    pruefe("die Wiederholung war eine neue Erfassung (5 insgesamt, 2 auf (1,1))",
           r == 5 && s.erfassungen[1][1] == 2, h);
    keine_schreibbefehle(&s);
    uft_diag_free(&ctx);
    stand_ab(&s);
}

static void t_dauerhaft_kaputt(void)
{
    puts("4. 0xC5 auf Zyl 1 Kopf 1 in JEDER Erfassung kaputt -- BAD, mit Ort");
    stand_t s; uft_diag_ctx_t ctx;
    if (!stand_auf(&s)) { pruefe("Pruefstand", 0, NULL); stand_ab(&s); return; }
    s.kaputt_zyl = 1; s.kaputt_kopf = 1; s.kaputt_id = 0xC5; s.kaputt_erfassungen = 1000;
    uft_diag_config_t c = geometrie(1, 3);
    unsigned r = scanne(&s, &ctx, &c);
    char h[128];
    snprintf(h, sizeof h, "GOOD=%d WEAK=%d BAD=%d", ctx.good_sectors, ctx.weak_sectors, ctx.bad_sectors);
    pruefe("35 GOOD, 1 BAD -- ein Pruefsummenfehler gilt nicht als gelesen",
           ctx.good_sectors == 35 && ctx.bad_sectors == 1 && ctx.weak_sectors == 0, h);
    uft_bad_sector_t l[4];
    size_t n = 4;
    uft_diag_get_bad_sectors(&ctx, l, &n);
    snprintf(h, sizeof h, "n=%zu, erster: Zyl %d Kopf %d Sektor 0x%02X", n,
             n ? l[0].track : -1, n ? l[0].side : -1, n ? (unsigned)l[0].sector : 0u);
    pruefe("Fehlerliste: Zyl 1, Kopf 1, Sektor 0xC5",
           n == 1 && l[0].track == 1 && l[0].side == 1 && l[0].sector == 0xC5, h);
    snprintf(h, sizeof h, "%u Erfassungen, Stelle (1,1) %d-mal", r, s.erfassungen[1][1]);
    pruefe("3 Versuche = 3 Erfassungen auf (1,1), 6 insgesamt",
           r == 6 && s.erfassungen[1][1] == 3, h);
    keine_schreibbefehle(&s);
    uft_diag_free(&ctx);
    stand_ab(&s);
}

static void t_kopfpruefsumme_falsch(void)
{
    /* Der Dekoder gibt einen Sektor mit falscher Kopfpruefsumme zurueck
     * (decode_mfm_sector setzt id_crc_ok = false und meldet FLUX_OK); der
     * Adapter darf ihn nicht als gelesen zaehlen. Ohne diesen Fall
     * entkam die Mutation `!s->id_crc_ok ||` gestrichen. */
    puts("4b. Kopfpruefsumme von 0xC5 auf Zyl 1 Kopf 1 in JEDER Erfassung falsch -- BAD");
    stand_t s; uft_diag_ctx_t ctx;
    if (!stand_auf(&s)) { pruefe("Pruefstand", 0, NULL); stand_ab(&s); return; }
    s.idcrc_zyl = 1; s.idcrc_kopf = 1; s.idcrc_id = 0xC5;
    uft_diag_config_t c = geometrie(1, 2);
    unsigned r = scanne(&s, &ctx, &c);
    char h[160];
    snprintf(h, sizeof h, "GOOD=%d WEAK=%d BAD=%d", ctx.good_sectors, ctx.weak_sectors, ctx.bad_sectors);
    pruefe("35 GOOD, 1 BAD -- ein Sektorkopf mit falscher Pruefsumme gilt nicht als gelesen",
           ctx.good_sectors == 35 && ctx.bad_sectors == 1 && ctx.weak_sectors == 0, h);
    pruefe("BAD steht auf Zyl 1 Kopf 1 an Stelle 4 (0xC5)",
           ctx.track_results[1 * 2 + 1].sector_status[4] == UFT_DIAG_SECTOR_BAD, NULL);
    snprintf(h, sizeof h, "%u Erfassungen, Stelle (1,1) %d-mal", r, s.erfassungen[1][1]);
    pruefe("2 Versuche = 2 Erfassungen auf (1,1), 5 insgesamt",
           r == 5 && s.erfassungen[1][1] == 2, h);
    uft_diag_free(&ctx);

    /* BAD aus dem RICHTIGEN Grund: der Sektor ist da, mit richtigem C/H/R,
     * richtiger Groesse und guter DATENpruefsumme -- nur der Kopf ist
     * falsch. Waere die Spleissung danebengegangen, fehlte der Sektor, und
     * BAD oben waere ein "nicht gefunden". */
    uft_diag_gw_t a;
    uint8_t puffer[SEKTOR];
    if (uft_diag_gw_init(&a, (struct uft_gw_device *)s.dev, FLUX_ENC_MFM,
                         FLUX_MFM_DD_BITCELL_NS, 1) != 0) {
        pruefe("uft_diag_gw_init", 0, NULL); stand_ab(&s); return;
    }
    int rc = uft_diag_gw_read(1, 1, 0xC5, puffer, SEKTOR, &a);
    int gefunden = 0, id_ok = -1, daten_ok = -1;
    size_t groesse = 0;
    for (size_t i = 0; i < a.track_cache->sector_count && i < FLUX_MAX_SECTORS; i++) {
        const flux_decoded_sector_t *d = &a.track_cache->sectors[i];
        if (d->sector != 0xC5 || d->cylinder != 1 || d->head != 1) continue;
        gefunden++;
        id_ok = d->id_crc_ok; daten_ok = d->data_crc_ok; groesse = d->data_size;
    }
    snprintf(h, sizeof h, "rc=%d, gefunden %d, id_crc_ok %d, data_crc_ok %d, %zu Byte",
             rc, gefunden, id_ok, daten_ok, groesse);
    pruefe("der Dekoder liefert 0xC5 (C1 H1) mit falscher Kopf-, guter Datenpruefsumme, "
           "512 Byte -- und der Adapter sagt -1",
           rc == -1 && gefunden == 1 && id_ok == 0 && daten_ok == 1 && groesse == SEKTOR, h);
    uft_diag_gw_free(&a);
    keine_schreibbefehle(&s);
    stand_ab(&s);
}

static void t_fremde_ortsmarke(void)
{
    puts("5. Zylinder 1 traegt im Sektorkopf C = 5 -- nicht dieser Sektor, BAD");
    stand_t s; uft_diag_ctx_t ctx;
    if (!stand_auf(&s)) { pruefe("Pruefstand", 0, NULL); stand_ab(&s); return; }
    s.fremde_ortsmarke_zyl = 1;
    uft_diag_config_t c = geometrie(1, 1);
    scanne(&s, &ctx, &c);
    char h[128];
    snprintf(h, sizeof h, "GOOD=%d BAD=%d", ctx.good_sectors, ctx.bad_sectors);
    pruefe("18 GOOD auf Zylinder 0, 18 BAD auf Zylinder 1",
           ctx.good_sectors == 18 && ctx.bad_sectors == 18 &&
           ctx.track_results[1 * 2 + 0].bad_sectors == 9 &&
           ctx.track_results[1 * 2 + 1].bad_sectors == 9, h);
    uft_diag_free(&ctx);
    stand_ab(&s);
}

static void t_fremder_kopf(void)
{
    puts("5b. Seite 1 traegt im Sektorkopf H = 0 -- nicht dieser Sektor, BAD");
    stand_t s; uft_diag_ctx_t ctx;
    if (!stand_auf(&s)) { pruefe("Pruefstand", 0, NULL); stand_ab(&s); return; }
    s.kopf1_traegt_h0 = true;
    uft_diag_config_t c = geometrie(1, 1);
    scanne(&s, &ctx, &c);
    char h[128];
    snprintf(h, sizeof h, "GOOD=%d BAD=%d", ctx.good_sectors, ctx.bad_sectors);
    pruefe("18 GOOD auf Seite 0, 18 BAD auf Seite 1",
           ctx.good_sectors == 18 && ctx.bad_sectors == 18 &&
           ctx.track_results[0 * 2 + 1].bad_sectors == 9 &&
           ctx.track_results[1 * 2 + 1].bad_sectors == 9, h);
    uft_diag_free(&ctx);
    stand_ab(&s);
}

static void t_falsche_sektorgroesse(void)
{
    puts("6. Scan mit 256 Byte gegen 512-Byte-Sektoren -- BAD statt abgeschnitten");
    stand_t s; uft_diag_ctx_t ctx;
    if (!stand_auf(&s)) { pruefe("Pruefstand", 0, NULL); stand_ab(&s); return; }
    uft_diag_config_t c = geometrie(1, 1);
    c.sector_size = 256;
    scanne(&s, &ctx, &c);
    char h[96];
    snprintf(h, sizeof h, "GOOD=%d BAD=%d", ctx.good_sectors, ctx.bad_sectors);
    pruefe("36 von 36 BAD", ctx.bad_sectors == 36 && ctx.good_sectors == 0, h);
    uft_diag_free(&ctx);
    stand_ab(&s);
}

static void t_keine_diskette(void)
{
    puts("7. Keine Diskette im Laufwerk (NO_INDEX) -- BAD, Fehler festgehalten");
    stand_t s; uft_diag_ctx_t ctx;
    if (!stand_auf(&s)) { pruefe("Pruefstand", 0, NULL); stand_ab(&s); return; }
    gw_fw_set_disk_present(&s.fw, false);
    uft_diag_config_t c = geometrie(1, 1);
    uft_diag_config_t k = c;
    uft_diag_gw_t a;
    if (uft_diag_init(&ctx, &k) != 0 ||
        uft_diag_gw_init(&a, (struct uft_gw_device *)s.dev, FLUX_ENC_MFM,
                         FLUX_MFM_DD_BITCELL_NS, 1) != 0) {
        pruefe("init", 0, NULL); stand_ab(&s); return;
    }
    uft_diag_surface_scan(&ctx, uft_diag_gw_read, &a);
    char h[96];
    snprintf(h, sizeof h, "GOOD=%d BAD=%d, letzter GW-Fehler %d",
             ctx.good_sectors, ctx.bad_sectors, a.last_gw_error);
    pruefe("36 von 36 BAD und ein GW-Fehler vermerkt",
           ctx.bad_sectors == 36 && ctx.good_sectors == 0 && a.last_gw_error != 0, h);
    uft_diag_gw_free(&a);
    uft_diag_free(&ctx);
    stand_ab(&s);
}

static void t_init_absagen(void)
{
    /* Mit einem ECHTEN Emulator-Geraet: sonst saegte eine fehlschlagende
     * GET_INFO-Anfrage ab, und die Pruefung der Kodierung waere gruen aus
     * dem falschen Grund (so von der Mutationsmatrix gefunden). */
    puts("8. Der Adapter nimmt keine Kodierung AUTO und keine 0 Umdrehungen");
    stand_t s;
    if (!stand_auf(&s)) { pruefe("Pruefstand", 0, NULL); stand_ab(&s); return; }
    struct uft_gw_device *dev = (struct uft_gw_device *)s.dev;
    uft_diag_gw_t a;
    pruefe("Gegenprobe: MFM mit 1 Umdrehung wird angenommen",
           uft_diag_gw_init(&a, dev, FLUX_ENC_MFM, 0, 1) == 0 && a.sample_freq == TAKT_HZ,
           NULL);
    uft_diag_gw_free(&a);
    pruefe("FLUX_ENC_AUTO: abgesagt",
           uft_diag_gw_init(&a, dev, FLUX_ENC_AUTO, 0, 1) == -1, NULL);
    uft_diag_gw_free(&a);
    pruefe("FLUX_ENC_GCR_C64: abgesagt (nur MFM und FM)",
           uft_diag_gw_init(&a, dev, FLUX_ENC_GCR_C64, 0, 1) == -1, NULL);
    uft_diag_gw_free(&a);
    pruefe("0 Umdrehungen: abgesagt",
           uft_diag_gw_init(&a, dev, FLUX_ENC_MFM, 0, 0) == -1, NULL);
    uft_diag_gw_free(&a);
    pruefe("kein Geraet: abgesagt",
           uft_diag_gw_init(&a, NULL, FLUX_ENC_MFM, 0, 1) == -1, NULL);
    uft_diag_gw_free(&a);
    stand_ab(&s);
}

static void t_schreibzaehler_zaehlt(void)
{
    /* Gegenprobe zu „kein Schreibbefehl": ohne sie waere die Zusage
     * `schreibbefehle == 0` auch dann gruen, wenn die Bruecke gar nicht
     * zaehlte. Der Automat weist den Befehl ab (ACK_WRPROT) -- es wird
     * nichts geschrieben, nur gezaehlt. Der Adapter ist hier NICHT
     * beteiligt; der Befehl kommt aus dem Test selbst. */
    puts("9. Gegenprobe: ein EraseFlux auf der Leitung wird gezaehlt");
    stand_t s;
    if (!stand_auf(&s)) { pruefe("Pruefstand", 0, NULL); stand_ab(&s); return; }
    int rc = uft_gw_erase_track(s.dev, 1);
    char h[96];
    snprintf(h, sizeof h, "rc=%d, %u Schreibbefehle", rc, s.draht.schreibbefehle);
    pruefe("gezaehlt: 1, und der Automat hat abgewiesen", s.draht.schreibbefehle == 1 && rc != 0, h);
    stand_ab(&s);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    puts("=== Oberflaechen-Scan ueber Greaseweazle, gegen den Automaten ===");
    t_cpc_diskette_alles_good();
    t_ohne_liste_alles_bad();
    t_wiederholung_ist_neue_erfassung();
    t_dauerhaft_kaputt();
    t_kopfpruefsumme_falsch();
    t_fremde_ortsmarke();
    t_fremder_kopf();
    t_falsche_sektorgroesse();
    t_keine_diskette();
    t_init_absagen();
    t_schreibzaehler_zaehlt();
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
