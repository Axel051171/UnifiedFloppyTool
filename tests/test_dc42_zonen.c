/**
 * @file test_dc42_zonen.c
 * @brief Apple 3,5"-GCR ist ZONIERT — 1590 von 1600 Sektoren kamen von
 *        der falschen Stelle (MF-1140)
 *
 * ── Der Befund ────────────────────────────────────────────────────────
 *
 * `dc42_get_geometry()` setzte fuer 400K und 800K GCR fest `spt = 10`,
 * mit dem eigenen Kommentar „variable SPT, use avg". Eine
 * Apple-3,5"-Diskette hat eine **Zonentafel**: je 16 Spuren 12, 11, 10,
 * 9 und 8 Sektoren.
 *
 * **Die Summe geht dabei auf** — 16 x (12+11+10+9+8) = 800 je Seite und
 * 80 x 10 = 800 —, also stimmten Gesamtzahl und Dateigroesse, und keine
 * Groessenpruefung konnte etwas sehen. Gemessen am Vorzustand mit
 * selbstbenennenden Sektoren:
 *
 *     open -> rc=0, gemeldet 80 / 2 / 10, 1600 gesamt
 *     an ihrer eigenen Marke :   10
 *     abweichend             : 1590
 *     erste Abweichung (0,1,0): erwartet "C00 H1 S00",
 *                               gelesen  "C00 H0 S10"
 *
 * Richtig waren allein die ersten zehn Sektoren von Spur 0 Kopf 0 — der
 * einzige Ort, an dem beide Modelle denselben Versatz ergeben. Das ist
 * woertlich die Lehre aus MF-1026 (`victor9k`): **eine Summe, die
 * aufgeht, sagt nichts ueber die Verteilung darin.**
 *
 * ── Warum der Durchschreibfall es nicht gefangen hat ──────────────────
 *
 * Lese- und Schreibseite benutzten DIESELBE falsche Versatzformel; ein
 * Rundlauf ist damit in sich stimmig (Klasse MF-1009/MF-1028). Genau
 * deshalb trennt `docs/WRITE_VERIFICATION_TIERS.md` die Stufe W1 von
 * W2: W1 belegt, dass die Aenderung die Datei ERREICHT, nicht dass sie
 * an der richtigen STELLE landet.
 *
 * ── Referenz und Abgrenzung ───────────────────────────────────────────
 *
 * Die Zonentafel stammt aus MAMEs `src/lib/formats/ap_dsk35.cpp`
 * (BSD-3-Clause), `load()` Z. 558-562: `int ns = 12 - (track/16);` —
 * Kanal *Spec* nach MF-695, gelesen und nicht uebernommen. In diesem
 * Baum ist sie seit MF-1031 an `2img` gegen floptool abgenommen, und
 * `dc42` RUFT sie jetzt dort statt eine zweite Kopie zu fuehren
 * (Muster MF-1034).
 *
 * **Die Tafel steht in diesem Test ein zweites Mal**, damit der Pruefer
 * nicht dieselbe Quelle befragt wie der Pruefling — Klasse MF-1000, so
 * gehandhabt bei `victor9k` in MF-1085.
 *
 * **Was dieser Test NICHT belegt:** eine echte Macintosh-DC42 von
 * fremder Hand. Im Korpus liegt allein `libdsk_uftk_pc720.dc42`, also
 * der **nicht** zonierte 720K-MFM-Fall; die T1b-Stufe von `dc42` ruht
 * auf ihm. Die zonierten GCR-Spielarten haben weiterhin kein
 * Fremderzeugnis — siehe P3-397.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "uft/uft_format_plugin.h"

extern const uft_format_plugin_t uft_format_plugin_dc42;

#define SS      512
#define ZYL     80

static int gruen = 0, rot = 0;

static void zusage(const char *was, int ok)
{
    if (ok) { printf("  [OK]   %s\n", was); gruen++; }
    else    { printf("  [ROT]  %s\n", was); rot++; }
}

/* Die Zonentafel, zweite Niederschrift — der Pruefer befragt nicht
 * dieselbe Quelle wie der Pruefling (MF-1000). */
static int zone_spt(int zyl)
{
    if (zyl < 16) return 12;
    if (zyl < 32) return 11;
    if (zyl < 48) return 10;
    if (zyl < 64) return 9;
    return 8;
}

