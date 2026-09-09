/**
 * @file uft_disk_create_image.c
 * @brief Ein neues, leeres Diskettenabbild anlegen (MF-991).
 *
 * ── Warum es das bisher nicht gab ────────────────────────────────────
 *
 * Die Plugin-Tafel hat seit jeher einen `create`-Zeiger:
 *
 *     uft_error_t (*create)(uft_disk_t*, const char* path,
 *                           const uft_geometry_t* geometry);
 *
 * Gemessen ueber `git ls-files`: **drei** von 137 Plugins fuellen ihn —
 * `img`, `hfe`, `g64` —, und **`->create(` hat im ganzen Baum null
 * Aufrufer**. Drei funktionierende Umsetzungen hinter einer Tuer, die es
 * nicht gab.
 *
 * Es gab sie einmal, und sie wurde zu Recht entfernt. MF-294 fand in
 * `uft_core.h` einen Prototyp `uft_disk_create(path, format, geometry)`
 * ohne Umsetzung: der einzige Koerper dieses Namens ist der **0-Argument-
 * Allokator** in `uft_core_stubs.c`. Ein Aufrufer haette still an das
 * falsche Symbol gebunden — Signaturbombe ohne Uebersetzerwarnung. MF-294
 * loeschte den Prototyp und schrieb dazu: *„When the unified-disk
 * subsystem gets implemented, use a distinct name."*
 *
 * Genau das ist hier gemacht. `uft_disk_create_image` kollidiert mit
 * nichts; `uft_disk_create` bleibt der Allokator, der er ist.
 *
 * ── Warum ein Fehlercode und kein Zeiger ─────────────────────────────
 *
 * `uft_disk_open()` gibt bei JEDEM Fehler NULL zurueck. Fuer das Oeffnen
 * mag das hingehen; fuers Erzeugen nicht: „dieses Format kann ich nicht
 * anlegen" und „die Datei liess sich nicht schreiben" sind zwei
 * verschiedene Auskuenfte, und wer sie zu einer zusammenzieht, zwingt den
 * Aufrufer zum Raten. Diese Funktion sagt, woran es lag.
 *
 * Insbesondere fuer die 134 Plugins **ohne** `create`: sie bekommen
 * `UFT_ERROR_NOT_SUPPORTED` und **es entsteht keine Datei**. Ein `UFT_OK`
 * fuer nicht getane Arbeit waere die Klasse „Erfolg ohne Tat", die dieser
 * Baum in MF-883/930 zwanzigmal gefunden hat.
 *
 * ── Warum eine vorhandene Datei nicht ueberschrieben wird ────────────
 *
 * Alle drei `create`-Umsetzungen beginnen mit `fopen(path, "wb")`. Das
 * **kuerzt** eine vorhandene Datei auf null, bevor irgendetwas geprueft
 * wird. Ein Aufruf mit dem Pfad einer vorhandenen Sammlung haette sie
 * ohne Rueckfrage vernichtet.
 *
 * Deshalb prueft diese Funktion **vorher** und sagt ohne ausdrueckliche
 * Freigabe `UFT_ERROR_FILE_EXISTS` — fail-closed, wie das Schreibtor
 * (MF-986). Die Freigabe ist ein eigenes Argument, damit sie im
 * Aufrufcode sichtbar dasteht und nicht in einer Vorgabe versteckt ist.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_error.h"
#include "uft/core/uft_disk_create_image.h"

/* Namensvergleich ohne Ruecksicht auf Gross-/Kleinschreibung.
 * Nicht `strcasecmp`: das ist POSIX und fehlt unter MSVC. */
static int namen_gleich(const char *a, const char *b)
{
    if (!a || !b) return 0;
    for (; *a && *b; a++, b++) {
        int ca = (*a >= 'A' && *a <= 'Z') ? *a + 32 : *a;
        int cb = (*b >= 'A' && *b <= 'Z') ? *b + 32 : *b;
        if (ca != cb) return 0;
    }
    return *a == '\0' && *b == '\0';
}

static const uft_format_plugin_t *plugin_nach_namen(const char *name)
{
    const size_t n = uft_get_format_count();
    for (size_t i = 0; i < n; i++) {
        const uft_format_plugin_t *p = uft_get_format_by_index(i);
        if (p && namen_gleich(p->name, name))
            return p;
    }
    return NULL;
}

static int datei_vorhanden(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

uft_error_t uft_disk_create_image(const char *path,
                                  const char *format_name,
                                  const uft_geometry_t *geometry,
                                  bool overwrite_existing,
                                  uft_disk_t **out)
{
    if (out) *out = NULL;
    if (!path || !path[0] || !format_name || !geometry || !out)
        return UFT_ERROR_INVALID_ARG;

    /* Eine Geometrie ohne Spuren, Koepfe oder Sektoren beschreibt keine
     * Diskette. Das hier ist KEIN Ersatz fuer die Pruefungen im Plugin —
     * das kennt seine eigenen Grenzen besser (img besteht z. B. auf 512
     * Byte je Sektor). Es faengt nur den Fall ab, in dem eine genullte
     * Struktur durchgereicht wurde. */
    if (geometry->cylinders == 0 || geometry->heads == 0 ||
        geometry->sectors == 0)
        return UFT_ERROR_INVALID_ARG;

    const uft_format_plugin_t *plugin = plugin_nach_namen(format_name);
    if (!plugin)
        return UFT_ERROR_UNKNOWN_FORMAT;

    /* Der ehrliche Teil: 134 der 137 Plugins koennen das nicht, und sie
     * sollen das sagen — nicht eine leere Datei hinterlassen und `UFT_OK`
     * melden. */
    if (!plugin->create)
        return UFT_ERROR_NOT_SUPPORTED;

    /* VOR dem Plugin pruefen: dessen `fopen(path, "wb")` kuerzt sofort. */
    if (!overwrite_existing && datei_vorhanden(path))
        return UFT_ERROR_FILE_EXISTS;

    uft_disk_t *disk = calloc(1, sizeof(uft_disk_t));
    if (!disk)
        return UFT_ERR_OUT_OF_MEMORY;

    strncpy(disk->path_buf, path, sizeof(disk->path_buf) - 1);
    disk->path_buf[sizeof(disk->path_buf) - 1] = '\0';
    disk->path      = disk->path_buf;
    disk->format    = plugin->format;
    disk->plugin    = plugin;   /* MF-445: merken, nicht spaeter raten */
    disk->read_only = false;    /* ein frisch erzeugtes Abbild ist beschreibbar */
    disk->geometry  = *geometry;

    const uft_error_t err = plugin->create(disk, path, geometry);
    if (err != UFT_OK) {
        free(disk);
        return err;             /* durchreichen, nicht einebnen */
    }

    disk->is_open = true;
    *out = disk;
    return UFT_OK;
}
