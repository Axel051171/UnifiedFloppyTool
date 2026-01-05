#include "uft/uft_error.h"
#include "flux_window.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

static int rev_make_slice(
    const ufm_revolution_t *in,
    uint32_t start_sample,
    uint32_t end_sample,
    ufm_revolution_t *out)
{
    if (!in || !out) return -EINVAL;
    if (end_sample < start_sample) return -EINVAL;
    if (end_sample > in->count) return -EINVAL;

    memset(out, 0, sizeof(*out));

    const uint32_t n = end_sample - start_sample;
    if (n == 0) {
        /* empty slice: legal, but pointless */
        return 0;
    }

    out->dt_ns = (uint32_t*)malloc((size_t)n * sizeof(uint32_t));
    if (!out->dt_ns) return -ENOMEM;
    memcpy(out->dt_ns, in->dt_ns + start_sample, (size_t)n * sizeof(uint32_t));
    out->count = n;

    /* Mark slice boundary as index=0. */
    out->index.offsets = (uint32_t*)malloc(sizeof(uint32_t));
    if (!out->index.offsets) {
        free(out->dt_ns);
        out->dt_ns = NULL;
        out->count = 0;
        return -ENOMEM;
    }
    out->index.count = 1;
    out->index.offsets[0] = 0;

    /* Quality is copied (archival); downstream can recompute if desired. */
    out->quality = in->quality;
    return 0;
}

int ufm_slice_by_index(
    const ufm_revolution_t *in,
    ufm_revolution_t **out_revs,
    uint32_t *out_n)
{
    if (!in || !out_revs || !out_n) return -EINVAL;
    *out_revs = NULL;
    *out_n = 0;

    if (!in->dt_ns || in->count == 0) return -EINVAL;

    /* If we don't have enough index pulses, return a single slice = whole track. */
    if (!in->index.offsets || in->index.count < 2) {
        ufm_revolution_t *one = (ufm_revolution_t*)calloc(1, sizeof(ufm_revolution_t));
        if (!one) return -ENOMEM;
        const int rc = rev_make_slice(in, 0, in->count, &one[0]);
        if (rc < 0) { free(one); return rc; }
        *out_revs = one;
        *out_n = 1;
        return 0;
    }

    /* We slice between consecutive index offsets. Ignore out-of-range offsets safely. */
    uint32_t valid = 0;
    for (uint32_t i = 0; i + 1 < in->index.count; i++) {
        const uint32_t a = in->index.offsets[i];
        const uint32_t b = in->index.offsets[i + 1];
        if (a <= b && b <= in->count) valid++;
    }
    if (valid == 0) {
        ufm_revolution_t *one = (ufm_revolution_t*)calloc(1, sizeof(ufm_revolution_t));
        if (!one) return -ENOMEM;
        const int rc = rev_make_slice(in, 0, in->count, &one[0]);
        if (rc < 0) { free(one); return rc; }
        *out_revs = one;
        *out_n = 1;
        return 0;
    }

    ufm_revolution_t *arr = (ufm_revolution_t*)calloc((size_t)valid, sizeof(ufm_revolution_t));
    if (!arr) return -ENOMEM;

    uint32_t outi = 0;
    for (uint32_t i = 0; i + 1 < in->index.count; i++) {
        const uint32_t a = in->index.offsets[i];
        const uint32_t b = in->index.offsets[i + 1];
        if (a > b || b > in->count) continue;
        const int rc = rev_make_slice(in, a, b, &arr[outi]);
        if (rc < 0) {
            for (uint32_t k = 0; k < outi; k++)
                ufm_revolution_free_contents(&arr[k]);
            free(arr);
            return rc;
        }
        outi++;
    }

    /* outi should equal valid, but keep defensive. */
    *out_revs = arr;
    *out_n = outi;
    return 0;
}
