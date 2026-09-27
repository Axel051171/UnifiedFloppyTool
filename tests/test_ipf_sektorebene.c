/**
 * @file test_ipf_sektorebene.c
 * @brief IPF liefert Sektoren — an fremd erzeugten Abbildern (MF-1372,
 *        P3-360 Teil 1)
 *
 * ── Der Befund ──────────────────────────────────────────────────────────
 *
 * Bis MF-1372 lieferte jede IPF Zellen und **null** Sektoren (P3-360).
 * Und fuer die zwei Pruefdateien dieses Tests lieferte sie nicht einmal
 * Zellen: gemessen vor der Behebung an den vollen 80-Zylinder-Fassungen
 * 0 von 160 Spuren mit Zellstrom, bei der Amiga-Datei „die Zellzahl trifft
 * die angesagten 100150 Bit nicht“, bei der PC-Datei „19 Bloecke, der
 * Leser haelt 16“. Drei Ursachen:
 *   - Elemente wurden nur beim SPS-Kodierer zerlegt; MF-1079 hielt fest,
 *     beim CAPS-Kodierer gebe es keine. Beide Dateien sind CAPS-kodiert
 *     und tragen Elementstroeme (Zaehlung in Byte).
 *   - hoechstens 16 Bloecke je Spur (P3-361); eine PC-DD-Spur hat 19.
 *   - der Zwischenraum war fest MFM(0x00) vorwaerts; beim CAPS-Kodierer
 *     nennt die Datei ihr Fuellbyte (PC: 0x4E), und gefuellt wird vorwaerts
 *     UND rueckwaerts mit der Naht in der Mitte.
 *
 * ── Die fremde Hand ─────────────────────────────────────────────────────
 *
 * Beide Dateien schreibt `disk-analyse` aus Keir Frasers disk-utilities
 * (Public Domain / Unlicense, Quellstand 5e690f3a), dessen IPF-Schreiber
 * (libdisk/container/ipf.c, 2011) von AIR — der Vorlage von
 * `uft_ipf_air.c` — unabhaengig ist. Die EINGABE ist UFT-eigen und
 * benennt sich selbst: jeder Sektor traegt `UFT-K Ccc Hh Sss ` alle
 * 17 Byte. Ein gelesener Sektor sagt damit, WO er hingehoert.
 * Rezept: tests/corpus_manifest/gen_ipf_corpus.py.
 *
 * Referenzen fuer die Zwischenraum-Regel, beide gelesen, nichts
 * uebernommen: Keir Fraser, ipfinfo/ipf.txt („Gap stream / 1. No gap
 * streams“), und MAME src/lib/formats/ipf_dsk.cpp (BSD-3-Clause,
 * `generate_block_gap_0()`).
 *
 * ── Was NICHT geprueft ist ──────────────────────────────────────────────
 *
 * * SPS-kodierte Dateien: deren Zellstrom ist an den zwei SPS-Abbildern
 *   des eingeschraenkten Korpus geeicht (test_ipf_zellstrom, MF-1079);
 *   ihre Sektorebene ist hier nicht gemessen, weil der Korpus fehlt.
 * * Gap-Stroeme (Typ 1-3) und Kopierschutzspuren.
 * * Die Zwischenraum-Zellen gegen eine Aufnahme: disk-analyse schreibt in
 *   Behaelter ohne Gap-Beschreibung (HFE) jeden Zwischenraum als MFM(0x00),
 *   dort ist es kein Massstab. Gemessen ist: die DATENbereiche aller 160
 *   Spuren beider Disketten sind zellgleich mit disk-analyses HFE aus
 *   derselben Quelle.
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_track.h"
#include "uft/formats/ipf/uft_ipf_sektoren.h"
#include "uft/formats/ipf/uft_ipf_air.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR "tests/corpus_free"
#endif

extern const uft_format_plugin_t uft_format_plugin_ipf;

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int bedingung)
{
    if (bedingung) { gruen++; printf("  ok   %s\n", was); }
    else { rot++; printf("  FAIL %s\n", was); }
}

/** Erwarteter Inhalt eines selbstbenennenden Sektors. */
static void marke_fuellen(uint8_t *ziel, size_t n, int c, int h, int s)
{
    char m[32];
    snprintf(m, sizeof m, "UFT-K C%02d H%d S%02d ", c, h, s);
    for (size_t i = 0; i < n; i++) ziel[i] = (uint8_t)m[i % 17u];
}

