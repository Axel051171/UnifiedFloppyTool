/**
 * @file test_gcr_liest_echte_spur.c
 * @brief Die GCR-Zerlegung an einer echten Aufnahme, gegen die
 *        Pruefsummen auf der Diskette (MF-1013).
 *
 * ── Woher die Frage kommt ────────────────────────────────────────────
 *
 * **P3-319:** zwei der 30 Funktionen in `src/formats/c64/uft_gcr_ops.c`
 * waren Fassaden.
 *
 *     gcr_extract_sector():
 *         /* This is a simplified implementation *\/
 *         memset(output, 0, SECTOR_SIZE);
 *         return 0;
 *
 * Der Aufrufer bekam 256 Nullbytes **und Erfolg** — erfundene Daten,
 * nicht bloss fehlende.
 *
 *     gcr_verify_track(..., const uint8_t *disk_id, ...):
 *         /* Simplified header parsing - full version would decode GCR *\/
 *         result->sectors[sector_idx].header_ok = true;
 *         result->sectors_good++;
 *
 * `header_ok = true` bedingungslos, sobald die obere Nibble wie `0x50`
 * aussah. Und `disk_id` wurde **nie benutzt** — eine Signatur, die eine
 * Pruefung zusagte, die nicht stattfand.
 *
 * Klasse **MF-864** (`flux_decode_fm()`, dessen Sektorteil aus zwei
 * Kommentaren bestand); hier schaerfer, weil Daten geliefert statt
 * weggelassen wurden.
 *
 * ── Warum das ohne fremdes Werkzeug beweisbar ist ───────────────────
 *
 * Die Pruefsummen stehen auf der **Diskette**, nicht in unserem Code:
 * der Kopfblock traegt `Spur ^ Sektor ^ ID1 ^ ID2`, der Datenblock das
 * XOR ueber seine 256 Byte. Geht beides an einer echten Aufnahme auf,
 * ist das eine Aussage ueber die Wirklichkeit — derselbe Weg wie
 * **MF-869** beim FM-Pfad („Die CRCs stehen auf der Diskette selbst,
 * 1979 geschrieben").
 *
 * Ein Rundlauf gegen unser eigenes `gcr_encode()` waere dagegen die
 * Selbstbestaetigung aus **MF-992** — er wuerde auch bei einer falschen
 * Zerlegung aufgehen, solange sie zur eigenen Kodierung passt.
 *
 * ── Die Aufnahme, und was das Manifest darueber sagt ────────────────
 *
 * `tests/corpus_free/vice_c1541_35trk.g64`, laut
 * `tests/corpus_manifest/manifest.json`:
 *
 *   tool    VICE 3.10 c1541 (release 3.10.0)
 *   source  c1541 -format "uftg64,42" g64 <img> ...
 *   origin  cross-tool
 *
 * Die Disk-ID ist damit **„42"** — sie steht im Erzeugungsbefehl, nicht
 * in unserem Code. Spur 18 (Halbspur 36) traegt 19 Sektoren.
 *
 * **Ausdruecklich NICHT verglichen** wird mit
 * `vice_c1541_35trk.d64`: das ist eine **andere Diskette**
 * (`-format "uftcorpus,42"`). MF-929 hat genau diese Verwechslung
 * einmal drei Runden gekostet, und P3-320 haelt sie fest.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "uft/formats/c64/uft_gcr_ops.h"
#include "uft/formats/c64/uft_d64_g64.h"

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

#define PFAD        "tests/corpus_free/vice_c1541_35trk.g64"
#define SPUR        18
/* G64 fuehrt 84 Halbspur-Eintraege, Index 0 = Spur 1.0. Spur t liegt
 * damit bei (t-1)*2 — Spur 18 also bei 34.
 *
 * MF-1013: meine erste Fassung nahm `SPUR * 2` = 36 und las damit
 * **Spur 19**. Vier Zusagen fielen, und alle vier lagen am Index, nicht
 * am Code: der dekodierte Kopf sagte `Spur 0x13` = 19, und Spur 19 ist
 * auf einer frisch formatierten Diskette leer — daher auch die 256
 * Nullbytes, die ich fuer die alte Fassung gehalten habe. Der Kopf
 * wurde die ganze Zeit richtig zerlegt. */
#define HALBSPUR    ((SPUR - 1) * 2)
#define SEKTOREN    19              /* Spur 18 liegt in Zone 2 */

