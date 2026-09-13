/**
 * @file test_sektor_laenge_stimmt.c
 * @brief Ein Sektor mit Bytes muss seine Laenge nennen (MF-1080)
 *
 * ── Der Befund ──────────────────────────────────────────────────────────
 *
 * `uft_sector_t` hat zwei Laengenfelder, und `include/uft/uft_types.h`
 * sagt selbst, welches gilt:
 *
 *     size_t   data_len;    ///< Datenlaenge in Bytes
 *     uint16_t data_size;   ///< Tatsaechliche Datengroesse (legacy)
 *
 * **Neun registrierte Plugins** bauten ihre Sektoren von Hand und setzten
 * nur das legacy-Feld: `apridisk`, `cfi`, `cpm`, `g64`, `hardsector`,
 * `logical`, `mgt`, `opus`, `posix` (und `rcpmfs`, dessen Zweig seit
 * MF-1035 unerreichbar ist). Gemessen am Vorzustand:
 *
 *     cpm  an einem cpmtools-Abbild : 2002 von 2002 Sektoren, Laenge 0
 *     mgt  an seinem Korpus-Abbild  : 1600 von 1600, Laenge 0
 *     opus an seinem Korpus-Abbild  : 2880 von 2880, Laenge 0
 *
 * Die Bytes waren dabei **richtig** — nur ihre Laenge war null.
 *
 * ── Warum das keine Kosmetik ist ────────────────────────────────────────
 *
 * Die meisten Kernleser tragen den Rueckfall `data_len ? data_len :
 * data_size` und merken deshalb nichts. **Zwei tragen ihn nicht**, und
 * beide laufen in Produktion:
 *
 *   * `uft_sector_copy()` kopiert nur `if (src->data && src->data_len >
 *     0)`. Gemessen: **rc = 0** — Erfolg — und ein Sektor mit
 *     `data == NULL`. Alle Bytes weg, Erfolg gemeldet. Gerufen von
 *     `uft_track_copy()`.
 *   * `uft_mfm_encode_from_track()` liest `(s->data && i < s->data_len)
 *     ? s->data[i] : 0x00`. Im A/B-Vergleich derselben Spur weichen
 *     **4743 von 65 536 Byte** ab — 7,2 % der kodierten Spur waren
 *     erfunden. Dieser Encoder schreibt UDI und wandelt IMG nach HFE.
 *
 * ── Was dieser Test tut ─────────────────────────────────────────────────
 *
 * Er baut seine Pruefdateien **selbst** und braucht deshalb keinen
 * Korpus: eine MGT von 819 200 Byte und eine CP/M-`ibm-3740` von
 * 256 256 Byte, beide mit selbstbenennenden Sektoren. Dann prueft er die
 * Kette bis zu den beiden Stellen ohne Rueckfall. Die Zusage ueber den
 * Encoder ist bewusst ein **A/B-Vergleich**: sie zeigt nicht nur, dass
 * es jetzt geht, sondern dass die Laenge den Unterschied macht.
 *
 * ── Was NICHT geprueft ist ──────────────────────────────────────────────
 *
 * * Ob die gesetzte Laenge die RICHTIGE ist — das kann nur eine Messung
 *   an einem fremd erzeugten Abbild (fuer `cpm` siehe das Orakel
 *   `cpmtools`).
 * * Die sieben uebrigen Plugins derselben Klasse; sie haelt statisch das
 *   Tor `scripts/audit_sektor_laenge.py` (Selbsttest 8/8, Grundlinie 0).
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_track.h"
#include "uft/core/uft_unified_types.h"
#include "uft/uft_mfm_encoder.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

extern const uft_format_plugin_t uft_format_plugin_mgt;
extern const uft_format_plugin_t uft_format_plugin_cpm;

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int bedingung, const char *detail)
{
    if (bedingung) { gruen++; printf("  ok   %s\n", was); }
    else { rot++; printf("  FAIL %s - %s\n", was, detail ? detail : ""); }
}

/**
 * Schreibt eine Datei, deren Sektoren sich selbst benennen.
 *
 * `frei` bleibt dabei unberuehrt (0xE5). Das ist kein Schoenheitsgriff:
 * `cpm_waehle()` prueft seit MF-1039 das Byte am VERZEICHNISANFANG und
 * nimmt nur `0xE5` oder einen Benutzerbereich 0..15 an. Bei `ibm-3740`
 * liegt es bei Versatz 6656, also in Sektor 52 - eine Markierung dort
 * faengt mit 'U' (0x55) an, und die Datei wird abgewiesen. Genau das ist
 * beim ersten Lauf passiert.
 */
