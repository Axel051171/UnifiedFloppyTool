/**
 * @file test_fat_atari_geometrie.c
 * @brief The Atari ST rows of the FAT geometry table against two
 *        independent sources (MF-1374).
 *
 * `uft_fat_std_geometries[]` (include/uft/fs/uft_fat12.h) listed the two
 * Atari ST DD rows with 3 sectors per FAT — the PC 720K value. Two
 * independent hands say 5 for an 80-track, 9-sector, 2-sector-cluster
 * Atari floppy:
 *
 *   * SED 5.68 (Anton Stepper, Claus Brod; Atari-ST disk monitor), the
 *     media-byte table of its help file SED_568.HLP — READ ONLY, channel
 *     *Spec*: the package is commercial software ("kommerzielle Software,
 *     keine PD-Software"), nothing of it is in this tree;
 *   * Hatari, tools/uft-scout/work/hatari/src/createBlankImage.c:13-27
 *     (table) and :143-148 (`else if (nTracks >= 80) SPF = 5;`) —
 *     GPL-2, read only.
 *
 * The arithmetic agrees: 1440 sectors - 1 reserved - 7 root (112 entries)
 * - 2 x SPF, in 2-sector clusters, is 711 clusters with SPF 5 and 713 with
 * SPF 3; a 12-bit FAT for 713 + 2 entries needs 1073 bytes = 3 sectors,
 * so 3 FITS — which is
 * why the PC table uses it and why the size cannot decide. The formatter
 * decides, and both sources name the Atari formatter's 5.
 *
 * What stays OPEN and is not asserted here: the HD row (SED: SPF 6,
 * Hatari: SPF 9 — the two disagree) and the SS media byte (SED 0xF9,
 * Hatari 0xF8, which Hatari notes "isn't used by ST-BIOS"). See P3-597.
 */
#include "uft/fs/uft_fat12.h"

#include <stdio.h>
#include <string.h>

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *hinweis)
{
    if (ok) { printf("  [ok ] %s\n", was); gruen++; }
    else    { printf("  [ROT] %s -- %s\n", was, hinweis); rot++; }
}

static const uft_fat_geometry_t *zeile(const char *name)
{
    for (const uft_fat_geometry_t *g = uft_fat_std_geometries; g->name; g++)
        if (strcmp(g->name, name) == 0) return g;
    return NULL;
}

int main(void)
{
    puts("=== FAT-Geometrietafel: Atari ST DD gegen SED 5.68 und Hatari ===");
    char h[160];

    const uft_fat_geometry_t *ds = zeile("Atari ST DS");
    const uft_fat_geometry_t *ss = zeile("Atari ST SS");
    pruefe("die Zeilen 'Atari ST DS' und 'Atari ST SS' gibt es",
           ds && ss, "Zeile fehlt");
    if (!ds || !ss) goto ende;

    snprintf(h, sizeof h, "DS fat_sectors = %u", (unsigned)ds->fat_sectors);
    pruefe("Atari ST DS: 5 Sektoren je FAT (SED 5.68, Hatari)",
           ds->fat_sectors == 5, h);
    snprintf(h, sizeof h, "SS fat_sectors = %u", (unsigned)ss->fat_sectors);
    pruefe("Atari ST SS: 5 Sektoren je FAT (Hatari: 80 Spuren -> SPF 5)",
           ss->fat_sectors == 5, h);

    /* The fields both sources agree on, held so that the correction does
     * not move anything else. */
    pruefe("DS: 1440 Sektoren, 9 je Spur, 2 Koepfe, 80 Spuren, SPC 2, 112 Eintraege",
           ds->total_sectors == 1440 && ds->sectors_per_track == 9 &&
           ds->heads == 2 && ds->tracks == 80 &&
           ds->sectors_per_cluster == 2 && ds->root_entries == 112,
           "ein zweites Feld hat sich verschoben");
    pruefe("SS: 720 Sektoren, 9 je Spur, 1 Kopf, 80 Spuren, SPC 2, 112 Eintraege",
           ss->total_sectors == 720 && ss->sectors_per_track == 9 &&
           ss->heads == 1 && ss->tracks == 80 &&
           ss->sectors_per_cluster == 2 && ss->root_entries == 112,
           "ein zweites Feld hat sich verschoben");

    /* The PC 720K row keeps its 3 — the correction is Atari-only. */
    const uft_fat_geometry_t *pc = zeile("3.5\" DD 720KB");
    pruefe("PC 720K behaelt 3 Sektoren je FAT",
           pc && pc->fat_sectors == 3, "die PC-Zeile wurde mitgeaendert");

ende:
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
