/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_cbmdos.c
 * @brief CBM-DOS-Verzeichnis lesen — MF-683
 *
 * Referenz: VICE 3.10 `c1541`, gegen den Befehl aus
 * `tests/corpus_manifest/manifest.json` geprueft. Struktur nach
 * CBM DOS 2.6 (Spur 18, Kette ab Sektor 1, 8 Eintraege je Sektor).
 * Ausfuehrlich im Header.
 */

#include "uft/fs/uft_cbmdos.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* ── Geometrie ───────────────────────────────────────────────────────────
 *
 * Die Sektorzahl je Spur steht hier noch einmal, obwohl sie im Baum
 * mehrfach existiert. Grund: dieses Modul soll ohne den Format-Layer
 * uebersetzbar sein — es liest ein Abbild, es benutzt keinen Parser. Die
 * Tabelle ist vier Zeilen und aus der 1541-Beschreibung eindeutig; sie
 * hier NICHT zu haben hiesse, das Dateisystem an einen Parser zu binden,
 * den es nicht braucht. */
#define CBM_SECTOR_SIZE     256
#define CBM_DIR_TRACK       18
#define CBM_DIR_SECTOR      1
#define CBM_BAM_SECTOR      0
#define CBM_ENTRIES_PER_SEC 8
#define CBM_ENTRY_SIZE      32
#define CBM_D64_35_SIZE     174848L

/* Wieviele Sektoren hat Spur @p track? 1..35. */
static int cbm_sectors_per_track(int track)
{
    if (track < 1)  return 0;
    if (track <= 17) return 21;
    if (track <= 24) return 19;
    if (track <= 30) return 18;
    if (track <= 40) return 17;   /* 36..40 nur bei erweiterten Abbildern */
    return 0;
}

/* Byte-Offset von Spur/Sektor im Abbild, oder -1. */
static long cbm_offset(int track, int sector)
{
    if (track < 1 || track > 40) return -1;
    int n = cbm_sectors_per_track(track);
    if (sector < 0 || sector >= n) return -1;
    long off = 0;
    for (int t = 1; t < track; t++)
        off += (long)cbm_sectors_per_track(t) * CBM_SECTOR_SIZE;
    return off + (long)sector * CBM_SECTOR_SIZE;
}

/* PETSCII-Name nach ASCII, 0xA0-Fuellung ab.
 *
 * CBM-DOS fuellt Namen rechts mit 0xA0 (Shifted Space). Ein Name darf
 * INNEN Leerzeichen haben — "uft marker" ist genau so einer —, darum
 * wird von hinten abgeschnitten und nicht am ersten Leerzeichen.
 *
 * Die Umwandlung bleibt bewusst bei den druckbaren Zeichen: PETSCII
 * 0x41..0x5A sind Grossbuchstaben, 0xC1..0xDA dieselben in der zweiten
 * Zeichensatz-Haelfte. Alles andere Druckbare wird durchgereicht,
 * Nicht-Druckbares wird '?'. Eine vollstaendige PETSCII-Tabelle waere
 * eine zweite Behauptung in einem Modul, das eine belegen soll — wer
 * sie braucht, baut sie mit eigenem Rotbeweis. */
static void cbm_name_to_ascii(const uint8_t *raw, char *out, size_t out_size)
{
    size_t len = 16;
    while (len > 0 && (raw[len - 1] == 0xA0 || raw[len - 1] == 0x00))
        len--;
    if (len >= out_size) len = out_size - 1;

    for (size_t i = 0; i < len; i++) {
        uint8_t c = raw[i];
        if (c >= 0xC1 && c <= 0xDA) c = (uint8_t)(c - 0x80);
        out[i] = (c >= 0x20 && c < 0x7F) ? (char)c : '?';
    }
    out[len] = '\0';
}

const char *uft_cbmdos_type_name(uft_cbmdos_type_t type)
{
    switch (type) {
        case UFT_CBMDOS_DEL: return "DEL";
        case UFT_CBMDOS_SEQ: return "SEQ";
        case UFT_CBMDOS_PRG: return "PRG";
        case UFT_CBMDOS_USR: return "USR";
        case UFT_CBMDOS_REL: return "REL";
        default:             return "???";
    }
}

void uft_cbmdos_free(uft_cbmdos_dir_t *dir)
{
    if (!dir) return;
    free(dir->entries);
    memset(dir, 0, sizeof(*dir));
}