int main(void)
{
    printf("=== GCR liest eine echte Spur (MF-1013) ===\n");

    FILE *f = fopen(PFAD, "rb");
    if (!f) {
        /* Ohne Korpus benannt ueberspringen — nicht schweigend gruen. */
        printf("  [SKIP] %s fehlt (Korpus nicht vorhanden)\n", PFAD);
        printf("\n%d gruen, %d rot (uebersprungen)\n", gruen, rot);
        return 0;
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *roh = (uint8_t *)malloc((size_t)n);
    int gelesen = (roh && fread(roh, 1, (size_t)n, f) == (size_t)n);
    fclose(f);
    pruefe("die Aufnahme laesst sich lesen", gelesen, PFAD);
    if (!gelesen) { free(roh); printf("\n%d gruen, %d rot\n", gruen, rot); return 1; }

    g64_image_t *img = NULL;
    int rc = g64_load_buffer(roh, (size_t)n, &img);
    char h[220];
    snprintf(h, sizeof(h), "g64_load_buffer lieferte %d", rc);
    pruefe("das G64 laesst sich zerlegen", rc == 0 && img != NULL, h);
    if (rc != 0 || !img) { free(roh); printf("\n%d gruen, %d rot\n", gruen, rot); return 1; }

    const uint8_t *gcr = NULL;
    size_t len = 0;
    uint8_t speed = 0xFF;
    rc = g64_get_track(img, HALBSPUR, &gcr, &len, &speed);
    snprintf(h, sizeof(h), "Halbspur %d: rc=%d, %zu Byte, Zone %u",
             HALBSPUR, rc, len, (unsigned)speed);
    pruefe("Spur 18 liegt vor", rc == 0 && gcr != NULL && len > 6000, h);
    if (rc != 0 || !gcr) { g64_free(img); free(roh);
                           printf("\n%d gruen, %d rot\n", gruen, rot); return 1; }

    /* ── Die Diskette prueft sich selbst ──────────────────────────── */
    /* MF-1013, und das ist der eigentliche Fund: die **Header**-ID
     * dieser Aufnahme ist `A0 A0`, nicht `42`.
     *
     * `c1541 -format "uftg64,42"` schreibt die `42` in die BAM (die
     * „Cosmetic"-ID bei 0xA2/0xA3), laesst aber die ID in den
     * SEKTORKOEPFEN auf dem CBM-Fuellzeichen `0xA0` stehen. Die
     * Diskette traegt damit **zwei verschiedene IDs** — genau die
     * Unterscheidung, die nibtools mit `extract_id()` und
     * `extract_cosmetic_id()` trennt und die UFT bisher nicht kannte
     * (P3-320).
     *
     * **Zwei unabhaengige Umsetzungen stimmen ueberein:** `nibscan`
     * meldet fuer dieselbe Datei „Header Disk ID:" mit zwei
     * unleserlichen Bytes (das sind die `0xA0`) und „Cosmetic Disk ID:
     * 42". Damit ist der Wert erklaert, den P3-320 als unerklaert
     * festgehalten hat — er ist echt.
     *
     * Der Wert steht hier ausgeschrieben und nicht aus dem Code
     * geholt (MF-913). */
    const uint8_t id[2] = { 0xA0, 0xA0 };

    gcr_verify_result_t vr;
    int fehler = gcr_verify_track(gcr, len, SPUR, id, &vr);

    snprintf(h, sizeof(h),
             "gefunden %d, gut %d, Kopffehler %d, Datenfehler %d, "
             "GCR-Fehler %d (Rueckgabe %d)",
             vr.sectors_found, vr.sectors_good, vr.header_errors,
             vr.data_errors, vr.gcr_errors, fehler);
    pruefe("Spur 18 traegt 19 Sektoren", vr.sectors_found == SEKTOREN, h);
    pruefe("und ALLE Pruefsummen der Diskette gehen auf",
           vr.sectors_good == SEKTOREN && vr.header_errors == 0
           && vr.data_errors == 0, h);

    /* MF-1013: `gcr_errors` kam vorher aus `gcr_check_errors()`, das
     * JEDEN Byteversatz auf gueltige 5-Bit-Codes prueft — auf genau
     * dieser Spur meldete es **5212 „Fehler"**, obwohl saemtliche
     * Pruefsummen aufgehen. Jetzt zaehlt es die Dekodierfehler in den
     * echten Bloecken, und die Rueckgabe (Summe aller drei Zaehler)
     * darf damit erst 0 werden. */
    pruefe("und die Fehlerzahl ist 0, nicht 5212",
           vr.gcr_errors == 0 && fehler == 0, h);

    /* Die Sektornummern muessen 0..18 sein, jede genau einmal. */
    {
        int gesehen[21];
        memset(gesehen, 0, sizeof(gesehen));
        int doppelt = 0, ausserhalb = 0;
        for (int i = 0; i < vr.sectors_found; i++) {
            int s = vr.sectors[i].sector_in_header;
            if (s < 0 || s >= SEKTOREN) { ausserhalb++; continue; }
            if (gesehen[s]++) doppelt++;
        }
        int vollstaendig = 1;
        for (int s = 0; s < SEKTOREN; s++) if (gesehen[s] != 1) vollstaendig = 0;
        snprintf(h, sizeof(h), "%d doppelt, %d ausserhalb 0..%d",
                 doppelt, ausserhalb, SEKTOREN - 1);
        pruefe("die Sektornummern sind 0..18, jede genau einmal",
               vollstaendig && doppelt == 0 && ausserhalb == 0, h);
    }

    /* Die ID aus dem Kopf muss die aus dem Erzeugungsbefehl sein. */
    {
        int passt = (vr.sectors_found > 0
                     && vr.sectors[0].id[0] == 0xA0
                     && vr.sectors[0].id[1] == 0xA0);
        snprintf(h, sizeof(h),
                 "Kopf-ID 0x%02X 0x%02X, erwartet 0xA0 0xA0 -- die '42' "
                 "aus dem Erzeugungsbefehl steht nur in der BAM",
                 vr.sectors_found ? (unsigned)vr.sectors[0].id[0] : 0u,
                 vr.sectors_found ? (unsigned)vr.sectors[0].id[1] : 0u);
        pruefe("die Disk-ID im Sektorkopf ist 0xA0 0xA0 (nicht '42')",
               passt, h);
    }

    /* ── gcr_extract_sector: echte Daten statt Nullen ─────────────── */
    {
        uint8_t sek[256];
        memset(sek, 0xCC, sizeof(sek));
        gcr_sector_verify_t v;
        int r = gcr_extract_sector(gcr, len, 0, sek, &v);
        snprintf(h, sizeof(h),
                 "Rueckgabe %d, header_ok=%d data_ok=%d", r,
                 (int)v.header_ok, (int)v.data_ok);
        pruefe("Sektor 0 laesst sich herausloesen", r == 0, h);

        /* GEGENPROBE gegen die alte Fassung: die lieferte 256 NULLEN.
         * Spur 18 Sektor 0 ist die BAM — sie ist niemals leer. */
        int alles_null = 1;
        for (size_t i = 0; i < sizeof(sek); i++)
            if (sek[i] != 0x00) { alles_null = 0; break; }
        pruefe("und liefert NICHT 256 Nullbytes", !alles_null,
               "das ist genau die alte Fassung");

        /* Die BAM traegt an 0x02 die DOS-Fassung 'A' (0x41). */
        /* Die BAM, Byte fuer Byte gegen die CBM-DOS-Festlegung:
         *   0x00-0x01  Spur/Sektor des ersten Verzeichnisblocks = 18/1
         *   0x02       DOS-Fassung 'A' (0x41)
         *   0x04-0x07  BAM-Eintrag Spur 1: 21 frei, dann die Bitmap
         *
         * Das ist dasselbe Bytemuster, das MF-992 an der VICE-D64
         * gemessen hat (`12 01 41 00 | 15 FF FF 1F`). */
        snprintf(h, sizeof(h),
                 "BAM-Anfang: %02X %02X %02X %02X | %02X %02X %02X",
                 (unsigned)sek[0], (unsigned)sek[1], (unsigned)sek[2],
                 (unsigned)sek[3], (unsigned)sek[4], (unsigned)sek[5],
                 (unsigned)sek[6]);
        pruefe("die BAM zeigt 18/1, DOS 'A' und 21 freie Bloecke auf Spur 1",
               sek[0] == 18 && sek[1] == 1 && sek[2] == 0x41
               && sek[4] == 21, h);
    }

    /* ── Gegenprobe: eine falsche ID MUSS auffallen ───────────────── */
    {
        const uint8_t falsch[2] = { 'X', 'Y' };
        gcr_verify_result_t vr2;
        gcr_verify_track(gcr, len, SPUR, falsch, &vr2);
        snprintf(h, sizeof(h),
                 "mit ID 'XY': %d Kopffehler von %d Sektoren",
                 vr2.header_errors, vr2.sectors_found);
        pruefe("eine falsche Disk-ID wird gemeldet",
               vr2.header_errors == vr2.sectors_found
               && vr2.sectors_found > 0, h);
    }

    /* ── Gegenprobe: eine falsche Spurnummer MUSS auffallen ───────── */
    {
        gcr_verify_result_t vr3;
        gcr_verify_track(gcr, len, SPUR + 1, id, &vr3);
        snprintf(h, sizeof(h),
                 "als Spur %d gelesen: %d Kopffehler von %d",
                 SPUR + 1, vr3.header_errors, vr3.sectors_found);
        pruefe("eine falsche Spurnummer wird gemeldet",
               vr3.header_errors == vr3.sectors_found
               && vr3.sectors_found > 0, h);
    }

    /* ── Gegenprobe: ein verfaelschtes Datenbyte MUSS auffallen ───── */
    {
        uint8_t *kopie = (uint8_t *)malloc(len);
        if (kopie) {
            memcpy(kopie, gcr, len);
            /* Mitten in die Spur greifen — irgendwo in einem Datenblock */
            kopie[len / 2] ^= 0xFF;

            gcr_verify_result_t vr4;
            gcr_verify_track(kopie, len, SPUR, id, &vr4);
            snprintf(h, sizeof(h),
                     "nach einem gekippten Byte: gut %d von %d, "
                     "Datenfehler %d, GCR-Fehler %d",
                     vr4.sectors_good, vr4.sectors_found,
                     vr4.data_errors, vr4.gcr_errors);
            pruefe("ein verfaelschtes Byte laesst die Pruefung fallen",
                   vr4.sectors_good < SEKTOREN, h);
            free(kopie);
        }
    }

    g64_free(img);
    free(roh);
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot == 0 ? 0 : 1;
}
