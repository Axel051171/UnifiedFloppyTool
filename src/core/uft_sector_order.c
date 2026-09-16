/* SPDX-License-Identifier: GPL-2.0-or-later */
/** @file uft_sector_order.c — Umsetzung von uft_sector_order.h. */

#include "uft/core/uft_sector_order.h"

static bool geo_ok(const uft_order_geometry_t *g) {
    return g && g->cylinders > 0u && g->heads > 0u && g->heads <= 2u &&
           g->sectors > 0u && g->sector_size > 0u;
}

uint32_t uft_order_sector_count(uft_sector_order_t order,
                                const uft_order_geometry_t *g) {
    if (!geo_ok(g)) return 0u;

    switch (order) {
    case UFT_ORDER_SINGLE_SIDED:
    case UFT_ORDER_INTERLEAVED_DS:
        /* Nur eine Seite traegt Daten. Bei INTERLEAVED_DS belegt die Datei
         * trotzdem nur die Nutzsektoren — das Medium ist zweiseitig, das
         * Abbild nicht. Genau diese Unterscheidung fehlt heute. */
        return (uint32_t)g->cylinders * g->sectors;
    default:
        return (uint32_t)g->cylinders * g->heads * g->sectors;
    }
}

const char *uft_order_name(uft_sector_order_t order) {
    switch (order) {
    case UFT_ORDER_CHS_INTERLEAVED: return "Zylinder/Kopf/Sektor verschachtelt";
    case UFT_ORDER_CHS:             return "Zylinder/Kopf/Sektor";
    case UFT_ORDER_HCS:             return "Kopf/Zylinder/Sektor";
    case UFT_ORDER_SERPENTINE:      return "Serpentine (Kopf 0 hin, Kopf 1 zurueck)";
    case UFT_ORDER_INTERLEAVED_DS:  return "einseitiges Format, zweiseitig verschachtelt";
    case UFT_ORDER_SINGLE_SIDED:    return "einseitig flach";
    default:                        return "unbekannt";
    }
}

bool uft_order_index_to_chs(uft_sector_order_t order,
                            const uft_order_geometry_t *g,
                            uint32_t index, uft_chs_t *out) {
    if (!geo_ok(g) || !out) return false;
    if (index >= uft_order_sector_count(order, g)) return false;

    const uint32_t spt  = g->sectors;
    const uint32_t cyls = g->cylinders;

    switch (order) {
    case UFT_ORDER_CHS_INTERLEAVED:
    case UFT_ORDER_CHS: {
        const uint32_t per_cyl = spt * g->heads;
        out->cyl    = (uint16_t)(index / per_cyl);
        const uint32_t rem = index % per_cyl;
        out->head   = (uint8_t)(rem / spt);
        out->sector = (uint8_t)((rem % spt) + 1u);
        return true;
    }
    case UFT_ORDER_HCS: {
        const uint32_t per_head = spt * cyls;
        out->head   = (uint8_t)(index / per_head);
        const uint32_t rem = index % per_head;
        out->cyl    = (uint16_t)(rem / spt);
        out->sector = (uint8_t)((rem % spt) + 1u);
        return true;
    }
    case UFT_ORDER_SERPENTINE: {
        /* Kopf 0: Zylinder 0..n-1 aufsteigend.
         * Kopf 1: Zylinder n-1..0 ABSTEIGEND.
         * XF551 QD: Sektor 721 (Index 720) -> Zylinder 39,
         *           Sektor 1440 (Index 1439) -> Zylinder 0. */
        const uint32_t per_head = spt * cyls;
        if (index < per_head) {
            out->head   = 0u;
            out->cyl    = (uint16_t)(index / spt);
            out->sector = (uint8_t)((index % spt) + 1u);
        } else {
            const uint32_t r = index - per_head;
            out->head   = 1u;
            out->cyl    = (uint16_t)(cyls - 1u - (r / spt));
            out->sector = (uint8_t)((r % spt) + 1u);
        }
        return true;
    }
    case UFT_ORDER_INTERLEAVED_DS:
    case UFT_ORDER_SINGLE_SIDED: {
        out->head   = (order == UFT_ORDER_INTERLEAVED_DS)
                          ? (uint8_t)(g->active_head & 1u) : 0u;
        out->cyl    = (uint16_t)(index / spt);
        out->sector = (uint8_t)((index % spt) + 1u);
        return true;
    }
    default:
        return false;
    }
}

bool uft_order_chs_to_index(uft_sector_order_t order,
                            const uft_order_geometry_t *g,
                            const uft_chs_t *chs, uint32_t *out_index) {
    if (!geo_ok(g) || !chs || !out_index) return false;
    if (chs->cyl >= g->cylinders) return false;
    if (chs->head >= g->heads) return false;
    if (chs->sector < 1u || chs->sector > g->sectors) return false;

    const uint32_t spt = g->sectors;
    const uint32_t s0  = (uint32_t)chs->sector - 1u;
    const uint32_t cyls = g->cylinders;

    switch (order) {
    case UFT_ORDER_CHS_INTERLEAVED:
    case UFT_ORDER_CHS:
        *out_index = ((uint32_t)chs->cyl * g->heads + chs->head) * spt + s0;
        return true;
    case UFT_ORDER_HCS:
        *out_index = ((uint32_t)chs->head * cyls + chs->cyl) * spt + s0;
        return true;
    case UFT_ORDER_SERPENTINE:
        if (chs->head == 0u)
            *out_index = (uint32_t)chs->cyl * spt + s0;
        else
            *out_index = cyls * spt + (uint32_t)(cyls - 1u - chs->cyl) * spt + s0;
        return true;
    case UFT_ORDER_SINGLE_SIDED:
        if (chs->head != 0u) return false;
        *out_index = (uint32_t)chs->cyl * spt + s0;
        return true;
    case UFT_ORDER_INTERLEAVED_DS:
        if (chs->head != (uint8_t)(g->active_head & 1u)) return false;
        *out_index = (uint32_t)chs->cyl * spt + s0;
        return true;
    default:
        return false;
    }
}
