/**
 * @file test_cas_gegen_mame.c
 * @brief CAS (MSX-Kassette): dreimal ein stiller Verlust (MF-1040).
 *
 * ── Die Referenz ─────────────────────────────────────────────────────
 *
 * MAMEs `formats/fmsx_cas.cpp` (BSD-3-Clause, Sean Young) — **nur
 * gelesen**, Kanal *Spec* nach MF-695:
 *
 *     static const uint8_t CasHeader[8] =
 *         { 0x1F,0xA6,0xDE,0xBA,0xCC,0x13,0x7D,0x74 };
 *
 *     if (caslen < 8) return -1;
 *     if (memcmp (casdata, CasHeader, sizeof (CasHeader))) return -1;
 *
 * Die Datei muss mit dem Kopf **beginnen**; danach laeuft MAME
 * **byteweise** und beginnt ueberall dort einen neuen Block, wo der Kopf
 * wieder auftaucht. **Beides tut UFTs Leser genauso** — das ist gemessen
 * und stimmt. Die Befunde lagen woanders.
 *
 * ── Der Rotbeweis ────────────────────────────────────────────────────
 *
 * Gemessen am unveraenderten Produktionspfad:
 *
 *     300 Bloecke  : open = 0, **256** Sektoren, Summe 10 240 statt
 *                    12 352 Byte  -> 44 Bloecke und 2112 Byte still weg
 *     70 000 Byte  : open = 0, ein Sektor mit **65 535** Byte,
 *                    Status UFT_SECTOR_OK  -> 4465 Byte still weg
 *     8 Byte (nur
 *     der Kopf)    : open = 0, ein Sektor, data_len **0**, Status OK
 *
 * Die ersten beiden sind die Klasse aus MF-1004 („das Orakel bricht ab,
 * UFT kuerzte still"), der dritte die aus MF-1009 („Erfolg ohne Tat").
 *
 * **Der dritte ist dabei schwerer als er aussieht.** An einer Datei mit
 * einem vollen Block und einem Kopf am Dateiende, gegen den Vorzustand
 * uebersetzt: `open` = 0, zwei Sektoren (der zweite 0 Byte, Status OK)
 * — und der Prozess endet mit **0xC0000374**, `STATUS_HEAP_CORRUPTION`.
 * Ein Sektor der Laenge null, dessen Zeiger hinter den Puffer zeigt, hat
 * den Heap zerstoert. Welche Zuteilung genau, ist nicht bestimmt;
 * gemessen ist die Wirkung. Die achte Zusage unten haelt genau diesen
 * Fall fest.
 * Und `uft_format_add_sector()` setzt jeden Sektor unbedingt auf
 * `UFT_SECTOR_OK` (MF-980) — die gekuerzten Daten waren von echten nicht
 * zu unterscheiden.
 *
 * ── Eine Abweichung von MAME, benannt statt verschwiegen ─────────────
 *
 * MAMEs Bedingung ist `if ((pos + 8) < caslen)` — **streng kleiner**.
 * Ein Kopf, der genau am Dateiende endet, ist dort kein Kopf; MAME gibt
 * die acht Bytes als Banddaten aus. UFT erkennt ihn, findet dahinter
 * nichts und weist die Datei ab. Beides ist vertretbar; UFT entscheidet
 * sich gegen acht Bytes, die erkennbar eine Marke sind und keine Daten.
 *
 * ── Was dieser Test NICHT behauptet ──────────────────────────────────
 *
 * **T2, nicht T1b.** Die Pruefdateien sind hauseigen; MAMEs Lader wandelt
 * CAS in ein WAV-Abtastfeld und gibt keine Bloecke zurueck, taugt also
 * nicht als zerlegendes Orakel. Und die Abbildung „eine Spur, je Block
 * ein Sektor" ist **UFTs eigene Konvention** — wie die 128 x 512 bei
 * `fds` (MF-1038). Sie steht in keiner Quelle.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_format_plugin.h"
#include "uft/uft_track.h"
#include "uft/uft_types.h"

extern const uft_format_plugin_t uft_format_plugin_cas;

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

#define F_DREI  "cas_msx_drei.cas"
#define F_VIELE "cas_msx_300bloecke.cas"
#define F_GROSS "cas_msx_grossblock.cas"
#define F_KOPF  "cas_msx_nurkopf.cas"
#define F_LEER  "cas_msx_leerblock.cas"

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

/* Oeffnet eine Korpusdatei und liefert den Fehlerkode. */
static uft_error_t oeffne(const uft_format_plugin_t *p, const char *name,
                          uft_disk_t *disk)
{
    char pfad[600];
    snprintf(pfad, sizeof(pfad), "%s/%s", UFT_CORPUS_DIR, name);
    memset(disk, 0, sizeof(*disk));
    return p->open(disk, pfad, true);
}