/**
 * Ist dieser Eintrag STRUKTURELL ein CBM-DOS-Eintrag?
 *
 * MF-889: bis hierher galt jeder Eintrag, dessen Typ-Nibble weder 0 noch
 * DEL war. Gemessen an einem pseudozufaelligen 174848-Byte-Puffer meldete
 * der Leser daraufhin `UFT_OK` mit acht Eintraegen, Diskname
 * `?????ELSZ???????` und einem ersten Eintrag `V??????????#*18?`,
 * Typ SEQ, **35973 Bloecke**.
 *
 * 35973 Bloecke auf einer Diskette mit 683 ist keine beschaedigte Datei,
 * das ist Rauschen. Ein Leser, der Rauschen als Verzeichnis annimmt, kann
 * die Frage "ist das ein D64?" nicht beantworten — dieselbe Klasse wie
 * MF-729, wo Sonden Konfidenz ohne Bedeutung vergaben.
 *
 * Geprueft werden nur STRUKTURFAKTEN des Formats, keine erfundenen
 * Schwellen (CBM DOS 2.6, `docs/format_specs/commodore/D64.TXT`):
 *
 *   - Der Typ steht in den unteren vier Bit und kennt fuenf Werte
 *     (DEL/SEQ/PRG/USR/REL). 5..15 ist kein Dateityp.
 *   - Der erste Datensektor liegt auf einer Spur, die es gibt, und in
 *     einem Sektor, den diese Spur hat. Spur 0 gibt es nicht.
 *   - Eine Datei kann nicht mehr Bloecke belegen, als die Diskette hat.
 *
 * Was hier NICHT geprueft wird: der Name. Ein CBM-Name darf jedes Byte
 * enthalten, auch unsinnig aussehende — daran zu entscheiden hiesse,
 * ungewoehnliche echte Namen zu verwerfen.
 */
static bool cbm_entry_plausibel(const uft_cbmdos_entry_t *d,
                                int spuren, int bloecke_gesamt)
{
    if ((int)d->type > (int)UFT_CBMDOS_REL) return false;
    if (d->track == 0 || d->track > spuren) return false;
    int spt = cbm_sectors_per_track(d->track);
    if (spt <= 0 || d->sector >= spt) return false;
    if (d->blocks > bloecke_gesamt) return false;
    return true;
}