typedef struct {
    unsigned spuren, sektoren, am_ort, crc_gut, gezeichnet;
    unsigned paare, max_null;   /* MFM-Zellregel ueber alle Spuren */
    unsigned falsche_art;
    unsigned mit_zellen;        /* Spuren mit Zellstrom — sonst ist die
                                   Zellregel leer erfuellt */
} bilanz_t;

static int bit_von(const uint8_t *p, size_t i)
{
    return (p[i >> 3] >> (7 - (i & 7u))) & 1;
}

static bool lies_diskette(const char *pfad, int zylinder, int spt,
                          int erste_id, uft_encoding_t art, bilanz_t *b)
{
    uft_disk_t disk;
    memset(&disk, 0, sizeof disk);
    memset(b, 0, sizeof *b);
    if (uft_format_plugin_ipf.open(&disk, pfad, true) != UFT_OK) return false;
    uint8_t soll[1024];
    for (int c = 0; c < zylinder; c++)
        for (int h = 0; h < 2; h++) {
            uft_track_t t;
            memset(&t, 0, sizeof t);
            if (uft_format_plugin_ipf.read_track(&disk, c, h, &t) != UFT_OK)
                continue;
            b->spuren++;
            if (t.encoding != art) b->falsche_art++;
            /* Zellregel: keine zwei benachbarten Einsen, hoechstens drei
             * Nullen am Stueck — auch in den Zwischenraeumen. */
            if (t.raw_data) {
                unsigned lauf = 0;
                b->mit_zellen++;
                for (size_t i = 0; i < t.raw_bits; i++) {
                    const int v = bit_von(t.raw_data, i);
                    if (v && i && bit_von(t.raw_data, i - 1)) b->paare++;
                    if (v) { if (lauf > b->max_null) b->max_null = lauf; lauf = 0; }
                    else lauf++;
                }
            }
            bool gesehen[64] = { false };
            for (size_t k = 0; k < t.sector_count; k++) {
                const uft_sector_t *s = &t.sectors[k];
                b->sektoren++;
                const int id = s->id.sector;
                if (s->crc_ok && s->id_crc_ok &&
                    !(s->status & (UFT_SECTOR_CRC_ERROR | UFT_SECTOR_ID_CRC_ERROR)))
                    b->crc_gut++;
                if (s->status & UFT_SECTOR_CRC_CHECKED) b->gezeichnet++;
                if (s->id.cylinder != c || s->id.head != h) continue;
                if (id < erste_id || id >= erste_id + spt || gesehen[id]) continue;
                if (s->data_len != 512 || !s->data) continue;
                marke_fuellen(soll, 512, c, h, id);
                if (memcmp(s->data, soll, 512) != 0) continue;
                gesehen[id] = true;
                b->am_ort++;
            }
            uft_track_release(&t);
        }
    uft_format_plugin_ipf.close(&disk);
    return true;
}