int main(void)
{
    const uft_format_plugin_t *p = &uft_format_plugin_cas;
    char pfad[600], d1[300];
    uint8_t *drei = NULL;
    size_t ndrei = 0;

    printf("=== CAS gegen MAMEs fmsx_cas.cpp (MF-1040) ===\n");

    snprintf(pfad, sizeof(pfad), "%s/%s", UFT_CORPUS_DIR, F_DREI);
    drei = lies(pfad, &ndrei);
    if (!drei) {
        printf("  [SKIP] Pruefdatei fehlt im Korpus (%s)\n", F_DREI);
        return 77;
    }

    /* ── 1. Die Kennung ist ein getroffenes Merkmal ──────────────── */
    {
        int conf = -1;
        bool ja = p->probe(drei, ndrei, ndrei, &conf);
        snprintf(d1, sizeof(d1), "probe=%d, Konfidenz=%d", (int)ja, conf);
        pruefe("die acht Kopfbytes 1F A6 DE BA CC 13 7D 74 ergeben "
               "Konfidenz 95 (MF-729: Merkmal getroffen)",
               ja && conf == 95, d1);
    }

    /* ── 2. Drei Bloecke, jeder an der richtigen Stelle ──────────── */
    {
        uft_disk_t disk;
        uft_error_t e = oeffne(p, F_DREI, &disk);
        if (e != UFT_OK) {
            snprintf(d1, sizeof(d1), "open=%d", (int)e);
            pruefe("drei Bloecke werden als drei Sektoren gelesen", 0, d1);
        } else {
            uft_track_t t;
            int ok = 0;
            size_t summe = 0;
            memset(&t, 0, sizeof(t));
            if (p->read_track(&disk, 0, 0, &t) == UFT_OK
                && t.sector_count == 3) {
                size_t s;
                int namen = 1;
                static const uint8_t art[3] = { 0xD3, 0xD0, 0xEA };
                for (s = 0; s < 3; s++) {
                    char soll[24], ist[24];
                    summe += t.sectors[s].data_len;
                    if (!t.sectors[s].data) { namen = 0; continue; }
                    /* Block: 10 Byte Blockart, dann der Name. */
                    if (t.sectors[s].data[0] != art[s]) namen = 0;
                    snprintf(soll, sizeof(soll), "UFT-K BLOCK%03d ",
                             (int)s);
                    memcpy(ist, t.sectors[s].data + 10, 15);
                    ist[15] = 0;
                    if (strcmp(ist, soll) != 0) namen = 0;
                }
                /* 674 Byte Datei - 3 x 8 Byte Kopf = 650 Byte Nutzlast */
                ok = namen && summe == ndrei - 3u * 8u;
                snprintf(d1, sizeof(d1), "%zu Sektoren, Summe %zu Byte "
                         "(Soll %zu), Namen und Blockarten stimmen: %d",
                         (size_t)t.sector_count, summe,
                         ndrei - 24u, namen);
            } else {
                snprintf(d1, sizeof(d1), "%zu Sektoren statt 3",
                         (size_t)t.sector_count);
            }
            uft_track_release(&t);
            p->close(&disk);
            pruefe("drei Bloecke: jeder nennt sich selbst, jeder traegt "
                   "seine MSX-Blockart (0xD3/0xD0/0xEA), und die Summe "
                   "ist die Datei ohne die drei Koepfe", ok, d1);
        }
    }

    /* ── 3. Gegenprobe: mehr Bloecke als das Feld fasst ──────────── */
    {
        uft_disk_t disk;
        uft_error_t e = oeffne(p, F_VIELE, &disk);
        snprintf(d1, sizeof(d1), "open=%d", (int)e);
        pruefe("300 Bloecke werden ABGEWIESEN — vorher wurden es 256 "
               "Sektoren, und 44 Bloecke mit 2112 Byte fielen still weg",
               e != UFT_OK, d1);
        if (e == UFT_OK) p->close(&disk);
    }

    /* ── 4. Gegenprobe: ein Block, den kein Sektor fasst ─────────── */
    {
        uft_disk_t disk;
        uft_error_t e = oeffne(p, F_GROSS, &disk);
        snprintf(d1, sizeof(d1), "open=%d", (int)e);
        pruefe("ein Block mit 70 000 Byte wird ABGEWIESEN — vorher wurde "
               "er auf 65 535 gekuerzt und als UFT_SECTOR_OK gemeldet "
               "(4465 Byte weg)", e != UFT_OK, d1);
        if (e == UFT_OK) p->close(&disk);
    }

    /* ── 5. Gegenprobe: eine Datei aus nur acht Kopfbytes ────────── */
    {
        uft_disk_t disk;
        uft_error_t e;
        int conf = -1;
        bool ja;
        uint8_t *k;
        size_t nk = 0;
        snprintf(pfad, sizeof(pfad), "%s/%s", UFT_CORPUS_DIR, F_KOPF);
        k = lies(pfad, &nk);
        ja = k ? p->probe(k, nk, nk, &conf) : true;
        free(k);
        e = oeffne(p, F_KOPF, &disk);
        snprintf(d1, sizeof(d1), "probe=%d (%d) bei %zu Byte, open=%d",
                 (int)ja, conf, nk, (int)e);
        pruefe("eine Datei aus GENAU den acht Kopfbytes wird von BEIDEN "
               "abgewiesen — vorher gab es einen Sektor mit data_len 0 "
               "und Status OK (Erfolg ohne Tat, MF-1009)",
               !ja && e != UFT_OK, d1);
        if (e == UFT_OK) p->close(&disk);
    }

    /* ── 6. Gegenprobe: die Sonde nimmt die DATEIgroesse ─────────── */
    {
        int conf = -1;
        bool ja = p->probe(drei, ndrei, 8, &conf);
        snprintf(d1, sizeof(d1), "probe(..., Dateigroesse 8) = %d (%d)",
                 (int)ja, conf);
        pruefe("Gegenprobe: derselbe Puffer mit Dateigroesse 8 faellt — "
               "vorher stand dort `(void)file_size` (MF-1029)", !ja, d1);
    }

    /* ── 7. Gegenprobe: die Datei muss mit dem Kopf BEGINNEN ─────── */
    {
        uint8_t *k = (uint8_t *)malloc(ndrei + 1);
        int conf = -1;
        bool ja = true;
        if (k) {
            k[0] = 0x00;
            memcpy(k + 1, drei, ndrei);
            ja = p->probe(k, ndrei + 1, ndrei + 1, &conf);
            free(k);
        }
        snprintf(d1, sizeof(d1), "probe=%d (%d)", (int)ja, conf);
        pruefe("ein Byte VOR dem Kopf laesst die Datei fallen — MAME sagt "
               "`if (memcmp (casdata, CasHeader, 8)) return -1`", !ja, d1);
    }

    /* ── 8. Ein LEERER Block mitten in einer gueltigen Datei ──────
     *
     * Ohne diese Zusage laesst sich nicht unterscheiden, welche der
     * beiden Pruefungen wirkt: `open` weist eine Datei ohne jeden Inhalt
     * ab, und `read_track` ueberspringt leere Bloecke. Bei der
     * 8-Byte-Datei greift schon die erste, also sind sie dort
     * **gegenseitig redundant** — die Mutationsmatrix hat das gezeigt.
     * Diese Datei traegt einen vollen Block und danach einen Kopf am
     * Dateiende: `open` gelingt, und genau dann muss `read_track` den
     * leeren Block weglassen. */
    {
        uft_disk_t disk;
        uft_error_t e = oeffne(p, F_LEER, &disk);
        int ok = 0;
        if (e == UFT_OK) {
            uft_track_t t;
            memset(&t, 0, sizeof(t));
            if (p->read_track(&disk, 0, 0, &t) == UFT_OK) {
                ok = (t.sector_count == 1
                      && t.sectors[0].data_len == 200);
                snprintf(d1, sizeof(d1), "%zu Sektoren, erster %zu Byte "
                         "(Soll 1 und 200)", (size_t)t.sector_count,
                         t.sectors ? t.sectors[0].data_len : (size_t)0);
            } else snprintf(d1, sizeof(d1), "read_track scheitert");
            uft_track_release(&t);
            p->close(&disk);
        } else snprintf(d1, sizeof(d1), "open=%d", (int)e);
        pruefe("ein voller Block und dahinter ein Kopf am Dateiende: die "
               "Datei geht auf und liefert GENAU EINEN Sektor — der leere "
               "Block wird uebersprungen, nicht als 0-Byte-Sektor "
               "gemeldet", ok, d1);
    }

    free(drei);
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