uft_error_t uft_cbmdos_read_directory(const char *path,
                                      uft_cbmdos_dir_t *out)
{
    if (!path || !out) return UFT_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));

    FILE *f = fopen(path, "rb");
    if (!f) return UFT_ERR_IO;

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERR_IO; }
    long size = ftell(f);
    if (size < CBM_D64_35_SIZE) {   /* kuerzer als 35 Spuren: kein D64 */
        fclose(f);
        return UFT_ERR_FORMAT;
    }

    /* ── BAM: Diskname und ID ────────────────────────────────────────
     *
     * Spur 18 Sektor 0, Name ab 0x90, ID ab 0xA2. Zwischen ID und
     * DOS-Typ steht ein 0xA0; c1541 zeigt beides zusammen als
     * "42 2A" — hier wird die ID gelesen und die Fuellung abgeschnitten. */
    uint8_t bam[CBM_SECTOR_SIZE];
    long bam_off = cbm_offset(CBM_DIR_TRACK, CBM_BAM_SECTOR);
    if (bam_off < 0 || fseek(f, bam_off, SEEK_SET) != 0 ||
        fread(bam, 1, sizeof(bam), f) != sizeof(bam)) {
        fclose(f);
        return UFT_ERR_IO;
    }
    cbm_name_to_ascii(bam + 0x90, out->disk_name, sizeof(out->disk_name));

    /* Die ID sind GENAU die zwei Bytes ab 0xA2 — das ist, was im
     * Formatier-Befehl hinter dem Komma steht (`-format "uftcorpus,42"`).
     * Darauf folgt 0xA0 als Trenner und ab 0xA5 der DOS-Typ ("2A").
     * c1541 zeigt beides nebeneinander als "42 2A"; das ist eine
     * Darstellungsfrage, keine Eigenschaft der ID.
     *
     * Der erste Entwurf las fuenf Bytes und lieferte "42?2A" — das
     * Fuellbyte wurde zum Fragezeichen. Aufgefallen ist es, weil der
     * Rotbeweis die ID gegen den BEFEHL prueft und nicht gegen das, was
     * der Leser gerade liefert. Genau dafuer steht die Erwartung im
     * Manifest und nicht im Test. */
    for (int i = 0; i < 2; i++) {
        uint8_t c = bam[0xA2 + i];
        if (c >= 0xC1 && c <= 0xDA) c = (uint8_t)(c - 0x80);
        out->disk_id[i] = (c >= 0x20 && c < 0x7F) ? (char)c : '?';
    }
    out->disk_id[2] = '\0';

    /* ── Die Verzeichniskette ────────────────────────────────────────
     *
     * Jeder Sektor beginnt mit Spur/Sektor des naechsten; Spur 0 heisst
     * Ende. Die Schleife zaehlt Schritte mit: eine beschaedigte Kette
     * kann im Kreis zeigen, und ein Verzeichnisleser, der daran haengen
     * bleibt, ist in einem Forensik-Werkzeug ein Angriff auf die
     * Geduld — und auf beschaedigten Disketten der Normalfall, nicht
     * die Ausnahme. 683 Bloecke ist die Obergrenze eines 35-Spur-D64;
     * mehr Schritte kann keine gueltige Kette haben. */
    int cap = 0, n = 0, verworfen = 0;
    uft_cbmdos_entry_t *list = NULL;

    /* Wie viele Spuren hat dieses Abbild? Die Groesse sagt es; mehr als
     * 42 Spuren gibt es bei CBM DOS nicht. */
    int spuren = 35, bloecke_gesamt = 683;
    if (size >= 205312)      { spuren = 42; bloecke_gesamt = 802; }
    else if (size >= 196608) { spuren = 40; bloecke_gesamt = 768; }

    int track = CBM_DIR_TRACK, sector = CBM_DIR_SECTOR;
    for (int schritt = 0; schritt < 683 && track != 0; schritt++) {
        long off = cbm_offset(track, sector);
        if (off < 0 || off + CBM_SECTOR_SIZE > size) break;

        uint8_t sec[CBM_SECTOR_SIZE];
        if (fseek(f, off, SEEK_SET) != 0 ||
            fread(sec, 1, sizeof(sec), f) != sizeof(sec)) break;

        for (int i = 0; i < CBM_ENTRIES_PER_SEC; i++) {
            const uint8_t *e = sec + i * CBM_ENTRY_SIZE;
            uint8_t tb = e[2];
            if (tb == 0x00) continue;            /* nie benutzt */

            uft_cbmdos_type_t typ = (uft_cbmdos_type_t)(tb & 0x0F);
            if (typ == UFT_CBMDOS_DEL) { out->deleted_count++; continue; }

            if (n == cap) {
                int neu = cap ? cap * 2 : 16;
                uft_cbmdos_entry_t *p =
                    realloc(list, (size_t)neu * sizeof(*p));
                if (!p) { free(list); fclose(f); return UFT_ERR_MEMORY; }
                list = p; cap = neu;
            }
            uft_cbmdos_entry_t kandidat;
            memset(&kandidat, 0, sizeof(kandidat));
            cbm_name_to_ascii(e + 5, kandidat.name, sizeof(kandidat.name));
            kandidat.type   = typ;
            kandidat.closed = (tb & 0x80) != 0;
            kandidat.locked = (tb & 0x40) != 0;
            kandidat.track  = e[3];
            kandidat.sector = e[4];
            kandidat.blocks = (uint16_t)(e[30] | (e[31] << 8));

            if (!cbm_entry_plausibel(&kandidat, spuren, bloecke_gesamt)) {
                verworfen++;
                continue;
            }
            list[n++] = kandidat;
        }

        track  = sec[0];
        sector = sec[1];
    }

    fclose(f);

    /* MF-889: Kein einziger tragfaehiger Eintrag, aber welche verworfen —
     * dann ist das kein CBM-Verzeichnis, sondern etwas anderes an dieser
     * Stelle. Eine leere Diskette ist davon unterscheidbar: sie hat 0
     * Eintraege UND 0 Verwuerfe.
     *
     * Der Unterschied zaehlt fuer den Aufrufer: `UFT_OK` mit 0 Eintraegen
     * heisst "gelesen, leer", `UFT_ERR_FORMAT` heisst "nicht gelesen". Die
     * Oberflaeche zeigt im zweiten Fall wieder ihre ehrliche Meldung
     * (`src/explorertab.cpp`), statt Rauschen als Dateiliste. */
    if (n == 0 && verworfen > 0) {
        free(list);
        return UFT_ERR_FORMAT;
    }

    out->entries     = list;
    out->entry_count = n;
    return UFT_OK;
}