static uint32_t dc42_summe(const uint8_t *d, size_t n)
{
    uint32_t s = 0;
    for (size_t i = 0; i + 1 < n; i += 2) {
        uint16_t w = (uint16_t)(((uint16_t)d[i] << 8) | d[i + 1]);
        s += w;
        s = (s >> 1) | (s << 31);
    }
    return s;
}

/**
 * Baut eine DC42 mit selbstbenennenden Sektoren in ZONENGERECHTER
 * Anordnung: Zylinder aussen, Kopf innen, Sektorzahl je Spur aus der
 * Tafel.
 *
 * `laenge_luegt` setzt absichtlich eine falsche `data_size` in den
 * Kopf, um die Gegenprobe zu fahren.
 */
static long baue(const char *pfad, uint8_t fmt, int koepfe,
                 int laenge_luegt)
{
    long sektoren = 0;
    for (int c = 0; c < ZYL; c++) sektoren += (long)zone_spt(c) * koepfe;
    const long datenlaenge = sektoren * SS;

    uint8_t *daten = calloc(1, (size_t)datenlaenge);
    if (!daten) return -1;

    long off = 0;
    for (int c = 0; c < ZYL; c++)
        for (int h = 0; h < koepfe; h++)
            for (int s = 0; s < zone_spt(c); s++) {
                char marke[40];
                snprintf(marke, sizeof marke, "UFT-K C%02d H%d S%02d ",
                         c, h, s);
                memcpy(daten + off, marke, strlen(marke));
                off += SS;
            }

    uint8_t kopf[84];
    memset(kopf, 0, sizeof kopf);
    const char *name = "UFT Zonenprobe";
    kopf[0] = (uint8_t)strlen(name);
    memcpy(kopf + 1, name, strlen(name));
    uint32_t angesagt = laenge_luegt ? (uint32_t)(datenlaenge - SS)
                                     : (uint32_t)datenlaenge;
    kopf[0x40] = (uint8_t)(angesagt >> 24); kopf[0x41] = (uint8_t)(angesagt >> 16);
    kopf[0x42] = (uint8_t)(angesagt >> 8);  kopf[0x43] = (uint8_t)angesagt;
    uint32_t ck = dc42_summe(daten, (size_t)datenlaenge);
    kopf[0x48] = (uint8_t)(ck >> 24); kopf[0x49] = (uint8_t)(ck >> 16);
    kopf[0x4A] = (uint8_t)(ck >> 8);  kopf[0x4B] = (uint8_t)ck;
    kopf[0x50] = fmt;
    kopf[0x51] = (koepfe == 2) ? 0x22 : 0x12;
    kopf[82] = 0x01; kopf[83] = 0x00;   /* Magic 0x0100 */

    FILE *f = fopen(pfad, "wb");
    if (!f) { free(daten); return -1; }
    fwrite(kopf, 1, sizeof kopf, f);
    fwrite(daten, 1, (size_t)datenlaenge, f);
    fclose(f);
    free(daten);
    return sektoren;
}

/** Liest alle Spuren und zaehlt, wie viele Sektoren an ihrer Marke liegen. */
static void pruefe_lage(const char *pfad, long erwartete_sektoren,
                        int erwartete_koepfe, const char *titel)
{
    uft_disk_t disk;
    memset(&disk, 0, sizeof disk);
    uft_error_t rc = uft_format_plugin_dc42.open(&disk, pfad, true);

    char t[200];
    snprintf(t, sizeof t, "%s: laesst sich oeffnen", titel);
    zusage(t, rc == UFT_OK);
    if (rc != UFT_OK) return;

    snprintf(t, sizeof t, "%s: %d Koepfe gemeldet", titel, erwartete_koepfe);
    zusage(t, disk.geometry.heads == erwartete_koepfe);

    /* Die gemeldete Sektorzahl ist die GROESSTE Zone, nicht ein Mittel */
    snprintf(t, sizeof t, "%s: geometry.sectors ist die groesste Zone (12)",
             titel);
    zusage(t, disk.geometry.sectors == 12);

    snprintf(t, sizeof t, "%s: total_sectors ist die WAHRE Summe (%ld)",
             titel, erwartete_sektoren);
    zusage(t, disk.geometry.total_sectors == (uint32_t)erwartete_sektoren);

    long richtig = 0, falsch = 0;
    for (int c = 0; c < disk.geometry.cylinders; c++)
        for (int h = 0; h < disk.geometry.heads; h++) {
            uft_track_t tr;
            memset(&tr, 0, sizeof tr);
            if (uft_format_plugin_dc42.read_track(&disk, c, h, &tr) != UFT_OK)
                continue;
            /* Die Spur muss ihre ZONENGERECHTE Sektorzahl melden */
            if ((int)tr.sector_count != zone_spt(c)) falsch += 1000;
            for (size_t s = 0; s < tr.sector_count; s++) {
                char erw[40];
                snprintf(erw, sizeof erw, "UFT-K C%02d H%d S%02d ",
                         c, h, (int)s);
                const uint8_t *d = tr.sectors[s].data;
                if (d && memcmp(d, erw, strlen(erw)) == 0) richtig++;
                else falsch++;
            }
            uft_track_cleanup(&tr);
        }
    uft_format_plugin_dc42.close(&disk);

    snprintf(t, sizeof t,
             "%s: %ld von %ld Sektoren an ihrer eigenen Marke",
             titel, richtig, erwartete_sektoren);
    zusage(t, richtig == erwartete_sektoren && falsch == 0);
}

