/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_flux_fm_sektoren.c
 * @brief Der FM-Weg liest Sektoren — oder er sagt es (MF-864)
 *
 * ── Der Befund, der diesen Test veranlasst hat ───────────────────────
 *
 * `flux_decode_fm()` in `src/flux/uft_flux_decoder.c` ist erreichbar:
 * der Verteiler `flux_decode_track()` routet FM dorthin (Zeile 2153).
 * Der Sektorteil bestand aus zwei Kommentaren:
 *
 *     int sync_pos = flux_find_sync(bits, bit_count, FM_SYNC_PATTERN, pos);
 *     if (sync_pos < 0) break;
 *     // FM sector decoding would go here - similar to MFM
 *     // For now, just note we found a sync
 *     pos = sync_pos + 16;
 *
 * Gemessen ueber den ganzen Funktionsrumpf: **keine** Zeile schrieb
 * `track->sectors[...]` oder erhoehte `sector_count`. Im MFM-Zweig
 * daneben gibt es beide. Jede FM-Diskette ueber den Flusspfad — Atari
 * 810/1050, TRS-80 SD, IBM 3740 — lieferte damit null Sektoren, waehrend
 * `CLAUDE.md` „Flux→Sektor" als Faehigkeit fuehrt.
 *
 * ── Woher die Spur kommt, und warum das die halbe Arbeit war ─────────
 *
 * Der Baum hat KEINEN FM- und keinen MFM-Encoder; das ist an vier
 * Stellen als Blocker vermerkt (u.a. `src/formats/kfx/uft_kfx.c:199`).
 * Ein Fixture, das ich baue, gelesen von einem Dekoder, den ich baue,
 * waere EINE Hand zweimal — die fuenfte Frage aus MF-644/760.
 *
 * Deshalb ist die Spur (`tests/fixtures/fm_ibm3740_track.h`,
 * erzeugt von `scripts/gen_fm_fixture.py`) an vier Stellen von einer
 * unabhaengigen Umsetzung abgenommen: `fluxtoimd` (Eric Smith 2016,
 * GPL-3-only), geklont unter `tools/uft-scout/work/fluxtoimd`. Die
 * Adressmarken gegen ihre vorberechneten Klassenwerte, ID- und
 * Datenfeld zurueck durch ihr `FM.decode()`, beide Pruefsummen mit
 * ihrer CRC-Klasse und ihren FM-Parametern.
 *
 * Uebernommen ist daraus **nichts**; sie wurde ausgefuehrt (Kanal
 * „Oracle", `docs/ORACLES.md`). Das Spurlayout stammt aus den Normen,
 * die die einfache Dichte festlegen: **ECMA 54 / ISO 5654 / ANSI
 * X3.73**.
 *
 * Erzeugt hat die Pruefsummen ihre CRC-Klasse — nachrechnen muss sie
 * **UFTs eigene** `flux_crc16_ccitt()`. Genau darin liegt der
 * Kreuzcheck: zwei Umsetzungen desselben Polynoms, und nur wenn beide
 * dasselbe sagen, sind `id_crc_ok` und `data_crc_ok` etwas wert.
 *
 * ── Was dieser Test NICHT belegt ─────────────────────────────────────
 *
 * Er laeuft gegen eine **erzeugte** Spur, nicht gegen eine Aufnahme von
 * einer echten Diskette. Im Korpus liegt kein FM-Flussabzug. Der Test
 * belegt: der Dekoder liest eine normgerechte FM-Spur. Er belegt
 * NICHT, dass er mit den Abweichungen echter Medien zurechtkommt —
 * dafuer braucht es eine Aufnahme, und die ist offen.
 */
#include "uft/flux/uft_flux_decoder.h"
#include "fixtures/fm_ibm3740_track.h"

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

/* 24 MHz, FM-Zelle 4 us == 96 Ticks. Dieselbe Wahl wie
 * tests/test_flux_index_wiring.c, damit beide dieselbe Zeitbasis haben. */
#define RATE_HZ     24000000u
#define ZELLE_TICKS 96u

#define KANALBITS   (FM_FIXTURE_BYTES * 16u)

/**
 * Verschraenkt das Fixture zu Kanalbits: je Bytepaar acht Mal
 * (Clockbit, Datenbit), hoechstwertiges Bit zuerst.
 *
 * Diese sechs Zeilen sind die EINZIGE Kodierlogik auf UFT-Seite, und
 * sie ist im Erzeuger gegen die Klassenwerte des Oracles abgenommen.
 */
static void kanalbits_bauen(uint8_t *bits)
{
    memset(bits, 0, (KANALBITS + 7u) / 8u);
    size_t k = 0;
    for (size_t i = 0; i < FM_FIXTURE_BYTES; i++) {
        const uint8_t daten = FM_FIXTURE[i][0];
        const uint8_t clock = FM_FIXTURE[i][1];
        for (int b = 7; b >= 0; b--) {
            if ((clock >> b) & 1) bits[k / 8] |= (uint8_t)(1u << (7 - (k % 8)));
            k++;
            if ((daten >> b) & 1) bits[k / 8] |= (uint8_t)(1u << (7 - (k % 8)));
            k++;
        }
    }
}

/** Jede Eins im Kanalstrom ist ein Flusswechsel in ihrer Zelle. */
static size_t fluss_bauen(const uint8_t *bits, uint32_t *tr, size_t max)
{
    size_t n = 0;
    for (size_t k = 0; k < KANALBITS && n < max; k++) {
        if ((bits[k / 8] >> (7 - (k % 8))) & 1)
            tr[n++] = (uint32_t)((k + 1) * ZELLE_TICKS);
    }
    return n;
}

static uint8_t *g_bits;
static uint32_t *g_tr;
static flux_raw_data_t g_flux;

static int fixture_laden(void)
{
    g_bits = (uint8_t *)calloc((KANALBITS + 7u) / 8u, 1);
    g_tr   = (uint32_t *)calloc(KANALBITS, sizeof(uint32_t));
    if (!g_bits || !g_tr) return 0;
    kanalbits_bauen(g_bits);
    size_t n = fluss_bauen(g_bits, g_tr, KANALBITS);
    memset(&g_flux, 0, sizeof g_flux);
    g_flux.transitions      = g_tr;
    g_flux.transition_count = (uint32_t)n;
    g_flux.sample_rate      = RATE_HZ;
    return n > 0;
}

static void fixture_frei(void)
{
    free(g_bits); g_bits = NULL;
    free(g_tr);   g_tr = NULL;
}

/** Erwartete Nutzdaten: Sektor s ist mit ((s*7 + i) & 0xFF) gefuellt. */
static int nutzdaten_stimmen(const flux_decoded_sector_t *sek)
{
    for (size_t i = 0; i < FM_FIXTURE_SEKTORGROESSE; i++) {
        uint8_t soll = (uint8_t)((sek->sector * 7u + i) & 0xFFu);
        if (sek->data[i] != soll) {
            printf("\n      Sektor %u Byte %zu: %02X statt %02X\n      ",
                   sek->sector, i, sek->data[i], soll);
            return 0;
        }
    }
    return 1;
}

static void dekodieren(flux_decoded_track_t *track)
{
    flux_decoder_options_t opts;
    flux_decoder_options_init(&opts);
    opts.bitcell_ns = FLUX_FM_BITCELL_NS;
    opts.use_pll    = false;      /* die Spur ist exakt getaktet */
    memset(track, 0, sizeof(*track));
    flux_decode_fm(&g_flux, track, &opts);
}

TEST(der_fm_weg_findet_alle_sektoren)
{
    /* DER ROTBEWEIS. Vor MF-864 stand hier
     * „FM sector decoding would go here" und `sector_count` blieb 0. */
    flux_decoded_track_t track;
    dekodieren(&track);

    if (track.sector_count != FM_FIXTURE_SEKTOREN) {
        printf("\n      %u Sektoren statt %u — der FM-Weg legt keine an\n"
               "      ", (unsigned)track.sector_count,
               (unsigned)FM_FIXTURE_SEKTOREN);
        _fail++;
    }
    flux_decoded_track_free(&track);
}

TEST(jeder_sektor_traegt_seinen_kopf)
{
    flux_decoded_track_t track;
    dekodieren(&track);
    ASSERT(track.sector_count == FM_FIXTURE_SEKTOREN);

    for (size_t i = 0; i < track.sector_count; i++) {
        const flux_decoded_sector_t *s = &track.sectors[i];
        if (s->cylinder != 0 || s->head != 0 || s->size_code != 0 ||
            s->sector != (uint8_t)(i + 1)) {
            printf("\n      Sektor %zu: C=%u H=%u S=%u N=%u erwartet "
                   "0/0/%zu/0\n      ", i, s->cylinder, s->head,
                   s->sector, s->size_code, i + 1);
            _fail++;
            break;
        }
    }
    flux_decoded_track_free(&track);
}

TEST(beide_pruefsummen_werden_bestaetigt)
{
    /* Der Kreuzcheck: erzeugt hat die CRCs die Oracle-Klasse, nachrechnen
     * muss sie UFTs eigene `flux_crc16_ccitt()`. Waere UFTs Rechnung eine
     * andere — etwa die MFM-Variante mit drei vorangestellten A1 —, wuerde
     * das hier auffallen. */
    flux_decoded_track_t track;
    dekodieren(&track);
    ASSERT(track.sector_count == FM_FIXTURE_SEKTOREN);

    for (size_t i = 0; i < track.sector_count; i++) {
        const flux_decoded_sector_t *s = &track.sectors[i];
        if (!s->id_crc_ok || !s->data_crc_ok) {
            printf("\n      Sektor %u: ID-CRC %s, Daten-CRC %s\n      ",
                   s->sector, s->id_crc_ok ? "ok" : "FALSCH",
                   s->data_crc_ok ? "ok" : "FALSCH");
            _fail++;
            break;
        }
    }
    flux_decoded_track_free(&track);
}

TEST(die_nutzdaten_kommen_byteidentisch_zurueck)
{
    flux_decoded_track_t track;
    dekodieren(&track);
    ASSERT(track.sector_count == FM_FIXTURE_SEKTOREN);

    for (size_t i = 0; i < track.sector_count; i++) {
        const flux_decoded_sector_t *s = &track.sectors[i];
        if (s->data_size != FM_FIXTURE_SEKTORGROESSE || !s->data) {
            printf("\n      Sektor %u: %zu Byte statt %u\n      ",
                   s->sector, s->data_size,
                   (unsigned)FM_FIXTURE_SEKTORGROESSE);
            _fail++;
            break;
        }
        if (!nutzdaten_stimmen(s)) { _fail++; break; }
    }
    flux_decoded_track_free(&track);
}

TEST(der_geloeschte_sektor_wird_als_solcher_gemeldet)
{
    /* Sektor 3 traegt die Deleted Data Address Mark (0xF8, Clock 0xC7 —
     * Kanalwort 0xF56A). Ein Dekoder, der sie als gewoehnliche Daten
     * durchreicht, verschweigt eine forensisch bedeutsame Aussage. */
    flux_decoded_track_t track;
    dekodieren(&track);
    ASSERT(track.sector_count == FM_FIXTURE_SEKTOREN);

    int gefunden = 0;
    for (size_t i = 0; i < track.sector_count; i++) {
        const flux_decoded_sector_t *s = &track.sectors[i];
        int soll = (s->sector == FM_FIXTURE_GELOESCHT);
        if (s->deleted != soll) {
            printf("\n      Sektor %u: deleted=%d, erwartet %d\n      ",
                   s->sector, (int)s->deleted, soll);
            _fail++;
            break;
        }
        if (soll) gefunden = 1;
    }
    if (!gefunden) {
        printf("\n      Sektor %u kam gar nicht vor\n      ",
               (unsigned)FM_FIXTURE_GELOESCHT);
        _fail++;
    }
    flux_decoded_track_free(&track);
}

TEST(kein_sektor_kommt_doppelt)
{
    /* Gegenprobe zu MF-218: dort las der MFM-Dekoder Sektoren mehrfach,
     * weil er den Suchlauf innerhalb des eigenen Sektors fortsetzte. Der
     * FM-Weg darf denselben Fehler nicht erben. */
    flux_decoded_track_t track;
    dekodieren(&track);
    ASSERT(track.sector_count == FM_FIXTURE_SEKTOREN);

    int gesehen[256];
    memset(gesehen, 0, sizeof gesehen);
    for (size_t i = 0; i < track.sector_count; i++) {
        uint8_t s = track.sectors[i].sector;
        if (gesehen[s]++) {
            printf("\n      Sektor %u kommt mehrfach vor\n      ", s);
            _fail++;
            break;
        }
    }
    flux_decoded_track_free(&track);
}

TEST(eine_scheinmarke_in_beschaedigten_daten_erzeugt_keinen_sektor)
{
    /* Die MF-218-Falle, fuer FM nachgestellt.
     *
     * Setzt der Dekoder die Suche bei `sync_pos + 16` fort statt hinter
     * dem Sektor, findet er jede Bitfolge im DATENFELD, die wie eine
     * Adressmarke aussieht — und legt einen Sektor an, den es nicht gibt.
     *
     * GEMESSEN (`scratchpad/suche_falschsync.py`, ueber alle 1- bis
     * 3-Byte-Folgen): in WOHLGEFORMTEN Nutzdaten kann 0xF57E an keiner
     * Bitposition stehen. Das Muster verlangt die Clockbits
     * 1,1,0,0,0,1,1,1; ein Nutzbyte traegt ueberall 1.
     *
     * Auf einer BESCHAEDIGTEN Spur gilt das nicht: ein Ausfall oder ein
     * schwaches Bit erzeugt beliebige Kanalbits. Genau dafuer gibt es
     * dieses Werkzeug — also wird der Fall hier hergestellt, statt sich
     * auf die Wohlgeformtheit zu verlassen.
     *
     * Ohne diesen Fall war die Absicherung ungeprueft: eine Mutation, die
     * `end_pos` durch `sync_pos + 16` ersetzt, blieb gruen. */
    ASSERT(g_bits != NULL);

    /* Erstes Datenfeld finden: die Data Address Mark (0xFB, Clock 0xC7). */
    size_t dam_byte = 0;
    for (size_t i = 0; i < FM_FIXTURE_BYTES; i++) {
        if (FM_FIXTURE[i][0] == 0xFB && FM_FIXTURE[i][1] == 0xC7) {
            dam_byte = i;
            break;
        }
    }
    ASSERT(dam_byte != 0);

    /* ZWEI Stoerstellen im Nutzinhalt, 20 Byte auseinander: eine, die wie
     * eine ID-Marke aussieht, und eine, die wie eine Datenmarke aussieht.
     *
     * Eine allein genuegt nicht — dann greift das Datenfeldfenster von 43
     * Byte als zweiter Schutz, und die Probe belegt nichts ueber die
     * Fortsetzungsposition. Erst das Paar stellt den Fall her, den
     * `end_pos` verhindern soll, und ein beschaedigter Bereich erzeugt
     * typischerweise mehr als eine Stoerung. */
    const size_t id_bit  = (dam_byte + 20u) * 16u;
    const size_t dat_bit = (dam_byte + 40u) * 16u;

    uint8_t sicherung[8];
    for (int i = 0; i < 4; i++) {
        sicherung[i]     = g_bits[id_bit  / 8 + i];
        sicherung[4 + i] = g_bits[dat_bit / 8 + i];
    }

    for (int b = 0; b < 16; b++) {
        size_t k = id_bit + (size_t)b;
        if ((FM_SYNC_PATTERN >> (15 - b)) & 1)
            g_bits[k / 8] |=  (uint8_t)(1u << (7 - (k % 8)));
        else
            g_bits[k / 8] &= (uint8_t)~(1u << (7 - (k % 8)));

        k = dat_bit + (size_t)b;
        if ((FM_DAM_PATTERN >> (15 - b)) & 1)
            g_bits[k / 8] |=  (uint8_t)(1u << (7 - (k % 8)));
        else
            g_bits[k / 8] &= (uint8_t)~(1u << (7 - (k % 8)));
    }
    g_flux.transition_count = (uint32_t)fluss_bauen(g_bits, g_tr, KANALBITS);

    flux_decoded_track_t track;
    dekodieren(&track);

    /* Vier echte Sektoren, kein fuenfter. Der beschaedigte traegt eine
     * falsche Pruefsumme — das ist der richtige Befund. */
    if (track.sector_count != FM_FIXTURE_SEKTOREN) {
        printf("\n      %u Sektoren statt %u — eine Scheinmarke im "
               "Datenfeld wurde als Sektor gelesen\n      ",
               (unsigned)track.sector_count, (unsigned)FM_FIXTURE_SEKTOREN);
        _fail++;
    } else {
        int ein_crc_fehler = 0;
        for (size_t i = 0; i < track.sector_count; i++)
            if (!track.sectors[i].data_crc_ok) ein_crc_fehler = 1;
        if (!ein_crc_fehler) {
            printf("\n      der beschaedigte Sektor meldet eine gueltige "
                   "Pruefsumme\n      ");
            _fail++;
        }
    }
    flux_decoded_track_free(&track);

    for (int i = 0; i < 4; i++) {
        g_bits[id_bit  / 8 + i] = sicherung[i];
        g_bits[dat_bit / 8 + i] = sicherung[4 + i];
    }
    g_flux.transition_count = (uint32_t)fluss_bauen(g_bits, g_tr, KANALBITS);
}

TEST(ein_verfaelschtes_datenbyte_faellt_auf)
{
    /* Der wichtigste Gegenbeweis: ein Dekoder, der die Pruefsumme nicht
     * wirklich rechnet, meldet auch hier `data_crc_ok`. Ein Datenbit wird
     * gekippt — der Sektor muss weiterhin GELESEN, seine Pruefsumme aber
     * als falsch gemeldet werden. Nicht verschwiegen, nicht verworfen. */
    ASSERT(g_bits != NULL);

    /* Ein Datenbit im ersten Sektor: das Fixture beginnt mit 40 Byte
     * GAP1, danach folgen Sync/IAM/GAP; das erste Datenfeld liegt sicher
     * hinter Byte 100. Wir kippen das Datenbit von Spurbyte 150. */
    const size_t byte_index = 150;
    const size_t bit_index  = byte_index * 16u + 1u;   /* +1 = Datenbit */
    g_bits[bit_index / 8] ^= (uint8_t)(1u << (7 - (bit_index % 8)));

    size_t n = fluss_bauen(g_bits, g_tr, KANALBITS);
    g_flux.transition_count = (uint32_t)n;

    flux_decoded_track_t track;
    dekodieren(&track);

    int irgendwo_falsch = 0;
    for (size_t i = 0; i < track.sector_count; i++)
        if (!track.sectors[i].data_crc_ok || !track.sectors[i].id_crc_ok)
            irgendwo_falsch = 1;

    if (!irgendwo_falsch) {
        printf("\n      ein gekipptes Bit blieb folgenlos — die "
               "Pruefsumme wird nicht gerechnet\n      ");
        _fail++;
    }
    flux_decoded_track_free(&track);

    /* Fixture wiederherstellen, damit die Reihenfolge der Tests egal ist. */
    g_bits[bit_index / 8] ^= (uint8_t)(1u << (7 - (bit_index % 8)));
    g_flux.transition_count = (uint32_t)fluss_bauen(g_bits, g_tr, KANALBITS);
}

TEST(auch_der_verteiler_mit_eingeschalteter_regelung_liest_sie)
{
    /* Die Faelle oben laufen mit `use_pll = false`, weil die Pruefspur
     * exakt getaktet ist. Der WIRKLICH erreichbare Weg ist ein anderer:
     * `flux_decode_track()` bekommt die Vorgabe aus
     * `flux_decoder_options_init()`, und dort steht `use_pll = true`.
     *
     * Ein Dekoder, der nur ohne Regelung liest, waere in der Anwendung
     * nutzlos — deshalb dieser Fall, und deshalb ueber den VERTEILER,
     * nicht ueber `flux_decode_fm()` direkt. */
    flux_decoder_options_t opts;
    flux_decoder_options_init(&opts);
    opts.encoding   = FLUX_ENC_FM;
    opts.bitcell_ns = FLUX_FM_BITCELL_NS;
    ASSERT(opts.use_pll == true);

    flux_decoded_track_t track;
    memset(&track, 0, sizeof track);
    flux_decode_track(&g_flux, &track, &opts);

    if (track.sector_count != FM_FIXTURE_SEKTOREN) {
        printf("\n      Verteiler: %u Sektoren statt %u\n      ",
               (unsigned)track.sector_count, (unsigned)FM_FIXTURE_SEKTOREN);
        _fail++;
    } else {
        for (size_t i = 0; i < track.sector_count; i++) {
            if (!track.sectors[i].data_crc_ok) {
                printf("\n      Verteiler: Sektor %u mit falscher "
                       "Pruefsumme\n      ", track.sectors[i].sector);
                _fail++;
                break;
            }
        }
    }
    flux_decoded_track_free(&track);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== FM: Sektoren aus dem Flusspfad (MF-864) ===\n");
    if (!fixture_laden()) {
        printf("ABBRUCH: Fixture liess sich nicht aufbauen\n");
        return 1;
    }
    RUN(der_fm_weg_findet_alle_sektoren);
    RUN(jeder_sektor_traegt_seinen_kopf);
    RUN(beide_pruefsummen_werden_bestaetigt);
    RUN(die_nutzdaten_kommen_byteidentisch_zurueck);
    RUN(der_geloeschte_sektor_wird_als_solcher_gemeldet);
    RUN(kein_sektor_kommt_doppelt);
    RUN(eine_scheinmarke_in_beschaedigten_daten_erzeugt_keinen_sektor);
    RUN(ein_verfaelschtes_datenbyte_faellt_auf);
    RUN(auch_der_verteiler_mit_eingeschalteter_regelung_liest_sie);
    fixture_frei();
    printf("\nErgebnis: %d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
