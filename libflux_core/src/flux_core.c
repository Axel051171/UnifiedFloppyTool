#include "uft/uft_error.h"
#include "flux_core.h"
#include "flux_logical.h"
#include <string.h>
#include <stdlib.h>

void ufm_disk_init(ufm_disk_t *d)
{
    if (!d) return;
    memset(d, 0, sizeof(*d));
    d->hw.type = UFM_HW_UNKNOWN;
    d->logical = NULL;
}

void ufm_revolution_free_contents(ufm_revolution_t *r)
{
    if (!r) return;
    free(r->dt_ns);
    r->dt_ns = NULL;
    r->count = 0;

    free(r->index.offsets);
    r->index.offsets = NULL;
    r->index.count = 0;
}

void ufm_disk_free(ufm_disk_t *d)
{
    if (!d) return;

    /* logical view */
    if (d->logical)
    {
        ufm_logical_image_t *li = (ufm_logical_image_t*)d->logical;
        ufm_logical_free(li);
        free(li);
        d->logical = NULL;
    }

    if (d->tracks)
    {
        const uint32_t n = (uint32_t)d->cyls * (uint32_t)d->heads;
        for (uint32_t i = 0; i < n; i++)
        {
            ufm_track_t *t = &d->tracks[i];
            if (t->revs)
            {
                for (uint32_t r = 0; r < t->rev_count; r++)
                    ufm_revolution_free_contents(&t->revs[r]);
                free(t->revs);
            }
            t->revs = NULL;
            t->rev_count = 0;
        }
        free(d->tracks);
    }

    /* reset everything */
    ufm_disk_init(d);
}

ufm_track_t* ufm_disk_track(ufm_disk_t *d, uint16_t cyl, uint16_t head)
{
    if (!d) return NULL;
    if (cyl >= d->cyls || head >= d->heads) return NULL;
    return &d->tracks[(uint32_t)cyl * (uint32_t)d->heads + (uint32_t)head];
}
