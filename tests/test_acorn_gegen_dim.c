/**
 * @file test_acorn_gegen_dim.c
 * @brief ADFS gegen DiscImageManagers Leer-Abbilder (MF-1072)
 *
 * `adl` und `adf_arc` standen beide auf **T2**: die Leser waren gegen
 * eine benannte Quelle geschrieben (DiscImageManager, geraldholdsworth,
 * GPL-3 — als *Spec* gelesen, kein Code uebernommen), aber im Korpus lag
 * **kein einziges Acorn-Abbild von fremder Hand**.
 *
 * ── Die Quelle lag im selben Klon wie die Spec ──────────────────────────
 *
 * `tools/uft-scout/work/DiscImageManager/Blank Images/` traegt **21
 * formatierte Leer-Abbilder**, darunter acht Acorn-ADFS. Sie sind seit
 * derselben Sichtung da, aus der die Formatbeschreibung stammt, und
 * niemand hat nach ihnen gefragt — dieselbe Lehre wie MF-1050
 * (`a2nibblize` lag im selben Klon wie `to_woz2`) und MF-1071 (fluxfox'
 * Testverzeichnis): **ein geklontes Paket ist mehr als der eine Grund,
 * aus dem man es geklont hat.**
 *
 * ── Warum ein LEERES Abbild hier trotzdem etwas beweist ─────────────────
 *
 * MF-1021 hat die Regel aufgestellt: ein erzeugtes Fixture ist erst dann
 * ein Beleg, wenn sein **Inhalt** nachgewiesen ist — dort war eine
 * `v9t9` zu 100 % `0xF6` und damit wertlos.
 *
 * Diese Abbilder sind nicht leer in dem Sinn. Gemessen tragen sie ein
 * **echtes ADFS-Verzeichnis**:
 *
 *     ADFS_L.adl : Kennung „Hugo" bei Versatz 0x201   (altes Format)
 *     ADFS_D.adf : „Nick" bei 0x401 UND „Hugo" bei 0xC01
 *     ADFS_E.adf : „Nick" bei 0x801
 *     ADFS_F.adf : „Nick" bei 0xC8801
 *
 * `Hugo` und `Nick` sind die beiden ADFS-Verzeichniskennungen; welche
 * wo steht, folgt aus Format und Anordnung. Ein Leser mit falschem
 * Versatz findet sie nicht an der erwarteten Stelle — deshalb prueft die
 * Zusage unten **die Kennung ueber den gelieferten SEKTOR**, nicht in
 * der Datei.
 *
 * -- Warum die Kennung allein nicht genuegt ------------------------------
 *
 * Sie genuegt nicht, und das ist **gemessen**: die Mutationsmatrix zu
 * MF-1072 hat die Zusage in ihrer ersten Fassung ueberfuehrt. Eine
 * vertauschte Versatzformel (`head * 80 + cyl` statt `cyl * 2 + head`)
 * ist eine **Permutation** der 160 Spuren — Spur (0,0) bleibt bei
 * beiden Formeln auf Versatz 0, die Kennung liegt weiter dort, Geometrie
 * und Sektorsumme aendern sich nicht. **Alle 160 Spuren waeren
 * vertauscht gewesen und jede Zusage gruen.** Dieselbe Klasse wie
 * MF-1026 (zwei Zonengrenzen, die sich zu 1224 aufheben) und MF-1014.
 *
 * Seit dieser Fassung wird jeder gelieferte Sektor **byteweise gegen
 * einen unabhaengig nachgerechneten Dateiversatz** gehalten — die Form
 * der Abnahme aus MF-1071. 2560 + 800 + 800 + 1600 = **5760 Sektoren**,
 * jeder an seiner Stelle nachgewiesen. Dazu haelt die Zusage die
 * Sondenkonfidenz im Band „nur die Groesse“ (< 50, MF-729): beide
 * Sonden sehen `(void)d` — den Inhalt der Datei nie.
 *
 * **Und die Grenze gehoert dazu, weil sie bleibt:** auch der byteweise
 * Vergleich faengt die Vertauschung NICHT, und der Grund ist gemessen
 * — von den 160 Spuren der ADFS_L sind **159 byteidentisch** (frisch
 * formatiert, nur Spur 0 traegt das Verzeichnis). Eine Permutation mit
 * Fixpunkt auf der einzigen unterscheidbaren Spur ist an einem leeren
 * Abbild **grundsaetzlich** unsichtbar; das ist keine Luecke der Zusage,
 * sondern eine des Belegstuecks (MF-1021 in umgekehrter Richtung). Die
 * Matrix isoliert die Zeile deshalb ueber eine **Verschiebung** um eine
 * Spur, die Spur 0 mitnimmt — und faengt sie. **9 von 9 Mutationen
 * gefangen**, Grundlauf zuerst.
 *
 * ── Der Befund, den diese Dateien ausgeloest haben ──────────────────────
 *
 * `adl_open()` hatte **keine Groessenpruefung**. Gemessen am Vorzustand:
 * `ADFS_S.adl` (323 584 Byte) und `ADFS_M.adl` (651 264) wurden beide
 * geoeffnet und lieferten **2560 Sektoren** — die Zahl der vollen
 * L-Diskette. Bei der ersten sind das **1296 Sektoren, die es nicht
 * gibt**, gelesen hinter dem Dateiende.
 *
 * Die Sonde war dabei die ganze Zeit ehrlich (`fs == ADL_SIZE`, gemessen
 * probe=0 fuer beide, probe=1 nur fuer `ADFS_L.adl`). **Das Oeffnen hat
 * ihre Zurueckhaltung aufgehoben** — die Gestalt von MF-1038. Seit
 * MF-1072 spiegelt `adl_open()` die Pruefung der Sonde.
 *
 * **Und die zwei kurzen Dateien tragen irrefuehrende Namen:** 323 584
 * ist weder ADFS S (163 840) noch M (327 680), sondern **M minus 4096**;
 * 651 264 ist **L minus 4096**. Was sie sind, sagt die Groesse allein
 * nicht — deshalb werden sie abgewiesen und nicht geraten. Sie liegen
 * aus demselben Grund NICHT im Korpus.
 *
 * ── Was NICHT geprueft ist ──────────────────────────────────────────────
 *
 * * **Dateiinhalte.** Die Abbilder sind frisch formatiert; geprueft sind
 *   Geometrie, Sektorzahl und die Verzeichniskennung, nicht Nutzdaten.
 * * **ADFS S und M.** Sie haben kein gueltiges Abbild (siehe oben) und
 *   damit auch keinen Beleg.
 * * **`adf_ext`.** Es weist alle acht Dateien ab — richtigerweise: es
 *   liest **UAE-1ADF** (Amiga, gegen WinUAEs `read_header_ext2`, MF-352),
 *   nicht Acorn ADFS. Die Endung `.adf` tragen beide Familien; das ist
 *   eine Namensgleichheit, keine Formatgleichheit.
 * * **Der Schreibpfad** beider Formate.
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_track.h"   /* uft_track_release — implizite Deklaration
                              * ist auf macOS-Clang ein FEHLER */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR ""