static uint8_t *datei_lesen(const char *pfad, size_t *n)
{
    FILE *f = fopen(pfad, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long g = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *p = (g > 0) ? (uint8_t *)malloc((size_t)g) : NULL;
    if (p && fread(p, 1, (size_t)g, f) != (size_t)g) { free(p); p = NULL; }
    fclose(f);
    *n = p ? (size_t)g : 0u;
    return p;
}

static const char *wegwerf(const char *name)
{
    static char pfad[1024];
    const char *d = getenv("TMPDIR");
    if (!d || !*d) d = getenv("TMP");
    if (!d || !*d) d = getenv("TEMP");
    if (!d || !*d) d = ".";
    snprintf(pfad, sizeof pfad, "%s/%s", d, name);
    return pfad;
}

int main(int argc, char **argv)
{
    /* argv[1] = Verzeichnis, argv[2] = Zylinderzahl: nur fuer die Messung
     * an den vollen Fassungen (gen_ipf_corpus.py --voll). */
    const char *dir = argc > 1 ? argv[1] : UFT_CORPUS_DIR;
    const int zyl = argc > 2 ? atoi(argv[2]) : 4;
    char amiga[1024], pcdd[1024];
    snprintf(amiga, sizeof amiga, "%s/disk_analyse_uftk_amiga.ipf", dir);
    snprintf(pcdd, sizeof pcdd, "%s/disk_analyse_uftk_pcdd.ipf", dir);
    const unsigned n_am = (unsigned)zyl * 2u * 11u;
    const unsigned n_pc = (unsigned)zyl * 2u * 9u;
    bilanz_t b;
    char text[160];

    printf("== Amiga (AmigaDOS, CAPS-kodiert) ==\n");
    pruefe("A1 die Datei oeffnet",
           lies_diskette(amiga, zyl, 11, 0, UFT_ENC_AMIGA_MFM, &b));
    snprintf(text, sizeof text, "A2 %u Spuren gelesen (erwartet %d)",
             b.spuren, zyl * 2);
    pruefe(text, b.spuren == (unsigned)zyl * 2u);
    snprintf(text, sizeof text, "A3 %u von %u Sektoren an ihrer eigenen "
             "Ortsmarke", b.am_ort, n_am);
    pruefe(text, b.am_ort == n_am);
    pruefe("A4 ... und keiner darueber hinaus", b.sektoren == n_am);
    pruefe("A5 alle mit gueltiger Kopf- und Datenpruefsumme", b.crc_gut == n_am);
    pruefe("A6 alle als GEPRUEFT gekennzeichnet (UFT_SECTOR_CRC_CHECKED)",
           b.gezeichnet == n_am);
    pruefe("A7 Kodierung nach dem Inhalt: Amiga-MFM, obwohl die Datei "
           "fuer beide Disketten „Amiga“ sagt", b.falsche_art == 0);
    snprintf(text, sizeof text, "A8 MFM-Zellregel: %u benachbarte Einsen, "
             "laengster Nulllauf %u", b.paare, b.max_null);
    pruefe(text, b.mit_zellen == b.spuren && b.spuren > 0 && b.paare == 0 && b.max_null <= 3);

    printf("== PC 720K (IBM-MFM, CAPS-kodiert, 19 Bloecke je Spur) ==\n");
    pruefe("P1 die Datei oeffnet",
           lies_diskette(pcdd, zyl, 9, 1, UFT_ENC_MFM, &b));
    snprintf(text, sizeof text, "P2 %u von %u Sektoren an ihrer eigenen "
             "Ortsmarke", b.am_ort, n_pc);
    pruefe(text, b.am_ort == n_pc);
    pruefe("P3 ... und keiner darueber hinaus", b.sektoren == n_pc);
    pruefe("P4 alle mit gueltiger Kopf- und Datenpruefsumme", b.crc_gut == n_pc);
    pruefe("P5 Kodierung nach dem Inhalt: IBM-MFM, obwohl die Datei "
           "„Amiga“ sagt", b.falsche_art == 0);
    snprintf(text, sizeof text, "P6 MFM-Zellregel mit Fuellbyte 0x4E: %u "
             "benachbarte Einsen, laengster Nulllauf %u", b.paare, b.max_null);
    pruefe(text, b.mit_zellen == b.spuren && b.spuren > 0 && b.paare == 0 && b.max_null <= 3);

    printf("== Zwischenraum hinter dem letzten Block (PC, Spur 0/0) ==\n");
    {
        /* Die Regel, hier unabhaengig vom Pruefling ausgeschrieben (Keir
         * Fraser ipf.txt „1. No gap streams“; MAME generate_block_gap_0):
         * Zelle a des Zwischenraums traegt vor der Naht das Musterbit an
         * Stelle (a mod 16), ab der Naht das an Stelle ((a - G) mod 16),
         * also am LUECKENENDE ausgerichtet. Beim letzten Block liegt die
         * Naht am Index (Spurbits - Startbit), wenn sie mindestens 16
         * Zellen von beiden Raendern entfernt ist. Geprueft werden nur
         * die Datenzellen (ungerade Musterstelle) — die Taktzellen haengen
         * am Kontext und sind durch P6 gedeckt. */
        size_t n = 0;
        uint8_t *roh = datei_lesen(pcdd, &n);
        ipf_air_disk_t *d = ipf_air_alloc();
        bool ok = roh && d && ipf_air_parse(roh, n, d) == IPF_AIR_OK;
        uint32_t tb = 0, dichte = 0, fl = 0, start_bit = 0, gv = 0;
        bool fz = false;
        int nb = 0;
        uint32_t anfang = 0, g = 0;
        if (ok) {
            ok = ipf_air_get_track_meta(d, 0, 0, &tb, &dichte, &fl, &fz) == 0 &&
                 ipf_air_get_track_start_bit(d, 0, 0, &start_bit) == 0 &&
                 (nb = ipf_air_get_block_count(d, 0, 0)) > 0 &&
                 ipf_air_get_block_gap_value(d, 0, 0, (uint32_t)nb - 1u, &gv) == 0;
            for (int i = 0; ok && i < nb; i++) {
                uint32_t db = 0, gb = 0;
                ok = ipf_air_get_block_sizes(d, 0, 0, (uint32_t)i, &db, &gb) == 0;
                if (i + 1 < nb) anfang += db + gb;
                else { anfang += db; g = gb; }
            }
        }
        pruefe("Z1 Blockzahlen und Startbit lesbar", ok && g > 32);
        snprintf(text, sizeof text, "Z2 die Datei nennt 0x4E als Fuellbyte "
                 "(gelesen 0x%02X)", (unsigned)gv);
        pruefe(text, ok && gv == 0x4Eu);

        uft_disk_t disk;
        uft_track_t t;
        memset(&disk, 0, sizeof disk);
        memset(&t, 0, sizeof t);
        bool zellen = ok &&
            uft_format_plugin_ipf.open(&disk, pcdd, true) == UFT_OK &&
            uft_format_plugin_ipf.read_track(&disk, 0, 0, &t) == UFT_OK &&
            t.raw_data && t.raw_bits == tb;
        pruefe("Z3 Zellstrom der Spur liegt vor, Laenge wie angesagt", zellen);
        if (zellen) {
            const uint32_t index = tb - start_bit;
            const uint32_t rel = index > anfang ? index - anfang : 0u;
            const uint32_t naht = (rel >= 16u && rel + 16u <= g) ? rel : g / 2u;
            const uint32_t ungerade = g & 1u;
            unsigned vor_ok = 0, vor_n = 0, nach_ok = 0, nach_n = 0;
            for (uint32_t a = 0; a < g; a++) {
                uint32_t p;
                if (a < naht) p = a & 15u;
                else if (a < naht + ungerade) continue;
                else p = (a - g) & 15u;
                if (!(p & 1u)) continue;
                const int soll = (int)((gv >> (7u - (p >> 1))) & 1u);
                const int ist = bit_von(t.raw_data, (size_t)anfang + a);
                if (a < naht) { vor_n++; vor_ok += (ist == soll); }
                else { nach_n++; nach_ok += (ist == soll); }
            }
            snprintf(text, sizeof text, "Z4 vor der Naht (bei Zelle %u von %u, "
                     "am Index): %u von %u Datenzellen = Muster ab Lueckenanfang",
                     naht, g, vor_ok, vor_n);
            pruefe(text, vor_n > 0 && vor_ok == vor_n);
            snprintf(text, sizeof text, "Z5 ab der Naht: %u von %u Datenzellen "
                     "= Muster am Lueckenende ausgerichtet", nach_ok, nach_n);
            pruefe(text, nach_n > 0 && nach_ok == nach_n);
            pruefe("Z6 die Naht liegt am Index, nicht in der Mitte (sonst waere "
                   "dieser Test blind fuer die Indexregel)", naht != g / 2u);
        }
        uft_track_release(&t);
        if (disk.plugin_data) uft_format_plugin_ipf.close(&disk);
        if (d) { ipf_air_free(d); free(d); }
        free(roh);
    }

    if (argc > 1) {
        printf("test_ipf_sektorebene (Messlauf): %d gruen, %d rot\n", gruen, rot);
        return rot == 0 ? 0 : 1;
    }

    printf("== Gegenprobe: ein Nutzbyte gekippt ==\n");
    {
        size_t n = 0;
        uint8_t *p = datei_lesen(pcdd, &n);
        pruefe("G1 PC-Datei lesbar", p != NULL);
        if (p) {
            /* Das erste Vorkommen der Marke von Sektor 0/0/5 im Behaelter:
             * ein Byte darin ist ein Datenbyte dieses Sektors. */
            const char *m = "UFT-K C00 H0 S05 ";
            size_t at = 0;
            for (size_t i = 0; i + 17 <= n; i++)
                if (memcmp(p + i, m, 17) == 0) { at = i; break; }
            pruefe("G2 die Marke von 0/0/5 steht in der Datei", at != 0);
            p[at + 3] ^= 0x20u;
            const char *tmp = wegwerf("uft_ipf_sektorebene.ipf");
            FILE *f = fopen(tmp, "wb");
            if (!f) { printf("Wegwerf-Datei fehlgeschlagen: %s\n", tmp); return 1; }
            fwrite(p, 1, n, f);
            fclose(f);
            uft_disk_t disk;
            memset(&disk, 0, sizeof disk);
            unsigned schlecht = 0, gut = 0, flagge = 0;
            if (uft_format_plugin_ipf.open(&disk, tmp, true) == UFT_OK) {
                uft_track_t t;
                memset(&t, 0, sizeof t);
                if (uft_format_plugin_ipf.read_track(&disk, 0, 0, &t) == UFT_OK) {
                    flagge = (t.status & UFT_TRACK_DATA_CRC) != 0;
                    for (size_t k = 0; k < t.sector_count; k++) {
                        const uft_sector_t *s = &t.sectors[k];
                        if (s->id.sector == 5)
                            schlecht += (!s->crc_ok &&
                                         (s->status & UFT_SECTOR_CRC_ERROR)) ? 1u : 0u;
                        else if (s->crc_ok) gut++;
                    }
                }
                uft_track_release(&t);
                uft_format_plugin_ipf.close(&disk);
            }
            remove(tmp);
            pruefe("G3 Sektor 5 wird geliefert, aber als CRC-Fehler gekennzeichnet",
                   schlecht == 1);
            pruefe("G4 die uebrigen 8 Sektoren der Spur bleiben gut", gut == 8);
            pruefe("G5 die Spur traegt UFT_TRACK_DATA_CRC (Behaelter-CRC)",
                   flagge == 1);
            free(p);
        }
    }

    printf("== Gegenprobe: Blockflaggen beim CAPS-Kodierer ==\n");
    {
        /* Keir Fraser, ipf.txt: „The flags field is ignored and assumed 0
         * if the encoder release in the INFO descriptor is 1“; MAME ebenso.
         * disk-analyse schreibt dort 0 — ohne diese Probe waere die Regel
         * unbelegt. Gesetzt wird 7 (beide Gap-Stroeme + Zaehlung in Bit)
         * im ersten Blockdeskriptor von Spur 0/0; wer das Feld liest,
         * zaehlt die Elemente in Bit und verliert die Spur. */
        size_t n = 0;
        uint8_t *p = datei_lesen(pcdd, &n);
        size_t pos = 0, data_at = 0;
        while (p && pos + 12 <= n) {
            const uint32_t len = ((uint32_t)p[pos + 4] << 24) |
                                 ((uint32_t)p[pos + 5] << 16) |
                                 ((uint32_t)p[pos + 6] << 8) | p[pos + 7];
            if (memcmp(p + pos, "DATA", 4) == 0) { data_at = pos; break; }
            if (len < 12) break;
            pos += len;
        }
        pruefe("F1 der erste DATA-Satz ist gefunden", data_at != 0);
        unsigned am_ort = 0, anzahl = 0;
        if (data_at) {
            uint8_t *flaggen = p + data_at + 28 + 20;
            flaggen[0] = 0; flaggen[1] = 0; flaggen[2] = 0; flaggen[3] = 7;
            const char *tmp = wegwerf("uft_ipf_flaggen.ipf");
            FILE *f = fopen(tmp, "wb");
            if (!f) { printf("Wegwerf-Datei fehlgeschlagen: %s\n", tmp); return 1; }
            fwrite(p, 1, n, f);
            fclose(f);
            uft_disk_t disk;
            uft_track_t t;
            memset(&disk, 0, sizeof disk);
            memset(&t, 0, sizeof t);
            uint8_t soll[512];
            if (uft_format_plugin_ipf.open(&disk, tmp, true) == UFT_OK &&
                uft_format_plugin_ipf.read_track(&disk, 0, 0, &t) == UFT_OK) {
                anzahl = (unsigned)t.sector_count;
                for (size_t k = 0; k < t.sector_count; k++) {
                    const uft_sector_t *s = &t.sectors[k];
                    marke_fuellen(soll, 512, 0, 0, s->id.sector);
                    if (s->data_len == 512 && memcmp(s->data, soll, 512) == 0)
                        am_ort++;
                }
            }
            uft_track_release(&t);
            if (disk.plugin_data) uft_format_plugin_ipf.close(&disk);
            remove(tmp);
        }
        snprintf(text, sizeof text, "F2 Flaggen 7 beim CAPS-Kodierer ignoriert: "
                 "%u von 9 Sektoren an ihrer Ortsmarke", am_ort);
        pruefe(text, am_ort == 9 && anzahl == 9);
        free(p);
    }

    printf("== Gegenprobe: IBM- UND Amiga-Koepfe in einer Spur ==\n");
    {
        uft_disk_t da, dp;
        uft_track_t ta, tp;
        memset(&da, 0, sizeof da); memset(&dp, 0, sizeof dp);
        memset(&ta, 0, sizeof ta); memset(&tp, 0, sizeof tp);
        bool ok = uft_format_plugin_ipf.open(&da, amiga, true) == UFT_OK &&
                  uft_format_plugin_ipf.open(&dp, pcdd, true) == UFT_OK &&
                  uft_format_plugin_ipf.read_track(&da, 0, 0, &ta) == UFT_OK &&
                  uft_format_plugin_ipf.read_track(&dp, 0, 0, &tp) == UFT_OK &&
                  ta.raw_data && tp.raw_data;
        pruefe("M1 beide Zellstroeme stehen", ok);
        if (ok) {
            const size_t na = ta.raw_size, np = tp.raw_size;
            uint8_t *beide = (uint8_t *)malloc(na + np);
            memcpy(beide, ta.raw_data, na);
            memcpy(beide + na, tp.raw_data, np);
            uft_track_t t;
            memset(&t, 0, sizeof t);
            uft_ipf_sektor_bericht_t sb;
            const int rc = uft_ipf_sektoren(beide, (uint32_t)((na + np) * 8u),
                                            &t, &sb);
            snprintf(text, sizeof text, "M2 mehrdeutig erkannt (IBM %u, Amiga %u "
                     "gueltige Koepfe)", sb.ibm_koepfe_ok, sb.amiga_koepfe_ok);
            pruefe(text, rc == 0 && sb.art == UFT_IPF_SEKTOR_MEHRDEUTIG);
            pruefe("M3 ... und KEIN Sektor angelegt statt geraten",
                   t.sector_count == 0);
            uft_track_release(&t);
            free(beide);
        }
        uft_track_release(&ta); uft_track_release(&tp);
        if (da.plugin_data) uft_format_plugin_ipf.close(&da);
        if (dp.plugin_data) uft_format_plugin_ipf.close(&dp);
    }

    printf("test_ipf_sektorebene: %d gruen, %d rot\n", gruen, rot);
    return rot == 0 ? 0 : 1;
}