static bool schreibe(const char *pfad, size_t groesse, size_t sektorgroesse,
                     size_t frei)
{
    FILE *f = fopen(pfad, "wb");
    uint8_t *puffer;
    size_t i, n = groesse / sektorgroesse;
    if (!f) return false;
    puffer = (uint8_t *)malloc(groesse);
    if (!puffer) { fclose(f); return false; }
    memset(puffer, 0xE5, groesse);
    for (i = 0; i < n; i++) {
        char marke[40];
        int len;
        if (i == frei) continue;
        len = snprintf(marke, sizeof marke, "UFT-SEKTOR %06u", (unsigned)i);
        memcpy(puffer + i * sektorgroesse, marke, (size_t)len);
    }
    fwrite(puffer, 1, groesse, f);
    fclose(f);
    free(puffer);
    return true;
}

/**
 * Laeuft ein Plugin ueber eine Datei und misst drei Dinge: Sektoren
 * gesamt, Sektoren mit `data_len == 0`, und Sektoren, deren erste Bytes
 * die erwartete Selbstbenennung tragen.
 */
static void lauf(const uft_format_plugin_t *p, const char *pfad,
                 unsigned *ges, unsigned *ohne_laenge, unsigned *benannt)
{
    uft_disk_t disk;
    int c, h;
    unsigned lauf_nr = 0;
    *ges = *ohne_laenge = *benannt = 0;
    memset(&disk, 0, sizeof disk);
    if (p->open(&disk, pfad, true) != UFT_OK) return;
    for (c = 0; c < disk.geometry.cylinders; c++)
        for (h = 0; h < disk.geometry.heads; h++) {
            uft_track_t t;
            size_t k;
            memset(&t, 0, sizeof t);
            if (p->read_track(&disk, c, h, &t) != UFT_OK) continue;
            for (k = 0; k < t.sector_count; k++) {
                const uft_sector_t *s = &t.sectors[k];
                char erwartet[40];
                (*ges)++;
                if (s->data_len == 0) (*ohne_laenge)++;
                snprintf(erwartet, sizeof erwartet, "UFT-SEKTOR %06u",
                         lauf_nr);
                if (s->data && s->data_size >= strlen(erwartet)
                    && memcmp(s->data, erwartet, strlen(erwartet)) == 0)
                    (*benannt)++;
                lauf_nr++;
            }
            uft_track_release(&t);
        }
    p->close(&disk);
}