#endif

extern const uft_format_plugin_t uft_format_plugin_adl;
extern const uft_format_plugin_t uft_format_plugin_adf_arc;

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int bedingung, const char *detail)
{
    if (bedingung) { gruen++; printf("  ok   %s\n", was); }
    else { rot++; printf("  FAIL %s - %s\n", was, detail ? detail : ""); }
}

/* Sucht die ADFS-Verzeichniskennung im gelieferten Spurinhalt. Gesucht
 * wird ueber die SEKTOREN, nicht in der Datei — nur so sagt ein Treffer
 * etwas ueber den Leser. */
static int kennung_in_spur(const uft_track_t *t, const char *kw)
{
    size_t k;
    for (k = 0; k < t->sector_count; k++) {
        const uft_sector_t *s = &t->sectors[k];
        size_t len = s->data_len ? s->data_len : s->data_size;
        size_t i;
        if (!s->data || len < 4) continue;
        for (i = 0; i + 4 <= len; i++)
            if (memcmp(s->data + i, kw, 4) == 0) return 1;
    }
    return 0;
}

static void lies(const char *name, const uft_format_plugin_t *p,
                 const char *datei, unsigned zyl, unsigned koepfe,
                 unsigned sekt, unsigned sgr, const char *kennung)
{
    char pfad[600], det[300];
    uft_disk_t disk;
    FILE *f;
    long gr;
    uint8_t kopf[4096];
    uint8_t *ganz = NULL;
    size_t gelesen;
    unsigned c, h, spuren = 0, sektoren = 0, kennung_gefunden = 0;
    unsigned abweichend = 0, verglichen = 0;
    int konf = -1, ok;

    snprintf(pfad, sizeof pfad, "%s/%s", UFT_CORPUS_DIR, datei);
    f = fopen(pfad, "rb");
    if (!f) { pruefe(name, 0, "Datei fehlt"); return; }
    fseek(f, 0, SEEK_END); gr = ftell(f); fseek(f, 0, SEEK_SET);
    gelesen = fread(kopf, 1, sizeof kopf, f);
    /* Die ganze Datei daneben halten: der Vergleich unten rechnet den
     * Versatz UNABHAENGIG vom Leser nach. */
    if (gr > 0) {
        ganz = (uint8_t *)malloc((size_t)gr);
        if (ganz) {
            fseek(f, 0, SEEK_SET);
            if (fread(ganz, 1, (size_t)gr, f) != (size_t)gr) {
                free(ganz); ganz = NULL;
            }
        }
    }
    fclose(f);
    if (!ganz) { pruefe(name, 0, "Datei nicht vollstaendig lesbar"); return; }

    ok = p->probe(kopf, gelesen, (size_t)gr, &konf) ? 1 : 0;

    memset(&disk, 0, sizeof disk);
    if (p->open(&disk, pfad, true) != UFT_OK) {
        snprintf(det, sizeof det, "%ld Byte, probe=%d - open scheitert",
                 gr, ok);
        pruefe(name, 0, det);
        free(ganz);
        return;
    }

    for (c = 0; c < zyl; c++)
        for (h = 0; h < koepfe; h++) {
            uft_track_t t;
            size_t k;
            /* Unabhaengig nachgerechnet: beide Formate legen die Spuren
             * kopf-verschraenkt ab (Seite 0 und 1 wechseln sich
             * spurweise ab), Sektoren linear innerhalb der Spur. */
            size_t basis = ((size_t)c * koepfe + h) * sekt * sgr;
            memset(&t, 0, sizeof t);
            if (p->read_track(&disk, (int)c, (int)h, &t) != UFT_OK) continue;
            spuren++;
            sektoren += (unsigned)t.sector_count;
            if (kennung && kennung_in_spur(&t, kennung)) kennung_gefunden++;
            for (k = 0; k < t.sector_count; k++) {
                const uft_sector_t *s = &t.sectors[k];
                size_t soll = basis + k * sgr;
                if (!s->data || s->data_len != sgr) { abweichend++; continue; }
                if (soll + sgr > (size_t)gr) { abweichend++; continue; }
                verglichen++;
                if (memcmp(s->data, ganz + soll, sgr) != 0) abweichend++;
            }
            uft_track_release(&t);
        }
    free(ganz);

    snprintf(det, sizeof det,
             "%ld B, probe=%d(%d), geo %dx%dx%dx%d, %u Spuren, %u Sektoren, "
             "Kennung in %u Spuren, %u verglichen / %u abweichend", gr, ok, konf,
             disk.geometry.cylinders, disk.geometry.heads,
             disk.geometry.sectors, disk.geometry.sector_size,
             spuren, sektoren, kennung_gefunden, verglichen, abweichend);
    pruefe(name,
           ok && konf > 0 && konf < 50            /* MF-729: nur die Groesse */
           && (unsigned)disk.geometry.cylinders == zyl
           && (unsigned)disk.geometry.heads == koepfe
           && (unsigned)disk.geometry.sectors == sekt
           && (unsigned)disk.geometry.sector_size == sgr
           && spuren == zyl * koepfe
           && sektoren == zyl * koepfe * sekt
           && verglichen == zyl * koepfe * sekt
           && abweichend == 0
           && (!kennung || kennung_gefunden >= 1), det);
    p->close(&disk);
}

