/**
 * @file test_2img_gegen_mame.c
 * @brief 2IMG: die byte-vertauschte Kennung, und eine erfundene
 *        3,5"-Geometrie (MF-1031)
 *
 * ── Die Quellen ─────────────────────────────────────────────────────
 *
 * **Erste Hand:** MAME `src/lib/formats/ap_dsk35.cpp`
 * (**BSD-3-Clause**; **nur gelesen**, Kanal *Spec* nach MF-695). Vier
 * Stellen tragen diesen Test:
 *
 *   Z. 452-459  `s_formats[]` — die **drei** gueltigen Paare aus
 *               Anordnung und Datenlaenge: (0|1, 143360) 5,25",
 *               (1, 409600) 3,5" einseitig, (1, 819200) 3,5"
 *               zweiseitig.
 *   Z. 469-470  *„Bernie ][ The Rescue wrote 2MGs with the signature
 *               byte-flipped, other fields are valid"* — die Kennung
 *               `"GMI2"` ist gueltig.
 *   Z. 483-486  Auch die **Datenlaenge** darf byte-vertauscht sein;
 *               MAME berichtigt sie.
 *   Z. 497-499  Struktur-Probe: `data_length + data_offset ==
 *               Dateigroesse`.
 *   Z. 558-567  `load()` fuer 3,5": Spur aussen, Kopf innen,
 *               `int ns = 12 - (track/16)`, 512 Byte je Sektor, und
 *               `sectors[si].sector = i` — die Nummer ist die Stelle
 *               in der DATEI.
 *
 * **Zweite, unabhaengige Hand:** `hxcfe`s `APPLE2_2MG`-Modul (GPL-2,
 * nur **ausgefuehrt**, nichts uebernommen). Gemessen an
 * `2img_spec_140k.2img`:
 *
 *     hxcfe -finput:2img_spec_140k.2img -infos
 *         -> 35 Spuren, 1 Seite, 560 Sektoren, 143360 Byte, 0 schlecht
 *     hxcfe -finput:2img_spec_140k.2img -conv:APPLE2_DO -foutput:x.do
 *         -> 143360 Byte, **byteidentisch** mit der Nutzlast
 *            (560 von 560 Sektoren)
 *
 * Damit ist die Pruefdatei von fremder Hand gelesen, nicht bloss von
 * UFT geschrieben.
 *
 * ── Und wo die beiden Haende sich widersprechen ─────────────────────
 *
 * Zweimal, und **beide Male gewinnt MAME mit Begruendung**:
 *
 *  1. `"GMI2"` — MAME nimmt an und nennt den Erzeuger; `hxcfe` weist ab
 *     (*„No loader support the file"*) und nennt keinen Grund.
 *  2. Die 3,5"-Zonentafel — `hxcfe` meldet fuer die 409 664 Byte grosse
 *     Datei **133 Spuren x 12 Sektoren x 256 Byte** (408 576 Byte, also
 *     1024 Byte zu wenig) und hat damit keine Zonentafel. MAMEs Tafel
 *     rechnet sich selbst auf: `16 * (12+11+10+9+8) * 512 = 409 600`,
 *     genau die Laenge in seinem eigenen `s_formats[]`.
 *
 * Ein Orakel ist eine Referenz, kein Beweis (MF-1015).
 *
 * ── Was der Vorzustand war ──────────────────────────────────────────
 *
 * Acht Befunde, im Kopf von `src/formats/2img/uft_2img.c` einzeln
 * ausgeschrieben. Die zwei schwersten:
 *
 *  * jede 2MG mit byte-vertauschter Kennung wurde **abgewiesen**;
 *  * jede **3,5"**-2MG wurde mit `35 x 16 x 256` gelesen, einseitig,
 *    Spurzahl `Datenlaenge / 4096` und auf 80 gedeckelt. Bei 819 200
 *    Nutzbytes waren damit **327 680** erreichbar und der Rest nicht,
 *    und die gemeldeten „Sektoren" waren 256-Byte-Haelften von
 *    512-Byte-Bloecken.
 *
 * ── Warum die Pruefdateien sich selbst benennen ─────────────────────
 *
 * Jeder Sektor beginnt mit `"UFT-K Cnn Hh Snn "`. Ein Leseergebnis sagt
 * damit nicht nur, DASS Bytes kamen, sondern ob die **richtige Stelle**
 * getroffen war — und bei einem Fehler, **welche Nachbarstelle**
 * geliefert wurde. Ohne dieses Muster waere MF-1021 auf einer
 * Fuellbyte-Diskette gehoben worden.
 *
 * **T2 und nicht T1b**, weil T1b einen fremden *Erzeuger* verlangt;
 * hier hat eine fremde Umsetzung nur *gelesen* (P3-333).
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"
#include "uft/formats/uft_2img.h"

extern const uft_format_plugin_t uft_format_plugin_2img;

#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR "tests/corpus_free"
#endif

#define F140 "2img_spec_140k.2img"
#define F800 "2img_spec_800k.2img"

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

static int schreibe(const char *pfad, const uint8_t *b, size_t n)
{
    FILE *f = fopen(pfad, "wb");
    if (!f) return 0;
    if (fwrite(b, 1, n, f) != n) { fclose(f); return 0; }
    fclose(f);
    return 1;
}

/** Erwarteter Kopf eines selbstbenennenden Sektors. */
static void erwarte_kopf(char *aus, size_t n, int cyl, int head, int sec)
{
    snprintf(aus, n, "UFT-K C%02d H%d S%02d ", cyl, head, sec);
}