int main(void)
{
    char det[300];
    const char *mgt_pfad = "uft_mf1080_probe.mgt";
    const char *cpm_pfad = "uft_mf1080_probe.cpm";
    unsigned ges = 0, ohne = 0, benannt = 0;

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("Ein Sektor mit Bytes nennt seine Laenge - MF-1080\n");
    printf("=================================================\n");

    if (!schreibe(mgt_pfad, 819200u, 512u, (size_t)-1)
        || !schreibe(cpm_pfad, 256256u, 128u, 52u)) {
        printf("SKIP: kein schreibbarer Ort fuer die Pruefdateien.\n");
        return 77;
    }

    /* ── 1. MGT: 1600 Sektoren, alle mit Laenge ────────────────────── */
    lauf(&uft_format_plugin_mgt, mgt_pfad, &ges, &ohne, &benannt);
    snprintf(det, sizeof det, "%u Sektoren, %u ohne Laenge, %u benannt",
             ges, ohne, benannt);
    pruefe("MGT: alle 1600 Sektoren tragen ihre Laenge UND ihre Bytes - "
           "vor MF-1080 waren es 1600 von 1600 mit Laenge 0",
           ges == 1600 && ohne == 0 && benannt == 1600, det);

    /* ── 2. CP/M: 2002 Sektoren, alle mit Laenge ───────────────────── */
    lauf(&uft_format_plugin_cpm, cpm_pfad, &ges, &ohne, &benannt);
    snprintf(det, sizeof det, "%u Sektoren, %u ohne Laenge, %u benannt",
             ges, ohne, benannt);
    pruefe("CP/M ibm-3740: alle 2002 Sektoren tragen ihre Laenge, und 2001 "
           "davon ihre Bytes - der 2002. ist der Verzeichnisanfang, der "
           "absichtlich leer bleibt (MF-1039)",
           ges == 2002 && ohne == 0 && benannt == 2001, det);

    /* ── 3. Die erste Stelle ohne Rueckfall: uft_sector_copy() ─────── */
    {
        uft_disk_t disk;
        uft_track_t t;
        memset(&disk, 0, sizeof disk);
        memset(&t, 0, sizeof t);
        if (uft_format_plugin_cpm.open(&disk, cpm_pfad, true) == UFT_OK
            && uft_format_plugin_cpm.read_track(&disk, 0, 0, &t) == UFT_OK
            && t.sector_count > 0) {
            uft_sector_t kopie;
            int rc;
            memset(&kopie, 0, sizeof kopie);
            rc = uft_sector_copy(&kopie, &t.sectors[0]);
            snprintf(det, sizeof det, "rc=%d data=%s data_len=%u", rc,
                     kopie.data ? "da" : "NULL", (unsigned)kopie.data_len);
            pruefe("uft_sector_copy() traegt die Bytes mit - vor MF-1080 "
                   "gab sie rc=0 zurueck UND einen Sektor mit data==NULL, "
                   "also Erfolg ohne Daten",
                   rc == 0 && kopie.data != NULL && kopie.data_len == 128
                   && memcmp(kopie.data, t.sectors[0].data, 128) == 0, det);
            if (kopie.data) free(kopie.data);
            uft_track_release(&t);
            uft_format_plugin_cpm.close(&disk);
        } else {
            pruefe("uft_sector_copy()", 0, "Spur 0 nicht lesbar");
        }
    }

    /* ── 4. Die zweite Stelle: der MFM-Encoder, als A/B ────────────── */
    {
        uft_disk_t disk;
        uft_track_t t;
        memset(&disk, 0, sizeof disk);
        memset(&t, 0, sizeof t);
        if (uft_format_plugin_cpm.open(&disk, cpm_pfad, true) == UFT_OK
            && uft_format_plugin_cpm.read_track(&disk, 0, 0, &t) == UFT_OK
            && t.sector_count > 0) {
            const size_t N = 64u * 1024u;
            uint8_t *a = (uint8_t *)calloc(1, N);
            uint8_t *b = (uint8_t *)calloc(1, N);
            size_t na = 0, nb = 0, i, abweichend = 0, m;
            if (a && b) {
                na = uft_mfm_encode_from_track(&t, a, N);
                for (i = 0; i < t.sector_count; i++)
                    t.sectors[i].data_len = 0;
                nb = uft_mfm_encode_from_track(&t, b, N);
                m = na < nb ? na : nb;
                for (i = 0; i < m; i++) if (a[i] != b[i]) abweichend++;
            }
            snprintf(det, sizeof det, "mit %u Byte, ohne %u Byte, "
                     "%u abweichend", (unsigned)na, (unsigned)nb,
                     (unsigned)abweichend);
            pruefe("der MFM-Encoder haengt an data_len: dieselbe Spur einmal "
                   "mit und einmal ohne Laenge ergibt VERSCHIEDENE Stroeme - "
                   "ohne sie schreibt er 0x00 statt der Daten",
                   na > 0 && na == nb && abweichend > 0, det);
            free(a); free(b);
            uft_track_release(&t);
            uft_format_plugin_cpm.close(&disk);
        } else {
            pruefe("MFM-Encoder A/B", 0, "Spur 0 nicht lesbar");
        }
    }

    remove(mgt_pfad);
    remove(cpm_pfad);
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
