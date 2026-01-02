#include "uft/uft_error.h"
#include "flux_logical.h"
#include "flux_core.h"

#include <stdlib.h>
#include <string.h>

void ufm_logical_init(ufm_logical_image_t *li)
{
    if (!li) return;
    memset(li, 0, sizeof(*li));
}

void ufm_logical_free(ufm_logical_image_t *li)
{
    if (!li) return;
    if (li->sectors)
    {
        for (uint32_t i = 0; i < li->count; i++)
        {
            free(li->sectors[i].data);
            free(li->sectors[i].meta);
            li->sectors[i].data = NULL;
            li->sectors[i].size = 0;
        }
        free(li->sectors);
    }
    ufm_logical_init(li);
}

int ufm_logical_reserve(ufm_logical_image_t *li, uint32_t want)
{
    if (!li) return -22; /*EINVAL*/
    if (want <= li->cap) return 0;

    uint32_t newcap = li->cap ? li->cap : 64u;
    while (newcap < want)
    {
        if (newcap > (uint32_t)(~0u) / 2u) return -12; /*ENOMEM*/
        newcap *= 2u;
    }

    ufm_sector_t *ns = (ufm_sector_t*)realloc(li->sectors, (size_t)newcap * sizeof(*ns));
    if (!ns) return -12;
    li->sectors = ns;
    li->cap = newcap;
    return 0;
}


static ufm_sector_t* logical_append_slot(ufm_logical_image_t *li)
{
    if (!li) return NULL;
    if (ufm_logical_reserve(li, li->count + 1u) < 0) return NULL;
    ufm_sector_t *s = &li->sectors[li->count++];
    memset(s, 0, sizeof(*s));
    return s;
}

ufm_sector_t* ufm_logical_add_sector_ref(
    ufm_logical_image_t *li,
    uint16_t cyl, uint16_t head, uint16_t sec,
    const uint8_t *data, uint32_t size,
    ufm_sec_flags_t flags)
{
    if (!li || !data || size == 0) return NULL;

    ufm_sector_t *s = logical_append_slot(li);
    if (!s) return NULL;

    s->data = (uint8_t*)malloc(size);
    if (!s->data)
    {
        li->count--; /* rollback slot */
        return NULL;
    }

    memcpy(s->data, data, size);
    s->size  = size;
    s->cyl   = cyl;
    s->head  = head;
    s->sec   = sec;
    s->flags = flags;
    s->confidence = UFM_CONF_UNKNOWN;

    /* meta_type/meta left as zero (NONE/NULL) unless a decoder attaches it. */
    return s;
}

int ufm_logical_add_sector(
    ufm_logical_image_t *li,
    uint16_t cyl, uint16_t head, uint16_t sec,
    const uint8_t *data, uint32_t size,
    ufm_sec_flags_t flags)
{
    if (!li || !data || size == 0) return -22; /* -EINVAL */
    return ufm_logical_add_sector_ref(li, cyl, head, sec, data, size, flags) ? 0 : -12; /* -ENOMEM */
}

ufm_sector_t* ufm_logical_find(ufm_logical_image_t *li, uint16_t cyl, uint16_t head, uint16_t sec)
{
    if (!li) return NULL;
    for (uint32_t i = 0; i < li->count; i++)
    {
        ufm_sector_t *s = &li->sectors[i];
        if (s->cyl == cyl && s->head == head && s->sec == sec)
            return s;
    }
    return NULL;
}

const ufm_sector_t* ufm_logical_find_const(const ufm_logical_image_t *li, uint16_t cyl, uint16_t head, uint16_t sec)
{
    if (!li) return NULL;
    for (uint32_t i = 0; i < li->count; i++)
    {
        const ufm_sector_t *s = &li->sectors[i];
        if (s->cyl == cyl && s->head == head && s->sec == sec)
            return s;
    }
    return NULL;
}

int ufm_disk_attach_logical(struct ufm_disk *d)
{
    if (!d) return -22;
    if (d->logical) return 0;
    ufm_logical_image_t *li = (ufm_logical_image_t*)calloc(1, sizeof(*li));
    if (!li) return -12;
    ufm_logical_init(li);
    d->logical = (struct ufm_logical_image*)li;
    return 0;
}