int main(void)
{
    char pfad[600], det[200];
    uft_disk_t disk;

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("ADFS gegen DiscImageManager - MF-1072\n");
    printf("=====================================\n");

    if (UFT_CORPUS_DIR[0] == '\0') {
        printf("SKIP: UFT_CORPUS_DIR nicht gesetzt.\n");
        return 77;      /* SKIP_RETURN_CODE, seit MF-598 */
    }
    snprintf(pfad, sizeof pfad, "%s/dim_adfs_l.adl", UFT_CORPUS_DIR);
    {
        FILE *f = fopen(pfad, "rb");
        if (!f) { printf("SKIP: %s fehlt.\n", pfad); return 77; }
        fclose(f);
    }

    /* ADFS L: 80 x 2 x 16 x 256 = 655 360, Spuren verschraenkt.
     * Die Groessen der Untertypen stehen im Kopf von `uft_adl.c` mit
     * Fundstelle (DiscImage_ADFS.pas:73-75). */
    lies("adl: ADFS_L liest 160 Spuren zu 16 Sektoren und traegt die "
         "Verzeichniskennung \"Hugo\" IM SEKTOR",
         &uft_format_plugin_adl, "dim_adfs_l.adl", 80, 2, 16, 256, "Hugo");

    /* ADFS D/E: 819 200 Byte = 80 x 2 x 5 x 1024. */
    lies("adf_arc: ADFS_D liest 160 Spuren zu 5 Sektoren a 1024",
         &uft_format_plugin_adf_arc, "dim_adfs_d.adf", 80, 2, 5, 1024, NULL);
    lies("adf_arc: ADFS_E liest 160 Spuren zu 5 Sektoren a 1024",
         &uft_format_plugin_adf_arc, "dim_adfs_e.adf", 80, 2, 5, 1024, NULL);

    /* ADFS F: 1 638 400 Byte = 80 x 2 x 10 x 1024. */
    lies("adf_arc: ADFS_F liest 160 Spuren zu 10 Sektoren a 1024 - die "
         "doppelte Kapazitaet, aus der Dateigroesse abgeleitet",
         &uft_format_plugin_adf_arc, "dim_adfs_f.adf", 80, 2, 10, 1024, NULL);

    /* Gegenprobe: `adl` darf die ADFS-D/E/F-Dateien NICHT annehmen. Vor
     * MF-1072 hat `open` jede Groesse angenommen. */
    {
        int angenommen = 0;
        const char *fremd[] = { "dim_adfs_d.adf", "dim_adfs_e.adf",
                                "dim_adfs_f.adf" };
        unsigned i;
        for (i = 0; i < 3; i++) {
            snprintf(pfad, sizeof pfad, "%s/%s", UFT_CORPUS_DIR, fremd[i]);
            memset(&disk, 0, sizeof disk);
            if (uft_format_plugin_adl.open(&disk, pfad, true) == UFT_OK) {
                angenommen++;
                uft_format_plugin_adl.close(&disk);
            }
        }
        snprintf(det, sizeof det, "%d von 3 angenommen", angenommen);
        pruefe("adl weist die drei ADFS-D/E/F-Abbilder ab - vor MF-1072 "
               "nahm `open` jede Groesse an und erfand die Geometrie",
               angenommen == 0, det);
    }

    /* Gegenprobe in die andere Richtung: `adf_arc` darf die ADFS-L-Datei
     * NICHT annehmen. 655 360 steht in seiner Groessentafel nicht, und
     * seine lineare Ablage traefe bei einer verschraenkten Datei die
     * falschen Bytes (so steht es im Kopf von `uft_adf_arc.c`). */
    {
        int angenommen;
        snprintf(pfad, sizeof pfad, "%s/dim_adfs_l.adl", UFT_CORPUS_DIR);
        memset(&disk, 0, sizeof disk);
        angenommen = (uft_format_plugin_adf_arc.open(&disk, pfad, true)
                      == UFT_OK);
        if (angenommen) uft_format_plugin_adf_arc.close(&disk);
        snprintf(det, sizeof det, "angenommen=%d", angenommen);
        pruefe("adf_arc weist das ADFS-L-Abbild ab - die Verschraenkung "
               "traefe mit seiner linearen Rechnung die falschen Bytes",
               angenommen == 0, det);
    }

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
