/**
 * @file test_zx_und_pc98_echte_abbilder.c
 * @brief mgt, opus, scl und fdi_pc98 an echten Abbildern (MF-1075)
 *
 * ── Wo die Abbilder herkommen, und warum sie hier nicht liegen ──────────
 *
 * Sie lagen die ganze Zeit im Baum — **in Archiven**. Die Regalsuche zu
 * MF-1073 hatte zuerst „null Treffer" gemeldet, und das war falsch:
 * gesucht wurde mit `find`, und `find` sieht nicht in `.zip` und
 * `.tar.gz` hinein. Nachgemessen ueber die Archivinhalte liegen **129**
 * Abbilder da, **zehn** davon in Formaten, die noch offen sind.
 *
 * Ihre Lizenz ist ungeklaert (P3-359), also werden sie **nicht
 * weitergegeben**: sie liegen unter `tests/corpus/`, das seit MF-364
 * gitignored ist, und nur ihre SHA-256 samt Herkunft steht im Manifest.
 * Ohne sie ueberspringt sich dieser Test benannt und behauptet nichts.
 *
 * ── Was sie belegen ─────────────────────────────────────────────────────
 *
 * MF-1074 hat gemessen, dass `mgt` und `opus` **ueber die Erkennung
 * unerreichbar** waren (`(void)file_size;` in beiden Wrappern). Der
 * Rotbeweis dort kommt aus erzeugten Puffern und genuegt fuer den
 * Defekt. Was er NICHT kann, ist zeigen, dass der Leser an einer echten
 * Diskette die richtigen Bytes an die richtige Stelle legt. Genau das
 * steht hier, und zwar **ueber den gelieferten Sektor**, nicht ueber die
 * Datei:
 *
 *     zxfd_plusd_gdos_tools.mgt : "CONFIG1_C" in Spur 0/0, Sektor-Nr 2
 *     scl_detroyt.scl           : "detroYT"   in Spur 0/0, Sektor-Nr 1
 *     zxfd_opus_dsdd.opd        : Sprungbefehl 0x18 in Sektor-Nr 0
 *
 * **Und die Gegenprobe sitzt im Korpus selbst:** `zxfd_plusd_clean.mgt`
 * ist dieselbe Groesse und dasselbe Format, nur frisch formatiert. Es
 * traegt keinen einzigen Dateinamen, und seine Sondenkonfidenz faellt
 * deshalb von **75** auf **40** — das Band „nur die Groesse" statt
 * „Struktur gelesen" (MF-729). Zwei Abbilder, ein Unterschied, und die
 * Sonde trifft ihn.
 *
 * Die Opus-Bootsektoren bestaetigen nebenbei die Vektoren, die MF-1074
 * aus der Beschreibung **erzeugt** hat: gemessen stehen dort
 * `18 05 50 12 50` (80 Zylinder, 18 Sektoren, zwei Koepfe, 256 Byte) und
 * `18 05 28 12 40` (40 Zylinder, ein Kopf) — Byte fuer Byte die Form,
 * die der erzeugte Puffer angenommen hatte.
 *
 * ── Was NICHT geprueft ist ──────────────────────────────────────────────
 *
 * * **Die Nutzdaten** der Dateien in den Abbildern; geprueft sind
 *   Geometrie, Sektorzahl und die Lage der Verzeichnis-/Katalogbytes.
 * * **`ipf`.** Die zwei echten SPS-Abbilder liegen daneben im selben
 *   Korpus, und UFT liest sie (164 Spuren, 900 822 Byte Bitstrom) — aber
 *   die Datei sagt 10 bis 11 Bloecke je Spur an, und geliefert werden
 *   **null Sektoren**. Das ist ein eigener Befund und gehoert nicht in
 *   diesen Test.
 * * **Die Schreibseite** aller vier Formate.
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_track.h"   /* uft_track_release — implizite Deklaration
                              * ist auf macOS-Clang ein FEHLER */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifndef UFT_CORPUS_RESTRICTED_DIR
#define UFT_CORPUS_RESTRICTED_DIR ""
#endif