/**
 * Alle Spuren lesen und jeden Sektor gegen seinen eigenen Namen halten.
 * @param spt_erwartet  0 = aus der Zonentafel, sonst fest
 */
static void pruefe_alle_spuren(const uft_format_plugin_t *p, uft_disk_t *d,
                               int zyl, int koepfe, int spt_fest,
                               int sektorgroesse, int soll_gesamt,
                               const char *wie)
{
    uft_track_t t;
    char erw[32], d1[260];
    int cyl, head, s, gesamt = 0, falsch = 0;
    int erster_falsch_cyl = -1, erster_falsch_head = -1, erster_falsch_s = -1;
    char erster_gefunden[32];

    memset(erster_gefunden, 0, sizeof(erster_gefunden));

    for (cyl = 0; cyl < zyl; cyl++) {
        for (head = 0; head < koepfe; head++) {
            int spt = spt_fest ? spt_fest : uft_2img_zone_spt(cyl);
            memset(&t, 0, sizeof(t));
            if (p->read_track(d, cyl, head, &t) != UFT_OK) {
                falsch += spt;
                if (erster_falsch_cyl < 0) {
                    erster_falsch_cyl = cyl; erster_falsch_head = head;
                    erster_falsch_s = 0;
                    snprintf(erster_gefunden, sizeof(erster_gefunden),
                             "read_track != UFT_OK");
                }
                continue;
            }
            if ((int)t.sector_count != spt) {
                falsch += spt;
                if (erster_falsch_cyl < 0) {
                    erster_falsch_cyl = cyl; erster_falsch_head = head;
                    erster_falsch_s = -1;
                    snprintf(erster_gefunden, sizeof(erster_gefunden),
                             "%d Sektoren statt %d", (int)t.sector_count, spt);
                }
                uft_track_release(&t);
                continue;
            }
            for (s = 0; s < spt; s++) {
                const uint8_t *dat = t.sectors[s].data;
                gesamt++;
                erwarte_kopf(erw, sizeof(erw), cyl, head, s);
                if (!dat || t.sectors[s].data_len != (size_t)sektorgroesse
                    || memcmp(dat, erw, strlen(erw)) != 0) {
                    falsch++;
                    if (erster_falsch_cyl < 0) {
                        erster_falsch_cyl = cyl;
                        erster_falsch_head = head;
                        erster_falsch_s = s;
                        if (dat) {
                            memcpy(erster_gefunden, dat, 17);
                            erster_gefunden[17] = 0;
                        } else {
                            snprintf(erster_gefunden,
                                     sizeof(erster_gefunden), "(keine Daten)");
                        }
                    }
                }
            }
            uft_track_release(&t);
        }
    }

    snprintf(d1, sizeof(d1), "%d Sektoren gelesen (Soll %d), %d falsch; "
             "erster Fehler C%d H%d S%d -> \"%s\"",
             gesamt, soll_gesamt, falsch, erster_falsch_cyl,
             erster_falsch_head, erster_falsch_s, erster_gefunden);
    pruefe(wie, gesamt == soll_gesamt && falsch == 0, d1);
}

