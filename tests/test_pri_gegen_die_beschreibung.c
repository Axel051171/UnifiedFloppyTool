/**
 * @file test_pri_gegen_die_beschreibung.c
 * @brief PRI: die Chunk-Felder waren vertauscht (MF-1036)
 *
 * ── Die Quelle ──────────────────────────────────────────────────────
 *
 * Der Spezifikationstext „PRI File Format (2020-03-26)" des **Urhebers**
 * (Hampa Hug, PCE). Er liegt im Baum als **Dokumentation** im Kopf von
 * `tools/uft-scout/work/HxCFloppyEmulator/libhxcfe/sources/loaders/
 * pri_loader/pri_format.h` — Kanal *Spec* nach MF-695: gelesen wird die
 * Beschreibung, **keine Zeile Code**.
 *
 * Aufbau je Chunk: **Kennung(4) · Groesse(4) · Daten(n) · CRC(4)**, alles
 * big-endian. Die Groesse zaehlt weder Kennung noch Groessenfeld noch
 * CRC; die CRC deckt Kennung + Groessenfeld + Daten. CRC-32 mit Polynom
 * **0x1EDC6F41**, Startwert 0.
 *
 * ── Die Eichung, und warum sie zaehlt ──────────────────────────────
 *
 * Die Beschreibung nennt **eine Zahl**: die CRC des leeren
 * `"END "`-Chunks ist **0x3D64AF78**. UFTs `uft_pri_crc()` ist aus der
 * Parameterangabe neu geschrieben, nicht uebernommen — und sie trifft
 * genau diese Zahl. Das ist ein **Beleg am Objekt** in derselben Art wie
 * MF-869, MF-1013 und die QRST-Pruefsumme aus MF-1028: die Quelle nennt
 * ein Ergebnis, und der eigene Code rechnet es nach.
 *
 * ── Was der Vorzustand war ──────────────────────────────────────────
 *
 * **UFT konnte keine PRI-Datei lesen, und die Sonde sagte trotzdem ja.**
 * Gemessen an einer spezifikationsgerechten Datei: `probe` = 1 mit
 * **Konfidenz 95**, `open` = **-25**. Damit gewann `pri` das
 * Erkennungsrennen und meldete dann, es koenne die Datei nicht lesen.
 * Gestalt von MF-961 (`86f`) und MF-1022 (`sap`).
 *
 * Der Grund war **vertauschte Felder**: der Leser nahm die Groesse bei
 * Versatz 0 und die Kennung bei 4. Er las damit die **Kennung als
 * Groesse** — bei einem `TEXT`-Chunk ergibt das 0x54455854, also ueber
 * 1,4 Milliarden, die Schranke griff, und die Schleife brach beim
 * **ersten** Chunk ab.
 *
 * Dazu: der Dateikopf ist **16** Byte, nicht 12 (der alte Leser begann
 * die Chunk-Schleife in der CRC des Kopfes und hielt `0x38D2C99D` fuer
 * eine Chunk-Groesse von 953 MB); die Version stand im Groessenfeld und
 * war damit immer **4**; die CRC wurde nie geprueft; und in TRAK waren
 * **Spurlaenge und Bittakt vertauscht**.
 *
 * ── Zweite Hand ─────────────────────────────────────────────────────
 *
 * hxcfes `PRI`-Modul (GPL-2, nur **ausgefuehrt**) liest die Pruefdateien
 * und meldet **5 Spuren, 2 Seiten**.
 *
 * **T2 und nicht T1b**, weil hxcfe PRI nur LESEN kann (`PRI;R ` in
 * seiner Modulliste) — es gibt keinen fremden *Erzeuger* (P3-333 gilt
 * hier weiter, anders als bei libdsk).
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

extern const uft_format_plugin_t uft_format_plugin_pri;
extern uint32_t uft_pri_crc(const uint8_t *b, size_t n);
extern int uft_pri_header_ok(const uint8_t *data, size_t size,
                             uint16_t *version);

#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR "tests/corpus_free"
#endif

#define F_EIN  "pri_spec_5x2.pri"
#define F_VOLL "pri_spec_voll.pri"
#define F_KURZ "pri_spec_kurzedata.pri"
#define KURZ_DATA 100u          /* DATA-Nutzlast der kurzen Datei */
#define BITS   50000u
#define SPURB  ((BITS + 7u) / 8u)       /* 6250 */

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *detail)
{
    if (ok) { gruen++; printf("  [OK]   %s\n", was); }
    else { rot++; printf("  [ROT]  %s\n         -> %s\n", was, detail); }
}

