/**
 * @file test_dim_gegen_hxcfe.c
 * @brief DIM (Sharp X68000): die Medientabelle ist aufgeloest — und
 *        zwei von vier Medienbytes wurden vorher FALSCH ZERLEGT
 *        (MF-1037).
 *
 * ── Woher die Frage kommt ────────────────────────────────────────────
 *
 * MF-1019 hat die Medientabelle ausdruecklich **offen gelassen** (als
 * P3-325), weil drei Umsetzungen drei verschiedene Tabellen hatten und
 * keine zwei bei mehr als zwei Werten uebereinstimmten. Der Test
 * `test_dim_kennung_und_groesse.c` hielt den damaligen Stand deshalb
 * **festgenagelt und nannte sich nicht bewiesen**.
 *
 * **Der Grund fuer den Widerspruch ist jetzt gemessen: zwei der drei
 * Tabellen sind Tabellen fuer ZWEI VERSCHIEDENE FORMATE.** Die
 * Formatbeschreibung im Kopf von hxcfes
 * `libhxcfe/sources/loaders/dim_x68k_loader/dim_x68k_format.h`
 * (Dokumentation, Kanal *Spec* nach MF-695 — keine Zeile Code) fuehrt
 * **vier** DIM-Medienbytes und listet die **DCP**-Medienbytes getrennt
 * daneben, darunter `0x11 = 2HD-BASIC` und `0x19 = 2DD-BASIC`. Genau
 * diese Werte standen in UFTs DIM-Tabelle.
 *
 * ── Der Rotbeweis: was der Vorzustand tat ────────────────────────────
 *
 * Gemessen am unveraenderten Produktionspfad, je Medienbyte eine
 * Pruefdatei mit selbstbenennenden Sektoren ("UFT-K Cnn Hh Snn "):
 *
 *     0x00  1 261 824 Byte  probe 88  open   0  77x2x8x1024  RICHTIG
 *     0x01  1 474 816 Byte  probe 88  open   0  77x2x8x1024  FALSCH
 *     0x02  1 229 056 Byte  probe  0  open -25               ABGEWIESEN
 *     0x03  1 474 816 Byte  probe 88  open   0  77x2x8x1024  FALSCH
 *
 * **Zwei von vier wurden angenommen und falsch zerlegt, mit Konfidenz
 * 88.** Spur (40,1) — die aeusserste Spur der zweiten Seite — lieferte
 * in beiden Faellen den Sektor "UFT-K C36 H0 S01": einen Block von der
 * **anderen Seite**, vier Zylinder daneben. Und weil 77 x 2 x 8 x 1024
 * nur 1 261 568 der 1 474 560 Nutzbytes abdeckt, fielen **212 992
 * Byte** still weg.
 *
 * Genau **eine** Datei hat die Groessenpruefung aus MF-1019 gerettet:
 * 0x02, weil sie kleiner ist als die falsche Rechnung verlangt. Bei
 * 0x01 und 0x03 ist sie **groesser**, und `fs < erwartet` sieht ein
 * Zuviel nicht — der Satz „ist ein Eintrag falsch, wird die Datei
 * abgewiesen statt falsch zerlegt" hielt nur in eine Richtung.
 *
 * ── Die Abnahme: hxcfe hat die Dateien SELBST ZERLEGT ────────────────
 *
 * Nicht nur gelesen, sondern Sektor fuer Sektor. hxcfes
 * `X68000_DIM`-Modul liest die vier Dateien und sein `IMD_IMG`-Modul
 * schreibt sie als ImageDisk — und **eine IMD nennt je Sektor
 * Zylinder, Kopf und Sektornummer ausdruecklich**. Damit liegt die
 * Anordnung von fremder Hand fest. Verglichen wurde byteweise gegen
 * die Stelle, die UFTs Formel liefert
 * (`256 + (zyl*koepfe+kopf)*spt*ss + (sektor-1)*ss`):
 *
 *     0x00   1232 von 1232 Sektoren byteidentisch
 *     0x01   1440 von 1440
 *     0x02   2400 von 2400
 *     0x03   2880 von 2880
 *     ────────────────────────────────────────────
 *            7952 von 7952, 0 abweichend
 *
 * Die einzigen Spuren, in denen hxcfes IMD von UFTs Sektorgroesse
 * abweicht, sind seine **leeren** Anhangsspuren (0 Sektoren,
 * Groessenkode 0) jenseits der Zylinderzahl der Diskette — es schreibt
 * einen Behaelter fester Groesse (79 bzw. 84 Zylinder). Das gehoert
 * dazu gesagt, damit „7952 von 7952" nicht mehr behauptet als es ist.
 *
 * Zweitens meldet `hxcfe -infos` je Datei die Geometrie im Klartext,
 * einschliesslich Drehzahl:
 *
 *     0x00  "1232kB, 77 tracks, 2 side(s),  8 sectors/track, rpm:360"
 *     0x01  "1440kB, 80 tracks, 2 side(s),  9 sectors/track, rpm:300"
 *     0x02  "1200kB, 80 tracks, 2 side(s), 15 sectors/track, rpm:360"
 *     0x03  "1440kB, 80 tracks, 2 side(s), 18 sectors/track, rpm:300"
 *
 * ── Wo MAME begruendet ueberstimmt wird ──────────────────────────────
 *
 * Fuer 0x03 sagt MAMEs `dim_dsk.cpp` `spt = 9, size = 3` (9 x 1024),
 * hxcfes Beschreibung sagt 18 x 512. **Beide ergeben 1 474 560 Byte**,
 * die Dateigroesse kann es also nicht entscheiden. Entschieden hat es
 * hxcfes **ausgefuehrter** Lader: 2880 Sektoren. Dieselbe Lage wie
 * MF-1015 (`udi`), nur diesmal mit einem Lauf statt einem Argument.
 *
 * ── Die Stufe, und warum sie nicht T1b ist ───────────────────────────
 *
 * **T2 und nicht T1b**, weil hxcfes DIM-Modul nur **lesen** kann
 * (`X68000_DIM;R ` in der Modulliste) — es gibt keinen fremden
 * *Erzeuger*. Anders als bei libdsk (MF-1032/1033) hilft hier kein
 * `-otype`. Die Pruefdateien sind hauseigen; fremd ist die **Zerlegung**.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_format_plugin.h"
#include "uft/uft_track.h"
#include "uft/uft_types.h"

extern const uft_format_plugin_t uft_format_plugin_dim;

static int gruen = 0;
static int rot = 0;

static void pruefe(const char *name, int bedingung, const char *hinweis)
{
    if (bedingung) {
        printf("  [OK]   %s\n", name);
        gruen++;
    } else {
        printf("  [ROT]  %s%s%s\n", name, hinweis ? "  -- " : "",
               hinweis ? hinweis : "");
        rot++;
    }
}

#define HDR 256

typedef struct {
    uint8_t     media;
    const char *datei;
    int         zyl, koepfe, spt;
    int         ss;
} fall_t;

static const fall_t FAELLE[] = {
    { 0x00, "dim_x68k_m00_1232k.dim", 77, 2,  8, 1024 },
    { 0x01, "dim_x68k_m01_1440k.dim", 80, 2,  9, 1024 },
    { 0x02, "dim_x68k_m02_1200k.dim", 80, 2, 15,  512 },
    { 0x03, "dim_x68k_m03_1440k.dim", 80, 2, 18,  512 },
};
#define NFAELLE ((int)(sizeof(FAELLE) / sizeof(FAELLE[0])))

/* Baut einen 256-Byte-Kopf mit Medienbyte und Kennung, danach `daten`
 * Byte Nutzlast — fuer die Gegenproben, die kein Korpus braucht. */
