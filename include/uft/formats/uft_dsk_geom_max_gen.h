/* =====================================================================
 * GENERATED FILE — DO NOT EDIT BY HAND.
 * Source of truth: src/formats/dsk_generic/uft_dsk_generic.c (dsk_geometries[])
 * Regenerate with: python scripts/generators/gen_dsk_geom_max.py
 * Any manual edits will be overwritten on the next generator run.
 * ===================================================================== */
#ifndef UFT_DSK_GEOM_MAX_GEN_H
#define UFT_DSK_GEOM_MAX_GEN_H

/**
 * @file uft_dsk_geom_max_gen.h
 * @brief Die groesste Sektorgroesse der DSK-Geometrietafel (ERZEUGT).
 *
 * Damit ein Puffer, der eine Tafelzeile fuellt, sich gegen die Tafel
 * absichern kann — zur UEBERSETZUNGSZEIT, nicht erst an der Diskette:
 *
 *     _Static_assert(sizeof(pad) >= UFT_GEOM_TABLE_MAX_SECTOR, "...");
 *
 * Gemessen beim Erzeugen: 49 Zeilen, Verteilung
 *     128 Byte :  5 Zeilen
 *     256 Byte : 22 Zeilen
 *     512 Byte : 20 Zeilen
 *    1024 Byte :  2 Zeilen
 *
 * Die groesste tragen: DSK_KC, DSK_RLD
 */
#define UFT_GEOM_TABLE_MAX_SECTOR 1024u

#endif /* UFT_DSK_GEOM_MAX_GEN_H */