static uint8_t *lies(const char *pfad, size_t *n)
{
    FILE *f = fopen(pfad, "rb");
    uint8_t *b;
    long len;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0) { fclose(f); return NULL; }
    b = (uint8_t *)malloc((size_t)len);
    if (!b || fread(b, 1, (size_t)len, f) != (size_t)len) {
        free(b); fclose(f); return NULL;
    }
    fclose(f);
    *n = (size_t)len;
    return b;
}

static int schreib(const char *pfad, const uint8_t *b, size_t n)
{
    FILE *f = fopen(pfad, "wb");
    if (!f) return 0;
    if (fwrite(b, 1, n, f) != n) { fclose(f); return 0; }
    fclose(f);
    return 1;
}

static void be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);  p[3] = (uint8_t)v;
}

/** Die CRC eines Chunks bei `pos` neu setzen (nach einer Aenderung). */
static void crc_neu(uint8_t *d, size_t pos)
{
    uint32_t n = ((uint32_t)d[pos + 4] << 24) | ((uint32_t)d[pos + 5] << 16)
               | ((uint32_t)d[pos + 6] << 8) | d[pos + 7];
    be32(d + pos + 8 + n, uft_pri_crc(d + pos, 8 + n));
}

int main(void)
{
    const uft_format_plugin_t *p = &uft_format_plugin_pri;
    const char *tmp = getenv("TEMP");
    char pfad[600], hilf[700], d1[300];
    uint8_t *ein = NULL, *voll = NULL, *kurz = NULL, *kopie = NULL;
    size_t nein = 0, nvoll = 0, nkurz = 0;

    printf("PRI gegen die Beschreibung des Urhebers (Kanal Spec)\n");
    printf("====================================================\n");

    if (!tmp) tmp = ".";
    snprintf(pfad, sizeof(pfad), "%s/%s", UFT_CORPUS_DIR, F_EIN);
    ein = lies(pfad, &nein);
    snprintf(pfad, sizeof(pfad), "%s/%s", UFT_CORPUS_DIR, F_VOLL);
    voll = lies(pfad, &nvoll);
    snprintf(pfad, sizeof(pfad), "%s/%s", UFT_CORPUS_DIR, F_KURZ);
    kurz = lies(pfad, &nkurz);
    if (!ein || !voll || !kurz) {
        printf("  [SKIP] Pruefdateien fehlen im Korpus (%s / %s / %s)\n",
               F_EIN, F_VOLL, F_KURZ);
        free(ein); free(voll); free(kurz);
        printf("\n%d gruen, %d rot, 1 uebersprungen\n", gruen, rot);
        return 0;
    }

    /* ── 1. Die Eichung an der Zahl aus der Beschreibung ──────────── */
    {
        uint8_t end[8];
        uint32_t c;
        memcpy(end, "END ", 4);
        be32(end + 4, 0);
        c = uft_pri_crc(end, 8);
        snprintf(d1, sizeof(d1), "CRC = 0x%08X, die Beschreibung nennt "
                 "0x3D64AF78", (unsigned)c);
        pruefe("Eichung: die CRC des leeren `\"END \"`-Chunks trifft die "
               "Zahl, die die Beschreibung selbst nennt — ein Beleg AM "
               "OBJEKT fuer eine neu geschriebene Rechnung",
               c == 0x3D64AF78u, d1);
    }

    /* ── 2. Der Dateikopf ist 16 Byte und traegt seine CRC ────────── */
    {
        uint16_t v = 0xFFFF;
        int ok = uft_pri_header_ok(ein, nein, &v);
        snprintf(d1, sizeof(d1), "%zu Byte; Kennung %c%c%c%c, Groesse %u, "
                 "Version %u, header_ok=%d", nein, ein[0], ein[1], ein[2],
                 ein[3], (unsigned)((ein[4] << 24) | (ein[5] << 16)
                 | (ein[6] << 8) | ein[7]), (unsigned)v, ok);
        pruefe("Dateikopf: `\"PRI \"` + Groesse **4** + Version 0 + CRC "
               "= 16 Byte, und die CRC geht auf (vorher: 12 Byte "
               "angenommen, Version aus dem Groessenfeld)",
               ok && v == 0 && memcmp(ein, "PRI ", 4) == 0
               && ein[4] == 0 && ein[5] == 0 && ein[6] == 0 && ein[7] == 4,
               d1);
    }

    /* ── 3. Sonde ─────────────────────────────────────────────────── */
    {
        int conf = -1;
        bool ja = p->probe(ein, 4096, nein, &conf);
        snprintf(d1, sizeof(d1), "probe=%d, Konfidenz=%d", (int)ja, conf);
        pruefe("Sonde nimmt an, Konfidenz 95 — Kennung UND CRC des "
               "Dateikopfs", ja && conf == 95, d1);
    }

    kopie = (uint8_t *)malloc(nein);
    if (!kopie) { printf("  [ROT] kein Speicher\n"); return 1; }

    /* ── 4. Gegenprobe: eine falsche Kopf-CRC ─────────────────────── */
    {
        int conf = -1;
        bool ja;
        memcpy(kopie, ein, nein);
        kopie[15] ^= 0x01;              /* letztes Byte der Kopf-CRC */
        ja = p->probe(kopie, 4096, nein, &conf);
        snprintf(d1, sizeof(d1), "probe=%d, Konfidenz=%d", (int)ja, conf);
        pruefe("Gegenprobe: ein einziges gekipptes Bit in der Kopf-CRC "
               "wird ABGEWIESEN und faellt ins Band „nur die Kennung\" "
               "(40) — vorher meldete die Sonde 95 auf vier Byte allein",
               !ja && conf == 40, d1);
    }

    /* ── 5. Gegenprobe: Groessenfeld des Kopfes ist nicht 4 ───────── */
    {
        int conf = -1;
        bool ja;
        memcpy(kopie, ein, nein);
        kopie[7] = 8;
        crc_neu(kopie, 0);              /* CRC mitziehen, damit sie NICHT
                                         * der Grund der Abweisung ist */
        ja = p->probe(kopie, 4096, nein, &conf);
        snprintf(d1, sizeof(d1), "probe=%d, Konfidenz=%d (CRC wurde "
                 "nachgezogen, damit sie nicht der Grund ist)",
                 (int)ja, conf);
        pruefe("Gegenprobe: Kopfgroesse 8 statt 4 wird abgewiesen — und "
               "die CRC ist dabei GUELTIG, der Grund ist also wirklich "
               "das Groessenfeld", !ja, d1);
    }

    /* ── 6. Gegenprobe: eine unbekannte Version ───────────────────── */
    {
        int conf = -1;
        bool ja;
        memcpy(kopie, ein, nein);
        kopie[8] = 0; kopie[9] = 1;     /* Version 1 */
        crc_neu(kopie, 0);
        ja = p->probe(kopie, 4096, nein, &conf);
        snprintf(d1, sizeof(d1), "probe=%d, Konfidenz=%d", (int)ja, conf);
        pruefe("Gegenprobe: Version 1 wird abgewiesen — die Beschreibung "
               "kennt nur 0, und eine andere koennte die Bedeutung der "
               "Chunks aendern", !ja, d1);
    }

    /* ── 7. Oeffnen und alle Spuren lesen ────────────────────────── */
    {
        uft_disk_t disk;
        uft_error_t e;
        int falsch = 0, gesamt = 0;
        char gefunden[32];
        memset(gefunden, 0, sizeof(gefunden));
        memset(&disk, 0, sizeof(disk));
        snprintf(pfad, sizeof(pfad), "%s/%s", UFT_CORPUS_DIR, F_EIN);
        e = p->open(&disk, pfad, true);
        if (e == UFT_OK) {
            int c, h;
            snprintf(d1, sizeof(d1), "%u Zylinder, %u Koepfe, "
                     "total_sectors %u", disk.geometry.cylinders,
                     disk.geometry.heads, disk.geometry.total_sectors);
            pruefe("open: 5 Zylinder, 2 Koepfe, 10 Spuren",
                   disk.geometry.cylinders == 5 && disk.geometry.heads == 2
                   && disk.geometry.total_sectors == 10, d1);

            for (c = 0; c < 5; c++) for (h = 0; h < 2; h++) {
                uft_track_t t;
                char erw[32];
                memset(&t, 0, sizeof(t));
                gesamt++;
                if (p->read_track(&disk, c, h, &t) != UFT_OK
                    || t.sector_count != 1 || !t.sectors[0].data) {
                    falsch++;
                    if (!gefunden[0])
                        snprintf(gefunden, sizeof(gefunden),
                                 "C%d H%d nicht lesbar", c, h);
                    continue;
                }
                snprintf(erw, sizeof(erw), "UFT-K C%02d H%d PRI ", c, h);
                if (t.sectors[0].data_len != SPURB
                    || memcmp(t.sectors[0].data, erw, strlen(erw)) != 0) {
                    falsch++;
                    if (!gefunden[0]) {
                        memcpy(gefunden, t.sectors[0].data, 19);
                        gefunden[19] = 0;
                    }
                }
                uft_track_release(&t);
            }
            snprintf(d1, sizeof(d1), "%d Spuren, %d falsch; erster Fehler "
                     "\"%s\"; erwartete Laenge %u Byte", gesamt, falsch,
                     gefunden, (unsigned)SPURB);
            pruefe("alle 10 Spuren benennen sich selbst richtig, und jede "
                   "ist (Spurlaenge+7)/8 = 6250 Byte lang — die Laenge "
                   "kommt aus TRAK +8, nicht aus der DATA-Groesse und "
                   "nicht aus dem Bittakt",
                   gesamt == 10 && falsch == 0, d1);
            p->close(&disk);
        } else {
            snprintf(d1, sizeof(d1), "open=%d", (int)e);
            pruefe("open: 5 Zylinder, 2 Koepfe, 10 Spuren", 0, d1);
        }
    }

    /* ── 8. WEAK und BCLK hindern das Lesen nicht ────────────────── */
    {
        uft_disk_t disk;
        uft_error_t e;
        memset(&disk, 0, sizeof(disk));
        snprintf(pfad, sizeof(pfad), "%s/%s", UFT_CORPUS_DIR, F_VOLL);
        e = p->open(&disk, pfad, true);
        snprintf(d1, sizeof(d1), "open=%d, %u Zylinder, %u Koepfe, "
                 "%u Spuren", (int)e, disk.geometry.cylinders,
                 disk.geometry.heads, disk.geometry.total_sectors);
        pruefe("dieselbe Diskette mit WEAK- und BCLK-Chunks liest sich "
               "unveraendert — sie werden erkannt und uebersprungen, "
               "nicht angewandt (die Merkmalstafel sagt das)",
               e == UFT_OK && disk.geometry.cylinders == 5
               && disk.geometry.heads == 2
               && disk.geometry.total_sectors == 10, d1);
        if (e == UFT_OK) p->close(&disk);
    }

    /* ── 9. Gegenprobe: eine falsche Chunk-CRC im Rumpf ──────────── */
    {
        uft_disk_t disk;
        uft_error_t e;
        memcpy(kopie, ein, nein);
        /* MF-1036: **in der NUTZLAST** kippen, nicht im Groessenfeld.
         *
         * Der erste Entwurf kippte Byte 20 — das ist das obere Byte der
         * TEXT-Chunk-Groesse. Damit passte der Rahmen nicht mehr in die
         * Datei, die Schleife brach an der Rahmenschranke ab, und die
         * Zusage war gruen aus dem FALSCHEN Grund: die Mutation „CRC
         * nicht pruefen" rutschte durch. Dieselbe Falle wie MF-1014,
         * MF-1026 und MF-1028.
         *
         * Byte 24 ist das erste Byte der TEXT-Nutzlast. Der Rahmen bleibt
         * gueltig, und **nur** die CRC scheitert. */
        kopie[24] ^= 0xFF;
        snprintf(hilf, sizeof(hilf), "%s/uft_pri_crc_kaputt.pri", tmp);
        if (schreib(hilf, kopie, nein)) {
            memset(&disk, 0, sizeof(disk));
            e = p->open(&disk, hilf, true);
            snprintf(d1, sizeof(d1), "open=%d", (int)e);
            pruefe("Gegenprobe: ein gekipptes Byte in einem Chunk laesst "
                   "seine CRC scheitern, und die Datei wird ABGEWIESEN — "
                   "vorher wurde keine CRC geprueft",
                   e != UFT_OK, d1);
            if (e == UFT_OK) p->close(&disk);
            remove(hilf);
        } else {
            pruefe("Gegenprobe: falsche Chunk-CRC", 0,
                   "Pruefdatei liess sich nicht schreiben");
        }
    }

    /* ── 10. Muell nach `"END "` wird ignoriert ──────────────────── */
    {
        uft_disk_t disk;
        uft_error_t e;
        uint8_t *lang = (uint8_t *)malloc(nein + 64);
        if (lang) {
            memcpy(lang, ein, nein);
            memset(lang + nein, 0x5A, 64);
            snprintf(hilf, sizeof(hilf), "%s/uft_pri_nach_end.pri", tmp);
            if (schreib(hilf, lang, nein + 64)) {
                memset(&disk, 0, sizeof(disk));
                e = p->open(&disk, hilf, true);
                snprintf(d1, sizeof(d1), "open=%d, %u Spuren", (int)e,
                         disk.geometry.total_sectors);
                pruefe("64 Byte Muell hinter `\"END \"` werden ignoriert — "
                       "so sagt es die Beschreibung („Any data that "
                       "follows should be ignored\")",
                       e == UFT_OK && disk.geometry.total_sectors == 10, d1);
                if (e == UFT_OK) p->close(&disk);
                remove(hilf);
            }
            free(lang);
        }
    }

    /* ── 11. M13: die Sonde muss die DATEIgroesse nehmen ───────────
     *
     * Die Falle aus MF-1029, zum sechsten Mal im Baum: vorher stand hier
     * `(void)file_size`. Dieselben Bytes, eine zu kleine Dateigroesse —
     * und die Antwort muss anders sein. */
    {
        int conf = -1;
        bool ja = p->probe(ein, nein, 20u, &conf);
        snprintf(d1, sizeof(d1), "probe=%d, Konfidenz=%d bei Dateigroesse "
                 "20 und %zu Byte Puffer", (int)ja, conf, nein);
        pruefe("Gegenprobe: ein grosser Puffer mit Dateigroesse 20 wird "
               "ABGEWIESEN — die Sonde nimmt die DATEIgroesse, nicht die "
               "Puffergroesse (MF-1029)", !ja, d1);
    }

    /* ── 12. M6: auch `open` prueft die Version ───────────────────── */
    {
        uft_disk_t disk;
        uft_error_t e;
        memcpy(kopie, ein, nein);
        kopie[8] = 0; kopie[9] = 1;     /* Version 1 */
        crc_neu(kopie, 0);
        snprintf(hilf, sizeof(hilf), "%s/uft_pri_version1.pri", tmp);
        if (schreib(hilf, kopie, nein)) {
            memset(&disk, 0, sizeof(disk));
            e = p->open(&disk, hilf, true);
            snprintf(d1, sizeof(d1), "open=%d", (int)e);
            pruefe("Gegenprobe: auch `open()` weist Version 1 ab — nicht "
                   "nur die Sonde. Sonst haenge die Pruefung an einem "
                   "einzigen Weg", e != UFT_OK, d1);
            if (e == UFT_OK) p->close(&disk);
            remove(hilf);
        }
    }

    /* ── 13. M10: eine KUERZERE DATA als die Spurlaenge ────────────
     *
     * Die Beschreibung erlaubt das ausdruecklich: „The DATA chunk may be
     * shorter than the track length in the preceding TRAK chunk
     * suggests. In that case the remainder of the track data should be
     * set to 0." Ohne diese Zusage liesse sich nicht unterscheiden, ob
     * die Laenge aus TRAK oder aus DATA kommt. */
    {
        uft_disk_t disk;
        uft_error_t e;
        int ok = 0;
        snprintf(pfad, sizeof(pfad), "%s/%s", UFT_CORPUS_DIR, F_KURZ);
        memset(&disk, 0, sizeof(disk));
        e = p->open(&disk, pfad, true);
        if (e == UFT_OK) {
            uft_track_t t;
            memset(&t, 0, sizeof(t));
            if (p->read_track(&disk, 0, 0, &t) == UFT_OK
                && t.sector_count == 1 && t.sectors[0].data) {
                size_t i;
                int rest_null = 1;
                for (i = KURZ_DATA; i < t.sectors[0].data_len; i++)
                    if (t.sectors[0].data[i] != 0) { rest_null = 0; break; }
                ok = (t.sectors[0].data_len == SPURB
                      && memcmp(t.sectors[0].data, "UFT-K C00 H0 PRI ", 17) == 0
                      && rest_null);
                snprintf(d1, sizeof(d1), "%zu Byte Spur (Soll %u), "
                         "DATA-Nutzlast %u Byte, Rest null: %d",
                         t.sectors[0].data_len, (unsigned)SPURB,
                         (unsigned)KURZ_DATA, rest_null);
                uft_track_release(&t);
            } else {
                snprintf(d1, sizeof(d1), "Spur 0 nicht lesbar");
            }
            p->close(&disk);
        } else {
            snprintf(d1, sizeof(d1), "open=%d", (int)e);
        }
        pruefe("eine DATA, die KUERZER ist als die Spurlaenge, wird auf "
               "6250 Byte mit Nullen aufgefuellt — die Laenge kommt aus "
               "TRAK, nicht aus DATA (so sagt es die Beschreibung)",
               ok, d1);
    }

    /* ── 14. M12: hinter `"END "` wird auch ein GUELTIGER Chunk
     *        ignoriert ───────────────────────────────────────────────
     *
     * Der erste Entwurf hing 64 Byte Muell an. Der ist nicht zerlegbar,
     * also brach die Schleife ohnehin ab — die Mutation „END beendet
     * nicht mehr" rutschte durch. Jetzt steht dort ein **gueltiges**
     * TRAK+DATA-Paar mit richtigen CRCs: wird END geachtet, bleiben es
     * 10 Spuren; wird es nicht geachtet, werden es 11. */
    {
        uft_disk_t disk;
        uft_error_t e;
        size_t zusatz = (8 + 16 + 4) + (8 + 32 + 4);
        uint8_t *lang = (uint8_t *)malloc(nein + zusatz);
        if (lang) {
            uint8_t *q = lang + nein;
            memcpy(lang, ein, nein);
            /* TRAK fuer Zylinder 9, Kopf 1 — eine Spur, die es sonst
             * nicht gibt. */
            memcpy(q, "TRAK", 4);
            be32(q + 4, 16);
            be32(q + 8, 9); be32(q + 12, 1);
            be32(q + 16, 256); be32(q + 20, 250000);
            be32(q + 24, uft_pri_crc(q, 24));
            q += 28;
            memcpy(q, "DATA", 4);
            be32(q + 4, 32);
            memset(q + 8, 0xAB, 32);
            be32(q + 40, uft_pri_crc(q, 40));

            snprintf(hilf, sizeof(hilf), "%s/uft_pri_nach_end_gueltig.pri",
                     tmp);
            if (schreib(hilf, lang, nein + zusatz)) {
                memset(&disk, 0, sizeof(disk));
                e = p->open(&disk, hilf, true);
                snprintf(d1, sizeof(d1), "open=%d, %u Zylinder, %u Spuren "
                         "(ohne die END-Regel waeren es 11 und 10 "
                         "Zylinder)", (int)e, disk.geometry.cylinders,
                         disk.geometry.total_sectors);
                pruefe("ein GUELTIGES TRAK+DATA-Paar hinter `\"END \"` "
                       "wird ignoriert — es bleiben 10 Spuren und 5 "
                       "Zylinder",
                       e == UFT_OK && disk.geometry.total_sectors == 10
                       && disk.geometry.cylinders == 5, d1);
                if (e == UFT_OK) p->close(&disk);
                remove(hilf);
            }
            free(lang);
        }
    }

    free(kopie); free(ein); free(voll); free(kurz);
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
