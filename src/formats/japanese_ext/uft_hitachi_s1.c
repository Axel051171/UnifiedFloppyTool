/**
 * @file uft_hitachi_s1.c
 * @brief Hitachi S1 Disk Format Support
 * @version 4.1.4
 * 
 * Hitachi S1 - Japanese Business Computer (1979-1985)
 * Used in banking and business applications
 * 
 * Disk formats:
 * - 8" SSSD: 77 tracks, 26 sectors, 128 bytes = 256 KB
 * - 8" DSDD: 77 tracks, 26 sectors, 256 bytes, DS = 1 MB
 * - 5.25" DD: 80 tracks, 16 sectors, 256 bytes = 320 KB
 */

#include "uft/uft_log.h"
#include "uft/formats/uft_hitachi_s1.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const struct {
    int tracks, sectors, heads, sector_size;
    size_t total_size;
    const char *name;
} g_hitachi_geom[] = {
    { 77, 26, 1, 128, 256256,   "S1 8\" SSSD 250KB" },
    { 77, 26, 2, 256, 1025024,  "S1 8\" DSDD 1MB" },
    { 80, 16, 2, 256, 655360,   "S1 5.25\" DS/DD 640KB" },
    { 80, 8, 2, 512, 655360,    "S1 5.25\" 8-sector 640KB" },
    { 0, 0, 0, 0, 0, NULL }
};


/* ── MF-921 (P3-182): was tragen ALLE Zeilen dieser Groesse? ─────────
 *
 * Die Sonde/der Leser darunter kehrte bei der ERSTEN passenden Zeile
 * zurueck. Steht dieselbe Gesamtgroesse zweimal, gab das still die
 * erste Geometrie aus — eine Auswahl, die entscheidet, wo sie nicht
 * entscheiden kann (dieselbe Form wie die Guard-Kollision MF-881,
 * gemessen von Tor 58 aus MF-906).
 *
 * Der forensische Punkt: aus der Dateilaenge allein ist die Aufteilung
 * nicht ableitbar — sie steht auf dem Traeger, nicht im Abzug. 409600
 * Byte sind 40 Spuren doppelseitig ODER 80 Spuren einseitig.
 *
 * Die Bytes bleiben immer. Zurueckgenommen wird je FELD: worin sich
 * alle passenden Zeilen einig sind, bleibt stehen; wo sie sich
 * unterscheiden, steht 0. */

static int hitachi_einigung(size_t size, int *tracks, int *sectors, int *heads, int *sector_size) {
    int treffer = 0;
    int v_tracks = 0; int u_tracks = 0;
    int v_sectors = 0; int u_sectors = 0;
    int v_heads = 0; int u_heads = 0;
    int v_sector_size = 0; int u_sector_size = 0;
    for (int i = 0; g_hitachi_geom[i].name; i++) {
        if (size != g_hitachi_geom[i].total_size) continue;
        treffer++;
        if (treffer == 1) v_tracks = g_hitachi_geom[i].tracks;
        else if (v_tracks != g_hitachi_geom[i].tracks) u_tracks = 1;
        if (treffer == 1) v_sectors = g_hitachi_geom[i].sectors;
        else if (v_sectors != g_hitachi_geom[i].sectors) u_sectors = 1;
        if (treffer == 1) v_heads = g_hitachi_geom[i].heads;
        else if (v_heads != g_hitachi_geom[i].heads) u_heads = 1;
        if (treffer == 1) v_sector_size = g_hitachi_geom[i].sector_size;
        else if (v_sector_size != g_hitachi_geom[i].sector_size) u_sector_size = 1;
    }
    if (tracks) *tracks = u_tracks ? 0 : v_tracks;
    if (sectors) *sectors = u_sectors ? 0 : v_sectors;
    if (heads) *heads = u_heads ? 0 : v_heads;
    if (sector_size) *sector_size = u_sector_size ? 0 : v_sector_size;
    return treffer;
}

int uft_hitachi_s1_probe(const uint8_t *data, size_t size) {
    if (!data || size < 128) return 0;
    
    for (int i = 0; g_hitachi_geom[i].name; i++) {
        if (size == g_hitachi_geom[i].total_size) {
            int confidence = 30;
            
            /* Hitachi S1 specific patterns */
            if (data[0] != 0x00 && data[0] != 0xFF) {
                confidence += 15;
            }
            
            return confidence > 35 ? confidence : 0;
        }
    }
    return 0;
}

int uft_hitachi_s1_read(const char *path, uft_hitachi_s1_image_t **image) {
    if (!path || !image) return UFT_ERR_INVALID_PARAM;
    
    FILE *f = fopen(path, "rb");
    if (!f) return UFT_ERR_IO;
    
    fseek(f, 0, SEEK_END);
    size_t size = (size_t)ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERR_IO; }
    
    uint8_t *data = malloc(size);
    if (!data) { fclose(f); return UFT_ERR_MEMORY; }
    
    if (fread(data, 1, size, f) != size) {
        free(data); fclose(f); return UFT_ERR_IO;
    }
    fclose(f);
    
    uft_hitachi_s1_image_t *img = calloc(1, sizeof(uft_hitachi_s1_image_t));
    if (!img) { free(data); return UFT_ERR_MEMORY; }
    
    /* MF-921: 655360 Byte stehen zweimal in g_hitachi_geom — 16
     * Sektoren zu 256 und 8 zu 512. Spuren und Koepfe sind sich beide
     * Zeilen einig (80/2), die Sektoraufteilung nicht. Genau das kommt
     * jetzt heraus. */
    int treffer = hitachi_einigung(size, &img->tracks, &img->sectors,
                                   &img->heads, &img->sector_size);
    if (treffer > 1) {
        UFT_WARN("Hitachi S1: %zu Byte passen auf %d Zeilen der "
                 "Geometrietabelle — was sich unterscheidet, bleibt "
                 "ungesetzt (0)", size, treffer);
    }
    
    img->data = data;
    img->size = size;
    *image = img;
    return UFT_OK;
}

void uft_hitachi_s1_free(uft_hitachi_s1_image_t *image) {
    if (image) { free(image->data); free(image); }
}

int uft_hitachi_s1_get_info(uft_hitachi_s1_image_t *img, char *buf, size_t buf_size) {
    if (!img || !buf) return UFT_ERR_INVALID_PARAM;
    snprintf(buf, buf_size,
        "Hitachi S1 Disk Image (Japan Business)\n"
        "Geometry: %d tracks x %d sectors x %d heads\n"
        "Sector Size: %d bytes\nTotal: %zu KB\n",
        img->tracks, img->sectors, img->heads, img->sector_size, img->size / 1024);
    return UFT_OK;
}
