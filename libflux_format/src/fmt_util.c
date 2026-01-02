#include "uft/uft_error.h"
#include "fmt_util.h"

#include <string.h>
#include <errno.h>
#include <stdlib.h>

bool fmt_read_exact(FILE *fp, void *dst, size_t n)
{
    if (!fp || !dst) return false;
    return fread(dst, 1, n, fp) == n;
}

size_t fmt_read_prefix(FILE *fp, uint8_t *buf, size_t cap)
{
    if (!fp || !buf || cap == 0) return 0;
    if (fseek(fp, 0, SEEK_SET) != 0) return 0;
    return fread(buf, 1, cap, fp);
}

static void track_init_empty(ufm_track_t *t, uint16_t cyl, uint16_t head)
{
    memset(t, 0, sizeof(*t));
    t->cyl = cyl;
    t->head = head;
}

int fmt_ufm_alloc_geom(ufm_disk_t *d, uint16_t cyls, uint16_t heads)
{
    if (!d) return -EINVAL;
    if (cyls == 0 || heads == 0) return -EINVAL;
    if (cyls > 2048 || heads > 8) return -EINVAL;

    ufm_disk_free(d);
    ufm_disk_init(d);

    size_t n = (size_t)cyls * (size_t)heads;
    d->tracks = (ufm_track_t*)calloc(n, sizeof(ufm_track_t));
    if (!d->tracks) return -ENOMEM;

    d->cyls = cyls;
    d->heads = heads;

    for (uint16_t c = 0; c < cyls; ++c)
        for (uint16_t h = 0; h < heads; ++h)
            track_init_empty(&d->tracks[(size_t)c * heads + h], c, h);

    return 0;
}

void fmt_set_label(ufm_disk_t *d, const char *label)
{
    if (!d || !label) return;
    memset(d->label, 0, sizeof(d->label));
    /* keep it simple + deterministic */
    strncpy(d->label, label, sizeof(d->label) - 1);
}