int main(void)
{
    printf("\nDC42-Zonentafel (MF-1140)\n\n");

    /* 800K, doppelseitig, zoniert */
    long n2 = baue("t_dc42_800k.dc42", 0x01, 2, 0);
    if (n2 > 0) {
        pruefe_lage("t_dc42_800k.dc42", n2, 2, "800K GCR");
        remove("t_dc42_800k.dc42");
    } else zusage("800K: Pruefabbild angelegt", 0);

    /* 400K, einseitig, dieselbe Tafel auf einer Seite */
    long n1 = baue("t_dc42_400k.dc42", 0x00, 1, 0);
    if (n1 > 0) {
        uft_disk_t d;
        memset(&d, 0, sizeof d);
        uft_error_t rc = uft_format_plugin_dc42.open(&d, "t_dc42_400k.dc42",
                                                     true);
        zusage("400K GCR: laesst sich oeffnen", rc == UFT_OK);
        if (rc == UFT_OK) {
            zusage("400K GCR: EINE Seite", d.geometry.heads == 1);
            zusage("400K GCR: 800 Sektoren gesamt",
                   d.geometry.total_sectors == 800);
            uft_format_plugin_dc42.close(&d);
        }
        remove("t_dc42_400k.dc42");
    } else zusage("400K: Pruefabbild angelegt", 0);

    /* Gegenprobe 1: die angesagte Laenge passt nicht zum Formatbyte.
     * Vorher wurde sie GEGLAUBT und die Geometrie trotzdem gemeldet
     * (Gestalt MF-1019/MF-1038); jetzt muss `open` absagen. */
    long n3 = baue("t_dc42_luegt.dc42", 0x01, 2, 1);
    if (n3 > 0) {
        uft_disk_t d;
        memset(&d, 0, sizeof d);
        uft_error_t rc = uft_format_plugin_dc42.open(&d, "t_dc42_luegt.dc42",
                                                     true);
        zusage("falsche data_size wird ABGEWIESEN statt geglaubt",
               rc != UFT_OK);
        if (rc == UFT_OK) uft_format_plugin_dc42.close(&d);
        remove("t_dc42_luegt.dc42");
    } else zusage("Gegenprobe: Pruefabbild angelegt", 0);

    /* Gegenprobe 2: negative Koordinaten. MF-519/529/931 — beim
     * Schreiben bestimmt der Index, WOHIN geschrieben wird. */
    long n4 = baue("t_dc42_neg.dc42", 0x01, 2, 0);
    if (n4 > 0) {
        uft_disk_t d;
        memset(&d, 0, sizeof d);
        if (uft_format_plugin_dc42.open(&d, "t_dc42_neg.dc42", true)
            == UFT_OK) {
            uft_track_t tr;
            memset(&tr, 0, sizeof tr);
            zusage("read_track weist Zylinder -1 ab",
                   uft_format_plugin_dc42.read_track(&d, -1, 0, &tr)
                   != UFT_OK);
            memset(&tr, 0, sizeof tr);
            zusage("read_track weist Kopf -1 ab",
                   uft_format_plugin_dc42.read_track(&d, 0, -1, &tr)
                   != UFT_OK);
            uft_format_plugin_dc42.close(&d);
        } else zusage("Gegenprobe negativ: oeffnen", 0);
        remove("t_dc42_neg.dc42");
    } else zusage("Gegenprobe negativ: Pruefabbild angelegt", 0);

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