static int baue(const char *pfad, uint8_t media, size_t daten)
{
    uint8_t *b = (uint8_t *)calloc(1, HDR + daten + 1);
    FILE *f;
    int ok;
    if (!b) return 0;
    b[0] = media;
    memcpy(b + 0xAB, "DIFC HEADER  ", 13);
    if (daten) memset(b + HDR, 0x5A, daten);
    f = fopen(pfad, "wb");
    if (!f) { free(b); return 0; }
    ok = (fwrite(b, 1, HDR + daten, f) == HDR + daten);
    fclose(f);
    free(b);
    return ok;
}

static uint8_t *lies(const char *pfad, size_t *n)
{
    FILE *f = fopen(pfad, "rb");
    uint8_t *b;
    long gr;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); gr = ftell(f); fseek(f, 0, SEEK_SET);
    if (gr <= 0) { fclose(f); return NULL; }
    b = (uint8_t *)malloc((size_t)gr);
    if (!b) { fclose(f); return NULL; }
    *n = fread(b, 1, (size_t)gr, f);
    fclose(f);
    return b;
}

int main(void)
{
    const uft_format_plugin_t *p = &uft_format_plugin_dim;
    char pfad[600], hilf[600], d1[260];
    const char *tmp = getenv("TEMP");
    int i;

    if (!tmp) tmp = getenv("TMPDIR");
    if (!tmp) tmp = ".";

    printf("=== DIM: die aufgeloeste Medientabelle (MF-1037) ===\n");

    /* ── 1..4 Die vier Medienbytes, jeder Sektor gegen seinen Namen ─── */
    for (i = 0; i < NFAELLE; i++) {
        const fall_t *f = &FAELLE[i];
        uft_disk_t disk;
        uft_error_t e;
        int geo_ok = 0, treffer = 0, fehl = 0;
        char erster[80];

        erster[0] = 0;
        snprintf(pfad, sizeof(pfad), "%s/%s", UFT_CORPUS_DIR, f->datei);
        memset(&disk, 0, sizeof(disk));
        e = p->open(&disk, pfad, true);
        snprintf(hilf, sizeof(hilf), "Medienbyte 0x%02X wird geoeffnet und "
                 "liefert %d x %d x %d x %d", f->media, f->zyl, f->koepfe,
                 f->spt, f->ss);
        if (e != UFT_OK) {
            snprintf(d1, sizeof(d1), "open=%d (Pruefdatei fehlt oder wird "
                     "abgewiesen)", (int)e);
            pruefe(hilf, 0, d1);
            continue;
        }
        geo_ok = (disk.geometry.cylinders == f->zyl
                  && disk.geometry.heads == f->koepfe
                  && disk.geometry.sectors == f->spt
                  && disk.geometry.sector_size == f->ss);
        snprintf(d1, sizeof(d1), "gemeldet %u x %u x %u x %u",
                 disk.geometry.cylinders, disk.geometry.heads,
                 disk.geometry.sectors, disk.geometry.sector_size);
        pruefe(hilf, geo_ok, d1);

        /* Jeder Sektor nennt sich selbst. Ein falscher Versatz liefert
         * damit nicht „irgendwas", sondern den NAMEN des Nachbarn. */
        if (geo_ok) {
            int c, h, s;
            for (c = 0; c < f->zyl; c++) {
                for (h = 0; h < f->koepfe; h++) {
                    uft_track_t t;
                    memset(&t, 0, sizeof(t));
                    if (p->read_track(&disk, c, h, &t) != UFT_OK
                        || (int)t.sector_count != f->spt) {
                        fehl += f->spt;
                        if (!erster[0])
                            snprintf(erster, sizeof(erster),
                                     "C%d H%d nicht lesbar", c, h);
                        uft_track_release(&t);
                        continue;
                    }
                    for (s = 0; s < f->spt; s++) {
                        char soll[24], ist[24];
                        snprintf(soll, sizeof(soll), "UFT-K C%02d H%d S%02d ",
                                 c, h, s + 1);
                        if (!t.sectors[s].data
                            || t.sectors[s].data_len != (size_t)f->ss) {
                            fehl++;
                            continue;
                        }
                        memcpy(ist, t.sectors[s].data, 17);
                        ist[17] = 0;
                        if (strcmp(ist, soll) == 0) treffer++;
                        else {
                            fehl++;
                            if (!erster[0])
                                snprintf(erster, sizeof(erster),
                                         "C%d H%d S%d: \"%s\" statt \"%s\"",
                                         c, h, s + 1, ist, soll);
                        }
                    }
                    uft_track_release(&t);
                }
            }
            snprintf(d1, sizeof(d1), "%d getroffen, %d daneben%s%s",
                     treffer, fehl, erster[0] ? " — " : "", erster);
            snprintf(hilf, sizeof(hilf), "0x%02X: alle %d Sektoren nennen "
                     "sich selbst (hxcfes IMD-Zerlegung stimmt byteweise "
                     "ueberein)", f->media, f->zyl * f->koepfe * f->spt);
            pruefe(hilf, fehl == 0 && treffer == f->zyl * f->koepfe * f->spt,
                   d1);
        }
        p->close(&disk);
    }

    /* ── 5. Die Sektor-IDs sind 1-basiert ───────────────────────────── */
    {
        uft_disk_t disk;
        uft_track_t t;
        int ok = 0;
        snprintf(pfad, sizeof(pfad), "%s/%s", UFT_CORPUS_DIR,
                 FAELLE[2].datei);
        memset(&disk, 0, sizeof(disk));
        if (p->open(&disk, pfad, true) == UFT_OK) {
            memset(&t, 0, sizeof(t));
            if (p->read_track(&disk, 5, 1, &t) == UFT_OK
                && t.sector_count == 15) {
                ok = (t.sectors[0].id.sector == 1
                      && t.sectors[14].id.sector == 15);
                snprintf(d1, sizeof(d1), "erster %u, letzter %u",
                         t.sectors[0].id.sector, t.sectors[14].id.sector);
            } else snprintf(d1, sizeof(d1), "Spur nicht lesbar");
            uft_track_release(&t);
            p->close(&disk);
        } else snprintf(d1, sizeof(d1), "open scheiterte");
        pruefe("die Sektornummern laufen 1..spt — so nennt sie hxcfes IMD "
               "in ihrer eigenen Nummernkarte", ok, d1);
    }

    /* ── 6. Gegenprobe: die drei DCP-Medienbytes werden abgewiesen ────
     *
     * Vorher lieferten sie eine Geometrie: 0x09 → 80x2x18x512,
     * 0x11 → 80x2x8x512, 0x19 → 80x2x9x512. Fuer DIM gibt es dafuer
     * keinen Beleg — hxcfes Lader weist sie gemessen ab, und seine
     * Beschreibung fuehrt 0x11/0x19 unter DCP. */
    {
        static const uint8_t dcp[] = { 0x09, 0x11, 0x19 };
        int alle = 1;
        size_t k;
        for (k = 0; k < sizeof(dcp); k++) {
            uft_disk_t disk;
            uft_error_t e;
            int conf = -1;
            uint8_t kopf[HDR];
            bool ja;
            memset(kopf, 0, sizeof(kopf));
            kopf[0] = dcp[k];
            memcpy(kopf + 0xAB, "DIFC HEADER  ", 13);
            ja = p->probe(kopf, sizeof(kopf), 2u * 1024 * 1024, &conf);
            snprintf(hilf, sizeof(hilf), "%s/uft_mf1037_%02X.dim", tmp,
                     dcp[k]);
            baue(hilf, dcp[k], 2u * 1024 * 1024 - HDR);
            memset(&disk, 0, sizeof(disk));
            e = p->open(&disk, hilf, true);
            if (ja || e == UFT_OK) {
                printf("       0x%02X: probe=%d, open=%d\n", dcp[k],
                       (int)ja, (int)e);
                alle = 0;
            }
            if (e == UFT_OK) p->close(&disk);
            remove(hilf);
        }
        pruefe("Gegenprobe: 0x09, 0x11 und 0x19 werden abgewiesen — das "
               "sind DCP-Medienbytes, eine andere Nummerierung", alle,
               NULL);
    }

    /* ── 7. Gegenprobe: die ALTE Geometrie fuer 0x01 wird abgewiesen ──
     *
     * Diese Datei traegt Medienbyte 0x01 und genau die Nutzlast, die der
     * Vorzustand verlangte (77 x 2 x 8 x 1024). Nach der aufgeloesten
     * Tabelle braucht 0x01 aber 1 474 560 Byte — die Datei ist also zu
     * kurz und muss fallen. Ohne diese Zusage liesse sich nicht
     * unterscheiden, ob die neue Tabelle wirklich greift. */
    {
        uft_disk_t disk;
        uft_error_t e;
        snprintf(hilf, sizeof(hilf), "%s/uft_mf1037_alt01.dim", tmp);
        baue(hilf, 0x01, 77u * 2 * 8 * 1024);
        memset(&disk, 0, sizeof(disk));
        e = p->open(&disk, hilf, true);
        snprintf(d1, sizeof(d1), "open=%d bei %u Byte Nutzlast", (int)e,
                 77u * 2 * 8 * 1024);
        pruefe("Gegenprobe: eine 0x01-Datei mit der ALTEN Nutzlast "
               "(77x2x8x1024) wird abgewiesen — 0x01 braucht 1 474 560 "
               "Byte", e != UFT_OK, d1);
        if (e == UFT_OK) p->close(&disk);
        remove(hilf);
    }

    /* ── 8. Gegenprobe: die Sonde nimmt die DATEIgroesse ─────────────
     *
     * Die Falle aus MF-1029, in diesem Baum sechsmal gefunden: ein
     * Plugin verwirft `file_size` und vergleicht gegen die
     * Puffergroesse. Bei DIM ist die Groesse nach der Kennung das
     * zweite Merkmal. */
    {
        size_t n = 0;
        uint8_t *b;
        snprintf(pfad, sizeof(pfad), "%s/%s", UFT_CORPUS_DIR,
                 FAELLE[0].datei);
        b = lies(pfad, &n);
        if (b && n >= HDR) {
            int c1 = -1, c2 = -1;
            bool mit = p->probe(b, HDR, n, &c1);
            bool ohne = p->probe(b, HDR, HDR, &c2);
            snprintf(d1, sizeof(d1), "mit %zu Byte Dateigroesse: %d (%d), "
                     "mit 256: %d (%d)", n, (int)mit, c1, (int)ohne, c2);
            pruefe("Gegenprobe: derselbe Puffer wird abgewiesen, wenn die "
                   "DATEIgroesse zu klein ist — die Sonde nimmt nicht die "
                   "Puffergroesse (MF-1029)", mit && !ohne, d1);
        } else {
            pruefe("Gegenprobe: derselbe Puffer wird abgewiesen, wenn die "
                   "DATEIgroesse zu klein ist", 0, "Pruefdatei fehlt");
        }
        free(b);
    }

    /* ── 9. Durchschreibprobe: die Schreibseite trifft dieselbe Stelle
     *
     * `write_track` rechnet den Versatz selbst. Eine zweite Rechnung ist
     * eine zweite Fehlerquelle (MF-519/529: in diesem Baum ist die
     * Schreibseite systematisch die aeltere). Hier wird eine Kopie
     * geoeffnet, EINE Spur mit einem Erkennungsmuster ueberschrieben,
     * geschlossen, neu geoeffnet — und gelesen. */
    {
        size_t n = 0;
        uint8_t *b;
        int ok = 0;
        int kopie_da = 0;
        snprintf(pfad, sizeof(pfad), "%s/%s", UFT_CORPUS_DIR,
                 FAELLE[2].datei);
        b = lies(pfad, &n);
        snprintf(hilf, sizeof(hilf), "%s/uft_mf1037_schreib.dim", tmp);
        snprintf(d1, sizeof(d1), "Pruefdatei fehlt");
        if (b) {
            FILE *f = fopen(hilf, "wb");
            if (f) {
                kopie_da = (fwrite(b, 1, n, f) == n);
                fclose(f);
            }
            free(b);
        }
        if (kopie_da) {
            uft_disk_t disk;
            uft_track_t t;
            memset(&disk, 0, sizeof(disk));
            snprintf(d1, sizeof(d1), "open (schreibbar) scheiterte");
            if (p->open(&disk, hilf, false) == UFT_OK) {
                memset(&t, 0, sizeof(t));
                if (p->read_track(&disk, 79, 1, &t) == UFT_OK
                    && t.sector_count == 15) {
                    memcpy(t.sectors[3].data, "UFT-SCHREIBPROBE ", 17);
                    if (p->write_track(&disk, 79, 1, &t) == UFT_OK) {
                        uft_disk_t d2;
                        uft_track_t t2;
                        uft_track_release(&t);
                        p->close(&disk);
                        memset(&d2, 0, sizeof(d2));
                        snprintf(d1, sizeof(d1), "Neuoeffnen scheiterte");
                        if (p->open(&d2, hilf, true) == UFT_OK) {
                            memset(&t2, 0, sizeof(t2));
                            if (p->read_track(&d2, 79, 1, &t2) == UFT_OK
                                && t2.sector_count == 15) {
                                ok = (memcmp(t2.sectors[3].data,
                                             "UFT-SCHREIBPROBE ", 17) == 0
                                      && memcmp(t2.sectors[4].data,
                                                "UFT-K C79 H1 S05 ", 17) == 0);
                                snprintf(d1, sizeof(d1),
                                         "S04 traegt \"%.17s\", S05 "
                                         "unveraendert \"%.17s\"",
                                         (const char *)t2.sectors[3].data,
                                         (const char *)t2.sectors[4].data);
                            }
                            uft_track_release(&t2);
                            p->close(&d2);
                        }
                    } else {
                        snprintf(d1, sizeof(d1), "write_track scheiterte");
                        uft_track_release(&t);
                        p->close(&disk);
                    }
                } else {
                    snprintf(d1, sizeof(d1), "Spur 79/1 nicht lesbar");
                    uft_track_release(&t);
                    p->close(&disk);
                }
            }
        }
        pruefe("Durchschreibprobe: die aeusserste Spur der zweiten Seite "
               "(79/1) wird geschrieben und nach Schliessen und Neuoeffnen "
               "zurueckgelesen — die Nachbarsektoren bleiben unberuehrt",
               ok, d1);
        remove(hilf);
    }

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