extern const uft_format_plugin_t uft_format_plugin_mgt;
extern const uft_format_plugin_t uft_format_plugin_opus;
extern const uft_format_plugin_t uft_format_plugin_scl;
extern const uft_format_plugin_t uft_format_plugin_fdi_pc98;

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int bedingung, const char *detail)
{
    if (bedingung) { gruen++; printf("  ok   %s\n", was); }
    else { rot++; printf("  FAIL %s - %s\n", was, detail ? detail : ""); }
}

/* Ergebnis eines vollstaendigen Durchlaufs. `wort_sektoren` zaehlt die
 * gelieferten SEKTOREN, in denen das gesuchte Wort steht — nicht die
 * Fundstellen in der Datei. Nur so sagt ein Treffer etwas ueber den
 * Leser statt ueber das Abbild. */
typedef struct {
    int      offen;
    int      probe_ok;
    int      konfidenz;
    unsigned zyl, koepfe, sekt, ssize;
    unsigned spuren, sektoren;
    unsigned wort_sektoren;
    int      erste_spur, erster_kopf, erste_nummer;
} lage_t;

static void lies(const uft_format_plugin_t *p, const char *pfad,
                 const char *wort, size_t wl, lage_t *aus)
{
    uft_disk_t disk;
    FILE *f;
    long gr;
    uint8_t kopf[8192];
    size_t gelesen;
    unsigned c, h;

    memset(aus, 0, sizeof *aus);
    aus->konfidenz = -1;
    aus->erste_spur = aus->erster_kopf = aus->erste_nummer = -1;

    f = fopen(pfad, "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END); gr = ftell(f); fseek(f, 0, SEEK_SET);
    gelesen = fread(kopf, 1, sizeof kopf, f);
    fclose(f);

    aus->probe_ok = p->probe(kopf, gelesen, (size_t)gr, &aus->konfidenz)
                    ? 1 : 0;

    memset(&disk, 0, sizeof disk);
    if (p->open(&disk, pfad, true) != UFT_OK) return;
    aus->offen = 1;
    aus->zyl    = (unsigned)disk.geometry.cylinders;
    aus->koepfe = (unsigned)disk.geometry.heads;
    aus->sekt   = (unsigned)disk.geometry.sectors;
    aus->ssize  = (unsigned)disk.geometry.sector_size;

    for (c = 0; c < aus->zyl; c++)
        for (h = 0; h < aus->koepfe; h++) {
            uft_track_t t;
            size_t k, i;
            memset(&t, 0, sizeof t);
            if (p->read_track(&disk, (int)c, (int)h, &t) != UFT_OK) continue;
            aus->spuren++;
            aus->sektoren += (unsigned)t.sector_count;
            for (k = 0; wort && wl && k < t.sector_count; k++) {
                const uft_sector_t *s = &t.sectors[k];
                size_t len = s->data_len ? s->data_len : s->data_size;
                if (!s->data || len < wl) continue;
                for (i = 0; i + wl <= len; i++)
                    if (memcmp(s->data + i, wort, wl) == 0) {
                        if (aus->wort_sektoren == 0) {
                            aus->erste_spur   = (int)c;
                            aus->erster_kopf  = (int)h;
                            aus->erste_nummer = (int)s->id.sector;
                        }
                        aus->wort_sektoren++;
                        break;
                    }
            }
            uft_track_release(&t);
        }
    p->close(&disk);
}

static int vorhanden(const char *pfad)
{
    FILE *f = fopen(pfad, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

int main(void)
{
    char pfad[600], det[300];
    lage_t l;
    /* Die zwei Opus-Bootsektoren, wie sie GEMESSEN in den Abbildern
     * stehen: JR, Versatz, Zylinder, Sektoren, Flags. */
    static const uint8_t OPUS_DS[5] = { 0x18, 0x05, 0x50, 0x12, 0x50 };
    static const uint8_t OPUS_SS[5] = { 0x18, 0x05, 0x28, 0x12, 0x40 };

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("ZX-Spectrum und PC-98 an echten Abbildern - MF-1075\n");
    printf("===================================================\n");

    if (UFT_CORPUS_RESTRICTED_DIR[0] == '\0') {
        printf("SKIP: UFT_CORPUS_RESTRICTED_DIR nicht gesetzt.\n");
        return 77;      /* SKIP_RETURN_CODE, seit MF-598 */
    }
    snprintf(pfad, sizeof pfad, "%s/zxfd_plusd_gdos_tools.mgt",
             UFT_CORPUS_RESTRICTED_DIR);
    if (!vorhanden(pfad)) {
        printf("SKIP: %s fehlt (eingeschraenkter Korpus, P3-359).\n", pfad);
        return 77;
    }

    /* ── 1. MGT mit Dateien ───────────────────────────────────────── */
    lies(&uft_format_plugin_mgt, pfad, "CONFIG1_C", 9, &l);
    snprintf(det, sizeof det,
             "probe=%d(%d) geo %ux%ux%ux%u %u Spuren %u Sektoren, Wort in "
             "%u Sektoren, zuerst %d/%d Nr %d", l.probe_ok, l.konfidenz,
             l.zyl, l.koepfe, l.sekt, l.ssize, l.spuren, l.sektoren,
             l.wort_sektoren, l.erste_spur, l.erster_kopf, l.erste_nummer);
    pruefe("mgt: das +D-Abbild mit Dateien liest 160 Spuren zu 10 Sektoren "
           "a 512, und der Verzeichnisname \"CONFIG1_C\" kommt IM "
           "gelieferten Sektor 2 der Spur 0/0 zurueck",
           l.offen && l.probe_ok && l.zyl == 80 && l.koepfe == 2
           && l.sekt == 10 && l.ssize == 512
           && l.spuren == 160 && l.sektoren == 1600
           && l.wort_sektoren >= 1
           && l.erste_spur == 0 && l.erster_kopf == 0
           && l.erste_nummer == 2, det);
    pruefe("mgt: und die Sonde meldet das Band \"Struktur gelesen\" "
           "(50-79, MF-729), weil ein benutzter Eintrag mit gueltigem "
           "Namen da ist",
           l.probe_ok && l.konfidenz >= 50 && l.konfidenz < 80, det);

    /* ── 2. MGT frisch formatiert: dieselbe Groesse, kein Inhalt ──── */
    snprintf(pfad, sizeof pfad, "%s/zxfd_plusd_clean.mgt",
             UFT_CORPUS_RESTRICTED_DIR);
    lies(&uft_format_plugin_mgt, pfad, "CONFIG1_C", 9, &l);
    snprintf(det, sizeof det,
             "probe=%d(%d) %u Spuren %u Sektoren, Wort in %u Sektoren",
             l.probe_ok, l.konfidenz, l.spuren, l.sektoren, l.wort_sektoren);
    pruefe("mgt: das frisch formatierte Abbild derselben Groesse wird "
           "ebenfalls angenommen, faellt aber ins Band \"nur die Groesse\" "
           "(< 50) und traegt keinen Dateinamen - die Gegenprobe zur "
           "Zusage darueber",
           l.offen && l.probe_ok && l.konfidenz > 0 && l.konfidenz < 50
           && l.spuren == 160 && l.sektoren == 1600
           && l.wort_sektoren == 0, det);

    /* ── 3. Opus Discovery, zweiseitig ────────────────────────────── */
    snprintf(pfad, sizeof pfad, "%s/zxfd_opus_dsdd.opd",
             UFT_CORPUS_RESTRICTED_DIR);
    lies(&uft_format_plugin_opus, pfad, (const char *)OPUS_DS, 5, &l);
    snprintf(det, sizeof det,
             "probe=%d(%d) geo %ux%ux%ux%u %u Spuren %u Sektoren, Bootsektor "
             "in %u Sektoren, zuerst %d/%d Nr %d", l.probe_ok, l.konfidenz,
             l.zyl, l.koepfe, l.sekt, l.ssize, l.spuren, l.sektoren,
             l.wort_sektoren, l.erste_spur, l.erster_kopf, l.erste_nummer);
    pruefe("opus: das zweiseitige Discovery-Abbild liest 160 Spuren zu 18 "
           "Sektoren a 256, und der Bootsektor `18 05 50 12 50` - 80 "
           "Zylinder, 18 Sektoren, zwei Koepfe - kommt als Sektor 0 der "
           "Spur 0/0 zurueck",
           l.offen && l.probe_ok && l.zyl == 80 && l.koepfe == 2
           && l.sekt == 18 && l.ssize == 256
           && l.spuren == 160 && l.sektoren == 2880
           && l.wort_sektoren == 1
           && l.erste_spur == 0 && l.erster_kopf == 0
           && l.erste_nummer == 0, det);

    /* ── 4. Opus Discovery, einseitig ─────────────────────────────── */
    snprintf(pfad, sizeof pfad, "%s/zxfd_opus_sssd.opd",
             UFT_CORPUS_RESTRICTED_DIR);
    lies(&uft_format_plugin_opus, pfad, (const char *)OPUS_SS, 5, &l);
    snprintf(det, sizeof det,
             "probe=%d(%d) geo %ux%ux%ux%u %u Spuren %u Sektoren, Bootsektor "
             "in %u Sektoren", l.probe_ok, l.konfidenz, l.zyl, l.koepfe,
             l.sekt, l.ssize, l.spuren, l.sektoren, l.wort_sektoren);
    pruefe("opus: das einseitige Abbild liest 40 Spuren zu 18 Sektoren, "
           "und seine Kopfzahl kommt AUS DEM BOOTSEKTOR (Flagbyte 0x40 "
           "statt 0x50), nicht aus einer Annahme",
           l.offen && l.probe_ok && l.zyl == 40 && l.koepfe == 1
           && l.sekt == 18 && l.ssize == 256
           && l.spuren == 40 && l.sektoren == 720
           && l.wort_sektoren == 1, det);

    /* ── 5. SCL: ein Archiv wird zur TR-DOS-Diskette ──────────────── */
    snprintf(pfad, sizeof pfad, "%s/scl_detroyt.scl",
             UFT_CORPUS_RESTRICTED_DIR);
    lies(&uft_format_plugin_scl, pfad, "detroYT", 7, &l);
    snprintf(det, sizeof det,
             "probe=%d(%d) geo %ux%ux%ux%u %u Spuren %u Sektoren, Name in "
             "%u Sektoren, zuerst %d/%d Nr %d", l.probe_ok, l.konfidenz,
             l.zyl, l.koepfe, l.sekt, l.ssize, l.spuren, l.sektoren,
             l.wort_sektoren, l.erste_spur, l.erster_kopf, l.erste_nummer);
    pruefe("scl: das Archiv wird als 80x2x16x256-Diskette ausgelegt, und "
           "der Katalogname \"detroYT\" steht IM gelieferten Sektor 1 der "
           "Spur 0/0 - dort, wo TR-DOS sein Verzeichnis fuehrt (MF-1014)",
           l.offen && l.probe_ok && l.zyl == 80 && l.koepfe == 2
           && l.sekt == 16 && l.ssize == 256
           && l.spuren == 160 && l.sektoren == 2560
           && l.wort_sektoren >= 1
           && l.erste_spur == 0 && l.erster_kopf == 0
           && l.erste_nummer == 1, det);

    /* ── 6. PC-98 FDI ─────────────────────────────────────────────── */
    snprintf(pfad, sizeof pfad, "%s/pc98_blank.fdi",
             UFT_CORPUS_RESTRICTED_DIR);
    lies(&uft_format_plugin_fdi_pc98, pfad, NULL, 0, &l);
    snprintf(det, sizeof det,
             "probe=%d(%d) geo %ux%ux%ux%u %u Spuren %u Sektoren",
             l.probe_ok, l.konfidenz, l.zyl, l.koepfe, l.sekt, l.ssize,
             l.spuren, l.sektoren);
    pruefe("fdi_pc98: das 1 265 664-Byte-Abbild liest 154 Spuren zu 8 "
           "Sektoren a 1024 - die Nutzlast 1 261 568 plus 4096 Byte Kopf, "
           "und die Geometrie kommt aus dem Kopf statt aus der Groesse",
           l.offen && l.probe_ok && l.zyl == 77 && l.koepfe == 2
           && l.sekt == 8 && l.ssize == 1024
           && l.spuren == 154 && l.sektoren == 1232, det);

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