int main(void)
{
    const uft_format_plugin_t *p = &uft_format_plugin_2img;
    const char *tmp = getenv("TEMP");
    char pfad[600], hilf[700], d1[300];
    uint8_t *f140 = NULL, *f800 = NULL, *kopie = NULL;
    size_t n140 = 0, n800 = 0;
    uft_disk_t disk;

    printf("2IMG gegen MAME ap_dsk35.cpp (BSD-3, nur gelesen)\n");
    printf("und gegen hxcfe APPLE2_2MG (GPL-2, nur ausgefuehrt)\n");
    printf("==================================================\n");

    snprintf(pfad, sizeof(pfad), "%s/%s", UFT_CORPUS_DIR, F140);
    f140 = lies(pfad, &n140);
    snprintf(pfad, sizeof(pfad), "%s/%s", UFT_CORPUS_DIR, F800);
    f800 = lies(pfad, &n800);
    if (!f140 || !f800) {
        printf("  [SKIP] Pruefdateien fehlen im Korpus (%s / %s)\n", F140, F800);
        free(f140); free(f800);
        printf("\n%d gruen, %d rot, 1 uebersprungen\n", gruen, rot);
        return 0;
    }
    if (!tmp) tmp = ".";
    printf("  %s: %zu Byte, %s: %zu Byte\n\n", F140, n140, F800, n800);

    /* ── 1. Der Kopf der 5,25"-Pruefdatei ──────────────────────────── */
    snprintf(d1, sizeof(d1), "%zu Byte; Kennung %c%c%c%c, Kopfgroesse %u, "
             "Version %u, Anordnung %u, Versatz %u, Laenge %u, Bloecke %u",
             n140, f140[0], f140[1], f140[2], f140[3],
             (unsigned)uft_read_le16(f140 + 0x08),
             (unsigned)uft_read_le16(f140 + 0x0A),
             (unsigned)uft_read_le32(f140 + 0x0C),
             (unsigned)uft_read_le32(f140 + 0x18),
             (unsigned)uft_read_le32(f140 + 0x1C),
             (unsigned)uft_read_le32(f140 + 0x14));
    pruefe("Pruefdatei 5,25\": 64 Byte Kopf + 143360 Nutzbytes, "
           "Anordnung 1 (ProDOS), 280 Bloecke",
           n140 == 143424u
           && memcmp(f140, "2IMG", 4) == 0
           && uft_read_le16(f140 + 0x08) == UFT_2IMG_HDR_SIZE
           && uft_read_le16(f140 + 0x0A) == 1
           && uft_read_le32(f140 + 0x0C) == 1
           && uft_read_le32(f140 + 0x18) == UFT_2IMG_HDR_SIZE
           && uft_read_le32(f140 + 0x1C) == 143360u
           && uft_read_le32(f140 + 0x14) == 280u, d1);

    /* ── 2. Die Zonentafel, und dass sie sich selbst aufrechnet ────── */
    {
        int c, summe = 0;
        for (c = 0; c < UFT_2IMG_M35_TRACKS; c++) summe += uft_2img_zone_spt(c);
        snprintf(d1, sizeof(d1), "Summe %d Sektoren (= %d Byte), "
                 "Raender 15->%d 16->%d 63->%d 64->%d 79->%d, "
                 "ausserhalb 80->%d",
                 summe, summe * 512, uft_2img_zone_spt(15),
                 uft_2img_zone_spt(16), uft_2img_zone_spt(63),
                 uft_2img_zone_spt(64), uft_2img_zone_spt(79),
                 uft_2img_zone_spt(80));
        pruefe("Zonentafel 12/11/10/9/8 an allen vier Raendern, Summe 800 "
               "Sektoren = 409600 Byte (genau MAMEs eigene Laenge)",
               summe == 800 && summe * 512 == 409600
               && uft_2img_zone_spt(15) == 12 && uft_2img_zone_spt(16) == 11
               && uft_2img_zone_spt(63) == 9 && uft_2img_zone_spt(64) == 8
               && uft_2img_zone_spt(79) == 8 && uft_2img_zone_spt(80) == 0, d1);
    }

    /* ── 3. Sonde: Kennung UND Struktur ───────────────────────────── */
    {
        int conf = -1;
        bool ja = p->probe(f140, 4096, n140, &conf);
        snprintf(d1, sizeof(d1), "probe=%d, Konfidenz=%d", ja, conf);
        pruefe("Sonde nimmt an, Konfidenz 95 — Kennung UND Struktur "
               "(Versatz + Laenge == Dateigroesse, MAME Z. 497)",
               ja && conf == 95, d1);
    }

    /* ── 4. Und die Sonde muss die DATEIgroesse benutzen ──────────────
     *
     * Das ist die Falle aus MF-1029, zum fuenften Mal: dort verwarf die
     * Sonde die Dateigroesse (`(void)file_size`) und verglich gegen die
     * PUFFERgroesse. Bei 2IMG war derselbe Wurf `(void)file_size`, und
     * damit war MAMEs Struktur-Probe gar nicht nachbaubar. Diese Zusage
     * haelt fest, dass die Groesse ANKOMMT: dieselben Bytes, eine
     * andere Dateigroesse, eine andere Antwort. */
    {
        int conf = -1;
        bool ja = p->probe(f140, 4096, n140 + 1, &conf);
        snprintf(d1, sizeof(d1), "probe=%d, Konfidenz=%d", ja, conf);
        pruefe("Dieselben Bytes mit falscher Dateigroesse: weiter "
               "angenommen, aber Konfidenz 80 statt 95 — die Sonde "
               "BENUTZT die Dateigroesse (MF-1029)",
               ja && conf == 80, d1);
    }

    /* ── 5. B1: die byte-vertauschte Kennung `"GMI2"` ──────────────── */
    kopie = (uint8_t *)malloc(n140);
    if (!kopie) { printf("  [ROT] kein Speicher\n"); return 1; }
    memcpy(kopie, f140, n140);
    memcpy(kopie, "GMI2", 4);
    {
        int conf = -1;
        bool ja = p->probe(kopie, 4096, n140, &conf);
        snprintf(d1, sizeof(d1), "probe=%d, Konfidenz=%d; es unterscheiden "
                 "sich genau die vier Kennungsbytes", ja, conf);
        pruefe("B1: `\"GMI2\"` wird angenommen wie `\"2IMG\"` — MAME nennt "
               "den Erzeuger (Bernie ][ The Rescue), hxcfe weist ab "
               "ohne Grund",
               ja && conf == 95
               && memcmp(kopie + 4, f140 + 4, n140 - 4) == 0, d1);
    }
    snprintf(hilf, sizeof(hilf), "%s/uft_2img_gmi2.2mg", tmp);
    if (schreibe(hilf, kopie, n140)) {
        memset(&disk, 0, sizeof(disk));
        {
            uft_error_t e = p->open(&disk, hilf, true);
            snprintf(d1, sizeof(d1), "open=%d, %u Zylinder, %u Koepfe, "
                     "%u Sektoren, %u Byte", (int)e,
                     disk.geometry.cylinders, disk.geometry.heads,
                     disk.geometry.sectors, disk.geometry.sector_size);
            pruefe("B1: eine GMI2-Datei laesst sich OEFFNEN — 35 x 1 x 16 "
                   "x 256", e == UFT_OK && disk.geometry.cylinders == 35
                   && disk.geometry.heads == 1
                   && disk.geometry.sectors == 16
                   && disk.geometry.sector_size == 256, d1);
            if (e == UFT_OK) p->close(&disk);
        }
        remove(hilf);
    }

    /* ── 6. B3: byte-vertauschte Datenlaenge ──────────────────────── */
    memcpy(kopie, f140, n140);
    kopie[0x1C] = 0x00; kopie[0x1D] = 0x02; kopie[0x1E] = 0x30;
    kopie[0x1F] = 0x00;   /* 143360 big-endian */
    {
        int conf = -1;
        bool ja = p->probe(kopie, 4096, n140, &conf);
        snprintf(d1, sizeof(d1), "probe=%d, Konfidenz=%d; als LE gelesen "
                 "waeren das %u Byte", ja, conf,
                 (unsigned)uft_read_le32(kopie + 0x1C));
        /* Konfidenz 95, nicht 80: die Struktur-Probe rechnet mit dem
         * BERICHTIGTEN Wert, und `64 + 143360 == 143424` geht auf. Das
         * ist genau MAMEs Reihenfolge — es setzt `data_length =
         * format.data_length` (Z. 485), BEVOR es die Groesse prueft
         * (Z. 497). Der erste Entwurf dieser Zusage erwartete 80; die
         * Messung hat sie berichtigt, nicht umgekehrt. */
        pruefe("B3: eine byte-vertauschte Datenlaenge wird erkannt und "
               "berichtigt (MAME Z. 483-486) — angenommen mit Konfidenz "
               "95, weil die Struktur-Probe den BERICHTIGTEN Wert nimmt",
               ja && conf == 95, d1);
    }
    snprintf(hilf, sizeof(hilf), "%s/uft_2img_swaplen.2mg", tmp);
    if (schreibe(hilf, kopie, n140)) {
        memset(&disk, 0, sizeof(disk));
        {
            uft_error_t e = p->open(&disk, hilf, true);
            snprintf(d1, sizeof(d1), "open=%d, %u Zylinder, %u Sektoren",
                     (int)e, disk.geometry.cylinders, disk.geometry.sectors);
            pruefe("B3: und sie laesst sich mit der BERICHTIGTEN Laenge "
                   "oeffnen — 35 Zylinder, nicht 3072",
                   e == UFT_OK && disk.geometry.cylinders == 35, d1);
            if (e == UFT_OK) p->close(&disk);
        }
        remove(hilf);
    }

    /* ── 7. B5: eine Laenge, die MAMEs Tafel nicht kennt ──────────── */
    memcpy(kopie, f140, n140);
    kopie[0x1C] = 0x01; kopie[0x1D] = 0x30; kopie[0x1E] = 0x02;
    kopie[0x1F] = 0x00;   /* 143361 */
    {
        int conf = -1;
        bool ja = p->probe(kopie, 4096, n140, &conf);
        snprintf(d1, sizeof(d1), "probe=%d, Konfidenz=%d", ja, conf);
        pruefe("B5 Gegenprobe: 143361 Nutzbytes stehen in keinem "
               "Tafeleintrag und werden ABGEWIESEN (vorher: 35 Zylinder "
               "geraten)", !ja, d1);
    }

    /* ── 8. B6: ein Datenversatz, der in den Kopf zeigt ───────────── */
    memcpy(kopie, f140, n140);
    kopie[0x18] = 32; kopie[0x19] = 0; kopie[0x1A] = 0; kopie[0x1B] = 0;
    {
        int conf = -1;
        bool ja = p->probe(kopie, 4096, n140, &conf);
        snprintf(d1, sizeof(d1), "probe=%d, Konfidenz=%d", ja, conf);
        pruefe("B6 Gegenprobe: Datenversatz 32 liegt IM Kopf und wird "
               "abgewiesen — vorher wurde er still auf 64 gehoben",
               !ja, d1);
    }

    /* ── 9. MF-725 bleibt: NIB-Inhalt wird abgesagt ───────────────── */
    memcpy(kopie, f140, n140);
    kopie[0x0C] = 2;
    snprintf(hilf, sizeof(hilf), "%s/uft_2img_nib.2mg", tmp);
    if (schreibe(hilf, kopie, n140)) {
        memset(&disk, 0, sizeof(disk));
        {
            uft_error_t e = p->open(&disk, hilf, true);
            snprintf(d1, sizeof(d1), "open=%d (erwartet %d)",
                     (int)e, (int)UFT_ERROR_NOT_SUPPORTED);
            pruefe("MF-725 bleibt: Anordnung 2 (roher Nibble-Strom) wird "
                   "ABGESAGT statt als Sektoren ausgegeben — MAME sagt "
                   "dasselbe (`// nibble images not supported`)",
                   e == UFT_ERROR_NOT_SUPPORTED, d1);
            if (e == UFT_OK) p->close(&disk);
        }
        remove(hilf);
    }

    /* ── 10. MF-729-Eichung: ein Nullpuffer ist keine 2MG ─────────── */
    {
        uint8_t null[256];
        int conf = -1;
        bool ja;
        memset(null, 0, sizeof(null));
        ja = p->probe(null, sizeof(null), sizeof(null), &conf);
        snprintf(d1, sizeof(d1), "probe=%d, Konfidenz=%d", ja, conf);
        pruefe("MF-729: ein Nullpuffer wird abgewiesen", !ja, d1);
    }

    /* ── 11. Die 5,25"-Datei ganz lesen ───────────────────────────── */
    snprintf(pfad, sizeof(pfad), "%s/%s", UFT_CORPUS_DIR, F140);
    memset(&disk, 0, sizeof(disk));
    if (p->open(&disk, pfad, true) == UFT_OK) {
        snprintf(d1, sizeof(d1), "%u x %u x %u x %u, gesamt %u",
                 disk.geometry.cylinders, disk.geometry.heads,
                 disk.geometry.sectors, disk.geometry.sector_size,
                 disk.geometry.total_sectors);
        pruefe("5,25\": 35 x 1 x 16 x 256, 560 Sektoren",
               disk.geometry.cylinders == 35 && disk.geometry.heads == 1
               && disk.geometry.sectors == 16
               && disk.geometry.sector_size == 256
               && disk.geometry.total_sectors == 560, d1);
        pruefe_alle_spuren(p, &disk, 35, 1, 16, 256, 560,
                           "5,25\": alle 560 Sektoren benennen sich selbst "
                           "richtig (dieselbe Nutzlast, die hxcfe "
                           "byteidentisch zurueckgibt)");
        p->close(&disk);
    } else {
        pruefe("5,25\": Datei laesst sich oeffnen", 0, "open != UFT_OK");
    }

    /* ── 12. B2: die 3,5"-Datei ganz lesen ────────────────────────── */
    snprintf(pfad, sizeof(pfad), "%s/%s", UFT_CORPUS_DIR, F800);
    memset(&disk, 0, sizeof(disk));
    if (p->open(&disk, pfad, true) == UFT_OK) {
        snprintf(d1, sizeof(d1), "%u x %u x %u (groesste Zone) x %u, "
                 "gesamt %u", disk.geometry.cylinders, disk.geometry.heads,
                 disk.geometry.sectors, disk.geometry.sector_size,
                 disk.geometry.total_sectors);
        pruefe("B2: 3,5\" einseitig: 80 Zylinder, 1 Kopf, 512 Byte, "
               "800 Sektoren — vorher 80 x 16 x 256 = 327680 von 409600 "
               "Byte erreichbar",
               disk.geometry.cylinders == 80 && disk.geometry.heads == 1
               && disk.geometry.sectors == 12
               && disk.geometry.sector_size == 512
               && disk.geometry.total_sectors == 800, d1);
        pruefe_alle_spuren(p, &disk, 80, 1, 0, 512, 800,
                           "B2: alle 800 Sektoren benennen sich selbst "
                           "richtig — das prueft Zonentafel und "
                           "Spurversatz zugleich");

        /* Obere Schranken */
        {
            uft_track_t t;
            uft_error_t e1, e2;
            memset(&t, 0, sizeof(t));
            e1 = p->read_track(&disk, 80, 0, &t);
            memset(&t, 0, sizeof(t));
            e2 = p->read_track(&disk, 0, 1, &t);
            snprintf(d1, sizeof(d1), "Zylinder 80 -> %d, Kopf 1 -> %d",
                     (int)e1, (int)e2);
            pruefe("Zylinder 80 und Kopf 1 werden auf einer einseitigen "
                   "3,5\"-Diskette abgewiesen",
                   e1 != UFT_OK && e2 != UFT_OK, d1);
        }
        p->close(&disk);
    } else {
        pruefe("B2: 3,5\"-Datei laesst sich oeffnen", 0, "open != UFT_OK");
    }

    /* ── 13. Der Spurversatz einzeln, gegen die Zonentafel ────────── */
    {
        long o0 = uft_2img_track_offset(0, 0, 1, 1, 12, 512, 64);
        long o1 = uft_2img_track_offset(1, 0, 1, 1, 12, 512, 64);
        long o16 = uft_2img_track_offset(16, 0, 1, 1, 12, 512, 64);
        long o79 = uft_2img_track_offset(79, 0, 1, 1, 12, 512, 64);
        long ds = uft_2img_track_offset(0, 1, 1, 2, 12, 512, 64);
        snprintf(d1, sizeof(d1), "C0=%ld C1=%ld C16=%ld C79=%ld, "
                 "zweiseitig C0H1=%ld", o0, o1, o16, o79, ds);
        /* C16 liegt hinter 16 Spuren mit je 12 Sektoren: 64 + 16*12*512 */
        pruefe("Spurversatz: 64 / 6208 / 98368 / 409088, und bei zwei "
               "Koepfen liegt C0H1 direkt hinter C0H0 (Kopf innen)",
               o0 == 64 && o1 == 64 + 12L * 512
               && o16 == 64 + 16L * 12 * 512
               && o79 == 64 + (409600L - 8L * 512)
               && ds == 64 + 12L * 512, d1);
    }

    free(kopie); free(f140); free(f800);
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
