/**
 * @file uft_heathkit.c
 * @brief Heathkit H8/H89 Disk Format Support
 * @version 4.1.4
 * 
 * Heathkit H8/H89 - Kit Computer (1977-1985)
 * Z80 CPU, HDOS operating system
 * Uses HARD-SECTORED disks (10 sector holes)
 * 
 * Disk formats:
 * - SS/SD: 40 tracks, 10 sectors, 256 bytes = 100 KB
 * - SS/DD: 40 tracks, 10 sectors, 512 bytes = 200 KB
 * - DS/DD: 80 tracks, 10 sectors, 512 bytes = 400 KB
 * 
 * Note: Hard-sectored disks have physical holes for each sector
 */

#include "uft/uft_log.h"
#include "uft/formats/uft_heathkit.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const struct {
    int tracks, sectors, heads, sector_size;
    size_t total_size;
    const char *name;
} g_heathkit_geom[] = {
    { 40, 10, 1, 256, 102400,  "H8/H89 SS/SD Hard-Sector 100KB" },
    { 40, 10, 1, 512, 204800,  "H8/H89 SS/DD Hard-Sector 200KB" },
    { 40, 10, 2, 512, 409600,  "H8/H89 DS/DD Hard-Sector 400KB" },
    { 80, 10, 1, 512, 409600,  "H89 SS/DD 80T 400KB" },
    { 80, 10, 2, 512, 819200,  "H89 DS/DD 80T 800KB" },
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

static int heathkit_einigung(size_t size, int *tracks, int *sectors, int *heads, int *sector_size) {
    int treffer = 0;
    int v_tracks = 0; int u_tracks = 0;
    int v_sectors = 0; int u_sectors = 0;
    int v_heads = 0; int u_heads = 0;
    int v_sector_size = 0; int u_sector_size = 0;
    for (int i = 0; g_heathkit_geom[i].name; i++) {
        if (size != g_heathkit_geom[i].total_size) continue;
        treffer++;
        if (treffer == 1) v_tracks = g_heathkit_geom[i].tracks;
        else if (v_tracks != g_heathkit_geom[i].tracks) u_tracks = 1;
        if (treffer == 1) v_sectors = g_heathkit_geom[i].sectors;
        else if (v_sectors != g_heathkit_geom[i].sectors) u_sectors = 1;
        if (treffer == 1) v_heads = g_heathkit_geom[i].heads;
        else if (v_heads != g_heathkit_geom[i].heads) u_heads = 1;
        if (treffer == 1) v_sector_size = g_heathkit_geom[i].sector_size;
        else if (v_sector_size != g_heathkit_geom[i].sector_size) u_sector_size = 1;
    }
    if (tracks) *tracks = u_tracks ? 0 : v_tracks;
    if (sectors) *sectors = u_sectors ? 0 : v_sectors;
    if (heads) *heads = u_heads ? 0 : v_heads;
    if (sector_size) *sector_size = u_sector_size ? 0 : v_sector_size;
    return treffer;
}

int uft_heathkit_probe(const uint8_t *data, size_t size) {
    if (!data || size < 256) return 0;
    
    for (int i = 0; g_heathkit_geom[i].name; i++) {
        if (size == g_heathkit_geom[i].total_size) {
            int confidence = 35;
            
            /* Check for HDOS boot signature */
            /* HDOS typically has specific patterns in sector 0 */
            if (data[0] == 0xAF || data[0] == 0xC3) {
                confidence += 20;
            }
            
            /* MF-921: hier stand
             *     if (g_heathkit_geom[i].sectors == 10) confidence += 10;
             * mit dem Kommentar „unique to Heath". Gemessen trifft die
             * Bedingung auf JEDE Zeile der Tabelle zu — das ist keine
             * Unterscheidung, sondern eine feste Zugabe. Entfernt.
             *
             * Ist die Groesse mehrdeutig, bleibt die Zahl im Band „nur
             * die Groesse" (30..49): ein einzelnes Bootbyte ist keine
             * gelesene Struktur. */
            int a = 0, b = 0, c = 0, d = 0;
            if (heathkit_einigung(size, &a, &b, &c, &d) > 1 &&
                confidence > 49) {
                confidence = 49;
            }
            return confidence > 45 ? confidence : 0;
        }
    }
    return 0;
}

int uft_heathkit_read(const char *path, uft_heathkit_image_t **image) {
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
    
    uft_heathkit_image_t *img = calloc(1, sizeof(uft_heathkit_image_t));
    if (!img) { free(data); return UFT_ERR_MEMORY; }
    
    img->hard_sectored = 1; /* Heathkit uses hard-sectored disks */
    
    /* MF-921: je Feld die Einigung aller passenden Zeilen; wo sie
     * sich unterscheiden, bleibt 0. Die Bytes bleiben in jedem Fall. */
    int treffer = heathkit_einigung(size, &img->tracks, &img->sectors,
                                    &img->heads, &img->sector_size);
    if (treffer > 1) {
        UFT_WARN("Heathkit: %zu Byte passen auf %d Zeilen der "
                 "Geometrietabelle — was sich unterscheidet, bleibt "
                 "ungesetzt (0)", size, treffer);
    }
    
    img->data = data;
    img->size = size;
    *image = img;
    return UFT_OK;
}

void uft_heathkit_free(uft_heathkit_image_t *image) {
    if (image) { free(image->data); free(image); }
}

int uft_heathkit_get_info(uft_heathkit_image_t *img, char *buf, size_t buf_size) {
    if (!img || !buf) return UFT_ERR_INVALID_PARAM;
    snprintf(buf, buf_size,
        "Heathkit H8/H89 Disk Image (HDOS)\n"
        "Type: %s\n"
        "Geometry: %d tracks x %d sectors x %d heads\n"
        "Sector Size: %d bytes\nTotal: %zu KB\n",
        img->hard_sectored ? "Hard-Sectored" : "Soft-Sectored",
        img->tracks, img->sectors, img->heads, img->sector_size, img->size / 1024);
    return UFT_OK;
}
